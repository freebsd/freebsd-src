/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_SN_PCI_BRIDGE_H
#define _ASM_SN_PCI_BRIDGE_H


/*
 * bridge.h - header file for bridge chip and bridge portion of xbridge chip
 *
 * Also including offsets for unique PIC registers.
 * The PIC asic is a follow-on to Xbridge and most of its registers are
 * identical to those of Xbridge.  PIC is different than Xbridge in that
 * it will accept 64 bit register access and that, in some cases, data
 * is kept in bits 63:32.   PIC registers that are identical to Xbridge
 * may be accessed identically to the Xbridge registers, allowing for lots
 * of code reuse.  Here are the access rules as described in the PIC
 * manual:
 * 
 * 	o Read a word on a DW boundary returns D31:00 of reg.
 * 	o Read a DW on a DW boundary returns D63:00 of reg.
 * 	o Write a word on a DW boundary loads D31:00 of reg.
 * 	o Write a DW on a DW boundary loads D63:00 of reg.
 * 	o No support for word boundary access that is not double word
 *           aligned.
 * 
 * So we can reuse a lot of bridge_s for PIC.  In bridge_s are included
 * #define tags and unions for 64 bit access to PIC registers.
 * For a detailed PIC register layout see pic.h.
 */

#ifdef __KERNEL__
#include <linux/config.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/pci/pic.h>
#else
#include <linux/config.h>
#include <xtalk/xwidget.h>
#include <pci/pic.h>
#endif

/* I/O page size */

#if PAGE_SIZE == 4096
#define IOPFNSHIFT		12	/* 4K per mapped page */
#else
#define IOPFNSHIFT		14	/* 16K per mapped page */
#endif				/* _PAGESZ */

#define IOPGSIZE		(1 << IOPFNSHIFT)
#define IOPG(x)			((x) >> IOPFNSHIFT)
#define IOPGOFF(x)		((x) & (IOPGSIZE-1))

/* Bridge RAM sizes */

#define BRIDGE_INTERNAL_ATES	128
#define XBRIDGE_INTERNAL_ATES	1024

#define BRIDGE_ATE_RAM_SIZE     (BRIDGE_INTERNAL_ATES<<3)	/* 1kB ATE */
#define XBRIDGE_ATE_RAM_SIZE    (XBRIDGE_INTERNAL_ATES<<3)	/* 8kB ATE */

#define PIC_WR_REQ_BUFSIZE      256

#define BRIDGE_CONFIG_BASE	0x20000		/* start of bridge's */
						/* map to each device's */
						/* config space */
#define BRIDGE_CONFIG1_BASE	0x28000		/* type 1 device config space */
#define BRIDGE_CONFIG_END	0x30000
#define BRIDGE_CONFIG_SLOT_SIZE 0x1000		/* each map == 4k */

#define BRIDGE_SSRAM_512K	0x00080000	/* 512kB */
#define BRIDGE_SSRAM_128K	0x00020000	/* 128kB */
#define BRIDGE_SSRAM_64K	0x00010000	/* 64kB */
#define BRIDGE_SSRAM_0K		0x00000000	/* 0kB */

/* ========================================================================
 *    Bridge address map
 */

#ifndef __ASSEMBLY__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All accesses to bridge hardware registers must be done
 * using 32-bit loads and stores.
 */
typedef uint32_t	bridgereg_t;

typedef uint64_t	bridge_ate_t;

/* pointers to bridge ATEs
 * are always "pointer to volatile"
 */
typedef volatile bridge_ate_t  *bridge_ate_p;

/*
 * It is generally preferred that hardware registers on the bridge
 * are located from C code via this structure.
 *
 * Generated from Bridge spec dated 04oct95
 */


/*
 * pic_widget_cfg_s is a local definition of widget_cfg_t but with
 * a union of 64bit & 32bit registers, since PIC has 64bit widget
 * registers but BRIDGE and XBRIDGE have 32bit.	 PIC registers that
 * have valid bits (ie. not just reserved) in the upper 32bits are
 * defined as a union so we can access them as 64bit for PIC and
 * as 32bit for BRIDGE and XBRIDGE.
 */ 
typedef volatile struct pic_widget_cfg_s {
    bridgereg_t		    _b_wid_id;		    /* 0x000004 */
    bridgereg_t		    _pad_000000;

    union {
	picreg_t	    _p_wid_stat;	    /* 0x000008 */
	struct {
	    bridgereg_t	    _b_wid_stat;	    /* 0x00000C */
	    bridgereg_t	    _b_pad_000008;
	} _b;
    } u_wid_stat;
    #define __p_wid_stat_64 u_wid_stat._p_wid_stat
    #define __b_wid_stat u_wid_stat._b._b_wid_stat

    bridgereg_t		    _b_wid_err_upper;	    /* 0x000014 */
    bridgereg_t		    _pad_000010;

    union {
	picreg_t	    _p_wid_err_lower;	    /* 0x000018 */
	struct {
	    bridgereg_t	    _b_wid_err_lower;	    /* 0x00001C */
	    bridgereg_t	    _b_pad_000018;
	} _b;
    } u_wid_err_lower;
    #define __p_wid_err_64 u_wid_err_lower._p_wid_err_lower
    #define __b_wid_err_lower u_wid_err_lower._b._b_wid_err_lower

    union {
	picreg_t	    _p_wid_control;	    /* 0x000020 */
	struct {
	    bridgereg_t	    _b_wid_control;	    /* 0x000024 */
	    bridgereg_t	    _b_pad_000020;
	} _b;
    } u_wid_control;
    #define __p_wid_control_64 u_wid_control._p_wid_control
    #define __b_wid_control u_wid_control._b._b_wid_control

    bridgereg_t		    _b_wid_req_timeout;	    /* 0x00002C */
    bridgereg_t		    _pad_000028;

    bridgereg_t		    _b_wid_int_upper;	    /* 0x000034 */
    bridgereg_t		    _pad_000030;

    union {
	picreg_t	    _p_wid_int_lower;	    /* 0x000038 */
	struct {
	    bridgereg_t	    _b_wid_int_lower;	    /* 0x00003C */
	    bridgereg_t	    _b_pad_000038;
	} _b;
    } u_wid_int_lower;
    #define __p_wid_int_64 u_wid_int_lower._p_wid_int_lower
    #define __b_wid_int_lower u_wid_int_lower._b._b_wid_int_lower

    bridgereg_t		    _b_wid_err_cmdword;	    /* 0x000044 */
    bridgereg_t		    _pad_000040;

    bridgereg_t		    _b_wid_llp;		    /* 0x00004C */
    bridgereg_t		    _pad_000048;

    bridgereg_t		    _b_wid_tflush;	    /* 0x000054 */
    bridgereg_t		    _pad_000050;
} pic_widget_cfg_t;

/*
 * BRIDGE, XBRIDGE, PIC register definitions.  NOTE: Prior to PIC, registers
 * were a 32bit quantity and double word aligned (and only accessible as a
 * 32bit word.  PIC registers are 64bits and accessible as words or double
 * words.  PIC registers that have valid bits (ie. not just reserved) in the
 * upper 32bits are defined as a union of one 64bit picreg_t and two 32bit
 * bridgereg_t so we can access them both ways.
 *
 * It is generally preferred that hardware registers on the bridge are
 * located from C code via this structure.
 *
 * Generated from Bridge spec dated 04oct95
 */

typedef volatile struct bridge_s {

    /* 0x000000-0x00FFFF -- Local Registers */

    /* 0x000000-0x000057 -- Standard Widget Configuration */
    union {
	widget_cfg_t	    xtalk_widget_def;	    /* 0x000000 */
	pic_widget_cfg_t    local_widget_def;	    /* 0x000000 */
    } u_wid;

    /* 32bit widget register access via the widget_cfg_t */
    #define b_widget u_wid.xtalk_widget_def

    /* 32bit widget register access via the pic_widget_cfg_t */
    #define b_wid_id u_wid.local_widget_def._b_wid_id
    #define b_wid_stat u_wid.local_widget_def.__b_wid_stat
    #define b_wid_err_upper u_wid.local_widget_def._b_wid_err_upper
    #define b_wid_err_lower u_wid.local_widget_def.__b_wid_err_lower
    #define b_wid_control u_wid.local_widget_def.__b_wid_control
    #define b_wid_req_timeout u_wid.local_widget_def._b_wid_req_timeout
    #define b_wid_int_upper u_wid.local_widget_def._b_wid_int_upper
    #define b_wid_int_lower u_wid.local_widget_def.__b_wid_int_lower
    #define b_wid_err_cmdword u_wid.local_widget_def._b_wid_err_cmdword
    #define b_wid_llp u_wid.local_widget_def._b_wid_llp
    #define b_wid_tflush u_wid.local_widget_def._b_wid_tflush

    /* 64bit widget register access via the pic_widget_cfg_t */
    #define p_wid_stat_64 u_wid.local_widget_def.__p_wid_stat_64
    #define p_wid_err_64 u_wid.local_widget_def.__p_wid_err_64
    #define p_wid_control_64 u_wid.local_widget_def.__p_wid_control_64
    #define p_wid_int_64 u_wid.local_widget_def.__p_wid_int_64

    /* 0x000058-0x00007F -- Bridge-specific Widget Configuration */
    bridgereg_t             b_wid_aux_err;          /* 0x00005C */
    bridgereg_t             _pad_000058;

    bridgereg_t             b_wid_resp_upper;       /* 0x000064 */
    bridgereg_t             _pad_000060;

    union {
        picreg_t            _p_wid_resp_lower;      /* 0x000068 */
        struct {
            bridgereg_t     _b_wid_resp_lower;      /* 0x00006C */
            bridgereg_t     _b_pad_000068;
        } _b;
    } u_wid_resp_lower;
    #define p_wid_resp_64 u_wid_resp_lower._p_wid_resp_lower
    #define b_wid_resp_lower u_wid_resp_lower._b._b_wid_resp_lower

    bridgereg_t             b_wid_tst_pin_ctrl;     /* 0x000074 */
    bridgereg_t             _pad_000070;

    union {
        picreg_t            _p_addr_lkerr;          /* 0x000078 */
        struct {
            bridgereg_t     _b_pad_00007C;
            bridgereg_t     _b_pad_000078;
        } _b;
    } u_addr_lkerr;
    #define p_addr_lkerr_64 u_addr_lkerr._p_addr_lkerr

    /* 0x000080-0x00008F -- PMU */
    bridgereg_t		    b_dir_map;		    /* 0x000084 */
    bridgereg_t		    _pad_000080;

    bridgereg_t		    _pad_00008C;
    bridgereg_t		    _pad_000088;

    /* 0x000090-0x00009F -- SSRAM */
    bridgereg_t             b_ram_perr_or_map_fault;/* 0x000094 */
    bridgereg_t		    _pad_000090;
    #define b_ram_perr  b_ram_perr_or_map_fault     /* Bridge */
    #define b_map_fault b_ram_perr_or_map_fault     /* Xbridge & PIC */

    bridgereg_t		    _pad_00009C;
    bridgereg_t		    _pad_000098;

    /* 0x0000A0-0x0000AF -- Arbitration */
    bridgereg_t		    b_arb;		    /* 0x0000A4 */
    bridgereg_t		    _pad_0000A0;

    bridgereg_t		    _pad_0000AC;
    bridgereg_t		    _pad_0000A8;

    /* 0x0000B0-0x0000BF -- Number In A Can or ATE Parity Error */
    union {
	picreg_t	    _p_ate_parity_err;	    /* 0x0000B0 */
	struct {
	    bridgereg_t	    _b_nic;		    /* 0x0000B4 */
	    bridgereg_t	    _b_pad_0000B0;
	} _b;
    } u_ate_parity_err_or_nic;
    #define p_ate_parity_err_64 u_ate_parity_err_or_nic._p_ate_parity_err
    #define b_nic u_ate_parity_err_or_nic._b._b_nic

    bridgereg_t		    _pad_0000BC;
    bridgereg_t		    _pad_0000B8;

    /* 0x0000C0-0x0000FF -- PCI/GIO */
    bridgereg_t		    b_bus_timeout;	    /* 0x0000C4 */
    bridgereg_t		    _pad_0000C0;
    #define b_pci_bus_timeout b_bus_timeout

    bridgereg_t		    b_pci_cfg;		    /* 0x0000CC */
    bridgereg_t		    _pad_0000C8;

    bridgereg_t		    b_pci_err_upper;	    /* 0x0000D4 */
    bridgereg_t		    _pad_0000D0;
    #define b_gio_err_upper b_pci_err_upper

    union {
	picreg_t	    _p_pci_err_lower;	    /* 0x0000D8 */
	struct {
	    bridgereg_t	    _b_pci_err_lower;	    /* 0x0000DC */
	    bridgereg_t	    _b_pad_0000D8;
	} _b;
    } u_pci_err_lower;
    #define p_pci_err_64 u_pci_err_lower._p_pci_err_lower
    #define b_pci_err_lower u_pci_err_lower._b._b_pci_err_lower
    #define b_gio_err_lower b_pci_err_lower

    bridgereg_t		    _pad_0000E0[8];

    /* 0x000100-0x0001FF -- Interrupt */
    union {
	picreg_t	    _p_int_status;	    /* 0x000100 */		
	struct {
	    bridgereg_t	    _b_int_status;	    /* 0x000104 */
	    bridgereg_t	    _b_pad_000100;
	} _b;
    } u_int_status;
    #define p_int_status_64 u_int_status._p_int_status
    #define b_int_status u_int_status._b._b_int_status

    union {
	picreg_t	    _p_int_enable;	    /* 0x000108 */		
	struct {
	    bridgereg_t	    _b_int_enable;	    /* 0x00010C */
	    bridgereg_t	    _b_pad_000108;
	} _b;
    } u_int_enable;
    #define p_int_enable_64 u_int_enable._p_int_enable
    #define b_int_enable u_int_enable._b._b_int_enable

    union {
	picreg_t	    _p_int_rst_stat;	    /* 0x000110 */		
	struct {
	    bridgereg_t	    _b_int_rst_stat;	    /* 0x000114 */
	    bridgereg_t	    _b_pad_000110;
	} _b;
    } u_int_rst_stat;
    #define p_int_rst_stat_64 u_int_rst_stat._p_int_rst_stat
    #define b_int_rst_stat u_int_rst_stat._b._b_int_rst_stat

    bridgereg_t		    b_int_mode;		    /* 0x00011C */
    bridgereg_t		    _pad_000118;

    bridgereg_t		    b_int_device;	    /* 0x000124 */
    bridgereg_t		    _pad_000120;

    bridgereg_t		    b_int_host_err;	    /* 0x00012C */
    bridgereg_t		    _pad_000128;

    union {
	picreg_t	    _p_int_addr[8];	    /* 0x0001{30,,,68} */
	struct {
	    bridgereg_t	    addr;		    /* 0x0001{34,,,6C} */
	    bridgereg_t	    _b_pad;
	} _b[8];
    } u_int_addr;
    #define p_int_addr_64 u_int_addr._p_int_addr
    #define b_int_addr u_int_addr._b

    union {
	picreg_t	    _p_err_int_view;	    /* 0x000170 */
	struct {
	    bridgereg_t	    _b_err_int_view;	    /* 0x000174 */
	    bridgereg_t	    _b_pad_000170;
	} _b;
    } u_err_int_view;
    #define p_err_int_view_64 u_err_int_view._p_err_int_view
    #define b_err_int_view u_err_int_view._b._b_err_int_view

    union {
	picreg_t	    _p_mult_int;	    /* 0x000178 */
	struct {
	    bridgereg_t	    _b_mult_int;	    /* 0x00017C */
	    bridgereg_t	    _b_pad_000178;
	} _b;
    } u_mult_int;
    #define p_mult_int_64 u_mult_int._p_mult_int
    #define b_mult_int u_mult_int._b._b_mult_int

    struct {
	bridgereg_t	    intr;		    /* 0x0001{84,,,BC} */
	bridgereg_t	    __pad;
    } b_force_always[8];

    struct {
	bridgereg_t	    intr;		    /* 0x0001{C4,,,FC} */
	bridgereg_t	    __pad;
    } b_force_pin[8];

    /* 0x000200-0x0003FF -- Device */
    struct {
	bridgereg_t	    reg;		    /* 0x0002{04,,,3C} */
	bridgereg_t	    __pad;
    } b_device[8];

    struct {
	bridgereg_t	    reg;		    /* 0x0002{44,,,7C} */
	bridgereg_t	    __pad;
    } b_wr_req_buf[8];

    struct {
	bridgereg_t	    reg;		    /* 0x0002{84,,,8C} */
	bridgereg_t	    __pad;
    } b_rrb_map[2];
    #define b_even_resp	b_rrb_map[0].reg	    /* 0x000284 */
    #define b_odd_resp	b_rrb_map[1].reg	    /* 0x00028C */

    bridgereg_t		    b_resp_status;	    /* 0x000294 */
    bridgereg_t		    _pad_000290;

    bridgereg_t		    b_resp_clear;	    /* 0x00029C */
    bridgereg_t		    _pad_000298;

    bridgereg_t		    _pad_0002A0[24];

    /* Xbridge/PIC only */
    union {
	struct {
	    picreg_t	    lower;		    /* 0x0003{08,,,F8} */
	    picreg_t	    upper;		    /* 0x0003{00,,,F0} */
	} _p[16];
	struct {
	    bridgereg_t	    upper;		    /* 0x0003{04,,,F4} */
	    bridgereg_t	    _b_pad1;
	    bridgereg_t	    lower;		    /* 0x0003{0C,,,FC} */
	    bridgereg_t	    _b_pad2;
	} _b[16];
    } u_buf_addr_match;
    #define p_buf_addr_match_64 u_buf_addr_match._p
    #define b_buf_addr_match u_buf_addr_match._b

    /* 0x000400-0x0005FF -- Performance Monitor Registers (even only) */
    struct {
	bridgereg_t	    flush_w_touch;	    /* 0x000{404,,,5C4} */
	bridgereg_t	    __pad1;
	bridgereg_t	    flush_wo_touch;	    /* 0x000{40C,,,5CC} */
	bridgereg_t	    __pad2;
	bridgereg_t	    inflight;		    /* 0x000{414,,,5D4} */
	bridgereg_t	    __pad3;
	bridgereg_t	    prefetch;		    /* 0x000{41C,,,5DC} */
	bridgereg_t	    __pad4;
	bridgereg_t	    total_pci_retry;	    /* 0x000{424,,,5E4} */
	bridgereg_t	    __pad5;
	bridgereg_t	    max_pci_retry;	    /* 0x000{42C,,,5EC} */
	bridgereg_t	    __pad6;
	bridgereg_t	    max_latency;	    /* 0x000{434,,,5F4} */
	bridgereg_t	    __pad7;
	bridgereg_t	    clear_all;		    /* 0x000{43C,,,5FC} */
	bridgereg_t	    __pad8;
    } b_buf_count[8];

    /*
     * "PCI/X registers that are specific to PIC".   See pic.h.
     */

    /* 0x000600-0x0009FF -- PCI/X registers */
    picreg_t		    p_pcix_bus_err_addr_64;     /* 0x000600 */
    picreg_t		    p_pcix_bus_err_attr_64;     /* 0x000608 */
    picreg_t		    p_pcix_bus_err_data_64;     /* 0x000610 */
    picreg_t		    p_pcix_pio_split_addr_64;	/* 0x000618 */
    picreg_t		    p_pcix_pio_split_attr_64;	/* 0x000620 */
    picreg_t		    p_pcix_dma_req_err_attr_64; /* 0x000628 */
    picreg_t		    p_pcix_dma_req_err_addr_64;	/* 0x000630 */
    picreg_t		    p_pcix_timeout_64;		/* 0x000638 */

    picreg_t		    _pad_000600[120];

    /* 0x000A00-0x000BFF -- PCI/X Read&Write Buffer */
    struct {
    	picreg_t	    p_buf_attr;		    /* 0X000{A08,,,AF8} */
    	picreg_t	    p_buf_addr;		    /* 0x000{A00,,,AF0} */
    } p_pcix_read_buf_64[16];

    struct {
	picreg_t	    p_buf_attr;		    /* 0x000{B08,,,BE8} */
	picreg_t	    p_buf_addr;		    /* 0x000{B00,,,BE0} */
	picreg_t	    __pad1;		    /* 0x000{B18,,,BF8} */
	picreg_t	    p_buf_valid;	    /* 0x000{B10,,,BF0} */
    } p_pcix_write_buf_64[8];

    /* 
     * end "PCI/X registers that are specific to PIC"
     */

    char		    _pad_000c00[0x010000 - 0x000c00];

    /* 0x010000-0x011fff -- Internal Address Translation Entry RAM */
    /*
     * Xbridge and PIC have 1024 internal ATE's and the Bridge has 128.
     * Make enough room for the Xbridge/PIC ATE's and depend on runtime
     * checks to limit access to bridge ATE's.
     *
     * In [X]bridge the internal ATE Ram is writen as double words only,
     * but due to internal design issues it is read back as single words. 
     * i.e:
     *   b_int_ate_ram[index].hi.rd << 32 | xb_int_ate_ram_lo[index].rd
     */
    union {
	bridge_ate_t	    wr;	/* write-only */    /* 0x01{0000,,,1FF8} */
	struct {
	    bridgereg_t	    rd; /* read-only */     /* 0x01{0004,,,1FFC} */
	    bridgereg_t	    _p_pad;
	} hi;
    } b_int_ate_ram[XBRIDGE_INTERNAL_ATES];
    #define b_int_ate_ram_lo(idx) b_int_ate_ram[idx+512].hi.rd

    /* 0x012000-0x013fff -- Internal Address Translation Entry RAM LOW */
    struct {
	bridgereg_t	    rd; /* read-only */	    /* 0x01{2004,,,3FFC} */
	bridgereg_t	    _p_pad;
    } xb_int_ate_ram_lo[XBRIDGE_INTERNAL_ATES];

    char		    _pad_014000[0x18000 - 0x014000];

    /* 0x18000-0x197F8 -- PIC Write Request Ram */
				/* 0x18000 - 0x187F8 */
    picreg_t		    p_wr_req_lower[PIC_WR_REQ_BUFSIZE];
				/* 0x18800 - 0x18FF8 */
    picreg_t		    p_wr_req_upper[PIC_WR_REQ_BUFSIZE];
				/* 0x19000 - 0x197F8 */
    picreg_t		    p_wr_req_parity[PIC_WR_REQ_BUFSIZE];

    char		    _pad_019800[0x20000 - 0x019800];

    /* 0x020000-0x027FFF -- PCI Device Configuration Spaces */
    union {				/* make all access sizes available. */
	uchar_t		    c[0x1000 / 1];	    /* 0x02{0000,,,7FFF} */
	uint16_t	    s[0x1000 / 2];	    /* 0x02{0000,,,7FFF} */
	uint32_t	    l[0x1000 / 4];	    /* 0x02{0000,,,7FFF} */
	uint64_t	    d[0x1000 / 8];	    /* 0x02{0000,,,7FFF} */
	union {
	    uchar_t	    c[0x100 / 1];
	    uint16_t	    s[0x100 / 2];
	    uint32_t	    l[0x100 / 4];
	    uint64_t	    d[0x100 / 8];
	} f[8];
    } b_type0_cfg_dev[8];			    /* 0x02{0000,,,7FFF} */

    /* 0x028000-0x028FFF -- PCI Type 1 Configuration Space */
    union {				/* make all access sizes available. */
	uchar_t		    c[0x1000 / 1];
	uint16_t	    s[0x1000 / 2];
	uint32_t	    l[0x1000 / 4];
	uint64_t	    d[0x1000 / 8];
        union {
            uchar_t         c[0x100 / 1];
            uint16_t        s[0x100 / 2];
            uint32_t        l[0x100 / 4];
            uint64_t        d[0x100 / 8];
	} f[8];
    } b_type1_cfg;				    /* 0x028000-0x029000 */

    char		    _pad_029000[0x007000];  /* 0x029000-0x030000 */

    /* 0x030000-0x030007 -- PCI Interrupt Acknowledge Cycle */
    union {
	uchar_t		    c[8 / 1];
	uint16_t	    s[8 / 2];
	uint32_t	    l[8 / 4];
	uint64_t	    d[8 / 8];
    } b_pci_iack;				    /* 0x030000-0x030007 */

    uchar_t		    _pad_030007[0x04fff8];  /* 0x030008-0x07FFFF */

    /* 0x080000-0x0FFFFF -- External Address Translation Entry RAM */
    bridge_ate_t	    b_ext_ate_ram[0x10000];

    /* 0x100000-0x1FFFFF -- Reserved */
    char		    _pad_100000[0x200000-0x100000];

    /* 0x200000-0xBFFFFF -- PCI/GIO Device Spaces */
    union {				/* make all access sizes available. */
	uchar_t		    c[0x100000 / 1];
	uint16_t	    s[0x100000 / 2];
	uint32_t	    l[0x100000 / 4];
	uint64_t	    d[0x100000 / 8];
    } b_devio_raw[10];

    /* b_devio macro is a bit strange; it reflects the
     * fact that the Bridge ASIC provides 2M for the
     * first two DevIO windows and 1M for the other six.
     */
    #define b_devio(n)	b_devio_raw[((n)<2)?(n*2):(n+2)]

    /* 0xC00000-0xFFFFFF -- External Flash Proms 1,0 */
    union {				/* make all access sizes available. */
	uchar_t		    c[0x400000 / 1];	/* read-only */
	uint16_t	    s[0x400000 / 2];	/* read-write */
	uint32_t	    l[0x400000 / 4];	/* read-only */
	uint64_t	    d[0x400000 / 8];	/* read-only */
    } b_external_flash;
} bridge_t;

#define berr_field	berr_un.berr_st
#endif				/* __ASSEMBLY__ */

/*
 * The values of these macros can and should be crosschecked
 * regularly against the offsets of the like-named fields
 * within the "bridge_t" structure above.
 */

/* Byte offset macros for Bridge internal registers */

#define BRIDGE_WID_ID		WIDGET_ID
#define BRIDGE_WID_STAT		WIDGET_STATUS
#define BRIDGE_WID_ERR_UPPER	WIDGET_ERR_UPPER_ADDR
#define BRIDGE_WID_ERR_LOWER	WIDGET_ERR_LOWER_ADDR
#define BRIDGE_WID_CONTROL	WIDGET_CONTROL
#define BRIDGE_WID_REQ_TIMEOUT	WIDGET_REQ_TIMEOUT
#define BRIDGE_WID_INT_UPPER	WIDGET_INTDEST_UPPER_ADDR
#define BRIDGE_WID_INT_LOWER	WIDGET_INTDEST_LOWER_ADDR
#define BRIDGE_WID_ERR_CMDWORD	WIDGET_ERR_CMD_WORD
#define BRIDGE_WID_LLP		WIDGET_LLP_CFG
#define BRIDGE_WID_TFLUSH	WIDGET_TFLUSH

