
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "string.h"

#include "nano/json.h"


/* ------------------------------------------------------------------------ */
static char *fails[] = {
	"0b1"
	,"00xccf"
	,"\"sdfdsf\\\""
#ifndef JSON_HEX_NUMBERS
	,"0x5"
	,"0xccf"
#endif
#ifndef JSON_FLOATS
	,"1.1"
#endif
};

/* ------------------------------------------------------------------------ */
static int test_fail(char const *source)
{
	jsn_t json[100];
	char *text = strdup(source);
	int p = json_parse(json, 100, text);
	if (p <= 0) {
		free(text);
		//printf("    [OK]\n");
		return 1;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	json_stringify(result, length - 5, json);

	char src[length];
	string_escape(src, src + length - 5, source);
	printf("    <<<%s>>> -> <%s>[%d]\n but is should be FAILED\n", src, result, p);
	free(result);
	return 0;
}


/* ------------------------------------------------------------------------ */
static int test_auto_fail(char const *source)
{
	char *text = strdup(source);
	jsn_t *json = json_auto_parse(text, NULL);
	if (!json) {
		free(text);
		//printf("    [OK]\n");
		return 1;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	json_stringify(result, length - 5, json);

	char src[length];
	string_escape(src, src + length - 5, source);
	printf("    <<<%s>>> -> <%s>\n but is should be FAILED\n", src, result);
	free(result);
	free(json);
	return 0;
}


/* ------------------------------------------------------------------------ */
static char *good[] = {
	"  null  ", "null"
	," true   ", "true"
	,"false   ", "false"
	,"  \"as\\\\123dfg\"  ", "\"as\\\\123dfg\""
	," \"\\b\\f\\n\\r\\u0001\\t\"", "\"\\b\\f\\n\\r\\u0001\\t\""
	,"\"русские буквы\"", "\"русские буквы\""
//	,"\"\\u0080\\u0091\\u009a\\u009E\\u009f\"", "\"\\u0080\\u0091\\u009a\\u009e\\u009f\""
	,"\"werw\xbc\001erer\"", "\"werw\xbc\\u0001erer\""
	,"1", "1"
	,"-1", "-1"
#ifdef JSON_64BITS_INTEGERS
	,"100000000000", "100000000000"
#else
	,"100000000000", "1215752192"
#endif
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
	 "{\"precision\":\"zip\",\"Latitude\":37.371991,\"Longitude\":-122.02602,\"Address\":\"\",\"City\":\"SUNNYVALE\",\"State\":\"CA\",\"Zip\":\"94085\",\"Country\":\"US\"}]"
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
		printf("    <<<%s>>> [FAILED] // parsing %d\n", source, -p);
		return 0;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	json_stringify(result, length - 5, json);

	if (!strcmp(result, expected)) {
		//printf("    [OK]\n");
		free(result);
		return 1;
	} else {
		printf("    <<<%s>>> -> <%s>[%d]\n but expected <%s> [FAILED] // serializing\n", source, result, p, expected);
	}
	free(result);
	return 0;
}


/* ------------------------------------------------------------------------ */
static int test_auto_ok(char const *source, char const *expected)
{
	char *text = strdup(source), *end;
	jsn_t *json = json_auto_parse(text, &end);
	if (!json) {
		free(text);
		printf("    <<<%s>>> [FAILED] // parsing %d\n", source, (int)(end - text));
		return 0;
	}
	size_t length = strlen(source) * 3 + 10;
	char *result = malloc(length);
	json_stringify(result, length - 5, json);

	free(json);
	if (!strcmp(result, expected)) {
		//printf("    [OK]\n");
		free(result);
		return 1;
	} else {
		printf("    <<<%s>>> -> <%s>\n but expected <%s> [FAILED] // serializing\n", source, result, expected);
	}
	free(result);
	return 0;
}


/* ------------------------------------------------------------------------ */
static int test_json_parse()
{
	printf("  Test BROKEN samples\n");
	for (int i = 0, n = sizeof fails / sizeof fails[0]; i < n; i += 2) {
		if (!test_fail(fails[i]))
			return 0;
	}

	printf("  Test CORRECT samples\n");
	for (int i = 0, n = sizeof good / sizeof good[0]; i < n; i += 2) {
		if (!test_ok(good[i], good[i + 1]))
			return 0;
	}
	return 1;
}


/* ------------------------------------------------------------------------ */
static int test_json_auto_parse()
{
	printf("  Test BROKEN samples\n");
	for (int i = 0, n = sizeof fails / sizeof fails[0]; i < n; i += 2) {
		if (!test_auto_fail(fails[i]))
			return 0;
	}

	printf("  Test CORRECT samples\n");
	for (int i = 0, n = sizeof good / sizeof good[0]; i < n; i += 2) {
		if (!test_auto_ok(good[i], good[i + 1]))
			return 0;
	}
	return 1;
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
			i, version, id, method, json_stringify(ser, sizeof ser, params));
		++i;
	}
	free(text);
	return 0;
}


