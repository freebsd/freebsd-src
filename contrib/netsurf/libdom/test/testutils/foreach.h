/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef domts_foreach_h_
#define domts_foreach_h_

#include <dom/dom.h>

#include <list.h>

/* The following six functions are used for the XML testcase's 
   <for-each> element. 
   And the <for-each> element can be converted to :

   unsigned int iterator;
   foreach_initialise_*(list, &iterator);
   while(get_next_*(list, &iterator, ret)){
	  do the loop work.
   }
*/

void foreach_initialise_domnodelist(dom_nodelist *list, unsigned int *iterator);
void foreach_initialise_list(list *list, unsigned int *iterator);
void foreach_initialise_domnamednodemap(dom_namednodemap *map, unsigned int *iterator);
void foreach_initialise_domhtmlcollection(dom_html_collection *coll, unsigned int *iterator);

bool _get_next_domnodelist(dom_nodelist *list, unsigned int *iterator, dom_node **ret);
#define get_next_domnodelist(l, i, r) _get_next_domnodelist( \
		(dom_nodelist *) (l), (unsigned int *) (i), (dom_node **) (r))

bool get_next_list(list *list, unsigned int *iterator, void **ret);

bool _get_next_domnamednodemap(dom_namednodemap *map, unsigned int *iterator, dom_node **ret);
#define get_next_domnamednodemap(m, i, r) _get_next_domnamednodemap( \
		(dom_namednodemap *) (m), (unsigned int *) (i), (dom_node **) (r))

bool _get_next_domhtmlcollection(dom_html_collection *coll, unsigned int *iterator, dom_node **ret);
#define get_next_domhtmlcollection(c, i, r) _get_next_domhtmlcollection( \
		(dom_html_collection *) (c), (unsigned int *) (i), (dom_node **) (r))

#endif
