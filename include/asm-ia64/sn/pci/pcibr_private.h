/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_PCIBR_PRIVATE_H
#define _ASM_SN_PCI_PCIBR_PRIVATE_H

/*
 * pcibr_private.h -- private definitions for pcibr
 * only the pcibr driver (and its closest friends)
 * should ever peek into this file.
 */

#include <linux/config.h>
#include <linux/pci.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pciio_private.h>
#include <asm/sn/ksys/l1.h>

/*
 * convenience typedefs
 */

typedef uint64_t pcibr_DMattr_t;
typedef uint32_t pcibr_ATEattr_t;

typedef struct pcibr_info_s *pcibr_info_t, **pcibr_info_h;
typedef struct pcibr_soft_s *pcibr_soft_t;
typedef struct pcibr_soft_slot_s *pcibr_soft_slot_t;
typedef struct pcibr_hints_s *pcibr_hints_t;
typedef struct pcibr_intr_list_s *pcibr_intr_list_t;
typedef struct pcibr_intr_wrap_s *pcibr_intr_wrap_t;
typedef struct pcibr_intr_cbuf_s *pcibr_intr_cbuf_t;

typedef volatile unsigned *cfg_p;
typedef volatile bridgereg_t *reg_p;

/*
 * extern functions
 */
cfg_p pcibr_slot_config_addr(bridge_t *, pciio_slot_t, int);
cfg_p pcibr_func_config_addr(bridge_t *, pciio_bus_t bus, pciio_slot_t, pciio_function_t, int);
unsigned pcibr_slot_config_get(bridge_t *, pciio_slot_t, int);
unsigned pcibr_func_config_get(bridge_t *, pciio_slot_t, pciio_function_t, int);
void pcibr_debug(uint32_t, vertex_hdl_t, char *, ...);
void pcibr_slot_config_set(bridge_t *, pciio_slot_t, int, unsigned);
void pcibr_func_config_set(bridge_t *, pciio_slot_t, pciio_function_t, int, 
								unsigned);
/*
 * PCIBR_DEBUG() macro and debug bitmask defines
 */
/* low freqency debug events (ie. initialization, resource allocation,...) */
#define PCIBR_DEBUG_INIT	0x00000001  /* bridge init */
#define PCIBR_DEBUG_HINTS	0x00000002  /* bridge hints */
#define PCIBR_DEBUG_ATTACH	0x00000004  /* bridge attach */
#define PCIBR_DEBUG_DETACH	0x00000008  /* bridge detach */
#define PCIBR_DEBUG_ATE		0x00000010  /* bridge ATE allocation */
#define PCIBR_DEBUG_RRB		0x00000020  /* bridge RRB allocation */
#define PCIBR_DEBUG_RBAR	0x00000040  /* bridge RBAR allocation */
#define PCIBR_DEBUG_PROBE	0x00000080  /* bridge device probing */
#define PCIBR_DEBUG_INTR_ERROR  0x00000100  /* bridge error interrupt */
#define PCIBR_DEBUG_ERROR_HDLR  0x00000200  /* bridge error handler */
#define PCIBR_DEBUG_CONFIG	0x00000400  /* device's config space */
#define PCIBR_DEBUG_BAR		0x00000800  /* device's BAR allocations */
#define PCIBR_DEBUG_INTR_ALLOC	0x00001000  /* device's intr allocation */
#define PCIBR_DEBUG_DEV_ATTACH	0x00002000  /* device's attach */
#define PCIBR_DEBUG_DEV_DETACH	0x00004000  /* device's detach */
#define PCIBR_DEBUG_HOTPLUG	0x00008000

/* high freqency debug events (ie. map allocation, direct translation,...) */
#define PCIBR_DEBUG_DEVREG	0x04000000  /* bridges device reg sets */
#define PCIBR_DEBUG_PIOMAP	0x08000000  /* pcibr_piomap */
#define PCIBR_DEBUG_PIODIR	0x10000000  /* pcibr_piotrans */
#define PCIBR_DEBUG_DMAMAP	0x20000000  /* pcibr_dmamap */
#define PCIBR_DEBUG_DMADIR	0x40000000  /* pcibr_dmatrans */
#define PCIBR_DEBUG_INTR	0x80000000  /* interrupts */

extern char	 *pcibr_debug_module;
extern int	  pcibr_debug_widget;
extern int	  pcibr_debug_slot;
extern uint32_t pcibr_debug_mask;

