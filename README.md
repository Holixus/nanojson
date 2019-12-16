# nanojson
A small footprint(code and memory) simple JSON parsing C library for embedded projects

## Usage examples

```c
static int test_gets()
{
	char ser[4096];
	char *text = strdup("\
[\
 {\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"capabilities\",\"params\":{}},\
 {\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"ololo\",\"params\":523424}\
]");

	printf("test_gets\n");

	jsn_t json[100];
	int len = json_parse(json, 100, text);
	if (len < 0) {
		perror("json_obj_scan parse");
		free(text);
		return 1;
	}

	printf("parsed %d nodes\n", len);

	int i = 0;
	json_foreach(json, index) {
		jsn_t *node = json + index;

		char const *version = json_string(json_item(node, "jsonrpc"), "0");
		char const *method  = json_string(json_item(node, "method"), NULL);
		int id              = json_number(json_item(node, "id"), -1);
		jsn_t *params       =             json_item(node, "params");

		printf("[%d]: version: %s, id: %d, method: %s, params: %s\n",
		       i, version, id, method, jsontoa(ser, sizeof ser, params));
		++i;
	}
	free(text);
	return 0;
}

```

## API

```c
typedef
enum {
	JS_UNDEFINED, JS_NULL, JS_BOOLEAN, JS_NUMBER, JS_STRING, JS_ARRAY, JS_OBJECT
} nj_type_t;


int json_parse(jsn_t *pool, size_t size, /* <-- */ char *text);

int json_type(jsn_t *node);

int is_json_object(jsn_t *node);
int is_json_array(jsn_t *node);

int json_length(jsn_t *node);

jsn_t *json_item(jsn_t *node, char const *id);
jsn_t *json_cell(jsn_t *node, int index);

int json_number(jsn_t *node, int missed_value);
char const *json_string(jsn_t *node, char const *missed_value);

/*
	json_foreach(obj, index) {
		jsn_t *node = obj + index;
		...
	}
*/

```

