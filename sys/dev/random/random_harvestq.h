/*-
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
struct harvest {
	uintmax_t somecounter;		/* fast counter for clock jitter */
	uint8_t entropy[HARVESTSIZE];	/* the harvested entropy */
	u_int size, bits;		/* stats about the entropy */
	u_int destination;		/* destination pool of this entropy */
	enum esource source;		/* origin of the entropy */
	STAILQ_ENTRY(harvest) next;	/* next item on the list */
};

void random_harvestq_init(void (*)(struct harvest *));
void random_harvestq_deinit(void);
void random_harvestq_internal(const void *, u_int, u_int, enum esource);

/* This is in randomdev.c as it needs to be fixed in the kernel */
void randomdev_set_wakeup_exit(void *);

extern void (*harvest_process_event)(struct harvest *);

extern int random_kthread_control;
extern struct mtx harvest_mtx;

#endif /* SYS_DEV_RANDOM_RANDOM_HARVESTQ_H_INCLUDED */
