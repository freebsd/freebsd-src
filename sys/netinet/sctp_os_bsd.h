/*-
 * Copyright (c) 2006, Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#ifndef __sctp_os_bsd_h__
#define __sctp_os_bsd_h__

/*
 * includes
 */
#include <sys/random.h>


/*
 *
 */
typedef struct mbuf *sctp_mbuf_t;

/*
 * general memory allocation
 */
#define SCTP_MALLOC(var, type, size, name) \
    do { \
	MALLOC(var, type, size, M_PCB, M_NOWAIT); \
    } while (0)

#define SCTP_FREE(var)	FREE(var, M_PCB)

#define SCTP_MALLOC_SONAME(var, type, size) \
    do { \
	MALLOC(var, type, size, M_SONAME, M_WAITOK | M_ZERO); \
    } while (0)

#define SCTP_FREE_SONAME(var)	FREE(var, M_SONAME)

/*
 * zone allocation functions
 */
#include <vm/uma.h>
/* SCTP_ZONE_INIT: initialize the zone */
typedef struct uma_zone *sctp_zone_t;

#define UMA_ZFLAG_FULL	0x0020
#define SCTP_ZONE_INIT(zone, name, size, number) { \
	zone = uma_zcreate(name, size, NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,\
		UMA_ZFLAG_FULL); \
	uma_zone_set_max(zone, number); \
}

/* SCTP_ZONE_GET: allocate element from the zone */
#define SCTP_ZONE_GET(zone) \
	uma_zalloc(zone, M_NOWAIT);

/* SCTP_ZONE_FREE: free element from the zone */
#define SCTP_ZONE_FREE(zone, element) \
	uma_zfree(zone, element);

/*
 * Functions
 */
#define sctp_read_random(buf, len)	read_random(buf, len)

#endif