/* ------------------------------------------------------------------------ */
static int test_boolean()
{
	static const struct {
		char const *text;
		int number;
	} samples[] = {
		 { "0", 0 }
		,{ "1", 1 }
		,{ "555", 1 }
		,{ "null", 0 }
		,{ "true", 1 }
		,{ "false", 0 }
		,{ "\"\"", 0 }
		,{ "\"123\"", 1 }
#ifdef JSON_64BITS_INTEGERS
		,{ "100000000000", 1 }
#else
		,{ "4294967296", 0 }
		,{ "100000000000", 1 }
#endif
#ifdef JSON_HEX_NUMBERS
		,{ "\"0x123\"", 1 }
#endif
		,{ "[]", 1 }
		,{ "[1]", 1 }
		,{ "{}", 1 }
	};

	for (int i = 0, n = sizeof samples / sizeof samples[0]; i < n; ++i) {
		char text[256];
		char const *source = samples[i].text;
		jsn_t json[5];
		strcpy(text, source);
		int p = json_parse(json, sizeof json / sizeof json[0], text);
		if (p <= 0)
			return printf("<<<%s>>> [FAILED] // parsing %d\n", source, -p), 0;

		int result = json_boolean(json, 0);
		int expected = samples[i].number;
		if (result == expected) {
			//printf("[OK]\n");
			continue;
		}

		printf("<<<%s>>> -> %d but expected %d [FAILED] // json_number\n", source, result, expected);
		return 0;
	}

	return 1;
}


/* ------------------------------------------------------------------------ */
static int test_number()
{
	static const struct {
		char const *text;
		jsn_number_t number;
	} samples[] = {
		 { "0", 0 }
		,{ "1", 1 }
		,{ "555", 555 }
		,{ "null", 0 }
		,{ "true", 1 }
		,{ "false", 0 }
		,{ "\"\"", 0 }
		,{ "\"123\"", 123 }
#ifdef JSON_64BITS_INTEGERS
		,{ "100000000000", 100000000000 }
#else
		,{ "4294967296", 0 }
		,{ "100000000000", 1215752192 }
#endif
#ifdef JSON_HEX_NUMBERS
		,{ "\"0x123\"", 291 }
#endif
#ifdef JSON_FLOATS
		,{ "\"1.1\"", 1 }
		,{ "1e4", 10000 }
#endif
		,{ "[]", 0 }
		,{ "[1]", 1 }
		,{ "{}", 0 }
	};

	for (int i = 0, n = sizeof samples / sizeof samples[0]; i < n; ++i) {
		char text[256];
		char const *source = samples[i].text;
		jsn_t json[5];
		strcpy(text, source);
		int p = json_parse(json, sizeof json / sizeof json[0], text);
		if (p <= 0)
			return printf("<<<%s>>> [FAILED] // parsing %d\n", source, -p), 0;

		int result = json_number(json, 0);
		int expected = samples[i].number;
		if (result == expected) {
			//printf("[OK]\n");
			continue;
		}

		printf("<<<%s>>> -> %d but expected %d [FAILED] // json_number\n", source, result, expected);
		return 0;
	}

	return 1;
}


