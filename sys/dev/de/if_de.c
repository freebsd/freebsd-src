/*-
 * Copyright (c) 1994, 1995 Matt Thomas (matt@lkg.dec.com)
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
 * $Id: if_de.c,v 1.45 1996/05/02 14:20:44 phk Exp $
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

#if defined(__FreeBSD__)
#include "de.h"
#endif
#if NDE > 0 || !defined(__FreeBSD__)

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
#include <sys/devconf.h>
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
#include <vm/pmap.h>
#include <vm/vm_kern.h>

#if defined(__FreeBSD__)
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
#include <eisa.h>
#if NEISA > 0
#include <i386/eisa/eisa.h>
#define	TULIP_EISA
#endif
#endif /* __bsdi__ */

#if defined(__NetBSD__)
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#if defined(__i386__)
#include <i386/isa/isa_machdep.h>
#endif
#endif /* __NetBSD__ */

/*
 * Intel CPUs should use I/O mapped access.
 *    (NetBSD doesn't support it yet)
 */
#if defined(__i386__) && !defined(__NetBSD__)
#define	TULIP_IOMAPPED
#endif

/*
 * This module supports
 *	the DEC DC21040 PCI Ethernet Controller.
 *	the DEC DC21041 PCI Ethernet Controller.
 *	the DEC DC21140 PCI Fast Ethernet Controller.
 */

typedef struct {
    tulip_desc_t *ri_first;
    tulip_desc_t *ri_last;
    tulip_desc_t *ri_nextin;
    tulip_desc_t *ri_nextout;
    int ri_max;
    int ri_free;
} tulip_ringinfo_t;

#ifdef TULIP_IOMAPPED
typedef tulip_uint16_t tulip_csrptr_t;

#define	TULIP_EISA_CSRSIZE	16
#define	TULIP_EISA_CSROFFSET	0
#define	TULIP_PCI_CSRSIZE	8
#define	TULIP_PCI_CSROFFSET	0

#define	TULIP_READ_CSR(sc, csr)			(inl((sc)->tulip_csrs.csr))
#define	TULIP_WRITE_CSR(sc, csr, val)   	outl((sc)->tulip_csrs.csr, val)

#define	TULIP_READ_CSRBYTE(sc, csr)		(inb((sc)->tulip_csrs.csr))
#define	TULIP_WRITE_CSRBYTE(sc, csr, val)	outb((sc)->tulip_csrs.csr, val)

#else /* TULIP_IOMAPPED */

typedef volatile tulip_uint32_t *tulip_csrptr_t;

#if defined(__alpha__)
#define	TULIP_PCI_CSRSIZE	(256 / sizeof(tulip_uint32_t))
#define	TULIP_PCI_CSROFFSET	(24 / sizeof(tulip_uint32_t))
#elif defined(__i386__)
#define	TULIP_PCI_CSRSIZE	(8 / sizeof(tulip_uint32_t))
#define	TULIP_PCI_CSROFFSET	0
#endif

/*
 * macros to read and write CSRs.  Note that the "0 +" in
 * READ_CSR is to prevent the macro from being an lvalue
 * and WRITE_CSR shouldn't be assigned from.
 */
#define	TULIP_READ_CSR(sc, csr)		(0 + *(sc)->tulip_csrs.csr)
#ifndef __alpha__
#define	TULIP_WRITE_CSR(sc, csr, val) \
	    ((void)(*(sc)->tulip_csrs.csr = (val)))
#else
#define	TULIP_WRITE_CSR(sc, csr, val) \
	    ((void)(*(sc)->tulip_csrs.csr = (val), MB()))
#endif

#endif /* TULIP_IOMAPPED */

