/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#if 0
#include <linux/ioport.h>
#include <linux/interrupt.h>
#endif
#include <asm/sn/sgi.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/pci_defs.h>
#include <asm/sn/prio.h>
#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>

/*
 * global variables to toggle the different levels of pcibr debugging.  
 *   -pcibr_debug_mask is the mask of the different types of debugging
 *    you want to enable.  See sys/PCI/pcibr_private.h 
 *   -pcibr_debug_module is the module you want to trace.  By default
 *    all modules are trace.  For IP35 this value has the format of
 *    something like "001c10".  For IP27 this value is a node number,
 *    i.e. "1", "2"...  For IP30 this is undefined and should be set to
 *    'all'.
 *   -pcibr_debug_widget is the widget you want to trace.  For IP27
 *    the widget isn't exposed in the hwpath so use the xio slot num.
 *    i.e. for 'io2' set pcibr_debug_widget to "2".
 *   -pcibr_debug_slot is the pci slot you want to trace.
 */
uint32_t pcibr_debug_mask = 0x0;	/* 0x00000000 to disable */
char      *pcibr_debug_module = "all";		/* 'all' for all modules */
int	   pcibr_debug_widget = -1;		/* '-1' for all widgets  */
int	   pcibr_debug_slot = -1;		/* '-1' for all slots    */

/*
 * Macros related to the Lucent USS 302/312 usb timeout workaround.  It
 * appears that if the lucent part can get into a retry loop if it sees a
 * DAC on the bus during a pio read retry.  The loop is broken after about
 * 1ms, so we need to set up bridges holding this part to allow at least
 * 1ms for pio.
 */

#define USS302_TIMEOUT_WAR

#ifdef USS302_TIMEOUT_WAR
#define LUCENT_USBHC_VENDOR_ID_NUM	0x11c1
#define LUCENT_USBHC302_DEVICE_ID_NUM	0x5801
#define LUCENT_USBHC312_DEVICE_ID_NUM	0x5802
#define USS302_BRIDGE_TIMEOUT_HLD	4
#endif

/* kbrick widgetnum-to-bus layout */
int p_busnum[MAX_PORT_NUM] = {                  /* widget#      */
        0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7    */
        2,                                      /* 0x8          */
        1,                                      /* 0x9          */
        0, 0,                                   /* 0xa - 0xb    */
        5,                                      /* 0xc          */
        6,                                      /* 0xd          */
        4,                                      /* 0xe          */
        3,                                      /* 0xf          */
};

#if PCIBR_SOFT_LIST
pcibr_list_p            pcibr_list = 0;
#endif

extern int              hwgraph_vertex_name_get(vertex_hdl_t vhdl, char *buf, uint buflen);
extern long             atoi(register char *p);
extern cnodeid_t        nodevertex_to_cnodeid(vertex_hdl_t vhdl);
extern char             *dev_to_name(vertex_hdl_t dev, char *buf, uint buflen);
extern struct map       *atemapalloc(uint64_t);
extern void             atefree(struct map *, size_t, uint64_t);
extern void             atemapfree(struct map *);
extern pciio_dmamap_t   get_free_pciio_dmamap(vertex_hdl_t);
extern void		free_pciio_dmamap(pcibr_dmamap_t);
extern void		xwidget_error_register(vertex_hdl_t, error_handler_f *, error_handler_arg_t);

#define	ATE_WRITE()    ate_write(pcibr_soft, ate_ptr, ate_count, ate)
#if PCIBR_FREEZE_TIME
#define	ATE_FREEZE()	s = ate_freeze(pcibr_dmamap, &freeze_time, cmd_regs)
#else
#define	ATE_FREEZE()	s = ate_freeze(pcibr_dmamap, cmd_regs)
#endif /* PCIBR_FREEZE_TIME */

#if PCIBR_FREEZE_TIME
#define	ATE_THAW()	ate_thaw(pcibr_dmamap, ate_index, ate, ate_total, freeze_time, cmd_regs, s)
#else
#define	ATE_THAW()	ate_thaw(pcibr_dmamap, ate_index, cmd_regs, s)
#endif

/* =====================================================================
 *    Function Table of Contents
 *
 *      The order of functions in this file has stopped
 *      making much sense. We might want to take a look
 *      at it some time and bring back some sanity, or
 *      perhaps bust this file into smaller chunks.
 */

extern int		 do_pcibr_rrb_free_all(pcibr_soft_t, bridge_t *, pciio_slot_t);
extern void              do_pcibr_rrb_autoalloc(pcibr_soft_t, int, int, int);

extern int  		 pcibr_wrb_flush(vertex_hdl_t);
extern int               pcibr_rrb_alloc(vertex_hdl_t, int *, int *);
extern void              pcibr_rrb_flush(vertex_hdl_t);

static int                pcibr_try_set_device(pcibr_soft_t, pciio_slot_t, unsigned, bridgereg_t);
void                     pcibr_release_device(pcibr_soft_t, pciio_slot_t, bridgereg_t);

extern void              pcibr_setwidint(xtalk_intr_t);
extern void              pcibr_clearwidint(bridge_t *);

extern iopaddr_t         pcibr_bus_addr_alloc(pcibr_soft_t, pciio_win_info_t,
                                              pciio_space_t, int, int, int);

int                      pcibr_attach(vertex_hdl_t);
int			 pcibr_attach2(vertex_hdl_t, bridge_t *, vertex_hdl_t,
				       int, pcibr_soft_t *);
int			 pcibr_detach(vertex_hdl_t);
int			 pcibr_pcix_rbars_calc(pcibr_soft_t);
extern int               pcibr_init_ext_ate_ram(bridge_t *);
extern int               pcibr_ate_alloc(pcibr_soft_t, int);
extern void              pcibr_ate_free(pcibr_soft_t, int, int);
extern int 		 pcibr_widget_to_bus(vertex_hdl_t pcibr_vhdl);

extern unsigned ate_freeze(pcibr_dmamap_t pcibr_dmamap,
#if PCIBR_FREEZE_TIME
	   		 unsigned *freeze_time_ptr,
#endif
	   		 unsigned *cmd_regs);
extern void ate_write(pcibr_soft_t pcibr_soft, bridge_ate_p ate_ptr, int ate_count, bridge_ate_t ate);
extern void ate_thaw(pcibr_dmamap_t pcibr_dmamap, int ate_index,
#if PCIBR_FREEZE_TIME
	 		bridge_ate_t ate,
	 		int ate_total,
	 		unsigned freeze_time_start,
#endif
	 		unsigned *cmd_regs,
	 		unsigned s);

pcibr_info_t      pcibr_info_get(vertex_hdl_t);

static iopaddr_t         pcibr_addr_pci_to_xio(vertex_hdl_t, pciio_slot_t, pciio_space_t, iopaddr_t, size_t, unsigned);

pcibr_piomap_t          pcibr_piomap_alloc(vertex_hdl_t, device_desc_t, pciio_space_t, iopaddr_t, size_t, size_t, unsigned);
void                    pcibr_piomap_free(pcibr_piomap_t);
caddr_t                 pcibr_piomap_addr(pcibr_piomap_t, iopaddr_t, size_t);
void                    pcibr_piomap_done(pcibr_piomap_t);
caddr_t                 pcibr_piotrans_addr(vertex_hdl_t, device_desc_t, pciio_space_t, iopaddr_t, size_t, unsigned);
iopaddr_t               pcibr_piospace_alloc(vertex_hdl_t, device_desc_t, pciio_space_t, size_t, size_t);
void                    pcibr_piospace_free(vertex_hdl_t, pciio_space_t, iopaddr_t, size_t);

static iopaddr_t         pcibr_flags_to_d64(unsigned, pcibr_soft_t);
extern bridge_ate_t     pcibr_flags_to_ate(unsigned);

pcibr_dmamap_t          pcibr_dmamap_alloc(vertex_hdl_t, device_desc_t, size_t, unsigned);
void                    pcibr_dmamap_free(pcibr_dmamap_t);
extern bridge_ate_p     pcibr_ate_addr(pcibr_soft_t, int);
static iopaddr_t         pcibr_addr_xio_to_pci(pcibr_soft_t, iopaddr_t, size_t);
iopaddr_t               pcibr_dmamap_addr(pcibr_dmamap_t, paddr_t, size_t);
void                    pcibr_dmamap_done(pcibr_dmamap_t);
cnodeid_t		pcibr_get_dmatrans_node(vertex_hdl_t);
iopaddr_t               pcibr_dmatrans_addr(vertex_hdl_t, device_desc_t, paddr_t, size_t, unsigned);
void                    pcibr_dmamap_drain(pcibr_dmamap_t);
void                    pcibr_dmaaddr_drain(vertex_hdl_t, paddr_t, size_t);
void                    pcibr_dmalist_drain(vertex_hdl_t, alenlist_t);
iopaddr_t               pcibr_dmamap_pciaddr_get(pcibr_dmamap_t);

extern unsigned		pcibr_intr_bits(pciio_info_t info, 
					pciio_intr_line_t lines, int nslots);
extern pcibr_intr_t     pcibr_intr_alloc(vertex_hdl_t, device_desc_t, pciio_intr_line_t, vertex_hdl_t);
extern void             pcibr_intr_free(pcibr_intr_t);
extern void             pcibr_setpciint(xtalk_intr_t);
extern int              pcibr_intr_connect(pcibr_intr_t, intr_func_t, intr_arg_t);
extern void             pcibr_intr_disconnect(pcibr_intr_t);

extern vertex_hdl_t     pcibr_intr_cpu_get(pcibr_intr_t);
extern void             pcibr_intr_func(intr_arg_t);

extern void             print_bridge_errcmd(uint32_t, char *);

extern void             pcibr_error_dump(pcibr_soft_t);
extern uint32_t       pcibr_errintr_group(uint32_t);
extern void	        pcibr_pioerr_check(pcibr_soft_t);
extern void             pcibr_error_intr_handler(int, void *, struct pt_regs *);

extern int              pcibr_addr_toslot(pcibr_soft_t, iopaddr_t, pciio_space_t *, iopaddr_t *, pciio_function_t *);
extern void             pcibr_error_cleanup(pcibr_soft_t, int);
extern void                    pcibr_device_disable(pcibr_soft_t, int);
extern int              pcibr_pioerror(pcibr_soft_t, int, ioerror_mode_t, ioerror_t *);
extern int              pcibr_dmard_error(pcibr_soft_t, int, ioerror_mode_t, ioerror_t *);
extern int              pcibr_dmawr_error(pcibr_soft_t, int, ioerror_mode_t, ioerror_t *);
extern int              pcibr_error_handler(error_handler_arg_t, int, ioerror_mode_t, ioerror_t *);
extern int              pcibr_error_handler_wrapper(error_handler_arg_t, int, ioerror_mode_t, ioerror_t *);
void                    pcibr_provider_startup(vertex_hdl_t);
void                    pcibr_provider_shutdown(vertex_hdl_t);

int                     pcibr_reset(vertex_hdl_t);
pciio_endian_t          pcibr_endian_set(vertex_hdl_t, pciio_endian_t, pciio_endian_t);
int                     pcibr_device_flags_set(vertex_hdl_t, pcibr_device_flags_t);

extern cfg_p            pcibr_config_addr(vertex_hdl_t, unsigned);
extern uint64_t         pcibr_config_get(vertex_hdl_t, unsigned, unsigned);
extern void             pcibr_config_set(vertex_hdl_t, unsigned, unsigned, uint64_t);

extern pcibr_hints_t    pcibr_hints_get(vertex_hdl_t, int);
extern void             pcibr_hints_fix_rrbs(vertex_hdl_t);
extern void             pcibr_hints_dualslot(vertex_hdl_t, pciio_slot_t, pciio_slot_t);
extern void	 	pcibr_hints_intr_bits(vertex_hdl_t, pcibr_intr_bits_f *);
extern void             pcibr_set_rrb_callback(vertex_hdl_t, rrb_alloc_funct_t);
extern void             pcibr_hints_handsoff(vertex_hdl_t);
extern void             pcibr_hints_subdevs(vertex_hdl_t, pciio_slot_t, uint64_t);

extern int		pcibr_slot_info_init(vertex_hdl_t,pciio_slot_t);
extern int		pcibr_slot_info_free(vertex_hdl_t,pciio_slot_t);
extern int	        pcibr_slot_info_return(pcibr_soft_t, pciio_slot_t,
                                               pcibr_slot_info_resp_t);
extern void       	pcibr_slot_func_info_return(pcibr_info_h, int,
                                                    pcibr_slot_func_info_resp_t);
extern int		pcibr_slot_addr_space_init(vertex_hdl_t,pciio_slot_t);
extern int		pcibr_slot_pcix_rbar_init(pcibr_soft_t, pciio_slot_t);
extern int		pcibr_slot_device_init(vertex_hdl_t, pciio_slot_t);
extern int		pcibr_slot_guest_info_init(vertex_hdl_t,pciio_slot_t);
extern int		pcibr_slot_call_device_attach(vertex_hdl_t,
						      pciio_slot_t, int);
extern int		pcibr_slot_call_device_detach(vertex_hdl_t,
						      pciio_slot_t, int);
extern int              pcibr_slot_attach(vertex_hdl_t, pciio_slot_t, int, 
                                                      char *, int *);
extern int              pcibr_slot_detach(vertex_hdl_t, pciio_slot_t, int,
                                                      char *, int *);

extern int		pcibr_slot_initial_rrb_alloc(vertex_hdl_t, pciio_slot_t);
extern int		pcibr_initial_rrb(vertex_hdl_t, pciio_slot_t, pciio_slot_t);

/* =====================================================================
 *    Device(x) register management
 */

/* pcibr_try_set_device: attempt to modify Device(x)
 * for the specified slot on the specified bridge
 * as requested in flags, limited to the specified
 * bits. Returns which BRIDGE bits were in conflict,
 * or ZERO if everything went OK.
 *
 * Caller MUST hold pcibr_lock when calling this function.
 */
static int
pcibr_try_set_device(pcibr_soft_t pcibr_soft,
		     pciio_slot_t slot,
		     unsigned flags,
		     bridgereg_t mask)
{
    bridge_t               *bridge;
    pcibr_soft_slot_t       slotp;
    bridgereg_t             old;
    bridgereg_t             new;
    bridgereg_t             chg;
    bridgereg_t             bad;
    bridgereg_t             badpmu;
    bridgereg_t             badd32;
    bridgereg_t             badd64;
    bridgereg_t             fix;
    unsigned long           s;
    bridgereg_t             xmask;

    xmask = mask;
    if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) {
    	if (mask == BRIDGE_DEV_PMU_BITS)
		xmask = XBRIDGE_DEV_PMU_BITS;
	if (mask == BRIDGE_DEV_D64_BITS)
		xmask = XBRIDGE_DEV_D64_BITS;
    }

    slotp = &pcibr_soft->bs_slot[slot];

    s = pcibr_lock(pcibr_soft);

    bridge = pcibr_soft->bs_base;

    old = slotp->bss_device;

    /* figure out what the desired
     * Device(x) bits are based on
     * the flags specified.
     */

    new = old;

    /* Currently, we inherit anything that
     * the new caller has not specified in
     * one way or another, unless we take
     * action here to not inherit.
     *
     * This is needed for the "swap" stuff,
     * since it could have been set via
     * pcibr_endian_set -- altho note that
     * any explicit PCIBR_BYTE_STREAM or
     * PCIBR_WORD_VALUES will freely override
     * the effect of that call (and vice
     * versa, no protection either way).
     *
     * I want to get rid of pcibr_endian_set
     * in favor of tracking DMA endianness
     * using the flags specified when DMA
     * channels are created.
     */

#define	BRIDGE_DEV_WRGA_BITS	(BRIDGE_DEV_PMU_WRGA_EN | BRIDGE_DEV_DIR_WRGA_EN)
#define	BRIDGE_DEV_SWAP_BITS	(BRIDGE_DEV_SWAP_PMU | BRIDGE_DEV_SWAP_DIR)

    /* Do not use Barrier, Write Gather,
     * or Prefetch unless asked.
     * Leave everything else as it
     * was from the last time.
     */
    new = new
	& ~BRIDGE_DEV_BARRIER
	& ~BRIDGE_DEV_WRGA_BITS
	& ~BRIDGE_DEV_PREF
	;

    /* Generic macro flags
     */
    if (flags & PCIIO_DMA_DATA) {
	new = (new
            & ~BRIDGE_DEV_BARRIER)      /* barrier off */
            | BRIDGE_DEV_PREF;          /* prefetch on */

    }
    if (flags & PCIIO_DMA_CMD) {
        new = ((new
            & ~BRIDGE_DEV_PREF)         /* prefetch off */
            & ~BRIDGE_DEV_WRGA_BITS)    /* write gather off */
            | BRIDGE_DEV_BARRIER;       /* barrier on */
    }
    /* Generic detail flags
     */
    if (flags & PCIIO_WRITE_GATHER)
	new |= BRIDGE_DEV_WRGA_BITS;
    if (flags & PCIIO_NOWRITE_GATHER)
	new &= ~BRIDGE_DEV_WRGA_BITS;

    if (flags & PCIIO_PREFETCH)
	new |= BRIDGE_DEV_PREF;
    if (flags & PCIIO_NOPREFETCH)
	new &= ~BRIDGE_DEV_PREF;

    if (flags & PCIBR_WRITE_GATHER)
	new |= BRIDGE_DEV_WRGA_BITS;
    if (flags & PCIBR_NOWRITE_GATHER)
	new &= ~BRIDGE_DEV_WRGA_BITS;

    if (flags & PCIIO_BYTE_STREAM)
	new |= (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) ? 
			BRIDGE_DEV_SWAP_DIR : BRIDGE_DEV_SWAP_BITS;
    if (flags & PCIIO_WORD_VALUES)
	new &= (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) ? 
			~BRIDGE_DEV_SWAP_DIR : ~BRIDGE_DEV_SWAP_BITS;

    /* Provider-specific flags
     */
    if (flags & PCIBR_PREFETCH)
	new |= BRIDGE_DEV_PREF;
    if (flags & PCIBR_NOPREFETCH)
	new &= ~BRIDGE_DEV_PREF;

    if (flags & PCIBR_PRECISE)
	new |= BRIDGE_DEV_PRECISE;
    if (flags & PCIBR_NOPRECISE)
	new &= ~BRIDGE_DEV_PRECISE;

    if (flags & PCIBR_BARRIER)
	new |= BRIDGE_DEV_BARRIER;
    if (flags & PCIBR_NOBARRIER)
	new &= ~BRIDGE_DEV_BARRIER;

    if (flags & PCIBR_64BIT)
	new |= BRIDGE_DEV_DEV_SIZE;
    if (flags & PCIBR_NO64BIT)
	new &= ~BRIDGE_DEV_DEV_SIZE;

    /*
     * PIC BRINGUP WAR (PV# 855271):
     * Allow setting BRIDGE_DEV_VIRTUAL_EN on PIC iff we're a 64-bit
     * device.  The bit is only intended for 64-bit devices and, on
     * PIC, can cause problems for 32-bit devices.
     */
    if (IS_PIC_SOFT(pcibr_soft) && mask == BRIDGE_DEV_D64_BITS &&
                                PCIBR_WAR_ENABLED(PV855271, pcibr_soft)) {
        if (flags & PCIBR_VCHAN1) {
                new |= BRIDGE_DEV_VIRTUAL_EN;
                xmask |= BRIDGE_DEV_VIRTUAL_EN;
        }
    }


    chg = old ^ new;				/* what are we changing, */
    chg &= xmask;				/* of the interesting bits */

    if (chg) {

	badd32 = slotp->bss_d32_uctr ? (BRIDGE_DEV_D32_BITS & chg) : 0;
	if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) {
		badpmu = slotp->bss_pmu_uctr ? (XBRIDGE_DEV_PMU_BITS & chg) : 0;
		badd64 = slotp->bss_d64_uctr ? (XBRIDGE_DEV_D64_BITS & chg) : 0;
	} else {
		badpmu = slotp->bss_pmu_uctr ? (BRIDGE_DEV_PMU_BITS & chg) : 0;
		badd64 = slotp->bss_d64_uctr ? (BRIDGE_DEV_D64_BITS & chg) : 0;
	}
	bad = badpmu | badd32 | badd64;

	if (bad) {

	    /* some conflicts can be resolved by
	     * forcing the bit on. this may cause
	     * some performance degredation in
	     * the stream(s) that want the bit off,
	     * but the alternative is not allowing
	     * the new stream at all.
	     */
            if ( (fix = bad & (BRIDGE_DEV_PRECISE |
                             BRIDGE_DEV_BARRIER)) ) {
		bad &= ~fix;
		/* don't change these bits if
		 * they are already set in "old"
		 */
		chg &= ~(fix & old);
	    }
	    /* some conflicts can be resolved by
	     * forcing the bit off. this may cause
	     * some performance degredation in
	     * the stream(s) that want the bit on,
	     * but the alternative is not allowing
	     * the new stream at all.
	     */
	    if ( (fix = bad & (BRIDGE_DEV_WRGA_BITS |
			     BRIDGE_DEV_PREF)) ) {
		bad &= ~fix;
		/* don't change these bits if
		 * we wanted to turn them on.
		 */
		chg &= ~(fix & new);
	    }
	    /* conflicts in other bits mean
	     * we can not establish this DMA
	     * channel while the other(s) are
	     * still present.
	     */
	    if (bad) {
		pcibr_unlock(pcibr_soft, s);
#ifdef PIC_LATER
		PCIBR_DEBUG((PCIBR_DEBUG_DEVREG, pcibr_soft->bs_vhdl,
			    "pcibr_try_set_device: mod blocked by %x\n",
			    bad, device_bits));
#endif
		return bad;
	    }
	}
    }
    if (mask == BRIDGE_DEV_PMU_BITS)
	slotp->bss_pmu_uctr++;
    if (mask == BRIDGE_DEV_D32_BITS)
	slotp->bss_d32_uctr++;
    if (mask == BRIDGE_DEV_D64_BITS)
	slotp->bss_d64_uctr++;

    /* the value we want to write is the
     * original value, with the bits for
     * our selected changes flipped, and
     * with any disabled features turned off.
     */
    new = old ^ chg;			/* only change what we want to change */

    if (slotp->bss_device == new) {
	pcibr_unlock(pcibr_soft, s);
	return 0;
    }
    if ( IS_PIC_SOFT(pcibr_soft) ) {
	bridge->b_device[slot].reg = new;
	slotp->bss_device = new;
	bridge->b_wid_tflush;		/* wait until Bridge PIO complete */
    }
    pcibr_unlock(pcibr_soft, s);

