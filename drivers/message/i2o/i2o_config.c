/*
 * I2O Configuration Interface Driver
 *
 * (C) Copyright 1999   Red Hat Software
 *	
 * Written by Alan Cox, Building Number Three Ltd
 *
 * Modified 04/20/1999 by Deepak Saxena
 *   - Added basic ioctl() support
 * Modified 06/07/1999 by Deepak Saxena
 *   - Added software download ioctl (still testing)
 * Modified 09/10/1999 by Auvo Häkkinen
 *   - Changes to i2o_cfg_reply(), ioctl_parms()
 *   - Added ioct_validate()
 * Modified 09/30/1999 by Taneli Vähäkangas
 *   - Fixed ioctl_swdl()
 * Modified 10/04/1999 by Taneli Vähäkangas
 *   - Changed ioctl_swdl(), implemented ioctl_swul() and ioctl_swdel()
 * Modified 11/18/199 by Deepak Saxena
 *   - Added event managmenet support
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/i2o.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/io.h>

static int i2o_cfg_context = -1;
static void *page_buf;
static spinlock_t i2o_config_lock = SPIN_LOCK_UNLOCKED;
struct wait_queue *i2o_wait_queue;

#define MODINC(x,y) ((x) = ((x) + 1) % (y))

struct i2o_cfg_info
{
	struct file* fp;
	struct fasync_struct *fasync;
	struct i2o_evt_info event_q[I2O_EVT_Q_LEN];
	u16		q_in;		// Queue head index
	u16		q_out;		// Queue tail index
	u16		q_len;		// Queue length
	u16		q_lost;		// Number of lost events
	u32		q_id;		// Event queue ID...used as tx_context
	struct	i2o_cfg_info *next;
};
static struct i2o_cfg_info *open_files = NULL;
static int i2o_cfg_info_id = 0;

static int ioctl_getiops(unsigned long);
static int ioctl_gethrt(unsigned long);
static int ioctl_getlct(unsigned long);
static int ioctl_parms(unsigned long, unsigned int);
static int ioctl_html(unsigned long);
static int ioctl_swdl(unsigned long);
static int ioctl_swul(unsigned long);
static int ioctl_swdel(unsigned long);
static int ioctl_validate(unsigned long); 
static int ioctl_evt_reg(unsigned long, struct file *);
static int ioctl_evt_get(unsigned long, struct file *);
static int cfg_fasync(int, struct file*, int);

/*
 *	This is the callback for any message we have posted. The message itself
 *	will be returned to the message pool when we return from the IRQ
 *
 *	This runs in irq context so be short and sweet.
 */
static void i2o_cfg_reply(struct i2o_handler *h, struct i2o_controller *c, struct i2o_message *m)
{
	u32 *msg = (u32 *)m;

	if (msg[0] & MSG_FAIL) {
		u32 *preserved_msg = (u32*)(c->mem_offset + msg[7]);

		printk(KERN_ERR "i2o_config: IOP failed to process the msg.\n");

		/* Release the preserved msg frame by resubmitting it as a NOP */

		preserved_msg[0] = THREE_WORD_MSG_SIZE | SGL_OFFSET_0;
		preserved_msg[1] = I2O_CMD_UTIL_NOP << 24 | HOST_TID << 12 | 0;
		preserved_msg[2] = 0;
		i2o_post_message(c, msg[7]);
	}

	if (msg[4] >> 24)  // ReqStatus != SUCCESS
		i2o_report_status(KERN_INFO,"i2o_config", msg);

	if(m->function == I2O_CMD_UTIL_EVT_REGISTER)
	{
		struct i2o_cfg_info *inf;

		for(inf = open_files; inf; inf = inf->next)
			if(inf->q_id == msg[3])
				break;

		//
		// If this is the case, it means that we're getting
		// events for a file descriptor that's been close()'d
		// w/o the user unregistering for events first.
		// The code currently assumes that the user will 
		// take care of unregistering for events before closing
		// a file.
		// 
		// TODO: 
		// Should we track event registartion and deregister
		// for events when a file is close()'d so this doesn't
		// happen? That would get rid of the search through
		// the linked list since file->private_data could point
		// directly to the i2o_config_info data structure...but
		// it would mean having all sorts of tables to track
		// what each file is registered for...I think the
		// current method is simpler. - DS
		//			
		if(!inf)
			return;

		inf->event_q[inf->q_in].id.iop = c->unit;
		inf->event_q[inf->q_in].id.tid = m->target_tid;
		inf->event_q[inf->q_in].id.evt_mask = msg[4];

		//
		// Data size = msg size - reply header
		//
		inf->event_q[inf->q_in].data_size = (m->size - 5) * 4;
		if(inf->event_q[inf->q_in].data_size)
			memcpy(inf->event_q[inf->q_in].evt_data, 
				(unsigned char *)(msg + 5),
				inf->event_q[inf->q_in].data_size);

		spin_lock(&i2o_config_lock);
		MODINC(inf->q_in, I2O_EVT_Q_LEN);
		if(inf->q_len == I2O_EVT_Q_LEN)
		{
			MODINC(inf->q_out, I2O_EVT_Q_LEN);
			inf->q_lost++;
		}
		else
		{
			// Keep I2OEVTGET on another CPU from touching this
			inf->q_len++;
		}
		spin_unlock(&i2o_config_lock);
		

//		printk(KERN_INFO "File %p w/id %d has %d events\n",
//			inf->fp, inf->q_id, inf->q_len);	

		kill_fasync(&inf->fasync, SIGIO, POLL_IN);
	}

	return;
}

