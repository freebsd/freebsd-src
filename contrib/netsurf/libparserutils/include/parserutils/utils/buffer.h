/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_utils_buffer_h_
#define parserutils_utils_buffer_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <parserutils/errors.h>
#include <parserutils/functypes.h>

struct parserutils_buffer
{
	uint8_t *data;
	size_t length;
	size_t allocated;
};
typedef struct parserutils_buffer parserutils_buffer;

parserutils_error parserutils_buffer_create(parserutils_buffer **buffer);
parserutils_error parserutils_buffer_destroy(parserutils_buffer *buffer);

parserutils_error parserutils_buffer_append(parserutils_buffer *buffer, 
		const uint8_t *data, size_t len);
parserutils_error parserutils_buffer_insert(parserutils_buffer *buffer, 
		size_t offset, const uint8_t *data, size_t len);
parserutils_error parserutils_buffer_discard(parserutils_buffer *buffer, 
		size_t offset, size_t len);

parserutils_error parserutils_buffer_grow(parserutils_buffer *buffer);

parserutils_error parserutils_buffer_randomise(parserutils_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif

