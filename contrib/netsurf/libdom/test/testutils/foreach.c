/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdbool.h>

#include "foreach.h"
#include "list.h"

#include <dom/dom.h>

/**
 * Please see foreach.h for the usage of the following functions 
 */

void foreach_initialise_domnodelist(dom_nodelist *list, unsigned int *iterator)
{
        (void)list;
	*iterator = 0;
}

void foreach_initialise_list(list *list, unsigned int *iterator)
{
        (void)list;
	*iterator = 0;
}

void foreach_initialise_domnamednodemap(dom_namednodemap *map, unsigned int *iterator)
{
        (void)map;
	*iterator = 0;
}

void foreach_initialise_domhtmlcollection(dom_html_collection *coll, unsigned int *iterator)
{
	(void)coll;
	*iterator = 0;
}

bool _get_next_domnodelist(dom_nodelist *list, unsigned int *iterator, dom_node **ret)
{
	dom_exception err;
	uint32_t len;

	err = dom_nodelist_get_length(list, &len);
	if (err != DOM_NO_ERR)
		return false;

	if (*iterator >= len)
		return false;

	err = dom_nodelist_item(list, (*iterator), ret);
	if (err != DOM_NO_ERR)
		return false;
	
	(*iterator)++;
	return true;
}

bool get_next_list(list *list, unsigned int *iterator, void **ret)
{
	unsigned int len = *iterator;
	unsigned int i = 0;
	struct list_elt *elt = list->head;

	for (; i < len; i++) {
		if (elt == NULL)
			return false;
		elt = elt->next;
	}

	if (elt == NULL)
		return false;

	*ret = elt->data;

	(*iterator)++;

	return true;
}

bool _get_next_domnamednodemap(dom_namednodemap *map, unsigned int *iterator, dom_node **ret)
{
	dom_exception err;
	uint32_t len;

	err = dom_namednodemap_get_length(map, &len);
	if (err != DOM_NO_ERR)
		return false;

	if (*iterator >= len)
		return false;

	err = dom_namednodemap_item(map, (*iterator), ret);
	if (err != DOM_NO_ERR)
		return false;
	
	(*iterator)++;	

	return true;
}

bool _get_next_domhtmlcollection(dom_html_collection *coll, unsigned int *iterator, dom_node **ret)
{
	dom_exception err;
	uint32_t len;

	err = dom_html_collection_get_length(coll, &len);
	if (err != DOM_NO_ERR)
		return false;

	if (*iterator >= len)
		return false;

	err = dom_html_collection_item(coll, (*iterator), ret);
	if (err != DOM_NO_ERR)
		return false;

	(*iterator)++;

	return true;
}
