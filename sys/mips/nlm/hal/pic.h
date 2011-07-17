/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __XLP_PIC_H__
#define __XLP_PIC_H__

/* PIC Specific registers */
#define XLP_PIC_CTRL_REG		0x40
#define XLP_PIC_BYTESWAP_REG		0x42
#define XLP_PIC_STATUS_REG		0x44
#define XLP_PIC_INTR_TIMEOUT		0x46
#define XLP_PIC_ICI0_INTR_TIMEOUT	0x48
#define XLP_PIC_ICI1_INTR_TIMEOUT	0x4a
#define XLP_PIC_ICI2_INTR_TIMEOUT	0x4c
#define XLP_PIC_IPI_CTRL_REG		0x4e
#define XLP_PIC_INT_ACK_REG		0x50
#define XLP_PIC_INT_PENDING0_REG	0x52
#define XLP_PIC_INT_PENDING1_REG	0x54
#define XLP_PIC_INT_PENDING2_REG	0x56

#define XLP_PIC_WDOG0_MAXVAL_REG	0x58
#define XLP_PIC_WDOG0_COUNT_REG		0x5a
#define XLP_PIC_WDOG0_ENABLE0_REG	0x5c
#define XLP_PIC_WDOG0_ENABLE1_REG	0x5e
#define XLP_PIC_WDOG0_BEATCMD_REG	0x60
#define XLP_PIC_WDOG0_BEAT0_REG		0x62
#define XLP_PIC_WDOG0_BEAT1_REG		0x64

#define XLP_PIC_WDOG1_MAXVAL_REG	0x66
#define XLP_PIC_WDOG1_COUNT_REG		0x68
#define XLP_PIC_WDOG1_ENABLE0_REG	0x6a
#define XLP_PIC_WDOG1_ENABLE1_REG	0x6c
#define XLP_PIC_WDOG1_BEATCMD_REG	0x6e
#define XLP_PIC_WDOG1_BEAT0_REG		0x70
#define XLP_PIC_WDOG1_BEAT1_REG		0x72

#define XLP_PIC_WDOG_MAXVAL_REG(i)	(XLP_PIC_WDOG0_MAXVAL_REG + ((i) ? 7 : 0))
#define XLP_PIC_WDOG_COUNT_REG(i)	(XLP_PIC_WDOG0_COUNT_REG + ((i) ? 7 : 0))
#define XLP_PIC_WDOG_ENABLE0_REG(i)	(XLP_PIC_WDOG0_ENABLE0_REG + ((i) ? 7 : 0))
#define XLP_PIC_WDOG_ENABLE1_REG(i)	(XLP_PIC_WDOG0_ENABLE1_REG + ((i) ? 7 : 0))
#define XLP_PIC_WDOG_BEATCMD_REG(i)	(XLP_PIC_WDOG0_BEATCMD_REG + ((i) ? 7 : 0))
#define XLP_PIC_WDOG_BEAT0_REG(i)	(XLP_PIC_WDOG0_BEAT0_REG + ((i) ? 7 : 0))
#define XLP_PIC_WDOG_BEAT1_REG(i)	(XLP_PIC_WDOG0_BEAT1_REG + ((i) ? 7 : 0))

#define XLP_PIC_SYSTIMER0_MAXVAL_REG	0x74
#define XLP_PIC_SYSTIMER1_MAXVAL_REG	0x76
#define XLP_PIC_SYSTIMER2_MAXVAL_REG	0x78
#define XLP_PIC_SYSTIMER3_MAXVAL_REG	0x7a
#define XLP_PIC_SYSTIMER4_MAXVAL_REG	0x7c
#define XLP_PIC_SYSTIMER5_MAXVAL_REG	0x7e
#define XLP_PIC_SYSTIMER6_MAXVAL_REG	0x80
#define XLP_PIC_SYSTIMER7_MAXVAL_REG	0x82
#define XLP_PIC_SYSTIMER_MAXVAL_REG(i)	(XLP_PIC_SYSTIMER0_MAXVAL_REG + ((i)*2))