/* For low frequency events (ie. initialization, resource allocation,...) */
#define PCIBR_DEBUG_ALWAYS(args) pcibr_debug args ;

/* XXX: habeck: maybe make PCIBR_DEBUG() always available?  Even in non-
 * debug kernels?  If tracing isn't enabled (i.e pcibr_debug_mask isn't
 * set, then the overhead for this macro is just an extra 'if' check.
 */
/* For high frequency events (ie. map allocation, direct translation,...) */
#if 1 || DEBUG
#define PCIBR_DEBUG(args) PCIBR_DEBUG_ALWAYS(args)
#else	/* DEBUG */
#define PCIBR_DEBUG(args)
#endif	/* DEBUG */

/*
 * Bridge sets up PIO using this information.
 */
struct pcibr_piomap_s {
    struct pciio_piomap_s   bp_pp;	/* generic stuff */

#define	bp_flags	bp_pp.pp_flags	/* PCIBR_PIOMAP flags */
#define	bp_dev		bp_pp.pp_dev	/* associated pci card */
#define	bp_slot		bp_pp.pp_slot	/* which slot the card is in */
#define	bp_space	bp_pp.pp_space	/* which address space */
#define	bp_pciaddr	bp_pp.pp_pciaddr	/* starting offset of mapping */
#define	bp_mapsz	bp_pp.pp_mapsz	/* size of this mapping */
#define	bp_kvaddr	bp_pp.pp_kvaddr	/* kernel virtual address to use */

    iopaddr_t               bp_xtalk_addr;	/* corresponding xtalk address */
    xtalk_piomap_t          bp_xtalk_pio;	/* corresponding xtalk resource */
    pcibr_piomap_t	    bp_next;	/* Next piomap on the list */
    pcibr_soft_t	    bp_soft;	/* backpointer to bridge soft data */
    atomic_t		    bp_toc[1];	/* PCI timeout counter */

};

/*
 * Bridge sets up DMA using this information.
 */
struct pcibr_dmamap_s {
    struct pciio_dmamap_s   bd_pd;
#define	bd_flags	bd_pd.pd_flags	/* PCIBR_DMAMAP flags */
#define	bd_dev		bd_pd.pd_dev	/* associated pci card */
#define	bd_slot		bd_pd.pd_slot	/* which slot the card is in */
    struct pcibr_soft_s    *bd_soft;	/* pcibr soft state backptr */
    xtalk_dmamap_t          bd_xtalk;	/* associated xtalk resources */

    size_t                  bd_max_size;	/* maximum size of mapping */
    xwidgetnum_t            bd_xio_port;	/* target XIO port */
    iopaddr_t               bd_xio_addr;	/* target XIO address */
    iopaddr_t               bd_pci_addr;	/* via PCI address */

    int                     bd_ate_index;	/* Address Translation Entry Index */
    int                     bd_ate_count;	/* number of ATE's allocated */
    bridge_ate_p            bd_ate_ptr;		/* where to write first ATE */
    bridge_ate_t            bd_ate_proto;	/* prototype ATE (for xioaddr=0) */
    bridge_ate_t            bd_ate_prime;	/* value of 1st ATE written */
};

#define	IBUFSIZE	5		/* size of circular buffer (holds 4) */

/*
 * Circular buffer used for interrupt processing
 */
struct pcibr_intr_cbuf_s {
    spinlock_t		ib_lock;		/* cbuf 'put' lock */
    int			ib_in;			/* index of next free entry */
    int			ib_out;			/* index of next full entry */
    pcibr_intr_wrap_t   ib_cbuf[IBUFSIZE];	/* circular buffer of wrap  */
};

/*
 * Bridge sets up interrupts using this information.
 */

struct pcibr_intr_s {
    struct pciio_intr_s     bi_pi;
#define	bi_flags	bi_pi.pi_flags	/* PCIBR_INTR flags */
#define	bi_dev		bi_pi.pi_dev	/* associated pci card */
#define	bi_lines	bi_pi.pi_lines	/* which PCI interrupt line(s) */
#define bi_func		bi_pi.pi_func	/* handler function (when connected) */
#define bi_arg		bi_pi.pi_arg	/* handler parameter (when connected) */
#define bi_mustruncpu	bi_pi.pi_mustruncpu /* Where we must run. */
#define bi_irq		bi_pi.pi_irq	/* IRQ assigned. */
#define bi_cpu		bi_pi.pi_cpu	/* cpu assigned. */
    unsigned                bi_ibits;	/* which Bridge interrupt bit(s) */
    pcibr_soft_t            bi_soft;	/* shortcut to soft info */
    struct pcibr_intr_cbuf_s bi_ibuf;	/* circular buffer of wrap ptrs */
    unsigned		bi_last_intr;	/* For Shub lb lost intr. bug */
};


