/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
 * All rights reserved.
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

#ifndef	_MACHINE_PCPU_H_
#define	_MACHINE_PCPU_H_

#ifdef _KERNEL

#define	PCPU_MD_FIELDS							\
	struct alpha_pcb pc_idlepcb;		/* pcb for idling */	\
	u_int64_t	pc_idlepcbphys;		/* pa of pc_idlepcb */	\
	u_int64_t	pc_pending_ipis;	/* pending IPI's */	\
	u_int32_t	pc_next_asn;		/* next ASN to alloc */	\
	u_int32_t	pc_current_asngen	/* ASN rollover check */

struct pcpu;

register struct pcpu *pcpup __asm__("$8");

#define	PCPU_GET(member)	(pcpup->pc_ ## member)
#define	PCPU_PTR(member)	(&pcpup->pc_ ## member)
#define	PCPU_SET(member,value)	(pcpup->pc_ ## member = (value))

#endif	/* _KERNEL */

#endif	/* !_MACHINE_PCPU_H_ */
