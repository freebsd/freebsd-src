/*
 * PCMCIA Card Interface Controller
 *
 * Copyright (c) 1999 Roger Hardiman
 * All rights reserved.
 *
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pccard/pcic.h,v 1.5.2.2 2000/08/03 01:08:04 peter Exp $
*/

#define PCIC_RF_IODF_WS		(0x01 << 16)
#define PCIC_RF_IODF_16BIT	(0x02 << 16)
#define PCIC_RF_IODF_CS16	(0x04 << 16)
#define PCIC_RF_IODF_ZEROWS	(0x08 << 16)

#define PCIC_RF_MDF_16BITS	(0x01 << 16)
#define PCIC_RF_MDF_ZEROWS	(0x02 << 16)
#define PCIC_RF_MDF_WS0		(0x04 << 16)
#define PCIC_RF_MDF_WS1		(0x08 << 16)
#define PCIC_RF_MDF_ATTR	(0x10 << 16)
#define PCIC_RF_MDF_WP		(0x20 << 16)
