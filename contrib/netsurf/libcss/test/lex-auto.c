#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/input/inputstream.h>

#include <libcss/libcss.h>

#include "charset/detect.h"
#include "lex/lex.h"
#include "utils/utils.h"

#include "testutils.h"

typedef struct exp_entry {
	css_token_type type;
#define EXP_ENTRY_TEXT_LEN (128)
	char text[EXP_ENTRY_TEXT_LEN];
	size_t textLen;
	bool hasText;
} exp_entry;

typedef struct line_ctx {
	size_t buflen;
	size_t bufused;
	uint8_t *buf;

	size_t explen;
	size_t expused;
	exp_entry *exp;

	bool indata;
	bool inexp;
} line_ctx;

static bool handle_line(const char *data, size_t datalen, void *pw);
static void css__parse_expected(line_ctx *ctx, const char *data, size_t len);
static const char *string_from_type(css_token_type type);
static css_token_type string_to_type(const char *data, size_t len);
static void run_test(const uint8_t *data, size_t len, 
		exp_entry *exp, size_t explen);

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
	ctx.expused = 0;
	ctx.exp = NULL;
	ctx.indata = false;
	ctx.inexp = false;

	assert(css__parse_testfile(argv[1], handle_line, &ctx) == true);

	/* and run final test */
	if (ctx.bufused > 0)
		run_test(ctx.buf, ctx.bufused, ctx.exp, ctx.expused);

	free(ctx.buf);
	free(ctx.exp);

	printf("PASS\n");

	return 0;
}

bool handle_line(const char *data, size_t datalen, void *pw)
{
	line_ctx *ctx = (line_ctx *) pw;

	if (data[0] == '#') {
		if (ctx->inexp) {
			/* This marks end of testcase, so run it */

			run_test(ctx->buf, ctx->bufused, 
					ctx->exp, ctx->expused);

			ctx->buf[0] = '\0';
			ctx->bufused = 0;

			ctx->expused = 0;
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

			css__parse_expected(ctx, data, datalen);
		}
	}

	return true;
}

void css__parse_expected(line_ctx *ctx, const char *data, size_t len)
{
	css_token_type type;
	const char *colon = css__parse_strnchr(data, len, ':');

	if (colon == NULL)
		colon = data + len;

	type = string_to_type(data, colon - data);

	/* Append to list of expected tokens */
	if (ctx->expused == ctx->explen) {
		size_t num = ctx->explen == 0 ? 4 : ctx->explen;

		exp_entry *temp = realloc(ctx->exp, 
				num * 2 * sizeof(exp_entry));
		if (temp == NULL) {
			assert(0 && "No memory for expected tokens");
		}

		ctx->exp = temp;
		ctx->explen = num * 2;
	}

	ctx->exp[ctx->expused].type = type;
	ctx->exp[ctx->expused].textLen = 0;
	ctx->exp[ctx->expused].hasText = (colon != data + len);

	if (colon != data + len) {
		const char *p = colon + 1;
		bool escape = false;

		for (len = len - (colon + 1 - data); len > 0; len--, p++) {
			char c;

			if (escape == false && *p == '\\') {
				escape = true;
				continue;
			}

			if (escape) {
				switch (*p) {
				case 'n':
					c = 0xa;
					break;
				case 't':
					c = 0x9;
					break;
				default:
					c = *p;
					break;
				}
				escape = false;
			} else {
				c = *p;
			}

			ctx->exp[ctx->expused].text[
					ctx->exp[ctx->expused].textLen] = c;
			ctx->exp[ctx->expused].textLen++;

			assert(ctx->exp[ctx->expused].textLen < 
					EXP_ENTRY_TEXT_LEN);
		}
	}
	ctx->expused++;
}

const char *string_from_type(css_token_type type)
{
	const char *names[] =
	{
		"IDENT", "ATKEYWORD", "HASH", "FUNCTION", "STRING", "INVALID",
		"URI", "UNICODE-RANGE", "CHAR", "NUMBER", "PERCENTAGE",
		"DIMENSION", "last_intern", "CDO", "CDC", "S", "COMMENT", 
		"INCLUDES", "DASHMATCH", "PREFIXMATCH", "SUFFIXMATCH",
		"SUBSTRINGMATCH", "EOF"
	};

	return names[type];
}

