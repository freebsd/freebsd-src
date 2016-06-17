/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Holds initial configuration information for devices.
 *
 * Version:	@(#)Space.c	1.0.7	08/12/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald J. Becker, <becker@scyld.com>
 *
 * Changelog:
 *		Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 09/1999
 *		- fix sbni: s/device/net_device/
 *		Paul Gortmaker (06/98): 
 *		 - sort probes in a sane way, make sure all (safe) probes
 *		   get run once & failed autoprobes don't autoprobe again.
 *
 *	FIXME:
 *		Phase out placeholder dev entries put in the linked list
 *		here in favour of drivers using init_etherdev(NULL, ...)
 *		combined with a single find_all_devs() function (for 2.3)
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netlink.h>
#include <linux/divert.h>

#define	NEXT_DEV	NULL


/* A unified ethernet device probe.  This is the easiest way to have every
   ethernet adaptor have the name "eth[0123...]".
   */

extern int ne2_probe(struct net_device *dev);
extern int hp100_probe(struct net_device *dev);
extern int ultra_probe(struct net_device *dev);
extern int ultra32_probe(struct net_device *dev);
extern int ultramca_probe(struct net_device *dev);
extern int wd_probe(struct net_device *dev);
extern int el2_probe(struct net_device *dev);
extern int ne_probe(struct net_device *dev);
extern int hp_probe(struct net_device *dev);
extern int hp_plus_probe(struct net_device *dev);
extern int znet_probe(struct net_device *);
extern int express_probe(struct net_device *);
extern int eepro_probe(struct net_device *);
extern int el3_probe(struct net_device *);
extern int at1500_probe(struct net_device *);
extern int at1700_probe(struct net_device *);
extern int fmv18x_probe(struct net_device *);
extern int eth16i_probe(struct net_device *);
extern int depca_probe(struct net_device *);
extern int i82596_probe(struct net_device *);
extern int ewrk3_probe(struct net_device *);
extern int de4x5_probe(struct net_device *);
extern int el1_probe(struct net_device *);
extern int wavelan_probe(struct net_device *);
extern int arlan_probe(struct net_device *);
extern int el16_probe(struct net_device *);
extern int elmc_probe(struct net_device *);
extern int skmca_probe(struct net_device *);
extern int elplus_probe(struct net_device *);
extern int ac3200_probe(struct net_device *);
extern int es_probe(struct net_device *);
extern int lne390_probe(struct net_device *);
extern int ne3210_probe(struct net_device *);
extern int e2100_probe(struct net_device *);
extern int ni5010_probe(struct net_device *);
extern int ni52_probe(struct net_device *);
extern int ni65_probe(struct net_device *);
extern int sonic_probe(struct net_device *);
extern int SK_init(struct net_device *);
extern int seeq8005_probe(struct net_device *);
extern int smc_init( struct net_device * );
extern int atarilance_probe(struct net_device *);
extern int sun3lance_probe(struct net_device *);
extern int sun3_82586_probe(struct net_device *);
extern int apne_probe(struct net_device *);
extern int bionet_probe(struct net_device *);
extern int pamsnet_probe(struct net_device *);
extern int cs89x0_probe(struct net_device *dev);
extern int ethertap_probe(struct net_device *dev);
extern int hplance_probe(struct net_device *dev);
extern int bagetlance_probe(struct net_device *);
extern int mvme147lance_probe(struct net_device *dev);
extern int tc515_probe(struct net_device *dev);
extern int lance_probe(struct net_device *dev);
extern int mace_probe(struct net_device *dev);
extern int macsonic_probe(struct net_device *dev);
extern int mac8390_probe(struct net_device *dev);
extern int mac89x0_probe(struct net_device *dev);
extern int mc32_probe(struct net_device *dev);
  
/* Detachable devices ("pocket adaptors") */
extern int de600_probe(struct net_device *);
extern int de620_probe(struct net_device *);

/* FDDI adapters */
extern int skfp_probe(struct net_device *dev);

/* Fibre Channel adapters */
extern int iph5526_probe(struct net_device *dev);

/* SBNI adapters */
extern int sbni_probe(struct net_device *);

struct devprobe
{
	int (*probe)(struct net_device *dev);
	int status;	/* non-zero if autoprobe has failed */
};

/*
 * probe_list walks a list of probe functions and calls each so long
 * as a non-zero ioaddr is given, or as long as it hasn't already failed 
 * to find a card in the past (as recorded by "status") when asked to
 * autoprobe (i.e. a probe that fails to find a card when autoprobing
 * will not be asked to autoprobe again).  It exits when a card is found.
 */
static int __init probe_list(struct net_device *dev, struct devprobe *plist)
{
	struct devprobe *p = plist;
	unsigned long base_addr = dev->base_addr;
#ifdef CONFIG_NET_DIVERT
	int ret;
#endif /* CONFIG_NET_DIVERT */

	while (p->probe != NULL) {
		if (base_addr && p->probe(dev) == 0) {	/* probe given addr */
#ifdef CONFIG_NET_DIVERT
			ret = alloc_divert_blk(dev);
			if (ret)
				return ret;
#endif /* CONFIG_NET_DIVERT */
			return 0;
		} else if (p->status == 0) {		/* has autoprobe failed yet? */
			p->status = p->probe(dev);	/* no, try autoprobe */
			if (p->status == 0) {
#ifdef CONFIG_NET_DIVERT
				ret = alloc_divert_blk(dev);
				if (ret)
					return ret;
#endif /* CONFIG_NET_DIVERT */
				return 0;
			}
		}
		p++;
	}
	return -ENODEV;
}

/*
 * This is a bit of an artificial separation as there are PCI drivers
 * that also probe for EISA cards (in the PCI group) and there are ISA
 * drivers that probe for EISA cards (in the ISA group).  These are the
 * EISA only driver probes, and also the legacy PCI probes
 */
static struct devprobe eisa_probes[] __initdata = {
#ifdef CONFIG_DE4X5             /* DEC DE425, DE434, DE435 adapters */
	{de4x5_probe, 0},
#endif
#ifdef CONFIG_ULTRA32 
	{ultra32_probe, 0},	
#endif
#ifdef CONFIG_AC3200	
	{ac3200_probe, 0},
#endif
#ifdef CONFIG_ES3210
	{es_probe, 0},
#endif
#ifdef CONFIG_LNE390
	{lne390_probe, 0},
#endif
#ifdef CONFIG_NE3210
	{ne3210_probe, 0},
#endif
	{NULL, 0},
};


static struct devprobe mca_probes[] __initdata = {
#ifdef CONFIG_ULTRAMCA 
	{ultramca_probe, 0},
#endif
#ifdef CONFIG_NE2_MCA
	{ne2_probe, 0},
#endif
#ifdef CONFIG_ELMC		/* 3c523 */
	{elmc_probe, 0},
#endif
#ifdef CONFIG_ELMC_II		/* 3c527 */
	{mc32_probe, 0},
#endif
#ifdef CONFIG_SKMC              /* SKnet Microchannel */
        {skmca_probe, 0},
#endif
	{NULL, 0},
};

/*
 * ISA probes that touch addresses < 0x400 (including those that also
 * look for EISA/PCI/MCA cards in addition to ISA cards).
 */
static struct devprobe isa_probes[] __initdata = {
#ifdef CONFIG_EL3		/* ISA, EISA, MCA 3c5x9 */
	{el3_probe, 0},
#endif
#ifdef CONFIG_HP100 		/* ISA, EISA & PCI */
	{hp100_probe, 0},
#endif	
#ifdef CONFIG_3C515
	{tc515_probe, 0},
#endif
#ifdef CONFIG_ULTRA 
	{ultra_probe, 0},
#endif
#ifdef CONFIG_WD80x3 
	{wd_probe, 0},
#endif
#ifdef CONFIG_EL2 		/* 3c503 */
	{el2_probe, 0},
#endif
#ifdef CONFIG_HPLAN
	{hp_probe, 0},
#endif
#ifdef CONFIG_HPLAN_PLUS
	{hp_plus_probe, 0},
#endif
#ifdef CONFIG_E2100		/* Cabletron E21xx series. */
	{e2100_probe, 0},
#endif
#ifdef CONFIG_NE2000		/* ISA (use ne2k-pci for PCI cards) */
	{ne_probe, 0},
#endif
#ifdef CONFIG_LANCE		/* ISA/VLB (use pcnet32 for PCI cards) */
	{lance_probe, 0},
#endif
#ifdef CONFIG_SMC9194
	{smc_init, 0},
#endif
#ifdef CONFIG_SEEQ8005 
	{seeq8005_probe, 0},
#endif
#ifdef CONFIG_AT1500
	{at1500_probe, 0},
#endif
#ifdef CONFIG_CS89x0
 	{cs89x0_probe, 0},
#endif
#ifdef CONFIG_AT1700
	{at1700_probe, 0},
#endif
#ifdef CONFIG_FMV18X		/* Fujitsu FMV-181/182 */
	{fmv18x_probe, 0},
#endif
#ifdef CONFIG_ETH16I
	{eth16i_probe, 0},	/* ICL EtherTeam 16i/32 */
#endif
#ifdef CONFIG_ZNET		/* Zenith Z-Note and some IBM Thinkpads. */
	{znet_probe, 0},
#endif
#ifdef CONFIG_EEXPRESS		/* Intel EtherExpress */
	{express_probe, 0},
#endif
#ifdef CONFIG_EEXPRESS_PRO	/* Intel EtherExpress Pro/10 */
	{eepro_probe, 0},
#endif
#ifdef CONFIG_DEPCA		/* DEC DEPCA */
	{depca_probe, 0},
#endif
#ifdef CONFIG_EWRK3             /* DEC EtherWORKS 3 */
    	{ewrk3_probe, 0},
#endif
#if defined(CONFIG_APRICOT) || defined(CONFIG_MVME16x_NET) || defined(CONFIG_BVME6000_NET)	/* Intel I82596 */
	{i82596_probe, 0},
#endif
#ifdef CONFIG_EL1		/* 3c501 */
	{el1_probe, 0},
#endif
#ifdef CONFIG_WAVELAN		/* WaveLAN */
	{wavelan_probe, 0},
#endif
#ifdef CONFIG_ARLAN		/* Aironet */
	{arlan_probe, 0},
#endif
#ifdef CONFIG_EL16		/* 3c507 */
	{el16_probe, 0},
#endif
#ifdef CONFIG_ELPLUS		/* 3c505 */
	{elplus_probe, 0},
#endif
#ifdef CONFIG_SK_G16
	{SK_init, 0},
#endif
#ifdef CONFIG_NI5010
	{ni5010_probe, 0},
#endif
#ifdef CONFIG_NI52
	{ni52_probe, 0},
#endif
#ifdef CONFIG_NI65
	{ni65_probe, 0},
#endif
	{NULL, 0},
};

static struct devprobe parport_probes[] __initdata = {
#ifdef CONFIG_DE600		/* D-Link DE-600 adapter */
	{de600_probe, 0},
#endif
#ifdef CONFIG_DE620		/* D-Link DE-620 adapter */
	{de620_probe, 0},
#endif
	{NULL, 0},
};

static struct devprobe m68k_probes[] __initdata = {
#ifdef CONFIG_ATARILANCE	/* Lance-based Atari ethernet boards */
	{atarilance_probe, 0},
#endif
#ifdef CONFIG_SUN3LANCE         /* sun3 onboard Lance chip */
	{sun3lance_probe, 0},
#endif
#ifdef CONFIG_SUN3_82586        /* sun3 onboard Intel 82586 chip */
	{sun3_82586_probe, 0},
#endif
#ifdef CONFIG_APNE		/* A1200 PCMCIA NE2000 */
	{apne_probe, 0},
#endif
#ifdef CONFIG_ATARI_BIONET	/* Atari Bionet Ethernet board */
	{bionet_probe, 0},
#endif
#ifdef CONFIG_ATARI_PAMSNET	/* Atari PAMsNet Ethernet board */
	{pamsnet_probe, 0},
#endif
#ifdef CONFIG_HPLANCE		/* HP300 internal Ethernet */
	{hplance_probe, 0},
#endif
#ifdef CONFIG_MVME147_NET	/* MVME147 internal Ethernet */
	{mvme147lance_probe, 0},
#endif
#ifdef CONFIG_MACMACE		/* Mac 68k Quadra AV builtin Ethernet */
	{mace_probe, 0},
#endif
#ifdef CONFIG_MACSONIC		/* Mac SONIC-based Ethernet of all sorts */ 
	{macsonic_probe, 0},
#endif
#ifdef CONFIG_MAC8390           /* NuBus NS8390-based cards */
	{mac8390_probe, 0},
#endif
#ifdef CONFIG_MAC89x0
 	{mac89x0_probe, 0},
#endif
	{NULL, 0},
};

static struct devprobe mips_probes[] __initdata = {
#ifdef CONFIG_MIPS_JAZZ_SONIC
	{sonic_probe, 0},
#endif
#ifdef CONFIG_BAGETLANCE        /* Lance-based Baget ethernet boards */
        {bagetlance_probe, 0},
#endif
	{NULL, 0},
};

/*
 * Unified ethernet device probe, segmented per architecture and
 * per bus interface. This drives the legacy devices only for now.
 */
 
static int __init ethif_probe(struct net_device *dev)
{
	unsigned long base_addr = dev->base_addr;

	/* 
	 * Backwards compatibility - historically an I/O base of 1 was 
	 * used to indicate not to probe for this ethN interface 
	 */
	if (base_addr == 1)
		return 1;		/* ENXIO */

	/* 
	 * The arch specific probes are 1st so that any on-board ethernet
	 * will be probed before other ISA/EISA/MCA/PCI bus cards.
	 */
	if (probe_list(dev, m68k_probes) == 0)
		return 0;
	if (probe_list(dev, mips_probes) == 0)
		return 0;
	if (probe_list(dev, eisa_probes) == 0)
		return 0;
	if (probe_list(dev, mca_probes) == 0)
		return 0;
        /*
         * Backwards compatibility - an I/O of 0xffe0 was used to indicate
         * that we shouldn't do a bunch of potentially risky ISA probes
         * for ethN (N>1).  Since the widespread use of modules, *nobody*
         * compiles a kernel with all the ISA drivers built in anymore,
         * and so we should delete this check in linux 2.3 - Paul G.
         */
	if (base_addr != 0xffe0 && probe_list(dev, isa_probes) == 0) 
		return 0;
	if (probe_list(dev, parport_probes) == 0)
		return 0;
	return -ENODEV;
}

#ifdef CONFIG_FDDI
static int __init fddiif_probe(struct net_device *dev)
{
    unsigned long base_addr = dev->base_addr;

    if (base_addr == 1)
	    return 1;		/* ENXIO */

    if (1
#ifdef CONFIG_APFDDI
	&& apfddi_init(dev)
#endif
#ifdef CONFIG_SKFP
	&& skfp_probe(dev)
#endif
	&& 1 ) {
	    return 1;	/* -ENODEV or -EAGAIN would be more accurate. */
    }
    return 0;
}
#endif


#ifdef CONFIG_NET_FC
static int fcif_probe(struct net_device *dev)
{
	if (dev->base_addr == -1)
		return 1;

	if (1
#ifdef CONFIG_IPHASE5526
	    && iph5526_probe(dev)
#endif
	    && 1 ) {
		return 1; /* -ENODEV or -EAGAIN would be more accurate. */
	}
	return 0;
}
#endif  /* CONFIG_NET_FC */


#ifdef CONFIG_ETHERTAP
    static struct net_device tap0_dev = { "tap0", 0, 0, 0, 0, NETLINK_TAPBASE, 0, 0, 0, 0, NEXT_DEV, ethertap_probe, };
#   undef NEXT_DEV
#   define NEXT_DEV	(&tap0_dev)
#endif

#ifdef CONFIG_SDLA
    extern int sdla_init(struct net_device *);
    static struct net_device sdla0_dev = { "sdla0", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, sdla_init, };

#   undef NEXT_DEV
#   define NEXT_DEV	(&sdla0_dev)
#endif

#if defined(CONFIG_LTPC)
    extern int ltpc_probe(struct net_device *);
    static struct net_device dev_ltpc = {
        "lt0",
                0, 0, 0, 0,
                0x0, 0,
                0, 0, 0, NEXT_DEV, ltpc_probe };
#   undef NEXT_DEV
#   define NEXT_DEV	(&dev_ltpc)
#endif  /* LTPC */

#if defined(CONFIG_COPS)
    extern int cops_probe(struct net_device *);
    static struct net_device cops2_dev = { "lt2", 0, 0, 0, 0, 0x0, 0, 0, 0, 0, NEXT_DEV, cops_probe };
    static struct net_device cops1_dev = { "lt1", 0, 0, 0, 0, 0x0, 0, 0, 0, 0, &cops2_dev, cops_probe };
    static struct net_device cops0_dev = { "lt0", 0, 0, 0, 0, 0x0, 0, 0, 0, 0, &cops1_dev, cops_probe };
#   undef NEXT_DEV
#   define NEXT_DEV     (&cops0_dev)
#endif  /* COPS */


/* The first device defaults to I/O base '0', which means autoprobe. */
#ifndef ETH0_ADDR
# define ETH0_ADDR 0
#endif
#ifndef ETH0_IRQ
# define ETH0_IRQ 0
#endif

/* "eth0" defaults to autoprobe (== 0), other use a base of 0xffe0 (== -0x20),
   which means "don't do ISA probes".  Distributions don't ship kernels with
   all ISA drivers compiled in anymore, so its probably no longer an issue. */

#define ETH_NOPROBE_ADDR 0xffe0

static struct net_device eth7_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, NEXT_DEV, ethif_probe };
static struct net_device eth6_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, &eth7_dev, ethif_probe };
static struct net_device eth5_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, &eth6_dev, ethif_probe };
static struct net_device eth4_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, &eth5_dev, ethif_probe };
static struct net_device eth3_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, &eth4_dev, ethif_probe };
static struct net_device eth2_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, &eth3_dev, ethif_probe };
static struct net_device eth1_dev = {
    "eth%d", 0,0,0,0,ETH_NOPROBE_ADDR /* I/O base*/, 0,0,0,0, &eth2_dev, ethif_probe };

