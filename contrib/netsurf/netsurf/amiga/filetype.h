/*
 * Copyright 2010 - 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_FILETYPE_H
#define AMIGA_FILETYPE_H
#include <stdbool.h>
#include <libwapcaplet/libwapcaplet.h>
#include "content/content_type.h"
#include "utils/errors.h"
#include <datatypes/datatypes.h>

struct hlcache_handle;
struct ami_mime_entry;

const char *fetch_filetype(const char *unix_path);

nserror ami_mime_init(const char *mimefile);
void ami_mime_free(void);
void ami_mime_entry_free(struct ami_mime_entry *mimeentry);
void ami_mime_dump(void);

struct Node *ami_mime_from_datatype(struct DataType *dt,
		lwc_string **mimetype, struct Node *start_node);
struct Node *ami_mime_to_filetype(lwc_string *mimetype,
		lwc_string **filetype, struct Node *start_node);

const char *ami_mime_content_to_filetype(struct hlcache_handle *c);
lwc_string *ami_mime_content_to_cmd(struct hlcache_handle *c);

struct Node *ami_mime_has_cmd(lwc_string **mimetype, struct Node *start_node);

bool ami_mime_compare(struct hlcache_handle *c, const char *type);

/* deprecated */
const char *ami_content_type_to_file_type(content_type type);

#endif
