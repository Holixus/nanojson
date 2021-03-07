# nanojson
A small footprint(code and memory) simple [JSON](https://www.rfc-editor.org/rfc/rfc8259.html) parsing C library for embedded projects

* 64 bits integers(optional)
* Hexadecimal("0x") integers(optional)
* Good tests coverage
* Buildable without float/double support
* Ability to parse JSON text without malloc usage

# Build

```
# cmake -DJSON_64BITS_INTEGERS=OFF -DJSON_HEX_NUMBERS=OFF -DJSON_FLOATS=OFF.
# make
```

## Build options

* `BUILD_SHARED_LIBRARY`(OFF) -- Build shared library (not only static)
* `JSON_64BITS_INTEGERS`(OFF) -- Enable support of 64 bits integers
* `JSON_HEX_NUMBERS`(OFF) -- Enabled support of 0x integers
* `JSON_FLOATS`(OFF) -- Enable support of Floating point Numbers
* `JSON_SHORT_NEXT`(OFF) -- Use `short` type for next field of jsn_t
* `JSON_PACKED`(OFF) -- Use packed json item structure

* `JSON_AUTO_PARSE_FN`(ON) -- Build json_auto_parse() function
  * `JSON_AUTO_PARSE_POOL_START_SIZE`(32) -- Initial jsn_t array size
  * `JSON_AUTO_PARSE_POOL_INCREASE`(n+32) -- Increase jsn_t array size formula

* `JSON_STRINGIFY_FN`(ON) -- Build json_stringify() function

* `BUILD_TESTS`(ON) -- Build tests application

# Include files

```c
#include "nano/json.h"
```

# Nodes types

```c
typedef
enum {
	JS_UNDEFINED=1, JS_NULL, JS_BOOLEAN, JS_NUMBER, JS_FLOAT, JS_STRING, JS_ARRAY, JS_OBJECT
} nj_type_t;
```


# Functions

## `int json_parse(jsn_t *pool, size_t size, char *text)`

* `pool` -- pointer to (somewhere allocated) array of `jsn_t` elements
* `size` -- number of pool elements
* `text` -- JSON text source. Will be corrupted because all strings will be stored in this buffer.

Parse JSON text to array of nodes. The first node is always root node. If parsing made succesfully
then source text memory will be used for storing of values identifiers and strings.
Please, don't forget this.


### Return value

The function return a number of used `jsn_t` elements of array pointed by `pool` argument.

On error, negative value is returned, and errno is set appropriately.

### Errors

* `ENOMEM` passed array of `jsn_t` elements is not enough for store parsed JSON data tree.
* `EINVAL` impossible to parse passed JSON text. The returned negative value is offset to broken
place of JSON code and text buffer will not be corrupted by parsing.


### Example
```c
	jsn_t json[100];
	int len = json_parse(json, sizeof json / sizeof json[0], text);
	if (len < 0) {
		perror("json_parse");
		...
	}
	...

```



## `jsn_t *json_auto_parse(char *text, char **end)`

* `text` -- JSON text source. Will be corrupted because all strings will be stored in this buffer.
* `end` -- in case of parsing error by the .

Parse JSON `text`.

### Return value

The function return a pointer to array `jsn_t` elements. Should be released by `free()`.

On error, NULL is returned, and errno is set appropriately.

### Errors

* `ENOMEM` Out of memory.
* `EINVAL` impossible to parse passed JSON text. If `end` is not NULL, a pointer to broken
JSON code will be stored in the pointer referenced by `end`. The text buffer will not be corrupted by parser.


### Example
```c
	char *err_pos;
	jsn_t *json = json_auto_parse(text, &err_pos);
	if (!json) {
		if (errno == EINVAL) {
			char crop[50];
			snprintf(crop, sizeof crop, "%s", err_pos);
			perror("json_auto_parse: JSON syntax error at the code '%s'", crop);
		} else
			perror("json_auto_parse");
		return -1;
	}
	...
	free(json);
```



## `char *json_stringify(char *out, size_t size, jsn_t *root)`

* `outbuf` -- output buffer for JSON text
* `size` -- size of output buffer including terminating zero
* `root` -- root element of json tree

Convert parsed JSON tree back to text. May be useful for debugging purpose.

### Return value

Return pointer to output buffer.

### Example

```c
	int source_len = strlen(source);
	jsn_t *json = json_auto_parse(source, NULL);
	if (!json)
		return -1;

	char string[source_len * 2];
	json_stringify(string, sizeof string, json);

	free(json);

	printf("JSON: %s\n", string);
	return 0;
```


## `jsn_t *json_item(jsn_t *node, char const *id)`

* `node` -- object json node to search element
* `id` -- string identifier of object element

Returns element of object with `id` or NULL if absent.


## `jsn_t *json_cell(jsn_t *node, int index)`

* `node` -- array json node to search element
* `index` -- index of array element

Returns element of array with `index` or NULL if absent.



## `char const *json_string(jsn_t *node, char const *missed_value)`

* `node` -- pointer to json node
* `missed_value` -- default value if node is undefined (NULL)

Returns pointer to string value of node. For non JS_STRING node will be casted
to static buffer string.

If node is NULL returns `missed_value`.



## `int json_boolean(jsn_t *node, int missed_value)`

* `node` -- pointer to json node
* `missed_value` -- default value if node is undefined (NULL)

Returns boolean(1 or 0) value of node. Non JS_BOOLEAN node will be casted 
to boolean by JS type convertation rules.

If node is NULL returns `missed_value`.


## `int json_number(jsn_t *node, int missed_value)`

* `node` -- pointer to json node
* `missed_value` -- default value if node is undefined (NULL)

Returns integer value of node. Non JS_NUMBER node will be casted 
by JS type convertation rules.

If node is NULL returns `missed_value`.


## `double json_float(jsn_t *node, double missed_value)`

* `node` -- pointer to json node
* `missed_value` -- default value if node is undefined (NULL)

Returns floating pointer(double) value of node. Non JS_FLOAT node will be casted 
by JS type convertation rules.


If node is NULL returns `missed_value`.


## `json_foreach`

For enumerating of child nodes of JS_OBJECT/JS_ARRAY object you can use `json_foreach` macro-definition.


```c
	jsn_t *obj = json_auto_parse("[1,2,3,4,5]");
	json_foreach(obj, offset) {
		jsn_t *node = obj + offset;
		printf("obj[%d] = '%s'\n", node->id.index, json_string(node));
	}
	free(obj);
```


# Big code example

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

	json_foreach(json, offset) {
		jsn_t *node = json + offset;

		char const *version = json_string(json_item(node, "jsonrpc"), "0");
		char const *method  = json_string(json_item(node, "method"), NULL);
		int id              = json_number(json_item(node, "id"), -1);
		jsn_t *params       =             json_item(node, "params");

		printf("[%d]: version: %s, id: %d, method: %s, params: %s\n",
		       node->id.index, version, id, method, json_stringify(ser, sizeof ser, params));
	}
	free(text);
	return 0;
}

```