typedef struct {
    tulip_csrptr_t csr_busmode;			/* CSR0 */
    tulip_csrptr_t csr_txpoll;			/* CSR1 */
    tulip_csrptr_t csr_rxpoll;			/* CSR2 */
    tulip_csrptr_t csr_rxlist;			/* CSR3 */
    tulip_csrptr_t csr_txlist;			/* CSR4 */
    tulip_csrptr_t csr_status;			/* CSR5 */
    tulip_csrptr_t csr_command;			/* CSR6 */
    tulip_csrptr_t csr_intr;			/* CSR7 */
    tulip_csrptr_t csr_missed_frame;		/* CSR8 */

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
 * but we gone to directly DMA'ing into MBUFs because with 100Mb
 * cards the copying is just too much of a hit.
 */
#if defined(__alpha__)
#define	TULIP_COPY_RXDATA	1
#endif

#define	TULIP_RXDESCS		16
#define	TULIP_TXDESCS		128
#define	TULIP_RXQ_TARGET	8

typedef enum {
    TULIP_DC21040_GENERIC,
    TULIP_DC21040_ZX314_MASTER,
    TULIP_DC21040_ZX314_SLAVE,
    TULIP_DC21140_DEC_EB,
    TULIP_DC21140_DEC_DE500,
    TULIP_DC21140_COGENT_EM100,
    TULIP_DC21140_ZNYX_ZX34X,
    TULIP_DC21041_GENERIC,
    TULIP_DC21041_DE450
} tulip_board_t;

typedef struct _tulip_softc_t tulip_softc_t;

typedef struct {
    tulip_board_t bd_type;
    const char *bd_description;
    int (*bd_media_probe)(tulip_softc_t *sc);
    void (*bd_media_select)(tulip_softc_t *sc);
} tulip_boardsw_t;

typedef enum {
    TULIP_DC21040, TULIP_DC21140,
    TULIP_DC21041, TULIP_DE425,
    TULIP_CHIPID_UNKNOWN
} tulip_chipid_t;

typedef enum {
    TULIP_PROBE_INACTIVE, TULIP_PROBE_10BASET, TULIP_PROBE_AUI,
    TULIP_PROBE_BNC
} tulip_probe_state_t;

typedef enum {
    TULIP_MEDIA_UNKNOWN, TULIP_MEDIA_10BASET,
    TULIP_MEDIA_BNC, TULIP_MEDIA_AUI,
    TULIP_MEDIA_BNCAUI, TULIP_MEDIA_100BASET
} tulip_media_t;

struct _tulip_softc_t {
#if defined(__bsdi__)
    struct device tulip_dev;		/* base device */
    struct isadev tulip_id;		/* ISA device */
    struct intrhand tulip_ih;		/* intrrupt vectoring */
    struct atshutdown tulip_ats;	/* shutdown hook */
#endif
#if defined(__NetBSD__)
    struct device tulip_dev;		/* base device */
    void *tulip_ih;			/* intrrupt vectoring */
    void *tulip_ats;			/* shutdown hook */
#endif
    struct arpcom tulip_ac;
    tulip_regfile_t tulip_csrs;
    unsigned tulip_flags;
#define	TULIP_WANTSETUP		0x0001
#define	TULIP_WANTHASH		0x0002
#define	TULIP_DOINGSETUP	0x0004
#define	TULIP_ALTPHYS		0x0008	/* use AUI */
#define	TULIP_TXPROBE_ACTIVE	0x0010
#define	TULIP_TXPROBE_OK	0x0020
#define	TULIP_INRESET		0x0040
#define	TULIP_WANTRXACT		0x0080
#define	TULIP_SLAVEDROM		0x0100
#define	TULIP_ROMOK		0x0200
    unsigned char tulip_rombuf[128];
    tulip_uint32_t tulip_setupbuf[192/sizeof(tulip_uint32_t)];
    tulip_uint32_t tulip_setupdata[192/sizeof(tulip_uint32_t)];
    tulip_uint32_t tulip_intrmask;
    tulip_uint32_t tulip_cmdmode;
    tulip_uint32_t tulip_revinfo;
    tulip_uint32_t tulip_gpticks;
    /* tulip_uint32_t tulip_bus; XXX */
    tulip_media_t tulip_media;
    tulip_probe_state_t tulip_probe_state;
    tulip_chipid_t tulip_chipid;
    const tulip_boardsw_t *tulip_boardsw;
    tulip_softc_t *tulip_slaves;
    struct ifqueue tulip_txq;
    struct ifqueue tulip_rxq;
    tulip_ringinfo_t tulip_rxinfo;
    tulip_ringinfo_t tulip_txinfo;
};

#ifndef IFF_ALTPHYS
#define	IFF_ALTPHYS	IFF_LINK0		/* In case it isn't defined */
#endif
static const char *tulip_chipdescs[] = { 
    "DC21040 [10Mb/s]",
    "DC21140 [10-100Mb/s]",
    "DC21041 [10Mb/s]",
#if defined(TULIP_EISA)
    "DE425 [10Mb/s]"
#endif
};

#if defined(__FreeBSD__)
typedef void ifnet_ret_t;
typedef int ioctl_cmd_t;
static tulip_softc_t *tulips[NDE];
#define	TULIP_UNIT_TO_SOFTC(unit)	(tulips[unit])
#endif
#if defined(__bsdi__)
typedef int ifnet_ret_t;
typedef int ioctl_cmd_t;
extern struct cfdriver decd;
#define	TULIP_UNIT_TO_SOFTC(unit)	((tulip_softc_t *) decd.cd_devs[unit])
#define	TULIP_BURSTSIZE(unit)		3
#endif
#if defined(__NetBSD__)
typedef void ifnet_ret_t;
typedef u_long ioctl_cmd_t;
extern struct cfdriver decd;
#define	TULIP_UNIT_TO_SOFTC(unit)	((tulip_softc_t *) decd.cd_devs[unit])
#endif

#ifndef TULIP_BURSTSIZE
#define	TULIP_BURSTSIZE(unit)		3
#endif

#define	tulip_if	tulip_ac.ac_if
#define	tulip_unit	tulip_ac.ac_if.if_unit
#define	tulip_name	tulip_ac.ac_if.if_name
#define	tulip_bpf	tulip_ac.ac_if.if_bpf
#define	tulip_hwaddr	tulip_ac.ac_enaddr

#define	TULIP_CRC32_POLY	0xEDB88320UL	/* CRC-32 Poly -- Little Endian */
#define	TULIP_CHECK_RXCRC	0
#define	TULIP_MAX_TXSEG		30

#define	TULIP_ADDREQUAL(a1, a2) \
	(((u_short *)a1)[0] == ((u_short *)a2)[0] \
	 && ((u_short *)a1)[1] == ((u_short *)a2)[1] \
	 && ((u_short *)a1)[2] == ((u_short *)a2)[2])
#define	TULIP_ADDRBRDCST(a1) \
	(((u_short *)a1)[0] == 0xFFFFU \
	 && ((u_short *)a1)[1] == 0xFFFFU \
	 && ((u_short *)a1)[2] == 0xFFFFU)

static ifnet_ret_t tulip_start(struct ifnet *ifp);
static void tulip_rx_intr(tulip_softc_t *sc);
static void tulip_addr_filter(tulip_softc_t *sc);


static int
tulip_dc21040_media_probe(
    tulip_softc_t * const sc)
{
    int cnt;

    TULIP_WRITE_CSR(sc, csr_sia_connectivity, 0);
    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    for (cnt = 0; cnt < 2400; cnt++) {
	if ((TULIP_READ_CSR(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) == 0)
	    break;
	DELAY(1000);
    }
    return (TULIP_READ_CSR(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) != 0;
}

static void
tulip_dc21040_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling Thinwire/AUI port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_AUI);
	sc->tulip_flags |= TULIP_ALTPHYS;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT/UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
	sc->tulip_flags &= ~TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_10BASET;
    }
}

static const tulip_boardsw_t tulip_dc21040_boardsw = {
    TULIP_DC21040_GENERIC,
    "",
    tulip_dc21040_media_probe,
    tulip_dc21040_media_select
};

static int
tulip_zx314_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_WRITE_CSR(sc, csr_sia_connectivity, 0);
    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    return 0;
}

static void
tulip_zx314_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
    if (sc->tulip_flags & TULIP_ALTPHYS)
	printf("%s%d: enabling 10baseT/UTP port\n",
	       sc->tulip_if.if_name, sc->tulip_if.if_unit);
    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_10BASET);
    sc->tulip_flags &= ~TULIP_ALTPHYS;
    sc->tulip_media = TULIP_MEDIA_10BASET;
}


static const tulip_boardsw_t tulip_dc21040_zx314_master_boardsw = {
    TULIP_DC21040_ZX314_MASTER,
    "ZNYX ZX314 ",
    tulip_zx314_media_probe,
    tulip_zx314_media_select
};

static const tulip_boardsw_t tulip_dc21040_zx314_slave_boardsw = {
    TULIP_DC21040_ZX314_SLAVE,
    "ZNYX ZX314 ",
    tulip_zx314_media_probe,
    tulip_zx314_media_select
};

static int
tulip_dc21140_evalboard_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EB_INIT);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);
    return (TULIP_READ_CSR(sc, csr_gp) & TULIP_GP_EB_OK100) != 0;
}

static void
tulip_dc21140_evalboard_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EB_INIT);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling 100baseTX UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	    |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags |= TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_100BASET;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode &= ~(TULIP_CMD_PORTSELECT
			       |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER);
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags &= ~TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_10BASET;
    }
}

static const tulip_boardsw_t tulip_dc21140_eb_boardsw = {
    TULIP_DC21140_DEC_EB,
    "",
    tulip_dc21140_evalboard_media_probe,
    tulip_dc21140_evalboard_media_select
};

static int
tulip_dc21140_cogent_em100_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EM100_PINS);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EM100_INIT);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    return 1;
}

static void
tulip_dc21140_cogent_em100_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EM100_PINS);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_EM100_INIT);
    if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	printf("%s%d: enabling 100baseTX UTP port\n",
	       sc->tulip_if.if_name, sc->tulip_if.if_unit);
    sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	|TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
    sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
    sc->tulip_flags |= TULIP_ALTPHYS;
    sc->tulip_media = TULIP_MEDIA_100BASET;
}

static const tulip_boardsw_t tulip_dc21140_cogent_em100_boardsw = {
    TULIP_DC21140_COGENT_EM100,
    "Cogent EM100 ",
    tulip_dc21140_cogent_em100_media_probe,
    tulip_dc21140_cogent_em100_media_select
};