#define BRIDGE_WID_AUX_ERR	0x00005C	/* Aux Error Command Word */
#define BRIDGE_WID_RESP_UPPER	0x000064	/* Response Buf Upper Addr */
#define BRIDGE_WID_RESP_LOWER	0x00006C	/* Response Buf Lower Addr */
#define BRIDGE_WID_TST_PIN_CTRL 0x000074	/* Test pin control */

#define BRIDGE_DIR_MAP		0x000084	/* Direct Map reg */

/* Bridge has SSRAM Parity Error and Xbridge has Map Fault here */
#define BRIDGE_RAM_PERR 	0x000094	/* SSRAM Parity Error */
#define BRIDGE_MAP_FAULT	0x000094	/* Map Fault */

#define BRIDGE_ARB		0x0000A4	/* Arbitration Priority reg */

#define BRIDGE_NIC		0x0000B4	/* Number In A Can */

#define BRIDGE_BUS_TIMEOUT	0x0000C4	/* Bus Timeout Register */
#define BRIDGE_PCI_BUS_TIMEOUT	BRIDGE_BUS_TIMEOUT
#define BRIDGE_PCI_CFG		0x0000CC	/* PCI Type 1 Config reg */
#define BRIDGE_PCI_ERR_UPPER	0x0000D4	/* PCI error Upper Addr */
#define BRIDGE_PCI_ERR_LOWER	0x0000DC	/* PCI error Lower Addr */

#define BRIDGE_INT_STATUS	0x000104	/* Interrupt Status */
#define BRIDGE_INT_ENABLE	0x00010C	/* Interrupt Enables */
#define BRIDGE_INT_RST_STAT	0x000114	/* Reset Intr Status */
#define BRIDGE_INT_MODE		0x00011C	/* Interrupt Mode */
#define BRIDGE_INT_DEVICE	0x000124	/* Interrupt Device */
#define BRIDGE_INT_HOST_ERR	0x00012C	/* Host Error Field */

#define BRIDGE_INT_ADDR0	0x000134	/* Host Address Reg */
#define BRIDGE_INT_ADDR_OFF	0x000008	/* Host Addr offset (1..7) */
#define BRIDGE_INT_ADDR(x)	(BRIDGE_INT_ADDR0+(x)*BRIDGE_INT_ADDR_OFF)

#define BRIDGE_INT_VIEW		0x000174	/* Interrupt view */
#define BRIDGE_MULTIPLE_INT	0x00017c	/* Multiple interrupt occurred */

#define BRIDGE_FORCE_ALWAYS0	0x000184	/* Force an interrupt (always)*/
#define BRIDGE_FORCE_ALWAYS_OFF 0x000008	/* Force Always offset */
#define BRIDGE_FORCE_ALWAYS(x)  (BRIDGE_FORCE_ALWAYS0+(x)*BRIDGE_FORCE_ALWAYS_OFF)

#define BRIDGE_FORCE_PIN0	0x0001c4	/* Force an interrupt */
#define BRIDGE_FORCE_PIN_OFF 	0x000008	/* Force Pin offset */
#define BRIDGE_FORCE_PIN(x)  (BRIDGE_FORCE_PIN0+(x)*BRIDGE_FORCE_PIN_OFF)

#define BRIDGE_DEVICE0		0x000204	/* Device 0 */
#define BRIDGE_DEVICE_OFF	0x000008	/* Device offset (1..7) */
#define BRIDGE_DEVICE(x)	(BRIDGE_DEVICE0+(x)*BRIDGE_DEVICE_OFF)

#define BRIDGE_WR_REQ_BUF0	0x000244	/* Write Request Buffer 0 */
#define BRIDGE_WR_REQ_BUF_OFF	0x000008	/* Buffer Offset (1..7) */
#define BRIDGE_WR_REQ_BUF(x)	(BRIDGE_WR_REQ_BUF0+(x)*BRIDGE_WR_REQ_BUF_OFF)

#define BRIDGE_EVEN_RESP	0x000284	/* Even Device Response Buf */
#define BRIDGE_ODD_RESP		0x00028C	/* Odd Device Response Buf */

#define BRIDGE_RESP_STATUS	0x000294	/* Read Response Status reg */
#define BRIDGE_RESP_CLEAR	0x00029C	/* Read Response Clear reg */

#define BRIDGE_BUF_ADDR_UPPER0	0x000304
#define BRIDGE_BUF_ADDR_UPPER_OFF 0x000010	/* PCI Buffer Upper Offset */
#define BRIDGE_BUF_ADDR_UPPER(x) (BRIDGE_BUF_ADDR_UPPER0+(x)*BRIDGE_BUF_ADDR_UPPER_OFF)

#define BRIDGE_BUF_ADDR_LOWER0	0x00030c
#define BRIDGE_BUF_ADDR_LOWER_OFF 0x000010	/* PCI Buffer Upper Offset */
#define BRIDGE_BUF_ADDR_LOWER(x) (BRIDGE_BUF_ADDR_LOWER0+(x)*BRIDGE_BUF_ADDR_LOWER_OFF)

/* 
 * Performance Monitor Registers.
 *
 * The Performance registers are those registers which are associated with
 * monitoring the performance of PCI generated reads to the host environ
 * ment. Because of the size of the register file only the even registers
 * were instrumented.
 */

#define BRIDGE_BUF_OFF 0x40
#define BRIDGE_BUF_NEXT(base, off) (base+((off)*BRIDGE_BUF_OFF))

/*
 * Buffer (x) Flush Count with Data Touch Register.
 *
 * This counter is incremented each time the corresponding response buffer
 * is flushed after at least a single data element in the buffer is used.
 * A word write to this address clears the count.
 */

#define BRIDGE_BUF_0_FLUSH_TOUCH  0x000404
#define BRIDGE_BUF_2_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 1)
#define BRIDGE_BUF_4_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 2)
#define BRIDGE_BUF_6_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 3)
#define BRIDGE_BUF_8_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 4)
#define BRIDGE_BUF_10_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 5)
#define BRIDGE_BUF_12_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 6)
#define BRIDGE_BUF_14_FLUSH_TOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_TOUCH, 7)

/*
 * Buffer (x) Flush Count w/o Data Touch Register
 *
 * This counter is incremented each time the corresponding response buffer
 * is flushed without any data element in the buffer being used. A word
 * write to this address clears the count.
 */


#define BRIDGE_BUF_0_FLUSH_NOTOUCH  0x00040c
#define BRIDGE_BUF_2_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 1)
#define BRIDGE_BUF_4_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 2)
#define BRIDGE_BUF_6_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 3)
#define BRIDGE_BUF_8_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 4)
#define BRIDGE_BUF_10_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 5)
#define BRIDGE_BUF_12_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 6)
#define BRIDGE_BUF_14_FLUSH_NOTOUCH  BRIDGE_BUF_NEXT(BRIDGE_BUF_0_FLUSH_NOTOUCH, 7)

/*
 * Buffer (x) Request in Flight Count Register
 *
 * This counter is incremented on each bus clock while the request is in
 * flight. A word write to this address clears the count.
 */

#define BRIDGE_BUF_0_INFLIGHT	 0x000414
#define BRIDGE_BUF_2_INFLIGHT    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 1)
#define BRIDGE_BUF_4_INFLIGHT    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 2)
#define BRIDGE_BUF_6_INFLIGHT    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 3)
#define BRIDGE_BUF_8_INFLIGHT    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 4)
#define BRIDGE_BUF_10_INFLIGHT   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 5)
#define BRIDGE_BUF_12_INFLIGHT   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 6)
#define BRIDGE_BUF_14_INFLIGHT   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_INFLIGHT, 7)

/*
 * Buffer (x) Prefetch Request Count Register
 *
 * This counter is incremented each time the request using this buffer was
 * generated from the prefetcher. A word write to this address clears the
 * count.
 */

#define BRIDGE_BUF_0_PREFETCH	 0x00041C
#define BRIDGE_BUF_2_PREFETCH    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 1)
#define BRIDGE_BUF_4_PREFETCH    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 2)
#define BRIDGE_BUF_6_PREFETCH    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 3)
#define BRIDGE_BUF_8_PREFETCH    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 4)
#define BRIDGE_BUF_10_PREFETCH   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 5)
#define BRIDGE_BUF_12_PREFETCH   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 6)
#define BRIDGE_BUF_14_PREFETCH   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PREFETCH, 7)

/*
 * Buffer (x) Total PCI Retry Count Register
 *
 * This counter is incremented each time a PCI bus retry occurs and the ad
 * dress matches the tag for the selected buffer. The buffer must also has
 * this request in-flight. A word write to this address clears the count.
 */

#define BRIDGE_BUF_0_PCI_RETRY	 0x000424
#define BRIDGE_BUF_2_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 1)
#define BRIDGE_BUF_4_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 2)
#define BRIDGE_BUF_6_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 3)
#define BRIDGE_BUF_8_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 4)
#define BRIDGE_BUF_10_PCI_RETRY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 5)
#define BRIDGE_BUF_12_PCI_RETRY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 6)
#define BRIDGE_BUF_14_PCI_RETRY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_PCI_RETRY, 7)

/*
 * Buffer (x) Max PCI Retry Count Register
 *
 * This counter is contains the maximum retry count for a single request
 * which was in-flight for this buffer. A word write to this address
 * clears the count.
 */

#define BRIDGE_BUF_0_MAX_PCI_RETRY	 0x00042C
#define BRIDGE_BUF_2_MAX_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 1)
#define BRIDGE_BUF_4_MAX_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 2)
#define BRIDGE_BUF_6_MAX_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 3)
#define BRIDGE_BUF_8_MAX_PCI_RETRY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 4)
#define BRIDGE_BUF_10_MAX_PCI_RETRY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 5)
#define BRIDGE_BUF_12_MAX_PCI_RETRY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 6)
#define BRIDGE_BUF_14_MAX_PCI_RETRY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_PCI_RETRY, 7)

/*
 * Buffer (x) Max Latency Count Register
 *
 * This counter is contains the maximum count (in bus clocks) for a single
 * request which was in-flight for this buffer. A word write to this
 * address clears the count.
 */

#define BRIDGE_BUF_0_MAX_LATENCY	 0x000434
#define BRIDGE_BUF_2_MAX_LATENCY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 1)
#define BRIDGE_BUF_4_MAX_LATENCY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 2)
#define BRIDGE_BUF_6_MAX_LATENCY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 3)
#define BRIDGE_BUF_8_MAX_LATENCY    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 4)
#define BRIDGE_BUF_10_MAX_LATENCY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 5)
#define BRIDGE_BUF_12_MAX_LATENCY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 6)
#define BRIDGE_BUF_14_MAX_LATENCY   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_MAX_LATENCY, 7)

/*
 * Buffer (x) Clear All Register
 *
 * Any access to this register clears all the count values for the (x)
 * registers.
 */

#define BRIDGE_BUF_0_CLEAR_ALL	 0x00043C
#define BRIDGE_BUF_2_CLEAR_ALL    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 1)
#define BRIDGE_BUF_4_CLEAR_ALL    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 2)
#define BRIDGE_BUF_6_CLEAR_ALL    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 3)
#define BRIDGE_BUF_8_CLEAR_ALL    BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 4)
#define BRIDGE_BUF_10_CLEAR_ALL   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 5)
#define BRIDGE_BUF_12_CLEAR_ALL   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 6)
#define BRIDGE_BUF_14_CLEAR_ALL   BRIDGE_BUF_NEXT(BRIDGE_BUF_0_CLEAR_ALL, 7)

/* end of Performance Monitor Registers */

/* Byte offset macros for Bridge I/O space.
 *
 * NOTE: Where applicable please use the PCIBR_xxx or PCIBRIDGE_xxx
 * macros (below) as they will handle [X]Bridge and PIC. For example,
 * PCIBRIDGE_TYPE0_CFG_DEV0() vs BRIDGE_TYPE0_CFG_DEV0
 */

#define BRIDGE_ATE_RAM		0x00010000	/* Internal Addr Xlat Ram */

#define BRIDGE_TYPE0_CFG_DEV0	0x00020000	/* Type 0 Cfg, Device 0 */
#define BRIDGE_TYPE0_CFG_SLOT_OFF	0x00001000	/* Type 0 Cfg Slot Offset (1..7) */
#define BRIDGE_TYPE0_CFG_FUNC_OFF	0x00000100	/* Type 0 Cfg Func Offset (1..7) */
#define BRIDGE_TYPE0_CFG_DEV(s)		(BRIDGE_TYPE0_CFG_DEV0+\
					 (s)*BRIDGE_TYPE0_CFG_SLOT_OFF)
#define BRIDGE_TYPE0_CFG_DEVF(s,f)	(BRIDGE_TYPE0_CFG_DEV0+\
					 (s)*BRIDGE_TYPE0_CFG_SLOT_OFF+\
					 (f)*BRIDGE_TYPE0_CFG_FUNC_OFF)

#define BRIDGE_TYPE1_CFG	0x00028000	/* Type 1 Cfg space */

#define BRIDGE_PCI_IACK		0x00030000	/* PCI Interrupt Ack */
#define BRIDGE_EXT_SSRAM	0x00080000	/* Extern SSRAM (ATE) */

/* Byte offset macros for Bridge device IO spaces */

#define BRIDGE_DEV_CNT		8	/* Up to 8 devices per bridge */
#define BRIDGE_DEVIO0		0x00200000	/* Device IO 0 Addr */
#define BRIDGE_DEVIO1		0x00400000	/* Device IO 1 Addr */
#define BRIDGE_DEVIO2		0x00600000	/* Device IO 2 Addr */
#define BRIDGE_DEVIO_OFF	0x00100000	/* Device IO Offset (3..7) */

