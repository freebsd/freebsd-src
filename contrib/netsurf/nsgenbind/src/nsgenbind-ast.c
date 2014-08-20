/* binding generator AST implementation for parser
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

/** @todo this currently stuffs everything in one global tree, not very nice
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "nsgenbind-ast.h"
#include "options.h"

/* parser and lexer interface */
extern int nsgenbind_debug;
extern int nsgenbind__flex_debug;
extern void nsgenbind_restart(FILE*);
extern int nsgenbind_parse(struct genbind_node **genbind_ast);

/* terminal nodes have a value only */
struct genbind_node {
	enum genbind_node_type type;
	struct genbind_node *l;
	union {
		void *value;
		struct genbind_node *node;
		char *text;
		int number; /* node data is an integer */
	} r;
};


char *genbind_strapp(char *a, char *b)
{
	char *fullstr;
	int fulllen;
	fulllen = strlen(a) + strlen(b) + 1;
	fullstr = malloc(fulllen);
	snprintf(fullstr, fulllen, "%s%s", a, b);
	free(a);
	free(b);
	return fullstr;
}

struct genbind_node *genbind_node_link(struct genbind_node *tgt, struct genbind_node *src)
{
	tgt->l = src;
	return tgt;
}


struct genbind_node *
genbind_new_node(enum genbind_node_type type, struct genbind_node *l, void *r)
{
	struct genbind_node *nn;
	nn = calloc(1, sizeof(struct genbind_node));
	nn->type = type;
	nn->l = l;
	nn->r.value = r;
	return nn;
}

int
genbind_node_for_each_type(struct genbind_node *node,
			   enum genbind_node_type type,
			   genbind_callback_t *cb,
			   void *ctx)
{
	int ret;

	if (node == NULL) {
		return -1;
	}
	if (node->l != NULL) {
		ret = genbind_node_for_each_type(node->l, type, cb, ctx);
		if (ret != 0) {
			return ret;
		}
	}
	if (node->type == type) {
		return cb(node, ctx);
	}

	return 0;
}


/* exported interface defined in nsgenbind-ast.h */
struct genbind_node *
genbind_node_find(struct genbind_node *node,
		  struct genbind_node *prev,
		  genbind_callback_t *cb,
		  void *ctx)
{
	struct genbind_node *ret;

	if ((node == NULL) || (node == prev)) {
		return NULL;
	}

	if (node->l != prev) {
		ret = genbind_node_find(node->l, prev, cb, ctx);
		if (ret != NULL) {
			return ret;
		}
	}

	if (cb(node, ctx) != 0) {
		return node;
	}

	return NULL;
}

/* exported interface documented in nsgenbind-ast.h */
struct genbind_node *
genbind_node_find_type(struct genbind_node *node,
		       struct genbind_node *prev,
		       enum genbind_node_type type)
{
	return genbind_node_find(node,
				 prev,
				 genbind_cmp_node_type,
				 (void *)type);
}

/* exported interface documented in nsgenbind-ast.h */
struct genbind_node *
genbind_node_find_type_ident(struct genbind_node *node,
			     struct genbind_node *prev,
			     enum genbind_node_type type,
			     const char *ident)
{
	struct genbind_node *found_node;
	struct genbind_node *ident_node;

	if (ident == NULL) {
		return NULL;
	}

	found_node = genbind_node_find_type(node, prev, type);


	while (found_node != NULL) {
		/* look for an ident node  */
		ident_node = genbind_node_find_type(genbind_node_getnode(found_node),
					       NULL,
					       GENBIND_NODE_TYPE_IDENT);
		if (ident_node != NULL) {
			if (strcmp(ident_node->r.text, ident) == 0)
				break;
		}

		/* look for next matching node */
		found_node = genbind_node_find_type(node, found_node, type);
	}
	return found_node;
}

