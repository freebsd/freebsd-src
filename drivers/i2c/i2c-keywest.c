/*
    i2c Support for Apple Keywest I2C Bus Controller

    Copyright (c) 2001 Benjamin Herrenschmidt <benh@kernel.crashing.org>

    Original work by
    
    Copyright (c) 2000 Philip Edelbrock <phil@stimpy.netroedge.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Changes:

    2001/12/13 BenH	New implementation
    2001/12/15 BenH	Add support for "byte" and "quick"
                        transfers. Add i2c_xfer routine.

    My understanding of the various modes supported by keywest are:

     - Dumb mode : not implemented, probably direct tweaking of lines
     - Standard mode : simple i2c transaction of type
         S Addr R/W A Data A Data ... T
     - Standard sub mode : combined 8 bit subaddr write with data read
         S Addr R/W A SubAddr A Data A Data ... T
     - Combined mode : Subaddress and Data sequences appended with no stop
         S Addr R/W A SubAddr S Addr R/W A Data A Data ... T

    Currently, this driver uses only Standard mode for i2c xfer, and
    smbus byte & quick transfers ; and uses StandardSub mode for
    other smbus transfers instead of combined as we need that for the
    sound driver to be happy
*/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/completion.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>

#include "i2c-keywest.h"

#define DBG(x...) do {\
	if (debug > 0) \
		printk(KERN_DEBUG "KW:" x); \
	} while(0)


MODULE_AUTHOR("Benjamin Herrenschmidt <benh@kernel.crashing.org>");
MODULE_DESCRIPTION("I2C driver for Apple's Keywest");
MODULE_LICENSE("GPL");
MODULE_PARM(probe, "i");
MODULE_PARM(debug, "i");
EXPORT_NO_SYMBOLS;

int probe = 0;
int debug = 0;

static struct keywest_iface *ifaces = NULL;


static void
do_stop(struct keywest_iface* iface, int result)
{
	write_reg(reg_control, read_reg(reg_control) | KW_I2C_CTL_STOP);
	iface->state = state_stop;
	iface->result = result;
}

/* Main state machine for standard & standard sub mode */
static int
handle_interrupt(struct keywest_iface *iface, u8 isr)
{
	int ack;
	int rearm_timer = 1;
	
	DBG("handle_interrupt(), got: %x, status: %x, state: %d\n",
		isr, read_reg(reg_status), iface->state);
	if (isr == 0 && iface->state != state_stop) {
		do_stop(iface, -1);
		return rearm_timer;
	}
	if (isr & KW_I2C_IRQ_STOP && iface->state != state_stop) {
		iface->result = -1;
		iface->state = state_stop;
	}
	switch(iface->state) {
	case state_addr:
		if (!(isr & KW_I2C_IRQ_ADDR)) {
			do_stop(iface, -1);
			break;
		}
		ack = read_reg(reg_status);
		DBG("ack on set address: %x\n", ack);
		if ((ack & KW_I2C_STAT_LAST_AAK) == 0) {
			do_stop(iface, -1);
			break;
		}
		/* Handle rw "quick" mode */
		if (iface->datalen == 0)
			do_stop(iface, 0);
		else if (iface->read_write == I2C_SMBUS_READ) {
			iface->state = state_read;
			if (iface->datalen > 1)
				write_reg(reg_control, read_reg(reg_control)
					| KW_I2C_CTL_AAK);
		} else {
			iface->state = state_write;
			DBG("write byte: %x\n", *(iface->data));
			write_reg(reg_data, *(iface->data++));
			iface->datalen--;
		}
		
		break;
	case state_read:
		if (!(isr & KW_I2C_IRQ_DATA)) {
			do_stop(iface, -1);
			break;
		}
		*(iface->data++) = read_reg(reg_data);
		DBG("read byte: %x\n", *(iface->data-1));
		iface->datalen--;
		if (iface->datalen == 0)
			iface->state = state_stop;
		else
			write_reg(reg_control, 0);
		break;
	case state_write:
		if (!(isr & KW_I2C_IRQ_DATA)) {
			do_stop(iface, -1);
			break;
		}
		/* Check ack status */
		ack = read_reg(reg_status);
		DBG("ack on data write: %x\n", ack);
		if ((ack & KW_I2C_STAT_LAST_AAK) == 0) {
			do_stop(iface, -1);
			break;
		}
		if (iface->datalen) {
			DBG("write byte: %x\n", *(iface->data));
			write_reg(reg_data, *(iface->data++));
			iface->datalen--;
		} else
			do_stop(iface, 0);
		break;
		
	case state_stop:
		if (!(isr & KW_I2C_IRQ_STOP) && (++iface->stopretry) < 10)
			do_stop(iface, -1);
		else {
			rearm_timer = 0;
			iface->state = state_idle;
			write_reg(reg_control, 0x00);
			write_reg(reg_ier, 0x00);
			complete(&iface->complete);
		}
		break;
	}
	
	write_reg(reg_isr, isr);

	return rearm_timer;
}

