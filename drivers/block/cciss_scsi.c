/*
 *    Disk Array driver for HP SA 5xxx and 6xxx Controllers, SCSI Tape module
 *    Copyright 2001, 2002 Hewlett-Packard Development Company, L.P.
 *    
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to Cciss-discuss@lists.sourceforge.net 
 *
 *    Author: Stephen M. Cameron
 */
#ifdef CONFIG_CISS_SCSI_TAPE

/* Here we have code to present the driver as a scsi driver
   as it is simultaneously presented as a block driver.  The
   reason for doing this is to allow access to SCSI tape drives
   through the array controller.  Note in particular, neither
   physical nor logical disks are presented through the scsi layer. */

#include "../scsi/scsi.h"
#include "../scsi/hosts.h"
#include <asm/atomic.h>
#include <linux/timer.h>

#include "cciss_scsi.h"

/* some prototypes... */
static int sendcmd(
	__u8	cmd,
	int	ctlr,
	void	*buff,
	size_t	size,
	unsigned int use_unit_num, /* 0: address the controller,
				      1: address logical volume log_unit,
				      2: address is in scsi3addr */
	unsigned int log_unit,
	__u8	page_code,
	unsigned char *scsi3addr );


int __init cciss_scsi_detect(Scsi_Host_Template *tpnt);
int cciss_scsi_release(struct Scsi_Host *sh);
const char *cciss_scsi_info(struct Scsi_Host *sa);

int cciss_scsi_proc_info(
		char *buffer, /* data buffer */
		char **start, 	   /* where data in buffer starts */
		off_t offset,	   /* offset from start of imaginary file */
		int length, 	   /* length of data in buffer */
		int hostnum, 	   /* which host adapter (always zero for me) */
		int func);	   /* 0 == read, 1 == write */

int cciss_scsi_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *));
#if 0
int cciss_scsi_abort(Scsi_Cmnd *cmd);
#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
int cciss_scsi_reset(Scsi_Cmnd *cmd, unsigned int reset_flags);
#else
int cciss_scsi_reset(Scsi_Cmnd *cmd);
#endif
#endif

static struct cciss_scsi_hba_t ccissscsi[MAX_CTLR];

/* We need one Scsi_Host_Template *per controller* instead of
   the usual one Scsi_Host_Template per controller *type*. This
   is so PCI hot plug could have a remote possibility of still
   working even with the SCSI system.  It's so
   scsi_unregister_module will differentiate the controllers.
   When register_scsi_module is called, each host template is
   customized (name change) in cciss_register_scsi()
   (that's called from cciss.c:cciss_init_one()) */

static
Scsi_Host_Template driver_template[MAX_CTLR];

#pragma pack(1)
struct cciss_scsi_cmd_stack_elem_t {
	CommandList_struct cmd;
	ErrorInfo_struct Err;
	__u32 busaddr; // 32 bits always, must fit through cmd register.
};

#pragma pack()

#define CMD_STACK_SIZE (SCSI_CCISS_CAN_QUEUE * \
		CCISS_MAX_SCSI_DEVS_PER_HBA + 2)
			// plus two for init time usage

#pragma pack(1)
struct cciss_scsi_cmd_stack_t {
	struct cciss_scsi_cmd_stack_elem_t *pool;
	struct cciss_scsi_cmd_stack_elem_t *elem[CMD_STACK_SIZE];
	dma_addr_t cmd_pool_handle;
	int top;
};
#pragma pack()

struct cciss_scsi_adapter_data_t {
	struct Scsi_Host *scsi_host;
	struct cciss_scsi_cmd_stack_t cmd_stack;
	int registered;
	spinlock_t lock; // to protect ccissscsi[ctlr];
};
#if 1
#define CPQ_TAPE_LOCK(ctlr, flags) spin_lock_irqsave( \
	&(((struct cciss_scsi_adapter_data_t *) \
	hba[ctlr]->scsi_ctlr)->lock), flags);
#define CPQ_TAPE_UNLOCK(ctlr, flags) spin_unlock_irqrestore( \
	&(((struct cciss_scsi_adapter_data_t *) \
	hba[ctlr]->scsi_ctlr)->lock), flags);
#else
#define CPQ_TAPE_LOCK(x,y)
#define CPQ_TAPE_UNLOCK(x,y)
#endif

static CommandList_struct *
scsi_cmd_alloc(ctlr_info_t *h)
{
	/* assume only one process in here at a time, locking done by caller. */

	/* take the top memory chunk off the stack and return it, if any. */
	struct cciss_scsi_cmd_stack_elem_t *c;
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	u64bit temp64;

	sa = (struct cciss_scsi_adapter_data_t *) h->scsi_ctlr;
	stk = &sa->cmd_stack;

	if (stk->top < 0)
		return NULL;
	c = stk->elem[stk->top];
	memset(&c->cmd, 0, sizeof(c->cmd));
	memset(&c->Err, 0, sizeof(c->Err));
	/* set physical addr of cmd and addr of scsi parameters */
	c->cmd.busaddr = c->busaddr;

	temp64.val = (__u64) (c->busaddr + sizeof(CommandList_struct));
	stk->top--;
	c->cmd.ErrDesc.Addr.lower = temp64.val32.lower;
	c->cmd.ErrDesc.Addr.upper = temp64.val32.upper;
	c->cmd.ErrDesc.Len = sizeof(ErrorInfo_struct);

	c->cmd.ctlr = h->ctlr;
	c->cmd.err_info = &c->Err;

	return (CommandList_struct *) c;
}

static void
scsi_cmd_free(ctlr_info_t *h, CommandList_struct *cmd)
{
	/* assume only one process in here at a time, locking done by caller. */
	/* drop the free memory chunk on top of the stack. */

	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;

	sa = (struct cciss_scsi_adapter_data_t *) h->scsi_ctlr;
	stk = &sa->cmd_stack;
	if (stk->top >= CMD_STACK_SIZE) {
		printk("cciss: scsi_cmd_free called too many times.\n");
		BUG();
	}
	stk->top++;
	stk->elem[stk->top] = (struct cciss_scsi_cmd_stack_elem_t *) cmd;
}

static int
scsi_cmd_stack_setup(int ctlr)
{
	int i;
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	size_t size;

	sa = (struct cciss_scsi_adapter_data_t *) hba[ctlr]->scsi_ctlr;
	stk = &sa->cmd_stack;
	size = sizeof(struct cciss_scsi_cmd_stack_elem_t) * CMD_STACK_SIZE;

	stk->pool = (struct cciss_scsi_cmd_stack_elem_t *)
		pci_alloc_consistent(hba[ctlr]->pdev, size, &stk->cmd_pool_handle);

	if (stk->pool == NULL) {
		printk("stk->pool is null\n");
		return -1;
	}

	for (i=0; i<CMD_STACK_SIZE; i++) {
		stk->elem[i] = &stk->pool[i];
		stk->elem[i]->busaddr = (__u32) (stk->cmd_pool_handle +
			(sizeof(struct cciss_scsi_cmd_stack_elem_t) * i));
	}
	stk->top = CMD_STACK_SIZE-1;
	return 0;
}

