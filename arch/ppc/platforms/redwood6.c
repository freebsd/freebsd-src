/*
 *
 *    Module name: redwood6.c
 *
 *    Description:
 *    	IBM redwood6 eval board file
 *
 *    Author: Armin Kuster
 *    Copyright 2002 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *      History:  July 10, 2002 - Armin
 *    	initial release
 *
 *    		: July 16, 2002 - Armin
 *    		added xilinx init code for ide interface
 *    		added IRQ table 
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <asm/io.h>
#include <asm/ppc4xx_pic.h>
#include <linux/delay.h>
#include <asm/machdep.h>


/*
 * Define all of the IRQ senses and polarities.  
 */

static u_char redwood6_IRQ_initsenses[] __initdata = {
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 0: RTC/FPC */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 1: Transport */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 2: Audio Dec */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 3: Video Dec */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 4: DMA Chan 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 5: DMA Chan 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 6: DMA Chan 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 7: DMA Chan 3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 8: SmartCard 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 9: IIC0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 10: IRR */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 11: Cap Timers */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 12: Cmp Timers */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 13: Serial Port */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 14: Soft Modem */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 15: Down Ctrs */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 16: SmartCard 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 17: Ext Int 7 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 18: Ext Int 8 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 19: Ext Int 9 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 20: Serial 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 21: Serial 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 22: Serial 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 23: XPT_DMA */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 24: DCR timeout */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 25: Ext Int 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 26: Ext Int 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 27: Ext Int 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 28: Ext Int 3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 29: Ext Int 4 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 30: Ext Int 5 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 31: Ext Int 6 */
};


void __init
board_setup_arch(void)
{
#ifdef CONFIG_IDE
	void *xilinx, *xilinx_1, *xilinx_2;
	unsigned short us_reg5;

	xilinx = (unsigned long) ioremap(IDE_XLINUX_MUX_BASE, 0x10);
	/* init xilinx control registers - enable ide mux, clear reset bit */
	if (!xilinx) {
		printk(KERN_CRIT
		       "redwood6_setup_arch() xilinxi ioremap failed\n");
		return;
	}
	xilinx_1 = xilinx + 0xa;
	xilinx_2 = xilinx + 0xe;

	us_reg5 = readb(xilinx_1);
	writeb(0x01d1, xilinx_1);
	writeb(0x0008, xilinx_2);

	udelay(10 * 1000);

	writeb(0x01d1, xilinx_1);
	writeb(0x0008, xilinx_2);
#endif

#ifdef DEBUG_BRINGUP
	bd_t *bip = (bd_t *) __res;
	printk("\n");
	printk("machine\t: %s\n", PPC4xx_MACHINE_NAME);
	printk("\n");
	printk("bi_s_version\t %s\n", bip->bi_s_version);
	printk("bi_r_version\t %s\n", bip->bi_r_version);
	printk("bi_memsize\t 0x%8.8x\t %dMBytes\n", bip->bi_memsize,
	       bip->bi_memsize / (1024 * 1000));
	printk("bi_enetaddr %d\t %2.2x%2.2x%2.2x-%2.2x%2.2x%2.2x\n", 0,
	       bip->bi_enetaddr[0], bip->bi_enetaddr[1], bip->bi_enetaddr[2],
	       bip->bi_enetaddr[3], bip->bi_enetaddr[4], bip->bi_enetaddr[5]);

	printk("bi_intfreq\t 0x%8.8x\t clock:\t %dMhz\n",
	       bip->bi_intfreq, bip->bi_intfreq / 1000000);

	printk("bi_busfreq\t 0x%8.8x\t plb bus clock:\t %dMHz\n",
	       bip->bi_busfreq, bip->bi_busfreq / 1000000);
	printk("bi_tbfreq\t 0x%8.8x\t TB freq:\t %dMHz\n",
	       bip->bi_tbfreq, bip->bi_tbfreq / 1000000);

	printk("\n");
#endif
	ibm4xxPIC_InitSenses = redwood6_IRQ_initsenses;
	ibm4xxPIC_NumInitSenses = sizeof(redwood6_IRQ_initsenses);

	/* Identify the system */
	printk(KERN_INFO "IBM Redwood6 (STBx25XX) Platform\n");
	printk(KERN_INFO
	       "Port by MontaVista Software, Inc. (source@mvista.com)\n");
}

void __init
board_io_mapping(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		unsigned long v, p;

		/* 0x400x0000 -> 0xe00x0000 */
		p = 0x40000000 | (i << 16);
		v = STBx25xx_IO_BASE | (i << 16);

		io_block_mapping(v, p, PAGE_SIZE,
				 _PAGE_NO_CACHE | pgprot_val(PAGE_KERNEL) |
				 _PAGE_GUARDED);
	}
}

void __init
board_setup_irq(void)
{
}

void __init
board_init(void)
{
}
