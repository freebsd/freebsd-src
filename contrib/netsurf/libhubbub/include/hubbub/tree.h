/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_tree_h_
#define hubbub_tree_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <hubbub/functypes.h>

/**
 * Create a comment node
 *
 * \param ctx     Client's context
 * \param data    String content of node
 * \param result  Pointer to location to receive created node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count must be 1.
 */
typedef hubbub_error (*hubbub_tree_create_comment)(void *ctx, 
		const hubbub_string *data,
		void **result);

/**
 * Create a doctype node
 *
 * \param ctx      Client's context
 * \param doctype  Data for doctype node (name, public id, system id)
 * \param result   Pointer to location to receive created node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count must be 1.
 */
typedef hubbub_error (*hubbub_tree_create_doctype)(void *ctx,
		const hubbub_doctype *doctype,
		void **result);

/**
 * Create an element node
 *
 * \param ctx     Client's context
 * \param tag     Data for element node (namespace, name, attributes)
 * \param result  Pointer to location to receive created node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count must be 1.
 */
typedef hubbub_error (*hubbub_tree_create_element)(void *ctx, 
		const hubbub_tag *tag, 
		void **result);

/**
 * Create a text node
 *
 * \param ctx     Client's context
 * \param data    String content of node
 * \param result  Pointer to location to receive created node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count must be 1.
 */
typedef hubbub_error (*hubbub_tree_create_text)(void *ctx, 
		const hubbub_string *data,
		void **result);

/**
 * Increase a node's reference count
 *
 * \param ctx   Client's context
 * \param node  Node to reference
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: node's reference count is one larger than before
 */
typedef hubbub_error (*hubbub_tree_ref_node)(void *ctx, void *node);

/**
 * Decrease a node's reference count
 *
 * \param ctx   Client's context
 * \param node  Node to reference
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: If the node's reference count becomes zero, and it has no 
 * parent, and it is not the document node, then it is destroyed. Otherwise,
 * the reference count is one less than before.
 */
typedef hubbub_error (*hubbub_tree_unref_node)(void *ctx, void *node);

/**
 * Append a node to the end of another's child list
 *
 * \param ctx     Client's context
 * \param parent  The node to append to
 * \param child   The node to append
 * \param result  Pointer to location to receive appended node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count is increased by 1
 *
 * Important: *result may not == child (e.g. if text nodes got coalesced)
 */
typedef hubbub_error (*hubbub_tree_append_child)(void *ctx, 
		void *parent, 
		void *child,
		void **result);

/**
 * Insert a node into another's child list
 *
 * \param ctx        Client's context
 * \param parent     The node to insert into
 * \param child      The node to insert
 * \param ref_child  The node to insert before
 * \param result     Pointer to location to receive inserted node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count is increased by 1
 *
 * Important: *result may not == child (e.g. if text nodes got coalesced)
 */
typedef hubbub_error (*hubbub_tree_insert_before)(void *ctx, 
		void *parent, 
		void *child,
		void *ref_child, 
		void **result);

/**
 * Remove a node from another's child list
 *
 * \param ctx     Client context
 * \param parent  The node to remove from
 * \param child   The node to remove
 * \param result  Pointer to location to receive removed node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count is increased by 1
 */
typedef hubbub_error (*hubbub_tree_remove_child)(void *ctx, 
		void *parent, 
		void *child,
		void **result);

/**
 * Clone a node
 * 
 * \param ctx     Client's context
 * \param node    The node to clone
 * \param deep    True to clone entire subtree, false to clone only the node
 * \param result  Pointer to location to receive clone
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * Postcondition: if successful, result's reference count must be 1.
 */
typedef hubbub_error (*hubbub_tree_clone_node)(void *ctx, 
		void *node, 
		bool deep,
		void **result);

/**
 * Move all the children of one node to another
 *
 * \param ctx         Client's context
 * \param node        The initial parent node
 * \param new_parent  The new parent node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_reparent_children)(void *ctx, 
		void *node, 
		void *new_parent);

/**
 * Retrieve the parent of a node
 *
 * \param ctx           Client context
 * \param node          Node to retrieve the parent of
 * \param element_only  True if the parent must be an element, false otherwise
 * \param result        Pointer to location to receive parent node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 *
 * If there is a parent node, but it is not an element node and element_only
 * is true, then act as if no parent existed.
 *
 * Postcondition: if there is a parent, then result's reference count must be
 * increased.
 */
typedef hubbub_error (*hubbub_tree_get_parent)(void *ctx, 
		void *node, 
		bool element_only, 
		void **result);

/**
 * Determine if a node has children
 *
 * \param ctx     Client's context
 * \param node    The node to inspect
 * \param result  Location to receive result
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_has_children)(void *ctx, 
		void *node, 
		bool *result);

/**
 * Associate a node with a form
 *
 * \param ctx   Client's context
 * \param form  The form to associate with
 * \param node  The node to associate
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_form_associate)(void *ctx, 
		void *form, 
		void *node);

/**
 * Add attributes to a node
 *
 * \param ctx           Client's context
 * \param node          The node to add to
 * \param attributes    Array of attributes to add
 * \param n_attributes  Number of entries in array
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_add_attributes)(void *ctx, 
		void *node,
		const hubbub_attribute *attributes, 
		uint32_t n_attributes);

/**
 * Notification of the quirks mode of a document
 *
 * \param ctx   Client's context
 * \param mode  The quirks mode
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_set_quirks_mode)(void *ctx, 
		hubbub_quirks_mode mode);

/**
 * Notification that a potential encoding change is required
 *
 * \param ctx      Client's context
 * \param charset  The new charset for the source data
 * \return HUBBUB_OK to continue using the current input handler, 
 *         HUBBUB_ENCODINGCHANGE to stop processing immediately and 
 *                               return control to the client,
 *         appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_encoding_change)(void *ctx, 
		const char *encname);

/**
 * Complete script processing
 *
 * \param ctx   Client's context
 * \param script The script
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_tree_complete_script)(void *ctx, void *script);

/**
 * Hubbub tree handler
 */
typedef struct hubbub_tree_handler {
	hubbub_tree_create_comment create_comment;	/**< Create comment */
	hubbub_tree_create_doctype create_doctype;	/**< Create doctype */
	hubbub_tree_create_element create_element;	/**< Create element */
	hubbub_tree_create_text create_text;		/**< Create text */
	hubbub_tree_ref_node ref_node;			/**< Reference node */
	hubbub_tree_unref_node unref_node;		/**< Unreference node */
	hubbub_tree_append_child append_child;		/**< Append child */
	hubbub_tree_insert_before insert_before;	/**< Insert before */
	hubbub_tree_remove_child remove_child;		/**< Remove child */
	hubbub_tree_clone_node clone_node;		/**< Clone node */
	hubbub_tree_reparent_children reparent_children;/**< Reparent children*/
	hubbub_tree_get_parent get_parent;		/**< Get parent */
	hubbub_tree_has_children has_children;		/**< Has children? */
	hubbub_tree_form_associate form_associate;	/**< Form associate */
	hubbub_tree_add_attributes add_attributes;	/**< Add attributes */
	hubbub_tree_set_quirks_mode set_quirks_mode;	/**< Set quirks mode */
	hubbub_tree_encoding_change encoding_change;	/**< Change encoding */
	hubbub_tree_complete_script complete_script;	/**< Script Complete */
	void *ctx;					/**< Context pointer */
} hubbub_tree_handler;

#ifdef __cplusplus
}
#endif

#endif