static void
scsi_cmd_stack_free(int ctlr)
{
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	size_t size;

	sa = (struct cciss_scsi_adapter_data_t *) hba[ctlr]->scsi_ctlr;
	stk = &sa->cmd_stack;
	if (stk->top != CMD_STACK_SIZE-1) {
		printk( "cciss: %d scsi commands are still outstanding.\n",
			CMD_STACK_SIZE - stk->top);
		// BUG();
		printk("WE HAVE A BUG HERE!!! stk=0x%08x\n",
			(unsigned int) stk);
	}
	size = sizeof(struct cciss_scsi_cmd_stack_elem_t) * CMD_STACK_SIZE;

	pci_free_consistent(hba[ctlr]->pdev, size, stk->pool, stk->cmd_pool_handle);
	stk->pool = NULL;
}

/* scsi_device_types comes from scsi.h */
#define DEVICETYPE(n) (n<0 || n>MAX_SCSI_DEVICE_CODE) ? \
	"Unknown" : scsi_device_types[n]

#if 0
static int xmargin=8;
static int amargin=60;

static void
print_bytes (unsigned char *c, int len, int hex, int ascii)
{

	int i;
	unsigned char *x;

	if (hex)
	{
		x = c;
		for (i=0;i<len;i++)
		{
			if ((i % xmargin) == 0 && i>0) printk("\n");
			if ((i % xmargin) == 0) printk("0x%04x:", i);
			printk(" %02x", *x);
			x++;
		}
		printk("\n");
	}
	if (ascii)
	{
		x = c;
		for (i=0;i<len;i++)
		{
			if ((i % amargin) == 0 && i>0) printk("\n");
			if ((i % amargin) == 0) printk("0x%04x:", i);
			if (*x > 26 && *x < 128) printk("%c", *x);
			else printk(".");
			x++;
		}
		printk("\n");
	}
}

static void
print_cmd(CommandList_struct *cp)
{
	printk("queue:%d\n", cp->Header.ReplyQueue);
	printk("sglist:%d\n", cp->Header.SGList);
	printk("sgtot:%d\n", cp->Header.SGTotal);
	printk("Tag:0x%08x/0x%08x\n", cp->Header.Tag.upper,
			cp->Header.Tag.lower);
	printk("LUN:0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		cp->Header.LUN.LunAddrBytes[0],
		cp->Header.LUN.LunAddrBytes[1],
		cp->Header.LUN.LunAddrBytes[2],
		cp->Header.LUN.LunAddrBytes[3],
		cp->Header.LUN.LunAddrBytes[4],
		cp->Header.LUN.LunAddrBytes[5],
		cp->Header.LUN.LunAddrBytes[6],
		cp->Header.LUN.LunAddrBytes[7]);
	printk("CDBLen:%d\n", cp->Request.CDBLen);
	printk("Type:%d\n",cp->Request.Type.Type);
	printk("Attr:%d\n",cp->Request.Type.Attribute);
	printk(" Dir:%d\n",cp->Request.Type.Direction);
	printk("Timeout:%d\n",cp->Request.Timeout);
	printk( "CDB: %02x %02x %02x %02x %02x %02x %02x %02x"
		" %02x %02x %02x %02x %02x %02x %02x %02x\n",
		cp->Request.CDB[0], cp->Request.CDB[1],
		cp->Request.CDB[2], cp->Request.CDB[3],
		cp->Request.CDB[4], cp->Request.CDB[5],
		cp->Request.CDB[6], cp->Request.CDB[7],
		cp->Request.CDB[8], cp->Request.CDB[9],
		cp->Request.CDB[10], cp->Request.CDB[11],
		cp->Request.CDB[12], cp->Request.CDB[13],
		cp->Request.CDB[14], cp->Request.CDB[15]),
	printk("edesc.Addr: 0x%08x/0%08x, Len  = %d\n",
		cp->ErrDesc.Addr.upper, cp->ErrDesc.Addr.lower,
			cp->ErrDesc.Len);
	printk("sgs..........Errorinfo:\n");
	printk("scsistatus:%d\n", cp->err_info->ScsiStatus);
	printk("senselen:%d\n", cp->err_info->SenseLen);
	printk("cmd status:%d\n", cp->err_info->CommandStatus);
	printk("resid cnt:%d\n", cp->err_info->ResidualCnt);
	printk("offense size:%d\n", cp->err_info->MoreErrInfo.Invalid_Cmd.offense_size);
	printk("offense byte:%d\n", cp->err_info->MoreErrInfo.Invalid_Cmd.offense_num);
	printk("offense value:%d\n", cp->err_info->MoreErrInfo.Invalid_Cmd.offense_value);

}

#endif

static int
find_bus_target_lun(int ctlr, int *bus, int *target, int *lun)
{
	/* finds an unused bus, target, lun for a new device */
	/* assumes hba[ctlr]->scsi_ctlr->lock is held */
	int i, found=0;
	unsigned char target_taken[CCISS_MAX_SCSI_DEVS_PER_HBA];

	memset(&target_taken[0], 0, CCISS_MAX_SCSI_DEVS_PER_HBA);

#	if SELF_SCSI_ID >= 0
		target_taken[SELF_SCSI_ID] = 1;
#	endif
	for (i=0;i<ccissscsi[ctlr].ndevices;i++)
		target_taken[ccissscsi[ctlr].dev[i].target] = 1;

	for (i=0;i<CCISS_MAX_SCSI_DEVS_PER_HBA;i++) {
		if (!target_taken[i]) {
			*bus = 0; *target=i; *lun = 0; found=1;
			break;
		}
	}
	return (!found);
}

static int
cciss_scsi_add_entry(int ctlr, int hostno,
		unsigned char *scsi3addr, int devtype)
{
	/* assumes hba[ctlr]->scsi_ctlr->lock is held */
	int n = ccissscsi[ctlr].ndevices;
	struct cciss_scsi_dev_t *sd;

	if (n >= CCISS_MAX_SCSI_DEVS_PER_HBA) {
		printk("cciss%d: Too many devices, "
			"some will be inaccessible.\n", ctlr);
		return -1;
	}
	sd = &ccissscsi[ctlr].dev[n];
	if (find_bus_target_lun(ctlr, &sd->bus, &sd->target, &sd->lun) != 0)
		return -1;
	memcpy(&sd->scsi3addr[0], scsi3addr, 8);
	sd->devtype = devtype;
	ccissscsi[ctlr].ndevices++;

	/* initially, (before registering with scsi layer) we don't
	   know our hostno and we don't want to print anything first
	   time anyway (the scsi layer's inquiries will show that info) */
	if (hostno != -1)
		printk("cciss%d: %s device c%db%dt%dl%d added.\n",
			ctlr, DEVICETYPE(sd->devtype), hostno,
			sd->bus, sd->target, sd->lun);
	return 0;
}

static void
cciss_scsi_remove_entry(int ctlr, int hostno, int entry)
{
	/* assumes hba[ctlr]->scsi_ctlr->lock is held */
	int i;
	struct cciss_scsi_dev_t sd;

	if (entry < 0 || entry >= CCISS_MAX_SCSI_DEVS_PER_HBA) return;
	sd = ccissscsi[ctlr].dev[entry];
	for (i=entry;i<ccissscsi[ctlr].ndevices-1;i++)
		ccissscsi[ctlr].dev[i] = ccissscsi[ctlr].dev[i+1];
	ccissscsi[ctlr].ndevices--;
	printk("cciss%d: %s device c%db%dt%dl%d removed.\n",
		ctlr, DEVICETYPE(sd.devtype), hostno,
			sd.bus, sd.target, sd.lun);
}


