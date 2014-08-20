#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hubbub/hubbub.h>
#include <hubbub/parser.h>
#include <hubbub/tree.h>

#include "utils/utils.h"

#include "testutils.h"

#define NODE_REF_CHUNK 8192
static uint16_t *node_ref;
static uintptr_t node_ref_alloc;
static uintptr_t node_counter;

#define GROW_REF							\
	if (node_counter >= node_ref_alloc) {				\
		uint16_t *temp = realloc(node_ref,			\
				(node_ref_alloc + NODE_REF_CHUNK) *	\
				sizeof(uint16_t));			\
		if (temp == NULL) {					\
			printf("FAIL - no memory\n");			\
			exit(1);					\
		}							\
		node_ref = temp;					\
		node_ref_alloc += NODE_REF_CHUNK;			\
	}

static hubbub_error create_comment(void *ctx, const hubbub_string *data, 
		void **result);
static hubbub_error create_doctype(void *ctx, const hubbub_doctype *doctype,
		void **result);
static hubbub_error create_element(void *ctx, const hubbub_tag *tag, 
		void **result);
static hubbub_error create_text(void *ctx, const hubbub_string *data, 
		void **result);
static hubbub_error ref_node(void *ctx, void *node);
static hubbub_error unref_node(void *ctx, void *node);
static hubbub_error append_child(void *ctx, void *parent, void *child, 
		void **result);
static hubbub_error insert_before(void *ctx, void *parent, void *child, 
		void *ref_child, void **result);
static hubbub_error remove_child(void *ctx, void *parent, void *child, 
		void **result);
static hubbub_error clone_node(void *ctx, void *node, bool deep, void **result);
static hubbub_error reparent_children(void *ctx, void *node, void *new_parent);
static hubbub_error get_parent(void *ctx, void *node, bool element_only, 
		void **result);
static hubbub_error has_children(void *ctx, void *node, bool *result);
static hubbub_error form_associate(void *ctx, void *form, void *node);
static hubbub_error add_attributes(void *ctx, void *node, 
		const hubbub_attribute *attributes, uint32_t n_attributes);
static hubbub_error set_quirks_mode(void *ctx, hubbub_quirks_mode mode);
static hubbub_error complete_script(void *ctx, void *script);

static hubbub_tree_handler tree_handler = {
	create_comment,
	create_doctype,
	create_element,
	create_text,
	ref_node,
	unref_node,
	append_child,
	insert_before,
	remove_child,
	clone_node,
	reparent_children,
	get_parent,
	has_children,
	form_associate,
	add_attributes,
	set_quirks_mode,
	NULL,
	complete_script,
	NULL
};

static int run_test(int argc, char **argv, unsigned int CHUNK_SIZE)
{
	hubbub_parser *parser;
	hubbub_parser_optparams params;
	FILE *fp;
	size_t len;
	uint8_t *buf = malloc(CHUNK_SIZE);
	const char *charset;
	hubbub_charset_source cssource;
	bool passed = true;
	uintptr_t n;

	UNUSED(argc);

	node_ref = calloc(NODE_REF_CHUNK, sizeof(uint16_t));
	if (node_ref == NULL) {
		printf("Failed allocating node_ref\n");
		return 1;
	}
	node_ref_alloc = NODE_REF_CHUNK;

	assert(hubbub_parser_create("UTF-8", false, &parser) == HUBBUB_OK);

	params.tree_handler = &tree_handler;
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_TREE_HANDLER,
			&params) == HUBBUB_OK);

	params.document_node = (void *) ++node_counter;
	ref_node(NULL, (void *) node_counter);
	assert(hubbub_parser_setopt(parser, HUBBUB_PARSER_DOCUMENT_NODE,
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
                
		assert(hubbub_parser_parse_chunk(parser,
				buf, bytes_read) == HUBBUB_OK);

		len -= bytes_read;
	}
        
        assert(len == 0);
        
	fclose(fp);

	free(buf);

	charset = hubbub_parser_read_charset(parser, &cssource);

	printf("Charset: %s (from %d)\n", charset, cssource);

	hubbub_parser_destroy(parser);

	/* Ensure that all nodes have been released by the treebuilder */
	for (n = 1; n <= node_counter; n++) {
		if (node_ref[n] != 0) {
			printf("%" PRIuPTR " still referenced (=%u)\n", n, node_ref[n]);
			passed = false;
		}
	}

	free(node_ref);
	node_counter = 0;

	printf("%s\n", passed ? "PASS" : "FAIL");

	return passed ? 0 : 1;
}

int main(int argc, char **argv)
{
	int ret;
        int shift;
        int offset;
	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

#define DO_TEST(n) if ((ret = run_test(argc, argv, (n))) != 0) return ret
        for (shift = 0; (1 << shift) != 16384; shift++)
        	for (offset = 0; offset < 10; offset += 3)
	                DO_TEST((1 << shift) + offset);

        return 0;
#undef DO_TEST
}

hubbub_error create_comment(void *ctx, const hubbub_string *data, void **result)
{
	printf("Creating (%" PRIuPTR ") [comment '%.*s']\n", ++node_counter,
			(int) data->len, data->ptr);

	assert(memchr(data->ptr, 0xff, data->len) == NULL);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return HUBBUB_OK;
}