static int
tulip_dc21140_znyx_zx34x_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_ZX34X_PINS);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_ZX34X_INIT);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);

    return (TULIP_READ_CSR(sc, csr_gp) & TULIP_GP_ZX34X_OK10);
}

static void
tulip_dc21140_znyx_zx34x_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_ZX34X_PINS);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_ZX34X_INIT);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling 100baseTX UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	    |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags |= TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_100BASET;
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode &= ~(TULIP_CMD_PORTSELECT
			       |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER);
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags &= ~TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_10BASET;
    }
}

static const tulip_boardsw_t tulip_dc21140_znyx_zx34x_boardsw = {
    TULIP_DC21140_ZNYX_ZX34X,
    "ZNYX ZX34X ",
    tulip_dc21140_znyx_zx34x_media_probe,
    tulip_dc21140_znyx_zx34x_media_select
};

static int
tulip_dc21140_de500_media_probe(
    tulip_softc_t * const sc)
{
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_DE500_PINS);
    DELAY(1000);
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_DE500_HALFDUPLEX);
    if ((TULIP_READ_CSR(sc, csr_gp) &
	(TULIP_GP_DE500_NOTOK_100|TULIP_GP_DE500_NOTOK_10)) !=
	(TULIP_GP_DE500_NOTOK_100|TULIP_GP_DE500_NOTOK_10))
	return (TULIP_READ_CSR(sc, csr_gp) & TULIP_GP_DE500_NOTOK_100) != 0;
    TULIP_WRITE_CSR(sc, csr_gp,
	TULIP_GP_DE500_HALFDUPLEX|TULIP_GP_DE500_FORCE_100);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_WRITE_CSR(sc, csr_command,
	TULIP_READ_CSR(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);
    return (TULIP_READ_CSR(sc, csr_gp) & TULIP_GP_DE500_NOTOK_100) != 0;
}

static void
tulip_dc21140_de500_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
    TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_DE500_PINS);
    if (sc->tulip_if.if_flags & IFF_ALTPHYS) {
	if ((sc->tulip_flags & TULIP_ALTPHYS) == 0)
	    printf("%s%d: enabling 100baseTX UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT
	    |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER;
	sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags |= TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_100BASET;
	TULIP_WRITE_CSR(sc, csr_gp,
	    TULIP_GP_DE500_HALFDUPLEX|TULIP_GP_DE500_FORCE_100);
    } else {
	if (sc->tulip_flags & TULIP_ALTPHYS)
	    printf("%s%d: enabling 10baseT UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	sc->tulip_cmdmode &= ~(TULIP_CMD_PORTSELECT
			       |TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER);
	sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	sc->tulip_flags &= ~TULIP_ALTPHYS;
	sc->tulip_media = TULIP_MEDIA_10BASET;
	TULIP_WRITE_CSR(sc, csr_gp, TULIP_GP_DE500_HALFDUPLEX);
    }
}

static const tulip_boardsw_t tulip_dc21140_de500_boardsw = {
    TULIP_DC21140_DEC_DE500, "Digital DE500 ",
    tulip_dc21140_de500_media_probe,
    tulip_dc21140_de500_media_select
};

static int
tulip_dc21041_media_probe(
    tulip_softc_t * const sc)
{
    return 0;
}

static void
tulip_dc21041_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_ENHCAPTEFFCT
	/* |TULIP_CMD_FULLDUPLEX */ |TULIP_CMD_THRSHLD160|TULIP_CMD_BACKOFFCTR;
    sc->tulip_intrmask |= TULIP_STS_NORMALINTR|TULIP_STS_GPTIMEOUT
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

    if (TULIP_READ_CSR(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) {
	if (sc->tulip_media == TULIP_MEDIA_10BASET) {
	    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	} else if (sc->tulip_media == TULIP_MEDIA_BNC) {
	    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_BNC);
	    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_BNC);
	    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_BNC);
	    return;
	} else if (sc->tulip_media == TULIP_MEDIA_AUI) {
	    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_AUI);
	    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_AUI);
	    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_AUI);
	    return;
	}

	switch (sc->tulip_probe_state) {
	    case TULIP_PROBE_INACTIVE: {
		TULIP_WRITE_CSR(sc, csr_command, sc->tulip_cmdmode);
		sc->tulip_if.if_flags |= IFF_OACTIVE;
		sc->tulip_gpticks = 200;
		sc->tulip_probe_state = TULIP_PROBE_10BASET;
		TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_10BASET);
		TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_10BASET);
		TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_10BASET);
		TULIP_WRITE_CSR(sc, csr_gp_timer, 12000000 / 204800); /* 120 ms */
		break;
	    }
	    case TULIP_PROBE_10BASET: {
		if (--sc->tulip_gpticks > 0) {
		    if ((TULIP_READ_CSR(sc, csr_sia_status) & TULIP_SIASTS_OTHERRXACTIVITY) == 0) {
			TULIP_WRITE_CSR(sc, csr_gp_timer, 12000000 / 204800); /* 120 ms */
			TULIP_WRITE_CSR(sc, csr_intr, sc->tulip_intrmask);
			break;
		    }
		}
		sc->tulip_gpticks = 4;
		if (TULIP_READ_CSR(sc, csr_sia_status) & TULIP_SIASTS_OTHERRXACTIVITY) {
		    sc->tulip_probe_state = TULIP_PROBE_BNC;
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_BNC);
		    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_BNC);
		    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_BNC);
		    TULIP_WRITE_CSR(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		} else {
		    sc->tulip_probe_state = TULIP_PROBE_AUI;
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_AUI);
		    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_AUI);
		    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_AUI);
		    TULIP_WRITE_CSR(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		}
		break;
	    }
	    case TULIP_PROBE_BNC:
	    case TULIP_PROBE_AUI: {
		if (sc->tulip_flags & TULIP_TXPROBE_OK) {
		    sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
		    sc->tulip_flags &= ~(TULIP_TXPROBE_OK|TULIP_TXPROBE_ACTIVE);
		    TULIP_WRITE_CSR(sc, csr_gp_timer, 0); /* disable */
		    if ((sc->tulip_probe_state == TULIP_PROBE_AUI
			 && sc->tulip_media != TULIP_MEDIA_AUI)
			|| (sc->tulip_probe_state == TULIP_PROBE_BNC
			    && sc->tulip_media != TULIP_MEDIA_AUI)) {
			printf("%s%d: enabling %s port\n",
			       sc->tulip_if.if_name, sc->tulip_if.if_unit,
			       sc->tulip_probe_state == TULIP_PROBE_BNC
			           ? "Thinwire/BNC" : "AUI");
			if (sc->tulip_probe_state == TULIP_PROBE_AUI)
			    sc->tulip_media = TULIP_MEDIA_AUI;
			else if (sc->tulip_probe_state == TULIP_PROBE_BNC)
			    sc->tulip_media = TULIP_MEDIA_BNC;
		    }
		    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		    break;
		}
		if ((sc->tulip_flags & TULIP_WANTRXACT) == 0
		    || (TULIP_READ_CSR(sc, csr_sia_status) & TULIP_SIASTS_RXACTIVITY)) {
		    if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0) {
			struct mbuf *m;
			/*
			 * Before we are sure this is the right media we need
			 * to send a small packet to make sure there's carrier.
			 * Strangely, BNC and AUI will "see" receive data if
			 * either is connected so the transmit is the only way
			 * to verify the connectivity.
			 */
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
			    TULIP_WRITE_CSR(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
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
			sc->tulip_flags |= TULIP_TXPROBE_ACTIVE;
			sc->tulip_flags &= ~TULIP_TXPROBE_OK;
			sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
			TULIP_WRITE_CSR(sc, csr_command, sc->tulip_cmdmode);
			IF_PREPEND(&sc->tulip_if.if_snd, m);
			tulip_start(&sc->tulip_if);
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
			printf("%s%d: autosense failed: cable problem?\n",
			       sc->tulip_name, sc->tulip_unit);
		    }
		}
		/*
		 * Since this media failed to probe, try the other one.
		 */
		if (sc->tulip_probe_state == TULIP_PROBE_AUI) {
		    sc->tulip_probe_state = TULIP_PROBE_BNC;
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_BNC);
		    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_BNC);
		    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_BNC);
		    TULIP_WRITE_CSR(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		} else {
		    sc->tulip_probe_state = TULIP_PROBE_AUI;
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
		    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_AUI);
		    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_AUI);
		    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_AUI);
		    TULIP_WRITE_CSR(sc, csr_gp_timer, 100000000 / 204800); /* 100 ms */
		}
		TULIP_WRITE_CSR(sc, csr_intr, sc->tulip_intrmask);
		break;
	    }
	}
    } else {
	/*
	 * If the link has passed LinkPass, 10baseT is the
	 * proper media to use.
	 */
	if (sc->tulip_media != TULIP_MEDIA_10BASET)
	    printf("%s%d: enabling 10baseT/UTP port\n",
		   sc->tulip_if.if_name, sc->tulip_if.if_unit);
	if (sc->tulip_media != TULIP_MEDIA_10BASET
		|| (sc->tulip_flags & TULIP_INRESET)) {
	    sc->tulip_media = TULIP_MEDIA_10BASET;
	    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    TULIP_WRITE_CSR(sc, csr_sia_connectivity, TULIP_DC21041_SIACONN_10BASET);
	    TULIP_WRITE_CSR(sc, csr_sia_tx_rx,        TULIP_DC21041_SIATXRX_10BASET);
	    TULIP_WRITE_CSR(sc, csr_sia_general,      TULIP_DC21041_SIAGEN_10BASET);
	}
	TULIP_WRITE_CSR(sc, csr_gp_timer, 0); /* disable */
	sc->tulip_gpticks = 1;
	sc->tulip_probe_state = TULIP_PROBE_10BASET;
	sc->tulip_intrmask &= ~TULIP_STS_GPTIMEOUT;
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }
    TULIP_WRITE_CSR(sc, csr_intr, sc->tulip_intrmask);
}

