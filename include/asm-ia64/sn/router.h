
/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_ROUTER_H
#define _ASM_IA64_SN_ROUTER_H

/*
 * Router Register definitions
 *
 * Macro argument _L always stands for a link number (1 to 8, inclusive).
 */

#ifndef __ASSEMBLY__

#include <linux/devfs_fs_kernel.h>
#include <asm/sn/vector.h>
#include <asm/sn/slotnum.h>
#include <asm/sn/arch.h>

typedef uint64_t	router_reg_t;

#define MAX_ROUTERS	64

#define MAX_ROUTER_PATH	80

#define ROUTER_REG_CAST		(volatile router_reg_t *)
#define PS_UINT_CAST		(__psunsigned_t)
#define UINT64_CAST		(uint64_t)
typedef signed char port_no_t;	 /* Type for router port number      */

#else 

#define ROUTERREG_CAST
#define PS_UINT_CAST
#define UINT64_CAST

#endif /* __ASSEMBLY__ */

#define MAX_ROUTER_PORTS (8)	 /* Max. number of ports on a router */

#define ALL_PORTS ((1 << MAX_ROUTER_PORTS) - 1)	/* for 0 based references */

#define PORT_INVALID (-1)	 /* Invalid port number              */

#define	IS_META(_rp)	((_rp)->flags & PCFG_ROUTER_META)

#define	IS_REPEATER(_rp)((_rp)->flags & PCFG_ROUTER_REPEATER)

/*
 * RR_TURN makes a given number of clockwise turns (0 to 7) from an inport
 * port to generate an output port.
 *
 * RR_DISTANCE returns the number of turns necessary (0 to 7) to go from
 * an input port (_L1 = 1 to 8) to an output port ( _L2 = 1 to 8).
 *
 * These are written to work on unsigned data.
 */

#define RR_TURN(_L, count)	((_L) + (count) > MAX_ROUTER_PORTS ?	\
				 (_L) + (count) - MAX_ROUTER_PORTS :	\
				 (_L) + (count))

#define RR_DISTANCE(_LS, _LD)	((_LD) >= (_LS) ?			\
				 (_LD) - (_LS) :			\
				 (_LD) + MAX_ROUTER_PORTS - (_LS))

/* Router register addresses */

#define RR_STATUS_REV_ID	0x00000	/* Status register and Revision ID  */
#define RR_PORT_RESET		0x00008	/* Multiple port reset              */
#define RR_PROT_CONF		0x00010	/* Inter-partition protection conf. */
#define RR_GLOBAL_PORT_DEF	0x00018 /* Global Port definitions          */
#define RR_GLOBAL_PARMS0	0x00020	/* Parameters shared by all 8 ports */
#define RR_GLOBAL_PARMS1	0x00028	/* Parameters shared by all 8 ports */
#define RR_DIAG_PARMS		0x00030	/* Parameters for diag. testing     */
#define RR_DEBUG_ADDR		0x00038 /* Debug address select - debug port*/
#define RR_LB_TO_L2		0x00040 /* Local Block to L2 cntrl intf reg */ 
#define RR_L2_TO_LB		0x00048 /* L2 cntrl intf to Local Block reg */
#define RR_JBUS_CONTROL		0x00050 /* read/write timing for JBUS intf  */

#define RR_SCRATCH_REG0		0x00100	/* Scratch 0 is 64 bits */
#define RR_SCRATCH_REG1		0x00108	/* Scratch 1 is 64 bits */
#define RR_SCRATCH_REG2		0x00110	/* Scratch 2 is 64 bits */
#define RR_SCRATCH_REG3		0x00118	/* Scratch 3 is 1 bit */
#define RR_SCRATCH_REG4		0x00120	/* Scratch 4 is 1 bit */

#define RR_JBUS0(_D)		(((_D) & 0x7) << 3 | 0x00200) /* JBUS0 addresses   */
#define RR_JBUS1(_D)		(((_D) & 0x7) << 3 | 0x00240) /* JBUS1 addresses   */

