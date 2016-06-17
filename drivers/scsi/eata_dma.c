/************************************************************
 *							    *
 *		    Linux EATA SCSI driver		    *
 *							    *
 *  based on the CAM document CAM/89-004 rev. 2.0c,	    *
 *  DPT's driver kit, some internal documents and source,   *
 *  and several other Linux scsi drivers and kernel docs.   *
 *							    *
 *  The driver currently:				    *
 *	-supports all ISA based EATA-DMA boards		    *
 *       like PM2011, PM2021, PM2041, PM3021                *
 *	-supports all EISA based EATA-DMA boards	    *
 *       like PM2012B, PM2022, PM2122, PM2322, PM2042,      *
 *            PM3122, PM3222, PM3332                        *
 *	-supports all PCI based EATA-DMA boards		    *
 *       like PM2024, PM2124, PM2044, PM2144, PM3224,       *
 *            PM3334                                        *
 *      -supports the Wide, Ultra Wide and Differential     *
 *       versions of the boards                             *
 *	-supports multiple HBAs with & without IRQ sharing  *
 *	-supports all SCSI channels on multi channel boards *
 *      -supports ix86 and MIPS, untested on ALPHA          *
 *	-needs identical IDs on all channels of a HBA	    * 
 *	-can be loaded as module			    *
 *	-displays statistical and hardware information	    *
 *	 in /proc/scsi/eata_dma				    *
 *      -provides rudimentary latency measurement           * 
 *       possibilities via /proc/scsi/eata_dma/<hostnum>    *
 *							    *
 *  (c)1993-96 Michael Neuffer			            *
 *             mike@i-Connect.Net                           *
 *	       neuffer@mail.uni-mainz.de	            *
 *							    *
 *  This program is free software; you can redistribute it  *
 *  and/or modify it under the terms of the GNU General	    *
 *  Public License as published by the Free Software	    *
 *  Foundation; either version 2 of the License, or	    *
 *  (at your option) any later version.			    *
 *							    *
 *  This program is distributed in the hope that it will be *
 *  useful, but WITHOUT ANY WARRANTY; without even the	    *
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A    *
 *  PARTICULAR PURPOSE.	 See the GNU General Public License *
 *  for more details.					    *
 *							    *
 *  You should have received a copy of the GNU General	    *
 *  Public License along with this kernel; if not, write to *
 *  the Free Software Foundation, Inc., 675 Mass Ave,	    *
 *  Cambridge, MA 02139, USA.				    *
 *							    *
 * I have to thank DPT for their excellent support. I took  *
 * me almost a year and a stopover at their HQ, on my first *
 * trip to the USA, to get it, but since then they've been  *
 * very helpful and tried to give me all the infos and	    *
 * support I need.					    *
 *							    *
 * Thanks also to Simon Shapiro, Greg Hosler and Mike       *
 * Jagdis who did a lot of testing and found quite a number *
 * of bugs during the development.                          *
 ************************************************************
 *  last change: 96/10/21                 OS: Linux 2.0.23  *
 ************************************************************/

/* Look in eata_dma.h for configuration and revision information */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/in.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/types.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/pgtable.h>
#ifdef __mips__
#include <asm/cachectl.h>
#include <linux/spinlock.h>
#endif
#include <linux/blk.h>
#include "scsi.h"
#include "sd.h"
#include "hosts.h"
#include "eata_dma.h"
#include "eata_dma_proc.h" 

#include <linux/stat.h>
#include <linux/config.h>	/* for CONFIG_PCI */

