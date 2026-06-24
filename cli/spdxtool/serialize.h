/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 *​
 *​ Copyright (C) 2026 Elizabeth Ashford.
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"

#ifndef CLI__SPDXTOOL__SERIALIZE_H
#define CLI__SPDXTOOL__SERIALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum spdxtool_serialize_type_
{
	SPDXTOOL_SERIALIZE_TYPE_STRING, // JSON string type
	SPDXTOOL_SERIALIZE_TYPE_INT, // JSON number type (int)
	SPDXTOOL_SERIALIZE_TYPE_BOOL, // JSON bool type
	SPDXTOOL_SERIALIZE_TYPE_NULL, // JSON null type
	SPDXTOOL_SERIALIZE_TYPE_OBJECT, // JSON object type
	SPDXTOOL_SERIALIZE_TYPE_ARRAY // JSON array type
} spdxtool_serialize_type_t;

typedef struct spdxtool_serialize_value_ {
	spdxtool_serialize_type_t type;
	union {
		char *s;
		int i;
		bool b;
		struct spdxtool_serialize_object_list_ *o;
		struct spdxtool_serialize_array_ *a;
	} value;
} spdxtool_serialize_value_t;

typedef struct spdxtool_serialize_object_ {
	char *key;
	spdxtool_serialize_value_t *value;
} spdxtool_serialize_object_t;

typedef struct spdxtool_serialize_object_list_ {
	pkgconf_list_t entries;
} spdxtool_serialize_object_list_t;

typedef struct spdxtool_serialize_array_ {
	pkgconf_list_t items;
} spdxtool_serialize_array_t;

bool
spdxtool_serialize_value_to_buf(pkgconf_buffer_t *buffer, spdxtool_serialize_value_t *value, unsigned int indent);

spdxtool_serialize_value_t *
spdxtool_serialize_value_dup(const spdxtool_serialize_value_t *value);

spdxtool_serialize_value_t *
spdxtool_serialize_object_add_take(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_value_t* value);

spdxtool_serialize_object_list_t *
spdxtool_serialize_object_list_new(void);

spdxtool_serialize_array_t *
spdxtool_serialize_array_new(void);

spdxtool_serialize_value_t *
spdxtool_serialize_array_add_take(spdxtool_serialize_array_t *array, spdxtool_serialize_value_t* value);

void
spdxtool_serialize_value_free(spdxtool_serialize_value_t *value);

void
spdxtool_serialize_object_list_free(spdxtool_serialize_object_list_t *object_list);

void
spdxtool_serialize_object_free(spdxtool_serialize_object_t *object);