#ifdef PIC_LATER
    PCIBR_DEBUG((PCIBR_DEBUG_DEVREG, pcibr_soft->bs_vhdl,
		"pcibr_try_set_device: Device(%d): %x\n",
		slot, new, device_bits));
#else
    printk("pcibr_try_set_device: Device(%d): %x\n", slot, new);
#endif
    return 0;
}

void
pcibr_release_device(pcibr_soft_t pcibr_soft,
		     pciio_slot_t slot,
		     bridgereg_t mask)
{
    pcibr_soft_slot_t       slotp;
    unsigned long           s;

    slotp = &pcibr_soft->bs_slot[slot];

    s = pcibr_lock(pcibr_soft);

    if (mask == BRIDGE_DEV_PMU_BITS)
	slotp->bss_pmu_uctr--;
    if (mask == BRIDGE_DEV_D32_BITS)
	slotp->bss_d32_uctr--;
    if (mask == BRIDGE_DEV_D64_BITS)
	slotp->bss_d64_uctr--;

    pcibr_unlock(pcibr_soft, s);
}


/* =====================================================================
 *    Bridge (pcibr) "Device Driver" entry points
 */


static int
pcibr_mmap(struct file * file, struct vm_area_struct * vma)
{
	vertex_hdl_t		pcibr_vhdl;
	pcibr_soft_t            pcibr_soft;
	bridge_t               *bridge;
	unsigned long		phys_addr;
	int			error = 0;

#ifdef CONFIG_HWGFS_FS
	pcibr_vhdl = (vertex_hdl_t) file->f_dentry->d_fsdata;
#else
	pcibr_vhdl = (vertex_hdl_t) file->private_data;
#endif
	pcibr_soft = pcibr_soft_get(pcibr_vhdl);
	bridge = pcibr_soft->bs_base;
	phys_addr = (unsigned long)bridge & ~0xc000000000000000; /* Mask out the Uncache bits */
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        vma->vm_flags |= VM_NONCACHED | VM_RESERVED | VM_IO;
        error = io_remap_page_range(vma->vm_start, phys_addr,
                                   vma->vm_end-vma->vm_start,
                                   vma->vm_page_prot);
	return(error);
}

/*
 * This is the file operation table for the pcibr driver.
 * As each of the functions are implemented, put the
 * appropriate function name below.
 */
static int pcibr_mmap(struct file * file, struct vm_area_struct * vma);
struct file_operations pcibr_fops = {
	.owner		= THIS_MODULE,
	.mmap		= pcibr_mmap,
};

/* This is special case code used by grio. There are plans to make
 * this a bit more general in the future, but till then this should
 * be sufficient.
 */
pciio_slot_t
pcibr_device_slot_get(vertex_hdl_t dev_vhdl)
{
    char                    devname[MAXDEVNAME];
    vertex_hdl_t            tdev;
    pciio_info_t            pciio_info;
    pciio_slot_t            slot = PCIIO_SLOT_NONE;

    vertex_to_name(dev_vhdl, devname, MAXDEVNAME);

    /* run back along the canonical path
     * until we find a PCI connection point.
     */
    tdev = hwgraph_connectpt_get(dev_vhdl);
    while (tdev != GRAPH_VERTEX_NONE) {
	pciio_info = pciio_info_chk(tdev);
	if (pciio_info) {
	    slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
	    break;
	}
	hwgraph_vertex_unref(tdev);
	tdev = hwgraph_connectpt_get(tdev);
    }
    hwgraph_vertex_unref(tdev);

    return slot;
}

pcibr_info_t
pcibr_info_get(vertex_hdl_t vhdl)
{
    return (pcibr_info_t) pciio_info_get(vhdl);
}

pcibr_info_t
pcibr_device_info_new(
			 pcibr_soft_t pcibr_soft,
			 pciio_slot_t slot,
			 pciio_function_t rfunc,
			 pciio_vendor_id_t vendor,
			 pciio_device_id_t device)
{
    pcibr_info_t            pcibr_info;
    pciio_function_t        func;
    int                     ibit;

    func = (rfunc == PCIIO_FUNC_NONE) ? 0 : rfunc;

    /*
     * Create a pciio_info_s for this device.  pciio_device_info_new()
     * will set the c_slot (which is suppose to represent the external
     * slot (i.e the slot number silk screened on the back of the I/O
     * brick)).  So for PIC we need to adjust this "internal slot" num
     * passed into us, into its external representation.  See comment
     * for the PCIBR_DEVICE_TO_SLOT macro for more information.
     */
    NEW(pcibr_info);
    pciio_device_info_new(&pcibr_info->f_c, pcibr_soft->bs_vhdl,
			  PCIBR_DEVICE_TO_SLOT(pcibr_soft, slot),
			  rfunc, vendor, device);
    pcibr_info->f_dev = slot;

    /* Set PCI bus number */
    pcibr_info->f_bus = pcibr_widget_to_bus(pcibr_soft->bs_vhdl);

    if (slot != PCIIO_SLOT_NONE) {

	/*
	 * Currently favored mapping from PCI
	 * slot number and INTA/B/C/D to Bridge
	 * PCI Interrupt Bit Number:
	 *
	 *     SLOT     A B C D
	 *      0       0 4 0 4
	 *      1       1 5 1 5
	 *      2       2 6 2 6
	 *      3       3 7 3 7
	 *      4       4 0 4 0
	 *      5       5 1 5 1
	 *      6       6 2 6 2
	 *      7       7 3 7 3
	 *
	 * XXX- allow pcibr_hints to override default
	 * XXX- allow ADMIN to override pcibr_hints
	 */
	for (ibit = 0; ibit < 4; ++ibit)
	    pcibr_info->f_ibit[ibit] =
		(slot + 4 * ibit) & 7;

	/*
	 * Record the info in the sparse func info space.
	 */
	if (func < pcibr_soft->bs_slot[slot].bss_ninfo)
	    pcibr_soft->bs_slot[slot].bss_infos[func] = pcibr_info;
    }
    return pcibr_info;
}


/*
 * pcibr_device_unregister
 *	This frees up any hardware resources reserved for this PCI device
 * 	and removes any PCI infrastructural information setup for it.
 *	This is usually used at the time of shutting down of the PCI card.
 */
int
pcibr_device_unregister(vertex_hdl_t pconn_vhdl)
{
    pciio_info_t	 pciio_info;
    vertex_hdl_t	 pcibr_vhdl;
    pciio_slot_t	 slot;
    pcibr_soft_t	 pcibr_soft;
    bridge_t		*bridge;
    int                  count_vchan0, count_vchan1;
    unsigned             s;
    int			 error_call;
    int			 error = 0;

    pciio_info = pciio_info_get(pconn_vhdl);

    pcibr_vhdl = pciio_info_master_get(pciio_info);
    slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);

    pcibr_soft = pcibr_soft_get(pcibr_vhdl);
    bridge = pcibr_soft->bs_base;

    /* Clear all the hardware xtalk resources for this device */
    xtalk_widgetdev_shutdown(pcibr_soft->bs_conn, slot);

    /* Flush all the rrbs */
    pcibr_rrb_flush(pconn_vhdl);

    /*
     * If the RRB configuration for this slot has changed, set it 
     * back to the boot-time default
     */
    if (pcibr_soft->bs_rrb_valid_dflt[slot][VCHAN0] >= 0) {

        s = pcibr_lock(pcibr_soft);

	/* PIC NOTE: If this is a BRIDGE, VCHAN2 & VCHAN3 will be zero so
	 * no need to conditionalize this (ie. "if (IS_PIC_SOFT())" ).
	 */
        pcibr_soft->bs_rrb_res[slot] = pcibr_soft->bs_rrb_res[slot] +
                                       pcibr_soft->bs_rrb_valid[slot][VCHAN0] +
                                       pcibr_soft->bs_rrb_valid[slot][VCHAN1] +
                                       pcibr_soft->bs_rrb_valid[slot][VCHAN2] +
                                       pcibr_soft->bs_rrb_valid[slot][VCHAN3];

        /* Free the rrbs allocated to this slot, both the normal & virtual */
	do_pcibr_rrb_free_all(pcibr_soft, bridge, slot);

        count_vchan0 = pcibr_soft->bs_rrb_valid_dflt[slot][VCHAN0];
        count_vchan1 = pcibr_soft->bs_rrb_valid_dflt[slot][VCHAN1];

        pcibr_unlock(pcibr_soft, s);

        pcibr_rrb_alloc(pconn_vhdl, &count_vchan0, &count_vchan1);

    }

    /* Flush the write buffers !! */
    error_call = pcibr_wrb_flush(pconn_vhdl);

    if (error_call)
        error = error_call;

    /* Clear the information specific to the slot */
    error_call = pcibr_slot_info_free(pcibr_vhdl, slot);

    if (error_call)
        error = error_call;

    return(error);
    
}

/*
 * pcibr_driver_reg_callback
 *      CDL will call this function for each device found in the PCI
 *      registry that matches the vendor/device IDs supported by 
 *      the driver being registered.  The device's connection vertex
 *      and the driver's attach function return status enable the
 *      slot's device status to be set.
 */
void
pcibr_driver_reg_callback(vertex_hdl_t pconn_vhdl,
			  int key1, int key2, int error)
{
    pciio_info_t	 pciio_info;
    pcibr_info_t         pcibr_info;
    vertex_hdl_t	 pcibr_vhdl;
    pciio_slot_t	 slot;
    pcibr_soft_t	 pcibr_soft;

    /* Do not set slot status for vendor/device ID wildcard drivers */
    if ((key1 == -1) || (key2 == -1))
        return;

    pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_info = pcibr_info_get(pconn_vhdl);

    pcibr_vhdl = pciio_info_master_get(pciio_info);
    slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);

    pcibr_soft = pcibr_soft_get(pcibr_vhdl);

#ifdef PIC_LATER
    /* This may be a loadable driver so lock out any pciconfig actions */
    mrlock(pcibr_soft->bs_bus_lock, MR_UPDATE, PZERO);
#endif

    pcibr_info->f_att_det_error = error;

    pcibr_soft->bs_slot[slot].slot_status &= ~SLOT_STATUS_MASK;

    if (error) {
        pcibr_soft->bs_slot[slot].slot_status |= SLOT_STARTUP_INCMPLT;
    } else {
        pcibr_soft->bs_slot[slot].slot_status |= SLOT_STARTUP_CMPLT;
    }
        
#ifdef PIC_LATER
    /* Release the bus lock */
    mrunlock(pcibr_soft->bs_bus_lock);
#endif
}

/*
 * pcibr_driver_unreg_callback
 *      CDL will call this function for each device found in the PCI
 *      registry that matches the vendor/device IDs supported by 
 *      the driver being unregistered.  The device's connection vertex
 *      and the driver's detach function return status enable the
 *      slot's device status to be set.
 */
void
pcibr_driver_unreg_callback(vertex_hdl_t pconn_vhdl, 
                            int key1, int key2, int error)
{
    pciio_info_t	 pciio_info;
    pcibr_info_t         pcibr_info;
    vertex_hdl_t	 pcibr_vhdl;
    pciio_slot_t	 slot;
    pcibr_soft_t	 pcibr_soft;

    /* Do not set slot status for vendor/device ID wildcard drivers */
    if ((key1 == -1) || (key2 == -1))
        return;

    pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_info = pcibr_info_get(pconn_vhdl);

    pcibr_vhdl = pciio_info_master_get(pciio_info);
    slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);

    pcibr_soft = pcibr_soft_get(pcibr_vhdl);

#ifdef PIC_LATER
    /* This may be a loadable driver so lock out any pciconfig actions */
    mrlock(pcibr_soft->bs_bus_lock, MR_UPDATE, PZERO);
#endif

    pcibr_info->f_att_det_error = error;

    pcibr_soft->bs_slot[slot].slot_status &= ~SLOT_STATUS_MASK;

    if (error) {
        pcibr_soft->bs_slot[slot].slot_status |= SLOT_SHUTDOWN_INCMPLT;
    } else {
        pcibr_soft->bs_slot[slot].slot_status |= SLOT_SHUTDOWN_CMPLT;
    }

#ifdef PIC_LATER
    /* Release the bus lock */
    mrunlock(pcibr_soft->bs_bus_lock);
#endif
}

/* 
 * build a convenience link path in the
 * form of ".../<iobrick>/bus/<busnum>"
 * 
 * returns 1 on success, 0 otherwise
 *
 * depends on hwgraph separator == '/'
 */
int
pcibr_bus_cnvlink(vertex_hdl_t f_c)
{
        char dst[MAXDEVNAME];
	char *dp = dst;
        char *cp, *xp;
        int widgetnum;
        char pcibus[8];
	vertex_hdl_t nvtx, svtx;
	int rv;

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, f_c, "pcibr_bus_cnvlink\n"));

	if (GRAPH_SUCCESS != hwgraph_vertex_name_get(f_c, dst, MAXDEVNAME)) {
		return 0;
	}

	/* dst example == /hw/module/001c02/Pbrick/xtalk/8/pci/direct */

	/* find the widget number */
	xp = strstr(dst, "/"EDGE_LBL_XTALK"/");
	if (xp == NULL) {
		return 0;
	}
	widgetnum = simple_strtoul(xp+7, NULL, 0);
	if (widgetnum < XBOW_PORT_8 || widgetnum > XBOW_PORT_F) {
		return 0;
	}

	/* remove "/pci/direct" from path */
	cp = strstr(dst, "/" EDGE_LBL_PCI "/" EDGE_LBL_DIRECT);
	if (cp == NULL) {
		return 0;
	}
	*cp = (char)NULL;

	/* get the vertex for the widget */
	if (GRAPH_SUCCESS != hwgraph_traverse(NULL, dp, &svtx))	{
		return 0;
	}

	*xp = (char)NULL;		/* remove "/xtalk/..." from path */

	/* dst example now == /hw/module/001c02/Pbrick */

	/* get the bus number */
        strcat(dst, "/");
        strcat(dst, EDGE_LBL_BUS);
        sprintf(pcibus, "%d", p_busnum[widgetnum]);

	/* link to bus to widget */
	rv = hwgraph_path_add(NULL, dp, &nvtx);
	if (GRAPH_SUCCESS == rv)
		rv = hwgraph_edge_add(nvtx, svtx, pcibus);

	return (rv == GRAPH_SUCCESS);
}


/*
 *    pcibr_attach: called every time the crosstalk
 *      infrastructure is asked to initialize a widget
 *      that matches the part number we handed to the
 *      registration routine above.
 */
/*ARGSUSED */
int
pcibr_attach(vertex_hdl_t xconn_vhdl)
{
    /* REFERENCED */
    graph_error_t           rc;
    vertex_hdl_t            pcibr_vhdl;
    bridge_t               *bridge;

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, xconn_vhdl, "pcibr_attach\n"));

    bridge = (bridge_t *)
	xtalk_piotrans_addr(xconn_vhdl, NULL,
			    0, sizeof(bridge_t), 0);
    /*
     * Create the vertex for the PCI bus, which we
     * will also use to hold the pcibr_soft and
     * which will be the "master" vertex for all the
     * pciio connection points we will hang off it.
     * This needs to happen before we call nic_bridge_vertex_info
     * as we are some of the *_vmc functions need access to the edges.
     *
     * Opening this vertex will provide access to
     * the Bridge registers themselves.
     */
    rc = hwgraph_path_add(xconn_vhdl, EDGE_LBL_PCI, &pcibr_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    pciio_provider_register(pcibr_vhdl, &pcibr_provider);
    pciio_provider_startup(pcibr_vhdl);

    return pcibr_attach2(xconn_vhdl, bridge, pcibr_vhdl, 0, NULL);
}


