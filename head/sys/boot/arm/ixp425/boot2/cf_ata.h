/*-
 * Copyright (c) 2008 John Hay.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef ARM_BOOT_CF_ATA_H
#define ARM_BOOT_CF_ATA_H

#define CF_DATA		0x00
#define CF_ERROR	0x01
#define CF_FEATURE	0x01
#define CF_SECT_CNT	0x02
#define CF_SECT_NUM	0x03
#define CF_CYL_L	0x04
#define CF_CYL_H	0x05
#define CF_DRV_HEAD	0x06
#define CF_D_MASTER		0x00
#define CF_D_LBA		0x40
#define CF_D_IBM		0xa0
#define CF_STATUS	0x07
#define CF_S_ERROR		0x01
#define CF_S_INDEX		0x02
#define CF_S_CORR		0x04
#define CF_S_DRQ		0x08
#define CF_S_DSC		0x10
#define CF_S_DWF		0x20
#define CF_S_READY		0x40
#define CF_S_BUSY		0x80
#define CF_COMMAND	0x07

/* This is according to the appnote, but Sam use 0x1e in avila_ata.c */
#define CF_ALT_STATUS	0x16
#define CF_ALT_DEV_CTR	0x16
#define CF_ALT_DEV_CTR2	0x1e
#define CF_A_IDS		0x02
#define CF_A_RESET		0x04
#define CF_A_4BIT		0x08

#define AVILA_IDE_GPIN		12

#endif /* !ARM_BOOT_CF_ATA_H */
