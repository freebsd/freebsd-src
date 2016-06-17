/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All Rights Reserved.
 */
#ifndef _ASM_SN_SN_XTALK_XBOW_H
#define _ASM_SN_SN_XTALK_XBOW_H

/*
 * xbow.h - header file for crossbow chip and xbow section of xbridge
 */

#include <linux/config.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/xtalk/xswitch.h>
#ifndef __ASSEMBLY__
#include <asm/sn/xtalk/xbow_info.h>
#endif


#define	XBOW_DRV_PREFIX	"xbow_"

/* The crossbow chip supports 8 8/16 bits I/O ports, numbered 0x8 through 0xf.
 * It also implements the widget 0 address space and register set.
 */
#define XBOW_PORT_0	0x0
#define XBOW_PORT_8	0x8
#define XBOW_PORT_9	0x9
#define XBOW_PORT_A	0xa
#define XBOW_PORT_B	0xb
#define XBOW_PORT_C	0xc
#define XBOW_PORT_D	0xd
#define XBOW_PORT_E	0xe
#define XBOW_PORT_F	0xf

#define MAX_XBOW_PORTS	8	/* number of ports on xbow chip */
#define BASE_XBOW_PORT	XBOW_PORT_8	/* Lowest external port */
#define MAX_PORT_NUM	0x10	/* maximum port number + 1 */
#define XBOW_WIDGET_ID	0	/* xbow is itself widget 0 */

#define XBOW_HUBLINK_LOW  0xa
#define XBOW_HUBLINK_HIGH 0xb

#define XBOW_PEER_LINK(link) (link == XBOW_HUBLINK_LOW) ? \
                                XBOW_HUBLINK_HIGH : XBOW_HUBLINK_LOW


#define	XBOW_CREDIT	4

#define MAX_XBOW_NAME 	16

#ifndef __ASSEMBLY__
typedef uint32_t      xbowreg_t;

#define XBOWCONST	(xbowreg_t)

/* Generic xbow register, given base and offset */
#define XBOW_REG_PTR(base, offset) ((volatile xbowreg_t*) \
	((__psunsigned_t)(base) + (__psunsigned_t)(offset)))

/* Register set for each xbow link */
typedef volatile struct xb_linkregs_s {
#ifdef LITTLE_ENDIAN
/* 
 * we access these through synergy unswizzled space, so the address
 * gets twiddled (i.e. references to 0x4 actually go to 0x0 and vv.)
 * That's why we put the register first and filler second.
 */
    xbowreg_t               link_ibf;
    xbowreg_t               filler0;	/* filler for proper alignment */
    xbowreg_t               link_control;
    xbowreg_t               filler1;
    xbowreg_t               link_status;
    xbowreg_t               filler2;
    xbowreg_t               link_arb_upper;
    xbowreg_t               filler3;
    xbowreg_t               link_arb_lower;
    xbowreg_t               filler4;
    xbowreg_t               link_status_clr;
    xbowreg_t               filler5;
    xbowreg_t               link_reset;
    xbowreg_t               filler6;
    xbowreg_t               link_aux_status;
    xbowreg_t               filler7;
#else
    xbowreg_t               filler0;	/* filler for proper alignment */
    xbowreg_t               link_ibf;
    xbowreg_t               filler1;
    xbowreg_t               link_control;
    xbowreg_t               filler2;
    xbowreg_t               link_status;
    xbowreg_t               filler3;
    xbowreg_t               link_arb_upper;
    xbowreg_t               filler4;
    xbowreg_t               link_arb_lower;
    xbowreg_t               filler5;
    xbowreg_t               link_status_clr;
    xbowreg_t               filler6;
    xbowreg_t               link_reset;
    xbowreg_t               filler7;
    xbowreg_t               link_aux_status;
#endif /* LITTLE_ENDIAN */
} xb_linkregs_t;