static u32 ISAbases[] =
{0x1F0, 0x170, 0x330, 0x230};
static unchar EISAbases[] =
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static uint registered_HBAs = 0;
static struct Scsi_Host *last_HBA = NULL;
static struct Scsi_Host *first_HBA = NULL;
static unchar reg_IRQ[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static unchar reg_IRQL[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static struct eata_sp *status = 0;   /* Statuspacket array   */
static void *dma_scratch = 0;

static struct eata_register *fake_int_base;
static int fake_int_result;
static int fake_int_happened;

static ulong int_counter = 0;
static ulong queue_counter = 0;

void eata_fake_int_handler(s32 irq, void *dev_id, struct pt_regs * regs)
{
    fake_int_result = inb((ulong)fake_int_base + HA_RSTATUS);
    fake_int_happened = TRUE;
    DBG(DBG_INTR3, printk("eata_fake_int_handler called irq%d base %p"
			  " res %#x\n", irq, fake_int_base, fake_int_result));
    return;
}

#include "eata_dma_proc.c"

#ifdef MODULE
int eata_release(struct Scsi_Host *sh)
{
    uint i;
    if (sh->irq && reg_IRQ[sh->irq] == 1) free_irq(sh->irq, NULL);
    else reg_IRQ[sh->irq]--;
    
    kfree((void *)status);
    kfree((void *)dma_scratch - 4);
    for (i = 0; i < sh->can_queue; i++){ /* Free all SG arrays */
	if(SD(sh)->ccb[i].sg_list != NULL)
	    kfree((void *) SD(sh)->ccb[i].sg_list);
    }
    
    if (SD(sh)->channel == 0) {
	if (sh->dma_channel != BUSMASTER) free_dma(sh->dma_channel);
	if (sh->io_port && sh->n_io_port)
	    release_region(sh->io_port, sh->n_io_port);
    }
    return(TRUE);
}
#endif


inline void eata_latency_in(struct eata_ccb *cp, hostdata *hd)
{
    uint time;
    time = jiffies - cp->timestamp;
    if(hd->all_lat[1] > time)
        hd->all_lat[1] = time;
    if(hd->all_lat[2] < time)
        hd->all_lat[2] = time;
    hd->all_lat[3] += time;
    hd->all_lat[0]++;
    if((cp->rw_latency) == WRITE) { /* was WRITE */
        if(hd->writes_lat[cp->sizeindex][1] > time)
	    hd->writes_lat[cp->sizeindex][1] = time;
	if(hd->writes_lat[cp->sizeindex][2] < time)
	    hd->writes_lat[cp->sizeindex][2] = time;
	hd->writes_lat[cp->sizeindex][3] += time;
	hd->writes_lat[cp->sizeindex][0]++;
    } else if((cp->rw_latency) == READ) {
        if(hd->reads_lat[cp->sizeindex][1] > time)
	    hd->reads_lat[cp->sizeindex][1] = time;
	if(hd->reads_lat[cp->sizeindex][2] < time)
	    hd->reads_lat[cp->sizeindex][2] = time;
	hd->reads_lat[cp->sizeindex][3] += time;
	hd->reads_lat[cp->sizeindex][0]++;
    }
} 

inline void eata_latency_out(struct eata_ccb *cp, Scsi_Cmnd *cmd)
{
    int x, z;
    short *sho;
    long *lon;
    x = 0;	                        /* just to keep GCC quiet */ 
    cp->timestamp = jiffies;	        /* For latency measurements */
    switch(cmd->cmnd[0]) {
    case WRITE_6:   
        x = cmd->cmnd[4]/2; 
	cp->rw_latency = WRITE;
	break;
    case READ_6:    
        x = cmd->cmnd[4]/2; 
	cp->rw_latency = READ;
	break;
    case WRITE_10:   
        sho = (short *) &cmd->cmnd[7];
	x = ntohs(*sho)/2;	      
	cp->rw_latency = WRITE;
	break;
    case READ_10:
        sho = (short *) &cmd->cmnd[7];
	x = ntohs(*sho)/2;	      
	cp->rw_latency = READ;
	break;
    case WRITE_12:   
        lon = (long *) &cmd->cmnd[6];
	x = ntohl(*lon)/2;	      
	cp->rw_latency = WRITE;
	break;
    case READ_12:
        lon = (long *) &cmd->cmnd[6];
	x = ntohl(*lon)/2;	      
	cp->rw_latency = READ;
	break;
    default:
        cp->rw_latency = OTHER;
	break;
    }
    if (cmd->cmnd[0] == WRITE_6 || cmd->cmnd[0] == WRITE_10 || 
	cmd->cmnd[0] == WRITE_12 || cmd->cmnd[0] == READ_6 || 
	cmd->cmnd[0] == READ_10 || cmd->cmnd[0] == READ_12) {
        for(z = 0; (x > (1 << z)) && (z <= 11); z++) 
	    /* nothing */;
	cp->sizeindex = z;
    } 
}

void eata_int_handler(int, void *, struct pt_regs *);

void do_eata_int_handler(int irq, void *dev_id, struct pt_regs * regs)
{
    unsigned long flags;

    spin_lock_irqsave(&io_request_lock, flags);
    eata_int_handler(irq, dev_id, regs);
    spin_unlock_irqrestore(&io_request_lock, flags);
}

void eata_int_handler(int irq, void *dev_id, struct pt_regs * regs)
{
    uint i, result = 0;
    uint hba_stat, scsi_stat, eata_stat;
    Scsi_Cmnd *cmd;
    struct eata_ccb *ccb;
    struct eata_sp *sp;
    uint base;
    uint x;
    struct Scsi_Host *sh;

    for (x = 1, sh = first_HBA; x <= registered_HBAs; x++, sh = SD(sh)->next) {
	if (sh->irq != irq)
	    continue;
	
	while(inb((uint)sh->base + HA_RAUXSTAT) & HA_AIRQ) {
	    
	    int_counter++;
	    
	    sp = &SD(sh)->sp;
#ifdef __mips__
            sys_cacheflush(sp, sizeof(struct eata_sp), 2);
#endif
	    ccb = sp->ccb;
	    
	    if(ccb == NULL) {
		eata_stat = inb((uint)sh->base + HA_RSTATUS);
		printk("eata_dma: int_handler, Spurious IRQ %d "
		       "received. CCB pointer not set.\n", irq);
		break;
	    }

	    cmd = ccb->cmd;
	    base = (uint) cmd->host->base;
       	    hba_stat = sp->hba_stat;
	    
	    scsi_stat = (sp->scsi_stat >> 1) & 0x1f; 
	    
	    if (sp->EOC == FALSE) {
		eata_stat = inb(base + HA_RSTATUS);
		printk(KERN_WARNING "eata_dma: int_handler, board: %x cmd %lx "
		       "returned unfinished.\n"
		       "EATA: %x HBA: %x SCSI: %x spadr %lx spadrirq %lx, "
		       "irq%d\n", base, (long)ccb, eata_stat, hba_stat, 
		       scsi_stat,(long)&status, (long)&status[irq], irq);
		cmd->result = DID_ERROR << 16;
		ccb->status = FREE;
		cmd->scsi_done(cmd);
		break;
	    } 
	    
           sp->EOC = FALSE; /* Clean out this flag */

           if (ccb->status == LOCKED || ccb->status == RESET) {
               printk("eata_dma: int_handler, reseted command pid %ld returned"
		      "\n", cmd->pid);
	       DBG(DBG_INTR && DBG_DELAY, DELAY(1));
	    }
	    
	    eata_stat = inb(base + HA_RSTATUS); 
	    DBG(DBG_INTR, printk("IRQ %d received, base %#.4x, pid %ld, "
				 "target: %x, lun: %x, ea_s: %#.2x, hba_s: "
				 "%#.2x \n", irq, base, cmd->pid, cmd->target,
				 cmd->lun, eata_stat, hba_stat));
	    
	    switch (hba_stat) {
	    case HA_NO_ERROR:	/* NO Error */
		if(HD(cmd)->do_latency == TRUE && ccb->timestamp) 
		    eata_latency_in(ccb, HD(cmd));
		result = DID_OK << 16;
		break;
	    case HA_ERR_SEL_TO:	        /* Selection Timeout */
	    case HA_ERR_CMD_TO:	        /* Command Timeout   */
		result = DID_TIME_OUT << 16;
		break;
	    case HA_BUS_RESET:		/* SCSI Bus Reset Received */
		result = DID_RESET << 16;
		DBG(DBG_STATUS, printk(KERN_WARNING "scsi%d: BUS RESET "
				       "received on cmd %ld\n", 
				       HD(cmd)->HBA_number, cmd->pid));
		break;
	    case HA_INIT_POWERUP:	/* Initial Controller Power-up */
		if (cmd->device->type != TYPE_TAPE)
		    result = DID_BUS_BUSY << 16;
		else
		    result = DID_ERROR << 16;
		
		for (i = 0; i < MAXTARGET; i++)
		DBG(DBG_STATUS, printk(KERN_DEBUG "scsi%d: cmd pid %ld "
				       "returned with INIT_POWERUP\n", 
				       HD(cmd)->HBA_number, cmd->pid));
		break;
	    case HA_CP_ABORT_NA:
	    case HA_CP_ABORTED:
		result = DID_ABORT << 16;
		DBG(DBG_STATUS, printk(KERN_WARNING "scsi%d: aborted cmd "
				       "returned\n", HD(cmd)->HBA_number));
 		break;
	    case HA_CP_RESET_NA:
	    case HA_CP_RESET:
	        HD(cmd)->resetlevel[cmd->channel] = 0; 
		result = DID_RESET << 16;
		DBG(DBG_STATUS, printk(KERN_WARNING "scsi%d: reseted cmd "
				       "pid %ldreturned\n", 
				       HD(cmd)->HBA_number, cmd->pid));
	    case HA_SCSI_HUNG:	        /* SCSI Hung                 */
	        printk(KERN_ERR "scsi%d: SCSI hung\n", HD(cmd)->HBA_number);
		result = DID_ERROR << 16;
		break;
	    case HA_RSENSE_FAIL:        /* Auto Request-Sense Failed */
	        DBG(DBG_STATUS, printk(KERN_ERR "scsi%d: Auto Request Sense "
				       "Failed\n", HD(cmd)->HBA_number));
		result = DID_ERROR << 16;
		break;
	    case HA_UNX_BUSPHASE:	/* Unexpected Bus Phase */
	    case HA_UNX_BUS_FREE:	/* Unexpected Bus Free */
	    case HA_BUS_PARITY:	        /* Bus Parity Error */
	    case HA_UNX_MSGRJCT:	/* Unexpected Message Reject */
	    case HA_RESET_STUCK:        /* SCSI Bus Reset Stuck */
	    case HA_PARITY_ERR:	        /* Controller Ram Parity */
	    default:
		result = DID_ERROR << 16;
		break;
	    }
	    cmd->result = result | (scsi_stat << 1); 
	    
#if DBG_INTR2
	    if (scsi_stat || result || hba_stat || eata_stat != 0x50 
		|| cmd->scsi_done == NULL || cmd->device->id == 7) 
		printk("HBA: %d, channel %d, id: %d, lun %d, pid %ld:\n" 
		       "eata_stat %#x, hba_stat %#.2x, scsi_stat %#.2x, "
		       "sense_key: %#x, result: %#.8x\n", x, 
		       cmd->device->channel, cmd->device->id, cmd->device->lun,
		       cmd->pid, eata_stat, hba_stat, scsi_stat, 
		       cmd->sense_buffer[2] & 0xf, cmd->result); 
	    DBG(DBG_INTR&&DBG_DELAY,DELAY(1));
#endif
	    
	    ccb->status = FREE;	    /* now we can release the slot  */
	    cmd->scsi_done(cmd);
	}
    }

    return;
}

inline int eata_send_command(u32 addr, u32 base, u8 command)
{
    long loop = R_LIMIT;
    
    while (inb(base + HA_RAUXSTAT) & HA_ABUSY)
	if (--loop == 0)
	    return(FALSE);

    if(addr != (u32) NULL)
        addr = virt_to_bus((void *)addr);

    /*
     * This is overkill.....but the MIPSen seem to need this
     * and it will be optimized away for i86 and ALPHA machines.
     */
    flush_cache_all();

    /* And now the address in nice little byte chunks */
#ifdef __LITTLE_ENDIAN
    outb(addr,       base + HA_WDMAADDR);
    outb(addr >> 8,  base + HA_WDMAADDR + 1);
    outb(addr >> 16, base + HA_WDMAADDR + 2);
    outb(addr >> 24, base + HA_WDMAADDR + 3);
#else
    outb(addr >> 24, base + HA_WDMAADDR);
    outb(addr >> 16, base + HA_WDMAADDR + 1);
    outb(addr >> 8,  base + HA_WDMAADDR + 2);
    outb(addr,       base + HA_WDMAADDR + 3);
#endif
    outb(command, base + HA_WCOMMAND);
    return(TRUE);
}

inline int eata_send_immediate(u32 base, u32 addr, u8 ifc, u8 code, u8 code2)
{
    if(addr != (u32) NULL)
        addr = virt_to_bus((void *)addr);

    /*
     * This is overkill.....but the MIPSen seem to need this
     * and it will be optimized away for i86 and ALPHA machines.
     */
    flush_cache_all();

    outb(0x0, base + HA_WDMAADDR - 1);
    if(addr){
#ifdef __LITTLE_ENDIAN
        outb(addr,       base + HA_WDMAADDR);
	outb(addr >> 8,  base + HA_WDMAADDR + 1);
	outb(addr >> 16, base + HA_WDMAADDR + 2);
	outb(addr >> 24, base + HA_WDMAADDR + 3);
#else
        outb(addr >> 24, base + HA_WDMAADDR);
	outb(addr >> 16, base + HA_WDMAADDR + 1);
	outb(addr >> 8,  base + HA_WDMAADDR + 2);
	outb(addr,       base + HA_WDMAADDR + 3);
#endif
    } else {
        outb(0x0, base + HA_WDMAADDR);
        outb(0x0, base + HA_WDMAADDR + 1);
 	outb(code2, base + HA_WCODE2);
	outb(code,  base + HA_WCODE);
    }
    
    outb(ifc, base + HA_WIFC);
    outb(EATA_CMD_IMMEDIATE, base + HA_WCOMMAND);
    return(TRUE);
}

int eata_queue(Scsi_Cmnd * cmd, void (* done) (Scsi_Cmnd *))
{
    unsigned int i, x, y;
    ulong flags;
    hostdata *hd;
    struct Scsi_Host *sh;
    struct eata_ccb *ccb;
    struct scatterlist *sl;

    
    save_flags(flags);
    cli();

#if 0
    for (x = 1, sh = first_HBA; x <= registered_HBAs; x++, sh = SD(sh)->next) {
      if(inb((uint)sh->base + HA_RAUXSTAT) & HA_AIRQ) {
            printk("eata_dma: scsi%d interrupt pending in eata_queue.\n"
		   "          Calling interrupt handler.\n", sh->host_no);
            eata_int_handler(sh->irq, 0, 0);
      }
    }
#endif
    
    queue_counter++;

    hd = HD(cmd);
    sh = cmd->host;
    
    if (cmd->cmnd[0] == REQUEST_SENSE && cmd->sense_buffer[0] != 0) {
        DBG(DBG_REQSENSE, printk(KERN_DEBUG "Tried to REQUEST SENSE\n"));
	cmd->result = DID_OK << 16;
	done(cmd);
	restore_flags(flags);

	return(0);
    }

    /* check for free slot */
    for (y = hd->last_ccb + 1, x = 0; x < sh->can_queue; x++, y++) { 
	if (y >= sh->can_queue)
	    y = 0;
	if (hd->ccb[y].status == FREE)
	    break;
    }
    
    hd->last_ccb = y;

    if (x >= sh->can_queue) { 
	cmd->result = DID_BUS_BUSY << 16;
	DBG(DBG_QUEUE && DBG_ABNORM, 
	    printk(KERN_CRIT "eata_queue pid %ld, HBA QUEUE FULL..., "
		   "returning DID_BUS_BUSY\n", cmd->pid));
	done(cmd);
	restore_flags(flags);
	return(0);
    }
    ccb = &hd->ccb[y];
    
    memset(ccb, 0, sizeof(struct eata_ccb) - sizeof(struct eata_sg_list *));
    
    ccb->status = USED;			/* claim free slot */

    restore_flags(flags);
    
    DBG(DBG_QUEUE, printk("eata_queue pid %ld, target: %x, lun: %x, y %d\n",
			  cmd->pid, cmd->target, cmd->lun, y));
    DBG(DBG_QUEUE && DBG_DELAY, DELAY(1));
    
    if(hd->do_latency == TRUE) 
        eata_latency_out(ccb, cmd);

    cmd->scsi_done = (void *)done;
    
    switch (cmd->cmnd[0]) {
    case CHANGE_DEFINITION: case COMPARE:	  case COPY:
    case COPY_VERIFY:	    case LOG_SELECT:	  case MODE_SELECT:
    case MODE_SELECT_10:    case SEND_DIAGNOSTIC: case WRITE_BUFFER:
    case FORMAT_UNIT:	    case REASSIGN_BLOCKS: case RESERVE:
    case SEARCH_EQUAL:	    case SEARCH_HIGH:	  case SEARCH_LOW:
    case WRITE_6:	    case WRITE_10:	  case WRITE_VERIFY:
    case UPDATE_BLOCK:	    case WRITE_LONG:	  case WRITE_SAME:	
    case SEARCH_HIGH_12:    case SEARCH_EQUAL_12: case SEARCH_LOW_12:
    case WRITE_12:	    case WRITE_VERIFY_12: case SET_WINDOW: 
    case MEDIUM_SCAN:	    case SEND_VOLUME_TAG:	     
    case 0xea:	    /* alternate number for WRITE LONG */
	ccb->DataOut = TRUE;	/* Output mode */
	break;
    case TEST_UNIT_READY:
    default:
	ccb->DataIn = TRUE;	/* Input mode  */
    }

    /* FIXME: This will will have to be changed once the midlevel driver 
     *        allows different HBA IDs on every channel.
     */
    if (cmd->target == sh->this_id) 
	ccb->Interpret = TRUE;	/* Interpret command */

    if (cmd->use_sg) {
	ccb->scatter = TRUE;	/* SG mode     */
	if (ccb->sg_list == NULL) {
	    ccb->sg_list = kmalloc(sh->sg_tablesize * sizeof(struct eata_sg_list),
				  GFP_ATOMIC | GFP_DMA);
	}
	if (ccb->sg_list == NULL)
	{
	    /*
	     *	Claim the bus was busy. Actually we are the problem but this
	     *  will do a deferred retry for us ;)
	     */
	    printk(KERN_ERR "eata_dma: Run out of DMA memory for SG lists !\n");
	    cmd->result = DID_BUS_BUSY << 16;
	    ccb->status = FREE;    
	    done(cmd);
	    return(0);
	}
	ccb->cp_dataDMA = htonl(virt_to_bus(ccb->sg_list)); 
	
	ccb->cp_datalen = htonl(cmd->use_sg * sizeof(struct eata_sg_list));
	sl=(struct scatterlist *)cmd->request_buffer;
	for(i = 0; i < cmd->use_sg; i++, sl++){
	    ccb->sg_list[i].data = htonl(virt_to_bus(sl->address));
	    ccb->sg_list[i].len = htonl((u32) sl->length);
	}
    } else {
	ccb->scatter = FALSE;
	ccb->cp_datalen = htonl(cmd->request_bufflen);
	ccb->cp_dataDMA = htonl(virt_to_bus(cmd->request_buffer));
    }
    
    ccb->Auto_Req_Sen = TRUE;
    ccb->cp_reqDMA = htonl(virt_to_bus(cmd->sense_buffer));
    ccb->reqlen = sizeof(cmd->sense_buffer);
    
    ccb->cp_id = cmd->target;
    ccb->cp_channel = cmd->channel;
    ccb->cp_lun = cmd->lun;
    ccb->cp_dispri = TRUE;
    ccb->cp_identify = TRUE;
    memcpy(ccb->cp_cdb, cmd->cmnd, cmd->cmd_len);
    
    ccb->cp_statDMA = htonl(virt_to_bus(&(hd->sp)));
    
    ccb->cp_viraddr = ccb; /* This will be passed thru, so we don't need to 
			    * convert it */
    ccb->cmd = cmd;
    cmd->host_scribble = (char *)&hd->ccb[y];	
    
    if(eata_send_command((u32) ccb, (u32) sh->base, EATA_CMD_DMA_SEND_CP) == FALSE) {
	cmd->result = DID_BUS_BUSY << 16;
	DBG(DBG_QUEUE && DBG_ABNORM, 
	    printk("eata_queue target %d, pid %ld, HBA busy, "
		   "returning DID_BUS_BUSY\n",cmd->target, cmd->pid));
	ccb->status = FREE;    
	done(cmd);
	return(0);
    }
    DBG(DBG_QUEUE, printk("Queued base %#.4x pid: %ld target: %x lun: %x "
			 "slot %d irq %d\n", (s32)sh->base, cmd->pid, 
			 cmd->target, cmd->lun, y, sh->irq));
    DBG(DBG_QUEUE && DBG_DELAY, DELAY(1));

    return(0);
}


int eata_abort(Scsi_Cmnd * cmd)
{
    ulong loop = HZ / 2;
    ulong flags;
    int x;
    struct Scsi_Host *sh;
 
    save_flags(flags);
    cli();

    DBG(DBG_ABNORM, printk("eata_abort called pid: %ld target: %x lun: %x"
			   " reason %x\n", cmd->pid, cmd->target, cmd->lun, 
			   cmd->abort_reason));
    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));

    /* Some interrupt controllers seem to loose interrupts */
    for (x = 1, sh = first_HBA; x <= registered_HBAs; x++, sh = SD(sh)->next) {
        if(inb((uint)sh->base + HA_RAUXSTAT) & HA_AIRQ) {
            printk("eata_dma: scsi%d interrupt pending in eata_abort.\n"
		   "          Calling interrupt handler.\n", sh->host_no);
	    eata_int_handler(sh->irq, 0, 0);
	}
    }

    while (inb((u32)(cmd->host->base) + HA_RAUXSTAT) & HA_ABUSY) {
	if (--loop == 0) {
	    printk("eata_dma: abort, timeout error.\n");
	    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	    restore_flags(flags);
	    return (SCSI_ABORT_ERROR);
	}
    }
    if (CD(cmd)->status == RESET) {
	printk("eata_dma: abort, command reset error.\n");
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	restore_flags(flags);
	return (SCSI_ABORT_ERROR);
    }
    if (CD(cmd)->status == LOCKED) {
	DBG(DBG_ABNORM, printk("eata_dma: abort, queue slot locked.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	restore_flags(flags);
	return (SCSI_ABORT_NOT_RUNNING);
    }
    if (CD(cmd)->status == USED) {
	DBG(DBG_ABNORM, printk("Returning: SCSI_ABORT_BUSY\n"));
	restore_flags(flags);
	return (SCSI_ABORT_BUSY);  /* SNOOZE */ 
    }
    if (CD(cmd)->status == FREE) {
	DBG(DBG_ABNORM, printk("Returning: SCSI_ABORT_NOT_RUNNING\n")); 
	restore_flags(flags);
	return (SCSI_ABORT_NOT_RUNNING);
    }
    restore_flags(flags);
    panic("eata_dma: abort: invalid slot status\n");
}

int eata_reset(Scsi_Cmnd * cmd, unsigned int resetflags)
{
    uint x; 
    /* 10 million PCI reads take at least one third of a second */
    ulong loop = 10 * 1000 * 1000;
    ulong flags;
    unchar success = FALSE;
    Scsi_Cmnd *sp; 
    struct Scsi_Host *sh;
    
    save_flags(flags);
    cli();
    
    DBG(DBG_ABNORM, printk("eata_reset called pid:%ld target: %x lun: %x"
			   " reason %x\n", cmd->pid, cmd->target, cmd->lun, 
			   cmd->abort_reason));
	
    for (x = 1, sh = first_HBA; x <= registered_HBAs; x++, sh = SD(sh)->next) {
        if(inb((uint)sh->base + HA_RAUXSTAT) & HA_AIRQ) {
            printk("eata_dma: scsi%d interrupt pending in eata_reset.\n"
		   "          Calling interrupt handler.\n", sh->host_no);
            eata_int_handler(sh->irq, 0, 0);
      }
    }

    if (HD(cmd)->state == RESET) {
	printk("eata_reset: exit, already in reset.\n");
	restore_flags(flags);
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_RESET_ERROR);
    }
    
    while (inb((u32)(cmd->host->base) + HA_RAUXSTAT) & HA_ABUSY)
	if (--loop == 0) {
	    printk("eata_reset: exit, timeout error.\n");
	    restore_flags(flags);
	    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	    return (SCSI_RESET_ERROR);
	}
 
    for (x = 0; x < cmd->host->can_queue; x++) {
	if (HD(cmd)->ccb[x].status == FREE)
	    continue;

	if (HD(cmd)->ccb[x].status == LOCKED) {
	    HD(cmd)->ccb[x].status = FREE;
	    printk("eata_reset: locked slot %d forced free.\n", x);
	    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	    continue;
	}


	sp = HD(cmd)->ccb[x].cmd;
	HD(cmd)->ccb[x].status = RESET;
	
	if (sp == NULL)
	    panic("eata_reset: slot %d, sp==NULL.\n", x);

	printk("eata_reset: slot %d in reset, pid %ld.\n", x, sp->pid);

	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	
	if (sp == cmd)
	    success = TRUE;
    }
    
    /* hard reset the HBA  */
    inb((u32) (cmd->host->base) + HA_RSTATUS);	/* This might cause trouble */
    eata_send_command(0, (u32) cmd->host->base, EATA_CMD_RESET);
    
    HD(cmd)->state = RESET;

    DBG(DBG_ABNORM, printk("eata_reset: board reset done, enabling "
			   "interrupts.\n"));

    DELAY(2); /* In theorie we should get interrupts and set free all
	       * used queueslots */
    
    DBG(DBG_ABNORM, printk("eata_reset: interrupts disabled again.\n"));
    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
    
    for (x = 0; x < cmd->host->can_queue; x++) {
	
	/* Skip slots already set free by interrupt and those that 
         * are still LOCKED from the last reset */
	if (HD(cmd)->ccb[x].status != RESET)
	    continue;
	
	sp = HD(cmd)->ccb[x].cmd;
	sp->result = DID_RESET << 16;
	
	/* This mailbox is still waiting for its interrupt */
	HD(cmd)->ccb[x].status = LOCKED;
	
	printk("eata_reset: slot %d locked, DID_RESET, pid %ld done.\n",
	       x, sp->pid);
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));

	sp->scsi_done(sp);
    }
    
    HD(cmd)->state = FALSE;
    restore_flags(flags);
    
    if (success) {
	DBG(DBG_ABNORM, printk("eata_reset: exit, pending.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_RESET_PENDING);
    } else {
	DBG(DBG_ABNORM, printk("eata_reset: exit, wakeup.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_RESET_PUNT);
    }
}

/* Here we try to determine the optimum queue depth for
 * each attached device.
 *
 * At the moment the algorithm is rather simple
 */
static void eata_select_queue_depths(struct Scsi_Host *host, 
				     Scsi_Device *devicelist)
{
    Scsi_Device *device;
    int devcount = 0; 
    int factor = 0;

#if CRIPPLE_QUEUE    
    for(device = devicelist; device != NULL; device = device->next) {
        if(device->host == host)
	    device->queue_depth = 2;
    }
#else
    /* First we do a sample run go find out what we have */
    for(device = devicelist; device != NULL; device = device->next) {
        if (device->host == host) {
	    devcount++;
	    switch(device->type) {
	    case TYPE_DISK:
	    case TYPE_MOD:
	        factor += TYPE_DISK_QUEUE;
		break;
	    case TYPE_TAPE:
	        factor += TYPE_TAPE_QUEUE;
		break;
	    case TYPE_WORM:
	    case TYPE_ROM:
	        factor += TYPE_ROM_QUEUE;
		break;
	    case TYPE_PROCESSOR:
	    case TYPE_SCANNER:
	    default:
	        factor += TYPE_OTHER_QUEUE;
		break;
	    }
	}
    }

    DBG(DBG_REGISTER, printk(KERN_DEBUG "scsi%d: needed queueslots %d\n", 
			     host->host_no, factor));

    if(factor == 0)    /* We don't want to get a DIV BY ZERO error */
        factor = 1;

    factor = (SD(host)->queuesize * 10) / factor;

    DBG(DBG_REGISTER, printk(KERN_DEBUG "scsi%d: using factor %dE-1\n", 
			     host->host_no, factor));

    /* Now that have the factor we can set the individual queuesizes */
    for(device = devicelist; device != NULL; device = device->next) {
        if(device->host == host) {
	    if(SD(device->host)->bustype != IS_ISA){
	        switch(device->type) {
		case TYPE_DISK:
		case TYPE_MOD:
		    device->queue_depth = (TYPE_DISK_QUEUE * factor) / 10;
		    break;
		case TYPE_TAPE:
		    device->queue_depth = (TYPE_TAPE_QUEUE * factor) / 10;
		    break;
		case TYPE_WORM:
		case TYPE_ROM:
	            device->queue_depth = (TYPE_ROM_QUEUE * factor) / 10;
		    break;
		case TYPE_PROCESSOR:
		case TYPE_SCANNER:
		default:
		    device->queue_depth = (TYPE_OTHER_QUEUE * factor) / 10;
		    break;
		}
	    } else /* ISA forces us to limit the queue depth because of the 
		    * bounce buffer memory overhead. I know this is cruel */
	        device->queue_depth = 2; 

	    /* 
	     * It showed that we need to set an upper limit of commands 
             * we can allow to  queue for a single device on the bus. 
	     * If we get above that limit, the broken midlevel SCSI code 
	     * will produce bogus timeouts and aborts en masse. :-(
	     */
	    if(device->queue_depth > UPPER_DEVICE_QUEUE_LIMIT)
		device->queue_depth = UPPER_DEVICE_QUEUE_LIMIT;
	    if(device->queue_depth == 0) 
		device->queue_depth = 1;

	    printk(KERN_INFO "scsi%d: queue depth for target %d on channel %d "
		   "set to %d\n", host->host_no, device->id, device->channel,
		   device->queue_depth);
	}
    }
#endif
}

#if CHECK_BLINK
int check_blink_state(long base)
{
    ushort loops = 10;
    u32 blinkindicator;
    u32 state = 0x12345678;
    u32 oldstate = 0;

    blinkindicator = htonl(0x54504442);
    while ((loops--) && (state != oldstate)) {
	oldstate = state;
	state = inl((uint) base + 1);
    }

    DBG(DBG_BLINK, printk("Did Blink check. Status: %d\n",
	      (state == oldstate) && (state == blinkindicator)));

    if ((state == oldstate) && (state == blinkindicator))
	return(TRUE);
    else
	return (FALSE);
}
#endif

char * get_board_data(u32 base, u32 irq, u32 id)
{
    struct eata_ccb *cp;
    struct eata_sp  *sp;
    static char *buff;
    ulong i;

    cp = (struct eata_ccb *) kmalloc(sizeof(struct eata_ccb),
				     GFP_ATOMIC | GFP_DMA);
				     
    if(cp==NULL)
    	return NULL;
    	
    sp = (struct eata_sp *) kmalloc(sizeof(struct eata_sp), 
					     GFP_ATOMIC | GFP_DMA);
    if(sp==NULL)
    {
        kfree(cp);
        return NULL;
    }				  

    buff = dma_scratch;
 
    memset(cp, 0, sizeof(struct eata_ccb));
    memset(sp, 0, sizeof(struct eata_sp));
    memset(buff, 0, 256);

    cp->DataIn = TRUE;	   
    cp->Interpret = TRUE;   /* Interpret command */
    cp->cp_dispri = TRUE;
    cp->cp_identify = TRUE;
 
    cp->cp_datalen = htonl(56);  
    cp->cp_dataDMA = htonl(virt_to_bus(buff));
    cp->cp_statDMA = htonl(virt_to_bus(sp));
    cp->cp_viraddr = cp;
    
    cp->cp_id = id;
    cp->cp_lun = 0;

    cp->cp_cdb[0] = INQUIRY;
    cp->cp_cdb[1] = 0;
    cp->cp_cdb[2] = 0;
    cp->cp_cdb[3] = 0;
    cp->cp_cdb[4] = 56;
    cp->cp_cdb[5] = 0;

    fake_int_base = (struct eata_register *) base;
    fake_int_result = FALSE;
    fake_int_happened = FALSE;

    eata_send_command((u32) cp, (u32) base, EATA_CMD_DMA_SEND_CP);
    
    i = jiffies + (3 * HZ);
    while (fake_int_happened == FALSE && time_before_eq(jiffies, i)) 
	barrier();
    
    DBG(DBG_INTR3, printk(KERN_DEBUG "fake_int_result: %#x hbastat %#x "
			  "scsistat %#x, buff %p sp %p\n",
			  fake_int_result, (u32) (sp->hba_stat /*& 0x7f*/), 
			  (u32) sp->scsi_stat, buff, sp));

    kfree((void *)cp);
    kfree((void *)sp);
    
    if ((fake_int_result & HA_SERROR) || time_after(jiffies, i)){
	printk(KERN_WARNING "eata_dma: trying to reset HBA at %x to clear "
	       "possible blink state\n", base); 
	/* hard reset the HBA  */
	inb((u32) (base) + HA_RSTATUS);
	eata_send_command(0, base, EATA_CMD_RESET);
	DELAY(1);
	return (NULL);
    } else
	return (buff);
}


int get_conf_PIO(u32 base, struct get_conf *buf)
{
    ulong loop = R_LIMIT;
    u16 *p;

    if(check_region(base, 9)) 
	return (FALSE);
     
    memset(buf, 0, sizeof(struct get_conf));

    while (inb(base + HA_RSTATUS) & HA_SBUSY)
	if (--loop == 0) 
	    return (FALSE);

    fake_int_base = (struct eata_register *) base;
    fake_int_result = FALSE;
    fake_int_happened = FALSE;
       
    DBG(DBG_PIO && DBG_PROBE,
	printk("Issuing PIO READ CONFIG to HBA at %#x\n", base));
    eata_send_command(0, base, EATA_CMD_PIO_READ_CONFIG);

    loop = R_LIMIT;
    for (p = (u16 *) buf; 
	 (long)p <= ((long)buf + (sizeof(struct get_conf) / 2)); p++) {
	while (!(inb(base + HA_RSTATUS) & HA_SDRQ))
	    if (--loop == 0)
		return (FALSE);

	loop = R_LIMIT;
	*p = inw(base + HA_RDATA);
    }

    if (!(inb(base + HA_RSTATUS) & HA_SERROR)) {	    /* Error ? */
	if (htonl(EATA_SIGNATURE) == buf->signature) {
	    DBG(DBG_PIO&&DBG_PROBE, printk("EATA Controller found at %x "
					   "EATA Level: %x\n", (uint) base, 
					   (uint) (buf->version)));
	    
	    while (inb(base + HA_RSTATUS) & HA_SDRQ) 
		inw(base + HA_RDATA);
	    return (TRUE);
	} 
    } else {
	DBG(DBG_PROBE, printk("eata_dma: get_conf_PIO, error during transfer "
		  "for HBA at %lx\n", (long)base));
    }
    return (FALSE);
}


void print_config(struct get_conf *gc)
{
    printk("LEN: %d ver:%d OCS:%d TAR:%d TRNXFR:%d MORES:%d DMAS:%d\n",
	   (u32) ntohl(gc->len), gc->version,
	   gc->OCS_enabled, gc->TAR_support, gc->TRNXFR, gc->MORE_support,
	   gc->DMA_support);
    printk("DMAV:%d HAAV:%d SCSIID0:%d ID1:%d ID2:%d QUEUE:%d SG:%d SEC:%d\n",
	   gc->DMA_valid, gc->HAA_valid, gc->scsi_id[3], gc->scsi_id[2],
	   gc->scsi_id[1], ntohs(gc->queuesiz), ntohs(gc->SGsiz), gc->SECOND);
    printk("IRQ:%d IRQT:%d DMAC:%d FORCADR:%d SG_64K:%d SG_UAE:%d MID:%d "
	   "MCH:%d MLUN:%d\n",
	   gc->IRQ, gc->IRQ_TR, (8 - gc->DMA_channel) & 7, gc->FORCADR, 
	   gc->SG_64K, gc->SG_UAE, gc->MAX_ID, gc->MAX_CHAN, gc->MAX_LUN); 
    printk("RIDQ:%d PCI:%d EISA:%d\n",
	   gc->ID_qest, gc->is_PCI, gc->is_EISA);
    DBG(DPT_DEBUG, DELAY(14));
}

short register_HBA(u32 base, struct get_conf *gc, Scsi_Host_Template * tpnt, 
		   u8 bustype)
{
    ulong size = 0;
    unchar dma_channel = 0;
    char *buff = 0;
    unchar bugs = 0;
    struct Scsi_Host *sh;
    hostdata *hd = NULL;
    int x;
    
    
    DBG(DBG_REGISTER, print_config(gc));

    if (gc->DMA_support == FALSE) {
	printk("The EATA HBA at %#.4x does not support DMA.\n" 
	       "Please use the EATA-PIO driver.\n", base);
	return (FALSE);
    }
    if(gc->HAA_valid == FALSE || ntohl(gc->len) < 0x22) 
	gc->MAX_CHAN = 0;

    if (reg_IRQ[gc->IRQ] == FALSE) {	/* Interrupt already registered ? */
	if (!request_irq(gc->IRQ, (void *) eata_fake_int_handler, SA_INTERRUPT,
			 "eata_dma", NULL)){
	    reg_IRQ[gc->IRQ]++;
	    if (!gc->IRQ_TR)
		reg_IRQL[gc->IRQ] = TRUE;   /* IRQ is edge triggered */
	} else {
	    printk("Couldn't allocate IRQ %d, Sorry.", gc->IRQ);
	    return (FALSE);
	}
    } else {		/* More than one HBA on this IRQ */
	if (reg_IRQL[gc->IRQ] == TRUE) {
	    printk("Can't support more than one HBA on this IRQ,\n"
		   "  if the IRQ is edge triggered. Sorry.\n");
	    return (FALSE);
	} else
	    reg_IRQ[gc->IRQ]++;
    }

 
    /* If DMA is supported but DMA_valid isn't set to indicate that
     * the channel number is given we must have pre 2.0 firmware (1.7?)
     * which leaves us to guess since the "newer ones" also don't set the 
     * DMA_valid bit.
     */
    if (gc->DMA_support && !gc->DMA_valid && gc->DMA_channel) {
      printk(KERN_WARNING "eata_dma: If you are using a pre 2.0 firmware "
	     "please update it !\n"
	     "          You can get new firmware releases from ftp.dpt.com\n");
	gc->DMA_channel = (base == 0x1f0 ? 3 /* DMA=5 */ : 2 /* DMA=6 */);
	gc->DMA_valid = TRUE;
    }

    /* if gc->DMA_valid it must be an ISA HBA and we have to register it */
    dma_channel = BUSMASTER;
    if (gc->DMA_valid) {
	if (request_dma(dma_channel = (8 - gc->DMA_channel) & 7, "eata_dma")) {
	    printk(KERN_WARNING "Unable to allocate DMA channel %d for ISA HBA"
		   " at %#.4x.\n", dma_channel, base);
	    reg_IRQ[gc->IRQ]--;
	    if (reg_IRQ[gc->IRQ] == 0)
		free_irq(gc->IRQ, NULL);
	    if (gc->IRQ_TR == FALSE)
		reg_IRQL[gc->IRQ] = FALSE; 
	    return (FALSE);
	}
    }

    if (dma_channel != BUSMASTER) {
	disable_dma(dma_channel);
	clear_dma_ff(dma_channel);
	set_dma_mode(dma_channel, DMA_MODE_CASCADE);
	enable_dma(dma_channel);
    }

    if (bustype != IS_EISA && bustype != IS_ISA)
	buff = get_board_data(base, gc->IRQ, gc->scsi_id[3]);

    if (buff == NULL) {
	if (bustype == IS_EISA || bustype == IS_ISA) {
	    bugs = bugs || BROKEN_INQUIRY;
	} else {
	    if (gc->DMA_support == FALSE)
		printk(KERN_WARNING "HBA at %#.4x doesn't support DMA. "
		       "Sorry\n", base);
	    else
		printk(KERN_WARNING "HBA at %#.4x does not react on INQUIRY. "
		       "Sorry.\n", base);
	    if (gc->DMA_valid) 
		free_dma(dma_channel);
	    reg_IRQ[gc->IRQ]--;
	    if (reg_IRQ[gc->IRQ] == 0)
		free_irq(gc->IRQ, NULL);
	    if (gc->IRQ_TR == FALSE)
		reg_IRQL[gc->IRQ] = FALSE; 
	    return (FALSE);
	}
    }
 
    if (gc->DMA_support == FALSE && buff != NULL)  
	printk(KERN_WARNING "HBA %.12sat %#.4x doesn't set the DMA_support "
	       "flag correctly.\n", &buff[16], base);
    
    request_region(base, 9, "eata_dma"); /* We already checked the 
					  * availability, so this
					  * should not fail.
					  */
    
    if(ntohs(gc->queuesiz) == 0) {
	gc->queuesiz = ntohs(64);
	printk(KERN_WARNING "Warning: Queue size has to be corrected. Assuming"
	       " 64 queueslots\n"
	       "         This might be a PM2012B with a defective Firmware\n"
	       "         Contact DPT support@dpt.com for an upgrade\n");
    }

    size = sizeof(hostdata) + ((sizeof(struct eata_ccb) + sizeof(long)) 
			       * ntohs(gc->queuesiz));

    DBG(DBG_REGISTER, printk("scsi_register size: %ld\n", size));

    sh = scsi_register(tpnt, size);
    
    if(sh != NULL) {

        hd = SD(sh);		   

	memset(hd->reads, 0, sizeof(u32) * 26); 
	
	sh->select_queue_depths = eata_select_queue_depths;
	
	hd->bustype = bustype;

	/*
	 * If we are using a ISA board, we can't use extended SG,
	 * because we would need excessive amounts of memory for
	 * bounce buffers.
	 */
	if (gc->SG_64K==TRUE && ntohs(gc->SGsiz)==64 && hd->bustype!=IS_ISA){
	    sh->sg_tablesize = SG_SIZE_BIG;
	} else {
	    sh->sg_tablesize = ntohs(gc->SGsiz);
	    if (sh->sg_tablesize > SG_SIZE || sh->sg_tablesize == 0) {
	        if (sh->sg_tablesize == 0)
		    printk(KERN_WARNING "Warning: SG size had to be fixed.\n"
			   "This might be a PM2012 with a defective Firmware"
			   "\nContact DPT support@dpt.com for an upgrade\n");
		sh->sg_tablesize = SG_SIZE;
	    }
	}
	hd->sgsize = sh->sg_tablesize;
    }

    if(sh != NULL) {
        sh->can_queue = hd->queuesize = ntohs(gc->queuesiz);
       	sh->cmd_per_lun = 0;
    }

    if(sh == NULL) { 
        DBG(DBG_REGISTER, printk(KERN_NOTICE "eata_dma: couldn't register HBA"
				 " at%x \n", base));
	scsi_unregister(sh);
	if (gc->DMA_valid) 
	    free_dma(dma_channel);
	
	reg_IRQ[gc->IRQ]--;
	if (reg_IRQ[gc->IRQ] == 0)
	    free_irq(gc->IRQ, NULL);
	if (gc->IRQ_TR == FALSE)
	    reg_IRQL[gc->IRQ] = FALSE; 
	return (FALSE);
    }

    
    hd->broken_INQUIRY = (bugs & BROKEN_INQUIRY);

    if(hd->broken_INQUIRY == TRUE) {
	strcpy(hd->vendor, "DPT");
	strcpy(hd->name, "??????????");
	strcpy(hd->revision, "???.?");
        hd->firmware_revision = 0;
    } else {	
	strncpy(hd->vendor, &buff[8], 8);
	hd->vendor[8] = 0;
	strncpy(hd->name, &buff[16], 17);
	hd->name[17] = 0;
	hd->revision[0] = buff[32];
	hd->revision[1] = buff[33];
	hd->revision[2] = buff[34];
	hd->revision[3] = '.';
	hd->revision[4] = buff[35];
	hd->revision[5] = 0;
        hd->firmware_revision = (buff[32] << 24) + (buff[33] << 16) 
	                            + (buff[34] << 8) + buff[35]; 
    }

    if (hd->firmware_revision >= (('0'<<24) + ('7'<<16) + ('G'<< 8) + '0'))
        hd->immediate_support = 1;
    else 
        hd->immediate_support = 0;

    switch (ntohl(gc->len)) {
    case 0x1c:
	hd->EATA_revision = 'a';
	break;
    case 0x1e:
	hd->EATA_revision = 'b';
	break;
    case 0x22:
	hd->EATA_revision = 'c';
	break;
    case 0x24:
	hd->EATA_revision = 'z';		
    default:
	hd->EATA_revision = '?';
    }


    if(ntohl(gc->len) >= 0x22) {
	sh->max_id = gc->MAX_ID + 1;
	sh->max_lun = gc->MAX_LUN + 1;
    } else {
	sh->max_id = 8;
	sh->max_lun = 8;
    }

    hd->HBA_number = sh->host_no;
    hd->channel = gc->MAX_CHAN;	    
    sh->max_channel = gc->MAX_CHAN; 
    sh->unique_id = base;
    sh->base = base;
    sh->io_port = base;
    sh->n_io_port = 9;
    sh->irq = gc->IRQ;
    sh->dma_channel = dma_channel;
    
    /* FIXME:
     * SCSI midlevel code should support different HBA ids on every channel
     */
    sh->this_id = gc->scsi_id[3];
    
    if (gc->SECOND)
	hd->primary = FALSE;
    else
	hd->primary = TRUE;
    
    if (hd->bustype != IS_ISA) {
	sh->unchecked_isa_dma = FALSE;
    } else {
	sh->unchecked_isa_dma = TRUE;	/* We're doing ISA DMA */
    }
    
    for(x = 0; x <= 11; x++){		 /* Initialize min. latency */
	hd->writes_lat[x][1] = 0xffffffff;
	hd->reads_lat[x][1] = 0xffffffff;
    }
    hd->all_lat[1] = 0xffffffff;

    hd->next = NULL;	/* build a linked list of all HBAs */
    hd->prev = last_HBA;
    if(hd->prev != NULL)
	SD(hd->prev)->next = sh;
    last_HBA = sh;
    if (first_HBA == NULL)
	first_HBA = sh;
    registered_HBAs++;
    
    return (TRUE);
}



void find_EISA(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
    u32 base;
    int i;
    
#if CHECKPAL
    u8 pal1, pal2, pal3;
#endif
    
    for (i = 0; i < MAXEISA; i++) {
	if (EISAbases[i] == TRUE) { /* Still a possibility ?	      */
	    
	    base = 0x1c88 + (i * 0x1000);
#if CHECKPAL
	    pal1 = inb((u16)base - 8);
	    pal2 = inb((u16)base - 7);
	    pal3 = inb((u16)base - 6);
	    
	    if (((pal1 == DPT_ID1) && (pal2 == DPT_ID2)) ||
		((pal1 == NEC_ID1) && (pal2 == NEC_ID2) && (pal3 == NEC_ID3))||
		((pal1 == ATT_ID1) && (pal2 == ATT_ID2) && (pal3 == ATT_ID3))){
		DBG(DBG_PROBE, printk("EISA EATA id tags found: %x %x %x \n",
				      (int)pal1, (int)pal2, (int)pal3));
#endif
		if (get_conf_PIO(base, buf) == TRUE) {
		    if (buf->IRQ) {  
			DBG(DBG_EISA, printk("Registering EISA HBA\n"));
			register_HBA(base, buf, tpnt, IS_EISA);
		    } else
			printk("eata_dma: No valid IRQ. HBA removed from list\n");
		}
#if CHECK_BLINK
		else {
		    if (check_blink_state(base)) 
			printk("HBA is in BLINK state. Consult your HBAs "
			       "Manual to correct this.\n");
		} 
#endif
		/* Nothing found here so we take it from the list */
		EISAbases[i] = 0;  
#if CHECKPAL
	    } 
#endif
	}
    }
    return; 
}

void find_ISA(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
    int i;
    
    for (i = 0; i < MAXISA; i++) {  
	if (ISAbases[i]) {  
	    if (get_conf_PIO(ISAbases[i],buf) == TRUE){
		DBG(DBG_ISA, printk("Registering ISA HBA\n"));
		register_HBA(ISAbases[i], buf, tpnt, IS_ISA);
	    } 
#if CHECK_BLINK
	    else {
		if (check_blink_state(ISAbases[i])) 
		    printk("HBA is in BLINK state. Consult your HBAs "
			   "Manual to correct this.\n");
	    }
#endif
	    ISAbases[i] = 0;
	}
    }
    return;
}

void find_PCI(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
#ifndef CONFIG_PCI
    printk("eata_dma: kernel PCI support not enabled. Skipping scan for PCI HBAs.\n");
#else
    struct pci_dev *dev = NULL; 
    u32 base, x;
    u8 pal1, pal2, pal3;

    while ((dev = pci_find_device(PCI_VENDOR_ID_DPT, PCI_DEVICE_ID_DPT, dev)) != NULL) {
	    DBG(DBG_PROBE && DBG_PCI, 
		printk("eata_dma: find_PCI, HBA at %s\n", dev->name));
	    if (pci_enable_device(dev))
	    	continue;
	    pci_set_master(dev);
	    base = pci_resource_flags(dev, 0);
	    if (base & IORESOURCE_MEM) {
		printk("eata_dma: invalid base address of device %s\n", dev->name);
		continue;
	    }
	    base = pci_resource_start(dev, 0);
            /* EISA tag there ? */
	    pal1 = inb(base);
	    pal2 = inb(base + 1);
	    pal3 = inb(base + 2);
	    if (((pal1 == DPT_ID1) && (pal2 == DPT_ID2)) ||
		((pal1 == NEC_ID1) && (pal2 == NEC_ID2) && 
		(pal3 == NEC_ID3)) ||
		((pal1 == ATT_ID1) && (pal2 == ATT_ID2) && 
		(pal3 == ATT_ID3)))
		base += 0x08;
	    else
		base += 0x10;   /* Now, THIS is the real address */
	    if (base != 0x1f8) {
		/* We didn't find it in the primary search */
		if (get_conf_PIO(base, buf) == TRUE) {
		    /* OK. We made it till here, so we can go now  
		     * and register it. We only have to check and 
		     * eventually remove it from the EISA and ISA list 
		     */
		    DBG(DBG_PCI, printk("Registering PCI HBA\n"));
		    register_HBA(base, buf, tpnt, IS_PCI);
		    
		    if (base < 0x1000) {
			for (x = 0; x < MAXISA; ++x) {
			    if (ISAbases[x] == base) {
				ISAbases[x] = 0;
				break;
			    }
			}
		    } else if ((base & 0x0fff) == 0x0c88) 
			EISAbases[(base >> 12) & 0x0f] = 0;
		} 
#if CHECK_BLINK
		else if (check_blink_state(base) == TRUE) {
		    printk("eata_dma: HBA is in BLINK state.\n"
			   "Consult your HBAs manual to correct this.\n");
		}
#endif
	    }
	}
#endif /* #ifndef CONFIG_PCI */
}

int eata_detect(Scsi_Host_Template * tpnt)
{
    struct Scsi_Host *HBA_ptr;
    struct get_conf gc;
    int i;
    
    DBG((DBG_PROBE && DBG_DELAY) || DPT_DEBUG,
	printk("Using lots of delays to let you read the debugging output\n"));

    tpnt->proc_name = "eata_dma";

    status = kmalloc(512, GFP_ATOMIC | GFP_DMA);
    dma_scratch = kmalloc(1024, GFP_ATOMIC | GFP_DMA);

    if(status == NULL || dma_scratch == NULL) {
	printk("eata_dma: can't allocate enough memory to probe for hosts !\n");
	if(status)
		kfree(status);
	if(dma_scratch)
		kfree(dma_scratch);
	return(0);
    }

    dma_scratch += 4;

    find_PCI(&gc, tpnt);
    
    find_EISA(&gc, tpnt);
    
    find_ISA(&gc, tpnt);
    
    for (i = 0; i <= MAXIRQ; i++) { /* Now that we know what we have, we     */
	if (reg_IRQ[i] >= 1){       /* exchange the interrupt handler which  */
	    free_irq(i, NULL);      /* we used for probing with the real one */
	    request_irq(i, (void *)(do_eata_int_handler), SA_INTERRUPT|SA_SHIRQ, 
			"eata_dma", NULL);
	}
    }

    HBA_ptr = first_HBA;

    if (registered_HBAs != 0) {
        printk("EATA (Extended Attachment) driver version: %d.%d%s"
               "\ndeveloped in co-operation with DPT\n"
               "(c) 1993-96 Michael Neuffer, mike@i-Connect.Net\n",
               VER_MAJOR, VER_MINOR, VER_SUB);
        printk("Registered HBAs:");
        printk("\nHBA no. Boardtype    Revis  EATA Bus  BaseIO IRQ"
               " DMA Ch ID Pr QS  S/G IS\n");
        for (i = 1; i <= registered_HBAs; i++) {
    	    printk("scsi%-2d: %.12s v%s 2.0%c %s %#.4x  %2d",
		   HBA_ptr->host_no, SD(HBA_ptr)->name, SD(HBA_ptr)->revision,
		   SD(HBA_ptr)->EATA_revision, (SD(HBA_ptr)->bustype == 'P')? 
		   "PCI ":(SD(HBA_ptr)->bustype == 'E')?"EISA":"ISA ",
		   (u32) HBA_ptr->base, HBA_ptr->irq);
	    if(HBA_ptr->dma_channel != BUSMASTER)
		printk("  %2x ", HBA_ptr->dma_channel);
	    else
		printk(" %s", "BMST");
	    printk(" %d  %d  %c %3d %3d %c\n", 
		   SD(HBA_ptr)->channel+1, HBA_ptr->this_id, 
		   (SD(HBA_ptr)->primary == TRUE)?'Y':'N', 
		   HBA_ptr->can_queue, HBA_ptr->sg_tablesize, 
		   (SD(HBA_ptr)->immediate_support == TRUE)?'Y':'N'); 
	    HBA_ptr = SD(HBA_ptr)->next;
	}
    } else {
	kfree((void *)status);
    }

    kfree((void *)dma_scratch - 4);

    DBG(DPT_DEBUG, DELAY(12));

    return(registered_HBAs);
}

MODULE_LICENSE("GPL");

/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = EATA_DMA;
#include "scsi_module.c"

/*
 * Overrides for Emacs so that we almost follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * tab-width: 8
 * End:
 */