/* exported interface documented in nsgenbind-ast.h */
struct genbind_node *
genbind_node_find_type_type(struct genbind_node *node,
			     struct genbind_node *prev,
			     enum genbind_node_type type,
			     const char *ident)
{
	struct genbind_node *found_node;
	struct genbind_node *ident_node;

	found_node = genbind_node_find_type(node, prev, type);


	while (found_node != NULL) {
		/* look for a type node  */
		ident_node = genbind_node_find_type(genbind_node_getnode(found_node),
					       NULL,
					       GENBIND_NODE_TYPE_TYPE);
		if (ident_node != NULL) {
			if (strcmp(ident_node->r.text, ident) == 0)
				break;
		}

		/* look for next matching node */
		found_node = genbind_node_find_type(node, found_node, type);
	}
	return found_node;
}

int genbind_cmp_node_type(struct genbind_node *node, void *ctx)
{
	if (node->type == (enum genbind_node_type)ctx)
		return 1;
	return 0;
}

char *genbind_node_gettext(struct genbind_node *node)
{
	if (node != NULL) {
		switch(node->type) {
		case GENBIND_NODE_TYPE_WEBIDLFILE:
		case GENBIND_NODE_TYPE_STRING:
		case GENBIND_NODE_TYPE_PREAMBLE:
		case GENBIND_NODE_TYPE_PROLOGUE:
		case GENBIND_NODE_TYPE_EPILOGUE:
		case GENBIND_NODE_TYPE_IDENT:
		case GENBIND_NODE_TYPE_TYPE:
		case GENBIND_NODE_TYPE_BINDING_INTERFACE:
		case GENBIND_NODE_TYPE_CBLOCK:
			return node->r.text;

		default:
			break;
		}
	}
	return NULL;
}

struct genbind_node *genbind_node_getnode(struct genbind_node *node)
{
	if (node != NULL) {
		switch(node->type) {
		case GENBIND_NODE_TYPE_HDRCOMMENT:
		case GENBIND_NODE_TYPE_BINDING:
		case GENBIND_NODE_TYPE_BINDING_PRIVATE:
		case GENBIND_NODE_TYPE_BINDING_INTERNAL:
		case GENBIND_NODE_TYPE_BINDING_PROPERTY:
		case GENBIND_NODE_TYPE_OPERATION:
		case GENBIND_NODE_TYPE_API:
		case GENBIND_NODE_TYPE_GETTER:
		case GENBIND_NODE_TYPE_SETTER:
			return node->r.node;

		default:
			break;
		}
	}
	return NULL;

}

int genbind_node_getint(struct genbind_node *node)
{
	if (node != NULL) {
		switch(node->type) {
		case GENBIND_NODE_TYPE_MODIFIER:
			return node->r.number;

		default:
			break;
		}
	}
	return -1;

}

static const char *genbind_node_type_to_str(enum genbind_node_type type)
{
	switch(type) {
	case GENBIND_NODE_TYPE_IDENT:
		return "Ident";

	case GENBIND_NODE_TYPE_ROOT:
		return "Root";

	case GENBIND_NODE_TYPE_WEBIDLFILE:
		return "webidlfile";

	case GENBIND_NODE_TYPE_HDRCOMMENT:
		return "HdrComment";

	case GENBIND_NODE_TYPE_STRING:
		return "String";

	case GENBIND_NODE_TYPE_PREAMBLE:
		return "Preamble";

	case GENBIND_NODE_TYPE_BINDING:
		return "Binding";

	case GENBIND_NODE_TYPE_TYPE:
		return "Type";

	case GENBIND_NODE_TYPE_BINDING_PRIVATE:
		return "Private";

	case GENBIND_NODE_TYPE_BINDING_INTERNAL:
		return "Internal";

	case GENBIND_NODE_TYPE_BINDING_INTERFACE:
		return "Interface";

	case GENBIND_NODE_TYPE_BINDING_PROPERTY:
		return "Property";

	case GENBIND_NODE_TYPE_OPERATION:
		return "Operation";

	case GENBIND_NODE_TYPE_API:
		return "API";

	case GENBIND_NODE_TYPE_GETTER:
		return "Getter";

	case GENBIND_NODE_TYPE_SETTER:
		return "Setter";

	case GENBIND_NODE_TYPE_CBLOCK:
		return "CBlock";

	default:
		return "Unknown";
	}
}