/*ARGSUSED */
int
pcibr_attach2(vertex_hdl_t xconn_vhdl, bridge_t *bridge, 
	      vertex_hdl_t pcibr_vhdl, int busnum, pcibr_soft_t *ret_softp)
{
    /* REFERENCED */
    vertex_hdl_t            ctlr_vhdl;
    bridgereg_t             id;
    int                     rev;
    pcibr_soft_t            pcibr_soft;
    pcibr_info_t            pcibr_info;
    xwidget_info_t          info;
    xtalk_intr_t            xtalk_intr;
    int                     slot;
    int                     ibit;
    vertex_hdl_t            noslot_conn;
    char                    devnm[MAXDEVNAME], *s;
    pcibr_hints_t           pcibr_hints;
    uint64_t                int_enable;
    picreg_t                int_enable_64;
    unsigned                rrb_fixed = 0;

#if PCI_FBBE
    int                     fast_back_to_back_enable;
#endif
    nasid_t		    nasid;
    int	                    iobrick_type_get_nasid(nasid_t nasid);
    int                     iobrick_module_get_nasid(nasid_t nasid);

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, pcibr_vhdl,
	        "pcibr_attach2: bridge=0x%p, busnum=%d\n", bridge, busnum));

    ctlr_vhdl = NULL;
    ctlr_vhdl = hwgraph_register(pcibr_vhdl, EDGE_LBL_CONTROLLER, 0, 
                DEVFS_FL_AUTO_DEVNUM, 0, 0,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0, 
		(struct file_operations *)&pcibr_fops, (void *)pcibr_vhdl);
    ASSERT(ctlr_vhdl != NULL);

    /*
     * Get the hint structure; if some NIC callback
     * marked this vertex as "hands-off" then we
     * just return here, before doing anything else.
     */
    pcibr_hints = pcibr_hints_get(xconn_vhdl, 0);

    if (pcibr_hints && pcibr_hints->ph_hands_off)
	return -1;			/* generic operations disabled */

    id = bridge->b_wid_id;
    rev = XWIDGET_PART_REV_NUM(id);

    hwgraph_info_add_LBL(pcibr_vhdl, INFO_LBL_PCIBR_ASIC_REV, (arbitrary_info_t) rev);

    /*
     * allocate soft state structure, fill in some
     * fields, and hook it up to our vertex.
     */
    NEW(pcibr_soft);
    if (ret_softp)
	*ret_softp = pcibr_soft;
    memset(pcibr_soft, 0, sizeof *pcibr_soft);
    pcibr_soft_set(pcibr_vhdl, pcibr_soft);
    pcibr_soft->bs_conn = xconn_vhdl;
    pcibr_soft->bs_vhdl = pcibr_vhdl;
    pcibr_soft->bs_base = bridge;
    pcibr_soft->bs_rev_num = rev;
    pcibr_soft->bs_intr_bits = (pcibr_intr_bits_f *)pcibr_intr_bits;

    pcibr_soft->bs_min_slot = 0;		/* lowest possible slot# */
    pcibr_soft->bs_max_slot = 7;		/* highest possible slot# */
    pcibr_soft->bs_busnum = busnum;
    pcibr_soft->bs_bridge_type = PCIBR_BRIDGETYPE_PIC;
    switch(pcibr_soft->bs_bridge_type) {
    case PCIBR_BRIDGETYPE_BRIDGE:
	pcibr_soft->bs_int_ate_size = BRIDGE_INTERNAL_ATES;
	pcibr_soft->bs_bridge_mode = 0;	/* speed is not available in bridge */
	break;
    case PCIBR_BRIDGETYPE_PIC:
        pcibr_soft->bs_min_slot = 0;
	pcibr_soft->bs_max_slot = 3;
	pcibr_soft->bs_int_ate_size = XBRIDGE_INTERNAL_ATES;
	pcibr_soft->bs_bridge_mode = 
	   (((bridge->p_wid_stat_64 & PIC_STAT_PCIX_SPEED) >> 33) |
	    ((bridge->p_wid_stat_64 & PIC_STAT_PCIX_ACTIVE) >> 33));

	/* We have to clear PIC's write request buffer to avoid parity
	 * errors.  See PV#854845.
	 */
	{
	int i;

	for (i=0; i < PIC_WR_REQ_BUFSIZE; i++) {
		bridge->p_wr_req_lower[i] = 0;
		bridge->p_wr_req_upper[i] = 0;
		bridge->p_wr_req_parity[i] = 0;
	}
	}

	break;
    case PCIBR_BRIDGETYPE_XBRIDGE:
	pcibr_soft->bs_int_ate_size = XBRIDGE_INTERNAL_ATES;
	pcibr_soft->bs_bridge_mode = 
	   ((bridge->b_wid_control & BRIDGE_CTRL_PCI_SPEED) >> 3);
	break;
    }

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, pcibr_vhdl,
		"pcibr_attach2: pcibr_soft=0x%x, mode=0x%x\n",
                pcibr_soft, pcibr_soft->bs_bridge_mode));
    pcibr_soft->bsi_err_intr = 0;

    /* Bridges up through REV C
     * are unable to set the direct
     * byteswappers to BYTE_STREAM.
     */
    if (pcibr_soft->bs_rev_num <= BRIDGE_PART_REV_C) {
	pcibr_soft->bs_pio_end_io = PCIIO_WORD_VALUES;
	pcibr_soft->bs_pio_end_mem = PCIIO_WORD_VALUES;
    }
#if PCIBR_SOFT_LIST
    /*
     * link all the pcibr_soft structs
     */
    {
	pcibr_list_p            self;

	NEW(self);
	self->bl_soft = pcibr_soft;
	self->bl_vhdl = pcibr_vhdl;
	self->bl_next = pcibr_list;
	pcibr_list = self;
    }
#endif /* PCIBR_SOFT_LIST */

    /*
     * get the name of this bridge vertex and keep the info. Use this
     * only where it is really needed now: like error interrupts.
     */
    s = dev_to_name(pcibr_vhdl, devnm, MAXDEVNAME);
    pcibr_soft->bs_name = kmalloc(strlen(s) + 1, GFP_KERNEL);
    strcpy(pcibr_soft->bs_name, s);

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, pcibr_vhdl,
		"pcibr_attach2: %s ASIC: rev %s (code=0x%x)\n",
		IS_XBRIDGE_SOFT(pcibr_soft) ? "XBridge" :
			IS_PIC_SOFT(pcibr_soft) ? "PIC" : "Bridge", 
		(rev == BRIDGE_PART_REV_A) ? "A" : 
                (rev == BRIDGE_PART_REV_B) ? "B" :
                (rev == BRIDGE_PART_REV_C) ? "C" :
                (rev == BRIDGE_PART_REV_D) ? "D" :
                (rev == XBRIDGE_PART_REV_A) ? "A" :
                (rev == XBRIDGE_PART_REV_B) ? "B" :
                (IS_PIC_PART_REV_A(rev)) ? "A" : 
                "unknown", rev, pcibr_soft->bs_name));

    info = xwidget_info_get(xconn_vhdl);
    pcibr_soft->bs_xid = xwidget_info_id_get(info);
    pcibr_soft->bs_master = xwidget_info_master_get(info);
    pcibr_soft->bs_mxid = xwidget_info_masterid_get(info);

    pcibr_soft->bs_first_slot = pcibr_soft->bs_min_slot;
    pcibr_soft->bs_last_slot = pcibr_soft->bs_max_slot;
    /*
     * Bridge can only reset slots 0, 1, 2, and 3.  Ibrick internal
     * slots 4, 5, 6, and 7 must be reset as a group, so do not
     * reset them.
     */
    pcibr_soft->bs_last_reset = 3;

    nasid = NASID_GET(bridge);

    if ((pcibr_soft->bs_bricktype = iobrick_type_get_nasid(nasid)) < 0)
	printk(KERN_WARNING "0x%p: Unknown bricktype : 0x%x\n", (void *)xconn_vhdl,
				(unsigned int)pcibr_soft->bs_bricktype);

    pcibr_soft->bs_moduleid = iobrick_module_get_nasid(nasid);

    if (pcibr_soft->bs_bricktype > 0) {
	switch (pcibr_soft->bs_bricktype) {
	case MODULE_PXBRICK:
	case MODULE_IXBRICK:
	case MODULE_OPUSBRICK:
	    pcibr_soft->bs_first_slot = 0;
	    pcibr_soft->bs_last_slot = 1;
	    pcibr_soft->bs_last_reset = 1;

	    /* If Bus 1 has IO9 then there are 4 devices in that bus.  Note
	     * we figure this out from klconfig since the kernel has yet to 
	     * probe
	     */
	    if (pcibr_widget_to_bus(pcibr_vhdl) == 1) {
		lboard_t *brd = (lboard_t *)KL_CONFIG_INFO(nasid);

		while (brd) {
		    if (brd->brd_flags & LOCAL_MASTER_IO6) {
			pcibr_soft->bs_last_slot = 3;
			pcibr_soft->bs_last_reset = 3;
		    }
		    brd = KLCF_NEXT(brd);
		}
	    }
	    break;
	case MODULE_PBRICK:
            pcibr_soft->bs_first_slot = 1;
            pcibr_soft->bs_last_slot = 2;
            pcibr_soft->bs_last_reset = 2;
            break;

        case MODULE_IBRICK:
	    /*
	     * Here's the current baseio layout for SN1 style systems:
	     *
	     *    0    1    2    3    4    5    6    7		slot#
	     *
	     *    x    scsi x    x    ioc3 usb  x    x  	O300 Ibrick
	     *
             * x == never occupied
             * E == external (add-in) slot
	     *
	     */
            pcibr_soft->bs_first_slot = 1;	/* Ibrick first slot == 1 */
            if (pcibr_soft->bs_xid == 0xe) { 
                pcibr_soft->bs_last_slot = 2;
                pcibr_soft->bs_last_reset = 2;
            } else {
		pcibr_soft->bs_last_slot = 6;
	    }
            break;

        case MODULE_CGBRICK:
            pcibr_soft->bs_first_slot = 0;
            pcibr_soft->bs_last_slot = 0;
            pcibr_soft->bs_last_reset = 0;
            break;

	default:
	    break;
        }

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, pcibr_vhdl,
		    "pcibr_attach2: %cbrick, slots %d-%d\n",
		    MODULE_GET_BTCHAR(pcibr_soft->bs_moduleid),
		    pcibr_soft->bs_first_slot, pcibr_soft->bs_last_slot));
    }

    /*
     * Initialize bridge and bus locks
     */
    spin_lock_init(&pcibr_soft->bs_lock);
#ifdef PIC_LATER
    mrinit(pcibr_soft->bs_bus_lock, "bus_lock");
#endif
    /*
     * If we have one, process the hints structure.
     */
    if (pcibr_hints) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_HINTS, pcibr_vhdl,
                    "pcibr_attach2: pcibr_hints=0x%x\n", pcibr_hints));

	rrb_fixed = pcibr_hints->ph_rrb_fixed;

	pcibr_soft->bs_rrb_fixed = rrb_fixed;

	if (pcibr_hints->ph_intr_bits) {
	    pcibr_soft->bs_intr_bits = pcibr_hints->ph_intr_bits;
	}

	for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot) {
	    int hslot = pcibr_hints->ph_host_slot[slot] - 1;

	    if (hslot < 0) {
		pcibr_soft->bs_slot[slot].host_slot = slot;
	    } else {
		pcibr_soft->bs_slot[slot].has_host = 1;
		pcibr_soft->bs_slot[slot].host_slot = hslot;
	    }
	}
    }
    /*
     * Set-up initial values for state fields
     */
    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot) {
	pcibr_soft->bs_slot[slot].bss_devio.bssd_space = PCIIO_SPACE_NONE;
	pcibr_soft->bs_slot[slot].bss_devio.bssd_ref_cnt = 0;
	pcibr_soft->bs_slot[slot].bss_d64_base = PCIBR_D64_BASE_UNSET;
	pcibr_soft->bs_slot[slot].bss_d32_base = PCIBR_D32_BASE_UNSET;
	pcibr_soft->bs_slot[slot].bss_ext_ates_active = ATOMIC_INIT(0);
	pcibr_soft->bs_rrb_valid_dflt[slot][VCHAN0] = -1;
    }

    for (ibit = 0; ibit < 8; ++ibit) {
	pcibr_soft->bs_intr[ibit].bsi_xtalk_intr = 0;
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_soft = pcibr_soft;
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_list = NULL;
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_stat = 
							&(bridge->b_int_status);
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_ibit = ibit;
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_hdlrcnt = 0;
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_shared = 0;
	pcibr_soft->bs_intr[ibit].bsi_pcibr_intr_wrap.iw_connected = 0;
    }

    /*
     * connect up our error handler.  PIC has 2 busses (thus resulting in 2
     * pcibr_soft structs under 1 widget), so only register a xwidget error
     * handler for PIC's bus0.  NOTE: for PIC pcibr_error_handler_wrapper()
     * is a wrapper routine we register that will call the real error handler
     * pcibr_error_handler() with the correct pcibr_soft struct.
     */
    if (IS_PIC_SOFT(pcibr_soft)) {
	if (busnum == 0) {
	    xwidget_error_register(xconn_vhdl, pcibr_error_handler_wrapper, pcibr_soft);
	}
    }

    /*
     * Initialize various Bridge registers.
     */
  
    /*
     * On pre-Rev.D bridges, set the PCI_RETRY_CNT
     * to zero to avoid dropping stores. (#475347)
     */
    if (rev < BRIDGE_PART_REV_D)
	bridge->b_bus_timeout &= ~BRIDGE_BUS_PCI_RETRY_MASK;

    /*
     * Clear all pending interrupts.
     */
    bridge->b_int_rst_stat = (BRIDGE_IRR_ALL_CLR);

    /* Initialize some PIC specific registers. */
    if (IS_PIC_SOFT(pcibr_soft)) {
	picreg_t pic_ctrl_reg = bridge->p_wid_control_64;

	/* Bridges Requester ID: bus = busnum, dev = 0, func = 0 */
	pic_ctrl_reg &= ~PIC_CTRL_BUS_NUM_MASK;
	pic_ctrl_reg |= PIC_CTRL_BUS_NUM(busnum);
	pic_ctrl_reg &= ~PIC_CTRL_DEV_NUM_MASK;
	pic_ctrl_reg &= ~PIC_CTRL_FUN_NUM_MASK;

	pic_ctrl_reg &= ~PIC_CTRL_NO_SNOOP;
	pic_ctrl_reg &= ~PIC_CTRL_RELAX_ORDER;

	/* enable parity checking on PICs internal RAM */
	pic_ctrl_reg |= PIC_CTRL_PAR_EN_RESP;
	pic_ctrl_reg |= PIC_CTRL_PAR_EN_ATE;
	/* PIC BRINGUP WAR (PV# 862253): dont enable write request
	 * parity checking.
	 */
	if (!PCIBR_WAR_ENABLED(PV862253, pcibr_soft)) {
	    pic_ctrl_reg |= PIC_CTRL_PAR_EN_REQ;
	}

	bridge->p_wid_control_64 = pic_ctrl_reg;
    }

    /*
     * Until otherwise set up,
     * assume all interrupts are
     * from slot 7(Bridge/Xbridge) or 3(PIC).
     * XXX. Not sure why we're doing this, made change for PIC
     * just to avoid setting reserved bits.
     */
    if (IS_PIC_SOFT(pcibr_soft))
	bridge->b_int_device = (uint32_t) 0x006db6db;

    {
	bridgereg_t             dirmap;
	paddr_t                 paddr;
	iopaddr_t               xbase;
	xwidgetnum_t            xport;
	iopaddr_t               offset;
	int                     num_entries = 0;
	int                     entry;
	cnodeid_t		cnodeid;
	nasid_t			nasid;

	/* Set the Bridge's 32-bit PCI to XTalk
	 * Direct Map register to the most useful
	 * value we can determine.  Note that we
	 * must use a single xid for all of:
	 *      direct-mapped 32-bit DMA accesses
	 *      direct-mapped 64-bit DMA accesses
	 *      DMA accesses through the PMU
	 *      interrupts
	 * This is the only way to guarantee that
	 * completion interrupts will reach a CPU
	 * after all DMA data has reached memory.
	 * (Of course, there may be a few special
	 * drivers/controlers that explicitly manage
	 * this ordering problem.)
	 */

	cnodeid = 0;  /* default node id */
	nasid = COMPACT_TO_NASID_NODEID(cnodeid);
	paddr = NODE_OFFSET(nasid) + 0;

	/* currently, we just assume that if we ask
	 * for a DMA mapping to "zero" the XIO
	 * host will transmute this into a request
	 * for the lowest hunk of memory.
	 */
	xbase = xtalk_dmatrans_addr(xconn_vhdl, 0,
				    paddr, PAGE_SIZE, 0);

	if (xbase != XIO_NOWHERE) {
	    if (XIO_PACKED(xbase)) {
		xport = XIO_PORT(xbase);
		xbase = XIO_ADDR(xbase);
	    } else
		xport = pcibr_soft->bs_mxid;

	    offset = xbase & ((1ull << BRIDGE_DIRMAP_OFF_ADDRSHFT) - 1ull);
	    xbase >>= BRIDGE_DIRMAP_OFF_ADDRSHFT;

	    dirmap = xport << BRIDGE_DIRMAP_W_ID_SHFT;

	    if (xbase)
		dirmap |= BRIDGE_DIRMAP_OFF & xbase;
	    else if (offset >= (512 << 20))
		dirmap |= BRIDGE_DIRMAP_ADD512;

	    bridge->b_dir_map = dirmap;
	}
	/*
	 * Set bridge's idea of page size according to the system's
	 * idea of "IO page size".  TBD: The idea of IO page size
	 * should really go away.
	 */
	/*
	 * ensure that we write and read without any interruption.
	 * The read following the write is required for the Bridge war
	 */
#if IOPGSIZE == 4096
        if (IS_PIC_SOFT(pcibr_soft)) {
            bridge->p_wid_control_64 &= ~BRIDGE_CTRL_PAGE_SIZE;
        } 
#elif IOPGSIZE == 16384
        if (IS_PIC_SOFT(pcibr_soft)) {
            bridge->p_wid_control_64 |= BRIDGE_CTRL_PAGE_SIZE;
        }
#else
	<<<Unable to deal with IOPGSIZE >>>;
#endif
	bridge->b_wid_control;		/* inval addr bug war */

	/* Initialize internal mapping entries */
	for (entry = 0; entry < pcibr_soft->bs_int_ate_size; entry++) {
	    bridge->b_int_ate_ram[entry].wr = 0;
	}

	/*
	 * Determine if there's external mapping SSRAM on this
	 * bridge.  Set up Bridge control register appropriately,
	 * inititlize SSRAM, and set software up to manage RAM
	 * entries as an allocatable resource.
	 *
	 * Currently, we just use the rm* routines to manage ATE
	 * allocation.  We should probably replace this with a
	 * Best Fit allocator.
	 *
	 * For now, if we have external SSRAM, avoid using
	 * the internal ssram: we can't turn PREFETCH on
	 * when we use the internal SSRAM; and besides,
	 * this also guarantees that no allocation will
	 * straddle the internal/external line, so we
	 * can increment ATE write addresses rather than
	 * recomparing against BRIDGE_INTERNAL_ATES every
	 * time.
	 */

	if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft))
		num_entries = 0;
	else
		num_entries = pcibr_init_ext_ate_ram(bridge);

	/* we always have 128 ATEs (512 for Xbridge) inside the chip
	 * even if disabled for debugging.
	 */
	pcibr_soft->bs_int_ate_resource.start = 0;
	pcibr_soft->bs_int_ate_resource.end = pcibr_soft->bs_int_ate_size - 1;

	if (num_entries > pcibr_soft->bs_int_ate_size) {
#if PCIBR_ATE_NOTBOTH			/* for debug -- forces us to use external ates */
	    printk("pcibr_attach: disabling internal ATEs.\n");
	    pcibr_ate_alloc(pcibr_soft, pcibr_soft->bs_int_ate_size);
#endif
	   pcibr_soft->bs_ext_ate_resource.start = pcibr_soft->bs_int_ate_size;
	   pcibr_soft->bs_ext_ate_resource.end = num_entries;
	}

        pcibr_soft->bs_allocated_ate_res = (void *) kmalloc(pcibr_soft->bs_int_ate_size * sizeof(unsigned long), GFP_KERNEL);
	memset(pcibr_soft->bs_allocated_ate_res, 0x0, pcibr_soft->bs_int_ate_size * sizeof(unsigned long));

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATE, pcibr_vhdl,
		    "pcibr_attach2: %d ATEs, %d internal & %d external\n",
		    num_entries ? num_entries : pcibr_soft->bs_int_ate_size,
		    pcibr_soft->bs_int_ate_size,
		    num_entries ? num_entries-pcibr_soft->bs_int_ate_size : 0));
    }

    {
	bridgereg_t             dirmap;
	iopaddr_t               xbase;

	/*
	 * now figure the *real* xtalk base address
	 * that dirmap sends us to.
	 */
	dirmap = bridge->b_dir_map;
	if (dirmap & BRIDGE_DIRMAP_OFF)
	    xbase = (iopaddr_t)(dirmap & BRIDGE_DIRMAP_OFF)
			<< BRIDGE_DIRMAP_OFF_ADDRSHFT;
	else if (dirmap & BRIDGE_DIRMAP_ADD512)
	    xbase = 512 << 20;
	else
	    xbase = 0;

	pcibr_soft->bs_dir_xbase = xbase;

	/* it is entirely possible that we may, at this
	 * point, have our dirmap pointing somewhere
	 * other than our "master" port.
	 */
	pcibr_soft->bs_dir_xport =
	    (dirmap & BRIDGE_DIRMAP_W_ID) >> BRIDGE_DIRMAP_W_ID_SHFT;
    }

    /* pcibr sources an error interrupt;
     * figure out where to send it.
     *
     * If any interrupts are enabled in bridge,
     * then the prom set us up and our interrupt
     * has already been reconnected in mlreset
     * above.
     *
     * Need to set the D_INTR_ISERR flag
     * in the dev_desc used for allocating the
     * error interrupt, so our interrupt will
     * be properly routed and prioritized.
     *
     * If our crosstalk provider wants to
     * fix widget error interrupts to specific
     * destinations, D_INTR_ISERR is how it
     * knows to do this.
     */

    xtalk_intr = xtalk_intr_alloc(xconn_vhdl, (device_desc_t)0, pcibr_vhdl);
	{
		int irq = ((hub_intr_t)xtalk_intr)->i_bit;
		int cpu = ((hub_intr_t)xtalk_intr)->i_cpuid;

		intr_unreserve_level(cpu, irq);
		((hub_intr_t)xtalk_intr)->i_bit = SGI_PCIBR_ERROR;
	}
    ASSERT(xtalk_intr != NULL);

    pcibr_soft->bsi_err_intr = xtalk_intr;

    /*
     * On IP35 with XBridge, we do some extra checks in pcibr_setwidint
     * in order to work around some addressing limitations.  In order
     * for that fire wall to work properly, we need to make sure we
     * start from a known clean state.
     */
    pcibr_clearwidint(bridge);

    xtalk_intr_connect(xtalk_intr, (intr_func_t) pcibr_error_intr_handler,
		(intr_arg_t) pcibr_soft, (xtalk_intr_setfunc_t)pcibr_setwidint, (void *)bridge);

    request_irq(SGI_PCIBR_ERROR, (void *)pcibr_error_intr_handler, SA_SHIRQ, "PCIBR error",
					(intr_arg_t) pcibr_soft);

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_INTR_ALLOC, pcibr_vhdl,
		"pcibr_setwidint: b_wid_int_upper=0x%x, b_wid_int_lower=0x%x\n",
		bridge->b_wid_int_upper, bridge->b_wid_int_lower));

    /*
     * now we can start handling error interrupts;
     * enable all of them.
     * NOTE: some PCI ints may already be enabled.
     */
    /* We read the INT_ENABLE register as a 64bit picreg_t for PIC and a
     * 32bit bridgereg_t for BRIDGE, but always process the result as a
     * 64bit value so the code can be "common" for both PIC and BRIDGE...
     */
    if (IS_PIC_SOFT(pcibr_soft)) {
	int_enable_64 = bridge->p_int_enable_64 | BRIDGE_ISR_ERRORS;
        int_enable = (uint64_t)int_enable_64;
#ifdef PFG_TEST
	int_enable = (uint64_t)0x7ffffeff7ffffeff;
#endif
    } 


