/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#include <stdbool.h>
#include <string.h>

#include "utils/log.h"

/* callback functions for search implementation */
static void gui_search_set_status(bool found, void *p);
static void gui_search_set_hourglass(bool active, void *p);
static void gui_search_add_recent(const char *string, void *p);
static void gui_search_set_forward_state(bool active, void *p);
static void gui_search_set_back_state(bool active, void *p);

/**
* Change the displayed search status.
* \param found  search pattern matched in text
* \param p the pointer sent to search_verify_new() / search_create_context()
*/
void gui_search_set_status(bool found, void *p)
{
}

/**
* display hourglass while searching
* \param active start/stop indicator
* \param p the pointer sent to search_verify_new() / search_create_context()
*/
void gui_search_set_hourglass(bool active, void *p)
{
}

/**
* add search string to recent searches list
* \param string search pattern
* \param p the pointer sent to search_verify_new() / search_create_context()
*/
void gui_search_add_recent(const char *string, void *p)
{
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/
void gui_search_set_forward_state(bool active, void *p)
{
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/
void gui_search_set_back_state(bool active, void *p)
{
}
