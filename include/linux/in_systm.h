/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Miscellaneous internetwork definitions for kernel.
 *
 * Version:	@(#)in_systm.h  1.0.0   12/17/93
 *
 * Authors:	Original taken from Berkeley BSD UNIX 4.3-RENO.
 *		Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_IN_SYSTM_H
#define _LINUX_IN_SYSTM_H

/*
 * Network types.
 * The n_ types are network-order variants of their natural
 * equivalents.  The Linux kernel NET-2 code does not use
 * them (yet), but it might in the future.  This is mostly
 * there for compatibility with BSD user-level programs.
 */
typedef u_short	n_short;	/* short as received from the net	*/
typedef u_long	n_long;		/* long as received from the net	*/
typedef u_long	n_time;		/* ms since 00:00 GMT, byte rev		*/

#endif /* _LINUX_IN_SYSTM_H */