/* 
 * PCIBR_INFO_SLOT_GET_EXT returns the external slot number that the card
 * resides in.  (i.e the slot number silk screened on the back of the I/O 
 * brick).  PCIBR_INFO_SLOT_GET_INT returns the internal slot (or device)
 * number used by the pcibr code to represent that external slot (i.e to 
 * set bit patterns in BRIDGE/PIC registers to represent the device, or to
 * offset into an array, or ...).
 *
 * In BRIDGE and XBRIDGE the external slot and internal device numbering 
 * are the same.  (0->0, 1->1, 2->2,... 7->7)  BUT in the PIC the external
 * slot number is always 1 greater than the internal device number (1->0, 
 * 2->1, 3->2, 4->3).  This is due to the fact that the PCI-X spec requires
 * that the 'bridge' (i.e PIC) be designated as 'device 0', thus external
 * slot numbering can't start at zero.
 *
 * PCIBR_DEVICE_TO_SLOT converts an internal device number to an external
 * slot number.  NOTE: PCIIO_SLOT_NONE stays as PCIIO_SLOT_NONE.
 *
 * PCIBR_SLOT_TO_DEVICE converts an external slot number to an internal
 * device number.  NOTE: PCIIO_SLOT_NONE stays as PCIIO_SLOT_NONE.
 */
#define PCIBR_INFO_SLOT_GET_EXT(info)	    (((pcibr_info_t)info)->f_slot)
#define PCIBR_INFO_SLOT_GET_INT(info)	    (((pcibr_info_t)info)->f_dev)

#define PCIBR_DEVICE_TO_SLOT(pcibr_soft, dev_num) \
	(((dev_num) != PCIIO_SLOT_NONE) ? \
	    (IS_PIC_SOFT((pcibr_soft)) ? ((dev_num) + 1) : (dev_num)) : \
	    PCIIO_SLOT_NONE)

#define PCIBR_SLOT_TO_DEVICE(pcibr_soft, slot) \
        (((slot) != PCIIO_SLOT_NONE) ? \
            (IS_PIC_SOFT((pcibr_soft)) ? ((slot) - 1) : (slot)) : \
            PCIIO_SLOT_NONE)

/*
 * per-connect point pcibr data, including standard pciio data in-line:
 */
struct pcibr_info_s {
    struct pciio_info_s	    f_c;	/* MUST BE FIRST. */
#define	f_vertex	f_c.c_vertex	/* back pointer to vertex */
#define	f_bus		f_c.c_bus	/* which bus the card is in */
#define	f_slot		f_c.c_slot	/* which slot the card is in */
#define	f_func		f_c.c_func	/* which func (on multi-func cards) */
#define	f_vendor	f_c.c_vendor	/* PCI card "vendor" code */
#define	f_device	f_c.c_device	/* PCI card "device" code */
#define	f_master	f_c.c_master	/* PCI bus provider */
#define	f_mfast		f_c.c_mfast	/* cached fastinfo from c_master */
#define	f_pops		f_c.c_pops	/* cached provider from c_master */
#define	f_efunc		f_c.c_efunc	/* error handling function */
#define	f_einfo		f_c.c_einfo	/* first parameter for efunc */
#define f_window        f_c.c_window    /* state of BASE regs */
#define	f_rwindow	f_c.c_rwindow	/* expansion ROM BASE regs */
#define	f_rbase		f_c.c_rbase	/* expansion ROM base */
#define	f_rsize		f_c.c_rsize	/* expansion ROM size */
#define f_piospace      f_c.c_piospace  /* additional I/O spaces allocated */

    /* pcibr-specific connection state */
    int			    f_ibit[4];	/* Bridge bit for each INTx */
    pcibr_piomap_t	    f_piomap;
    int                     f_att_det_error;
    pciio_slot_t	    f_dev;	/* which device the card represents */
    cap_pcix_type0_t	   *f_pcix_cap;	/* pointer to the pcix capability */
};

/* =====================================================================
 *          Shared Interrupt Information
 */

