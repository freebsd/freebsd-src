/* memory.c

   Memory-resident database... */

/*
 * Copyright (c) 1995-2001 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: memory.c,v 1.66.2.3 2001/10/17 03:25:10 mellon Exp $ Copyright (c) 1995-2000 The Internet Software Consortium.  All rights reserved.\n";
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
