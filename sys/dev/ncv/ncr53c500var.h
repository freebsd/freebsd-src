/*	$FreeBSD$	*/
/*	$NecBSD: ncr53c500var.h,v 1.11.18.1 2001/06/08 06:27:45 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
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

#ifndef	_NCR53C500VAR_H_
#define	_NCR53C500VAR_H_

/*****************************************************************
 * Host adapter structure
 *****************************************************************/
struct ncv_softc {
	struct scsi_low_softc sc_sclow;		/* generic data */

	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_ioh;

	int port_rid;
	int port_rid_dmy;
	int irq_rid;
	int mem_rid;
	struct resource *port_res;
	struct resource *port_res_dmy;
	struct resource *irq_res;
	struct resource *mem_res;

	void *ncv_intrhand;

	int sc_tmaxcnt;
	int sc_selstop;			/* sel atn stop asserted */
	int sc_compseq;			/* completion seq cmd asserted */
	int sc_sdatalen;		/* start datalen */
	int sc_tdatalen;		/* temp xfer data len */

	struct ncv_hw sc_hw;		/* hardware register images */
};

/*****************************************************************
 * Lun information 
 *****************************************************************/
struct ncv_targ_info {
	struct targ_info nti_ti;
	
	u_int8_t nti_reg_cfg3;		/* cfg3 images per lun */
	u_int8_t nti_reg_offset;	/* synch offset register per lun */
	u_int8_t nti_reg_period;	/* synch period register per lun */
};

/*****************************************************************
 * Proto
 *****************************************************************/
int ncvprobesubr(bus_space_tag_t, bus_space_handle_t ioh, u_int, int);
void ncvattachsubr(struct ncv_softc *);
int ncvintr(void *);

#endif	/* !_NCR53C500VAR_H_ */