struct pcibr_intr_list_s {
    pcibr_intr_list_t       il_next;
    pcibr_intr_t            il_intr;
    volatile bridgereg_t   *il_wrbf;	/* ptr to b_wr_req_buf[] */
};

/* =====================================================================
 *          Interrupt Wrapper Data
 */
struct pcibr_intr_wrap_s {
    pcibr_soft_t            iw_soft;	/* which bridge */
    volatile bridgereg_t   *iw_stat;	/* ptr to b_int_status */
    bridgereg_t             iw_ibit;	/* bit in b_int_status */
    pcibr_intr_list_t       iw_list;	/* ghostbusters! */
    int			    iw_hdlrcnt;	/* running handler count */
    int			    iw_shared;  /* if Bridge bit is shared */
    int			    iw_connected; /* if already connected */
};

#define	PCIBR_ISR_ERR_START		8
#define PCIBR_ISR_MAX_ERRS_BRIDGE 	32
#define PCIBR_ISR_MAX_ERRS_PIC		45
#define PCIBR_ISR_MAX_ERRS	PCIBR_ISR_MAX_ERRS_PIC

/*
 * PCI Base Address Register window allocation constants.
 * To reduce the size of the internal resource mapping structures, do
 * not use the entire PCI bus I/O address space
 */ 
#define PCIBR_BUS_IO_BASE      0x100000
#define PCIBR_BUS_IO_MAX       0x0FFFFFFF
#define PCIBR_BUS_IO_PAGE      0x100000

#define PCIBR_BUS_SWIN_BASE    PAGE_SIZE
#define PCIBR_BUS_SWIN_MAX     0x000FFFFF
#define PCIBR_BUS_SWIN_PAGE    PAGE_SIZE

#define PCIBR_BUS_MEM_BASE     0x200000
#define PCIBR_BUS_MEM_MAX      0x3FFFFFFF
#define PCIBR_BUS_MEM_PAGE     0x100000

/* defines for pcibr_soft_s->bs_bridge_type */
#define PCIBR_BRIDGETYPE_BRIDGE		0
#define PCIBR_BRIDGETYPE_XBRIDGE	1
#define PCIBR_BRIDGETYPE_PIC		2
#define IS_XBRIDGE_SOFT(ps) (ps->bs_bridge_type == PCIBR_BRIDGETYPE_XBRIDGE)
#define IS_PIC_SOFT(ps)     (ps->bs_bridge_type == PCIBR_BRIDGETYPE_PIC)
#define IS_PIC_BUSNUM_SOFT(ps, bus)	\
		(IS_PIC_SOFT(ps) && ((ps)->bs_busnum == (bus)))
#define IS_BRIDGE_SOFT(ps)  (ps->bs_bridge_type == PCIBR_BRIDGETYPE_BRIDGE)
#define IS_XBRIDGE_OR_PIC_SOFT(ps) (IS_XBRIDGE_SOFT(ps) || IS_PIC_SOFT(ps))

/*
 * Runtime checks for workarounds.
 */
#define PCIBR_WAR_ENABLED(pv, pcibr_soft) \
	((1 << XWIDGET_PART_REV_NUM_REV(pcibr_soft->bs_rev_num)) & pv)
/*
 * Defines for individual WARs. Each is a bitmask of applicable
 * part revision numbers. (1 << 1) == rev A, (1 << 2) == rev B, etc.
 */
#define PV854697 (~0)     /* PIC: write 64bit regs as 64bits. permanent */
#define PV854827 (~0)     /* PIC: fake widget 0xf presence bit. permanent */
#define PV855271 (1 << 1) /* PIC: PIC: use virt chan iff 64-bit device. */
#define PV855272 (1 << 1) /* PIC: runaway interrupt WAR */
#define PV856155 (1 << 1) /* PIC: arbitration WAR */
#define PV856864 (1 << 1) /* PIC: lower timeout to free TNUMs quicker */
#define PV856866 (1 << 1) /* PIC: avoid rrb's 0/1/8/9. */
#define PV862253 (1 << 1) /* PIC: don't enable write req RAM parity checking */
#define PV867308 (3 << 1) /* PIC: make LLP error interrupts FATAL for PIC */