#if BRIDGE_ERROR_INTR_WAR
    if (pcibr_soft->bs_rev_num == BRIDGE_PART_REV_A) {
	/*
	 * We commonly get master timeouts when talking to ql.
	 * We also see RESP_XTALK_ERROR and LLP_TX_RETRY interrupts.
	 * Insure that these are all disabled for now.
	 */
	int_enable &= ~(BRIDGE_IMR_PCI_MST_TIMEOUT |
			BRIDGE_ISR_RESP_XTLK_ERR |
			BRIDGE_ISR_LLP_TX_RETRY);
    }
    if (pcibr_soft->bs_rev_num < BRIDGE_PART_REV_C) {
	int_enable &= ~BRIDGE_ISR_BAD_XRESP_PKT;
    }
#endif				/* BRIDGE_ERROR_INTR_WAR */

#ifdef QL_SCSI_CTRL_WAR			/* for IP30 only */
    /* Really a QL rev A issue, but all newer hearts have newer QLs.
     * Forces all IO6/MSCSI to be new.
     */
    if (heart_rev() == HEART_REV_A)
	int_enable &= ~BRIDGE_IMR_PCI_MST_TIMEOUT;
#endif

#ifdef BRIDGE1_TIMEOUT_WAR
    if (pcibr_soft->bs_rev_num == BRIDGE_PART_REV_A) {
	/*
	 * Turn off these interrupts.  They can't be trusted in bridge 1
	 */
	int_enable &= ~(BRIDGE_IMR_XREAD_REQ_TIMEOUT |
			BRIDGE_IMR_UNEXP_RESP);
    }
#endif

    /* PIC BRINGUP WAR (PV# 856864 & 856865): allow the tnums that are
     * locked out to be freed up sooner (by timing out) so that the
     * read tnums are never completely used up.
     */
    if (IS_PIC_SOFT(pcibr_soft) && PCIBR_WAR_ENABLED(PV856864, pcibr_soft)) {
        int_enable &= ~PIC_ISR_PCIX_REQ_TOUT;
        int_enable &= ~BRIDGE_ISR_XREAD_REQ_TIMEOUT;

        bridge->b_wid_req_timeout = 0x750;
    }

    /*
     * PIC BRINGUP WAR (PV# 856866, 859504, 861476, 861478): Don't use
     * RRB0, RRB8, RRB1, and RRB9.  Assign them to DEVICE[2|3]--VCHAN3
     * so they are not used
     */
    if (IS_PIC_SOFT(pcibr_soft) && PCIBR_WAR_ENABLED(PV856866, pcibr_soft)) {
        bridge->b_even_resp |= 0x000f000f;
        bridge->b_odd_resp |= 0x000f000f;
    }

    if (IS_PIC_SOFT(pcibr_soft)) {
        bridge->p_int_enable_64 = (picreg_t)int_enable;
    }
    bridge->b_int_mode = 0;		/* do not send "clear interrupt" packets */

    bridge->b_wid_tflush;		/* wait until Bridge PIO complete */

    /*
     * Depending on the rev of bridge, disable certain features.
     * Easiest way seems to be to force the PCIBR_NOwhatever
     * flag to be on for all DMA calls, which overrides any
     * PCIBR_whatever flag or even the setting of whatever
     * from the PCIIO_DMA_class flags (or even from the other
     * PCIBR flags, since NO overrides YES).
     */
    pcibr_soft->bs_dma_flags = 0;

    /* PREFETCH:
     * Always completely disabled for REV.A;
     * at "pcibr_prefetch_enable_rev", anyone
     * asking for PCIIO_PREFETCH gets it.
     * Between these two points, you have to ask
     * for PCIBR_PREFETCH, which promises that
     * your driver knows about known Bridge WARs.
     */
    if (pcibr_soft->bs_rev_num < BRIDGE_PART_REV_B)
	pcibr_soft->bs_dma_flags |= PCIBR_NOPREFETCH;
    else if (pcibr_soft->bs_rev_num < 
		(BRIDGE_WIDGET_PART_NUM << 4))
	pcibr_soft->bs_dma_flags |= PCIIO_NOPREFETCH;

    /* WRITE_GATHER: Disabled */
    if (pcibr_soft->bs_rev_num < 
		(BRIDGE_WIDGET_PART_NUM << 4))
	pcibr_soft->bs_dma_flags |= PCIBR_NOWRITE_GATHER;

    /* PIC only supports 64-bit direct mapping in PCI-X mode.  Since
     * all PCI-X devices that initiate memory transactions must be
     * capable of generating 64-bit addressed, we force 64-bit DMAs.
     */
    if (IS_PCIX(pcibr_soft)) {
	pcibr_soft->bs_dma_flags |= PCIIO_DMA_A64;
    }

    {

    iopaddr_t               prom_base_addr = pcibr_soft->bs_xid << 24;
    int                     prom_base_size = 0x1000000;
    int			    status;
    struct resource	    *res;

    /* Allocate resource maps based on bus page size; for I/O and memory
     * space, free all pages except those in the base area and in the
     * range set by the PROM. 
     *
     * PROM creates BAR addresses in this format: 0x0ws00000 where w is
     * the widget number and s is the device register offset for the slot.
     */

    /* Setup the Bus's PCI IO Root Resource. */
    pcibr_soft->bs_io_win_root_resource.start = PCIBR_BUS_IO_BASE;
    pcibr_soft->bs_io_win_root_resource.end = 0xffffffff;
    res = (struct resource *) kmalloc( sizeof(struct resource), GFP_KERNEL);
    if (!res)
	panic("PCIBR:Unable to allocate resource structure\n");

    /* Block off the range used by PROM. */
    res->start = prom_base_addr;
    res->end = prom_base_addr + (prom_base_size - 1);
    status = request_resource(&pcibr_soft->bs_io_win_root_resource, res);
    if (status)
	panic("PCIBR:Unable to request_resource()\n");

    /* Setup the Small Window Root Resource */
    pcibr_soft->bs_swin_root_resource.start = PAGE_SIZE;
    pcibr_soft->bs_swin_root_resource.end = 0x000FFFFF;

    /* Setup the Bus's PCI Memory Root Resource */
    pcibr_soft->bs_mem_win_root_resource.start = 0x200000;
    pcibr_soft->bs_mem_win_root_resource.end = 0xffffffff;
    res = (struct resource *) kmalloc( sizeof(struct resource), GFP_KERNEL);
    if (!res)
        panic("PCIBR:Unable to allocate resource structure\n");

    /* Block off the range used by PROM. */
    res->start = prom_base_addr;
    res->end = prom_base_addr + (prom_base_size - 1);;
    status = request_resource(&pcibr_soft->bs_mem_win_root_resource, res);
    if (status)
        panic("PCIBR:Unable to request_resource()\n");

    }

    /* build "no-slot" connection point
     */
    pcibr_info = pcibr_device_info_new
	(pcibr_soft, PCIIO_SLOT_NONE, PCIIO_FUNC_NONE,
	 PCIIO_VENDOR_ID_NONE, PCIIO_DEVICE_ID_NONE);
    noslot_conn = pciio_device_info_register
	(pcibr_vhdl, &pcibr_info->f_c);

    /* Remember the no slot connection point info for tearing it
     * down during detach.
     */
    pcibr_soft->bs_noslot_conn = noslot_conn;
    pcibr_soft->bs_noslot_info = pcibr_info;
#if PCI_FBBE
    fast_back_to_back_enable = 1;
#endif

#if PCI_FBBE
    if (fast_back_to_back_enable) {
	/*
	 * All devices on the bus are capable of fast back to back, so
	 * we need to set the fast back to back bit in all devices on
	 * the bus that are capable of doing such accesses.
	 */
    }
#endif

    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot) {
	/* Find out what is out there */
	(void)pcibr_slot_info_init(pcibr_vhdl,slot);
    }
    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot)
	/* Set up the address space for this slot in the PCI land */
	(void)pcibr_slot_addr_space_init(pcibr_vhdl, slot);

    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot)
	/* Setup the device register */
	(void)pcibr_slot_device_init(pcibr_vhdl, slot);

    if (IS_PCIX(pcibr_soft)) {
        pcibr_soft->bs_pcix_rbar_inuse = 0;
        pcibr_soft->bs_pcix_rbar_avail = NUM_RBAR;
	pcibr_soft->bs_pcix_rbar_percent_allowed = 
					pcibr_pcix_rbars_calc(pcibr_soft);

	for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot)
	    /* Setup the PCI-X Read Buffer Attribute Registers (RBARs) */
	    (void)pcibr_slot_pcix_rbar_init(pcibr_soft, slot);
    }

    /* Set up convenience links */
    if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft))
	pcibr_bus_cnvlink(pcibr_soft->bs_vhdl);

    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot)
	/* Setup host/guest relations */
	(void)pcibr_slot_guest_info_init(pcibr_vhdl, slot);

    /* Handle initial RRB management for Bridge and Xbridge */
    pcibr_initial_rrb(pcibr_vhdl, 
                      pcibr_soft->bs_first_slot, pcibr_soft->bs_last_slot);
    
{  /* Before any drivers get called that may want to re-allocate
    * RRB's, let's get some special cases pre-allocated. Drivers
    * may override these pre-allocations, but by doing pre-allocations
    * now we're assured not to step all over what the driver intended.
    *
    * Note: Someday this should probably be moved over to pcibr_rrb.c
    */
    /*
     * Each Pbrick PCI bus only has slots 1 and 2.   Similarly for
     * widget 0xe on Ibricks.  Allocate RRB's accordingly.
     */
    if (pcibr_soft->bs_bricktype > 0) {
	switch (pcibr_soft->bs_bricktype) {
	case MODULE_PBRICK:
		do_pcibr_rrb_autoalloc(pcibr_soft, 1, VCHAN0, 8);
		do_pcibr_rrb_autoalloc(pcibr_soft, 2, VCHAN0, 8);
		break;
	case MODULE_IBRICK:
	  	/* port 0xe on the Ibrick only has slots 1 and 2 */
		if (pcibr_soft->bs_xid == 0xe) {
			do_pcibr_rrb_autoalloc(pcibr_soft, 1, VCHAN0, 8);
			do_pcibr_rrb_autoalloc(pcibr_soft, 2, VCHAN0, 8);
		}
		else {
		    	/* allocate one RRB for the serial port */
			do_pcibr_rrb_autoalloc(pcibr_soft, 0, VCHAN0, 1);
		}
		break;
	case MODULE_PXBRICK:
	case MODULE_IXBRICK:
	case MODULE_OPUSBRICK:
		/* 
		 * If the IO9 is in the PXBrick (bus1, slot1) allocate
                 * RRBs to all the devices
		 */
		if ((pcibr_widget_to_bus(pcibr_vhdl) == 1) &&
		    (pcibr_soft->bs_slot[0].bss_vendor_id == 0x10A9) &&
		    (pcibr_soft->bs_slot[0].bss_device_id == 0x100A)) {
			do_pcibr_rrb_autoalloc(pcibr_soft, 0, VCHAN0, 4);
			do_pcibr_rrb_autoalloc(pcibr_soft, 1, VCHAN0, 4);
			do_pcibr_rrb_autoalloc(pcibr_soft, 2, VCHAN0, 4);
			do_pcibr_rrb_autoalloc(pcibr_soft, 3, VCHAN0, 4);
		} else {
			do_pcibr_rrb_autoalloc(pcibr_soft, 0, VCHAN0, 4);
			do_pcibr_rrb_autoalloc(pcibr_soft, 1, VCHAN0, 4);
		}
		break;

        case MODULE_CGBRICK:
                do_pcibr_rrb_autoalloc(pcibr_soft, 0, VCHAN0, 8);
                break;
	} /* switch */
    }
}  /* OK Special RRB allocations are done. */

    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot)
	/* Call the device attach */
	(void)pcibr_slot_call_device_attach(pcibr_vhdl, slot, 0);

    pciio_device_attach(noslot_conn, (int)0);

    return 0;
}

/*
 * pcibr_detach:
 *	Detach the bridge device from the hwgraph after cleaning out all the 
 *	underlying vertices.
 */

int
pcibr_detach(vertex_hdl_t xconn)
{
    pciio_slot_t	slot;
    vertex_hdl_t	pcibr_vhdl;
    pcibr_soft_t	pcibr_soft;
    bridge_t		*bridge;
    unsigned             s;

    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DETACH, xconn, "pcibr_detach\n"));

    /* Get the bridge vertex from its xtalk connection point */
    if (hwgraph_traverse(xconn, EDGE_LBL_PCI, &pcibr_vhdl) != GRAPH_SUCCESS)
	return(1);

    pcibr_soft = pcibr_soft_get(pcibr_vhdl);
    bridge = pcibr_soft->bs_base;


    s = pcibr_lock(pcibr_soft);
    /* Disable the interrupts from the bridge */
    if (IS_PIC_SOFT(pcibr_soft)) {
	bridge->p_int_enable_64 = 0;
    }
    pcibr_unlock(pcibr_soft, s);

    /* Detach all the PCI devices talking to this bridge */
    for (slot = pcibr_soft->bs_min_slot; 
				slot < PCIBR_NUM_SLOTS(pcibr_soft); ++slot) {
	pcibr_slot_detach(pcibr_vhdl, slot, 0, (char *)NULL, (int *)NULL);
    }

    /* Unregister the no-slot connection point */
    pciio_device_info_unregister(pcibr_vhdl,
				 &(pcibr_soft->bs_noslot_info->f_c));

    kfree(pcibr_soft->bs_name);
    
    /* Disconnect the error interrupt and free the xtalk resources 
     * associated with it.
     */
    xtalk_intr_disconnect(pcibr_soft->bsi_err_intr);
    xtalk_intr_free(pcibr_soft->bsi_err_intr);

    /* Clear the software state maintained by the bridge driver for this
     * bridge.
     */
    DEL(pcibr_soft);
    /* Remove the Bridge revision labelled info */
    (void)hwgraph_info_remove_LBL(pcibr_vhdl, INFO_LBL_PCIBR_ASIC_REV, NULL);
    /* Remove the character device associated with this bridge */
    (void)hwgraph_edge_remove(pcibr_vhdl, EDGE_LBL_CONTROLLER, NULL);
    /* Remove the PCI bridge vertex */
    (void)hwgraph_edge_remove(xconn, EDGE_LBL_PCI, NULL);

    return(0);
}

int
pcibr_asic_rev(vertex_hdl_t pconn_vhdl)
{
    vertex_hdl_t          pcibr_vhdl;
    int                     tmp_vhdl;
    arbitrary_info_t        ainfo;

    if (GRAPH_SUCCESS !=
	hwgraph_traverse(pconn_vhdl, EDGE_LBL_MASTER, &pcibr_vhdl))
	return -1;

    tmp_vhdl = hwgraph_info_get_LBL(pcibr_vhdl, INFO_LBL_PCIBR_ASIC_REV, &ainfo);

    /*
     * Any hwgraph function that returns a vertex handle will implicity
     * increment that vertex's reference count.  The caller must explicity
     * decrement the vertex's referece count after the last reference to
     * that vertex.
     *
     * Decrement reference count incremented by call to hwgraph_traverse().
     *
     */
    hwgraph_vertex_unref(pcibr_vhdl);

    if (tmp_vhdl != GRAPH_SUCCESS) 
	return -1;
    return (int) ainfo;
}

/* =====================================================================
 *    PIO MANAGEMENT
 */

