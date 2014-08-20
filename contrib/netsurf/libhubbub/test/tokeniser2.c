#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <json.h>

#include <parserutils/input/inputstream.h>

#include <hubbub/hubbub.h>

#include "utils/utils.h"

#include "tokeniser/tokeniser.h"

#include "testutils.h"

typedef struct context {
	const uint8_t *pbuffer;

	const uint8_t *input;
	size_t input_len;

	struct array_list *output;
	int output_index;
	size_t char_off;

	const char *last_start_tag;
	struct array_list *content_model;
	bool process_cdata;
} context;

static void run_test(context *ctx);
static hubbub_error token_handler(const hubbub_token *token, void *pw);

int main(int argc, char **argv)
{
	struct json_object *json;
	struct array_list *tests;
	struct lh_entry *entry;
	char *key;
	struct json_object *val;
	int i;
	context ctx;

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	json = json_object_from_file(argv[1]);
	assert(!is_error(json));

	assert(strcmp((char *) ((json_object_get_object(json)->head)->k),
			"tests") == 0);

	/* Get array of tests */
	tests = json_object_get_array((struct json_object *)
			(json_object_get_object(json)->head)->v);

	for (i = 0; i < array_list_length(tests); i++) {
		/* Get test */
		struct json_object *test =
			(struct json_object *) array_list_get_idx(tests, i);

		ctx.last_start_tag = NULL;
		ctx.content_model = NULL;
		ctx.process_cdata = false;

		/* Extract settings */
		for (entry = json_object_get_object(test)->head; entry;
				entry = entry->next) {
			key = (char *) entry->k;
			val = (struct json_object *) entry->v;

			if (strcmp(key, "description") == 0) {
				printf("Test: %s\n",
					json_object_get_string(val));
			} else if (strcmp(key, "input") == 0) {
				ctx.input = (const uint8_t *)
					json_object_get_string(val);
				ctx.input_len =	json_object_get_string_len(val);
			} else if (strcmp(key, "output") == 0) {
				ctx.output = json_object_get_array(val);
				ctx.output_index = 0;
				ctx.char_off = 0;
			} else if (strcmp(key, "lastStartTag") == 0) {
				ctx.last_start_tag = (const char *)
						json_object_get_string(val);
			} else if (strcmp(key, "contentModelFlags") == 0) {
				ctx.content_model =
						json_object_get_array(val);
			} else if (strcmp(key, "processCDATA") == 0) {
				ctx.process_cdata =
					json_object_get_boolean(val);
			}
		}

		/* And run the test */
		run_test(&ctx);
	}

	json_object_put(json);

	printf("PASS\n");

	return 0;
}

void run_test(context *ctx)
{
	parserutils_inputstream *stream;
	hubbub_tokeniser *tok;
	hubbub_tokeniser_optparams params;
	int i, max_i;
	struct array_list *outputsave = ctx->output;

	if (ctx->content_model == NULL) {
		max_i = 1;
	} else {
		max_i = array_list_length(ctx->content_model);
	}

	/* We test for each of the content models specified */
	for (i = 0; i < max_i; i++) {
		/* Reset expected output */
		ctx->output = outputsave;
		ctx->output_index = 0;
		ctx->char_off = 0;

		assert(parserutils_inputstream_create("UTF-8", 0, NULL,
				&stream) == PARSERUTILS_OK);

		assert(hubbub_tokeniser_create(stream, &tok) == HUBBUB_OK);

		if (ctx->last_start_tag != NULL) {
			/* Fake up a start tag, in PCDATA state */
			size_t len = strlen(ctx->last_start_tag) + 3;
			uint8_t *buf = malloc(len);

			snprintf((char *) buf, len, "<%s>", 
					ctx->last_start_tag);

			assert(parserutils_inputstream_append(stream,
				buf, len - 1) == PARSERUTILS_OK);

			assert(hubbub_tokeniser_run(tok) == HUBBUB_OK);

			free(buf);
		}

		if (ctx->process_cdata) {
			params.process_cdata = ctx->process_cdata;
			assert(hubbub_tokeniser_setopt(tok,
					HUBBUB_TOKENISER_PROCESS_CDATA,
					&params) == HUBBUB_OK);
		}

		params.token_handler.handler = token_handler;
		params.token_handler.pw = ctx;
		assert(hubbub_tokeniser_setopt(tok,
				HUBBUB_TOKENISER_TOKEN_HANDLER,
				&params) == HUBBUB_OK);

		if (ctx->content_model == NULL) {
			params.content_model.model =
					HUBBUB_CONTENT_MODEL_PCDATA;
		} else {
			const char *cm = json_object_get_string(
				(struct json_object *)
				array_list_get_idx(ctx->content_model, i));

			if (strcmp(cm, "PCDATA") == 0) {
				params.content_model.model =
						HUBBUB_CONTENT_MODEL_PCDATA;
			} else if (strcmp(cm, "RCDATA") == 0) {
				params.content_model.model =
						HUBBUB_CONTENT_MODEL_RCDATA;
			} else if (strcmp(cm, "CDATA") == 0) {
				params.content_model.model =
						HUBBUB_CONTENT_MODEL_CDATA;
			} else {
				params.content_model.model =
					HUBBUB_CONTENT_MODEL_PLAINTEXT;
			}
		}
		assert(hubbub_tokeniser_setopt(tok,
				HUBBUB_TOKENISER_CONTENT_MODEL,
				&params) == HUBBUB_OK);

		assert(parserutils_inputstream_append(stream,
				ctx->input, ctx->input_len) == PARSERUTILS_OK);

		assert(parserutils_inputstream_append(stream, NULL, 0) ==
				PARSERUTILS_OK);

		printf("Input: '%.*s' (%d)\n", (int) ctx->input_len,
				(const char *) ctx->input, 
				(int) ctx->input_len);

		assert(hubbub_tokeniser_run(tok) == HUBBUB_OK);

		hubbub_tokeniser_destroy(tok);

		parserutils_inputstream_destroy(stream);
	}
}

