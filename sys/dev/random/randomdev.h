/*-
 * Copyright (c) 2001 Mark R V Murray
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

/* This header contains only those definitions that are global
 * and non algorithm-specific for the entropy processor
 */

/* #define ENTROPYSOURCE nn	   entropy sources (actually classes)
 *					This is properly defined in
 *					an enum in sys/random.h
 */

/* Cryptographic block size in bits */
#define	BLOCKSIZE	256

/* The ring size _MUST_ be a power of 2 */
#define HARVEST_RING_SIZE	1024	/* harvest ring buffer size */
#define HARVEST_RING_MASK	(HARVEST_RING_SIZE - 1)

#define HARVESTSIZE	16	/* max size of each harvested entropy unit */

SYSCTL_DECL(_kern_random);

/* These are used to queue harvested packets of entropy. The entropy
 * buffer size is pretty arbitrary.
 */
struct harvest {
	u_int64_t somecounter;		/* fast counter for clock jitter */
	u_char entropy[HARVESTSIZE];	/* the harvested entropy */
	u_int size, bits, frac;		/* stats about the entropy */
	enum esource source;		/* stats about the entropy */
};

void random_init(void);
void random_deinit(void);
void random_init_harvester(void (*)(u_int64_t, void *, u_int, u_int, u_int, enum esource), int (*)(void *, int));
void random_deinit_harvester(void);
void random_set_wakeup_exit(void *);
void random_process_event(struct harvest *event);
void random_reseed(void);
void random_unblock(void);

int read_random_real(void *, int);

/* If this was c++, this would be a template */
#define RANDOM_CHECK_UINT(name, min, max)				\
static int								\
random_check_uint_##name(SYSCTL_HANDLER_ARGS)				\
{									\
	if (oidp->oid_arg1 != NULL) {					\
		 if (*(u_int *)(oidp->oid_arg1) <= (min))		\
			*(u_int *)(oidp->oid_arg1) = (min);		\
		 else if (*(u_int *)(oidp->oid_arg1) > (max))		\
			*(u_int *)(oidp->oid_arg1) = (max);		\
	}								\
        return sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,	\
		req);							\
}
