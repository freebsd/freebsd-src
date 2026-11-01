/* Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Special handler chains: the outputs of the handlers that already ran belong
 * to the chunk being added, so a later failure must release them.
 */
#include <stdlib.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

static size_t fail_at, allocations;
static const void *watched;
static int watched_freed;

static void *
handler_test_alloc(size_t size)
{
	if (++allocations == fail_at) {
		return NULL;
	}
	return malloc(size);
}

static void
handler_test_free(void *p)
{
	if (p != NULL && p == watched) {
		watched_freed++;
	}
	free(p);
}

#define UCL_ALLOC(size) handler_test_alloc(size)
#define UCL_FREE(size, ptr) handler_test_free(ptr)
/* Only the parser in this translation unit uses the hooks, as in
 * test_copy_failure.c; ucl_chunk_free() still comes from the library. */
#include "../src/ucl_parser.c"

struct handler_state {
	int calls;
	int freed;
	size_t skip;          /* leading bytes to drop, such as a magic */
	bool fail;            /* reject the input */
	bool fail_next_alloc; /* fail the parser's next allocation */
	const unsigned char *source;
	unsigned char *output;
};

static bool
copy_handler(struct ucl_parser *parser,
			 const unsigned char *source, size_t source_len,
			 unsigned char **destination, size_t *dest_len,
			 void *user_data)
{
	struct handler_state *st = user_data;

	(void) parser;
	st->calls++;
	st->source = source;
	if (st->fail) {
		return false;
	}
	assert(source_len > st->skip);
	st->output = malloc(source_len - st->skip);
	assert(st->output != NULL);
	memcpy(st->output, source + st->skip, source_len - st->skip);
	*destination = st->output;
	*dest_len = source_len - st->skip;
	if (st->fail_next_alloc) {
		/* That is the chain node that records this output */
		fail_at = allocations + 1;
		watched = st->output;
	}
	return true;
}

static void
free_output(unsigned char *data, size_t len, void *user_data)
{
	struct handler_state *st = user_data;

	(void) len;
	assert(data == st->output);
	st->freed++;
	free(data);
}

static void
init_handler(struct ucl_parser_special_handler *handler, const char *magic,
			 enum ucl_special_handler_flags flags, bool with_free,
			 struct handler_state *st)
{
	memset(handler, 0, sizeof(*handler));
	handler->magic = (const unsigned char *) magic;
	handler->magic_len = magic != NULL ? strlen(magic) : 0;
	handler->flags = flags;
	handler->handler = copy_handler;
	handler->free_function = with_free ? free_output : NULL;
	handler->user_data = st;
}

static void
test_chain_success(void)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser_special_handler strip, copy;
	struct handler_state strip_st = {0}, copy_st = {0};
	const char input[] = "SH1:key = value;";
	ucl_object_t *top;

	assert(parser != NULL);
	strip_st.skip = 4;
	init_handler(&strip, "SH1:", UCL_SPECIAL_HANDLER_DEFAULT, true, &strip_st);
	init_handler(&copy, NULL, UCL_SPECIAL_HANDLER_PREPROCESS_ALL, true, &copy_st);
	ucl_parser_add_special_handler(parser, &strip);
	ucl_parser_add_special_handler(parser, &copy);

	assert(ucl_parser_add_string(parser, input, sizeof(input) - 1));
	/* Handlers run in registration order, each on the previous output */
	assert(strip_st.calls == 1 && copy_st.calls == 1);
	assert(copy_st.source == strip_st.output);
	top = ucl_parser_get_object(parser);
	assert(top != NULL);
	assert(strcmp(ucl_object_tostring(ucl_object_lookup(top, "key")), "value") == 0);
	ucl_object_unref(top);
	/* The outputs live as long as their chunk */
	assert(strip_st.freed == 0 && copy_st.freed == 0);
	ucl_parser_free(parser);
	assert(strip_st.freed == 1 && copy_st.freed == 1);
}

