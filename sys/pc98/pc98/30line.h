/*-
 * Copyright (c) 1994, 1995, 1996.  FreeBSD(98) porting team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
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
 * $FreeBSD$
 */

#ifndef __PC98_PC98_30LINE_H__
#define __PC98_PC98_30LINE_H__

#include <pc98/pc98/module.h>

#ifndef	LINE30_ROW
#define LINE30_ROW	30
#endif

#define	_CR	80
#ifndef	_VS
#define	_VS	2
#endif
#ifndef	_HS
#define	_HS	1 + 1
#endif
#ifndef	_HFP
#define	_HFP	3 + 1
#endif
#ifndef	_HBP
#define	_HBP	14 + 1
#endif
#ifndef	_VFP
#define _VFP	11
#endif
#ifndef	_VBP
#define _VBP	44
#endif

#define _LF	LINE30_ROW*16

#define	_GDC_RESET	0x00
#define _GDC_SYNC	0x0e
#define _GDC_MASTER	0x6f
#define _GDC_SLAVE	0x6e
#define _GDC_START	0x0d
#define _GDC_STOP	0x0c
#define _GDC_SCROLL	0x70
#define _GDC_PITCH	0x47

#define GDC_CR	0
#define GDC_VS	1
#define GDC_HS	2
#define GDC_HFP	3
#define GDC_HBP	4
#define GDC_VFP	5
#define GDC_VBP	6
#define GDC_LF	7


#define _24KHZ	0
#define _31KHZ	1

#define _2_5MHZ	0
#define _5MHZ	1

#define _25L		0
#define _30L		1

#define T25_G400	0
#define T30_G400	1
#define T30_G480	2

static void master_gdc_cmd(unsigned int);
static void master_gdc_prm(unsigned int);
static void master_gdc_word_prm(unsigned int);
#ifdef LINE30
static void master_gdc_fifo_empty(void);
#endif
static void master_gdc_wait_vsync(void);

static void gdc_cmd(unsigned int);
#ifdef LINE30
static void gdc_prm(unsigned int);
static void gdc_word_prm(unsigned int);
static void gdc_fifo_empty(void);
#endif
static void gdc_wait_vsync(void);

#ifdef LINE30
static int check_gdc_clock(void);

static int gdc_INFO = _25L;
#endif
static int gdc_FH = _24KHZ;
static void initialize_gdc(unsigned int, int);

#ifdef LINE30
static unsigned int master_param[2][2][8] = {
{{78,	 8,	7,	9,	7,	7,	25,	400},	/* 400/24k */
 {_CR-2, _VS,	_HS-1,	_HFP-1,	_HBP-1,	_VFP,	_VBP,	_LF}},	/* 480/24k */
{{78,	 2,	7,	3,	7,	13,	34,	400},	/* 400/31k */
 {78,	 2,	11,	3,	3,	6,	37,	480}}};	/* 480/31k */

static unsigned int slave_param[2][6][8] = {
{{38,	8,	3,	4,	3,	7,	25,	400},	/* normal */
 {78,	8,	7,	9,	7,	7,	25,	400},
 {_CR/2-2,	_VS,	(_HS)/2-1,	(_HFP)/2-1,	(_HBP)/2-1,
  _VFP+(_LF-400)/2+8,	_VBP+(_LF-400)/2-8,	400},		/* 30 & 400 */
 {_CR-2,	_VS,	_HS-1,	_HFP-1,	_HBP-1,
  _VFP+(_LF-400)/2+8,	_VBP+(_LF-400)/2-8,	400},
 {_CR/2-2,	_VS,	(_HS)/2-1,	(_HFP)/2-1,	(_HBP)/2-1,
  _VFP,	_VBP,	_LF},						/* 30 & 480 */
 {_CR-2,	_VS,	_HS-1,	_HFP-1,	_HBP-1,	_VFP,	_VBP,	_LF}},
{{38,	2,	3,	1,	3,	13,	34,	400},	/* normal */
 {78,	2,	7,	3,	7,	13,	34,	400},
 {38,	2,	5,	1,	1,	6+48,	37+32,	400},	/* 30 & 400 */
 {78,	2,	11,	3,	3,	6+48,	37+32,	400},
 {38,	2,	5,	1,	1,	6,	37,	480},	/* 30 & 480 */
 {78,	2,	11,	3,	3,	6,	37,	480}}};

static int SlavePCH[2] = {40,80};
static int MasterPCH = 80;
static int SlaveScrlLF[3] = {400,400,_LF};
#endif

#endif /* __PC98_PC98_30LINE_H__ */
