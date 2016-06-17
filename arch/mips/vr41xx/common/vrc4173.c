/*
 * FILE NAME
 *	drivers/char/vrc4173.c
 * 
 * BRIEF MODULE DESCRIPTION
 *	NEC VRC4173 driver for NEC VR4122/VR4131.
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
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/types.h>

#include <asm/vr41xx/vr41xx.h>
#include <asm/vr41xx/vrc4173.h>

MODULE_DESCRIPTION("NEC VRC4173 driver for NEC VR4122/4131");
MODULE_AUTHOR("Yoichi Yuasa <yyuasa@mvista.com>");
MODULE_LICENSE("GPL");

#define VRC4173_CMUCLKMSK	0x040
#define VRC4173_CMUSRST		0x042

#define VRC4173_SELECTREG	0x09e

#define VRC4173_SYSINT1REG	0x060
#define VRC4173_MSYSINT1REG	0x06c

static struct pci_device_id vrc4173_table[] __devinitdata = {
	{PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_VRC4173, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0, }
};

unsigned long vrc4173_io_offset = 0;

EXPORT_SYMBOL(vrc4173_io_offset);

static u16 vrc4173_cmuclkmsk;
static int vrc4173_initialized;

void vrc4173_clock_supply(u16 mask)
{
	if (vrc4173_initialized) {
		vrc4173_cmuclkmsk |= mask;
		vrc4173_outw(vrc4173_cmuclkmsk, VRC4173_CMUCLKMSK);
	}
}

void vrc4173_clock_mask(u16 mask)
{
	if (vrc4173_initialized) {
		vrc4173_cmuclkmsk &= ~mask;
		vrc4173_outw(vrc4173_cmuclkmsk, VRC4173_CMUCLKMSK);
	}
}

static inline void vrc4173_cmu_init(void)
{
	vrc4173_cmuclkmsk = vrc4173_inw(VRC4173_CMUCLKMSK);
}

EXPORT_SYMBOL(vrc4173_clock_supply);
EXPORT_SYMBOL(vrc4173_clock_mask);

void vrc4173_select_function(int func)
{
	u16 val;

	if (vrc4173_initialized) {
		val = vrc4173_inw(VRC4173_SELECTREG);
		switch(func) {
		case PS2CH1_SELECT:
			val |= 0x0004;
			break;
		case PS2CH2_SELECT:
			val |= 0x0002;
			break;
		case TOUCHPANEL_SELECT:
			val &= 0x0007;
			break;
		case KIU8_SELECT:
			val &= 0x000e;
			break;
		case KIU10_SELECT:
			val &= 0x000c;
			break;
		case KIU12_SELECT:
			val &= 0x0008;
			break;
		case GPIO_SELECT:
			val |= 0x0008;
			break;
		}
		vrc4173_outw(val, VRC4173_SELECTREG);
	}
}

EXPORT_SYMBOL(vrc4173_select_function);

static void enable_vrc4173_irq(unsigned int irq)
{
	u16 val;

	val = vrc4173_inw(VRC4173_MSYSINT1REG);
	val |= (u16)1 << (irq - VRC4173_IRQ_BASE);
	vrc4173_outw(val, VRC4173_MSYSINT1REG);
}

static void disable_vrc4173_irq(unsigned int irq)
{
	u16 val;

	val = vrc4173_inw(VRC4173_MSYSINT1REG);
	val &= ~((u16)1 << (irq - VRC4173_IRQ_BASE));
	vrc4173_outw(val, VRC4173_MSYSINT1REG);
}

static unsigned int startup_vrc4173_irq(unsigned int irq)
{
	enable_vrc4173_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_vrc4173_irq	disable_vrc4173_irq
#define ack_vrc4173_irq		disable_vrc4173_irq

static void end_vrc4173_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_vrc4173_irq(irq);
}

static struct hw_interrupt_type vrc4173_irq_type = {
	"VRC4173",
	startup_vrc4173_irq,
	shutdown_vrc4173_irq,
	enable_vrc4173_irq,
	disable_vrc4173_irq,
	ack_vrc4173_irq,
	end_vrc4173_irq,
	NULL
};

static int vrc4173_get_irq_number(int irq)
{
	u16 status, mask;
	int i;

        status = vrc4173_inw(VRC4173_SYSINT1REG);
        mask = vrc4173_inw(VRC4173_MSYSINT1REG);

	status &= mask;
	if (status) {
		for (i = 0; i < 16; i++)
			if (status & (0x0001 << i))
				return VRC4173_IRQ_BASE + i;
	}

	return -EINVAL;
}

static inline void vrc4173_icu_init(int cascade_irq)
{
	int i;

	if (cascade_irq < GIU_IRQ(0) || cascade_irq > GIU_IRQ(15))
		return;
	
	vrc4173_outw(0, VRC4173_MSYSINT1REG);

	vr41xx_set_irq_trigger(GIU_IRQ_TO_PIN(cascade_irq), TRIGGER_LEVEL, SIGNAL_THROUGH);
	vr41xx_set_irq_level(GIU_IRQ_TO_PIN(cascade_irq), LEVEL_LOW);

	for (i = VRC4173_IRQ_BASE; i <= VRC4173_IRQ_LAST; i++)
                irq_desc[i].handler = &vrc4173_irq_type;
}

static int __devinit vrc4173_probe(struct pci_dev *pdev,
                                   const struct pci_device_id *ent)
{
	unsigned long start, flags;
	int err;

	if ((err = pci_enable_device(pdev)) < 0) {
		printk(KERN_ERR "vrc4173: failed to enable device -- err=%d\n", err);
		return err;
	}

	pci_set_master(pdev);

	start = pci_resource_start(pdev, 0);
	if (!start) {
		printk(KERN_ERR "vrc4173:No PCI I/O resources, aborting\n");
		return -ENODEV;
	}

	if (!start || (((flags = pci_resource_flags(pdev, 0)) & IORESOURCE_IO) == 0)) {
		printk(KERN_ERR "vrc4173: No PCI I/O resources, aborting\n");
		return -ENODEV;
	}

	if ((err = pci_request_regions(pdev, "NEC VRC4173")) < 0) {
		printk(KERN_ERR "vrc4173: PCI resources are busy, aborting\n");
		return err;
	}

	set_vrc4173_io_offset(start);

	vrc4173_cmu_init();

	vrc4173_icu_init(pdev->irq);

	if ((err = vr41xx_cascade_irq(pdev->irq, vrc4173_get_irq_number)) < 0) {
		printk(KERN_ERR
		       "vrc4173: IRQ resource %d is busy, aborting\n", pdev->irq);
		return err;
	}

	printk(KERN_INFO
	       "NEC VRC4173 at 0x%#08lx, IRQ is cascaded to %d\n", start, pdev->irq);

	return 0;
}

static struct pci_driver vrc4173_driver = {
	name:		"NEC VRC4173",
	probe:		vrc4173_probe,
	remove:		NULL,
	id_table:	vrc4173_table,
};

static int __devinit vrc4173_init(void)
{
	int err;

	if ((err = pci_module_init(&vrc4173_driver)) < 0)
		return err;

	vrc4173_initialized = 1;

	return 0;
}

static void __devexit vrc4173_exit(void)
{
	vrc4173_initialized = 0;

	pci_unregister_driver(&vrc4173_driver);
}

module_init(vrc4173_init);
module_exit(vrc4173_exit);