typedef volatile struct xbow_s {
    /* standard widget configuration                       0x000000-0x000057 */
    widget_cfg_t            xb_widget;  /* 0x000000 */

    /* helper fieldnames for accessing bridge widget */

#define xb_wid_id                       xb_widget.w_id
#define xb_wid_stat                     xb_widget.w_status
#define xb_wid_err_upper                xb_widget.w_err_upper_addr
#define xb_wid_err_lower                xb_widget.w_err_lower_addr
#define xb_wid_control                  xb_widget.w_control
#define xb_wid_req_timeout              xb_widget.w_req_timeout
#define xb_wid_int_upper                xb_widget.w_intdest_upper_addr
#define xb_wid_int_lower                xb_widget.w_intdest_lower_addr
#define xb_wid_err_cmdword              xb_widget.w_err_cmd_word
#define xb_wid_llp                      xb_widget.w_llp_cfg
#define xb_wid_stat_clr                 xb_widget.w_tflush

#ifdef LITTLE_ENDIAN
/* 
 * we access these through synergy unswizzled space, so the address
 * gets twiddled (i.e. references to 0x4 actually go to 0x0 and vv.)
 * That's why we put the register first and filler second.
 */
    /* xbow-specific widget configuration                  0x000058-0x0000FF */
    xbowreg_t               xb_wid_arb_reload;  /* 0x00005C */
    xbowreg_t               _pad_000058;
    xbowreg_t               xb_perf_ctr_a;      /* 0x000064 */
    xbowreg_t               _pad_000060;
    xbowreg_t               xb_perf_ctr_b;      /* 0x00006c */
    xbowreg_t               _pad_000068;
    xbowreg_t               xb_nic;     /* 0x000074 */
    xbowreg_t               _pad_000070;

    /* Xbridge only */
    xbowreg_t               xb_w0_rst_fnc;      /* 0x00007C */
    xbowreg_t               _pad_000078;
    xbowreg_t               xb_l8_rst_fnc;      /* 0x000084 */
    xbowreg_t               _pad_000080;
    xbowreg_t               xb_l9_rst_fnc;      /* 0x00008c */
    xbowreg_t               _pad_000088;
    xbowreg_t               xb_la_rst_fnc;      /* 0x000094 */
    xbowreg_t               _pad_000090;
    xbowreg_t               xb_lb_rst_fnc;      /* 0x00009c */
    xbowreg_t               _pad_000098;
    xbowreg_t               xb_lc_rst_fnc;      /* 0x0000a4 */
    xbowreg_t               _pad_0000a0;
    xbowreg_t               xb_ld_rst_fnc;      /* 0x0000ac */
    xbowreg_t               _pad_0000a8;
    xbowreg_t               xb_le_rst_fnc;      /* 0x0000b4 */
    xbowreg_t               _pad_0000b0;
    xbowreg_t               xb_lf_rst_fnc;      /* 0x0000bc */
    xbowreg_t               _pad_0000b8;
    xbowreg_t               xb_lock;            /* 0x0000c4 */
    xbowreg_t               _pad_0000c0;
    xbowreg_t               xb_lock_clr;        /* 0x0000cc */
    xbowreg_t               _pad_0000c8;
    /* end of Xbridge only */
    xbowreg_t               _pad_0000d0[12];
#else
    /* xbow-specific widget configuration                  0x000058-0x0000FF */
    xbowreg_t               _pad_000058;
    xbowreg_t               xb_wid_arb_reload;  /* 0x00005C */
    xbowreg_t               _pad_000060;
    xbowreg_t               xb_perf_ctr_a;      /* 0x000064 */
    xbowreg_t               _pad_000068;
    xbowreg_t               xb_perf_ctr_b;      /* 0x00006c */
    xbowreg_t               _pad_000070;
    xbowreg_t               xb_nic;     /* 0x000074 */

    /* Xbridge only */
    xbowreg_t               _pad_000078;
    xbowreg_t               xb_w0_rst_fnc;      /* 0x00007C */
    xbowreg_t               _pad_000080;
    xbowreg_t               xb_l8_rst_fnc;      /* 0x000084 */
    xbowreg_t               _pad_000088;
    xbowreg_t               xb_l9_rst_fnc;      /* 0x00008c */
    xbowreg_t               _pad_000090;
    xbowreg_t               xb_la_rst_fnc;      /* 0x000094 */
    xbowreg_t               _pad_000098;
    xbowreg_t               xb_lb_rst_fnc;      /* 0x00009c */
    xbowreg_t               _pad_0000a0;
    xbowreg_t               xb_lc_rst_fnc;      /* 0x0000a4 */
    xbowreg_t               _pad_0000a8;
    xbowreg_t               xb_ld_rst_fnc;      /* 0x0000ac */
    xbowreg_t               _pad_0000b0;
    xbowreg_t               xb_le_rst_fnc;      /* 0x0000b4 */
    xbowreg_t               _pad_0000b8;
    xbowreg_t               xb_lf_rst_fnc;      /* 0x0000bc */
    xbowreg_t               _pad_0000c0;
    xbowreg_t               xb_lock;            /* 0x0000c4 */
    xbowreg_t               _pad_0000c8;
    xbowreg_t               xb_lock_clr;        /* 0x0000cc */
    /* end of Xbridge only */
    xbowreg_t               _pad_0000d0[12];
#endif /* LITTLE_ENDIAN */

    /* Link Specific Registers, port 8..15                 0x000100-0x000300 */
    xb_linkregs_t           xb_link_raw[MAX_XBOW_PORTS];
#define xb_link(p)      xb_link_raw[(p) & (MAX_XBOW_PORTS - 1)]

} xbow_t;

/* Configuration structure which describes each xbow link */
typedef struct xbow_cfg_s {
    int			    xb_port;	/* port number (0-15) */
    int			    xb_flags;	/* port software flags */
    short		    xb_shift;	/* shift for arb reg (mask is 0xff) */
    short		    xb_ul;	/* upper or lower arb reg */
    int			    xb_pad;	/* use this later (pad to ptr align) */
    xb_linkregs_t	   *xb_linkregs;	/* pointer to link registers */
    widget_cfg_t	   *xb_widget;	/* pointer to widget registers */
    char		    xb_name[MAX_XBOW_NAME];	/* port name */
    xbowreg_t		    xb_sh_arb_upper;	/* shadow upper arb register */
    xbowreg_t		    xb_sh_arb_lower;	/* shadow lower arb register */
} xbow_cfg_t;

#define XB_FLAGS_EXISTS		0x1	/* device exists */
#define XB_FLAGS_MASTER		0x2
#define XB_FLAGS_SLAVE		0x0
#define XB_FLAGS_GBR		0x4
#define XB_FLAGS_16BIT		0x8
#define XB_FLAGS_8BIT		0x0

/* get xbow config information for port p */
#define XB_CONFIG(p)	xbow_cfg[xb_ports[p]]

/* is widget port number valid?  (based on version 7.0 of xbow spec) */
#define XBOW_WIDGET_IS_VALID(wid) ((wid) >= XBOW_PORT_8 && (wid) <= XBOW_PORT_F)