static struct net_device eth0_dev = {
    "eth%d", 0, 0, 0, 0, ETH0_ADDR, ETH0_IRQ, 0, 0, 0, &eth1_dev, ethif_probe };

#   undef NEXT_DEV
#   define NEXT_DEV	(&eth0_dev)



#ifdef CONFIG_TR
/* Token-ring device probe */
extern int ibmtr_probe(struct net_device *);
extern int smctr_probe(struct net_device *);

static int
trif_probe(struct net_device *dev)
{
    if (1
#ifdef CONFIG_IBMTR
	&& ibmtr_probe(dev)
#endif
#ifdef CONFIG_SMCTR
	&& smctr_probe(dev)
#endif
	&& 1 ) {
	return 1;	/* -ENODEV or -EAGAIN would be more accurate. */
    }
    return 0;
}
static struct net_device tr7_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, NEXT_DEV, trif_probe };
static struct net_device tr6_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr7_dev, trif_probe };
static struct net_device tr5_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr6_dev, trif_probe };
static struct net_device tr4_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr5_dev, trif_probe };
static struct net_device tr3_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr4_dev, trif_probe };
static struct net_device tr2_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr3_dev, trif_probe };
static struct net_device tr1_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr2_dev, trif_probe };
static struct net_device tr0_dev = {
    "tr%d",0,0,0,0,0,0,0,0,0, &tr1_dev, trif_probe };
