/*
 * Copyright (c) 2001-2002
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
 * Copyright (c) 2003-2004
 *	Hartmut Brandt.
 * 	All rights reserved.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef ATMCONFIG_DEVICE_H_
#define ATMCONFIG_DEVICE_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <stdint.h>

/*
 * ATM interface table
 */
struct atmif {
	TAILQ_ENTRY(atmif) link;
	uint64_t	found;
	int32_t		index;
	char		*ifname;
	size_t		ifnamelen;
	uint32_t	pcr;
	int32_t		media;
	uint32_t	vpi_bits;
	uint32_t	vci_bits;
	uint32_t	max_vpcs;
	uint32_t	max_vccs;
	u_char		*esi;
	size_t		esilen;
	int32_t		carrier;
	int32_t		mode;
};
TAILQ_HEAD(atmif_list, atmif);

/* list of all ATM interfaces */
extern struct atmif_list atmif_list;

/* fetch this table */
void atmif_fetchtable(void);

/* find an ATM interface by name */
struct atmif *atmif_find_name(const char *);

/* find an ATM interface by index */
struct atmif *atmif_find(u_int);

#endif