/* whether to use upper or lower arbitration register, given source widget id */
#define XBOW_ARB_IS_UPPER(wid) 	((wid) >= XBOW_PORT_8 && (wid) <= XBOW_PORT_B)
#define XBOW_ARB_IS_LOWER(wid) 	((wid) >= XBOW_PORT_C && (wid) <= XBOW_PORT_F)

/* offset of arbitration register, given source widget id */
#define XBOW_ARB_OFF(wid) 	(XBOW_ARB_IS_UPPER(wid) ? 0x1c : 0x24)

#endif				/* __ASSEMBLY__ */

#define	XBOW_WID_ID		WIDGET_ID
#define	XBOW_WID_STAT		WIDGET_STATUS
#define	XBOW_WID_ERR_UPPER	WIDGET_ERR_UPPER_ADDR
#define	XBOW_WID_ERR_LOWER	WIDGET_ERR_LOWER_ADDR
#define	XBOW_WID_CONTROL	WIDGET_CONTROL
#define	XBOW_WID_REQ_TO		WIDGET_REQ_TIMEOUT
#define	XBOW_WID_INT_UPPER	WIDGET_INTDEST_UPPER_ADDR
#define	XBOW_WID_INT_LOWER	WIDGET_INTDEST_LOWER_ADDR
#define	XBOW_WID_ERR_CMDWORD	WIDGET_ERR_CMD_WORD
#define	XBOW_WID_LLP		WIDGET_LLP_CFG
#define	XBOW_WID_STAT_CLR	WIDGET_TFLUSH
#define XBOW_WID_ARB_RELOAD 	0x5c
#define XBOW_WID_PERF_CTR_A 	0x64
#define XBOW_WID_PERF_CTR_B 	0x6c
#define XBOW_WID_NIC 		0x74

/* Xbridge only */
#define XBOW_W0_RST_FNC		0x00007C
#define	XBOW_L8_RST_FNC		0x000084
#define	XBOW_L9_RST_FNC		0x00008c
#define	XBOW_LA_RST_FNC		0x000094
#define	XBOW_LB_RST_FNC		0x00009c
#define	XBOW_LC_RST_FNC		0x0000a4
#define	XBOW_LD_RST_FNC		0x0000ac
#define	XBOW_LE_RST_FNC		0x0000b4
#define	XBOW_LF_RST_FNC		0x0000bc
#define XBOW_RESET_FENCE(x) ((x) > 7 && (x) < 16) ? \
				(XBOW_W0_RST_FNC + ((x) - 7) * 8) : \
				((x) == 0) ? XBOW_W0_RST_FNC : 0
#define XBOW_LOCK		0x0000c4
#define XBOW_LOCK_CLR		0x0000cc
/* End of Xbridge only */

/* used only in ide, but defined here within the reserved portion */
/*              of the widget0 address space (before 0xf4) */
#define	XBOW_WID_UNDEF		0xe4

/* pointer to link arbitration register, given xbow base, dst and src widget id */
#define XBOW_PRIO_ARBREG_PTR(base, dst_wid, src_wid) \
	XBOW_REG_PTR(XBOW_PRIO_LINKREGS_PTR(base, dst_wid), XBOW_ARB_OFF(src_wid))

/* pointer to link registers base, given xbow base and destination widget id */
#define XBOW_PRIO_LINKREGS_PTR(base, dst_wid) (xb_linkregs_t*) \
	XBOW_REG_PTR(base, XB_LINK_REG_BASE(dst_wid))

/* xbow link register set base, legal value for x is 0x8..0xf */
#define	XB_LINK_BASE		0x100
#define	XB_LINK_OFFSET		0x40
#define	XB_LINK_REG_BASE(x)	(XB_LINK_BASE + ((x) & (MAX_XBOW_PORTS - 1)) * XB_LINK_OFFSET)

#define	XB_LINK_IBUF_FLUSH(x)	(XB_LINK_REG_BASE(x) + 0x4)
#define	XB_LINK_CTRL(x)		(XB_LINK_REG_BASE(x) + 0xc)
#define	XB_LINK_STATUS(x)	(XB_LINK_REG_BASE(x) + 0x14)
#define	XB_LINK_ARB_UPPER(x)	(XB_LINK_REG_BASE(x) + 0x1c)
#define	XB_LINK_ARB_LOWER(x)	(XB_LINK_REG_BASE(x) + 0x24)
#define	XB_LINK_STATUS_CLR(x)	(XB_LINK_REG_BASE(x) + 0x2c)
#define	XB_LINK_RESET(x)	(XB_LINK_REG_BASE(x) + 0x34)
#define	XB_LINK_AUX_STATUS(x)	(XB_LINK_REG_BASE(x) + 0x3c)

/* link_control(x) */
#define	XB_CTRL_LINKALIVE_IE		0x80000000	/* link comes alive */
     /* reserved:			0x40000000 */
#define	XB_CTRL_PERF_CTR_MODE_MSK	0x30000000	/* perf counter mode */
#define	XB_CTRL_IBUF_LEVEL_MSK		0x0e000000	/* input packet buffer level */
#define	XB_CTRL_8BIT_MODE		0x01000000	/* force link into 8 bit mode */
#define XB_CTRL_BAD_LLP_PKT		0x00800000	/* force bad LLP packet */
#define XB_CTRL_WIDGET_CR_MSK		0x007c0000	/* LLP widget credit mask */
#define XB_CTRL_WIDGET_CR_SHFT	18			/* LLP widget credit shift */
#define XB_CTRL_ILLEGAL_DST_IE		0x00020000	/* illegal destination */
#define XB_CTRL_OALLOC_IBUF_IE		0x00010000	/* overallocated input buffer */
     /* reserved:			0x0000fe00 */
