
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdarg.h"
#include "string.h"
#include "errno.h"

#include "nano/json.h"

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

typedef
struct jsn_parser jsn_parser_t;

/* ------------------------------------------------------------------------ */
struct jsn_parser {
	char *text;       /* source text */
	char *ptr;        /* current parser position */

	jsn_t *pool;            /* array of json nodes */
	size_t free_node_index; /* index of first free node */
	size_t pool_size;       /* total array size */

	jsn_t *(* alloc)(jsn_parser_t *p);
};


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

/* ------------------------------------------------------------------------ */
int match_number(char **p, jsn_t *obj)
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
			return match_number(&p->ptr, obj) ?: ( errno = EINVAL, 0);
		}
	}

	p->ptr += 1;

	int is_object = open_char == '{';
	int close_char = is_object ? '}' : ']';

	int index = 0;
	obj->data.object.first = 0;
	int prev_index = obj - p->pool;
	int obj_index = obj - p->pool;

	if (match_char(&p->ptr, close_char))
		goto _empty;

	do {
		after_space(&p->ptr);
		jsn_t *node = p->alloc(p);
		if (!node)
			return 0;
		int node_index = (int)(node - p->pool);

		jsn_t *prev = p->pool + prev_index;
		if (obj_index != prev_index)
			prev->next = node_index - obj_index;
		else
			prev->data.object.first = node_index - obj_index;

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

		prev_index = node_index;
		++index;
	} while (match_char(&p->ptr, ','));

	if (!match_char(&p->ptr,  close_char))
		return errno = EINVAL, 0;

_empty:
	if (obj_index != prev_index)
		p->pool[prev_index].next = 0; // end of list
	obj = p->pool + obj_index;
	obj->data.object.length = index;
	return obj->type = (is_object ? JS_OBJECT : JS_ARRAY);
}


/* ------------------------------------------------------------------------ */
static int basic_parse(jsn_parser_t *p)
{
	if (!match_json(p, p->alloc(p)))
		return p->text - p->ptr; // return negative offset to error

	if (after_space(&p->ptr))
		return errno = EMSGSIZE, p->text - p->ptr;

	for (int i = 0; i < p->free_node_index; ++i) {
		jsn_t *node = p->pool + i;
		if (node->type == JS_STRING)
			string_unescape(node->data.string);
		if (node->id_type == JS_STRING)
			string_unescape(node->id.string);
	}

	return p->free_node_index; // return number of parsed js nodes (>0)
}


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
int json_parse(jsn_t *pool, size_t size, char *text)
{
	jsn_parser_t p = {
		.text = text,
		.ptr = text,
		.pool = pool,
		.free_node_index = 0,
		.pool_size = size,
		.alloc = jsn_alloc
	};

	return basic_parse(&p);
}


/* ------------------------------------------------------------------------ */
static jsn_t *jsn_realloc(jsn_parser_t *p)
{
	if (p->free_node_index >= p->pool_size) {
		jsn_t *pool = realloc(p->pool, sizeof(jsn_t) * (p->pool_size + 32));
		if (!pool)
			return errno = ENOMEM, NULL;
		p->pool = pool;
	}
	jsn_t *j = p->pool + p->free_node_index++;
	memset(j, 0, sizeof *j);
	return j;
}


/* ------------------------------------------------------------------------ */
static int jsn_free_tail(jsn_parser_t *p)
{
	if (p->free_node_index < p->pool_size) {
		jsn_t *pool = realloc(p->pool, sizeof(jsn_t) * p->free_node_index);
		if (!pool)
			return errno = ENOMEM, -1;
		p->pool = pool;
	}
	return 0;
}


#define START_POOL_SIZE (16)

/* ------------------------------------------------------------------------ */
jsn_t *json_auto_parse(char *text, char **end)
{
	jsn_parser_t p = {
		.text = text,
		.ptr = text,
		.free_node_index = 0,
		.pool_size = START_POOL_SIZE,
		.pool = malloc(START_POOL_SIZE * sizeof(jsn_t)),
		.alloc = jsn_realloc
	};

	int len = basic_parse(&p);
	if (end)
		*end = p.ptr;

	if (len <= 0) {
		free(p.pool);
		return NULL;
	} else
		jsn_free_tail(&p);

	return p.pool;
}