hubbub_error token_handler(const hubbub_token *token, void *pw)
{
	static const char *token_names[] = {
		"DOCTYPE", "StartTag", "EndTag",
		"Comment", "Character", "EOF"
	};
	size_t i;
	context *ctx = (context *) pw;
	struct json_object *obj = NULL;
	struct array_list *items;

	for (; ctx->output_index < array_list_length(ctx->output);
			ctx->output_index++) {
		/* Get object for index */
		obj = (struct json_object *)
				array_list_get_idx(ctx->output,
						ctx->output_index);

		/* If it's not a string, we've found the expected output */
		if (json_object_get_type(obj) != json_type_string)
			break;

		/* Otherwise, it must be a parse error */
		assert(strcmp(json_object_get_string(obj),
				 "ParseError") == 0);
	}

	/* If we've run off the end, this is an error -- the tokeniser has
	 * produced more tokens than expected. We allow for the generation
	 * of a terminating EOF token, however. */
	assert("too many tokens" &&
			(ctx->output_index < array_list_length(ctx->output) ||
			token->type == HUBBUB_TOKEN_EOF));

	/* Got a terminating EOF -- no error */
	if (ctx->output_index >= array_list_length(ctx->output))
		return HUBBUB_OK;

	/* Now increment the output index so we don't re-expect this token */
	ctx->output_index++;

	/* Expected output must be an array */
	assert(json_object_get_type(obj) == json_type_array);

	items = json_object_get_array(obj);

	printf("got %s: expected %s\n", token_names[token->type],
			json_object_get_string((struct json_object *)
				array_list_get_idx(items, 0)));

	/* Make sure we got the token we expected */
	assert(strcmp(token_names[token->type],
			json_object_get_string((struct json_object *)
				array_list_get_idx(items, 0))) == 0);

	switch (token->type) {
	case HUBBUB_TOKEN_DOCTYPE:
	{
		const char *expname = json_object_get_string(
				array_list_get_idx(items, 1));
		const char *exppub = json_object_get_string(
				array_list_get_idx(items, 2));
		const char *expsys = json_object_get_string(
				array_list_get_idx(items, 3));
		bool expquirks = !json_object_get_boolean(
				array_list_get_idx(items, 4));
		const char *gotname = (const char *)token->data.doctype.name.ptr;
		const char *gotpub, *gotsys;

		printf("'%.*s' %sids:\n",
				(int) token->data.doctype.name.len,
				gotname,
				token->data.doctype.force_quirks ?
						"(force-quirks) " : "");

		if (token->data.doctype.public_missing) {
			gotpub = NULL;
			printf("\tpublic: missing\n");
		} else {
			gotpub = (const char *) token->data.doctype.public_id.ptr;
			printf("\tpublic: '%.*s' (%d)\n",
				(int) token->data.doctype.public_id.len,
				gotpub,
				(int) token->data.doctype.public_id.len);
		}

		if (token->data.doctype.system_missing) {
			gotsys = NULL;
			printf("\tsystem: missing\n");
		} else {
			gotsys = (const char *) token->data.doctype.system_id.ptr;
			printf("\tsystem: '%.*s' (%d)\n",
				(int) token->data.doctype.system_id.len,
				gotsys,
				(int) token->data.doctype.system_id.len);
		}

		assert(token->data.doctype.name.len == strlen(expname));
		assert(strncmp(gotname, expname, strlen(expname)) == 0);

		assert((exppub == NULL) ==
				(token->data.doctype.public_missing == true));
		if (exppub) {
			assert(token->data.doctype.public_id.len == strlen(exppub));
			assert(strncmp(gotpub, exppub, strlen(exppub)) == 0);
		}

		assert((expsys == NULL) ==
				(token->data.doctype.system_missing == true));
		if (gotsys) {
			assert(token->data.doctype.system_id.len == strlen(expsys));
			assert(strncmp(gotsys, expsys, strlen(expsys)) == 0);
		}

		assert(expquirks == token->data.doctype.force_quirks);
	}
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		const char *expname = json_object_get_string(
				array_list_get_idx(items, 1));
		struct lh_entry *expattrs = json_object_get_object(
				array_list_get_idx(items, 2))->head;
		bool self_closing = json_object_get_boolean(
				array_list_get_idx(items, 3));

		const char *tagname = (const char *)
				token->data.tag.name.ptr;

		printf("expected: '%s' %s\n",
				expname,
				(self_closing) ? "(self-closing) " : "");

		printf("     got: '%.*s' %s\n",
				(int) token->data.tag.name.len,
				tagname,
				(token->data.tag.self_closing) ?
						"(self-closing) " : "");

		if (token->data.tag.n_attributes > 0) {
			printf("attributes:\n");
		}

		assert(token->data.tag.name.len == strlen(expname));
		assert(strncmp(tagname, expname, strlen(expname)) == 0);

		assert((token->data.tag.n_attributes == 0) ==
				(expattrs == NULL));

		assert(self_closing == token->data.tag.self_closing);

		for (i = 0; i < token->data.tag.n_attributes; i++) {
			char *expname = (char *) expattrs->k;
			const char *expval = json_object_get_string(
					(struct json_object *) expattrs->v);
			const char *gotname = (const char *)
				token->data.tag.attributes[i].name.ptr;
			size_t namelen =
				token->data.tag.attributes[i].name.len;
			const char *gotval = (const char *)
				token->data.tag.attributes[i].value.ptr;
			size_t vallen =
				token->data.tag.attributes[i].value.len;

			printf("\t'%.*s' = '%.*s'\n",
					(int) namelen, gotname,
					(int) vallen, gotval);

			assert(namelen == strlen(expname));
			assert(strncmp(gotname, expname,
						strlen(expname)) == 0);
			assert(vallen == strlen(expval));
			assert(strncmp(gotval, expval, strlen(expval)) == 0);

			expattrs = expattrs->next;
		}

		assert(expattrs == NULL);
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		const char *expname = json_object_get_string(
				array_list_get_idx(items, 1));
		const char *tagname = (const char *)
				token->data.tag.name.ptr;

		printf("'%.*s' %s\n",
				(int) token->data.tag.name.len,
				tagname,
				(token->data.tag.n_attributes > 0) ?
						"attributes:" : "");

		assert(token->data.tag.name.len == strlen(expname));
		assert(strncmp(tagname, expname, strlen(expname)) == 0);
	}
		break;
	case HUBBUB_TOKEN_COMMENT:
	{
		const char *expstr = json_object_get_string(
				array_list_get_idx(items, 1));
		const char *gotstr = (const char *)
				token->data.comment.ptr;

		printf("expected: '%s'\n", expstr);
		printf("     got: '%.*s'\n",
				(int) token->data.comment.len, gotstr);

		assert(token->data.comment.len == strlen(expstr));
		assert(strncmp(gotstr, expstr, strlen(expstr)) == 0);
	}
		break;
	case HUBBUB_TOKEN_CHARACTER:
	{
		int expstrlen = json_object_get_string_len(
				array_list_get_idx(items, 1));
		const char *expstr =json_object_get_string( 
				array_list_get_idx(items, 1));
		const char *gotstr = (const char *)
				token->data.character.ptr;
		size_t len = min(token->data.character.len,
				expstrlen - ctx->char_off);

		printf("expected: '%.*s'\n", (int) len, expstr + ctx->char_off);
		printf("     got: '%.*s'\n", 
				(int) token->data.character.len, gotstr);

		assert(memcmp(gotstr, expstr + ctx->char_off, len) == 0);

		if (len < token->data.character.len) {
			/* Expected token only contained part of the data
			 * Calculate how much is left, then try again with
			 * the next expected token */
			hubbub_token t;

			t.type = HUBBUB_TOKEN_CHARACTER;
			t.data.character.ptr += len;
			t.data.character.len -= len;

			ctx->char_off = 0;

			token_handler(&t, pw);
		} else if (strlen(expstr + ctx->char_off) >
				token->data.character.len) {
			/* Tokeniser output only contained part of the data
			 * in the expected token; calculate the offset into
			 * the token and process the remainder next time */
			ctx->char_off += len;
			ctx->output_index--;
		} else {
			/* Exact match - clear offset */
			ctx->char_off = 0;
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		printf("\n");
		break;
	}

	return HUBBUB_OK;
}
