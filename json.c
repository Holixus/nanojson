
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdarg.h"
#include "string.h"
#include "errno.h"
#include "json.h"

/*

FALSE    false
TRUE     true
NULL     null

SPACE    '\032' | '\r' | '\n' | '\t'

number   [-]? [0-9]+

double
string   '"' ( '\' ( ["\/bfnrt] | u[0-9A-Fa-f]{4} ) | [^\] ) * '"'

array    '[' (json (',' json)*)? ']'

object   '{' (string : json (',' string : json)*)? '}'

json     object | array | TRUE | FALSE | NULL | number | string

*/

typedef struct jsn_parser jsn_parser_t;

/* ------------------------------------------------------------------------ */
struct jsn_parser {
	char *text;
	char *ptr;
	jsn_t *pool;
	size_t free_node_index;
	size_t pool_size;
};


/* ------------------------------------------------------------------------ */
static jsn_t *jsn_alloc(jsn_parser_t *p)
{
	if (p->free_node_index >= p->pool_size)
		return errno = ENOMEM, NULL;
	jsn_t *j = p->pool + p->free_node_index++;
	memset(j, 0, sizeof *j);
	return j;
}


/* ------------------------------------------------------------------------ */
static int is_id_char(int c)
{
	if (c < 'A')
		return '0' <= c && c <= '9';
	if (c >= 'a')
		return (c <= 'z') || c == '_';
	return c <= 'Z';
}


/* ------------------------------------------------------------------------ */
static int after_space(char **p)
{
	char *s = *p;
	for (uint32_t ch; ch = *s, ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; ++s)
		;

	return *(*p = s);
}


/* ------------------------------------------------------------------------ */
static int match_number(char **p, int32_t *num)
{
	char *s = *p;

	int sign = 0;
	if (*s == '-')
		sign = 1, ++s;

	if (*s < '0' || '9' < *s)
		return 0;

	int32_t n = 0;
	do {
		n = n * 10 + *s++ - '0';
	} while ('0' <= *s && *s <= '9');

	*num = sign ? -n : n;
	*p = s;
	return 1;
}


/* ------------------------------------------------------------------------ */
static int hextonibble(char digit)
{
	switch (digit >> 6) {
	case 1:;
		int n = (digit & ~32) - 'A' + 10;
		return 9 < n && n < 16 ? n : 256;

	case 0:
		digit -= '0';
		if (0 <= digit && digit < 10)
			return digit;
	}

	return 256;
}


/* ------------------------------------------------------------------------ */
int string_unescape(char *s)
{
	char *d = s;
	for (; *s && *s != '"'; ++s) {
		if (*s == '\\') {
			switch (*++s) {
			case '"': *d++ = '"'; break;
			case '/': *d++ = '/'; break;
			case '\\':*d++ = '\\'; break;
			case 'b': *d++ = '\b'; break;
			case 'f': *d++ = '\f'; break;
			case 'n': *d++ = '\n'; break;
			case 'r': *d++ = '\r'; break;
			case 't': *d++ = '\t'; break;
			case 'u': {
					uint32_t uc = 0;
					for (int i = 1; i < 5; ++i) {
						uint32_t d = hextonibble(s[i]);
						if (d > 15) {
							--s;
							goto _not_escape;
						}
						uc = uc * 16 + d;
					}
					s += 4;
					if (!(uc & 0xFF80))
						*d++ = uc;
					else
						if (!(uc & 0xF800)) {
							*d++ = 0xC0 | (uc >> 6);
							*d++ = 0x80 | (uc & 0x3F);
						} else {
							*d++ = 0xE0 | (uc >> 12);
							*d++ = 0x80 | (0x3F & uc >> 6);
							*d++ = 0x80 | (0x3F & uc);
						}
				} break;
			default:
				goto _not_escape;
			}
		} else
_not_escape:
			*d++ = *s;
	}
	if (*s == '"') {
		*d = 0;
		return 1;
	}

	return 0;
}