#define XLP_PIC_SYSTIMER0_COUNT_REG	0x84
#define XLP_PIC_SYSTIMER1_COUNT_REG	0x86
#define XLP_PIC_SYSTIMER2_COUNT_REG	0x88
#define XLP_PIC_SYSTIMER3_COUNT_REG	0x8a
#define XLP_PIC_SYSTIMER4_COUNT_REG	0x8c
#define XLP_PIC_SYSTIMER5_COUNT_REG	0x8e
#define XLP_PIC_SYSTIMER6_COUNT_REG	0x90
#define XLP_PIC_SYSTIMER7_COUNT_REG	0x92
#define XLP_PIC_SYSTIMER_COUNT_REG(i)	(XLP_PIC_SYSTIMER0_COUNT_REG + ((i)*2))

#define XLP_PIC_ITE0_N0_N1_REG		0x94
#define XLP_PIC_ITE1_N0_N1_REG		0x98
#define XLP_PIC_ITE2_N0_N1_REG		0x9c
#define XLP_PIC_ITE3_N0_N1_REG		0xa0
#define XLP_PIC_ITE4_N0_N1_REG		0xa4
#define XLP_PIC_ITE5_N0_N1_REG		0xa8
#define XLP_PIC_ITE6_N0_N1_REG		0xac
#define XLP_PIC_ITE7_N0_N1_REG		0xb0
#define XLP_PIC_ITE_N0_N1_REG(i)	(XLP_PIC_ITE0_N0_N1_REG + ((i)*4))

#define XLP_PIC_ITE0_N2_N3_REG		0x96
#define XLP_PIC_ITE1_N2_N3_REG		0x9a
#define XLP_PIC_ITE2_N2_N3_REG		0x9e
#define XLP_PIC_ITE3_N2_N3_REG		0xa2
#define XLP_PIC_ITE4_N2_N3_REG		0xa6
#define XLP_PIC_ITE5_N2_N3_REG		0xaa
#define XLP_PIC_ITE6_N2_N3_REG		0xae
#define XLP_PIC_ITE7_N2_N3_REG		0xb2
#define XLP_PIC_ITE_N2_N3_REG(i)	(XLP_PIC_ITE0_N2_N3_REG + ((i)*4))

#define XLP_PIC_IRT0_REG		0xb4
#define XLP_PIC_IRT_REG(i)		(XLP_PIC_IRT0_REG + ((i)*2))

/* PIC IRT indices */

#define XLP_PIC_IRT_WD0_INDEX		0
#define XLP_PIC_IRT_WD1_INDEX		1
#define XLP_PIC_IRT_WD_NMI0_INDEX	2
#define XLP_PIC_IRT_WD_NMI1_INDEX	3
#define XLP_PIC_IRT_TIMER0_INDEX	4
#define XLP_PIC_IRT_TIMER1_INDEX	5
#define XLP_PIC_IRT_TIMER2_INDEX	6
#define XLP_PIC_IRT_TIMER3_INDEX	7
#define XLP_PIC_IRT_TIMER4_INDEX	8
#define XLP_PIC_IRT_TIMER5_INDEX	9
#define XLP_PIC_IRT_TIMER6_INDEX	10
#define XLP_PIC_IRT_TIMER7_INDEX	11
#define XLP_PIC_IRT_TIMER_INDEX(i)	(XLP_PIC_IRT_TIMER0_INDEX + (i))

#define XLP_PIC_IRT_MSGQ0_INDEX		12
#define XLP_PIC_IRT_MSGQ_INDEX(i)	(XLP_PIC_IRT_MSGQ0_INDEX + (i))
/* 12 to 43 */
#define XLP_PIC_IRT_MSG0_INDEX		44
#define XLP_PIC_IRT_MSG1_INDEX		45

