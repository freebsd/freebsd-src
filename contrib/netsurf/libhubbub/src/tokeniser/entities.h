/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_tokeniser_entities_h_
#define hubbub_tokeniser_entities_h_

#include <inttypes.h>

#include <hubbub/errors.h>
#include <hubbub/functypes.h>

/* Step-wise search for an entity in the dictionary */
hubbub_error hubbub_entities_search_step(uint8_t c, uint32_t *result,
		int32_t *context);

#endif