#define SCSI3ADDR_EQ(a,b) ( \
	(a)[7] == (b)[7] && \
	(a)[6] == (b)[6] && \
	(a)[5] == (b)[5] && \
	(a)[4] == (b)[4] && \
	(a)[3] == (b)[3] && \
	(a)[2] == (b)[2] && \
	(a)[1] == (b)[1] && \
	(a)[0] == (b)[0])

static int
adjust_cciss_scsi_table(int ctlr, int hostno,
	struct cciss_scsi_dev_t sd[], int nsds)
{
	/* sd contains scsi3 addresses and devtypes, but
	   bus target and lun are not filled in.  This funciton
	   takes what's in sd to be the current and adjusts
	   ccissscsi[] to be in line with what's in sd. */

	int i,j, found, changes=0;
	struct cciss_scsi_dev_t *csd;
	unsigned long flags;

	CPQ_TAPE_LOCK(ctlr, flags);

	/* find any devices in ccissscsi[] that are not in
	   sd[] and remove them from ccissscsi[] */

	i = 0;
	while(i<ccissscsi[ctlr].ndevices) {
		csd = &ccissscsi[ctlr].dev[i];
		found=0;
		for (j=0;j<nsds;j++) {
			if (SCSI3ADDR_EQ(sd[j].scsi3addr,
				csd->scsi3addr)) {
				if (sd[j].devtype == csd->devtype)
					found=2;
				else
					found=1;
				break;
			}
		}

		if (found == 0) { /* device no longer present. */
			changes++;
			/* printk("cciss%d: %s device c%db%dt%dl%d removed.\n",
				ctlr, DEVICETYPE(csd->devtype), hostno,
					csd->bus, csd->target, csd->lun); */
			cciss_scsi_remove_entry(ctlr, hostno, i);
			/* note, i not incremented */
		}
		else if (found == 1) { /* device is different kind */
			changes++;
			printk("cciss%d: device c%db%dt%dl%d type changed "
				"(device type now %s).\n",
				ctlr, hostno, csd->bus, csd->target, csd->lun,
					DEVICETYPE(csd->devtype));
			csd->devtype = sd[j].devtype;
			i++;	/* so just move along. */
		} else 		/* device is same as it ever was, */
			i++;	/* so just move along. */
	}

	/* Now, make sure every device listed in sd[] is also
 	   listed in ccissscsi[], adding them if they aren't found */

	for (i=0;i<nsds;i++) {
		found=0;
		for (j=0;j<ccissscsi[ctlr].ndevices;j++) {
			csd = &ccissscsi[ctlr].dev[j];
			if (SCSI3ADDR_EQ(sd[i].scsi3addr,
				csd->scsi3addr)) {
				if (sd[i].devtype == csd->devtype)
					found=2;	/* found device */
				else
					found=1; 	/* found a bug. */
				break;
			}
		}
		if (!found) {
			changes++;
			if (cciss_scsi_add_entry(ctlr, hostno,
				&sd[i].scsi3addr[0], sd[i].devtype) != 0)
				break;
		} else if (found == 1) {
			/* should never happen... */
			changes++;
			printk("cciss%d: device unexpectedly changed type\n",
				ctlr);
			/* but if it does happen, we just ignore that device */
		}
	}
	CPQ_TAPE_UNLOCK(ctlr, flags);

	if (!changes)
		printk("cciss%d: No device changes detected.\n", ctlr);

	return 0;
}

static int
lookup_scsi3addr(int ctlr, int bus, int target, int lun, char *scsi3addr)
{
	int i;
	struct cciss_scsi_dev_t *sd;
	unsigned long flags;

	CPQ_TAPE_LOCK(ctlr, flags);
	for (i=0;i<ccissscsi[ctlr].ndevices;i++) {
		sd = &ccissscsi[ctlr].dev[i];
		if (sd->bus == bus &&
		    sd->target == target &&
		    sd->lun == lun) {
			memcpy(scsi3addr, &sd->scsi3addr[0], 8);
			CPQ_TAPE_UNLOCK(ctlr, flags);
			return 0;
		}
	}
	CPQ_TAPE_UNLOCK(ctlr, flags);
	return -1;
}


static void
cciss_find_non_disk_devices(int cntl_num)
{
	ReportLunData_struct *ld_buff;
	InquiryData_struct *inq_buff;
	int return_code;
	int i;
	int listlength = 0;
	int num_luns;
	unsigned char scsi3addr[8];
	unsigned long flags;
	int reportlunsize = sizeof(*ld_buff) + CISS_MAX_PHYS_LUN * 8;

	hba[cntl_num]->scsi_ctlr = (void *)
		kmalloc(sizeof(struct cciss_scsi_adapter_data_t),
			GFP_KERNEL);
	if (hba[cntl_num]->scsi_ctlr == NULL)
		return;

	((struct cciss_scsi_adapter_data_t *)
		hba[cntl_num]->scsi_ctlr)->scsi_host = NULL;
	((struct cciss_scsi_adapter_data_t *)
		hba[cntl_num]->scsi_ctlr)->lock = SPIN_LOCK_UNLOCKED;
	((struct cciss_scsi_adapter_data_t *)
		hba[cntl_num]->scsi_ctlr)->registered = 0;

	if (scsi_cmd_stack_setup(cntl_num) != 0) {
		printk("Trouble, returned non-zero!\n");
		return;
	}

	ld_buff = kmalloc(reportlunsize, GFP_KERNEL);
	if (ld_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		return;
	}
	memset(ld_buff, 0, sizeof(ReportLunData_struct));
	inq_buff = kmalloc(sizeof( InquiryData_struct), GFP_KERNEL);
	if (inq_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		kfree(ld_buff);
		return;
	}

	/* Get the physical luns */
	return_code = sendcmd(CISS_REPORT_PHYS, cntl_num, ld_buff,
			reportlunsize, 0, 0, 0, NULL );

	if( return_code == IO_OK) {
		unsigned char *c = &ld_buff->LUNListLength[0];
		listlength = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
	}
	else {  /* getting report of physical luns failed */
		printk(KERN_WARNING "cciss: report physical luns"
			" command failed\n");
		listlength = 0;
	}

	CPQ_TAPE_LOCK(cntl_num, flags);
	ccissscsi[cntl_num].ndevices = 0;
	num_luns = listlength / 8; // 8 bytes pre entry
	/* printk("Found %d LUNs\n", num_luns); */

	if (num_luns > CISS_MAX_PHYS_LUN)
	{
		printk(KERN_WARNING
			"cciss: Maximum physical LUNs (%d) exceeded.  "
			"%d LUNs ignored.\n", CISS_MAX_PHYS_LUN,
			num_luns - CISS_MAX_PHYS_LUN);
		num_luns = CISS_MAX_PHYS_LUN;
	}

	for(i=0; i<num_luns; i++) {
		/* Execute an inquiry to figure the device type */
		/* Skip over masked devices */
		if (ld_buff->LUN[i][3] & 0xC0) continue;
		memset(inq_buff, 0, sizeof(InquiryData_struct));
		memcpy(scsi3addr, ld_buff->LUN[i], 8); /* ugly... */
		return_code = sendcmd(CISS_INQUIRY, cntl_num, inq_buff,
			sizeof(InquiryData_struct), 2, 0 ,0, scsi3addr );
	  	if (return_code == IO_OK) {
			if(inq_buff->data_byte[8] == 0xFF)
			{
			   printk(KERN_WARNING "cciss: inquiry failed\n");
			} else {
			   int devtype;

			   /* printk("Inquiry...\n");
			   print_bytes((unsigned char *) inq_buff, 36, 1, 1); */
			   devtype = (inq_buff->data_byte[0] & 0x1f);

			   switch (devtype)
			   {
			    case 0x01: /* sequential access, (tape) */
			    case 0x08: /* medium changer */
					  /* this is the only kind of dev */
					  /* we want to expose here. */
				if (cciss_scsi_add_entry(cntl_num, -1,
					(unsigned char *) ld_buff->LUN[i],
					devtype) != 0)
						i=num_luns; // leave loop
				break;
			    default:
				break;
			   }

			}
		}
		else printk("cciss: inquiry failed.\n");
	}
#if 0
	for (i=0;i<ccissscsi[cntl_num].ndevices;i++)
		printk("Tape device presented at c%db%dt%dl%d\n",
			cntl_num, // <-- this is wrong
			ccissscsi[cntl_num].dev[i].bus,
			ccissscsi[cntl_num].dev[i].target,
			ccissscsi[cntl_num].dev[i].lun);
#endif
	CPQ_TAPE_UNLOCK(cntl_num, flags);
	kfree(ld_buff);
	kfree(inq_buff);
	return;
}