/* ------------------------------------------------------------------------ */
char *string_escape(char *p, char *e, char const *s)
{
	static char const hex[] = "01234567890abcdef";
	while (p < e && *s) {
		int c = *s++;
		if (32 <= c && c <= 127) {
			switch (c) {
			case '"':
			case '/':
			case '\\':
				*p++ = '\\';
			}
			*p++ = c;
			continue;
		}
		*p++ = '\\';
		if (c < ' ')
			switch (c) {
			case '\b': *p++ = 'b'; continue;
			case '\f': *p++ = 'f'; continue;
			case '\n': *p++ = 'n'; continue;
			case '\r': *p++ = 'r'; continue;
			case '\t': *p++ = 't'; continue;
			}
		uint32_t uc = 0xFFFF;
		if (!(c & 0x80))
			uc = c;
		else
			if ((c & 0xE0) == 0xC0) {
				if ((s[0] & 0300) == 0200) {
					uc = ((c & 0x1F) << 6) + (s[0] & 0x3F);
					++s;
				}
			} else
				if ((c & 0xF0) == 0xE0) {
					if ((s[0] & 0300) == 0200 && (s[1] & 0300) == 0200) {
						uc = ((c & 0xF) << 12) + ((s[0] & 0x3F) << 6) + (s[1] & 0x3F);
						s += 2;
					}
				}
		*p++ = 'u';
		p[0] = hex[uc >> 12 & 15];
		p[1] = hex[uc >>  8 & 15];
		p[2] = hex[uc >>  4 & 15];
		p[3] = hex[uc >>  0 & 15];
		p += 4;
	}
	if (p < e)
		*p = 0;
	return p;
}


/* ------------------------------------------------------------------------ */
static int match_string(char **p, char **str)
{
	char *s = *p;
	if (*s != '"')
		return 0;

	*str = ++s;

	for (; *s && *s != '"'; ++s) {
		if (*s == '\\') {
			switch (*++s) {
			case '"':
			case '/':
			case '\\':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
				break;
			case 'u': {
					for (int i = 1; i < 5; ++i) {
						uint32_t d = hextonibble(s[i]);
						if (d > 15) {
							--s;
							goto _not_escape;
						}
					}
					s += 4;
				} break;
			default:
				goto _not_escape;
			}
		} else
_not_escape:;
	}
	if (*s == '"') {
		*p = s + 1;
		return 1;
	}
	return 0;
}


/* ------------------------------------------------------------------------ */
static int match_json(jsn_parser_t *p, jsn_t *obj)
{
	char *s = p->ptr;
	int open_char = after_space(&s);
	if (open_char != '[' && open_char != '{' ) {
		switch (open_char) {
		case '"':
			if (!match_string(&s, &obj->data.string))
				return errno = EINVAL, 0;
			p->ptr = s;
			return obj->type = JS_STRING;
		case 'n':
			if (s[1] != 'u' || s[2] != 'l' || s[3] != 'l' || is_id_char(s[4]))
				return errno = EINVAL, 0;
			p->ptr = s + 4;
			return obj->type = JS_NULL;
		case 't':
			if (s[1] != 'r' || s[2] != 'u' || s[3] != 'e' || is_id_char(s[4]))
				return errno = EINVAL, 0;
			p->ptr = s + 4;
			obj->data.number = 1;
			return obj->type = JS_BOOLEAN;
		case 'f':
			if (s[1] != 'a' || s[2] != 'l' || s[3] != 's'  || s[4] != 'e' || is_id_char(s[5]))
				return errno = EINVAL, 0;
			p->ptr = s + 5;
			obj->data.number = 0;
			return obj->type = JS_BOOLEAN;
		default:
			if (!match_number(&s, &obj->data.number))
				return errno = EINVAL, 0;
			p->ptr = s;
			return obj->type = JS_NUMBER;
		}
	}

	int is_object = *s == '{';
	int close_char = is_object ? '}' : ']';
	++s;

	int index = 0;
	short *next = &obj->data.object.nodes;

	if (after_space(&s) == close_char)
		goto _empty;

	do {
		after_space(&s);
		jsn_t *node = jsn_alloc(p);
		if (!node)
			return errno = ENOMEM, 0;
		*next = (short)(node - obj);
		if (is_object) {
			p->ptr = s;
			if (!match_string(&s, &node->id.string))
				return errno = EINVAL, 0;
			node->id_type = JS_STRING;
			p->ptr = s;
			if (after_space(&s) != ':')
				return errno = EINVAL, 0;
			++s;
			after_space(&s);
		} else {
			node->id.number = index;
			node->id_type = JS_NUMBER;
		}

		p->ptr = s;
		if (!match_json(p, node))
			return 0;

		s = p->ptr;
		next = &node->next;
		++index;
		if (after_space(&s) != ',')
			break;
		++s;
	} while (1);

	if (after_space(&s) != close_char)
		return errno = EINVAL, 0;

_empty:
	*next = -1;
	p->ptr = s + 1;
	obj->data.object.length = index;
	return obj->type = is_object ? JS_OBJECT : JS_ARRAY;
}