#define BRIDGE_DEVIO_2MB	0x00200000	/* Device IO Offset (0..1) */
#define BRIDGE_DEVIO_1MB	0x00100000	/* Device IO Offset (2..7) */

#ifndef __ASSEMBLY__

#define BRIDGE_DEVIO(x)		((x)<=1 ? BRIDGE_DEVIO0+(x)*BRIDGE_DEVIO_2MB : BRIDGE_DEVIO2+((x)-2)*BRIDGE_DEVIO_1MB)

/*
 * The device space macros for PIC are more complicated because the PIC has
 * two PCI/X bridges under the same widget.  For PIC bus 0, the addresses are
 * basically the same as for the [X]Bridge.  For PIC bus 1, the addresses are
 * offset by 0x800000.   Here are two sets of macros.  They are 
 * "PCIBRIDGE_xxx" that return the address based on the supplied bus number
 * and also equivalent "PCIBR_xxx" macros that may be used with a
 * pcibr_soft_s structure.   Both should work with all bridges.
 */
#define PIC_BUS1_OFFSET 0x800000

#define PCIBRIDGE_TYPE0_CFG_DEV0(busnum) \
    ((busnum) ? BRIDGE_TYPE0_CFG_DEV0 + PIC_BUS1_OFFSET : \
                    BRIDGE_TYPE0_CFG_DEV0)
#define PCIBRIDGE_TYPE1_CFG(busnum) \
    ((busnum) ? BRIDGE_TYPE1_CFG + PIC_BUS1_OFFSET : BRIDGE_TYPE1_CFG)
#define PCIBRIDGE_TYPE0_CFG_DEV(busnum, s) \
        (PCIBRIDGE_TYPE0_CFG_DEV0(busnum)+\
        (s)*BRIDGE_TYPE0_CFG_SLOT_OFF)
#define PCIBRIDGE_TYPE0_CFG_DEVF(busnum, s, f) \
        (PCIBRIDGE_TYPE0_CFG_DEV0(busnum)+\
        (s)*BRIDGE_TYPE0_CFG_SLOT_OFF+\
        (f)*BRIDGE_TYPE0_CFG_FUNC_OFF)
#define PCIBRIDGE_DEVIO0(busnum) ((busnum) ? \
        (BRIDGE_DEVIO0 + PIC_BUS1_OFFSET) : BRIDGE_DEVIO0)
#define PCIBRIDGE_DEVIO1(busnum) ((busnum) ? \
        (BRIDGE_DEVIO1 + PIC_BUS1_OFFSET) : BRIDGE_DEVIO1)
#define PCIBRIDGE_DEVIO2(busnum) ((busnum) ? \
        (BRIDGE_DEVIO2 + PIC_BUS1_OFFSET) : BRIDGE_DEVIO2)
#define PCIBRIDGE_DEVIO(busnum, x) \
    ((x)<=1 ? PCIBRIDGE_DEVIO0(busnum)+(x)*BRIDGE_DEVIO_2MB : \
        PCIBRIDGE_DEVIO2(busnum)+((x)-2)*BRIDGE_DEVIO_1MB)

#define PCIBR_BRIDGE_DEVIO0(ps)     PCIBRIDGE_DEVIO0((ps)->bs_busnum)
#define PCIBR_BRIDGE_DEVIO1(ps)     PCIBRIDGE_DEVIO1((ps)->bs_busnum)
#define PCIBR_BRIDGE_DEVIO2(ps)     PCIBRIDGE_DEVIO2((ps)->bs_busnum)
#define PCIBR_BRIDGE_DEVIO(ps, s)   PCIBRIDGE_DEVIO((ps)->bs_busnum, s)

#define PCIBR_TYPE1_CFG(ps)         PCIBRIDGE_TYPE1_CFG((ps)->bs_busnum)
#define PCIBR_BUS_TYPE0_CFG_DEV0(ps) PCIBR_TYPE0_CFG_DEV(ps, 0)
#define PCIBR_TYPE0_CFG_DEV(ps, s) \
    ((IS_PIC_SOFT(ps)) ? PCIBRIDGE_TYPE0_CFG_DEV((ps)->bs_busnum, s+1) : \
		  	     PCIBRIDGE_TYPE0_CFG_DEV((ps)->bs_busnum, s))
#define PCIBR_BUS_TYPE0_CFG_DEVF(ps,s,f) \
    ((IS_PIC_SOFT(ps)) ? PCIBRIDGE_TYPE0_CFG_DEVF((ps)->bs_busnum,(s+1),f) : \
			     PCIBRIDGE_TYPE0_CFG_DEVF((ps)->bs_busnum,s,f))

#endif				/* LANGUAGE_C */

#define BRIDGE_EXTERNAL_FLASH	0x00C00000	/* External Flash PROMS */

/* ========================================================================
 *    Bridge register bit field definitions
 */

/* Widget part number of bridge */
#define BRIDGE_WIDGET_PART_NUM		0xc002
#define XBRIDGE_WIDGET_PART_NUM		0xd002

/* Manufacturer of bridge */
#define BRIDGE_WIDGET_MFGR_NUM		0x036
#define XBRIDGE_WIDGET_MFGR_NUM		0x024

/* Revision numbers for known [X]Bridge revisions */
#define BRIDGE_REV_A			0x1
#define BRIDGE_REV_B			0x2
#define BRIDGE_REV_C			0x3
#define	BRIDGE_REV_D			0x4
#define XBRIDGE_REV_A			0x1
#define XBRIDGE_REV_B			0x2

/* macros to determine bridge type. 'wid' == widget identification */
#define IS_BRIDGE(wid) (XWIDGET_PART_NUM(wid) == BRIDGE_WIDGET_PART_NUM && \
			XWIDGET_MFG_NUM(wid) == BRIDGE_WIDGET_MFGR_NUM)
#define IS_XBRIDGE(wid) (XWIDGET_PART_NUM(wid) == XBRIDGE_WIDGET_PART_NUM && \
			XWIDGET_MFG_NUM(wid) == XBRIDGE_WIDGET_MFGR_NUM)
#define IS_PIC_BUS0(wid) (XWIDGET_PART_NUM(wid) == PIC_WIDGET_PART_NUM_BUS0 && \
			XWIDGET_MFG_NUM(wid) == PIC_WIDGET_MFGR_NUM)
#define IS_PIC_BUS1(wid) (XWIDGET_PART_NUM(wid) == PIC_WIDGET_PART_NUM_BUS1 && \
			XWIDGET_MFG_NUM(wid) == PIC_WIDGET_MFGR_NUM)
#define IS_PIC_BRIDGE(wid) (IS_PIC_BUS0(wid) || IS_PIC_BUS1(wid))

/* Part + Rev numbers allows distinction and acscending sequence */
#define BRIDGE_PART_REV_A	(BRIDGE_WIDGET_PART_NUM << 4 | BRIDGE_REV_A)
#define BRIDGE_PART_REV_B	(BRIDGE_WIDGET_PART_NUM << 4 | BRIDGE_REV_B)
#define BRIDGE_PART_REV_C	(BRIDGE_WIDGET_PART_NUM << 4 | BRIDGE_REV_C)
#define	BRIDGE_PART_REV_D	(BRIDGE_WIDGET_PART_NUM << 4 | BRIDGE_REV_D)
#define XBRIDGE_PART_REV_A	(XBRIDGE_WIDGET_PART_NUM << 4 | XBRIDGE_REV_A)
#define XBRIDGE_PART_REV_B	(XBRIDGE_WIDGET_PART_NUM << 4 | XBRIDGE_REV_B)

/* Bridge widget status register bits definition */
#define PIC_STAT_PCIX_SPEED             (0x3ull << 34)
#define PIC_STAT_PCIX_ACTIVE            (0x1ull << 33)
#define BRIDGE_STAT_LLP_REC_CNT		(0xFFu << 24)
#define BRIDGE_STAT_LLP_TX_CNT		(0xFF << 16)
#define BRIDGE_STAT_FLASH_SELECT	(0x1 << 6)
#define BRIDGE_STAT_PCI_GIO_N		(0x1 << 5)
#define BRIDGE_STAT_PENDING		(0x1F << 0)

/* Bridge widget control register bits definition */
#define PIC_CTRL_NO_SNOOP		(0x1ull << 62)
#define PIC_CTRL_RELAX_ORDER		(0x1ull << 61)
#define PIC_CTRL_BUS_NUM(x)		((unsigned long long)(x) << 48)
#define PIC_CTRL_BUS_NUM_MASK		(PIC_CTRL_BUS_NUM(0xff))
#define PIC_CTRL_DEV_NUM(x)		((unsigned long long)(x) << 43)
#define PIC_CTRL_DEV_NUM_MASK		(PIC_CTRL_DEV_NUM(0x1f))
#define PIC_CTRL_FUN_NUM(x)		((unsigned long long)(x) << 40)
#define PIC_CTRL_FUN_NUM_MASK		(PIC_CTRL_FUN_NUM(0x7))
#define PIC_CTRL_PAR_EN_REQ		(0x1ull << 29)
#define PIC_CTRL_PAR_EN_RESP		(0x1ull << 30)
#define PIC_CTRL_PAR_EN_ATE		(0x1ull << 31)
#define BRIDGE_CTRL_FLASH_WR_EN		(0x1ul << 31)   /* bridge only */
#define BRIDGE_CTRL_EN_CLK50		(0x1 << 30)
#define BRIDGE_CTRL_EN_CLK40		(0x1 << 29)
#define BRIDGE_CTRL_EN_CLK33		(0x1 << 28)
#define BRIDGE_CTRL_RST(n)		((n) << 24)
#define BRIDGE_CTRL_RST_MASK		(BRIDGE_CTRL_RST(0xF))
#define BRIDGE_CTRL_RST_PIN(x)		(BRIDGE_CTRL_RST(0x1 << (x)))
#define BRIDGE_CTRL_IO_SWAP		(0x1 << 23)
#define BRIDGE_CTRL_MEM_SWAP		(0x1 << 22)
#define BRIDGE_CTRL_PAGE_SIZE		(0x1 << 21)
#define BRIDGE_CTRL_SS_PAR_BAD		(0x1 << 20)
#define BRIDGE_CTRL_SS_PAR_EN		(0x1 << 19)
#define BRIDGE_CTRL_SSRAM_SIZE(n)	((n) << 17)
#define BRIDGE_CTRL_SSRAM_SIZE_MASK	(BRIDGE_CTRL_SSRAM_SIZE(0x3))
#define BRIDGE_CTRL_SSRAM_512K		(BRIDGE_CTRL_SSRAM_SIZE(0x3))
#define BRIDGE_CTRL_SSRAM_128K		(BRIDGE_CTRL_SSRAM_SIZE(0x2))
#define BRIDGE_CTRL_SSRAM_64K		(BRIDGE_CTRL_SSRAM_SIZE(0x1))
#define BRIDGE_CTRL_SSRAM_1K		(BRIDGE_CTRL_SSRAM_SIZE(0x0))
#define BRIDGE_CTRL_F_BAD_PKT		(0x1 << 16)
#define BRIDGE_CTRL_LLP_XBAR_CRD(n)	((n) << 12)
#define BRIDGE_CTRL_LLP_XBAR_CRD_MASK	(BRIDGE_CTRL_LLP_XBAR_CRD(0xf))
#define BRIDGE_CTRL_CLR_RLLP_CNT	(0x1 << 11)
#define BRIDGE_CTRL_CLR_TLLP_CNT	(0x1 << 10)
#define BRIDGE_CTRL_SYS_END		(0x1 << 9)
#define BRIDGE_CTRL_PCI_SPEED		(0x3 << 4)

#define BRIDGE_CTRL_BUS_SPEED(n)        ((n) << 4)
#define BRIDGE_CTRL_BUS_SPEED_MASK      (BRIDGE_CTRL_BUS_SPEED(0x3))
#define BRIDGE_CTRL_BUS_SPEED_33        0x00
#define BRIDGE_CTRL_BUS_SPEED_66        0x10
#define BRIDGE_CTRL_MAX_TRANS(n)	((n) << 4)
#define BRIDGE_CTRL_MAX_TRANS_MASK	(BRIDGE_CTRL_MAX_TRANS(0x1f))
#define BRIDGE_CTRL_WIDGET_ID(n)	((n) << 0)
#define BRIDGE_CTRL_WIDGET_ID_MASK	(BRIDGE_CTRL_WIDGET_ID(0xf))

/* Bridge Response buffer Error Upper Register bit fields definition */
#define BRIDGE_RESP_ERRUPPR_DEVNUM_SHFT (20)
#define BRIDGE_RESP_ERRUPPR_DEVNUM_MASK (0x7 << BRIDGE_RESP_ERRUPPR_DEVNUM_SHFT)
#define BRIDGE_RESP_ERRUPPR_BUFNUM_SHFT (16)
#define BRIDGE_RESP_ERRUPPR_BUFNUM_MASK (0xF << BRIDGE_RESP_ERRUPPR_BUFNUM_SHFT)
#define BRIDGE_RESP_ERRRUPPR_BUFMASK	(0xFFFF)

#define BRIDGE_RESP_ERRUPPR_BUFNUM(x)	\
			(((x) & BRIDGE_RESP_ERRUPPR_BUFNUM_MASK) >> \
				BRIDGE_RESP_ERRUPPR_BUFNUM_SHFT)

#define BRIDGE_RESP_ERRUPPR_DEVICE(x)	\
			(((x) &	 BRIDGE_RESP_ERRUPPR_DEVNUM_MASK) >> \
				 BRIDGE_RESP_ERRUPPR_DEVNUM_SHFT)

