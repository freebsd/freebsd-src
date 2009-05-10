/*      $NetBSD: if_atm.h,v 1.7 1996/11/09 23:02:27 chuck Exp $       */
/* $FreeBSD$ */

/*-
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
#define	ATM_DEVICE_UNKNOWN	0
#define	ATM_DEVICE_PCA200E	1	/* Fore/Marconi PCA200-E */
#define	ATM_DEVICE_HE155	2	/* Fore/Marconi HE155 */
#define	ATM_DEVICE_HE622	3	/* Fore/Marconi HE622 */
#define	ATM_DEVICE_ENI155P	4	/* Efficient networks 155p */
#define	ATM_DEVICE_ADP155P	5	/* Adaptec 155p */
#define	ATM_DEVICE_FORELE25	6	/* ForeRunnerLE 25 */
#define	ATM_DEVICE_FORELE155	7	/* ForeRunnerLE 155 */
#define	ATM_DEVICE_NICSTAR25	8	/* other 77211 25.6MBit */
#define	ATM_DEVICE_NICSTAR155	9	/* other 77211 155MBit */
#define	ATM_DEVICE_IDTABR25	10	/* 77252 based card 25MBit */
#define	ATM_DEVICE_IDTABR155	11	/* 77252 based card 155MBit */
#define	ATM_DEVICE_PROATM25	12	/* 77252 based ProSum card 25MBit */
#define	ATM_DEVICE_PROATM155	13	/* 77252 based ProSum card 155MBit */
#define	ATM_DEVICE_VIRTUAL	14	/* virtual ATM device (netgraph) */

/* map to strings and vendors */
#define	ATM_DEVICE_NAMES						\
	{ "Unknown",		"Unknown" },				\
	{ "PCA200-E",		"Fore/Marconi" },			\
	{ "HE155",		"Fore/Marconi" },			\
	{ "HE622",		"Fore/Marconi" },			\
	{ "ENI155p",		"Efficient Networks" },			\
	{ "ADP155p",		"Adaptec" },				\
	{ "ForeRunnerLE25",	"Fore/Marconi" },			\
	{ "ForeRunnerLE155",	"Fore/Marconi" },			\
	{ "IDT77211/25",	"IDT" },				\
	{ "IDT77211/155",	"IDT" },				\
	{ "IDT77252/25",	"IDT" },				\
	{ "IDT77252/155",	"IDT" },				\
	{ "ProATM/25",		"ProSum" },				\
	{ "ProATM/155",		"ProSum" },				\
	{ "Virtual",		"NetGraph" },

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

/*
 * Traffic parameters for ATM connections. This contains all parameters
 * to accomodate UBR, UBR+MCR, CBR, VBR and ABR connections.
 *
 * Keep in sync with ng_atm.h
 */
struct atmio_tparam {
	uint32_t	pcr;	/* 24bit: Peak Cell Rate */
	uint32_t	scr;	/* 24bit: VBR Sustainable Cell Rate */
	uint32_t	mbs;	/* 24bit: VBR Maximum burst size */
	uint32_t	mcr;	/* 24bit: ABR/VBR/UBR+MCR MCR */
	uint32_t	icr;	/* 24bit: ABR ICR */
	uint32_t	tbe;	/* 24bit: ABR TBE (1...2^24-1) */
	uint8_t		nrm;	/*  3bit: ABR Nrm */
	uint8_t		trm;	/*  3bit: ABR Trm */
	uint16_t	adtf;	/* 10bit: ABR ADTF */
	uint8_t		rif;	/*  4bit: ABR RIF */
	uint8_t		rdf;	/*  4bit: ABR RDF */
	uint8_t		cdf;	/*  3bit: ABR CDF */
};

/*
 * VCC parameters
 *
 * Keep in sync with ng_atm.h
 */
struct atmio_vcc {
	uint16_t	flags;		/* VCC flags */
	uint16_t	vpi;
	uint16_t	vci;
	uint16_t	rmtu;		/* maximum receive PDU */
	uint16_t	tmtu;		/* maximum transmit PDU */
	uint8_t		aal;		/* aal type */
	uint8_t		traffic;	/* traffic type */
	struct atmio_tparam tparam;	/* traffic parameters */
};