/*
 *	Each of these describes an i2o message handler. They are
 *	multiplexed by the i2o_core code
 */
 
struct i2o_handler cfg_handler=
{
	i2o_cfg_reply,
	NULL,
	NULL,
	NULL,
	"Configuration",
	0,
	0xffffffff	// All classes
};

static ssize_t cfg_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "i2o_config write not yet supported\n");

	return 0;
}


static ssize_t cfg_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	return 0;
}

/*
 * IOCTL Handler
 */
static int cfg_ioctl(struct inode *inode, struct file *fp, unsigned int cmd,
	unsigned long arg)
{
	int ret;

	switch(cmd)
	{	
		case I2OGETIOPS:
			ret = ioctl_getiops(arg);
			break;

		case I2OHRTGET:
			ret = ioctl_gethrt(arg);
			break;

		case I2OLCTGET:
			ret = ioctl_getlct(arg);
			break;

		case I2OPARMSET:
			ret = ioctl_parms(arg, I2OPARMSET);
			break;

		case I2OPARMGET:
			ret = ioctl_parms(arg, I2OPARMGET);
			break;

		case I2OSWDL:
			ret = ioctl_swdl(arg);
			break;

		case I2OSWUL:
			ret = ioctl_swul(arg);
			break;

		case I2OSWDEL:
			ret = ioctl_swdel(arg);
			break;

		case I2OVALIDATE:
			ret = ioctl_validate(arg);
			break;
			
		case I2OHTML:
			ret = ioctl_html(arg);
			break;

		case I2OEVTREG:
			ret = ioctl_evt_reg(arg, fp);
			break;

		case I2OEVTGET:
			ret = ioctl_evt_get(arg, fp);
			break;

		default:
			ret = -EINVAL;
	}

	return ret;
}

int ioctl_getiops(unsigned long arg)
{
	u8 *user_iop_table = (u8*)arg;
	struct i2o_controller *c = NULL;
	int i;
	u8 foo[MAX_I2O_CONTROLLERS];

	if(!access_ok(VERIFY_WRITE, user_iop_table,  MAX_I2O_CONTROLLERS))
		return -EFAULT;

	for(i = 0; i < MAX_I2O_CONTROLLERS; i++)
	{
		c = i2o_find_controller(i);
		if(c)
		{
			foo[i] = 1;
			i2o_unlock_controller(c);
		}
		else
		{
			foo[i] = 0;
		}
	}

	__copy_to_user(user_iop_table, foo, MAX_I2O_CONTROLLERS);
	return 0;
}

