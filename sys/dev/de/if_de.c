/*-
 * Copyright (c) 1994, 1995, 1996 Matt Thomas (matt@3am-software.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * $Id: if_de.c,v 1.49 1996/08/06 21:09:25 phk Exp $
 *
 */

/*
 * DEC DC21040 PCI Ethernet Controller
 *
 * Written by Matt Thomas
 * BPF support code stolen directly from if_ec.c
 *
 *   This driver supports the DEC DE435 or any other PCI
 *   board which support DC21040, DC21041, or DC21140 (mostly).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#if defined(__FreeBSD__)
#include <machine/clock.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#if defined(__FreeBSD__)
#include <vm/pmap.h>
#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/dc21040.h>
#endif
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#include <i386/pci/pci.h>
#include <i386/pci/ic/dc21040.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#if _BSDI_VERSION < 199510
#include <eisa.h>
#else
#define	NEISA 0
#endif
#if NEISA > 0 && _BSDI_VERSION >= 199401
#include <i386/eisa/eisa.h>
#define	TULIP_EISA
#endif
#endif /* __bsdi__ */

#if defined(__NetBSD__)
#include <machine/bus.h>
#if defined(__alpha__)
#include <machine/intr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#endif /* __NetBSD__ */

/*
 * Intel CPUs should use I/O mapped access.
 */
#if defined(__i386__) || defined(TULIP_EISA)
#define	TULIP_IOMAPPED
#endif

#if 0
/*
 * This turns on all sort of debugging stuff and make the
 * driver much larger.
 */
#define TULIP_DEBUG
#endif

/*
 * This module supports
 *	the DEC DC21040 PCI Ethernet Controller.
 *	the DEC DC21041 PCI Ethernet Controller.
 *	the DEC DC21140 PCI Fast Ethernet Controller.
 */

#ifdef TULIP_IOMAPPED
#define	TULIP_EISA_CSRSIZE	16
#define	TULIP_EISA_CSROFFSET	0
#define	TULIP_PCI_CSRSIZE	8
#define	TULIP_PCI_CSROFFSET	0

#if defined(__NetBSD__)
typedef bus_io_size_t tulip_csrptr_t;

#define TULIP_CSR_READ(sc, csr) \
    bus_io_read_4((sc)->tulip_bc, (sc)->tulip_ioh, (sc)->tulip_csrs.csr)
#define TULIP_CSR_WRITE(sc, csr, val) \
    bus_io_write_4((sc)->tulip_bc, (sc)->tulip_ioh, (sc)->tulip_csrs.csr, (val))

#define TULIP_CSR_READBYTE(sc, csr) \
    bus_io_read_1((sc)->tulip_bc, (sc)->tulip_ioh, (sc)->tulip_csrs.csr)
#define TULIP_CSR_WRITEBYTE(sc, csr, val) \
    bus_io_write_1((sc)->tulip_bc, (sc)->tulip_ioh, (sc)->tulip_csrs.csr, (val))
#else
typedef tulip_uint16_t tulip_csrptr_t;

#define	TULIP_CSR_READ(sc, csr)			(inl((sc)->tulip_csrs.csr))
#define	TULIP_CSR_WRITE(sc, csr, val)   	outl((sc)->tulip_csrs.csr, val)

#define	TULIP_CSR_READBYTE(sc, csr)		(inb((sc)->tulip_csrs.csr))
#define	TULIP_CSR_WRITEBYTE(sc, csr, val)	outb((sc)->tulip_csrs.csr, val)
#endif /* __NetBSD__ */

#else /* TULIP_IOMAPPED */

#define	TULIP_PCI_CSRSIZE	8
#define	TULIP_PCI_CSROFFSET	0

#if defined(__NetBSD__)
typedef bus_mem_size_t tulip_csrptr_t;

#define TULIP_CSR_READ(sc, csr) \
    bus_mem_read_4((sc)->tulip_bc, (sc)->tulip_memh, (sc)->tulip_csrs.csr)
#define TULIP_CSR_WRITE(sc, csr, val) \
    bus_mem_write_4((sc)->tulip_bc, (sc)->tulip_memh, (sc)->tulip_csrs.csr, \
      (val))
#else
typedef volatile tulip_uint32_t *tulip_csrptr_t;

/*
 * macros to read and write CSRs.  Note that the "0 +" in
 * READ_CSR is to prevent the macro from being an lvalue
 * and WRITE_CSR shouldn't be assigned from.
 */
#define	TULIP_CSR_READ(sc, csr)		(0 + *(sc)->tulip_csrs.csr)
#define	TULIP_CSR_WRITE(sc, csr, val)	((void)(*(sc)->tulip_csrs.csr = (val)))
#endif /* __NetBSD__ */

#endif /* TULIP_IOMAPPED */

/*
 * This structure contains "pointers" for the registers on
 * the various 21x4x chips.  CSR0 through CSR8 are common
 * to all chips.  After that, it gets messy...
 */
typedef struct {
    tulip_csrptr_t csr_busmode;			/* CSR0 */
    tulip_csrptr_t csr_txpoll;			/* CSR1 */
    tulip_csrptr_t csr_rxpoll;			/* CSR2 */
    tulip_csrptr_t csr_rxlist;			/* CSR3 */
    tulip_csrptr_t csr_txlist;			/* CSR4 */
    tulip_csrptr_t csr_status;			/* CSR5 */
    tulip_csrptr_t csr_command;			/* CSR6 */
    tulip_csrptr_t csr_intr;			/* CSR7 */
    tulip_csrptr_t csr_missed_frames;		/* CSR8 */

    /* DC21040 specific registers */

    tulip_csrptr_t csr_enetrom;			/* CSR9 */
    tulip_csrptr_t csr_reserved;		/* CSR10 */
    tulip_csrptr_t csr_full_duplex;		/* CSR11 */

    /* DC21040/DC21041 common registers */

    tulip_csrptr_t csr_sia_status;		/* CSR12 */
    tulip_csrptr_t csr_sia_connectivity;	/* CSR13 */
    tulip_csrptr_t csr_sia_tx_rx;		/* CSR14 */
    tulip_csrptr_t csr_sia_general;		/* CSR15 */

    /* DC21140/DC21041 common registers */

    tulip_csrptr_t csr_srom_mii;		/* CSR9 */
    tulip_csrptr_t csr_gp_timer;		/* CSR11 */

    /* DC21140 specific registers */

    tulip_csrptr_t csr_gp;			/* CSR12 */
    tulip_csrptr_t csr_watchdog;		/* CSR15 */

    /* DC21041 specific registers */

    tulip_csrptr_t csr_bootrom;			/* CSR10 */
} tulip_regfile_t;

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
} tulip_ringinfo_t;

/*
 * The DC21040 has a stupid restriction in that the receive
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
#if defined(__alpha__)
#define	TULIP_COPY_RXDATA	1
#endif

#define	TULIP_RXDESCS		48
#define	TULIP_TXDESCS		128
#define	TULIP_RXQ_TARGET	32
#if TULIP_RXQ_TARGET >= TULIP_RXDESCS
#error TULIP_RXQ_TARGET must be less than TULIP_RXDESCS
#endif
#define	TULIP_RX_BUFLEN		((MCLBYTES < 2048 ? MCLBYTES : 2048) - 16)

/*
 * Forward reference to make C happy.
 */
typedef struct _tulip_softc_t tulip_softc_t;

/*
 * Some boards need to treated specially.  The following enumeration
 * identifies the cards with quirks (or those we just want to single
 * out for special merit or scorn).
 */
typedef enum {
    TULIP_DC21040_GENERIC,		/* Generic DC21040 (works with most any board) */
    TULIP_DC21040_ZX314_MASTER,		/* ZNYX ZX314 Master 21040 (it has the interrupt line) */
    TULIP_DC21040_ZX314_SLAVE,		/* ZNYX ZX314 Slave 21040 (its interrupt is tied to the master's */
    TULIP_DC21140_DEC_EB,		/* Digital Semicondutor 21140 Evaluation Board */
    TULIP_DC21140_DEC_DE500,		/* Digital DE500-?? 10/100 */
    TULIP_DC21140_SMC_9332,		/* SMC 9332 */
    TULIP_DC21140_COGENT_EM100,		/* Cogent EM100 100 only */
    TULIP_DC21140_ZNYX_ZX34X,		/* ZNYX ZX342 10/100 */
    TULIP_DC21041_GENERIC,		/* Generic DC21041 card */
    TULIP_DC21041_DEC_DE450		/* Digital DE450 */
} tulip_board_t;

/*
 * This data structure is used to abstract out the quirks.
 * media_probe  = tries to determine the media type.
 * media_select = enables the current media (or autosenses)
 * media_preset = 21140, etal requires bit to set before the
 *		  the software reset; hence pre-set.  Should be
 *		  pre-reset but that's ugly.
 * mii_probe	= probe for PHY devices connected via the MII interface
 *		  on 21140, etal.
 */

typedef struct {
    tulip_board_t bd_type;
    const char *bd_description;
    int (*bd_media_probe)(tulip_softc_t *sc);
    void (*bd_media_select)(tulip_softc_t *sc);
    void (*bd_media_preset)(tulip_softc_t *sc);
    void (*bd_mii_probe)(tulip_softc_t *sc);
} tulip_boardsw_t;

/*
 * The next few declarations are for MII/PHY based board.
 *
 *    The first enumeration identifies a superset of various datums
 * that can be obtained from various PHY chips.  Not all PHYs will
 * support all datums.
 *    The modedata structure indicates what register contains
 * a datum, what mask is applied the register contents, and what the
 * result should be.
 *    The attr structure records information about a supported PHY.
 *    The phy structure records information about a PHY instance.
 */

typedef enum {
    PHY_MODE_10T,
    PHY_MODE_100TX,
    PHY_MODE_100T4,
    PHY_MODE_FULLDUPLEX,
    PHY_MODE_MAX
} phy_mode_t;

typedef struct {
    unsigned short pm_regno;
    unsigned short pm_mask;
    unsigned short pm_value;
} phy_modedata_t;

typedef struct {
    const char *attr_name;
    unsigned attr_id;
    unsigned short attr_flags;
#define	PHY_NEED_HARD_RESET	0x0001
#define	PHY_DUAL_CYCLE_TA	0x0002
    phy_modedata_t attr_modes[PHY_MODE_MAX];
} phy_attr_t;

typedef struct _tulip_phy_t {
    const struct _tulip_phy_t *phy_next;
    const phy_attr_t *phy_attr;
    unsigned phy_devaddr;
    unsigned phy_status;
} tulip_phy_t;

/*
 * The various controllers support.  Technically the DE425 is just
 * a 21040 on EISA.  But since it remarkable difference from normal
 * 21040s, we give it its own chip id.
 */

typedef enum {
    TULIP_DC21040, TULIP_DE425,
    TULIP_DC21041,
    TULIP_DC21140, TULIP_DC21140A, TULIP_DC21142,
    TULIP_CHIPID_UNKNOWN
} tulip_chipid_t;

/*
 * Various probe states used when trying to autosense the media.
 * While we could try to autosense on the 21040, it a pain and so
 * until someone complain we won't.  However, the 21041 and MII
 * 2114x do support autosense.
 */

typedef enum {
    TULIP_PROBE_INACTIVE, TULIP_PROBE_10BASET, TULIP_PROBE_AUI,
    TULIP_PROBE_BNC, TULIP_PROBE_PHYRESET, TULIP_PROBE_PHYAUTONEG,
    TULIP_PROBE_MEDIATEST, TULIP_PROBE_FAILED
} tulip_probe_state_t;

/*
 * Various physical media types supported.
 * BNCAUI is BNC or AUI since on the 21040 you can't really tell
 * which is in use.
 */
typedef enum {
    TULIP_MEDIA_UNKNOWN,
    TULIP_MEDIA_10BASET,
    TULIP_MEDIA_BNC,
    TULIP_MEDIA_AUI,
    TULIP_MEDIA_BNCAUI,
    TULIP_MEDIA_10BASET_FD,
    TULIP_MEDIA_100BASETX,
    TULIP_MEDIA_100BASETX_FD,
    TULIP_MEDIA_100BASET4
} tulip_media_t;

typedef struct {
    /*
     * Transmit Statistics
     */
    tulip_uint32_t dot3StatsSingleCollisionFrames;
    tulip_uint32_t dot3StatsMultipleCollisionFrames;
    tulip_uint32_t dot3StatsSQETestErrors;
    tulip_uint32_t dot3StatsDeferredTransmissions;
    tulip_uint32_t dot3StatsLateCollisions;
    tulip_uint32_t dot3StatsExcessiveCollisions;
    tulip_uint32_t dot3StatsInternalMacTransmitErrors;
    tulip_uint32_t dot3StatsCarrierSenseErrors;
    /*
     * Receive Statistics
     */
    tulip_uint32_t dot3StatsMissedFrames;	/* not in rfc1650! */
    tulip_uint32_t dot3StatsAlignmentErrors;
    tulip_uint32_t dot3StatsFCSErrors;
    tulip_uint32_t dot3StatsFrameTooLongs;
    tulip_uint32_t dot3StatsInternalMacReceiveErrors;
} tulip_dot3_stats_t;

/*
 * Now to important stuff.  This is softc structure (where does softc
 * come from??? No idea) for the tulip device.  
 *
 */
struct _tulip_softc_t {
#if defined(__bsdi__)
    struct device tulip_dev;		/* base device */
    struct isadev tulip_id;		/* ISA device */
    struct intrhand tulip_ih;		/* intrrupt vectoring */
    struct atshutdown tulip_ats;	/* shutdown hook */
#if _BSDI_VERSION < 199401
    caddr_t tulip_bpf;			/* for BPF */
#else
    prf_t tulip_pf;			/* printf function */
#endif
#endif
#if defined(__NetBSD__)
    struct device tulip_dev;		/* base device */
    void *tulip_ih;			/* intrrupt vectoring */
    void *tulip_ats;			/* shutdown hook */
    bus_chipset_tag_t tulip_bc;
    pci_chipset_tag_t tulip_pc;
#ifdef TULIP_IOMAPPED
    bus_io_handle_t tulip_ioh;		/* I/O region handle */
#else
    bus_io_handle_t tulip_memh;		/* memory region handle */
#endif
#endif
    struct arpcom tulip_ac;
    tulip_regfile_t tulip_csrs;
    unsigned tulip_flags;
#define	TULIP_WANTSETUP		0x00000001
#define	TULIP_WANTHASH		0x00000002
#define	TULIP_DOINGSETUP	0x00000004
#define	TULIP_ALTPHYS		0x00000008
#define	TULIP_PRINTMEDIA	0x00000010
#define	TULIP_TXPROBE_ACTIVE	0x00000020
#define	TULIP_TXPROBE_OK	0x00000040
#define	TULIP_WANTRXACT		0x00000080
#define	TULIP_RXACT		0x00000100
#define	TULIP_INRESET		0x00000200
#define	TULIP_NEEDRESET		0x00000400
#define	TULIP_SQETEST		0x00000800
#define	TULIP_ROMOK		0x00001000
#define	TULIP_SLAVEDROM		0x00002000
#define	TULIP_SLAVEDINTR	0x00004000
#define	TULIP_LINKSUSPECT	0x00008000
#define	TULIP_LINKUP		0x00010000
#define	TULIP_RXBUFSLOW		0x00020000
#define	TULIP_NOMESSAGES	0x00040000
#define	TULIP_SYSTEMERROR	0x00080000
#define	TULIP_DEVICEPROBE	0x00100000
#define	TULIP_FAKEGPTIMEOUT	0x00200000
    unsigned char tulip_rombuf[128];
    tulip_uint32_t tulip_setupbuf[192/sizeof(tulip_uint32_t)];
    tulip_uint32_t tulip_setupdata[192/sizeof(tulip_uint32_t)];
    tulip_uint32_t tulip_intrmask;
    tulip_uint32_t tulip_cmdmode;
    tulip_uint32_t tulip_revinfo;
    tulip_uint32_t tulip_gpticks;
    tulip_uint32_t tulip_gpunits;
    tulip_uint32_t tulip_last_system_error : 3;
    tulip_uint32_t tulip_txtimer : 2;
    tulip_uint32_t tulip_system_errors;
    tulip_uint32_t tulip_statusbits;
    tulip_uint32_t tulip_abilities;
    /* tulip_uint32_t tulip_bus; XXX */
    tulip_media_t tulip_media;
    tulip_probe_state_t tulip_probe_state;
    tulip_chipid_t tulip_chipid;
    const char *tulip_boardid;
    char tulip_boardidbuf[16];
    const tulip_boardsw_t *tulip_boardsw;
    tulip_softc_t *tulip_slaves;
    tulip_phy_t *tulip_phys;
#ifdef TULIP_DEBUG
    struct {
	tulip_uint32_t dbg_intrs;
	tulip_uint32_t dbg_msdelay;
	tulip_uint32_t dbg_gpticks;
	enum {
	    TULIP_GPTMR_10MB,
	    TULIP_GPTMR_10MB_MII,
	    TULIP_GPTMR_100MB_MII
	} dbg_gprate;
	tulip_uint32_t dbg_gpintrs;
	tulip_uint32_t dbg_gpintrs_hz;
	tulip_uint32_t dbg_link_downed;
	tulip_uint32_t dbg_link_suspected;
	u_int16_t dbg_phyregs[32][4];
	tulip_uint32_t dbg_rxlowbufs;
	tulip_uint32_t dbg_rxintrs;
	tulip_uint32_t dbg_last_rxintrs;
	tulip_uint32_t dbg_high_rxintrs_hz;
	tulip_uint32_t dbg_rxpktsperintr[TULIP_RXDESCS];
    } tulip_dbg;
#endif
    struct ifqueue tulip_txq;
    struct ifqueue tulip_rxq;
    tulip_dot3_stats_t tulip_dot3stats;
    tulip_ringinfo_t tulip_rxinfo;
    tulip_ringinfo_t tulip_txinfo;
    tulip_desc_t tulip_rxdescs[TULIP_RXDESCS];
    tulip_desc_t tulip_txdescs[TULIP_TXDESCS];
};

static const char * const tulip_chipdescs[] = { 
    "DC21040 [10Mb/s]",
#if defined(TULIP_EISA)
    "DE425 [10Mb/s]",
#else
    NULL,
#endif
    "DC21041 [10Mb/s]",
    "DC21140 [10-100Mb/s]",
    "DC21140A [10-100Mb/s]",
    "DC21142 [10-100Mb/s]",
};

static const char * const tulip_mediums[] = {
    "unknown",			/* TULIP_MEDIA_UNKNOWN */
    "10baseT",			/* TULIP_MEDIA_10BASET */
    "BNC",			/* TULIP_MEDIA_BNC */
    "AUI",			/* TULIP_MEDIA_AUI */
    "BNC/AUI",			/* TULIP_MEDIA_BNCAUI */
    "Full Duplex 10baseT",	/* TULIP_MEDIA_10BASET_FD */
    "100baseTX",		/* TULIP_MEDIA_100BASET */
    "Full Duplex 100baseTX",	/* TULIP_MEDIA_100BASET_FD */
    "100baseT4",		/* TULIP_MEDIA_100BASET4 */
};