/* Bridge direct mapping register bits definition */
#define BRIDGE_DIRMAP_W_ID_SHFT		20
#define BRIDGE_DIRMAP_W_ID		(0xf << BRIDGE_DIRMAP_W_ID_SHFT)
#define BRIDGE_DIRMAP_RMF_64		(0x1 << 18)
#define BRIDGE_DIRMAP_ADD512		(0x1 << 17)
#define BRIDGE_DIRMAP_OFF		(0x1ffff << 0)
#define BRIDGE_DIRMAP_OFF_ADDRSHFT	(31)	/* lsbit of DIRMAP_OFF is xtalk address bit 31 */

/* Bridge Arbitration register bits definition */
#define BRIDGE_ARB_REQ_WAIT_TICK(x)	((x) << 16)
#define BRIDGE_ARB_REQ_WAIT_TICK_MASK	BRIDGE_ARB_REQ_WAIT_TICK(0x3)
#define BRIDGE_ARB_REQ_WAIT_EN(x)	((x) << 8)
#define BRIDGE_ARB_REQ_WAIT_EN_MASK	BRIDGE_ARB_REQ_WAIT_EN(0xff)
#define BRIDGE_ARB_FREEZE_GNT		(1 << 6)
#define BRIDGE_ARB_HPRI_RING_B2		(1 << 5)
#define BRIDGE_ARB_HPRI_RING_B1		(1 << 4)
#define BRIDGE_ARB_HPRI_RING_B0		(1 << 3)
#define BRIDGE_ARB_LPRI_RING_B2		(1 << 2)
#define BRIDGE_ARB_LPRI_RING_B1		(1 << 1)
#define BRIDGE_ARB_LPRI_RING_B0		(1 << 0)

/* Bridge Bus time-out register bits definition */
#define BRIDGE_BUS_PCI_RETRY_HLD(x)	((x) << 16)
#define BRIDGE_BUS_PCI_RETRY_HLD_MASK	BRIDGE_BUS_PCI_RETRY_HLD(0x1f)
#define BRIDGE_BUS_GIO_TIMEOUT		(1 << 12)
#define BRIDGE_BUS_PCI_RETRY_CNT(x)	((x) << 0)
#define BRIDGE_BUS_PCI_RETRY_MASK	BRIDGE_BUS_PCI_RETRY_CNT(0x3ff)

/* Bridge interrupt status register bits definition */
#define PIC_ISR_PCIX_SPLIT_MSG_PE	(0x1ull << 45)
#define PIC_ISR_PCIX_SPLIT_EMSG		(0x1ull << 44)
#define PIC_ISR_PCIX_SPLIT_TO		(0x1ull << 43)
#define PIC_ISR_PCIX_UNEX_COMP		(0x1ull << 42)
#define PIC_ISR_INT_RAM_PERR		(0x1ull << 41)
#define PIC_ISR_PCIX_ARB_ERR		(0x1ull << 40)
#define PIC_ISR_PCIX_REQ_TOUT		(0x1ull << 39)
#define PIC_ISR_PCIX_TABORT		(0x1ull << 38)
#define PIC_ISR_PCIX_PERR		(0x1ull << 37)
#define PIC_ISR_PCIX_SERR		(0x1ull << 36)
#define PIC_ISR_PCIX_MRETRY		(0x1ull << 35)
#define PIC_ISR_PCIX_MTOUT		(0x1ull << 34)
#define PIC_ISR_PCIX_DA_PARITY		(0x1ull << 33)
#define PIC_ISR_PCIX_AD_PARITY		(0x1ull << 32)
#define BRIDGE_ISR_MULTI_ERR		(0x1u << 31)	/* bridge only */
#define BRIDGE_ISR_PMU_ESIZE_FAULT	(0x1 << 30)	/* bridge only */
#define BRIDGE_ISR_PAGE_FAULT		(0x1 << 30)	/* xbridge only */
#define BRIDGE_ISR_UNEXP_RESP		(0x1 << 29)
#define BRIDGE_ISR_BAD_XRESP_PKT	(0x1 << 28)
#define BRIDGE_ISR_BAD_XREQ_PKT		(0x1 << 27)
#define BRIDGE_ISR_RESP_XTLK_ERR	(0x1 << 26)
#define BRIDGE_ISR_REQ_XTLK_ERR		(0x1 << 25)
#define BRIDGE_ISR_INVLD_ADDR		(0x1 << 24)
#define BRIDGE_ISR_UNSUPPORTED_XOP	(0x1 << 23)
#define BRIDGE_ISR_XREQ_FIFO_OFLOW	(0x1 << 22)
#define BRIDGE_ISR_LLP_REC_SNERR	(0x1 << 21)
#define BRIDGE_ISR_LLP_REC_CBERR	(0x1 << 20)
#define BRIDGE_ISR_LLP_RCTY		(0x1 << 19)
#define BRIDGE_ISR_LLP_TX_RETRY		(0x1 << 18)
#define BRIDGE_ISR_LLP_TCTY		(0x1 << 17)
#define BRIDGE_ISR_SSRAM_PERR		(0x1 << 16)
#define BRIDGE_ISR_PCI_ABORT		(0x1 << 15)
#define BRIDGE_ISR_PCI_PARITY		(0x1 << 14)
#define BRIDGE_ISR_PCI_SERR		(0x1 << 13)
#define BRIDGE_ISR_PCI_PERR		(0x1 << 12)
#define BRIDGE_ISR_PCI_MST_TIMEOUT	(0x1 << 11)
#define BRIDGE_ISR_GIO_MST_TIMEOUT	BRIDGE_ISR_PCI_MST_TIMEOUT
#define BRIDGE_ISR_PCI_RETRY_CNT	(0x1 << 10)
#define BRIDGE_ISR_XREAD_REQ_TIMEOUT	(0x1 << 9)
#define BRIDGE_ISR_GIO_B_ENBL_ERR	(0x1 << 8)
#define BRIDGE_ISR_INT_MSK		(0xff << 0)
#define BRIDGE_ISR_INT(x)		(0x1 << (x))

#define BRIDGE_ISR_LINK_ERROR		\
		(BRIDGE_ISR_LLP_REC_SNERR|BRIDGE_ISR_LLP_REC_CBERR|	\
		 BRIDGE_ISR_LLP_RCTY|BRIDGE_ISR_LLP_TX_RETRY|		\
		 BRIDGE_ISR_LLP_TCTY)

#define BRIDGE_ISR_PCIBUS_PIOERR	\
		(BRIDGE_ISR_PCI_MST_TIMEOUT|BRIDGE_ISR_PCI_ABORT|	\
		 PIC_ISR_PCIX_MTOUT|PIC_ISR_PCIX_TABORT)

#define BRIDGE_ISR_PCIBUS_ERROR		\
		(BRIDGE_ISR_PCIBUS_PIOERR|BRIDGE_ISR_PCI_PERR|		\
		 BRIDGE_ISR_PCI_SERR|BRIDGE_ISR_PCI_RETRY_CNT|		\
		 BRIDGE_ISR_PCI_PARITY|PIC_ISR_PCIX_PERR|		\
		 PIC_ISR_PCIX_SERR|PIC_ISR_PCIX_MRETRY|			\
		 PIC_ISR_PCIX_AD_PARITY|PIC_ISR_PCIX_DA_PARITY|		\
		 PIC_ISR_PCIX_REQ_TOUT|PIC_ISR_PCIX_UNEX_COMP|		\
		 PIC_ISR_PCIX_SPLIT_TO|PIC_ISR_PCIX_SPLIT_EMSG|		\
		 PIC_ISR_PCIX_SPLIT_MSG_PE)

#define BRIDGE_ISR_XTALK_ERROR		\
		(BRIDGE_ISR_XREAD_REQ_TIMEOUT|BRIDGE_ISR_XREQ_FIFO_OFLOW|\
		 BRIDGE_ISR_UNSUPPORTED_XOP|BRIDGE_ISR_INVLD_ADDR|	\
		 BRIDGE_ISR_REQ_XTLK_ERR|BRIDGE_ISR_RESP_XTLK_ERR|	\
		 BRIDGE_ISR_BAD_XREQ_PKT|BRIDGE_ISR_BAD_XRESP_PKT|	\
		 BRIDGE_ISR_UNEXP_RESP)

#define BRIDGE_ISR_ERRORS		\
		(BRIDGE_ISR_LINK_ERROR|BRIDGE_ISR_PCIBUS_ERROR|		\
		 BRIDGE_ISR_XTALK_ERROR|BRIDGE_ISR_SSRAM_PERR|		\
		 BRIDGE_ISR_PMU_ESIZE_FAULT|PIC_ISR_INT_RAM_PERR)

/*
 * List of Errors which are fatal and kill the sytem
 */
#define BRIDGE_ISR_ERROR_FATAL		\
		((BRIDGE_ISR_XTALK_ERROR & ~BRIDGE_ISR_XREAD_REQ_TIMEOUT)|\
		 BRIDGE_ISR_PCI_SERR|BRIDGE_ISR_PCI_PARITY|		  \
		 PIC_ISR_PCIX_SERR|PIC_ISR_PCIX_AD_PARITY|		  \
		 PIC_ISR_PCIX_DA_PARITY|				  \
		 PIC_ISR_INT_RAM_PERR|PIC_ISR_PCIX_SPLIT_MSG_PE )

#define BRIDGE_ISR_ERROR_DUMP		\
		(BRIDGE_ISR_PCIBUS_ERROR|BRIDGE_ISR_PMU_ESIZE_FAULT|	\
		 BRIDGE_ISR_XTALK_ERROR|BRIDGE_ISR_SSRAM_PERR|		\
		 PIC_ISR_PCIX_ARB_ERR|PIC_ISR_INT_RAM_PERR)

/* Bridge interrupt enable register bits definition */
#define PIC_IMR_PCIX_SPLIT_MSG_PE	PIC_ISR_PCIX_SPLIT_MSG_PE
#define PIC_IMR_PCIX_SPLIT_EMSG		PIC_ISR_PCIX_SPLIT_EMSG
#define PIC_IMR_PCIX_SPLIT_TO		PIC_ISR_PCIX_SPLIT_TO
#define PIC_IMR_PCIX_UNEX_COMP		PIC_ISR_PCIX_UNEX_COMP
#define PIC_IMR_INT_RAM_PERR		PIC_ISR_INT_RAM_PERR
#define PIC_IMR_PCIX_ARB_ERR		PIC_ISR_PCIX_ARB_ERR
#define PIC_IMR_PCIX_REQ_TOUR		PIC_ISR_PCIX_REQ_TOUT
#define PIC_IMR_PCIX_TABORT		PIC_ISR_PCIX_TABORT
#define PIC_IMR_PCIX_PERR		PIC_ISR_PCIX_PERR
#define PIC_IMR_PCIX_SERR		PIC_ISR_PCIX_SERR
#define PIC_IMR_PCIX_MRETRY		PIC_ISR_PCIX_MRETRY
#define PIC_IMR_PCIX_MTOUT		PIC_ISR_PCIX_MTOUT
#define PIC_IMR_PCIX_DA_PARITY		PIC_ISR_PCIX_DA_PARITY
#define PIC_IMR_PCIX_AD_PARITY		PIC_ISR_PCIX_AD_PARITY
#define BRIDGE_IMR_UNEXP_RESP		BRIDGE_ISR_UNEXP_RESP
#define BRIDGE_IMR_PMU_ESIZE_FAULT	BRIDGE_ISR_PMU_ESIZE_FAULT
#define BRIDGE_IMR_BAD_XRESP_PKT	BRIDGE_ISR_BAD_XRESP_PKT
#define BRIDGE_IMR_BAD_XREQ_PKT		BRIDGE_ISR_BAD_XREQ_PKT
#define BRIDGE_IMR_RESP_XTLK_ERR	BRIDGE_ISR_RESP_XTLK_ERR
#define BRIDGE_IMR_REQ_XTLK_ERR		BRIDGE_ISR_REQ_XTLK_ERR
#define BRIDGE_IMR_INVLD_ADDR		BRIDGE_ISR_INVLD_ADDR
#define BRIDGE_IMR_UNSUPPORTED_XOP	BRIDGE_ISR_UNSUPPORTED_XOP
#define BRIDGE_IMR_XREQ_FIFO_OFLOW	BRIDGE_ISR_XREQ_FIFO_OFLOW
#define BRIDGE_IMR_LLP_REC_SNERR	BRIDGE_ISR_LLP_REC_SNERR
#define BRIDGE_IMR_LLP_REC_CBERR	BRIDGE_ISR_LLP_REC_CBERR
#define BRIDGE_IMR_LLP_RCTY		BRIDGE_ISR_LLP_RCTY
#define BRIDGE_IMR_LLP_TX_RETRY		BRIDGE_ISR_LLP_TX_RETRY
#define BRIDGE_IMR_LLP_TCTY		BRIDGE_ISR_LLP_TCTY
#define BRIDGE_IMR_SSRAM_PERR		BRIDGE_ISR_SSRAM_PERR
#define BRIDGE_IMR_PCI_ABORT		BRIDGE_ISR_PCI_ABORT
#define BRIDGE_IMR_PCI_PARITY		BRIDGE_ISR_PCI_PARITY
#define BRIDGE_IMR_PCI_SERR		BRIDGE_ISR_PCI_SERR
#define BRIDGE_IMR_PCI_PERR		BRIDGE_ISR_PCI_PERR
#define BRIDGE_IMR_PCI_MST_TIMEOUT	BRIDGE_ISR_PCI_MST_TIMEOUT
#define BRIDGE_IMR_GIO_MST_TIMEOUT	BRIDGE_ISR_GIO_MST_TIMEOUT
#define BRIDGE_IMR_PCI_RETRY_CNT	BRIDGE_ISR_PCI_RETRY_CNT
#define BRIDGE_IMR_XREAD_REQ_TIMEOUT	BRIDGE_ISR_XREAD_REQ_TIMEOUT
#define BRIDGE_IMR_GIO_B_ENBL_ERR	BRIDGE_ISR_GIO_B_ENBL_ERR
#define BRIDGE_IMR_INT_MSK		BRIDGE_ISR_INT_MSK
#define BRIDGE_IMR_INT(x)		BRIDGE_ISR_INT(x)

