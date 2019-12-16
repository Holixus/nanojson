
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
		json_stringify(a, a + size, root);
	return a;
}

/* ------------------------------------------------------------------------ */
static char *fails[] = {
#ifndef JSON_HEX_NUMBERS
	"0x5"
	,"0xccf",
#endif
	"0b1"
	,"\"sdfdsf\\\""
};

/* ------------------------------------------------------------------------ */
static int test_fail(char const *source)
{
	jsn_t json[100];
	char *text = strdup(source);
	int p = json_parse(json, 100, text);
	if (p <= 0) {
		free(text);
		//printf("[OK]\n");
		return 1;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	jsontoa(result, length - 5, json);

	char src[length];
	string_escape(src, src + length - 5, source);
	printf("<<<%s>>> -> <%s>[%d]\n but is should be FAILED\n", src, result, p);
	free(result);
	return 0;
}


/* ------------------------------------------------------------------------ */
static char *good[] = {
	"  null  ", "null"
	," true   ", "true"
	,"false   ", "false"
	,"  \"as123dfg\"  ", "\"as123dfg\""
	," \"\\b\\f\\n\\r\\u0001\\t\"", "\"\\b\\f\\n\\r\\u0001\\t\""
	,"1", "1"
	,"-1", "-1"
#ifdef JSON_HEX_NUMBERS
	,"0x40", "64"
	,"-0x100", "-256"
#endif
	," [ ] ", "[]"
	,"{} ", "{}"
	," [ { } ]", "[{}]"
	,"{\"t\":[]}", "{\"t\":[]}"
	,"12312312", "12312312"
	,"[1,\n2, -555 ,true,false,null]", "[1,2,-555,true,false,null]"
	," { \"b\" : 1 } ", "{\"b\":1}"
	,"[{ \"jsonrpc\": \"2.0\",\"id\":\"0\",\"method\":\"capabilities\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"capabilities\",\"params\":{}}]"
	,"[{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"capabilities\",\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"capabilities\",\"params\":{}}]"

	,"{\n\
      \"Image\": {\n\
          \"Width\":  800,\n\
          \"Height\": 600,\n\
          \"Title\":  \"View from 15th Floor\",\n\
          \"Thumbnail\": {\n\
              \"Url\":    \"http://www.example.com/image/481989943\",\n\
              \"Height\": 125,\n\
              \"Width\":  \"100\"\n\
          },\n\
          \"IDs\": [116, 943, 234, 38793]\n\
        }\n\
   }\n"
	,"{\"Image\":{\"Width\":800,\"Height\":600,\"Title\":\"View from 15th Floor\",\"Thumbnail\":"
	   "{\"Url\":\"http://www.example.com/image/481989943\",\"Height\":125,\"Width\":\"100\"},\"IDs\":[116,943,234,38793]}}"

#ifdef JSON_FLOATS
,"   [\n\
      {\n\
         \"precision\": \"zip\",\n\
         \"Latitude\":  37.7668,\n\
         \"Longitude\": -122.3959,\n\
         \"Address\":   \"\",\n\
         \"City\":      \"SAN FRANCISCO\",\n\
         \"State\":     \"CA\",\n\
         \"Zip\":       \"94107\",\n\
         \"Country\":   \"US\"\n\
      },\n\
      {\n\
         \"precision\": \"zip\",\n\
         \"Latitude\":  37.371991,\n\
         \"Longitude\": -122.026020,\n\
         \"Address\":   \"\",\n\
         \"City\":      \"SUNNYVALE\",\n\
         \"State\":     \"CA\",\n\
         \"Zip\":       \"94085\",\n\
         \"Country\":   \"US\"\n\
      }\n\
   ]\n"
	,"[{\"precision\":\"zip\",\"Latitude\":37.7668,\"Longitude\":-122.3959,\"Address\":\"\",\"City\":\"SAN FRANCISCO\",\"State\":\"CA\",\"Zip\":\"94107\",\"Country\":\"US\"},"
	 "{\"precision\":\"zip\",\"Latitude\":37.371991,\"Longitude\":-122.026020,\"Address\":\"\",\"City\":\"SUNNYVALE\",\"State\":\"CA\",\"Zip\":\"94085\",\"Country\":\"US\"}]"
#endif
};

/* ------------------------------------------------------------------------ */
static int test_ok(char const *source, char const *expected)
{
	jsn_t json[100];
	char *text = strdup(source);
	int p = json_parse(json, 100, text);
	if (p <= 0) {
		free(text);
		printf("<<<%s>>> [FAILED] // parsing %d\n", source, -p);
		return 0;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	jsontoa(result, length - 5, json);

	if (!strcmp(result, expected)) {
		//printf("[OK]\n");
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

/* ------------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
	printf("Test BROKEN samples\n");
	for (int i = 0, n = sizeof fails / sizeof fails[0]; i < n; i += 2) {
		if (!test_fail(fails[i]))
			return -1;
	}

	printf("Test CORRECT samples\n");
	for (int i = 0, n = sizeof good / sizeof good[0]; i < n; i += 2) {
		if (!test_ok(good[i], good[i + 1]))
			return -1;
	}

	test_gets();

	if (argc < 2)
		return 0;

	jsn_t json[100];
	if (json_parse(json, 100, argv[1]) < 0) {
		perror("json parse");
		return 1;
	}
	printf("ok\n");
	char text[4096];
	json_stringify(text, text + sizeof text, json);
	puts(text);
	return 0;
}
