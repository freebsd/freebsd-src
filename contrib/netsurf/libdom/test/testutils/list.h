/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 James Shaw <jshaw@netsurf-browser.org>
 */

#ifndef list_h_
#define list_h_

#include <stdbool.h>

#include "comparators.h"

/* The element type in the list
 * 
 * The high byte is used for category type
 * The low byte is used for concrete type
 */
typedef enum TYPE {
	INT = 0x0001,
	STRING = 0x0100,
	DOM_STRING = 0x0101,
	NODE = 0x0200
} TYPE;


struct list_elt {
	void* data;
	struct list_elt* next;
};

typedef struct list {
	unsigned int size;
	TYPE type;
	struct list_elt* head;
	struct list_elt* tail;
} list;

struct list* list_new(TYPE type);
void list_destroy(struct list* list);

/**
 * Add data to the tail of the list.
 */
void list_add(struct list* list, void* data);

/**
 * Remove element containing data from list.
 * The list element is freed, but the caller must free the data itself
 * if necessary.
 * 
 * Returns true if data was found in the list.
 */
bool list_remove(struct list* list, void* data);

struct list* list_clone(struct list* list);
/**
 * Tests if data is equal to any element in the list.
 */
bool list_contains(struct list* list, void* data,
		comparator comparator);

/**
 * Tests if superlist contains all elements in sublist.  Order is not important.
 */
bool list_contains_all(struct list* superList, struct list* subList, 
		comparator comparator);

#endif
