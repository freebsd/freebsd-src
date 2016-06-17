/*  linux/drivers/i2c/scx200_acb.c 

    Copyright (c) 2001,2002 Christer Weinigel <wingel@nano-system.com>

    National Semiconductor SCx200 ACCESS.bus support
    
    Based on i2c-keywest.c which is:
        Copyright (c) 2001 Benjamin Herrenschmidt <benh@kernel.crashing.org>
        Copyright (c) 2000 Philip Edelbrock <phil@stimpy.netroedge.com>
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.
   
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.
   
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/smp_lock.h>
#include <linux/pci.h>
#include <asm/io.h>

#include <linux/scx200.h>

#define NAME "scx200_acb"

MODULE_AUTHOR("Christer Weinigel <wingel@nano-system.com>");
MODULE_DESCRIPTION("NatSemi SCx200 ACCESS.bus Driver");
MODULE_LICENSE("GPL");

#define MAX_DEVICES 4
static int base[MAX_DEVICES] = { 0x840 };
MODULE_PARM(base, "1-4i");
MODULE_PARM_DESC(base, "Base addresses for the ACCESS.bus controllers");

#define DEBUG 0

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG NAME ": " x)
#else
#define DBG(x...)
#endif

/* The hardware supports interrupt driven mode too, but I haven't
   implemented that. */
#define POLLED_MODE 1
#define POLL_TIMEOUT (HZ)

enum scx200_acb_state {
	state_idle,
	state_address,
	state_command,
	state_repeat_start,
	state_quick,
	state_read,
	state_write,
};

static const char *scx200_acb_state_name[] = {
	"idle",
	"address",
	"command",
	"repeat_start",
	"quick",
	"read",
	"write",
};

/* Physical interface */
struct scx200_acb_iface
{
	struct scx200_acb_iface *next;
	struct i2c_adapter adapter;
	unsigned base;
	struct semaphore sem;

	/* State machine data */
	enum scx200_acb_state state;
	int result;
	u8 address_byte;
	u8 command;
	u8 *ptr;
	char needs_reset;
	unsigned len;
};

/* Register Definitions */
#define ACBSDA		(iface->base + 0)
#define ACBST		(iface->base + 1)
#define    ACBST_SDAST		0x40 /* SDA Status */
#define    ACBST_BER		0x20 
#define    ACBST_NEGACK		0x10 /* Negative Acknowledge */
#define    ACBST_STASTR		0x08 /* Stall After Start */
#define    ACBST_MASTER		0x02
#define ACBCST		(iface->base + 2)
#define    ACBCST_BB		0x02
#define ACBCTL1		(iface->base + 3)
#define    ACBCTL1_STASTRE	0x80
#define    ACBCTL1_NMINTE	0x40
#define	   ACBCTL1_ACK		0x10
#define	   ACBCTL1_STOP		0x02
#define	   ACBCTL1_START	0x01
#define ACBADDR		(iface->base + 4)
#define ACBCTL2		(iface->base + 5)
#define    ACBCTL2_ENABLE	0x01

/************************************************************************/