#define XB_CTRL_BNDWDTH_ALLOC_IE	0x00000100	/* bandwidth alloc */
#define XB_CTRL_RCV_CNT_OFLOW_IE	0x00000080	/* rcv retry overflow */
#define XB_CTRL_XMT_CNT_OFLOW_IE	0x00000040	/* xmt retry overflow */
#define XB_CTRL_XMT_MAX_RTRY_IE		0x00000020	/* max transmit retry */
#define XB_CTRL_RCV_IE			0x00000010	/* receive */
#define XB_CTRL_XMT_RTRY_IE		0x00000008	/* transmit retry */
     /* reserved:			0x00000004 */
#define	XB_CTRL_MAXREQ_TOUT_IE		0x00000002	/* maximum request timeout */
#define	XB_CTRL_SRC_TOUT_IE		0x00000001	/* source timeout */

/* link_status(x) */
#define	XB_STAT_LINKALIVE		XB_CTRL_LINKALIVE_IE
     /* reserved:			0x7ff80000 */
#define	XB_STAT_MULTI_ERR		0x00040000	/* multi error */
#define	XB_STAT_ILLEGAL_DST_ERR		XB_CTRL_ILLEGAL_DST_IE
#define	XB_STAT_OALLOC_IBUF_ERR		XB_CTRL_OALLOC_IBUF_IE
#define	XB_STAT_BNDWDTH_ALLOC_ID_MSK	0x0000ff00	/* port bitmask */
#define	XB_STAT_RCV_CNT_OFLOW_ERR	XB_CTRL_RCV_CNT_OFLOW_IE
#define	XB_STAT_XMT_CNT_OFLOW_ERR	XB_CTRL_XMT_CNT_OFLOW_IE
#define	XB_STAT_XMT_MAX_RTRY_ERR	XB_CTRL_XMT_MAX_RTRY_IE
#define	XB_STAT_RCV_ERR			XB_CTRL_RCV_IE
#define	XB_STAT_XMT_RTRY_ERR		XB_CTRL_XMT_RTRY_IE
     /* reserved:			0x00000004 */
#define	XB_STAT_MAXREQ_TOUT_ERR		XB_CTRL_MAXREQ_TOUT_IE
#define	XB_STAT_SRC_TOUT_ERR		XB_CTRL_SRC_TOUT_IE

/* link_aux_status(x) */
#define	XB_AUX_STAT_RCV_CNT	0xff000000
#define	XB_AUX_STAT_XMT_CNT	0x00ff0000
#define	XB_AUX_STAT_TOUT_DST	0x0000ff00
#define	XB_AUX_LINKFAIL_RST_BAD	0x00000040
#define	XB_AUX_STAT_PRESENT	0x00000020
#define	XB_AUX_STAT_PORT_WIDTH	0x00000010
     /*	reserved:		0x0000000f */

/*
 * link_arb_upper/link_arb_lower(x), (reg) should be the link_arb_upper
 * register if (x) is 0x8..0xb, link_arb_lower if (x) is 0xc..0xf
 */
#define	XB_ARB_GBR_MSK		0x1f
#define	XB_ARB_RR_MSK		0x7
#define	XB_ARB_GBR_SHFT(x)	(((x) & 0x3) * 8)
#define	XB_ARB_RR_SHFT(x)	(((x) & 0x3) * 8 + 5)
#define	XB_ARB_GBR_CNT(reg,x)	((reg) >> XB_ARB_GBR_SHFT(x) & XB_ARB_GBR_MSK)
#define	XB_ARB_RR_CNT(reg,x)	((reg) >> XB_ARB_RR_SHFT(x) & XB_ARB_RR_MSK)

/* XBOW_WID_STAT */
#define	XB_WID_STAT_LINK_INTR_SHFT	(24)
#define	XB_WID_STAT_LINK_INTR_MASK	(0xFF << XB_WID_STAT_LINK_INTR_SHFT)
#define	XB_WID_STAT_LINK_INTR(x)	(0x1 << (((x)&7) + XB_WID_STAT_LINK_INTR_SHFT))
#define	XB_WID_STAT_WIDGET0_INTR	0x00800000
#define XB_WID_STAT_SRCID_MASK		0x000003c0	/* Xbridge only */
#define	XB_WID_STAT_REG_ACC_ERR		0x00000020
#define XB_WID_STAT_RECV_TOUT		0x00000010	/* Xbridge only */
#define XB_WID_STAT_ARB_TOUT		0x00000008	/* Xbridge only */
#define	XB_WID_STAT_XTALK_ERR		0x00000004
#define XB_WID_STAT_DST_TOUT		0x00000002	/* Xbridge only */
#define	XB_WID_STAT_MULTI_ERR		0x00000001

#define XB_WID_STAT_SRCID_SHFT		6

/* XBOW_WID_CONTROL */
#define XB_WID_CTRL_REG_ACC_IE		XB_WID_STAT_REG_ACC_ERR
#define XB_WID_CTRL_RECV_TOUT		XB_WID_STAT_RECV_TOUT
#define XB_WID_CTRL_ARB_TOUT		XB_WID_STAT_ARB_TOUT
#define XB_WID_CTRL_XTALK_IE		XB_WID_STAT_XTALK_ERR

