/*	$FreeBSD$	*/
/*	$NecBSD: nspvar.h,v 1.7 1999/04/15 01:35:55 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
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

#ifndef	_NSPVAR_H_
#define	_NSPVAR_H_

/*****************************************************************
 * Host adapter structure
 *****************************************************************/
struct nsp_softc {
	struct scsi_low_softc sc_sclow;		/* generic data */

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;

	void *sc_ih;

	int sc_seltout;				/* selection timeout counter */
	int sc_timer;				/* timer start */

	int sc_xmode;
#define	NSP_HIGH_SMIT		2		/* write address data mode */
#define	NSP_MID_SMIT		1		/* mem access */
#define	NSP_PIO			0		/* io access */

	u_int sc_idbit;				/* host id bit pattern */
	u_int sc_mask;				/* bus width mask */
	u_int sc_cnt;				/* fifo R/W count (host) */
	u_int8_t sc_iclkdiv;			/* scsi chip clock divisor */
	u_int8_t sc_clkdiv;			/* asic chip clock divisor */
	u_int8_t sc_xfermr;			/* fifo control reg */
	u_int8_t sc_icr;			/* interrupt control reg */

	u_int8_t sc_busc;			/* busc registers */
	u_long sc_ringp;			/* data buffer ring pointer */
#if defined (__FreeBSD__) && __FreeBSD_version >= 400001
	int port_rid;
	int irq_rid;
	int mem_rid;
	struct resource *port_res;
	struct resource *irq_res;
	struct resource *mem_res;
	void *nsp_intrhand;
#endif
};

/*****************************************************************
 * Lun information 
 *****************************************************************/
struct nsp_lun_info {
	struct lun_info nli_li;			/* generic lun info */

	u_int8_t nli_reg_syncr;			/* sync registers per devices */
	u_int8_t nli_reg_ackwidth;		/* ackwidth per devices */
};

/*****************************************************************
 * Proto
 *****************************************************************/
int nspprobesubr __P((bus_space_tag_t, bus_space_handle_t, u_int));
void nspattachsubr __P((struct nsp_softc *));
int nspprint __P((void *, const char *));
int nspintr __P((void *));

#if	defined(i386)
#define	SOFT_INTR_REQUIRED(slp)	(softintr((slp)->sl_irq))
#else	/* !i386 */
#define	SOFT_INTR_REQUIRED(slp)
#endif	/* !i386 */
#endif	/* !_NSPVAR_H_ */
