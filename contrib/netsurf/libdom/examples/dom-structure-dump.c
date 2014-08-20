/*
 * This file is part of LibDOM.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * Copyright 2010 - 2011 Michael Drake <tlsa@netsurf-browser.org>
 */

/*
 * Load an HTML file into LibDOM with Hubbub and print out the DOM structure.
 *
 * This example demonstrates the following:
 *
 * 1. Using LibDOM's Hubbub binding to read an HTML file into LibDOM.
 * 2. Walking around the DOM tree.
 * 3. Accessing DOM node attributes.
 *
 * Example input:
 *      <html><body><h1 class="woo">NetSurf</h1>
 *      <p>NetSurf is <em>awesome</em>!</p>
 *      <div><h2>Hubbub</h2><p>Hubbub is too.</p>
 *      <p>Big time.</p></div></body></html>
 *
 * Example output:
 *
 * HTML
 * +-BODY
 * | +-H1 class="woo"
 * | +-P
 * | | +-EM
 * | +-DIV
 * | | +-H2
 * | | +-P
 * | | +-P
 *
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>


/**
 * Generate a LibDOM document DOM from an HTML file
 *
 * \param file  The file path
 * \return  pointer to DOM document, or NULL on error
 */
dom_document *create_doc_dom_from_file(char *file)
{
	size_t buffer_size = 1024;
	dom_hubbub_parser *parser = NULL;
	FILE *handle;
	int chunk_length;
	dom_hubbub_error error;
	dom_hubbub_parser_params params;
	dom_document *doc;
	unsigned char buffer[buffer_size];

	params.enc = NULL;
	params.fix_enc = true;
	params.enable_script = false;
	params.msg = NULL;
	params.script = NULL;
	params.ctx = NULL;
	params.daf = NULL;

	/* Create Hubbub parser */
	error = dom_hubbub_parser_create(&params, &parser, &doc);
	if (error != DOM_HUBBUB_OK) {
		printf("Can't create Hubbub Parser\n");
		return NULL;
	}

	/* Open input file */
	handle = fopen(file, "rb");
	if (handle == NULL) {
		dom_hubbub_parser_destroy(parser);
		printf("Can't open test input file: %s\n", file);
		return NULL;
	}

	/* Parse input file in chunks */
	chunk_length = buffer_size;
	while (chunk_length == buffer_size) {
		chunk_length = fread(buffer, 1, buffer_size, handle);
		error = dom_hubbub_parser_parse_chunk(parser, buffer,
				chunk_length);
		if (error != DOM_HUBBUB_OK) {
			dom_hubbub_parser_destroy(parser);
			printf("Parsing errors occur\n");
			return NULL;
		}
	}

	/* Done parsing file */
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		dom_hubbub_parser_destroy(parser);
		printf("Parsing error when construct DOM\n");
		return NULL;
	}

	/* Finished with parser */
	dom_hubbub_parser_destroy(parser);

	/* Close input file */
	if (fclose(handle) != 0) {
		printf("Can't close test input file: %s\n", file);
		return NULL;
	}

	return doc;
}


/**
 * Dump attribute/value for an element node
 *
 * \param node       The element node to dump attribute details for
 * \param attribute  The attribute to dump
 * \return  true on success, or false on error
 */
bool dump_dom_element_attribute(dom_node *node, char *attribute)
{
	dom_exception exc;
	dom_string *attr = NULL;
	dom_string *attr_value = NULL;
	dom_node_type type;
	const char *string;
	size_t length;

	/* Should only have element nodes here */
	exc = dom_node_get_node_type(node, &type);
	if (exc != DOM_NO_ERR) {
		printf(" Exception raised for node_get_node_type\n");
		return false;
	}
	assert(type == DOM_ELEMENT_NODE);

	/* Create a dom_string containing required attribute name. */
	exc = dom_string_create_interned((uint8_t *)attribute,
			strlen(attribute), &attr);
	if (exc != DOM_NO_ERR) {
		printf(" Exception raised for dom_string_create\n");
		return false;
	}

	/* Get class attribute's value */
	exc = dom_element_get_attribute(node, attr, &attr_value);
	if (exc != DOM_NO_ERR) {
		printf(" Exception raised for element_get_attribute\n");
		dom_string_unref(attr);
		return false;
	} else if (attr_value == NULL) {
		/* Element lacks required attribute */
		dom_string_unref(attr);
		return true;
	}

	/* Finished with the attr dom_string */
	dom_string_unref(attr);

	/* Get attribute value's string data */
	string = dom_string_data(attr_value);
	length = dom_string_byte_length(attr_value);

	/* Print attribute info */
	printf(" %s=\"%.*s\"", attribute, (int)length, string);

	/* Finished with the attr_value dom_string */
	dom_string_unref(attr_value);

	return true;
}