#define RR_SCRATCH_REG0_WZ	0x00500	/* Scratch 0 is 64 bits */
#define RR_SCRATCH_REG1_WZ	0x00508	/* Scratch 1 is 64 bits */
#define RR_SCRATCH_REG2_WZ	0x00510	/* Scratch 2 is 64 bits */
#define RR_SCRATCH_REG3_SZ	0x00518	/* Scratch 3 is 1 bit */
#define RR_SCRATCH_REG4_SZ	0x00520	/* Scratch 4 is 1 bit */

#define RR_VECTOR_HW_BAR(context) (0x08000 | (context)<<3) /* barrier config registers */
/* Port-specific registers (_L is the link number from 1 to 8) */

#define RR_PORT_PARMS(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0000) /* LLP parameters     */
#define RR_STATUS_ERROR(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0008) /* Port-related errs  */
#define RR_CHANNEL_TEST(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0010) /* Port LLP chan test */
#define RR_RESET_MASK(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0018) /* Remote reset mask  */
#define RR_HISTOGRAM0(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0020) /* Port usage histgrm */
#define RR_HISTOGRAM1(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0028) /* Port usage histgrm */
#define RR_HISTOGRAM0_WC(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0030) /* Port usage histgrm */
#define RR_HISTOGRAM1_WC(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0038) /* Port usage histgrm */
#define RR_ERROR_CLEAR(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0088) /* Read/clear errors  */
#define RR_GLOBAL_TABLE0(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0100) /* starting address of global table for this port */
#define RR_GLOBAL_TABLE(_L, _x) (RR_GLOBAL_TABLE0(_L) + ((_x) << 3))
#define RR_LOCAL_TABLE0(_L)	(((_L+1) & 0xe) << 15 | ((_L+1) & 0x1) << 11 | 0x0200) /* starting address of local table for this port */
#define RR_LOCAL_TABLE(_L, _x) (RR_LOCAL_TABLE0(_L) + ((_x) << 3))

#define RR_META_ENTRIES		16

#define RR_LOCAL_ENTRIES	128

/*
 * RR_STATUS_REV_ID mask and shift definitions
 */

#define RSRI_INPORT_SHFT	52
#define RSRI_INPORT_MASK	(UINT64_CAST 0xf << 52)
#define RSRI_LINKWORKING_BIT(_L) (35 + 2 * (_L))
#define RSRI_LINKWORKING(_L)	(UINT64_CAST 1 << (35 + 2 * (_L)))
#define RSRI_LINKRESETFAIL(_L)	(UINT64_CAST 1 << (34 + 2 * (_L)))
#define RSRI_LSTAT_SHFT(_L)	(34 + 2 * (_L))
#define RSRI_LSTAT_MASK(_L)	(UINT64_CAST 0x3 << 34 + 2 * (_L))
#define RSRI_LOCALSBERROR	(UINT64_CAST 1 << 35)
#define RSRI_LOCALSTUCK		(UINT64_CAST 1 << 34)
#define RSRI_LOCALBADVEC	(UINT64_CAST 1 << 33)
#define RSRI_LOCALTAILERR	(UINT64_CAST 1 << 32)
#define RSRI_LOCAL_SHFT 	32
#define RSRI_LOCAL_MASK		(UINT64_CAST 0xf << 32)
#define RSRI_CHIPREV_SHFT	28
#define RSRI_CHIPREV_MASK	(UINT64_CAST 0xf << 28)
#define RSRI_CHIPID_SHFT	12
#define RSRI_CHIPID_MASK	(UINT64_CAST 0xffff << 12)
#define RSRI_MFGID_SHFT		1
#define RSRI_MFGID_MASK		(UINT64_CAST 0x7ff << 1)

#define RSRI_LSTAT_WENTDOWN	0
#define RSRI_LSTAT_RESETFAIL	1
#define RSRI_LSTAT_LINKUP	2
#define RSRI_LSTAT_NOTUSED	3

/*
 * RR_PORT_RESET mask definitions
 */

#define RPRESET_WARM		(UINT64_CAST 1 << 9)
#define RPRESET_LINK(_L)	(UINT64_CAST 1 << (_L))
#define RPRESET_LOCAL		(UINT64_CAST 1)

/*
 * RR_PROT_CONF mask and shift definitions
 */