static const tulip_boardsw_t tulip_dc21041_boardsw = {
    TULIP_DC21041_GENERIC,
    "",
    tulip_dc21041_media_probe,
    tulip_dc21041_media_select
};

static const tulip_boardsw_t tulip_dc21041_de450_boardsw = {
    TULIP_DC21041_DE450,
    "Digital DE450 ",
    tulip_dc21041_media_probe,
    tulip_dc21041_media_select
};

static void
tulip_reset(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t *ri;
    tulip_desc_t *di;

    TULIP_WRITE_CSR(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    sc->tulip_flags |= TULIP_INRESET;
    sc->tulip_intrmask = 0;
    TULIP_WRITE_CSR(sc, csr_intr, sc->tulip_intrmask);

    TULIP_WRITE_CSR(sc, csr_txlist, vtophys(&sc->tulip_txinfo.ri_first[0]));
    TULIP_WRITE_CSR(sc, csr_rxlist, vtophys(&sc->tulip_rxinfo.ri_first[0]));
    TULIP_WRITE_CSR(sc, csr_busmode,
        (1 << (TULIP_BURSTSIZE(sc->tulip_unit) + 8))
	|TULIP_BUSMODE_CACHE_ALIGN8
	|(BYTE_ORDER != LITTLE_ENDIAN ? TULIP_BUSMODE_BIGENDIAN : 0));

    sc->tulip_txq.ifq_maxlen = TULIP_TXDESCS;
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

    sc->tulip_intrmask |= TULIP_STS_NORMALINTR|TULIP_STS_RXINTR|TULIP_STS_TXINTR
	|TULIP_STS_ABNRMLINTR|TULIP_STS_SYSERROR|TULIP_STS_TXSTOPPED
	    |TULIP_STS_TXBABBLE|TULIP_STS_LINKFAIL|TULIP_STS_RXSTOPPED;
    sc->tulip_flags &= ~(TULIP_DOINGSETUP|TULIP_WANTSETUP|TULIP_INRESET);
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
	    tulip_start(&sc->tulip_if);
	}
	TULIP_WRITE_CSR(sc, csr_intr, sc->tulip_intrmask);
	TULIP_WRITE_CSR(sc, csr_command, sc->tulip_cmdmode);
    } else {
	tulip_reset(sc);
	sc->tulip_if.if_flags &= ~IFF_RUNNING;
    }
}


#if TULIP_CHECK_RXCRC
static unsigned
tulip_crc32(
    u_char *addr,
    int len)
{
    unsigned int crc = 0xFFFFFFFF;
    static unsigned int crctbl[256];
    int idx;
    static int done;
    /*
     * initialize the multicast address CRC table
     */
    for (idx = 0; !done && idx < 256; idx++) {
	unsigned int tmp = idx;
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	tmp = (tmp >> 1) ^ (tmp & 1 ? TULIP_CRC32_POLY : 0);	/* XOR */
	crctbl[idx] = tmp;
    }
    done = 1;

    while (len-- > 0)
	crc = (crc >> 8) ^ crctbl[*addr++] ^ crctbl[crc & 0xFF];

    return crc;
}
#endif

