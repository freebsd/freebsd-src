/*-
 * Copyright (c) 2003-2009 RMI Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * __FBSDID("$FreeBSD$");
 *
 * RMI_BSD */
#ifndef _RMI_PIC_H_
#define _RMI_PIC_H_
#include <sys/cdefs.h>




extern int rmi_spin_mutex_safe;

#include <sys/lock.h>
#include <sys/mutex.h>
#include <mips/rmi/iomap.h>

#define PIC_IRT_WD_INDEX     0
#define PIC_IRT_TIMER_0_INDEX      1
#define PIC_IRT_TIMER_1_INDEX      2
#define PIC_IRT_TIMER_2_INDEX      3
#define PIC_IRT_TIMER_3_INDEX      4
#define PIC_IRT_TIMER_4_INDEX      5
#define PIC_IRT_TIMER_5_INDEX      6
#define PIC_IRT_TIMER_6_INDEX      7
#define PIC_IRT_TIMER_7_INDEX      8
#define PIC_IRT_CLOCK_INDEX        PIC_IRT_TIMER_7_INDEX
#define PIC_IRT_UART_0_INDEX       9
#define PIC_IRT_UART_1_INDEX       10
#define PIC_IRT_I2C_0_INDEX       11
#define PIC_IRT_I2C_1_INDEX       12
#define PIC_IRT_PCMCIA_INDEX           13
#define PIC_IRT_GPIO_INDEX             14
#define PIC_IRT_HYPER_INDEX            15
#define PIC_IRT_PCIX_INDEX             16
#define PIC_IRT_GMAC0_INDEX            17
#define PIC_IRT_GMAC1_INDEX            18
#define PIC_IRT_GMAC2_INDEX            19
#define PIC_IRT_GMAC3_INDEX            20
#define PIC_IRT_XGS0_INDEX             21
#define PIC_IRT_XGS1_INDEX             22
#define PIC_IRT_HYPER_FATAL_INDEX      23
#define PIC_IRT_PCIX_FATAL_INDEX       24
#define PIC_IRT_BRIDGE_AERR_INDEX      25
#define PIC_IRT_BRIDGE_BERR_INDEX     26
#define PIC_IRT_BRIDGE_TB_INDEX        27
#define PIC_IRT_BRIDGE_AERR_NMI_INDEX  28

/* numbering for XLS */
#define PIC_IRT_BRIDGE_ERR_INDEX       25
#define PIC_IRT_PCIE_LINK0_INDEX       26
#define PIC_IRT_PCIE_LINK1_INDEX       27
#define PIC_IRT_PCIE_LINK2_INDEX       23
#define PIC_IRT_PCIE_LINK3_INDEX       24
#define PIC_IRT_PCIE_INT_INDEX         28
#define PIC_IRT_PCIE_FATAL_INDEX       29
#define PIC_IRT_GPIO_B_INDEX           30
#define PIC_IRT_USB_INDEX              31

#define PIC_NUM_IRTS                   32

#define PIC_SYS_TIMER_MAXVAL_0_BASE 0x100
#define PIC_SYS_TIMER_MAXVAL_1_BASE 0x110

#define PIC_SYS_TIMER_0_BASE 0x120
#define PIC_SYS_TIMER_1_BASE 0x130

#define PIC_CLOCK_TIMER 7

#define PIC_CTRL    0x00
#define PIC_IPI     0x04
#define PIC_INT_ACK 0x06

#define WD_MAX_VAL_0 0x08
#define WD_MAX_VAL_1 0x09
#define WD_MASK_0    0x0a
#define WD_MASK_1    0x0b
#define WD_HEARBEAT_0 0x0c
#define WD_HEARBEAT_1 0x0d

#define PIC_IRT_0_BASE 0x40
#define PIC_IRT_1_BASE 0x80

