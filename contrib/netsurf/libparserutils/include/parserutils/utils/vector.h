/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_utils_vector_h_
#define parserutils_utils_vector_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include <parserutils/errors.h>
#include <parserutils/functypes.h>

struct parserutils_vector;
typedef struct parserutils_vector parserutils_vector;

parserutils_error parserutils_vector_create(size_t item_size, 
		size_t chunk_size, parserutils_vector **vector);
parserutils_error parserutils_vector_destroy(parserutils_vector *vector);

parserutils_error parserutils_vector_append(parserutils_vector *vector, 
		void *item);
parserutils_error parserutils_vector_clear(parserutils_vector *vector);
parserutils_error parserutils_vector_remove_last(parserutils_vector *vector);
parserutils_error parserutils_vector_get_length(parserutils_vector *vector, size_t *length);

const void *parserutils_vector_iterate(const parserutils_vector *vector, 
		int32_t *ctx);
const void *parserutils_vector_peek(const parserutils_vector *vector,
		int32_t ctx);

#ifdef __cplusplus
}
#endif

#endif

