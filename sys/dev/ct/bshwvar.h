/* $FreeBSD$ */
/*	$NecBSD: bshwvar.h,v 1.3 1999/04/15 01:36:10 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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
#ifndef	_BSHWVAR_H_
#define	_BSHWVAR_H_

/*
 * bshwvar.h
 * NEC 55 compatible board specific definitions
 */

#define	BSHW_DEFAULT_CHIPCLK	20	/* 20MHz */
#define	BSHW_DEFAULT_HOSTID	7

struct bshw {
#define	BSHW_SYNC_RELOAD	0x01
#define	BSHW_SMFIFO		0x02
#define	BSHW_DOUBLE_DMACHAN	0x04
	u_int hw_flags;

	u_int sregaddr;

	int ((*dma_init) __P((struct ct_softc *)));
	void ((*dma_start) __P((struct ct_softc *)));
	void ((*dma_stop) __P((struct ct_softc *)));
};

struct bshw_softc {
	int sc_hostid;
	int sc_irq;			/* irq */
	int sc_drq;			/* drq */

	/* dma transfer */
	u_int8_t *sc_segaddr;
	u_int8_t *sc_bufp;
	int sc_seglen;
	int sc_tdatalen;		/* temp datalen */

	/* private bounce */
	u_int8_t *sc_bounce_phys;
	u_int8_t *sc_bounce_addr;
	u_int sc_bounce_size;
	bus_addr_t sc_minphys;
	
	/* hardware */
	struct bshw *sc_hw;
};

void bshw_synch_setup __P((struct ct_softc *, struct lun_info *));
void bshw_bus_reset __P((struct ct_softc *));
int bshw_read_settings __P((bus_space_tag_t, bus_space_handle_t, struct bshw_softc *));
void bshw_smit_xfer_start __P((struct ct_softc *));
void bshw_smit_xfer_stop __P((struct ct_softc *));
void bshw_dma_xfer_start __P((struct ct_softc *));
void bshw_dma_xfer_stop __P((struct ct_softc *));
extern struct dvcfg_hwsel bshw_hwsel;
#endif	/* !_BSHWVAR_H_ */