static iopaddr_t
pcibr_addr_pci_to_xio(vertex_hdl_t pconn_vhdl,
		      pciio_slot_t slot,
		      pciio_space_t space,
		      iopaddr_t pci_addr,
		      size_t req_size,
		      unsigned flags)
{
    pcibr_info_t            pcibr_info = pcibr_info_get(pconn_vhdl);
    pciio_info_t            pciio_info = &pcibr_info->f_c;
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    bridge_t               *bridge = pcibr_soft->bs_base;

    unsigned                bar;	/* which BASE reg on device is decoding */
    iopaddr_t               xio_addr = XIO_NOWHERE;
    iopaddr_t               base;	/* base of devio(x) mapped area on PCI */
    iopaddr_t               limit;	/* base of devio(x) mapped area on PCI */

    pciio_space_t           wspace;	/* which space device is decoding */
    iopaddr_t               wbase;	/* base of device decode on PCI */
    size_t                  wsize;	/* size of device decode on PCI */

    int                     try;	/* DevIO(x) window scanning order control */
    int			    maxtry, halftry;
    int                     win;	/* which DevIO(x) window is being used */
    pciio_space_t           mspace;	/* target space for devio(x) register */
    iopaddr_t               mbase;	/* base of devio(x) mapped area on PCI */
    size_t                  msize;	/* size of devio(x) mapped area on PCI */
    size_t                  mmask;	/* addr bits stored in Device(x) */
    char		    tmp_str[512];

    unsigned long           s;

    s = pcibr_lock(pcibr_soft);

    if (pcibr_soft->bs_slot[slot].has_host) {
	slot = pcibr_soft->bs_slot[slot].host_slot;
	pcibr_info = pcibr_soft->bs_slot[slot].bss_infos[0];

	/*
	 * Special case for dual-slot pci devices such as ioc3 on IP27
	 * baseio.  In these cases, pconn_vhdl should never be for a pci
	 * function on a subordiate PCI bus, so we can safely reset pciio_info
	 * to be the info struct embedded in pcibr_info.  Failure to do this
	 * results in using a bogus pciio_info_t for calculations done later
	 * in this routine.
	 */

	pciio_info = &pcibr_info->f_c;
    }
    if (space == PCIIO_SPACE_NONE)
	goto done;

    if (space == PCIIO_SPACE_CFG) {
	/*
	 * Usually, the first mapping
	 * established to a PCI device
	 * is to its config space.
	 *
	 * In any case, we definitely
	 * do NOT need to worry about
	 * PCI BASE registers, and
	 * MUST NOT attempt to point
	 * the DevIO(x) window at
	 * this access ...
	 */
	if (((flags & PCIIO_BYTE_STREAM) == 0) &&
	    ((pci_addr + req_size) <= BRIDGE_TYPE0_CFG_FUNC_OFF))
	    xio_addr = pci_addr + PCIBR_TYPE0_CFG_DEV(pcibr_soft, slot);

	goto done;
    }
    if (space == PCIIO_SPACE_ROM) {
	/* PIO to the Expansion Rom.
	 * Driver is responsible for
	 * enabling and disabling
	 * decodes properly.
	 */
	wbase = pciio_info->c_rbase;
	wsize = pciio_info->c_rsize;

	/*
	 * While the driver should know better
	 * than to attempt to map more space
	 * than the device is decoding, he might
	 * do it; better to bail out here.
	 */
	if ((pci_addr + req_size) > wsize)
	    goto done;

	pci_addr += wbase;
	space = PCIIO_SPACE_MEM;
    }
    /*
     * reduce window mappings to raw
     * space mappings (maybe allocating
     * windows), and try for DevIO(x)
     * usage (setting it if it is available).
     */
    bar = space - PCIIO_SPACE_WIN0;
    if (bar < 6) {
	wspace = pciio_info->c_window[bar].w_space;
	if (wspace == PCIIO_SPACE_NONE)
	    goto done;

	/* get PCI base and size */
	wbase = pciio_info->c_window[bar].w_base;
	wsize = pciio_info->c_window[bar].w_size;

	/*
	 * While the driver should know better
	 * than to attempt to map more space
	 * than the device is decoding, he might
	 * do it; better to bail out here.
	 */
	if ((pci_addr + req_size) > wsize)
	    goto done;

	/* shift from window relative to
	 * decoded space relative.
	 */
	pci_addr += wbase;
	space = wspace;
    } else
	bar = -1;

    /* Scan all the DevIO(x) windows twice looking for one
     * that can satisfy our request. The first time through,
     * only look at assigned windows; the second time, also
     * look at PCIIO_SPACE_NONE windows. Arrange the order
     * so we always look at our own window first.
     *
     * We will not attempt to satisfy a single request
     * by concatinating multiple windows.
     */
    maxtry = PCIBR_NUM_SLOTS(pcibr_soft) * 2;
    halftry = PCIBR_NUM_SLOTS(pcibr_soft) - 1;
    for (try = 0; try < maxtry; ++try) {
	bridgereg_t             devreg;
	unsigned                offset;

	/* calculate win based on slot, attempt, and max possible
	   devices on bus */
	win = (try + slot) % PCIBR_NUM_SLOTS(pcibr_soft);

	/* If this DevIO(x) mapping area can provide
	 * a mapping to this address, use it.
	 */
	msize = (win < 2) ? 0x200000 : 0x100000;
	mmask = -msize;
	if (space != PCIIO_SPACE_IO)
	    mmask &= 0x3FFFFFFF;

	offset = pci_addr & (msize - 1);

	/* If this window can't possibly handle that request,
	 * go on to the next window.
	 */
	if (((pci_addr & (msize - 1)) + req_size) > msize)
	    continue;

	devreg = pcibr_soft->bs_slot[win].bss_device;

	/* Is this window "nailed down"?
	 * If not, maybe we can use it.
	 * (only check this the second time through)
	 */
	mspace = pcibr_soft->bs_slot[win].bss_devio.bssd_space;
	if ((try > halftry) && (mspace == PCIIO_SPACE_NONE)) {

	    /* If this is the primary DevIO(x) window
	     * for some other device, skip it.
	     */
	    if ((win != slot) &&
		(PCIIO_VENDOR_ID_NONE !=
		 pcibr_soft->bs_slot[win].bss_vendor_id))
		continue;

	    /* It's a free window, and we fit in it.
	     * Set up Device(win) to our taste.
	     */
	    mbase = pci_addr & mmask;

	    /* check that we would really get from
	     * here to there.
	     */
	    if ((mbase | offset) != pci_addr)
		continue;

	    devreg &= ~BRIDGE_DEV_OFF_MASK;
	    if (space != PCIIO_SPACE_IO)
		devreg |= BRIDGE_DEV_DEV_IO_MEM;
	    else
		devreg &= ~BRIDGE_DEV_DEV_IO_MEM;
	    devreg |= (mbase >> 20) & BRIDGE_DEV_OFF_MASK;

	    /* default is WORD_VALUES.
	     * if you specify both,
	     * operation is undefined.
	     */
	    if (flags & PCIIO_BYTE_STREAM)
		devreg |= BRIDGE_DEV_DEV_SWAP;
	    else
		devreg &= ~BRIDGE_DEV_DEV_SWAP;

	    if (pcibr_soft->bs_slot[win].bss_device != devreg) {
		if ( IS_PIC_SOFT(pcibr_soft) ) {
			bridge->b_device[win].reg = devreg;
			pcibr_soft->bs_slot[win].bss_device = devreg;
			bridge->b_wid_tflush;   /* wait until Bridge PIO complete */
		}

#ifdef PCI_LATER
		PCIBR_DEBUG((PCIBR_DEBUG_DEVREG, pconn_vhdl, 
			    "pcibr_addr_pci_to_xio: Device(%d): %x\n",
			    win, devreg, device_bits));
#endif
	    }
	    pcibr_soft->bs_slot[win].bss_devio.bssd_space = space;
	    pcibr_soft->bs_slot[win].bss_devio.bssd_base = mbase;
	    xio_addr = PCIBR_BRIDGE_DEVIO(pcibr_soft, win) + (pci_addr - mbase);

            /* Increment this DevIO's use count */
            pcibr_soft->bs_slot[win].bss_devio.bssd_ref_cnt++;

            /* Save the DevIO register index used to access this BAR */
            if (bar != -1)
                pcibr_info->f_window[bar].w_devio_index = win;

	    /*
	     * The kernel only allows functions to have so many variable args,
	     * attempting to call PCIBR_DEBUG_ALWAYS() with more than 5 printk
	     * arguments fails so sprintf() it into a temporary string.
	     */
	    if (pcibr_debug_mask & PCIBR_DEBUG_PIOMAP) {
#ifdef PIC_LATER
	        sprintf(tmp_str, "pcibr_addr_pci_to_xio: map to %x[%x..%x] for "
		        "slot %d allocates DevIO(%d) Device(%d) set to %x\n",
		        space, space_desc, pci_addr, pci_addr + req_size - 1,
		        slot, win, win, devreg, device_bits);
#else
	        sprintf(tmp_str, "pcibr_addr_pci_to_xio: map to [%lx..%lx] for "
		        "slot %d allocates DevIO(%d) Device(%d) set to %lx\n",
		        (unsigned long)pci_addr, (unsigned long)(pci_addr + req_size - 1),
		        (unsigned int)slot, win, win, (unsigned long)devreg);
#endif
	        PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl, "%s", tmp_str));
	    }
	    goto done;
	}				/* endif DevIO(x) not pointed */
	mbase = pcibr_soft->bs_slot[win].bss_devio.bssd_base;

	/* Now check for request incompat with DevIO(x)
	 */
	if ((mspace != space) ||
	    (pci_addr < mbase) ||
	    ((pci_addr + req_size) > (mbase + msize)) ||
	    ((flags & PCIIO_BYTE_STREAM) && !(devreg & BRIDGE_DEV_DEV_SWAP)) ||
	    (!(flags & PCIIO_BYTE_STREAM) && (devreg & BRIDGE_DEV_DEV_SWAP)))
	    continue;

	/* DevIO(x) window is pointed at PCI space
	 * that includes our target. Calculate the
	 * final XIO address, release the lock and
	 * return.
	 */
	xio_addr = PCIBR_BRIDGE_DEVIO(pcibr_soft, win) + (pci_addr - mbase);

        /* Increment this DevIO's use count */
        pcibr_soft->bs_slot[win].bss_devio.bssd_ref_cnt++;

        /* Save the DevIO register index used to access this BAR */
        if (bar != -1)
            pcibr_info->f_window[bar].w_devio_index = win;

	if (pcibr_debug_mask & PCIBR_DEBUG_PIOMAP) {
#ifdef PIC_LATER
	    sprintf(tmp_str, "pcibr_addr_pci_to_xio: map to %x[%x..%x] for "
		    "slot %d uses DevIO(%d)\n", space, space_desc, pci_addr,
		    pci_addr + req_size - 1, slot, win);
#endif
	    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl, "%s", tmp_str));
	}
	goto done;
    }

    switch (space) {
	/*
	 * Accesses to device decode
	 * areas that do a not fit
	 * within the DevIO(x) space are
	 * modified to be accesses via
	 * the direct mapping areas.
	 *
	 * If necessary, drivers can
	 * explicitly ask for mappings
	 * into these address spaces,
	 * but this should never be needed.
	 */
    case PCIIO_SPACE_MEM:		/* "mem space" */
    case PCIIO_SPACE_MEM32:		/* "mem, use 32-bit-wide bus" */
	if (IS_PIC_BUSNUM_SOFT(pcibr_soft, 0)) {	/* PIC bus 0 */
		base = PICBRIDGE0_PCI_MEM32_BASE;
		limit = PICBRIDGE0_PCI_MEM32_LIMIT;
	} else if (IS_PIC_BUSNUM_SOFT(pcibr_soft, 1)) {	/* PIC bus 1 */
		base = PICBRIDGE1_PCI_MEM32_BASE;
		limit = PICBRIDGE1_PCI_MEM32_LIMIT;
	} else {					/* Bridge/Xbridge */
		base = BRIDGE_PCI_MEM32_BASE;
		limit = BRIDGE_PCI_MEM32_LIMIT;
	}

	if ((pci_addr + base + req_size - 1) <= limit)
	    xio_addr = pci_addr + base;
	break;

    case PCIIO_SPACE_MEM64:		/* "mem, use 64-bit-wide bus" */
	if (IS_PIC_BUSNUM_SOFT(pcibr_soft, 0)) {	/* PIC bus 0 */
		base = PICBRIDGE0_PCI_MEM64_BASE;
		limit = PICBRIDGE0_PCI_MEM64_LIMIT;
	} else if (IS_PIC_BUSNUM_SOFT(pcibr_soft, 1)) {	/* PIC bus 1 */
		base = PICBRIDGE1_PCI_MEM64_BASE;
		limit = PICBRIDGE1_PCI_MEM64_LIMIT;
	} else {					/* Bridge/Xbridge */
		base = BRIDGE_PCI_MEM64_BASE;
		limit = BRIDGE_PCI_MEM64_LIMIT;
	}

	if ((pci_addr + base + req_size - 1) <= limit)
	    xio_addr = pci_addr + base;
	break;

    case PCIIO_SPACE_IO:		/* "i/o space" */
	/*
	 * PIC bridges do not support big-window aliases into PCI I/O space
	 */
	if (IS_PIC_SOFT(pcibr_soft)) {
		xio_addr = XIO_NOWHERE;
		break;
	}

	/* Bridge Hardware Bug WAR #482741:
	 * The 4G area that maps directly from
	 * XIO space to PCI I/O space is busted
	 * until Bridge Rev D.
	 */
	if ((pcibr_soft->bs_rev_num > BRIDGE_PART_REV_C) &&
	    ((pci_addr + BRIDGE_PCI_IO_BASE + req_size - 1) <=
	     BRIDGE_PCI_IO_LIMIT))
	    xio_addr = pci_addr + BRIDGE_PCI_IO_BASE;
	break;
    }

    /* Check that "Direct PIO" byteswapping matches,
     * try to change it if it does not.
     */
    if (xio_addr != XIO_NOWHERE) {
	unsigned                bst;	/* nonzero to set bytestream */
	unsigned               *bfp;	/* addr of record of how swapper is set */
	unsigned                swb;	/* which control bit to mung */
	unsigned                bfo;	/* current swapper setting */
	unsigned                bfn;	/* desired swapper setting */

	bfp = ((space == PCIIO_SPACE_IO)
	       ? (&pcibr_soft->bs_pio_end_io)
	       : (&pcibr_soft->bs_pio_end_mem));

	bfo = *bfp;

	bst = flags & PCIIO_BYTE_STREAM;

	bfn = bst ? PCIIO_BYTE_STREAM : PCIIO_WORD_VALUES;

	if (bfn == bfo) {		/* we already match. */
	    ;
	} else if (bfo != 0) {		/* we have a conflict. */
	    if (pcibr_debug_mask & PCIBR_DEBUG_PIOMAP) {
#ifdef PIC_LATER
	        sprintf(tmp_str, "pcibr_addr_pci_to_xio: swap conflict in %x, "
		        "was%s%s, want%s%s\n", space, space_desc,
		        bfo & PCIIO_BYTE_STREAM ? " BYTE_STREAM" : "",
		        bfo & PCIIO_WORD_VALUES ? " WORD_VALUES" : "",
		        bfn & PCIIO_BYTE_STREAM ? " BYTE_STREAM" : "",
		        bfn & PCIIO_WORD_VALUES ? " WORD_VALUES" : "");
#endif
	        PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl, "%s", tmp_str));
	    }
	    xio_addr = XIO_NOWHERE;
	} else {			/* OK to make the change. */
	    swb = (space == PCIIO_SPACE_IO) ? BRIDGE_CTRL_IO_SWAP : BRIDGE_CTRL_MEM_SWAP;
	    if ( IS_PIC_SOFT(pcibr_soft) ) {
	    	picreg_t             octl, nctl;
		octl = bridge->p_wid_control_64;
		nctl = bst ? octl | (uint64_t)swb : octl & ((uint64_t)~swb);

		if (octl != nctl)		/* make the change if any */
			bridge->b_wid_control = nctl;
	    }
	    *bfp = bfn;			/* record the assignment */

	    if (pcibr_debug_mask & PCIBR_DEBUG_PIOMAP) {
#ifdef PIC_LATER
	        sprintf(tmp_str, "pcibr_addr_pci_to_xio: swap for %x set "
			"to%s%s\n", space, space_desc,
		        bfn & PCIIO_BYTE_STREAM ? " BYTE_STREAM" : "",
		        bfn & PCIIO_WORD_VALUES ? " WORD_VALUES" : "");
#endif
	        PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl, "%s", tmp_str));
	    }
	}
    }
  done:
    pcibr_unlock(pcibr_soft, s);
    return xio_addr;
}

/*ARGSUSED6 */
pcibr_piomap_t
pcibr_piomap_alloc(vertex_hdl_t pconn_vhdl,
		   device_desc_t dev_desc,
		   pciio_space_t space,
		   iopaddr_t pci_addr,
		   size_t req_size,
		   size_t req_size_max,
		   unsigned flags)
{
    pcibr_info_t	    pcibr_info = pcibr_info_get(pconn_vhdl);
    pciio_info_t            pciio_info = &pcibr_info->f_c;
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;

    pcibr_piomap_t         *mapptr;
    pcibr_piomap_t          maplist;
    pcibr_piomap_t          pcibr_piomap;
    iopaddr_t               xio_addr;
    xtalk_piomap_t          xtalk_piomap;
    unsigned long           s;

    /* Make sure that the req sizes are non-zero */
    if ((req_size < 1) || (req_size_max < 1)) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		    "pcibr_piomap_alloc: req_size | req_size_max < 1\n"));
	return NULL;
    }

    /*
     * Code to translate slot/space/addr
     * into xio_addr is common between
     * this routine and pcibr_piotrans_addr.
     */
    xio_addr = pcibr_addr_pci_to_xio(pconn_vhdl, pciio_slot, space, pci_addr, req_size, flags);

    if (xio_addr == XIO_NOWHERE) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		    "pcibr_piomap_alloc: xio_addr == XIO_NOWHERE\n"));
	return NULL;
    }

    /* Check the piomap list to see if there is already an allocated
     * piomap entry but not in use. If so use that one. Otherwise
     * allocate a new piomap entry and add it to the piomap list
     */
    mapptr = &(pcibr_info->f_piomap);

    s = pcibr_lock(pcibr_soft);
    for (pcibr_piomap = *mapptr;
	 pcibr_piomap != NULL;
	 pcibr_piomap = pcibr_piomap->bp_next) {
	if (pcibr_piomap->bp_mapsz == 0)
	    break;
    }

    if (pcibr_piomap)
	mapptr = NULL;
    else {
	pcibr_unlock(pcibr_soft, s);
	NEW(pcibr_piomap);
    }

    pcibr_piomap->bp_dev = pconn_vhdl;
    pcibr_piomap->bp_slot = PCIBR_DEVICE_TO_SLOT(pcibr_soft, pciio_slot);
    pcibr_piomap->bp_flags = flags;
    pcibr_piomap->bp_space = space;
    pcibr_piomap->bp_pciaddr = pci_addr;
    pcibr_piomap->bp_mapsz = req_size;
    pcibr_piomap->bp_soft = pcibr_soft;
    pcibr_piomap->bp_toc[0] = ATOMIC_INIT(0);

    if (mapptr) {
	s = pcibr_lock(pcibr_soft);
	maplist = *mapptr;
	pcibr_piomap->bp_next = maplist;
	*mapptr = pcibr_piomap;
    }
    pcibr_unlock(pcibr_soft, s);


    if (pcibr_piomap) {
	xtalk_piomap =
	    xtalk_piomap_alloc(xconn_vhdl, 0,
			       xio_addr,
			       req_size, req_size_max,
			       flags & PIOMAP_FLAGS);
	if (xtalk_piomap) {
	    pcibr_piomap->bp_xtalk_addr = xio_addr;
	    pcibr_piomap->bp_xtalk_pio = xtalk_piomap;
	} else {
	    pcibr_piomap->bp_mapsz = 0;
	    pcibr_piomap = 0;
	}
    }
    
    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		"pcibr_piomap_alloc: map=0x%x\n", pcibr_piomap));

    return pcibr_piomap;
}

/*ARGSUSED */
void
pcibr_piomap_free(pcibr_piomap_t pcibr_piomap)
{
    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pcibr_piomap->bp_dev,
		"pcibr_piomap_free: map=0x%x\n", pcibr_piomap));

    xtalk_piomap_free(pcibr_piomap->bp_xtalk_pio);
    pcibr_piomap->bp_xtalk_pio = 0;
    pcibr_piomap->bp_mapsz = 0;
}