static const tulip_media_t tulip_phy_statuses[] = {
    TULIP_MEDIA_10BASET, TULIP_MEDIA_10BASET_FD,
    TULIP_MEDIA_100BASETX, TULIP_MEDIA_100BASETX_FD,
    TULIP_MEDIA_100BASET4
};

static const char * const tulip_system_errors[] = {
    "parity error",
    "master abort",
    "target abort",
    "reserved #3",
    "reserved #4",
    "reserved #5",
    "reserved #6",
    "reserved #7",
};

static const char * const tulip_status_bits[] = {
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

#ifndef IFF_ALTPHYS
#define	IFF_ALTPHYS	IFF_LINK2		/* In case it isn't defined */
#endif

#ifndef IFF_FULLDUPLEX
#define	IFF_FULLDUPLEX	IFF_LINK1
#endif

#ifndef	IFF_NOAUTONEG
#if IFF_ALTPHYS == IFF_LINK2
#define	IFF_NOAUTONEG	IFF_LINK0
#else 
#define	IFF_NOAUTONEG	IFF_LINK2
#endif
#endif

#if (IFF_ALTPHYS&IFF_FULLDUPLEX&IFF_NOAUTONEG) != 0
#error IFF_ALTPHYS, IFF_FULLDUPLEX, IFF_NOAUTONEG overlap
#endif


#if defined(__FreeBSD__)
typedef void ifnet_ret_t;
typedef int ioctl_cmd_t;
#define	TULIP_COUNTINCR		4
tulip_softc_t **tulips;
int tulip_count;
#if BSD >= 199506
#define TULIP_IFP_TO_SOFTC(ifp) ((tulip_softc_t *)((ifp)->if_softc))
#if NBPFILTER > 0
#define	TULIP_BPF_MTAP(sc, m)	bpf_mtap(&(sc)->tulip_if, m)
#define	TULIP_BPF_TAP(sc, p, l)	bpf_tap(&(sc)->tulip_if, p, l)
#define	TULIP_BPF_ATTACH(sc)	bpfattach(&(sc)->tulip_if, DLT_EN10MB, sizeof(struct ether_header))
#endif
#define	tulip_intrfunc_t	void
#define	TULIP_VOID_INTRFUNC
#define	IFF_NOTRAILERS		0
#define	CLBYTES			PAGE_SIZE
#if 0
#define	TULIP_KVATOPHYS(sc, va)	kvtop(va)
#endif
#define	TULIP_EADDR_FMT		"%6D"
#define	TULIP_EADDR_ARGS(addr)	addr, ":"
#else
extern int bootverbose;
#define TULIP_IFP_TO_SOFTC(ifp)         (TULIP_UNIT_TO_SOFTC((ifp)->if_unit))
#endif
#define	TULIP_UNIT_TO_SOFTC(unit)	(tulips[unit])
#define	TULIP_BURSTSIZE(unit)		pci_max_burst_len
#define	loudprintf			if (bootverbose) printf
#endif

#if defined(__bsdi__)
typedef int ifnet_ret_t;
typedef int ioctl_cmd_t;
extern struct cfdriver decd;
#define	TULIP_UNIT_TO_SOFTC(unit)	((tulip_softc_t *) decd.cd_devs[unit])
#define TULIP_IFP_TO_SOFTC(ifp)		(TULIP_UNIT_TO_SOFTC((ifp)->if_unit))
#if _BSDI_VERSION >= 199510
#if 0
#define	TULIP_BURSTSIZE(unit)		log2_burst_size
#endif
#define	loudprintf			aprint_verbose
#define	printf				(*sc->tulip_pf)
#elif _BSDI_VERSION <= 199401
#define	DRQNONE				0
#define	loudprintf			printf
static void
arp_ifinit(
    struct arpcom *ac,
    struct ifaddr *ifa)
{
    ac->ac_ipaddr = IA_SIN(ifa)->sin_addr;
    arpwhohas(ac, &ac->ac_ipaddr);
}
#endif
#endif	/* __bsdi__ */

#if defined(__NetBSD__)
typedef void ifnet_ret_t;
typedef u_long ioctl_cmd_t;
extern struct cfattach de_ca;
extern struct cfdriver de_cd;
#define	TULIP_UNIT_TO_SOFTC(unit)	((tulip_softc_t *) de_cd.cd_devs[unit])
#define TULIP_IFP_TO_SOFTC(ifp)         ((tulip_softc_t *)((ifp)->if_softc))
#define	tulip_xname			tulip_ac.ac_if.if_xname
#define	tulip_unit			tulip_dev.dv_unit
#define	loudprintf			printf
#define	TULIP_PRINTF_FMT		"%s"
#define	TULIP_PRINTF_ARGS		sc->tulip_xname
#if defined(__alpha__)
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#define TULIP_KVATOPHYS(va)		(vtophys(va) | 0x40000000)
#endif
#endif	/* __NetBSD__ */

#ifndef TULIP_PRINTF_FMT
#define	TULIP_PRINTF_FMT		"%s%d"
#endif
#ifndef TULIP_PRINTF_ARGS
#define	TULIP_PRINTF_ARGS		sc->tulip_name, sc->tulip_unit
#endif

#ifndef TULIP_BURSTSIZE
#define	TULIP_BURSTSIZE(unit)		3
#endif

#define	tulip_if	tulip_ac.ac_if
#ifndef tulip_unit
#define	tulip_unit	tulip_ac.ac_if.if_unit
#endif
#define	tulip_name	tulip_ac.ac_if.if_name
#define	tulip_hwaddr	tulip_ac.ac_enaddr

#if !defined(tulip_bpf) && (!defined(__bsdi__) || _BSDI_VERSION >= 199401)
#define	tulip_bpf	tulip_ac.ac_if.if_bpf
#endif

#if !defined(tulip_intrfunc_t)
#define	tulip_intrfunc_t	int
#endif

#if !defined(TULIP_KVATOPHYS)
#define	TULIP_KVATOPHYS(sc, va)	vtophys(va)
#endif

/*
 * While I think FreeBSD's 2.2 change to the bpf is a nice simplification,
 * it does add yet more conditional code to this driver.  Sigh.
 */
#if !defined(TULIP_BPF_MTAP) && NBPFILTER > 0
#define	TULIP_BPF_MTAP(sc, m)	bpf_mtap((sc)->tulip_bpf, m)
#define	TULIP_BPF_TAP(sc, p, l)	bpf_tap((sc)->tulip_bpf, p, l)
#define	TULIP_BPF_ATTACH(sc)	bpfattach(&(sc)->tulip_bpf, &(sc)->tulip_if, DLT_EN10MB, sizeof(struct ether_header))
#endif

/*
 * However, this change to FreeBSD I am much less enamored with.
 */
#if !defined(TULIP_EADDR_FMT)
#define	TULIP_EADDR_FMT		"%s"
#define	TULIP_EADDR_ARGS(addr)	ether_sprintf(addr)
#endif

#define	TULIP_CRC32_POLY	0xEDB88320UL	/* CRC-32 Poly -- Little Endian */
#define	TULIP_MAX_TXSEG		30

#define	TULIP_ADDREQUAL(a1, a2) \
	(((u_int16_t *)a1)[0] == ((u_int16_t *)a2)[0] \
	 && ((u_int16_t *)a1)[1] == ((u_int16_t *)a2)[1] \
	 && ((u_int16_t *)a1)[2] == ((u_int16_t *)a2)[2])
#define	TULIP_ADDRBRDCST(a1) \
	(((u_int16_t *)a1)[0] == 0xFFFFU \
	 && ((u_int16_t *)a1)[1] == 0xFFFFU \
	 && ((u_int16_t *)a1)[2] == 0xFFFFU)

static tulip_intrfunc_t tulip_intr(void *arg);
static void tulip_reset(tulip_softc_t * const sc);
static ifnet_ret_t tulip_ifstart(struct ifnet *ifp);
static void tulip_rx_intr(tulip_softc_t * const sc);
static void tulip_addr_filter(tulip_softc_t * const sc);
static unsigned tulip_mii_readreg(tulip_softc_t * const sc, unsigned devaddr, unsigned regno);
static void tulip_mii_writereg(tulip_softc_t * const sc, unsigned devaddr, unsigned regno, unsigned data);

static int
tulip_dc21040_media_probe(
    tulip_softc_t * const sc)
{
    int cnt;

    TULIP_CSR_WRITE(sc, csr_sia_connectivity, 0);
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    for (cnt = 0; cnt < 2400; cnt++) {
	if ((TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) == 0)
	    break;
	DELAY(1000);
    }
    sc->tulip_if.if_baudrate = 10000000;
    return (TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) != 0;
}

static void
tulip_dc21040_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
    sc->tulip_flags |= TULIP_SQETEST|TULIP_LINKUP;
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_AUI);
	sc->tulip_media = TULIP_MEDIA_BNCAUI;
	sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
    } else {
	if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	    sc->tulip_media = TULIP_MEDIA_10BASET_FD;
	    sc->tulip_flags &= ~TULIP_SQETEST;
	} else {
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	    sc->tulip_media = TULIP_MEDIA_10BASET;
	}
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    sc->tulip_flags ^= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    }
}

static int
tulip_dc21040_10baset_only_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, 0);
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    sc->tulip_if.if_baudrate = 10000000;
    return 0;
}

static void
tulip_dc21040_10baset_only_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_flags |= TULIP_LINKUP;
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	sc->tulip_media = TULIP_MEDIA_10BASET_FD;
	sc->tulip_flags &= ~TULIP_SQETEST;
    } else {
	sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	sc->tulip_media = TULIP_MEDIA_10BASET;
	sc->tulip_flags |= TULIP_SQETEST;
    }
    if (sc->tulip_flags & TULIP_ALTPHYS)
	sc->tulip_flags ^= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
    sc->tulip_flags &= ~TULIP_ALTPHYS;
}

static int
tulip_dc21040_auibnc_only_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, 0);
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_AUI);
    sc->tulip_if.if_baudrate = 10000000;
    sc->tulip_flags |= TULIP_SQETEST|TULIP_LINKUP;
    return 0;
}

static void
tulip_dc21040_auibnc_only_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_AUI);
    if (sc->tulip_if.if_flags & IFF_FULLDUPLEX)
	sc->tulip_if.if_flags &= ~IFF_FULLDUPLEX;
    sc->tulip_media = TULIP_MEDIA_BNCAUI;
    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
    if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
    sc->tulip_flags &= ~TULIP_ALTPHYS;
}

static const tulip_boardsw_t tulip_dc21040_boardsw = {
    TULIP_DC21040_GENERIC,
    "",
    tulip_dc21040_media_probe,
    tulip_dc21040_media_select,
    NULL,
    NULL
};

static const tulip_boardsw_t tulip_dc21040_10baset_only_boardsw = {
    TULIP_DC21040_GENERIC,
    "",
    tulip_dc21040_10baset_only_media_probe,
    tulip_dc21040_10baset_only_media_select,
    NULL,
    NULL
};

static const tulip_boardsw_t tulip_dc21040_auibnc_only_boardsw = {
    TULIP_DC21040_GENERIC,
    "",
    tulip_dc21040_auibnc_only_media_probe,
    tulip_dc21040_auibnc_only_media_select,
    NULL,
    NULL
};

static const tulip_boardsw_t tulip_dc21040_zx314_master_boardsw = {
    TULIP_DC21040_ZX314_MASTER,
    "ZNYX ZX314 ",
    tulip_dc21040_10baset_only_media_probe,
    tulip_dc21040_10baset_only_media_select
};

static const tulip_boardsw_t tulip_dc21040_zx314_slave_boardsw = {
    TULIP_DC21040_ZX314_SLAVE,
    "ZNYX ZX314 ",
    tulip_dc21040_10baset_only_media_probe,
    tulip_dc21040_10baset_only_media_select
};

static const phy_attr_t tulip_phy_attrlist[] = {
    { "NS DP83840", 0x20005c00, 0,	/* 08-00-17 */
      {
	{ 0x19, 0x40, 0x40 },	/* 10TX */
	{ 0x19, 0x40, 0x00 },	/* 100TX */
      }
    },
    { "Seeq 80C240", 0x0281F400, 0,	/* 00-A0-7D */
      {
	{ 0x12, 0x10, 0x00 },	/* 10T */
	{ },			/* 100TX */
	{ 0x12, 0x10, 0x10 },	/* 100T4 */
	{ 0x12, 0x08, 0x08 },	/* FULL_DUPLEX */
      }
    },
    { NULL }
};

static void
tulip_dc21140_mii_probe(
    tulip_softc_t * const sc)
{
    unsigned devaddr;

    for (devaddr = 31; devaddr > 0; devaddr--) {
	unsigned status = tulip_mii_readreg(sc, devaddr, PHYREG_STATUS);
	unsigned media;
	unsigned id;
	const phy_attr_t *attr;
	tulip_phy_t *phy;
	const char *sep;
	if (status == 0 || status == 0xFFFF || status < PHYSTS_10BASET)
	    continue;
	if ((status & PHYSTS_EXTENDED_REGS) == 0) {
	    loudprintf(TULIP_PRINTF_FMT "(phy%d): skipping (no extended register set)\n",
		   TULIP_PRINTF_ARGS, devaddr);
	    continue;
	}
	id = (tulip_mii_readreg(sc, devaddr, PHYREG_IDLOW) << 16) |
	    tulip_mii_readreg(sc, devaddr, PHYREG_IDHIGH);
	for (attr = tulip_phy_attrlist; attr->attr_name != NULL; attr++) {
	    if ((id & ~0x0F) == attr->attr_id)
		break;
	}
	if (attr->attr_name == NULL) {
	    loudprintf(TULIP_PRINTF_FMT "(phy%d): skipping (unrecogized id 0x%08x)\n",
		   TULIP_PRINTF_ARGS, devaddr, id & ~0x0F);
	    continue;
	}

	MALLOC(phy, tulip_phy_t *, sizeof(tulip_phy_t), M_DEVBUF, M_NOWAIT);
	if (phy == NULL) {
	    loudprintf(TULIP_PRINTF_FMT "(phy%d): skipping (memory allocation failed)\n",
		   TULIP_PRINTF_ARGS, devaddr);
	    continue;
	}
	phy->phy_attr = attr;
	phy->phy_devaddr = devaddr;
	phy->phy_status = status;
	phy->phy_next = sc->tulip_phys;
	sc->tulip_phys = phy;

	loudprintf(TULIP_PRINTF_FMT "(phy%d): model = %s%s\n",
	       TULIP_PRINTF_ARGS,
	       phy->phy_devaddr, phy->phy_attr->attr_name,
	       (phy->phy_status & PHYSTS_CAN_AUTONEG)
	           ? " (supports media autonegotiation)"
	           : "");
	loudprintf(TULIP_PRINTF_FMT "(phy%d): media = ",
	       TULIP_PRINTF_ARGS, phy->phy_devaddr);
	for (media = 11, sep = ""; media < 16; media++) {
	    if (status & (1 << media)) {
		loudprintf("%s%s", sep, tulip_mediums[tulip_phy_statuses[media-11]]);
		sep = ", ";
	    }
	}
	loudprintf("\n");
    }
}

/*
 * The general purpose timer of the 21140/21140a/21142 is kind
 * of strange.  It can run on one of 3 speeds depending on the mode
 * of the chip.
 *
 *	10Mb/s port	204.8  microseconds (also speed of DC21041 timer)
 *	100Mb/s MII	 81.92 microseconds
 *	10Mb/s MII	819.2  microseconds
 *
 * So we use a tick of a 819.2 microseconds and bias the number of ticks
 * required based on the mode in which we are running.  2560/3125 = .8192
 * so we use the reciprocal to scale the ms delay to 21140 ticks.
 */
static void
tulip_dc21140_gp_timer_set(
    tulip_softc_t * const sc,
    unsigned msdelay)
{
    tulip_uint32_t cmdmode = TULIP_CSR_READ(sc, csr_command);
#ifdef TULIP_DEBUG
    sc->tulip_dbg.dbg_msdelay = msdelay;
#endif
    if ((cmdmode & TULIP_CMD_PORTSELECT) == 0) {
	msdelay *= 4;
#ifdef TULIP_DEBUG
	sc->tulip_dbg.dbg_gprate = TULIP_GPTMR_10MB_MII;
#endif
    } else if ((cmdmode & TULIP_CMD_TXTHRSHLDCTL) == 0) {
	msdelay *= 10;
#ifdef TULIP_DEBUG
	sc->tulip_dbg.dbg_gprate = TULIP_GPTMR_100MB_MII;
    } else {
	sc->tulip_dbg.dbg_gprate = TULIP_GPTMR_10MB;
#endif
    }
#if 0
    if (sc->tulip_chipid == TULIP_DC21140A)
	msdelay *= 10;
#endif
    TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_GPTIMEOUT);
    TULIP_CSR_WRITE(sc, csr_gp_timer, (msdelay * 313 + 128) / 256);
    if (sc->tulip_flags & TULIP_DEVICEPROBE) {
	sc->tulip_flags |= TULIP_FAKEGPTIMEOUT;
    } else {
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	sc->tulip_flags &= ~TULIP_FAKEGPTIMEOUT;
    }
#ifdef TULIP_DEBUG
    sc->tulip_dbg.dbg_gpticks = (msdelay * 313 + 128) / 256;
#endif
}

static int
tulip_dc21140_map_abilities(
    tulip_softc_t * const sc,
    const tulip_phy_t * const phy,
    unsigned abilities)
{
    sc->tulip_abilities = abilities;
    if (abilities & PHYSTS_100BASETX_FD) {
	sc->tulip_media = TULIP_MEDIA_100BASETX_FD;
    } else if (abilities & PHYSTS_100BASETX) {
	sc->tulip_media = TULIP_MEDIA_100BASETX;
    } else if (abilities & PHYSTS_100BASET4) {
	sc->tulip_media = TULIP_MEDIA_100BASET4;
    } else if (abilities & PHYSTS_10BASET_FD) {
	sc->tulip_media = TULIP_MEDIA_10BASET_FD;
    } else if (abilities & PHYSTS_10BASET) {
	sc->tulip_media = TULIP_MEDIA_10BASET;
    } else {
	sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	return 1;
    }
    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
    sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
    sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_NEEDRESET;
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    return 0;
}

