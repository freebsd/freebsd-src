/*-
 * Copyright (c) 2013-2014 Mark R V Murray
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef SYS_DEV_RANDOM_RANDOM_HARVESTQ_H_INCLUDED
#define SYS_DEV_RANDOM_RANDOM_HARVESTQ_H_INCLUDED

#define HARVESTSIZE	16	/* max size of each harvested entropy unit */

/* These are used to queue harvested packets of entropy. The entropy
 * buffer size is pretty arbitrary.
 */
struct harvest_event {
	uintmax_t			he_somecounter;		/* fast counter for clock jitter */
	uint8_t				he_entropy[HARVESTSIZE];/* some harvested entropy */
	u_int				he_size;		/* harvested entropy byte count */
	u_int				he_bits;		/* stats about the entropy */
	u_int				he_destination;		/* destination pool of this entropy */
	enum random_entropy_source	he_source;		/* origin of the entropy */
};

void random_harvestq_init(void (*)(struct harvest_event *), int);
void random_harvestq_deinit(void);
void random_harvestq_internal(const void *, u_int, u_int, enum random_entropy_source);

/* Pool count is used by anything needing to know how many entropy
 * pools are currently being maintained.
 * This is of use to (e.g.) the live source feed where we need to give
 * all the pools a top-up.
 */
extern int harvest_pool_count;

/* This is in randomdev.c as it needs to be permanently in the kernel */
void randomdev_set_wakeup_exit(void *);

/* Force all currently pending queue contents to clear, and kick the software processor */
void random_harvestq_flush(void);

/* Function called to process one harvested stochastic event */
extern void (*harvest_process_event)(struct harvest_event *);

/* Round-robin destination cache. */
extern u_int harvest_destination[ENTROPYSOURCE];

#endif /* SYS_DEV_RANDOM_RANDOM_HARVESTQ_H_INCLUDED */
