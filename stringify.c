
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdarg.h"
#include "string.h"
#include "errno.h"

#include "nano/json.h"

#ifdef JSON_STRINGIFY_FN

/* ------------------------------------------------------------------------ */
char *string_escape(char *p, char *e, char const *s)
{
	static char const hex[] = "0123456789abcdef";
	while (p < e && *s) {
		unsigned int c = (unsigned char)*s++;
		if (c >= ' ' && c != '"' && c != '\\') {
			*p++ = c;
			continue;
		}
		*p++ = '\\';
		switch (c) {
		case '"':
		//case '/':
		case '\\': *p++ = c; continue;
		case '\b': *p++ = 'b'; continue;
		case '\f': *p++ = 'f'; continue;
		case '\n': *p++ = 'n'; continue;
		case '\r': *p++ = 'r'; continue;
		case '\t': *p++ = 't'; continue;
		}

		*p++ = 'u';
		unsigned int uc = c;
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
static char *json_to_str(char *p, char *e, jsn_t *root)
{
	switch (root->type) {
	case JS_UNDEFINED:
		return p + snprintf(p, (size_t)(e-p), "undefined");
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
			if (node->next > 0)
				if (p < e) *p++ = ',';
		}
		if (p < e) *p++ = is_object ? '}' : ']';
		break;
#ifdef DEBUG
	default:
		return p + snprintf(p, (size_t)(e-p), "<<<bad-type-%d>>>", root->type);
#endif
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

#endif /* JSON_STRINGIFY_FN */