/* 
 * Bridge interrupt reset register bits definition.  Note, PIC can
 * reset indiviual error interrupts, BRIDGE & XBRIDGE can only do 
 * groups of them.
 */
#define PIC_IRR_PCIX_SPLIT_MSG_PE	PIC_ISR_PCIX_SPLIT_MSG_PE
#define PIC_IRR_PCIX_SPLIT_EMSG		PIC_ISR_PCIX_SPLIT_EMSG
#define PIC_IRR_PCIX_SPLIT_TO		PIC_ISR_PCIX_SPLIT_TO
#define PIC_IRR_PCIX_UNEX_COMP		PIC_ISR_PCIX_UNEX_COMP
#define PIC_IRR_INT_RAM_PERR		PIC_ISR_INT_RAM_PERR
#define PIC_IRR_PCIX_ARB_ERR		PIC_ISR_PCIX_ARB_ERR
#define PIC_IRR_PCIX_REQ_TOUT		PIC_ISR_PCIX_REQ_TOUT
#define PIC_IRR_PCIX_TABORT		PIC_ISR_PCIX_TABORT
#define PIC_IRR_PCIX_PERR		PIC_ISR_PCIX_PERR
#define PIC_IRR_PCIX_SERR		PIC_ISR_PCIX_SERR
#define PIC_IRR_PCIX_MRETRY		PIC_ISR_PCIX_MRETRY
#define PIC_IRR_PCIX_MTOUT		PIC_ISR_PCIX_MTOUT
#define PIC_IRR_PCIX_DA_PARITY		PIC_ISR_PCIX_DA_PARITY
#define PIC_IRR_PCIX_AD_PARITY		PIC_ISR_PCIX_AD_PARITY
#define PIC_IRR_PAGE_FAULT		BRIDGE_ISR_PAGE_FAULT
#define PIC_IRR_UNEXP_RESP		BRIDGE_ISR_UNEXP_RESP
#define PIC_IRR_BAD_XRESP_PKT		BRIDGE_ISR_BAD_XRESP_PKT
#define PIC_IRR_BAD_XREQ_PKT		BRIDGE_ISR_BAD_XREQ_PKT
#define PIC_IRR_RESP_XTLK_ERR		BRIDGE_ISR_RESP_XTLK_ERR
#define PIC_IRR_REQ_XTLK_ERR		BRIDGE_ISR_REQ_XTLK_ERR
#define PIC_IRR_INVLD_ADDR		BRIDGE_ISR_INVLD_ADDR
#define PIC_IRR_UNSUPPORTED_XOP		BRIDGE_ISR_UNSUPPORTED_XOP
#define PIC_IRR_XREQ_FIFO_OFLOW		BRIDGE_ISR_XREQ_FIFO_OFLOW
#define PIC_IRR_LLP_REC_SNERR		BRIDGE_ISR_LLP_REC_SNERR
#define PIC_IRR_LLP_REC_CBERR		BRIDGE_ISR_LLP_REC_CBERR
#define PIC_IRR_LLP_RCTY		BRIDGE_ISR_LLP_RCTY
#define PIC_IRR_LLP_TX_RETRY		BRIDGE_ISR_LLP_TX_RETRY
#define PIC_IRR_LLP_TCTY		BRIDGE_ISR_LLP_TCTY
#define PIC_IRR_PCI_ABORT		BRIDGE_ISR_PCI_ABORT
#define PIC_IRR_PCI_PARITY		BRIDGE_ISR_PCI_PARITY
#define PIC_IRR_PCI_SERR		BRIDGE_ISR_PCI_SERR
#define PIC_IRR_PCI_PERR		BRIDGE_ISR_PCI_PERR
#define PIC_IRR_PCI_MST_TIMEOUT		BRIDGE_ISR_PCI_MST_TIMEOUT
#define PIC_IRR_PCI_RETRY_CNT		BRIDGE_ISR_PCI_RETRY_CNT
#define PIC_IRR_XREAD_REQ_TIMEOUT	BRIDGE_ISR_XREAD_REQ_TIMEOUT
#define BRIDGE_IRR_MULTI_CLR		(0x1 << 6)
#define BRIDGE_IRR_CRP_GRP_CLR		(0x1 << 5)
#define BRIDGE_IRR_RESP_BUF_GRP_CLR	(0x1 << 4)
#define BRIDGE_IRR_REQ_DSP_GRP_CLR	(0x1 << 3)
#define BRIDGE_IRR_LLP_GRP_CLR		(0x1 << 2)
#define BRIDGE_IRR_SSRAM_GRP_CLR	(0x1 << 1)
#define BRIDGE_IRR_PCI_GRP_CLR		(0x1 << 0)
#define BRIDGE_IRR_GIO_GRP_CLR		(0x1 << 0)
#define BRIDGE_IRR_ALL_CLR		0x7f

#define BRIDGE_IRR_CRP_GRP		(BRIDGE_ISR_UNEXP_RESP | \
					 BRIDGE_ISR_XREQ_FIFO_OFLOW)
#define BRIDGE_IRR_RESP_BUF_GRP		(BRIDGE_ISR_BAD_XRESP_PKT | \
					 BRIDGE_ISR_RESP_XTLK_ERR | \
					 BRIDGE_ISR_XREAD_REQ_TIMEOUT)
#define BRIDGE_IRR_REQ_DSP_GRP		(BRIDGE_ISR_UNSUPPORTED_XOP | \
					 BRIDGE_ISR_BAD_XREQ_PKT | \
					 BRIDGE_ISR_REQ_XTLK_ERR | \
					 BRIDGE_ISR_INVLD_ADDR)
#define BRIDGE_IRR_LLP_GRP		(BRIDGE_ISR_LLP_REC_SNERR | \
					 BRIDGE_ISR_LLP_REC_CBERR | \
					 BRIDGE_ISR_LLP_RCTY | \
					 BRIDGE_ISR_LLP_TX_RETRY | \
					 BRIDGE_ISR_LLP_TCTY)
#define BRIDGE_IRR_SSRAM_GRP		(BRIDGE_ISR_SSRAM_PERR | \
					 BRIDGE_ISR_PMU_ESIZE_FAULT)
#define BRIDGE_IRR_PCI_GRP		(BRIDGE_ISR_PCI_ABORT | \
					 BRIDGE_ISR_PCI_PARITY | \
					 BRIDGE_ISR_PCI_SERR | \
					 BRIDGE_ISR_PCI_PERR | \
					 BRIDGE_ISR_PCI_MST_TIMEOUT | \
					 BRIDGE_ISR_PCI_RETRY_CNT)

#define BRIDGE_IRR_GIO_GRP		(BRIDGE_ISR_GIO_B_ENBL_ERR | \
					 BRIDGE_ISR_GIO_MST_TIMEOUT)

#define PIC_IRR_RAM_GRP			PIC_ISR_INT_RAM_PERR

#define PIC_PCIX_GRP_CLR		(PIC_IRR_PCIX_AD_PARITY | \
					 PIC_IRR_PCIX_DA_PARITY | \
					 PIC_IRR_PCIX_MTOUT | \
					 PIC_IRR_PCIX_MRETRY | \
					 PIC_IRR_PCIX_SERR | \
					 PIC_IRR_PCIX_PERR | \
					 PIC_IRR_PCIX_TABORT | \
					 PIC_ISR_PCIX_REQ_TOUT | \
					 PIC_ISR_PCIX_UNEX_COMP | \
					 PIC_ISR_PCIX_SPLIT_TO | \
					 PIC_ISR_PCIX_SPLIT_EMSG | \
					 PIC_ISR_PCIX_SPLIT_MSG_PE)

/* Bridge INT_DEV register bits definition */
#define BRIDGE_INT_DEV_SHFT(n)		((n)*3)
#define BRIDGE_INT_DEV_MASK(n)		(0x7 << BRIDGE_INT_DEV_SHFT(n))
#define BRIDGE_INT_DEV_SET(_dev, _line) (_dev << BRIDGE_INT_DEV_SHFT(_line))	

/* Bridge interrupt(x) register bits definition */
#define BRIDGE_INT_ADDR_HOST		0x0003FF00
#define BRIDGE_INT_ADDR_FLD		0x000000FF

/* PIC interrupt(x) register bits definition */
#define PIC_INT_ADDR_FLD                0x00FF000000000000
#define PIC_INT_ADDR_HOST               0x0000FFFFFFFFFFFF

#define BRIDGE_TMO_PCI_RETRY_HLD_MASK	0x1f0000
#define BRIDGE_TMO_GIO_TIMEOUT_MASK	0x001000
#define BRIDGE_TMO_PCI_RETRY_CNT_MASK	0x0003ff

#define BRIDGE_TMO_PCI_RETRY_CNT_MAX	0x3ff

/* Bridge device(x) register bits definition */
#define BRIDGE_DEV_ERR_LOCK_EN		(1ull << 28)
#define BRIDGE_DEV_PAGE_CHK_DIS		(1ull << 27)
#define BRIDGE_DEV_FORCE_PCI_PAR	(1ull << 26)
#define BRIDGE_DEV_VIRTUAL_EN		(1ull << 25)
#define BRIDGE_DEV_PMU_WRGA_EN		(1ull << 24)
#define BRIDGE_DEV_DIR_WRGA_EN		(1ull << 23)
#define BRIDGE_DEV_DEV_SIZE		(1ull << 22)
#define BRIDGE_DEV_RT			(1ull << 21)
#define BRIDGE_DEV_SWAP_PMU		(1ull << 20)
#define BRIDGE_DEV_SWAP_DIR		(1ull << 19)
#define BRIDGE_DEV_PREF			(1ull << 18)
#define BRIDGE_DEV_PRECISE		(1ull << 17)
#define BRIDGE_DEV_COH			(1ull << 16)
#define BRIDGE_DEV_BARRIER		(1ull << 15)
#define BRIDGE_DEV_GBR			(1ull << 14)
#define BRIDGE_DEV_DEV_SWAP		(1ull << 13)
#define BRIDGE_DEV_DEV_IO_MEM		(1ull << 12)
#define BRIDGE_DEV_OFF_MASK		0x00000fff
#define BRIDGE_DEV_OFF_ADDR_SHFT	20

#define XBRIDGE_DEV_PMU_BITS		BRIDGE_DEV_PMU_WRGA_EN
#define BRIDGE_DEV_PMU_BITS		(BRIDGE_DEV_PMU_WRGA_EN		| \
					 BRIDGE_DEV_SWAP_PMU)
#define BRIDGE_DEV_D32_BITS		(BRIDGE_DEV_DIR_WRGA_EN		| \
					 BRIDGE_DEV_SWAP_DIR		| \
					 BRIDGE_DEV_PREF		| \
					 BRIDGE_DEV_PRECISE		| \
					 BRIDGE_DEV_COH			| \
					 BRIDGE_DEV_BARRIER)
#define XBRIDGE_DEV_D64_BITS		(BRIDGE_DEV_DIR_WRGA_EN		| \
					 BRIDGE_DEV_COH			| \
					 BRIDGE_DEV_BARRIER)
#define BRIDGE_DEV_D64_BITS		(BRIDGE_DEV_DIR_WRGA_EN		| \
					 BRIDGE_DEV_SWAP_DIR		| \
					 BRIDGE_DEV_COH			| \
					 BRIDGE_DEV_BARRIER)

/* Bridge Error Upper register bit field definition */
#define BRIDGE_ERRUPPR_DEVMASTER	(0x1 << 20)	/* Device was master */
#define BRIDGE_ERRUPPR_PCIVDEV		(0x1 << 19)	/* Virtual Req value */
#define BRIDGE_ERRUPPR_DEVNUM_SHFT	(16)
#define BRIDGE_ERRUPPR_DEVNUM_MASK	(0x7 << BRIDGE_ERRUPPR_DEVNUM_SHFT)
#define BRIDGE_ERRUPPR_DEVICE(err)	(((err) >> BRIDGE_ERRUPPR_DEVNUM_SHFT) & 0x7)
#define BRIDGE_ERRUPPR_ADDRMASK		(0xFFFF)

/* Bridge interrupt mode register bits definition */
#define BRIDGE_INTMODE_CLR_PKT_EN(x)	(0x1 << (x))

/* this should be written to the xbow's link_control(x) register */
#define BRIDGE_CREDIT	3

/* RRB assignment register */
#define	BRIDGE_RRB_EN	0x8	/* after shifting down */
#define	BRIDGE_RRB_DEV	0x7	/* after shifting down */
#define	BRIDGE_RRB_VDEV	0x4	/* after shifting down, 2 virtual channels */
#define	BRIDGE_RRB_PDEV	0x3	/* after shifting down, 8 devices */

#define	PIC_RRB_EN	0x8	/* after shifting down */
#define	PIC_RRB_DEV	0x7	/* after shifting down */
#define	PIC_RRB_VDEV	0x6	/* after shifting down, 4 virtual channels */
#define	PIC_RRB_PDEV	0x1	/* after shifting down, 4 devices */