int ioctl_gethrt(unsigned long arg)
{
	struct i2o_controller *c;
	struct i2o_cmd_hrtlct *cmd = (struct i2o_cmd_hrtlct*)arg;
	struct i2o_cmd_hrtlct kcmd;
	i2o_hrt *hrt;
	int len;
	u32 reslen;
	int ret = 0;

	if(copy_from_user(&kcmd, cmd, sizeof(struct i2o_cmd_hrtlct)))
		return -EFAULT;

	if(get_user(reslen, kcmd.reslen) < 0)
		return -EFAULT;

	if(kcmd.resbuf == NULL)
		return -EFAULT;

	c = i2o_find_controller(kcmd.iop);
	if(!c)
		return -ENXIO;
		
	hrt = (i2o_hrt *)c->hrt;

	i2o_unlock_controller(c);

	len = 8 + ((hrt->entry_len * hrt->num_entries) << 2);
	
	/* We did a get user...so assuming mem is ok...is this bad? */
	put_user(len, kcmd.reslen);
	if(len > reslen)
		ret = -ENOBUFS;	
	if(copy_to_user(kcmd.resbuf, (void*)hrt, len))
		ret = -EFAULT;

	return ret;
}

int ioctl_getlct(unsigned long arg)
{
	struct i2o_controller *c;
	struct i2o_cmd_hrtlct *cmd = (struct i2o_cmd_hrtlct*)arg;
	struct i2o_cmd_hrtlct kcmd;
	i2o_lct *lct;
	int len;
	int ret = 0;
	u32 reslen;

	if(copy_from_user(&kcmd, cmd, sizeof(struct i2o_cmd_hrtlct)))
		return -EFAULT;

	if(get_user(reslen, kcmd.reslen) < 0)
		return -EFAULT;

	if(kcmd.resbuf == NULL)
		return -EFAULT;

	c = i2o_find_controller(kcmd.iop);
	if(!c)
		return -ENXIO;

	lct = (i2o_lct *)c->lct;
	i2o_unlock_controller(c);

	len = (unsigned int)lct->table_size << 2;
	put_user(len, kcmd.reslen);
	if(len > reslen)
		ret = -ENOBUFS;	
	else if(copy_to_user(kcmd.resbuf, (void*)lct, len))
		ret = -EFAULT;

	return ret;
}

static int ioctl_parms(unsigned long arg, unsigned int type)
{
	int ret = 0;
	struct i2o_controller *c;
	struct i2o_cmd_psetget *cmd = (struct i2o_cmd_psetget*)arg;
	struct i2o_cmd_psetget kcmd;
	u32 reslen;
	u8 *ops;
	u8 *res;
	int len;

	u32 i2o_cmd = (type == I2OPARMGET ? 
				I2O_CMD_UTIL_PARAMS_GET :
				I2O_CMD_UTIL_PARAMS_SET);

	if(copy_from_user(&kcmd, cmd, sizeof(struct i2o_cmd_psetget)))
		return -EFAULT;

	if(get_user(reslen, kcmd.reslen))
		return -EFAULT;

	c = i2o_find_controller(kcmd.iop);
	if(!c)
		return -ENXIO;

	ops = (u8*)kmalloc(kcmd.oplen, GFP_KERNEL);
	if(!ops)
	{
		i2o_unlock_controller(c);
		return -ENOMEM;
	}

	if(copy_from_user(ops, kcmd.opbuf, kcmd.oplen))
	{
		i2o_unlock_controller(c);
		kfree(ops);
		return -EFAULT;
	}

	/*
	 * It's possible to have a _very_ large table
	 * and that the user asks for all of it at once...
	 */
	res = (u8*)kmalloc(65536, GFP_KERNEL);
	if(!res)
	{
		i2o_unlock_controller(c);
		kfree(ops);
		return -ENOMEM;
	}

	len = i2o_issue_params(i2o_cmd, c, kcmd.tid, 
				ops, kcmd.oplen, res, 65536);
	i2o_unlock_controller(c);
	kfree(ops);
        
	if (len < 0) {
		kfree(res);
		return -EAGAIN;
	}

	put_user(len, kcmd.reslen);
	if(len > reslen)
		ret = -ENOBUFS;
	else if(copy_to_user(kcmd.resbuf, res, len))
		ret = -EFAULT;

	kfree(res);

	return ret;
}

