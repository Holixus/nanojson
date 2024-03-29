
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "limits.h"
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
static int is_alpha_char(int c)
{
	return c <= 'Z' ? 'A' <= c : 'a' <= c && c <= 'z';
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
#if LLONG_MAX == INT64_MAX
	obj->data.number = strtoll(s, p, 0);
#else
#if LONG_MAX == INT64_MAX
	obj->data.number = strtol(s, p, 0);
#else
	#error "impossible to choose of 64 bits strtol function"
#endif
#endif
#else
#if LLONG_MAX == INT32_MAX
	obj->data.number = strtoll(s, p, 0);
#else
#if LONG_MAX == INT32_MAX
	obj->data.number = strtol(s, p, 0);
#else
#if LONG_MAX > INT32_MAX
	long l = strtol(s, p, 0);
	if (l > INT32_MAX) l = INT32_MAX;
	else if (l < INT32_MIN) l = INT32_MIN;
	obj->data.number = l;
#else
	#error "impossible to choose of 32 bits strtol function"
#endif
#endif
#endif
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
int string_unescape(char *d, char *s)
{
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

	if (match_char(&p->ptr, close_char))
		goto _empty;

	int prev_ofs = obj - p->pool;
	int obj_ofs = obj - p->pool;
	do {
		jsn_t *node = p->alloc(p);
		if (!node)
			return 0;

		int node_ofs = (int)(node - p->pool);
		if (obj_ofs != prev_ofs)
			p->pool[prev_ofs].next = node_ofs - obj_ofs;
		prev_ofs = node_ofs;

		after_space(&p->ptr);
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

		++index;
	} while (match_char(&p->ptr, ','));

	if (!match_char(&p->ptr,  close_char))
		return errno = EINVAL, 0;

	obj = p->pool + obj_ofs;

_empty:
	obj->data.length = index;
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
			string_unescape(node->data.string, node->data.string);
		if (node->id_type == JS_STRING)
			string_unescape(node->id.string, node->id.string);
	}

	return p->free_node_index; // return number of parsed js nodes (>0)
}


/* ------------------------------------------------------------------------ */
static jsn_t *jsn_alloc(jsn_parser_t *p)
{
	if (p->free_node_index >= p->pool_size)
		return errno = ENOMEM, NULL;
	jsn_t *j = p->pool + p->free_node_index++;
	j->next = 0;
	j->type = 0;
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

#ifdef JSON_AUTO_PARSE_FN

/* ------------------------------------------------------------------------ */
static jsn_t *jsn_realloc(jsn_parser_t *p)
{
	if (p->free_node_index >= p->pool_size) {
		jsn_t *pool = realloc(p->pool, sizeof(jsn_t) * JSON_AUTO_PARSE_POOL_INCREASE(p->pool_size));
		if (!pool)
			return NULL;
		p->pool = pool;
		p->pool_size = JSON_AUTO_PARSE_POOL_INCREASE(p->pool_size);
	}
	return jsn_alloc(p);
}


/* ------------------------------------------------------------------------ */
static int jsn_free_tail(jsn_parser_t *p)
{
	if (p->free_node_index < p->pool_size) {
		jsn_t *pool = realloc(p->pool, sizeof(jsn_t) * p->free_node_index);
		if (!pool)
			return -1;
		p->pool = pool;
	}
	return 0;
}


/* ------------------------------------------------------------------------ */
jsn_t *json_auto_parse(char *text, char **end)
{
	jsn_parser_t p = {
		.text = text,
		.ptr = text,
		.free_node_index = 0,
		.pool_size = JSON_AUTO_PARSE_POOL_START_SIZE,
		.pool = malloc(JSON_AUTO_PARSE_POOL_START_SIZE * sizeof(jsn_t)),
		.alloc = jsn_realloc
	};

	if (!p.pool)
		return NULL;

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

#endif /* JSON_AUTO_PARSE */

#ifdef JSON_GET_FN

static int match_id(char **p, char *id)
{
	if (!is_alpha_char(**p))
		return 0;

	char *s = *p;
	do {
		++s;
	} while (is_id_char(*s));

	size_t len = (unsigned)(s - *p);
	if (len > JSON_MAX_ID_LENGTH)
		return 0;

	memcpy(id, *p, len);
	id[len] = 0;
	*p = s;
	return 1;
}

static int match_inum(char **p, int *num)
{
	if (**p < '0' || '9' < **p)
		return 0;

	int v = 0;
	char *s = *p;
	do {
		v = 10 * v + (*s - '0');
		++s;
	} while (is_id_char(*s));
	*num = v;
	*p = s;
	return 1;
}

/* ------------------------------------------------------------------------ */
jsn_t *json_get(jsn_t *obj, char const *path)
{
/*
ID       [a-zA-Z] [a-zA-Z0-9_]*
INUM     [0-9]+
INDEX    '[' ( INUM | string ) ']'
path     ( '.' ID | INDEX ) )*
*/
	char *p = (char *)path;

	after_space(&p);
	while (*p && obj) {
		switch (*p) {
		case '.': {
				char id[JSON_MAX_ID_LENGTH];
				++p;
				if (!match_id(&p, id))
					return errno = EINVAL, NULL;

				obj = json_item(obj, id);
				continue;
			}
		case '[': {
				int i;
				++p;
				if (match_inum(&p, &i)) {
					if (!match_char(&p, ']'))
						return errno = EINVAL, NULL;

					obj = json_cell(obj, i);
					continue;
				} else {
					char *s;
					if (match_string(&p, &s)) {
						if (!match_char(&p, ']'))
							return errno = EINVAL, NULL;

						char *str = (char *)malloc(p - s);
						string_unescape(str, s);
						obj = json_item(obj, str);
						free(str);
						continue;
					}
				}
				return errno = EINVAL, NULL;
			}
		default:
			return errno = EINVAL, NULL;
		}
		after_space(&p);
	}

	return obj;
}

#endif /* JSON_GET_FN */
