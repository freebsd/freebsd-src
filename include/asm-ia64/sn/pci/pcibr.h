/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_PCIBR_H
#define _ASM_SN_PCI_PCIBR_H

#if defined(__KERNEL__)

#include <linux/config.h>
#include <asm/sn/dmamap.h>
#include <asm/sn/driver.h>
#include <asm/sn/pio.h>

#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/bridge.h>

/* =====================================================================
 *    symbolic constants used by pcibr's xtalk bus provider
 */

#define PCIBR_PIOMAP_BUSY		0x80000000

#define PCIBR_DMAMAP_BUSY		0x80000000
#define	PCIBR_DMAMAP_SSRAM		0x40000000

#define PCIBR_INTR_BLOCKED		0x40000000
#define PCIBR_INTR_BUSY			0x80000000

#ifndef __ASSEMBLY__

/* =====================================================================
 *    opaque types used by pcibr's xtalk bus provider
 */

typedef struct pcibr_piomap_s *pcibr_piomap_t;
typedef struct pcibr_dmamap_s *pcibr_dmamap_t;
typedef struct pcibr_intr_s *pcibr_intr_t;

/* =====================================================================
 *    primary entry points: Bridge (pcibr) device driver
 *
 *	These functions are normal device driver entry points
 *	and are called along with the similar entry points from
 *	other device drivers. They are included here as documentation
 *	of their existence and purpose.
 *
 *	pcibr_init() is called to inform us that there is a pcibr driver
 *	configured into the kernel; it is responsible for registering
 *	as a crosstalk widget and providing a routine to be called
 *	when a widget with the proper part number is observed.
 *
 *	pcibr_attach() is called for each vertex in the hardware graph
 *	corresponding to a crosstalk widget with the manufacturer
 *	code and part number registered by pcibr_init().
 */

extern int		pcibr_attach(vertex_hdl_t);

/* =====================================================================
 *    bus provider function table
 *
 *	Normally, this table is only handed off explicitly
 *	during provider initialization, and the PCI generic
 *	layer will stash a pointer to it in the vertex; however,
 *	exporting it explicitly enables a performance hack in
 *	the generic PCI provider where if we know at compile
 *	time that the only possible PCI provider is a
 *	pcibr, we can go directly to this ops table.
 */

extern pciio_provider_t pcibr_provider;
extern pciio_provider_t pci_pic_provider;

/* =====================================================================
 *    secondary entry points: pcibr PCI bus provider
 *
 *	These functions are normally exported explicitly by
 *	a direct call from the pcibr initialization routine
 *	into the generic crosstalk provider; they are included
 *	here to enable a more aggressive performance hack in
 *	the generic crosstalk layer, where if we know that the
 *	only possible crosstalk provider is pcibr, and we can
 *	guarantee that all entry points are properly named, and
 *	we can deal with the implicit casting properly, then
 *	we can turn many of the generic provider routines into
 *	plain brances, or even eliminate them (given sufficient
 *	smarts on the part of the compilation system).
 */

extern pcibr_piomap_t	pcibr_piomap_alloc(vertex_hdl_t dev,
					   device_desc_t dev_desc,
					   pciio_space_t space,
					   iopaddr_t pci_addr,
					   size_t byte_count,
					   size_t byte_count_max,
					   unsigned flags);

extern void		pcibr_piomap_free(pcibr_piomap_t piomap);

extern caddr_t		pcibr_piomap_addr(pcibr_piomap_t piomap,
					  iopaddr_t xtalk_addr,
					  size_t byte_count);

extern void		pcibr_piomap_done(pcibr_piomap_t piomap);

extern caddr_t		pcibr_piotrans_addr(vertex_hdl_t dev,
					    device_desc_t dev_desc,
					    pciio_space_t space,
					    iopaddr_t pci_addr,
					    size_t byte_count,
					    unsigned flags);

extern iopaddr_t	pcibr_piospace_alloc(vertex_hdl_t dev,
					     device_desc_t dev_desc,
					     pciio_space_t space,
					     size_t byte_count,
					     size_t alignment);
extern void		pcibr_piospace_free(vertex_hdl_t dev,
					    pciio_space_t space,
					    iopaddr_t pciaddr,
					    size_t byte_count);

