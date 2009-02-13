/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	pcause1tr6.h - 1TR6 causes definitions
 *	--------------------------------------
 *
 *	$Id: pcause_1tr6.h,v 1.5 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD$
 *
 *	last edit-date: [Mon Dec 13 21:56:10 1999]
 *
 *---------------------------------------------------------------------------*/

char *print_cause_1tr6(unsigned char code);

/* 1TR6 protocol causes */

#define	CAUSE_1TR6_SHUTDN	0x00	/* normal D-channel shutdown */
#define CAUSE_1TR6_ICRV		0x01	/* invalid call reference value */
#define CAUSE_1TR6_BSNI		0x03	/* bearer service not implemented */
#define CAUSE_1TR6_CIDNE	0x07	/* call identity does not exist */
#define CAUSE_1TR6_CIIU		0x08	/* call identity in use */
#define CAUSE_1TR6_NCA		0x0A	/* no channel available */
#define CAUSE_1TR6_RFNI		0x10	/* requested facility not implemented */
#define CAUSE_1TR6_RFNS		0x11	/* requested facility not subscribed */
#define CAUSE_1TR6_OCB		0x20	/* outgoing calls barred */
#define CAUSE_1TR6_UAB		0x21	/* user access busy */
#define CAUSE_1TR6_NECUG	0x22	/* non existent CUG */
#define CAUSE_1TR6_NECUG1	0x23	/* non existent CUG */
#define CAUSE_1TR6_SPV		0x25	/* kommunikationsbeziehung als SPV nicht erlaubt */
#define CAUSE_1TR6_DNO		0x35	/* destination not obtainable */
#define CAUSE_1TR6_NC		0x38	/* number changed */
#define CAUSE_1TR6_OOO		0x39	/* out of order */
#define CAUSE_1TR6_NUR		0x3A	/* no user responding */
#define CAUSE_1TR6_UB		0x3B	/* user busy */
#define CAUSE_1TR6_ICB		0x3D	/* incoming calls barred */
#define CAUSE_1TR6_CR		0x3E	/* call rejected */
#define CAUSE_1TR6_NCO		0x59	/* network congestion */
#define CAUSE_1TR6_RUI		0x5A	/* remote user initiated */
#define CAUSE_1TR6_LPE		0x70	/* local procedure error */
#define CAUSE_1TR6_RPE		0x71	/* remote procedure error */
#define CAUSE_1TR6_RUS		0x72	/* remote user suspended */
#define CAUSE_1TR6_RUR		0x73	/* remote user resumed */
#define CAUSE_1TR6_UIDL		0x7F	/* user info discharded locally */

/* EOF */