#define XLP_PIC_IRT_PCIE_MSIX0_INDEX	46
#define XLP_PIC_IRT_PCIE_MSIX_INDEX(i)	(XLP_PIC_IRT_PCIE_MSIX0_INDEX + (i))
/* 46 to 77 */
#define XLP_PIC_IRT_PCIE_LINK0_INDEX	78
#define XLP_PIC_IRT_PCIE_LINK1_INDEX	79
#define XLP_PIC_IRT_PCIE_LINK2_INDEX	80
#define XLP_PIC_IRT_PCIE_LINK3_INDEX	81
#define XLP_PIC_IRT_PCIE_LINK_INDEX(i)	(XLP_PIC_IRT_PCIE_LINK0_INDEX + (i))
/* 78 to 81 */
#define XLP_PIC_IRT_NA0_INDEX		82
#define XLP_PIC_IRT_NA_INDEX(i)		(XLP_PIC_IRT_NA0_INDEX + (i))
/* 82 to 113 */
#define XLP_PIC_IRT_POE_INDEX		114
#define XLP_PIC_IRT_USB0_INDEX		115
#define XLP_PIC_IRT_EHCI0_INDEX		115
#define XLP_PIC_IRT_EHCI1_INDEX		118
#define XLP_PIC_IRT_USB_INDEX(i)	(XLP_PIC_IRT_USB0_INDEX + (i))
/* 115 to 120 */
#define XLP_PIC_IRT_GDX_INDEX		121
#define XLP_PIC_IRT_SEC_INDEX		122
#define XLP_PIC_IRT_RSA_INDEX		123
#define XLP_PIC_IRT_COMP0_INDEX		124
#define XLP_PIC_IRT_COMP_INDEX(i)	(XLP_PIC_IRT_COMP0_INDEX + (i))
/* 124 to 127 */
#define XLP_PIC_IRT_GBU_INDEX		128
/* coherent inter chip */
#define XLP_PIC_IRT_CIC0_INDEX		129
#define XLP_PIC_IRT_CIC1_INDEX		130
#define XLP_PIC_IRT_CIC2_INDEX		131
#define XLP_PIC_IRT_CAM_INDEX		132
#define XLP_PIC_IRT_UART0_INDEX		133
#define XLP_PIC_IRT_UART1_INDEX		134
#define XLP_PIC_IRT_I2C0_INDEX		135
#define XLP_PIC_IRT_I2C1_INDEX		136
#define XLP_PIC_IRT_SYS0_INDEX		137
#define XLP_PIC_IRT_SYS1_INDEX		138
#define XLP_PIC_IRT_JTAG_INDEX		139
#define XLP_PIC_IRT_PIC_INDEX		140
#define XLP_PIC_IRT_NBU_INDEX		141
#define XLP_PIC_IRT_TCU_INDEX		142
/* global coherency */
#define XLP_PIC_IRT_GCU_INDEX		143
#define XLP_PIC_IRT_DMC0_INDEX		144
#define XLP_PIC_IRT_DMC1_INDEX		145
#define XLP_PIC_IRT_GPIO0_INDEX		146
#define XLP_PIC_IRT_GPIO_INDEX(i)	(XLP_PIC_IRT_GPIO0_INDEX + (i))
/* 146 to 149 */
#define XLP_PIC_IRT_NOR_INDEX		150
#define XLP_PIC_IRT_NAND_INDEX		151
#define XLP_PIC_IRT_SPI_INDEX		152
#define XLP_PIC_IRT_MMC_INDEX		153

