/**
 *       Copyright (c) 1997 by Matthew N. Dodd <winter@jurai.net>
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Credits:  Based on and part of the DPT driver for FreeBSD written and
 *           maintained by Simon Shapiro <shimon@simon-shapiro.org>
 */

/*
 * $Id: dpt_eisa.h,v 1.1 1998/03/10 21:31:06 ShimonR Exp ShimonR $
 */

#define DPT_EISA_SLOT_OFFSET		0xc88	/* 8 */
#define DPT_EISA_IOSIZE			sizeof(eata_reg_t)

#define ISA_PRIMARY_WD_ADDRESS		0x1f8   

#define	DPT_EISA_DPT2402		0x12142402
#define	DPT_EISA_DPTA401		0x1214A401
#define	DPT_EISA_DPTA402		0x1214A402
#define	DPT_EISA_DPTA410		0x1214A410
#define	DPT_EISA_DPTA411		0x1214A411
#define	DPT_EISA_DPTA412		0x1214A412
#define	DPT_EISA_DPTA420		0x1214A420
#define	DPT_EISA_DPTA501		0x1214A501
#define	DPT_EISA_DPTA502		0x1214A502
#define	DPT_EISA_DPTA701		0x1214A701
#define	DPT_EISA_DPTBC01		0x1214BC01
#define	DPT_EISA_NEC8200		0x12148200
#define	DPT_EISA_ATT2408		0x12142408

