/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_characterdata_h_
#define dom_core_characterdata_h_

#include <dom/core/exceptions.h>
#include <dom/core/node.h>

typedef struct dom_characterdata dom_characterdata;

/* The vtable for characterdata */
typedef struct dom_characterdata_vtable {
	struct dom_node_vtable base;

	dom_exception (*dom_characterdata_get_data)(
			struct dom_characterdata *cdata,
			dom_string **data);
	dom_exception (*dom_characterdata_set_data)(
			struct dom_characterdata *cdata,
			dom_string *data);
	dom_exception (*dom_characterdata_get_length)(
			struct dom_characterdata *cdata,
			uint32_t *length);
	dom_exception (*dom_characterdata_substring_data)(
			struct dom_characterdata *cdata, uint32_t offset,
			uint32_t count, dom_string **data);
	dom_exception (*dom_characterdata_append_data)(
			struct dom_characterdata *cdata,
			dom_string *data);
	dom_exception (*dom_characterdata_insert_data)(
			struct dom_characterdata *cdata,
			uint32_t offset, dom_string *data);
	dom_exception (*dom_characterdata_delete_data)(
			struct dom_characterdata *cdata,
			uint32_t offset, uint32_t count);
	dom_exception (*dom_characterdata_replace_data)(
			struct dom_characterdata *cdata, uint32_t offset,
			uint32_t count, dom_string *data);
} dom_characterdata_vtable;


static inline dom_exception dom_characterdata_get_data(
		struct dom_characterdata *cdata, dom_string **data)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_get_data(cdata, data);
}
#define dom_characterdata_get_data(c, d) dom_characterdata_get_data( \
		(struct dom_characterdata *) (c), (d))

static inline dom_exception dom_characterdata_set_data(
		struct dom_characterdata *cdata, dom_string *data)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_set_data(cdata, data);
}
#define dom_characterdata_set_data(c, d) dom_characterdata_set_data( \
		(struct dom_characterdata *) (c), (d))

static inline dom_exception dom_characterdata_get_length(
		struct dom_characterdata *cdata, uint32_t *length)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_get_length(cdata, length);
}
#define dom_characterdata_get_length(c, l) dom_characterdata_get_length( \
		(struct dom_characterdata *) (c), (uint32_t *) (l))

static inline dom_exception dom_characterdata_substring_data(
		struct dom_characterdata *cdata, uint32_t offset,
		uint32_t count, dom_string **data)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_substring_data(cdata, offset, count,
			data);
}
#define dom_characterdata_substring_data(c, o, ct, d) \
		dom_characterdata_substring_data( \
		(struct dom_characterdata *) (c), (uint32_t) (o), \
		(uint32_t) (ct), (d))

static inline dom_exception dom_characterdata_append_data(
		struct dom_characterdata *cdata, dom_string *data)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_append_data(cdata, data);
}
#define dom_characterdata_append_data(c, d) dom_characterdata_append_data( \
		(struct dom_characterdata *) (c), (d))

static inline dom_exception dom_characterdata_insert_data(
		struct dom_characterdata *cdata, uint32_t offset, 
		dom_string *data)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_insert_data(cdata, offset, data);
}
#define dom_characterdata_insert_data(c, o, d) dom_characterdata_insert_data( \
		(struct dom_characterdata *) (c), (uint32_t) (o), (d))

static inline dom_exception dom_characterdata_delete_data(
		struct dom_characterdata *cdata, uint32_t offset, 
		uint32_t count)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_delete_data(cdata, offset, count);
}
#define dom_characterdata_delete_data(c, o, ct) dom_characterdata_delete_data(\
		(struct dom_characterdata *) (c), (uint32_t) (o), \
		(uint32_t) (ct))

static inline dom_exception dom_characterdata_replace_data(
		struct dom_characterdata *cdata, uint32_t offset, 
		uint32_t count, dom_string *data)
{
	return ((dom_characterdata_vtable *) ((dom_node *) cdata)->vtable)->
			dom_characterdata_replace_data(cdata, offset, count, 
			data);
}
#define dom_characterdata_replace_data(c, o, ct, d) \
		dom_characterdata_replace_data(\
		(struct dom_characterdata *) (c), (uint32_t) (o),\
		(uint32_t) (ct), (d))

#endif