css_token_type string_to_type(const char *data, size_t len)
{
	if (len == 5 && strncasecmp(data, "IDENT", len) == 0)
		return CSS_TOKEN_IDENT;
	else if (len == 9 && strncasecmp(data, "ATKEYWORD", len) == 0)
		return CSS_TOKEN_ATKEYWORD;
	else if (len == 6 && strncasecmp(data, "STRING", len) == 0)
		return CSS_TOKEN_STRING;
	else if (len == 7 && strncasecmp(data, "INVALID", len) == 0)
		return CSS_TOKEN_INVALID_STRING;
	else if (len == 4 && strncasecmp(data, "HASH", len) == 0)
		return CSS_TOKEN_HASH;
	else if (len == 6 && strncasecmp(data, "NUMBER", len) == 0)
		return CSS_TOKEN_NUMBER;
	else if (len == 10 && strncasecmp(data, "PERCENTAGE", len) == 0)
		return CSS_TOKEN_PERCENTAGE;
	else if (len == 9 && strncasecmp(data, "DIMENSION", len) == 0)
		return CSS_TOKEN_DIMENSION;
	else if (len == 3 && strncasecmp(data, "URI", len) == 0)
		return CSS_TOKEN_URI;
	else if (len == 13 && strncasecmp(data, "UNICODE-RANGE", len) == 0)
		return CSS_TOKEN_UNICODE_RANGE;
	else if (len == 3 && strncasecmp(data, "CDO", len) == 0)
		return CSS_TOKEN_CDO;
	else if (len == 3 && strncasecmp(data, "CDC", len) == 0)
		return CSS_TOKEN_CDC;
	else if (len == 1 && strncasecmp(data, "S", len) == 0)
		return CSS_TOKEN_S;
	else if (len == 7 && strncasecmp(data, "COMMENT", len) == 0)
		return CSS_TOKEN_COMMENT;
	else if (len == 8 && strncasecmp(data, "FUNCTION", len) == 0)
		return CSS_TOKEN_FUNCTION;
	else if (len == 8 && strncasecmp(data, "INCLUDES", len) == 0)
		return CSS_TOKEN_INCLUDES;
	else if (len == 9 && strncasecmp(data, "DASHMATCH", len) == 0)
		return CSS_TOKEN_DASHMATCH;
	else if (len == 11 && strncasecmp(data, "PREFIXMATCH", len) == 0)
		return CSS_TOKEN_PREFIXMATCH;
	else if (len == 11 && strncasecmp(data, "SUFFIXMATCH", len) == 0)
		return CSS_TOKEN_SUFFIXMATCH;
	else if (len == 14 && strncasecmp(data, "SUBSTRINGMATCH", len) == 0)
		return CSS_TOKEN_SUBSTRINGMATCH;
	else if (len == 4 && strncasecmp(data, "CHAR", len) == 0)
		return CSS_TOKEN_CHAR;
	else
		return CSS_TOKEN_EOF;
}

void run_test(const uint8_t *data, size_t len, exp_entry *exp, size_t explen)
{
	parserutils_inputstream *input;
	css_lexer *lexer;
	css_error error;
	css_token *tok;
	size_t e;
	static int testnum;

	assert(parserutils_inputstream_create("UTF-8", CSS_CHARSET_DICTATED,
			css__charset_extract, &input) == PARSERUTILS_OK);

	assert(css__lexer_create(input, &lexer) == CSS_OK);

	assert(parserutils_inputstream_append(input, data, len) == 
			PARSERUTILS_OK);

	assert(parserutils_inputstream_append(input, NULL, 0) == 
			PARSERUTILS_OK);

	e = 0;
	testnum++;

	while ((error = css__lexer_get_token(lexer, &tok)) == CSS_OK) {
		if (tok->type != exp[e].type) {
			printf("%d: Got token %s, Expected %s [%d, %d]\n",
				testnum, string_from_type(tok->type), 
				string_from_type(exp[e].type),
				tok->line, tok->col);
			assert(0 && "Types differ");
		}

		if (exp[e].hasText) {
			if (tok->data.len != exp[e].textLen) {
				printf("%d: Got length %d, Expected %d\n",
					testnum, (int) tok->data.len, 
					(int) exp[e].textLen);
				assert(0 && "Text lengths differ");
			}

			if (strncmp((char *) tok->data.data, exp[e].text, 
					tok->data.len) != 0) {
				printf("%d: Got data '%.*s', Expected '%.*s'\n",
					testnum, 
					(int) tok->data.len, tok->data.data,
					(int) exp[e].textLen, exp[e].text);
				assert(0 && "Text differs");
			}
		}

		e++;

		if (tok->type == CSS_TOKEN_EOF)
			break;
	}

	assert(e == explen);

	css__lexer_destroy(lexer);

	parserutils_inputstream_destroy(input);

	printf("Test %d: PASS\n", testnum);
}