extern pcibr_dmamap_t	pcibr_dmamap_alloc(vertex_hdl_t dev,
					   device_desc_t dev_desc,
					   size_t byte_count_max,
					   unsigned flags);

extern void		pcibr_dmamap_free(pcibr_dmamap_t dmamap);

extern iopaddr_t	pcibr_dmamap_addr(pcibr_dmamap_t dmamap,
					  paddr_t paddr,
					  size_t byte_count);

extern alenlist_t	pcibr_dmamap_list(pcibr_dmamap_t dmamap,
					  alenlist_t palenlist,
					  unsigned flags);

extern void		pcibr_dmamap_done(pcibr_dmamap_t dmamap);

/*
 * pcibr_get_dmatrans_node() will return the compact node id to which  
 * all 32-bit Direct Mapping memory accesses will be directed.
 * (This node id can be different for each PCI bus.) 
 */

extern cnodeid_t	pcibr_get_dmatrans_node(vertex_hdl_t pconn_vhdl);

extern iopaddr_t	pcibr_dmatrans_addr(vertex_hdl_t dev,
					    device_desc_t dev_desc,
					    paddr_t paddr,
					    size_t byte_count,
					    unsigned flags);

extern alenlist_t	pcibr_dmatrans_list(vertex_hdl_t dev,
					    device_desc_t dev_desc,
					    alenlist_t palenlist,
					    unsigned flags);

extern void		pcibr_dmamap_drain(pcibr_dmamap_t map);

extern void		pcibr_dmaaddr_drain(vertex_hdl_t vhdl,
					    paddr_t addr,
					    size_t bytes);

extern void		pcibr_dmalist_drain(vertex_hdl_t vhdl,
					    alenlist_t list);

typedef unsigned	pcibr_intr_ibit_f(pciio_info_t info,
					  pciio_intr_line_t lines);

extern void		pcibr_intr_ibit_set(vertex_hdl_t, pcibr_intr_ibit_f *);

extern pcibr_intr_t	pcibr_intr_alloc(vertex_hdl_t dev,
					 device_desc_t dev_desc,
					 pciio_intr_line_t lines,
					 vertex_hdl_t owner_dev);

extern void		pcibr_intr_free(pcibr_intr_t intr);

extern int		pcibr_intr_connect(pcibr_intr_t intr, intr_func_t, intr_arg_t);

extern void		pcibr_intr_disconnect(pcibr_intr_t intr);

extern vertex_hdl_t	pcibr_intr_cpu_get(pcibr_intr_t intr);

extern void		pcibr_provider_startup(vertex_hdl_t pcibr);

extern void		pcibr_provider_shutdown(vertex_hdl_t pcibr);

extern int		pcibr_reset(vertex_hdl_t dev);

extern pciio_endian_t	pcibr_endian_set(vertex_hdl_t dev,
					 pciio_endian_t device_end,
					 pciio_endian_t desired_end);

extern uint64_t		pcibr_config_get(vertex_hdl_t conn,
					 unsigned reg,
					 unsigned size);

extern void		pcibr_config_set(vertex_hdl_t conn,
					 unsigned reg,
					 unsigned size,
					 uint64_t value);

extern int		pcibr_error_devenable(vertex_hdl_t pconn_vhdl,
					      int error_code);

extern int		pcibr_wrb_flush(vertex_hdl_t pconn_vhdl);
extern int		pcibr_rrb_check(vertex_hdl_t pconn_vhdl,
					int *count_vchan0,
					int *count_vchan1,
					int *count_reserved,
					int *count_pool);

extern int		pcibr_alloc_all_rrbs(vertex_hdl_t vhdl, int even_odd,
					     int dev_1_rrbs, int virt1,
					     int dev_2_rrbs, int virt2,
					     int dev_3_rrbs, int virt3,
					     int dev_4_rrbs, int virt4);

typedef void
rrb_alloc_funct_f	(vertex_hdl_t xconn_vhdl,
			 int *vendor_list);

typedef rrb_alloc_funct_f      *rrb_alloc_funct_t;

void			pcibr_set_rrb_callback(vertex_hdl_t xconn_vhdl,
					       rrb_alloc_funct_f *func);