static void
tulip_dc21140_autonegotiate(
    tulip_softc_t * const sc,
    const tulip_phy_t * const phy)
{
    tulip_uint32_t data;

    if (sc->tulip_flags & TULIP_INRESET) {
	sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    }
    if (sc->tulip_if.if_flags & IFF_NOAUTONEG) {
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_CONTROL);
	if (data & PHYCTL_AUTONEG_ENABLE) {
	    data &= ~PHYCTL_AUTONEG_ENABLE;
	    tulip_mii_writereg(sc, phy->phy_devaddr, PHYREG_CONTROL, data);
	}
	return;
    }

  again:
    switch (sc->tulip_probe_state) {
        case TULIP_PROBE_INACTIVE: {
	    sc->tulip_flags |= TULIP_TXPROBE_ACTIVE;
	    tulip_mii_writereg(sc, phy->phy_devaddr, PHYREG_CONTROL, PHYCTL_RESET);
	    sc->tulip_gpticks = 10;
	    sc->tulip_intrmask |= TULIP_STS_ABNRMLINTR|TULIP_STS_GPTIMEOUT|TULIP_STS_NORMALINTR;
	    sc->tulip_probe_state = TULIP_PROBE_PHYRESET;
	    goto again;
	}
        case TULIP_PROBE_PHYRESET: {
	    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_CONTROL);
	    if (data & PHYCTL_RESET) {
		if (--sc->tulip_gpticks > 0) {
		    tulip_dc21140_gp_timer_set(sc, 100);
		    return;
		}
		printf(TULIP_PRINTF_FMT "(phy%d): error: reset of PHY never completed!\n",
			   TULIP_PRINTF_ARGS, phy->phy_devaddr);
		sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
		sc->tulip_probe_state = TULIP_PROBE_FAILED;
		sc->tulip_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
		sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
		return;
	    }
	    if ((phy->phy_status & PHYSTS_CAN_AUTONEG) == 0
		    && (sc->tulip_if.if_flags & IFF_NOAUTONEG)) {
#ifdef TULIP_DEBUG
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation disabled\n",
			   TULIP_PRINTF_ARGS, phy->phy_devaddr);
#endif
		sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
		return;
	    }
	    if (tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_AUTONEG_ADVERTISEMENT) != ((phy->phy_status >> 6) | 0x01))
		tulip_mii_writereg(sc, phy->phy_devaddr, PHYREG_AUTONEG_ADVERTISEMENT, (phy->phy_status >> 6) | 0x01);
	    tulip_mii_writereg(sc, phy->phy_devaddr, PHYREG_CONTROL, data|PHYCTL_AUTONEG_RESTART|PHYCTL_AUTONEG_ENABLE);
	    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_CONTROL);
#ifdef TULIP_DEBUG
	    if ((data & PHYCTL_AUTONEG_ENABLE) == 0)
		loudprintf(TULIP_PRINTF_FMT "(phy%d): oops: enable autonegotiation failed: 0x%04x\n",
			   TULIP_PRINTF_ARGS, phy->phy_devaddr, data);
	    else
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation restarted: 0x%04x\n",
			   TULIP_PRINTF_ARGS, phy->phy_devaddr, data);
#endif
	    sc->tulip_probe_state = TULIP_PROBE_PHYAUTONEG;
	    sc->tulip_gpticks = 60;
	    goto again;
	}
        case TULIP_PROBE_PHYAUTONEG: {
	    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_STATUS);
	    if ((data & PHYSTS_AUTONEG_DONE) == 0) {
		if (--sc->tulip_gpticks > 0) {
		    tulip_dc21140_gp_timer_set(sc, 100);
		    return;
		}
#ifdef TULIP_DEBUG
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation timeout: sts=0x%04x, ctl=0x%04x\n",
			   TULIP_PRINTF_ARGS, phy->phy_devaddr, data,
			   tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_CONTROL));
#endif
		sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
		return;
	    }
	    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_AUTONEG_ABILITIES);
#ifdef TULIP_DEBUG
	    loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation complete: 0x%04x\n",
		       TULIP_PRINTF_ARGS, phy->phy_devaddr, data);
#endif
	    data = (data << 6) & phy->phy_status;
	    tulip_dc21140_map_abilities(sc, phy, data);
	    return;
	}
    }
#ifdef TULIP_DEBUG
    loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation failure: state = %d\n",
	       TULIP_PRINTF_ARGS, phy->phy_devaddr, sc->tulip_probe_state);
#endif
}

static tulip_media_t
tulip_dc21140_phy_readspecific(
    tulip_softc_t * const sc,
    const tulip_phy_t * const phy)
{
    const phy_attr_t * const attr = phy->phy_attr;
    unsigned data;
    unsigned idx = 0;
    static const tulip_media_t table[] = {
	TULIP_MEDIA_UNKNOWN,
	TULIP_MEDIA_10BASET,
	TULIP_MEDIA_100BASETX,
	TULIP_MEDIA_100BASET4,
	TULIP_MEDIA_UNKNOWN,
	TULIP_MEDIA_10BASET_FD,
	TULIP_MEDIA_100BASETX_FD,
	TULIP_MEDIA_UNKNOWN
    };

    /*
     * Don't read phy specific registers if link is not up.
     */
    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_STATUS);
    if ((data & PHYSTS_LINK_UP) == 0)
	return TULIP_MEDIA_UNKNOWN;

    if (attr->attr_modes[PHY_MODE_100TX].pm_regno) {
	const phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_100TX];
	data = tulip_mii_readreg(sc, phy->phy_devaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 2;
    }
    if (idx == 0 && attr->attr_modes[PHY_MODE_100T4].pm_regno) {
	const phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_100T4];
	data = tulip_mii_readreg(sc, phy->phy_devaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 3;
    }
    if (idx == 0 && attr->attr_modes[PHY_MODE_10T].pm_regno) {
	const phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_10T];
	data = tulip_mii_readreg(sc, phy->phy_devaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 1;
    } 
    if (idx != 0 && attr->attr_modes[PHY_MODE_FULLDUPLEX].pm_regno) {
	const phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_FULLDUPLEX];
	data = tulip_mii_readreg(sc, phy->phy_devaddr, pm->pm_regno);
	idx += ((data & pm->pm_mask) == pm->pm_value ? 4 : 0);
    }
    return table[idx];
}

static void
tulip_dc21140_mii_link_monitor(
    tulip_softc_t * const sc,
    const tulip_phy_t * const phy)
{
    tulip_uint32_t data;

    tulip_dc21140_gp_timer_set(sc, 425);
    /*
     * Have we seen some packets?  If so, the link must be good.
     */
    if ((sc->tulip_flags & (TULIP_RXACT|TULIP_LINKSUSPECT|TULIP_LINKUP)) == (TULIP_RXACT|TULIP_LINKUP)) {
	sc->tulip_flags &= ~TULIP_RXACT;
	return;
    }

    /*
     * Read the PHY status register.
     */
    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_STATUS);
    if ((sc->tulip_if.if_flags & IFF_NOAUTONEG) == 0 && (data & PHYSTS_AUTONEG_DONE)) {
	/*
	 * If autonegotiation hasn't been disabled and the PHY has complete 
	 * autonegotiation, see the if the remote systems abilities have changed.
	 * If so, upgrade or downgrade as appropriate.
	 */
	unsigned abilities = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_AUTONEG_ABILITIES);
	abilities = (abilities << 6) & phy->phy_status;
	if (abilities != sc->tulip_abilities) {
	    sc->tulip_flags |= TULIP_PRINTMEDIA;
#ifdef TULIP_DEBUG
	    loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation changed: 0x%04x -> 0x%04x\n",
		       TULIP_PRINTF_ARGS, phy->phy_devaddr,
		       sc->tulip_abilities, abilities);
#endif
	    tulip_dc21140_map_abilities(sc, phy, abilities);
	    return;
	}
    }
    /*
     * The link is now up.  If was down, say its back up.
     */
    if ((data & (PHYSTS_LINK_UP|PHYSTS_REMOTE_FAULT)) == PHYSTS_LINK_UP) {
	if ((sc->tulip_if.if_flags & IFF_NOAUTONEG) == 0) {
	    tulip_media_t media = tulip_dc21140_phy_readspecific(sc, phy);
	    if (media != sc->tulip_media && media != TULIP_MEDIA_UNKNOWN) {
		sc->tulip_media = media;
		sc->tulip_flags |= TULIP_PRINTMEDIA;
	    }
	}
	sc->tulip_gpticks = 0;
	if (sc->tulip_flags & TULIP_PRINTMEDIA) {
	    printf(TULIP_PRINTF_FMT ": %senabling %s port\n",
		   TULIP_PRINTF_ARGS,
		   (sc->tulip_flags & TULIP_LINKUP) ? "" : "link up: ",
		   tulip_mediums[sc->tulip_media]);
	} else if ((sc->tulip_flags & TULIP_LINKUP) == 0) {
	    printf(TULIP_PRINTF_FMT ": link up\n", TULIP_PRINTF_ARGS);
	}
	sc->tulip_flags &= ~(TULIP_PRINTMEDIA|TULIP_LINKSUSPECT|TULIP_RXACT);
	sc->tulip_flags |= TULIP_LINKUP;
	return;
    }
    /*
     * The link may be down.  Mark it as suspect.  If suspect for 12 ticks,
     * mark it down.  If autonegotiation is not disabled, restart the media
     * probe to see if the media has changed.
     */
    if ((sc->tulip_flags & TULIP_LINKSUSPECT) == 0) {
	sc->tulip_flags |= TULIP_LINKSUSPECT;
	sc->tulip_flags &= ~TULIP_LINKUP;
	sc->tulip_gpticks = 12;
#ifdef TULIP_DEBUG
	sc->tulip_dbg.dbg_link_suspected++;
#endif
	return;
    }
    if (--sc->tulip_gpticks > 0)
	return;
    if (sc->tulip_flags & TULIP_LINKSUSPECT) {
	printf(TULIP_PRINTF_FMT ": link down: cable problem?\n", TULIP_PRINTF_ARGS);
	sc->tulip_flags &= ~TULIP_LINKSUSPECT;
#ifdef TULIP_DEBUG
	sc->tulip_dbg.dbg_link_downed++;
#endif
    }
    if (sc->tulip_if.if_flags & IFF_NOAUTONEG)
	return;
    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    tulip_dc21140_autonegotiate(sc, phy);
}

static void
tulip_dc21140_nomii_media_preset(
    tulip_softc_t * const sc)
{
    sc->tulip_flags &= ~TULIP_SQETEST;
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	    |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
	sc->tulip_if.if_baudrate = 100000000;
    } else {
	sc->tulip_cmdmode &= ~(TULIP_CMD_PORTSELECT
			       |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER);
	sc->tulip_if.if_baudrate = 10000000;
	if ((sc->tulip_cmdmode & TULIP_CMD_FULLDUPLEX) == 0)
	    sc->tulip_flags |= TULIP_SQETEST;
    }
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
}

static void
tulip_dc21140_mii_media_preset(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT;
    sc->tulip_flags &= ~TULIP_SQETEST;
    if (sc->tulip_media != TULIP_MEDIA_UNKNOWN) {
	switch (sc->tulip_media) {
	    case TULIP_MEDIA_10BASET: {
		sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
		sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
		sc->tulip_if.if_baudrate = 10000000;
		sc->tulip_flags |= TULIP_SQETEST;
		break;
	    }
	    case TULIP_MEDIA_10BASET_FD: {
		sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX|TULIP_CMD_TXTHRSHLDCTL;
		sc->tulip_if.if_baudrate = 10000000;
		break;
	    }
	    case TULIP_MEDIA_100BASET4:
	    case TULIP_MEDIA_100BASETX: {
		sc->tulip_cmdmode &= ~(TULIP_CMD_FULLDUPLEX|TULIP_CMD_TXTHRSHLDCTL);
		sc->tulip_if.if_baudrate = 100000000;
		break;
	    }
	    case TULIP_MEDIA_100BASETX_FD: {
		sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
		sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
		sc->tulip_if.if_baudrate = 100000000;
		break;
	    }
	}
    }
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
}

static void
tulip_dc21140_nomii_100only_media_preset(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	|TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
}


static int
tulip_dc21140_evalboard_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);
    return (TULIP_CSR_READ(sc, csr_gp) & TULIP_GP_EB_OK100) != 0;
}

static void
tulip_dc21140_evalboard_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_flags |= TULIP_LINKUP;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_INIT);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_media = TULIP_MEDIA_100BASETX;
	sc->tulip_flags &= ~TULIP_SQETEST;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    sc->tulip_flags ^= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_media = TULIP_MEDIA_10BASET;
	sc->tulip_flags |= TULIP_SQETEST;
    }
#ifdef BIG_PACKET
    if (sc->tulip_if.if_mtu > ETHERMTU) {
	TULIP_CSR_WRITE(sc, csr_watchdog, TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE);
    }
#endif
}

static const tulip_boardsw_t tulip_dc21140_eb_boardsw = {
    TULIP_DC21140_DEC_EB,
    "",
    tulip_dc21140_evalboard_media_probe,
    tulip_dc21140_evalboard_media_select,
    tulip_dc21140_nomii_media_preset,
};

static int
tulip_dc21140_smc9332_media_probe(
    tulip_softc_t * const sc)
{
    int idx;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(200000);
    for (idx = 1000; idx > 0; idx--) {
	tulip_uint32_t csr = TULIP_CSR_READ(sc, csr_gp);
	if ((csr & (TULIP_GP_SMC_9332_OK100|TULIP_GP_SMC_9332_OK10)) == TULIP_GP_SMC_9332_OK100)
	    return 1;
	if ((csr & (TULIP_GP_SMC_9332_OK100|TULIP_GP_SMC_9332_OK10)) == TULIP_GP_SMC_9332_OK10)
	    return 0;
	DELAY(1000);
    }
    return 0;
}
 
static void
tulip_dc21140_smc9332_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_flags |= TULIP_LINKUP;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_INIT);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_media = TULIP_MEDIA_100BASETX;
	sc->tulip_flags &= ~TULIP_SQETEST;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    sc->tulip_flags ^= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_media = TULIP_MEDIA_10BASET;
	sc->tulip_flags |= TULIP_SQETEST;
    }
#ifdef BIG_PACKET
    if (sc->tulip_if.if_mtu > ETHERMTU) {
	TULIP_CSR_WRITE(sc, csr_watchdog, TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE);
    }
#endif
}

static const tulip_boardsw_t tulip_dc21140_smc9332_boardsw = {
    TULIP_DC21140_SMC_9332,
    "SMC 9332 ",
    tulip_dc21140_smc9332_media_probe,
    tulip_dc21140_smc9332_media_select,
    tulip_dc21140_nomii_media_preset,
};

static int
tulip_dc21140_cogent_em100_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    return 1;
}

static void
tulip_dc21140_cogent_em100_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_flags |= TULIP_LINKUP;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_INIT);
    if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
    sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
    sc->tulip_media = TULIP_MEDIA_100BASETX;
#ifdef BIG_PACKET
    if (sc->tulip_if.if_mtu > ETHERMTU) {
	TULIP_CSR_WRITE(sc, csr_watchdog, TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE);
    }
#endif
}

static const tulip_boardsw_t tulip_dc21140_cogent_em100_boardsw = {
    TULIP_DC21140_COGENT_EM100,
    "Cogent EM100 ",
    tulip_dc21140_cogent_em100_media_probe,
    tulip_dc21140_cogent_em100_media_select,
    tulip_dc21140_nomii_100only_media_preset
};


static int
tulip_dc21140_znyx_zx34x_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);

    return (TULIP_CSR_READ(sc, csr_gp) & TULIP_GP_ZX34X_OK10);
}

static void
tulip_dc21140_znyx_zx34x_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_flags |= TULIP_LINKUP;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_INIT);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_media = TULIP_MEDIA_100BASETX;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    sc->tulip_flags ^= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_media = TULIP_MEDIA_10BASET;
    }
#ifdef BIG_PACKET
    if (sc->tulip_if.if_mtu > ETHERMTU) {
	TULIP_CSR_WRITE(sc, csr_watchdog, TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE);
    }
#endif
}

static const tulip_boardsw_t tulip_dc21140_znyx_zx34x_boardsw = {
    TULIP_DC21140_ZNYX_ZX34X,
    "ZNYX ZX34X ",
    tulip_dc21140_znyx_zx34x_media_probe,
    tulip_dc21140_znyx_zx34x_media_select,
    tulip_dc21140_nomii_media_preset,
};

static const struct {
    unsigned short value_gp;
    unsigned short value_phyctl;
} tulip_dc21140_de500_csrvalues[] = {
    { TULIP_GP_DE500_HALFDUPLEX, 0 },	/* TULIP_MEDIA_UNKNOWN */
    { TULIP_GP_DE500_HALFDUPLEX, 0 },	/* TULIP_MEDIA_10BASET */
    { /* n/a */ },			/* TULIP_MEDIA_BNC */
    { /* n/a */ },			/* TULIP_MEDIA_AUI */
    { /* n/a */ },			/* TULIP_MEDIA_BNCAUI */
    { 0, PHYCTL_FULL_DUPLEX },		/* TULIP_MEDIA_10BASET_FD */
    { TULIP_GP_DE500_HALFDUPLEX|	/* TULIP_MEDIA_100BASET */
      TULIP_GP_DE500_FORCE_100, PHYCTL_SELECT_100MB },
    { TULIP_GP_DE500_FORCE_100,		/* TULIP_MEDIA_100BASET_FD */
      PHYCTL_SELECT_100MB|PHYCTL_FULL_DUPLEX },
    { TULIP_GP_DE500_HALFDUPLEX|	/* TULIP_MEDIA_100BASET4 */
      TULIP_GP_DE500_FORCE_100, PHYCTL_SELECT_100MB },
};

static void
tulip_dc21140_de500_media_select(
    tulip_softc_t * const sc)
{
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	    sc->tulip_media = TULIP_MEDIA_100BASETX_FD;
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	} else {
	    sc->tulip_media = TULIP_MEDIA_100BASETX;
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	}
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    sc->tulip_flags |= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
    } else {
	if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	    sc->tulip_media = TULIP_MEDIA_10BASET_FD;
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	} else {
	    sc->tulip_media = TULIP_MEDIA_10BASET;
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	}
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    sc->tulip_flags ^= TULIP_PRINTMEDIA|TULIP_ALTPHYS;
    }
}

static int
tulip_dc21140_de500xa_media_probe(
    tulip_softc_t * const sc)
{
    int idx;

    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_DE500_PINS);
    DELAY(500);
    TULIP_CSR_WRITE(sc, csr_gp,
		    TULIP_GP_DE500_HALFDUPLEX|TULIP_GP_DE500_FORCE_100);
    DELAY(1000);
    TULIP_CSR_WRITE(sc, csr_command,
		    TULIP_CSR_READ(sc, csr_command)
		    |TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
		    |TULIP_CMD_SCRAMBLER|TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
		    TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    for (idx = 2400; idx > 0; idx--) {
	tulip_uint32_t data;
	DELAY(1000);
	data = ~TULIP_CSR_READ(sc, csr_gp);
	if ((data & (TULIP_GP_DE500_LINK_PASS|TULIP_GP_DE500_SYM_LINK)) == (TULIP_GP_DE500_SYM_LINK|TULIP_GP_DE500_LINK_PASS))
	    return 1;
    }
    return 0;
}

static void
tulip_dc21140_de500xa_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_flags |= TULIP_LINKUP;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_DE500_PINS);
    tulip_dc21140_de500_media_select(sc);
    TULIP_CSR_WRITE(sc, csr_gp, tulip_dc21140_de500_csrvalues[sc->tulip_media].value_gp);
#ifdef BIG_PACKET
    if (sc->tulip_if.if_mtu > ETHERMTU) {
	TULIP_CSR_WRITE(sc, csr_watchdog, TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE);
    }
#endif
}

