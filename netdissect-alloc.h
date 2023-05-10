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

#ifndef netdissect_alloc_h
#define netdissect_alloc_h

#include <stdarg.h>
#include "netdissect-stdinc.h"
#include "netdissect.h"

typedef struct nd_mem_chunk {
	void *prev_mem_p;
	/* variable size data */
} nd_mem_chunk_t;

void * nd_malloc(netdissect_options *, size_t);
void nd_free_all(netdissect_options *);

#endif /* netdissect_alloc_h */