/* XBOW_WID_INT_UPPER */
/* defined in xwidget.h for WIDGET_INTDEST_UPPER_ADDR */

/* XBOW WIDGET part number, in the ID register */
#define XBOW_WIDGET_PART_NUM	0x0		/* crossbow */
#define XXBOW_WIDGET_PART_NUM	0xd000		/* Xbridge */
#define	XBOW_WIDGET_MFGR_NUM	0x0
#define	XXBOW_WIDGET_MFGR_NUM	0x0
#define PXBOW_WIDGET_PART_NUM   0xd100          /* PIC */

#define	XBOW_REV_1_0		0x1	/* xbow rev 1.0 is "1" */
#define	XBOW_REV_1_1		0x2	/* xbow rev 1.1 is "2" */
#define XBOW_REV_1_2		0x3	/* xbow rev 1.2 is "3" */
#define XBOW_REV_1_3		0x4	/* xbow rev 1.3 is "4" */
#define XBOW_REV_2_0		0x5	/* xbow rev 2.0 is "5" */

#define XXBOW_PART_REV_1_0		(XXBOW_WIDGET_PART_NUM << 4 | 0x1 )
#define XXBOW_PART_REV_2_0		(XXBOW_WIDGET_PART_NUM << 4 | 0x2 )

/* XBOW_WID_ARB_RELOAD */
#define	XBOW_WID_ARB_RELOAD_INT	0x3f	/* GBR reload interval */

#define IS_XBRIDGE_XBOW(wid) \
        (XWIDGET_PART_NUM(wid) == XXBOW_WIDGET_PART_NUM && \
                        XWIDGET_MFG_NUM(wid) == XXBOW_WIDGET_MFGR_NUM)

#define IS_PIC_XBOW(wid) \
        (XWIDGET_PART_NUM(wid) == PXBOW_WIDGET_PART_NUM && \
                        XWIDGET_MFG_NUM(wid) == XXBOW_WIDGET_MFGR_NUM)

#define XBOW_WAR_ENABLED(pv, widid) ((1 << XWIDGET_REV_NUM(widid)) & pv)
#define PV854827 (~0)     /* PIC: fake widget 0xf presence bit. permanent */
#define PV863579 (1 << 1) /* PIC: PIO to PIC register */


#ifndef __ASSEMBLY__
/*
 * XBOW Widget 0 Register formats.
 * Format for many of these registers are similar to the standard
 * widget register format described as part of xtalk specification
 * Standard widget register field format description is available in
 * xwidget.h
 * Following structures define the format for xbow widget 0 registers
 */
/*
 * Xbow Widget 0 Command error word
 */
#ifdef LITTLE_ENDIAN

typedef union xbw0_cmdword_u {
    xbowreg_t               cmdword;
    struct {
	uint32_t              rsvd:8,		/* Reserved */
                                barr:1,         /* Barrier operation */
                                error:1,        /* Error Occured */
                                vbpm:1,         /* Virtual Backplane message */
                                gbr:1,  /* GBR enable ?                 */
                                ds:2,   /* Data size                    */
                                ct:1,   /* Is it a coherent transaction */
                                tnum:5,         /* Transaction Number */
                                pactyp:4,       /* Packet type: */
                                srcid:4,        /* Source ID number */
                                destid:4;       /* Desination ID number */

    } xbw0_cmdfield;
} xbw0_cmdword_t;

#else

typedef union xbw0_cmdword_u {
    xbowreg_t		    cmdword;
    struct {
	uint32_t		destid:4,	/* Desination ID number */
				srcid:4,	/* Source ID number */
				pactyp:4,	/* Packet type: */
				tnum:5,		/* Transaction Number */
				ct:1,	/* Is it a coherent transaction */
				ds:2,	/* Data size			*/
				gbr:1,	/* GBR enable ?			*/
				vbpm:1,		/* Virtual Backplane message */
				error:1,	/* Error Occured */
				barr:1,		/* Barrier operation */
				rsvd:8;		/* Reserved */
    } xbw0_cmdfield;
} xbw0_cmdword_t;

#endif

#define	xbcmd_destid	xbw0_cmdfield.destid
#define	xbcmd_srcid	xbw0_cmdfield.srcid
#define	xbcmd_pactyp	xbw0_cmdfield.pactyp
#define	xbcmd_tnum	xbw0_cmdfield.tnum
#define	xbcmd_ct	xbw0_cmdfield.ct
#define	xbcmd_ds	xbw0_cmdfield.ds
#define	xbcmd_gbr	xbw0_cmdfield.gbr
#define	xbcmd_vbpm	xbw0_cmdfield.vbpm
#define	xbcmd_error	xbw0_cmdfield.error
#define	xbcmd_barr	xbw0_cmdfield.barr

/*
 * Values for field PACTYP in xbow error command word
 */
#define	XBCMDTYP_READREQ	0	/* Read Request   packet  */
#define	XBCMDTYP_READRESP	1	/* Read Response packet   */
#define	XBCMDTYP_WRREQ_RESP	2	/* Write Request with response    */
#define	XBCMDTYP_WRRESP		3	/* Write Response */
#define	XBCMDTYP_WRREQ_NORESP	4	/* Write request with  No Response */
#define	XBCMDTYP_FETCHOP	6	/* Fetch & Op packet      */
#define	XBCMDTYP_STOREOP	8	/* Store & Op packet      */
#define	XBCMDTYP_SPLPKT_REQ	0xE	/* Special packet request */
#define	XBCMDTYP_SPLPKT_RESP	0xF	/* Special packet response        */