static const tulip_boardsw_t tulip_dc21140_de500xa_boardsw = {
    TULIP_DC21140_DEC_DE500, "Digital DE500-XA ",
    tulip_dc21140_de500xa_media_probe,
    tulip_dc21140_de500xa_media_select,
    tulip_dc21140_nomii_media_preset,
};

static int
tulip_dc21140_de500aa_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_DE500_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_DE500_PHY_RESET);
    DELAY(1000);
    TULIP_CSR_WRITE(sc, csr_gp, 0);

    TULIP_CSR_WRITE(sc, csr_command, TULIP_CMD_PORTSELECT);
    return 0;
}

static void
tulip_dc21140_de500aa_media_select(
    tulip_softc_t * const sc)
{
    const tulip_phy_t *phy = sc->tulip_phys;
    tulip_uint32_t data;

    if (phy == NULL)
	return;

    /*
     * Defer autosensing until out of device probe (will be
     * triggered by ifwatchdog or ifioctl).
     */

    if (sc->tulip_media == TULIP_MEDIA_UNKNOWN) {
	tulip_media_t old_media;
	if (sc->tulip_probe_state != TULIP_PROBE_MEDIATEST)
	    tulip_dc21140_autonegotiate(sc, phy);
	if (sc->tulip_probe_state != TULIP_PROBE_MEDIATEST)
	    return;
	old_media = sc->tulip_media;
	if (sc->tulip_if.if_flags & IFF_NOAUTONEG) {
	    tulip_dc21140_de500_media_select(sc);
	} else {
	    sc->tulip_media = tulip_dc21140_phy_readspecific(sc, phy);
	    if (sc->tulip_media == TULIP_MEDIA_UNKNOWN) {
		sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		tulip_dc21140_autonegotiate(sc, phy);
		return;
	    }
	    sc->tulip_flags |= TULIP_PRINTMEDIA;
	}
	sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
	sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	if (sc->tulip_flags & TULIP_INRESET)
	    goto in_reset;
	if (sc->tulip_media != old_media)
	    sc->tulip_flags |= TULIP_NEEDRESET;
	return;
    }
    if ((sc->tulip_flags & TULIP_INRESET) == 0) {
	tulip_dc21140_mii_link_monitor(sc, phy);
	return;
    }
  in_reset:
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	sc->tulip_flags |= TULIP_ALTPHYS;
    } else {
	sc->tulip_flags &= ~TULIP_ALTPHYS;
    }
    sc->tulip_gpticks = 8;
    sc->tulip_intrmask |= TULIP_STS_ABNRMLINTR|TULIP_STS_GPTIMEOUT|TULIP_STS_NORMALINTR;
    tulip_dc21140_gp_timer_set(sc, 425);
    data = tulip_mii_readreg(sc, phy->phy_devaddr, PHYREG_CONTROL);
    if ((data & PHYCTL_AUTONEG_ENABLE) == 0) {
	data &= ~(PHYCTL_SELECT_100MB|PHYCTL_FULL_DUPLEX);
	data |= tulip_dc21140_de500_csrvalues[sc->tulip_media].value_phyctl;
	tulip_mii_writereg(sc, phy->phy_devaddr, PHYREG_CONTROL, data);
    }
}

static const tulip_boardsw_t tulip_dc21140_de500aa_boardsw = {
    TULIP_DC21140_DEC_DE500, "Digital DE500-AA ",
    tulip_dc21140_de500aa_media_probe,
    tulip_dc21140_de500aa_media_select,
    tulip_dc21140_mii_media_preset,
    tulip_dc21140_mii_probe,
};

static int
tulip_dc21041_media_probe(
    tulip_softc_t * const sc)
{
    sc->tulip_if.if_baudrate = 10000000;
    return 0;
}

#ifdef BIG_PACKET
#define TULIP_DC21041_SIAGEN_WATCHDOG	(sc->tulip_if.if_mtu > ETHERMTU ? TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE : 0)
#else
#define	TULIP_DC21041_SIAGEN_WATCHDOG	0
#endif

static void
tulip_dc21041_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_ENHCAPTEFFCT
	|TULIP_CMD_THRSHLD160|TULIP_CMD_BACKOFFCTR;
    sc->tulip_intrmask |= TULIP_STS_NORMALINTR|TULIP_STS_GPTIMEOUT|TULIP_STS_TXINTR
	|TULIP_STS_ABNRMLINTR|TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL;
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0) {
	    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	    sc->tulip_flags &= ~(TULIP_TXPROBE_OK|TULIP_TXPROBE_ACTIVE);
	    sc->tulip_flags |= TULIP_ALTPHYS|TULIP_WANTRXACT;
	    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
	}
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS) {
	    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	    sc->tulip_flags &= ~(TULIP_TXPROBE_OK|TULIP_TXPROBE_ACTIVE|TULIP_ALTPHYS);
	    sc->tulip_flags |= TULIP_WANTRXACT;
	    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
	}
    }

    if (TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) {
	if (sc->tulip_media == TULIP_MEDIA_10BASET) {
	    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	} else if (sc->tulip_media == TULIP_MEDIA_BNC) {
	    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    DELAY(50);
	    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_BNC);
	    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_BNC);
	    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_BNC|TULIP_DC21041_SIAGEN_WATCHDOG);
	    DELAY(50);
	    return;
	} else if (sc->tulip_media == TULIP_MEDIA_AUI) {
	    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    DELAY(50);
	    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_AUI);
	    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_AUI);
	    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_AUI|TULIP_DC21041_SIAGEN_WATCHDOG);
	    DELAY(50);
	    return;
	}

	switch (sc->tulip_probe_state) {
	    case TULIP_PROBE_INACTIVE: {
		sc->tulip_if.if_flags |= IFF_OACTIVE;
		sc->tulip_gpticks = 200;
		sc->tulip_probe_state = TULIP_PROBE_10BASET;
		sc->tulip_flags |= TULIP_TXPROBE_ACTIVE;
		sc->tulip_flags &= ~(TULIP_TXPROBE_OK|TULIP_LINKUP);
		sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
		TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode & ~TULIP_CMD_RXRUN);

		TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		DELAY(50);
		TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_10BASET);
		if (sc->tulip_cmdmode & TULIP_CMD_FULLDUPLEX)
		    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_10BASET_FD);
		else 
		    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_10BASET);
		TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_10BASET|TULIP_DC21041_SIAGEN_WATCHDOG);
		DELAY(50);
		TULIP_CSR_WRITE(sc, csr_gp_timer, 12000000 / 204800); /* 120 ms */
		TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_GPTIMEOUT);
		tulip_ifstart(&sc->tulip_if);
		break;
	    }
	    case TULIP_PROBE_10BASET: {
		if (--sc->tulip_gpticks > 0) {
		    if ((TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_OTHERRXACTIVITY) == 0) {
			TULIP_CSR_WRITE(sc, csr_gp_timer, 12000000 / 204800); /* 120 ms */
		 /* TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask); */
			break;
		    }
		}
		sc->tulip_gpticks = 4;
		if (TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_OTHERRXACTIVITY) {
		    sc->tulip_probe_state = TULIP_PROBE_BNC;
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_BNC);
		    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_BNC);
		    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_BNC|TULIP_DC21041_SIAGEN_WATCHDOG);
		    TULIP_CSR_WRITE(sc, csr_sia_status, TULIP_SIASTS_OTHERRXACTIVITY);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		} else {
		    sc->tulip_probe_state = TULIP_PROBE_AUI;
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_AUI);
		    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_AUI);
		    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_AUI|TULIP_DC21041_SIAGEN_WATCHDOG);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		}
		break;
	    }
	    case TULIP_PROBE_BNC:
	    case TULIP_PROBE_AUI: {
		if (sc->tulip_flags & TULIP_TXPROBE_OK) {
		    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
		    sc->tulip_flags &= ~(TULIP_TXPROBE_OK|TULIP_TXPROBE_ACTIVE);
		    sc->tulip_flags |= TULIP_LINKUP;
		    TULIP_CSR_WRITE(sc, csr_gp_timer, 0); /* disable */
		    if (sc->tulip_probe_state == TULIP_PROBE_AUI) {
			if (sc->tulip_media != TULIP_MEDIA_AUI) {
			    sc->tulip_media = TULIP_MEDIA_AUI;
			    sc->tulip_flags |= TULIP_PRINTMEDIA;
			}
		    } else if (sc->tulip_probe_state == TULIP_PROBE_BNC) {
			if (sc->tulip_media != TULIP_MEDIA_BNC) {
			    sc->tulip_media = TULIP_MEDIA_BNC;
			    sc->tulip_flags |= TULIP_PRINTMEDIA;
			}
		    }
		    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
		    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		    break;
		}
		if ((sc->tulip_flags & TULIP_WANTRXACT) == 0
		    || (TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_RXACTIVITY)) {
		    if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0) {
			struct mbuf *m;
			/*
			 * Before we are sure this is the right media we need
			 * to send a small packet to make sure there's carrier.
			 * Strangely, BNC and AUI will 'see" receive data if
			 * either is connected so the transmit is the only way
			 * to verify the connectivity.
			 */
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
			    TULIP_CSR_WRITE(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
			    break;
			}
			/*
			 * Construct a LLC TEST message which will point to ourselves.
			 */
			bcopy(sc->tulip_hwaddr, mtod(m, struct ether_header *)->ether_dhost, 6);
			bcopy(sc->tulip_hwaddr, mtod(m, struct ether_header *)->ether_shost, 6);
			mtod(m, struct ether_header *)->ether_type = htons(3);
			mtod(m, unsigned char *)[14] = 0;
			mtod(m, unsigned char *)[15] = 0;
			mtod(m, unsigned char *)[16] = 0xE3;	/* LLC Class1 TEST (no poll) */
			m->m_len = m->m_pkthdr.len = sizeof(struct ether_header) + 3;
			/*
			 * send it!
			 */
			sc->tulip_flags &= ~TULIP_TXPROBE_OK;
			IF_PREPEND(&sc->tulip_if.if_snd, m);
			tulip_ifstart(&sc->tulip_if);
			break;
		    }
		    sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
		}
		/*
		 * Take 2 passes through before deciding to not
		 * wait for receive activity.  Then take another
		 * two passes before spitting out a warning.
		 */
		if (sc->tulip_gpticks > 0 && --sc->tulip_gpticks == 0) {
		    if (sc->tulip_flags & TULIP_WANTRXACT) {
			sc->tulip_flags &= ~TULIP_WANTRXACT;
			sc->tulip_gpticks = 4;
		    } else {
			printf(TULIP_PRINTF_FMT ": autosense failed: cable problem?\n",
			       TULIP_PRINTF_ARGS);
		    }
		}
		/*
		 * Since this media failed to probe, try the other one.
		 */
		if (sc->tulip_probe_state == TULIP_PROBE_AUI) {
		    sc->tulip_probe_state = TULIP_PROBE_BNC;
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_BNC);
		    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_BNC);
		    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_BNC|TULIP_DC21041_SIAGEN_WATCHDOG);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		} else {
		    sc->tulip_probe_state = TULIP_PROBE_AUI;
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_AUI);
		    TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_AUI);
		    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_AUI|TULIP_DC21041_SIAGEN_WATCHDOG);
		    DELAY(50);
		    TULIP_CSR_WRITE(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		}
		break;
	    }
	}
    } else {
	/*
	 * If the link has passed LinkPass, 10baseT is the
	 * proper media to use.
	 */
	if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	    if (sc->tulip_media != TULIP_MEDIA_10BASET_FD) {
		sc->tulip_media = TULIP_MEDIA_10BASET_FD;
		sc->tulip_flags |= TULIP_PRINTMEDIA;
		sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	    }
	} else {
	    if (sc->tulip_media != TULIP_MEDIA_10BASET) {
		sc->tulip_media = TULIP_MEDIA_10BASET;
		sc->tulip_flags |= TULIP_PRINTMEDIA;
		sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	    }
	}
	if (sc->tulip_media != TULIP_MEDIA_10BASET
		|| (sc->tulip_flags & TULIP_INRESET)) {
	    sc->tulip_media = TULIP_MEDIA_10BASET;
	    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    DELAY(50);
	    TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_10BASET);
	    if (sc->tulip_cmdmode & TULIP_CMD_FULLDUPLEX)
		TULIP_CSR_WRITE(sc, csr_sia_tx_rx,    TULIP_DC21041_SIATXRX_10BASET_FD);
	    else 
		TULIP_CSR_WRITE(sc, csr_sia_tx_rx,    TULIP_DC21041_SIATXRX_10BASET);
	    TULIP_CSR_WRITE(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_10BASET|TULIP_DC21041_SIAGEN_WATCHDOG);
	    DELAY(50);
	}
	TULIP_CSR_WRITE(sc, csr_gp_timer, 0); /* disable */
	sc->tulip_gpticks = 1;
	sc->tulip_probe_state = TULIP_PROBE_10BASET;
	sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	sc->tulip_flags |= TULIP_LINKUP;
	sc->tulip_flags &= ~(TULIP_TXPROBE_OK|TULIP_TXPROBE_ACTIVE);
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
    }
    if (sc->tulip_flags & TULIP_DEVICEPROBE) {
	sc->tulip_flags |= TULIP_FAKEGPTIMEOUT;
    } else {
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	sc->tulip_flags &= ~TULIP_FAKEGPTIMEOUT;
    }
}

static const tulip_boardsw_t tulip_dc21041_boardsw = {
    TULIP_DC21041_GENERIC,
    "",
    tulip_dc21041_media_probe,
    tulip_dc21041_media_select
};

static void
tulip_reset(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t *ri;
    tulip_desc_t *di;

    /*
     * Brilliant.  Simply brilliant.  When switching modes/speeds
     * on a DC2114*, you need to set the appriopriate MII/PCS/SCL/PS
     * bits in CSR6 and then do a software reset to get the DC21140
     * to properly reset its internal pathways to the right places.
     *   Grrrr.
     */
    if (sc->tulip_boardsw->bd_media_preset != NULL)
	(*sc->tulip_boardsw->bd_media_preset)(sc);

    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    sc->tulip_flags |= TULIP_INRESET;
    sc->tulip_flags &= ~(TULIP_NEEDRESET|TULIP_RXBUFSLOW);
    sc->tulip_intrmask = 0;
    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);

    TULIP_CSR_WRITE(sc, csr_txlist, TULIP_KVATOPHYS(sc, &sc->tulip_txinfo.ri_first[0]));
    TULIP_CSR_WRITE(sc, csr_rxlist, TULIP_KVATOPHYS(sc, &sc->tulip_rxinfo.ri_first[0]));
    TULIP_CSR_WRITE(sc, csr_busmode,
	(1 << (TULIP_BURSTSIZE(sc->tulip_unit) + 8))
	|TULIP_BUSMODE_CACHE_ALIGN8
	|(BYTE_ORDER != LITTLE_ENDIAN ? TULIP_BUSMODE_BIGENDIAN : 0));

    sc->tulip_gpticks = 0;
    sc->tulip_txtimer = 0;
    sc->tulip_txq.ifq_maxlen = TULIP_TXDESCS;
    sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    /*
     * Free all the mbufs that were on the transmit ring.
     */
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_txq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    ri = &sc->tulip_txinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++)
	di->d_status = 0;

    /*
     * We need to collect all the mbufs were on the 
     * receive ring before we reinit it either to put
     * them back on or to know if we have to allocate
     * more.
     */
    ri = &sc->tulip_rxinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	di->d_status = 0;
	di->d_length1 = 0; di->d_addr1 = 0;
	di->d_length2 = 0; di->d_addr2 = 0;
    }
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_rxq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    (*sc->tulip_boardsw->bd_media_select)(sc);
#ifdef TULIP_DEBUG
    if ((sc->tulip_flags & (TULIP_DEVICEPROBE|TULIP_NEEDRESET)) == TULIP_NEEDRESET)
	printf(TULIP_PRINTF_FMT ": tulip_reset: additional reset needed?!?\n",
	       TULIP_PRINTF_ARGS);
#endif
    if ((sc->tulip_flags & (TULIP_LINKUP|TULIP_PRINTMEDIA)) == (TULIP_LINKUP|TULIP_PRINTMEDIA)) {
	printf(TULIP_PRINTF_FMT ": enabling %s port\n",
	       TULIP_PRINTF_ARGS,
	       tulip_mediums[sc->tulip_media]);
	sc->tulip_flags &= ~TULIP_PRINTMEDIA;
    }
    if (sc->tulip_chipid == TULIP_DC21041)
	TULIP_CSR_WRITE(sc, csr_sia_status, TULIP_CSR_READ(sc, csr_sia_status));

    sc->tulip_intrmask |= TULIP_STS_NORMALINTR|TULIP_STS_RXINTR|TULIP_STS_TXINTR
	|TULIP_STS_ABNRMLINTR|TULIP_STS_SYSERROR|TULIP_STS_TXSTOPPED
	    |TULIP_STS_TXBABBLE|TULIP_STS_LINKFAIL|TULIP_STS_RXSTOPPED;
    sc->tulip_flags &= ~(TULIP_DOINGSETUP|TULIP_WANTSETUP|TULIP_INRESET
			 |TULIP_RXACT);
    tulip_addr_filter(sc);
}

static void
tulip_init(
    tulip_softc_t * const sc)
{
    if (sc->tulip_if.if_flags & IFF_UP) {
	sc->tulip_if.if_flags |= IFF_RUNNING;
	if (sc->tulip_if.if_flags & IFF_PROMISC) {
	    sc->tulip_cmdmode |= TULIP_CMD_PROMISCUOUS;
	} else {
	    sc->tulip_cmdmode &= ~TULIP_CMD_PROMISCUOUS;
	    if (sc->tulip_if.if_flags & IFF_ALLMULTI) {
		sc->tulip_cmdmode |= TULIP_CMD_ALLMULTI;
	    } else {
		sc->tulip_cmdmode &= ~TULIP_CMD_ALLMULTI;
	    }
	}
	sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
	if ((sc->tulip_flags & TULIP_WANTSETUP) == 0) {
	    tulip_rx_intr(sc);
	    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
	    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
	} else {
	    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
	    tulip_ifstart(&sc->tulip_if);
	}
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
    } else {
	tulip_reset(sc);
	sc->tulip_if.if_flags &= ~IFF_RUNNING;
    }
}

