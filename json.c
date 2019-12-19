
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdarg.h"
#include "string.h"
#include "errno.h"
#include "json.h"
#ifdef JSON_FLOATS
#include "math.h"
#endif

/*

FALSE    false
TRUE     true
NULL     null

SPACE    '\032' | '\r' | '\n' | '\t'

number   [-]? [0-9]+ ('.' [0-9]* ([Ee] [0-9]+)? )?

string   '"' ( '\' ( ["\/bfnrt] | u[0-9A-Fa-f]{4} ) | [^\] ) * '"'

array    '[' (json (',' json)*)? ']'

object   '{' (string : json (',' string : json)*)? '}'

json     object | array | TRUE | FALSE | NULL | number | string

*/


/* ------------------------------------------------------------------------ */
typedef
struct jsn_parser {
	char *text;       /* source text */
	char *ptr;        /* current parser position */

	jsn_t *pool;            /* array of json nodes */
	size_t free_node_index; /* index of first free node */
	size_t pool_size;       /* total array size */
} jsn_parser_t;


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
		return c <= 'z' || c == '_';
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
static int match_char(char **p, int ch)
{
	return after_space(p) == ch ? (++*p, 1) : 0;
}

#ifdef JSON_64BITS_INTEGERS
#define JSN_NUMBER_FORMAT "%li"
#else
#define JSN_NUMBER_FORMAT "%d"
#endif
#ifdef JSON_FLOATS
#define FLOAT_NUMBER_FORMAT "%.11f"
#endif

/* ------------------------------------------------------------------------ */
static int match_number(char **p, jsn_t *obj)
{
	char *s = *p;
	char *c = s;
	if (*c == '-')
		++c;
#ifndef JSON_HEX_NUMBERS
	if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X'))
		return 0;
#endif
	while ('0' <= *c && *c <= '9')
		++c;
#ifdef JSON_FLOATS
	if (*c == '.' || *c == 'E' || *c == 'e') {
		obj->data.floating = strtod(s, p);
		if (s == *p)
			return 0;
		return obj->type = JS_FLOAT;
	}
#endif
#ifdef JSON_64BITS_INTEGERS
	obj->data.number = strtoll(s, p, 0);
#else
	obj->data.number = strtol(s, p, 0);
#endif
	return *p == s ? 0 : (obj->type = JS_NUMBER);
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
			case '"': *d++ = '"'; continue;
			case '/': *d++ = '/'; continue;
			case '\\':*d++ = '\\'; continue;
			case 'b': *d++ = '\b'; continue;
			case 'f': *d++ = '\f'; continue;
			case 'n': *d++ = '\n'; continue;
			case 'r': *d++ = '\r'; continue;
			case 't': *d++ = '\t'; continue;
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
				} continue;
			default:
				if (!*s)
					goto _fail;
			}
		}
_not_escape:
		*d++ = *s;
	}
_fail:
	*d = 0;
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
			//case '/':
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

	for (++s; *s && *s != '"'; ++s) {
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
				--s;
				goto _not_escape;
			}
		} else
_not_escape:;
	}
	if (*s == '"') {
		*str = *p + 1;
		*p = s + 1;
		return 1;
	}
	return 0;
}