static void
tulip_rx_intr(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t * const ri = &sc->tulip_rxinfo;
    struct ifnet * const ifp = &sc->tulip_if;

    for (;;) {
	struct ether_header eh;
	tulip_desc_t *eop = ri->ri_nextin;
	int total_len = 0;
	struct mbuf *m = NULL;
	int accept = 0;

	if (sc->tulip_rxq.ifq_len < TULIP_RXQ_TARGET)
	     goto queue_mbuf;

	if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER)
	    break;
	
	total_len = ((eop->d_status >> 16) & 0x7FF) - 4;
	IF_DEQUEUE(&sc->tulip_rxq, m);
	if ((eop->d_status & TULIP_DSTS_ERRSUM) == 0) {

#if TULIP_CHECK_RXCRC
	    unsigned crc = tulip_crc32(mtod(m, unsigned char *), total_len);
	    if (~crc != *((unsigned *) &bufaddr[total_len])) {
		printf("%s%d: bad rx crc: %08x [rx] != %08x\n",
		       sc->tulip_name, sc->tulip_unit,
		       *((unsigned *) &bufaddr[total_len]), ~crc);
		goto next;
	    }
#endif
	    eh = *mtod(m, struct ether_header *);
#if NBPFILTER > 0
	    if (sc->tulip_bpf != NULL)
		bpf_tap(ifp, mtod(m, caddr_t), total_len);
#endif
	    if ((sc->tulip_if.if_flags & IFF_PROMISC)
		    && (eh.ether_dhost[0] & 1) == 0
		    && !TULIP_ADDREQUAL(eh.ether_dhost, sc->tulip_ac.ac_enaddr))
		    goto next;
	    accept = 1;
	    total_len -= sizeof(struct ether_header);
	} else {
	    ifp->if_ierrors++;
	}
      next:
	ifp->if_ipackets++;
	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;
      queue_mbuf:
	/*
	 * Either we are priming the TULIP with mbufs (m == NULL)
	 * or we are about to accept an mbuf for the upper layers
	 * so we need to allocate an mbuf to replace it.  If we
	 * can't replace, then count it as an input error and reuse
	 * the mbuf.
	 */
	if (accept || m == NULL) {
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
		if (m0 != NULL) {
#if defined(__bsdi__)
		    eh.ether_type = ntohs(eh.ether_type);
#endif
#if !defined(TULIP_COPY_RXDATA)
		    m->m_data += sizeof(struct ether_header);
		    m->m_len = m->m_pkthdr.len = total_len;
		    m->m_pkthdr.rcvif = ifp;
		    ether_input(ifp, &eh, m);
		    m = m0;
#else
		    bcopy(mtod(m, caddr_t) + sizeof(struct ether_header),
			  mtod(m0, caddr_t), total_len);
		    m0->m_len = m0->m_pkthdr.len = total_len;
		    m0->m_pkthdr.rcvif = ifp;
		    ether_input(ifp, &eh, m0);
#endif
		} else {
		    ifp->if_ierrors++;
		}
	    } else {
		m = m0;
	    }
	}
	if (m == NULL)
	    break;
	/*
	 * Now give the buffer to the TULIP and save in our
	 * receive queue.
	 */
	ri->ri_nextout->d_length1 = MCLBYTES - 4;
	ri->ri_nextout->d_addr1 = vtophys(mtod(m, caddr_t));
	ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
	if (++ri->ri_nextout == ri->ri_last)
	    ri->ri_nextout = ri->ri_first;
	IF_ENQUEUE(&sc->tulip_rxq, m);
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
		if ((sc->tulip_flags & TULIP_WANTSETUP) == 0) {
		    tulip_rx_intr(sc);
		    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
		    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
		    TULIP_WRITE_CSR(sc, csr_status, TULIP_STS_RXSTOPPED);
		    TULIP_WRITE_CSR(sc, csr_command, sc->tulip_cmdmode);
		    TULIP_WRITE_CSR(sc, csr_intr, sc->tulip_intrmask);
		}
	   } else {
		IF_DEQUEUE(&sc->tulip_txq, m);
		m_freem(m);
		xmits++;
		if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) {
		    if ((ri->ri_nextin->d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxEXCCOLL)) == 0)
			sc->tulip_flags |= TULIP_TXPROBE_OK;
		    (*sc->tulip_boardsw->bd_media_select)(sc);
		} else {
		    sc->tulip_if.if_collisions +=
			(ri->ri_nextin->d_status & TULIP_DSTS_TxCOLLMASK)
			    >> TULIP_DSTS_V_TxCOLLCNT;
		    if (ri->ri_nextin->d_status & TULIP_DSTS_ERRSUM)
			sc->tulip_if.if_oerrors++;
		}
	    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;
	ri->ri_free++;
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }
    sc->tulip_if.if_opackets += xmits;
    return xmits;
}

static ifnet_ret_t
tulip_start(
    struct ifnet * const ifp)
{
    tulip_softc_t * const sc = ifp->if_softc;
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
	    ri->ri_nextout->d_addr1 = vtophys(sc->tulip_setupbuf);
	    ri->ri_nextout->d_length2 = 0;
	    ri->ri_nextout->d_addr2 = 0;
	    ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
	    TULIP_WRITE_CSR(sc, csr_txpoll, 1);
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
	    unsigned clsize = PAGE_SIZE - (((u_long) addr) & PAGE_MASK);

	    next_m0 = m0->m_next;
	    while (len > 0) {
		unsigned slen = min(len, clsize);

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
		    eop->d_addr1 = vtophys(addr); eop->d_length1 = slen;
		} else {
		    /*
		     *  Fill in second half of descriptor
		     */
		    eop->d_addr2 = vtophys(addr); eop->d_length2 = slen;
		}
		d_status = TULIP_DSTS_OWNER;
		len -= slen;
		addr += slen;
		clsize = PAGE_SIZE;
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
	    bpf_mtap(ifp, m);
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

	TULIP_WRITE_CSR(sc, csr_txpoll, 1);
    }
    if (m != NULL) {
	ifp->if_flags |= IFF_OACTIVE;
	IF_PREPEND(ifq, m);
    }
}

static void
tulip_intr(
    void *arg)
{
    tulip_softc_t * sc = (tulip_softc_t *) arg;
    tulip_uint32_t csr;
#if defined(__bsdi__)
    int progress = 1;
#else
    int progress = 0;
#endif

    do {
	while ((csr = TULIP_READ_CSR(sc, csr_status)) & (TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR)) {
	    progress = 1;
	    TULIP_WRITE_CSR(sc, csr_status, csr & sc->tulip_intrmask);

	    if (csr & TULIP_STS_SYSERROR) {
		if ((csr & TULIP_STS_ERRORMASK) == TULIP_STS_ERR_PARITY) {
		    tulip_reset(sc);
		    tulip_init(sc);
		    break;
		}
	    }
	    if (csr & (TULIP_STS_GPTIMEOUT|TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL)) {
		if (sc->tulip_chipid == TULIP_DC21041) {
		    (*sc->tulip_boardsw->bd_media_select)(sc);
		    if (csr & (TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL))
			csr &= ~TULIP_STS_ABNRMLINTR;
		}
	    }
	    if (csr & TULIP_STS_ABNRMLINTR) {
		printf("%s%d: abnormal interrupt: 0x%05x [0x%05x]\n",
		       sc->tulip_name, sc->tulip_unit, csr, csr & sc->tulip_intrmask);
		TULIP_WRITE_CSR(sc, csr_command, sc->tulip_cmdmode);
	    }
	    if (csr & TULIP_STS_RXINTR)
		tulip_rx_intr(sc);
	    if (sc->tulip_txinfo.ri_free < sc->tulip_txinfo.ri_max) {
		tulip_tx_intr(sc);
		tulip_start(&sc->tulip_if);
	    }
	}
    } while ((sc = sc->tulip_slaves) != NULL);
}

/*
 *
 */