static void
tulip_rx_intr(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t * const ri = &sc->tulip_rxinfo;
    struct ifnet * const ifp = &sc->tulip_if;
    int fillok = 1;
#ifdef TULIP_DEBUG
    int cnt = 0;
#endif

    for (;;) {
	struct ether_header eh;
	tulip_desc_t *eop = ri->ri_nextin;
	int total_len = 0, last_offset = 0;
	struct mbuf *ms = NULL, *me = NULL;
	int accept = 0;

	if (fillok && sc->tulip_rxq.ifq_len < TULIP_RXQ_TARGET)
	    goto queue_mbuf;

#ifdef TULIP_DEBUG
	if (cnt == ri->ri_max) {
	    sc->tulip_dbg.dbg_rxintrs++;
	    sc->tulip_dbg.dbg_rxpktsperintr[cnt]++;
	    return;
	}
#endif

	if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER) {
#ifdef TULIP_DEBUG
	    sc->tulip_dbg.dbg_rxintrs++;
	    sc->tulip_dbg.dbg_rxpktsperintr[cnt]++;
#endif
	    return;
	}
	/*
	 * It is possible (though improbable unless the BIG_PACKET support
	 * is enabled or MCLBYTES < 1518) for a received packet to cross
	 * more than one receive descriptor.  
	 */
	while ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_RxLASTDESC) == 0) {
	    if (++eop == ri->ri_last)
		eop = ri->ri_first;
	    if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER) {
#ifdef TULIP_DEBUG
		sc->tulip_dbg.dbg_rxintrs++;
		sc->tulip_dbg.dbg_rxpktsperintr[cnt]++;
#endif
		return;
	    }
	    total_len++;
	}

	/*
	 * Dequeue the first buffer for the start of the packet.  Hopefully this
	 * will be the only one we need to dequeue.  However, if the packet consumed
	 * multiple descriptors, then we need to dequeue those buffers and chain to
	 * the starting mbuf.  All buffers but the last buffer have the same length
	 * so we can set that now.  (we add to last_offset instead of multiplying 
	 * since we normally won't go into the loop and thereby saving a ourselves
	 * from doing a multiplication by 0 in the normal case).
	 */
	IF_DEQUEUE(&sc->tulip_rxq, ms);
	for (me = ms; total_len > 0; total_len--) {
	    me->m_len = TULIP_RX_BUFLEN;
	    last_offset += TULIP_RX_BUFLEN;
	    IF_DEQUEUE(&sc->tulip_rxq, me->m_next);
	    me = me->m_next;
	}

	/*
	 *  Now get the size of received packet (minus the CRC).
	 */
	total_len = ((eop->d_status >> 16) & 0x7FFF) - 4;
	if ((eop->d_status & TULIP_DSTS_ERRSUM) == 0
#ifdef BIG_PACKET
	     || (total_len <= sc->tulip_if.if_mtu + sizeof(struct ether_header) && 
		 (eop->d_status & (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxRUNT|
				  TULIP_DSTS_RxCOLLSEEN|TULIP_DSTS_RxBADCRC|
				  TULIP_DSTS_RxOVERFLOW)) == 0)
#endif
		) {
	    me->m_len = total_len - last_offset;
	    eh = *mtod(ms, struct ether_header *);
#if NBPFILTER > 0
	    if (sc->tulip_bpf != NULL)
		if (me == ms)
		    TULIP_BPF_TAP(sc, mtod(ms, caddr_t), total_len);
		else
		    TULIP_BPF_MTAP(sc, ms);
#endif
	    if ((sc->tulip_if.if_flags & IFF_PROMISC)
		    && (eh.ether_dhost[0] & 1) == 0
		    && !TULIP_ADDREQUAL(eh.ether_dhost, sc->tulip_ac.ac_enaddr))
		    goto next;
	    accept = 1;
	    sc->tulip_flags |= TULIP_RXACT;
	    total_len -= sizeof(struct ether_header);
	} else {
	    ifp->if_ierrors++;
	    if (eop->d_status & (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxOVERFLOW|TULIP_DSTS_RxWATCHDOG)) {
		sc->tulip_dot3stats.dot3StatsInternalMacReceiveErrors++;
	    } else {
		const char *error = NULL;
		if (eop->d_status & TULIP_DSTS_RxTOOLONG) {
		    sc->tulip_dot3stats.dot3StatsFrameTooLongs++;
		    error = "frame too long";
		}
		if (eop->d_status & TULIP_DSTS_RxBADCRC) {
		    if (eop->d_status & TULIP_DSTS_RxDRBBLBIT) {
			sc->tulip_dot3stats.dot3StatsAlignmentErrors++;
			error = "alignment error";
		    } else {
			sc->tulip_dot3stats.dot3StatsFCSErrors++;
			error = "bad crc";
		    }
		}
		if (error != NULL && (sc->tulip_flags & TULIP_NOMESSAGES) == 0) {
		    printf(TULIP_PRINTF_FMT ": receive: " TULIP_EADDR_FMT ": %s\n",
			   TULIP_PRINTF_ARGS,
			   TULIP_EADDR_ARGS(mtod(ms, u_char *) + 6),
			   error);
		    sc->tulip_flags |= TULIP_NOMESSAGES;
		}
	    }
	}
      next:
#ifdef TULIP_DEBUG
	cnt++;
#endif
	ifp->if_ipackets++;
	if (++eop == ri->ri_last)
	    eop = ri->ri_first;
	ri->ri_nextin = eop;
      queue_mbuf:
	/*
	 * Either we are priming the TULIP with mbufs (m == NULL)
	 * or we are about to accept an mbuf for the upper layers
	 * so we need to allocate an mbuf to replace it.  If we
	 * can't replace it, send up it anyways.  This may cause
	 * us to drop packets in the future but that's better than
	 * being caught in livelock.
	 *
	 * Note that if this packet crossed multiple descriptors
	 * we don't even try to reallocate all the mbufs here.
	 * Instead we rely on the test of the beginning of
	 * the loop to refill for the extra consumed mbufs.
	 */
	if (accept || ms == NULL) {
	    struct mbuf *m0;
	    MGETHDR(m0, M_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
#if defined(TULIP_COPY_RXDATA)
		if (!accept || total_len >= MHLEN) {
#endif
		    MCLGET(m0, M_DONTWAIT);
		    if ((m0->m_flags & M_EXT) == 0) {
			m_freem(m0);
			m0 = NULL;
		    }
#if defined(TULIP_COPY_RXDATA)
		}
#endif
	    }
	    if (accept) {
#if defined(__bsdi__)
		eh.ether_type = ntohs(eh.ether_type);
#endif
#if !defined(TULIP_COPY_RXDATA)
		ms->m_data += sizeof(struct ether_header);
		ms->m_len -= sizeof(struct ether_header);
		ms->m_pkthdr.len = total_len;
		ms->m_pkthdr.rcvif = ifp;
		ether_input(ifp, &eh, ms);
#else
#ifdef BIG_PACKET
#error BIG_PACKET is incompatible with TULIP_COPY_RXDATA
#endif
		if (ms == me)
		    bcopy(mtod(ms, caddr_t) + sizeof(struct ether_header),
			  mtod(m0, caddr_t), total_len);
		else
		    m_copydata(ms, 0, total_len, mtod(m0, caddr_t));
		m0->m_len = m0->m_pkthdr.len = total_len;
		m0->m_pkthdr.rcvif = ifp;
		ether_input(ifp, &eh, m0);
		m0 = ms;
#endif
	    }
	    ms = m0;
	}
	if (ms == NULL) {
	    /*
	     * Couldn't allocate a new buffer.  Don't bother 
	     * trying to replenish the receive queue.
	     */
	    fillok = 0;
	    sc->tulip_flags |= TULIP_RXBUFSLOW;
#ifdef TULIP_DEBUG
	    sc->tulip_dbg.dbg_rxlowbufs++;
#endif
	    continue;
	}
	/*
	 * Now give the buffer(s) to the TULIP and save in our
	 * receive queue.
	 */
	do {
	    ri->ri_nextout->d_length1 = TULIP_RX_BUFLEN;
	    ri->ri_nextout->d_addr1 = TULIP_KVATOPHYS(sc, mtod(ms, caddr_t));
	    ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	    me = ms->m_next;
	    ms->m_next = NULL;
	    IF_ENQUEUE(&sc->tulip_rxq, ms);
	} while ((ms = me) != NULL);

	if (sc->tulip_rxq.ifq_len == TULIP_RXQ_TARGET)
	    sc->tulip_flags &= ~TULIP_RXBUFSLOW;
    }
}

static int
tulip_tx_intr(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t * const ri = &sc->tulip_txinfo;
    struct mbuf *m;
    int xmits = 0;

    while (ri->ri_free < ri->ri_max) {
	if (((volatile tulip_desc_t *) ri->ri_nextin)->d_status & TULIP_DSTS_OWNER)
	    break;

	if (ri->ri_nextin->d_flag & TULIP_DFLAG_TxLASTSEG) {
	    if (ri->ri_nextin->d_flag & TULIP_DFLAG_TxSETUPPKT) {
		/*
		 * We've just finished processing a setup packet.
		 * Mark that we can finished it.  If there's not
		 * another pending, startup the TULIP receiver.
		 * Make sure we ack the RXSTOPPED so we won't get
		 * an abormal interrupt indication.
		 */
		sc->tulip_flags &= ~TULIP_DOINGSETUP;
		if ((sc->tulip_flags & TULIP_WANTSETUP) == 0
		        && (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0) {
		    tulip_rx_intr(sc);
		    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
		    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
		    TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
		    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
		}
	    } else {
		tulip_desc_t * eop = ri->ri_nextin;
		IF_DEQUEUE(&sc->tulip_txq, m);
		m_freem(m);
		xmits++;
		if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) {
		    if ((eop->d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxEXCCOLL)) == 0)
			sc->tulip_flags |= TULIP_TXPROBE_OK;
		    (*sc->tulip_boardsw->bd_media_select)(sc);
		    if (sc->tulip_chipid == TULIP_DC21041)
			TULIP_CSR_WRITE(sc, csr_sia_status, TULIP_CSR_READ(sc, csr_sia_status));
		} else {
		    if (eop->d_status & TULIP_DSTS_ERRSUM) {
			sc->tulip_if.if_oerrors++;
			if (eop->d_status & TULIP_DSTS_TxEXCCOLL)
			    sc->tulip_dot3stats.dot3StatsExcessiveCollisions++;
			if (eop->d_status & TULIP_DSTS_TxLATECOLL)
			    sc->tulip_dot3stats.dot3StatsLateCollisions++;
			if (eop->d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxCARRLOSS))
			    sc->tulip_dot3stats.dot3StatsCarrierSenseErrors++;
			if (eop->d_status & (TULIP_DSTS_TxUNDERFLOW|TULIP_DSTS_TxBABBLE))
			    sc->tulip_dot3stats.dot3StatsInternalMacTransmitErrors++;
		    } else {
			tulip_uint32_t collisions = 
			    (eop->d_status & TULIP_DSTS_TxCOLLMASK)
				>> TULIP_DSTS_V_TxCOLLCNT;
			sc->tulip_if.if_collisions += collisions;
			if (collisions == 1)
			    sc->tulip_dot3stats.dot3StatsSingleCollisionFrames++;
			else if (collisions > 1)
			    sc->tulip_dot3stats.dot3StatsMultipleCollisionFrames++;
			else if (eop->d_status & TULIP_DSTS_TxDEFERRED)
			    sc->tulip_dot3stats.dot3StatsDeferredTransmissions++;
			/*
			 * SQE is only valid for 10baseT/BNC/AUI when not
			 * running in full-duplex.  In order to speed up the
			 * test, the corresponding bit in tulip_flags needs to
			 * set as well to get us to count SQE Test Errors.
			 */
			if (eop->d_status & TULIP_DSTS_TxNOHRTBT & sc->tulip_flags)
			    sc->tulip_dot3stats.dot3StatsSQETestErrors++;
		    }
		}
	    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;
	ri->ri_free++;
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }
    /*
     * If nothing left to transmit, disable the timer.
     * Else if progress, reset the timer back to 2 ticks.
     */
    if (ri->ri_free == ri->ri_max)
	sc->tulip_txtimer = 0;
    else if (xmits > 0)
	sc->tulip_txtimer = 2;
    sc->tulip_if.if_opackets += xmits;
    return xmits;
}