int genbind_ast_dump(struct genbind_node *node, int indent)
{
	const char *SPACES="                                                                               ";
	char *txt;

	while (node != NULL) {
		printf("%.*s%s", indent, SPACES,  genbind_node_type_to_str(node->type));

		txt = genbind_node_gettext(node);
		if (txt == NULL) {
			printf("\n");
			genbind_ast_dump(genbind_node_getnode(node), indent + 2);
		} else {
			printf(": \"%.*s\"\n", 75 - indent, txt);
		}
		node = node->l;
	}
	return 0;
}

FILE *genbindopen(const char *filename)
{
	FILE *genfile;
	char *fullname;
	int fulllen;
	static char *prevfilepath = NULL;

	/* try filename raw */
	genfile = fopen(filename, "r");
	if (genfile != NULL) {
		if (options->verbose) {
			printf("Opened Genbind file %s\n", filename);
		}
		if (prevfilepath == NULL) {
			fullname = strrchr(filename, '/');
			if (fullname == NULL) {
				fulllen = strlen(filename);
			} else {
				fulllen = fullname - filename;
			}
			prevfilepath = strndup(filename,fulllen);
		}
		if (options->depfilehandle != NULL) {
			fprintf(options->depfilehandle, " \\\n\t%s",
				filename);
		}
		return genfile;
	}

	/* try based on previous filename */
	if (prevfilepath != NULL) {
		fulllen = strlen(prevfilepath) + strlen(filename) + 2;
		fullname = malloc(fulllen);
		snprintf(fullname, fulllen, "%s/%s", prevfilepath, filename);
		if (options->debug) {
			printf("Attempting to open Genbind file %s\n", fullname);
		}
		genfile = fopen(fullname, "r");
		if (genfile != NULL) {
			if (options->verbose) {
				printf("Opened Genbind file %s\n", fullname);
			}
			if (options->depfilehandle != NULL) {
				fprintf(options->depfilehandle, " \\\n\t%s",
					fullname);
			}
			free(fullname);
			return genfile;
		}
		free(fullname);
	}

	/* try on idl path */
	if (options->idlpath != NULL) {
		fulllen = strlen(options->idlpath) + strlen(filename) + 2;
		fullname = malloc(fulllen);
		snprintf(fullname, fulllen, "%s/%s", options->idlpath, filename);
		genfile = fopen(fullname, "r");
		if ((genfile != NULL) && options->verbose) {
			printf("Opend Genbind file %s\n", fullname);
			if (options->depfilehandle != NULL) {
				fprintf(options->depfilehandle, " \\\n\t%s",
					fullname);
			}
		}

		free(fullname);
	}

	return genfile;
}

int genbind_parsefile(char *infilename, struct genbind_node **ast)
{
	FILE *infile;

	/* open input file */
	if ((infilename[0] == '-') &&
	    (infilename[1] == 0)) {
		if (options->verbose) {
			printf("Using stdin for input\n");
		}
		infile = stdin;
	} else {
		infile = genbindopen(infilename);
	}

	if (!infile) {
		fprintf(stderr, "Error opening %s: %s\n",
			infilename,
			strerror(errno));
		return 3;
	}

	if (options->debug) {
		nsgenbind_debug = 1;
		nsgenbind__flex_debug = 1;
	}

	/* set flex to read from file */
	nsgenbind_restart(infile);

	/* process binding */
	return nsgenbind_parse(ast);

}

#ifdef NEED_STRNDUP

char *strndup(const char *s, size_t n)
{
	size_t len;
	char *s2;

	for (len = 0; len != n && s[len]; len++)
		continue;

	s2 = malloc(len + 1);
	if (!s2)
		return 0;

	memcpy(s2, s, len);
	s2[len] = 0;
	return s2;
}

#endif

