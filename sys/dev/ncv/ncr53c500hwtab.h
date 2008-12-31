/*	$FreeBSD: src/sys/dev/ncv/ncr53c500hwtab.h,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $	*/
/*	$NecBSD: ncr53c500hwtab.h,v 1.2.18.1 2001/06/08 06:27:44 honda Exp $	*/
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

static struct ncv_hw ncv_template = {
	0,			/* CFG1 img */
	C2_FE | C2_SCSI2,	/* CFG2 img */
	0,			/* CFG3 img */
	C4_ANE,			/* CFG4 img */
	0x80,			/* CFG5 img */

	CLK_40M_F,		/* clock */
	200 / 4,		/* max period */
	15,			/* max offset */

	0x10,			/* CFG3_FSCSI bit */
	0x08,			/* CFG3_FCLK bit */
};
