#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/parserutils.h>

#include "utils/utils.h"

#include "input/filter.h"

#include "testutils.h"

int main(int argc, char **argv)
{
	parserutils_filter_optparams params;
	parserutils_filter *input;
	uint8_t inbuf[64], outbuf[64];
	size_t inlen, outlen;
	const uint8_t *in = inbuf;
	uint8_t *out = outbuf;

	UNUSED(argc);
	UNUSED(argv);

	/* Create input filter */
	assert(parserutils__filter_create("UTF-8", &input) == PARSERUTILS_OK);

	/* Convert filter to UTF-8 encoding */
	params.encoding.name = "UTF-8";
	assert(parserutils__filter_setopt(input, PARSERUTILS_FILTER_SET_ENCODING,
			(parserutils_filter_optparams *) &params) == 
			PARSERUTILS_OK);


	/* Simple case - valid input & output buffer large enough */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xc2\xa0o!");
	inlen = strlen((const char *) inbuf);
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xc2\xa0o!",
			SLEN("hell\xc2\xa0o!")) == 0);


	/* Too small an output buffer; no encoding edge cases */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hello!");
	inlen = strlen((const char *) inbuf);
	outbuf[0] = '\0';
	outlen = 5;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_NOMEM);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	outlen = 64 - 5 + outlen;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hello!",
			SLEN("hello!")) == 0);


	/* Illegal input sequence; output buffer large enough */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\x96o!");
	inlen = strlen((const char *) inbuf);
	outbuf[0] = '\0';
	outlen = 64;

	/* Input does loose decoding, converting to U+FFFD if illegal
	 * input is encountered */
	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xef\xbf\xbdo!",
			SLEN("hell\xef\xbf\xbdo!")) == 0);


	/* Input ends mid-sequence */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xc2\xa0o!");
	inlen = strlen((const char *) inbuf) - 3;
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 3;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xc2\xa0o!",
			SLEN("hell\xc2\xa0o!")) == 0);


	/* Input ends mid-sequence, but second attempt has too small a
	 * buffer, but large enough to write out the incomplete character. */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xc2\xa0o!");
	inlen = strlen((const char *) inbuf) - 3;
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 3;
	outlen = 3;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_NOMEM);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	outlen = 64 - 7;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xc2\xa0o!",
			SLEN("hell\xc2\xa0o!")) == 0);


	/* Input ends mid-sequence, but second attempt has too small a
	 * buffer, not large enough to write out the incomplete character. */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xc2\xa0o!");
	inlen = strlen((const char *) inbuf) - 3;
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 3;
	outlen = 1;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_NOMEM);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	outlen = 60;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xc2\xa0o!",
			SLEN("hell\xc2\xa0o!")) == 0);


	/* Input ends mid-sequence, but second attempt contains
	 * invalid character */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xc2\xc2o!");
	inlen = strlen((const char *) inbuf) - 3;
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 3;

	/* Input does loose decoding, converting to U+FFFD if illegal
	 * input is encountered */
	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xef\xbf\xbd\xef\xbf\xbdo!",
			SLEN("hell\xef\xbf\xbd\xef\xbf\xbdo!")) == 0);


	/* Input ends mid-sequence, but second attempt contains another
	 * incomplete character */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xc2\xa0\xc2\xa1o!");
	inlen = strlen((const char *) inbuf) - 5;
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 2;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 3;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xc2\xa0\xc2\xa1o!",
			SLEN("hell\xc2\xa0\xc2\xa1o!")) == 0);


	/* Input ends mid-sequence, but second attempt contains insufficient
	 * data to complete the incomplete character */
	in = inbuf;
	out = outbuf;
	strcpy((char *) inbuf, "hell\xe2\x80\xa2o!");
	inlen = strlen((const char *) inbuf) - 4;
	outbuf[0] = '\0';
	outlen = 64;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 1;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	inlen += 3;

	assert(parserutils__filter_process_chunk(input, &in, &inlen,
			&out, &outlen) == PARSERUTILS_OK);

	printf("'%.*s' %d '%.*s' %d\n", (int) inlen, in, (int) inlen,
			(int) (out - ((uint8_t *) outbuf)),
			outbuf, (int) outlen);

	assert(parserutils__filter_reset(input) == PARSERUTILS_OK);

	assert(memcmp(outbuf, "hell\xe2\x80\xa2o!",
			SLEN("hell\xe2\x80\xa2o!")) == 0);


	/* Clean up */
	parserutils__filter_destroy(input);

	printf("PASS\n");

	return 0;
}
