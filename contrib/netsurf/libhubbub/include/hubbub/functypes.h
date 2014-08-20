/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007-8 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_functypes_h_
#define hubbub_functypes_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <hubbub/types.h>

/**
 * Type of token handling function
 *
 * \param token  Pointer to token to handle
 * \param pw     Pointer to client data
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
typedef hubbub_error (*hubbub_token_handler)(
		const hubbub_token *token, void *pw);

/**
 * Type of parse error handling function
 *
 * \param line     Source line on which error occurred
 * \param col      Column in ::line of start of erroneous input
 * \param message  Error message
 * \param pw       Pointer to client data
 */
typedef void (*hubbub_error_handler)(uint32_t line, uint32_t col,
		const char *message, void *pw);

#ifdef __cplusplus
}
#endif

#endif