int ioctl_html(unsigned long arg)
{
	struct i2o_html *cmd = (struct i2o_html*)arg;
	struct i2o_html kcmd;
	struct i2o_controller *c;
	u8 *res = NULL;
	void *query = NULL;
	int ret = 0;
	int token;
	u32 len;
	u32 reslen;
	u32 msg[MSG_FRAME_SIZE];

	if(copy_from_user(&kcmd, cmd, sizeof(struct i2o_html)))
	{
		printk(KERN_INFO "i2o_config: can't copy html cmd\n");
		return -EFAULT;
	}

	if(get_user(reslen, kcmd.reslen) < 0)
	{
		printk(KERN_INFO "i2o_config: can't copy html reslen\n");
		return -EFAULT;
	}

	if(!kcmd.resbuf)		
	{
		printk(KERN_INFO "i2o_config: NULL html buffer\n");
		return -EFAULT;
	}

	c = i2o_find_controller(kcmd.iop);
	if(!c)
		return -ENXIO;

	if(kcmd.qlen) /* Check for post data */
	{
		query = kmalloc(kcmd.qlen, GFP_KERNEL);
		if(!query)
		{
			i2o_unlock_controller(c);
			return -ENOMEM;
		}
		if(copy_from_user(query, kcmd.qbuf, kcmd.qlen))
		{
			i2o_unlock_controller(c);
			printk(KERN_INFO "i2o_config: could not get query\n");
			kfree(query);
			return -EFAULT;
		}
	}

	res = kmalloc(65536, GFP_KERNEL);
	if(!res)
	{
		i2o_unlock_controller(c);
		kfree(query);
		return -ENOMEM;
	}

	msg[1] = (I2O_CMD_UTIL_CONFIG_DIALOG << 24)|HOST_TID<<12|kcmd.tid;
	msg[2] = i2o_cfg_context;
	msg[3] = 0;
	msg[4] = kcmd.page;
	msg[5] = 0xD0000000|65536;
	msg[6] = virt_to_bus(res);
	if(!kcmd.qlen) /* Check for post data */
		msg[0] = SEVEN_WORD_MSG_SIZE|SGL_OFFSET_5;
	else
	{
		msg[0] = NINE_WORD_MSG_SIZE|SGL_OFFSET_5;
		msg[5] = 0x50000000|65536;
		msg[7] = 0xD4000000|(kcmd.qlen);
		msg[8] = virt_to_bus(query);
	}
	/*
	Wait for a considerable time till the Controller 
	does its job before timing out. The controller might
	take more time to process this request if there are
	many devices connected to it.
	*/
	token = i2o_post_wait_mem(c, msg, 9*4, 400, query, res);
	if(token < 0)
	{
		printk(KERN_DEBUG "token = %#10x\n", token);
		i2o_unlock_controller(c);
		
		if(token != -ETIMEDOUT)
		{
			kfree(res);
			if(kcmd.qlen) kfree(query);
		}

		return token;
	}
	i2o_unlock_controller(c);

	len = strnlen(res, 65536);
	put_user(len, kcmd.reslen);
	if(len > reslen)
		ret = -ENOMEM;
	if(copy_to_user(kcmd.resbuf, res, len))
		ret = -EFAULT;

	kfree(res);
	if(kcmd.qlen) 
		kfree(query);

	return ret;
}
 
