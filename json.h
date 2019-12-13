#ifndef NANO_JSON_H
#define NANO_JSON_H


typedef
enum {
	JS_UNDEFINED, JS_NULL, JS_BOOLEAN, JS_NUMBER, JS_STRING, JS_ARRAY, JS_OBJECT
} nj_type_t;

typedef
struct jsn {
	union {
		char *string;
		uint32_t number;
	} id;
	char id_type;
	char type;
	short next;
	union {
		int32_t number;
		char *string;
		struct {
			short nodes;
			short length;
		} object;
	} data;
} __attribute__((packed)) jsn_t;


/* internal but may be usefull functions */

int string_unescape(char *s);
char *string_escape(char *p, char *e, char const *s);


/* main functions */

int json_parse(jsn_t *pool, size_t size, /* <-- */ char *text);

char *json_stringify(char *outbuf, char *end, /* <-- */ jsn_t *root);


static inline int json_type(jsn_t *node) { return node->type; }
static inline int is_json_object(jsn_t *node) { return node->type == JS_OBJECT; }
static inline int is_json_array(jsn_t *node) { return node->type == JS_ARRAY; }

int json_length(jsn_t *node);

jsn_t *json_item(jsn_t *node, char const *id);
jsn_t *json_cell(jsn_t *node, int index);

int json_number(jsn_t *node, int absent);
char const *json_string(jsn_t *node, char const *absent);

#define json_foreach(obj, index) \
	for (int index = obj->data.object.nodes; index >= 0; index = obj[index].next)

/*
	json_foreach(obj, index) {
		jsn_t *node = obj + index;
		...
	}
*/

#endif
