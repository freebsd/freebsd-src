/* handle.c

   Functions for maintaining handles on objects. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
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

#include <omapip/omapip_p.h>

/* The handle table is a hierarchical tree designed for quick mapping
   of handle identifiers to objects.  Objects contain their own handle
   identifiers if they have them, so the reverse mapping is also
   quick.  The hierarchy is made up of table objects, each of which
   has 120 entries, a flag indicating whether the table is a leaf
   table or an indirect table, the handle of the first object covered
   by the table and the first object after that that's *not* covered
   by the table, a count of how many objects of either type are
   currently stored in the table, and an array of 120 entries pointing
   either to objects or tables.

   When we go to add an object to the table, we look to see if the
   next object handle to be assigned is covered by the outermost
   table.  If it is, we find the place within that table where the
   next handle should go, and if necessary create additional nodes in
   the tree to contain the new handle.  The pointer to the object is
   then stored in the correct position.
   
   Theoretically, we could have some code here to free up handle
   tables as they go out of use, but by and large handle tables won't
   go out of use, so this is being skipped for now.  It shouldn't be
   too hard to implement in the future if there's a different
   application. */

omapi_handle_table_t *omapi_handle_table;
omapi_handle_t omapi_next_handle = 1;	/* Next handle to be assigned. */

static isc_result_t omapi_handle_lookup_in (omapi_object_t **,
					    omapi_handle_t,
					    omapi_handle_table_t *);
static isc_result_t omapi_object_handle_in_table (omapi_handle_t,
						  omapi_handle_table_t *,
						  omapi_object_t *);
static isc_result_t omapi_handle_table_enclose (omapi_handle_table_t **);

isc_result_t omapi_object_handle (omapi_handle_t *h, omapi_object_t *o)
{
	int tabix;
	isc_result_t status;

	if (o -> handle) {
		*h = o -> handle;
		return ISC_R_SUCCESS;
	}
	
	if (!omapi_handle_table) {
		omapi_handle_table = dmalloc (sizeof *omapi_handle_table, MDL);
		if (!omapi_handle_table)
			return ISC_R_NOMEMORY;
		memset (omapi_handle_table, 0, sizeof *omapi_handle_table);
		omapi_handle_table -> first = 0;
		omapi_handle_table -> limit = OMAPI_HANDLE_TABLE_SIZE;
		omapi_handle_table -> leafp = 1;
	}

	/* If this handle doesn't fit in the outer table, we need to
	   make a new outer table.  This is a while loop in case for
	   some reason we decide to do disjoint handle allocation,
	   where the next level of indirection still isn't big enough
	   to enclose the next handle ID. */

	while (omapi_next_handle >= omapi_handle_table -> limit) {
		omapi_handle_table_t *new;
		
		new = dmalloc (sizeof *new, MDL);
		if (!new)
			return ISC_R_NOMEMORY;
		memset (new, 0, sizeof *new);
		new -> first = 0;
		new -> limit = (omapi_handle_table -> limit *
					       OMAPI_HANDLE_TABLE_SIZE);
		new -> leafp = 0;
		new -> children [0].table = omapi_handle_table;
		omapi_handle_table = new;
	}

	/* Try to cram this handle into the existing table. */
	status = omapi_object_handle_in_table (omapi_next_handle,
					       omapi_handle_table, o);
	/* If it worked, return the next handle and increment it. */
	if (status == ISC_R_SUCCESS) {
		*h = omapi_next_handle;
		omapi_next_handle++;
		return ISC_R_SUCCESS;
	}
	if (status != ISC_R_NOSPACE)
		return status;

	status = omapi_handle_table_enclose (&omapi_handle_table);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_handle_in_table (omapi_next_handle,
					       omapi_handle_table, o);
	if (status != ISC_R_SUCCESS)
		return status;
	*h = omapi_next_handle;
	omapi_next_handle++;

	return ISC_R_SUCCESS;
}