static void scx200_acb_machine(struct scx200_acb_iface *iface, u8 status)
{
	const char *errmsg;

	DBG("state %s, status = 0x%02x\n", 
	    scx200_acb_state_name[iface->state], status);

	if (status & ACBST_BER) {
		errmsg = "bus error";
		goto error;
	}
	if (!(status & ACBST_MASTER)) {
		errmsg = "not master";
		goto error;
	}
	if (status & ACBST_NEGACK)
		goto negack;

	switch (iface->state) {
	case state_idle:
		printk(KERN_WARNING NAME ": %s, interrupt in idle state\n", 
		       iface->adapter.name);
		break;

	case state_address:
		/* Do a pointer write first */
		outb(iface->address_byte & ~1, ACBSDA);

		iface->state = state_command;
		break;

	case state_command:
		outb(iface->command, ACBSDA);

		if (iface->address_byte & 1)
			iface->state = state_repeat_start;
		else
			iface->state = state_write;
		break;

	case state_repeat_start:
		outb(inb(ACBCTL1) | ACBCTL1_START, ACBCTL1);
		/* fallthrough */
		
	case state_quick:
		if (iface->address_byte & 1) {
			if (iface->len == 1) 
				outb(inb(ACBCTL1) | ACBCTL1_ACK, ACBCTL1);
			else
				outb(inb(ACBCTL1) & ~ACBCTL1_ACK, ACBCTL1);
			outb(iface->address_byte, ACBSDA);

			iface->state = state_read;
		} else {
			outb(iface->address_byte, ACBSDA);

			iface->state = state_write;
		}
		break;

	case state_read:
		/* Set ACK if receiving the last byte */
		if (iface->len == 1)
			outb(inb(ACBCTL1) | ACBCTL1_ACK, ACBCTL1);
		else
			outb(inb(ACBCTL1) & ~ACBCTL1_ACK, ACBCTL1);

		*iface->ptr++ = inb(ACBSDA);
		--iface->len;

		if (iface->len == 0) {
			iface->result = 0;
			iface->state = state_idle;
			outb(inb(ACBCTL1) | ACBCTL1_STOP, ACBCTL1);
		}

		break;

	case state_write:
		if (iface->len == 0) {
			iface->result = 0;
			iface->state = state_idle;
			outb(inb(ACBCTL1) | ACBCTL1_STOP, ACBCTL1);
			break;
		}
		
		outb(*iface->ptr++, ACBSDA);
		--iface->len;
		
		break;
	}

	return;

 negack:
	DBG("negative acknowledge in state %s\n", 
	    scx200_acb_state_name[iface->state]);

	iface->state = state_idle;
	iface->result = -ENXIO;

	outb(inb(ACBCTL1) | ACBCTL1_STOP, ACBCTL1);
	outb(ACBST_STASTR | ACBST_NEGACK, ACBST);
	return;

 error:
	printk(KERN_ERR NAME ": %s, %s in state %s\n", iface->adapter.name, 
	       errmsg, scx200_acb_state_name[iface->state]);

	iface->state = state_idle;
	iface->result = -EIO;
	iface->needs_reset = 1;
}

static void scx200_acb_timeout(struct scx200_acb_iface *iface) 
{
	printk(KERN_ERR NAME ": %s, timeout in state %s\n", 
	       iface->adapter.name, scx200_acb_state_name[iface->state]);

	iface->state = state_idle;
	iface->result = -EIO;
	iface->needs_reset = 1;
}

#ifdef POLLED_MODE
static void scx200_acb_poll(struct scx200_acb_iface *iface)
{
	u8 status = 0;
	unsigned long timeout;

	timeout = jiffies + POLL_TIMEOUT;
	while (time_before(jiffies, timeout)) {
		status = inb(ACBST);
		if ((status & (ACBST_SDAST|ACBST_BER|ACBST_NEGACK)) != 0) {
			scx200_acb_machine(iface, status);
			return;
		}
		yield();
	}

	scx200_acb_timeout(iface);
}
#endif /* POLLED_MODE */

static void scx200_acb_reset(struct scx200_acb_iface *iface)
{
	/* Disable the ACCESS.bus device and Configure the SCL
           frequency: 16 clock cycles */
	outb(0x70, ACBCTL2);
	/* Polling mode */
	outb(0, ACBCTL1);
	/* Disable slave address */
	outb(0, ACBADDR);
	/* Enable the ACCESS.bus device */
	outb(inb(ACBCTL2) | ACBCTL2_ENABLE, ACBCTL2);
	/* Free STALL after START */
	outb(inb(ACBCTL1) & ~(ACBCTL1_STASTRE | ACBCTL1_NMINTE), ACBCTL1);
	/* Send a STOP */
	outb(inb(ACBCTL1) | ACBCTL1_STOP, ACBCTL1);
	/* Clear BER, NEGACK and STASTR bits */
	outb(ACBST_BER | ACBST_NEGACK | ACBST_STASTR, ACBST);
	/* Clear BB bit */
	outb(inb(ACBCST) | ACBCST_BB, ACBCST);
}

