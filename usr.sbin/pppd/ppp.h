/*
 * ppp.h - PPP global declarations.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD$
 */

#ifndef __PPP_H__
#define __PPP_H__

#define NPPP	1		/* One PPP interface supported (per process) */

/*
 * Data Link Layer header = Address, Control, Protocol.
 */
#define ALLSTATIONS	0xff	/* All-Stations Address */
#define UI		0x03	/* Unnumbered Information */
#define LCP		0xc021	/* Link Control Protocol */
#define IPCP		0x8021	/* IP Control Protocol */
#define UPAP		0xc023	/* User/Password Authentication Protocol */
#define CHAP            0xc223  /* Crytpographic Handshake Protocol */
#define LQR		0xc025	/* Link Quality Report protocol */
#define IP_VJ_COMP	0x002d	/* VJ TCP compressed IP packet */
#define DLLHEADERLEN	(sizeof (u_char) + sizeof (u_char) + sizeof (u_short))
#define MTU		1500	/* Default MTU */

#endif /* __PPP_H__ */