static void
tulip_delay_300ns(
    tulip_softc_t * const sc)
{
    TULIP_READ_CSR(sc, csr_busmode); TULIP_READ_CSR(sc, csr_busmode);
    TULIP_READ_CSR(sc, csr_busmode); TULIP_READ_CSR(sc, csr_busmode);

    TULIP_READ_CSR(sc, csr_busmode); TULIP_READ_CSR(sc, csr_busmode);
    TULIP_READ_CSR(sc, csr_busmode); TULIP_READ_CSR(sc, csr_busmode);

    TULIP_READ_CSR(sc, csr_busmode); TULIP_READ_CSR(sc, csr_busmode);
    TULIP_READ_CSR(sc, csr_busmode); TULIP_READ_CSR(sc, csr_busmode);
}

#define EMIT    do { TULIP_WRITE_CSR(sc, csr_srom_mii, csr); tulip_delay_300ns(sc); } while (0)

static void
tulip_idle_srom(
    tulip_softc_t * const sc)
{
    unsigned bit, csr;
    
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
    csr ^= SROMCS; EMIT; EMIT;
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
        csr  = SROMSEL | SROMRD;        EMIT;
        csr ^= SROMCSON;                EMIT;
        csr ^=            SROMCLKON;    EMIT;
    
        lastbit = 0;
        for (bits = idx|cmdmask, bit = bitwidth + 3; bit > 0; bit--, bits <<= 1) {
            const unsigned thisbit = bits & msb;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
            if (thisbit != lastbit) {
                csr ^= SROMDOUT; EMIT;  /* clock low; invert data */
            }
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
            lastbit = thisbit;
        }
        csr ^= SROMCLKOFF; EMIT;

        for (data = 0, bits = 0; bits < 16; bits++) {
            data <<= 1;
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */ 
            data |= TULIP_READ_CSR(sc, csr_srom_mii) & SROMDIN ? 1 : 0;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        }
	sc->tulip_rombuf[idx*2] = data & 0xFF;
	sc->tulip_rombuf[idx*2+1] = data >> 8;
        csr  = SROMSEL | SROMRD; EMIT;
        csr  = 0; EMIT;
    }
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
	TULIP_WRITE_CSR(sc, csr_enetrom, 1);
	for (idx = 0; idx < 32; idx++) {
	    int cnt = 0;
	    while (((csr = TULIP_READ_CSR(sc, csr_enetrom)) & 0x80000000L) && cnt < 10000)
		cnt++;
	    sc->tulip_rombuf[idx] = csr & 0xFF;
	}
	sc->tulip_boardsw = &tulip_dc21040_boardsw;
#if defined(TULIP_EISA)
    } else if (sc->tulip_chipid == TULIP_DE425) {
	int cnt;
	for (idx = 0, cnt = 0; idx < sizeof(testpat) && cnt < 32; cnt++) {
	    tmpbuf[idx] = TULIP_READ_CSRBYTE(sc, csr_enetrom);
	    if (tmpbuf[idx] == testpat[idx])
		++idx;
	    else
		idx = 0;
	}
	for (idx = 0; idx < 32; idx++)
	    sc->tulip_rombuf[idx] = TULIP_READ_CSRBYTE(sc, csr_enetrom);
	sc->tulip_boardsw = &tulip_dc21040_boardsw;
#endif /* TULIP_EISA */
    } else {
	int new_srom_fmt = 0;
	/*
	 * Assume all DC21140 board are compatible with the
	 * DEC 10/100 evaluation board.  Not really valid but
	 * it's the best we can do until every one switches to
	 * the new SROM format.
	 */
	if (sc->tulip_chipid == TULIP_DC21140)
	    sc->tulip_boardsw = &tulip_dc21140_eb_boardsw;
	/*
	 * Thankfully all DC21041's act the same.
	 */
	if (sc->tulip_chipid == TULIP_DC21041)
	    sc->tulip_boardsw = &tulip_dc21041_boardsw;
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
	    /*
	     * New SROM format.  Copy out the Ethernet address.
	     * If it contains a DE500-XA string, then it must be
	     * a DE500-XA.
	     */
	    bcopy(sc->tulip_rombuf + 20, sc->tulip_hwaddr, 6);
	    if (bcmp(sc->tulip_rombuf + 29, "DE500-XA", 8) == 0)
		sc->tulip_boardsw = &tulip_dc21140_de500_boardsw;
	    if (bcmp(sc->tulip_rombuf + 29, "DE450", 5) == 0)
		sc->tulip_boardsw = &tulip_dc21041_de450_boardsw;
	    if (sc->tulip_boardsw == NULL)
		return -6;
	    sc->tulip_flags |= TULIP_ROMOK;
	    return 0;
	}
    }


    if (bcmp(&sc->tulip_rombuf[0], &sc->tulip_rombuf[16], 8) != 0) {
	/*
	 * Some folks don't use the standard ethernet rom format
	 * but instead just put the address in the first 6 bytes
	 * of the rom and let the rest be all 0xffs.  (Can we say
	 * ZNYX???)
	 */
	for (idx = 6; idx < 32; idx++) {
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
	if (sc->tulip_hwaddr[0] == TULIP_OUI_ZNYX_0
		&& sc->tulip_hwaddr[1] == TULIP_OUI_ZNYX_1
		&& sc->tulip_hwaddr[2] == TULIP_OUI_ZNYX_2
	        && (sc->tulip_hwaddr[3] & ~3) == 0xF0) {
	    /*
	     * Now if the OUI is ZNYX and hwaddr[3] == 0xF0 .. 0xF3
	     * then it's a ZX314 Master port.
	     */
	    sc->tulip_boardsw = &tulip_dc21040_zx314_master_boardsw;
	}
	return 0;
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

    cksum = *(u_short *) &sc->tulip_hwaddr[0];
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_short *) &sc->tulip_hwaddr[2];
    if (cksum > 65535) cksum -= 65535;
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_short *) &sc->tulip_hwaddr[4];
    if (cksum >= 65535) cksum -= 65535;

    rom_cksum = *(u_short *) &sc->tulip_rombuf[6];
	
    if (cksum != rom_cksum)
	return -1;

    /*
     * Check for various boards based on OUI.  Did I say braindead?
     */

    if (sc->tulip_chipid == TULIP_DC21140) {
	if (sc->tulip_hwaddr[0] == TULIP_OUI_COGENT_0
		&& sc->tulip_hwaddr[1] == TULIP_OUI_COGENT_1
		&& sc->tulip_hwaddr[2] == TULIP_OUI_COGENT_2) {
	    if (sc->tulip_rombuf[32] == TULIP_COGENT_EM100_ID)
		sc->tulip_boardsw = &tulip_dc21140_cogent_em100_boardsw;
	}
	if (sc->tulip_hwaddr[0] == TULIP_OUI_ZNYX_0
		&& sc->tulip_hwaddr[1] == TULIP_OUI_ZNYX_1
		&& sc->tulip_hwaddr[2] == TULIP_OUI_ZNYX_2) {
	    /* this at least works for the zx342 from Znyx */
	    sc->tulip_boardsw = &tulip_dc21140_znyx_zx34x_boardsw;
	}
    }

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
    int i;

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
	sp[39] = ((u_short *) sc->tulip_ac.ac_enaddr)[0]; 
	sp[40] = ((u_short *) sc->tulip_ac.ac_enaddr)[1]; 
	sp[41] = ((u_short *) sc->tulip_ac.ac_enaddr)[2];
    } else {
	/*
	 * Else can get perfect filtering for 16 addresses.
	 */
	i = 0;
	ETHER_FIRST_MULTI(step, &sc->tulip_ac, enm);
	for (; enm != NULL; i++) {
	    *sp++ = ((u_short *) enm->enm_addrlo)[0]; 
	    *sp++ = ((u_short *) enm->enm_addrlo)[1]; 
	    *sp++ = ((u_short *) enm->enm_addrlo)[2];
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
	    *sp++ = ((u_short *) sc->tulip_ac.ac_enaddr)[0]; 
	    *sp++ = ((u_short *) sc->tulip_ac.ac_enaddr)[1]; 
	    *sp++ = ((u_short *) sc->tulip_ac.ac_enaddr)[2];
	}
    }
}

