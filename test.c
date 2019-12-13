
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "string.h"

#include "json.h"


/* ------------------------------------------------------------------------ */
static char *jsontoa(char *a, size_t size, jsn_t *root)
{
	*a = 0;
	if (root)
		jsontostr(a, a + size, root);
	return a;
}

/* ------------------------------------------------------------------------ * /
static char *fails[] = {
	""
};*/

/* ------------------------------------------------------------------------ */
static char *good[] = {
	"  null  ", "null",
	" true   ", "true",
	"false   ", "false",
	"  \"as123dfg\"  ", "\"as123dfg\"",
	" \"\\b\\f\\n\\r\\u0001\\t\"", "\"\\b\\f\\n\\r\\u0001\\t\"",
	"1", "1",
	"-1", "-1",
	" [ ] ", "[]",
	"{} ", "{}",
	" [ { } ]", "[{}]",
	"{\"t\":[]}", "{\"t\":[]}",
	"12312312", "12312312",
	"[1,\n2, -555 ,true,false,null]", "[1,2,-555,true,false,null]",
	" { \"b\" : 1 } ", "{\"b\":1}",
	"[{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"capabilities\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"capabilities\",\"params\":{}}]",
	  "[{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"capabilities\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"capabilities\",\"params\":{}}]"
};

/* ------------------------------------------------------------------------ */
static int test_ok(char const *source, char const *expected)
{
	jsn_t json[100];
	memset(json, 0, sizeof json);
	char *text = strdup(source);
	int p = json_parse(text, json, 100);
	if (p < 0) {
		free(text);
		printf("<<<%s>>> [FAILED] // parsing %d\n", source, -p);
		return 0;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	jsontoa(result, length - 5, json);

	if (!strcmp(result, expected)) {
		printf("[OK]\n");
		free(result);
		return 1;
	} else {
		char src[length];
		//char res[strlen(result) * 3 + 10];
		//char str[strlen(result) * 3 + 10];
		string_escape(src, src + length - 5, source);
		//string_escape(res, res + sizeof res - 5, result);
		//string_escape(str, str + sizeof str - 5, expected);
		printf("<<<%s>>> -> <%s>[%d]\n but expected <%s> [FAILED] // serializing\n", src, result, p, expected);
	}
	free(result);
	return 0;
}

/* ------------------------------------------------------------------------ */
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
	int len = json_parse(text, json, 100);
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

/* ------------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
	for (int i = 0, n = sizeof good / sizeof good[0]; i < n; i += 2) {
		if (!test_ok(good[i], good[i + 1]))
			return -1;
	}

	test_gets();

	jsn_t json[100];
	if (json_parse(argv[1], json, 100) < 0) {
		perror("json parse");
		return 1;
	}
	printf("ok\n");
	char text[4096];
	jsontostr(text, text + sizeof text, json);
	puts(text);
	return 0;
}