/* Interrupt handler */
static void
keywest_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct keywest_iface *iface = (struct keywest_iface *)dev_id;

	spin_lock(&iface->lock);
	del_timer(&iface->timeout_timer);
	if (handle_interrupt(iface, read_reg(reg_isr))) {
		iface->timeout_timer.expires = jiffies + POLL_TIMEOUT;
		add_timer(&iface->timeout_timer);
	}
	spin_unlock(&iface->lock);
}

static void
keywest_timeout(unsigned long data)
{
	struct keywest_iface *iface = (struct keywest_iface *)data;

	DBG("timeout !\n");
	spin_lock_irq(&iface->lock);
	if (handle_interrupt(iface, read_reg(reg_isr))) {
		iface->timeout_timer.expires = jiffies + POLL_TIMEOUT;
		add_timer(&iface->timeout_timer);
	}
	spin_unlock(&iface->lock);
}

/*
 * SMBUS-type transfer entrypoint
 */
static s32
keywest_smbus_xfer(	struct i2c_adapter*	adap,
			u16			addr,
			unsigned short		flags,
			char			read_write,
			u8			command,
			int			size,
			union i2c_smbus_data*	data)
{
	struct keywest_chan* chan = (struct keywest_chan*)adap->data;
	struct keywest_iface* iface = chan->iface;
	int len;
	u8* buffer;
	u16 cur_word;
	int rc = 0;

	if (iface->state == state_dead)
		return -1;
		
	/* Prepare datas & select mode */
	iface->cur_mode &= ~KW_I2C_MODE_MODE_MASK;
	switch (size) {
	    case I2C_SMBUS_QUICK:
	    	len = 0;
	    	buffer = NULL;
	    	iface->cur_mode |= KW_I2C_MODE_STANDARD;
	    	break;
	    case I2C_SMBUS_BYTE:
	    	len = 1;
	    	buffer = &data->byte;
	    	iface->cur_mode |= KW_I2C_MODE_STANDARD;
	    	break;
	    case I2C_SMBUS_BYTE_DATA:
	    	len = 1;
	    	buffer = &data->byte;
	    	iface->cur_mode |= KW_I2C_MODE_STANDARDSUB;
	    	break;
	    case I2C_SMBUS_WORD_DATA:
	    	len = 2;
	    	cur_word = cpu_to_le16(data->word);
	    	buffer = (u8 *)&cur_word;
	    	iface->cur_mode |= KW_I2C_MODE_STANDARDSUB;
		break;
	    case I2C_SMBUS_BLOCK_DATA:
	    	len = data->block[0];
	    	buffer = &data->block[1];
	    	iface->cur_mode |= KW_I2C_MODE_STANDARDSUB;
		break;
	    default:
	    	return -1;
	}

