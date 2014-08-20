/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *				http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 James Shaw <jshaw@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggeleyb.nku@gmail.com>
 */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <dom/core/string.h>
#include <dom/core/node.h>

#include "comparators.h"
#include "list.h"
#include "domtsasserts.h"

/**
 * Private helper function.
 * Create a new list_elt and initialise it.
 */
struct list_elt* list_new_elt(void* data);

struct list_elt* list_new_elt(void* data) {
	struct list_elt* elt = malloc(sizeof(struct list_elt));
	assert(elt != NULL);
	elt->data = data;
	elt->next = NULL;
	return elt;
}

struct list* list_new(TYPE type)
{
	struct list* list = malloc(sizeof(struct list));
	assert(list != NULL);
	list->size = 0;
	list->type = type;
	list->head = NULL;
	list->tail = NULL;
	return list;
}

void list_destroy(struct list* list)
{
	struct list_elt* elt = list->head;
	while (elt != NULL) {
		if (list->type == DOM_STRING)
			dom_string_unref((dom_string *) elt->data);
		if (list->type == NODE)
			dom_node_unref(elt->data);
		struct list_elt* nextElt = elt->next;
		free(elt);
		elt = nextElt;
	}
	free(list);
}

void list_add(struct list* list, void* data)
{
	struct list_elt* elt = list_new_elt(data);
	struct list_elt* tail = list->tail;

	/* if tail was set, make its 'next' ptr point to elt */
	if (tail != NULL) {
		tail->next = elt;
	}

	/* make elt the new tail */
	list->tail = elt;

	if (list->head == NULL) {
		list->head = elt;
	}

	/* inc the size of the list */
	list->size++;
	if (list->type == DOM_STRING)
		dom_string_ref((dom_string *) data);
	if (list->type == NODE)
		dom_node_ref(data);
}

bool list_remove(struct list* list, void* data)
{	
	struct list_elt* prevElt = NULL;
	struct list_elt* elt = list->head;
	
	bool found = false;
	
	while (elt != NULL) {
		struct list_elt* nextElt = elt->next;
		
		/* if data is identical, fix up pointers, and free the element */
		if (data == elt->data) {
			if (prevElt == NULL) {
				list->head = nextElt;
			} else {
				prevElt->next = nextElt;
			}
			free(elt);
			list->size--;
			found = true;
			break;
		}
		
		prevElt = elt;
		elt = nextElt;
	}
	
	return found;
}

struct list* list_clone(struct list* list)
{
	struct list* newList = list_new(list->type);
	struct list_elt* elt = list->head;
	
	while (elt != NULL) {
		list_add(newList, elt->data);
		elt = elt->next;
	}
	
	return newList;
}

bool list_contains(struct list* list, void* data, comparator comparator)
{
	struct list_elt* elt = list->head;
	while (elt != NULL) {
		if (comparator(elt->data, data) == 0) {
			return true;
		}
		elt = elt->next;
	}
	return false;
}

bool list_contains_all(struct list* superList, struct list* subList, 
		comparator comparator)
{
	struct list_elt* subElt = subList->head;
	struct list* superListClone = list_clone(superList);
	
	bool found = true;
	
	while (subElt != NULL) {
		struct list_elt* superElt = superListClone->head;

		found = false;
		while (superElt != NULL && found == false) {
			if (comparator(superElt->data, subElt->data) == 0) {
				found = true;
				list_remove(superListClone, superElt->data);
				break;
			}
			superElt = superElt->next;
		}
		
		if (found == false)
			break;
		subElt = subElt->next;
	}

	free(superListClone);
	
	return found;
}

