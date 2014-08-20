/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_interntal_events_mutation_name_event_h_
#define dom_interntal_events_mutation_name_event_h_

#include <dom/events/mutation_name_event.h>

#include "events/mutation_event.h"

/**
 * The MutationName event 
 */
struct dom_mutation_name_event {
	struct dom_mutation_event base;

	dom_string *prev_namespace;
	dom_string *prev_nodename;
};

/* Constructor */
dom_exception _dom_mutation_name_event_create(struct dom_document *doc, 
		struct dom_mutation_name_event **evt);

/* Destructor */
void _dom_mutation_name_event_destroy(struct dom_mutation_name_event *evt);

/* Initialise function */
dom_exception _dom_mutation_name_event_initialise(struct dom_document *doc, 
		struct dom_mutation_name_event *evt);

/* Finalise function */
void _dom_mutation_name_event_finalise(struct dom_mutation_name_event *evt);

#endif

