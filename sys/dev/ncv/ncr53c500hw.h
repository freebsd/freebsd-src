/*	$FreeBSD$	*/
/*	$NecBSD: ncr53c500hw.h,v 1.6.18.1 2001/06/08 06:27:44 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__NCR53C500HW_H_
#define	__NCR53C500HW_H_

#include <compat/netbsd/dvcfg.h>

#define	NCV_HOSTID	7
#define	NCV_NTARGETS	8
#define	NCV_NLUNS	8

struct ncv_hw {
	/* configuration images */
	u_int8_t hw_cfg1;
	u_int8_t hw_cfg2;
	u_int8_t hw_cfg3;
	u_int8_t hw_cfg4;
	u_int8_t hw_cfg5;

	/* synch */
	u_int8_t hw_clk;
	u_int8_t hw_mperiod;
	u_int8_t hw_moffset;

	/* cfg3 quirks */
	u_int8_t hw_cfg3_fscsi;
	u_int8_t hw_cfg3_fclk;
};

/* dvcfg */
#define	NCV_C5IMG(flags)	((DVCFG_MAJOR(flags) >> 8) & 0xff)
#define	NCV_CLKFACTOR(flags)	(DVCFG_MAJOR(flags) & 0x0f)
#define	NCVHWCFG_MAX10M		0x01
#define	NCVHWCFG_SCSI1		0x02
#define	NCVHWCFG_SLOW		0x04
#define	NCVHWCFG_FIFOBUG	0x08
#define	NCV_SPECIAL(flags)	((DVCFG_MAJOR(flags) >> 4) & 0x0f)
#endif	/* !__NCR53C500HW_H_ */