int ioctl_swdl(unsigned long arg)
{
	struct i2o_sw_xfer kxfer;
	struct i2o_sw_xfer *pxfer = (struct i2o_sw_xfer *)arg;
	unsigned char maxfrag = 0, curfrag = 1;
	unsigned char *buffer;
	u32 msg[9];
	unsigned int status = 0, swlen = 0, fragsize = 8192;
	struct i2o_controller *c;

	if(copy_from_user(&kxfer, pxfer, sizeof(struct i2o_sw_xfer)))
		return -EFAULT;

	if(get_user(swlen, kxfer.swlen) < 0)
		return -EFAULT;

	if(get_user(maxfrag, kxfer.maxfrag) < 0)
		return -EFAULT;

	if(get_user(curfrag, kxfer.curfrag) < 0)
		return -EFAULT;

	if(curfrag==maxfrag) fragsize = swlen-(maxfrag-1)*8192;

	if(!kxfer.buf || !access_ok(VERIFY_READ, kxfer.buf, fragsize))
		return -EFAULT;
	
	c = i2o_find_controller(kxfer.iop);
	if(!c)
		return -ENXIO;

	buffer=kmalloc(fragsize, GFP_KERNEL);
	if (buffer==NULL)
	{
		i2o_unlock_controller(c);
		return -ENOMEM;
	}
	__copy_from_user(buffer, kxfer.buf, fragsize);

	msg[0]= NINE_WORD_MSG_SIZE | SGL_OFFSET_7;
	msg[1]= I2O_CMD_SW_DOWNLOAD<<24 | HOST_TID<<12 | ADAPTER_TID;
	msg[2]= (u32)cfg_handler.context;
	msg[3]= 0;
	msg[4]= (((u32)kxfer.flags)<<24) | (((u32)kxfer.sw_type)<<16) |
		(((u32)maxfrag)<<8) | (((u32)curfrag));
	msg[5]= swlen;
	msg[6]= kxfer.sw_id;
	msg[7]= (0xD0000000 | fragsize);
	msg[8]= virt_to_bus(buffer);

//	printk("i2o_config: swdl frag %d/%d (size %d)\n", curfrag, maxfrag, fragsize);
	status = i2o_post_wait_mem(c, msg, sizeof(msg), 60, buffer, NULL);

	i2o_unlock_controller(c);
	if(status != -ETIMEDOUT)
		kfree(buffer);
	
	if (status != I2O_POST_WAIT_OK)
	{
		// it fails if you try and send frags out of order
		// and for some yet unknown reasons too
		printk(KERN_INFO "i2o_config: swdl failed, DetailedStatus = %d\n", status);
		return status;
	}

	return 0;
}

int ioctl_swul(unsigned long arg)
{
	struct i2o_sw_xfer kxfer;
	struct i2o_sw_xfer *pxfer = (struct i2o_sw_xfer *)arg;
	unsigned char maxfrag = 0, curfrag = 1;
	unsigned char *buffer;
	u32 msg[9];
	unsigned int status = 0, swlen = 0, fragsize = 8192;
	struct i2o_controller *c;
	
	if(copy_from_user(&kxfer, pxfer, sizeof(struct i2o_sw_xfer)))
		return -EFAULT;
		
	if(get_user(swlen, kxfer.swlen) < 0)
		return -EFAULT;
		
	if(get_user(maxfrag, kxfer.maxfrag) < 0)
		return -EFAULT;
		
	if(get_user(curfrag, kxfer.curfrag) < 0)
		return -EFAULT;
	
	if(curfrag==maxfrag) fragsize = swlen-(maxfrag-1)*8192;
	
	if(!kxfer.buf || !access_ok(VERIFY_WRITE, kxfer.buf, fragsize))
		return -EFAULT;
		
	c = i2o_find_controller(kxfer.iop);
	if(!c)
		return -ENXIO;
		
	buffer=kmalloc(fragsize, GFP_KERNEL);
	if (buffer==NULL)
	{
		i2o_unlock_controller(c);
		return -ENOMEM;
	}
	
	msg[0]= NINE_WORD_MSG_SIZE | SGL_OFFSET_7;
	msg[1]= I2O_CMD_SW_UPLOAD<<24 | HOST_TID<<12 | ADAPTER_TID;
	msg[2]= (u32)cfg_handler.context;
	msg[3]= 0;
	msg[4]= (u32)kxfer.flags<<24|(u32)kxfer.sw_type<<16|(u32)maxfrag<<8|(u32)curfrag;
	msg[5]= swlen;
	msg[6]= kxfer.sw_id;
	msg[7]= (0xD0000000 | fragsize);
	msg[8]= virt_to_bus(buffer);
	
//	printk("i2o_config: swul frag %d/%d (size %d)\n", curfrag, maxfrag, fragsize);
	status = i2o_post_wait_mem(c, msg, sizeof(msg), 60, buffer, NULL);
	i2o_unlock_controller(c);
	
	if (status != I2O_POST_WAIT_OK)
	{
		if(status != -ETIMEDOUT)
			kfree(buffer);
		printk(KERN_INFO "i2o_config: swul failed, DetailedStatus = %d\n", status);
		return status;
	}
	
	__copy_to_user(kxfer.buf, buffer, fragsize);
	kfree(buffer);
	
	return 0;
}