static void
complete_scsi_command( CommandList_struct *cp, int timeout, __u32 tag)
{
	Scsi_Cmnd *cmd;
	ctlr_info_t *ctlr;
	u64bit addr64;
	ErrorInfo_struct *ei;

	ei = cp->err_info;

	/* First, see if it was a message rather than a command */
	if (cp->Request.Type.Type == TYPE_MSG)  {
		cp->cmd_type = CMD_MSG_DONE;
		return;
	}

	/* we stored ptr to scsi cmd in the buffer head pointer */
	cmd = (Scsi_Cmnd *) cp->scsi_cmd;
	ctlr = hba[cp->ctlr];

	/* undo the DMA mappings */

	if (cmd->use_sg) {
		pci_unmap_sg(ctlr->pdev,
			cmd->buffer, cmd->use_sg,
				scsi_to_pci_dma_dir(cmd->sc_data_direction));
	}
	else if (cmd->request_bufflen) {
		addr64.val32.lower = cp->SG[0].Addr.lower;
		addr64.val32.upper = cp->SG[0].Addr.upper;
		pci_unmap_single(ctlr->pdev, (dma_addr_t) addr64.val,
			cmd->request_bufflen,
				scsi_to_pci_dma_dir(cmd->sc_data_direction));
	}

	cmd->result = (DID_OK << 16); 		/* host byte */
	cmd->result |= (COMMAND_COMPLETE << 8);	/* msg byte */
	/* cmd->result |= (GOOD < 1); */		/* status byte */

	cmd->result |= (ei->ScsiStatus);
	/* printk("Scsistatus is 0x%02x\n", ei->ScsiStatus);  */

	/* copy the sense data whether we need to or not. */

	memcpy(cmd->sense_buffer, ei->SenseInfo,
		ei->SenseLen > SCSI_SENSE_BUFFERSIZE ?
			SCSI_SENSE_BUFFERSIZE :
			ei->SenseLen);
	cmd->resid = ei->ResidualCnt;

	if(ei->CommandStatus != 0)
	{ /* an error has occurred */
		switch(ei->CommandStatus)
		{
			case CMD_TARGET_STATUS:
				/* Pass it up to the upper layers... */
				if( ei->ScsiStatus)
					cmd->result |= (ei->ScsiStatus < 1);
				else {  /* scsi status is zero??? How??? */

	/* Ordinarily, this case should never happen, but there is a bug
	   in some released firmware revisions that allows it to happen
	   if, for example, a 4100 backplane loses power and the tape
	   drive is in it.  We assume that it's a fatal error of some
	   kind because we can't show that it wasn't. We will make it
	   look like selection timeout since that is the most common
	   reason for this to occur, and it's severe enough. */

					cmd->result = DID_NO_CONNECT << 16;
				}
			break;
			case CMD_DATA_UNDERRUN: /* let mid layer handle it. */
			break;
			case CMD_DATA_OVERRUN:
				printk(KERN_WARNING "cciss: cp %p has"
					" completed with data overrun "
					"reported\n", cp);
			break;
			case CMD_INVALID: {
				/* print_bytes(cp, sizeof(*cp), 1, 0);
				print_cmd(cp); */
     /* We get CMD_INVALID if you address a non-existent tape drive instead
	of a selection timeout (no response).  You will see this if you yank
	out a tape drive, then try to access it. This is kind of a shame
	because it means that any other CMD_INVALID (e.g. driver bug) will
	get interpreted as a missing target. */
				cmd->result = DID_NO_CONNECT << 16;
				}
			break;
			case CMD_PROTOCOL_ERR:
				printk(KERN_WARNING "cciss: cp %p has "
					"protocol error \n", cp);
			break;
			case CMD_HARDWARE_ERR:
				cmd->result = DID_ERROR << 16;
				printk(KERN_WARNING "cciss: cp %p had "
					" hardware error\n", cp);
			break;
			case CMD_CONNECTION_LOST:
				cmd->result = DID_ERROR << 16;
				printk(KERN_WARNING "cciss: cp %p had "
					"connection lost\n", cp);
			break;
			case CMD_ABORTED:
				cmd->result = DID_ABORT << 16;
				printk(KERN_WARNING "cciss: cp %p was "
					"aborted\n", cp);
			break;
			case CMD_ABORT_FAILED:
				cmd->result = DID_ERROR << 16;
				printk(KERN_WARNING "cciss: cp %p reports "
					"abort failed\n", cp);
			break;
			case CMD_UNSOLICITED_ABORT:
				cmd->result = DID_ABORT << 16;
				printk(KERN_WARNING "cciss: cp %p aborted "
					"do to an unsolicited abort\n", cp);
			break;
			case CMD_TIMEOUT:
				cmd->result = DID_TIME_OUT << 16;
				printk(KERN_WARNING "cciss: cp %p timedout\n",
					cp);
			break;
			default:
				cmd->result = DID_ERROR << 16;
				printk(KERN_WARNING "cciss: cp %p returned "
					"unknown status %x\n", cp,
						ei->CommandStatus);
		}
	}
	cmd->scsi_done(cmd);
	scsi_cmd_free(ctlr, cp);
}

/* cciss_scsi_detect is called from the scsi mid layer.
   The scsi mid layer (scsi_register_module) is
   called from cciss.c:cciss_init_one().  */