static s32 scx200_acb_smbus_xfer(struct i2c_adapter *adapter,
				u16 address, unsigned short flags,	
				char rw, u8 command, int size, 
				union i2c_smbus_data *data)
{
	struct scx200_acb_iface *iface = adapter->data;
	int len;
	u8 *buffer;
	u16 cur_word;
	int rc;

	switch (size) {
	case I2C_SMBUS_QUICK:
	    	len = 0;
	    	buffer = NULL;
	    	break;
	case I2C_SMBUS_BYTE:
		if (rw == I2C_SMBUS_READ) {
			len = 1;
			buffer = &data->byte;
		} else {
			len = 1;
			buffer = &command;
		}
	    	break;
	case I2C_SMBUS_BYTE_DATA:
	    	len = 1;
	    	buffer = &data->byte;
	    	break;
	case I2C_SMBUS_WORD_DATA:
		len = 2;
	    	cur_word = cpu_to_le16(data->word);
	    	buffer = (u8 *)&cur_word;
		break;
	case I2C_SMBUS_BLOCK_DATA:
	    	len = data->block[0];
	    	buffer = &data->block[1];
		break;
	default:
	    	return -EINVAL;
	}

	DBG("size=%d, address=0x%x, command=0x%x, len=%d, read=%d\n",
	    size, address, command, len, rw == I2C_SMBUS_READ);

	if (!len && rw == I2C_SMBUS_READ) {
		printk(KERN_WARNING NAME ": %s, zero length read\n", 
		       adapter->name);
		return -EINVAL;
	}

	if (len && !buffer) {
		printk(KERN_WARNING NAME ": %s, nonzero length but no buffer\n", adapter->name);
		return -EFAULT;
	}

	down(&iface->sem);

	iface->address_byte = address<<1;
	if (rw == I2C_SMBUS_READ)
		iface->address_byte |= 1;
	iface->command = command;
	iface->ptr = buffer;
	iface->len = len;
	iface->result = -EINVAL;
	iface->needs_reset = 0;

	outb(inb(ACBCTL1) | ACBCTL1_START, ACBCTL1);

	if (size == I2C_SMBUS_QUICK || size == I2C_SMBUS_BYTE)
		iface->state = state_quick;
	else
		iface->state = state_address;

#ifdef POLLED_MODE
	while (iface->state != state_idle)
		scx200_acb_poll(iface);
#else /* POLLED_MODE */
#error Interrupt driven mode not implemented
#endif /* POLLED_MODE */	

	if (iface->needs_reset)
		scx200_acb_reset(iface);

	rc = iface->result;

	up(&iface->sem);

	if (rc == 0 && size == I2C_SMBUS_WORD_DATA && rw == I2C_SMBUS_READ)
	    	data->word = le16_to_cpu(cur_word);

#if DEBUG
	printk(KERN_DEBUG NAME ": transfer done, result: %d", rc);
	if (buffer) {
		int i;
		printk(" data:");
		for (i = 0; i < len; ++i)
			printk(" %02x", buffer[i]);
	}
	printk("\n");
#endif

	return rc;
}

static u32 scx200_acb_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	       I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	       I2C_FUNC_SMBUS_BLOCK_DATA;
}

static int scx200_acb_reg(struct i2c_client *client)
{
	return 0;
}

static int scx200_acb_unreg(struct i2c_client *client)
{
	return 0;
}

