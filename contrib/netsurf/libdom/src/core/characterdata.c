/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/core/characterdata.h>
#include <dom/core/string.h>
#include <dom/events/events.h>

#include "core/characterdata.h"
#include "core/document.h"
#include "core/node.h"
#include "utils/utils.h"
#include "events/mutation_event.h"

/* The virtual functions for dom_characterdata, we make this vtable
 * public to each child class */
struct dom_characterdata_vtable characterdata_vtable = {
	{
		{
			DOM_NODE_EVENT_TARGET_VTABLE
		},
		DOM_NODE_VTABLE_CHARACTERDATA
	},
	DOM_CHARACTERDATA_VTABLE
};


/* Create a DOM characterdata node and compose the vtable */
dom_characterdata *_dom_characterdata_create(void)
{
	dom_characterdata *cdata = malloc(sizeof(struct dom_characterdata));
	if (cdata == NULL)
		return NULL;

	cdata->base.base.vtable = &characterdata_vtable;
	cdata->base.vtable = NULL;

	return cdata;
}

/**
 * Initialise a character data node
 *
 * \param node   The node to initialise
 * \param doc    The document which owns the node
 * \param type   The node type required
 * \param name   The node name, or NULL
 * \param value  The node value, or NULL
 * \return DOM_NO_ERR on success.
 *
 * ::doc, ::name and ::value will have their reference counts increased.
 */
dom_exception _dom_characterdata_initialise(struct dom_characterdata *cdata,
		struct dom_document *doc, dom_node_type type,
		dom_string *name, dom_string *value)
{
	return _dom_node_initialise(&cdata->base, doc, type, 
			name, value, NULL, NULL);
}

/**
 * Finalise a character data node
 *
 * \param cdata  The node to finalise
 *
 * The contents of ::cdata will be cleaned up. ::cdata will not be freed.
 */
void _dom_characterdata_finalise(struct dom_characterdata *cdata)
{
	_dom_node_finalise(&cdata->base);
}


/*----------------------------------------------------------------------*/

/* The public virtual functions */

/**
 * Retrieve data from a character data node
 *
 * \param cdata  Character data node to retrieve data from
 * \param data   Pointer to location to receive data
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 *
 * DOM3Core states that this can raise DOMSTRING_SIZE_ERR. It will not in
 * this implementation; dom_strings are unbounded.
 */
dom_exception _dom_characterdata_get_data(struct dom_characterdata *cdata,
		dom_string **data)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;

	if (c->value != NULL) {
		dom_string_ref(c->value);
	}
	*data = c->value;

	return DOM_NO_ERR;
}

/**
 * Set the content of a character data node
 *
 * \param cdata  Node to set the content of
 * \param data   New value for node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::cdata is readonly.
 *
 * The new content will have its reference count increased, so the caller
 * should unref it after the call (as the caller should have already claimed
 * a reference on the string). The node's existing content will be unrefed.
 */
dom_exception _dom_characterdata_set_data(struct dom_characterdata *cdata,
		dom_string *data)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;
	dom_exception err;
	struct dom_document *doc;
	bool success = true;

	if (_dom_node_readonly(c)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	/* Dispatch a DOMCharacterDataModified event */
	doc = dom_node_get_owner(cdata);
	err = _dom_dispatch_characterdata_modified_event(doc, c, c->value,
			data, &success);
	if (err != DOM_NO_ERR)
		return err;

	if (c->value != NULL) {
		dom_string_unref(c->value);
	}

	dom_string_ref(data);
	c->value = data;

	success = true;
	return _dom_dispatch_subtree_modified_event(doc, c->parent, &success);
}

/**
 * Get the length (in characters) of a character data node's content
 *
 * \param cdata   Node to read content length of
 * \param length  Pointer to location to receive character length of content
 * \return DOM_NO_ERR.
 */
dom_exception _dom_characterdata_get_length(struct dom_characterdata *cdata,
		uint32_t *length)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;

	if (c->value != NULL) {
		*length = dom_string_length(c->value);
	} else {
		*length = 0;
	}

	return DOM_NO_ERR;
}