/* VCC flags */
#define	ATMIO_FLAG_LLCSNAP	0x0002	/* same as ATM_PH_LLCSNAP */
#define	ATMIO_FLAG_NG		0x0010	/* owned by netgraph */
#define	ATMIO_FLAG_HARP		0x0020	/* owned by HARP */
#define	ATMIO_FLAG_NORX		0x0100	/* not receiving on this VCC */
#define	ATMIO_FLAG_NOTX		0x0200	/* not transmitting on this VCC */
#define	ATMIO_FLAG_PVC		0x0400	/* this is a PVC */
#define	ATMIO_FLAG_ASYNC	0x0800	/* async open/close */
#define	ATMIO_FLAGS	"\020\2LLCSNAP\5NG\6HARP\11NORX\12NOTX\13PVC\14ASYNC"

#define	ATMIO_AAL_0		0	/* pure cells */
#define	ATMIO_AAL_34		4	/* AAL3 and 4 */
#define	ATMIO_AAL_5		5	/* AAL5 */
#define	ATMIO_AAL_RAW		10	/* whatever the card does */

#define	ATMIO_TRAFFIC_UBR	0
#define	ATMIO_TRAFFIC_CBR	1
#define	ATMIO_TRAFFIC_ABR	2
#define	ATMIO_TRAFFIC_VBR	3

/*
 * VCC table
 *
 * Keep in sync with ng_atm.h
 */
struct atmio_vcctable {
	uint32_t	count;		/* number of vccs */
	struct atmio_vcc vccs[0];	/* array of VCCs */
};

/*
 * Peak cell rates for various physical media. Note, that there are
 * different opinions on what the correct values are.
 */
#define	ATM_RATE_25_6M		59259
#define	ATM_RATE_155M		353208
#define	ATM_RATE_622M		1412830
#define	ATM_RATE_2_4G		5651320

#ifdef _KERNEL
/*
 * Common fields for all ATM interfaces. Each driver's softc must start with
 * this structure.
 */
struct ifatm {
	struct ifnet	*ifp;
	struct ifatm_mib mib;		/* exported data */
	void		*phy;		/* usually SUNI */
	void		*ngpriv;	/* netgraph link */
};
#define	IFP2IFATM(ifp)	((struct ifatm *)(ifp)->if_l2com)
#endif

/*
 * Keep structures in sync with ng_atm.h
 *
 * These are used by netgraph/harp to call the driver
 * NATM uses the atm_pseudoioctl instead.
 */
struct atmio_openvcc {
	void		*rxhand;	/* handle argument */
	struct atmio_vcc param;		/* parameters */
};

struct atmio_closevcc {
	uint16_t	vpi;
	uint16_t	vci;
};

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
#define	RTALLOC1(A,B)		rtalloc1((A),(B))
#elif defined(__FreeBSD__)
#define	RTALLOC1(A,B)		rtalloc1((A),(B),0UL)
#endif

/*
 * pseudo header for packet transmission
 */
struct atm_pseudohdr {
	uint8_t		atm_ph[4];	/* flags+VPI+VCI1(msb)+VCI2(lsb) */
};

#define	ATM_PH_FLAGS(X)	((X)->atm_ph[0])
#define	ATM_PH_VPI(X)	((X)->atm_ph[1])
#define	ATM_PH_VCI(X)	((((X)->atm_ph[2]) << 8) | ((X)->atm_ph[3]))
#define	ATM_PH_SETVCI(X,V) { \
	(X)->atm_ph[2] = ((V) >> 8) & 0xff; \
	(X)->atm_ph[3] = ((V) & 0xff); \
}

/* use AAL5? (0 == aal0) */
#define	ATM_PH_AAL5	0x01
/* use the LLC SNAP encoding (iff aal5) */
#define	ATM_PH_LLCSNAP ATMIO_FLAG_LLCSNAP

#define	ATM_PH_DRIVER7  0x40	/* reserve for driver's use */
#define	ATM_PH_DRIVER8  0x80	/* reserve for driver's use */

#define	ATMMTU		9180	/* ATM MTU size for IP */
				/* XXX: could be 9188 with LLC/SNAP according
					to comer */

#define	SIOCATMGETVCCS	_IOW('a', 125, struct atmio_vcctable)
#define	SIOCATMOPENVCC	_IOR('a', 126, struct atmio_openvcc)
#define	SIOCATMCLOSEVCC _IOR('a', 127, struct atmio_closevcc)

