/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_input_filter_h_
#define parserutils_input_filter_h_

#include <inttypes.h>

#include <parserutils/errors.h>
#include <parserutils/functypes.h>

typedef struct parserutils_filter parserutils_filter;

/**
 * Input filter option types
 */
typedef enum parserutils_filter_opttype {
	PARSERUTILS_FILTER_SET_ENCODING       = 0
} parserutils_filter_opttype;

/**
 * Input filter option parameters
 */
typedef union parserutils_filter_optparams {
	/** Parameters for encoding setting */
	struct {
		/** Encoding name */
		const char *name;
	} encoding;
} parserutils_filter_optparams;


/* Create an input filter */
parserutils_error parserutils__filter_create(const char *int_enc,
		parserutils_filter **filter);
/* Destroy an input filter */
parserutils_error parserutils__filter_destroy(parserutils_filter *input);

/* Configure an input filter */
parserutils_error parserutils__filter_setopt(parserutils_filter *input,
		parserutils_filter_opttype type,
		parserutils_filter_optparams *params);

/* Process a chunk of data */
parserutils_error parserutils__filter_process_chunk(parserutils_filter *input,
		const uint8_t **data, size_t *len,
		uint8_t **output, size_t *outlen);

/* Reset an input filter's state */
parserutils_error parserutils__filter_reset(parserutils_filter *input);

#endif