/*
 * Values for field ds (datasize) in xbow error command word
 */
#define	XBCMDSZ_DOUBLEWORD	0
#define	XBCMDSZ_QUARTRCACHE	1
#define	XBCMDSZ_FULLCACHE	2

/*
 * Xbow widget 0 Status register format.
 */
#ifdef LITTLE_ENDIAN

typedef union xbw0_status_u {
    xbowreg_t               statusword;
    struct {
       uint32_t		mult_err:1,	/* Multiple error occurred */
                                connect_tout:1, /* Connection timeout   */
                                xtalk_err:1,    /* Xtalk pkt with error bit */
                                /* End of Xbridge only */
                                w0_arb_tout,    /* arbiter timeout err */
                                w0_recv_tout,   /* receive timeout err */
                                /* Xbridge only */
                                regacc_err:1,   /* Reg Access error     */
                                src_id:4,       /* source id. Xbridge only */
                                resvd1:13,
                                wid0intr:1;     /* Widget 0 err intr */
    } xbw0_stfield;
} xbw0_status_t;

#else

typedef union xbw0_status_u {
    xbowreg_t		    statusword;
    struct {
	uint32_t		linkXintr:8,	/* link(x) error intr */
				wid0intr:1,	/* Widget 0 err intr */
				resvd1:13,
				src_id:4,	/* source id. Xbridge only */
				regacc_err:1,	/* Reg Access error	*/
				/* Xbridge only */
				w0_recv_tout,	/* receive timeout err */
				w0_arb_tout,	/* arbiter timeout err */
				/* End of Xbridge only */
				xtalk_err:1,	/* Xtalk pkt with error bit */
				connect_tout:1, /* Connection timeout	*/
				mult_err:1;	/* Multiple error occurred */
    } xbw0_stfield;
} xbw0_status_t;

#endif

#define	xbst_linkXintr		xbw0_stfield.linkXintr
#define	xbst_w0intr		xbw0_stfield.wid0intr
#define	xbst_regacc_err		xbw0_stfield.regacc_err
#define	xbst_xtalk_err		xbw0_stfield.xtalk_err
#define	xbst_connect_tout	xbw0_stfield.connect_tout
#define	xbst_mult_err		xbw0_stfield.mult_err
#define xbst_src_id		xbw0_stfield.src_id	    /* Xbridge only */
#define xbst_w0_recv_tout	xbw0_stfield.w0_recv_tout   /* Xbridge only */
#define xbst_w0_arb_tout	xbw0_stfield.w0_arb_tout    /* Xbridge only */

/*
 * Xbow widget 0 Control register format
 */
#ifdef LITTLE_ENDIAN

typedef union xbw0_ctrl_u {
    xbowreg_t               ctrlword;
    struct {
	uint32_t              
				resvd3:1,
                                conntout_intr:1,
                                xtalkerr_intr:1,
                                w0_arg_tout_intr:1,     /* Xbridge only */
                                w0_recv_tout_intr:1,    /* Xbridge only */
                                accerr_intr:1,
                                enable_w0_tout_cntr:1,  /* Xbridge only */
                                enable_watchdog:1,      /* Xbridge only */
                                resvd1:24;
    } xbw0_ctrlfield;
} xbw0_ctrl_t;

#else

typedef union xbw0_ctrl_u {
    xbowreg_t		    ctrlword;
    struct {
	uint32_t
				resvd1:24,
				enable_watchdog:1,	/* Xbridge only */
				enable_w0_tout_cntr:1,	/* Xbridge only */
				accerr_intr:1,
				w0_recv_tout_intr:1,	/* Xbridge only */
				w0_arg_tout_intr:1,	/* Xbridge only */
				xtalkerr_intr:1,
				conntout_intr:1,
				resvd3:1;
    } xbw0_ctrlfield;
} xbw0_ctrl_t;

#endif

#ifdef LITTLE_ENDIAN

typedef union xbow_linkctrl_u {
    xbowreg_t               xbl_ctrlword;
    struct {
	uint32_t 		srcto_intr:1,
                                maxto_intr:1, 
                                rsvd3:1,
                                trx_retry_intr:1, 
                                rcv_err_intr:1, 
                                trx_max_retry_intr:1,
                                trxov_intr:1, 
                                rcvov_intr:1,
                                bwalloc_intr:1, 
                                rsvd2:7, 
                                obuf_intr:1,
                                idest_intr:1, 
                                llp_credit:5, 
                                force_badllp:1,
                                send_bm8:1, 
                                inbuf_level:3, 
                                perf_mode:2,
                                rsvd1:1, 
       		                alive_intr:1;

    } xb_linkcontrol;
} xbow_linkctrl_t;

#else

typedef union xbow_linkctrl_u {
    xbowreg_t		    xbl_ctrlword;
    struct {
	uint32_t		alive_intr:1, 
				rsvd1:1, 
				perf_mode:2,
				inbuf_level:3, 
				send_bm8:1, 
				force_badllp:1,
				llp_credit:5, 
				idest_intr:1, 
				obuf_intr:1,
				rsvd2:7, 
				bwalloc_intr:1, 
				rcvov_intr:1,
				trxov_intr:1, 
				trx_max_retry_intr:1,
				rcv_err_intr:1, 
				trx_retry_intr:1, 
				rsvd3:1,
				maxto_intr:1, 
				srcto_intr:1;
    } xb_linkcontrol;
} xbow_linkctrl_t;

