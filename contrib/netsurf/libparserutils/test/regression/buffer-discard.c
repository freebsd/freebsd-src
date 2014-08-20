#include <stdio.h>
#include <string.h>

#include <parserutils/parserutils.h>
#include <parserutils/utils/buffer.h>

#include "utils/utils.h"

#include "testutils.h"

#define BUFF_LEN 2000

int main(int argc, char **argv)
{
	uint8_t data[BUFF_LEN];
	parserutils_buffer *buf;
	int i;

	UNUSED(argc);
	UNUSED(argv);

	assert(parserutils_buffer_create(&buf) == PARSERUTILS_OK);

	/* Populate the data with '4's */
	for (i = 0; i < BUFF_LEN; i++)
		data[i] = '4';

	assert(parserutils_buffer_append(buf, data, BUFF_LEN) ==
			PARSERUTILS_OK);

	/* Double the size, appending 'c's */
	for (i = 0; i < BUFF_LEN; i++)
		data[i] = 'c';

	assert(parserutils_buffer_append(buf, data, BUFF_LEN) ==
			PARSERUTILS_OK);
	assert(buf->length == 2 * BUFF_LEN);

	/* Now reduce the length by half */
	/* Buffer length is all '4's now */
	buf->length = BUFF_LEN;

	/* Now discard half of the 4s from the middle of the buffer */
	assert(parserutils_buffer_discard(buf, BUFF_LEN / 4, BUFF_LEN / 2) ==
			PARSERUTILS_OK);

	/* Now check that the length is what we expect */
	assert(buf->length == BUFF_LEN / 2);

	/* Now check that the buffer contains what we expect */
	for (i = 0; i < BUFF_LEN / 2; i++)
		assert(buf->data[i] == '4');

	/* Now check that the space we allocated beyond the buffer length is
	 * as we expect, and not overwritten with 'c', which should be beyond
	 * what the buffer_ code is allowed to move. */
	for (i = BUFF_LEN / 2; i < BUFF_LEN; i++)
		assert(buf->data[i] != 'c');
	

	assert(parserutils_buffer_destroy(buf) == PARSERUTILS_OK);

	printf("PASS\n");

	return 0;
}