/* PIC control register defines */
#define XLP_PIC_ITV_OFFSET		32 /* interrupt timeout value */
#define XLP_PIC_ICI_OFFSET		19 /* ICI interrupt timeout enable */
#define XLP_PIC_ITE_OFFSET		18 /* interrupt timeout enable */
#define XLP_PIC_STE_OFFSET		10 /* system timer interrupt enable */
#define XLP_PIC_WWR1_OFFSET		8  /* watchdog timer 1 wraparound count for reset */
#define XLP_PIC_WWR0_OFFSET		6  /* watchdog timer 0 wraparound count for reset */
#define XLP_PIC_WWN1_OFFSET		4  /* watchdog timer 1 wraparound count for NMI */
#define XLP_PIC_WWN0_OFFSET		2  /* watchdog timer 0 wraparound count for NMI */
#define XLP_PIC_WTE_OFFSET		0  /* watchdog timer enable */

/* PIC Status register defines */
#define XLP_PIC_ICI_STATUS_OFFSET	33 /* ICI interrupt timeout interrupt status */
#define XLP_PIC_ITE_STATUS_OFFSET	32 /* interrupt timeout interrupt status */
#define XLP_PIC_STS_STATUS_OFFSET	4  /* System timer interrupt status */
#define XLP_PIC_WNS_STATUS_OFFSET	2  /* NMI interrupt status for watchdog timers */
#define XLP_PIC_WIS_STATUS_OFFSET	0  /* Interrupt status for watchdog timers */

/* PIC IPI control register offsets */
#define XLP_PIC_IPICTRL_NMI_OFFSET	32
#define XLP_PIC_IPICTRL_RIV_OFFSET	20 /* received interrupt vector */
#define XLP_PIC_IPICTRL_IDB_OFFSET	16 /* interrupt destination base */
#define XLP_PIC_IPICTRL_DTE_OFFSET	16 /* interrupt destination thread enables */

/* PIC IRT register offsets */
#define XLP_PIC_IRT_ENABLE_OFFSET	31
#define XLP_PIC_IRT_NMI_OFFSET		29
#define XLP_PIC_IRT_SCH_OFFSET		28 /* Scheduling scheme */
#define XLP_PIC_IRT_RVEC_OFFSET		20 /* Interrupt receive vectors */
#define XLP_PIC_IRT_DT_OFFSET		19 /* Destination type */
#define XLP_PIC_IRT_DB_OFFSET		16 /* Destination base */
#define XLP_PIC_IRT_DTE_OFFSET		0  /* Destination thread enables */

#define XLP_PIC_MAX_IRQ			64
#define XLP_PIC_MAX_IRT			160
#define XLP_PIC_TIMER_FREQ		133000000

#if !defined(LOCORE) && !defined(__ASSEMBLY__)

#define	nlm_rdreg_pic(b, r)		nlm_read_reg64_kseg(b,r)
#define	nlm_wreg_pic(b, r, v)		nlm_write_reg64_kseg(b,r,v)
#define nlm_pcibase_pic(node)		nlm_pcicfg_base(XLP_IO_PIC_OFFSET(node))
#define nlm_regbase_pic(node)		nlm_pcibase_pic(node)

/* IRT and h/w interrupt routines */
static __inline__ int
nlm_pic_get_numirts(uint64_t pcibase)
{
	return (nlm_pci_rdreg(pcibase, XLP_PCI_IRTINFO_REG) >> 16);
}

static __inline__ int
nlm_pic_get_startirt(uint64_t base)
{
	return (nlm_pci_rdreg(base, XLP_PCI_IRTINFO_REG) & 0xff);
}


static __inline__ int
nlm_pic_read_irt(uint64_t base, int irt_index)
{
	return nlm_rdreg_pic(base, XLP_PIC_IRT_REG(irt_index));
}

/* IRT's can be written into in two modes
 * ITE mode - Here the destination of the interrupt is one of the
 *   eight interrupt-thread-enable groups, allowing the interrupt
 *   to be distributed to any thread on any node
 * ID mode - In ID mode, the IRT has the DB and DTE fields.
 *   DB[18:17] hold the node select and DB[16], if set to 0 selects
 *   cpu-cores 0-3, and if set to 1 selects cpu-cores 4-7.
 *   The DTE[15:0] field is a thread mask, allowing the PIC to broadcast
 *   the interrupt to 1-16 threads selectable from that mask
 */