static ifnet_ret_t
tulip_ifstart(
    struct ifnet * const ifp)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);
    struct ifqueue * const ifq = &ifp->if_snd;
    tulip_ringinfo_t * const ri = &sc->tulip_txinfo;
    struct mbuf *m, *m0, *next_m0;

    if ((ifp->if_flags & IFF_RUNNING) == 0
	    && (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
	return;

    for (;;) {
	tulip_desc_t *eop, *nextout;
	int segcnt, free, recopy;
	tulip_uint32_t d_status;

	if (sc->tulip_flags & TULIP_WANTSETUP) {
	    if ((sc->tulip_flags & TULIP_DOINGSETUP) || ri->ri_free == 1) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	    }
	    bcopy(sc->tulip_setupdata, sc->tulip_setupbuf,
		   sizeof(sc->tulip_setupbuf));
	    sc->tulip_flags &= ~TULIP_WANTSETUP;
	    sc->tulip_flags |= TULIP_DOINGSETUP;
	    ri->ri_free--;
	    ri->ri_nextout->d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
	    ri->ri_nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG|TULIP_DFLAG_TxLASTSEG
		    |TULIP_DFLAG_TxSETUPPKT|TULIP_DFLAG_TxWANTINTR;
	    if (sc->tulip_flags & TULIP_WANTHASH)
		ri->ri_nextout->d_flag |= TULIP_DFLAG_TxHASHFILT;
	    ri->ri_nextout->d_length1 = sizeof(sc->tulip_setupbuf);
	    ri->ri_nextout->d_addr1 = TULIP_KVATOPHYS(sc, sc->tulip_setupbuf);
	    ri->ri_nextout->d_length2 = 0;
	    ri->ri_nextout->d_addr2 = 0;
	    ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
	    TULIP_CSR_WRITE(sc, csr_txpoll, 1);
	    /*
	     * Advance the ring for the next transmit packet.
	     */
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	    /*
	     * Make sure the next descriptor is owned by us since it
	     * may have been set up above if we ran out of room in the
	     * ring.
	     */
	    ri->ri_nextout->d_status = 0;
	}

	IF_DEQUEUE(ifq, m);
	if (m == NULL)
	    break;

	/*
	 * Now we try to fill in our transmit descriptors.  This is
	 * a bit reminiscent of going on the Ark two by two
	 * since each descriptor for the TULIP can describe
	 * two buffers.  So we advance through packet filling
	 * each of the two entries at a time to to fill each
	 * descriptor.  Clear the first and last segment bits
	 * in each descriptor (actually just clear everything
	 * but the end-of-ring or chain bits) to make sure
	 * we don't get messed up by previously sent packets.
	 *
	 * We may fail to put the entire packet on the ring if
	 * there is either not enough ring entries free or if the
	 * packet has more than MAX_TXSEG segments.  In the former
	 * case we will just wait for the ring to empty.  In the
	 * latter case we have to recopy.
	 */
	d_status = 0;
	recopy = 0;
	eop = nextout = ri->ri_nextout;
	m0 = m;
	segcnt = 0;
	free = ri->ri_free;
	do {
	    int len = m0->m_len;
	    caddr_t addr = mtod(m0, caddr_t);
	    unsigned clsize = CLBYTES - (((u_long) addr) & (CLBYTES-1));

	    next_m0 = m0->m_next;
	    while (len > 0) {
		unsigned slen = min(len, clsize);
#ifdef BIG_PACKET
		int partial = 0;
		if (slen >= 2048)
		    slen = 2040, partial = 1;
#endif
		segcnt++;
		if (segcnt > TULIP_MAX_TXSEG) {
		    recopy = 1;
		    next_m0 = NULL; /* to break out of outside loop */
		    break;
		}
		if (segcnt & 1) {
		    if (--free == 0) {
			/*
			 * There's no more room but since nothing
			 * has been committed at this point, just
			 * show output is active, put back the
			 * mbuf and return.
			 */
			ifp->if_flags |= IFF_OACTIVE;
			IF_PREPEND(ifq, m);
			return;
		    }
		    eop = nextout;
		    if (++nextout == ri->ri_last)
			nextout = ri->ri_first;
		    eop->d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
		    eop->d_status = d_status;
		    eop->d_addr1 = TULIP_KVATOPHYS(sc, addr);
		    eop->d_length1 = slen;
		} else {
		    /*
		     *  Fill in second half of descriptor
		     */
		    eop->d_addr2 = TULIP_KVATOPHYS(sc, addr);
		    eop->d_length2 = slen;
		}
		d_status = TULIP_DSTS_OWNER;
		len -= slen;
		addr += slen;
#ifdef BIG_PACKET
		if (partial)
		    continue;
#endif
		clsize = CLBYTES;
	    }
	} while ((m0 = next_m0) != NULL);

	/*
	 * The packet exceeds the number of transmit buffer
	 * entries that we can use for one packet, so we have
	 * recopy it into one mbuf and then try again.
	 */
	if (recopy) {
	    MGETHDR(m0, M_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
		if (m->m_pkthdr.len > MHLEN) {
		    MCLGET(m0, M_DONTWAIT);
		    if ((m0->m_flags & M_EXT) == 0) {
			m_freem(m);
			m_freem(m0);
			continue;
		    }
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
		IF_PREPEND(ifq, m0);
	    }
	    m_freem(m);
	    continue;
	}

	/*
	 * The descriptors have been filled in.  Now get ready
	 * to transmit.
	 */
#if NBPFILTER > 0
	if (sc->tulip_bpf != NULL)
	    TULIP_BPF_MTAP(sc, m);
#endif
	IF_ENQUEUE(&sc->tulip_txq, m);

	/*
	 * Make sure the next descriptor after this packet is owned
	 * by us since it may have been set up above if we ran out
	 * of room in the ring.
	 */
	nextout->d_status = 0;

	/*
	 * If we only used the first segment of the last descriptor,
	 * make sure the second segment will not be used.
	 */
	if (segcnt & 1) {
	    eop->d_addr2 = 0;
	    eop->d_length2 = 0;
	}

	/*
	 * Mark the last and first segments, indicate we want a transmit
	 * complete interrupt, give the descriptors to the TULIP, and tell
	 * it to transmit!
	 */
	eop->d_flag |= TULIP_DFLAG_TxLASTSEG|TULIP_DFLAG_TxWANTINTR;

	/*
	 * Note that ri->ri_nextout is still the start of the packet
	 * and until we set the OWNER bit, we can still back out of
	 * everything we have done.
	 */
	ri->ri_nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG;
	ri->ri_nextout->d_status = TULIP_DSTS_OWNER;

	/*
	 * This advances the ring for us.
	 */
	ri->ri_nextout = nextout;
	ri->ri_free = free;

	TULIP_CSR_WRITE(sc, csr_txpoll, 1);

	if (sc->tulip_txtimer == 0)
	    sc->tulip_txtimer = 2;
    }
    if (m != NULL) {
	ifp->if_flags |= IFF_OACTIVE;
	IF_PREPEND(ifq, m);
    }
}

static void
tulip_print_abnormal_interrupt(
    tulip_softc_t * const sc,
    tulip_uint32_t csr)
{
    const char * const *msgp = tulip_status_bits;
    const char *sep;

    csr &= (1 << (sizeof(tulip_status_bits)/sizeof(tulip_status_bits[0]))) - 1;
    printf(TULIP_PRINTF_FMT ": abnormal interrupt:", TULIP_PRINTF_ARGS);
    for (sep = " "; csr != 0; csr >>= 1, msgp++) {
	if ((csr & 1) && *msgp != NULL) {
	    printf("%s%s", sep, *msgp);
	    sep = ", ";
	}
    }
    printf("\n");
}

static tulip_intrfunc_t
tulip_intr(
    void *arg)
{
    tulip_softc_t * sc = (tulip_softc_t *) arg;
    tulip_uint32_t csr;
#if !defined(TULIP_VOID_INTRFUNC)
    int progress = 0;
#endif

    do {
#if defined(TULIP_DEBUG)
	sc->tulip_dbg.dbg_intrs++;
#endif
	while ((csr = TULIP_CSR_READ(sc, csr_status)) & (TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR)) {
#if !defined(TULIP_VOID_INTRFUNC)
	    progress = 1;
#endif
	    TULIP_CSR_WRITE(sc, csr_status, csr);

	    if (csr & TULIP_STS_SYSERROR) {
		sc->tulip_last_system_error = (csr & TULIP_STS_ERRORMASK) >> TULIP_STS_ERR_SHIFT;
		if (sc->tulip_flags & TULIP_NOMESSAGES) {
		    sc->tulip_flags |= TULIP_SYSTEMERROR;
		} else {
		    printf(TULIP_PRINTF_FMT ": system error: %s\n",
			   TULIP_PRINTF_ARGS,
			   tulip_system_errors[sc->tulip_last_system_error]);
		}
		sc->tulip_flags |= TULIP_NEEDRESET;
		sc->tulip_system_errors++;
		break;
	    }
	    if (csr & (TULIP_STS_GPTIMEOUT|TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL)) {
#if defined(TULIP_DEBUG)
		sc->tulip_dbg.dbg_gpintrs++;
#endif
		if (sc->tulip_chipid == TULIP_DC21041) {
		    (*sc->tulip_boardsw->bd_media_select)(sc);
		    if (csr & (TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL))
			csr &= ~TULIP_STS_ABNRMLINTR;
		    TULIP_CSR_WRITE(sc, csr_sia_status, TULIP_CSR_READ(sc, csr_sia_status));
		} else if (sc->tulip_chipid == TULIP_DC21140 || sc->tulip_chipid == TULIP_DC21140A) {
		    (*sc->tulip_boardsw->bd_media_select)(sc);
		    csr &= ~(TULIP_STS_ABNRMLINTR|TULIP_STS_GPTIMEOUT);
		}
		if ((sc->tulip_flags & (TULIP_LINKUP|TULIP_PRINTMEDIA)) == (TULIP_LINKUP|TULIP_PRINTMEDIA)) {
		    printf(TULIP_PRINTF_FMT ": enabling %s port\n",
			   TULIP_PRINTF_ARGS,
			   tulip_mediums[sc->tulip_media]);
		    sc->tulip_flags &= ~TULIP_PRINTMEDIA;
		}
	    }
	    if (csr & TULIP_STS_ABNRMLINTR) {
		tulip_uint32_t tmp = csr & sc->tulip_intrmask
			& ~(TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR);
		if (sc->tulip_flags & TULIP_NOMESSAGES) {
		    sc->tulip_statusbits |= tmp;
		} else {
		    tulip_print_abnormal_interrupt(sc, tmp);
		    sc->tulip_flags |= TULIP_NOMESSAGES;
		}
		TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	    }
	    if (csr & (TULIP_STS_RXINTR|TULIP_STS_RXNOBUF)) {
		tulip_rx_intr(sc);
		if (csr & TULIP_STS_RXNOBUF)
		    sc->tulip_dot3stats.dot3StatsMissedFrames +=
			TULIP_CSR_READ(sc, csr_missed_frames) & 0xFFFF;
	    }
	    if (sc->tulip_txinfo.ri_free < sc->tulip_txinfo.ri_max) {
		tulip_tx_intr(sc);
		tulip_ifstart(&sc->tulip_if);
	    }
	}
	if (sc->tulip_flags & TULIP_NEEDRESET) {
	    tulip_reset(sc);
	    tulip_init(sc);
	}
    } while ((sc = sc->tulip_slaves) != NULL);
#if !defined(TULIP_VOID_INTRFUNC)
    return progress;
#endif
}

/*
 * 
 */

static void
tulip_delay_300ns(
    tulip_softc_t * const sc)
{
    int idx;
    for (idx = (300 / 33) + 1; idx > 0; idx--)
	TULIP_CSR_READ(sc, csr_busmode);
}

#define EMIT    do { TULIP_CSR_WRITE(sc, csr_srom_mii, csr); tulip_delay_300ns(sc); } while (0)

static void
tulip_idle_srom(
    tulip_softc_t * const sc)
{
    unsigned bit, csr;
    
    csr  = SROMSEL ; EMIT;
    csr  = SROMSEL | SROMRD; EMIT;  
    csr ^= SROMCS; EMIT;
    csr ^= SROMCLKON; EMIT;

    /*
     * Write 25 cycles of 0 which will force the SROM to be idle.
     */
    for (bit = 3 + SROM_BITWIDTH + 16; bit > 0; bit--) {
        csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
    }
    csr ^= SROMCLKOFF; EMIT;
    csr ^= SROMCS; EMIT;
    csr  = 0; EMIT;
}

     
static void
tulip_read_srom(
    tulip_softc_t * const sc)
{   
    int idx; 
    const unsigned bitwidth = SROM_BITWIDTH;
    const unsigned cmdmask = (SROMCMD_RD << bitwidth);
    const unsigned msb = 1 << (bitwidth + 3 - 1);
    unsigned lastidx = (1 << bitwidth) - 1;

    tulip_idle_srom(sc);

    for (idx = 0; idx <= lastidx; idx++) {
        unsigned lastbit, data, bits, bit, csr;
	csr  = SROMSEL ;	        EMIT;
        csr  = SROMSEL | SROMRD;        EMIT;
        csr ^= SROMCSON;                EMIT;
        csr ^=            SROMCLKON;    EMIT;
    
        lastbit = 0;
        for (bits = idx|cmdmask, bit = bitwidth + 3; bit > 0; bit--, bits <<= 1) {
            const unsigned thisbit = bits & msb;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
            if (thisbit != lastbit) {
                csr ^= SROMDOUT; EMIT;  /* clock low; invert data */
            } else {
		EMIT;
	    }
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
            lastbit = thisbit;
        }
        csr ^= SROMCLKOFF; EMIT;

        for (data = 0, bits = 0; bits < 16; bits++) {
            data <<= 1;
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */ 
            data |= TULIP_CSR_READ(sc, csr_srom_mii) & SROMDIN ? 1 : 0;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        }
	sc->tulip_rombuf[idx*2] = data & 0xFF;
	sc->tulip_rombuf[idx*2+1] = data >> 8;
	csr  = SROMSEL | SROMRD; EMIT;
	csr  = 0; EMIT;
    }
    tulip_idle_srom(sc);
}

#define MII_EMIT    do { TULIP_CSR_WRITE(sc, csr_srom_mii, csr); tulip_delay_300ns(sc); } while (0)

static void
tulip_mii_sendbits(
    tulip_softc_t * const sc,
    unsigned data,
    unsigned bits)
{
    unsigned msb = 1 << (bits - 1);
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned lastbit = (csr & MII_DOUT) ? msb : 0;

    csr |= MII_WR; MII_EMIT;  		/* clock low; assert write */

    for (; bits > 0; bits--, data <<= 1) {
	const unsigned thisbit = data & msb;
	if (thisbit != lastbit) {
	    csr ^= MII_DOUT; MII_EMIT;  /* clock low; invert data */
	}
	csr ^= MII_CLKON; MII_EMIT;     /* clock high; data valid */
	lastbit = thisbit;
	csr ^= MII_CLKOFF; MII_EMIT;    /* clock low; data not valid */
    }
}

static void
tulip_mii_turnaround(
    tulip_softc_t * const sc,
    unsigned cmd)
{
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);

    if (cmd == MII_WRCMD) {
	csr |= MII_DOUT; MII_EMIT;	/* clock low; change data */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
	csr ^= MII_DOUT; MII_EMIT;	/* clock low; change data */
    } else {
	csr |= MII_RD; MII_EMIT;	/* clock low; switch to read */
    }
    csr ^= MII_CLKON; MII_EMIT;		/* clock high; data valid */
    csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
}

static unsigned
tulip_mii_readbits(
    tulip_softc_t * const sc)
{
    unsigned data;
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    int idx;

    for (idx = 0, data = 0; idx < 16; idx++) {
	data <<= 1;	/* this is NOOP on the first pass through */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	if (TULIP_CSR_READ(sc, csr_srom_mii) & MII_DIN)
	    data |= 1;
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
    }
    csr ^= MII_RD; MII_EMIT;		/* clock low; turn off read */

    return data;
}

static unsigned
tulip_mii_readreg(
    tulip_softc_t * const sc,
    unsigned devaddr,
    unsigned regno)
{
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned data;

    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    tulip_mii_sendbits(sc, MII_PREAMBLE, 32);
    tulip_mii_sendbits(sc, MII_RDCMD, 8);
    tulip_mii_sendbits(sc, devaddr, 5);
    tulip_mii_sendbits(sc, regno, 5);
    tulip_mii_turnaround(sc, MII_RDCMD);

    data = tulip_mii_readbits(sc);
#ifdef TULIP_DEBUG
    sc->tulip_dbg.dbg_phyregs[regno][0] = data;
    sc->tulip_dbg.dbg_phyregs[regno][1]++;
#endif
    return data;
}

static void
tulip_mii_writereg(
    tulip_softc_t * const sc,
    unsigned devaddr,
    unsigned regno,
    unsigned data)
{
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    tulip_mii_sendbits(sc, MII_PREAMBLE, 32);
    tulip_mii_sendbits(sc, MII_WRCMD, 8);
    tulip_mii_sendbits(sc, devaddr, 5);
    tulip_mii_sendbits(sc, regno, 5);
    tulip_mii_turnaround(sc, MII_WRCMD);
    tulip_mii_sendbits(sc, data, 16);
#ifdef TULIP_DEBUG
    sc->tulip_dbg.dbg_phyregs[regno][2] = data;
    sc->tulip_dbg.dbg_phyregs[regno][3]++;
#endif
}

#define	tulip_mchash(mca)	(tulip_crc32(mca, 6) & 0x1FF)
#define	tulip_srom_crcok(databuf)	( \
    ((tulip_crc32(databuf, 126) & 0xFFFF) ^ 0xFFFF)== \
     ((databuf)[126] | ((databuf)[127] << 8)))

static unsigned
tulip_crc32(
    const unsigned char *databuf,
    size_t datalen)
{
    u_int idx, bit, data, crc = 0xFFFFFFFFUL;

    for (idx = 0; idx < datalen; idx++)
        for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1)
            crc = (crc >> 1) ^ (((crc ^ data) & 1) ? TULIP_CRC32_POLY : 0);
    return crc;
}

static void
tulip_identify_smc_nic(
    tulip_softc_t *sc)
{
    tulip_uint32_t id1, id2, ei;
    int auibnc = 0, utp = 0;
    char *cp;

    if (sc->tulip_chipid == TULIP_DC21041)
	return;
    if (sc->tulip_chipid == TULIP_DC21140) {
	sc->tulip_boardsw = &tulip_dc21140_smc9332_boardsw;
	return;
    }
    id1 = sc->tulip_rombuf[0x60] | (sc->tulip_rombuf[0x61] << 8);
    id2 = sc->tulip_rombuf[0x62] | (sc->tulip_rombuf[0x63] << 8);
    ei  = sc->tulip_rombuf[0x66] | (sc->tulip_rombuf[0x67] << 8);

    strcpy(sc->tulip_boardidbuf, "SMC 8432");
    cp = &sc->tulip_boardidbuf[8];
    if ((id1 & 1) == 0)
	*cp++ = 'B', auibnc = 1;
    if ((id1 & 0xFF) > 0x32)
	*cp++ = 'T', utp = 1;
    if ((id1 & 0x4000) == 0)
	*cp++ = 'A', auibnc = 1;
    if (id2 == 0x15) {
	sc->tulip_boardidbuf[7] = '4';
	*cp++ = '-';
	*cp++ = 'C';
	*cp++ = 'H';
	*cp++ = (ei ? '2' : '1');
    }
    *cp++ = ' ';
    *cp = '\0';
    if (utp && !auibnc)
	sc->tulip_boardsw = &tulip_dc21040_10baset_only_boardsw;
    else if (!utp && auibnc)
	sc->tulip_boardsw = &tulip_dc21040_auibnc_only_boardsw;
}

/*
 * This deals with the vagaries of the address roms and the
 * brain-deadness that various vendors commit in using them.
 */
static int
tulip_read_macaddr(
    tulip_softc_t *sc)
{
    int cksum, rom_cksum, idx;
    tulip_uint32_t csr;
    unsigned char tmpbuf[8];
    static const u_char testpat[] = { 0xFF, 0, 0x55, 0xAA, 0xFF, 0, 0x55, 0xAA };

    if (sc->tulip_chipid == TULIP_DC21040) {
	TULIP_CSR_WRITE(sc, csr_enetrom, 1);
	for (idx = 0; idx < sizeof(sc->tulip_rombuf); idx++) {
	    int cnt = 0;
	    while (((csr = TULIP_CSR_READ(sc, csr_enetrom)) & 0x80000000L) && cnt < 10000)
		cnt++;
	    sc->tulip_rombuf[idx] = csr & 0xFF;
	}
	sc->tulip_boardsw = &tulip_dc21040_boardsw;
#if defined(TULIP_EISA)
    } else if (sc->tulip_chipid == TULIP_DE425) {
	int cnt;
	for (idx = 0, cnt = 0; idx < sizeof(testpat) && cnt < 32; cnt++) {
	    tmpbuf[idx] = TULIP_CSR_READBYTE(sc, csr_enetrom);
	    if (tmpbuf[idx] == testpat[idx])
		++idx;
	    else
		idx = 0;
	}
	for (idx = 0; idx < 32; idx++)
	    sc->tulip_rombuf[idx] = TULIP_CSR_READBYTE(sc, csr_enetrom);
	sc->tulip_boardsw = &tulip_dc21040_boardsw;
#endif /* TULIP_EISA */
    } else {
	int new_srom_fmt = 0;
	/*
	 * Thankfully all DC21041's act the same.
	 * Assume all DC21140 board are compatible with the
	 * DEC 10/100 evaluation board.  Not really valid but
	 * it's the best we can do until every one switches to
	 * the new SROM format.
	 */
	if (sc->tulip_chipid == TULIP_DC21041)
	    sc->tulip_boardsw = &tulip_dc21041_boardsw;
	else
	    sc->tulip_boardsw = &tulip_dc21140_eb_boardsw;
	tulip_read_srom(sc);
	if (tulip_srom_crcok(sc->tulip_rombuf)) {
	    /*
	     * SROM CRC is valid therefore it must be in the
	     * new format.
	     */
	    new_srom_fmt = 1;
	} else if (sc->tulip_rombuf[126] == 0xff && sc->tulip_rombuf[127] == 0xFF) {
	    /*
	     * No checksum is present.  See if the SROM id checks out;
	     * the first 18 bytes should be 0 followed by a 1 followed
	     * by the number of adapters (which we don't deal with yet).
	     */
	    for (idx = 0; idx < 18; idx++) {
		if (sc->tulip_rombuf[idx] != 0)
		    break;
	    }
	    if (idx == 18 && sc->tulip_rombuf[18] == 1 && sc->tulip_rombuf[19] != 0)
		new_srom_fmt = 2;
	}
	if (new_srom_fmt) {
	    int copy_name = 0;
	    /*
	     * New SROM format.  Copy out the Ethernet address.
	     * If it contains a DE500-XA string, then it must be
	     * a DE500-XA.
	     */
	    bcopy(sc->tulip_rombuf + 20, sc->tulip_hwaddr, 6);
	    if (bcmp(sc->tulip_rombuf + 29, "DE500-XA", 8) == 0) {
		sc->tulip_boardsw = &tulip_dc21140_de500xa_boardsw;
		copy_name = 1;
	    } else if (bcmp(sc->tulip_rombuf + 29, "DE500-AA", 8) == 0) {
		sc->tulip_boardsw = &tulip_dc21140_de500aa_boardsw;
		copy_name = 1;
	    } else if (bcmp(sc->tulip_rombuf + 29, "DE450", 5) == 0) {
		copy_name = 1;
	    }
	    if (copy_name) {
		bcopy(sc->tulip_rombuf + 29, sc->tulip_boardidbuf, 8);
		sc->tulip_boardidbuf[8] = ' ';
		sc->tulip_boardid = sc->tulip_boardidbuf;
	    }
	    if (sc->tulip_boardsw == NULL)
		return -6;
	    goto check_oui;
	}
    }


    if (bcmp(&sc->tulip_rombuf[0], &sc->tulip_rombuf[16], 8) != 0) {
	/*
	 * Some folks don't use the standard ethernet rom format
	 * but instead just put the address in the first 6 bytes
	 * of the rom and let the rest be all 0xffs.  (Can we say
	 * ZNYX???) (well sometimes they put in a checksum so we'll
	 * start at 8).
	 */
	for (idx = 8; idx < 32; idx++) {
	    if (sc->tulip_rombuf[idx] != 0xFF)
		return -4;
	}
	/*
	 * Make sure the address is not multicast or locally assigned
	 * that the OUI is not 00-00-00.
	 */
	if ((sc->tulip_rombuf[0] & 3) != 0)
	    return -4;
	if (sc->tulip_rombuf[0] == 0 && sc->tulip_rombuf[1] == 0
		&& sc->tulip_rombuf[2] == 0)
	    return -4;
	bcopy(sc->tulip_rombuf, sc->tulip_hwaddr, 6);
	sc->tulip_flags |= TULIP_ROMOK;
	goto check_oui;
    } else {
	/*
	 * A number of makers of multiport boards (ZNYX and Cogent)
	 * only put on one address ROM on their DC21040 boards.  So
	 * if the ROM is all zeros and this is a DC21040, look at the
	 * previous configured boards (as long as they are on the same
	 * PCI bus and the bus number is non-zero) until we find the
	 * master board with address ROM.  We then use its address ROM
	 * as the base for this board.  (we add our relative board
	 * to the last byte of its address).
	 */
	if (sc->tulip_chipid == TULIP_DC21040 /* && sc->tulip_bus != 0 XXX */) {
	    for (idx = 0; idx < 32; idx++) {
		if (sc->tulip_rombuf[idx] != 0)
		    break;
	    }
	    if (idx == 32) {
		int root_unit;
		tulip_softc_t *root_sc = NULL;
		for (root_unit = sc->tulip_unit - 1; root_unit >= 0; root_unit--) {
		    root_sc = TULIP_UNIT_TO_SOFTC(root_unit);
		    if (root_sc == NULL || (root_sc->tulip_flags & (TULIP_ROMOK|TULIP_SLAVEDROM)) == TULIP_ROMOK)
			break;
		    root_sc = NULL;
		}
		if (root_sc != NULL
			/* && root_sc->tulip_bus == sc->tulip_bus XXX */) {
		    bcopy(root_sc->tulip_hwaddr, sc->tulip_hwaddr, 6);
		    sc->tulip_hwaddr[5] += sc->tulip_unit - root_sc->tulip_unit;
		    sc->tulip_flags |= TULIP_SLAVEDROM;
		    if (root_sc->tulip_boardsw->bd_type == TULIP_DC21040_ZX314_MASTER) {
			sc->tulip_boardsw = &tulip_dc21040_zx314_slave_boardsw;
			/*
			 * Now for a truly disgusting kludge: all 4 DC21040s on
			 * the ZX314 share the same INTA line so the mapping
			 * setup by the BIOS on the PCI bridge is worthless.
			 * Rather than reprogramming the value in the config
			 * register, we will handle this internally.
			 */
			sc->tulip_slaves = root_sc->tulip_slaves;
			root_sc->tulip_slaves = sc;
			sc->tulip_flags |= TULIP_SLAVEDINTR;
		    }
		    return 0;
		}
	    }
	}
    }

    /*
     * This is the standard DEC address ROM test.
     */

    if (bcmp(&sc->tulip_rombuf[24], testpat, 8) != 0)
	return -3;

    tmpbuf[0] = sc->tulip_rombuf[15]; tmpbuf[1] = sc->tulip_rombuf[14];
    tmpbuf[2] = sc->tulip_rombuf[13]; tmpbuf[3] = sc->tulip_rombuf[12];
    tmpbuf[4] = sc->tulip_rombuf[11]; tmpbuf[5] = sc->tulip_rombuf[10];
    tmpbuf[6] = sc->tulip_rombuf[9];  tmpbuf[7] = sc->tulip_rombuf[8];
    if (bcmp(&sc->tulip_rombuf[0], tmpbuf, 8) != 0)
	return -2;

    bcopy(sc->tulip_rombuf, sc->tulip_hwaddr, 6);

    cksum = *(u_int16_t *) &sc->tulip_hwaddr[0];
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_int16_t *) &sc->tulip_hwaddr[2];
    if (cksum > 65535) cksum -= 65535;
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_int16_t *) &sc->tulip_hwaddr[4];
    if (cksum >= 65535) cksum -= 65535;

    rom_cksum = *(u_int16_t *) &sc->tulip_rombuf[6];
	
    if (cksum != rom_cksum)
	return -1;

  check_oui:
    /*
     * Check for various boards based on OUI.  Did I say braindead?
     */
   if (sc->tulip_hwaddr[0] == TULIP_OUI_COGENT_0
	    && sc->tulip_hwaddr[1] == TULIP_OUI_COGENT_1
	    && sc->tulip_hwaddr[2] == TULIP_OUI_COGENT_2) {
	if (sc->tulip_chipid == TULIP_DC21140 || sc->tulip_chipid == TULIP_DC21140A) {
	    if (sc->tulip_rombuf[32] == TULIP_COGENT_EM100_ID)
		sc->tulip_boardsw = &tulip_dc21140_cogent_em100_boardsw;
	}
    } else if (sc->tulip_hwaddr[0] == TULIP_OUI_ZNYX_0
	    && sc->tulip_hwaddr[1] == TULIP_OUI_ZNYX_1
	    && sc->tulip_hwaddr[2] == TULIP_OUI_ZNYX_2) {
	if (sc->tulip_chipid == TULIP_DC21140 || sc->tulip_chipid == TULIP_DC21140A) {
	    /* this at least works for the zx342 from Znyx */
	    sc->tulip_boardsw = &tulip_dc21140_znyx_zx34x_boardsw;
	} else if (sc->tulip_chipid == TULIP_DC21040
	        && (sc->tulip_hwaddr[3] & ~3) == 0xF0
	        && (sc->tulip_hwaddr[5] & 3) == 0) {
	    sc->tulip_boardsw = &tulip_dc21040_zx314_master_boardsw;
	}
    } else if (sc->tulip_hwaddr[0] == TULIP_OUI_SMC_0
	       && sc->tulip_hwaddr[1] == TULIP_OUI_SMC_1
	       && sc->tulip_hwaddr[2] == TULIP_OUI_SMC_2) {
	tulip_identify_smc_nic(sc);
    }

    if (sc->tulip_boardidbuf[0] != '\0')
	sc->tulip_boardid = sc->tulip_boardidbuf;
    else
	sc->tulip_boardid = sc->tulip_boardsw->bd_description;
    sc->tulip_flags |= TULIP_ROMOK;
    return 0;
}