/* RRB status register */
#define	BRIDGE_RRB_VALID(r)	(0x00010000<<(r))
#define	BRIDGE_RRB_INUSE(r)	(0x00000001<<(r))

/* RRB clear register */
#define	BRIDGE_RRB_CLEAR(r)	(0x00000001<<(r))

/* Defines for the virtual channels so we don't hardcode 0-3 within code */
#define VCHAN0	0	/* virtual channel 0 (ie. the "normal" channel) */
#define VCHAN1	1	/* virtual channel 1 */
#define VCHAN2	2	/* virtual channel 2 - PIC only */
#define VCHAN3	3	/* virtual channel 3 - PIC only */

/* PIC: PCI-X Read Buffer Attribute Register (RBAR) */
#define NUM_RBAR 16	/* number of RBAR registers */

/* xbox system controller declarations */
#define XBOX_BRIDGE_WID         8
#define FLASH_PROM1_BASE        0xE00000 /* To read the xbox sysctlr status */
#define XBOX_RPS_EXISTS		1 << 6	 /* RPS bit in status register */
#define XBOX_RPS_FAIL		1 << 4	 /* RPS status bit in register */

/* ========================================================================
 */
/*
 * Macros for Xtalk to Bridge bus (PCI/GIO) PIO
 * refer to section 4.2.1 of Bridge Spec for xtalk to PCI/GIO PIO mappings
 */
/* XTALK addresses that map into Bridge Bus addr space */
#define BRIDGE_PIO32_XTALK_ALIAS_BASE	0x000040000000L
#define BRIDGE_PIO32_XTALK_ALIAS_LIMIT	0x00007FFFFFFFL
#define BRIDGE_PIO64_XTALK_ALIAS_BASE	0x000080000000L
#define BRIDGE_PIO64_XTALK_ALIAS_LIMIT	0x0000BFFFFFFFL
#define BRIDGE_PCIIO_XTALK_ALIAS_BASE	0x000100000000L
#define BRIDGE_PCIIO_XTALK_ALIAS_LIMIT	0x0001FFFFFFFFL

/* Ranges of PCI bus space that can be accessed via PIO from xtalk */
#define BRIDGE_MIN_PIO_ADDR_MEM		0x00000000	/* 1G PCI memory space */
#define BRIDGE_MAX_PIO_ADDR_MEM		0x3fffffff
#define BRIDGE_MIN_PIO_ADDR_IO		0x00000000	/* 4G PCI IO space */
#define BRIDGE_MAX_PIO_ADDR_IO		0xffffffff

/* XTALK addresses that map into PCI addresses */
#define BRIDGE_PCI_MEM32_BASE		BRIDGE_PIO32_XTALK_ALIAS_BASE
#define BRIDGE_PCI_MEM32_LIMIT		BRIDGE_PIO32_XTALK_ALIAS_LIMIT
#define BRIDGE_PCI_MEM64_BASE		BRIDGE_PIO64_XTALK_ALIAS_BASE
#define BRIDGE_PCI_MEM64_LIMIT		BRIDGE_PIO64_XTALK_ALIAS_LIMIT
#define BRIDGE_PCI_IO_BASE		BRIDGE_PCIIO_XTALK_ALIAS_BASE
#define BRIDGE_PCI_IO_LIMIT		BRIDGE_PCIIO_XTALK_ALIAS_LIMIT

/*
 * Macros for Xtalk to Bridge bus (PCI) PIO
 * refer to section 5.2.1 Figure 4 of the "PCI Interface Chip (PIC) Volume II
 * Programmer's Reference" (Revision 0.8 as of this writing).
 *
 * These are PIC bridge specific.  A separate set of macros was defined
 * because PIC deviates from Bridge/Xbridge by not supporting a big-window
 * alias for PCI I/O space, and also redefines XTALK addresses
 * 0x0000C0000000L and 0x000100000000L to be PCI MEM aliases for the second
 * bus.
 */

/* XTALK addresses that map into PIC Bridge Bus addr space */
#define PICBRIDGE0_PIO32_XTALK_ALIAS_BASE	0x000040000000L
#define PICBRIDGE0_PIO32_XTALK_ALIAS_LIMIT	0x00007FFFFFFFL
#define PICBRIDGE0_PIO64_XTALK_ALIAS_BASE	0x000080000000L
#define PICBRIDGE0_PIO64_XTALK_ALIAS_LIMIT	0x0000BFFFFFFFL
#define PICBRIDGE1_PIO32_XTALK_ALIAS_BASE	0x0000C0000000L
#define PICBRIDGE1_PIO32_XTALK_ALIAS_LIMIT	0x0000FFFFFFFFL
#define PICBRIDGE1_PIO64_XTALK_ALIAS_BASE	0x000100000000L
#define PICBRIDGE1_PIO64_XTALK_ALIAS_LIMIT	0x00013FFFFFFFL

/* XTALK addresses that map into PCI addresses */
#define PICBRIDGE0_PCI_MEM32_BASE	PICBRIDGE0_PIO32_XTALK_ALIAS_BASE
#define PICBRIDGE0_PCI_MEM32_LIMIT	PICBRIDGE0_PIO32_XTALK_ALIAS_LIMIT
#define PICBRIDGE0_PCI_MEM64_BASE	PICBRIDGE0_PIO64_XTALK_ALIAS_BASE
#define PICBRIDGE0_PCI_MEM64_LIMIT	PICBRIDGE0_PIO64_XTALK_ALIAS_LIMIT
#define PICBRIDGE1_PCI_MEM32_BASE	PICBRIDGE1_PIO32_XTALK_ALIAS_BASE
#define PICBRIDGE1_PCI_MEM32_LIMIT	PICBRIDGE1_PIO32_XTALK_ALIAS_LIMIT
#define PICBRIDGE1_PCI_MEM64_BASE	PICBRIDGE1_PIO64_XTALK_ALIAS_BASE
#define PICBRIDGE1_PCI_MEM64_LIMIT	PICBRIDGE1_PIO64_XTALK_ALIAS_LIMIT

/*
 * Macros for Bridge bus (PCI/GIO) to Xtalk DMA
 */
/* Bridge Bus DMA addresses */
#define BRIDGE_LOCAL_BASE		0
#define BRIDGE_DMA_MAPPED_BASE		0x40000000
#define BRIDGE_DMA_MAPPED_SIZE		0x40000000	/* 1G Bytes */
#define BRIDGE_DMA_DIRECT_BASE		0x80000000
#define BRIDGE_DMA_DIRECT_SIZE		0x80000000	/* 2G Bytes */

#define PCI32_LOCAL_BASE		BRIDGE_LOCAL_BASE

/* PCI addresses of regions decoded by Bridge for DMA */
#define PCI32_MAPPED_BASE		BRIDGE_DMA_MAPPED_BASE
#define PCI32_DIRECT_BASE		BRIDGE_DMA_DIRECT_BASE

#ifndef __ASSEMBLY__

#define IS_PCI32_LOCAL(x)	((uint64_t)(x) < PCI32_MAPPED_BASE)
#define IS_PCI32_MAPPED(x)	((uint64_t)(x) < PCI32_DIRECT_BASE && \
					(uint64_t)(x) >= PCI32_MAPPED_BASE)
#define IS_PCI32_DIRECT(x)	((uint64_t)(x) >= PCI32_MAPPED_BASE)
#define IS_PCI64(x)		((uint64_t)(x) >= PCI64_BASE)
#endif				/* __ASSEMBLY__ */

/*
 * The GIO address space.
 */
/* Xtalk to GIO PIO */
#define BRIDGE_GIO_MEM32_BASE		BRIDGE_PIO32_XTALK_ALIAS_BASE
#define BRIDGE_GIO_MEM32_LIMIT		BRIDGE_PIO32_XTALK_ALIAS_LIMIT

#define GIO_LOCAL_BASE			BRIDGE_LOCAL_BASE

/* GIO addresses of regions decoded by Bridge for DMA */
#define GIO_MAPPED_BASE			BRIDGE_DMA_MAPPED_BASE
#define GIO_DIRECT_BASE			BRIDGE_DMA_DIRECT_BASE

#ifndef __ASSEMBLY__

#define IS_GIO_LOCAL(x)		((uint64_t)(x) < GIO_MAPPED_BASE)
#define IS_GIO_MAPPED(x)	((uint64_t)(x) < GIO_DIRECT_BASE && \
					(uint64_t)(x) >= GIO_MAPPED_BASE)
#define IS_GIO_DIRECT(x)	((uint64_t)(x) >= GIO_MAPPED_BASE)
#endif				/* __ASSEMBLY__ */

/* PCI to xtalk mapping */

/* given a DIR_OFF value and a pci/gio 32 bits direct address, determine
 * which xtalk address is accessed
 */
#define BRIDGE_DIRECT_32_SEG_SIZE	BRIDGE_DMA_DIRECT_SIZE
#define BRIDGE_DIRECT_32_TO_XTALK(dir_off,adr)		\
	((dir_off) * BRIDGE_DIRECT_32_SEG_SIZE +	\
		((adr) & (BRIDGE_DIRECT_32_SEG_SIZE - 1)) + PHYS_RAMBASE)

/* 64-bit address attribute masks */
#define PCI64_ATTR_TARG_MASK	0xf000000000000000
#define PCI64_ATTR_TARG_SHFT	60
#define PCI64_ATTR_PREF		(1ull << 59)
#define PCI64_ATTR_PREC		(1ull << 58)
#define PCI64_ATTR_VIRTUAL	(1ull << 57)
#define PCI64_ATTR_BAR		(1ull << 56)
#define PCI64_ATTR_SWAP		(1ull << 55)
#define PCI64_ATTR_RMF_MASK	0x00ff000000000000
#define PCI64_ATTR_RMF_SHFT	48

#ifndef __ASSEMBLY__
/* Address translation entry for mapped pci32 accesses */
typedef union ate_u {
    uint64_t		    ent;
    struct xb_ate_s {					/* xbridge */
	uint64_t		:16;
	uint64_t		addr:36;
	uint64_t		targ:4;
	uint64_t		reserved:2;
        uint64_t		swap:1;
	uint64_t		barrier:1;
	uint64_t		prefetch:1;
	uint64_t		precise:1;
	uint64_t		coherent:1;
	uint64_t		valid:1;
    } xb_field;
    struct ate_s {					/* bridge */
	uint64_t		rmf:16;
	uint64_t		addr:36;
	uint64_t		targ:4;
	uint64_t		reserved:3;
	uint64_t		barrier:1;
	uint64_t		prefetch:1;
	uint64_t		precise:1;
	uint64_t		coherent:1;
	uint64_t		valid:1;
    } field;
} ate_t;
#endif				/* __ASSEMBLY__ */

#define ATE_V		(1 << 0)
#define ATE_CO		(1 << 1)
#define ATE_PREC	(1 << 2)
#define ATE_PREF	(1 << 3)
#define ATE_BAR		(1 << 4)
#define ATE_SWAP        (1 << 5)

#define ATE_PFNSHIFT		12
#define ATE_TIDSHIFT		8
#define ATE_RMFSHIFT		48

#define mkate(xaddr, xid, attr) ((xaddr) & 0x0000fffffffff000ULL) | \
				((xid)<<ATE_TIDSHIFT) | \
				(attr)

/*
 * for xbridge, bit 29 of the pci address is the swap bit */
#define ATE_SWAPSHIFT		29
#define ATE_SWAP_ON(x)		((x) |= (1 << ATE_SWAPSHIFT))
#define ATE_SWAP_OFF(x)		((x) &= ~(1 << ATE_SWAPSHIFT))

/* extern declarations */

#ifndef __ASSEMBLY__

/* ========================================================================
 */

#ifdef	MACROFIELD_LINE
/*
 * This table forms a relation between the byte offset macros normally
 * used for ASM coding and the calculated byte offsets of the fields
 * in the C structure.
 *
 * See bridge_check.c and bridge_html.c for further details.
 */
#ifndef MACROFIELD_LINE_BITFIELD
#define MACROFIELD_LINE_BITFIELD(m)	/* ignored */
#endif

struct macrofield_s	bridge_macrofield[] =
{