static isc_result_t omapi_object_handle_in_table (omapi_handle_t h,
						  omapi_handle_table_t *table,
						  omapi_object_t *o)
{
	omapi_handle_table_t *inner;
	omapi_handle_t scale, index;
	isc_result_t status;

	if (table -> first > h || table -> limit <= h)
		return ISC_R_NOSPACE;
	
	/* If this is a leaf table, just stash the object in the
	   appropriate place. */
	if (table -> leafp) {
		status = (omapi_object_reference
			  (&table -> children [h - table -> first].object,
			   o, MDL));
		if (status != ISC_R_SUCCESS)
			return status;
		o -> handle = h;
		return ISC_R_SUCCESS;
	}

	/* Scale is the number of handles represented by each child of this
	   table.   For a leaf table, scale would be 1.   For a first level
	   of indirection, 120.   For a second, 120 * 120.   Et cetera. */
	scale = (table -> limit - table -> first) / OMAPI_HANDLE_TABLE_SIZE;

	/* So the next most direct table from this one that contains the
	   handle must be the subtable of this table whose index into this
	   table's array of children is the handle divided by the scale. */
	index = (h - table -> first) / scale;
	inner = table -> children [index].table;

	/* If there is no more direct table than this one in the slot
	   we came up with, make one. */
	if (!inner) {
		inner = dmalloc (sizeof *inner, MDL);
		if (!inner)
			return ISC_R_NOMEMORY;
		memset (inner, 0, sizeof *inner);
		inner -> first = index * scale + table -> first;
		inner -> limit = inner -> first + scale;
		if (scale == OMAPI_HANDLE_TABLE_SIZE)
			inner -> leafp = 1;
		table -> children [index].table = inner;
	}

	status = omapi_object_handle_in_table (h, inner, o);
	if (status == ISC_R_NOSPACE) {
		status = (omapi_handle_table_enclose
			  (&table -> children [index].table));
		if (status != ISC_R_SUCCESS)
			return status;

		return omapi_object_handle_in_table
			(h, table -> children [index].table, o);
	}
	return status;
}

static isc_result_t omapi_handle_table_enclose (omapi_handle_table_t **table)
{
	omapi_handle_table_t *inner = *table;
	omapi_handle_table_t *new;
	int index, base, scale;

	/* The scale of the table we're enclosing is going to be the
	   difference between its "first" and "limit" members.  So the
	   scale of the table enclosing it is going to be that multiplied
	   by the table size. */
	scale = (inner -> first - inner -> limit) * OMAPI_HANDLE_TABLE_SIZE;

	/* The range that the enclosing table covers is going to be
	   the result of subtracting the remainder of dividing the
	   enclosed table's first entry number by the enclosing
	   table's scale.  If handle IDs are being allocated
	   sequentially, the enclosing table's "first" value will be
	   the same as the enclosed table's "first" value. */
	base = inner -> first - inner -> first % scale;

	/* The index into the enclosing table at which the enclosed table
	   will be stored is going to be the difference between the "first"
	   value of the enclosing table and the enclosed table - zero, if
	   we are allocating sequentially. */
	index = (base - inner -> first) / OMAPI_HANDLE_TABLE_SIZE;

	new = dmalloc (sizeof *new, MDL);
	if (!new)
		return ISC_R_NOMEMORY;
	memset (new, 0, sizeof *new);
	new -> first = base;
	new -> limit = base + scale;
	if (scale == OMAPI_HANDLE_TABLE_SIZE)
		new -> leafp = 0;
	new -> children [index].table = inner;
	*table = new;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_handle_lookup (omapi_object_t **o, omapi_handle_t h)
{
	return omapi_handle_lookup_in (o, h, omapi_handle_table);
}

static isc_result_t omapi_handle_lookup_in (omapi_object_t **o,
					    omapi_handle_t h,
					    omapi_handle_table_t *table)

{
	omapi_handle_table_t *inner;
	omapi_handle_t scale, index;

	if (!table || table -> first > h || table -> limit <= h)
		return ISC_R_NOTFOUND;
	
	/* If this is a leaf table, just grab the object. */
	if (table -> leafp) {
		/* Not there? */
		if (!table -> children [h - table -> first].object)
			return ISC_R_NOTFOUND;
		return omapi_object_reference
			(o, table -> children [h - table -> first].object,
			 MDL);
	}

	/* Scale is the number of handles represented by each child of this
	   table.   For a leaf table, scale would be 1.   For a first level
	   of indirection, 120.   For a second, 120 * 120.   Et cetera. */
	scale = (table -> limit - table -> first) / OMAPI_HANDLE_TABLE_SIZE;

	/* So the next most direct table from this one that contains the
	   handle must be the subtable of this table whose index into this
	   table's array of children is the handle divided by the scale. */
	index = (h - table -> first) / scale;
	inner = table -> children [index].table;

	return omapi_handle_lookup_in (o, h, table -> children [index].table);
}

/* For looking up objects based on handles that have been sent on the wire. */
isc_result_t omapi_handle_td_lookup (omapi_object_t **obj,
				     omapi_typed_data_t *handle)
{
	isc_result_t status;
	omapi_handle_t h;

	if (handle -> type == omapi_datatype_int)
		h = handle -> u.integer;
	else if (handle -> type == omapi_datatype_data &&
		 handle -> u.buffer.len == sizeof h) {
		memcpy (&h, handle -> u.buffer.value, sizeof h);
		h = ntohl (h);
	} else
		return ISC_R_INVALIDARG;
	return omapi_handle_lookup (obj, h);
}
