/* binding generator output
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#ifndef nsgenbind_jsapi_libdom_h
#define nsgenbind_jsapi_libdom_h

struct binding {
	struct genbind_node *gb_ast; /* root node of binding AST */
	struct webidl_node *wi_ast; /* root node of webidl AST */


	const char *name; /* name of the binding */
	const char *interface; /* webidl interface binding is for */

	bool has_private; /* true if the binding requires a private structure */
	bool has_global; /* true if the binding is for a global */
	struct genbind_node *binding_list; /* node list of the binding */

	struct genbind_node *addproperty; /* binding api add property node or NULL */
	struct genbind_node *delproperty; /* binding api delete property node or NULL */
	struct genbind_node *getproperty; /* binding api get property node or NULL */
	struct genbind_node *setproperty; /* binding api set property node or NULL */
	struct genbind_node *enumerate; /* binding api enumerate node or NULL */
	struct genbind_node *resolve; /* binding api resolve node or NULL */
	struct genbind_node *finalise; /* binding api finalise node or NULL */
	struct genbind_node *mark; /* binding api mark node or NULL */

	const char *hdrguard; /* header file guard name */

	FILE *outfile ; /* file handle output should be written to,
			 * allows reuse of callback routines to output
			 * to headers and source files 
			 */
	FILE *srcfile ; /* output source file */
	FILE *hdrfile ; /* output header file */
};

/** Generate binding between jsapi and netsurf libdom */
int jsapi_libdom_output(char *outfile, char *hdrfile, struct genbind_node *genbind_root);

/** output code block from a node */
void output_code_block(struct binding *binding, struct genbind_node *codelist);

/* Generate jsapi native function specifiers */
int output_function_spec(struct binding *binding);

/* Generate jsapi native function bodys
 *
 * web IDL describes methods as operators
 * http://www.w3.org/TR/WebIDL/#idl-operations
 *
 * This walks the web IDL AST to find all operator interface members
 * and construct appropriate jsapi native function body to implement
 * them. 
 *
 * Function body contents can be overriden with an operator code
 * block in the binding definition.
 *
 * @param binding The binding information 
 * @param interface The interface to generate operator bodys for
 */
int output_operator_body(struct binding *binding, const char *interface);


/** generate property tinyid enum */
int output_property_tinyid(struct binding *binding);

/** generate property specifier structure */
int output_property_spec(struct binding *binding);

/** generate property function bodies */
int output_property_body(struct binding *binding);

/** generate property definitions for constants */
int output_const_defines(struct binding *binding, const char *interface);




#endif