/* defines for pcibr_soft_s->bs_bridge_mode */
#define PCIBR_BRIDGEMODE_PCI_33		0x0
#define PCIBR_BRIDGEMODE_PCI_66		0x2
#define PCIBR_BRIDGEMODE_PCIX_66	0x3
#define PCIBR_BRIDGEMODE_PCIX_100	0x5
#define PCIBR_BRIDGEMODE_PCIX_133	0x7
#define BUSSPEED_MASK			0x6
#define BUSTYPE_MASK			0x1

#define IS_PCI(ps)	(!IS_PCIX(ps))
#define IS_PCIX(ps)	((ps)->bs_bridge_mode & BUSTYPE_MASK)

#define IS_33MHZ(ps)	((ps)->bs_bridge_mode == PCIBR_BRIDGEMODE_PCI_33)
#define IS_66MHZ(ps)	(((ps)->bs_bridge_mode == PCIBR_BRIDGEMODE_PCI_66) || \
			 ((ps)->bs_bridge_mode == PCIBR_BRIDGEMODE_PCIX_66))
#define IS_100MHZ(ps)	((ps)->bs_bridge_mode == PCIBR_BRIDGEMODE_PCIX_100)
#define IS_133MHZ(ps)	((ps)->bs_bridge_mode == PCIBR_BRIDGEMODE_PCIX_133)


/* Number of PCI slots.   NOTE: this works as long as the first slot
 * is zero.  Otherwise use ((ps->bs_max_slot+1) - ps->bs_min_slot)
 */
#define PCIBR_NUM_SLOTS(ps) (ps->bs_max_slot+1)

/* =====================================================================
 *            Bridge Device State structure
 *
 *      one instance of this structure is kept for each
 *      Bridge ASIC in the system.
 */

struct pcibr_soft_s {
    vertex_hdl_t          bs_conn;		/* xtalk connection point */
    vertex_hdl_t          bs_vhdl;		/* vertex owned by pcibr */
    uint64_t                bs_int_enable;	/* Mask of enabled intrs */
    bridge_t               *bs_base;		/* PIO pointer to Bridge chip */
    char                   *bs_name;		/* hw graph name */
    xwidgetnum_t            bs_xid;		/* Bridge's xtalk ID number */
    vertex_hdl_t          bs_master;		/* xtalk master vertex */
    xwidgetnum_t            bs_mxid;		/* master's xtalk ID number */
    pciio_slot_t            bs_first_slot;      /* first existing slot */
    pciio_slot_t            bs_last_slot;       /* last existing slot */
    pciio_slot_t            bs_last_reset;      /* last slot to reset */
    pciio_slot_t	    bs_min_slot;	/* lowest possible slot */
    pciio_slot_t	    bs_max_slot;	/* highest possible slot */
    pcibr_soft_t	    bs_peers_soft;	/* PICs other bus's soft */
    int			    bs_busnum;		/* PIC has two pci busses */

    iopaddr_t               bs_dir_xbase;	/* xtalk address for 32-bit PCI direct map */
    xwidgetnum_t	    bs_dir_xport;	/* xtalk port for 32-bit PCI direct map */

    struct resource	    bs_int_ate_resource;/* root resource for internal ATEs */
    struct resource	    bs_ext_ate_resource;/* root resource for external ATEs */
    void	 	    *bs_allocated_ate_res;/* resource struct allocated */
    short		    bs_int_ate_size;	/* number of internal ates */
    short		    bs_bridge_type;	/* see defines above */
    short		    bs_bridge_mode;	/* see defines above */
    int                     bs_rev_num;		/* revision number of Bridge */

    /* bs_dma_flags are the forced dma flags used on all DMAs. Used for
     * working around ASIC rev issues and protocol specific requirements
     */
    unsigned                bs_dma_flags;	/* forced DMA flags */

    moduleid_t		    bs_moduleid;	/* io brick moduleid */
    short		    bs_bricktype;	/* io brick type */

    /*
     * Lock used primarily to get mutual exclusion while managing any
     * bridge resources..
     */
    spinlock_t              bs_lock;
    
    vertex_hdl_t	    bs_noslot_conn;	/* NO-SLOT connection point */
    pcibr_info_t	    bs_noslot_info;
    struct pcibr_soft_slot_s {
	/* information we keep about each CFG slot */

	/* some devices (ioc3 in non-slotted
	 * configurations, sometimes) make use
	 * of more than one REQ/GNT/INT* signal
	 * sets. The slot corresponding to the
	 * IDSEL that the device responds to is
	 * called the host slot; the slot
	 * numbers that the device is stealing
	 * REQ/GNT/INT bits from are known as
	 * the guest slots.
	 */
	int                     has_host;
	pciio_slot_t            host_slot;
	vertex_hdl_t		slot_conn;

