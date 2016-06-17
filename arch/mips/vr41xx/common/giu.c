/*
 * FILE NAME
 *	arch/mips/vr41xx/common/giu.c
 *
 * BRIEF MODULE DESCRIPTION
 *	General-purpose I/O Unit Interrupt routines for NEC VR4100 series.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
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
 *  - New creation, NEC VR4111, VR4121, VR4122 and VR4131 are supported.
 *
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *  - Added support for NEC VR4133.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/vr41xx/vr41xx.h>

#define GIUIOSELL_TYPE1	KSEG1ADDR(0x0b000100)
#define GIUIOSELL_TYPE2	KSEG1ADDR(0x0f000140)

#define GIUIOSELL	0x00
#define GIUIOSELH	0x02
#define GIUINTSTATL	0x08
#define GIUINTSTATH	0x0a
#define GIUINTENL	0x0c
#define GIUINTENH	0x0e
#define GIUINTTYPL	0x10
#define GIUINTTYPH	0x12
#define GIUINTALSELL	0x14
#define GIUINTALSELH	0x16
#define GIUINTHTSELL	0x18
#define GIUINTHTSELH	0x1a
#define GIUFEDGEINHL	0x20
#define GIUFEDGEINHH	0x22
#define GIUREDGEINHL	0x24
#define GIUREDGEINHH	0x26

static uint32_t giu_base;

#define read_giuint(offset)		readw(giu_base + (offset))
#define write_giuint(val, offset)	writew((val), giu_base + (offset))

static inline uint16_t set_giuint(uint8_t offset, uint16_t set)
{
	uint16_t res;

	res = read_giuint(offset);
	res |= set;
	write_giuint(res, offset);

	return res;
}

static inline uint16_t clear_giuint(uint8_t offset, uint16_t clear)
{
	uint16_t res;

	res = read_giuint(offset);
	res &= ~clear;
	write_giuint(res, offset);

	return res;
}

void vr41xx_enable_giuint(int pin)
{
	if (pin < 16)
		set_giuint(GIUINTENL, (uint16_t)1 << pin);
	else
		set_giuint(GIUINTENH, (uint16_t)1 << (pin - 16));
}

void vr41xx_disable_giuint(int pin)
{
	if (pin < 16)
		clear_giuint(GIUINTENL, (uint16_t)1 << pin);
	else
		clear_giuint(GIUINTENH, (uint16_t)1 << (pin - 16));
}

void vr41xx_clear_giuint(int pin)
{
	if (pin < 16)
		write_giuint((uint16_t)1 << pin, GIUINTSTATL);
	else
		write_giuint((uint16_t)1 << (pin - 16), GIUINTSTATH);
}

void vr41xx_set_irq_trigger(int pin, int trigger, int hold)
{
	uint16_t mask;

	if (pin < 16) {
		mask = (uint16_t)1 << pin;
		if (trigger != TRIGGER_LEVEL) {
        		set_giuint(GIUINTTYPL, mask);
			if (hold == SIGNAL_HOLD)
				set_giuint(GIUINTHTSELL, mask);
			else
				clear_giuint(GIUINTHTSELL, mask);
			if (current_cpu_data.cputype == CPU_VR4133) {
				switch (trigger) {
				case TRIGGER_EDGE_FALLING:
					set_giuint(GIUFEDGEINHL, mask);
					clear_giuint(GIUREDGEINHL, mask);
					break;
				case TRIGGER_EDGE_RISING:
					clear_giuint(GIUFEDGEINHL, mask);
					set_giuint(GIUREDGEINHL, mask);
					break;
				default:
					set_giuint(GIUFEDGEINHL, mask);
					set_giuint(GIUREDGEINHL, mask);
					break;
				}
			}
		} else {
			clear_giuint(GIUINTTYPL, mask);
			clear_giuint(GIUINTHTSELL, mask);
		}
	} else {
		mask = (uint16_t)1 << (pin - 16);
		if (trigger != TRIGGER_LEVEL) {
			set_giuint(GIUINTTYPH, mask);
			if (hold == SIGNAL_HOLD)
				set_giuint(GIUINTHTSELH, mask);
			else
				clear_giuint(GIUINTHTSELH, mask);
			if (current_cpu_data.cputype == CPU_VR4133) {
				switch (trigger) {
				case TRIGGER_EDGE_FALLING:
					set_giuint(GIUFEDGEINHH, mask);
					clear_giuint(GIUREDGEINHH, mask);
					break;
				case TRIGGER_EDGE_RISING:
					clear_giuint(GIUFEDGEINHH, mask);
					set_giuint(GIUREDGEINHH, mask);
					break;
				default:
					set_giuint(GIUFEDGEINHH, mask);
					set_giuint(GIUREDGEINHH, mask);
					break;
				}
			}
		} else {
			clear_giuint(GIUINTTYPH, mask);
			clear_giuint(GIUINTHTSELH, mask);
		}
	}

	vr41xx_clear_giuint(pin);
}

void vr41xx_set_irq_level(int pin, int level)
{
	uint16_t mask;

	if (pin < 16) {
		mask = (uint16_t)1 << pin;
		if (level == LEVEL_HIGH)
			set_giuint(GIUINTALSELL, mask);
		else
			clear_giuint(GIUINTALSELL, mask);
	} else {
		mask = (uint16_t)1 << (pin - 16);
		if (level == LEVEL_HIGH)
			set_giuint(GIUINTALSELH, mask);
		else
			clear_giuint(GIUINTALSELH, mask);
	}

	vr41xx_clear_giuint(pin);
}

#define GIUINT_NR_IRQS		32

enum {
	GIUINT_NO_CASCADE,
	GIUINT_CASCADE
};

struct vr41xx_giuint_cascade {
	unsigned int flag;
	int (*get_irq_number)(int irq);
};

static struct vr41xx_giuint_cascade giuint_cascade[GIUINT_NR_IRQS];
static struct irqaction giu_cascade = {no_action, 0, 0, "cascade", NULL, NULL};

static int no_irq_number(int irq)
{
	return -EINVAL;
}

int vr41xx_cascade_irq(unsigned int irq, int (*get_irq_number)(int irq))
{
	unsigned int pin;
	int retval;

	if (irq < GIU_IRQ(0) || irq > GIU_IRQ(31))
		return -EINVAL;

	if(!get_irq_number)
		return -EINVAL;

	pin = GIU_IRQ_TO_PIN(irq);
	giuint_cascade[pin].flag = GIUINT_CASCADE;
	giuint_cascade[pin].get_irq_number = get_irq_number;

	retval = setup_irq(irq, &giu_cascade);
	if (retval) {
		giuint_cascade[pin].flag = GIUINT_NO_CASCADE;
		giuint_cascade[pin].get_irq_number = no_irq_number;
	}

	return retval;
}

unsigned int giuint_do_IRQ(int pin, struct pt_regs *regs)
{
	struct vr41xx_giuint_cascade *cascade;
	unsigned int retval = 0;
	int giuint_irq, cascade_irq;

	disable_irq(GIUINT_CASCADE_IRQ);
	cascade = &giuint_cascade[pin];
	giuint_irq = GIU_IRQ(pin);
	if (cascade->flag == GIUINT_CASCADE) {
		cascade_irq = cascade->get_irq_number(giuint_irq);
		disable_irq(giuint_irq);
		if (cascade_irq > 0)
			retval = do_IRQ(cascade_irq, regs);
		enable_irq(giuint_irq);
	} else
		retval = do_IRQ(giuint_irq, regs);
	enable_irq(GIUINT_CASCADE_IRQ);

	return retval;
}

void (*board_irq_init)(void) = NULL;

void __init vr41xx_giuint_init(void)
{
	int i;

	switch (current_cpu_data.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		giu_base = GIUIOSELL_TYPE1;
		break;
	case CPU_VR4122:
	case CPU_VR4131:
	case CPU_VR4133:
		giu_base = GIUIOSELL_TYPE2;
		break;
	default:
		panic("GIU: Unexpected CPU of NEC VR4100 series");
		break;
	}

	for (i = 0; i < GIUINT_NR_IRQS; i++) {
                vr41xx_disable_giuint(i);
		giuint_cascade[i].flag = GIUINT_NO_CASCADE;
		giuint_cascade[i].get_irq_number = no_irq_number;
	}

	if (setup_irq(GIUINT_CASCADE_IRQ, &giu_cascade))
		printk("GIUINT: Can not cascade IRQ %d.\n", GIUINT_CASCADE_IRQ);

	if (board_irq_init)
		board_irq_init();
}
