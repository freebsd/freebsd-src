/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_attr_h_
#define dom_core_attr_h_

#include <stdbool.h>

#include <dom/core/exceptions.h>
#include <dom/core/node.h>

struct dom_element;
struct dom_type_info;
struct dom_node;
struct dom_attr;

typedef struct dom_attr dom_attr;

/**
 * The attribute type
 */
typedef enum {
	DOM_ATTR_UNSET = 0,
	DOM_ATTR_STRING,
	DOM_ATTR_BOOL,
	DOM_ATTR_SHORT,
	DOM_ATTR_INTEGER
} dom_attr_type;

/* DOM Attr vtable */
typedef struct dom_attr_vtable {
	struct dom_node_vtable base;

	dom_exception (*dom_attr_get_name)(struct dom_attr *attr,
			dom_string **result);
	dom_exception (*dom_attr_get_specified)(struct dom_attr *attr,
			bool *result);
	dom_exception (*dom_attr_get_value)(struct dom_attr *attr,
			dom_string **result);
	dom_exception (*dom_attr_set_value)(struct dom_attr *attr,
			dom_string *value);
	dom_exception (*dom_attr_get_owner_element)(struct dom_attr *attr,
			struct dom_element **result);
	dom_exception (*dom_attr_get_schema_type_info)(struct dom_attr *attr,
			struct dom_type_info **result);
	dom_exception (*dom_attr_is_id)(struct dom_attr *attr, bool *result);
} dom_attr_vtable;

static inline dom_exception dom_attr_get_name(struct dom_attr *attr,
		dom_string **result)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_get_name(attr, result);
}
#define dom_attr_get_name(a, r) dom_attr_get_name((struct dom_attr *) (a), (r))

static inline dom_exception dom_attr_get_specified(struct dom_attr *attr,
		bool *result)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_get_specified(attr, result);
}
#define dom_attr_get_specified(a, r) dom_attr_get_specified( \
		(struct dom_attr *) (a), (bool *) (r))

static inline dom_exception dom_attr_get_value(struct dom_attr *attr,
		dom_string **result)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_get_value(attr, result);
}
#define dom_attr_get_value(a, r) dom_attr_get_value((struct dom_attr *) (a), (r))

static inline dom_exception dom_attr_set_value(struct dom_attr *attr,
		dom_string *value)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_set_value(attr, value);
}
#define dom_attr_set_value(a, v) dom_attr_set_value((struct dom_attr *) (a), (v))

static inline dom_exception dom_attr_get_owner_element(struct dom_attr *attr,
		struct dom_element **result)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_get_owner_element(attr, result);
}
#define dom_attr_get_owner_element(a, r) dom_attr_get_owner_element(\
		(struct dom_attr *) (a), (struct dom_element **) (r))

static inline dom_exception dom_attr_get_schema_type_info(
		struct dom_attr *attr, struct dom_type_info **result)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_get_schema_type_info(attr, result);
}
#define dom_attr_get_schema_type_info(a, r) dom_attr_get_schema_type_info( \
		(struct dom_attr *) (a), (struct dom_type_info **) (r))

static inline dom_exception dom_attr_is_id(struct dom_attr *attr, bool *result)
{
	return ((dom_attr_vtable *) ((dom_node *) attr)->vtable)->
			dom_attr_is_id(attr, result);
}
#define dom_attr_is_id(a, r) dom_attr_is_id((struct dom_attr *) (a), \
		(bool *) (r))

/*-----------------------------------------------------------------------*/
/**
 * Following are our implementation specific APIs.
 *
 * These APIs are defined for the purpose that there are some attributes in
 * HTML and other DOM module whose type is not DOMString, but uint32_t or
 * boolean, for those types of attributes, clients should call one of the
 * following APIs to set it. 
 *
 * When an Attr node is created, its type is unset and it can be turned into
 * any of the four types. Once the type is fixed by calling any of the four
 * APIs:
 * dom_attr_set_value
 * dom_attr_set_integer
 * dom_attr_set_short
 * dom_attr_set_bool
 * it can't be modified in future. 
 *
 * For integer/short/bool type of attributes, we provide no string
 * repensentation of them, so when you call dom_attr_get_value on these
 * three type of attribute nodes, you will always get a empty dom_string.
 * If you want to do something with Attr node, you must know its type
 * firstly by calling dom_attr_get_type before you decide to call other
 * dom_attr_get_* functions.
 */
dom_attr_type dom_attr_get_type(dom_attr *a);
dom_exception dom_attr_get_integer(dom_attr *a, uint32_t *value);
dom_exception dom_attr_set_integer(dom_attr *a, uint32_t value);
dom_exception dom_attr_get_short(dom_attr *a, unsigned short *value);
dom_exception dom_attr_set_short(dom_attr *a, unsigned short value);
dom_exception dom_attr_get_bool(dom_attr *a, bool *value);
dom_exception dom_attr_set_bool(dom_attr *a, bool value);
/* Make a attribute node readonly */
void dom_attr_mark_readonly(dom_attr *a);

#endif