extern int		pcibr_device_unregister(vertex_hdl_t);
extern int		pcibr_dma_enabled(vertex_hdl_t);
/*
 * Bridge-specific flags that can be set via pcibr_device_flags_set
 * and cleared via pcibr_device_flags_clear.  Other flags are
 * more generic and are maniuplated through PCI-generic interfaces.
 *
 * Note that all PCI implementation-specific flags (Bridge flags, in
 * this case) are in bits 15-31.  The lower 15 bits are reserved
 * for PCI-generic flags.
 *
 * Some of these flags have been "promoted" to the
 * generic layer, so they can be used without having
 * to "know" that the PCI bus is hosted by a Bridge.
 *
 * PCIBR_NO_ATE_ROUNDUP: Request that no rounding up be done when 
 * allocating ATE's. ATE count computation will assume that the
 * address to be mapped will start on a page boundary.
 */
#define PCIBR_NO_ATE_ROUNDUP    0x00008000
#define PCIBR_WRITE_GATHER	0x00010000	/* please use PCIIO version */
#define PCIBR_NOWRITE_GATHER	0x00020000	/* please use PCIIO version */
#define PCIBR_PREFETCH		0x00040000	/* please use PCIIO version */
#define PCIBR_NOPREFETCH	0x00080000	/* please use PCIIO version */
#define PCIBR_PRECISE		0x00100000
#define PCIBR_NOPRECISE		0x00200000
#define PCIBR_BARRIER		0x00400000
#define PCIBR_NOBARRIER		0x00800000
#define PCIBR_VCHAN0		0x01000000
#define PCIBR_VCHAN1		0x02000000
#define PCIBR_64BIT		0x04000000
#define PCIBR_NO64BIT		0x08000000
#define PCIBR_SWAP		0x10000000
#define PCIBR_NOSWAP		0x20000000

#define	PCIBR_EXTERNAL_ATES	0x40000000	/* uses external ATEs */
#define	PCIBR_ACTIVE		0x80000000	/* need a "done" */

/* Flags that have meaning to pcibr_device_flags_{set,clear} */
#define PCIBR_DEVICE_FLAGS (	\
	PCIBR_WRITE_GATHER	|\
	PCIBR_NOWRITE_GATHER	|\
	PCIBR_PREFETCH		|\
	PCIBR_NOPREFETCH	|\
	PCIBR_PRECISE		|\
	PCIBR_NOPRECISE		|\
	PCIBR_BARRIER		|\
	PCIBR_NOBARRIER		\
)

/* Flags that have meaning to *_dmamap_alloc, *_dmatrans_{addr,list} */
#define PCIBR_DMA_FLAGS (	\
	PCIBR_PREFETCH		|\
	PCIBR_NOPREFETCH	|\
	PCIBR_PRECISE		|\
	PCIBR_NOPRECISE		|\
	PCIBR_BARRIER		|\
	PCIBR_NOBARRIER		|\
	PCIBR_VCHAN0		|\
	PCIBR_VCHAN1		\
)

typedef int		pcibr_device_flags_t;

/*
 * Set bits in the Bridge Device(x) register for this device.
 * "flags" are defined above. NOTE: this includes turning
 * things *OFF* as well as turning them *ON* ...
 */
extern int		pcibr_device_flags_set(vertex_hdl_t dev,
					     pcibr_device_flags_t flags);

/*
 * Allocate Read Response Buffers for use by the specified device.
 * count_vchan0 is the total number of buffers desired for the
 * "normal" channel.  count_vchan1 is the total number of buffers
 * desired for the "virtual" channel.  Returns 0 on success, or
 * <0 on failure, which occurs when we're unable to allocate any
 * buffers to a channel that desires at least one buffer.
 */
extern int		pcibr_rrb_alloc(vertex_hdl_t pconn_vhdl,
					int *count_vchan0,
					int *count_vchan1);

/*
 * Get the starting PCIbus address out of the given DMA map.
 * This function is supposed to be used by a close friend of PCI bridge
 * since it relies on the fact that the starting address of the map is fixed at
 * the allocation time in the current implementation of PCI bridge.
 */