/*ARGSUSED */
caddr_t
pcibr_piomap_addr(pcibr_piomap_t pcibr_piomap,
		  iopaddr_t pci_addr,
		  size_t req_size)
{
    caddr_t	addr;
    addr = xtalk_piomap_addr(pcibr_piomap->bp_xtalk_pio,
			     pcibr_piomap->bp_xtalk_addr +
			     pci_addr - pcibr_piomap->bp_pciaddr,
			     req_size);
    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pcibr_piomap->bp_dev,
                "pcibr_piomap_free: map=0x%x, addr=0x%x\n", 
		pcibr_piomap, addr));

    return(addr);
}

/*ARGSUSED */
void
pcibr_piomap_done(pcibr_piomap_t pcibr_piomap)
{
    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pcibr_piomap->bp_dev,
		"pcibr_piomap_done: map=0x%x\n", pcibr_piomap));
    xtalk_piomap_done(pcibr_piomap->bp_xtalk_pio);
}

/*ARGSUSED */
caddr_t
pcibr_piotrans_addr(vertex_hdl_t pconn_vhdl,
		    device_desc_t dev_desc,
		    pciio_space_t space,
		    iopaddr_t pci_addr,
		    size_t req_size,
		    unsigned flags)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;

    iopaddr_t               xio_addr;
    caddr_t		    addr;

    xio_addr = pcibr_addr_pci_to_xio(pconn_vhdl, pciio_slot, space, pci_addr, req_size, flags);

    if (xio_addr == XIO_NOWHERE) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_PIODIR, pconn_vhdl,
		    "pcibr_piotrans_addr: xio_addr == XIO_NOWHERE\n"));
	return NULL;
    }

    addr = xtalk_piotrans_addr(xconn_vhdl, 0, xio_addr, req_size, flags & PIOMAP_FLAGS);
    PCIBR_DEBUG((PCIBR_DEBUG_PIODIR, pconn_vhdl,
		"pcibr_piotrans_addr: xio_addr=0x%x, addr=0x%x\n",
		xio_addr, addr));
    return(addr);
}

/*
 * PIO Space allocation and management.
 *      Allocate and Manage the PCI PIO space (mem and io space)
 *      This routine is pretty simplistic at this time, and
 *      does pretty trivial management of allocation and freeing.
 *      The current scheme is prone for fragmentation.
 *      Change the scheme to use bitmaps.
 */

/*ARGSUSED */
iopaddr_t
pcibr_piospace_alloc(vertex_hdl_t pconn_vhdl,
		     device_desc_t dev_desc,
		     pciio_space_t space,
		     size_t req_size,
		     size_t alignment)
{
    pcibr_info_t            pcibr_info = pcibr_info_get(pconn_vhdl);
    pciio_info_t            pciio_info = &pcibr_info->f_c;
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);

    pciio_piospace_t        piosp;
    unsigned long           s;

    iopaddr_t               start_addr;
    size_t                  align_mask;

    /*
     * Check for proper alignment
     */
    ASSERT(alignment >= PAGE_SIZE);
    ASSERT((alignment & (alignment - 1)) == 0);

    align_mask = alignment - 1;
    s = pcibr_lock(pcibr_soft);

    /*
     * First look if a previously allocated chunk exists.
     */
    if ((piosp = pcibr_info->f_piospace)) {
	/*
	 * Look through the list for a right sized free chunk.
	 */
	do {
	    if (piosp->free &&
		(piosp->space == space) &&
		(piosp->count >= req_size) &&
		!(piosp->start & align_mask)) {
		piosp->free = 0;
		pcibr_unlock(pcibr_soft, s);
		return piosp->start;
	    }
	    piosp = piosp->next;
	} while (piosp);
    }
    ASSERT(!piosp);

    /*
     * Allocate PCI bus address, usually for the Universe chip driver;
     * do not pass window info since the actual PCI bus address
     * space will never be freed.  The space may be reused after it
     * is logically released by pcibr_piospace_free().
     */
    switch (space) {
    case PCIIO_SPACE_IO:
        start_addr = pcibr_bus_addr_alloc(pcibr_soft, NULL,
                                          PCIIO_SPACE_IO,
                                          0, req_size, alignment);
	break;

    case PCIIO_SPACE_MEM:
    case PCIIO_SPACE_MEM32:
        start_addr = pcibr_bus_addr_alloc(pcibr_soft, NULL,
                                          PCIIO_SPACE_MEM32,
                                          0, req_size, alignment);
	break;

    default:
	ASSERT(0);
	pcibr_unlock(pcibr_soft, s);
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		    "pcibr_piospace_alloc: unknown space %d\n", space));
	return 0;
    }

    /*
     * If too big a request, reject it.
     */
    if (!start_addr) {
	pcibr_unlock(pcibr_soft, s);
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		    "pcibr_piospace_alloc: request 0x%x to big\n", req_size));
	return 0;
    }

    NEW(piosp);
    piosp->free = 0;
    piosp->space = space;
    piosp->start = start_addr;
    piosp->count = req_size;
    piosp->next = pcibr_info->f_piospace;
    pcibr_info->f_piospace = piosp;

    pcibr_unlock(pcibr_soft, s);

    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		"pcibr_piospace_alloc: piosp=0x%x\n", piosp));

    return start_addr;
}

/*ARGSUSED */
void
pcibr_piospace_free(vertex_hdl_t pconn_vhdl,
		    pciio_space_t space,
		    iopaddr_t pciaddr,
		    size_t req_size)
{
    pcibr_info_t            pcibr_info = pcibr_info_get(pconn_vhdl);
#ifdef PIC_LATER
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pcibr_info->f_mfast;
#endif

    pciio_piospace_t        piosp;
    unsigned long           s;
    char                    name[1024];

    /*
     * Look through the bridge data structures for the pciio_piospace_t
     * structure corresponding to  'pciaddr'
     */
    s = pcibr_lock(pcibr_soft);
    piosp = pcibr_info->f_piospace;
    while (piosp) {
	/*
	 * Piospace free can only be for the complete
	 * chunk and not parts of it..
	 */
	if (piosp->start == pciaddr) {
	    if (piosp->count == req_size)
		break;
	    /*
	     * Improper size passed for freeing..
	     * Print a message and break;
	     */
	    hwgraph_vertex_name_get(pconn_vhdl, name, 1024);
	    printk(KERN_WARNING  "pcibr_piospace_free: error");
	    printk(KERN_WARNING  "Device %s freeing size (0x%lx) different than allocated (0x%lx)",
					name, req_size, piosp->count);
	    printk(KERN_WARNING  "Freeing 0x%lx instead", piosp->count);
	    break;
	}
	piosp = piosp->next;
    }

    if (!piosp) {
	printk(KERN_WARNING  
		"pcibr_piospace_free: Address 0x%lx size 0x%lx - No match\n",
		pciaddr, req_size);
	pcibr_unlock(pcibr_soft, s);
	return;
    }
    piosp->free = 1;
    pcibr_unlock(pcibr_soft, s);

    PCIBR_DEBUG((PCIBR_DEBUG_PIOMAP, pconn_vhdl,
		"pcibr_piospace_free: piosp=0x%x\n", piosp));
    return;
}

/* =====================================================================
 *    DMA MANAGEMENT
 *
 *      The Bridge ASIC provides three methods of doing
 *      DMA: via a "direct map" register available in
 *      32-bit PCI space (which selects a contiguous 2G
 *      address space on some other widget), via
 *      "direct" addressing via 64-bit PCI space (all
 *      destination information comes from the PCI
 *      address, including transfer attributes), and via
 *      a "mapped" region that allows a bunch of
 *      different small mappings to be established with
 *      the PMU.
 *
 *      For efficiency, we most prefer to use the 32-bit
 *      direct mapping facility, since it requires no
 *      resource allocations. The advantage of using the
 *      PMU over the 64-bit direct is that single-cycle
 *      PCI addressing can be used; the advantage of
 *      using 64-bit direct over PMU addressing is that
 *      we do not have to allocate entries in the PMU.
 */

/*
 * Convert PCI-generic software flags and Bridge-specific software flags
 * into Bridge-specific Direct Map attribute bits.
 */
static iopaddr_t
pcibr_flags_to_d64(unsigned flags, pcibr_soft_t pcibr_soft)
{
    iopaddr_t               attributes = 0;

    /* Sanity check: Bridge only allows use of VCHAN1 via 64-bit addrs */
#ifdef LATER
    ASSERT_ALWAYS(!(flags & PCIBR_VCHAN1) || (flags & PCIIO_DMA_A64));
#endif

    /* Generic macro flags
     */
    if (flags & PCIIO_DMA_DATA) {	/* standard data channel */
	attributes &= ~PCI64_ATTR_BAR;	/* no barrier bit */
	attributes |= PCI64_ATTR_PREF;	/* prefetch on */
    }
    if (flags & PCIIO_DMA_CMD) {	/* standard command channel */
	attributes |= PCI64_ATTR_BAR;	/* barrier bit on */
	attributes &= ~PCI64_ATTR_PREF;	/* disable prefetch */
    }
    /* Generic detail flags
     */
    if (flags & PCIIO_PREFETCH)
	attributes |= PCI64_ATTR_PREF;
    if (flags & PCIIO_NOPREFETCH)
	attributes &= ~PCI64_ATTR_PREF;

    /* the swap bit is in the address attributes for xbridge */
    if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) {
    	if (flags & PCIIO_BYTE_STREAM)
        	attributes |= PCI64_ATTR_SWAP;
    	if (flags & PCIIO_WORD_VALUES)
        	attributes &= ~PCI64_ATTR_SWAP;
    }

    /* Provider-specific flags
     */
    if (flags & PCIBR_BARRIER)
	attributes |= PCI64_ATTR_BAR;
    if (flags & PCIBR_NOBARRIER)
	attributes &= ~PCI64_ATTR_BAR;

    if (flags & PCIBR_PREFETCH)
	attributes |= PCI64_ATTR_PREF;
    if (flags & PCIBR_NOPREFETCH)
	attributes &= ~PCI64_ATTR_PREF;

    if (flags & PCIBR_PRECISE)
	attributes |= PCI64_ATTR_PREC;
    if (flags & PCIBR_NOPRECISE)
	attributes &= ~PCI64_ATTR_PREC;

    if (flags & PCIBR_VCHAN1)
	attributes |= PCI64_ATTR_VIRTUAL;
    if (flags & PCIBR_VCHAN0)
	attributes &= ~PCI64_ATTR_VIRTUAL;

    /* PIC in PCI-X mode only supports barrier & swap */
    if (IS_PCIX(pcibr_soft)) {
	attributes &= (PCI64_ATTR_BAR | PCI64_ATTR_SWAP);
    }

    return (attributes);
}

/*ARGSUSED */
pcibr_dmamap_t
pcibr_dmamap_alloc(vertex_hdl_t pconn_vhdl,
		   device_desc_t dev_desc,
		   size_t req_size_max,
		   unsigned flags)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;
    pciio_slot_t            slot;
    xwidgetnum_t            xio_port;

    xtalk_dmamap_t          xtalk_dmamap;
    pcibr_dmamap_t          pcibr_dmamap;
    int                     ate_count;
    int                     ate_index;
    int			    vchan = VCHAN0;

    /* merge in forced flags */
    flags |= pcibr_soft->bs_dma_flags;

    /*
     * On SNIA64, these maps are pre-allocated because pcibr_dmamap_alloc()
     * can be called within an interrupt thread.
     */
    pcibr_dmamap = (pcibr_dmamap_t)get_free_pciio_dmamap(pcibr_soft->bs_vhdl);

    if (!pcibr_dmamap)
	return 0;

    xtalk_dmamap = xtalk_dmamap_alloc(xconn_vhdl, dev_desc, req_size_max,
				      flags & DMAMAP_FLAGS);
    if (!xtalk_dmamap) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMAMAP, pconn_vhdl,
		    "pcibr_dmamap_alloc: xtalk_dmamap_alloc failed\n"));
	free_pciio_dmamap(pcibr_dmamap);
	return 0;
    }
    xio_port = pcibr_soft->bs_mxid;
    slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);

    pcibr_dmamap->bd_dev = pconn_vhdl;
    pcibr_dmamap->bd_slot = PCIBR_DEVICE_TO_SLOT(pcibr_soft, slot);
    pcibr_dmamap->bd_soft = pcibr_soft;
    pcibr_dmamap->bd_xtalk = xtalk_dmamap;
    pcibr_dmamap->bd_max_size = req_size_max;
    pcibr_dmamap->bd_xio_port = xio_port;

    if (flags & PCIIO_DMA_A64) {
	if (!pcibr_try_set_device(pcibr_soft, slot, flags, BRIDGE_DEV_D64_BITS)) {
	    iopaddr_t               pci_addr;
	    int                     have_rrbs;
	    int                     min_rrbs;

	    /* Device is capable of A64 operations,
	     * and the attributes of the DMA are
	     * consistent with any previous DMA
	     * mappings using shared resources.
	     */

	    pci_addr = pcibr_flags_to_d64(flags, pcibr_soft);

	    pcibr_dmamap->bd_flags = flags;
	    pcibr_dmamap->bd_xio_addr = 0;
	    pcibr_dmamap->bd_pci_addr = pci_addr;

	    /* If in PCI mode, make sure we have an RRB (or two). 
	     */
	    if (IS_PCI(pcibr_soft) && 
		!(pcibr_soft->bs_rrb_fixed & (1 << slot))) {
		if (flags & PCIBR_VCHAN1)
		    vchan = VCHAN1;
		have_rrbs = pcibr_soft->bs_rrb_valid[slot][vchan];
		if (have_rrbs < 2) {
		    if (pci_addr & PCI64_ATTR_PREF)
			min_rrbs = 2;
		    else
			min_rrbs = 1;
		    if (have_rrbs < min_rrbs)
			do_pcibr_rrb_autoalloc(pcibr_soft, slot, vchan,
					       min_rrbs - have_rrbs);
		}
	    }
	    PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP | PCIBR_DEBUG_DMADIR, pconn_vhdl,
		 	"pcibr_dmamap_alloc: using direct64, map=0x%x\n",
			pcibr_dmamap));
	    return pcibr_dmamap;
	}
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMAMAP | PCIBR_DEBUG_DMADIR, pconn_vhdl,
		    "pcibr_dmamap_alloc: unable to use direct64\n"));

	/* PIC only supports 64-bit direct mapping in PCI-X mode. */
	if (IS_PCIX(pcibr_soft)) {
	    DEL(pcibr_dmamap);
	    return 0;
	}

	flags &= ~PCIIO_DMA_A64;
    }
    if (flags & PCIIO_FIXED) {
	/* warning: mappings may fail later,
	 * if direct32 can't get to the address.
	 */
	if (!pcibr_try_set_device(pcibr_soft, slot, flags, BRIDGE_DEV_D32_BITS)) {
	    /* User desires DIRECT A32 operations,
	     * and the attributes of the DMA are
	     * consistent with any previous DMA
	     * mappings using shared resources.
	     * Mapping calls may fail if target
	     * is outside the direct32 range.
	     */
	    PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP | PCIBR_DEBUG_DMADIR, pconn_vhdl,
			"pcibr_dmamap_alloc: using direct32, map=0x%x\n", 
			pcibr_dmamap));
	    pcibr_dmamap->bd_flags = flags;
	    pcibr_dmamap->bd_xio_addr = pcibr_soft->bs_dir_xbase;
	    pcibr_dmamap->bd_pci_addr = PCI32_DIRECT_BASE;
	    return pcibr_dmamap;
	}
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMAMAP | PCIBR_DEBUG_DMADIR, pconn_vhdl,
		    "pcibr_dmamap_alloc: unable to use direct32\n"));

	/* If the user demands FIXED and we can't
	 * give it to him, fail.
	 */
	xtalk_dmamap_free(xtalk_dmamap);
	free_pciio_dmamap(pcibr_dmamap);
	return 0;
    }
    /*
     * Allocate Address Translation Entries from the mapping RAM.
     * Unless the PCIBR_NO_ATE_ROUNDUP flag is specified,
     * the maximum number of ATEs is based on the worst-case
     * scenario, where the requested target is in the
     * last byte of an ATE; thus, mapping IOPGSIZE+2
     * does end up requiring three ATEs.
     */
    if (!(flags & PCIBR_NO_ATE_ROUNDUP)) {
	ate_count = IOPG((IOPGSIZE - 1)	/* worst case start offset */
		     +req_size_max	/* max mapping bytes */
		     - 1) + 1;		/* round UP */
    } else {	/* assume requested target is page aligned */
	ate_count = IOPG(req_size_max   /* max mapping bytes */
		     - 1) + 1;		/* round UP */
    }

    ate_index = pcibr_ate_alloc(pcibr_soft, ate_count);

    if (ate_index != -1) {
	if (!pcibr_try_set_device(pcibr_soft, slot, flags, BRIDGE_DEV_PMU_BITS)) {
	    bridge_ate_t            ate_proto;
	    int                     have_rrbs;
	    int                     min_rrbs;

	    PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP, pconn_vhdl,
			"pcibr_dmamap_alloc: using PMU, ate_index=%d, "
			"pcibr_dmamap=0x%x\n", ate_index, pcibr_dmamap));

	    ate_proto = pcibr_flags_to_ate(flags);

	    pcibr_dmamap->bd_flags = flags;
	    pcibr_dmamap->bd_pci_addr =
		PCI32_MAPPED_BASE + IOPGSIZE * ate_index;
	    /*
	     * for xbridge the byte-swap bit == bit 29 of PCI address
	     */
	    if (IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) {
		    if (flags & PCIIO_BYTE_STREAM)
			    ATE_SWAP_ON(pcibr_dmamap->bd_pci_addr);
		    /*
		     * If swap was set in bss_device in pcibr_endian_set()
		     * we need to change the address bit.
		     */
		    if (pcibr_soft->bs_slot[slot].bss_device & 
							BRIDGE_DEV_SWAP_PMU)
			    ATE_SWAP_ON(pcibr_dmamap->bd_pci_addr);
		    if (flags & PCIIO_WORD_VALUES)
			    ATE_SWAP_OFF(pcibr_dmamap->bd_pci_addr);
	    }
	    pcibr_dmamap->bd_xio_addr = 0;
	    pcibr_dmamap->bd_ate_ptr = pcibr_ate_addr(pcibr_soft, ate_index);
	    pcibr_dmamap->bd_ate_index = ate_index;
	    pcibr_dmamap->bd_ate_count = ate_count;
	    pcibr_dmamap->bd_ate_proto = ate_proto;

	    /* Make sure we have an RRB (or two).
	     */
	    if (!(pcibr_soft->bs_rrb_fixed & (1 << slot))) {
		have_rrbs = pcibr_soft->bs_rrb_valid[slot][vchan];
		if (have_rrbs < 2) {
		    if (ate_proto & ATE_PREF)
			min_rrbs = 2;
		    else
			min_rrbs = 1;
		    if (have_rrbs < min_rrbs)
			do_pcibr_rrb_autoalloc(pcibr_soft, slot, vchan,
					       min_rrbs - have_rrbs);
		}
	    }
	    if (ate_index >= pcibr_soft->bs_int_ate_size && 
				!IS_XBRIDGE_OR_PIC_SOFT(pcibr_soft)) {
		bridge_t               *bridge = pcibr_soft->bs_base;
		volatile unsigned      *cmd_regp;
		unsigned                cmd_reg = 0;
		unsigned long           s;

		pcibr_dmamap->bd_flags |= PCIBR_DMAMAP_SSRAM;

		s = pcibr_lock(pcibr_soft);
		cmd_regp = pcibr_slot_config_addr(bridge, slot, 
						PCI_CFG_COMMAND/4);
		if ( IS_PIC_SOFT(pcibr_soft) ) {
			cmd_reg = pcibr_slot_config_get(bridge, slot, PCI_CFG_COMMAND/4);
		}
		pcibr_soft->bs_slot[slot].bss_cmd_pointer = cmd_regp;
		pcibr_soft->bs_slot[slot].bss_cmd_shadow = cmd_reg;
		pcibr_unlock(pcibr_soft, s);
	    }
	    return pcibr_dmamap;
	}
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMAMAP, pconn_vhdl,
		    "pcibr_dmamap_alloc: PMU use failed, ate_index=%d\n",
		    ate_index));

	pcibr_ate_free(pcibr_soft, ate_index, ate_count);
    }
    /* total failure: sorry, you just can't
     * get from here to there that way.
     */
    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMAMAP, pconn_vhdl,
		"pcibr_dmamap_alloc: complete failure.\n"));
    xtalk_dmamap_free(xtalk_dmamap);
    free_pciio_dmamap(pcibr_dmamap);
    return 0;
}

