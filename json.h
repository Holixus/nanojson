#ifndef NANO_JSON_H
#define NANO_JSON_H


typedef
enum { JS_FAIL, JS_NULL, JS_BOOLEAN, JS_NUMBER, JS_STRING, JS_ARRAY, JS_OBJECT } nj_type_t;

typedef struct nj_node jsn_t;

struct nj_node {
	union {
		char *string;
		uint32_t index;
	} id;
	char has_string_id;
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
} __attribute__((packed));


int string_unescape(char *s);
char *string_escape(char *p, char *e, char const *s);

int json_parse(char *text, jsn_t *pool, size_t size);

char *jsontostr(char *p, char *e, jsn_t *root);


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
		//jsn_t *node = obj + index;

#endif