static int
tulip_ioctl(
    struct ifnet * const ifp,
    ioctl_cmd_t cmd,
    caddr_t data)
{
    tulip_softc_t * const sc = ifp->if_softc;
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *) data;
    int s, error = 0;

    s = splimp();

    switch (cmd) {
	case SIOCSIFADDR: {

	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    tulip_init(sc);
#if defined(__FreeBSD__) || defined(__NetBSD__)
		    arp_ifinit(&sc->tulip_ac, ifa);
#elif defined(__bsdi__)
		    sc->tulip_ac.ac_ipaddr = IA_SIN(ifa)->sin_addr;
		    arpwhohas(&sc->tulip_ac, &IA_SIN(ifa)->sin_addr);
#endif
		    break;
		}
#endif /* INET */

#ifdef NS
		/*
		 * This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->tulip_ac.ac_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->tulip_ac.ac_enaddr,
			      sizeof sc->tulip_ac.ac_enaddr);
		    }

		    tulip_init(sc);
		    break;
		}
#endif /* NS */

		default: {
		    tulip_init(sc);
		    break;
		}
	    }
	    break;
	}
	case SIOCGIFADDR: {
	    bcopy((caddr_t) sc->tulip_ac.ac_enaddr,
		  (caddr_t) ((struct sockaddr *)&ifr->ifr_data)->sa_data,
		  6);
	    break;
	}

	case SIOCSIFFLAGS: {
	    /*
	     * Changing the connection forces a reset.
	     */
	    if (sc->tulip_flags & TULIP_ALTPHYS) {
		if ((ifp->if_flags & IFF_ALTPHYS) == 0)
		    tulip_reset(sc);
	    } else {
		if (ifp->if_flags & IFF_ALTPHYS)
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
	    if (ifr->ifr_mtu > ETHERMTU) {
		error = EINVAL;
		break;
	    }
	    ifp->if_mtu = ifr->ifr_mtu;
	    break;
#endif

	default: {
	    error = EINVAL;
	    break;
	}
    }

    splx(s);
    return error;
}

static void
tulip_attach(
    tulip_softc_t * const sc)
{
    struct ifnet * const ifp = &sc->tulip_if;

    ifp->if_softc = sc;
    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;
    ifp->if_ioctl = tulip_ioctl;
    ifp->if_output = ether_output;
    ifp->if_start = tulip_start;
  
#ifdef __FreeBSD__
    printf("%s%d", sc->tulip_name, sc->tulip_unit);
#endif
    printf(": %s%s pass %d.%d Ethernet address %6D\n", 
	   sc->tulip_boardsw->bd_description,
	   tulip_chipdescs[sc->tulip_chipid],
	   (sc->tulip_revinfo & 0xF0) >> 4,
	   sc->tulip_revinfo & 0x0F,
	   sc->tulip_hwaddr, ":");

    if ((*sc->tulip_boardsw->bd_media_probe)(sc)) {
	ifp->if_flags |= IFF_ALTPHYS;
    } else {
	sc->tulip_flags |= TULIP_ALTPHYS;
    }

    tulip_reset(sc);

    if_attach(ifp);
    ether_ifattach(ifp);

#if NBPFILTER > 0
    bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
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
    sc->tulip_csrs.csr_missed_frame	= csr_base +  8 * csr_size;
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
    } else if (sc->tulip_chipid == TULIP_DC21140) {
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
    struct kern_devconf * const kdc,
    int force)
{
    if (kdc->kdc_unit < NDE) {
	tulip_softc_t * const sc = TULIP_UNIT_TO_SOFTC(kdc->kdc_unit);
	TULIP_WRITE_CSR(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
			   33MHz that comes to two microseconds but wait a
			   bit longer anyways) */
    }
    (void) dev_detach(kdc);
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
    if (PCI_CHIPID(device_id) == DC21140_CHIPID)
	return "Digital DC21140 Fast Ethernet";
    return NULL;
}

static void  tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);
static u_long tulip_pci_count;

