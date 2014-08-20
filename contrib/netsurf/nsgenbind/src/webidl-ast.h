/* Web IDL AST interface 
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#ifndef nsgenbind_webidl_ast_h
#define nsgenbind_webidl_ast_h

enum webidl_node_type {
	/* generic node types which define structure or attributes */
	WEBIDL_NODE_TYPE_ROOT = 0,
	WEBIDL_NODE_TYPE_IDENT,
	/** access modifier e.g. for attributes or types */
	WEBIDL_NODE_TYPE_MODIFIER,
	/** a list of nodes (interface members, arguments)  */
	WEBIDL_NODE_TYPE_LIST, 

        /* non structural node types */
	WEBIDL_NODE_TYPE_INTERFACE,
	WEBIDL_NODE_TYPE_INTERFACE_INHERITANCE,
	WEBIDL_NODE_TYPE_INTERFACE_IMPLEMENTS,

	WEBIDL_NODE_TYPE_ATTRIBUTE,
	WEBIDL_NODE_TYPE_OPERATION,
	WEBIDL_NODE_TYPE_CONST,

	WEBIDL_NODE_TYPE_OPTIONAL_ARGUMENT,
	WEBIDL_NODE_TYPE_ARGUMENT,
	WEBIDL_NODE_TYPE_ELLIPSIS,
	WEBIDL_NODE_TYPE_TYPE,
	WEBIDL_NODE_TYPE_TYPE_BASE,
	WEBIDL_NODE_TYPE_TYPE_NULLABLE,
	WEBIDL_NODE_TYPE_TYPE_ARRAY,

	WEBIDL_NODE_TYPE_LITERAL_NULL,
	WEBIDL_NODE_TYPE_LITERAL_INT,
	WEBIDL_NODE_TYPE_LITERAL_BOOL,
	WEBIDL_NODE_TYPE_LITERAL_FLOAT,

	WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE,

};

enum webidl_type {
	WEBIDL_TYPE_USER,
	WEBIDL_TYPE_BOOL,
	WEBIDL_TYPE_BYTE,
	WEBIDL_TYPE_OCTET,
	WEBIDL_TYPE_FLOAT,
	WEBIDL_TYPE_DOUBLE,
	WEBIDL_TYPE_SHORT,
	WEBIDL_TYPE_LONG,
	WEBIDL_TYPE_LONGLONG,
	WEBIDL_TYPE_STRING,
	WEBIDL_TYPE_SEQUENCE,
	WEBIDL_TYPE_OBJECT,
	WEBIDL_TYPE_DATE,
	WEBIDL_TYPE_VOID,
};

enum webidl_type_modifier {
	WEBIDL_TYPE_MODIFIER_UNSIGNED,
	WEBIDL_TYPE_MODIFIER_UNRESTRICTED,
	WEBIDL_TYPE_READONLY,
};

struct webidl_node;

/** callback for search and iteration routines */
typedef int (webidl_callback_t)(struct webidl_node *node, void *ctx);

int webidl_cmp_node_type(struct webidl_node *node, void *ctx);

struct webidl_node *webidl_node_new(enum webidl_node_type, struct webidl_node *l, void *r);

void webidl_node_set(struct webidl_node *node, enum webidl_node_type type, void *r);

struct webidl_node *webidl_node_prepend(struct webidl_node *list, struct webidl_node *node);
struct webidl_node *webidl_node_append(struct webidl_node *list, struct webidl_node *node);

struct webidl_node *webidl_node_add(struct webidl_node *node, struct webidl_node *list);

/* node contents acessors */
char *webidl_node_gettext(struct webidl_node *node);
struct webidl_node *webidl_node_getnode(struct webidl_node *node);
int webidl_node_getint(struct webidl_node *node);
enum webidl_node_type webidl_node_gettype(struct webidl_node *node);

/* node searches */
int webidl_node_for_each_type(struct webidl_node *node,
			   enum webidl_node_type type,
			   webidl_callback_t *cb,
			      void *ctx);

struct webidl_node *
webidl_node_find(struct webidl_node *node,
		  struct webidl_node *prev,
		  webidl_callback_t *cb,
		 void *ctx);

struct webidl_node *
webidl_node_find_type(struct webidl_node *node,
		  struct webidl_node *prev,
		      enum webidl_node_type type);

struct webidl_node *
webidl_node_find_type_ident(struct webidl_node *root_node, 
			    enum webidl_node_type type, 
			    const char *ident);


/* debug dump */
int webidl_ast_dump(struct webidl_node *node, int indent);

/** parse web idl file */
int webidl_parsefile(char *filename, struct webidl_node **webidl_ast);

#endif
