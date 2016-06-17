/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Au1000 reset routines.
 *
 * Copyright 2001 MontaVista Software Inc.
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
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/reboot.h>
#include <asm/system.h>
#include <asm/au1000.h>

extern int au_sleep(void);

void au1000_restart(char *command)
{
	/* Set all integrated peripherals to disabled states */
	u32 prid = read_c0_prid();

	printk(KERN_NOTICE "\n** Resetting Integrated Peripherals\n");
	switch (prid & 0xFF000000)
	{
	case 0x00000000: /* Au1000 */
		au_writel(0x02, 0xb0000010); /* ac97_enable */
		au_writel(0x08, 0xb017fffc); /* usbh_enable - early errata */
		asm("sync");
		au_writel(0x00, 0xb017fffc); /* usbh_enable */
		au_writel(0x00, 0xb0200058); /* usbd_enable */
		au_writel(0x00, 0xb0300040); /* ir_enable */
		au_writel(0x00, 0xb4004104); /* mac dma */
		au_writel(0x00, 0xb4004114); /* mac dma */
		au_writel(0x00, 0xb4004124); /* mac dma */
		au_writel(0x00, 0xb4004134); /* mac dma */
		au_writel(0x00, 0xb0520000); /* macen0 */
		au_writel(0x00, 0xb0520004); /* macen1 */
		au_writel(0x00, 0xb1000008); /* i2s_enable  */
		au_writel(0x00, 0xb1100100); /* uart0_enable */
		au_writel(0x00, 0xb1200100); /* uart1_enable */
		au_writel(0x00, 0xb1300100); /* uart2_enable */
		au_writel(0x00, 0xb1400100); /* uart3_enable */
		au_writel(0x02, 0xb1600100); /* ssi0_enable */
		au_writel(0x02, 0xb1680100); /* ssi1_enable */
		au_writel(0x00, 0xb1900020); /* sys_freqctrl0 */
		au_writel(0x00, 0xb1900024); /* sys_freqctrl1 */
		au_writel(0x00, 0xb1900028); /* sys_clksrc */
		au_writel(0x10, 0xb1900060); /* sys_cpupll */
		au_writel(0x00, 0xb1900064); /* sys_auxpll */
		au_writel(0x00, 0xb1900100); /* sys_pininputen */
		break;
	case 0x01000000: /* Au1500 */
		au_writel(0x02, 0xb0000010); /* ac97_enable */
		au_writel(0x08, 0xb017fffc); /* usbh_enable - early errata */
		asm("sync");
		au_writel(0x00, 0xb017fffc); /* usbh_enable */
		au_writel(0x00, 0xb0200058); /* usbd_enable */
		au_writel(0x00, 0xb4004104); /* mac dma */
		au_writel(0x00, 0xb4004114); /* mac dma */
		au_writel(0x00, 0xb4004124); /* mac dma */
		au_writel(0x00, 0xb4004134); /* mac dma */
		au_writel(0x00, 0xb1520000); /* macen0 */
		au_writel(0x00, 0xb1520004); /* macen1 */
		au_writel(0x00, 0xb1100100); /* uart0_enable */
		au_writel(0x00, 0xb1400100); /* uart3_enable */
		au_writel(0x00, 0xb1900020); /* sys_freqctrl0 */
		au_writel(0x00, 0xb1900024); /* sys_freqctrl1 */
		au_writel(0x00, 0xb1900028); /* sys_clksrc */
		au_writel(0x10, 0xb1900060); /* sys_cpupll */
		au_writel(0x00, 0xb1900064); /* sys_auxpll */
		au_writel(0x00, 0xb1900100); /* sys_pininputen */
		break;
	case 0x02000000: /* Au1100 */
		au_writel(0x02, 0xb0000010); /* ac97_enable */
		au_writel(0x08, 0xb017fffc); /* usbh_enable - early errata */
		asm("sync");
		au_writel(0x00, 0xb017fffc); /* usbh_enable */
		au_writel(0x00, 0xb0200058); /* usbd_enable */
		au_writel(0x00, 0xb0300040); /* ir_enable */
		au_writel(0x00, 0xb4004104); /* mac dma */
		au_writel(0x00, 0xb4004114); /* mac dma */
		au_writel(0x00, 0xb4004124); /* mac dma */
		au_writel(0x00, 0xb4004134); /* mac dma */
		au_writel(0x00, 0xb0520000); /* macen0 */
		au_writel(0x00, 0xb1000008); /* i2s_enable  */
		au_writel(0x00, 0xb1100100); /* uart0_enable */
		au_writel(0x00, 0xb1200100); /* uart1_enable */
		au_writel(0x00, 0xb1400100); /* uart3_enable */
		au_writel(0x02, 0xb1600100); /* ssi0_enable */
		au_writel(0x02, 0xb1680100); /* ssi1_enable */
		au_writel(0x00, 0xb1900020); /* sys_freqctrl0 */
		au_writel(0x00, 0xb1900024); /* sys_freqctrl1 */
		au_writel(0x00, 0xb1900028); /* sys_clksrc */
		au_writel(0x10, 0xb1900060); /* sys_cpupll */
		au_writel(0x00, 0xb1900064); /* sys_auxpll */
		au_writel(0x00, 0xb1900100); /* sys_pininputen */
		break;

	default:
		break;
	}

	set_c0_status(ST0_BEV | ST0_ERL);
	set_c0_config(CONF_CM_UNCACHED);
	flush_cache_all();
	write_c0_wired(0);

#if defined(CONFIG_MIPS_PB1500) || defined(CONFIG_MIPS_PB1100) || defined(CONFIG_MIPS_DB1000) || defined(CONFIG_MIPS_DB1100) || defined(CONFIG_MIPS_DB1500)
	/* Do a HW reset if the board can do it */

	au_writel(0x00000000, 0xAE00001C);
#endif

	__asm__ __volatile__("jr\t%0"::"r"(0xbfc00000));
}

void au1000_halt(void)
{
	printk(KERN_NOTICE "\n** You can safely turn off the power\n");
#ifdef CONFIG_PM
	au_sleep();

	/* should not get here */
	printk(KERN_ERR "Unable to put cpu in sleep mode\n");
	while(1);
#else
	while (1)
		__asm__(".set\tmips3\n\t"
	                "wait\n\t"
			".set\tmips0");
#endif
}

void au1000_power_off(void)
{
	au1000_halt();
}