#   undef       NEXT_DEV
#   define      NEXT_DEV        (&tr0_dev)

#endif 

#ifdef CONFIG_FDDI
	static struct net_device fddi7_dev =
		{"fddi7", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, fddiif_probe};
	static struct net_device fddi6_dev =
		{"fddi6", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi7_dev, fddiif_probe};
	static struct net_device fddi5_dev =
		{"fddi5", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi6_dev, fddiif_probe};
	static struct net_device fddi4_dev =
		{"fddi4", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi5_dev, fddiif_probe};
	static struct net_device fddi3_dev =
		{"fddi3", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi4_dev, fddiif_probe};
	static struct net_device fddi2_dev =
		{"fddi2", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi3_dev, fddiif_probe};
	static struct net_device fddi1_dev =
		{"fddi1", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi2_dev, fddiif_probe};
	static struct net_device fddi0_dev =
		{"fddi0", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fddi1_dev, fddiif_probe};
#undef	NEXT_DEV
#define	NEXT_DEV	(&fddi0_dev)
#endif 


#ifdef CONFIG_NET_FC
    static struct net_device fc1_dev = {
        "fc1", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, fcif_probe};
	static struct net_device fc0_dev = {
		"fc0", 0, 0, 0, 0, 0, 0, 0, 0, 0, &fc1_dev, fcif_probe};