#define RPCONF_DIRCMPDIS_SHFT	13
#define RPCONF_DIRCMPDIS_MASK	(UINT64_CAST 1 << 13)
#define RPCONF_FORCELOCAL	(UINT64_CAST 1 << 12)
#define RPCONF_FLOCAL_SHFT	12
#define RPCONF_METAID_SHFT	8
#define RPCONF_METAID_MASK	(UINT64_CAST 0xf << 8)
#define RPCONF_RESETOK(_L)	(UINT64_CAST 1 << ((_L) - 1))

/*
 * RR_GLOBAL_PORT_DEF mask and shift definitions
 */

#define RGPD_MGLBLNHBR_ID_SHFT	12	/* -global neighbor ID */
#define RGPD_MGLBLNHBR_ID_MASK	(UINT64_CAST 0xf << 12)
#define RGPD_MGLBLNHBR_VLD_SHFT	11	/* -global neighbor Valid */
#define RGPD_MGLBLNHBR_VLD_MASK	(UINT64_CAST 0x1 << 11)
#define RGPD_MGLBLPORT_SHFT	8	/* -global neighbor Port */
#define RGPD_MGLBLPORT_MASK	(UINT64_CAST 0x7 << 8)
#define RGPD_PGLBLNHBR_ID_SHFT	4	/* +global neighbor ID */
#define RGPD_PGLBLNHBR_ID_MASK	(UINT64_CAST 0xf << 4)
#define RGPD_PGLBLNHBR_VLD_SHFT	3	/* +global neighbor Valid */
#define RGPD_PGLBLNHBR_VLD_MASK	(UINT64_CAST 0x1 << 3)
#define RGPD_PGLBLPORT_SHFT	0	/* +global neighbor Port */
#define RGPD_PGLBLPORT_MASK	(UINT64_CAST 0x7 << 0)

#define GLBL_PARMS_REGS		2	/* Two Global Parms registers */

/*
 * RR_GLOBAL_PARMS0 mask and shift definitions
 */

#define RGPARM0_ARB_VALUE_SHFT	54	/* Local Block Arbitration State */
#define RGPARM0_ARB_VALUE_MASK	(UINT64_CAST 0x7 << 54)
#define RGPARM0_ROTATEARB_SHFT	53	/* Rotate Local Block Arbitration */
#define RGPARM0_ROTATEARB_MASK	(UINT64_CAST 0x1 << 53)
#define RGPARM0_FAIREN_SHFT	52	/* Fairness logic Enable */
#define RGPARM0_FAIREN_MASK	(UINT64_CAST 0x1 << 52)
#define RGPARM0_LOCGNTTO_SHFT	40	/* Local grant timeout */
#define RGPARM0_LOCGNTTO_MASK	(UINT64_CAST 0xfff << 40)
#define RGPARM0_DATELINE_SHFT	38	/* Dateline crossing router */
#define RGPARM0_DATELINE_MASK	(UINT64_CAST 0x1 << 38)
#define RGPARM0_MAXRETRY_SHFT	28	/* Max retry count */
#define RGPARM0_MAXRETRY_MASK	(UINT64_CAST 0x3ff << 28)
#define RGPARM0_URGWRAP_SHFT	20	/* Urgent wrap */
#define RGPARM0_URGWRAP_MASK	(UINT64_CAST 0xff << 20)
#define RGPARM0_DEADLKTO_SHFT	16	/* Deadlock timeout */
#define RGPARM0_DEADLKTO_MASK	(UINT64_CAST 0xf << 16)
#define RGPARM0_URGVAL_SHFT	12	/* Urgent value */
#define RGPARM0_URGVAL_MASK	(UINT64_CAST 0xf << 12)
#define RGPARM0_VCHSELEN_SHFT	11	/* VCH_SEL_EN */
#define RGPARM0_VCHSELEN_MASK	(UINT64_CAST 0x1 << 11)
#define RGPARM0_LOCURGTO_SHFT	9	/* Local urgent timeout */
#define RGPARM0_LOCURGTO_MASK	(UINT64_CAST 0x3 << 9)
#define RGPARM0_TAILVAL_SHFT	5	/* Tail value */
#define RGPARM0_TAILVAL_MASK	(UINT64_CAST 0xf << 5)
#define RGPARM0_CLOCK_SHFT	1	/* Global clock select */
#define RGPARM0_CLOCK_MASK	(UINT64_CAST 0xf << 1)
#define RGPARM0_BYPEN_SHFT	0
#define RGPARM0_BYPEN_MASK	(UINT64_CAST 1)	/* Bypass enable */

