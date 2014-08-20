#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include <libcss/libcss.h>

#include "charset/detect.h"
#include "utils/utils.h"

#include "testutils.h"

typedef struct line_ctx {
	size_t buflen;
	size_t bufused;
	uint8_t *buf;
	char enc[64];
	bool indata;
	bool inenc;
} line_ctx;

static bool handle_line(const char *data, size_t datalen, void *pw);
static void run_test(const uint8_t *data, size_t len, char *expected);

int main(int argc, char **argv)
{
	line_ctx ctx;

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	ctx.buflen = css__parse_filesize(argv[1]);
	if (ctx.buflen == 0)
		return 1;

	ctx.buf = malloc(ctx.buflen);
	if (ctx.buf == NULL) {
		printf("Failed allocating %u bytes\n",
				(unsigned int) ctx.buflen);
		return 1;
	}

	ctx.buf[0] = '\0';
	ctx.enc[0] = '\0';
	ctx.bufused = 0;
	ctx.indata = false;
	ctx.inenc = false;

	assert(css__parse_testfile(argv[1], handle_line, &ctx) == true);

	/* and run final test */
	if (ctx.bufused > 0 && ctx.buf[ctx.bufused - 1] == '\n')
		ctx.bufused -= 1;

	run_test(ctx.buf, ctx.bufused, ctx.enc);

	free(ctx.buf);

	printf("PASS\n");

	return 0;
}

bool handle_line(const char *data, size_t datalen, void *pw)
{
	line_ctx *ctx = (line_ctx *) pw;

	if (data[0] == '#') {
		if (ctx->inenc) {
			/* This marks end of testcase, so run it */

			if (ctx->buf[ctx->bufused - 1] == '\n')
				ctx->bufused -= 1;

			run_test(ctx->buf, ctx->bufused, ctx->enc);

			ctx->buf[0] = '\0';
			ctx->enc[0] = '\0';
			ctx->bufused = 0;
		}

		ctx->indata = (strncasecmp(data+1, "data", 4) == 0);
		ctx->inenc  = (strncasecmp(data+1, "encoding", 8) == 0);
	} else {
		if (ctx->indata) {
			memcpy(ctx->buf + ctx->bufused, data, datalen);
			ctx->bufused += datalen;
		}
		if (ctx->inenc) {
			strcpy(ctx->enc, data);
			if (ctx->enc[strlen(ctx->enc) - 1] == '\n')
				ctx->enc[strlen(ctx->enc) - 1] = '\0';
		}
	}

	return true;
}

void run_test(const uint8_t *data, size_t len, char *expected)
{
	uint16_t mibenum = 0;
	css_charset_source source = CSS_CHARSET_DEFAULT;
	static int testnum;

	assert(css__charset_extract(data, len, &mibenum, &source) ==
			PARSERUTILS_OK);

	assert(mibenum != 0);

	printf("%d: Detected charset %s (%d) Source %d Expected %s (%d)\n",
			++testnum, parserutils_charset_mibenum_to_name(mibenum),
			mibenum, source, expected,
			parserutils_charset_mibenum_from_name(
				expected, strlen(expected)));

	assert(mibenum == parserutils_charset_mibenum_from_name(
			expected, strlen(expected)));
}
