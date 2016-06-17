/*
 * FILE NAME
 *	arch/mips/vr41xx/common/icu.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Interrupt Control Unit routines for the NEC VR4100 series.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC VR4122 and VR4131 are supported.
 *  - Added support for NEC VR4111 and VR4121.
 *
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *  - Coped with INTASSIGN of NEC VR4133.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>

#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/vr41xx/vr41xx.h>

extern asmlinkage void vr41xx_handle_interrupt(void);

extern void vr41xx_giuint_init(void);
extern void vr41xx_enable_giuint(int pin);
extern void vr41xx_disable_giuint(int pin);
extern void vr41xx_clear_giuint(int pin);
extern unsigned int giuint_do_IRQ(int pin, struct pt_regs *regs);

static uint32_t icu1_base;
static uint32_t icu2_base;

static unsigned char sysint1_assign[16] = {
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char sysint2_assign[16] = {
	2, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#define SYSINT1REG_TYPE1	KSEG1ADDR(0x0b000080)
#define SYSINT2REG_TYPE1	KSEG1ADDR(0x0b000200)

#define SYSINT1REG_TYPE2	KSEG1ADDR(0x0f000080)
#define SYSINT2REG_TYPE2	KSEG1ADDR(0x0f0000a0)

#define SYSINT1REG	0x00
#define INTASSIGN0	0x04
#define INTASSIGN1	0x06
#define GIUINTLREG	0x08
#define MSYSINT1REG	0x0c
#define MGIUINTLREG	0x14
#define NMIREG		0x18
#define SOFTREG		0x1a
#define INTASSIGN2	0x1c
#define INTASSIGN3	0x1e

#define SYSINT2REG	0x00
#define GIUINTHREG	0x02
#define MSYSINT2REG	0x06
#define MGIUINTHREG	0x08

#define SYSINT1_IRQ_TO_PIN(x)	((x) - SYSINT1_IRQ_BASE)	/* Pin 0-15 */
#define SYSINT2_IRQ_TO_PIN(x)	((x) - SYSINT2_IRQ_BASE)	/* Pin 0-15 */

#define read_icu1(offset)	readw(icu1_base + (offset))
#define write_icu1(val, offset)	writew((val), icu1_base + (offset))

#define read_icu2(offset)	readw(icu2_base + (offset))
#define write_icu2(val, offset)	writew((val), icu2_base + (offset))

#define INTASSIGN_MAX	4
#define INTASSIGN_MASK	0x0007

static inline uint16_t set_icu1(uint8_t offset, uint16_t set)
{
	uint16_t res;

	res = read_icu1(offset);
	res |= set;
	write_icu1(res, offset);

	return res;
}

static inline uint16_t clear_icu1(uint8_t offset, uint16_t clear)
{
	uint16_t res;

	res = read_icu1(offset);
	res &= ~clear;
	write_icu1(res, offset);

	return res;
}

static inline uint16_t set_icu2(uint8_t offset, uint16_t set)
{
	uint16_t res;

	res = read_icu2(offset);
	res |= set;
	write_icu2(res, offset);

	return res;
}

static inline uint16_t clear_icu2(uint8_t offset, uint16_t clear)
{
	uint16_t res;

	res = read_icu2(offset);
	res &= ~clear;
	write_icu2(res, offset);

	return res;
}

/*=======================================================================*/

static void enable_sysint1_irq(unsigned int irq)
{
	set_icu1(MSYSINT1REG, (uint16_t)1 << SYSINT1_IRQ_TO_PIN(irq));
}

static void disable_sysint1_irq(unsigned int irq)
{
	clear_icu1(MSYSINT1REG, (uint16_t)1 << SYSINT1_IRQ_TO_PIN(irq));
}

static unsigned int startup_sysint1_irq(unsigned int irq)
{
	set_icu1(MSYSINT1REG, (uint16_t)1 << SYSINT1_IRQ_TO_PIN(irq));

	return 0; /* never anything pending */
}

#define shutdown_sysint1_irq	disable_sysint1_irq
#define ack_sysint1_irq		disable_sysint1_irq

static void end_sysint1_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		set_icu1(MSYSINT1REG, (uint16_t)1 << SYSINT1_IRQ_TO_PIN(irq));
}