/*
 * RR_GLOBAL_PARMS1 shift and mask definitions
 */

#define RGPARM1_TTOWRAP_SHFT	12	/* Tail timeout wrap */
#define RGPARM1_TTOWRAP_MASK	(UINT64_CAST 0xfffff << 12)
#define RGPARM1_AGERATE_SHFT	8	/* Age rate */
#define RGPARM1_AGERATE_MASK	(UINT64_CAST 0xf << 8)
#define RGPARM1_JSWSTAT_SHFT	0	/* JTAG Sw Register bits */
#define RGPARM1_JSWSTAT_MASK	(UINT64_CAST 0xff << 0)

/*
 * RR_DIAG_PARMS mask and shift definitions
 */

#define RDPARM_ABSHISTOGRAM	(UINT64_CAST 1 << 17)	/* Absolute histgrm */
#define RDPARM_DEADLOCKRESET	(UINT64_CAST 1 << 16)	/* Reset on deadlck */
#define RDPARM_DISABLE(_L)	(UINT64_CAST 1 << ((_L) +  7))
#define RDPARM_SENDERROR(_L)	(UINT64_CAST 1 << ((_L) -  1))

/*
 * RR_DEBUG_ADDR mask and shift definitions
 */

#define RDA_DATA_SHFT		10	/* Observed debug data */
#define RDA_DATA_MASK		(UINT64_CAST 0xffff << 10)
#define RDA_ADDR_SHFT		0	/* debug address for data */
#define RDA_ADDR_MASK		(UINT64_CAST 0x3ff << 0)

/*
 * RR_LB_TO_L2 mask and shift definitions
 */

#define RLBTOL2_DATA_VLD_SHFT	32	/* data is valid for JTAG controller */
#define RLBTOL2_DATA_VLD_MASK	(UINT64_CAST 0x1 << 32)
#define RLBTOL2_DATA_SHFT	0	/* data bits for JTAG controller */
#define RLBTOL2_DATA_MASK	(UINT64_CAST 0xffffffff)

/*
 * RR_L2_TO_LB mask and shift definitions
 */

#define RL2TOLB_DATA_VLD_SHFT	33	/* data is valid from JTAG controller */
#define RL2TOLB_DATA_VLD_MASK	(UINT64_CAST 0x1 << 33)
#define RL2TOLB_PARITY_SHFT	32	/* sw implemented parity for data */
#define RL2TOLB_PARITY_MASK	(UINT64_CAST 0x1 << 32)
#define RL2TOLB_DATA_SHFT	0	/* data bits from JTAG controller */
#define RL2TOLB_DATA_MASK	(UINT64_CAST 0xffffffff)

/*
 * RR_JBUS_CONTROL mask and shift definitions
 */

#define RJC_POS_BITS_SHFT	20	/* Router position bits */
#define RJC_POS_BITS_MASK	(UINT64_CAST 0xf << 20)
#define RJC_RD_DATA_STROBE_SHFT	16	/* count when read data is strobed in */
#define RJC_RD_DATA_STROBE_MASK	(UINT64_CAST 0xf << 16)
#define RJC_WE_OE_HOLD_SHFT	8	/* time OE or WE is held */
#define RJC_WE_OE_HOLD_MASK	(UINT64_CAST 0xff << 8)
#define RJC_ADDR_SET_HLD_SHFT	0	/* time address driven around OE/WE */
#define RJC_ADDR_SET_HLD_MASK	(UINT64_CAST 0xff)

/*
 * RR_SCRATCH_REGx mask and shift definitions
 *  note: these fields represent a software convention, and are not
 *        understood/interpreted by the hardware. 
 */

