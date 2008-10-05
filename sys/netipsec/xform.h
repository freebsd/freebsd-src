/*	$FreeBSD$	*/
/*	$OpenBSD: ip_ipsp.h,v 1.119 2002/03/14 01:27:11 millert Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr),
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Niklas Hallqvist (niklas@appli.se).
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
 * Additional features in 1999 by Angelos D. Keromytis and Niklas Hallqvist.
 *
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 1999 Niklas Hallqvist.
 * Copyright (c) 2001, Angelos D. Keromytis.
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

#ifndef _NETIPSEC_XFORM_H_
#define _NETIPSEC_XFORM_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <opencrypto/xform.h>

#define	AH_HMAC_HASHLEN		12	/* 96 bits of authenticator */
#define	AH_HMAC_INITIAL_RPL	1	/* replay counter initial value */

/*
 * Packet tag assigned on completion of IPsec processing; used
 * to speedup processing when/if the packet comes back for more
 * processing.
 */
struct tdb_ident {
	u_int32_t spi;
	union sockaddr_union dst;
	u_int8_t proto;
	/* Cache those two for enc(4) in xform_ipip. */
	u_int8_t alg_auth;
	u_int8_t alg_enc;
};

/*
 * Opaque data structure hung off a crypto operation descriptor.
 */
struct tdb_crypto {
	struct ipsecrequest	*tc_isr;	/* ipsec request state */
	u_int32_t		tc_spi;		/* associated SPI */
	union sockaddr_union	tc_dst;		/* dst addr of packet */
	u_int8_t		tc_proto;	/* current protocol, e.g. AH */
	u_int8_t		tc_nxt;		/* next protocol, e.g. IPV4 */
	int			tc_protoff;	/* current protocol offset */
	int			tc_skip;	/* data offset */
	caddr_t			tc_ptr;		/* associated crypto data */
};

struct secasvar;
struct ipescrequest;

struct xformsw {
	u_short	xf_type;		/* xform ID */
#define	XF_IP4		1	/* IP inside IP */
#define	XF_AH		2	/* AH */
#define	XF_ESP		3	/* ESP */
#define	XF_TCPSIGNATURE	5	/* TCP MD5 Signature option, RFC 2358 */
#define	XF_IPCOMP	6	/* IPCOMP */
	u_short	xf_flags;
#define	XFT_AUTH	0x0001
#define	XFT_CONF	0x0100
#define	XFT_COMP	0x1000
	char	*xf_name;			/* human-readable name */
	int	(*xf_init)(struct secasvar*, struct xformsw*);	/* setup */
	int	(*xf_zeroize)(struct secasvar*);		/* cleanup */
	int	(*xf_input)(struct mbuf*, struct secasvar*,	/* input */
			int, int);
	int	(*xf_output)(struct mbuf*,	       		/* output */
			struct ipsecrequest *, struct mbuf **, int, int);
	struct xformsw *xf_next;		/* list of registered xforms */
};

#ifdef _KERNEL
extern void xform_register(struct xformsw*);
extern int xform_init(struct secasvar *sav, int xftype);

struct cryptoini;

/* XF_IP4 */
extern	int ip4_input6(struct mbuf **m, int *offp, int proto);
extern	void ip4_input(struct mbuf *m, int);
extern	int ipip_output(struct mbuf *, struct ipsecrequest *,
			struct mbuf **, int, int);

/* XF_AH */
extern int ah_init0(struct secasvar *, struct xformsw *, struct cryptoini *);
extern int ah_zeroize(struct secasvar *sav);
extern struct auth_hash *ah_algorithm_lookup(int alg);
extern size_t ah_hdrsiz(struct secasvar *);

/* XF_ESP */
extern struct enc_xform *esp_algorithm_lookup(int alg);
extern size_t esp_hdrsiz(struct secasvar *sav);

/* XF_COMP */
extern struct comp_algo *ipcomp_algorithm_lookup(int alg);

#endif /* _KERNEL */
#endif /* _NETIPSEC_XFORM_H_ */
