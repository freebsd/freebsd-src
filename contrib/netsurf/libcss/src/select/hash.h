/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_select_hash_h_
#define css_select_hash_h_

#include <libwapcaplet/libwapcaplet.h>

#include <libcss/errors.h>
#include <libcss/functypes.h>

#include "select/bloom.h"

/* Ugh. We need this to avoid circular includes. Happy! */
struct css_selector;

typedef struct css_selector_hash css_selector_hash;

struct css_hash_selection_requirments {
	css_qname qname;		/* Element name, or universal "*" */
	lwc_string *class;		/* Name of class, or NULL */
	lwc_string *id;			/* Name of id, or NULL */
	lwc_string *uni;		/* Universal element string "*" */
	uint64_t media;			/* Media type(s) we're selecting for */
	const css_bloom *node_bloom;	/* Node's bloom filter */
};

typedef css_error (*css_selector_hash_iterator)(
		const struct css_hash_selection_requirments *req,
		const struct css_selector **current,
		const struct css_selector ***next);

css_error css__selector_hash_create(css_selector_hash **hash);
css_error css__selector_hash_destroy(css_selector_hash *hash);

css_error css__selector_hash_insert(css_selector_hash *hash,
		const struct css_selector *selector);
css_error css__selector_hash_remove(css_selector_hash *hash,
		const struct css_selector *selector);

css_error css__selector_hash_find(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const struct css_selector ***matched);
css_error css__selector_hash_find_by_class(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const struct css_selector ***matched);
css_error css__selector_hash_find_by_id(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const struct css_selector ***matched);
css_error css__selector_hash_find_universal(css_selector_hash *hash,
		const struct css_hash_selection_requirments *req,
		css_selector_hash_iterator *iterator,
		const struct css_selector ***matched);

css_error css__selector_hash_size(css_selector_hash *hash, size_t *size);

#endif