#define	RSCR0_BOOTED_SHFT	63
#define	RSCR0_BOOTED_MASK	(UINT64_CAST 0x1 << RSCR0_BOOTED_SHFT)
#define RSCR0_LOCALID_SHFT	56
#define RSCR0_LOCALID_MASK	(UINT64_CAST 0x7f << RSCR0_LOCALID_SHFT)
#define	RSCR0_UNUSED_SHFT	48
#define	RSCR0_UNUSED_MASK	(UINT64_CAST 0xff << RSCR0_UNUSED_SHFT)
#define RSCR0_NIC_SHFT		0
#define RSCR0_NIC_MASK		(UINT64_CAST 0xffffffffffff)

#define RSCR1_MODID_SHFT	0
#define RSCR1_MODID_MASK	(UINT64_CAST 0xffff)

/*
 * RR_VECTOR_HW_BAR mask and shift definitions
 */

#define BAR_TX_SHFT		27	/* Barrier in trans(m)it when read */
#define BAR_TX_MASK		(UINT64_CAST 1 << BAR_TX_SHFT)
#define BAR_VLD_SHFT		26	/* Valid Configuration */
#define BAR_VLD_MASK		(UINT64_CAST 1 << BAR_VLD_SHFT)
#define BAR_SEQ_SHFT		24	/* Sequence number */
#define BAR_SEQ_MASK		(UINT64_CAST 3 << BAR_SEQ_SHFT)
#define BAR_LEAFSTATE_SHFT	18	/* Leaf State */
#define BAR_LEAFSTATE_MASK	(UINT64_CAST 0x3f << BAR_LEAFSTATE_SHFT)
#define BAR_PARENT_SHFT		14	/* Parent Port */
#define BAR_PARENT_MASK		(UINT64_CAST 0xf << BAR_PARENT_SHFT)
#define BAR_CHILDREN_SHFT	6	/* Child Select port bits */
#define BAR_CHILDREN_MASK	(UINT64_CAST 0xff << BAR_CHILDREN_SHFT)
#define BAR_LEAFCOUNT_SHFT	0	/* Leaf Count to trigger parent */
#define BAR_LEAFCOUNT_MASK	(UINT64_CAST 0x3f)

/*
 * RR_PORT_PARMS(_L) mask and shift definitions
 */

#define RPPARM_MIPRESETEN_SHFT	29	/* Message In Progress reset enable */
#define RPPARM_MIPRESETEN_MASK	(UINT64_CAST 0x1 << 29)
#define RPPARM_UBAREN_SHFT	28	/* Enable user barrier requests */
#define RPPARM_UBAREN_MASK	(UINT64_CAST 0x1 << 28)
#define RPPARM_OUTPDTO_SHFT	24	/* Output Port Deadlock TO value */
#define RPPARM_OUTPDTO_MASK	(UINT64_CAST 0xf << 24)
#define RPPARM_PORTMATE_SHFT	21	/* Port Mate for the port */
#define RPPARM_PORTMATE_MASK	(UINT64_CAST 0x7 << 21)
#define RPPARM_HISTEN_SHFT	20	/* Histogram counter enable */
#define RPPARM_HISTEN_MASK	(UINT64_CAST 0x1 << 20)
#define RPPARM_HISTSEL_SHFT	18
#define RPPARM_HISTSEL_MASK	(UINT64_CAST 0x3 << 18)
#define RPPARM_DAMQHS_SHFT	16
#define RPPARM_DAMQHS_MASK	(UINT64_CAST 0x3 << 16)
#define RPPARM_NULLTO_SHFT	10
#define RPPARM_NULLTO_MASK	(UINT64_CAST 0x3f << 10)
#define RPPARM_MAXBURST_SHFT	0
#define RPPARM_MAXBURST_MASK	(UINT64_CAST 0x3ff)

/*
 * NOTE: Normally the kernel tracks only UTILIZATION statistics.
 * The other 2 should not be used, except during any experimentation
 * with the router.
 */
#define RPPARM_HISTSEL_AGE	0	/* Histogram age characterization.  */
#define RPPARM_HISTSEL_UTIL	1	/* Histogram link utilization 	    */
#define RPPARM_HISTSEL_DAMQ	2	/* Histogram DAMQ characterization. */

/*
 * RR_STATUS_ERROR(_L) and RR_ERROR_CLEAR(_L) mask and shift definitions
 */