static void scx200_acb_inc_use(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

static void scx200_acb_dec_use(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

/* For now, we only handle combined mode (smbus) */
static struct i2c_algorithm scx200_acb_algorithm = {
	name:		"NatSemi SCx200 ACCESS.bus",
	id:		I2C_ALGO_SMBUS,
	smbus_xfer:	scx200_acb_smbus_xfer,
	functionality:	scx200_acb_func,
};

struct scx200_acb_iface *scx200_acb_list;

int scx200_acb_probe(struct scx200_acb_iface *iface)
{
	u8 val;

	/* Disable the ACCESS.bus device and Configure the SCL
           frequency: 16 clock cycles */
	outb(0x70, ACBCTL2);

	if (inb(ACBCTL2) != 0x70) {
		DBG("ACBCTL2 readback failed\n");
		return -ENXIO;
	}

	outb(inb(ACBCTL1) | ACBCTL1_NMINTE, ACBCTL1);

	val = inb(ACBCTL1);
	if (val) {
		DBG("disabled, but ACBCTL1=0x%02x\n", val);
		return -ENXIO;
	}

	outb(inb(ACBCTL2) | ACBCTL2_ENABLE, ACBCTL2);

	outb(inb(ACBCTL1) | ACBCTL1_NMINTE, ACBCTL1);

	val = inb(ACBCTL1);
	if ((val & ACBCTL1_NMINTE) != ACBCTL1_NMINTE) {
		DBG("enabled, but NMINTE won't be set, ACBCTL1=0x%02x\n", val);
		return -ENXIO;
	}

	return 0;
}

static int  __init scx200_acb_create(int base, int index)
{
	struct scx200_acb_iface *iface;
	struct i2c_adapter *adapter;
	int rc = 0;
	char description[64];

	iface = kmalloc(sizeof(*iface), GFP_KERNEL);
	if (!iface) {
		printk(KERN_ERR NAME ": can't allocate memory\n");
		rc = -ENOMEM;
		goto errout;
	}

	memset(iface, 0, sizeof(*iface));
	adapter = &iface->adapter;
	adapter->data = iface;
	sprintf(adapter->name, "SCx200 ACB%d", index);
	adapter->id = I2C_ALGO_SMBUS;
	adapter->algo = &scx200_acb_algorithm;
	adapter->inc_use = scx200_acb_inc_use;
	adapter->dec_use = scx200_acb_dec_use;
	adapter->client_register = scx200_acb_reg;
	adapter->client_unregister = scx200_acb_unreg;

	init_MUTEX(&iface->sem);

	sprintf(description, "NatSemi SCx200 ACCESS.bus [%s]", adapter->name);
	if (request_region(base, 8, description) == 0) {
		printk(KERN_ERR NAME ": %s, can't allocate io 0x%x-0x%x\n", 
		       adapter->name, base, base + 8-1);
		rc = -EBUSY;
		goto errout;
	}
	iface->base = base;

	rc = scx200_acb_probe(iface);
	if (rc) {
		printk(KERN_WARNING NAME ": %s, probe failed\n", adapter->name);
		goto errout;
	}

	scx200_acb_reset(iface);

	if (i2c_add_adapter(adapter) < 0) {
		printk(KERN_ERR NAME ": %s, failed to register\n", adapter->name);
		rc = -ENODEV;
		goto errout;
	}

	lock_kernel();
	iface->next = scx200_acb_list;
	scx200_acb_list = iface;
	unlock_kernel();

	return 0;

 errout:
	if (iface) {
		if (iface->base)
			release_region(iface->base, 8);
		kfree(iface);
	}
	return rc;
}

static int __init scx200_acb_init(void)
{
	int i;
	int rc;

	printk(KERN_DEBUG NAME ": NatSemi SCx200 ACCESS.bus Driver\n");

	/* Verify that this really is a SCx200 processor */
	if (pci_find_device(PCI_VENDOR_ID_NS,
			    PCI_DEVICE_ID_NS_SCx200_BRIDGE,
			    NULL) == NULL)
		return -ENODEV;

	rc = -ENXIO;
	for (i = 0; i < MAX_DEVICES; ++i) {
		if (base[i] > 0)
			rc = scx200_acb_create(base[i], i);
	}
	if (scx200_acb_list)
		return 0;
	return rc;
}

static void __exit scx200_acb_cleanup(void)
{
	struct scx200_acb_iface *iface;
	lock_kernel();
	while ((iface = scx200_acb_list) != NULL) {
		scx200_acb_list = iface->next;
		unlock_kernel();

		i2c_del_adapter(&iface->adapter);
		release_region(iface->base, 8);
		kfree(iface);
		lock_kernel();
	}
	unlock_kernel();
}

module_init(scx200_acb_init);
module_exit(scx200_acb_cleanup);

/*
    Local variables:
        compile-command: "make -k -C ../.. SUBDIRS=drivers/i2c modules"
        c-basic-offset: 8
    End:
*/

