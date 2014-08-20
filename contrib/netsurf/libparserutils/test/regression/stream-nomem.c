#include <stdio.h>
#include <string.h>

#include <parserutils/parserutils.h>
#include <parserutils/input/inputstream.h>

#include "utils/utils.h"

#include "testutils.h"

int main(int argc, char **argv)
{
	parserutils_inputstream *stream;

	/* This is specially calculated so that the inputstream is forced to 
	 * reallocate (it assumes that the inputstream's buffer chunk size 
	 * is 4k) */
#define BUFFER_SIZE (4096 + 4)
	uint8_t input_buffer[BUFFER_SIZE];
//	uint8_t *buffer;
//	size_t buflen;
	const uint8_t *c;
	size_t clen;

	UNUSED(argc);
	UNUSED(argv);

	/* Populate the buffer with something sane */
	memset(input_buffer, 'a', BUFFER_SIZE);
	/* Now, set up our test data */
	input_buffer[BUFFER_SIZE - 1] = '5';
	input_buffer[BUFFER_SIZE - 2] = '4';
	input_buffer[BUFFER_SIZE - 3] = '\xbd';
	input_buffer[BUFFER_SIZE - 4] = '\xbf';
	/* This byte will occupy the 4095th byte in the buffer and
	 * thus cause the entirety of U+FFFD to be buffered until after
	 * the buffer has been enlarged */
	input_buffer[BUFFER_SIZE - 5] = '\xef';
	input_buffer[BUFFER_SIZE - 6] = '3';
	input_buffer[BUFFER_SIZE - 7] = '2';
	input_buffer[BUFFER_SIZE - 8] = '1';

	assert(parserutils_inputstream_create("UTF-8", 0, 
			NULL, &stream) == PARSERUTILS_OK);

	assert(parserutils_inputstream_append(stream, 
			input_buffer, BUFFER_SIZE) == PARSERUTILS_OK);

	assert(parserutils_inputstream_append(stream, NULL, 0) == 
			PARSERUTILS_OK);

	while (parserutils_inputstream_peek(stream, 0, &c, &clen) != 
			PARSERUTILS_EOF)
		parserutils_inputstream_advance(stream, clen);

/*
	assert(css_inputstream_claim_buffer(stream, &buffer, &buflen) == 
			CSS_OK);

	assert(buflen == BUFFER_SIZE);

	printf("Buffer: '%.*s'\n", 8, buffer + (BUFFER_SIZE - 8));

	assert( buffer[BUFFER_SIZE - 6] == '3' && 
		buffer[BUFFER_SIZE - 5] == (uint8_t) '\xef' && 
		buffer[BUFFER_SIZE - 4] == (uint8_t) '\xbf' && 
		buffer[BUFFER_SIZE - 3] == (uint8_t) '\xbd' && 
		buffer[BUFFER_SIZE - 2] == '4');

	free(buffer);
*/

	parserutils_inputstream_destroy(stream);

	printf("PASS\n");

	return 0;
}