static void
tulip_addr_filter(
    tulip_softc_t * const sc)
{
    tulip_uint32_t *sp = sc->tulip_setupdata;
    struct ether_multistep step;
    struct ether_multi *enm;
    int i = 0;

    sc->tulip_flags &= ~TULIP_WANTHASH;
    sc->tulip_flags |= TULIP_WANTSETUP;
    sc->tulip_cmdmode &= ~TULIP_CMD_RXRUN;
    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
    if (sc->tulip_ac.ac_multicnt > 14) {
	unsigned hash;
	/*
	 * If we have more than 14 multicasts, we have
	 * go into hash perfect mode (512 bit multicast
	 * hash and one perfect hardware).
	 */

	bzero(sc->tulip_setupdata, sizeof(sc->tulip_setupdata));
	hash = tulip_mchash(etherbroadcastaddr);
	sp[hash >> 4] |= 1 << (hash & 0xF);
	ETHER_FIRST_MULTI(step, &sc->tulip_ac, enm);
	while (enm != NULL) {
	    hash = tulip_mchash(enm->enm_addrlo);
	    sp[hash >> 4] |= 1 << (hash & 0xF);
	    ETHER_NEXT_MULTI(step, enm);
	}
	sc->tulip_flags |= TULIP_WANTHASH;
	sp[39] = ((u_int16_t *) sc->tulip_ac.ac_enaddr)[0]; 
	sp[40] = ((u_int16_t *) sc->tulip_ac.ac_enaddr)[1]; 
	sp[41] = ((u_int16_t *) sc->tulip_ac.ac_enaddr)[2];
    } else {
	/*
	 * Else can get perfect filtering for 16 addresses.
	 */
	ETHER_FIRST_MULTI(step, &sc->tulip_ac, enm);
	for (; enm != NULL; i++) {
	    *sp++ = ((u_int16_t *) enm->enm_addrlo)[0]; 
	    *sp++ = ((u_int16_t *) enm->enm_addrlo)[1]; 
	    *sp++ = ((u_int16_t *) enm->enm_addrlo)[2];
	    ETHER_NEXT_MULTI(step, enm);
	}
	/*
	 * Add the broadcast address.
	 */
	i++;
	*sp++ = 0xFFFF;
	*sp++ = 0xFFFF;
	*sp++ = 0xFFFF;
	/*
	 * Pad the rest with our hardware address
	 */
	for (; i < 16; i++) {
	    *sp++ = ((u_int16_t *) sc->tulip_ac.ac_enaddr)[0]; 
	    *sp++ = ((u_int16_t *) sc->tulip_ac.ac_enaddr)[1]; 
	    *sp++ = ((u_int16_t *) sc->tulip_ac.ac_enaddr)[2];
	}
    }
}

static int
tulip_ifioctl(
    struct ifnet * const ifp,
    ioctl_cmd_t cmd,
    caddr_t data)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    s = splimp();

    switch (cmd) {
	case SIOCSIFADDR: 
	case SIOCGIFADDR: 
		ether_ioctl(ifp, cmd, data);
		break;

	case SIOCSIFFLAGS: {
	    /*
	     * Changing the connection forces a reset.
	     */
	    if (sc->tulip_flags & TULIP_ALTPHYS) {
		if ((ifp->if_flags & IFF_ALTPHYS) == 0) {
		    sc->tulip_flags |= TULIP_NEEDRESET;
		}
	    } else {
		if (ifp->if_flags & IFF_ALTPHYS) {
		    sc->tulip_flags |= TULIP_NEEDRESET;
		}
	    }
	    if (sc->tulip_flags & TULIP_NEEDRESET) {
		sc->tulip_media = TULIP_MEDIA_UNKNOWN;
		sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		sc->tulip_flags &= ~(TULIP_TXPROBE_ACTIVE|TULIP_TXPROBE_OK|TULIP_WANTRXACT);
		tulip_reset(sc);
	    }
	    tulip_init(sc);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    if (cmd == SIOCADDMULTI)
		error = ether_addmulti(ifr, &sc->tulip_ac);
	    else
		error = ether_delmulti(ifr, &sc->tulip_ac);

	    if (error == ENETRESET) {
		tulip_addr_filter(sc);		/* reset multicast filtering */
		tulip_init(sc);
		error = 0;
	    }
	    break;
	}
#if defined(SIOCSIFMTU)
#if !defined(ifr_mtu)
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU:
	    /*
	     * Set the interface MTU.
	     */
	    if (ifr->ifr_mtu > ETHERMTU
#ifdef BIG_PACKET
		    && sc->tulip_chipid != TULIP_DC21140
		    && sc->tulip_chipid != TULIP_DC21140A
		    && sc->tulip_chipid != TULIP_DC21041
#endif
		) {
		error = EINVAL;
		break;
	    }
	    ifp->if_mtu = ifr->ifr_mtu;
#ifdef BIG_PACKET
	    tulip_reset(sc);
	    tulip_init(sc);
#endif
	    break;
#endif /* SIOCSIFMTU */

	default: {
	    error = EINVAL;
	    break;
	}
    }

    splx(s);
    return error;
}

static void
tulip_ifwatchdog(
    struct ifnet *ifp)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);

#if defined(TULIP_DEBUG)
    tulip_uint32_t rxintrs = sc->tulip_dbg.dbg_rxintrs - sc->tulip_dbg.dbg_last_rxintrs;
    if (rxintrs > sc->tulip_dbg.dbg_high_rxintrs_hz)
	sc->tulip_dbg.dbg_high_rxintrs_hz = rxintrs;
    sc->tulip_dbg.dbg_last_rxintrs = sc->tulip_dbg.dbg_rxintrs;
    sc->tulip_dbg.dbg_gpintrs_hz = sc->tulip_dbg.dbg_gpintrs;
    sc->tulip_dbg.dbg_gpintrs = 0;
#endif /* TULIP_DEBUG */

    sc->tulip_if.if_timer = 1;
    /*
     * These should be rare so do a bulk test up front so we can just skip
     * them if needed.
     */
    if (sc->tulip_flags & (TULIP_SYSTEMERROR|TULIP_RXBUFSLOW|TULIP_FAKEGPTIMEOUT|TULIP_NOMESSAGES)) {
	/*
	 * This for those devices that need to autosense.  Interrupts are not
	 * allowed during device probe so we fake one here to start the
	 * autosense.  Do this before the others since it can effect their
	 * state.
	 */
	if (sc->tulip_flags & TULIP_FAKEGPTIMEOUT)
	    (*sc->tulip_boardsw->bd_media_select)(sc);

	/*
	 * If the number of receive buffer is low, try to refill
	 */
	if (sc->tulip_flags & TULIP_RXBUFSLOW)
	    tulip_rx_intr(sc);

	if (sc->tulip_flags & TULIP_SYSTEMERROR) {
	    printf(TULIP_PRINTF_FMT ": %d system errors: last was %s\n",
		   TULIP_PRINTF_ARGS, sc->tulip_system_errors,
		   tulip_system_errors[sc->tulip_last_system_error]);
	}
	if (sc->tulip_statusbits) {
	    tulip_print_abnormal_interrupt(sc, sc->tulip_statusbits);
	    sc->tulip_statusbits = 0;
	}

	sc->tulip_flags &= ~(TULIP_NOMESSAGES|TULIP_SYSTEMERROR);
    }

    if (sc->tulip_txtimer && --sc->tulip_txtimer == 0) {
	printf(TULIP_PRINTF_FMT ": transmission timeout\n", TULIP_PRINTF_ARGS);
	sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
	sc->tulip_flags &= ~(TULIP_TXPROBE_ACTIVE|TULIP_TXPROBE_OK|TULIP_WANTRXACT|TULIP_LINKUP|TULIP_LINKSUSPECT);
	tulip_reset(sc);
	tulip_init(sc);
    }
}
#if defined(__bsdi__) || (defined(__FreeBSD__) && BSD < 199506)
static ifnet_ret_t
tulip_ifwatchdog_wrapper(
    int unit)
{
    tulip_ifwatchdog(&TULIP_UNIT_TO_SOFTC(unit)->tulip_if);
}
#define	tulip_ifwatchdog	tulip_ifwatchdog_wrapper
#endif

/*
 * All printf's are real as of now!
 */
#ifdef printf
#undef printf
#endif
#if !defined(IFF_NOTRAILERS)
#define IFF_NOTRAILERS		0
#endif

static void
tulip_attach(
    tulip_softc_t * const sc)
{
    struct ifnet * const ifp = &sc->tulip_if;

    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
    ifp->if_ioctl = tulip_ifioctl;
    ifp->if_start = tulip_ifstart;
    ifp->if_watchdog = tulip_ifwatchdog;
    ifp->if_init = (if_init_f_t*)tulip_init;
    ifp->if_timer = 1;
#if !defined(__bsdi__) || _BSDI_VERSION < 199401
    ifp->if_output = ether_output;
#endif
#if defined(__bsdi__) && _BSDI_VERSION < 199401
    ifp->if_mtu = ETHERMTU;
#endif
  
#if defined(__bsdi__) && _BSDI_VERSION >= 199510
    aprint_naive(": DEC Ethernet");
    aprint_normal(": %s%s", sc->tulip_boardid,
        tulip_chipdescs[sc->tulip_chipid]);
    aprint_verbose(" pass %d.%d", (sc->tulip_revinfo & 0xF0) >> 4,
        sc->tulip_revinfo & 0x0F);
    printf("\n");
    sc->tulip_pf = aprint_normal;
    aprint_normal(TULIP_PRINTF_FMT ": address " TULIP_EADDR_FMT "\n",
		  TULIP_PRINTF_ARGS,
		  TULIP_EADDR_ARGS(sc->tulip_hwaddr));
#else
    printf(
#if defined(__bsdi__)
	   "\n"
#endif
	   TULIP_PRINTF_FMT ": %s%s pass %d.%d\n",
	   TULIP_PRINTF_ARGS,
	   sc->tulip_boardid,
	   tulip_chipdescs[sc->tulip_chipid],
	   (sc->tulip_revinfo & 0xF0) >> 4,
	   sc->tulip_revinfo & 0x0F);
    printf(TULIP_PRINTF_FMT ": address " TULIP_EADDR_FMT "\n",
	   TULIP_PRINTF_ARGS,
	   TULIP_EADDR_ARGS(sc->tulip_hwaddr));
#endif


    if (sc->tulip_boardsw->bd_mii_probe != NULL)
	(*sc->tulip_boardsw->bd_mii_probe)(sc);

    if ((*sc->tulip_boardsw->bd_media_probe)(sc)) {
	ifp->if_flags |= IFF_ALTPHYS;
    } else {
	sc->tulip_flags |= TULIP_ALTPHYS;
    }

    sc->tulip_flags |= TULIP_DEVICEPROBE;
    tulip_reset(sc);
    sc->tulip_flags &= ~TULIP_DEVICEPROBE;

#if defined(__bsdi__) && _BSDI_VERSION >= 199510
    sc->tulip_pf = printf;
    ether_attach(ifp);
#else
    if_attach(ifp);
#if defined(__NetBSD__) || (defined(__FreeBSD__) && BSD >= 199506)
    ether_ifattach(ifp);
#endif
#endif /* __bsdi__ */

#if NBPFILTER > 0
    TULIP_BPF_ATTACH(sc);
#endif
}

static void
tulip_initcsrs(
    tulip_softc_t * const sc,
    tulip_csrptr_t csr_base,
    size_t csr_size)
{
    sc->tulip_csrs.csr_busmode		= csr_base +  0 * csr_size;
    sc->tulip_csrs.csr_txpoll		= csr_base +  1 * csr_size;
    sc->tulip_csrs.csr_rxpoll		= csr_base +  2 * csr_size;
    sc->tulip_csrs.csr_rxlist		= csr_base +  3 * csr_size;
    sc->tulip_csrs.csr_txlist		= csr_base +  4 * csr_size;
    sc->tulip_csrs.csr_status		= csr_base +  5 * csr_size;
    sc->tulip_csrs.csr_command		= csr_base +  6 * csr_size;
    sc->tulip_csrs.csr_intr		= csr_base +  7 * csr_size;
    sc->tulip_csrs.csr_missed_frames	= csr_base +  8 * csr_size;
    if (sc->tulip_chipid == TULIP_DC21040) {
	sc->tulip_csrs.csr_enetrom		= csr_base +  9 * csr_size;
	sc->tulip_csrs.csr_reserved		= csr_base + 10 * csr_size;
	sc->tulip_csrs.csr_full_duplex		= csr_base + 11 * csr_size;
	sc->tulip_csrs.csr_sia_status		= csr_base + 12 * csr_size;
	sc->tulip_csrs.csr_sia_connectivity	= csr_base + 13 * csr_size;
	sc->tulip_csrs.csr_sia_tx_rx 		= csr_base + 14 * csr_size;
	sc->tulip_csrs.csr_sia_general		= csr_base + 15 * csr_size;
#if defined(TULIP_EISA)
    } else if (sc->tulip_chipid == TULIP_DE425) {
	sc->tulip_csrs.csr_enetrom		= csr_base + DE425_ENETROM_OFFSET;
	sc->tulip_csrs.csr_reserved		= csr_base + 10 * csr_size;
	sc->tulip_csrs.csr_full_duplex		= csr_base + 11 * csr_size;
	sc->tulip_csrs.csr_sia_status		= csr_base + 12 * csr_size;
	sc->tulip_csrs.csr_sia_connectivity	= csr_base + 13 * csr_size;
	sc->tulip_csrs.csr_sia_tx_rx 		= csr_base + 14 * csr_size;
	sc->tulip_csrs.csr_sia_general		= csr_base + 15 * csr_size;
#endif /* TULIP_EISA */
    } else if (sc->tulip_chipid == TULIP_DC21140 || sc->tulip_chipid == TULIP_DC21140A) {
	sc->tulip_csrs.csr_srom_mii		= csr_base +  9 * csr_size;
	sc->tulip_csrs.csr_gp_timer		= csr_base + 11 * csr_size;
	sc->tulip_csrs.csr_gp			= csr_base + 12 * csr_size;
	sc->tulip_csrs.csr_watchdog		= csr_base + 15 * csr_size;
    } else if (sc->tulip_chipid == TULIP_DC21041) {
	sc->tulip_csrs.csr_srom_mii		= csr_base +  9 * csr_size;
	sc->tulip_csrs.csr_bootrom		= csr_base + 10 * csr_size;
	sc->tulip_csrs.csr_gp_timer		= csr_base + 11 * csr_size;
	sc->tulip_csrs.csr_sia_status		= csr_base + 12 * csr_size;
	sc->tulip_csrs.csr_sia_connectivity	= csr_base + 13 * csr_size;
	sc->tulip_csrs.csr_sia_tx_rx 		= csr_base + 14 * csr_size;
	sc->tulip_csrs.csr_sia_general		= csr_base + 15 * csr_size;
    }
}

