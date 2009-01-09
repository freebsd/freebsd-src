/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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
 */

/* This file contains various macro functions. */

#ifndef _USB2_MFUNC_H_
#define	_USB2_MFUNC_H_

#define	USB_MAKE_001(n,ENUM) ENUM,
#define	USB_MAKE_ENUM(m) \
enum { m(USB_MAKE_001,) m##_MAX }

#define	USB_MAKE_002(n,ENUM) #ENUM,
#define	USB_MAKE_DEBUG_TABLE(m) \
static const char * m[m##_MAX] = { m(USB_MAKE_002,) }

#define	USB_LOG2(n) (	    \
((x) <= (1<<0x00)) ? 0x00 : \
((x) <= (1<<0x01)) ? 0x01 : \
((x) <= (1<<0x02)) ? 0x02 : \
((x) <= (1<<0x03)) ? 0x03 : \
((x) <= (1<<0x04)) ? 0x04 : \
((x) <= (1<<0x05)) ? 0x05 : \
((x) <= (1<<0x06)) ? 0x06 : \
((x) <= (1<<0x07)) ? 0x07 : \
((x) <= (1<<0x08)) ? 0x08 : \
((x) <= (1<<0x09)) ? 0x09 : \
((x) <= (1<<0x0A)) ? 0x0A : \
((x) <= (1<<0x0B)) ? 0x0B : \
((x) <= (1<<0x0C)) ? 0x0C : \
((x) <= (1<<0x0D)) ? 0x0D : \
((x) <= (1<<0x0E)) ? 0x0E : \
((x) <= (1<<0x0F)) ? 0x0F : \
((x) <= (1<<0x10)) ? 0x10 : \
((x) <= (1<<0x11)) ? 0x11 : \
((x) <= (1<<0x12)) ? 0x12 : \
((x) <= (1<<0x13)) ? 0x13 : \
((x) <= (1<<0x14)) ? 0x14 : \
((x) <= (1<<0x15)) ? 0x15 : \
((x) <= (1<<0x16)) ? 0x16 : \
((x) <= (1<<0x17)) ? 0x17 : \
((x) <= (1<<0x18)) ? 0x18 : \
((x) <= (1<<0x19)) ? 0x19 : \
((x) <= (1<<0x1A)) ? 0x1A : \
((x) <= (1<<0x1B)) ? 0x1B : \
((x) <= (1<<0x1C)) ? 0x1C : \
((x) <= (1<<0x1D)) ? 0x1D : \
((x) <= (1<<0x1E)) ? 0x1E : \
0x1F)


/* helper for converting pointers to integers */
#define	USB_P2U(ptr) \
  (((const uint8_t *)(ptr)) - ((const uint8_t *)0))

/* helper for computing offsets */
#define	USB_ADD_BYTES(ptr,size) \
  ((void *)(USB_P2U(ptr) + (size)))

/* debug macro */
#define	USB_ASSERT KASSERT

#endif					/* _USB2_MFUNC_H_ */