static struct hw_interrupt_type sysint1_irq_type = {
	.typename	= "SYSINT1",
	.startup	= startup_sysint1_irq,
	.shutdown	= shutdown_sysint1_irq,
	.enable		= enable_sysint1_irq,
	.disable	= disable_sysint1_irq,
	.ack		= ack_sysint1_irq,
	.end		= end_sysint1_irq,
};

/*=======================================================================*/

static void enable_sysint2_irq(unsigned int irq)
{
	set_icu2(MSYSINT2REG, (uint16_t)1 << SYSINT2_IRQ_TO_PIN(irq));
}

static void disable_sysint2_irq(unsigned int irq)
{
	clear_icu2(MSYSINT2REG, (uint16_t)1 << SYSINT2_IRQ_TO_PIN(irq));
}

static unsigned int startup_sysint2_irq(unsigned int irq)
{
	set_icu2(MSYSINT2REG, (uint16_t)1 << SYSINT2_IRQ_TO_PIN(irq));

	return 0; /* never anything pending */
}

#define shutdown_sysint2_irq	disable_sysint2_irq
#define ack_sysint2_irq		disable_sysint2_irq

static void end_sysint2_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		set_icu2(MSYSINT2REG, (uint16_t)1 << SYSINT2_IRQ_TO_PIN(irq));
}

static struct hw_interrupt_type sysint2_irq_type = {
	.typename	= "SYSINT2",
	.startup	= startup_sysint2_irq,
	.shutdown	= shutdown_sysint2_irq,
	.enable		= enable_sysint2_irq,
	.disable	= disable_sysint2_irq,
	.ack		= ack_sysint2_irq,
	.end		= end_sysint2_irq,
};

/*=======================================================================*/

static void enable_giuint_irq(unsigned int irq)
{
	int pin;

	pin = GIU_IRQ_TO_PIN(irq);
	if (pin < 16)
		set_icu1(MGIUINTLREG, (uint16_t)1 << pin);
	else
		set_icu2(MGIUINTHREG, (uint16_t)1 << (pin - 16));
	vr41xx_enable_giuint(pin);
}

static void disable_giuint_irq(unsigned int irq)
{
	int pin;

	pin = GIU_IRQ_TO_PIN(irq);
	vr41xx_disable_giuint(pin);
	if (pin < 16)
		clear_icu1(MGIUINTLREG, (uint16_t)1 << pin);
	else
		clear_icu2(MGIUINTHREG, (uint16_t)1 << (pin - 16));
}

static unsigned int startup_giuint_irq(unsigned int irq)
{
	vr41xx_clear_giuint(GIU_IRQ_TO_PIN(irq));

	enable_giuint_irq(irq);

	return 0; /* never anything pending */
}

#define shutdown_giuint_irq	disable_giuint_irq

static void ack_giuint_irq(unsigned int irq)
{
	disable_giuint_irq(irq);

	vr41xx_clear_giuint(GIU_IRQ_TO_PIN(irq));
}

static void end_giuint_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_giuint_irq(irq);
}

static struct hw_interrupt_type giuint_irq_type = {
	.typename	= "GIUINT",
	.startup	= startup_giuint_irq,
	.shutdown	= shutdown_giuint_irq,
	.enable		= enable_giuint_irq,
	.disable	= disable_giuint_irq,
	.ack		= ack_giuint_irq,
	.end		= end_giuint_irq,
};

/*=======================================================================*/

static struct irqaction icu_cascade = {no_action, 0, 0, "cascade", NULL, NULL};