#define	SIOCATMGVCCS	_IOWR('i', 230, struct ifreq)

/*
 * XXX forget all the garbage in if_llc.h and do it the easy way
 */
#define	ATMLLC_HDR "\252\252\3\0\0\0"
struct atmllc {
	uint8_t		llchdr[6];	/* aa.aa.03.00.00.00 */
	uint8_t		type[2];	/* "ethernet" type */
};

/* ATM_LLC macros: note type code in host byte order */
#define	ATM_LLC_TYPE(X) (((X)->type[0] << 8) | ((X)->type[1]))
#define	ATM_LLC_SETTYPE(X, V) do {		\
	(X)->type[0] = ((V) >> 8) & 0xff;	\
	(X)->type[1] = ((V) & 0xff);		\
    } while (0)

/*
 * Events that are emitted by the driver. Currently the only consumer
 * of this is the netgraph node.
 */
#define	ATMEV_FLOW_CONTROL	0x0001	/* channel busy state changed */
#define	ATMEV_IFSTATE_CHANGED	0x0002	/* up/down or carrier */
#define	ATMEV_VCC_CHANGED	0x0003	/* PVC deleted/create */
#define	ATMEV_ACR_CHANGED	0x0004	/* ABR ACR has changed */

struct atmev_flow_control {
	uint16_t	vpi;		/* channel that is changed */
	uint16_t	vci;
	u_int		busy : 1;	/* != 0 -> ATM layer busy */
};

struct atmev_ifstate_changed {
	u_int		running : 1;	/* interface is running now */
	u_int		carrier : 1;	/* carrier detected (or not) */
};

struct atmev_vcc_changed {
	uint16_t	vpi;		/* channel that is changed */
	uint16_t	vci;
	u_int		up : 1;		/* 1 - created, 0 - deleted */
};

struct atmev_acr_changed {
	uint16_t	vpi;		/* channel that is changed */
	uint16_t	vci;
	uint32_t	acr;		/* new ACR */
};

#ifdef _KERNEL
void	atm_ifattach(struct ifnet *);
void	atm_ifdetach(struct ifnet *);
void	atm_input(struct ifnet *, struct atm_pseudohdr *,
	    struct mbuf *, void *);
int	atm_output(struct ifnet *, struct mbuf *, struct sockaddr *, 
	    struct route *);
struct atmio_vcctable *atm_getvccs(struct atmio_vcc **, u_int, u_int,
	    struct mtx *, int);

void	atm_event(struct ifnet *, u_int, void *);

#define	ATMEV_SEND_FLOW_CONTROL(ATMIF, VPI, VCI, BUSY)			\
	do {								\
		struct atmev_flow_control _arg;				\
		_arg.vpi = (VPI);					\
		_arg.vci = (VCI);					\
		_arg.busy = (BUSY);					\
		atm_event((ATMIF)->ifp, ATMEV_FLOW_CONTROL, &_arg);	\
	} while (0)

#define	ATMEV_SEND_VCC_CHANGED(ATMIF, VPI, VCI, UP)			\
	do {								\
		struct atmev_vcc_changed _arg;				\
		_arg.vpi = (VPI);					\
		_arg.vci = (VCI);					\
		_arg.up = (UP);						\
		atm_event((ATMIF)->ifp, ATMEV_VCC_CHANGED, &_arg);	\
	} while (0)

#define	ATMEV_SEND_IFSTATE_CHANGED(ATMIF, CARRIER)			\
	do {								\
		struct atmev_ifstate_changed _arg;			\
		_arg.running = (((ATMIF)->ifp->if_drv_flags &		\
		    IFF_DRV_RUNNING) != 0);				\
		_arg.carrier = ((CARRIER) != 0);			\
		atm_event((ATMIF)->ifp, ATMEV_IFSTATE_CHANGED, &_arg); \
	} while (0)

#define	ATMEV_SEND_ACR_CHANGED(ATMIF, VPI, VCI, ACR)			\
	do {								\
		struct atmev_acr_changed _arg;				\
		_arg.vpi = (VPI);					\
		_arg.vci = (VCI);					\
		_arg.acr= (ACR);					\
		atm_event((ATMIF)->ifp, ATMEV_ACR_CHANGED, &_arg);	\
	} while (0)
#endif