int __init
cciss_scsi_detect(Scsi_Host_Template *tpnt)
{
	int i;
	struct Scsi_Host *sh;

	/* Tell the kernel we want to be a SCSI driver... */
	sh = scsi_register(tpnt, sizeof(struct ctlr_info *));
	if (sh == NULL) return 0;

	sh->io_port = 0;	// good enough?  FIXME,
	sh->n_io_port = 0;	// I don't think we use these two...

	sh->this_id = SELF_SCSI_ID;

	i = simple_strtol((char *)&tpnt->name[5], NULL, 10);

	if (i<0 || i>=MAX_CTLR || hba[i] == NULL) {
		/* we didn't find ourself... we shouldn't get here. */
		printk("cciss_scsi_detect: could not find ourself in hba[]\n");
		return 0;
	}

	((struct cciss_scsi_adapter_data_t *)
		hba[i]->scsi_ctlr)->scsi_host = (void *) sh;
	sh->hostdata[0] = (unsigned long) hba[i];
	sh->irq = hba[i]->intr;
	sh->unique_id = sh->irq;
	scsi_set_pci_device(sh, hba[i]->pdev);

	return 1;	/* Say we have 1 scsi adapter, this will be */
			/* called multiple times, once for each adapter */
			/* from cciss.c:cciss_init_one().  We do it this */
			/* way for PCI-hot plug reasons. (we don't know how */
			/* many adapters we have total, so we say we have */
			/* 1, each of a unique type.) */
}

static void __exit cleanup_cciss_module(void);
int
cciss_scsi_release(struct Scsi_Host *sh)
{
	return 0;
}

static void
cciss_unmap_one(struct pci_dev *pdev,
		CommandList_struct *cp,
		size_t buflen,
		int data_direction)
{
	u64bit addr64;

	addr64.val32.lower = cp->SG[0].Addr.lower;
	addr64.val32.upper = cp->SG[0].Addr.upper;
	pci_unmap_single(pdev, (dma_addr_t) addr64.val, buflen, data_direction);
}

static void
cciss_map_one(struct pci_dev *pdev,
		CommandList_struct *cp,
		unsigned char *buf,
		size_t buflen,
		int data_direction)
{
	__u64 addr64;

	addr64 = (__u64) pci_map_single(pdev, buf, buflen, data_direction);
	cp->SG[0].Addr.lower =
	  (__u32) (addr64 & (__u64) 0x00000000FFFFFFFF);
	cp->SG[0].Addr.upper =
	  (__u32) ((addr64 >> 32) & (__u64) 0x00000000FFFFFFFF);
	cp->SG[0].Len = buflen;
	cp->Header.SGList = (__u8) 1;   /* no. SGs contig in this cmd */
	cp->Header.SGTotal = (__u16) 1; /* total sgs in this cmd list */
}

static int
cciss_scsi_do_simple_cmd(ctlr_info_t *c,
			CommandList_struct *cp,
			unsigned char *scsi3addr,
			unsigned char *cdb,
			unsigned char cdblen,
			unsigned char *buf, int bufsize,
			int direction)
{
	unsigned long flags;
	DECLARE_COMPLETION(wait);

	cp->cmd_type = CMD_IOCTL_PEND;		// treat this like an ioctl
	cp->scsi_cmd = NULL;
	cp->Header.ReplyQueue = 0;  // unused in simple mode
	memcpy(&cp->Header.LUN, scsi3addr, sizeof(cp->Header.LUN));
	cp->Header.Tag.lower = cp->busaddr;  // Use k. address of cmd as tag
	// Fill in the request block...

	/* printk("Using scsi3addr 0x%02x%0x2%0x2%0x2%0x2%0x2%0x2%0x2\n",
		scsi3addr[0], scsi3addr[1], scsi3addr[2], scsi3addr[3],
		scsi3addr[4], scsi3addr[5], scsi3addr[6], scsi3addr[7]); */

	memset(cp->Request.CDB, 0, sizeof(cp->Request.CDB));
	memcpy(cp->Request.CDB, cdb, cdblen);
	cp->Request.Timeout = 0;	// No timeout
	cp->Request.CDBLen = cdblen;
	cp->Request.Type.Type = TYPE_CMD;
	cp->Request.Type.Attribute = ATTR_SIMPLE;
	cp->Request.Type.Direction = direction;

	/* Fill in the SG list and do dma mapping */
	cciss_map_one(c->pdev, cp,
			(unsigned char *) buf, bufsize,
			scsi_to_pci_dma_dir(SCSI_DATA_READ));

	cp->waiting = &wait;

	/* Put the request on the tail of the request queue */
	spin_lock_irqsave(&io_request_lock, flags);
	addQ(&c->reqQ, cp);
	c->Qdepth++;
	start_io(c);
	spin_unlock_irqrestore(&io_request_lock, flags);

	wait_for_completion(&wait);

	/* undo the dma mapping */
	cciss_unmap_one(c->pdev, cp, bufsize,
				scsi_to_pci_dma_dir(SCSI_DATA_READ));

	return(0);
}

static void
cciss_scsi_interpret_error(CommandList_struct *cp)
{
	ErrorInfo_struct *ei;

	ei = cp->err_info;
	switch(ei->CommandStatus)
	{
		case CMD_TARGET_STATUS:
			printk(KERN_WARNING "cciss: cmd %p has "
				"completed with errors\n", cp);
			printk(KERN_WARNING "cciss: cmd %p "
				"has SCSI Status = %x\n",
					cp,
					ei->ScsiStatus);
			if (ei->ScsiStatus == 0)
				printk(KERN_WARNING
				"cciss:SCSI status is abnormally zero.  "
				"(probably indicates selection timeout "
				"reported incorrectly due to a known "
				"firmware bug, circa July, 2001.)\n");
		break;
		case CMD_DATA_UNDERRUN: /* let mid layer handle it. */
			printk("UNDERRUN\n");
		break;
		case CMD_DATA_OVERRUN:
			printk(KERN_WARNING "cciss: cp %p has"
				" completed with data overrun "
				"reported\n", cp);
		break;
		case CMD_INVALID: {
			/* controller unfortunately reports SCSI passthru's */
			/* to non-existent targets as invalid commands. */
			printk(KERN_WARNING "cciss: cp %p is "
				"reported invalid (probably means "						"target device no longer present)\n",
				cp);
			/* print_bytes((unsigned char *) cp, sizeof(*cp), 1, 0);
			print_cmd(cp);  */
			}
		break;
		case CMD_PROTOCOL_ERR:
			printk(KERN_WARNING "cciss: cp %p has "
				"protocol error \n", cp);
		break;
		case CMD_HARDWARE_ERR:
			/* cmd->result = DID_ERROR << 16; */
			printk(KERN_WARNING "cciss: cp %p had "
				" hardware error\n", cp);
		break;
		case CMD_CONNECTION_LOST:
			printk(KERN_WARNING "cciss: cp %p had "
				"connection lost\n", cp);
		break;
		case CMD_ABORTED:
			printk(KERN_WARNING "cciss: cp %p was "
				"aborted\n", cp);
		break;
		case CMD_ABORT_FAILED:
			printk(KERN_WARNING "cciss: cp %p reports "
				"abort failed\n", cp);
		break;
		case CMD_UNSOLICITED_ABORT:
			printk(KERN_WARNING "cciss: cp %p aborted "
				"do to an unsolicited abort\n", cp);
		break;
		case CMD_TIMEOUT:
			printk(KERN_WARNING "cciss: cp %p timedout\n",
				cp);
		break;
		default:
			printk(KERN_WARNING "cciss: cp %p returned "
				"unknown status %x\n", cp,
					ei->CommandStatus);
	}
}