/**
 * Extract a range of data from a character data node
 *
 * \param cdata   The node to extract data from
 * \param offset  The character offset of substring to extract
 * \param count   The number of characters to extract
 * \param data    Pointer to location to receive substring
 * \return DOM_NO_ERR         on success,
 *         DOM_INDEX_SIZE_ERR if ::offset is negative or greater than the 
 *                            number of characters in ::cdata or 
 *                            ::count is negative.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 *
 * DOM3Core states that this can raise DOMSTRING_SIZE_ERR. It will not in
 * this implementation; dom_strings are unbounded.
 */
dom_exception _dom_characterdata_substring_data(
		struct dom_characterdata *cdata, uint32_t offset,
		uint32_t count, dom_string **data)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;
	uint32_t len, end;

	if ((int32_t) offset < 0 || (int32_t) count < 0) {
		return DOM_INDEX_SIZE_ERR;
	}

	if (c->value != NULL) {
		len = dom_string_length(c->value);
	} else {
		len = 0;
	}

	if (offset > len) {
		return DOM_INDEX_SIZE_ERR;
	}

	end = (offset + count) >= len ? len : offset + count;

	return dom_string_substr(c->value, offset, end, data);
}

/**
 * Append data to the end of a character data node's content
 *
 * \param cdata  The node to append data to
 * \param data   The data to append
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::cdata is readonly.
 */
dom_exception _dom_characterdata_append_data(struct dom_characterdata *cdata,
		dom_string *data)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;
	dom_string *temp;
	dom_exception err;
	struct dom_document *doc;
	bool success = true;

	if (_dom_node_readonly(c)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	err = dom_string_concat(c->value, data, &temp);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Dispatch a DOMCharacterDataModified event */
	doc = dom_node_get_owner(cdata);
	err = _dom_dispatch_characterdata_modified_event(doc, c, c->value,
			temp, &success);
	if (err != DOM_NO_ERR) {
		dom_string_unref(temp);
		return err;
	}

	if (c->value != NULL) {
		dom_string_unref(c->value);
	}

	c->value = temp;

	success = true;
	return _dom_dispatch_subtree_modified_event(doc, c->parent, &success);
}

/**
 * Insert data into a character data node's content
 *
 * \param cdata   The node to insert into
 * \param offset  The character offset to insert at
 * \param data    The data to insert
 * \return DOM_NO_ERR                      on success,
 *         DOM_INDEX_SIZE_ERR              if ::offset is negative or greater 
 *                                         than the number of characters in 
 *                                         ::cdata,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::cdata is readonly.
 */
dom_exception _dom_characterdata_insert_data(struct dom_characterdata *cdata,
		uint32_t offset, dom_string *data)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;
	dom_string *temp;
	uint32_t len;
	dom_exception err;
	struct dom_document *doc;
	bool success = true;

	if (_dom_node_readonly(c)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	if ((int32_t) offset < 0) {
		return DOM_INDEX_SIZE_ERR;
	}

	if (c->value != NULL) {
		len = dom_string_length(c->value);
	} else {
		len = 0;
	}

	if (offset > len) {
		return DOM_INDEX_SIZE_ERR;
	}

	err = dom_string_insert(c->value, data, offset, &temp);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Dispatch a DOMCharacterDataModified event */
	doc = dom_node_get_owner(cdata);
	err = _dom_dispatch_characterdata_modified_event(doc, c, c->value,
			temp, &success);
	if (err != DOM_NO_ERR)
		return err;

	if (c->value != NULL) {
		dom_string_unref(c->value);
	}

	c->value = temp;

	success = true;
	return _dom_dispatch_subtree_modified_event(doc, c->parent, &success);
}

/**
 * Delete data from a character data node's content
 *
 * \param cdata   The node to delete from
 * \param offset  The character offset to start deletion from
 * \param count   The number of characters to delete
 * \return DOM_NO_ERR                      on success,
 *         DOM_INDEX_SIZE_ERR              if ::offset is negative or greater 
 *                                         than the number of characters in 
 *                                         ::cdata or ::count is negative,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::cdata is readonly.
 */
