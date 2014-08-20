#include <stdio.h>
#include <stdlib.h>

#include <parserutils/parserutils.h>

#include "input/filter.h"

#include "testutils.h"

int main(int argc, char **argv)
{
	parserutils_filter *input;
	parserutils_filter_optparams params;
	parserutils_error expected;

#ifndef WITHOUT_ICONV_FILTER
	expected = PARSERUTILS_OK;
#else
	expected = PARSERUTILS_BADENCODING;
#endif

	UNUSED(argc);
	UNUSED(argv);

	assert(parserutils__filter_create("UTF-8", &input) == PARSERUTILS_OK);

	params.encoding.name = "GBK";
	assert(parserutils__filter_setopt(input, 
			PARSERUTILS_FILTER_SET_ENCODING, &params) == 
			expected);

	params.encoding.name = "GBK";
	assert(parserutils__filter_setopt(input, 
			PARSERUTILS_FILTER_SET_ENCODING, &params) == 
			expected);

	parserutils__filter_destroy(input);

	printf("PASS\n");

	return 0;
}