	/* Original driver had this limitation */
	if (len > 32)
		len = 32;

	down(&iface->sem);

	DBG("chan: %d, addr: 0x%x, transfer len: %d, read: %d\n",
		chan->chan_no, addr, len, read_write == I2C_SMBUS_READ);

	iface->data = buffer;
	iface->datalen = len;
	iface->state = state_addr;
	iface->result = 0;
	iface->stopretry = 0;
	iface->read_write = read_write;
	
	/* Setup channel & clear pending irqs */
	write_reg(reg_mode, iface->cur_mode | (chan->chan_no << 4));
	write_reg(reg_isr, read_reg(reg_isr));
	write_reg(reg_status, 0);

	/* Set up address and r/w bit */
	write_reg(reg_addr,
		(addr << 1) | ((read_write == I2C_SMBUS_READ) ? 0x01 : 0x00));

	/* Set up the sub address */
	if ((iface->cur_mode & KW_I2C_MODE_MODE_MASK) == KW_I2C_MODE_STANDARDSUB
	    || (iface->cur_mode & KW_I2C_MODE_MODE_MASK) == KW_I2C_MODE_COMBINED)
		write_reg(reg_subaddr, command);

	/* Arm timeout */
	iface->timeout_timer.expires = jiffies + POLL_TIMEOUT;
	add_timer(&iface->timeout_timer);

	/* Start sending address & enable interrupt*/
	write_reg(reg_control, read_reg(reg_control) | KW_I2C_CTL_XADDR);
	write_reg(reg_ier, KW_I2C_IRQ_MASK);

	wait_for_completion(&iface->complete);	

	rc = iface->result;	
	DBG("transfer done, result: %d\n", rc);

	if (rc == 0 && size == I2C_SMBUS_WORD_DATA && read_write == I2C_SMBUS_READ)
	    	data->word = le16_to_cpu(cur_word);
	
	/* Release sem */
	up(&iface->sem);
	
	return rc;
}

/*
 * Generic i2c master transfer entrypoint
 */
static int
keywest_xfer(	struct i2c_adapter *adap,
		struct i2c_msg msgs[], 
		int num)
{
	struct keywest_chan* chan = (struct keywest_chan*)adap->data;
	struct keywest_iface* iface = chan->iface;
	struct i2c_msg *pmsg;
	int i, completed;
	int rc = 0;

	down(&iface->sem);

	/* Set adapter to standard mode */
	iface->cur_mode &= ~KW_I2C_MODE_MODE_MASK;
	iface->cur_mode |= KW_I2C_MODE_STANDARD;

	completed = 0;
	for (i = 0; rc >= 0 && i < num;) {
		u8 addr;
		
		pmsg = &msgs[i++];
		addr = pmsg->addr;
		if (pmsg->flags & I2C_M_TEN) {
			printk(KERN_ERR "i2c-keywest: 10 bits addr not supported !\n");
			rc = -EINVAL;
			break;
		}
		DBG("xfer: chan: %d, doing %s %d bytes to 0x%02x - %d of %d messages\n",
		     chan->chan_no,
		     pmsg->flags & I2C_M_RD ? "read" : "write",
                     pmsg->len, addr, i, num);
    
		/* Setup channel & clear pending irqs */
		write_reg(reg_mode, iface->cur_mode | (chan->chan_no << 4));
		write_reg(reg_isr, read_reg(reg_isr));
		write_reg(reg_status, 0);
		
		iface->data = pmsg->buf;
		iface->datalen = pmsg->len;
		iface->state = state_addr;
		iface->result = 0;
		iface->stopretry = 0;
		if (pmsg->flags & I2C_M_RD)
			iface->read_write = I2C_SMBUS_READ;
		else
			iface->read_write = I2C_SMBUS_WRITE;

		/* Set up address and r/w bit */
		if (pmsg->flags & I2C_M_REV_DIR_ADDR)
			addr ^= 1;		
		write_reg(reg_addr,
			(addr << 1) |
			((iface->read_write == I2C_SMBUS_READ) ? 0x01 : 0x00));

		/* Arm timeout */
		iface->timeout_timer.expires = jiffies + POLL_TIMEOUT;
		add_timer(&iface->timeout_timer);

		/* Start sending address & enable interrupt*/
		write_reg(reg_control, read_reg(reg_control) | KW_I2C_CTL_XADDR);
		write_reg(reg_ier, KW_I2C_IRQ_MASK);

		wait_for_completion(&iface->complete);	

		rc = iface->result;
		if (rc == 0)
			completed++;
		DBG("transfer done, result: %d\n", rc);
	}

	/* Release sem */
	up(&iface->sem);

	return completed;
}