#define RSERR_POWERNOK		(UINT64_CAST 1 << 38)
#define RSERR_PORT_DEADLOCK     (UINT64_CAST 1 << 37)
#define RSERR_WARMRESET         (UINT64_CAST 1 << 36)
#define RSERR_LINKRESET         (UINT64_CAST 1 << 35)
#define RSERR_RETRYTIMEOUT      (UINT64_CAST 1 << 34)
#define RSERR_FIFOOVERFLOW	(UINT64_CAST 1 << 33)
#define RSERR_ILLEGALPORT	(UINT64_CAST 1 << 32)
#define RSERR_DEADLOCKTO_SHFT	28
#define RSERR_DEADLOCKTO_MASK	(UINT64_CAST 0xf << 28)
#define RSERR_RECVTAILTO_SHFT	24
#define RSERR_RECVTAILTO_MASK	(UINT64_CAST 0xf << 24)
#define RSERR_RETRYCNT_SHFT	16
#define RSERR_RETRYCNT_MASK	(UINT64_CAST 0xff << 16)
#define RSERR_CBERRCNT_SHFT	8
#define RSERR_CBERRCNT_MASK	(UINT64_CAST 0xff << 8)
#define RSERR_SNERRCNT_SHFT	0
#define RSERR_SNERRCNT_MASK	(UINT64_CAST 0xff << 0)


#define PORT_STATUS_UP		(1 << 0)	/* Router link up */
#define PORT_STATUS_FENCE	(1 << 1)	/* Router link fenced */
#define PORT_STATUS_RESETFAIL	(1 << 2)	/* Router link didnot 
						 * come out of reset */
#define PORT_STATUS_DISCFAIL	(1 << 3)	/* Router link failed after 
						 * out of reset but before
						 * router tables were
						 * programmed
						 */
#define PORT_STATUS_KERNFAIL	(1 << 4)	/* Router link failed
						 * after reset and the 
						 * router tables were
						 * programmed
						 */
#define PORT_STATUS_UNDEF	(1 << 5)	/* Unable to pinpoint
						 * why the router link
						 * went down
						 */	
#define PROBE_RESULT_BAD	(-1)		/* Set if any of the router
						 * links failed after reset
						 */
#define PROBE_RESULT_GOOD	(0)		/* Set if all the router links
						 * which came out of reset 
						 * are up
						 */

/* Should be enough for 256 CPUs */
#define MAX_RTR_BREADTH		64		/* Max # of routers possible */