void
spdxtool_serialize_array_free(spdxtool_serialize_array_t *array);

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t * spdxtool_serialize_value_string(const char *s)
 *
 *    Construct a JSON string value. The string is copied internally.
 *    If this return value is not stolen, it must be freed with spdxtool_serialize_value_free().
 *
 *    :param const char *s: String to copy. May be NULL, in which case the value holds NULL.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_STRING.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_value_string(const char *s)
{
	if (!s)
		return NULL;

	char *sv = strdup(s);
	if (!sv)
		return NULL;

	spdxtool_serialize_value_t *value = calloc(1, sizeof(spdxtool_serialize_value_t));
	if (!value)
	{
		free(sv);
		return NULL;
	}

	value->type = SPDXTOOL_SERIALIZE_TYPE_STRING;
	value->value.s = sv;
	return value;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_value_int(int d)
 *
 *    Construct a JSON integer value.
 *    If this return value is not stolen, it must be freed with spdxtool_serialize_value_free().
 *
 *    :param int d: int value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_INT.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_value_int(int i)
{
	spdxtool_serialize_value_t *value = calloc(1, sizeof(spdxtool_serialize_value_t));
	if (!value)
		return NULL;

	value->type = SPDXTOOL_SERIALIZE_TYPE_INT;
	value->value.i = i;
	return value;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_value_bool(bool b)
 *
 *    Construct a JSON boolean value.
 *    If this return value is not stolen, it must be freed with spdxtool_serialize_value_free().
 *
 *    :param bool b: Boolean value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_BOOL.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_value_bool(bool b)
{
	spdxtool_serialize_value_t *value = calloc(1, sizeof(spdxtool_serialize_value_t));
	if (!value)
		return NULL;

	value->type = SPDXTOOL_SERIALIZE_TYPE_BOOL;
	value->value.b = b;
	return value;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_value_null(void)
 *
 *    Construct a JSON null value.
 *    If this return value is not stolen, it must be freed with spdxtool_serialize_value_free().
 *
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_NULL.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_value_null(void)
{
	spdxtool_serialize_value_t *value = calloc(1, sizeof(spdxtool_serialize_value_t));
	if (!value)
		return NULL;

	value->type = SPDXTOOL_SERIALIZE_TYPE_NULL;
	return value;
}


/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_value_object(spdxtool_serialize_object_list_t *object_list)
 *
 *    Construct a JSON object value wrapping an existing object list.
 *    The returned value takes ownership of the object list.
 *    If this return value is not stolen, it must be freed with spdxtool_serialize_value_free().
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to wrap. Ownership transfers to the returned value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_OBJECT.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_value_object(spdxtool_serialize_object_list_t *object_list)
{
	spdxtool_serialize_value_t *value = calloc(1, sizeof(spdxtool_serialize_value_t));
	if (!value)
		return NULL;

	value->type = SPDXTOOL_SERIALIZE_TYPE_OBJECT;
	value->value.o = object_list;
	return value;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_value_array(spdxtool_serialize_array_t *array)
 *
 *    Construct a JSON array value wrapping an existing array.
 *    The returned value takes ownership of the array.
 *    If this return value is not stolen, it must be freed with spdxtool_serialize_value_free().
 *
 *    :param spdxtool_serialize_array_t *array: Array to wrap. Ownership transfers to the returned value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_ARRAY.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_value_array(spdxtool_serialize_array_t *array)
{
	spdxtool_serialize_value_t *value = calloc(1, sizeof(spdxtool_serialize_value_t));
	if (!value)
		return NULL;

	value->type = SPDXTOOL_SERIALIZE_TYPE_ARRAY;
	value->value.a = array;
	return value;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_object_add_string(spdxtool_serialize_object_list_t *object_list, const char *key, const char *value)
 *
 *    Add a string key-value pair to a JSON object. The string is copied internally.
 *    Unconditionally adds the key even if value is NULL.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :param const char *value: String value to copy.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_STRING, located in the object.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_string(spdxtool_serialize_object_list_t *object_list, const char *key, const char *value)
{
	return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_string(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_string_opt(spdxtool_serialize_object_list_t *object_list, const char *key, const char *value)
 *
 *    Add a string key-value pair to a JSON object only if value is non-NULL.
 *    Use this for optional fields that should be omitted entirely when absent.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :param const char *value: String value to copy, or NULL to skip.
 *    :return: If value is set: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_STRING, located in the object.
 *             This object is not owned by the caller.
 *             If value is not set: NULL.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_string_opt(spdxtool_serialize_object_list_t *object_list, const char *key, const char *value)
{
	if (value)
		return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_string(value));

	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_int(spdxtool_serialize_object_list_t *object_list, const char *key, int value)
 *
 *    Add a int key-value pair to a JSON object.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :param int value: Integer value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_INT, located in the object.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_int(spdxtool_serialize_object_list_t *object_list, const char *key, int value)
{
	return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_int(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_bool(spdxtool_serialize_object_list_t *object_list, const char *key, bool value)
 *
 *    Add a boolean key-value pair to a JSON object.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :param bool value: Boolean value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_BOOL, located in the object.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_bool(spdxtool_serialize_object_list_t *object_list, const char *key, bool value)
{
	return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_bool(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_null(spdxtool_serialize_object_list_t *object_list, const char *key)
 *
 *    Add a null key-value pair to a JSON object.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_NULL, located in the object.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_null(spdxtool_serialize_object_list_t *object_list, const char *key)
{
	return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_null());
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_object(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_object_list_t *value)
 *
 *    Add an object key-value pair to a JSON object.
 *    This takes ownership of the object in value unconditionally, freeing on failure.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :param spdxtool_serialize_object_list_t *value: Object value to add.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_OBJECT, located in the object.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_object(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_object_list_t *value)
{
	return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_object(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_array(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_array_t *value)
 *
 *    Add an array key-value pair to a JSON object.
 *    This takes ownership of the array in value unconditionally, freeing on failure.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string.
 *    :param spdxtool_serialize_array_t *value: Array value to add.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_ARRAY, located in the object.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_object_add_array(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_array_t *value)
{
	return spdxtool_serialize_object_add_take(object_list, key, spdxtool_serialize_value_array(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_string(spdxtool_serialize_array_t *array, const char *value)
 *
 *    Append a string value to a JSON array. The string is copied internally.
 *    Unconditionally appends even if value is NULL.
 *
 *    :param spdxtool_serialize_array_t *array: Array to append to.
 *    :param const char *value: String value to copy.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_STRING, located in the array.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_string(spdxtool_serialize_array_t *array, const char *value)
{
	return spdxtool_serialize_array_add_take(array, spdxtool_serialize_value_string(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_string_opt(spdxtool_serialize_array_t *a, const char *value)
 *
 *    Append a string value to a JSON array only if value is non-NULL.
 *    Use this for optional array entries that should be omitted when absent.
 *
 *    :param spdxtool_serialize_array_t *a: Array to append to.
 *    :param const char *value: String value to copy, or NULL to skip.
 *    :return: If value is set: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_STRING, located in the array.
 *             This object is not owned by the caller.
 *             If value is not set: NULL.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_string_opt(spdxtool_serialize_array_t *array, const char *value)
{
	if (value)
		return spdxtool_serialize_array_add_take(array, spdxtool_serialize_value_string(value));

	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_int(spdxtool_serialize_array_t *array, int value)
 *
 *    Append a int value to a JSON array.
 *
 *    :param spdxtool_serialize_array_t *array: Array to append to.
 *    :param int value: integer value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_INT, located in the array.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_int(spdxtool_serialize_array_t *array, int value)
{
	return spdxtool_serialize_array_add_take(array, spdxtool_serialize_value_int(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_bool(spdxtool_serialize_array_t *array, bool value)
 *
 *    Append a boolean value to a JSON array.
 *
 *    :param spdxtool_serialize_array_t *array: Array to append to.
 *    :param bool value: Boolean value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_BOOL, located in the array.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_bool(spdxtool_serialize_array_t *array, bool value)
{
	return spdxtool_serialize_array_add_take(array, spdxtool_serialize_value_bool(value));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_null(spdxtool_serialize_array_t *array)
 *
 *    Append a null value to a JSON array.
 *
 *    :param spdxtool_serialize_array_t *array: Array to append to.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_NULL, located in the array.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_null(spdxtool_serialize_array_t *array)
{
	return spdxtool_serialize_array_add_take(array, spdxtool_serialize_value_null());
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_object(spdxtool_serialize_array_t *array, spdxtool_serialize_object_list_t *value)
 *
 *    Append an object value to a JSON array.
 *    This takes ownership of the object in value unconditionally, freeing on failure.
 *
 *    :param spdxtool_serialize_array_t *array: Array to append to.
 *    :param spdxtool_serialize_object_list_t *value: Object value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_OBJECT, located in the array.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_object(spdxtool_serialize_array_t *array, spdxtool_serialize_object_list_t *value)
{
	if (!value)
		return NULL;

	spdxtool_serialize_value_t *ret = spdxtool_serialize_value_object(value);
	if (!ret)
	{
		// Since we take possession of the pointer unconditionally, clean up.
		spdxtool_serialize_object_list_free(value);
		return NULL;
	}

	return spdxtool_serialize_array_add_take(array, ret);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_array(spdxtool_serialize_array_t *array, spdxtool_serialize_array_t *value)
 *
 *    Append an array value to a JSON array.
 *    This takes ownership of the array in value unconditionally, freeing on failure.
 *
 *    :param spdxtool_serialize_array_t *array: Array to append to.
 *    :param spdxtool_serialize_array_t *value: Array value.
 *    :return: spdxtool_serialize_value_t * of type SPDXTOOL_SERIALIZE_TYPE_ARRAY, located in the array.
 *             This object is not owned by the caller.
 */
static inline spdxtool_serialize_value_t *
spdxtool_serialize_array_add_array(spdxtool_serialize_array_t *array, spdxtool_serialize_array_t *value)
{
	if (!value)
		return NULL;

	spdxtool_serialize_value_t *ret = spdxtool_serialize_value_array(value);
	if (!ret)
	{
		// Since we take possession of the pointer unconditionally, clean up.
		spdxtool_serialize_array_free(value);
		return NULL;
	}

	return spdxtool_serialize_array_add_take(array, ret);
}

spdxtool_serialize_value_t *
spdxtool_serialize_sbom(pkgconf_client_t *client, spdxtool_core_agent_t *agent, spdxtool_core_creation_info_t *creation, spdxtool_core_spdx_document_t *spdx);

#ifdef __cplusplus
}
#endif

#endif
