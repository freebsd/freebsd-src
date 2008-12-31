/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cxgb/ulp/tom/cxgb_tcp_offload.c,v 1.4.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * grab bag of accessor routines that will either be moved to netinet
 * or removed
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/sockopt.h>
#include <sys/sockbuf.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_offload.h>
#include <netinet/tcp_syncache.h>
#include <netinet/toedev.h>

#include <dev/cxgb/ulp/tom/cxgb_tcp_offload.h>


/*
 * This file contains code as a short-term staging area before it is moved in 
 * to sys/netinet/tcp_offload.c
 */

void
sockbuf_lock(struct sockbuf *sb)
{

	SOCKBUF_LOCK(sb);
}

void
sockbuf_lock_assert(struct sockbuf *sb)
{

	SOCKBUF_LOCK_ASSERT(sb);
}

void
sockbuf_unlock(struct sockbuf *sb)
{

	SOCKBUF_UNLOCK(sb);
}

int
sockbuf_sbspace(struct sockbuf *sb)
{

	return (sbspace(sb));
}

