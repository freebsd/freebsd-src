/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD: src/sys/alpha/tlsb/kftxxvar.h,v 1.4.2.1 2000/07/03 20:08:01 mjacob Exp $
 */

/*
 * Instance vars for children of a KFTIA or KFTHA node.
 */
enum kft_dev_ivars {
	KFT_IVAR_NAME,
	KFT_IVAR_NODE,
	KFT_IVAR_DTYPE,
	KFT_IVAR_HOSENUM
};

/*
 * Simplified accessors for kft devices
 */

#define KFT_ACCESSOR(A, B, T)						 \
									 \
static __inline T kft_get_ ## A(device_t dev)				 \
{									 \
	u_long v;							 \
	BUS_READ_IVAR(device_get_parent(dev), dev, KFT_IVAR_ ## B, &v);  \
	return (T) v;							 \
}

KFT_ACCESSOR(name,	NAME,		const char*)
KFT_ACCESSOR(node,	NODE,		int)
KFT_ACCESSOR(dtype,	DTYPE,		u_int16_t)
KFT_ACCESSOR(hosenum,	HOSENUM,	u_int16_t)
