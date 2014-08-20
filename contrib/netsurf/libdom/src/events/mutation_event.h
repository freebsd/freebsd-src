/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_interntal_events_mutation_event_h_
#define dom_interntal_events_mutation_event_h_

#include <dom/events/mutation_event.h>

#include "events/event.h"

/**
 * The MutationEvent
 */
struct dom_mutation_event {
	struct dom_event base;

	struct dom_node *related_node;
	dom_string *prev_value;
	dom_string *new_value;
	dom_string *attr_name;
	dom_mutation_type change;
};

/* Constructor */
dom_exception _dom_mutation_event_create(struct dom_document *doc, 
		struct dom_mutation_event **evt);

/* Destructor */
void _dom_mutation_event_destroy(struct dom_mutation_event *evt);

/* Initialise function */
dom_exception _dom_mutation_event_initialise(struct dom_document *doc, 
		struct dom_mutation_event *evt);

/* Finalise function */
void _dom_mutation_event_finalise(struct dom_mutation_event *evt);

#endif