extern iopaddr_t	pcibr_dmamap_pciaddr_get(pcibr_dmamap_t);

extern xwidget_intr_preset_f pcibr_xintr_preset;

extern void		pcibr_hints_fix_rrbs(vertex_hdl_t);
extern void		pcibr_hints_dualslot(vertex_hdl_t, pciio_slot_t, pciio_slot_t);
extern void		pcibr_hints_subdevs(vertex_hdl_t, pciio_slot_t, ulong);
extern void		pcibr_hints_handsoff(vertex_hdl_t);

typedef unsigned	pcibr_intr_bits_f(pciio_info_t, pciio_intr_line_t, int);
extern void		pcibr_hints_intr_bits(vertex_hdl_t, pcibr_intr_bits_f *);

extern int		pcibr_asic_rev(vertex_hdl_t);

#endif 	/* __ASSEMBLY__ */
#endif	/* #if defined(__KERNEL__) */
/* 
 * Some useful ioctls into the pcibr driver
 */
#define PCIBR			'p'
#define _PCIBR(x)		((PCIBR << 8) | (x))

#define PCIBR_SLOT_STARTUP	_PCIBR(1)
#define PCIBR_SLOT_SHUTDOWN     _PCIBR(2)
#define PCIBR_SLOT_QUERY	_PCIBR(3)

/*
 * Bit defintions for variable slot_status in struct
 * pcibr_soft_slot_s.  They are here so that both
 * the pcibr driver and the pciconfig command can
 * reference them.
 */
#define SLOT_STARTUP_CMPLT      0x01
#define SLOT_STARTUP_INCMPLT    0x02
#define SLOT_SHUTDOWN_CMPLT     0x04
#define SLOT_SHUTDOWN_INCMPLT   0x08
#define SLOT_POWER_UP           0x10
#define SLOT_POWER_DOWN         0x20
#define SLOT_IS_SYS_CRITICAL    0x40

#define SLOT_STATUS_MASK        (SLOT_STARTUP_CMPLT | SLOT_STARTUP_INCMPLT | \
                                 SLOT_SHUTDOWN_CMPLT | SLOT_SHUTDOWN_INCMPLT)
#define SLOT_POWER_MASK         (SLOT_POWER_UP | SLOT_POWER_DOWN)

/*
 * Bit definitions for variable resp_f_staus.
 * They are here so that both the pcibr driver
 * and the pciconfig command can reference them.
 */
#define FUNC_IS_VALID           0x01
#define FUNC_IS_SYS_CRITICAL    0x02

/*
 * Structures for requesting PCI bridge information and receiving a response
 */
typedef struct pcibr_slot_req_s *pcibr_slot_req_t;
typedef struct pcibr_slot_up_resp_s *pcibr_slot_up_resp_t;
typedef struct pcibr_slot_down_resp_s *pcibr_slot_down_resp_t;
typedef struct pcibr_slot_info_resp_s *pcibr_slot_info_resp_t;
typedef struct pcibr_slot_func_info_resp_s *pcibr_slot_func_info_resp_t;

#define L1_QSIZE                128      /* our L1 message buffer size */
struct pcibr_slot_req_s {
    int                      req_slot;
    union {
        pcibr_slot_up_resp_t     up;
        pcibr_slot_down_resp_t   down;
        pcibr_slot_info_resp_t   query;
        void                    *any;
    }                       req_respp;
    int                     req_size;
};

struct pcibr_slot_up_resp_s {
    int                     resp_sub_errno;
    char                    resp_l1_msg[L1_QSIZE + 1];
};

struct pcibr_slot_down_resp_s {
    int                     resp_sub_errno;
    char                    resp_l1_msg[L1_QSIZE + 1];
};

