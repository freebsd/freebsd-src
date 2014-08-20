/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>
#include <gem.h>
#include <cflib.h>
#include "aestabs.h"

#ifndef NDEBUG
# define DEBUG_PRINT(x) 	printf x
#else
# define DEBUG_PRINT(x)
#endif


AES_TABLIST * tablist_declare(OBJECT *tree, aes_tablist_user_func user_func)
{
    AES_TABLIST * newlist = malloc(sizeof(AES_TABLIST));

    newlist->first = NULL;
    newlist->tree = tree;
    newlist->user_func = user_func;
    DEBUG_PRINT(("aes_tablist_declare: %p\n", newlist));
    return(newlist);
}


AES_TAB * tablist_add(AES_TABLIST * tablist, short obj_tab, OBJECT * page_tree,
					short obj_page)
{
    AES_TAB * newtab = malloc(sizeof(AES_TAB));

    assert(newtab);
    assert(tablist);

    newtab->next = NULL;
    newtab->prev = NULL;
    newtab->obj_tab = obj_tab;
    newtab->obj_page = obj_page;
    newtab->page_tree = page_tree;

    if(newtab->page_tree == NULL){
		newtab->page_tree = tablist->tree;
    }

    if (tablist->first == NULL) {
        tablist->first = newtab;
        set_state(tablist->tree, newtab->obj_tab, OS_SELECTED, 0);
    } else {
        AES_TAB *tmp = tablist->first;
        while( tmp->next != NULL ) {
            tmp = tmp->next;
        }
        tmp->next = newtab;
        newtab->prev = tmp;
        newtab->next = NULL;
        set_state(tablist->tree, newtab->obj_tab, OS_SELECTED, 0);
    }

    // TODO: Set the visible flag on that register?

    DEBUG_PRINT(("tablist_add:  Tab=%p\n", newtab));

    return(newtab);
}


short tablist_activate(AES_TABLIST * tablist, short tab, short options)
{
    AES_TAB *tmp, *activated=NULL, *deactivated=NULL;
    struct aes_tab_s *active;
    short activated_pg = -1;
    short is_tab = 0;

    assert(tablist);
    assert(tablist->first);

    active = tablist_get_active(tablist);

	if (active != NULL) {
		if ((options & AES_TABLIST_OPTION_FORCE_EVENTS) == 0) {
			if(active->obj_tab == tab)
				return(0);
		}
	}

    tmp = tablist->first;
    while (tmp != NULL) {
        if(tmp->obj_tab == tab) {
            is_tab = 1;
        }
        tmp = tmp->next;
    }

    if(is_tab == 0) {
        return(0);
    }

    tmp = tablist->first;
    while ( tmp != NULL ) {
        if(tab != tmp->obj_tab) {
            if (get_state(tablist->tree, tmp->obj_tab, OS_SELECTED) != 0) {
                deactivated = tmp;
                set_state(tablist->tree, tmp->obj_tab, OS_SELECTED, 0);
            }
            // the tab registers can share the same page, consider that:
            if (tablist->tree == tmp->page_tree
					&& activated_pg != tmp->obj_page) {

					set_flag(tablist->tree, tmp->obj_page, OF_HIDETREE, 1);
				}
        } else {
        	activated = tmp;
            // this tab must the selected / visible
            set_state(tablist->tree, tmp->obj_tab, OS_SELECTED, 1);
            if(tablist->tree == tmp->page_tree)
				set_flag(tablist->tree, tmp->obj_page, OF_HIDETREE, 0);
            activated_pg = tmp->obj_page;
        }
        tmp = tmp->next;
    }

    if(tablist->user_func != NULL) {
    	AES_TABLIST_FUNC_ARGS args;
    	if(deactivated){
			args.event = AES_TABLIST_TAB_DEACTIVATED;
			args.tab = deactivated;
			tablist->user_func(tablist, &args);
    	}
    	if(activated){
			args.event = AES_TABLIST_TAB_ACTIVATED;
			args.tab = activated;
			tablist->user_func(tablist, &args);
    	}
    }
    return(1);
}

struct aes_tab_s *tablist_get_active(AES_TABLIST * tablist)
{
    AES_TAB *tmp = tablist->first;
    while( tmp != NULL ) {
        if(get_state(tablist->tree, tmp->obj_tab, OS_SELECTED) != 0) {
            // that's the one
            return(tmp);
        }
        tmp = tmp->next;
    }
    return(NULL);
}

AES_TAB * tablist_find(AES_TABLIST * tablist, OBJECT * page, short tab)
{
    AES_TAB *tmp = tablist->first;
    while( tmp != NULL ) {
        if((tmp->page_tree == page) && (tab == tmp->obj_tab)) {
            return(tmp);
        }
        tmp = tmp->next;
    }
    return(NULL);
}

void tablist_delete(AES_TABLIST *tablist)
{
    AES_TAB *tmp = tablist->first, *cur;
    while ( tmp != NULL ) {
        cur = tmp;
        tmp = tmp->next;
        DEBUG_PRINT(("tablist_delete, Freeing tab: %p\n", cur));
        free(cur);
    }
    DEBUG_PRINT(("tablist_delete, Freeing list: %p\n", tablist));
    free(tablist);
}