/* ------------------------------------------------------------------------ */
int json_parse(jsn_t *pool, size_t size, char *text)
{
	jsn_parser_t p = {
		.text = text,
		.ptr = text,
		.pool = pool,
		.free_node_index = 0,
		.pool_size = size
	};

	jsn_t *root = jsn_alloc(&p);

	if (!match_json(&p, root)) {
		//printf("failed : <<<%s>>>\n", p.ptr);
		return p.text - p.ptr;
	}

	for (int i = 0; i < p.free_node_index; ++i) {
		jsn_t *node = pool + i;
		if (node->type == JS_STRING)
			string_unescape(node->data.string);
		if (node->id_type == JS_STRING)
			string_unescape(node->id.string);
	}

	return p.free_node_index;
}


/* ------------------------------------------------------------------------ */
jsn_t *json_item(jsn_t *obj, char const *id)
{
	json_foreach(obj, index)
		if (!strcmp(id, obj[index].id.string))
			return obj + index;

	return NULL;
}


/* ------------------------------------------------------------------------ */
jsn_t *json_cell(jsn_t *obj, int index)
{
	if (index >= obj->data.object.length)
		return NULL;

	json_foreach(obj, index)
		if (index == obj[index].id.number)
			return obj + index;

	return NULL;
}


/* ------------------------------------------------------------------------ */
int json_length(jsn_t *obj)
{
	return obj->type >= JS_ARRAY ? obj->data.object.length : 0;
}


/* ------------------------------------------------------------------------ */
int json_number(jsn_t *node, int absent)
{
	if (!node)
		return absent;

	switch (node->type) {
	case JS_NUMBER:
	case JS_BOOLEAN:
		return node->data.number;
	case JS_NULL:
		return 0;
	case JS_STRING:
		return atoi(node->data.string);
	}
	return absent;
}


/* ------------------------------------------------------------------------ */
char const *json_string(jsn_t *node, char const *absent)
{
	if (!node)
		return absent;

	if (node->type == JS_STRING)
		return node->data.string;

	return NULL;
}


/* ------------------------------------------------------------------------ */
char *json_stringify(char *p, char *e, jsn_t *root)
{
	switch (root->type) {
	case JS_NULL:
		return p + snprintf(p, (size_t)(e-p), "null");
	case JS_BOOLEAN:
		return p + snprintf(p, (size_t)(e-p), root->data.number ? "true" : "false");
	case JS_NUMBER:
		return p + snprintf(p, (size_t)(e-p), "%d", root->data.number);
	case JS_STRING:
		if (p < e) *p++ = '"';
		p = string_escape(p, e, root->data.string);
		if (p < e) *p++ = '"';
		break;
	case JS_OBJECT:
		if (p < e) *p++ = '{';
		json_foreach(root, index) {
			jsn_t *node = root + index;
			if (p < e) *p++ = '"';
			p = string_escape(p, e, node->id.string);
			if (p < e) *p++ = '"';
			if (p < e) *p++ = ':';
			p = json_stringify(p, e, node);
			if (node->next >= 0)
				if (p < e) *p++ = ',';
		}
		if (p < e) *p++ = '}';
		break;
	case JS_ARRAY:
		if (p < e) *p++ = '[';
		json_foreach(root, index) {
			jsn_t *node = root + index;
			p = json_stringify(p, e, node);
			if (node->next >= 0)
				if (p < e) *p++ = ',';
		}
		if (p < e) *p++ = ']';
		break;
	}
	if (p < e)
		*p = 0;
	return p;
}

