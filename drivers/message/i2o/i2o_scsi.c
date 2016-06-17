/* 
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2, or (at your option) any
 *  later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  Complications for I2O scsi
 *
 *	o	Each (bus,lun) is a logical device in I2O. We keep a map
 *		table. We spoof failed selection for unmapped units
 *	o	Request sense buffers can come back for free. 
 *	o	Scatter gather is a bit dynamic. We have to investigate at
 *		setup time.
 *	o	Some of our resources are dynamically shared. The i2o core
 *		needs a message reservation protocol to avoid swap v net
 *		deadlocking. We need to back off queue requests.
 *	
 *	In general the firmware wants to help. Where its help isn't performance
 *	useful we just ignore the aid. Its not worth the code in truth.
 *
 *	Fixes:
 *		Steve Ralston	:	Scatter gather now works
 *
 *	To Do
 *		64bit cleanups
 *		Fix the resource management problems.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/prefetch.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/blk.h>
#include <linux/version.h>
#include <linux/i2o.h>
#include "../../scsi/scsi.h"
#include "../../scsi/hosts.h"
#include "../../scsi/sd.h"
#include "i2o_scsi.h"

#define VERSION_STRING        "Version 0.0.1"

#define dprintk(x)

#define MAXHOSTS 32

struct i2o_scsi_host
{
	struct i2o_controller *controller;
	s16 task[16][8];		/* Allow 16 devices for now */
	unsigned long tagclock[16][8];	/* Tag clock for queueing */
	s16 bus_task;		/* The adapter TID */
};

static int scsi_context;
static int lun_done;
static int i2o_scsi_hosts;

static u32 *retry[32];
static struct i2o_controller *retry_ctrl[32];
static struct timer_list retry_timer;
static int retry_ct = 0;

static atomic_t queue_depth;

/*
 *	SG Chain buffer support...
 */

#define SG_MAX_FRAGS		64

/*
 *	FIXME: we should allocate one of these per bus we find as we
 *	locate them not in a lump at boot.
 */
 
typedef struct _chain_buf
{
	u32 sg_flags_cnt[SG_MAX_FRAGS];
	u32 sg_buf[SG_MAX_FRAGS];
} chain_buf;

#define SG_CHAIN_BUF_SZ sizeof(chain_buf)

#define SG_MAX_BUFS		(i2o_num_controllers * I2O_SCSI_CAN_QUEUE)
#define SG_CHAIN_POOL_SZ	(SG_MAX_BUFS * SG_CHAIN_BUF_SZ)

static int max_sg_len = 0;
static chain_buf *sg_chain_pool = NULL;
static int sg_chain_tag = 0;
static int sg_max_frags = SG_MAX_FRAGS;

/*
 *	Retry congested frames. This actually needs pushing down into
 *	i2o core. We should only bother the OSM with this when we can't
 *	queue and retry the frame. Or perhaps we should call the OSM
 *	and its default handler should be this in the core, and this
 *	call a 2nd "I give up" handler in the OSM ?
 */
 
