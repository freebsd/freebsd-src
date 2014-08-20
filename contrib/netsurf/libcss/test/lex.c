#include <inttypes.h>
#include <stdio.h>

#include <parserutils/charset/utf8.h>
#include <parserutils/input/inputstream.h>

#include <libcss/libcss.h>

#include "charset/detect.h"
#include "utils/utils.h"

#include "lex/lex.h"

#include "testutils.h"

#define ITERATIONS (1)
#define DUMP_TOKENS (0)

static void printToken(const css_token *token)
{
#if !DUMP_TOKENS
	UNUSED(token);
#else
	printf("[%d, %d] : ", token->line, token->col);

	switch (token->type) {
	case CSS_TOKEN_IDENT:
		printf("IDENT(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_ATKEYWORD:
		printf("ATKEYWORD(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_STRING:
		printf("STRING(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_INVALID_STRING:
		printf("INVALID(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_HASH:
		printf("HASH(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_NUMBER:
		printf("NUMBER(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_PERCENTAGE:
		printf("PERCENTAGE(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_DIMENSION:
		printf("DIMENSION(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_URI:
		printf("URI(%.*s)", (int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_UNICODE_RANGE:
		printf("UNICODE-RANGE(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_CDO:
		printf("CDO");
		break;
	case CSS_TOKEN_CDC:
		printf("CDC");
		break;
	case CSS_TOKEN_S:
		printf("S");
		break;
	case CSS_TOKEN_COMMENT:
		printf("COMMENT(%.*s)", (int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_FUNCTION:
		printf("FUNCTION(%.*s)", 
				(int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_INCLUDES:
		printf("INCLUDES");
		break;
	case CSS_TOKEN_DASHMATCH:
		printf("DASHMATCH");
		break;
	case CSS_TOKEN_PREFIXMATCH:
		printf("PREFIXMATCH");
		break;
	case CSS_TOKEN_SUFFIXMATCH:
		printf("SUFFIXMATCH");
		break;
	case CSS_TOKEN_SUBSTRINGMATCH:
		printf("SUBSTRINGMATCH");
		break;
	case CSS_TOKEN_CHAR:
		printf("CHAR(%.*s)", (int) token->data.len, token->data.data);
		break;
	case CSS_TOKEN_EOF:
		printf("EOF");
		break;

	case CSS_TOKEN_LAST_INTERN_LOWER:
	case CSS_TOKEN_LAST_INTERN:
		break;
	}

	printf("\n");
#endif
}

int main(int argc, char **argv)
{
	parserutils_inputstream *stream;
	css_lexer *lexer;
	FILE *fp;
	size_t len;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];
	css_token *tok;
	css_error error;
	int i;

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	for (i = 0; i < ITERATIONS; i++) {
		assert(parserutils_inputstream_create("UTF-8", 
			CSS_CHARSET_DICTATED,css__charset_extract, 
			&stream) == PARSERUTILS_OK);

		assert(css__lexer_create(stream, &lexer) == 
			CSS_OK);

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

			assert(parserutils_inputstream_append(stream,
					buf, CHUNK_SIZE) == PARSERUTILS_OK);

			len -= CHUNK_SIZE;

			while ((error = css__lexer_get_token(lexer, &tok)) == 
					CSS_OK) {
				printToken(tok);

				if (tok->type == CSS_TOKEN_EOF)
					break;
			}
		}

		if (len > 0) {
			size_t read = fread(buf, 1, len, fp);
			assert(read == len);

			assert(parserutils_inputstream_append(stream,
					buf, len) == PARSERUTILS_OK);

			len = 0;
		}

		fclose(fp);

		assert(parserutils_inputstream_append(stream, NULL, 0) == 
				PARSERUTILS_OK);

		while ((error = css__lexer_get_token(lexer, &tok)) == CSS_OK) {
			printToken(tok);

			if (tok->type == CSS_TOKEN_EOF)
				break;
		}

		css__lexer_destroy(lexer);

		parserutils_inputstream_destroy(stream);
	}

	printf("PASS\n");

	return 0;
}

