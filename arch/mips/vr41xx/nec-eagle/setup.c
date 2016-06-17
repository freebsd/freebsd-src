/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/setup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Setup for the NEC Eagle/Hawk board.
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
 *  - Moved mips_pci_channels[] from arch/mips/vr41xx/vr4122/eagle/setup.c.
 *  - Added support for NEC Hawk.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC Eagle is supported.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/ide.h>
#include <linux/ioport.h>

#include <asm/pci_channel.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/vr41xx/eagle.h>

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif

extern void eagle_irq_init(void);

#ifdef CONFIG_PCI

extern void vrc4173_preinit(void);

static struct resource vr41xx_pci_io_resource = {
	"PCI I/O space",
	VR41XX_PCI_IO_START,
	VR41XX_PCI_IO_END,
	IORESOURCE_IO
};

static struct resource vr41xx_pci_mem_resource = {
	"PCI memory space",
	VR41XX_PCI_MEM_START,
	VR41XX_PCI_MEM_END,
	IORESOURCE_MEM
};

extern struct pci_ops vr41xx_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{&vr41xx_pci_ops, &vr41xx_pci_io_resource, &vr41xx_pci_mem_resource, 0, 256},
	{NULL, NULL, NULL, 0, 0}
};

struct vr41xx_pci_address_space vr41xx_pci_mem1 = {
	VR41XX_PCI_MEM1_BASE,
	VR41XX_PCI_MEM1_MASK,
	IO_MEM1_RESOURCE_START
};

struct vr41xx_pci_address_space vr41xx_pci_mem2 = {
	VR41XX_PCI_MEM2_BASE,
	VR41XX_PCI_MEM2_MASK,
	IO_MEM2_RESOURCE_START
};

struct vr41xx_pci_address_space vr41xx_pci_io = {
	VR41XX_PCI_IO_BASE,
	VR41XX_PCI_IO_MASK,
	IO_PORT_RESOURCE_START
};

static struct vr41xx_pci_address_map pci_address_map = {
	&vr41xx_pci_mem1,
	&vr41xx_pci_mem2,
	&vr41xx_pci_io
};
#endif

void __init nec_eagle_setup(void)
{
	set_io_port_base(IO_PORT_BASE);
	ioport_resource.start = IO_PORT_RESOURCE_START;
	ioport_resource.end = IO_PORT_RESOURCE_END;
	iomem_resource.start = IO_MEM1_RESOURCE_START;
	iomem_resource.end = IO_MEM2_RESOURCE_END;

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

	_machine_restart = vr41xx_restart;
	_machine_halt = vr41xx_halt;
	_machine_power_off = vr41xx_power_off;

	board_time_init = vr41xx_time_init;
	board_timer_setup = vr41xx_timer_setup;

	board_irq_init = eagle_irq_init;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

#if defined(CONFIG_IDE) || defined(CONFIG_IDE_MODULE)
	ide_ops = &vr41xx_ide_ops;
#endif

	vr41xx_bcu_init();

	vr41xx_cmu_init();

#ifdef CONFIG_SERIAL
	vr41xx_dsiu_init();
	vr41xx_siu_init(SIU_RS232C, 0);
#endif

#ifdef CONFIG_PCI
	vr41xx_pciu_init(&pci_address_map);

	vrc4173_preinit();
#endif
}