        /* PCI Hot-Plug status word */
        int 			slot_status;

	/* Potentially several connection points
	 * for this slot. bss_ninfo is how many,
	 * and bss_infos is a pointer to
	 * an array pcibr_info_t values (which are
	 * pointers to pcibr_info structs, stored
	 * as device_info in connection ponts).
	 */
	int			bss_ninfo;
	pcibr_info_h	        bss_infos;

	/* Temporary Compatibility Macros, for
	 * stuff that has moved out of bs_slot
	 * and into the info structure. These
	 * will go away when their users have
	 * converted over to multifunction-
	 * friendly use of bss_{ninfo,infos}.
	 */
#define	bss_vendor_id	bss_infos[0]->f_vendor
#define	bss_device_id	bss_infos[0]->f_device
#define	bss_window	bss_infos[0]->f_window
#define	bssw_space	w_space
#define	bssw_base	w_base
#define	bssw_size	w_size

	/* Where is DevIO(x) pointing? */
	/* bssd_space is NONE if it is not assigned. */
	struct {
	    pciio_space_t           bssd_space;
	    iopaddr_t               bssd_base;
            int                     bssd_ref_cnt;
	} bss_devio;

	/* Shadow value for Device(x) register,
	 * so we don't have to go to the chip.
	 */
	bridgereg_t             bss_device;

	/* Number of sets on GBR/REALTIME bit outstanding
	 * Used by Priority I/O for tracking reservations
	 */
	int                     bss_pri_uctr;

	/* Number of "uses" of PMU, 32-bit direct,
	 * and 64-bit direct DMA (0:none, <0: trans,
	 * >0: how many dmamaps). Device(x) bits
	 * controlling attribute of each kind of
	 * channel can't be changed by dmamap_alloc
	 * or dmatrans if the controlling counter
	 * is nonzero. dmatrans is forever.
	 */
	int                     bss_pmu_uctr;
	int                     bss_d32_uctr;
	int                     bss_d64_uctr;

	/* When the contents of mapping configuration
	 * information is locked down by dmatrans,
	 * repeated checks of the same flags should
	 * be shortcircuited for efficiency.
	 */
	iopaddr_t		bss_d64_base;
	unsigned		bss_d64_flags;
	iopaddr_t		bss_d32_base;
	unsigned		bss_d32_flags;

	/* Shadow information used for implementing
	 * Bridge Hardware WAR #484930
	 */
	atomic_t		bss_ext_ates_active;
        volatile unsigned      *bss_cmd_pointer;
	unsigned		bss_cmd_shadow;

    } bs_slot[8];

    pcibr_intr_bits_f	       *bs_intr_bits;

    /* PIC PCI-X Read Buffer Management :
     * bs_pcix_num_funcs: the total number of PCI-X functions
     *  on the bus
     * bs_pcix_split_tot: total number of outstanding split
     *  transactions requested by all functions on the bus
     * bs_pcix_rbar_percent_allowed: the percentage of the
     *  total number of buffers a function requested that are 
     *  available to it, not including the 1 RBAR guaranteed 
     *  to it.
     * bs_pcix_rbar_inuse: number of RBARs in use.
     * bs_pcix_rbar_avail: number of RBARs available.  NOTE:
     *  this value can go negative if we oversubscribe the 
     *  RBARs.  (i.e.  We have 16 RBARs but 17 functions).
     */
    int			    bs_pcix_num_funcs;
    int			    bs_pcix_split_tot;
    int			    bs_pcix_rbar_percent_allowed;

    int			    bs_pcix_rbar_inuse;
    int			    bs_pcix_rbar_avail;