#ifdef JSON_FLOATS
/* ------------------------------------------------------------------------ */
static int test_float()
{
	static const struct {
		char const *text;
		double floating;
	} samples[] = {
		 { "0", 0.d }
		,{ "1", 1.d }
		,{ "555", 555.d }
		,{ "null", 0.d }
		,{ "true", 1.d }
		,{ "false", 0.d }
		,{ "\"\"", 0.d }
		,{ "\"123\"", 123.d }
#ifdef JSON_64BITS_INTEGERS
		,{ "100000000000", 100000000000.d }
#else
		,{ "100000000000", 1215752192.d }
#endif
		,{ "1e4", 1e4 }
#ifdef JSON_HEX_NUMBERS
		,{ "\"0x123\"", 291.d }
#endif
		,{ "[]", 0.d }
		,{ "[1]", 1.d }
		,{ "{}", 0.d }
	};

	for (int i = 0, n = sizeof samples / sizeof samples[0]; i < n; ++i) {
		char text[256];
		char const *source = samples[i].text;
		jsn_t json[5];
		strcpy(text, source);
		int p = json_parse(json, sizeof json / sizeof json[0], text);
		if (p <= 0)
			return printf("<<<%s>>> [FAILED] // parsing %d\n", source, -p), 0;

		double result = json_float(json, 0.d);
		double expected = samples[i].floating;
		if (result == expected) {
			//printf("[OK]\n");
			continue;
		}

		printf("<<<%s>>> -> %f but expected %f [FAILED] // json_float\n", source, result, expected);
		return 0;
	}

	return 1;
}
#endif


/* ------------------------------------------------------------------------ */
static int test_string()
{
	static const struct {
		char const *text;
		char const *string;
	} samples[] = {
		 { "0", "0" }
		,{ "1", "1" }
		,{ "555", "555" }
		,{ "null", "null" }
		,{ "true", "true" }
		,{ "false", "false" }
		,{ "\"\"", "" }
		,{ "\"123\"", "123" }
		,{ "\"1\\23\"", "123" }
#ifdef JSON_64BITS_INTEGERS
		,{ "100000000000", "100000000000" }
#else
		,{ "4294967296",   "0" }
		,{ "100000000000", "1215752192" }
#endif
#ifdef JSON_HEX_NUMBERS
		,{ "0x123", "291" }
		,{ "\"0x123\"", "0x123" }
#endif
#ifdef JSON_FLOATS
		,{ "1.1", "1.1" }
#endif
		,{ "[]",  "[object Array]" }
		,{ "[1]", "[object Array]" }
		,{ "{}",  "[object Object]" }
	};

	for (int i = 0, n = sizeof samples / sizeof samples[0]; i < n; ++i) {
		char text[256];
		char const *source = samples[i].text;
		jsn_t json[5];
		strcpy(text, source);
		int p = json_parse(json, sizeof json / sizeof json[0], text);
		if (p <= 0)
			return printf("<<<%s>>> [FAILED] // parsing %d\n", source, -p), 0;

		char const *result = json_string(json, 0);
		char const *expected = samples[i].string;
		if (!strcmp(result, expected)) {
			//printf("[OK]\n");
			continue;
		}

		printf("<<<%s>>> -> \"%s\" but expected \"%s\" [FAILED] // json_string\n", source, result, expected);
		return 0;
	}

	return 1;
}


/* ------------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
	printf("sizeof jsn_t: %u\n", (unsigned int)sizeof (jsn_t));

	printf("Test json_parse()\n");
	if (!test_json_parse())
		return -1;

	printf("Test json_auto_parse()\n");
	if (!test_json_auto_parse())
		return -1;

	printf("Test json_boolean()\n");
	if (!test_boolean())
		return -1;

	printf("Test json_number()\n");
	if (!test_number())
		return -1;

#ifdef JSON_FLOATS
	printf("Test json_float()\n");
	if (!test_float())
		return -1;
#endif

	printf("Test json_string()\n");
	if (!test_string())
		return -1;

/*
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
	puts(json_stringify(text, sizeof text, json));*/
	return 0;
}
