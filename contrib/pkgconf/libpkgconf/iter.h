/*
 * iter.h
 * Linked lists and iterators.
 *
 * Copyright (c) 2013 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef LIBPKGCONF_ITER_H
#define LIBPKGCONF_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pkgconf_node_ pkgconf_node_t;

struct pkgconf_node_ {
	pkgconf_node_t *prev, *next;
	void *data;
};

typedef struct {
	pkgconf_node_t *head, *tail;
	size_t length;
} pkgconf_list_t;

#define PKGCONF_LIST_INITIALIZER		{ NULL, NULL, 0 }

static inline void
pkgconf_list_zero(pkgconf_list_t *list)
{
	list->head = NULL;
	list->tail = NULL;
	list->length = 0;
}

static inline void
pkgconf_node_insert(pkgconf_node_t *node, void *data, pkgconf_list_t *list)
{
	pkgconf_node_t *tnode;

	node->data = data;

	if (list->head == NULL)
	{
		list->head = node;
		list->tail = node;
		list->length = 1;
		return;
	}

	tnode = list->head;

	node->next = tnode;
	tnode->prev = node;

	list->head = node;
	list->length++;
}

static inline void
pkgconf_node_insert_tail(pkgconf_node_t *node, void *data, pkgconf_list_t *list)
{
	pkgconf_node_t *tnode;

	node->data = data;

	if (list->tail == NULL)
	{
		list->head = node;
		list->tail = node;
		list->length = 1;
		return;
	}

	tnode = list->tail;

	node->prev = tnode;
	tnode->next = node;

	list->tail = node;
	list->length++;
}

static inline void
pkgconf_node_delete(pkgconf_node_t *node, pkgconf_list_t *list)
{
	list->length--;

	if (node->prev == NULL)
		list->head = node->next;
	else
		node->prev->next = node->next;

	if (node->next == NULL)
		list->tail = node->prev;
	else
		node->next->prev = node->prev;
}

#ifdef __cplusplus
}
#endif

#endif
