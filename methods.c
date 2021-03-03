
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdarg.h"
#include "string.h"
#include "errno.h"

#include "nano/json.h"

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
int json_boolean(jsn_t *node, int absent)
{
	if (!node)
		return absent;

	switch (node->type) {
	case JS_UNDEFINED:
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
	case JS_UNDEFINED:
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
	case JS_UNDEFINED:
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
		if (!*s)
			return (double)0;
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
char *float2str(char *p, char *e, double f)
{
	e = p + snprintf(p, (size_t)(e-p), JSN_FLOAT_FORMAT, f);
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
	case JS_UNDEFINED:
		return "undefined";

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