#endif


#define	xbctl_accerr_intr	(xbw0_ctrlfield.accerr_intr)
#define	xbctl_xtalkerr_intr	(xbw0_ctrlfield.xtalkerr_intr)
#define	xbctl_cnntout_intr	(xbw0_ctrlfield.conntout_intr)

#define	XBW0_CTRL_ACCERR_INTR	(1 << 5)
#define	XBW0_CTRL_XTERR_INTR	(1 << 2)
#define	XBW0_CTRL_CONNTOUT_INTR	(1 << 1)

/*
 * Xbow Link specific Registers structure definitions.
 */

#ifdef LITTLE_ENDIAN

typedef union xbow_linkX_status_u {
    xbowreg_t               linkstatus;
    struct {
	uint32_t               pkt_toutsrc:1,
                                pkt_toutconn:1, /* max_req_tout in Xbridge */
                                pkt_toutdest:1, /* reserved in Xbridge */
                                llp_xmitretry:1,
                                llp_rcverror:1,
                                llp_maxtxretry:1,
                                llp_txovflow:1,
                                llp_rxovflow:1,
                                bw_errport:8,   /* BW allocation error port   */
                                ioe:1,          /* Input overallocation error */
                                illdest:1,
                                merror:1,
                                resvd1:12,
				alive:1;
    } xb_linkstatus;
} xbwX_stat_t;

#else

typedef union xbow_linkX_status_u {
    xbowreg_t		    linkstatus;
    struct {
	uint32_t		alive:1,
				resvd1:12,
				merror:1,
				illdest:1,
				ioe:1,		/* Input overallocation error */
				bw_errport:8,	/* BW allocation error port   */
				llp_rxovflow:1,
				llp_txovflow:1,
				llp_maxtxretry:1,
				llp_rcverror:1,
				llp_xmitretry:1,
				pkt_toutdest:1, /* reserved in Xbridge */
				pkt_toutconn:1, /* max_req_tout in Xbridge */
				pkt_toutsrc:1;
    } xb_linkstatus;
} xbwX_stat_t;

#endif

#define	link_alive		xb_linkstatus.alive
#define	link_multierror		xb_linkstatus.merror
#define	link_illegal_dest	xb_linkstatus.illdest
#define	link_ioe		xb_linkstatus.ioe
#define link_max_req_tout	xb_linkstatus.pkt_toutconn  /* Xbridge */
#define link_pkt_toutconn	xb_linkstatus.pkt_toutconn  /* Xbow */
#define link_pkt_toutdest	xb_linkstatus.pkt_toutdest
#define	link_pkt_toutsrc	xb_linkstatus.pkt_toutsrc

#ifdef LITTLE_ENDIAN

typedef union xbow_aux_linkX_status_u {
    xbowreg_t               aux_linkstatus;
    struct {
	uint32_t 		rsvd2:4,
                                bit_mode_8:1,
                                wid_present:1,
                                fail_mode:1,
                                rsvd1:1,
                                to_src_loc:8,
                                tx_retry_cnt:8,
				rx_err_cnt:8;
    } xb_aux_linkstatus;
} xbow_aux_link_status_t;

#else

typedef union xbow_aux_linkX_status_u {
    xbowreg_t		    aux_linkstatus;
    struct {
	uint32_t		rx_err_cnt:8,
				tx_retry_cnt:8,
				to_src_loc:8,
				rsvd1:1,
				fail_mode:1,
				wid_present:1,
				bit_mode_8:1,
				rsvd2:4;
    } xb_aux_linkstatus;
} xbow_aux_link_status_t;

#endif


#ifdef LITTLE_ENDIAN

typedef union xbow_perf_count_u {
    xbowreg_t               xb_counter_val;
    struct {
        uint32_t 		count:20,
                                link_select:3,
				rsvd:9;
    } xb_perf;
} xbow_perfcount_t;

#else

typedef union xbow_perf_count_u {
    xbowreg_t               xb_counter_val;
    struct {
	uint32_t              rsvd:9, 
				link_select:3, 
				count:20;
    } xb_perf;
} xbow_perfcount_t;

#endif

#define XBOW_COUNTER_MASK	0xFFFFF

extern int              xbow_widget_present(xbow_t * xbow, int port);

extern xwidget_intr_preset_f xbow_intr_preset;
extern xswitch_reset_link_f xbow_reset_link;
void                    xbow_mlreset(xbow_t *);

/* ========================================================================
 */

#ifdef	MACROFIELD_LINE
/*
 * This table forms a relation between the byte offset macros normally
 * used for ASM coding and the calculated byte offsets of the fields
 * in the C structure.
 *
 * See xbow_check.c xbow_html.c for further details.
 */
#ifndef MACROFIELD_LINE_BITFIELD
#define MACROFIELD_LINE_BITFIELD(m)	/* ignored */
#endif

struct macrofield_s     xbow_macrofield[] =
{