static void
test_handler_failure(void)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser_special_handler first, second;
	struct handler_state first_st = {0}, second_st = {0};
	const char input[] = "key = value;";

	assert(parser != NULL);
	second_st.fail = true;
	init_handler(&first, NULL, UCL_SPECIAL_HANDLER_PREPROCESS_ALL, true, &first_st);
	init_handler(&second, NULL, UCL_SPECIAL_HANDLER_PREPROCESS_ALL, true, &second_st);
	ucl_parser_add_special_handler(parser, &first);
	ucl_parser_add_special_handler(parser, &second);

	assert(!ucl_parser_add_string(parser, input, sizeof(input) - 1));
	assert(first_st.calls == 1 && second_st.calls == 1);
	assert(second_st.source == first_st.output);
	assert(strstr(ucl_parser_get_error(parser), "external handler") != NULL);
	/* The first output goes away with the rejected chunk, exactly once */
	assert(first_st.freed == 1 && second_st.freed == 0);
	ucl_parser_free(parser);
	assert(first_st.freed == 1);
}

static void
test_chain_alloc_failure(bool with_free)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser_special_handler first, second;
	struct handler_state first_st = {0}, second_st = {0};
	const char input[] = "key = value;";

	assert(parser != NULL);
	second_st.fail_next_alloc = true;
	init_handler(&first, NULL, UCL_SPECIAL_HANDLER_PREPROCESS_ALL, true, &first_st);
	init_handler(&second, NULL, UCL_SPECIAL_HANDLER_PREPROCESS_ALL, with_free,
				 &second_st);
	ucl_parser_add_special_handler(parser, &first);
	ucl_parser_add_special_handler(parser, &second);

	watched_freed = 0;
	assert(!ucl_parser_add_string(parser, input, sizeof(input) - 1));
	assert(fail_at != 0 && allocations >= fail_at);
	fail_at = 0;
	watched = NULL;
	assert(first_st.calls == 1 && second_st.calls == 1);
	assert(strstr(ucl_parser_get_error(parser), "cannot allocate") != NULL);
	/* The output without a chain node is released the way its node would be */
	assert(first_st.freed == 1);
	if (with_free) {
		assert(second_st.freed == 1 && watched_freed == 0);
	}
	else {
		assert(second_st.freed == 0 && watched_freed == 1);
	}
	ucl_parser_free(parser);
	assert(first_st.freed == 1);
}

static void
test_insert_chunk_failure(void)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser_special_handler reject;
	struct handler_state reject_st = {0};
	const char rejected[] = "REJECT b = 2;", inserted[] = "c = 3;";
	ucl_object_t *top;

	assert(parser != NULL);
	assert(ucl_parser_add_string(parser, "a = 1;", 0));
	reject_st.fail = true;
	init_handler(&reject, "REJECT", UCL_SPECIAL_HANDLER_DEFAULT, true, &reject_st);
	ucl_parser_add_special_handler(parser, &reject);

	/*
	 * Neither chunk gets onto the parser's stack, so removing the top of
	 * the stack afterwards would free the chunk of the base document.
	 */
	assert(!ucl_parser_insert_chunk(parser, (const unsigned char *) rejected,
									sizeof(rejected) - 1));
	assert(reject_st.calls == 1);
	assert(!ucl_parser_insert_chunk(parser, NULL, 1));
	assert(parser->chunks != NULL && parser->chunks->next == NULL);
	assert(parser->recursion == 1);
	assert(ucl_parser_insert_chunk(parser, (const unsigned char *) inserted,
								   sizeof(inserted) - 1));
	top = ucl_parser_get_object(parser);
	assert(top != NULL);
	assert(ucl_object_toint(ucl_object_lookup(top, "a")) == 1);
	assert(ucl_object_lookup(top, "b") == NULL);
	assert(ucl_object_toint(ucl_object_lookup(top, "c")) == 3);
	ucl_object_unref(top);
	ucl_parser_free(parser);
}

int main(void)
{
	test_chain_success();
	test_handler_failure();
	test_chain_alloc_failure(true);
	test_chain_alloc_failure(false);
	test_insert_chunk_failure();
	return 0;
}