    /* RRB MANAGEMENT
     * bs_rrb_fixed: bitmap of slots whose RRB
     *	allocations we should not "automatically" change
     * bs_rrb_avail: number of RRBs that have not
     *  been allocated or reserved for {even,odd} slots
     * bs_rrb_res: number of RRBs currently reserved for the
     *	use of the index slot number
     * bs_rrb_res_dflt: number of RRBs reserved at boot
     *  time for the use of the index slot number
     * bs_rrb_valid: number of RRBs currently marked valid
     *	for the indexed slot/vchan number; array[slot][vchan]
     * bs_rrb_valid_dflt: number of RRBs marked valid at boot
     *  time for the indexed slot/vchan number; array[slot][vchan]
     */
    int                     bs_rrb_fixed;
    int                     bs_rrb_avail[2];
    int                     bs_rrb_res[8];
    int                     bs_rrb_res_dflt[8];
    int			    bs_rrb_valid[8][4];
    int			    bs_rrb_valid_dflt[8][4];
    struct {
	/* Each Bridge interrupt bit has a single XIO
	 * interrupt channel allocated.
	 */
	xtalk_intr_t            bsi_xtalk_intr;
	/*
	 * A wrapper structure is associated with each
	 * Bridge interrupt bit.
	 */
	struct pcibr_intr_wrap_s  bsi_pcibr_intr_wrap;

    } bs_intr[8];

    xtalk_intr_t		bsi_err_intr;

    /*
     * We stash away some information in this structure on getting
     * an error interrupt. This information is used during PIO read/
     * write error handling.
     *
     * As it stands now, we do not re-enable the error interrupt
     * till the error is resolved. Error resolution happens either at
     * bus error time for PIO Read errors (~100 microseconds), or at
     * the scheduled timeout time for PIO write errors (~milliseconds).
     * If this delay causes problems, we may need to move towards
     * a different scheme..
     *
     * Note that there is no locking while looking at this data structure.
     * There should not be any race between bus error code and
     * error interrupt code.. will look into this if needed.
     *
     * NOTE: The above discussion of error interrupt processing is
     *       no longer true. Whether it should again be true, is
     *       being looked into.
     */
    struct br_errintr_info {
	int                     bserr_toutcnt;
#ifdef LATER
	toid_t                  bserr_toutid;	/* Timeout started by errintr */
#endif	/* LATER */
	iopaddr_t               bserr_addr;	/* Address where error occured */
	uint64_t		bserr_intstat;	/* interrupts active at error dump */
    } bs_errinfo;

    /*
     * PCI Bus Space allocation data structure.
     *
     * The resource mapping functions rmalloc() and rmfree() are used
     * to manage the PCI bus I/O, small window, and memory  address 
     * spaces.
     *
     * This info is used to assign PCI bus space addresses to cards
     * via their BARs and to the callers of the pcibr_piospace_alloc()
     * interface.
     *
     * Users of the pcibr_piospace_alloc() interface, such as the VME
     * Universe chip, need PCI bus space that is not acquired by BARs.
     * Most of these users need "large" amounts of PIO space (typically
     * in Megabytes), and they generally tend to take once and never
     * release. 
     */
    struct pciio_win_map_s	bs_io_win_map;	/* I/O addr space */
    struct pciio_win_map_s	bs_swin_map;	/* Small window addr space */
    struct pciio_win_map_s	bs_mem_win_map;	/* Memory addr space */

    struct resource		bs_io_win_root_resource; /* I/O addr space */
    struct resource		bs_swin_root_resource; /* Small window addr space */
    struct resource		bs_mem_win_root_resource; /* Memory addr space */

    int                   bs_bus_addr_status;    /* Bus space status */

#define PCIBR_BUS_ADDR_MEM_FREED       1  /* Reserved PROM mem addr freed */
#define PCIBR_BUS_ADDR_IO_FREED        2  /* Reserved PROM I/O addr freed */

    struct bs_errintr_stat_s {
	uint32_t              bs_errcount_total;
	uint32_t              bs_lasterr_timestamp;
	uint32_t              bs_lasterr_snapshot;
    } bs_errintr_stat[PCIBR_ISR_MAX_ERRS];

    /*
     * Bridge-wide endianness control for
     * large-window PIO mappings
     *
     * These fields are set to PCIIO_BYTE_SWAP
     * or PCIIO_WORD_VALUES once the swapper
     * has been configured, one way or the other,
     * for the direct windows. If they are zero,
     * nobody has a PIO mapping through that window,
     * and the swapper can be set either way.
     */
    unsigned		bs_pio_end_io;
    unsigned		bs_pio_end_mem;
};

#define	PCIBR_ERRTIME_THRESHOLD		(100)
#define	PCIBR_ERRRATE_THRESHOLD		(100)

/*
 * pcibr will respond to hints dropped in its vertex
 * using the following structure.
 */
