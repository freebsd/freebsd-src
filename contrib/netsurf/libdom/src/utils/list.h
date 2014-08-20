/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 *
 * This file contains the list structure used to compose lists. 
 * 
 * Note: This is a implementation of a doubld-linked cyclar list.
 */

#ifndef dom_utils_list_h_
#define dom_utils_list_h_

#include <stddef.h>

struct list_entry {
	struct list_entry *prev;
	struct list_entry *next;
};

/**
 * Initialise a list_entry structure
 *
 * \param ent  The entry to initialise
 */
static inline void list_init(struct list_entry *ent)
{
	ent->prev = ent;
	ent->next = ent;
}

/**
 * Append a new list_entry after the list
 *
 * \param head  The list header
 * \param new   The new entry
 */
static inline void list_append(struct list_entry *head, struct list_entry *new)
{
	new->next = head;
	new->prev = head->prev;
	head->prev->next = new;
	head->prev = new;
}

/**
 * Delete a list_entry from the list
 *
 * \param entry  The entry need to be deleted from the list
 */
static inline void list_del(struct list_entry *ent)
{
	ent->prev->next = ent->next;
	ent->next->prev = ent->prev;

	ent->prev = ent;
	ent->next = ent;
}

#endif