static __inline__ void
nlm_pic_write_irt_raw(uint64_t base, int irt_index, int en, int nmi, int sch,
		int vec, int dt, int db, int dte)
{
	uint64_t val =
		(((en & 0x1) << XLP_PIC_IRT_ENABLE_OFFSET) |
		 ((nmi & 0x1) << XLP_PIC_IRT_NMI_OFFSET) |
		 ((sch & 0x1) << XLP_PIC_IRT_SCH_OFFSET) |
		 ((vec & 0x3f) << XLP_PIC_IRT_RVEC_OFFSET) |
		 ((dt & 0x1 ) << XLP_PIC_IRT_DT_OFFSET) |
		 ((db & 0x7) << XLP_PIC_IRT_DB_OFFSET) |
		 (dte & 0xffff));
	nlm_wreg_pic(base, XLP_PIC_IRT_REG(irt_index), val);
}

/* write IRT in ID mode */
static __inline__ void
nlm_pic_write_irt_id(uint64_t base, int irt_index, int en, int nmi, int vec,
		int node, int cpugroup, uint32_t cpu_mask)
{
	nlm_pic_write_irt_raw(base, irt_index, en, nmi, 1, vec, 1,
			(node << 1) | cpugroup , cpu_mask);
}

/* write IRT in ITE mode */
static __inline__ void
nlm_pic_write_ite(uint64_t base, int ite, uint32_t node0_thrmask,
	uint32_t node1_thrmask, uint32_t node2_thrmask, uint32_t node3_thrmask)
{
	uint64_t tm10 = ((uint64_t)node1_thrmask << 32) | node0_thrmask;
	uint64_t tm32 = ((uint64_t)node1_thrmask << 32) | node0_thrmask;

	/* Enable the ITE register for all nodes */
	nlm_wreg_pic(base, XLP_PIC_ITE_N0_N1_REG(ite), tm10);
	nlm_wreg_pic(base, XLP_PIC_ITE_N2_N3_REG(ite), tm32);
}

static __inline__ void
nlm_pic_write_irt_ite(uint64_t base, int irt_index, int ite, int en, int nmi,
		int sch, int vec)
{
	nlm_pic_write_irt_raw(base, irt_index, en, nmi, sch, vec, 0, ite, 0);
}

/* Goto PIC on that node, and ack the interrupt */
static __inline__ void nlm_pic_ack(uint64_t src_base, int irt)
{
	nlm_wreg_pic(src_base, XLP_PIC_INT_ACK_REG, irt);
	/* ack in the status registers for watchdog and system timers */
	if (irt < 12)
		nlm_wreg_pic(src_base, XLP_PIC_STATUS_REG, (1 << irt));
}

/* IPI routines */

static __inline__ void
nlm_pic_send_ipi(uint64_t local_base, int target_node, int vcpu, int vec, int nmi)
{
	uint64_t ipi =
		(((uint64_t)nmi << XLP_PIC_IPICTRL_NMI_OFFSET) |
		(vec << XLP_PIC_IPICTRL_RIV_OFFSET) |
		(target_node << 17) |
		(1 << (vcpu & 0xf)));
	if (vcpu > 15)
		ipi |= 0x10000; /* set bit 16 to select cpus 16-31 */

	nlm_wreg_pic(local_base, XLP_PIC_IPI_CTRL_REG, ipi);
}

/* System Timer routines -- broadcasts systemtimer to 16 vcpus defined in cpu_mask */