/**
 * Print a line in a DOM structure dump for an element
 *
 * \param node   The node to dump
 * \param depth  The node's depth
 * \return  true on success, or false on error
 */
bool dump_dom_element(dom_node *node, int depth)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	int i;
	const char *string;
	size_t length;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);
	if (exc != DOM_NO_ERR) {
		printf("Exception raised for node_get_node_type\n");
		return false;
	} else if (type != DOM_ELEMENT_NODE) {
		/* Nothing to print */
 		return true;
	}

	/* Get element name */
	exc = dom_node_get_node_name(node, &node_name);
	if (exc != DOM_NO_ERR) {
		printf("Exception raised for get_node_name\n");
		return false;
	} else if (node_name == NULL) {
		printf("Broken: root_name == NULL\n");
 		return false;
	}

	/* Print ASCII tree structure for current node */
	if (depth > 0) {
		for (i = 0; i < depth; i++) {
			printf("| ");
		}
		printf("+-");
	}

	/* Get string data and print element name */
	string = dom_string_data(node_name);
	length = dom_string_byte_length(node_name);
	printf("[%.*s]", (int)length, string);
	
	if (length == 5 && strncmp(string, "title", 5) == 0) {
		/* Title tag, gather the title */
		dom_string *str;
		exc = dom_node_get_text_content(node, &str);
		if (exc == DOM_NO_ERR && str != NULL) {
			printf(" $%.*s$", (int)dom_string_byte_length(str),
			       dom_string_data(str));
			dom_string_unref(str);
		}
	}

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	/* Print the element's id & class, if it has them */
	if (dump_dom_element_attribute(node, "id") == false ||
			dump_dom_element_attribute(node, "class") == false) {
		/* Error occured */
		printf("\n");
		return false;
	}

	printf("\n");
	return true;
}


/**
 * Walk though a DOM (sub)tree, in depth first order, printing DOM structure.
 *
 * \param node   The root node to start from
 * \param depth  The depth of 'node' in the (sub)tree
 */
bool dump_dom_structure(dom_node *node, int depth)
{
	dom_exception exc;
	dom_node *child;

	/* Print this node's entry */
	if (dump_dom_element(node, depth) == false) {
		/* There was an error; return */
		return false;
	}

	/* Get the node's first child */
	exc = dom_node_get_first_child(node, &child);
	if (exc != DOM_NO_ERR) {
		printf("Exception raised for node_get_first_child\n");
		return false;
	} else if (child != NULL) {
		/* node has children;  decend to children's depth */
		depth++;

		/* Loop though all node's children */
		do {
			dom_node *next_child;

			/* Visit node's descendents */
			if (dump_dom_structure(child, depth) == false) {
				/* There was an error; return */
				dom_node_unref(child);
				return false;
			}

			/* Go to next sibling */
			exc = dom_node_get_next_sibling(child, &next_child);
			if (exc != DOM_NO_ERR) {
				printf("Exception raised for "
						"node_get_next_sibling\n");
				dom_node_unref(child);
				return false;
			}

			dom_node_unref(child);
			child = next_child;
		} while (child != NULL); /* No more children */
	}

	return true;
}


/**
 * Main entry point from OS.
 */
int main(int argc, char **argv)
{
	dom_exception exc; /* returned by libdom functions */
	dom_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */

	/* Load up the input HTML file */
	doc = create_doc_dom_from_file((argc > 1) ? (argv[1]) : "files/test.html");
	if (doc == NULL) {
		printf("Failed to load document.\n");
		return EXIT_FAILURE;
	}

	/* Get root element */
	exc = dom_document_get_document_element(doc, &root);
	if (exc != DOM_NO_ERR) {
		printf("Exception raised for get_document_element\n");
		dom_node_unref(doc);
 		return EXIT_FAILURE;
	} else if (root == NULL) {
		printf("Broken: root == NULL\n");
		dom_node_unref(doc);
 		return EXIT_FAILURE;
	}

	/* Dump DOM structure */
	if (dump_dom_structure(root, 0) == false) {
		printf("Failed to complete DOM structure dump.\n");
		dom_node_unref(root);
		dom_node_unref(doc);
		return EXIT_FAILURE;
	}

	dom_node_unref(root);

	/* Finished with the dom_document */
	dom_node_unref(doc);

	return EXIT_SUCCESS;
}