    MACROFIELD_LINE(BRIDGE_WID_ID, b_wid_id)
    MACROFIELD_LINE_BITFIELD(WIDGET_REV_NUM)
    MACROFIELD_LINE_BITFIELD(WIDGET_PART_NUM)
    MACROFIELD_LINE_BITFIELD(WIDGET_MFG_NUM)
    MACROFIELD_LINE(BRIDGE_WID_STAT, b_wid_stat)
    MACROFIELD_LINE_BITFIELD(BRIDGE_STAT_LLP_REC_CNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_STAT_LLP_TX_CNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_STAT_FLASH_SELECT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_STAT_PCI_GIO_N)
    MACROFIELD_LINE_BITFIELD(BRIDGE_STAT_PENDING)
    MACROFIELD_LINE(BRIDGE_WID_ERR_UPPER, b_wid_err_upper)
    MACROFIELD_LINE(BRIDGE_WID_ERR_LOWER, b_wid_err_lower)
    MACROFIELD_LINE(BRIDGE_WID_CONTROL, b_wid_control)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_FLASH_WR_EN)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_EN_CLK50)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_EN_CLK40)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_EN_CLK33)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_RST_MASK)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_IO_SWAP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_MEM_SWAP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_PAGE_SIZE)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_SS_PAR_BAD)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_SS_PAR_EN)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_SSRAM_SIZE_MASK)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_F_BAD_PKT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_LLP_XBAR_CRD_MASK)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_CLR_RLLP_CNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_CLR_TLLP_CNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_SYS_END)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_MAX_TRANS_MASK)
    MACROFIELD_LINE_BITFIELD(BRIDGE_CTRL_WIDGET_ID_MASK)
    MACROFIELD_LINE(BRIDGE_WID_REQ_TIMEOUT, b_wid_req_timeout)
    MACROFIELD_LINE(BRIDGE_WID_INT_UPPER, b_wid_int_upper)
    MACROFIELD_LINE_BITFIELD(WIDGET_INT_VECTOR)
    MACROFIELD_LINE_BITFIELD(WIDGET_TARGET_ID)
    MACROFIELD_LINE_BITFIELD(WIDGET_UPP_ADDR)
    MACROFIELD_LINE(BRIDGE_WID_INT_LOWER, b_wid_int_lower)
    MACROFIELD_LINE(BRIDGE_WID_ERR_CMDWORD, b_wid_err_cmdword)
    MACROFIELD_LINE_BITFIELD(WIDGET_DIDN)
    MACROFIELD_LINE_BITFIELD(WIDGET_SIDN)
    MACROFIELD_LINE_BITFIELD(WIDGET_PACTYP)
    MACROFIELD_LINE_BITFIELD(WIDGET_TNUM)
    MACROFIELD_LINE_BITFIELD(WIDGET_COHERENT)
    MACROFIELD_LINE_BITFIELD(WIDGET_DS)
    MACROFIELD_LINE_BITFIELD(WIDGET_GBR)
    MACROFIELD_LINE_BITFIELD(WIDGET_VBPM)
    MACROFIELD_LINE_BITFIELD(WIDGET_ERROR)
    MACROFIELD_LINE_BITFIELD(WIDGET_BARRIER)
    MACROFIELD_LINE(BRIDGE_WID_LLP, b_wid_llp)
    MACROFIELD_LINE_BITFIELD(WIDGET_LLP_MAXRETRY)
    MACROFIELD_LINE_BITFIELD(WIDGET_LLP_NULLTIMEOUT)
    MACROFIELD_LINE_BITFIELD(WIDGET_LLP_MAXBURST)
    MACROFIELD_LINE(BRIDGE_WID_TFLUSH, b_wid_tflush)
    MACROFIELD_LINE(BRIDGE_WID_AUX_ERR, b_wid_aux_err)
    MACROFIELD_LINE(BRIDGE_WID_RESP_UPPER, b_wid_resp_upper)
    MACROFIELD_LINE(BRIDGE_WID_RESP_LOWER, b_wid_resp_lower)
    MACROFIELD_LINE(BRIDGE_WID_TST_PIN_CTRL, b_wid_tst_pin_ctrl)
    MACROFIELD_LINE(BRIDGE_DIR_MAP, b_dir_map)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DIRMAP_W_ID)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DIRMAP_RMF_64)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DIRMAP_ADD512)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DIRMAP_OFF)
    MACROFIELD_LINE(BRIDGE_RAM_PERR, b_ram_perr)
    MACROFIELD_LINE(BRIDGE_ARB, b_arb)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_REQ_WAIT_TICK_MASK)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_REQ_WAIT_EN_MASK)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_FREEZE_GNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_HPRI_RING_B2)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_HPRI_RING_B1)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_HPRI_RING_B0)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_LPRI_RING_B2)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_LPRI_RING_B1)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ARB_LPRI_RING_B0)
    MACROFIELD_LINE(BRIDGE_NIC, b_nic)
    MACROFIELD_LINE(BRIDGE_PCI_BUS_TIMEOUT, b_pci_bus_timeout)
    MACROFIELD_LINE(BRIDGE_PCI_CFG, b_pci_cfg)
    MACROFIELD_LINE(BRIDGE_PCI_ERR_UPPER, b_pci_err_upper)
    MACROFIELD_LINE(BRIDGE_PCI_ERR_LOWER, b_pci_err_lower)
    MACROFIELD_LINE(BRIDGE_INT_STATUS, b_int_status)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_MULTI_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PMU_ESIZE_FAULT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_UNEXP_RESP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_BAD_XRESP_PKT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_BAD_XREQ_PKT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_RESP_XTLK_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_REQ_XTLK_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_INVLD_ADDR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_UNSUPPORTED_XOP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_XREQ_FIFO_OFLOW)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_LLP_REC_SNERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_LLP_REC_CBERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_LLP_RCTY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_LLP_TX_RETRY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_LLP_TCTY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_SSRAM_PERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PCI_ABORT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PCI_PARITY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PCI_SERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PCI_PERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PCI_MST_TIMEOUT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_PCI_RETRY_CNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_XREAD_REQ_TIMEOUT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_GIO_B_ENBL_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_ISR_INT_MSK)
    MACROFIELD_LINE(BRIDGE_INT_ENABLE, b_int_enable)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_UNEXP_RESP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PMU_ESIZE_FAULT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_BAD_XRESP_PKT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_BAD_XREQ_PKT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_RESP_XTLK_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_REQ_XTLK_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_INVLD_ADDR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_UNSUPPORTED_XOP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_XREQ_FIFO_OFLOW)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_LLP_REC_SNERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_LLP_REC_CBERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_LLP_RCTY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_LLP_TX_RETRY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_LLP_TCTY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_SSRAM_PERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PCI_ABORT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PCI_PARITY)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PCI_SERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PCI_PERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PCI_MST_TIMEOUT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_PCI_RETRY_CNT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_XREAD_REQ_TIMEOUT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_GIO_B_ENBL_ERR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IMR_INT_MSK)
    MACROFIELD_LINE(BRIDGE_INT_RST_STAT, b_int_rst_stat)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_ALL_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_MULTI_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_CRP_GRP_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_RESP_BUF_GRP_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_REQ_DSP_GRP_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_LLP_GRP_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_SSRAM_GRP_CLR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_IRR_PCI_GRP_CLR)
    MACROFIELD_LINE(BRIDGE_INT_MODE, b_int_mode)
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(7))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(6))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(5))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(4))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(3))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(2))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(1))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INTMODE_CLR_PKT_EN(0))
    MACROFIELD_LINE(BRIDGE_INT_DEVICE, b_int_device)
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(7))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(6))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(5))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(4))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(3))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(2))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(1))
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_DEV_MASK(0))
    MACROFIELD_LINE(BRIDGE_INT_HOST_ERR, b_int_host_err)
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_ADDR_HOST)
    MACROFIELD_LINE_BITFIELD(BRIDGE_INT_ADDR_FLD)
    MACROFIELD_LINE(BRIDGE_INT_ADDR0, b_int_addr[0].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(0), b_int_addr[0].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(1), b_int_addr[1].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(2), b_int_addr[2].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(3), b_int_addr[3].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(4), b_int_addr[4].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(5), b_int_addr[5].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(6), b_int_addr[6].addr)
    MACROFIELD_LINE(BRIDGE_INT_ADDR(7), b_int_addr[7].addr)
    MACROFIELD_LINE(BRIDGE_DEVICE0, b_device[0].reg)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_ERR_LOCK_EN)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_PAGE_CHK_DIS)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_FORCE_PCI_PAR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_VIRTUAL_EN)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_PMU_WRGA_EN)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_DIR_WRGA_EN)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_DEV_SIZE)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_RT)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_SWAP_PMU)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_SWAP_DIR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_PREF)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_PRECISE)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_COH)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_BARRIER)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_GBR)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_DEV_SWAP)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_DEV_IO_MEM)
    MACROFIELD_LINE_BITFIELD(BRIDGE_DEV_OFF_MASK)
    MACROFIELD_LINE(BRIDGE_DEVICE(0), b_device[0].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(1), b_device[1].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(2), b_device[2].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(3), b_device[3].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(4), b_device[4].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(5), b_device[5].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(6), b_device[6].reg)
    MACROFIELD_LINE(BRIDGE_DEVICE(7), b_device[7].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF0, b_wr_req_buf[0].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(0), b_wr_req_buf[0].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(1), b_wr_req_buf[1].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(2), b_wr_req_buf[2].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(3), b_wr_req_buf[3].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(4), b_wr_req_buf[4].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(5), b_wr_req_buf[5].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(6), b_wr_req_buf[6].reg)
    MACROFIELD_LINE(BRIDGE_WR_REQ_BUF(7), b_wr_req_buf[7].reg)
    MACROFIELD_LINE(BRIDGE_EVEN_RESP, b_even_resp)
    MACROFIELD_LINE(BRIDGE_ODD_RESP, b_odd_resp)
    MACROFIELD_LINE(BRIDGE_RESP_STATUS, b_resp_status)
    MACROFIELD_LINE(BRIDGE_RESP_CLEAR, b_resp_clear)
    MACROFIELD_LINE(BRIDGE_ATE_RAM, b_int_ate_ram)
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV0, b_type0_cfg_dev[0])

    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(0), b_type0_cfg_dev[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,0), b_type0_cfg_dev[0].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,1), b_type0_cfg_dev[0].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,2), b_type0_cfg_dev[0].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,3), b_type0_cfg_dev[0].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,4), b_type0_cfg_dev[0].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,5), b_type0_cfg_dev[0].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,6), b_type0_cfg_dev[0].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(0,7), b_type0_cfg_dev[0].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(1), b_type0_cfg_dev[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,0), b_type0_cfg_dev[1].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,1), b_type0_cfg_dev[1].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,2), b_type0_cfg_dev[1].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,3), b_type0_cfg_dev[1].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,4), b_type0_cfg_dev[1].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,5), b_type0_cfg_dev[1].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,6), b_type0_cfg_dev[1].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(1,7), b_type0_cfg_dev[1].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(2), b_type0_cfg_dev[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,0), b_type0_cfg_dev[2].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,1), b_type0_cfg_dev[2].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,2), b_type0_cfg_dev[2].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,3), b_type0_cfg_dev[2].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,4), b_type0_cfg_dev[2].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,5), b_type0_cfg_dev[2].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,6), b_type0_cfg_dev[2].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(2,7), b_type0_cfg_dev[2].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(3), b_type0_cfg_dev[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,0), b_type0_cfg_dev[3].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,1), b_type0_cfg_dev[3].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,2), b_type0_cfg_dev[3].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,3), b_type0_cfg_dev[3].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,4), b_type0_cfg_dev[3].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,5), b_type0_cfg_dev[3].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,6), b_type0_cfg_dev[3].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(3,7), b_type0_cfg_dev[3].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(4), b_type0_cfg_dev[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,0), b_type0_cfg_dev[4].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,1), b_type0_cfg_dev[4].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,2), b_type0_cfg_dev[4].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,3), b_type0_cfg_dev[4].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,4), b_type0_cfg_dev[4].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,5), b_type0_cfg_dev[4].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,6), b_type0_cfg_dev[4].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(4,7), b_type0_cfg_dev[4].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(5), b_type0_cfg_dev[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,0), b_type0_cfg_dev[5].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,1), b_type0_cfg_dev[5].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,2), b_type0_cfg_dev[5].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,3), b_type0_cfg_dev[5].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,4), b_type0_cfg_dev[5].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,5), b_type0_cfg_dev[5].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,6), b_type0_cfg_dev[5].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(5,7), b_type0_cfg_dev[5].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(6), b_type0_cfg_dev[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,0), b_type0_cfg_dev[6].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,1), b_type0_cfg_dev[6].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,2), b_type0_cfg_dev[6].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,3), b_type0_cfg_dev[6].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,4), b_type0_cfg_dev[6].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,5), b_type0_cfg_dev[6].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,6), b_type0_cfg_dev[6].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(6,7), b_type0_cfg_dev[6].f[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEV(7), b_type0_cfg_dev[7])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,0), b_type0_cfg_dev[7].f[0])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,1), b_type0_cfg_dev[7].f[1])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,2), b_type0_cfg_dev[7].f[2])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,3), b_type0_cfg_dev[7].f[3])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,4), b_type0_cfg_dev[7].f[4])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,5), b_type0_cfg_dev[7].f[5])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,6), b_type0_cfg_dev[7].f[6])
    MACROFIELD_LINE(BRIDGE_TYPE0_CFG_DEVF(7,7), b_type0_cfg_dev[7].f[7])

    MACROFIELD_LINE(BRIDGE_TYPE1_CFG, b_type1_cfg)
    MACROFIELD_LINE(BRIDGE_PCI_IACK, b_pci_iack)
    MACROFIELD_LINE(BRIDGE_EXT_SSRAM, b_ext_ate_ram)
    MACROFIELD_LINE(BRIDGE_DEVIO0, b_devio(0))
    MACROFIELD_LINE(BRIDGE_DEVIO(0), b_devio(0))
    MACROFIELD_LINE(BRIDGE_DEVIO(1), b_devio(1))
    MACROFIELD_LINE(BRIDGE_DEVIO(2), b_devio(2))
    MACROFIELD_LINE(BRIDGE_DEVIO(3), b_devio(3))
    MACROFIELD_LINE(BRIDGE_DEVIO(4), b_devio(4))
    MACROFIELD_LINE(BRIDGE_DEVIO(5), b_devio(5))
    MACROFIELD_LINE(BRIDGE_DEVIO(6), b_devio(6))
    MACROFIELD_LINE(BRIDGE_DEVIO(7), b_devio(7))
    MACROFIELD_LINE(BRIDGE_EXTERNAL_FLASH, b_external_flash)
};
#endif

#ifdef __cplusplus
};
#endif
#endif				/* C or C++ */ 

#endif                          /* _ASM_SN_PCI_BRIDGE_H */