static u32
keywest_func(struct i2c_adapter * adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	       I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	       I2C_FUNC_SMBUS_BLOCK_DATA;
}

static void
keywest_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

static void
keywest_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

/* For now, we only handle combined mode (smbus) */
static struct i2c_algorithm keywest_algorithm = {
	name:		"Keywest i2c",
	id:		I2C_ALGO_SMBUS,
	smbus_xfer:	keywest_smbus_xfer,
	master_xfer:	keywest_xfer,
	functionality:	keywest_func,
};


static int
create_iface(struct device_node* np)
{
	unsigned long steps, *psteps, *prate;
	unsigned bsteps, tsize, i, nchan, addroffset;
	struct keywest_iface* iface;
	int rc;

	psteps = (unsigned long *)get_property(np, "AAPL,address-step", NULL);
	steps = psteps ? (*psteps) : 0x10;

	/* Hrm... maybe we can be smarter here */
	for (bsteps = 0; (steps & 0x01) == 0; bsteps++)
		steps >>= 1;

	if (!strcmp(np->parent->name, "uni-n")) {
		nchan = 2;
		addroffset = 3;
	} else {
		addroffset = 0;
		nchan = 1;
	}

	tsize = sizeof(struct keywest_iface) +
		(sizeof(struct keywest_chan) + 4) * nchan;
	iface = (struct keywest_iface *) kmalloc(tsize, GFP_KERNEL);
	if (iface == NULL) {
		printk(KERN_ERR "i2c-keywest: can't allocate inteface !\n");
		return -ENOMEM;
	}
	memset(iface, 0, tsize);
	init_MUTEX(&iface->sem);
	spin_lock_init(&iface->lock);
	init_completion(&iface->complete);
	iface->bsteps = bsteps;
	iface->chan_count = nchan;
	iface->state = state_idle;
	iface->irq = np->intrs[0].line;
	iface->channels = (struct keywest_chan *)
		(((unsigned long)(iface + 1) + 3UL) & ~3UL);
	iface->base = (unsigned long)ioremap(np->addrs[0].address + addroffset,
						np->addrs[0].size);
	if (iface->base == 0) {
		printk(KERN_ERR "i2c-keywest: can't map inteface !\n");
		kfree(iface);
		return -ENOMEM;
	}

	init_timer(&iface->timeout_timer);
	iface->timeout_timer.function = keywest_timeout;
	iface->timeout_timer.data = (unsigned long)iface;

	/* Select interface rate */
	iface->cur_mode = KW_I2C_MODE_100KHZ;
	prate = (unsigned long *)get_property(np, "AAPL,i2c-rate", NULL);
	if (prate) switch(*prate) {
	case 100:
		iface->cur_mode = KW_I2C_MODE_100KHZ;
		break;
	case 50:
		iface->cur_mode = KW_I2C_MODE_50KHZ;
		break;
	case 25:
		iface->cur_mode = KW_I2C_MODE_25KHZ;
		break;
	default:
		printk(KERN_WARNING "i2c-keywest: unknown rate %ldKhz, using 100KHz\n",
			*prate);
	}
	
	/* Select standard mode by default */
	iface->cur_mode |= KW_I2C_MODE_STANDARD;
	
	/* Write mode */
	write_reg(reg_mode, iface->cur_mode);
	
	/* Switch interrupts off & clear them*/
	write_reg(reg_ier, 0x00);
	write_reg(reg_isr, KW_I2C_IRQ_MASK);

	/* Request chip interrupt */	
	rc = request_irq(iface->irq, keywest_irq, 0, "keywest i2c", iface);
	if (rc) {
		printk(KERN_ERR "i2c-keywest: can't get IRQ %d !\n", iface->irq);
		iounmap((void *)iface->base);
		kfree(iface);
		return -ENODEV;
	}

	for (i=0; i<nchan; i++) {
		struct keywest_chan* chan = &iface->channels[i];
		u8 addr;
		
		sprintf(chan->adapter.name, "%s %d", np->parent->name, i);
		chan->iface = iface;
		chan->chan_no = i;
		chan->adapter.id = I2C_ALGO_SMBUS;
		chan->adapter.algo = &keywest_algorithm;
		chan->adapter.algo_data = NULL;
		chan->adapter.inc_use = keywest_inc;
		chan->adapter.dec_use = keywest_dec;
		chan->adapter.client_register = NULL;
		chan->adapter.client_unregister = NULL;
		chan->adapter.data = chan;

		rc = i2c_add_adapter(&chan->adapter);
		if (rc) {
			printk("i2c-keywest.c: Adapter %s registration failed\n",
				chan->adapter.name);
			chan->adapter.data = NULL;
		}
		if (probe) {
			printk("Probe: ");
			for (addr = 0x00; addr <= 0x7f; addr++) {
				if (i2c_smbus_xfer(&chan->adapter,addr,
				    0,0,0,I2C_SMBUS_QUICK,NULL) >= 0)
					printk("%02x ", addr);
			}
			printk("\n");
		}
	}

	printk(KERN_INFO "Found KeyWest i2c on \"%s\", %d channel%s, stepping: %d bits\n",
		np->parent->name, nchan, nchan > 1 ? "s" : "", bsteps);
		
	iface->next = ifaces;
	ifaces = iface;
	return 0;
}