#define PIC_IRT_0_WD     (PIC_IRT_0_BASE   + PIC_IRT_WD_INDEX)
#define PIC_IRT_1_WD     (PIC_IRT_1_BASE   + PIC_IRT_WD_INDEX)
#define PIC_IRT_0_TIMER_0     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_0_INDEX)
#define PIC_IRT_1_TIMER_0     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_0_INDEX)
#define PIC_IRT_0_TIMER_1     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_1_INDEX)
#define PIC_IRT_1_TIMER_1     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_1_INDEX)
#define PIC_IRT_0_TIMER_2     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_2_INDEX)
#define PIC_IRT_1_TIMER_2     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_2_INDEX)
#define PIC_IRT_0_TIMER_3     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_3_INDEX)
#define PIC_IRT_1_TIMER_3     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_3_INDEX)
#define PIC_IRT_0_TIMER_4     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_4_INDEX)
#define PIC_IRT_1_TIMER_4     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_4_INDEX)
#define PIC_IRT_0_TIMER_5     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_5_INDEX)
#define PIC_IRT_1_TIMER_5     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_5_INDEX)
#define PIC_IRT_0_TIMER_6     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_6_INDEX)
#define PIC_IRT_1_TIMER_6     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_6_INDEX)
#define PIC_IRT_0_TIMER_7     (PIC_IRT_0_BASE   + PIC_IRT_TIMER_7_INDEX)
#define PIC_IRT_1_TIMER_7     (PIC_IRT_1_BASE   + PIC_IRT_TIMER_7_INDEX)
#define PIC_IRT_0_CLOCK       (PIC_IRT_0_TIMER_7)
#define PIC_IRT_1_CLOCK       (PIC_IRT_1_TIMER_7)
#define PIC_IRT_0_UART_0 (PIC_IRT_0_BASE + PIC_IRT_UART_0_INDEX)
#define PIC_IRT_1_UART_0 (PIC_IRT_1_BASE + PIC_IRT_UART_0_INDEX)
#define PIC_IRT_0_UART_1 (PIC_IRT_0_BASE + PIC_IRT_UART_1_INDEX)
#define PIC_IRT_1_UART_1 (PIC_IRT_1_BASE + PIC_IRT_UART_1_INDEX)
#define PIC_IRT_0_I2C_0 (PIC_IRT_0_BASE + PIC_IRT_I2C_0_INDEX)
#define PIC_IRT_1_I2C_0 (PIC_IRT_1_BASE + PIC_IRT_I2C_0_INDEX)
#define PIC_IRT_0_I2C_1 (PIC_IRT_0_BASE + PIC_IRT_I2C_1_INDEX)
#define PIC_IRT_1_I2C_1 (PIC_IRT_1_BASE + PIC_IRT_I2C_1_INDEX)
#define PIC_IRT_0_HYPER       (PIC_IRT_0_BASE + PIC_IRT_HYPER_INDEX)
#define PIC_IRT_1_HYPER       (PIC_IRT_1_BASE + PIC_IRT_HYPER_INDEX)
#define PIC_IRT_0_PCIX        (PIC_IRT_0_BASE + PIC_IRT_PCIX_INDEX)
#define PIC_IRT_1_PCIX        (PIC_IRT_1_BASE + PIC_IRT_PCIX_INDEX)

#define PIC_TIMER_0_MAXVAL_0   (PIC_SYS_TIMER_MAXVAL_0_BASE + 0)
#define PIC_TIMER_0_MAXVAL_1   (PIC_SYS_TIMER_MAXVAL_1_BASE + 0)
#define PIC_TIMER_0_COUNTER_0  (PIC_SYS_TIMER_0_BASE + 0)
#define PIC_TIMER_0_COUNTER_1  (PIC_SYS_TIMER_1_BASE + 0)
#define PIC_TIMER_6_MAXVAL_0   (PIC_SYS_TIMER_MAXVAL_0_BASE + 6)
#define PIC_TIMER_6_MAXVAL_1   (PIC_SYS_TIMER_MAXVAL_1_BASE + 6)
#define PIC_TIMER_6_COUNTER_0  (PIC_SYS_TIMER_0_BASE + 6)
#define PIC_TIMER_6_COUNTER_1  (PIC_SYS_TIMER_1_BASE + 6)
#define PIC_TIMER_7_MAXVAL_0   (PIC_SYS_TIMER_MAXVAL_0_BASE + 7)
#define PIC_TIMER_7_MAXVAL_1   (PIC_SYS_TIMER_MAXVAL_1_BASE + 7)
#define PIC_TIMER_7_COUNTER_0  (PIC_SYS_TIMER_0_BASE + 7)
#define PIC_TIMER_7_COUNTER_1  (PIC_SYS_TIMER_1_BASE + 7)