static void i2o_retry_run(unsigned long f)
{
	int i;
	unsigned long flags;
	
	spin_lock_irqsave(&io_request_lock, flags);
	for(i=0;i<retry_ct;i++)
		i2o_post_message(retry_ctrl[i], virt_to_bus(retry[i]));
	retry_ct=0;
	
	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void flush_pending(void)
{
	int i;
	unsigned long flags;
	
	spin_lock_irqsave(&io_request_lock, flags);

	for(i=0;i<retry_ct;i++)
	{
		retry[i][0]&=~0xFFFFFF;
		retry[i][0]|=I2O_CMD_UTIL_NOP<<24;
		i2o_post_message(retry_ctrl[i],virt_to_bus(retry[i]));
	}
	retry_ct=0;
	
	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i2o_scsi_reply(struct i2o_handler *h, struct i2o_controller *c, struct i2o_message *msg)
{
	Scsi_Cmnd *current_command;
	u32 *m = (u32 *)msg;
	u8 as,ds,st;

	spin_lock_prefetch(&io_request_lock);		

	if(m[0] & (1<<13))
	{
		printk("IOP fail.\n");
		printk("From %d To %d Cmd %d.\n",
			(m[1]>>12)&0xFFF,
			m[1]&0xFFF,
			m[1]>>24);
		printk("Failure Code %d.\n", m[4]>>24);
		if(m[4]&(1<<16))
			printk("Format error.\n");
		if(m[4]&(1<<17))
			printk("Path error.\n");
		if(m[4]&(1<<18))
			printk("Path State.\n");
		if(m[4]&(1<<18))
			printk("Congestion.\n");
		
		m=(u32 *)bus_to_virt(m[7]);
		printk("Failing message is %p.\n", m);
		
		if((m[4]&(1<<18)) && retry_ct < 32)
		{
			retry_ctrl[retry_ct]=c;
			retry[retry_ct]=m;
			if(!retry_ct++)
			{
				retry_timer.expires=jiffies+1;
				add_timer(&retry_timer);
			}
		}
		else
		{
			/* Create a scsi error for this */
			current_command = (Scsi_Cmnd *)m[3];
			printk("Aborted %ld\n", current_command->serial_number);

			spin_lock_irq(&io_request_lock);			
			current_command->result = DID_ERROR << 16;
			current_command->scsi_done(current_command);
			spin_unlock_irq(&io_request_lock);			
			
			/* Now flush the message by making it a NOP */
			m[0]&=0x00FFFFFF;
			m[0]|=(I2O_CMD_UTIL_NOP)<<24;
			i2o_post_message(c,virt_to_bus(m));
		}
		return;
	}
	
	prefetchw(&queue_depth);
		
	
	/*
	 *	Low byte is device status, next is adapter status,
	 *	(then one byte reserved), then request status.
	 */
	ds=(u8)le32_to_cpu(m[4]); 
	as=(u8)le32_to_cpu(m[4]>>8);
	st=(u8)le32_to_cpu(m[4]>>24);
	
	dprintk(("i2o got a scsi reply %08X: ", m[0]));
	dprintk(("m[2]=%08X: ", m[2]));
	dprintk(("m[4]=%08X\n", m[4]));

	if(m[2]&0x80000000)
	{
		if(m[2]&0x40000000)
		{
			dprintk(("Event.\n"));
			lun_done=1;
			return;
		}
		printk(KERN_ERR "i2o_scsi: bus reset reply.\n");
		return;
	}

	/*
 	 *	FIXME: 64bit breakage
	 */
	current_command = (Scsi_Cmnd *)m[3];
	
	/*
	 *	Is this a control request coming back - eg an abort ?
	 */
	 
	if(current_command==NULL)
	{
		if(st)
			dprintk(("SCSI abort: %08X", m[4]));
		dprintk(("SCSI abort completed.\n"));
		return;
	}
	
	dprintk(("Completed %ld\n", current_command->serial_number));
	
	atomic_dec(&queue_depth);
	
	if(st == 0x06)
	{
		if(le32_to_cpu(m[5]) < current_command->underflow)
		{
			int i;
			printk(KERN_ERR "SCSI: underflow 0x%08X 0x%08X\n",
				le32_to_cpu(m[5]), current_command->underflow);
			printk("Cmd: ");
			for(i=0;i<15;i++)
				printk("%02X ", current_command->cmnd[i]);
			printk(".\n");
		}
		else st=0;
	}
	
	if(st)
	{
		/* An error has occurred */

		dprintk((KERN_DEBUG "SCSI error %08X", m[4]));
			
		if (as == 0x0E) 
			/* SCSI Reset */
			current_command->result = DID_RESET << 16;
		else if (as == 0x0F)
			current_command->result = DID_PARITY << 16;
		else
			current_command->result = DID_ERROR << 16;
	}
	else
		/*
		 *	It worked maybe ?
		 */		
		current_command->result = DID_OK << 16 | ds;
	spin_lock(&io_request_lock);
	current_command->scsi_done(current_command);
	spin_unlock(&io_request_lock);
	return;
}

struct i2o_handler i2o_scsi_handler=
{
	i2o_scsi_reply,
	NULL,
	NULL,
	NULL,
	"I2O SCSI OSM",
	0,
	I2O_CLASS_SCSI_PERIPHERAL
};

static int i2o_find_lun(struct i2o_controller *c, struct i2o_device *d, int *target, int *lun)
{
	u8 reply[8];
	
	if(i2o_query_scalar(c, d->lct_data.tid, 0, 3, reply, 4)<0)
		return -1;
		
	*target=reply[0];
	
	if(i2o_query_scalar(c, d->lct_data.tid, 0, 4, reply, 8)<0)
		return -1;

	*lun=reply[1];

	dprintk(("SCSI (%d,%d)\n", *target, *lun));
	return 0;
}

static void i2o_scsi_init(struct i2o_controller *c, struct i2o_device *d, struct Scsi_Host *shpnt)
{
	struct i2o_device *unit;
	struct i2o_scsi_host *h =(struct i2o_scsi_host *)shpnt->hostdata;
	int lun;
	int target;
	
	h->controller=c;
	h->bus_task=d->lct_data.tid;
	
	for(target=0;target<16;target++)
		for(lun=0;lun<8;lun++)
			h->task[target][lun] = -1;
			
	for(unit=c->devices;unit!=NULL;unit=unit->next)
	{
		dprintk(("Class %03X, parent %d, want %d.\n",
			unit->lct_data.class_id, unit->lct_data.parent_tid, d->lct_data.tid));
			
		/* Only look at scsi and fc devices */
		if (    (unit->lct_data.class_id != I2O_CLASS_SCSI_PERIPHERAL)
		     && (unit->lct_data.class_id != I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL)
		   )
			continue;

		/* On our bus ? */
		dprintk(("Found a disk (%d).\n", unit->lct_data.tid));
		if ((unit->lct_data.parent_tid == d->lct_data.tid)
		     || (unit->lct_data.parent_tid == d->lct_data.parent_tid)
		   )
		{
			u16 limit;
			dprintk(("Its ours.\n"));
			if(i2o_find_lun(c, unit, &target, &lun)==-1)
			{
				printk(KERN_ERR "i2o_scsi: Unable to get lun for tid %d.\n", unit->lct_data.tid);
				continue;
			}
			dprintk(("Found disk %d %d.\n", target, lun));
			h->task[target][lun]=unit->lct_data.tid;
			h->tagclock[target][lun]=jiffies;

			/* Get the max fragments/request */
			i2o_query_scalar(c, d->lct_data.tid, 0xF103, 3, &limit, 2);
			
			/* sanity */
			if ( limit == 0 )
			{
				printk(KERN_WARNING "i2o_scsi: Ignoring unreasonable SG limit of 0 from IOP!\n");
				limit = 1;
			}
			
			shpnt->sg_tablesize = limit;

			dprintk(("i2o_scsi: set scatter-gather to %d.\n", 
				shpnt->sg_tablesize));
		}
	}		
}

static int i2o_scsi_detect(Scsi_Host_Template * tpnt)
{
	unsigned long flags;
	struct Scsi_Host *shpnt = NULL;
	int i;
	int count;

	printk("i2o_scsi.c: %s\n", VERSION_STRING);

	if(i2o_install_handler(&i2o_scsi_handler)<0)
	{
		printk(KERN_ERR "i2o_scsi: Unable to install OSM handler.\n");
		return 0;
	}
	scsi_context = i2o_scsi_handler.context;
	
	if((sg_chain_pool = kmalloc(SG_CHAIN_POOL_SZ, GFP_KERNEL)) == NULL)
	{
		printk("i2o_scsi: Unable to alloc %d byte SG chain buffer pool.\n", SG_CHAIN_POOL_SZ);
		printk("i2o_scsi: SG chaining DISABLED!\n");
		sg_max_frags = 11;
	}
	else
	{
		printk("  chain_pool: %d bytes @ %p\n", SG_CHAIN_POOL_SZ, sg_chain_pool);
		printk("  (%d byte buffers X %d can_queue X %d i2o controllers)\n",
				SG_CHAIN_BUF_SZ, I2O_SCSI_CAN_QUEUE, i2o_num_controllers);
		sg_max_frags = SG_MAX_FRAGS;    // 64
	}
	
	init_timer(&retry_timer);
	retry_timer.data = 0UL;
	retry_timer.function = i2o_retry_run;
	
//	printk("SCSI OSM at %d.\n", scsi_context);

	for (count = 0, i = 0; i < MAX_I2O_CONTROLLERS; i++)
	{
		struct i2o_controller *c=i2o_find_controller(i);
		struct i2o_device *d;
		/*
		 *	This controller doesn't exist.
		 */
		
		if(c==NULL)
			continue;
			
		/*
		 *	Fixme - we need some altered device locking. This
		 *	is racing with device addition in theory. Easy to fix.
		 */
		
		for(d=c->devices;d!=NULL;d=d->next)
		{
			/*
			 *	bus_adapter, SCSI (obsolete), or FibreChannel busses only
			 */
			if(    (d->lct_data.class_id!=I2O_CLASS_BUS_ADAPTER_PORT)	// bus_adapter
//			    && (d->lct_data.class_id!=I2O_CLASS_FIBRE_CHANNEL_PORT)	// FC_PORT
			  )
				continue;
		
			shpnt = scsi_register(tpnt, sizeof(struct i2o_scsi_host));
			if(shpnt==NULL)
				continue;
			save_flags(flags);
			cli();
			shpnt->unique_id = (u32)d;
			shpnt->io_port = 0;
			shpnt->n_io_port = 0;
			shpnt->irq = 0;
			shpnt->this_id = /* Good question */15;
			restore_flags(flags);
			i2o_scsi_init(c, d, shpnt);
			count++;
		}
	}
	i2o_scsi_hosts = count;
	
	if(count==0)
	{
		if(sg_chain_pool!=NULL)
		{
			kfree(sg_chain_pool);
			sg_chain_pool = NULL;
		}
		flush_pending();
		del_timer(&retry_timer);
		i2o_remove_handler(&i2o_scsi_handler);
	}
	
	return count;
}

static int i2o_scsi_release(struct Scsi_Host *host)
{
	if(--i2o_scsi_hosts==0)
	{
		if(sg_chain_pool!=NULL)
		{
			kfree(sg_chain_pool);
			sg_chain_pool = NULL;
		}
		flush_pending();
		del_timer(&retry_timer);
		i2o_remove_handler(&i2o_scsi_handler);
	}
	return 0;
}


static const char *i2o_scsi_info(struct Scsi_Host *SChost)
{
	struct i2o_scsi_host *hostdata;
	hostdata = (struct i2o_scsi_host *)SChost->hostdata;
	return(&hostdata->controller->name[0]);
}

static int i2o_scsi_queuecommand(Scsi_Cmnd * SCpnt, void (*done) (Scsi_Cmnd *))
{
	int i;
	int tid;
	struct i2o_controller *c;
	Scsi_Cmnd *current_command;
	struct Scsi_Host *host;
	struct i2o_scsi_host *hostdata;
	u32 *msg, *mptr;
	u32 m;
	u32 *lenptr;
	int direction;
	int scsidir;
	u32 len;
	u32 reqlen;
	u32 tag;
	
	static int max_qd = 1;
	
	/*
	 *	Do the incoming paperwork
	 */
	 
	host = SCpnt->host;
	hostdata = (struct i2o_scsi_host *)host->hostdata;
	 
	c = hostdata->controller;
	prefetch(c);
	prefetchw(&queue_depth);

	SCpnt->scsi_done = done;
	
	if(SCpnt->target > 15)
	{
		printk(KERN_ERR "i2o_scsi: Wild target %d.\n", SCpnt->target);
		return -1;
	}
	
	tid = hostdata->task[SCpnt->target][SCpnt->lun];
	
	dprintk(("qcmd: Tid = %d\n", tid));
	
	current_command = SCpnt;		/* set current command                */
	current_command->scsi_done = done;	/* set ptr to done function           */

	/* We don't have such a device. Pretend we did the command 
	   and that selection timed out */
	
	if(tid == -1)
	{
		SCpnt->result = DID_NO_CONNECT << 16;
		done(SCpnt);
		return 0;
	}
	
	dprintk(("Real scsi messages.\n"));

	
	/*
	 *	Obtain an I2O message. Right now we _have_ to obtain one
	 *	until the scsi layer stuff is cleaned up.
	 */	
	 
	do
	{
		mb();
		m = le32_to_cpu(I2O_POST_READ32(c));
	}
	while(m==0xFFFFFFFF);

	msg = (u32 *)(c->mem_offset + m);
	
	/*
	 *	Put together a scsi execscb message
	 */
	
	len = SCpnt->request_bufflen;
	direction = 0x00000000;			// SGL IN  (osm<--iop)
	
	if(SCpnt->sc_data_direction == SCSI_DATA_NONE)
		scsidir = 0x00000000;			// DATA NO XFER
	else if(SCpnt->sc_data_direction == SCSI_DATA_WRITE)
	{
		direction=0x04000000;	// SGL OUT  (osm-->iop)
		scsidir  =0x80000000;	// DATA OUT (iop-->dev)
	}
	else if(SCpnt->sc_data_direction == SCSI_DATA_READ)
	{
		scsidir  =0x40000000;	// DATA IN  (iop<--dev)
	}
	else
	{
		/* Unknown - kill the command */
		SCpnt->result = DID_NO_CONNECT << 16;
		done(SCpnt);
		return 0;
	}
	
	i2o_raw_writel(I2O_CMD_SCSI_EXEC<<24|HOST_TID<<12|tid, &msg[1]);
	i2o_raw_writel(scsi_context, &msg[2]);	/* So the I2O layer passes to us */
	/* Sorry 64bit folks. FIXME */
	i2o_raw_writel((u32)SCpnt, &msg[3]);	/* We want the SCSI control block back */

	/* LSI_920_PCI_QUIRK
	 *
	 *	Intermittant observations of msg frame word data corruption
	 *	observed on msg[4] after:
	 *	  WRITE, READ-MODIFY-WRITE
	 *	operations.  19990606 -sralston
	 *
	 *	(Hence we build this word via tag. Its good practice anyway
	 *	 we don't want fetches over PCI needlessly)
	 */

	tag=0;
	
	/*
	 *	Attach tags to the devices
	 */	
	if(SCpnt->device->tagged_supported)
	{
		/*
		 *	Some drives are too stupid to handle fairness issues
		 *	with tagged queueing. We throw in the odd ordered
		 *	tag to stop them starving themselves.
		 */
		if((jiffies - hostdata->tagclock[SCpnt->target][SCpnt->lun]) > (5*HZ))
		{
			tag=0x01800000;		/* ORDERED! */
			hostdata->tagclock[SCpnt->target][SCpnt->lun]=jiffies;
		}
		else
		{
			/* Hmmm...  I always see value of 0 here,
			 *  of which {HEAD_OF, ORDERED, SIMPLE} are NOT!  -sralston
			 */
			if(SCpnt->tag == HEAD_OF_QUEUE_TAG)
				tag=0x01000000;
			else if(SCpnt->tag == ORDERED_QUEUE_TAG)
				tag=0x01800000;
		}
	}

	/* Direction, disconnect ok, tag, CDBLen */
	i2o_raw_writel(scsidir|0x20000000|SCpnt->cmd_len|tag, &msg[4]);

	mptr=msg+5;

	/* 
	 *	Write SCSI command into the message - always 16 byte block 
	 */
	 
	memcpy_toio(mptr, SCpnt->cmnd, 16);
	mptr+=4;
	lenptr=mptr++;		/* Remember me - fill in when we know */
	
	reqlen = 12;		// SINGLE SGE
	
	/*
	 *	Now fill in the SGList and command 
	 *
	 *	FIXME: we need to set the sglist limits according to the 
	 *	message size of the I2O controller. We might only have room
	 *	for 6 or so worst case
	 *
	 *	FIXME: pci dma mapping
	 */
	
	if(SCpnt->use_sg)
	{
		struct scatterlist *sg = (struct scatterlist *)SCpnt->request_buffer;
		int chain = 0;
		
		len = 0;

		if((sg_max_frags > 11) && (SCpnt->use_sg > 11))
		{
			chain = 1;
			/*
			 *	Need to chain!
			 */
			i2o_raw_writel(direction|0xB0000000|(SCpnt->use_sg*2*4), mptr++);
			i2o_raw_writel(virt_to_bus(sg_chain_pool + sg_chain_tag), mptr);
			mptr = (u32*)(sg_chain_pool + sg_chain_tag);
			if (SCpnt->use_sg > max_sg_len)
			{
				max_sg_len = SCpnt->use_sg;
				printk("i2o_scsi: Chain SG! SCpnt=%p, SG_FragCnt=%d, SG_idx=%d\n",
					SCpnt, SCpnt->use_sg, sg_chain_tag);
			}
			if ( ++sg_chain_tag == SG_MAX_BUFS )
				sg_chain_tag = 0;
			for(i = 0 ; i < SCpnt->use_sg; i++)
			{
				*mptr++=direction|0x10000000|sg->length;
				len+=sg->length;
				*mptr++=virt_to_bus(sg->address);
				sg++;
			}
			mptr[-2]=direction|0xD0000000|(sg-1)->length;
		}
		else
		{		
			for(i = 0 ; i < SCpnt->use_sg; i++)
			{
				i2o_raw_writel(direction|0x10000000|sg->length, mptr++);
				len+=sg->length;
				i2o_raw_writel(virt_to_bus(sg->address), mptr++);
				sg++;
			}

			/* Make this an end of list. Again evade the 920 bug and
			   unwanted PCI read traffic */
		
			i2o_raw_writel(direction|0xD0000000|(sg-1)->length, &mptr[-2]);
		}
		
		if(!chain)
			reqlen = mptr - msg;
		
		i2o_raw_writel(len, lenptr);
		
		if(len != SCpnt->underflow)
			printk("Cmd len %08X Cmd underflow %08X\n",
				len, SCpnt->underflow);
	}
	else
	{
		dprintk(("non sg for %p, %d\n", SCpnt->request_buffer,
				SCpnt->request_bufflen));
		i2o_raw_writel(len = SCpnt->request_bufflen, lenptr);
		if(len == 0)
		{
			reqlen = 9;
		}
		else
		{
			i2o_raw_writel(0xD0000000|direction|SCpnt->request_bufflen, mptr++);
			i2o_raw_writel(virt_to_bus(SCpnt->request_buffer), mptr++);
		}
	}
	
	/*
	 *	Stick the headers on 
	 */

	i2o_raw_writel(reqlen<<16 | SGL_OFFSET_10, msg);
	
	/* Queue the message */
	i2o_post_message(c,m);
	
	atomic_inc(&queue_depth);
	
	if(atomic_read(&queue_depth)> max_qd)
	{
		max_qd=atomic_read(&queue_depth);
		printk("Queue depth now %d.\n", max_qd);
	}
	
	mb();
	dprintk(("Issued %ld\n", current_command->serial_number));
	
	return 0;
}

static void internal_done(Scsi_Cmnd * SCpnt)
{
	SCpnt->SCp.Status++;
}

static int i2o_scsi_command(Scsi_Cmnd * SCpnt)
{
	i2o_scsi_queuecommand(SCpnt, internal_done);
	SCpnt->SCp.Status = 0;
	while (!SCpnt->SCp.Status)
		barrier();
	return SCpnt->result;
}

static int i2o_scsi_abort(Scsi_Cmnd * SCpnt)
{
	struct i2o_controller *c;
	struct Scsi_Host *host;
	struct i2o_scsi_host *hostdata;
	unsigned long msg;
	u32 m;
	int tid;
	
	printk("i2o_scsi: Aborting command block.\n");
	
	host = SCpnt->host;
	hostdata = (struct i2o_scsi_host *)host->hostdata;
	tid = hostdata->task[SCpnt->target][SCpnt->lun];
	if(tid==-1)
	{
		printk(KERN_ERR "impossible command to abort.\n");
		return SCSI_ABORT_NOT_RUNNING;
	}
	c = hostdata->controller;
	
	/*
	 *	Obtain an I2O message. Right now we _have_ to obtain one
	 *	until the scsi layer stuff is cleaned up.
	 */	
	 
	do
	{
		mb();
		m = le32_to_cpu(I2O_POST_READ32(c));
	}
	while(m==0xFFFFFFFF);
	msg = c->mem_offset + m;
	
	i2o_raw_writel(FIVE_WORD_MSG_SIZE, msg);
	i2o_raw_writel(I2O_CMD_SCSI_ABORT<<24|HOST_TID<<12|tid, msg+4);
	i2o_raw_writel(scsi_context, msg+8);
	i2o_raw_writel(0, msg+12);	/* Not needed for an abort */
	i2o_raw_writel((u32)SCpnt, msg+16);	/* FIXME 32bitism */
	wmb();
	i2o_post_message(c,m);
	wmb();
	return SCSI_ABORT_PENDING;
}

static int i2o_scsi_reset(Scsi_Cmnd * SCpnt, unsigned int reset_flags)
{
	int tid;
	struct i2o_controller *c;
	struct Scsi_Host *host;
	struct i2o_scsi_host *hostdata;
	u32 m;
	unsigned long msg;

	/*
	 *	Find the TID for the bus
	 */

	printk("i2o_scsi: Attempting to reset the bus.\n");
	
	host = SCpnt->host;
	hostdata = (struct i2o_scsi_host *)host->hostdata;
	tid = hostdata->bus_task;
	c = hostdata->controller;

	/*
	 *	Now send a SCSI reset request. Any remaining commands
	 *	will be aborted by the IOP. We need to catch the reply
	 *	possibly ?
	 */
	 
	m = le32_to_cpu(I2O_POST_READ32(c));
	
	/*
	 *	No free messages, try again next time - no big deal
	 */
	 
	if(m == 0xFFFFFFFF)
		return SCSI_RESET_PUNT;
	
	msg = c->mem_offset + m;
	i2o_raw_writel(FOUR_WORD_MSG_SIZE|SGL_OFFSET_0, msg);
	i2o_raw_writel(I2O_CMD_SCSI_BUSRESET<<24|HOST_TID<<12|tid, msg+4);
	i2o_raw_writel(scsi_context|0x80000000, msg+8);
	/* We use the top bit to split controller and unit transactions */
	/* Now store unit,tid so we can tie the completion back to a specific device */
	i2o_raw_writel(c->unit << 16 | tid, msg+12);
	wmb();
	i2o_post_message(c,m);
	return SCSI_RESET_PENDING;
}

/*
 *	This is anyones guess quite frankly.
 */
 
static int i2o_scsi_bios_param(Disk * disk, kdev_t dev, int *ip)
{
	int size;

	size = disk->capacity;
	ip[0] = 64;		/* heads                        */
	ip[1] = 32;		/* sectors                      */
	if ((ip[2] = size >> 11) > 1024) {	/* cylinders, test for big disk */
		ip[0] = 255;	/* heads                        */
		ip[1] = 63;	/* sectors                      */
		ip[2] = size / (255 * 63);	/* cylinders                    */
	}
	return 0;
}

MODULE_AUTHOR("Red Hat Software");
MODULE_LICENSE("GPL");


static Scsi_Host_Template driver_template = I2OSCSI;

#include "../../scsi/scsi_module.c"
