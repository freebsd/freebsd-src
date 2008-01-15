/*	$FreeBSD: src/sys/dev/stg/tmc18c30var.h,v 1.8 2005/01/06 01:43:23 imp Exp $	*/
/*	$NecBSD: tmc18c30var.h,v 1.12.18.2 2001/06/13 05:51:23 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998, 1999, 2000, 2001
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
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

#ifndef	_TMC18C30VAR_H_
#define	_TMC18C30VAR_H_

/*****************************************************************
 * Host adapter structure
 *****************************************************************/
struct stg_softc {
	struct scsi_low_softc sc_sclow;	/* generic data */

#ifdef	__NetBSD__
	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_ioh;

	void *sc_ih;
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_ioh;

	int port_rid;
	int irq_rid;
	int mem_rid;
	struct resource *port_res;
	struct resource *irq_res;
	struct resource *mem_res;

	void *stg_intrhand;
#endif	/* __FreeBSD__ */

	int sc_tmaxcnt;
	u_int sc_chip;			/* chip type */
	u_int sc_fsz;			/* fifo size */
	u_int sc_idbit;			/* host id bit */
	u_int sc_wthold;		/* write thread */
	u_int sc_rthold;		/* read thread */
	u_int sc_maxwsize;		/* max write size */
	int sc_dataout_timeout;		/* data out timeout counter */
	int sc_ubf_timeout;		/* unexpected bus free timeout */

	u_int8_t sc_fcWinit;		/* write flags */
	u_int8_t sc_fcRinit;		/* read flags */

	u_int8_t sc_icinit;		/* interrupt masks */
	u_int8_t sc_busc;		/* default bus control register */
	u_int8_t sc_imsg;		/* identify msg required */
	u_int8_t sc_busimg;		/* bus control register image */
};

/*****************************************************************
 * Lun information 
 *****************************************************************/
struct stg_targ_info {
	struct targ_info sti_ti;		/* generic data */

	u_int8_t sti_reg_synch;		/* synch register per lun */
};

/*****************************************************************
 * Proto
 *****************************************************************/
int stgprobesubr(bus_space_tag_t, bus_space_handle_t, u_int);
void stgattachsubr(struct stg_softc *);
int stgprint(void *, const char *);
int stgintr(void *);

#if	defined(__i386__) && 0
#define	SOFT_INTR_REQUIRED(slp)	(softintr((slp)->sl_irq))
#else	/* !__i386__ */
#define	SOFT_INTR_REQUIRED(slp)
#endif	/* !__i386__ */
#endif	/* !_TMC18C30VAR_H_ */