#define PIC_IRQ_BASE      8
#define PIC_IRT_FIRST_IRQ PIC_IRQ_BASE
#define PIC_WD_IRQ      (PIC_IRQ_BASE + PIC_IRT_WD_INDEX)
#define PIC_TIMER_0_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_0_INDEX)
#define PIC_TIMER_1_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_1_INDEX)
#define PIC_TIMER_2_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_2_INDEX)
#define PIC_TIMER_3_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_3_INDEX)
#define PIC_TIMER_4_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_4_INDEX)
#define PIC_TIMER_5_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_5_INDEX)
#define PIC_TIMER_6_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_6_INDEX)
#define PIC_TIMER_7_IRQ (PIC_IRQ_BASE + PIC_IRT_TIMER_7_INDEX)
#define PIC_CLOCK_IRQ   (PIC_TIMER_7_IRQ)
#define PIC_UART_0_IRQ  (PIC_IRQ_BASE + PIC_IRT_UART_0_INDEX)
#define PIC_UART_1_IRQ  (PIC_IRQ_BASE + PIC_IRT_UART_1_INDEX)
#define PIC_I2C_0_IRQ   (PIC_IRQ_BASE + PIC_IRT_I2C_0_INDEX)
#define PIC_I2C_1_IRQ   (PIC_IRQ_BASE + PIC_IRT_I2C_1_INDEX)
#define PIC_PCMCIA_IRQ           (PIC_IRQ_BASE + PIC_IRT_PCMCIA_INDEX)
#define PIC_GPIO_IRQ             (PIC_IRQ_BASE + PIC_IRT_GPIO_INDEX)
#define PIC_HYPER_IRQ            (PIC_IRQ_BASE + PIC_IRT_HYPER_INDEX)
#define PIC_PCIX_IRQ             (PIC_IRQ_BASE + PIC_IRT_PCIX_INDEX)
#define PIC_GMAC_0_IRQ            (PIC_IRQ_BASE + PIC_IRT_GMAC0_INDEX)
#define PIC_GMAC_1_IRQ            (PIC_IRQ_BASE + PIC_IRT_GMAC1_INDEX)
#define PIC_GMAC_2_IRQ            (PIC_IRQ_BASE + PIC_IRT_GMAC2_INDEX)
#define PIC_GMAC_3_IRQ            (PIC_IRQ_BASE + PIC_IRT_GMAC3_INDEX)
#define PIC_XGS_0_IRQ             (PIC_IRQ_BASE + PIC_IRT_XGS0_INDEX)
#define PIC_XGS_1_IRQ             (PIC_IRQ_BASE + PIC_IRT_XGS1_INDEX)
#define PIC_HYPER_FATAL_IRQ      (PIC_IRQ_BASE + PIC_IRT_HYPER_FATAL_INDEX)
#define PIC_PCIX_FATAL_IRQ       (PIC_IRQ_BASE + PIC_IRT_PCIX_FATAL_INDEX)
#define PIC_BRIDGE_AERR_IRQ      (PIC_IRQ_BASE + PIC_IRT_BRIDGE_AERR_INDEX)
#define PIC_BRIDGE_BERR_IRQ      (PIC_IRQ_BASE + PIC_IRT_BRIDGE_BERR_INDEX)
#define PIC_BRIDGE_TB_IRQ        (PIC_IRQ_BASE + PIC_IRT_BRIDGE_TB_INDEX)
#define PIC_BRIDGE_AERR_NMI_IRQ  (PIC_IRQ_BASE + PIC_IRT_BRIDGE_AERR_NMI_INDEX)