static int
cciss_scsi_do_inquiry(ctlr_info_t *c, unsigned char *scsi3addr,
		 InquiryData_struct *buf)
{
	int rc;
	CommandList_struct *cp;
	char cdb[6];
	ErrorInfo_struct *ei;

	cp = scsi_cmd_alloc(c);
	ei = cp->err_info;

	if (cp == NULL) {			/* trouble... */
		printk("cmd_alloc returned NULL!\n");
		return -1;
	}

	cdb[0] = CISS_INQUIRY;
	cdb[1] = 0;
	cdb[2] = 0;
	cdb[3] = 0;
	cdb[4] = sizeof(*buf) & 0xff;
	cdb[5] = 0;
	rc = cciss_scsi_do_simple_cmd(c, cp, scsi3addr, cdb,
				6, (unsigned char *) buf,
				sizeof(*buf), XFER_READ);

	if (rc != 0) return rc; /* something went wrong */

	if (ei->CommandStatus != 0 &&
	    ei->CommandStatus != CMD_DATA_UNDERRUN) {
		cciss_scsi_interpret_error(cp);
		scsi_cmd_free(c, cp);
		return -1;
	}
	scsi_cmd_free(c, cp);
	return 0;
}

static int
cciss_scsi_do_report_phys_luns(ctlr_info_t *c,
		ReportLunData_struct *buf, int bufsize)
{
	int rc;
	CommandList_struct *cp;
	unsigned char cdb[12];
	unsigned char scsi3addr[8];
	ErrorInfo_struct *ei;

	cp = scsi_cmd_alloc(c);
	if (cp == NULL) {			/* trouble... */
		printk("cmd_alloc returned NULL!\n");
		return -1;
	}

	memset(&scsi3addr[0], 0, 8); /* address the controller */
	cdb[0] = CISS_REPORT_PHYS;
	cdb[1] = 0;
	cdb[2] = 0;
	cdb[3] = 0;
	cdb[4] = 0;
	cdb[5] = 0;
	cdb[6] = (bufsize >> 24) & 0xFF;  //MSB
	cdb[7] = (bufsize >> 16) & 0xFF;
	cdb[8] = (bufsize >> 8) & 0xFF;
	cdb[9] = bufsize & 0xFF;
	cdb[10] = 0;
	cdb[11] = 0;

	rc = cciss_scsi_do_simple_cmd(c, cp, scsi3addr,
				cdb, 12,
				(unsigned char *) buf,
				bufsize, XFER_READ);

	if (rc != 0) return rc; /* something went wrong */

	ei = cp->err_info;
	if (ei->CommandStatus != 0 &&
	    ei->CommandStatus != CMD_DATA_UNDERRUN) {
		cciss_scsi_interpret_error(cp);
		scsi_cmd_free(c, cp);
		return -1;
	}
	scsi_cmd_free(c, cp);
	return 0;
}

static void
cciss_update_non_disk_devices(int cntl_num, int hostno)
{
	/* the idea here is we could get notified from /proc
	   that some devices have changed, so we do a report
	   physical luns cmd, and adjust our list of devices
	   accordingly.  (We can't rely on the scsi-mid layer just
	   doing inquiries, because the "busses" that the scsi
	   mid-layer probes are totally fabricated by this driver,
	   so new devices wouldn't show up.

	   the scsi3addr's of devices won't change so long as the
	   adapter is not reset.  That means we can rescan and
	   tell which devices we already know about, vs. new
	   devices, vs.  disappearing devices.

	   Also, if you yank out a tape drive, then put in a disk
	   in it's place, (say, a configured volume from another
	   array controller for instance)  _don't_ poke this driver
	   (so it thinks it's still a tape, but _do_ poke the scsi
	   mid layer, so it does an inquiry... the scsi mid layer
	   could see the physical disk.  This would be bad.  Need to
	   think about how to prevent that.  One idea would be to
	   snoop all scsi responses and if an inquiry repsonse comes
	   back that reports a disk, chuck it an return selection
	   timeout instead and adjust our table...  Not sure i like
	   that though.

	 */

	ReportLunData_struct *ld_buff;
	InquiryData_struct *inq_buff;
	unsigned char scsi3addr[8];
	ctlr_info_t *c;
	__u32 num_luns=0;
	unsigned char *ch;
	/* unsigned char found[CCISS_MAX_SCSI_DEVS_PER_HBA]; */
	struct cciss_scsi_dev_t currentsd[CCISS_MAX_SCSI_DEVS_PER_HBA];
	int ncurrent=0;
	int reportlunsize = sizeof(*ld_buff) + CISS_MAX_PHYS_LUN * 8;
	int i;

	c = (ctlr_info_t *) hba[cntl_num];
	ld_buff = kmalloc(reportlunsize, GFP_KERNEL);
	if (ld_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		return;
	}
	memset(ld_buff, 0, reportlunsize);
	inq_buff = kmalloc(sizeof( InquiryData_struct), GFP_KERNEL);
	if (inq_buff == NULL) {
		printk(KERN_ERR "cciss: out of memory\n");
		kfree(ld_buff);
		return;
	}

	if (cciss_scsi_do_report_phys_luns(c, ld_buff, reportlunsize) == 0) {
		ch = &ld_buff->LUNListLength[0];
		num_luns = ((ch[0]<<24) | (ch[1]<<16) | (ch[2]<<8) | ch[3]) / 8;
		if (num_luns > CISS_MAX_PHYS_LUN) {
			printk(KERN_WARNING
				"cciss: Maximum physical LUNs (%d) exceeded.  "
				"%d LUNs ignored.\n", CISS_MAX_PHYS_LUN,
				num_luns - CISS_MAX_PHYS_LUN);
			num_luns = CISS_MAX_PHYS_LUN;
		}
	}
	else {
		printk(KERN_ERR  "cciss: Report physical LUNs failed.\n");
		return;
	}


	/* adjust our table of devices */
	for(i=0; i<num_luns; i++)
	{
		int devtype;

		/* for each physical lun, do an inquiry */
		if (ld_buff->LUN[i][3] & 0xC0) continue;
		memset(inq_buff, 0, sizeof(InquiryData_struct));
		memcpy(&scsi3addr[0], &ld_buff->LUN[i][0], 8);

		if (cciss_scsi_do_inquiry(hba[cntl_num],
			scsi3addr, inq_buff) != 0)
		{
			/* Inquiry failed (msg printed already) */
			devtype = 0; /* so we will skip this device. */
		} else /* what kind of device is this? */
			devtype = (inq_buff->data_byte[0] & 0x1f);

		switch (devtype)
		{
		  case 0x01: /* sequential access, (tape) */
		  case 0x08: /* medium changer */
			if (ncurrent >= CCISS_MAX_SCSI_DEVS_PER_HBA) {
				printk(KERN_INFO "cciss%d: %s ignored, "
					"too many devices.\n", cntl_num,
					DEVICETYPE(devtype));
				break;
			}
			memcpy(&currentsd[ncurrent].scsi3addr[0],
				&scsi3addr[0], 8);
			currentsd[ncurrent].devtype = devtype;
			currentsd[ncurrent].bus = -1;
			currentsd[ncurrent].target = -1;
			currentsd[ncurrent].lun = -1;
			ncurrent++;
			break;
		  default:
			break;
		}
	}

	adjust_cciss_scsi_table(cntl_num, hostno, currentsd, ncurrent);

	kfree(inq_buff);
	kfree(ld_buff);
	return;
}

