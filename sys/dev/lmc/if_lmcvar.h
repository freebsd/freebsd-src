/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * Copyright (c) LAN Media Corporation 1998, 1999.
 * Copyright (c) 2000 Stephen Kiernan (sk-ports@vegamuse.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * $FreeBSD$
 *	From NetBSD: if_devar.h,v 1.21 1997/10/16 22:02:32 matt Exp
 *	$Id: if_lmcvar.h,v 1.6 1999/01/12 14:16:58 explorer Exp $
 */

#if !defined(_DEV_LMC_IF_LMCVAR_H)
#define _DEV_LMC_IF_LMCVAR_H

#define LMC_MTU 1500
#define PPP_HEADER_LEN 4
#define BIG_PACKET

#if !defined(PCI_VENDOR_LMC)
#define PCI_VENDOR_LMC		0x1376
#endif
#if !defined(PCI_PRODUCT_LMC_HSSI)
#define PCI_PRODUCT_LMC_HSSI	0x0003
#endif
#if !defined(PCI_PRODUCT_LMC_DS3)
#define PCI_PRODUCT_LMC_DS3	0x0004
#endif
#if !defined(PCI_PRODUCT_LMC_SSI)
#define PCI_PRODUCT_LMC_SSI	0x0005
#endif
#if !defined(PCI_PRODUCT_LMC_T1)
#define PCI_PRODUCT_LMC_T1	0x0006 
#endif

#ifdef LMC_IOMAPPED
#define	LMC_EISA_CSRSIZE	16
#define	LMC_EISA_CSROFFSET	0
#define	LMC_PCI_CSRSIZE	8
#define	LMC_PCI_CSROFFSET	0

typedef u_int16_t lmc_csrptr_t;

#define	LMC_CSR_READ(sc, csr)			(inl((sc)->lmc_csrs.csr))
#define	LMC_CSR_WRITE(sc, csr, val)   	outl((sc)->lmc_csrs.csr, val)

#define	LMC_CSR_READBYTE(sc, csr)		(inb((sc)->lmc_csrs.csr))
#define	LMC_CSR_WRITEBYTE(sc, csr, val)	outb((sc)->lmc_csrs.csr, val)

#else /* LMC_IOMAPPED */

#define	LMC_PCI_CSRSIZE	8
#define	LMC_PCI_CSROFFSET	0

typedef volatile u_int32_t *lmc_csrptr_t;

/*
 * macros to read and write CSRs.  Note that the "0 +" in
 * READ_CSR is to prevent the macro from being an lvalue
 * and WRITE_CSR shouldn't be assigned from.
 */
#define	LMC_CSR_READ(sc, csr)		(0 + *(sc)->lmc_csrs.csr)
#define	LMC_CSR_WRITE(sc, csr, val)	((void)(*(sc)->lmc_csrs.csr = (val)))

#endif /* LMC_IOMAPPED */

/*
 * This structure contains "pointers" for the registers on
 * the various 21x4x chips.  CSR0 through CSR8 are common
 * to all chips.  After that, it gets messy...
 */
typedef struct {
    lmc_csrptr_t csr_busmode;			/* CSR0 */
    lmc_csrptr_t csr_txpoll;			/* CSR1 */
    lmc_csrptr_t csr_rxpoll;			/* CSR2 */
    lmc_csrptr_t csr_rxlist;			/* CSR3 */
    lmc_csrptr_t csr_txlist;			/* CSR4 */
    lmc_csrptr_t csr_status;			/* CSR5 */
    lmc_csrptr_t csr_command;			/* CSR6 */
    lmc_csrptr_t csr_intr;			/* CSR7 */
    lmc_csrptr_t csr_missed_frames;		/* CSR8 */
    lmc_csrptr_t csr_9;			/* CSR9 */
    lmc_csrptr_t csr_10;			/* CSR10 */
    lmc_csrptr_t csr_11;			/* CSR11 */
    lmc_csrptr_t csr_12;			/* CSR12 */
    lmc_csrptr_t csr_13;			/* CSR13 */
    lmc_csrptr_t csr_14;			/* CSR14 */
    lmc_csrptr_t csr_15;			/* CSR15 */
} lmc_regfile_t;

#define	csr_enetrom		csr_9	/* 21040 */
#define	csr_reserved		csr_10	/* 21040 */
#define	csr_full_duplex		csr_11	/* 21040 */
#define	csr_bootrom		csr_10	/* 21041/21140A/?? */
#define	csr_gp			csr_12	/* 21140* */
#define	csr_watchdog		csr_15	/* 21140* */
#define	csr_gp_timer		csr_11	/* 21041/21140* */
#define	csr_srom_mii		csr_9	/* 21041/21140* */
#define	csr_sia_status		csr_12	/* 2104x */
#define csr_sia_connectivity	csr_13	/* 2104x */
#define csr_sia_tx_rx		csr_14	/* 2104x */
#define csr_sia_general		csr_15	/* 2104x */

/*
 * While 21x4x allows chaining of its descriptors, this driver
 * doesn't take advantage of it.  We keep the descriptors in a
 * traditional FIFO ring.  
 */
typedef struct {
    tulip_desc_t *ri_first;	/* first entry in ring */
    tulip_desc_t *ri_last;	/* one after last entry */
    tulip_desc_t *ri_nextin;	/* next to processed by host */
    tulip_desc_t *ri_nextout;	/* next to processed by adapter */
    int ri_max;
    int ri_free;
} lmc_ringinfo_t;

/*
 * The 21040 has a stupid restriction in that the receive
 * buffers must be longword aligned.  But since Ethernet
 * headers are not a multiple of longwords in size this forces
 * the data to non-longword aligned.  Since IP requires the
 * data to be longword aligned, we need to copy it after it has
 * been DMA'ed in our memory.
 *
 * Since we have to copy it anyways, we might as well as allocate
 * dedicated receive space for the input.  This allows to use a
 * small receive buffer size and more ring entries to be able to
 * better keep with a flood of tiny Ethernet packets.
 *
 * The receive space MUST ALWAYS be a multiple of the page size.
 * And the number of receive descriptors multiplied by the size
 * of the receive buffers must equal the recevive space.  This
 * is so that we can manipulate the page tables so that even if a
 * packet wraps around the end of the receive space, we can 
 * treat it as virtually contiguous.
 *
 * The above used to be true (the stupid restriction is still true)
 * but we gone to directly DMA'ing into MBUFs (unless it's on an 
 * architecture which can't handle unaligned accesses) because with
 * 100Mb/s cards the copying is just too much of a hit.
 */

#define	LMC_RXDESCS		48
#define	LMC_TXDESCS		128
#define	LMC_RXQ_TARGET	32
#if LMC_RXQ_TARGET >= LMC_RXDESCS
#error LMC_RXQ_TARGET must be less than LMC_RXDESCS
#endif

#define	LMC_RX_BUFLEN		((MCLBYTES < 2048 ? MCLBYTES : 2048) - 16)

/*
 * The various controllers support.  Technically the DE425 is just
 * a 21040 on EISA.  But since it remarkably difference from normal
 * 21040s, we give it its own chip id.
 */

typedef enum {
    LMC_21140, LMC_21140A,
    LMC_CHIPID_UNKNOWN
} lmc_chipid_t;

#define	LMC_BIT(b)		(1L << ((int)(b)))

typedef struct lmc_xinfo {
        u_int32_t       Magic0;                         /* BEEFCAFE */

        u_int32_t       PciCardType;
        u_int32_t       PciSlotNumber;          /* PCI slot number       */

        u_int16_t       DriverMajorVersion;
        u_int16_t       DriverMinorVersion;
        u_int16_t       DriverSubVersion;

        u_int16_t       XilinxRevisionNumber;
        u_int16_t       MaxFrameSize;

        u_int16_t       t1_alarm1_status;
        u_int16_t       t1_alarm2_status;

        int                     link_status;
        u_int32_t       mii_reg16;

        u_int32_t       Magic1;                         /* DEADBEEF */
} LMC_XINFO;


typedef struct {
    /*
     * Transmit Statistics
     */
    u_int32_t dot3StatsSingleCollisionFrames;
    u_int32_t dot3StatsMultipleCollisionFrames;
    u_int32_t dot3StatsSQETestErrors;
    u_int32_t dot3StatsDeferredTransmissions;
    u_int32_t dot3StatsLateCollisions;
    u_int32_t dot3StatsExcessiveCollisions;
    u_int32_t dot3StatsCarrierSenseErrors;
    u_int32_t dot3StatsInternalMacTransmitErrors;
    u_int32_t dot3StatsInternalTransmitUnderflows;	/* not in rfc1650! */
    u_int32_t dot3StatsInternalTransmitBabbles;		/* not in rfc1650! */
    /*
     * Receive Statistics
     */
    u_int32_t dot3StatsMissedFrames;	/* not in rfc1650! */
    u_int32_t dot3StatsAlignmentErrors;
    u_int32_t dot3StatsFCSErrors;
    u_int32_t dot3StatsFrameTooLongs;
    u_int32_t dot3StatsInternalMacReceiveErrors;
} lmc_dot3_stats_t;

/*
 * Now to important stuff.  This is softc structure (where does softc
 * come from??? No idea) for the tulip device.  
 *
 */
struct lmc___softc {

    const char *lmc_name;
    int		lmc_unit;

    u_int8_t lmc_enaddr[6];		/* yes, a small hack... */
    lmc_regfile_t lmc_csrs;
    volatile u_int32_t lmc_txtick;
    volatile u_int32_t lmc_rxtick;
    u_int32_t lmc_flags;

    u_int32_t lmc_features;	/* static bits indicating features of chip */
    u_int32_t lmc_intrmask;	/* our copy of csr_intr */
    u_int32_t lmc_cmdmode;	/* our copy of csr_cmdmode */
    u_int32_t lmc_last_system_error : 3;	/* last system error (only value is LMC_SYSTEMERROR is also set) */
    u_int32_t lmc_system_errors;	/* number of system errors encountered */
    u_int32_t lmc_statusbits;	/* status bits from CSR5 that may need to be printed */

    u_int8_t lmc_revinfo;			/* revision of chip */
    u_int8_t lmc_cardtype;		/* LMC_CARDTYPE_HSSI or ..._DS3 */
    u_int32_t		lmc_gpio_io;	/* state of in/out settings */
    u_int32_t		lmc_gpio;	/* state of outputs */
    u_int8_t lmc_gp;

    lmc_chipid_t lmc_chipid;		/* type of chip we are using */
    u_int32_t lmc_miireg16;
    struct ifqueue lmc_txq;
    struct ifqueue lmc_rxq;
    lmc_dot3_stats_t lmc_dot3stats;
    lmc_ringinfo_t lmc_rxinfo;
    lmc_ringinfo_t lmc_txinfo;
    u_int8_t lmc_rombuf[128];
    lmc_media_t *lmc_media;
    lmc_ctl_t ictl;
    LMC_XINFO lmc_xinfo;

    u_int8_t lmc_pci_busno;		/* needed for multiport boards */
    u_int8_t lmc_pci_devno;		/* needed for multiport boards */
    tulip_desc_t *lmc_rxdescs;
    tulip_desc_t *lmc_txdescs;

    u_int32_t	lmc_crcSize;
    char	lmc_timing;		/* for HSSI and SSI */
    u_int16_t	t1_alarm1_status;
    u_int16_t	t1_alarm2_status;
 
    int		lmc_running;
    char	lmc_nodename[NG_NODELEN + 1];
    int		lmc_datahooks;
    node_p	lmc_node;
    hook_p	lmc_hook;
    hook_p	lmc_debug_hook;
    struct ifqueue lmc_xmitq_hipri;
    struct ifqueue lmc_xmitq;
    struct callout_handle lmc_handle;
    char        lmc_xmit_busy;
    int         lmc_out_dog;
    u_long	lmc_inbytes, lmc_outbytes;         /* stats */
    u_long	lmc_lastinbytes, lmc_lastoutbytes; /* a second ago */
    u_long	lmc_inrate, lmc_outrate;	   /* highest rate seen */
    u_long	lmc_inlast;			   /* last input N secs ago */
    u_long	lmc_out_deficit;		   /* output since last input */
    u_long	lmc_oerrors, lmc_ierrors;
    u_long	lmc_opackets, lmc_ipackets;
};


#define LMC_DOG_HOLDOFF     6       /* dog holds off for 6 secs */
#define LMC_QUITE_A_WHILE   300     /* 5 MINUTES */
#define LMC_LOTS_OF_PACKETS 100

/* Node type name and type cookie */
#define NG_LMC_NODE_TYPE	"lmc"
#define NG_LMC_COOKIE		956095698

/* Netgraph hooks */
#define NG_LMC_HOOK_DEBUG	"debug"
#define NG_LMC_HOOK_CONTROL	"control"
#define NG_LMC_HOOK_RAW		"rawdata"

/* Netgraph commands understood by this node type */
enum {
	NGM_LMC_SET_CTL = 1,
	NGM_LMC_GET_CTL,
};

/*
 * lmc_flags
 */
#define	LMC_IFUP		0x00000001
#define	LMC_00000002		0x00000002
#define	LMC_00000004		0x00000004
#define	LMC_00000008		0x00000008
#define	LMC_00000010		0x00000010
#define	LMC_MODEMOK		0x00000020
#define	LMC_00000040		0x00000040
#define	LMC_00000080		0x00000080
#define	LMC_RXACT		0x00000100
#define	LMC_INRESET		0x00000200
#define	LMC_NEEDRESET		0x00000400
#define	LMC_00000800		0x00000800
#define	LMC_00001000		0x00001000
#define	LMC_00002000		0x00002000
#define	LMC_WANTTXSTART		0x00004000
#define	LMC_NEWTXTHRESH		0x00008000
#define	LMC_NOAUTOSENSE		0x00010000
#define	LMC_PRINTLINKUP		0x00020000
#define	LMC_LINKUP		0x00040000
#define	LMC_RXBUFSLOW		0x00080000
#define	LMC_NOMESSAGES		0x00100000
#define	LMC_SYSTEMERROR		0x00200000
#define	LMC_TIMEOUTPENDING	0x00400000
#define	LMC_00800000		0x00800000
#define	LMC_01000000		0x01000000
#define	LMC_02000000		0x02000000
#define	LMC_RXIGNORE		0x04000000
#define	LMC_08000000		0x08000000
#define	LMC_10000000		0x10000000
#define	LMC_20000000		0x20000000
#define	LMC_40000000		0x40000000
#define	LMC_80000000		0x80000000

/*
 * lmc_features
 */
#define	LMC_HAVE_GPR		0x00000001	/* have gp register (140[A]) */
#define	LMC_HAVE_RXBADOVRFLW	0x00000002	/* RX corrupts on overflow */
#define	LMC_HAVE_POWERMGMT	0x00000004	/* Snooze/sleep modes */
#define	LMC_HAVE_MII		0x00000008	/* Some medium on MII */
#define	LMC_HAVE_SIANWAY	0x00000010	/* SIA does NWAY */
#define	LMC_HAVE_DUALSENSE	0x00000020	/* SIA senses both AUI & TP */
#define	LMC_HAVE_SIAGP		0x00000040	/* SIA has a GP port */
#define	LMC_HAVE_BROKEN_HASH	0x00000080	/* Broken Multicast Hash */
#define	LMC_HAVE_ISVSROM	0x00000100	/* uses ISV SROM Format */
#define	LMC_HAVE_BASEROM	0x00000200	/* Board ROM can be cloned */
#define	LMC_HAVE_SLAVEDROM	0x00000400	/* Board ROM cloned */
#define	LMC_HAVE_SLAVEDINTR	0x00000800	/* Board slaved interrupt */
#define	LMC_HAVE_SHAREDINTR	0x00001000	/* Board shares interrupts */
#define	LMC_HAVE_OKROM		0x00002000	/* ROM was recognized */
#define	LMC_HAVE_NOMEDIA	0x00004000	/* did not detect any media */
#define	LMC_HAVE_STOREFWD	0x00008000	/* have CMD_STOREFWD */
#define	LMC_HAVE_SIA100		0x00010000	/* has LS100 in SIA status */

static const char * const lmc_system_errors[] = {
    "parity error",
    "master abort",
    "target abort",
    "reserved #3",
    "reserved #4",
    "reserved #5",
    "reserved #6",
    "reserved #7",
};

static const char * const lmc_status_bits[] = {
    NULL,
    "transmit process stopped",
    NULL,
    "transmit jabber timeout",

    NULL,
    "transmit underflow",
    NULL,
    "receive underflow",

    "receive process stopped",
    "receive watchdog timeout",
    NULL,
    NULL,

    "link failure",
    NULL,
    NULL,
};

/*
 * This driver supports a maximum of 32 tulip boards.
 * This should be enough for the forseeable future.
 */
#define	LMC_MAX_DEVICES	32

typedef void ifnet_ret_t;
typedef int ioctl_cmd_t;
static lmc_softc_t *tulips[LMC_MAX_DEVICES];
#define LMC_IFP_TO_SOFTC(ifp) ((lmc_softc_t *)((ifp)->if_softc))
#define	lmc_intrfunc_t	void
#define	LMC_VOID_INTRFUNC
#define	IFF_NOTRAILERS		0
#define	CLBYTES			PAGE_SIZE
#define	LMC_EADDR_FMT		"%6D"
#define	LMC_EADDR_ARGS(addr)	addr, ":"
#define	LMC_UNIT_TO_SOFTC(unit)	(tulips[unit])
#define	LMC_BURSTSIZE(unit)		pci_max_burst_len
#define	loudprintf			if (bootverbose) printf

#ifndef LMC_PRINTF_FMT
#define	LMC_PRINTF_FMT		"%s%d"
#endif
#ifndef LMC_PRINTF_ARGS
#define	LMC_PRINTF_ARGS		sc->lmc_name, sc->lmc_unit
#endif

#ifndef LMC_BURSTSIZE
#define	LMC_BURSTSIZE(unit)		3
#endif

#if !defined(lmc_intrfunc_t)
#define	lmc_intrfunc_t	int
#endif

#if !defined(LMC_KVATOPHYS)
#define	LMC_KVATOPHYS(sc, va)	vtophys(va)
#endif

#ifndef LMC_RAISESPL
#define	LMC_RAISESPL()		splimp()
#endif
#ifndef LMC_RAISESOFTSPL
#define	LMC_RAISESOFTSPL()		splnet()
#endif
#ifndef TULUP_RESTORESPL
#define	LMC_RESTORESPL(s)		splx(s)
#endif

/*
 * While I think FreeBSD's 2.2 change to the bpf is a nice simplification,
 * it does add yet more conditional code to this driver.  Sigh.
 */
#if !defined(LMC_BPF_MTAP) && NBPFILTER > 0
#define	LMC_BPF_MTAP(sc, m)	bpf_mtap((sc)->lmc_bpf, m)
#define	LMC_BPF_TAP(sc, p, l)	bpf_tap((sc)->lmc_bpf, p, l)
#define	LMC_BPF_ATTACH(sc)	bpfattach(&(sc)->lmc_bpf, &(sc)->lmc_sppp.pp_if, DLT_PPP, PPP_HEADER_LEN)
#endif

/*
 * However, this change to FreeBSD I am much less enamored with.
 */
#if !defined(LMC_EADDR_FMT)
#define	LMC_EADDR_FMT		"%s"
#define	LMC_EADDR_ARGS(addr)	ether_sprintf(addr)
#endif

#define	LMC_CRC32_POLY	0xEDB88320UL	/* CRC-32 Poly -- Little Endian */
#define	LMC_MAX_TXSEG		30

#define	LMC_ADDREQUAL(a1, a2) \
	(((u_int16_t *)a1)[0] == ((u_int16_t *)a2)[0] \
	 && ((u_int16_t *)a1)[1] == ((u_int16_t *)a2)[1] \
	 && ((u_int16_t *)a1)[2] == ((u_int16_t *)a2)[2])
#define	LMC_ADDRBRDCST(a1) \
	(((u_int16_t *)a1)[0] == 0xFFFFU \
	 && ((u_int16_t *)a1)[1] == 0xFFFFU \
	 && ((u_int16_t *)a1)[2] == 0xFFFFU)

typedef int lmc_spl_t;

#endif /* !defined(_DEV_LMC_IF_LMCVAR_H) */