static struct pci_device dedevice = {
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
    TULIP_WRITE_CSR(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
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

#if defined(TULIP_EISA)
    if ((slot = eisa_match(cf, ia)) != 0) {
	unsigned tmp;
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
    } else {
#endif /* TULIP_EISA */
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
#if defined(TULIP_EISA)
    }
#endif

    /* PCI bus masters don't use host DMA channels */
    ia->ia_drq = DRQNONE;

    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("de%d: error: desired IRQ of %d does not match device's actual IRQ of %d,\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK && (ia->ia_irq = isa_irqalloc(irq)) == 0) {
	printf("de%d: warning: IRQ %d is shared\n", cf->cf_unit, ffs(irq) - 1);
	ia->ia_irq = irq;
    }
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
    0, "de", tulip_probe, tulip_pci_attach, DV_IFNET, sizeof(tulip_softc_t),
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
    TULIP_WRITE_CSR(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
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

struct cfdriver decd = {
    0, "de", tulip_pci_probe, tulip_pci_attach, DV_IFNET, sizeof(tulip_softc_t)
};

#endif /* __NetBSD__ */

static void
tulip_pci_attach(
    TULIP_PCI_ATTACH_ARGS)
{
#if defined(__FreeBSD__)
    tulip_softc_t *sc;
#endif
#if defined(__bsdi__)
    tulip_softc_t * const sc = (tulip_softc_t *) self;
    struct isa_attach_args * const ia = (struct isa_attach_args *) aux;
    pci_devaddr_t *pa = (pci_devaddr_t *) ia->ia_aux;
    int unit = sc->tulip_dev.dv_unit;
#endif
#if defined(__NetBSD__)
    tulip_softc_t * const sc = (tulip_softc_t *) self;
    struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
    int unit = sc->tulip_dev.dv_unit;
#endif
    int retval, idx, revinfo, id;
#if !defined(TULIP_IOMAPPED) && !defined(__bsdi__)
    vm_offset_t pa_csrs;
#endif
    unsigned csroffset = TULIP_PCI_CSROFFSET;
    unsigned csrsize = TULIP_PCI_CSRSIZE;
    tulip_csrptr_t csr_base;
    tulip_desc_t *rxdescs, *txdescs;
    tulip_chipid_t chipid = TULIP_CHIPID_UNKNOWN;

#if defined(__FreeBSD__)
    if (unit >= NDE) {
	printf("de%d: not configured; kernel is built for only %d device%s.\n",
	       unit, NDE, NDE == 1 ? "" : "s");
	return;
    }
#endif

#if defined(__FreeBSD__)
    revinfo = pci_conf_read(config_id, PCI_CFRV) & 0xFF;
    id = pci_conf_read(config_id, PCI_CFID);
#endif
#if defined(__bsdi__)
    if (pa != NULL) {
	revinfo = pci_inl(pa, PCI_CFRV) & 0xFF;
	id = pci_inl(pa, PCI_CFID);
#if defined(TULIP_EISA)
    } else {
	revinfo = inl(ia->ia_iobase + DE425_CFRV) & 0xFF;
	csroffset = TULIP_EISA_CSROFFSET;
	csrsize = TULIP_EISA_CSRSIZE;
	chipid = TULIP_DE425;
#endif
    }
#endif
#if defined(__NetBSD__)
    revinfo = pci_conf_read(pa->pa_tag, PCI_CFRV) & 0xFF;
    id = pa->pa_id;
#endif

    if (PCI_VENDORID(id) == DEC_VENDORID) {
	if (PCI_CHIPID(id) == DC21040_CHIPID) chipid = TULIP_DC21040;
	else if (PCI_CHIPID(id) == DC21140_CHIPID) chipid = TULIP_DC21140;
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
#ifdef __FreeBSD__
	printf("de%d", unit);
#endif
	printf(": not configured; DC21140 pass 1.1 required (%d.%d found)\n",
	       revinfo >> 4, revinfo & 0x0f);
	return;
    }

#if defined(__FreeBSD__)
    sc = (tulip_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
	return;
    bzero(sc, sizeof(*sc));				/* Zero out the softc*/
#endif

    rxdescs = (tulip_desc_t *)
	malloc(sizeof(tulip_desc_t) * TULIP_RXDESCS, M_DEVBUF, M_NOWAIT);
    if (rxdescs == NULL) {
#if defined(__FreeBSD__)
	free((caddr_t) sc, M_DEVBUF);
#endif
	return;
    }

    txdescs = (tulip_desc_t *)
	malloc(sizeof(tulip_desc_t) * TULIP_TXDESCS, M_DEVBUF, M_NOWAIT);
    if (txdescs == NULL) {
	free((caddr_t) rxdescs, M_DEVBUF);
#if defined(__FreeBSD__)
	free((caddr_t) sc, M_DEVBUF);
#endif
	return;
    }

    sc->tulip_chipid = chipid;
    sc->tulip_unit = unit;
    sc->tulip_name = "de";
    sc->tulip_revinfo = revinfo;
#if defined(__FreeBSD__)
#if defined(TULIP_IOMAPPED)
    retval = pci_map_port(config_id, PCI_CBIO, &csr_base);
#else
    retval = pci_map_mem(config_id, PCI_CBMA, (vm_offset_t *) &csr_base, &pa_csrs);
#endif
    if (!retval) {
	free((caddr_t) txdescs, M_DEVBUF);
	free((caddr_t) rxdescs, M_DEVBUF);
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
#if defined(TULIP_IOMAPPED)
    retval = pci_map_io(pa->pa_tag, PCI_CBIO, &csr_base);
#else
    retval = pci_map_mem(pa->pa_tag, PCI_CBMA, (vm_offset_t *) &csr_base, &pa_csrs);
#endif
    if (retval) {
	free((caddr_t) txdescs, M_DEVBUF);
	free((caddr_t) rxdescs, M_DEVBUF);
	return;
    }
#endif /* __NetBSD__ */

    tulip_initcsrs(sc, csr_base + csroffset, csrsize);
    tulip_initring(sc, &sc->tulip_rxinfo, rxdescs, TULIP_RXDESCS);
    tulip_initring(sc, &sc->tulip_txinfo, txdescs, TULIP_TXDESCS);
    if ((retval = tulip_read_macaddr(sc)) < 0) {
#ifdef __FreeBSD__
	printf("%s%d", sc->tulip_name, sc->tulip_unit);
#endif
	printf(": can't read ENET ROM (why=%d) (", retval);
	for (idx = 0; idx < 32; idx++)
	    printf("%02x", sc->tulip_rombuf[idx]);
	printf("\n");
	printf("%s%d: %s%s pass %d.%d Ethernet address %s\n",
	       sc->tulip_name, sc->tulip_unit,
	       (sc->tulip_boardsw != NULL ? sc->tulip_boardsw->bd_description : ""),
	       tulip_chipdescs[sc->tulip_chipid],
	       (sc->tulip_revinfo & 0xF0) >> 4, sc->tulip_revinfo & 0x0F,
	       "unknown");
    } else {
	/*
	 * Make sure there won't be any interrupts or such...
	 */
	TULIP_WRITE_CSR(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	DELAY(10);	/* Wait 10 microsends (actually 50 PCI cycles but at 
			   33MHz that comes to two microseconds but wait a
			   bit longer anyways) */
#if defined(__NetBSD__)
	if (sc->tulip_boardsw->bd_type != TULIP_DC21040_ZX314_SLAVE) {
	    sc->tulip_ih = pci_map_int(pa->pa_tag, PCI_IPL_NET, tulip_intr, sc);
	    if (sc->tulip_ih == NULL) {
		printf("%s%d: couldn't map interrupt\n",
		       sc->tulip_name, sc->tulip_unit);
		return;
	    }
#if defined(__i386__)
	    /* gross but netbsd won't print the irq otherwise */
	    printf(" irq %d", ((struct intrhand *) sc->tulip_ih)->ih_irq);
#endif
	}
	sc->tulip_ats = shutdownhook_establish(tulip_pci_shutdown, sc);
	if (sc->tulip_ats == NULL)
	    printf("%s%d: warning: couldn't establish shutdown hook\n",
		   sc->tulip_name, sc->tulip_unit);
#endif
#if defined(__FreeBSD__)
	if (sc->tulip_boardsw->bd_type != TULIP_DC21040_ZX314_SLAVE) {
	    if (!pci_map_int(config_id, tulip_intr, (void*) sc, &net_imask)) {
		printf("%s%d: couldn't map interrupt\n",
			sc->tulip_name, sc->tulip_unit);
		return;
	    }
	}
#endif
#if defined(__bsdi__)
	if (sc->tulip_boardsw->bd_type != TULIP_DC21040_ZX314_SLAVE) {
	    isa_establish(&sc->tulip_id, &sc->tulip_dev);

	    sc->tulip_ih.ih_fun = tulip_intr;
	    sc->tulip_ih.ih_arg = (void *)sc;
	    intr_establish(ia->ia_irq, &sc->tulip_ih, DV_NET);
	}

	sc->tulip_ats.func = tulip_shutdown;
	sc->tulip_ats.arg = (void *) sc;
	atshutdown(&sc->tulip_ats, ATSH_ADD);
#endif
	tulip_reset(sc);
	tulip_attach(sc);
    }
}
#endif /* NDE > 0 */