struct pcibr_slot_info_resp_s {
    short		    resp_bs_bridge_type;
    short		    resp_bs_bridge_mode;
    int                     resp_has_host;
    char                    resp_host_slot;
    vertex_hdl_t            resp_slot_conn;
    char                    resp_slot_conn_name[MAXDEVNAME];
    int                     resp_slot_status;
    int                     resp_l1_bus_num;
    int                     resp_bss_ninfo;
    char                    resp_bss_devio_bssd_space[16];
    iopaddr_t               resp_bss_devio_bssd_base; 
    bridgereg_t             resp_bss_device;
    int                     resp_bss_pmu_uctr;
    int                     resp_bss_d32_uctr;
    int                     resp_bss_d64_uctr;
    iopaddr_t               resp_bss_d64_base;
    unsigned                resp_bss_d64_flags;
    iopaddr_t               resp_bss_d32_base;
    unsigned                resp_bss_d32_flags;
    atomic_t                resp_bss_ext_ates_active;
    volatile unsigned      *resp_bss_cmd_pointer;
    unsigned                resp_bss_cmd_shadow;
    int                     resp_bs_rrb_valid;
    int                     resp_bs_rrb_valid_v1;
    int                     resp_bs_rrb_valid_v2;
    int                     resp_bs_rrb_valid_v3;
    int                     resp_bs_rrb_res;
    bridgereg_t             resp_b_resp;
    bridgereg_t             resp_b_int_device;
    bridgereg_t             resp_b_int_enable;
    bridgereg_t             resp_b_int_host;
    picreg_t		    resp_p_int_enable;
    picreg_t		    resp_p_int_host;
    struct pcibr_slot_func_info_resp_s {
        int                     resp_f_status;
        char                    resp_f_slot_name[MAXDEVNAME];
        char                    resp_f_bus;
        char                    resp_f_slot;
        char                    resp_f_func;
        char                    resp_f_master_name[MAXDEVNAME];
        void                   *resp_f_pops;
        error_handler_f        *resp_f_efunc;
        error_handler_arg_t     resp_f_einfo;
        int                     resp_f_vendor;
        int                     resp_f_device;

        struct {
            char                    resp_w_space[16];
            iopaddr_t               resp_w_base;
            size_t                  resp_w_size;
        } resp_f_window[6];

        unsigned                resp_f_rbase;
        unsigned                resp_f_rsize;
        int                     resp_f_ibit[4];
        int                     resp_f_att_det_error;

    } resp_func[8];
};


/*
 * PCI specific errors, interpreted by pciconfig command
 */

/* EPERM                          1    */
#define PCI_SLOT_ALREADY_UP       2     /* slot already up */
#define PCI_SLOT_ALREADY_DOWN     3     /* slot already down */
#define PCI_IS_SYS_CRITICAL       4     /* slot is system critical */
/* EIO                            5    */
/* ENXIO                          6    */
#define PCI_L1_ERR                7     /* L1 console command error */
#define PCI_NOT_A_BRIDGE          8     /* device is not a bridge */
#define PCI_SLOT_IN_SHOEHORN      9     /* slot is in a shorhorn */
#define PCI_NOT_A_SLOT           10     /* slot is invalid */
#define PCI_RESP_AREA_TOO_SMALL  11     /* slot is invalid */
/* ENOMEM                        12    */
#define PCI_NO_DRIVER            13     /* no driver for device */
/* EFAULT                        14    */
#define PCI_EMPTY_33MHZ          15     /* empty 33 MHz bus */
/* EBUSY                         16    */
#define PCI_SLOT_RESET_ERR       17     /* slot reset error */
#define PCI_SLOT_INFO_INIT_ERR   18     /* slot info init error */
/* ENODEV                        19    */
#define PCI_SLOT_ADDR_INIT_ERR   20     /* slot addr space init error */
#define PCI_SLOT_DEV_INIT_ERR    21     /* slot device init error */
/* EINVAL                        22    */
#define PCI_SLOT_GUEST_INIT_ERR  23     /* slot guest info init error */
#define PCI_SLOT_RRB_ALLOC_ERR   24     /* slot initial rrb alloc error */
#define PCI_SLOT_DRV_ATTACH_ERR  25     /* driver attach error */
#define PCI_SLOT_DRV_DETACH_ERR  26     /* driver detach error */
/* EFBIG                         27    */
#define PCI_MULTI_FUNC_ERR       28     /* multi-function card error */
#define PCI_SLOT_RBAR_ALLOC_ERR  29     /* slot PCI-X RBAR alloc error */
/* ERANGE                        34    */
/* EUNATCH                       42    */

#endif				/* _ASM_SN_PCI_PCIBR_H */
