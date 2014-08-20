#include <inttypes.h>
#include <stdio.h>

#include <libcss/libcss.h>

#include "charset/detect.h"
#include "utils/utils.h"

#include "lex/lex.h"
#include "parse/parse.h"

#include "testutils.h"

#define ITERATIONS (1)
#define DUMP_EVENTS (0)

#if DUMP_EVENTS
static const char *event_names[] = {
	"START_STYLESHEET",
	"END_STYLESHEET",
	"START_RULESET",
	"END_RULESET",
	"START_ATRULE",
	"END_ATRULE",
	"START_BLOCK",
	"END_BLOCK",
	"BLOCK_CONTENT",
	"DECLARATION"
};
#endif

static css_error event_handler(css_parser_event type, 
		const parserutils_vector *tokens, void *pw)
{
#if !DUMP_EVENTS
	UNUSED(type);
	UNUSED(tokens);
	UNUSED(pw);
#else
	int32_t ctx = 0;
	const css_token *token;

	UNUSED(pw);

	printf("%s%s", tokens != NULL ? "  " : "", event_names[type]);

	if (tokens == NULL) {
		printf("\n");
		return CSS_OK;
	}

	do {
		token = parserutils_vector_iterate(tokens, &ctx);
		if (token == NULL)
			break;

		printf("\n    %d", token->type);

		if (token->data.data != NULL)
			printf(" %.*s", (int) token->data.len, token->data.data);
	} while (token != NULL);

	printf("\n");
#endif

	return CSS_OK;
}

int main(int argc, char **argv)
{
	css_parser_optparams params;
	css_parser *parser;
	FILE *fp;
	size_t len;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];
	css_error error;
	int i;

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	for (i = 0; i < ITERATIONS; i++) {
		assert(css__parser_create("UTF-8", CSS_CHARSET_DICTATED,
				&parser) == CSS_OK);

		params.event_handler.handler = event_handler;
		params.event_handler.pw = NULL;
		assert(css__parser_setopt(parser, CSS_PARSER_EVENT_HANDLER, 
				&params) == CSS_OK);

		fp = fopen(argv[1], "rb");
		if (fp == NULL) {
			printf("Failed opening %s\n", argv[1]);
			return 1;
		}

		fseek(fp, 0, SEEK_END);
		len = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		while (len >= CHUNK_SIZE) {
			size_t read = fread(buf, 1, CHUNK_SIZE, fp);
			assert(read == CHUNK_SIZE);

			error = css__parser_parse_chunk(parser, buf, CHUNK_SIZE);
			assert(error == CSS_OK || error == CSS_NEEDDATA);

			len -= CHUNK_SIZE;
		}

		if (len > 0) {
			size_t read = fread(buf, 1, len, fp);
			assert(read == len);

			error = css__parser_parse_chunk(parser, buf, len);
			assert(error == CSS_OK || error == CSS_NEEDDATA);

			len = 0;
		}

		fclose(fp);

		assert(css__parser_completed(parser) == CSS_OK);

		css__parser_destroy(parser);

	}

	printf("PASS\n");
        
	return 0;
}