/* Get the require set of bits in a var. corr to a sequence of bits  */
#define GET_FIELD(var, fname) \
        ((var) >> fname##_SHFT & fname##_MASK >> fname##_SHFT)
/* Set the require set of bits in a var. corr to a sequence of bits  */
#define SET_FIELD(var, fname, fval) \
        ((var) = (var) & ~fname##_MASK | (uint64_t) (fval) << fname##_SHFT)


#ifndef __ASSEMBLY__

typedef struct router_map_ent_s {
	uint64_t	nic;
	moduleid_t	module;
	slotid_t	slot;
} router_map_ent_t;

struct rr_status_error_fmt {
	uint64_t	rserr_unused		: 30,
			rserr_fifooverflow	: 1,
			rserr_illegalport	: 1,
			rserr_deadlockto	: 4,
			rserr_recvtailto	: 4,
			rserr_retrycnt		: 8,
			rserr_cberrcnt		: 8,
			rserr_snerrcnt		: 8;
};

/*
 * This type is used to store "absolute" counts of router events
 */
typedef int	router_count_t;

/* All utilizations are on a scale from 0 - 1023. */
#define RP_BYPASS_UTIL	0
#define RP_RCV_UTIL	1
#define RP_SEND_UTIL	2
#define RP_TOTAL_PKTS	3	/* Free running clock/packet counter */

#define RP_NUM_UTILS	3

#define RP_HIST_REGS	2
#define RP_NUM_BUCKETS  4
#define RP_HIST_TYPES	3

#define RP_AGE0		0
#define RP_AGE1		1
#define RP_AGE2		2
#define RP_AGE3		3


#define RR_UTIL_SCALE	1024

/*
 * Router port-oriented information
 */
typedef struct router_port_info_s {
	router_reg_t	rp_histograms[RP_HIST_REGS];/* Port usage info */
	router_reg_t	rp_port_error;		/* Port error info */
	router_count_t	rp_retry_errors;	/* Total retry errors */
	router_count_t	rp_sn_errors;		/* Total sn errors */
	router_count_t	rp_cb_errors;		/* Total cb errors */
	int		rp_overflows;		/* Total count overflows */
	int		rp_excess_err;		/* Port has excessive errors */
	ushort		rp_util[RP_NUM_BUCKETS];/* Port utilization */
} router_port_info_t;

#define ROUTER_INFO_VERSION	7

struct lboard_s;

/*
 * Router information
 */
typedef struct router_info_s {
	char		ri_version;	/* structure version		    */
	cnodeid_t	ri_cnode;	/* cnode of its legal guardian hub  */
	nasid_t		ri_nasid;	/* Nasid of same 		    */
	char		ri_ledcache;	/* Last LED bitmap		    */
	char		ri_leds;	/* Current LED bitmap		    */
	char		ri_portmask;	/* Active port bitmap		    */
	router_reg_t	ri_stat_rev_id;	/* Status rev ID value		    */
	net_vec_t	ri_vector;	/* vector from guardian to router   */
	int		ri_writeid;	/* router's vector write ID	    */
	int64_t	ri_timebase;	/* Time of first sample		    */
	int64_t	ri_timestamp;	/* Time of last sample		    */
	router_port_info_t ri_port[MAX_ROUTER_PORTS]; /* per port info      */
	moduleid_t	ri_module;	/* Which module are we in?	    */
	slotid_t	ri_slotnum;	/* Which slot are we in?	    */
	router_reg_t	ri_glbl_parms[GLBL_PARMS_REGS];
					/* Global parms0&1 register contents*/
	vertex_hdl_t	ri_vertex;	/* hardware graph vertex            */
	router_reg_t	ri_prot_conf;	/* protection config. register	    */
	int64_t	ri_per_minute;	/* Ticks per minute		    */

	/*
	 * Everything below here is for kernel use only and may change at	
	 * at any time with or without a change in the revision number
	 *
	 * Any pointers or things that come and go with DEBUG must go at
 	 * the bottom of the structure, below the user stuff.
	 */
	char		ri_hist_type;   /* histogram type		    */
	vertex_hdl_t	ri_guardian;	/* guardian node for the router	    */
	int64_t	ri_last_print;	/* When did we last print	    */
	char		ri_print;	/* Should we print 		    */
	char 		ri_just_blink;	/* Should we blink the LEDs         */
	
#ifdef DEBUG
	int64_t	ri_deltatime;	/* Time it took to sample	    */
#endif
	spinlock_t	ri_lock;	/* Lock for access to router info   */
	net_vec_t	*ri_vecarray;	/* Pointer to array of vectors	    */
	struct lboard_s	*ri_brd;	/* Pointer to board structure	    */
	char *		ri_name;	/* This board's hwg path 	    */
        unsigned char	ri_port_maint[MAX_ROUTER_PORTS]; /* should we send a 
					message to availmon */
} router_info_t;


/* Router info location specifiers */

#define RIP_PROMLOG			2	/* Router info in promlog */
#define RIP_CONSOLE			4	/* Router info on console */

#define ROUTER_INFO_PRINT(_rip,_where)	(_rip->ri_print |= _where)	
					/* Set the field used to check if a 
					 * router info can be printed
					 */
#define IS_ROUTER_INFO_PRINTED(_rip,_where)	\
					(_rip->ri_print & _where)	
					/* Was the router info printed to
					 * the given location (_where) ?
					 * Mainly used to prevent duplicate
					 * router error states.
					 */
#define ROUTER_INFO_LOCK(_rip,_s)	_s = mutex_spinlock(&(_rip->ri_lock))
					/* Take the lock on router info
					 * to gain exclusive access
					 */
#define ROUTER_INFO_UNLOCK(_rip,_s)	mutex_spinunlock(&(_rip->ri_lock),_s)
					/* Release the lock on router info */
/* 
 * Router info hanging in the nodepda 
 */
typedef struct nodepda_router_info_s {
	vertex_hdl_t 	router_vhdl;	/* vertex handle of the router 	    */
	short		router_port;	/* port thru which we entered       */
	short		router_portmask;
	moduleid_t	router_module;	/* module in which router is there  */
	slotid_t	router_slot;	/* router slot			    */
	unsigned char	router_type;	/* kind of router 		    */
	net_vec_t	router_vector;	/* vector from the guardian node    */

	router_info_t	*router_infop;	/* info hanging off the hwg vertex  */
	struct nodepda_router_info_s *router_next;
	                                /* pointer to next element 	    */
} nodepda_router_info_t;

#define ROUTER_NAME_SIZE	20	/* Max size of a router name */

#define NORMAL_ROUTER_NAME	"normal_router"
#define NULL_ROUTER_NAME	"null_router"
#define META_ROUTER_NAME	"meta_router"
#define REPEATER_ROUTER_NAME	"repeater_router"
#define UNKNOWN_ROUTER_NAME	"unknown_router" 

/* The following definitions are needed by the router traversing
 * code either using the hardware graph or using vector operations.
 */
/* Structure of the router queue element */
typedef struct router_elt_s {
	union {
		/* queue element structure during router probing */
		struct {
			/* number-in-a-can (unique) for the router */
			nic_t		nic;	
			/* vector route from the master hub to 
			 * this router.
			 */
			net_vec_t	vec;	
			/* port status */
			uint64_t	status;	
			char		port_status[MAX_ROUTER_PORTS + 1];
		} r_elt;
		/* queue element structure during router guardian 
		 * assignment
		 */
		struct {
			/* vertex handle for the router */
			vertex_hdl_t	vhdl;
			/* guardian for this router */
			vertex_hdl_t	guard;	
			/* vector router from the guardian to the router */
			net_vec_t	vec;
		} k_elt;
	} u;
	                        /* easy to use port status interpretation */
} router_elt_t;

/* structure of the router queue */

typedef struct router_queue_s {
	char		head;	/* Point where a queue element is inserted */
	char		tail;	/* Point where a queue element is removed */
	int		type;
	router_elt_t	array[MAX_RTR_BREADTH];
	                        /* Entries for queue elements */
} router_queue_t;


#endif /* __ASSEMBLY__ */

/*
 * RR_HISTOGRAM(_L) mask and shift definitions
 * There are two 64 bit histogram registers, so the following macros take
 * into account dealing with an array of 4 32 bit values indexed by _x
 */

#define RHIST_BUCKET_SHFT(_x)	(32 * ((_x) & 0x1))
#define RHIST_BUCKET_MASK(_x)	(UINT64_CAST 0xffffffff << RHIST_BUCKET_SHFT((_x) & 0x1))
#define RHIST_GET_BUCKET(_x, _reg)	\
	((RHIST_BUCKET_MASK(_x) & ((_reg)[(_x) >> 1])) >> RHIST_BUCKET_SHFT(_x))

/*
 * RR_RESET_MASK(_L) mask and shift definitions
 */

#define RRM_RESETOK(_L)		(UINT64_CAST 1 << ((_L) - 1))
#define RRM_RESETOK_ALL		ALL_PORTS

/*
 * RR_META_TABLE(_x) and RR_LOCAL_TABLE(_x) mask and shift definitions
 */

#define RTABLE_SHFT(_L)		(4 * ((_L) - 1))
#define RTABLE_MASK(_L)		(UINT64_CAST 0x7 << RTABLE_SHFT(_L))


#define	ROUTERINFO_STKSZ	4096

#ifndef __ASSEMBLY__

int router_reg_read(router_info_t *rip, int regno, router_reg_t *val);
int router_reg_write(router_info_t *rip, int regno, router_reg_t val);
int router_get_info(vertex_hdl_t routerv, router_info_t *, int);
int router_set_leds(router_info_t *rip);
void router_print_state(router_info_t *rip, int level,
		   void (*pf)(int, char *, ...),int print_where);
void capture_router_stats(router_info_t *rip);


int 	probe_routers(void);
void 	get_routername(unsigned char brd_type,char *rtrname);
void 	router_guardians_set(vertex_hdl_t hwgraph_root);
int 	router_hist_reselect(router_info_t *, int64_t);
#endif /* __ASSEMBLY__ */

#endif /* _ASM_IA64_SN_ROUTER_H */
