/* memory.c

   Memory-resident database... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: memory.c,v 1.66.2.5 2004/06/10 17:59:19 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

struct group *root_group;
group_hash_t *group_name_hash;
int (*group_write_hook) (struct group_object *);

isc_result_t delete_group (struct group_object *group, int writep)
{
	struct group_object *d;

	/* The group should exist and be hashed - if not, it's invalid. */
	if (group_name_hash) {
		d = (struct group_object *)0;
		group_hash_lookup (&d, group_name_hash, group -> name,
				   strlen (group -> name), MDL);
	} else
		return ISC_R_INVALIDARG;
	if (!d)
		return ISC_R_INVALIDARG;

	/* Also not okay to delete a group that's not the one in
	   the hash table. */
	if (d != group)
		return ISC_R_INVALIDARG;

	/* If it's dynamic, and we're deleting it, we can just blow away the
	   hash table entry. */
	if ((group -> flags & GROUP_OBJECT_DYNAMIC) &&
	    !(group -> flags & GROUP_OBJECT_STATIC)) {
		group_hash_delete (group_name_hash,
				   group -> name, strlen (group -> name), MDL);
	} else {
		group -> flags |= GROUP_OBJECT_DELETED;
		if (group -> group)
			group_dereference (&group -> group, MDL);
	}

	/* Store the group declaration in the lease file. */
	if (writep && group_write_hook) {
		if (!(*group_write_hook) (group))
			return ISC_R_IOERROR;
	}
	return ISC_R_SUCCESS;
}

isc_result_t supersede_group (struct group_object *group, int writep)
{
	struct group_object *t, *u;
	isc_result_t status;

	/* Register the group in the group name hash table,
	   so we can look it up later. */
	if (group_name_hash) {
		t = (struct group_object *)0;
		group_hash_lookup (&t, group_name_hash,
			group -> name,
			     strlen (group -> name), MDL);
		if (t && t != group) {
			/* If this isn't a dynamic entry, then we need to flag
			   the replacement as not dynamic either - otherwise,
			   if the dynamic entry is deleted later, the static
			   entry will come back next time the server is stopped
			   and restarted. */
			if (!(t -> flags & GROUP_OBJECT_DYNAMIC))
				group -> flags |= GROUP_OBJECT_STATIC;

			/* Delete the old object if it hasn't already been
			   deleted.  If it has already been deleted, get rid of
			   the hash table entry.  This is a legitimate
			   situation - a deleted static object needs to be kept
			   around so we remember it's deleted. */
			if (!(t -> flags & GROUP_OBJECT_DELETED))
				delete_group (t, 0);
			else {
				group_hash_delete (group_name_hash,
						   group -> name,
						   strlen (group -> name),
						   MDL);
				group_object_dereference (&t, MDL);
			}
		}
	} else {
		group_new_hash (&group_name_hash, 0, MDL);
		t = (struct group_object *)0;
	}

	/* Add the group to the group name hash if it's not
	   already there, and also thread it into the list of
	   dynamic groups if appropriate. */
	if (!t) {
		group_hash_add (group_name_hash, group -> name,
				strlen (group -> name), group, MDL);
	}

	/* Store the group declaration in the lease file. */
	if (writep && group_write_hook) {
		if (!(*group_write_hook) (group))
			return ISC_R_IOERROR;
	}
	return ISC_R_SUCCESS;
}

int clone_group (struct group **gp, struct group *group,
		 const char *file, int line)
{
	isc_result_t status;
	struct group *g = (struct group *)0;

	/* Normally gp should contain the null pointer, but for convenience
	   it's permissible to clone a group into itself. */
	if (*gp && *gp != group)
		return 0;
	if (!group_allocate (&g, file, line))
		return 0;
	if (group == *gp)
		*gp = (struct group *)0;
	group_reference (gp, g, file, line);
	g -> authoritative = group -> authoritative;
	group_reference (&g -> next, group, file, line);
	group_dereference (&g, file, line);
	return 1;
}
