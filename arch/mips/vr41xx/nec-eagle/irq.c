/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/irq.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Interrupt routines for the NEC Eagle/Hawk board.
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
 *  - Added support for NEC Hawk.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC Eagle is supported.
 */
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/vr41xx/eagle.h>

static void enable_pciint_irq(unsigned int irq)
{
	u8 val;

	val = readb(NEC_EAGLE_PCIINTMSKREG);
	val |= (u8)1 << (irq - PCIINT_IRQ_BASE);
	writeb(val, NEC_EAGLE_PCIINTMSKREG);
}

static void disable_pciint_irq(unsigned int irq)
{
	u8 val;

	val = readb(NEC_EAGLE_PCIINTMSKREG);
	val &= ~((u8)1 << (irq - PCIINT_IRQ_BASE));
	writeb(val, NEC_EAGLE_PCIINTMSKREG);
}

static unsigned int startup_pciint_irq(unsigned int irq)
{
	enable_pciint_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_pciint_irq	disable_pciint_irq
#define ack_pciint_irq		disable_pciint_irq

static void end_pciint_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_pciint_irq(irq);
}

static struct hw_interrupt_type pciint_irq_type = {
	"PCIINT",
	startup_pciint_irq,
	shutdown_pciint_irq,
       	enable_pciint_irq,
	disable_pciint_irq,
	ack_pciint_irq,
	end_pciint_irq,
	NULL
};

static void enable_sdbint_irq(unsigned int irq)
{
	u8 val;

	val = readb(NEC_EAGLE_SDBINTMSK);
	val |= (u8)1 << (irq - SDBINT_IRQ_BASE);
	writeb(val, NEC_EAGLE_SDBINTMSK);
}

static void disable_sdbint_irq(unsigned int irq)
{
	u8 val;

	val = readb(NEC_EAGLE_SDBINTMSK);
	val &= ~((u8)1 << (irq - SDBINT_IRQ_BASE));
	writeb(val, NEC_EAGLE_SDBINTMSK);
}

static unsigned int startup_sdbint_irq(unsigned int irq)
{
	enable_sdbint_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_sdbint_irq	disable_sdbint_irq
#define ack_sdbint_irq		disable_sdbint_irq

static void end_sdbint_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_sdbint_irq(irq);
}

static struct hw_interrupt_type sdbint_irq_type = {
	"SDBINT",
	startup_sdbint_irq,
	shutdown_sdbint_irq,
       	enable_sdbint_irq,
	disable_sdbint_irq,
	ack_sdbint_irq,
	end_sdbint_irq,
	NULL
};

static int eagle_get_irq_number(int irq)
{
	u8 sdbint, pciint;
	int i;

	sdbint = readb(NEC_EAGLE_SDBINT);
	sdbint &= (NEC_EAGLE_SDBINT_DEG | NEC_EAGLE_SDBINT_ENUM |
	           NEC_EAGLE_SDBINT_SIO1INT | NEC_EAGLE_SDBINT_SIO2INT |
	           NEC_EAGLE_SDBINT_PARINT);
	pciint = readb(NEC_EAGLE_PCIINTREG);
	pciint &= (NEC_EAGLE_PCIINT_CP_INTA | NEC_EAGLE_PCIINT_CP_INTB |
	           NEC_EAGLE_PCIINT_CP_INTC | NEC_EAGLE_PCIINT_CP_INTD |
	           NEC_EAGLE_PCIINT_LANINT);

	for (i = 1; i < 6; i++)
		if (sdbint & (0x01 << i))
			return SDBINT_IRQ_BASE + i;

	for (i = 0; i < 5; i++)
		if (pciint & (0x01 << i))
			return PCIINT_IRQ_BASE + i;

	return -EINVAL;
}

void __init eagle_irq_init(void)
{
	int i;

	writeb(0, NEC_EAGLE_SDBINTMSK);
	writeb(0, NEC_EAGLE_PCIINTMSKREG);

	vr41xx_set_irq_trigger(PCISLOT_PIN, TRIGGER_LEVEL, SIGNAL_THROUGH);
	vr41xx_set_irq_level(PCISLOT_PIN, LEVEL_HIGH);

	vr41xx_set_irq_trigger(FPGA_PIN, TRIGGER_LEVEL, SIGNAL_THROUGH);
	vr41xx_set_irq_level(FPGA_PIN, LEVEL_HIGH);

	vr41xx_set_irq_trigger(DCD_PIN, TRIGGER_EDGE, SIGNAL_HOLD);
	vr41xx_set_irq_level(DCD_PIN, LEVEL_LOW);

	for (i = SDBINT_IRQ_BASE; i <= SDBINT_IRQ_LAST; i++)
		irq_desc[i].handler = &sdbint_irq_type;

	for (i = PCIINT_IRQ_BASE; i <= PCIINT_IRQ_LAST; i++)
		irq_desc[i].handler = &pciint_irq_type;

	vr41xx_cascade_irq(FPGA_CASCADE_IRQ, eagle_get_irq_number);
}
