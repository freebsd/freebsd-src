/*      $NetBSD: if_atm.h,v 1.7 1996/11/09 23:02:27 chuck Exp $       */
/* $FreeBSD: src/sys/net/if_atm.h,v 1.8 2003/05/05 16:35:52 harti Exp $ */

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *	Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * net/if_atm.h
 */

/*
 * Classification of ATM cards.
 */
#define ATM_DEVICE_UNKNOWN	0
#define ATM_DEVICE_PCA200E	1	/* Fore/Marconi PCA200-E */
#define ATM_DEVICE_HE155	2	/* Fore/Marconi HE155 */
#define ATM_DEVICE_HE622	3	/* Fore/Marconi HE622 */
#define ATM_DEVICE_ENI155P	4	/* Efficient networks 155p */
#define ATM_DEVICE_ADP155P	5	/* Adaptec 155p */

/* map to strings and vendors */
#define ATM_DEVICE_NAMES					\
	{ "Unknown",	"Unknown" },				\
	{ "PCA200-E",	"Fore/Marconi" },			\
	{ "HE155",	"Fore/Marconi" },			\
	{ "HE622",	"Fore/Marconi" },			\
	{ "ENI155p",	"Efficient Networks" },			\
	{ "ADP155p",	"Adaptec" },

/*
 * This is the common link layer MIB for all ATM interfaces. Much of the
 * information here is needed for ILMI. This will be augmented by statistics
 * at some point.
 */
struct ifatm_mib {
	/* configuration data */
	uint8_t		device;		/* type of card */
	u_char		esi[6];		/* end system identifier (MAC) */
	uint32_t	serial;		/* card serial number */
	uint32_t	hw_version;	/* card version */
	uint32_t	sw_version;	/* firmware version (if any) */
	uint32_t	pcr;		/* supported peak cell rate */
	uint32_t	media;		/* physical media */
	uint8_t		vpi_bits;	/* number of used bits in VPI field */
	uint8_t		vci_bits;	/* number of used bits in VCI field */
	uint16_t	max_vpcs;	/* maximum number of VPCs */
	uint32_t	max_vccs;	/* maximum number of VCCs */
};

#ifdef _KERNEL
/*
 * Common fields for all ATM interfaces. Each driver's softc must start with
 * this structure.
 */
struct ifatm {
	struct ifnet	ifnet;		/* required by if_var.h */
	struct ifatm_mib mib;		/* exported data */
	void		*phy;		/* usually SUNI */
	void		*ngpriv;	/* netgraph link */
};
#endif

/*
 * Peak cell rates for various physical media. Note, that there are
 * different opinions on what the correct values are.
 */
#define ATM_RATE_155M		353208
#define ATM_RATE_622M		1412830
#define ATM_RATE_24G		5651320

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
#define RTALLOC1(A,B)		rtalloc1((A),(B))
#elif defined(__FreeBSD__)
#define RTALLOC1(A,B)		rtalloc1((A),(B),0UL)
#endif

/*
 * pseudo header for packet transmission
 */
struct atm_pseudohdr {
  u_int8_t atm_ph[4];	/* flags+VPI+VCI1(msb)+VCI2(lsb) */
};

#define ATM_PH_FLAGS(X)	((X)->atm_ph[0])
#define ATM_PH_VPI(X)	((X)->atm_ph[1])
#define ATM_PH_VCI(X)	((((X)->atm_ph[2]) << 8) | ((X)->atm_ph[3]))
#define ATM_PH_SETVCI(X,V) { \
	(X)->atm_ph[2] = ((V) >> 8) & 0xff; \
	(X)->atm_ph[3] = ((V) & 0xff); \
}

#define ATM_PH_AAL5    0x01	/* use AAL5? (0 == aal0) */
#define ATM_PH_LLCSNAP 0x02	/* use the LLC SNAP encoding (iff aal5) */

#define ATM_PH_DRIVER7  0x40	/* reserve for driver's use */
#define ATM_PH_DRIVER8  0x80	/* reserve for driver's use */

#define ATMMTU		9180	/* ATM MTU size for IP */
				/* XXX: could be 9188 with LLC/SNAP according
					to comer */

/* user's ioctl hook for raw atm mode */
#define SIOCRAWATM	_IOWR('a', 122, int)	/* set driver's raw mode */

/* atm_pseudoioctl: turns on and off RX VCIs  [for internal use only!] */
struct atm_pseudoioctl {
  struct atm_pseudohdr aph;
  void *rxhand;
};
#define SIOCATMENA	_IOWR('a', 123, struct atm_pseudoioctl) /* enable */
#define SIOCATMDIS	_IOWR('a', 124, struct atm_pseudoioctl) /* disable */


/*
 * XXX forget all the garbage in if_llc.h and do it the easy way
 */

#define ATMLLC_HDR "\252\252\3\0\0\0"
struct atmllc {
  u_int8_t llchdr[6];	/* aa.aa.03.00.00.00 */
  u_int8_t type[2];	/* "ethernet" type */
};

/* ATM_LLC macros: note type code in host byte order */
#define ATM_LLC_TYPE(X) (((X)->type[0] << 8) | ((X)->type[1]))
#define ATM_LLC_SETTYPE(X,V) { \
	(X)->type[0] = ((V) >> 8) & 0xff; \
	(X)->type[1] = ((V) & 0xff); \
}

#ifdef _KERNEL
void	atm_ifattach(struct ifnet *);
void	atm_ifdetach(struct ifnet *);
void	atm_input(struct ifnet *, struct atm_pseudohdr *,
		struct mbuf *, void *);
int	atm_output(struct ifnet *, struct mbuf *, struct sockaddr *, 
		struct rtentry *);
#endif

