/*	$FreeBSD$	*/
/*	$NecBSD: tmc18c30var.h,v 1.12 1998/11/30 00:08:30 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
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

	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_ioh;

	void *sc_ih;

	u_int sc_chip;			/* chip type */
	u_int sc_fsz;			/* fifo size */
	u_int sc_idbit;			/* host id bit */
	u_int8_t sc_fcb;		/* fifo intr thread */

	u_int8_t sc_fcWinit;		/* write flags */
	u_int8_t sc_fcRinit;		/* read flags */

	u_int8_t sc_fcsp;		/* special control flags */
	u_int8_t sc_icinit;		/* interrupt masks */
	u_int8_t sc_busc;		/* default bus control register */
	u_int8_t sc_imsg;		/* identify msg required */
	u_int8_t sc_busimg;		/* bus control register image */
#if defined (__FreeBSD__) && __FreeBSD_version >= 400001
	int port_rid;
	int irq_rid;
	int mem_rid;
	struct resource *port_res;
	struct resource *irq_res;
	struct resource *mem_res;
	void *stg_intrhand;
#endif
};

/*****************************************************************
 * Lun information 
 *****************************************************************/
struct stg_lun_info {
	struct lun_info sli_li;		/* generic data */

	u_int8_t sli_reg_synch;		/* synch register per lun */
};

/*****************************************************************
 * Proto
 *****************************************************************/
int stgprobesubr __P((bus_space_tag_t, bus_space_handle_t, u_int));
void stgattachsubr __P((struct stg_softc *));
int stgprint __P((void *, const char *));
int stgintr __P((void *));

#if	defined(i386)
#define	SOFT_INTR_REQUIRED(slp)	(softintr((slp)->sl_irq))
#else	/* !i386 */
#define	SOFT_INTR_REQUIRED(slp)
#endif	/* !i386 */
#endif	/* !_TMC18C30VAR_H_ */
