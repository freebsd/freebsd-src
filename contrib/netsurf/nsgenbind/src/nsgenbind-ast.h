/* binding file AST interface 
 *
 * This file is part of nsnsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#ifndef nsgenbind_nsgenbind_ast_h
#define nsgenbind_nsgenbind_ast_h

enum genbind_node_type {
	GENBIND_NODE_TYPE_ROOT = 0,
	GENBIND_NODE_TYPE_IDENT, /**< generic identifier string */
	GENBIND_NODE_TYPE_TYPE, /**< generic type string */
	GENBIND_NODE_TYPE_MODIFIER, /**< node modifier */

	GENBIND_NODE_TYPE_CBLOCK,
	GENBIND_NODE_TYPE_WEBIDLFILE,
	GENBIND_NODE_TYPE_HDRCOMMENT,
	GENBIND_NODE_TYPE_STRING,
	GENBIND_NODE_TYPE_PREAMBLE,
	GENBIND_NODE_TYPE_PROLOGUE,
	GENBIND_NODE_TYPE_EPILOGUE,
	GENBIND_NODE_TYPE_BINDING,
	GENBIND_NODE_TYPE_BINDING_PRIVATE,
	GENBIND_NODE_TYPE_BINDING_INTERNAL,
	GENBIND_NODE_TYPE_BINDING_INTERFACE,
	GENBIND_NODE_TYPE_BINDING_PROPERTY,
	GENBIND_NODE_TYPE_API,
	GENBIND_NODE_TYPE_OPERATION,
	GENBIND_NODE_TYPE_GETTER,
	GENBIND_NODE_TYPE_SETTER,
};

/* modifier flags */
enum genbind_type_modifier {
	GENBIND_TYPE_NONE = 0,
	GENBIND_TYPE_TYPE = 1, /**< identifies a type handler */
	GENBIND_TYPE_UNSHARED = 2, /**< unshared item */
	GENBIND_TYPE_TYPE_UNSHARED = 3, /**< identifies a unshared type handler */
};


struct genbind_node;

/** callback for search and iteration routines */
typedef int (genbind_callback_t)(struct genbind_node *node, void *ctx);

int genbind_cmp_node_type(struct genbind_node *node, void *ctx);

FILE *genbindopen(const char *filename);

int genbind_parsefile(char *infilename, struct genbind_node **ast);

char *genbind_strapp(char *a, char *b);

struct genbind_node *genbind_new_node(enum genbind_node_type type, struct genbind_node *l, void *r);
struct genbind_node *genbind_node_link(struct genbind_node *tgt, struct genbind_node *src);

int genbind_ast_dump(struct genbind_node *ast, int indent);

/** Depth first left hand search using user provided comparison 
 *
 * @param node The node to start the search from
 * @param prev The node at which to stop the search, either NULL to
 *             search the full tree depth (initial search) or the result 
 *             of a previous search to continue.
 * @param cb Comparison callback
 * @param ctx Context for callback
 */
struct genbind_node *
genbind_node_find(struct genbind_node *node,
		  struct genbind_node *prev,
		  genbind_callback_t *cb,
		  void *ctx);

/** Depth first left hand search returning nodes of the specified type
 *
 * @param node The node to start the search from
 * @param prev The node at which to stop the search, either NULL to
 *             search the full tree depth (initial search) or the result 
 *             of a previous search to continue.
 * @param nodetype The type of node to seach for
 */
struct genbind_node *
genbind_node_find_type(struct genbind_node *node,
		       struct genbind_node *prev,
		       enum genbind_node_type nodetype);

/** Depth first left hand search returning nodes of the specified type
 *    and a ident child node with matching text
 *
 * @param node The node to start the search from
 * @param prev The node at which to stop the search, either NULL to
 *             search the full tree depth (initial search) or the result 
 *             of a previous search to continue.
 * @param nodetype The type of node to seach for
 * @param ident The text to match the ident child node to
 */
struct genbind_node *
genbind_node_find_type_ident(struct genbind_node *node,
			     struct genbind_node *prev,
			     enum genbind_node_type nodetype,
			     const char *ident);

/** Depth first left hand search returning nodes of the specified type
 *    and a type child node with matching text
 *
 * @param node The node to start the search from
 * @param prev The node at which to stop the search, either NULL to
 *             search the full tree depth (initial search) or the result 
 *             of a previous search to continue.
 * @param nodetype The type of node to seach for
 * @param type The text to match the type child node to
 */
struct genbind_node *
genbind_node_find_type_type(struct genbind_node *node,
			     struct genbind_node *prev,
			     enum genbind_node_type nodetype,
			     const char *type);

int genbind_node_for_each_type(struct genbind_node *node, enum genbind_node_type type, genbind_callback_t *cb, void *ctx);

char *genbind_node_gettext(struct genbind_node *node);
struct genbind_node *genbind_node_getnode(struct genbind_node *node);
int genbind_node_getint(struct genbind_node *node);

#ifdef _WIN32
#define NEED_STRNDUP 1
char *strndup(const char *s, size_t n);
#endif

#endif
