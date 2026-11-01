#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "ucl.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size < 3) {
		return 0;
	}

	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE);
	if (parser == NULL) {
		return 0;
	}

	if (ucl_parser_add_chunk_full(parser, (const unsigned char *)data, size,
			0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK)) {
		ucl_object_t *obj = ucl_parser_get_object(parser);
		if (obj != NULL) {
			ucl_object_unref(obj);
		}
	}

	ucl_parser_free(parser);
	return 0;
}
