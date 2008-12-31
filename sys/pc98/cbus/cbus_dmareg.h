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
 * $FreeBSD: src/sys/pc98/cbus/cbus_dmareg.h,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _PC98_CBUS_CBUS_DMAREG_H_
#define _PC98_CBUS_CBUS_DMAREG_H_

#include <dev/ic/i8237.h>

#define	IO_DMA		0x01			/* 8237A DMA Controller */

/*
 * Register definitions for DMA controller 1 (channels 0..3):
 */
#define	DMA1_CHN(c)	(IO_DMA + (4*(c)))	/* addr reg for channel c */
#define	DMA1_STATUS	(IO_DMA + 0x10)		/* status register */
#define	DMA1_SMSK	(IO_DMA + 0x14)		/* single mask register */
#define	DMA1_MODE	(IO_DMA + 0x16)		/* mode register */
#define	DMA1_FFC	(IO_DMA + 0x18)		/* clear first/last FF */

#endif /* _PC98_CBUS_CBUS_DMAREG_H_ */