hubbub_error create_doctype(void *ctx, const hubbub_doctype *doctype, 
		void **result)
{
	printf("Creating (%" PRIuPTR ") [doctype '%.*s']\n", ++node_counter,
			(int) doctype->name.len, doctype->name.ptr);

	assert(memchr(doctype->name.ptr, 0xff, doctype->name.len) == NULL);
	if (doctype->public_missing == false) {
		assert(memchr(doctype->public_id.ptr, 0xff,
				doctype->public_id.len) == NULL);
	}
	if (doctype->system_missing == false) {
		assert(memchr(doctype->system_id.ptr, 0xff,
				doctype->system_id.len) == NULL);
	}

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return HUBBUB_OK;
}

hubbub_error create_element(void *ctx, const hubbub_tag *tag, void **result)
{
	uint32_t i;

	printf("Creating (%" PRIuPTR ") [element '%.*s']\n", ++node_counter,
			(int) tag->name.len, tag->name.ptr);

	assert(memchr(tag->name.ptr, 0xff, tag->name.len) == NULL);
	for (i = 0; i < tag->n_attributes; i++) {
		hubbub_attribute *attr = &tag->attributes[i];

		assert(memchr(attr->name.ptr, 0xff, attr->name.len) == NULL);
		assert(memchr(attr->value.ptr, 0xff, attr->value.len) == NULL);
	}

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return HUBBUB_OK;
}

hubbub_error create_text(void *ctx, const hubbub_string *data, void **result)
{
	printf("Creating (%" PRIuPTR ") [text '%.*s']\n", ++node_counter,
			(int) data->len, data->ptr);

	assert(memchr(data->ptr, 0xff, data->len) == NULL);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return HUBBUB_OK;
}

hubbub_error ref_node(void *ctx, void *node)
{
	UNUSED(ctx);

	printf("Referencing %" PRIuPTR " (=%u)\n", 
			(uintptr_t) node, ++node_ref[(uintptr_t) node]);

	return HUBBUB_OK;
}

hubbub_error unref_node(void *ctx, void *node)
{
	UNUSED(ctx);

	printf("Unreferencing %" PRIuPTR " (=%u)\n", 
			(uintptr_t) node, --node_ref[(uintptr_t) node]);

	return HUBBUB_OK;
}

hubbub_error append_child(void *ctx, void *parent, void *child, void **result)
{
	printf("Appending %" PRIuPTR " to %" PRIuPTR "\n", (uintptr_t) child, (uintptr_t) parent);
	ref_node(ctx, child);

	*result = (void *) child;

	return HUBBUB_OK;
}

hubbub_error insert_before(void *ctx, void *parent, void *child, 
		void *ref_child, void **result)
{
	printf("Inserting %" PRIuPTR " in %" PRIuPTR " before %" PRIuPTR "\n", (uintptr_t) child, 
			(uintptr_t) parent, (uintptr_t) ref_child);
	ref_node(ctx, child);

	*result = (void *) child;

	return HUBBUB_OK;
}

hubbub_error remove_child(void *ctx, void *parent, void *child, void **result)
{
	printf("Removing %" PRIuPTR " from %" PRIuPTR "\n", (uintptr_t) child, (uintptr_t) parent);
	ref_node(ctx, child);

	*result = (void *) child;

	return HUBBUB_OK;
}

hubbub_error clone_node(void *ctx, void *node, bool deep, void **result)
{
	printf("%sCloning %" PRIuPTR " -> %" PRIuPTR "\n", deep ? "Deep-" : "",
			(uintptr_t) node, ++node_counter);

	GROW_REF
	node_ref[node_counter] = 0;

	ref_node(ctx, (void *) node_counter);

	*result = (void *) node_counter;

	return HUBBUB_OK;
}

hubbub_error reparent_children(void *ctx, void *node, void *new_parent)
{
	UNUSED(ctx);

	printf("Reparenting children of %" PRIuPTR " to %" PRIuPTR "\n", 
				(uintptr_t) node, (uintptr_t) new_parent);

	return HUBBUB_OK;
}

hubbub_error get_parent(void *ctx, void *node, bool element_only, void **result)
{
	printf("Retrieving parent of %" PRIuPTR " (%s)\n", (uintptr_t) node,
			element_only ? "element only" : "");

	ref_node(ctx, (void *) 1);
	*result = (void *) 1;

	return HUBBUB_OK;
}

hubbub_error has_children(void *ctx, void *node, bool *result)
{
	UNUSED(ctx);

	printf("Want children for %" PRIuPTR "\n", (uintptr_t) node);

	*result = false;

	return HUBBUB_OK;
}

hubbub_error form_associate(void *ctx, void *form, void *node)
{
	UNUSED(ctx);

	printf("Associating %" PRIuPTR " with form %" PRIuPTR "\n", 
			(uintptr_t) node, (uintptr_t) form);

	return HUBBUB_OK;
}

hubbub_error add_attributes(void *ctx, void *node, 
		const hubbub_attribute *attributes, uint32_t n_attributes)
{
	uint32_t i;

	UNUSED(ctx);
	UNUSED(attributes);
	UNUSED(n_attributes);

	printf("Adding attributes to %" PRIuPTR "\n", (uintptr_t) node);

	for (i = 0; i < n_attributes; i++) {
		const hubbub_attribute *attr = &attributes[i];

		assert(memchr(attr->name.ptr, 0xff, attr->name.len) == NULL);
		assert(memchr(attr->value.ptr, 0xff, attr->value.len) == NULL);
	}

	return HUBBUB_OK;
}

hubbub_error set_quirks_mode(void *ctx, hubbub_quirks_mode mode)
{
	UNUSED(ctx);

	printf("Quirks mode = %u\n", mode);

	return HUBBUB_OK;
}

hubbub_error complete_script(void *ctx, void *script)
{
	UNUSED(ctx);
	UNUSED(script);

	return HUBBUB_OK;
}