int ioctl_swdel(unsigned long arg)
{
	struct i2o_controller *c;
	struct i2o_sw_xfer kxfer, *pxfer = (struct i2o_sw_xfer *)arg;
	u32 msg[7];
	unsigned int swlen;
	int token;
	
	if (copy_from_user(&kxfer, pxfer, sizeof(struct i2o_sw_xfer)))
		return -EFAULT;
		
	if (get_user(swlen, kxfer.swlen) < 0)
		return -EFAULT;
		
	c = i2o_find_controller(kxfer.iop);
	if (!c)
		return -ENXIO;

	msg[0] = SEVEN_WORD_MSG_SIZE | SGL_OFFSET_0;
	msg[1] = I2O_CMD_SW_REMOVE<<24 | HOST_TID<<12 | ADAPTER_TID;
	msg[2] = (u32)i2o_cfg_context;
	msg[3] = 0;
	msg[4] = (u32)kxfer.flags<<24 | (u32)kxfer.sw_type<<16;
	msg[5] = swlen;
	msg[6] = kxfer.sw_id;

	token = i2o_post_wait(c, msg, sizeof(msg), 10);
	i2o_unlock_controller(c);
	
	if (token != I2O_POST_WAIT_OK)
	{
		printk(KERN_INFO "i2o_config: swdel failed, DetailedStatus = %d\n", token);
		return -ETIMEDOUT;
	}
	
	return 0;
}

int ioctl_validate(unsigned long arg)
{
        int token;
        int iop = (int)arg;
        u32 msg[4];
        struct i2o_controller *c;

        c=i2o_find_controller(iop);
        if (!c)
                return -ENXIO;

        msg[0] = FOUR_WORD_MSG_SIZE|SGL_OFFSET_0;
        msg[1] = I2O_CMD_CONFIG_VALIDATE<<24 | HOST_TID<<12 | iop;
        msg[2] = (u32)i2o_cfg_context;
        msg[3] = 0;

        token = i2o_post_wait(c, msg, sizeof(msg), 10);
        i2o_unlock_controller(c);

        if (token != I2O_POST_WAIT_OK)
        {
                printk(KERN_INFO "Can't validate configuration, ErrorStatus = %d\n",
                	token);
                return -ETIMEDOUT;
        }

        return 0;
}   

static int ioctl_evt_reg(unsigned long arg, struct file *fp)
{
	u32 msg[5];
	struct i2o_evt_id *pdesc = (struct i2o_evt_id *)arg;
	struct i2o_evt_id kdesc;
	struct i2o_controller *iop;
	struct i2o_device *d;

	if (copy_from_user(&kdesc, pdesc, sizeof(struct i2o_evt_id)))
		return -EFAULT;

	/* IOP exists? */
	iop = i2o_find_controller(kdesc.iop);
	if(!iop)
		return -ENXIO;
	i2o_unlock_controller(iop);

	/* Device exists? */
	for(d = iop->devices; d; d = d->next)
		if(d->lct_data.tid == kdesc.tid)
			break;

	if(!d)
		return -ENODEV;

	msg[0] = FOUR_WORD_MSG_SIZE|SGL_OFFSET_0;
	msg[1] = I2O_CMD_UTIL_EVT_REGISTER<<24 | HOST_TID<<12 | kdesc.tid;
	msg[2] = (u32)i2o_cfg_context;
	msg[3] = (u32)fp->private_data;
	msg[4] = kdesc.evt_mask;

	i2o_post_this(iop, msg, 20);

	return 0;
}	

static int ioctl_evt_get(unsigned long arg, struct file *fp)
{
	u32 id = (u32)fp->private_data;
	struct i2o_cfg_info *p = NULL;
	struct i2o_evt_get *uget = (struct i2o_evt_get*)arg;
	struct i2o_evt_get kget;
	unsigned long flags;

	for(p = open_files; p; p = p->next)
		if(p->q_id == id)
			break;

	if(!p->q_len)
	{
		return -ENOENT;
		return 0;
	}

	memcpy(&kget.info, &p->event_q[p->q_out], sizeof(struct i2o_evt_info));
	MODINC(p->q_out, I2O_EVT_Q_LEN);
	spin_lock_irqsave(&i2o_config_lock, flags);
	p->q_len--;
	kget.pending = p->q_len;
	kget.lost = p->q_lost;
	spin_unlock_irqrestore(&i2o_config_lock, flags);

	if(copy_to_user(uget, &kget, sizeof(struct i2o_evt_get)))
		return -EFAULT;
	return 0;
}