static int
is_keyword(char *ptr, int len, char *verb)  // Thanks to ncr53c8xx.c
{
	int verb_len = strlen(verb);
	if (len >= verb_len && !memcmp(verb,ptr,verb_len))
		return verb_len;
	else
		return 0;
}

static int
cciss_scsi_user_command(int ctlr, int hostno, char *buffer, int length)
{
	int arg_len;

	if ((arg_len = is_keyword(buffer, length, "rescan")) != 0)
		cciss_update_non_disk_devices(ctlr, hostno);
	else
		return -EINVAL;
	return length;
}

/* It's a pity that we need this, but, we do... */
extern struct Scsi_Host *scsi_hostlist;  /* from ../scsi/hosts.c */

int
cciss_scsi_proc_info(char *buffer, /* data buffer */
		char **start, 	   /* where data in buffer starts */
		off_t offset,	   /* offset from start of imaginary file */
		int length, 	   /* length of data in buffer */
		int hostnum, 	   /* which host adapter (always zero for me) */
		int func)	   /* 0 == read, 1 == write */
{

	int buflen, datalen;
	struct Scsi_Host *sh;
	int found;
	ctlr_info_t *ci;
	int cntl_num;

	/* Lets see if we can find our Scsi_Host...
	   this might be kind of "bad", searching scis_hostlist this way
	   but how else can we find the scsi host?  I think I've seen
	   this coded both ways, (circular list and null terminated list)
	   I coded it to work either way, since I wasn't sure.  */

	sh = scsi_hostlist;
	found=0;
	do {
		if (sh == NULL) break;
		if (sh->host_no == hostnum) {
			found++;
			break;
		}
		sh = sh->next;
	} while (sh != scsi_hostlist && sh != NULL);

	if (sh == NULL || found == 0) /* This really shouldn't ever happen. */
		return -EINVAL;

	ci = (ctlr_info_t *) sh->hostdata[0];
	if (ci == NULL)  /* This really shouldn't ever happen. */
		return -EINVAL;

	cntl_num = ci->ctlr;	/* Get our index into the hba[] array */

	if (func == 0) {	/* User is reading from /proc/scsi/ciss*?/?*  */
		buflen = sprintf(buffer, "hostnum=%d\n", hostnum);

		datalen = buflen - offset;
		if (datalen < 0) { 	/* they're reading past EOF. */
			datalen = 0;
			*start = buffer+buflen;
		} else
			*start = buffer + offset;
		return(datalen);
	} else 	/* User is writing to /proc/scsi/cciss*?/?*  ... */
		return cciss_scsi_user_command(cntl_num, hostnum,
			buffer, length);
}

/* this is via the generic proc support */
const char *
cciss_scsi_info(struct Scsi_Host *sa)
{
	static char buf[300];
	ctlr_info_t *ci;

	/* probably need to work on putting a bit more info in here... */
	/* this is output via the /proc filesystem. */

	ci = (ctlr_info_t *) sa->hostdata[0];

	sprintf(buf, "%s %c%c%c%c\n",
		ci->product_name,
		ci->firm_ver[0],
		ci->firm_ver[1],
		ci->firm_ver[2],
		ci->firm_ver[3]);

	return buf;
}


/* cciss_scatter_gather takes a Scsi_Cmnd, (cmd), and does the pci
   dma mapping  and fills in the scatter gather entries of the
   cciss command, cp. */

static void
cciss_scatter_gather(struct pci_dev *pdev,
		CommandList_struct *cp,
		Scsi_Cmnd *cmd)
{
	unsigned int use_sg, nsegs=0, len;
	struct scatterlist *scatter = (struct scatterlist *) cmd->buffer;
	__u64 addr64;

	/* is it just one virtual address? */
	if (!cmd->use_sg) {
		if (cmd->request_bufflen) {	/* anything to xfer? */

			addr64 = (__u64) pci_map_single(pdev,
				cmd->request_buffer,
				cmd->request_bufflen,
				scsi_to_pci_dma_dir(cmd->sc_data_direction));

			cp->SG[0].Addr.lower =
			  (__u32) (addr64 & (__u64) 0x00000000FFFFFFFF);
			cp->SG[0].Addr.upper =
			  (__u32) ((addr64 >> 32) & (__u64) 0x00000000FFFFFFFF);
			cp->SG[0].Len = cmd->request_bufflen;
			nsegs=1;
		}
	} /* else, must be a list of virtual addresses.... */
	else if (cmd->use_sg <= MAXSGENTRIES) {	/* not too many addrs? */

		use_sg = pci_map_sg(pdev, cmd->buffer, cmd->use_sg,
			scsi_to_pci_dma_dir(cmd->sc_data_direction));

		for (nsegs=0; nsegs < use_sg; nsegs++) {
			addr64 = (__u64) sg_dma_address(&scatter[nsegs]);
			len  = sg_dma_len(&scatter[nsegs]);
			cp->SG[nsegs].Addr.lower =
			  (__u32) (addr64 & (__u64) 0x00000000FFFFFFFF);
			cp->SG[nsegs].Addr.upper =
			  (__u32) ((addr64 >> 32) & (__u64) 0x00000000FFFFFFFF);
			cp->SG[nsegs].Len = len;
			cp->SG[nsegs].Ext = 0;  // we are not chaining
		}
	} else BUG();

	cp->Header.SGList = (__u8) nsegs;   /* no. SGs contig in this cmd */
	cp->Header.SGTotal = (__u16) nsegs; /* total sgs in this cmd list */
	return;
}


