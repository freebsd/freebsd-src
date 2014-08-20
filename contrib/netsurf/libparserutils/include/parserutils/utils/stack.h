/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_utils_stack_h_
#define parserutils_utils_stack_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include <parserutils/errors.h>
#include <parserutils/functypes.h>

struct parserutils_stack;
typedef struct parserutils_stack parserutils_stack;

parserutils_error parserutils_stack_create(size_t item_size, size_t chunk_size,
		parserutils_stack **stack);
parserutils_error parserutils_stack_destroy(parserutils_stack *stack);

parserutils_error parserutils_stack_push(parserutils_stack *stack, 
		const void *item);
parserutils_error parserutils_stack_pop(parserutils_stack *stack, void *item);

void *parserutils_stack_get_current(parserutils_stack *stack);

#ifdef __cplusplus
}
#endif

#endif