#   undef       NEXT_DEV
#   define      NEXT_DEV        (&fc0_dev)
#endif


#ifdef CONFIG_SBNI
	static struct net_device sbni7_dev =
		{"sbni7", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, sbni_probe};
	static struct net_device sbni6_dev =
		{"sbni6", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni7_dev, sbni_probe};
	static struct net_device sbni5_dev =
		{"sbni5", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni6_dev, sbni_probe};
	static struct net_device sbni4_dev =
		{"sbni4", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni5_dev, sbni_probe};
	static struct net_device sbni3_dev =
		{"sbni3", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni4_dev, sbni_probe};
	static struct net_device sbni2_dev =
		{"sbni2", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni3_dev, sbni_probe};
	static struct net_device sbni1_dev =
		{"sbni1", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni2_dev, sbni_probe};
	static struct net_device sbni0_dev =
		{"sbni0", 0, 0, 0, 0, 0, 0, 0, 0, 0, &sbni1_dev, sbni_probe};

#undef	NEXT_DEV
#define	NEXT_DEV	(&sbni0_dev)
#endif 
	
/*
 *	The loopback device is global so it can be directly referenced
 *	by the network code. Also, it must be first on device list.
 */

extern int loopback_init(struct net_device *dev);
struct net_device loopback_dev = 
	{"lo", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, loopback_init};

/*
 * The @dev_base list is protected by @dev_base_lock and the rtln
 * semaphore.
 *
 * Pure readers hold dev_base_lock for reading.
 *
 * Writers must hold the rtnl semaphore while they loop through the
 * dev_base list, and hold dev_base_lock for writing when they do the
 * actual updates.  This allows pure readers to access the list even
 * while a writer is preparing to update it.
 *
 * To put it another way, dev_base_lock is held for writing only to
 * protect against pure readers; the rtnl semaphore provides the
 * protection against other writers.
 *
 * See, for example usages, register_netdevice() and
 * unregister_netdevice(), which must be called with the rtnl
 * semaphore held.
 */
struct net_device *dev_base = &loopback_dev;
rwlock_t dev_base_lock = RW_LOCK_UNLOCKED;

