/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: ebusvar.h,v 1.5 2001/07/20 00:07:13 eeh Exp
 *	and
 *	from: FreeBSD: src/sys/dev/pci/pcivar.h,v 1.51 2001/02/27
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_EBUS_EBUSVAR_H_
#define _SPARC64_EBUS_EBUSVAR_H_

enum ebus_device_ivars {
	EBUS_IVAR_COMPAT,
	EBUS_IVAR_NAME,
	EBUS_IVAR_NODE,
};

/*
 * Simplified accessors for ebus devices
 */
#define EBUS_ACCESSOR(var, ivar, type) \
	__BUS_ACCESSOR(ebus, var, EBUS, ivar, type)

EBUS_ACCESSOR(compat,	COMPAT,		char *)
EBUS_ACCESSOR(name,	NAME,		char *)
EBUS_ACCESSOR(node,	NODE,		phandle_t)

#undef EBUS_ACCESSOR

#endif /* !_SPARC64_EBUS_EBUSVAR_H_ */
