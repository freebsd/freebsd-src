/*
 * Copyright (c) 2018 The TCPDUMP project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include "netdissect-alloc.h"

static void nd_add_alloc_list(netdissect_options *, nd_mem_chunk_t *);

/*
 * nd_free_all() is intended to be used after a packet printing
 */

/* Add a memory chunk in allocation linked list */
static void
nd_add_alloc_list(netdissect_options *ndo, nd_mem_chunk_t *chunkp)
{
	if (ndo->ndo_last_mem_p == NULL)	/* first memory allocation */
		chunkp->prev_mem_p = NULL;
	else					/* previous memory allocation */
		chunkp->prev_mem_p = ndo->ndo_last_mem_p;
	ndo->ndo_last_mem_p = chunkp;
}

/* malloc replacement, with tracking in a linked list */
void *
nd_malloc(netdissect_options *ndo, size_t size)
{
	nd_mem_chunk_t *chunkp = malloc(sizeof(nd_mem_chunk_t) + size);
	if (chunkp == NULL)
		return NULL;
	nd_add_alloc_list(ndo, chunkp);
	return chunkp + 1;
}

/* Free chunks in allocation linked list from last to first */
void
nd_free_all(netdissect_options *ndo)
{
	nd_mem_chunk_t *current, *previous;
	current = ndo->ndo_last_mem_p;
	while (current != NULL) {
		previous = current->prev_mem_p;
		free(current);
		current = previous;
	}
	ndo->ndo_last_mem_p = NULL;
}