int
cciss_scsi_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *))
{
	ctlr_info_t **c;
	int ctlr, rc;
	unsigned char scsi3addr[8];
	CommandList_struct *cp;

	// Get the ptr to our adapter structure (hba[i]) out of cmd->host.
	// We violate cmd->host privacy here.  (Is there another way?)
	c = (ctlr_info_t **) &cmd->host->hostdata[0];
	ctlr = (*c)->ctlr;

	rc = lookup_scsi3addr(ctlr, cmd->channel, cmd->target, cmd->lun,
			scsi3addr);
	if (rc != 0) {
		/* the scsi nexus does not match any that we presented... */
		/* pretend to mid layer that we got selection timeout */
		cmd->result = DID_NO_CONNECT << 16;
		done(cmd);
		/* we might want to think about registering controller itself
		   as a processor device on the bus so sg binds to it. */
		return 0;
	}

	// printk("cciss_queue_command, p=%p, cmd=0x%02x, c%db%dt%dl%d\n",
	//	cmd, cmd->cmnd[0], ctlr, cmd->channel, cmd->target, cmd->lun);

	/* Ok, we have a reasonable scsi nexus, so send the cmd down, and
		see what the device thinks of it. */

	cp = scsi_cmd_alloc(*c);
	if (cp == NULL) {			/* trouble... */
		printk("scsi_cmd_alloc returned NULL!\n");
		/* FIXME: next 3 lines are -> BAD! <- */
		cmd->result = DID_NO_CONNECT << 16;
		done(cmd);
		return 0;
	}

	// Fill in the command list header

	cmd->scsi_done = done;    // save this for use by completion code

	// save cp in case we have to abort it
	cmd->host_scribble = (unsigned char *) cp;

	cp->cmd_type = CMD_SCSI;
	cp->scsi_cmd = cmd;
	cp->Header.ReplyQueue = 0;  // unused in simple mode
	memcpy(&cp->Header.LUN.LunAddrBytes[0], &scsi3addr[0], 8);
	cp->Header.Tag.lower = cp->busaddr;  // Use k. address of cmd as tag

	// Fill in the request block...

	cp->Request.Timeout = 0; // No timeout 
	memset(cp->Request.CDB, 0, sizeof(cp->Request.CDB));
	if (cmd->cmd_len > sizeof(cp->Request.CDB)) BUG();
	cp->Request.CDBLen = cmd->cmd_len;
	memcpy(cp->Request.CDB, cmd->cmnd, cmd->cmd_len);
	cp->Request.Type.Type = TYPE_CMD;
	cp->Request.Type.Attribute = ATTR_SIMPLE;
	switch(cmd->sc_data_direction)
	{
	  case SCSI_DATA_WRITE: cp->Request.Type.Direction = XFER_WRITE; break;
	  case SCSI_DATA_READ: cp->Request.Type.Direction = XFER_READ; break;
	  case SCSI_DATA_NONE: cp->Request.Type.Direction = XFER_NONE; break;

	  case SCSI_DATA_UNKNOWN:
		// This can happen if a buggy application does a scsi passthru
		// and sets both inlen and outlen to non-zero. ( see
		// ../scsi/scsi_ioctl.c:scsi_ioctl_send_command() )

		cp->Request.Type.Direction = XFER_RSVD;
		// This is technically wrong, and cciss controllers should
		// reject it with CMD_INVALID, which is the most correct
		// response, but non-fibre backends appear to let it
		// slide by, and give the same results as if this field
		// were set correctly.  Either way is acceptable for
		// our purposes here.

		break;

	  default:
		printk("cciss: unknown data direction: %d\n",
			cmd->sc_data_direction);
		BUG();
		break;
	}

	cciss_scatter_gather((*c)->pdev, cp, cmd); // Fill the SG list

	/* Put the request on the tail of the request queue */

	addQ(&(*c)->reqQ, cp);
	(*c)->Qdepth++;
	start_io(*c);

	/* the cmd'll come back via intr handler in complete_scsi_command()  */
	return 0;
}

static void
init_driver_template(int ctlr)
{
	memset(&driver_template[ctlr], 0, sizeof(driver_template[ctlr]));
	driver_template[ctlr].name = ccissscsi[ctlr].name;
	driver_template[ctlr].proc_name = ccissscsi[ctlr].name;
	driver_template[ctlr].detect = cciss_scsi_detect;
	driver_template[ctlr].release = cciss_scsi_release;
	driver_template[ctlr].proc_info = cciss_scsi_proc_info;
	driver_template[ctlr].queuecommand = cciss_scsi_queue_command;
	driver_template[ctlr].eh_abort_handler = NULL;
	driver_template[ctlr].eh_device_reset_handler = NULL;
	driver_template[ctlr].bios_param = scsicam_bios_param;
	driver_template[ctlr].can_queue = SCSI_CCISS_CAN_QUEUE;
	driver_template[ctlr].this_id = SELF_SCSI_ID;
	driver_template[ctlr].sg_tablesize = MAXSGENTRIES;
	driver_template[ctlr].cmd_per_lun = 1;
	driver_template[ctlr].use_new_eh_code = 1;
	driver_template[ctlr].use_clustering = DISABLE_CLUSTERING;
	driver_template[ctlr].module = THIS_MODULE;

	/* set scsi_host to NULL so our detect routine will
	   find us on register */

	((struct cciss_scsi_adapter_data_t *)
		hba[ctlr]->scsi_ctlr)->scsi_host = NULL;

}

static void
cciss_unregister_scsi(int ctlr)
{
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	unsigned long flags;

	/* we are being forcibly unloaded, and may not refuse. */

	spin_lock_irqsave(&io_request_lock, flags);
	sa = (struct cciss_scsi_adapter_data_t *) hba[ctlr]->scsi_ctlr;
	stk = &sa->cmd_stack;

	/* if we weren't ever actually registered, don't unregister */
	if (((struct cciss_scsi_adapter_data_t *)
		hba[ctlr]->scsi_ctlr)->registered) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		scsi_unregister_module(MODULE_SCSI_HA, &driver_template[ctlr]);
		spin_lock_irqsave(&io_request_lock, flags);
	}
	init_driver_template(ctlr);
	scsi_cmd_stack_free(ctlr);
	kfree(hba[ctlr]->scsi_ctlr);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

static int
cciss_register_scsi(int ctlr, int this_is_init_time)
{
	unsigned long flags;

	CPQ_TAPE_LOCK(ctlr, flags);

	sprintf( ccissscsi[ctlr].name, "cciss%d", ctlr );
	
	init_driver_template(ctlr);

	/* Since this is really a block driver, the SCSI core may not be
	   initialized yet, in which case, calling scsi_register_module
	   would hang.  instead, we will do it later, via /proc filesystem
	   and rc scripts, when we know SCSI core is good to go. */

	if (this_is_init_time) {
		CPQ_TAPE_UNLOCK(ctlr, flags);
		return 0;
	}

	/* Only register if SCSI devices are detected. */
	if (ccissscsi[ctlr].ndevices != 0) {
		((struct cciss_scsi_adapter_data_t *)
			hba[ctlr]->scsi_ctlr)->registered = 1;
		CPQ_TAPE_UNLOCK(ctlr, flags);
		return scsi_register_module(MODULE_SCSI_HA,
			&driver_template[ctlr]);
	}
	CPQ_TAPE_UNLOCK(ctlr, flags);
	printk(KERN_INFO
		"cciss%d: No appropriate SCSI device detected, "
		"SCSI subsystem not engaged.\n", ctlr);
	return 0;
}

static int
cciss_engage_scsi(int ctlr)
{
	struct cciss_scsi_adapter_data_t *sa;
	struct cciss_scsi_cmd_stack_t *stk;
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);
	sa = (struct cciss_scsi_adapter_data_t *) hba[ctlr]->scsi_ctlr;
	stk = &sa->cmd_stack;

	if (((struct cciss_scsi_adapter_data_t *)
		hba[ctlr]->scsi_ctlr)->registered) {
		printk("cciss%d: SCSI subsystem already engaged.\n", ctlr);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return ENXIO;
	}
	spin_unlock_irqrestore(&io_request_lock, flags);
	cciss_update_non_disk_devices(ctlr, -1);
	cciss_register_scsi(ctlr, 0);
	return 0;
}

static void
cciss_proc_tape_report(int ctlr, unsigned char *buffer, off_t *pos, off_t *len)
{
	int size;
	unsigned int flags;

	*pos = *pos -1; *len = *len - 1; // cut off the last trailing newline

	CPQ_TAPE_LOCK(ctlr, flags);
	size = sprintf(buffer + *len,
		"Sequential access devices: %d\n\n",
		 ccissscsi[ctlr].ndevices);
	CPQ_TAPE_UNLOCK(ctlr, flags);
	*pos += size; *len += size;
}

#else /* no CONFIG_CISS_SCSI_TAPE */

/* If no tape support, then these become defined out of existence */

#define cciss_find_non_disk_devices(cntl_num)
#define cciss_unregister_scsi(ctlr)
#define cciss_register_scsi(ctlr, this_is_init_time)
#define cciss_proc_tape_report(ctlr, buffer, pos, len)

#endif /* CONFIG_CISS_SCSI_TAPE */
