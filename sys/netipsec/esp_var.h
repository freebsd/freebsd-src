/*	$FreeBSD: src/sys/netipsec/esp_var.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $	*/
/*	$OpenBSD: ip_esp.h,v 1.37 2002/06/09 16:26:10 itojun Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 2001 Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef _NETIPSEC_ESP_VAR_H_
#define _NETIPSEC_ESP_VAR_H_

/*
 * These define the algorithm indices into the histogram.  They're
 * presently based on the PF_KEY v2 protocol values which is bogus;
 * they should be decoupled from the protocol at which time we can
 * pack them and reduce the size of the array to a reasonable value.
 */
#define	ESP_ALG_MAX	256		/* NB: could be < but skipjack is 249 */

struct espstat {
	u_int32_t	esps_hdrops;	/* Packet shorter than header shows */
	u_int32_t	esps_nopf;	/* Protocol family not supported */
	u_int32_t	esps_notdb;
	u_int32_t	esps_badkcr;
	u_int32_t	esps_qfull;
	u_int32_t	esps_noxform;
	u_int32_t	esps_badilen;
	u_int32_t	esps_wrap;	/* Replay counter wrapped around */
	u_int32_t	esps_badenc;	/* Bad encryption detected */
	u_int32_t	esps_badauth;	/* Only valid for transforms with auth */
	u_int32_t	esps_replay;	/* Possible packet replay detected */
	u_int32_t	esps_input;	/* Input ESP packets */
	u_int32_t	esps_output;	/* Output ESP packets */
	u_int32_t	esps_invalid;	/* Trying to use an invalid TDB */
	u_int64_t	esps_ibytes;	/* Input bytes */
	u_int64_t	esps_obytes;	/* Output bytes */
	u_int32_t	esps_toobig;	/* Packet got larger than IP_MAXPACKET */
	u_int32_t	esps_pdrops;	/* Packet blocked due to policy */
	u_int32_t	esps_crypto;	/* Crypto processing failure */
	u_int32_t	esps_tunnel;	/* Tunnel sanity check failure */
	u_int32_t	esps_hist[ESP_ALG_MAX];	/* Per-algorithm op count */
};

#ifdef _KERNEL
extern	int esp_enable;
extern	struct espstat espstat;
#endif /* _KERNEL */
#endif /*_NETIPSEC_ESP_VAR_H_*/