static int cfg_open(struct inode *inode, struct file *file)
{
	struct i2o_cfg_info *tmp = 
		(struct i2o_cfg_info *)kmalloc(sizeof(struct i2o_cfg_info), GFP_KERNEL);
	unsigned long flags;

	if(!tmp)
		return -ENOMEM;

	file->private_data = (void*)(i2o_cfg_info_id++);
	tmp->fp = file;
	tmp->fasync = NULL;
	tmp->q_id = (u32)file->private_data;
	tmp->q_len = 0;
	tmp->q_in = 0;
	tmp->q_out = 0;
	tmp->q_lost = 0;
	tmp->next = open_files;

	spin_lock_irqsave(&i2o_config_lock, flags);
	open_files = tmp;
	spin_unlock_irqrestore(&i2o_config_lock, flags);
	
	return 0;
}

static int cfg_release(struct inode *inode, struct file *file)
{
	u32 id = (u32)file->private_data;
	struct i2o_cfg_info *p1, *p2;
	unsigned long flags;

	lock_kernel();
	p1 = p2 = NULL;

	spin_lock_irqsave(&i2o_config_lock, flags);
	for(p1 = open_files; p1; )
	{
		if(p1->q_id == id)
		{

			if(p1->fasync)
				cfg_fasync(-1, file, 0);
			if(p2)
				p2->next = p1->next;
			else
				open_files = p1->next;

			kfree(p1);
			break;
		}
		p2 = p1;
		p1 = p1->next;
	}
	spin_unlock_irqrestore(&i2o_config_lock, flags);
	unlock_kernel();

	return 0;
}

static int cfg_fasync(int fd, struct file *fp, int on)
{
	u32 id = (u32)fp->private_data;
	struct i2o_cfg_info *p;

	for(p = open_files; p; p = p->next)
		if(p->q_id == id)
			break;

	if(!p)
		return -EBADF;

	return fasync_helper(fd, fp, on, &p->fasync);
}

static struct file_operations config_fops =
{
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		cfg_read,
	write:		cfg_write,
	ioctl:		cfg_ioctl,
	open:		cfg_open,
	release:	cfg_release,
	fasync:		cfg_fasync,
};

static struct miscdevice i2o_miscdev = {
	I2O_MINOR,
	"i2octl",
	&config_fops
};	

static int __init i2o_config_init(void)
{
	printk(KERN_INFO "I2O configuration manager v 0.04.\n");
	printk(KERN_INFO "  (C) Copyright 1999 Red Hat Software\n");
	
	if((page_buf = kmalloc(4096, GFP_KERNEL))==NULL)
	{
		printk(KERN_ERR "i2o_config: no memory for page buffer.\n");
		return -ENOBUFS;
	}
	if(misc_register(&i2o_miscdev) < 0)
	{
		printk(KERN_ERR "i2o_config: can't register device.\n");
		kfree(page_buf);
		return -EBUSY;
	}
	/*
	 *	Install our handler
	 */
	if(i2o_install_handler(&cfg_handler)<0)
	{
		kfree(page_buf);
		printk(KERN_ERR "i2o_config: handler register failed.\n");
		misc_deregister(&i2o_miscdev);
		return -EBUSY;
	}
	/*
	 *	The low 16bits of the transaction context must match this
	 *	for everything we post. Otherwise someone else gets our mail
	 */
	i2o_cfg_context = cfg_handler.context;
	return 0;
}

static void i2o_config_exit(void)
{
	misc_deregister(&i2o_miscdev);
	
	if(page_buf)
		kfree(page_buf);
	if(i2o_cfg_context != -1)
		i2o_remove_handler(&cfg_handler);
}
 
EXPORT_NO_SYMBOLS;
MODULE_AUTHOR("Red Hat Software");
MODULE_DESCRIPTION("I2O Configuration");
MODULE_LICENSE("GPL");

module_init(i2o_config_init);
module_exit(i2o_config_exit);
