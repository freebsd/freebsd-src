#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "ucl.h"
#include "ucl_internal.h"
#include <ctype.h>

typedef ucl_object_t* (*ucl_msgpack_test)(void);


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	
	if(size<3){
		return 0;
	}

	struct ucl_parser *parser;
	
	ucl_object_t *obj = ucl_object_new_full (UCL_OBJECT, 2);
	obj->type = UCL_OBJECT;

	parser = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE);
	parser->stack = NULL;

	bool res = ucl_parser_add_chunk_full(parser, (const unsigned char*)data, size, 0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);

	ucl_parser_free (parser);
	return 0;
}
