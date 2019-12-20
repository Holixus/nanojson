# nanojson
A small footprint(code and memory) simple JSON parsing C library for embedded projects

Supports 64 bits integers(optional) and hexadecimal("0x") integers(optional).


## Build

```
# cmake .
# make
```


### Build options

* `JSON_64BITS_INTEGERS` "Enable support of 64 bits integers"
* `JSON_HEX_NUMBERS` "Enabled support of 0x integers"
* `JSON_FLOATS` "Enable support of Floating point Numbers"


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
		       i, version, id, method, json_stringify(ser, sizeof ser, params));
		++i;
	}
	free(text);
	return 0;
}

```

## API

### Nodes types

```c
typedef
enum {
	JS_UNDEFINED, JS_NULL, JS_BOOLEAN, JS_NUMBER, JS_FLOAT, JS_STRING, JS_ARRAY, JS_OBJECT
} nj_type_t;
```


### Functions

#### `int json_parse(jsn_t *pool, size_t size, char *text)`

* `pool` -- pointer to allocated(somewhere) array of `jsn_t` elements
* `size` -- number of pool elements
* `text` -- JSON text source. Will be corrupted because all strings will be stored in this buffer.

##### Return value

The function return a number of used `jsn_t` elements of array pointed by `pool` argument.

On error, negative value is returned, and errno is set appropriately.

##### Errors

* `ENOMEM` passed array of `jsn_t` elements is not enough for store parsed JSON data tree.
* `EINVAL` impossible to parse passed JSON text. The returned negative value is offset to broked
place of JSON code and text buffer will not be corrupted by parsing.


##### Example
```c
	jsn_t json[100];
	int len = json_parse(json, sizeof json / sizeof json[0], text);
	if (len < 0) {
		perror("json_parse");
		...
	}
	...

```

#### `jsn_t *json_auto_parse(char *text, char **end)`

#### `int json_stringify(char *out, size_t size,/* <-- */ jsn_t *root)`


#### `jsn_t *json_item(jsn_t *node, char const *id)`
#### `jsn_t *json_cell(jsn_t *node, int index)`
#### `char const *json_string(jsn_t *node, char const *missed_value)`
#### `int json_boolean(jsn_t *node, int missed_value)`
#### `int json_number(jsn_t *node, int missed_value)`
#### `double json_float(jsn_t *node, double missed_value)`

#### `json_foreach`
#### ``

```c
	json_foreach(obj, offset) {
		jsn_t *node = obj + offset;
		...
	}
```

