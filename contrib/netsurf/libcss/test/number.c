#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libcss/libcss.h>

#include "utils/utils.h"

#include "testutils.h"

typedef struct line_ctx {
	size_t buflen;
	size_t bufused;
	uint8_t *buf;

	size_t explen;
	char exp[256];

	bool indata;
	bool inexp;
} line_ctx;

static bool handle_line(const char *data, size_t datalen, void *pw);
static void run_test(const uint8_t *data, size_t len, 
		const char *exp, size_t explen);
static void print_css_fixed(char *buf, size_t len, css_fixed f);

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
	ctx.bufused = 0;
	ctx.explen = 0;
	ctx.indata = false;
	ctx.inexp = false;

	assert(css__parse_testfile(argv[1], handle_line, &ctx) == true);

	/* and run final test */
	if (ctx.bufused > 0)
		run_test(ctx.buf, ctx.bufused - 1, ctx.exp, ctx.explen);

	free(ctx.buf);

	printf("PASS\n");

	return 0;
}

bool handle_line(const char *data, size_t datalen, void *pw)
{
	line_ctx *ctx = (line_ctx *) pw;

	if (data[0] == '#') {
		if (ctx->inexp) {
			/* This marks end of testcase, so run it */

			run_test(ctx->buf, ctx->bufused - 1, 
					ctx->exp, ctx->explen);

			ctx->buf[0] = '\0';
			ctx->bufused = 0;

			ctx->explen = 0;
		}

		if (ctx->indata && strncasecmp(data+1, "expected", 8) == 0) {
			ctx->indata = false;
			ctx->inexp = true;
		} else if (!ctx->indata) {
			ctx->indata = (strncasecmp(data+1, "data", 4) == 0);
			ctx->inexp  = (strncasecmp(data+1, "expected", 8) == 0);
		} else {
			memcpy(ctx->buf + ctx->bufused, data, datalen);
			ctx->bufused += datalen;
		}
	} else {
		if (ctx->indata) {
			memcpy(ctx->buf + ctx->bufused, data, datalen);
			ctx->bufused += datalen;
		}
		if (ctx->inexp) {
			if (data[datalen - 1] == '\n')
				datalen -= 1;

			memcpy(ctx->exp, data, datalen);
			ctx->explen = datalen;
		}
	}

	return true;
}

void run_test(const uint8_t *data, size_t len, const char *exp, size_t explen)
{
        lwc_string *in;
	size_t consumed;
	css_fixed result;
	char buf[256];

	UNUSED(exp);
	UNUSED(explen);
        
        assert(lwc_intern_string((const char *)data, len, &in) == lwc_error_ok);
        
	result = css__number_from_lwc_string(in, false, &consumed);

	print_css_fixed(buf, sizeof(buf), result);

	printf("got: %s expected: %.*s\n", buf, (int) explen, exp);

	assert(strncmp(buf, exp, explen) == 0);

	lwc_string_unref(in);
}

void print_css_fixed(char *buf, size_t len, css_fixed f)
{
#define ABS(x) (uint32_t)((x) < 0 ? -(x) : (x))
	uint32_t uintpart = FIXTOINT(ABS(f));
	/* + 500 to ensure round to nearest (division will truncate) */
	uint32_t fracpart = ((ABS(f) & 0x3ff) * 1000 + 500) / (1 << 10);
#undef ABS
	size_t flen = 0;
	char tmp[20];
	size_t tlen = 0;

	if (len == 0)
		return;

	if (f < 0) {
		buf[0] = '-';
		buf++;
		len--;
	}

	do {
		tmp[tlen] = "0123456789"[uintpart % 10];
		tlen++;

		uintpart /= 10;
	} while (tlen < 20 && uintpart != 0);

	while (len > 0 && tlen > 0) {
		buf[0] = tmp[--tlen];
		buf++;
		len--;
	}

	if (len > 0) {
		buf[0] = '.';
		buf++;
		len--;
	}

	do {
		tmp[tlen] = "0123456789"[fracpart % 10];
		tlen++;

		fracpart /= 10;
	} while (tlen < 20 && fracpart != 0);

	while (len > 0 && tlen > 0) {
		buf[0] = tmp[--tlen];
		buf++;
		len--;
		flen++;
	}

	while (len > 0 && flen < 3) {
		buf[0] = '0';
		buf++;
		len--;
		flen++;
	}

	if (len > 0) {
		buf[0] = '\0';
		buf++;
		len--;
	}
}