static void __init vr41xx_icu_init(void)
{
	int i;

	switch (current_cpu_data.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		icu1_base = SYSINT1REG_TYPE1;
		icu2_base = SYSINT2REG_TYPE1;
		break;
	case CPU_VR4122:
	case CPU_VR4131:
	case CPU_VR4133:
		icu1_base = SYSINT1REG_TYPE2;
		icu2_base = SYSINT2REG_TYPE2;
		break;
	default:
		panic("Unexpected CPU of NEC VR4100 series");
		break;
	}

	write_icu1(0, MSYSINT1REG);
	write_icu1(0, MGIUINTLREG);

	write_icu2(0, MSYSINT2REG);
	write_icu2(0, MGIUINTHREG);

	for (i = SYSINT1_IRQ_BASE; i <= GIU_IRQ_LAST; i++) {
		if (i >= SYSINT1_IRQ_BASE && i <= SYSINT1_IRQ_LAST)
			irq_desc[i].handler = &sysint1_irq_type;
		else if (i >= SYSINT2_IRQ_BASE && i <= SYSINT2_IRQ_LAST)
			irq_desc[i].handler = &sysint2_irq_type;
		else if (i >= GIU_IRQ_BASE && i <= GIU_IRQ_LAST)
			irq_desc[i].handler = &giuint_irq_type;
	}

	setup_irq(INT0_CASCADE_IRQ, &icu_cascade);
	setup_irq(INT1_CASCADE_IRQ, &icu_cascade);
	setup_irq(INT2_CASCADE_IRQ, &icu_cascade);
	setup_irq(INT3_CASCADE_IRQ, &icu_cascade);
	setup_irq(INT4_CASCADE_IRQ, &icu_cascade);
}

void __init init_IRQ(void)
{
	memset(irq_desc, 0, sizeof(irq_desc));

	init_generic_irq();
	mips_cpu_irq_init(MIPS_CPU_IRQ_BASE);
	vr41xx_icu_init();

	vr41xx_giuint_init();

	set_except_vector(0, vr41xx_handle_interrupt);
}

/*=======================================================================*/

static inline int set_sysint1_assign(unsigned int irq, unsigned char assign)
{
	irq_desc_t *desc = irq_desc + irq;
	uint16_t intassign0, intassign1;
	unsigned int pin;

	pin = SYSINT1_IRQ_TO_PIN(irq);

	spin_lock_irq(&desc->lock);

	intassign0 = read_icu1(INTASSIGN0);
	intassign1 = read_icu1(INTASSIGN1);

	switch (pin) {
	case 0:
		intassign0 &= ~INTASSIGN_MASK;
		intassign0 |= (uint16_t)assign;
		break;
	case 1:
		intassign0 &= ~(INTASSIGN_MASK << 3);
		intassign0 |= (uint16_t)assign << 3;
		break;
	case 2:
		intassign0 &= ~(INTASSIGN_MASK << 6);
		intassign0 |= (uint16_t)assign << 6;
		break;
	case 3:
		intassign0 &= ~(INTASSIGN_MASK << 9);
		intassign0 |= (uint16_t)assign << 9;
		break;
	case 8:
		intassign0 &= ~(INTASSIGN_MASK << 12);
		intassign0 |= (uint16_t)assign << 12;
		break;
	case 9:
		intassign1 &= ~INTASSIGN_MASK;
		intassign1 |= (uint16_t)assign;
		break;
	case 11:
		intassign1 &= ~(INTASSIGN_MASK << 6);
		intassign1 |= (uint16_t)assign << 6;
		break;
	case 12:
		intassign1 &= ~(INTASSIGN_MASK << 9);
		intassign1 |= (uint16_t)assign << 9;
		break;
	default:
		return -EINVAL;
	}

	sysint1_assign[pin] = assign;
	write_icu1(intassign0, INTASSIGN0);
	write_icu1(intassign1, INTASSIGN1);

	spin_unlock_irq(&desc->lock);

	return 0;
}

