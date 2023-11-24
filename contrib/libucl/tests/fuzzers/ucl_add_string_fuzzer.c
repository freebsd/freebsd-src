#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "ucl.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// If size is 0 we need a null-terminated string.
	// We dont null-terminate the string and by the design
	// of the API passing 0 as size with non null-terminated string
	// gives undefined behavior. 	
	if(size==0){
		return 0;
	}
	struct ucl_parser *parser;
       	parser = ucl_parser_new(0);
	
	ucl_parser_add_string(parser, (char *)data, size);
	
	if (ucl_parser_get_error(parser) != NULL) {
		return 0;
	}

	ucl_parser_free (parser);
        return 0;
}
