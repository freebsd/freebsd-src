/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Alchemy Au1x00 board setup.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/mc146818rtc.h>
#include <linux/delay.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/keyboard.h>
#include <asm/mipsregs.h>
#include <asm/reboot.h>
#include <asm/pgtable.h>
#include <asm/au1000.h>
#include <asm/time.h>

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif

#ifdef CONFIG_BLK_DEV_IDE
extern struct ide_ops std_ide_ops;
extern struct ide_ops *ide_ops;
#endif

extern struct rtc_ops no_rtc_ops;
extern char * __init prom_getcmdline(void);
extern void __init board_setup(void);
extern void au1000_restart(char *);
extern void au1000_halt(void);
extern void au1000_power_off(void);
extern struct resource ioport_resource;
extern struct resource iomem_resource;
#if defined(CONFIG_64BIT_PHYS_ADDR) && defined(CONFIG_SOC_AU1500)
extern phys_t (*fixup_bigphys_addr)(phys_t phys_addr, phys_t size);
static phys_t au1500_fixup_bigphys_addr(phys_t phys_addr, phys_t size);
#endif
extern void au1xxx_time_init(void);
extern void au1xxx_timer_setup(void);

void __init au1x00_setup(void)
{
	char *argptr;

	/* Various early Au1000 Errata corrected by this */
	set_c0_config(1<<19); /* Config[OD] */

	board_setup();  /* board specific setup */

	argptr = prom_getcmdline();

#ifdef CONFIG_AU1X00_SERIAL_CONSOLE
	if ((argptr = strstr(argptr, "console=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,115200");
	}
#endif	  

#ifdef CONFIG_FB_AU1100
    if ((argptr = strstr(argptr, "video=")) == NULL) {
        argptr = prom_getcmdline();
        /* default panel */
        //strcat(argptr, " video=au1100fb:panel:Sharp_320x240_16");
#ifdef CONFIG_MIPS_HYDROGEN3
        strcat(argptr, " video=au1100fb:panel:Hydrogen_3_NEC_panel_320x240,nohwcursor");
#else
        strcat(argptr, " video=au1100fb:panel:s10,nohwcursor");
#endif
    }
#endif

#ifdef CONFIG_FB_E1356
	if ((argptr = strstr(argptr, "video=")) == NULL) {
		argptr = prom_getcmdline();
#ifdef CONFIG_MIPS_PB1000
		strcat(argptr, " video=e1356fb:system:pb1000,mmunalign:1");
#else
		strcat(argptr, " video=e1356fb:system:pb1500");
#endif
	}
#endif

#ifdef CONFIG_FB_XPERT98
	if ((argptr = strstr(argptr, "video=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " video=atyfb:1024x768-8@70");
	}
#endif

#if defined(CONFIG_SOUND_AU1X00) && !defined(CONFIG_SOC_AU1000)
	// au1000 does not support vra, au1500 and au1100 do
	strcat(argptr, " au1000_audio=vra");
	argptr = prom_getcmdline();
#endif
	_machine_restart = au1000_restart;
	_machine_halt = au1000_halt;
	_machine_power_off = au1000_power_off;
#if defined(CONFIG_64BIT_PHYS_ADDR) && defined(CONFIG_SOC_AU1500)
	fixup_bigphys_addr = au1500_fixup_bigphys_addr;
#endif

	board_time_init = au1xxx_time_init;
	board_timer_setup = au1xxx_timer_setup;

	// IO/MEM resources. 
	set_io_port_base(0);
	ioport_resource.start = IOPORT_RESOURCE_START;
	ioport_resource.end = IOPORT_RESOURCE_END;
	iomem_resource.start = IOMEM_RESOURCE_START;
	iomem_resource.end = IOMEM_RESOURCE_END;

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

#if defined (CONFIG_USB_OHCI) || defined (CONFIG_AU1X00_USB_DEVICE)
#ifdef CONFIG_USB_OHCI
	if ((argptr = strstr(argptr, "usb_ohci=")) == NULL) {
	        char usb_args[80];
		argptr = prom_getcmdline();
		memset(usb_args, 0, sizeof(usb_args));
		sprintf(usb_args, " usb_ohci=base:0x%x,len:0x%x,irq:%d",
			USB_OHCI_BASE, USB_OHCI_LEN, AU1000_USB_HOST_INT);
		strcat(argptr, usb_args);
	}
#endif

#ifdef CONFIG_USB_OHCI
	// enable host controller and wait for reset done
	au_writel(0x08, USB_HOST_CONFIG);
	udelay(1000);
	au_writel(0x0E, USB_HOST_CONFIG);
	udelay(1000);
	au_readl(USB_HOST_CONFIG); // throw away first read
	while (!(au_readl(USB_HOST_CONFIG) & 0x10))
		au_readl(USB_HOST_CONFIG);
#endif
#endif // defined (CONFIG_USB_OHCI) || defined (CONFIG_AU1X00_USB_DEVICE)

#ifdef CONFIG_FB
	// Needed if PCI video card in use
	conswitchp = &dummy_con;
#endif

#ifndef CONFIG_SERIAL_NONSTANDARD
	/* don't touch the default serial console */
        au_writel(0, UART0_ADDR + UART_CLK);
#endif

#ifdef CONFIG_BLK_DEV_IDE
	/* Board setup takes precedence for unique devices.
	*/
	if (ide_ops == NULL)
		ide_ops = &std_ide_ops;
#endif

	while (au_readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_E0S);
	au_writel(SYS_CNTRL_E0 | SYS_CNTRL_EN0, SYS_COUNTER_CNTRL);
	au_sync();
	while (au_readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_T0S);
	au_writel(0, SYS_TOYTRIM);
}

#if defined(CONFIG_64BIT_PHYS_ADDR) && (defined(CONFIG_SOC_AU1500) || defined(CONFIG_SOC_AU1550))
/* This routine should be valid for all Au1500 based boards */
static phys_t au1500_fixup_bigphys_addr(phys_t phys_addr, phys_t size)
{
	u32 pci_start = (u32)Au1500_PCI_MEM_START;
	u32 pci_end = (u32)Au1500_PCI_MEM_END;

	/* Don't fixup 36 bit addresses */
	if ((phys_addr >> 32) != 0) return phys_addr;

	/* check for pci memory window */
	if ((phys_addr >= pci_start) && ((phys_addr + size) < pci_end)) {
		return (phys_t)((phys_addr - pci_start) +
				     Au1500_PCI_MEM_START);
	}
	else 
		return phys_addr;
}
#endif
