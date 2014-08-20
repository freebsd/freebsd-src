#include <inttypes.h>
#include <stdio.h>

#include <parserutils/input/inputstream.h>

#include <hubbub/hubbub.h>

#include "utils/utils.h"

#include "tokeniser/tokeniser.h"

#include "testutils.h"

static hubbub_error token_handler(const hubbub_token *token, void *pw);

int main(int argc, char **argv)
{
	parserutils_inputstream *stream;
	hubbub_tokeniser *tok;
	hubbub_tokeniser_optparams params;
	FILE *fp;
	size_t len;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	assert(parserutils_inputstream_create("UTF-8", 0, NULL,
			&stream) == PARSERUTILS_OK);

	assert(hubbub_tokeniser_create(stream, &tok) == HUBBUB_OK);

	params.token_handler.handler = token_handler;
	params.token_handler.pw = NULL;
	assert(hubbub_tokeniser_setopt(tok, HUBBUB_TOKENISER_TOKEN_HANDLER,
			&params) == HUBBUB_OK);

	fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		printf("Failed opening %s\n", argv[1]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	while (len > 0) {
		ssize_t bytes_read = fread(buf, 1, CHUNK_SIZE, fp);
                
                if (bytes_read < 1)
                        break;
                
		assert(parserutils_inputstream_append(stream,
				buf, bytes_read) == PARSERUTILS_OK);

		len -= bytes_read;

		assert(hubbub_tokeniser_run(tok) == HUBBUB_OK);
	}
        
        assert(len == 0);
        
	fclose(fp);

	hubbub_tokeniser_destroy(tok);

	parserutils_inputstream_destroy(stream);

	printf("PASS\n");

	return 0;
}

hubbub_error token_handler(const hubbub_token *token, void *pw)
{
	static const char *token_names[] = {
		"DOCTYPE", "START TAG", "END TAG",
		"COMMENT", "CHARACTERS", "EOF"
	};
	size_t i;

	UNUSED(pw);

	printf("%s: ", token_names[token->type]);

	switch (token->type) {
	case HUBBUB_TOKEN_DOCTYPE:
		printf("'%.*s' %sids:\n",
				(int) token->data.doctype.name.len,
				token->data.doctype.name.ptr,
				token->data.doctype.force_quirks ?
						"(force-quirks) " : "");

		if (token->data.doctype.public_missing)
			printf("\tpublic: missing\n");
		else
			printf("\tpublic: '%.*s'\n",
				(int) token->data.doctype.public_id.len,
				token->data.doctype.public_id.ptr);

		if (token->data.doctype.system_missing)
			printf("\tsystem: missing\n");
		else
			printf("\tsystem: '%.*s'\n",
				(int) token->data.doctype.system_id.len,
				token->data.doctype.system_id.ptr);

		break;
	case HUBBUB_TOKEN_START_TAG:
		printf("'%.*s' %s%s\n",
				(int) token->data.tag.name.len,
				token->data.tag.name.ptr,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "",
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");
		for (i = 0; i < token->data.tag.n_attributes; i++) {
			printf("\t'%.*s' = '%.*s'\n",
					(int) token->data.tag.attributes[i].name.len,
					token->data.tag.attributes[i].name.ptr,
					(int) token->data.tag.attributes[i].value.len,
					token->data.tag.attributes[i].value.ptr);
		}
		break;
	case HUBBUB_TOKEN_END_TAG:
		printf("'%.*s' %s%s\n",
				(int) token->data.tag.name.len,
				token->data.tag.name.ptr,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "",
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");
		for (i = 0; i < token->data.tag.n_attributes; i++) {
			printf("\t'%.*s' = '%.*s'\n",
					(int) token->data.tag.attributes[i].name.len,
					token->data.tag.attributes[i].name.ptr,
					(int) token->data.tag.attributes[i].value.len,
					token->data.tag.attributes[i].value.ptr);
		}
		break;
	case HUBBUB_TOKEN_COMMENT:
		printf("'%.*s'\n", (int) token->data.comment.len,
				token->data.comment.ptr);
		break;
	case HUBBUB_TOKEN_CHARACTER:
		printf("'%.*s'\n", (int) token->data.character.len,
				token->data.character.ptr);
		break;
	case HUBBUB_TOKEN_EOF:
		printf("\n");
		break;
	}

	return HUBBUB_OK;
}
