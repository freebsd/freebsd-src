/* 
 * implement arrays for dc
 *
 * Copyright (C) 1994, 1997, 1998 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to:
 *
 *    The Free Software Foundation, Inc.
 *    59 Temple Place, Suite 330
 *    Boston, MA 02111 USA
 */

/* This module is the only one that knows what arrays look like. */

#include "config.h"

#include <stdio.h>	/* "dc-proto.h" wants this */
#ifdef HAVE_STDLIB_H
/* get size_t definition from "almost ANSI" compiling environments. */
#include <stdlib.h>
#endif
#include "dc.h"
#include "dc-proto.h"
#include "dc-regdef.h"

/* what's most useful: quick access or sparse arrays? */
/* I'll go with sparse arrays for now */
struct dc_array {
	int Index;
	dc_data value;
	struct dc_array *next;
};


/* initialize the arrays */
void
dc_array_init DC_DECLVOID()
{
}

/* store value into array_id[Index] */
void
dc_array_set DC_DECLARG((array_id, Index, value))
	int array_id DC_DECLSEP
	int Index DC_DECLSEP
	dc_data value DC_DECLEND
{
	struct dc_array *cur;
	struct dc_array *prev=NULL;
	struct dc_array *newentry;

	cur = dc_get_stacked_array(array_id);
	while (cur && cur->Index < Index){
		prev = cur;
		cur = cur->next;
	}
	if (cur && cur->Index == Index){
		if (cur->value.dc_type == DC_NUMBER)
			dc_free_num(&cur->value.v.number);
		else if (cur->value.dc_type == DC_STRING)
			dc_free_str(&cur->value.v.string);
		else
			dc_garbage(" in array", array_id);
		cur->value = value;
	}else{
		newentry = dc_malloc(sizeof *newentry);
		newentry->Index = Index;
		newentry->value = value;
		newentry->next = cur;
		if (prev)
			prev->next = newentry;
		else
			dc_set_stacked_array(array_id, newentry);
	}
}

/* retrieve a dup of a value from array_id[Index] */
/* A zero value is returned if the specified value is unintialized. */
dc_data
dc_array_get DC_DECLARG((array_id, Index))
	int array_id DC_DECLSEP
	int Index DC_DECLEND
{
	struct dc_array *cur;

	for (cur=dc_get_stacked_array(array_id); cur; cur=cur->next)
		if (cur->Index == Index)
			return dc_dup(cur->value);
	return dc_int2data(0);
}

/* free an array chain */
void
dc_array_free DC_DECLARG((a_head))
	struct dc_array *a_head DC_DECLEND
{
	struct dc_array *cur;
	struct dc_array *next;

	for (cur=a_head; cur; cur=next) {
		next = cur->next;
		if (cur->value.dc_type == DC_NUMBER)
			dc_free_num(&cur->value.v.number);
		else if (cur->value.dc_type == DC_STRING)
			dc_free_str(&cur->value.v.string);
		else
			dc_garbage("in stack", -1);
		free(cur);
	}
}