/* ------------------------------------------------------------------------ */
static int match_json(jsn_parser_t *p, jsn_t *obj)
{
	int open_char = after_space(&p->ptr);
	if (open_char != '[' && open_char != '{' ) {
		char *s = p->ptr;
		switch (open_char) {
		case '"':
			if (!match_string(&p->ptr, &obj->data.string))
				return errno = EINVAL, 0;
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
			if (!match_number(&p->ptr, obj))
				return errno = EINVAL, 0;
			return obj->type;
		}
	}

	p->ptr += 1;

	int is_object = open_char == '{';
	int close_char = is_object ? '}' : ']';

	int index = 0;
	short *next = &obj->data.object.first;

	if (match_char(&p->ptr, close_char))
		goto _empty;

	do {
		after_space(&p->ptr);
		jsn_t *node = jsn_alloc(p);
		if (!node)
			return 0;
		*next = (short)(node - obj);
		if (is_object) {
			if (!match_string(&p->ptr, &node->id.string))
				return errno = EINVAL, 0;
			node->id_type = JS_STRING;
			if (!match_char(&p->ptr, ':'))
				return errno = EINVAL, 0;
		} else {
			node->id.number = index;
			node->id_type = JS_NUMBER;
		}

		if (!match_json(p, node))
			return 0;

		next = &node->next;
		++index;
	} while (match_char(&p->ptr, ','));

	if (!match_char(&p->ptr,  close_char))
		return errno = EINVAL, 0;

_empty:
	*next = -1; // end of list
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

	if (!match_json(&p, jsn_alloc(&p)))
		return p.text - p.ptr; // return negative offset to error

	if (after_space(&p.ptr))
		return errno = EMSGSIZE, p.text - p.ptr;

	for (int i = 0; i < p.free_node_index; ++i) {
		jsn_t *node = pool + i;
		if (node->type == JS_STRING)
			string_unescape(node->data.string);
		if (node->id_type == JS_STRING)
			string_unescape(node->id.string);
	}

	return p.free_node_index; // return number of parsed js nodes (>0)
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
int json_boolean(jsn_t *node, int absent)
{
	if (!node)
		return absent;

	switch (node->type) {
	case JS_NULL:
		return 0;
	case JS_BOOLEAN:
	case JS_NUMBER:
		return node->data.number ? 1 : 0;
#ifdef JSON_FLOATS
	case JS_FLOAT:
		return (int)round(node->data.floating) ? 1 : 0;
#endif
	case JS_STRING:
		return node->data.string[0] ? 1 : 0;
	case JS_ARRAY:
	case JS_OBJECT:
		return 1;
	}
	return absent;
}


/* ------------------------------------------------------------------------ */
jsn_number_t json_number(jsn_t *node, jsn_number_t absent)
{
	if (!node)
		return absent;

	switch (node->type) {
	case JS_NULL:
		return 0;
	case JS_BOOLEAN:
	case JS_NUMBER:
		return node->data.number;
#ifdef JSON_FLOATS
	case JS_FLOAT:;
		double f = round(node->data.floating);
#ifdef JSON_64BITS_INTEGERS
		return (jsn_number_t)f;
#else
		return (jsn_number_t)(int64_t)f;
#endif
#endif
	case JS_STRING:;
		jsn_t num;
		char *s = node->data.string;
		if (!match_number(&s, &num))
			return 0;
		return json_number(&num, absent);
	case JS_ARRAY:
		if (node->data.object.length == 1)
			return json_number(node + node->data.object.first, absent);
	case JS_OBJECT:
		return 0;
	}
	return absent;
}


#ifdef JSON_FLOATS
double json_float(jsn_t *node, double absent)
{
	if (!node)
		return absent;

	switch (node->type) {
	case JS_NULL:
		return 0.d;
	case JS_BOOLEAN:
	case JS_NUMBER:
		return (double)node->data.number;
	case JS_FLOAT:;
		return node->data.floating;
	case JS_STRING:;
		jsn_t num;
		char *s = node->data.string;
		if (!match_number(&s, &num))
			return NAN;
		return json_float(&num, absent);
	case JS_ARRAY:
		if (node->data.object.length == 1)
			return json_float(node + node->data.object.first, absent);
	case JS_OBJECT:
		return 0.d;
	}
	return absent;
}

#endif

#ifdef JSON_64BITS_INTEGERS
#define NSB_LENGTH 16
#else
#define NSB_LENGTH 32
#endif
#define NSB_NUM    16 /* should be degree of 2 ( 2,4,8,16,32...) */

/* ------------------------------------------------------------------------ */
static char *get_number_string_buffer()
{
	static char bufs[NSB_NUM][NSB_LENGTH/* string length */];
	static int i = 0;
	return bufs[i++ & (NSB_NUM-1)];
}

#ifdef JSON_FLOATS
/* ------------------------------------------------------------------------ */
static char *float2str(char *p, char *e, double f)
{
	e = p + snprintf(p, (size_t)(e-p), FLOAT_NUMBER_FORMAT, f);
	while (e[-1] == '0' && (('0' <= e[-2] && e[-2] <= '9') || e[-2] == '.'))
		--e;
	*e = 0;
	return e;
}
#endif

/* ------------------------------------------------------------------------ */
char const *json_string(jsn_t *node, char const *absent)
{
	if (!node)
		return absent;

	switch (node->type) {
	case JS_NULL:
		return "null";

	case JS_BOOLEAN:
		return node->data.number ? "true" : "false";

	case JS_NUMBER: {
			char *buf = get_number_string_buffer();
			snprintf(buf, NSB_LENGTH, JSN_NUMBER_FORMAT, node->data.number);
			return buf;
		}

#ifdef JSON_FLOATS
	case JS_FLOAT: {
			char *buf = get_number_string_buffer();
			float2str(buf, buf + NSB_LENGTH, node->data.floating);
			return buf;
		}
#endif

	case JS_STRING:
		return node->data.string;

	case JS_ARRAY:
		return "[object Array]";
	case JS_OBJECT:
		return "[object Object]";
	}

	return absent;
}


/* ------------------------------------------------------------------------ */
static char *json_to_str(char *p, char *e, jsn_t *root)
{
	switch (root->type) {
	case JS_NULL:
		return p + snprintf(p, (size_t)(e-p), "null");
	case JS_BOOLEAN:
		return p + snprintf(p, (size_t)(e-p), root->data.number ? "true" : "false");
	case JS_NUMBER:
		return p + snprintf(p, (size_t)(e-p), JSN_NUMBER_FORMAT, root->data.number);
#ifdef JSON_FLOATS
	case JS_FLOAT:
		return float2str(p, e, root->data.floating);
#endif
	case JS_STRING:
		if (p < e) *p++ = '"';
		p = string_escape(p, e, root->data.string);
		if (p < e) *p++ = '"';
		break;
	case JS_OBJECT:
	case JS_ARRAY:;
		int is_object = root->type == JS_OBJECT;
		if (p < e) *p++ = is_object ? '{' : '[';
		json_foreach(root, index) {
			jsn_t *node = root + index;
			if (is_object) {
				if (p < e) *p++ = '"';
				p = string_escape(p, e, node->id.string);
				if (p < e) *p++ = '"';
				if (p < e) *p++ = ':';
			}
			p = json_to_str(p, e, node);
			if (node->next >= 0)
				if (p < e) *p++ = ',';
		}
		if (p < e) *p++ = is_object ? '}' : ']';
		break;
	}
	if (p < e)
		*p = 0;
	return p;
}


/* ------------------------------------------------------------------------ */
char *json_stringify(char *out, size_t size, jsn_t *root)
{
	*out = 0;
	if (root)
		json_to_str(out, out + size - 1, root);
	return out;
}
