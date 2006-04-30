/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_NEXUSVAR_H_
#define _MACHINE_NEXUSVAR_H_

enum nexus_ivars {
	NEXUS_IVAR_NODE,
	NEXUS_IVAR_NAME,
	NEXUS_IVAR_DEVICE_TYPE,
	NEXUS_IVAR_MODEL,
	NEXUS_IVAR_REG,
	NEXUS_IVAR_NREG,
	NEXUS_IVAR_INTERRUPTS,
	NEXUS_IVAR_NINTERRUPTS,
	NEXUS_IVAR_DMATAG,
};

#define NEXUS_ACCESSOR(var, ivar, type)					\
	__BUS_ACCESSOR(nexus, var, NEXUS, ivar, type)

NEXUS_ACCESSOR(node,		NODE,			phandle_t)
NEXUS_ACCESSOR(name,		NAME,			char *)
NEXUS_ACCESSOR(device_type,	DEVICE_TYPE,		char *)
NEXUS_ACCESSOR(model,		MODEL,			char *)
NEXUS_ACCESSOR(reg,		REG,			struct upa_regs *)
NEXUS_ACCESSOR(nreg,		NREG,			int)
NEXUS_ACCESSOR(interrupts,	INTERRUPTS,		u_int *)
NEXUS_ACCESSOR(ninterrupts,	NINTERRUPTS,		int)
NEXUS_ACCESSOR(dmatag,		DMATAG,			bus_dma_tag_t)

#undef NEXUS_ACCESSOR

#endif /* _MACHINE_NEXUSVAR_H_ */