struct pcibr_hints_s {
    /* ph_host_slot is actually +1 so "0" means "no host" */
    pciio_slot_t            ph_host_slot[8];	/* REQ/GNT/INT in use by ... */
    unsigned                ph_rrb_fixed;	/* do not change RRB allocations */
    unsigned                ph_hands_off;	/* prevent further pcibr operations */
    rrb_alloc_funct_t       rrb_alloc_funct;	/* do dynamic rrb allocation */
    pcibr_intr_bits_f	   *ph_intr_bits;	/* map PCI INT[ABCD] to Bridge Int(n) */
};

/*
 * Number of bridge non-fatal error interrupts we can see before
 * we decide to disable that interrupt.
 */
#define	PCIBR_ERRINTR_DISABLE_LEVEL	10000

/* =====================================================================
 *    Bridge (pcibr) state management functions
 *
 *      pcibr_soft_get is here because we do it in a lot
 *      of places and I want to make sure they all stay
 *      in step with each other.
 *
 *      pcibr_soft_set is here because I want it to be
 *      closely associated with pcibr_soft_get, even
 *      though it is only called in one place.
 */

#define pcibr_soft_get(v)       ((pcibr_soft_t)hwgraph_fastinfo_get((v)))
#define pcibr_soft_set(v,i)     (hwgraph_fastinfo_set((v), (arbitrary_info_t)(i)))

/*
 * mem alloc/free macros
 */
#define NEWAf(ptr,n,f)	(ptr = snia_kmem_zalloc((n)*sizeof (*(ptr))))
#define NEWA(ptr,n)	(ptr = snia_kmem_zalloc((n)*sizeof (*(ptr))))
#define DELA(ptr,n)	(kfree(ptr))

#define NEWf(ptr,f)	NEWAf(ptr,1,f)
#define NEW(ptr)	NEWA(ptr,1)
#define DEL(ptr)	DELA(ptr,1)

/*
 * Additional PIO spaces per slot are
 * recorded in this structure.
 */
struct pciio_piospace_s {
    pciio_piospace_t        next;	/* another space for this device */
    char                    free;	/* 1 if free, 0 if in use */
    pciio_space_t           space;	/* Which space is in use */
    iopaddr_t               start;	/* Starting address of the PIO space */
    size_t                  count;	/* size of PIO space */
};

/* Use io spin locks. This ensures that all the PIO writes from a particular
 * CPU to a particular IO device are synched before the start of the next
 * set of PIO operations to the same device.
 */
#ifdef PCI_LATER
#define pcibr_lock(pcibr_soft)		io_splock(pcibr_soft->bs_lock)
#define pcibr_unlock(pcibr_soft, s)	io_spunlock(pcibr_soft->bs_lock,s)
#else
#define pcibr_lock(pcibr_soft)		1
#define pcibr_unlock(pcibr_soft, s)	
#endif	/* PCI_LATER */

#define PCIBR_VALID_SLOT(ps, s)     (s < PCIBR_NUM_SLOTS(ps))
#define PCIBR_D64_BASE_UNSET    (0xFFFFFFFFFFFFFFFF)
#define PCIBR_D32_BASE_UNSET    (0xFFFFFFFF)
#define INFO_LBL_PCIBR_ASIC_REV "_pcibr_asic_rev"

#define PCIBR_SOFT_LIST 1
#if PCIBR_SOFT_LIST
typedef struct pcibr_list_s *pcibr_list_p;
struct pcibr_list_s {
	pcibr_list_p            bl_next;
	pcibr_soft_t            bl_soft;
	vertex_hdl_t          bl_vhdl;
};
#endif /* PCIBR_SOFT_LIST */


// Devices per widget: 2 buses, 2 slots per bus, 8 functions per slot.
#define DEV_PER_WIDGET (2*2*8)

struct sn_flush_device_list {
	int bus;
	int pin;
	struct bar_list {
		unsigned long start;
		unsigned long end;
	} bar_list[PCI_ROM_RESOURCE];
	unsigned long force_int_addr;
	volatile unsigned long flush_addr;
	spinlock_t flush_lock;
};

struct sn_flush_nasid_entry  {
        struct sn_flush_device_list **widget_p;
        unsigned long        iio_itte1;
        unsigned long        iio_itte2;
        unsigned long        iio_itte3;
        unsigned long        iio_itte4;
        unsigned long        iio_itte5;
        unsigned long        iio_itte6;
        unsigned long        iio_itte7;
};

#endif				/* _ASM_SN_PCI_PCIBR_PRIVATE_H */