#define PIC_BRIDGE_ERR_IRQ       (PIC_IRQ_BASE + PIC_IRT_BRIDGE_ERR_INDEX)
#define PIC_PCIE_LINK0_IRQ       (PIC_IRQ_BASE + PIC_IRT_PCIE_LINK0_INDEX)
#define PIC_PCIE_LINK1_IRQ       (PIC_IRQ_BASE + PIC_IRT_PCIE_LINK1_INDEX)
#define PIC_PCIE_LINK2_IRQ       (PIC_IRQ_BASE + PIC_IRT_PCIE_LINK2_INDEX)
#define PIC_PCIE_LINK3_IRQ       (PIC_IRQ_BASE + PIC_IRT_PCIE_LINK3_INDEX)
#define PIC_PCIE_INT_IRQ         (PIC_IRQ_BASE + PIC_IRT_PCIE_INT__INDEX)
#define PIC_PCIE_FATAL_IRQ       (PIC_IRQ_BASE + PIC_IRT_PCIE_FATAL_INDEX)
#define PIC_GPIO_B_IRQ           (PIC_IRQ_BASE + PIC_IRT_GPIO_B_INDEX)
#define PIC_USB_IRQ              (PIC_IRQ_BASE + PIC_IRT_USB_INDEX)

#define PIC_IRT_LAST_IRQ         PIC_USB_IRQ

#define PIC_IRQ_IS_EDGE_TRIGGERED(irq) ( ((irq)>=PIC_TIMER_0_IRQ) && ((irq)<=PIC_TIMER_7_IRQ) )

#define PIC_IRQ_IS_IRT(irq) ( ((irq)>=PIC_IRT_FIRST_IRQ) && ((irq)<=PIC_IRT_LAST_IRQ) )


extern struct mtx xlr_pic_lock;


static __inline__ __uint32_t 
pic_read_control(int haslock)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	__uint32_t reg;

	if ((rmi_spin_mutex_safe) && (haslock == 0))
	  mtx_lock_spin(&xlr_pic_lock);
	xlr_read_reg(mmio, PIC_CTRL);
	if ((rmi_spin_mutex_safe) && (haslock == 0))
	  mtx_unlock_spin(&xlr_pic_lock);
	return reg;
}

static __inline__ void 
pic_write_control(__uint32_t control, int haslock)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	if ((rmi_spin_mutex_safe) && (haslock == 0))
	  mtx_lock_spin(&xlr_pic_lock);
	xlr_write_reg(mmio, PIC_CTRL, control);
	if ((rmi_spin_mutex_safe) && (haslock == 0))
	  mtx_unlock_spin(&xlr_pic_lock);
}
static __inline__ void 
pic_update_control(__uint32_t control, int haslock)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	if ((rmi_spin_mutex_safe) && (haslock == 0))
	  mtx_lock_spin(&xlr_pic_lock);
	xlr_write_reg(mmio, PIC_CTRL, (control | xlr_read_reg(mmio, PIC_CTRL)));
	if ((rmi_spin_mutex_safe) && (haslock == 0))
	  mtx_unlock_spin(&xlr_pic_lock);
}

static __inline__ void 
pic_ack(int irq, int haslock)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	/* ack the pic, if needed */
	if (!PIC_IRQ_IS_IRT(irq))
		return;

	if (PIC_IRQ_IS_EDGE_TRIGGERED(irq)) {
	  if ((rmi_spin_mutex_safe) && (haslock == 0))
		mtx_lock_spin(&xlr_pic_lock);
	  xlr_write_reg(mmio, PIC_INT_ACK, (1 << (irq - PIC_IRQ_BASE)));
	  if ((rmi_spin_mutex_safe) && (haslock == 0))
		mtx_unlock_spin(&xlr_pic_lock);
		return;
	}
	return;
}

static inline void 
pic_delayed_ack(int irq, int haslock)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	if (!PIC_IRQ_IS_IRT(irq))
		return;

	if (!PIC_IRQ_IS_EDGE_TRIGGERED(irq)) {
		if ((rmi_spin_mutex_safe)&& (haslock == 0))
		  mtx_lock_spin(&xlr_pic_lock);
		xlr_write_reg(mmio, PIC_INT_ACK, (1 << (irq - PIC_IRQ_BASE)));
		if ((rmi_spin_mutex_safe) && (haslock == 0))
		  mtx_unlock_spin(&xlr_pic_lock);
		return;
	}
}

static inline
void pic_send_ipi(int cpu, int ipi, int haslock)
{
        xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
        int tid, pid;

        tid = cpu & 0x3;
        pid = (cpu >> 2) & 0x7;

	xlr_write_reg(mmio, PIC_IPI,  (pid << 20) | (tid << 16) | ipi);
}

#endif				/* _RMI_PIC_H_ */