/*ARGSUSED */
void
pcibr_dmamap_free(pcibr_dmamap_t pcibr_dmamap)
{
    pcibr_soft_t            pcibr_soft = pcibr_dmamap->bd_soft;
    pciio_slot_t            slot = PCIBR_SLOT_TO_DEVICE(pcibr_soft,
							pcibr_dmamap->bd_slot);

    unsigned                flags = pcibr_dmamap->bd_flags;

    /* Make sure that bss_ext_ates_active
     * is properly kept up to date.
     */

    if (PCIBR_DMAMAP_BUSY & flags)
	if (PCIBR_DMAMAP_SSRAM & flags)
	    atomic_dec(&(pcibr_soft->bs_slot[slot]. bss_ext_ates_active));

    xtalk_dmamap_free(pcibr_dmamap->bd_xtalk);

    if (pcibr_dmamap->bd_flags & PCIIO_DMA_A64) {
	pcibr_release_device(pcibr_soft, slot, BRIDGE_DEV_D64_BITS);
    }
    if (pcibr_dmamap->bd_ate_count) {
	pcibr_ate_free(pcibr_dmamap->bd_soft,
		       pcibr_dmamap->bd_ate_index,
		       pcibr_dmamap->bd_ate_count);
	pcibr_release_device(pcibr_soft, slot, BRIDGE_DEV_PMU_BITS);
    }

    PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP, pcibr_dmamap->bd_dev,
		"pcibr_dmamap_free: pcibr_dmamap=0x%x\n", pcibr_dmamap));

    free_pciio_dmamap(pcibr_dmamap);
}

/*
 *    pcibr_addr_xio_to_pci: given a PIO range, hand
 *      back the corresponding base PCI MEM address;
 *      this is used to short-circuit DMA requests that
 *      loop back onto this PCI bus.
 */
static iopaddr_t
pcibr_addr_xio_to_pci(pcibr_soft_t soft,
		      iopaddr_t xio_addr,
		      size_t req_size)
{
    iopaddr_t               xio_lim = xio_addr + req_size - 1;
    iopaddr_t               pci_addr;
    pciio_slot_t            slot;

    if (IS_PIC_BUSNUM_SOFT(soft, 0)) {
    	if ((xio_addr >= PICBRIDGE0_PCI_MEM32_BASE) &&
	    (xio_lim <= PICBRIDGE0_PCI_MEM32_LIMIT)) {
	    pci_addr = xio_addr - PICBRIDGE0_PCI_MEM32_BASE;
	    return pci_addr;
    	}
    	if ((xio_addr >= PICBRIDGE0_PCI_MEM64_BASE) &&
	    (xio_lim <= PICBRIDGE0_PCI_MEM64_LIMIT)) {
	    pci_addr = xio_addr - PICBRIDGE0_PCI_MEM64_BASE;
	    return pci_addr;
    	}
    } else if (IS_PIC_BUSNUM_SOFT(soft, 1)) {
    	if ((xio_addr >= PICBRIDGE1_PCI_MEM32_BASE) &&
	    (xio_lim <= PICBRIDGE1_PCI_MEM32_LIMIT)) {
	    pci_addr = xio_addr - PICBRIDGE1_PCI_MEM32_BASE;
	    return pci_addr;
    	}
    	if ((xio_addr >= PICBRIDGE1_PCI_MEM64_BASE) &&
	    (xio_lim <= PICBRIDGE1_PCI_MEM64_LIMIT)) {
	    pci_addr = xio_addr - PICBRIDGE1_PCI_MEM64_BASE;
	    return pci_addr;
    	}
    } else {
    if ((xio_addr >= BRIDGE_PCI_MEM32_BASE) &&
	(xio_lim <= BRIDGE_PCI_MEM32_LIMIT)) {
	pci_addr = xio_addr - BRIDGE_PCI_MEM32_BASE;
	return pci_addr;
    }
    if ((xio_addr >= BRIDGE_PCI_MEM64_BASE) &&
	(xio_lim <= BRIDGE_PCI_MEM64_LIMIT)) {
	pci_addr = xio_addr - BRIDGE_PCI_MEM64_BASE;
	return pci_addr;
    }
    }
    for (slot = soft->bs_min_slot; slot < PCIBR_NUM_SLOTS(soft); ++slot)
	if ((xio_addr >= PCIBR_BRIDGE_DEVIO(soft, slot)) &&
	    (xio_lim < PCIBR_BRIDGE_DEVIO(soft, slot + 1))) {
	    bridgereg_t             dev;

	    dev = soft->bs_slot[slot].bss_device;
	    pci_addr = dev & BRIDGE_DEV_OFF_MASK;
	    pci_addr <<= BRIDGE_DEV_OFF_ADDR_SHFT;
	    pci_addr += xio_addr - PCIBR_BRIDGE_DEVIO(soft, slot);
	    return (dev & BRIDGE_DEV_DEV_IO_MEM) ? pci_addr : PCI_NOWHERE;
	}
    return 0;
}

/*ARGSUSED */
iopaddr_t
pcibr_dmamap_addr(pcibr_dmamap_t pcibr_dmamap,
		  paddr_t paddr,
		  size_t req_size)
{
    pcibr_soft_t            pcibr_soft;
    iopaddr_t               xio_addr;
    xwidgetnum_t            xio_port;
    iopaddr_t               pci_addr;
    unsigned                flags;

    ASSERT(pcibr_dmamap != NULL);
    ASSERT(req_size > 0);
    ASSERT(req_size <= pcibr_dmamap->bd_max_size);

    pcibr_soft = pcibr_dmamap->bd_soft;

    flags = pcibr_dmamap->bd_flags;

    xio_addr = xtalk_dmamap_addr(pcibr_dmamap->bd_xtalk, paddr, req_size);
    if (XIO_PACKED(xio_addr)) {
	xio_port = XIO_PORT(xio_addr);
	xio_addr = XIO_ADDR(xio_addr);
    } else
	xio_port = pcibr_dmamap->bd_xio_port;

    /* If this DMA is to an address that
     * refers back to this Bridge chip,
     * reduce it back to the correct
     * PCI MEM address.
     */
    if (xio_port == pcibr_soft->bs_xid) {
	pci_addr = pcibr_addr_xio_to_pci(pcibr_soft, xio_addr, req_size);
    } else if (flags & PCIIO_DMA_A64) {
	/* A64 DMA:
	 * always use 64-bit direct mapping,
	 * which always works.
	 * Device(x) was set up during
	 * dmamap allocation.
	 */

	/* attributes are already bundled up into bd_pci_addr.
	 */
	pci_addr = pcibr_dmamap->bd_pci_addr
	    | ((uint64_t) xio_port << PCI64_ATTR_TARG_SHFT)
	    | xio_addr;

	/* Bridge Hardware WAR #482836:
	 * If the transfer is not cache aligned
	 * and the Bridge Rev is <= B, force
	 * prefetch to be off.
	 */
	if (flags & PCIBR_NOPREFETCH)
	    pci_addr &= ~PCI64_ATTR_PREF;

	PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP | PCIBR_DEBUG_DMADIR, 
		    pcibr_dmamap->bd_dev,
		    "pcibr_dmamap_addr: (direct64): wanted paddr [0x%x..0x%x] "
		    "XIO port 0x%x offset 0x%x, returning PCI 0x%x\n",
		    paddr, paddr + req_size - 1, xio_port, xio_addr, pci_addr));

    } else if (flags & PCIIO_FIXED) {
	/* A32 direct DMA:
	 * always use 32-bit direct mapping,
	 * which may fail.
	 * Device(x) was set up during
	 * dmamap allocation.
	 */

	if (xio_port != pcibr_soft->bs_dir_xport)
	    pci_addr = 0;		/* wrong DIDN */
	else if (xio_addr < pcibr_dmamap->bd_xio_addr)
	    pci_addr = 0;		/* out of range */
	else if ((xio_addr + req_size) >
		 (pcibr_dmamap->bd_xio_addr + BRIDGE_DMA_DIRECT_SIZE))
	    pci_addr = 0;		/* out of range */
	else
	    pci_addr = pcibr_dmamap->bd_pci_addr +
		xio_addr - pcibr_dmamap->bd_xio_addr;

	PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP | PCIBR_DEBUG_DMADIR, 
		    pcibr_dmamap->bd_dev,
		    "pcibr_dmamap_addr (direct32): wanted paddr [0x%x..0x%x] "
		    "XIO port 0x%x offset 0x%x, returning PCI 0x%x\n",
		    paddr, paddr + req_size - 1, xio_port, xio_addr, pci_addr));

    } else {
	bridge_t               *bridge = pcibr_soft->bs_base;
	iopaddr_t               offset = IOPGOFF(xio_addr);
	bridge_ate_t            ate_proto = pcibr_dmamap->bd_ate_proto;
	int                     ate_count = IOPG(offset + req_size - 1) + 1;

	int                     ate_index = pcibr_dmamap->bd_ate_index;
	unsigned                cmd_regs[8];
	unsigned                s;

#if PCIBR_FREEZE_TIME
	int                     ate_total = ate_count;
	unsigned                freeze_time;
#endif
	bridge_ate_p            ate_ptr = pcibr_dmamap->bd_ate_ptr;
	bridge_ate_t            ate;

	/* Bridge Hardware WAR #482836:
	 * If the transfer is not cache aligned
	 * and the Bridge Rev is <= B, force
	 * prefetch to be off.
	 */
	if (flags & PCIBR_NOPREFETCH)
	    ate_proto &= ~ATE_PREF;

	ate = ate_proto
	    | (xio_port << ATE_TIDSHIFT)
	    | (xio_addr - offset);

	pci_addr = pcibr_dmamap->bd_pci_addr + offset;

	/* Fill in our mapping registers
	 * with the appropriate xtalk data,
	 * and hand back the PCI address.
	 */

	ASSERT(ate_count > 0);
	if (ate_count <= pcibr_dmamap->bd_ate_count) {
		ATE_FREEZE();
		ATE_WRITE();
		ATE_THAW();
		if ( IS_PIC_SOFT(pcibr_soft) ) {
			bridge->b_wid_tflush;	/* wait until Bridge PIO complete */
		}
		PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP, pcibr_dmamap->bd_dev,
			    "pcibr_dmamap_addr (PMU) : wanted paddr "
			    "[0x%x..0x%x] returning PCI 0x%x\n", 
			    paddr, paddr + req_size - 1, pci_addr));

	} else {
		/* The number of ATE's required is greater than the number
		 * allocated for this map. One way this can happen is if
		 * pcibr_dmamap_alloc() was called with the PCIBR_NO_ATE_ROUNDUP
		 * flag, and then when that map is used (right now), the
		 * target address tells us we really did need to roundup.
		 * The other possibility is that the map is just plain too
		 * small to handle the requested target area.
		 */
		PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMAMAP, pcibr_dmamap->bd_dev, 
		            "pcibr_dmamap_addr (PMU) : wanted paddr "
			    "[0x%x..0x%x] ate_count 0x%x bd_ate_count 0x%x "
			    "ATE's required > number allocated\n",
			     paddr, paddr + req_size - 1,
			     ate_count, pcibr_dmamap->bd_ate_count));
		pci_addr = 0;
	}

    }
    return pci_addr;
}

/*ARGSUSED */
void
pcibr_dmamap_done(pcibr_dmamap_t pcibr_dmamap)
{
#ifdef PIC_LATER
    pcibr_soft_t            pcibr_soft = pcibr_dmamap->bd_soft;
    pciio_slot_t            slot = PCIBR_SLOT_TO_DEVICE(pcibr_soft,
#endif
    /*
     * We could go through and invalidate ATEs here;
     * for performance reasons, we don't.
     * We also don't enforce the strict alternation
     * between _addr/_list and _done, but Hub does.
     */

    if (pcibr_dmamap->bd_flags & PCIBR_DMAMAP_BUSY) {
	pcibr_dmamap->bd_flags &= ~PCIBR_DMAMAP_BUSY;

	if (pcibr_dmamap->bd_flags & PCIBR_DMAMAP_SSRAM)
	    atomic_dec(&(pcibr_dmamap->bd_soft->bs_slot[pcibr_dmamap->bd_slot]. bss_ext_ates_active));
    }
    xtalk_dmamap_done(pcibr_dmamap->bd_xtalk);

    PCIBR_DEBUG((PCIBR_DEBUG_DMAMAP, pcibr_dmamap->bd_dev,
		"pcibr_dmamap_done: pcibr_dmamap=0x%x\n", pcibr_dmamap));
}


/*
 * For each bridge, the DIR_OFF value in the Direct Mapping Register
 * determines the PCI to Crosstalk memory mapping to be used for all
 * 32-bit Direct Mapping memory accesses. This mapping can be to any
 * node in the system. This function will return that compact node id.
 */

/*ARGSUSED */
cnodeid_t
pcibr_get_dmatrans_node(vertex_hdl_t pconn_vhdl)
{

	pciio_info_t	pciio_info = pciio_info_get(pconn_vhdl);
	pcibr_soft_t	pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);

	return(NASID_TO_COMPACT_NODEID(NASID_GET(pcibr_soft->bs_dir_xbase)));
}

/*ARGSUSED */
iopaddr_t
pcibr_dmatrans_addr(vertex_hdl_t pconn_vhdl,
		    device_desc_t dev_desc,
		    paddr_t paddr,
		    size_t req_size,
		    unsigned flags)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
    pcibr_soft_slot_t       slotp = &pcibr_soft->bs_slot[pciio_slot];

    xwidgetnum_t            xio_port;
    iopaddr_t               xio_addr;
    iopaddr_t               pci_addr;

    int                     have_rrbs;
    int                     min_rrbs;
    int			    vchan = VCHAN0;

    /* merge in forced flags */
    flags |= pcibr_soft->bs_dma_flags;

    xio_addr = xtalk_dmatrans_addr(xconn_vhdl, 0, paddr, req_size,
				   flags & DMAMAP_FLAGS);
    if (!xio_addr) {
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMADIR, pconn_vhdl,
		    "pcibr_dmatrans_addr: wanted paddr [0x%x..0x%x], "
		    "xtalk_dmatrans_addr failed with 0x%x\n",
		    paddr, paddr + req_size - 1, xio_addr));
	return 0;
    }
    /*
     * find which XIO port this goes to.
     */
    if (XIO_PACKED(xio_addr)) {
	if (xio_addr == XIO_NOWHERE) {
	    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DMADIR, pconn_vhdl,
		        "pcibr_dmatrans_addr: wanted paddr [0x%x..0x%x], "
		        "xtalk_dmatrans_addr failed with XIO_NOWHERE\n",
		        paddr, paddr + req_size - 1));
	    return 0;
	}
	xio_port = XIO_PORT(xio_addr);
	xio_addr = XIO_ADDR(xio_addr);

    } else
	xio_port = pcibr_soft->bs_mxid;

    /*
     * If this DMA comes back to us,
     * return the PCI MEM address on
     * which it would land, or NULL
     * if the target is something
     * on bridge other than PCI MEM.
     */
    if (xio_port == pcibr_soft->bs_xid) {
	pci_addr = pcibr_addr_xio_to_pci(pcibr_soft, xio_addr, req_size);
        PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
		    "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
		    "xio_port=0x%x, pci_addr=0x%x\n",
		    paddr, paddr + req_size - 1, xio_port, pci_addr));
	return pci_addr;
    }
    /* If the caller can use A64, try to
     * satisfy the request with the 64-bit
     * direct map. This can fail if the
     * configuration bits in Device(x)
     * conflict with our flags.
     */

    if (flags & PCIIO_DMA_A64) {
	pci_addr = slotp->bss_d64_base;
	if (!(flags & PCIBR_VCHAN1))
	    flags |= PCIBR_VCHAN0;
	if ((pci_addr != PCIBR_D64_BASE_UNSET) &&
	    (flags == slotp->bss_d64_flags)) {

	    pci_addr |=  xio_addr
		| ((uint64_t) xio_port << PCI64_ATTR_TARG_SHFT);

#if HWG_PERF_CHECK
	    if (xio_addr != 0x20000000)
#endif
		PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
			    "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
			    "xio_port=0x%x, direct64: pci_addr=0x%x\n",
			    paddr, paddr + req_size - 1, xio_addr, pci_addr));
	    return (pci_addr);
	}
	if (!pcibr_try_set_device(pcibr_soft, pciio_slot, flags, BRIDGE_DEV_D64_BITS)) {
	    pci_addr = pcibr_flags_to_d64(flags, pcibr_soft);
	    slotp->bss_d64_flags = flags;
	    slotp->bss_d64_base = pci_addr;
            pci_addr |= xio_addr
		| ((uint64_t) xio_port << PCI64_ATTR_TARG_SHFT);

	    /* If in PCI mode, make sure we have an RRB (or two).
	     */
	    if (IS_PCI(pcibr_soft) && 
		!(pcibr_soft->bs_rrb_fixed & (1 << pciio_slot))) {
		if (flags & PCIBR_VCHAN1)
		    vchan = VCHAN1;
		have_rrbs = pcibr_soft->bs_rrb_valid[pciio_slot][vchan];
		if (have_rrbs < 2) {
		    if (pci_addr & PCI64_ATTR_PREF)
			min_rrbs = 2;
		    else
			min_rrbs = 1;
		    if (have_rrbs < min_rrbs)
			do_pcibr_rrb_autoalloc(pcibr_soft, pciio_slot, vchan,
					       min_rrbs - have_rrbs);
		}
	    }
#if HWG_PERF_CHECK
	    if (xio_addr != 0x20000000)
#endif
		PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
			    "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
			    "xio_port=0x%x, direct64: pci_addr=0x%x, "
			    "new flags: 0x%x\n", paddr, paddr + req_size - 1,
			    xio_addr, pci_addr, (uint64_t) flags));
	    return (pci_addr);
	}

	PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
		    "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
		    "xio_port=0x%x, Unable to set direct64 Device(x) bits\n",
		    paddr, paddr + req_size - 1, xio_addr));

	/* PIC only supports 64-bit direct mapping in PCI-X mode */
	if (IS_PCIX(pcibr_soft)) {
	    return 0;
	}

	/* our flags conflict with Device(x). try direct32*/
	flags = flags & ~(PCIIO_DMA_A64 | PCIBR_VCHAN0);
    }
    /* Try to satisfy the request with the 32-bit direct
     * map. This can fail if the configuration bits in
     * Device(x) conflict with our flags, or if the
     * target address is outside where DIR_OFF points.
     */
    {
	size_t                  map_size = 1ULL << 31;
	iopaddr_t               xio_base = pcibr_soft->bs_dir_xbase;
	iopaddr_t               offset = xio_addr - xio_base;
	iopaddr_t               endoff = req_size + offset;

	if ((req_size > map_size) ||
	    (xio_addr < xio_base) ||
	    (xio_port != pcibr_soft->bs_dir_xport) ||
	    (endoff > map_size)) {

	    PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
			"pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
			"xio_port=0x%x, xio region outside direct32 target\n",
			paddr, paddr + req_size - 1, xio_addr));
	} else {
	    pci_addr = slotp->bss_d32_base;
	    if ((pci_addr != PCIBR_D32_BASE_UNSET) &&
		(flags == slotp->bss_d32_flags)) {

		pci_addr |= offset;

		PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
                            "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
                            "xio_port=0x%x, direct32: pci_addr=0x%x\n",
                            paddr, paddr + req_size - 1, xio_addr, pci_addr));

		return (pci_addr);
	    }
	    if (!pcibr_try_set_device(pcibr_soft, pciio_slot, flags, BRIDGE_DEV_D32_BITS)) {

		pci_addr = PCI32_DIRECT_BASE;
		slotp->bss_d32_flags = flags;
		slotp->bss_d32_base = pci_addr;
		pci_addr |= offset;

		/* Make sure we have an RRB (or two).
		 */
		if (!(pcibr_soft->bs_rrb_fixed & (1 << pciio_slot))) {
		    have_rrbs = pcibr_soft->bs_rrb_valid[pciio_slot][vchan];
		    if (have_rrbs < 2) {
			if (slotp->bss_device & BRIDGE_DEV_PREF)
			    min_rrbs = 2;
			else
			    min_rrbs = 1;
			if (have_rrbs < min_rrbs)
			    do_pcibr_rrb_autoalloc(pcibr_soft, pciio_slot, 
						   vchan, min_rrbs - have_rrbs);
		    }
		}
#if HWG_PERF_CHECK
		if (xio_addr != 0x20000000)
#endif
                    PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
                            "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
                            "xio_port=0x%x, direct32: pci_addr=0x%x, "
			    "new flags: 0x%x\n", paddr, paddr + req_size - 1,
			    xio_addr, pci_addr, (uint64_t) flags));

		return (pci_addr);
	    }
	    /* our flags conflict with Device(x).
	     */
	    PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
                    "pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
                    "xio_port=0x%x, Unable to set direct32 Device(x) bits\n",
                    paddr, paddr + req_size - 1, xio_port));
	}
    }

    PCIBR_DEBUG((PCIBR_DEBUG_DMADIR, pconn_vhdl,
		"pcibr_dmatrans_addr:  wanted paddr [0x%x..0x%x], "
		"xio_port=0x%x, No acceptable PCI address found\n",
		paddr, paddr + req_size - 1, xio_port));

    return 0;
}

