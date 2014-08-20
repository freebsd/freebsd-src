/* const property generation
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "options.h"
#include "nsgenbind-ast.h"
#include "webidl-ast.h"
#include "jsapi-libdom.h"

static int output_cast_literal(struct binding *binding,
			 struct webidl_node *node)
{
	struct webidl_node *type_node = NULL;
	struct webidl_node *literal_node = NULL;
	struct webidl_node *type_base = NULL;
	enum webidl_type webidl_arg_type;

	type_node = webidl_node_find_type(webidl_node_getnode(node),
					 NULL,
					 WEBIDL_NODE_TYPE_TYPE);

	type_base = webidl_node_find_type(webidl_node_getnode(type_node),
					      NULL,
					      WEBIDL_NODE_TYPE_TYPE_BASE);

	webidl_arg_type = webidl_node_getint(type_base);

	switch (webidl_arg_type) {

	case WEBIDL_TYPE_BOOL:
		/* JSBool */
		literal_node = webidl_node_find_type(webidl_node_getnode(node),
					 NULL,
					 WEBIDL_NODE_TYPE_LITERAL_BOOL);
		fprintf(binding->outfile, "BOOLEAN_TO_JSVAL(JS_FALSE)");
		break;

	case WEBIDL_TYPE_FLOAT:
	case WEBIDL_TYPE_DOUBLE:
		/* double */
		literal_node = webidl_node_find_type(webidl_node_getnode(node),
					 NULL,
					 WEBIDL_NODE_TYPE_LITERAL_FLOAT);
		fprintf(binding->outfile, "DOUBLE_TO_JSVAL(0.0)");
		break;

	case WEBIDL_TYPE_LONG:
		/* int32_t  */
		literal_node = webidl_node_find_type(webidl_node_getnode(node),
					 NULL,
					 WEBIDL_NODE_TYPE_LITERAL_INT);
		fprintf(binding->outfile,
			"INT_TO_JSVAL(%d)",
			webidl_node_getint(literal_node));
		break;

	case WEBIDL_TYPE_SHORT:
		/* int16_t  */
		literal_node = webidl_node_find_type(webidl_node_getnode(node),
					 NULL,
					 WEBIDL_NODE_TYPE_LITERAL_INT);
		fprintf(binding->outfile,
			"INT_TO_JSVAL(%d)",
			webidl_node_getint(literal_node));
		break;


	case WEBIDL_TYPE_STRING:
	case WEBIDL_TYPE_BYTE:
	case WEBIDL_TYPE_OCTET:
	case WEBIDL_TYPE_LONGLONG:
	case WEBIDL_TYPE_SEQUENCE:
	case WEBIDL_TYPE_OBJECT:
	case WEBIDL_TYPE_DATE:
	case WEBIDL_TYPE_VOID:
	case WEBIDL_TYPE_USER:
	default:
		WARN(WARNING_UNIMPLEMENTED, "types not allowed as literal");
		break; /* @todo these types are not allowed here */
	}

	return 0;
}

static int webidl_const_define_cb(struct webidl_node *node, void *ctx)
{
	struct binding *binding = ctx;
	struct webidl_node *ident_node;

	ident_node = webidl_node_find_type(webidl_node_getnode(node),
					   NULL,
					   WEBIDL_NODE_TYPE_IDENT);
	if (ident_node == NULL) {
		/* Broken AST - must have ident */
		return 1;
	}

	fprintf(binding->outfile,
		"\tJS_DefineProperty(cx,\n"
		"\t\tprototype,\n"
		"\t\t\"%s\",\n"
		"\t\t",
		webidl_node_gettext(ident_node));

	output_cast_literal(binding, node);

	fprintf(binding->outfile,
		",\n"
		"\t\tJS_PropertyStub,\n"
		"\t\tJS_StrictPropertyStub,\n"
		"\t\tJSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT);\n\n");

	return 0;

}


/* callback to emit implements property spec */
static int webidl_const_spec_implements_cb(struct webidl_node *node, void *ctx)
{
	struct binding *binding = ctx;

	return output_const_defines(binding, webidl_node_gettext(node));
}

int
output_const_defines(struct binding *binding, const char *interface)
{
	struct webidl_node *interface_node;
	struct webidl_node *members_node;
	struct webidl_node *inherit_node;
	int res = 0;

	/* find interface in webidl with correct ident attached */
	interface_node = webidl_node_find_type_ident(binding->wi_ast,
						     WEBIDL_NODE_TYPE_INTERFACE,
						     interface);

	if (interface_node == NULL) {
		fprintf(stderr,
			"Unable to find interface %s in loaded WebIDL\n",
			interface);
		return -1;
	}

	/* generate property entries for each list (partial interfaces) */
	members_node = webidl_node_find_type(webidl_node_getnode(interface_node),
					NULL,
					WEBIDL_NODE_TYPE_LIST);

	while (members_node != NULL) {
		fprintf(binding->outfile,"\t/**** %s ****/\n", interface);

		/* for each const emit a property define */
		webidl_node_for_each_type(webidl_node_getnode(members_node),
					  WEBIDL_NODE_TYPE_CONST,
					  webidl_const_define_cb,
					  binding);


		members_node = webidl_node_find_type(webidl_node_getnode(interface_node),
						members_node,
						WEBIDL_NODE_TYPE_LIST);
	}

	/* check for inherited nodes and insert them too */
	inherit_node = webidl_node_find(webidl_node_getnode(interface_node),
					NULL,
					webidl_cmp_node_type,
					(void *)WEBIDL_NODE_TYPE_INTERFACE_INHERITANCE);

	if (inherit_node != NULL) {
		res = output_const_defines(binding,
					   webidl_node_gettext(inherit_node));
	}

	if (res == 0) {
		res = webidl_node_for_each_type(webidl_node_getnode(interface_node),
					WEBIDL_NODE_TYPE_INTERFACE_IMPLEMENTS,
					webidl_const_spec_implements_cb,
					binding);
	}

	return res;
}