static void
tulip_initring(
    tulip_softc_t * const sc,
    tulip_ringinfo_t * const ri,
    tulip_desc_t *descs,
    int ndescs)
{
    ri->ri_max = ndescs;
    ri->ri_first = descs;
    ri->ri_last = ri->ri_first + ri->ri_max;
    bzero((caddr_t) ri->ri_first, sizeof(ri->ri_first[0]) * ri->ri_max);
    ri->ri_last[-1].d_flag = TULIP_DFLAG_ENDRING;
}

/*
 * This is the PCI configuration support.  Since the DC21040 is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * DC21040 in the config file.
 */

#define	PCI_CFID	0x00	/* Configuration ID */
#define	PCI_CFCS	0x04	/* Configurtion Command/Status */
#define	PCI_CFRV	0x08	/* Configuration Revision */
#define	PCI_CFLT	0x0c	/* Configuration Latency Timer */
#define	PCI_CBIO	0x10	/* Configuration Base IO Address */
#define	PCI_CBMA	0x14	/* Configuration Base Memory Address */
#define	PCI_CFIT	0x3c	/* Configuration Interrupt */
#define	PCI_CFDA	0x40	/* Configuration Driver Area */

#if defined(TULIP_EISA)
static const int tulip_eisa_irqs[4] = { IRQ5, IRQ9, IRQ10, IRQ11 };
#endif

#if defined(__FreeBSD__)

#define	TULIP_PCI_ATTACH_ARGS	pcici_t config_id, int unit

static int
tulip_pci_shutdown(
    int unit,
    int force)
{
    tulip_softc_t * const sc = TULIP_UNIT_TO_SOFTC(unit);
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		       33MHz that comes to two microseconds but wait a
		       bit longer anyways) */
    return 0;
}

static char*
tulip_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
    if (PCI_VENDORID(device_id) != DEC_VENDORID)
	return NULL;
    if (PCI_CHIPID(device_id) == DC21040_CHIPID)
	return "Digital DC21040 Ethernet";
    if (PCI_CHIPID(device_id) == DC21041_CHIPID)
	return "Digital DC21041 Ethernet";
    if (PCI_CHIPID(device_id) == DC21140_CHIPID) {
	tulip_uint32_t revinfo = pci_conf_read(config_id, PCI_CFRV) & 0xFF;
	if (revinfo >= 0x20)
	    return "Digital DC21140A Fast Ethernet";
	else
	    return "Digital DC21140 Fast Ethernet";

    }
    return NULL;
}

static void  tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);
static u_long tulip_pci_count;

struct pci_device dedevice = {
    "de",
    tulip_pci_probe,
    tulip_pci_attach,
   &tulip_pci_count,
    tulip_pci_shutdown,
};

DATA_SET (pcidevice_set, dedevice);
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#define	TULIP_PCI_ATTACH_ARGS	struct device * const parent, struct device * const self, void * const aux

static void
tulip_shutdown(
    void *arg)
{
    tulip_softc_t * const sc = (tulip_softc_t *) arg;
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
			   33MHz that comes to two microseconds but wait a
			   bit longer anyways) */
}

static int
tulip_pci_match(
    pci_devaddr_t *pa)
{
    int irq;
    unsigned id;

    id = pci_inl(pa, PCI_VENDOR_ID);
    if (PCI_VENDORID(id) != DEC_VENDORID)
	return 0;
    id = PCI_CHIPID(id);
    if (id != DC21040_CHIPID && id != DC21041_CHIPID && id != DC21140_CHIPID)
	return 0;
    irq = pci_inl(pa, PCI_I_LINE) & 0xFF;
    if (irq == 0 || irq >= 16) {
	printf("de?: invalid IRQ %d; skipping\n", irq);
	return 0;
    }
    return 1;
}

static int
tulip_probe(
    struct device *parent,
    struct cfdata *cf,
    void *aux)
{
    struct isa_attach_args * const ia = (struct isa_attach_args *) aux;
    unsigned irq, slot;
    pci_devaddr_t *pa;

#if _BSDI_VERSION >= 199401
    switch (ia->ia_bustype) {
    case BUS_PCI:
#endif
	pa = pci_scan(tulip_pci_match);
	if (pa == NULL)
	    return 0;

	irq = (1 << (pci_inl(pa, PCI_I_LINE) & 0xFF));

	/* Get the base address; assume the BIOS set it up correctly */
#if defined(TULIP_IOMAPPED)
	ia->ia_maddr = NULL;
	ia->ia_msize = 0;
	ia->ia_iobase = pci_inl(pa, PCI_CBIO) & ~7;
	pci_outl(pa, PCI_CBIO, 0xFFFFFFFF);
	ia->ia_iosize = ((~pci_inl(pa, PCI_CBIO)) | 7) + 1;
	pci_outl(pa, PCI_CBIO, (int) ia->ia_iobase);

	/* Disable memory space access */
	pci_outl(pa, PCI_COMMAND, pci_inl(pa, PCI_COMMAND) & ~2);
#else
	ia->ia_maddr = (caddr_t) (pci_inl(pa, PCI_CBMA) & ~7);
	pci_outl(pa, PCI_CBMA, 0xFFFFFFFF);
	ia->ia_msize = ((~pci_inl(pa, PCI_CBMA)) | 7) + 1;
	pci_outl(pa, PCI_CBMA, (int) ia->ia_maddr);
	ia->ia_iobase = 0;
	ia->ia_iosize = 0;

	/* Disable I/O space access */
	pci_outl(pa, PCI_COMMAND, pci_inl(pa, PCI_COMMAND) & ~1);
#endif /* TULIP_IOMAPPED */

	ia->ia_aux = (void *) pa;
#if _BSDI_VERSION >= 199401
	break;

#if defined(TULIP_EISA)
    case BUS_EISA: {
	unsigned tmp;

	if ((slot = eisa_match(cf, ia)) == 0)
	    return 0;
	ia->ia_iobase = slot << 12;
	ia->ia_iosize = EISA_NPORT;
	eisa_slotalloc(slot);
	tmp = inb(ia->ia_iobase + DE425_CFG0);
	irq = tulip_eisa_irqs[(tmp >> 1) & 0x03];
	/*
	 * Until BSD/OS likes level interrupts, force
	 * the DE425 into edge-triggered mode.
	 */
	if ((tmp & 1) == 0)
	    outb(ia->ia_iobase + DE425_CFG0, tmp | 1);
	/*
	 * CBIO needs to map to the EISA slot
	 * enable I/O access and Master
	 */
	outl(ia->ia_iobase + DE425_CBIO, ia->ia_iobase);
	outl(ia->ia_iobase + DE425_CFCS, 5 | inl(ia->ia_iobase + DE425_CFCS));
	ia->ia_aux = NULL;
	break;
    }
#endif /* TULIP_EISA */
    default:
	return 0;
    }
#endif

    /* PCI bus masters don't use host DMA channels */
    ia->ia_drq = DRQNONE;

    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("de%d: error: desired IRQ of %d does not match device's "
	    "actual IRQ of %d,\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK)
	ia->ia_irq = irq;
#ifdef IRQSHARE
    ia->ia_irq |= IRQSHARE;
#endif
    return 1;
}

static void tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);

#if defined(TULIP_EISA)
static char *tulip_eisa_ids[] = {
    "DEC4250",
    NULL
};
#endif

struct cfdriver decd = {
    0, "de", tulip_probe, tulip_pci_attach,
#if _BSDI_VERSION >= 199401
    DV_IFNET,
#endif
    sizeof(tulip_softc_t),
#if defined(TULIP_EISA)
    tulip_eisa_ids
#endif
};

#endif /* __bsdi__ */

#if defined(__NetBSD__)
#define	TULIP_PCI_ATTACH_ARGS	struct device * const parent, struct device * const self, void * const aux

static void
tulip_pci_shutdown(
    void *arg)
{
    tulip_softc_t * const sc = (tulip_softc_t *) arg;
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
			   33MHz that comes to two microseconds but wait a
			   bit longer anyways) */
}

static int
tulip_pci_probe(
    struct device *parent,
    void *match,
    void *aux)
{
    struct pci_attach_args *pa = (struct pci_attach_args *) aux;

    if (PCI_VENDORID(pa->pa_id) != DEC_VENDORID)
	return 0;
    if (PCI_CHIPID(pa->pa_id) == DC21040_CHIPID
	    || PCI_CHIPID(pa->pa_id) == DC21041_CHIPID
	    || PCI_CHIPID(pa->pa_id) == DC21140_CHIPID)
	return 1;

    return 0;
}

static void tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);

struct cfattach de_ca = {
    sizeof(tulip_softc_t), tulip_pci_probe, tulip_pci_attach
};

struct cfdriver de_cd = {
    0, "de", DV_IFNET
};

#endif /* __NetBSD__ */

static void
tulip_pci_attach(
    TULIP_PCI_ATTACH_ARGS)
{
#if defined(__FreeBSD__)
    tulip_softc_t *sc;
#define	PCI_CONF_WRITE(r, v)	pci_conf_write(config_id, (r), (v))
#define	PCI_CONF_READ(r)	pci_conf_read(config_id, (r))
#endif
#if defined(__bsdi__)
    tulip_softc_t * const sc = (tulip_softc_t *) self;
    struct isa_attach_args * const ia = (struct isa_attach_args *) aux;
    pci_devaddr_t *pa = (pci_devaddr_t *) ia->ia_aux;
    const int unit = sc->tulip_dev.dv_unit;
#define	PCI_CONF_WRITE(r, v)	pci_outl(pa, (r), (v))
#define	PCI_CONF_READ(r)	pci_inl(pa, (r))
#endif
#if defined(__NetBSD__)
    tulip_softc_t * const sc = (tulip_softc_t *) self;
    struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
    const int unit = sc->tulip_dev.dv_unit;
#if defined(TULIP_IOMAPPED)
    bus_io_addr_t iobase;
    bus_io_size_t iosize;
#else
    bus_mem_addr_t membase;
    bus_mem_size_t memsize;
#endif
#define	PCI_CONF_WRITE(r, v)	pci_conf_write(pa->pa_pc, pa->pa_tag, (r), (v))
#define	PCI_CONF_READ(r)	pci_conf_read(pa->pa_pc, pa->pa_tag, (r))
#endif /* __NetBSD__ */
    int retval, idx;
    tulip_uint32_t revinfo, cfdainfo, id;
#if !defined(TULIP_IOMAPPED) && defined(__FreeBSD__)
    vm_offset_t pa_csrs;
#endif
    unsigned csroffset = TULIP_PCI_CSROFFSET;
    unsigned csrsize = TULIP_PCI_CSRSIZE;
    tulip_csrptr_t csr_base;
    tulip_chipid_t chipid = TULIP_CHIPID_UNKNOWN;

#if defined(__FreeBSD__)
    if (unit >= tulip_count) {
	tulip_softc_t **new_tulips =
	    (tulip_softc_t **) malloc((tulip_count + TULIP_COUNTINCR) * sizeof(tulip_softc_t *), M_DEVBUF, M_WAITOK);
	if (new_tulips == NULL) {
	    printf("de%d: not configured; can't allocate memory\n", unit);
	    return;
	}
	if (tulips != NULL) {
	    bcopy(tulips, new_tulips, tulip_count * sizeof(tulips[0]));
	    free(tulips, M_DEVBUF);
	}
	bzero(&new_tulips[tulip_count], TULIP_COUNTINCR * sizeof(new_tulips[0]));
	tulip_count += TULIP_COUNTINCR;
	tulips = new_tulips;
    }
#endif

#if defined(__bsdi__)
    if (pa != NULL) {
	revinfo = pci_inl(pa, PCI_CFRV) & 0xFF;
	id = pci_inl(pa, PCI_CFID);
	cfdainfo = pci_inl(pa, PCI_CFDA);
#if defined(TULIP_EISA)
    } else {
	revinfo = inl(ia->ia_iobase + DE425_CFRV) & 0xFF;
	csroffset = TULIP_EISA_CSROFFSET;
	csrsize = TULIP_EISA_CSRSIZE;
	chipid = TULIP_DE425;
	cfdainfo = 0;
#endif
    }
#else /* __bsdi__ */
    revinfo  = PCI_CONF_READ(PCI_CFRV) & 0xFF;
    id       = PCI_CONF_READ(PCI_CFID);
    cfdainfo = PCI_CONF_READ(PCI_CFDA);
#endif

    if (PCI_VENDORID(id) == DEC_VENDORID) {
	if (PCI_CHIPID(id) == DC21040_CHIPID) chipid = TULIP_DC21040;
	else if (PCI_CHIPID(id) == DC21140_CHIPID) {
	    chipid = (revinfo >= 0x20) ? TULIP_DC21140A : TULIP_DC21140;
	}
	else if (PCI_CHIPID(id) == DC21041_CHIPID) chipid = TULIP_DC21041;
    }
    if (chipid == TULIP_CHIPID_UNKNOWN)
	return;

    if ((chipid == TULIP_DC21040 || chipid == TULIP_DE425) && revinfo < 0x20) {
#ifdef __FreeBSD__
	printf("de%d", unit);
#endif
	printf(": not configured; DC21040 pass 2.0 required (%d.%d found)\n",
	       revinfo >> 4, revinfo & 0x0f);
	return;
    } else if (chipid == TULIP_DC21140 && revinfo < 0x11) {
#ifndef __FreeBSD__
	printf("\n");
#endif
	printf("de%d: not configured; DC21140 pass 1.1 required (%d.%d found)\n",
	       unit, revinfo >> 4, revinfo & 0x0f);
	return;
    }

    if ((chipid == TULIP_DC21041 || chipid == TULIP_DC21140A)
	&& (cfdainfo & (TULIP_CFDA_SLEEP|TULIP_CFDA_SNOOZE))) {
	cfdainfo &= ~(TULIP_CFDA_SLEEP|TULIP_CFDA_SNOOZE);
	PCI_CONF_WRITE(PCI_CFDA, cfdainfo);
	printf("de%d: waking device from sleep/snooze mode\n", unit);
	DELAY(11*1000);
    }


#if defined(__FreeBSD__)
    sc = (tulip_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
	return;
    bzero(sc, sizeof(*sc));				/* Zero out the softc*/
#endif

    sc->tulip_chipid = chipid;
#if defined(__NetBSD__)
    bcopy(self->dv_xname, sc->tulip_if.if_xname, IFNAMSIZ);
    sc->tulip_if.if_softc = sc;
    sc->tulip_bc = pa->pa_bc;
    sc->tulip_pc = pa->pa_pc;
#else
    sc->tulip_unit = unit;
    sc->tulip_name = "de";
#endif
    sc->tulip_revinfo = revinfo;
#if defined(__FreeBSD__)
#if BSD >= 199506
    sc->tulip_if.if_softc = sc;
#endif
#if defined(TULIP_IOMAPPED)
    retval = pci_map_port(config_id, PCI_CBIO, &csr_base);
#else
    retval = pci_map_mem(config_id, PCI_CBMA, (vm_offset_t *) &csr_base, &pa_csrs);
#endif
    if (!retval) {
	free((caddr_t) sc, M_DEVBUF);
	return;
    }
    tulips[unit] = sc;
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#if defined(TULIP_IOMAPPED)
    csr_base = ia->ia_iobase;
#else
    csr_base = (vm_offset_t) mapphys((vm_offset_t) ia->ia_maddr, ia->ia_msize);
#endif
#endif /* __bsdi__ */

#if defined(__NetBSD__)
    csr_base = 0;
#if defined(TULIP_IOMAPPED)
    if (pci_io_find(pa->pa_pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)
	|| bus_io_map(pa->pa_bc, iobase, iosize, &sc->tulip_ioh))
	return;
#else
    if (pci_mem_find(pa->pa_pc, pa->pa_tag, PCI_CBMA, &membase, &memsize, NULL)
	|| bus_mem_map(pa->pa_bc, membase, memsize, 0, &sc->tulip_memh))
	return;
#endif
#endif /* __NetBSD__ */

    tulip_initcsrs(sc, csr_base + csroffset, csrsize);
    tulip_initring(sc, &sc->tulip_rxinfo, sc->tulip_rxdescs, TULIP_RXDESCS);
    tulip_initring(sc, &sc->tulip_txinfo, sc->tulip_txdescs, TULIP_TXDESCS);
    if ((retval = tulip_read_macaddr(sc)) < 0) {
#ifdef __FreeBSD__
	printf(TULIP_PRINTF_FMT, TULIP_PRINTF_ARGS);
#endif
	printf(": can't read ENET ROM (why=%d) (", retval);
	for (idx = 0; idx < 32; idx++)
	    printf("%02x", sc->tulip_rombuf[idx]);
	printf("\n");
	printf(TULIP_PRINTF_FMT ": %s%s pass %d.%d\n",
	       TULIP_PRINTF_ARGS,
	       (sc->tulip_boardid != NULL ? sc->tulip_boardid : ""),
	       tulip_chipdescs[sc->tulip_chipid],
	       (sc->tulip_revinfo & 0xF0) >> 4, sc->tulip_revinfo & 0x0F);
	printf(TULIP_PRINTF_FMT ": address unknown\n", TULIP_PRINTF_ARGS);
    } else {
	int s;
	/*
	 * Make sure there won't be any interrupts or such...
	 */
	TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
			   33MHz that comes to two microseconds but wait a
			   bit longer anyways) */
#if defined(__NetBSD__)
	if ((sc->tulip_flags & TULIP_SLAVEDINTR) == 0) {
	    pci_intr_handle_t intrhandle;
	    const char *intrstr;

	    if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			     pa->pa_intrline, &intrhandle)) {
		printf(": couldn't map interrupt\n");
		return;
	    }
	    intrstr = pci_intr_string(pa->pa_pc, intrhandle);
	    sc->tulip_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
					     tulip_intr, sc);
	    if (sc->tulip_ih == NULL)
		printf(": couldn't establish interrupt");
	    if (intrstr != NULL)
		printf(" at %s", intrstr);
	    printf("\n");
	    if (sc->tulip_ih == NULL)
		return;
	}
	sc->tulip_ats = shutdownhook_establish(tulip_pci_shutdown, sc);
	if (sc->tulip_ats == NULL)
	    printf("\n%s: warning: couldn't establish shutdown hook\n",
		   sc->tulip_xname);
#endif
#if defined(__FreeBSD__)
	if ((sc->tulip_flags & TULIP_SLAVEDINTR) == 0) {
	    if (!pci_map_int (config_id, tulip_intr, (void*) sc, &net_imask)) {
		printf(TULIP_PRINTF_FMT ": couldn't map interrupt\n",
		       TULIP_PRINTF_ARGS);
		return;
	    }
	}
#endif
#if defined(__bsdi__)
	if ((sc->tulip_flags & TULIP_SLAVEDINTR) == 0) {
	    isa_establish(&sc->tulip_id, &sc->tulip_dev);

	    sc->tulip_ih.ih_fun = tulip_intr;
	    sc->tulip_ih.ih_arg = (void *)sc;
	    intr_establish(ia->ia_irq, &sc->tulip_ih, DV_NET);
	}

	sc->tulip_ats.func = tulip_shutdown;
	sc->tulip_ats.arg = (void *) sc;
	atshutdown(&sc->tulip_ats, ATSH_ADD);
#endif
	s = splimp();
	tulip_reset(sc);
	tulip_attach(sc);
	splx(s);
    }
}