static __inline__ void
nlm_pic_set_systimer(uint64_t base, int timer, uint64_t value, int irq, int node,
		int cpugroup, uint32_t cpumask)
{
	uint64_t pic_ctrl = nlm_rdreg_pic(base, XLP_PIC_CTRL_REG);
	int en;

	en = (cpumask != 0);
	nlm_wreg_pic(base, XLP_PIC_SYSTIMER_MAXVAL_REG(timer), value);
	nlm_pic_write_irt_id(base, XLP_PIC_IRT_TIMER_INDEX(timer),
		en, 0, irq, node, cpugroup, cpumask);

	/* enable the timer */
	pic_ctrl |= (1 << (XLP_PIC_STE_OFFSET+timer));
	nlm_wreg_pic(base, XLP_PIC_CTRL_REG, pic_ctrl);
}

static __inline__ uint64_t
nlm_pic_read_systimer(uint64_t base, int timer)
{
	return nlm_rdreg_pic(base, XLP_PIC_SYSTIMER_COUNT_REG(timer));
}

/* Watchdog timer routines */

/* node - XLP node
 * timer - watchdog timer. valid values are 0 and 1
 * wrap_around_count - defines the number of times the watchdog timer can wrap-around
 *     after which the reset / NMI gets generated to the threads defined in thread-enable-masks.
 * value - the vatchdog timer max value, upto which the timer will count down
 */

static __inline__ void
nlm_pic_set_wdogtimer(uint64_t base, int timer, int wrap_around_count, int nmi,
		uint32_t node0_thrmask, uint32_t node1_thrmask,
		uint32_t node2_thrmask, uint32_t node3_thrmask, uint64_t value)
{
	uint64_t pic_ctrl = nlm_rdreg_pic(base, XLP_PIC_CTRL_REG);
	uint64_t mask0, mask1;

	if (timer > 1 || wrap_around_count > 3)
		return;

	/* enable watchdog timer interrupt */
	pic_ctrl |= (((1 << timer) & 0xf));

	if (timer) {
		if (nmi)
			pic_ctrl |= (wrap_around_count << XLP_PIC_WWN1_OFFSET);
		else
			pic_ctrl |= (wrap_around_count << XLP_PIC_WWN0_OFFSET);
	} else {
		if (nmi)
			pic_ctrl |= (wrap_around_count << XLP_PIC_WWR1_OFFSET);
		else
			pic_ctrl |= (wrap_around_count << XLP_PIC_WWR0_OFFSET);
	}

	mask0 = ((unsigned long long)node1_thrmask << 32) | node0_thrmask;
	mask1 = ((unsigned long long)node3_thrmask << 32) | node2_thrmask;

	nlm_wreg_pic(base, XLP_PIC_WDOG_MAXVAL_REG(timer), value);

	nlm_wreg_pic(base, XLP_PIC_WDOG_ENABLE0_REG(timer), mask0);
	nlm_wreg_pic(base, XLP_PIC_WDOG_ENABLE1_REG(timer), mask1);

	nlm_wreg_pic(base, XLP_PIC_CTRL_REG, pic_ctrl);
}

/* watchdog's need to be "stroked" by heartbeats from vcpus.
 * On XLP, the heartbeat bit for a specific cpu thread on a specific
 * node is set according to the following formula:
 * 32N + 4C + T
 * where N = node, C=cpu-core number, T=thread number
 *
 * src_node = source node of watchdog timer interrupts. These interrupts
 * get generated from the PIC on src_node.
 * timer = watchdog timer 0 or 1
 * node = node for which the hearbeat is being done
 * cpu = cpu-core for which the hearbeat is being done
 * thread = h/w thread for which the hearbeat is being done
 */
static __inline__ void
nlm_pic_set_wdog_heartbeat(uint64_t base, int timer, int node, int cpu,
		int thread)
{
	int val = 32 * node + 4 * cpu + thread;

	nlm_wreg_pic(base, XLP_PIC_WDOG_BEATCMD_REG(timer), val);
}

#endif /* !LOCORE && !__ASSEMBLY__ */
#endif