static inline int set_sysint2_assign(unsigned int irq, unsigned char assign)
{
	irq_desc_t *desc = irq_desc + irq;
	uint16_t intassign2, intassign3;
	unsigned int pin;

	pin = SYSINT2_IRQ_TO_PIN(irq);

	spin_lock_irq(&desc->lock);

	intassign2 = read_icu1(INTASSIGN2);
	intassign3 = read_icu1(INTASSIGN3);

	switch (pin) {
	case 0:
		intassign2 &= ~INTASSIGN_MASK;
		intassign2 |= (uint16_t)assign;
		break;
	case 1:
		intassign2 &= ~(INTASSIGN_MASK << 3);
		intassign2 |= (uint16_t)assign << 3;
		break;
	case 3:
		intassign2 &= ~(INTASSIGN_MASK << 6);
		intassign2 |= (uint16_t)assign << 6;
		break;
	case 4:
		intassign2 &= ~(INTASSIGN_MASK << 9);
		intassign2 |= (uint16_t)assign << 9;
		break;
	case 5:
		intassign2 &= ~(INTASSIGN_MASK << 12);
		intassign2 |= (uint16_t)assign << 12;
		break;
	case 6:
		intassign3 &= ~INTASSIGN_MASK;
		intassign3 |= (uint16_t)assign;
		break;
	case 7:
		intassign3 &= ~(INTASSIGN_MASK << 3);
		intassign3 |= (uint16_t)assign << 3;
		break;
	case 8:
		intassign3 &= ~(INTASSIGN_MASK << 6);
		intassign3 |= (uint16_t)assign << 6;
		break;
	case 9:
		intassign3 &= ~(INTASSIGN_MASK << 9);
		intassign3 |= (uint16_t)assign << 9;
		break;
	case 10:
		intassign3 &= ~(INTASSIGN_MASK << 12);
		intassign3 |= (uint16_t)assign << 12;
		break;
	default:
		return -EINVAL;
	}

	sysint2_assign[pin] = assign;
	write_icu1(intassign2, INTASSIGN2);
	write_icu1(intassign3, INTASSIGN3);

	spin_unlock_irq(&desc->lock);

	return 0;
}

int vr41xx_set_intassign(unsigned int irq, unsigned char intassign)
{
	int retval = -EINVAL;

	if (current_cpu_data.cputype != CPU_VR4133)
		return -EINVAL;

	if (intassign > INTASSIGN_MAX)
		return -EINVAL;

	if (irq >= SYSINT1_IRQ_BASE && irq <= SYSINT1_IRQ_LAST)
		retval = set_sysint1_assign(irq, intassign);
	else if (irq >= SYSINT2_IRQ_BASE && irq <= SYSINT2_IRQ_LAST)
		retval = set_sysint2_assign(irq, intassign);

	return retval;
}

/*=======================================================================*/

static inline void giuint_irq_dispatch(uint16_t pendl, uint16_t pendh,
                                       struct pt_regs *regs)
{
	int i;

	if (pendl) {
		for (i = 0; i < 16; i++) {
			if (pendl & ((uint16_t)1 << i)) {
				giuint_do_IRQ(i, regs);
				return;
			}
		}
	} else {
		for (i = 0; i < 16; i++) {
			if (pendh & ((uint16_t)1 << i)) {
				giuint_do_IRQ(i + 16, regs);
				return;
			}
		}
	}
}

asmlinkage void irq_dispatch(unsigned char intnum, struct pt_regs *regs)
{
	uint16_t pend1, pend2, pendl, pendh;
	uint16_t mask1, mask2, maskl, maskh;
	int i;

	pend1 = read_icu1(SYSINT1REG);
	mask1 = read_icu1(MSYSINT1REG);

	pend2 = read_icu2(SYSINT2REG);
	mask2 = read_icu2(MSYSINT2REG);

	pendl = read_icu1(GIUINTLREG);
	maskl = read_icu1(MGIUINTLREG);

	pendh = read_icu2(GIUINTHREG);
	maskh = read_icu2(MGIUINTHREG);

	mask1 &= pend1;
	mask2 &= pend2;
	maskl &= pendl;
	maskh &= pendh;

	if (mask1) {
		for (i = 0; i < 16; i++) {
			if (intnum == sysint1_assign[i] &&
			    (mask1 & ((uint16_t)1 << i))) {
				if (i == 8 && (maskl | maskh)) {
					giuint_irq_dispatch(maskl, maskh, regs);
					return;
				} else {
					do_IRQ(SYSINT1_IRQ(i), regs);
					return;
				}
			}
		}
	}

	if (mask2) {
		for (i = 0; i < 16; i++) {
			if (intnum == sysint2_assign[i] &&
			    (mask2 & ((uint16_t)1 << i))) {
				do_IRQ(SYSINT2_IRQ(i), regs);
				return;
			}
		}
	}

	printk(KERN_ERR "spurious interrupt: %04x,%04x,%04x,%04x\n", pend1, pend2, pendl, pendh);
	atomic_inc(&irq_err_count);
}