void
pcibr_dmamap_drain(pcibr_dmamap_t map)
{
    xtalk_dmamap_drain(map->bd_xtalk);
}

void
pcibr_dmaaddr_drain(vertex_hdl_t pconn_vhdl,
		    paddr_t paddr,
		    size_t bytes)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;

    xtalk_dmaaddr_drain(xconn_vhdl, paddr, bytes);
}

void
pcibr_dmalist_drain(vertex_hdl_t pconn_vhdl,
		    alenlist_t list)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    vertex_hdl_t            xconn_vhdl = pcibr_soft->bs_conn;

    xtalk_dmalist_drain(xconn_vhdl, list);
}

/*
 * Get the starting PCIbus address out of the given DMA map.
 * This function is supposed to be used by a close friend of PCI bridge
 * since it relies on the fact that the starting address of the map is fixed at
 * the allocation time in the current implementation of PCI bridge.
 */
iopaddr_t
pcibr_dmamap_pciaddr_get(pcibr_dmamap_t pcibr_dmamap)
{
    return (pcibr_dmamap->bd_pci_addr);
}

/* =====================================================================
 *    CONFIGURATION MANAGEMENT
 */
/*ARGSUSED */
void
pcibr_provider_startup(vertex_hdl_t pcibr)
{
}

/*ARGSUSED */
void
pcibr_provider_shutdown(vertex_hdl_t pcibr)
{
}

int
pcibr_reset(vertex_hdl_t conn)
{
#ifdef PIC_LATER
    pciio_info_t            pciio_info = pciio_info_get(conn);
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    bridge_t               *bridge = pcibr_soft->bs_base;
    bridgereg_t             ctlreg;
    unsigned                cfgctl[8];
    unsigned long           s;
    int                     f, nf;
    pcibr_info_h            pcibr_infoh;
    pcibr_info_t            pcibr_info;
    int                     win;
    int                     error = 0;
#endif	/* PIC_LATER */

	BUG();
#ifdef PIC_LATER
    if (pcibr_soft->bs_slot[pciio_slot].has_host) {
	pciio_slot = pcibr_soft->bs_slot[pciio_slot].host_slot;
	pcibr_info = pcibr_soft->bs_slot[pciio_slot].bss_infos[0];
    }

    if ((pciio_slot >= pcibr_soft->bs_first_slot) &&
        (pciio_slot <= pcibr_soft->bs_last_reset)) {
	s = pcibr_lock(pcibr_soft);
	nf = pcibr_soft->bs_slot[pciio_slot].bss_ninfo;
	pcibr_infoh = pcibr_soft->bs_slot[pciio_slot].bss_infos;
	for (f = 0; f < nf; ++f)
	    if (pcibr_infoh[f])
		cfgctl[f] = pcibr_func_config_get(bridge, pciio_slot, f,
							PCI_CFG_COMMAND/4);

        error = iobrick_pci_slot_rst(pcibr_soft->bs_l1sc,
                             pcibr_widget_to_bus(pcibr_soft->bs_vhdl),
                             PCIBR_DEVICE_TO_SLOT(pcibr_soft,pciio_slot),
                             NULL);

	ctlreg = bridge->b_wid_control;
	bridge->b_wid_control = ctlreg & ~BRIDGE_CTRL_RST_PIN(pciio_slot);
        nano_delay(&ts);
	bridge->b_wid_control = ctlreg | BRIDGE_CTRL_RST_PIN(pciio_slot);
        nano_delay(&ts);

	for (f = 0; f < nf; ++f)
	    if ((pcibr_info = pcibr_infoh[f]))
		for (win = 0; win < 6; ++win)
		    if (pcibr_info->f_window[win].w_base != 0)
			pcibr_func_config_set(bridge, pciio_slot, f,
					PCI_CFG_BASE_ADDR(win) / 4, 
					pcibr_info->f_window[win].w_base);
	for (f = 0; f < nf; ++f)
	    if (pcibr_infoh[f])
		pcibr_func_config_set(bridge, pciio_slot, f,
					PCI_CFG_COMMAND / 4,
					cfgctl[f]);
	pcibr_unlock(pcibr_soft, s);

	if (error)
            return(-1);

	return 0;
    }
    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DETACH, conn,
    		"pcibr_reset unimplemented for slot %d\n", conn, pciio_slot));
#endif	/* PIC_LATER */
    return -1;
}

pciio_endian_t
pcibr_endian_set(vertex_hdl_t pconn_vhdl,
		 pciio_endian_t device_end,
		 pciio_endian_t desired_end)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    bridgereg_t             devreg;
    unsigned long           s;

    /*
     * Bridge supports hardware swapping; so we can always
     * arrange for the caller's desired endianness.
     */

    s = pcibr_lock(pcibr_soft);
    devreg = pcibr_soft->bs_slot[pciio_slot].bss_device;
    if (device_end != desired_end)
	devreg |= BRIDGE_DEV_SWAP_BITS;
    else
	devreg &= ~BRIDGE_DEV_SWAP_BITS;

    /* NOTE- if we ever put SWAP bits
     * onto the disabled list, we will
     * have to change the logic here.
     */
    if (pcibr_soft->bs_slot[pciio_slot].bss_device != devreg) {
	bridge_t               *bridge = pcibr_soft->bs_base;

	if ( IS_PIC_SOFT(pcibr_soft) ) {
		bridge->b_device[pciio_slot].reg = devreg;
		pcibr_soft->bs_slot[pciio_slot].bss_device = devreg;
		bridge->b_wid_tflush;		/* wait until Bridge PIO complete */
	}
    }
    pcibr_unlock(pcibr_soft, s);

#ifdef PIC_LATER
    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DEVREG, pconn_vhdl,
    		"pcibr_endian_set: Device(%d): %x\n",
		pciio_slot, devreg, device_bits));
#else
    printk("pcibr_endian_set: Device(%d): %x\n", pciio_slot, devreg);
#endif
    return desired_end;
}

/*
 * Interfaces to allow special (e.g. SGI) drivers to set/clear
 * Bridge-specific device flags.  Many flags are modified through
 * PCI-generic interfaces; we don't allow them to be directly
 * manipulated here.  Only flags that at this point seem pretty
 * Bridge-specific can be set through these special interfaces.
 * We may add more flags as the need arises, or remove flags and
 * create PCI-generic interfaces as the need arises.
 *
 * Returns 0 on failure, 1 on success
 */
int
pcibr_device_flags_set(vertex_hdl_t pconn_vhdl,
		       pcibr_device_flags_t flags)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pciio_slot_t            pciio_slot = PCIBR_INFO_SLOT_GET_INT(pciio_info);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
    bridgereg_t             set = 0;
    bridgereg_t             clr = 0;

    ASSERT((flags & PCIBR_DEVICE_FLAGS) == flags);

    if (flags & PCIBR_WRITE_GATHER)
	set |= BRIDGE_DEV_PMU_WRGA_EN;
    if (flags & PCIBR_NOWRITE_GATHER)
	clr |= BRIDGE_DEV_PMU_WRGA_EN;

    if (flags & PCIBR_WRITE_GATHER)
	set |= BRIDGE_DEV_DIR_WRGA_EN;
    if (flags & PCIBR_NOWRITE_GATHER)
	clr |= BRIDGE_DEV_DIR_WRGA_EN;

    if (flags & PCIBR_PREFETCH)
	set |= BRIDGE_DEV_PREF;
    if (flags & PCIBR_NOPREFETCH)
	clr |= BRIDGE_DEV_PREF;

    if (flags & PCIBR_PRECISE)
	set |= BRIDGE_DEV_PRECISE;
    if (flags & PCIBR_NOPRECISE)
	clr |= BRIDGE_DEV_PRECISE;

    if (flags & PCIBR_BARRIER)
	set |= BRIDGE_DEV_BARRIER;
    if (flags & PCIBR_NOBARRIER)
	clr |= BRIDGE_DEV_BARRIER;

    if (flags & PCIBR_64BIT)
	set |= BRIDGE_DEV_DEV_SIZE;
    if (flags & PCIBR_NO64BIT)
	clr |= BRIDGE_DEV_DEV_SIZE;

    if (set || clr) {
	bridgereg_t             devreg;
	unsigned long           s;

	s = pcibr_lock(pcibr_soft);
	devreg = pcibr_soft->bs_slot[pciio_slot].bss_device;
	devreg = (devreg & ~clr) | set;
	if (pcibr_soft->bs_slot[pciio_slot].bss_device != devreg) {
	    bridge_t               *bridge = pcibr_soft->bs_base;

	    if ( IS_PIC_SOFT(pcibr_soft) ) {
		bridge->b_device[pciio_slot].reg = devreg;
		pcibr_soft->bs_slot[pciio_slot].bss_device = devreg;
		bridge->b_wid_tflush;	/* wait until Bridge PIO complete */
	    }
	}
	pcibr_unlock(pcibr_soft, s);
#ifdef PIC_LATER
	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_DEVREG, pconn_vhdl,
		    "pcibr_device_flags_set: Device(%d): %x\n",
		    pciio_slot, devreg, device_bits));
#else
	printk("pcibr_device_flags_set: Device(%d): %x\n", pciio_slot, devreg);
#endif
    }
    return (1);
}

/*
 * PIC has 16 RBARs per bus; meaning it can have a total of 16 outstanding 
 * split transactions.  If the functions on the bus have requested a total 
 * of 16 or less, then we can give them what they requested (ie. 100%). 
 * Otherwise we have make sure each function can get at least one buffer
 * and then divide the rest of the buffers up among the functions as ``A 
 * PERCENTAGE OF WHAT THEY REQUESTED'' (i.e. 0% - 100% of a function's
 * pcix_type0_status.max_out_split).  This percentage does not include the
 * one RBAR that all functions get by default.
 */
int
pcibr_pcix_rbars_calc(pcibr_soft_t pcibr_soft)
{
    /* 'percent_allowed' is the percentage of requested RBARs that functions
     * are allowed, ***less the 1 RBAR that all functions get by default***
     */
    int percent_allowed; 

    if (pcibr_soft->bs_pcix_num_funcs) {
	if (pcibr_soft->bs_pcix_num_funcs > NUM_RBAR) {
	    printk(KERN_WARNING
		"%lx: Must oversubscribe Read Buffer Attribute Registers"
		"(RBAR).  Bus has %d RBARs but %d funcs need them.\n",
		(unsigned long)pcibr_soft->bs_vhdl, NUM_RBAR, pcibr_soft->bs_pcix_num_funcs);
	    percent_allowed = 0;
	} else {
	    percent_allowed = (((NUM_RBAR-pcibr_soft->bs_pcix_num_funcs)*100) /
		               pcibr_soft->bs_pcix_split_tot);

	    /* +1 to percentage to solve rounding errors that occur because
	     * we're not doing fractional math. (ie. ((3 * 66%) / 100) = 1)
	     * but should be "2" if doing true fractional math.  NOTE: Since
	     * the greatest number of outstanding transactions a function 
	     * can request is 32, this "+1" will always work (i.e. we won't
	     * accidentally oversubscribe the RBARs because of this rounding
	     * of the percentage).
	     */
	    percent_allowed=(percent_allowed > 100) ? 100 : percent_allowed+1;
	}
    } else {
	return(ENODEV);
    }

    return(percent_allowed);
}

pciio_provider_t        pcibr_provider =
{
    (pciio_piomap_alloc_f *) pcibr_piomap_alloc,
    (pciio_piomap_free_f *) pcibr_piomap_free,
    (pciio_piomap_addr_f *) pcibr_piomap_addr,
    (pciio_piomap_done_f *) pcibr_piomap_done,
    (pciio_piotrans_addr_f *) pcibr_piotrans_addr,
    (pciio_piospace_alloc_f *) pcibr_piospace_alloc,
    (pciio_piospace_free_f *) pcibr_piospace_free,

    (pciio_dmamap_alloc_f *) pcibr_dmamap_alloc,
    (pciio_dmamap_free_f *) pcibr_dmamap_free,
    (pciio_dmamap_addr_f *) pcibr_dmamap_addr,
    (pciio_dmamap_done_f *) pcibr_dmamap_done,
    (pciio_dmatrans_addr_f *) pcibr_dmatrans_addr,
    (pciio_dmamap_drain_f *) pcibr_dmamap_drain,
    (pciio_dmaaddr_drain_f *) pcibr_dmaaddr_drain,
    (pciio_dmalist_drain_f *) pcibr_dmalist_drain,

    (pciio_intr_alloc_f *) pcibr_intr_alloc,
    (pciio_intr_free_f *) pcibr_intr_free,
    (pciio_intr_connect_f *) pcibr_intr_connect,
    (pciio_intr_disconnect_f *) pcibr_intr_disconnect,
    (pciio_intr_cpu_get_f *) pcibr_intr_cpu_get,

    (pciio_provider_startup_f *) pcibr_provider_startup,
    (pciio_provider_shutdown_f *) pcibr_provider_shutdown,
    (pciio_reset_f *) pcibr_reset,
    (pciio_endian_set_f *) pcibr_endian_set,
    (pciio_config_get_f *) pcibr_config_get,
    (pciio_config_set_f *) pcibr_config_set,
    (pciio_error_devenable_f *) 0,
    (pciio_error_extract_f *) 0,
    (pciio_driver_reg_callback_f *) 0,
    (pciio_driver_unreg_callback_f *) 0,
    (pciio_device_unregister_f 	*) 0,
    (pciio_dma_enabled_f		*) pcibr_dma_enabled,
};

int
pcibr_dma_enabled(vertex_hdl_t pconn_vhdl)
{
    pciio_info_t            pciio_info = pciio_info_get(pconn_vhdl);
    pcibr_soft_t            pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
	

    return xtalk_dma_enabled(pcibr_soft->bs_conn);
}


/*
 * pcibr_debug() is used to print pcibr debug messages to the console.  A
 * user enables tracing by setting the following global variables:
 *
 *    pcibr_debug_mask 	   -Bitmask of what to trace. see pcibr_private.h
 *    pcibr_debug_module   -Module to trace.  'all' means trace all modules
 *    pcibr_debug_widget   -Widget to trace. '-1' means trace all widgets
 *    pcibr_debug_slot	   -Slot to trace.  '-1' means trace all slots
 *
 * 'type' is the type of debugging that the current PCIBR_DEBUG macro is
 * tracing.  'vhdl' (which can be NULL) is the vhdl associated with the
 * debug statement.  If there is a 'vhdl' associated with this debug
 * statement, it is parsed to obtain the module, widget, and slot.  If the
 * globals above match the PCIBR_DEBUG params, then the debug info in the
 * parameter 'format' is sent to the console.
 */
void
pcibr_debug(uint32_t type, vertex_hdl_t vhdl, char *format, ...)
{
    char hwpath[MAXDEVNAME] = "\0";
    char copy_of_hwpath[MAXDEVNAME];
    char *module = "all";
    short widget = -1;
    short slot = -1;
    va_list ap;

    if (pcibr_debug_mask & type) {
        if (vhdl) {
            if (!hwgraph_vertex_name_get(vhdl, hwpath, MAXDEVNAME)) {
                char *cp;

                if (strcmp(module, pcibr_debug_module)) {
		    /* use a copy */
                    (void)strcpy(copy_of_hwpath, hwpath);
                    cp = strstr(copy_of_hwpath, "/module/");
                    if (cp) {
                        cp += strlen("/module");
                        module = strsep(&cp, "/");
                    }
                }
                if (pcibr_debug_widget != -1) {
                    cp = strstr(hwpath, "/xtalk/");
                    if (cp) {
                        cp += strlen("/xtalk/");
                        widget = simple_strtoul(cp, NULL, 0);
                    }
                }
                if (pcibr_debug_slot != -1) {
                    cp = strstr(hwpath, "/pci/");
                    if (cp) {
                        cp += strlen("/pci/");
                        slot = simple_strtoul(cp, NULL, 0);
                    }
                }
            }
        }
        if ((vhdl == NULL) ||
            (!strcmp(module, pcibr_debug_module) &&
             (widget == pcibr_debug_widget) &&
             (slot == pcibr_debug_slot))) {
#ifdef LATER
            printk("PCIBR_DEBUG<%d>\t: %s :", cpuid(), hwpath);
#else
            printk("PCIBR_DEBUG\t: %s :", hwpath);
#endif
	    /*
	     * Kernel printk translates to this 3 line sequence.
	     * Since we have a variable length argument list, we
	     * need to call printk this way rather than directly
	     */
	    {
		char buffer[500];

		va_start(ap, format);
		vsnprintf(buffer, 500, format, ap);
		va_end(ap);
		buffer[499] = (char)0;	/* just to be safe */
		printk("%s", buffer);
	    }
        }
    }
}

int
isIO9(nasid_t nasid) {
	lboard_t *brd = (lboard_t *)KL_CONFIG_INFO(nasid);

	while (brd) {
		if (brd->brd_flags & LOCAL_MASTER_IO6) {
			return 1;
		}
		brd = KLCF_NEXT(brd);
	}
	/* if it's dual ported, check the peer also */
	nasid = NODEPDA(NASID_TO_COMPACT_NODEID(nasid))->xbow_peer;
	if (nasid < 0) return 0;
	brd = (lboard_t *)KL_CONFIG_INFO(nasid);
	while (brd) {
		if (brd->brd_flags & LOCAL_MASTER_IO6) {
			return 1;
		}
		brd = KLCF_NEXT(brd);
	}
	return 0;
}