static void
dispose_iface(struct keywest_iface *iface)
{
	int i, rc;
	
	ifaces = iface->next;

	/* Make sure we stop all activity */
	down(&iface->sem);
	spin_lock_irq(&iface->lock);
	while (iface->state != state_idle) {
		spin_unlock_irq(&iface->lock);
		set_task_state(current,TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/10);
		spin_lock_irq(&iface->lock);
	}
	iface->state = state_dead;
	spin_unlock_irq(&iface->lock);
	free_irq(iface->irq, iface);
	up(&iface->sem);

	/* Release all channels */
	for (i=0; i<iface->chan_count; i++) {
		struct keywest_chan* chan = &iface->channels[i];
		if (!chan->adapter.data)
			continue;
		rc = i2c_del_adapter(&chan->adapter);
		chan->adapter.data = NULL;
		/* We aren't that prepared to deal with this... */
		if (rc)
			printk("i2c-keywest.c: i2c_del_adapter failed, that's bad !\n");
	}
	iounmap((void *)iface->base);
	kfree(iface);
}

static int __init
i2c_keywest_init(void)
{
	struct device_node *np;
	int rc = -ENODEV;
	
	np = find_compatible_devices("i2c", "keywest");
	while (np != 0) {
		if (np->n_addrs >= 1 && np->n_intrs >= 1)
			rc = create_iface(np);
		np = np->next;
	}
	if (ifaces)
		rc = 0;
	return rc;
}

static void __exit
i2c_keywest_cleanup(void)
{
	while(ifaces)
		dispose_iface(ifaces);
}

module_init(i2c_keywest_init);
module_exit(i2c_keywest_cleanup);
