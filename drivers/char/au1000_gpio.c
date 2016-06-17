/*
 * FILE NAME au1000_gpio.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Alchemy Au1000 GPIO.
 *
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          Steve Longerbeam <stevel@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE	LIABLE FOR ANY   DIRECT, INDIRECT,
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

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/au1000.h>
#include <asm/au1000_gpio.h>

#define VERSION "0.01"

static const struct {
	u32 active_hi;
	u32 avail_mask;
} pinfunc_to_avail[15] = {
	{1,  0x7<<16},   // 0  = SSI0     / GPIO[18:16]
	{-1, 0},         // 1  = AC97     / SSI1
	{1,  1<<19},     // 2  = IRDA     / GPIO19
	{1,  1<<20},     // 3  = UART0    / GPIO20
	{1,  0x1f<<24},  // 4  = NIC2     / GPIO[28:24]
	{1,  0x7<<29},   // 5  = I2S      / GPIO[31:29]
	{0,  1<<8},      // 6  = I2SDI    / GPIO8
	{0,  0x3f<<9},   // 7  = UART3    / GPIO[14:9]
	{0,  1<<15},     // 8  = IRFIRSEL / GPIO15
	{0,  1<<2},      // 9  = EXTCLK0 or OSC / GPIO2
	{0,  1<<3},      // 10 = EXTCLK1  / GPIO3
	{0,  1<<6},      // 11 = SMROMCKE / GPIO6
	{1,  1<<21},     // 12 = UART1    / GPIO21
	{1,  1<<22},     // 13 = UART2    / GPIO22
	{1,  1<<23}      // 14 = UART3    / GPIO23
};

	
u32 get_au1000_avail_gpio_mask(void)
{
	int i;
	u32 pinfunc = inl(SYS_PINFUNC);
	u32 avail_mask = 0; // start with no gpio available

	// first, check for GPIO's reprogrammed as peripheral pins
	for (i=0; i<15; i++) {
		if (pinfunc_to_avail[i].active_hi < 0)
			continue;
		if (!(pinfunc_to_avail[i].active_hi ^
		      ((pinfunc & (1<<i)) ? 1:0)))
			avail_mask |= pinfunc_to_avail[i].avail_mask;
	}

	// check for GPIO's used as interrupt sources
	avail_mask &= ~(inl(IC1_MASKRD) &
			(inl(IC1_CFG0RD) | inl(IC1_CFG1RD)));

#ifdef CONFIG_USB_OHCI
	avail_mask &= ~((1<<4) | (1<<11));
#ifndef CONFIG_AU1X00_USB_DEVICE
	avail_mask &= ~((1<<5) | (1<<13));
#endif
#endif
	
	return avail_mask;
}


/*
 * Tristate the requested GPIO pins specified in data.
 * Only available GPIOs will be tristated.
 */
int au1000gpio_tristate(u32 data)
{
	data &= get_au1000_avail_gpio_mask();

	if (data)
		outl(data, SYS_TRIOUTCLR);

	return 0;
}


/*
 * Return the pin state. Pins configured as outputs will return
 * the output state, and pins configured as inputs (tri-stated)
 * will return input pin state.
 */
int au1000gpio_in(u32 *data)
{
	*data = inl(SYS_PINSTATERD);
	return 0;
}


/*
 * Set/clear GPIO pins. Only available GPIOs will be affected.
 */
int au1000gpio_set(u32 data)
{
	data &= get_au1000_avail_gpio_mask();

	if (data)
		outl(data, SYS_OUTPUTSET);
	return 0;
}

int au1000gpio_clear(u32 data)
{
	data &= get_au1000_avail_gpio_mask();

	if (data)
		outl(data, SYS_OUTPUTCLR);
	return 0;
}

/*
 * Output data to GPIO pins. Only available GPIOs will be affected.
 */
int au1000gpio_out(u32 data)
{
	au1000gpio_set(data);
	au1000gpio_clear(~data);
	return 0;
}


EXPORT_SYMBOL(get_au1000_avail_gpio_mask);
EXPORT_SYMBOL(au1000gpio_tristate);
EXPORT_SYMBOL(au1000gpio_in);
EXPORT_SYMBOL(au1000gpio_set);
EXPORT_SYMBOL(au1000gpio_clear);
EXPORT_SYMBOL(au1000gpio_out);


static int au1000gpio_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;

	return 0;
}


static int au1000gpio_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;

	return 0;
}


static int au1000gpio_ioctl(struct inode *inode, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	int status;
	u32 val;
	
	switch(cmd) {
	case AU1000GPIO_IN:
		
		status = au1000gpio_in(&val);
		if (status != 0)
			return status;

		return put_user(val, (u32 *)arg);

	case AU1000GPIO_OUT:

		if (get_user(val, (u32 *)arg)) 
			return -EFAULT;

		return au1000gpio_out(val);

	case AU1000GPIO_SET:

		if (get_user(val, (u32 *)arg)) 
			return -EFAULT;

		return au1000gpio_set(val);
		
	case AU1000GPIO_CLEAR:

		if (get_user(val, (u32 *)arg)) 
			return -EFAULT;

		return au1000gpio_clear(val);

	case AU1000GPIO_TRISTATE:

		if (get_user(val, (u32 *)arg)) 
			return -EFAULT;

		return au1000gpio_tristate(val);

	case AU1000GPIO_AVAIL_MASK:
		
		return put_user(get_au1000_avail_gpio_mask(),
				(u32 *)arg);
		
	default:
		return -ENOIOCTLCMD;

	}

	return 0;
}


static struct file_operations au1000gpio_fops =
{
	owner:		THIS_MODULE,
	ioctl:		au1000gpio_ioctl,
	open:		au1000gpio_open,
	release:	au1000gpio_release,
};


static struct miscdevice au1000gpio_miscdev =
{
	GPIO_MINOR,
	"au1000_gpio",
	&au1000gpio_fops
};


int __init au1000gpio_init(void)
{
	misc_register(&au1000gpio_miscdev);
	printk("Au1000 gpio driver, version %s\n", VERSION);
	return 0;
}	


void __exit au1000gpio_exit(void)
{
	misc_deregister(&au1000gpio_miscdev);
}


module_init(au1000gpio_init);
module_exit(au1000gpio_exit);
