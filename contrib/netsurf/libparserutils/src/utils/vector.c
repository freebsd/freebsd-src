/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <inttypes.h>
#include <string.h>

#include <parserutils/utils/vector.h>

/**
 * Vector object
 */
struct parserutils_vector
{
	size_t item_size;		/**< Size of an item in the vector */
	size_t chunk_size;		/**< Size of a vector chunk */
	size_t items_allocated;		/**< Number of slots allocated */
	int32_t current_item;		/**< Index of current item */
	void *items;			/**< Items in vector */
};

/**
 * Create a vector
 *
 * \param item_size   Length, in bytes, of an item in the vector
 * \param chunk_size  Number of vector slots in a chunk
 * \param vector      Pointer to location to receive vector instance
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_BADPARM on bad parameters,
 *         PARSERUTILS_NOMEM on memory exhaustion
 */
parserutils_error parserutils_vector_create(size_t item_size, 
		size_t chunk_size, parserutils_vector **vector)
{
	parserutils_vector *v;

	if (item_size == 0 || chunk_size == 0 || vector == NULL)
		return PARSERUTILS_BADPARM;

	v = malloc(sizeof(parserutils_vector));
	if (v == NULL)
		return PARSERUTILS_NOMEM;

	v->items = malloc(item_size * chunk_size);
	if (v->items == NULL) {
		free(v);
		return PARSERUTILS_NOMEM;
	}

	v->item_size = item_size;
	v->chunk_size = chunk_size;
	v->items_allocated = chunk_size;
	v->current_item = -1;

	*vector = v;

	return PARSERUTILS_OK;
}

/**
 * Destroy a vector instance
 *
 * \param vector  The vector to destroy
 * \return PARSERUTILS_OK on success, appropriate error otherwise.
 */
parserutils_error parserutils_vector_destroy(parserutils_vector *vector)
{
	if (vector == NULL)
		return PARSERUTILS_BADPARM;

	free(vector->items);
	free(vector);

	return PARSERUTILS_OK;
}

/**
 * Append an item to the vector
 *
 * \param vector  The vector to append to
 * \param item    The item to append
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_vector_append(parserutils_vector *vector, 
		void *item)
{
	int32_t slot;

	if (vector == NULL || item == NULL)
		return PARSERUTILS_BADPARM;

	/* Ensure we'll get a valid slot */
	if (vector->current_item < -1 || vector->current_item == INT32_MAX)
		return PARSERUTILS_INVALID;

	slot = vector->current_item + 1;

	if ((size_t) slot >= vector->items_allocated) {
		void *temp = realloc(vector->items,
				(vector->items_allocated + vector->chunk_size) *
				vector->item_size);
		if (temp == NULL)
			return PARSERUTILS_NOMEM;

		vector->items = temp;
		vector->items_allocated += vector->chunk_size;
	}

	memcpy((uint8_t *) vector->items + (slot * vector->item_size), 
			item, vector->item_size);
	vector->current_item = slot;

	return PARSERUTILS_OK;
}

/**
 * Clear a vector
 *
 * \param vector  The vector to clear
 * \return PARSERUTILS_OK on success, appropriate error otherwise.
 */
parserutils_error parserutils_vector_clear(parserutils_vector *vector)
{
	if (vector == NULL)
		return PARSERUTILS_BADPARM;

	if (vector->current_item < 0)
		return PARSERUTILS_INVALID;

	vector->current_item = -1;

	return PARSERUTILS_OK;
}

/**
 * Remove the last item from a vector
 *
 * \param vector  The vector to remove from
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_vector_remove_last(parserutils_vector *vector)
{
	if (vector == NULL)
		return PARSERUTILS_BADPARM;

	if (vector->current_item < 0)
		return PARSERUTILS_INVALID;

	vector->current_item--;

	return PARSERUTILS_OK;
}

/**
 * Acquire the length (in items) of the vector.
 *
 * \param vector  The vector to interrogate.
 * \param length  Pointer to location to receive length information.
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_vector_get_length(parserutils_vector *vector,
                                                size_t *length)
{
        if (vector == NULL)
                return PARSERUTILS_BADPARM;
        
        if (length == NULL)
                return PARSERUTILS_BADPARM;
        
        *length = vector->current_item + 1;
        
        return PARSERUTILS_OK;
}

/**
 * Iterate over a vector
 *
 * \param vector  The vector to iterate over
 * \param ctx     Pointer to an integer for the iterator to use as context.
 * \return Pointer to current item, or NULL if no more
 *
 * \note The value pointed to by \a ctx must be zero to begin the iteration.
 */
const void *parserutils_vector_iterate(const parserutils_vector *vector, 
		int32_t *ctx)
{
	void *item;

	if (vector == NULL || ctx == NULL || vector->current_item < 0)
		return NULL;

	if ((*ctx) > vector->current_item)
		return NULL;

	item = (uint8_t *) vector->items + ((*ctx) * vector->item_size);

	(*ctx)++;

	return item;
}

/**
 * Peek at an item in a vector
 *
 * \param vector  The vector to iterate over
 * \param ctx     Integer for the iterator to use as context.
 * \return Pointer to item, or NULL if no more
 */
const void *parserutils_vector_peek(const parserutils_vector *vector, 
		int32_t ctx)
{
	if (vector == NULL || vector->current_item < 0)
		return NULL;

	if (ctx > vector->current_item)
		return NULL;

	return (uint8_t *) vector->items + (ctx * vector->item_size);
}


#ifndef NDEBUG
#include <stdio.h>

extern void parserutils_vector_dump(parserutils_vector *vector, 
		const char *prefix, void (*printer)(void *item));

void parserutils_vector_dump(parserutils_vector *vector, const char *prefix,
		void (*printer)(void *item))
{
	int32_t i;

	if (vector == NULL || printer == NULL)
		return;

	for (i = 0; i <= vector->current_item; i++) {
		printf("%s %d: ", prefix != NULL ? prefix : "", i);
		printer((uint8_t *) vector->items + (i * vector->item_size));
		printf("\n");
	}
}

#endif

