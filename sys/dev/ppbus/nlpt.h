/*-
 * Copyright (c) 1997 Nicolas Souchu
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Distantly from :
 *	@(#)lptreg.h      1.1 (Berkeley) 12/19/90
 *	Id: lptreg.h,v 1.6 1997/02/22 09:36:52 peter Exp 
 *
 *	$Id: nlpt.h,v 1.1 1997/08/14 13:57:40 msmith Exp $
 */
#ifndef __NLPT_H
#define __NLPT_H

#define	LPS_NERR		0x08	/* printer no error */
#define	LPS_SEL			0x10	/* printer selected */
#define	LPS_OUT			0x20	/* printer out of paper */
#define	LPS_NACK		0x40	/* printer no ack of data */
#define	LPS_NBSY		0x80	/* printer no ack of data */

#define	LPC_STB			0x01	/* strobe data to printer */
#define	LPC_AUTOL		0x02	/* automatic linefeed */
#define	LPC_NINIT		0x04	/* initialize printer */
#define	LPC_SEL			0x08	/* printer selected */
#define	LPC_ENA			0x10	/* enable IRQ */

struct lpt_data {
	unsigned short lpt_unit;

	struct ppb_device lpt_dev;

	short	sc_state;
	/* default case: negative prime, negative ack, handshake strobe,
	   prime once */
	u_char	sc_control;
	char	sc_flags;
#define LP_POS_INIT	0x04	/* if we are a postive init signal */
#define LP_POS_ACK	0x08	/* if we are a positive going ack */
#define LP_NO_PRIME	0x10	/* don't prime the printer at all */
#define LP_PRIMEOPEN	0x20	/* prime on every open */
#define LP_AUTOLF	0x40	/* tell printer to do an automatic lf */
#define LP_BYPASS	0x80	/* bypass  printer ready checks */
	struct	buf *sc_inbuf;
	short	sc_xfercnt ;
	char	sc_primed;
	char	*sc_cp ;
	u_char	sc_irq ;	/* IRQ status of port */
#define LP_HAS_IRQ	0x01	/* we have an irq available */
#define LP_USE_IRQ	0x02	/* we are using our irq */
#define LP_ENABLE_IRQ	0x04	/* enable IRQ on open */
	u_char	sc_backoff ;	/* time to call lptout() again */

#ifdef DEVFS
	void	*devfs_token;
	void	*devfs_token_ctl;
#endif
};

#endif
