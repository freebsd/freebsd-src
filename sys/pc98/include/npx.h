/*-
 * Copyright (C) 2005 TAKAHASHI Yoshihiro. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pc98/include/npx.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _PC98_INCLUDE_NPX_H_
#define _PC98_INCLUDE_NPX_H_

#include <i386/npx.h>

#ifdef _KERNEL

#undef	IO_NPX
#define	IO_NPX		0x0F8		/* Numeric Coprocessor */
#undef	IO_NPXSIZE
#define	IO_NPXSIZE	8		/* 80387/80487 NPX registers */

#undef	IRQ_NPX
#define	IRQ_NPX		8

/* full reset of npx: not needed on pc98 */
#undef npx_full_reset
#define npx_full_reset()

#endif /* _KERNEL */

#endif /* _PC98_INCLUDE_NPX_H_ */