    MACROFIELD_LINE(XBOW_WID_ID, xb_wid_id)
    MACROFIELD_LINE(XBOW_WID_STAT, xb_wid_stat)
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0xF))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0xE))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0xD))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0xC))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0xB))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0xA))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0x9))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_LINK_INTR(0x8))
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_WIDGET0_INTR)
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_REG_ACC_ERR)
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_XTALK_ERR)
    MACROFIELD_LINE_BITFIELD(XB_WID_STAT_MULTI_ERR)
    MACROFIELD_LINE(XBOW_WID_ERR_UPPER, xb_wid_err_upper)
    MACROFIELD_LINE(XBOW_WID_ERR_LOWER, xb_wid_err_lower)
    MACROFIELD_LINE(XBOW_WID_CONTROL, xb_wid_control)
    MACROFIELD_LINE_BITFIELD(XB_WID_CTRL_REG_ACC_IE)
    MACROFIELD_LINE_BITFIELD(XB_WID_CTRL_XTALK_IE)
    MACROFIELD_LINE(XBOW_WID_REQ_TO, xb_wid_req_timeout)
    MACROFIELD_LINE(XBOW_WID_INT_UPPER, xb_wid_int_upper)
    MACROFIELD_LINE(XBOW_WID_INT_LOWER, xb_wid_int_lower)
    MACROFIELD_LINE(XBOW_WID_ERR_CMDWORD, xb_wid_err_cmdword)
    MACROFIELD_LINE(XBOW_WID_LLP, xb_wid_llp)
    MACROFIELD_LINE(XBOW_WID_STAT_CLR, xb_wid_stat_clr)
    MACROFIELD_LINE(XBOW_WID_ARB_RELOAD, xb_wid_arb_reload)
    MACROFIELD_LINE(XBOW_WID_PERF_CTR_A, xb_perf_ctr_a)
    MACROFIELD_LINE(XBOW_WID_PERF_CTR_B, xb_perf_ctr_b)
    MACROFIELD_LINE(XBOW_WID_NIC, xb_nic)
    MACROFIELD_LINE(XB_LINK_REG_BASE(8), xb_link(8))
    MACROFIELD_LINE(XB_LINK_IBUF_FLUSH(8), xb_link(8).link_ibf)
    MACROFIELD_LINE(XB_LINK_CTRL(8), xb_link(8).link_control)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_LINKALIVE_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_PERF_CTR_MODE_MSK)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_IBUF_LEVEL_MSK)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_8BIT_MODE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_BAD_LLP_PKT)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_WIDGET_CR_MSK)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_ILLEGAL_DST_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_OALLOC_IBUF_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_BNDWDTH_ALLOC_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_RCV_CNT_OFLOW_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_XMT_CNT_OFLOW_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_XMT_MAX_RTRY_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_RCV_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_XMT_RTRY_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_MAXREQ_TOUT_IE)
    MACROFIELD_LINE_BITFIELD(XB_CTRL_SRC_TOUT_IE)
    MACROFIELD_LINE(XB_LINK_STATUS(8), xb_link(8).link_status)
    MACROFIELD_LINE_BITFIELD(XB_STAT_LINKALIVE)
    MACROFIELD_LINE_BITFIELD(XB_STAT_MULTI_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_ILLEGAL_DST_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_OALLOC_IBUF_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_BNDWDTH_ALLOC_ID_MSK)
    MACROFIELD_LINE_BITFIELD(XB_STAT_RCV_CNT_OFLOW_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_XMT_CNT_OFLOW_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_XMT_MAX_RTRY_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_RCV_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_XMT_RTRY_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_MAXREQ_TOUT_ERR)
    MACROFIELD_LINE_BITFIELD(XB_STAT_SRC_TOUT_ERR)
    MACROFIELD_LINE(XB_LINK_ARB_UPPER(8), xb_link(8).link_arb_upper)
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0xb))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0xb))
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0xa))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0xa))
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0x9))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0x9))
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0x8))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0x8))
    MACROFIELD_LINE(XB_LINK_ARB_LOWER(8), xb_link(8).link_arb_lower)
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0xf))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0xf))
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0xe))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0xe))
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0xd))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0xd))
    MACROFIELD_LINE_BITFIELD(XB_ARB_RR_MSK << XB_ARB_RR_SHFT(0xc))
    MACROFIELD_LINE_BITFIELD(XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(0xc))
    MACROFIELD_LINE(XB_LINK_STATUS_CLR(8), xb_link(8).link_status_clr)
    MACROFIELD_LINE(XB_LINK_RESET(8), xb_link(8).link_reset)
    MACROFIELD_LINE(XB_LINK_AUX_STATUS(8), xb_link(8).link_aux_status)
    MACROFIELD_LINE_BITFIELD(XB_AUX_STAT_RCV_CNT)
    MACROFIELD_LINE_BITFIELD(XB_AUX_STAT_XMT_CNT)
    MACROFIELD_LINE_BITFIELD(XB_AUX_LINKFAIL_RST_BAD)
    MACROFIELD_LINE_BITFIELD(XB_AUX_STAT_PRESENT)
    MACROFIELD_LINE_BITFIELD(XB_AUX_STAT_PORT_WIDTH)
    MACROFIELD_LINE_BITFIELD(XB_AUX_STAT_TOUT_DST)
    MACROFIELD_LINE(XB_LINK_REG_BASE(0x8), xb_link(0x8))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0x9), xb_link(0x9))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0xA), xb_link(0xA))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0xB), xb_link(0xB))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0xC), xb_link(0xC))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0xD), xb_link(0xD))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0xE), xb_link(0xE))
    MACROFIELD_LINE(XB_LINK_REG_BASE(0xF), xb_link(0xF))
};				/* xbow_macrofield[] */

#endif				/* MACROFIELD_LINE */

#endif				/* __ASSEMBLY__ */
#endif                          /* _ASM_SN_SN_XTALK_XBOW_H */