dom_exception _dom_characterdata_delete_data(struct dom_characterdata *cdata,
		uint32_t offset, uint32_t count)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;
	dom_string *temp;
	uint32_t len, end;
	dom_exception err;
	struct dom_document *doc;
	bool success = true;
	dom_string *empty;

	if (_dom_node_readonly(c)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	if ((int32_t) offset < 0 || (int32_t) count < 0) {
		return DOM_INDEX_SIZE_ERR;
	}

	if (c->value != NULL) {
		len = dom_string_length(c->value);
	} else {
		len = 0;
	}

	if (offset > len) {
		return DOM_INDEX_SIZE_ERR;
	}

	end = (offset + count) >= len ? len : offset + count;

	empty = ((struct dom_document *)
		 ((struct dom_node_internal *)c)->owner)->_memo_empty;

	err = dom_string_replace(c->value, empty, offset, end, &temp);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Dispatch a DOMCharacterDataModified event */
	doc = dom_node_get_owner(cdata);
	err = _dom_dispatch_characterdata_modified_event(doc, c, c->value,
			temp, &success);
	if (err != DOM_NO_ERR)
		return err;

	if (c->value != NULL) {
		dom_string_unref(c->value);
	}

	c->value = temp;

	success = true;
	return _dom_dispatch_subtree_modified_event(doc, c->parent, &success);
}

/**
 * Replace a section of a character data node's content
 *
 * \param cdata   The node to modify
 * \param offset  The character offset of the sequence to replace
 * \param count   The number of characters to replace
 * \param data    The replacement data
 * \return DOM_NO_ERR                      on success,
 *         DOM_INDEX_SIZE_ERR              if ::offset is negative or greater 
 *                                         than the number of characters in 
 *                                         ::cdata or ::count is negative,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::cdata is readonly.
 */
dom_exception _dom_characterdata_replace_data(struct dom_characterdata *cdata,
		uint32_t offset, uint32_t count,
		dom_string *data)
{
	struct dom_node_internal *c = (struct dom_node_internal *) cdata;
	dom_string *temp;
	uint32_t len, end;
	dom_exception err;
	struct dom_document *doc;
	bool success = true;

	if (_dom_node_readonly(c)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	if ((int32_t) offset < 0 || (int32_t) count < 0) {
		return DOM_INDEX_SIZE_ERR;
	}

	if (c->value != NULL) {
		len = dom_string_length(c->value);
	} else {
		len = 0;
	}

	if (offset > len) {
		return DOM_INDEX_SIZE_ERR;
	}

	end = (offset + count) >= len ? len : offset + count;

	err = dom_string_replace(c->value, data, offset, end, &temp);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Dispatch a DOMCharacterDataModified event */
	doc = dom_node_get_owner(cdata);
	err = _dom_dispatch_characterdata_modified_event(doc, c, c->value, temp,
			&success);
	if (err != DOM_NO_ERR)
		return err;

	if (c->value != NULL) {
		dom_string_unref(c->value);
	}

	c->value = temp;

	success = true;
	return _dom_dispatch_subtree_modified_event(doc, c->parent, &success);
}

dom_exception _dom_characterdata_get_text_content(dom_node_internal *node,
						  dom_string **result)
{
	dom_characterdata *cdata = (dom_characterdata *)node;
	
	return dom_characterdata_get_data(cdata, result);
}

dom_exception _dom_characterdata_set_text_content(dom_node_internal *node,
						  dom_string *content)
{
	dom_characterdata *cdata = (dom_characterdata *)node;
	
	return dom_characterdata_set_data(cdata, content);
}

/*----------------------------------------------------------------------*/

/* The protected virtual functions of Node, see core/node.h for details */
void _dom_characterdata_destroy(struct dom_node_internal *node)
{
	assert("Should never be here" == NULL);
	UNUSED(node);
}

/* The copy constructor of this class */
dom_exception _dom_characterdata_copy(dom_node_internal *old, 
		dom_node_internal **copy)
{
	dom_characterdata *new_node;
	dom_exception err;

	new_node = malloc(sizeof(dom_characterdata));
	if (new_node == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_characterdata_copy_internal(old, new_node);
	if (err != DOM_NO_ERR) {
		free(new_node);
		return err;
	}

	*copy = (dom_node_internal *) new_node;

	return DOM_NO_ERR;
}

dom_exception _dom_characterdata_copy_internal(dom_characterdata *old,
		dom_characterdata *new)
{
	return dom_node_copy_internal(old, new);
}

