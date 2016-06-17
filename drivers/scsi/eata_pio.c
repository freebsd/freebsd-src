/************************************************************
 *                                                          *
 *               Linux EATA SCSI PIO driver                 *
 *                                                          *
 *  based on the CAM document CAM/89-004 rev. 2.0c,         *
 *  DPT's driver kit, some internal documents and source,   *
 *  and several other Linux scsi drivers and kernel docs.   *
 *                                                          *
 *  The driver currently:                                   *
 *      -supports all EATA-PIO boards                       *
 *      -only supports DASD devices                         *
 *                                                          *
 *  (c)1993-96 Michael Neuffer, Alfred Arnold               *
 *             neuffer@goofy.zdv.uni-mainz.de               *
 *             a.arnold@kfa-juelich.de                      * 
 *                                                          *
 *  This program is free software; you can redistribute it  *
 *  and/or modify it under the terms of the GNU General     *
 *  Public License as published by the Free Software        *
 *  Foundation; either version 2 of the License, or         *
 *  (at your option) any later version.                     *
 *                                                          *
 *  This program is distributed in the hope that it will be *
 *  useful, but WITHOUT ANY WARRANTY; without even the      *
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A    *
 *  PARTICULAR PURPOSE.  See the GNU General Public License *
 *  for more details.                                       *
 *                                                          *
 *  You should have received a copy of the GNU General      *
 *  Public License along with this kernel; if not, write to *
 *  the Free Software Foundation, Inc., 675 Mass Ave,       *
 *  Cambridge, MA 02139, USA.                               *
 *                                                          *
 ************************************************************
 *  last change: 96/07/16                  OS: Linux 2.0.8  *
 ************************************************************/

/* Look in eata_pio.h for configuration information */

#include <linux/module.h>
 
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/in.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include "eata_pio.h"
#include "eata_dma_proc.h"
#include "scsi.h"
#include "sd.h"

#include <linux/stat.h>
#include <linux/config.h>	/* for CONFIG_PCI */
#include <linux/blk.h>
#include <linux/spinlock.h>

static uint ISAbases[MAXISA] =
{0x1F0, 0x170, 0x330, 0x230};
static uint ISAirqs[MAXISA] =
{14,12,15,11};
static unchar EISAbases[] =
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static uint registered_HBAs = 0;
static struct Scsi_Host *last_HBA = NULL;
static struct Scsi_Host *first_HBA = NULL;
static unchar reg_IRQ[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static unchar reg_IRQL[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static ulong int_counter = 0;
static ulong queue_counter = 0;

#include "eata_pio_proc.c"
 
#ifdef MODULE
int eata_pio_release(struct Scsi_Host *sh)
{
    if (sh->irq && reg_IRQ[sh->irq] == 1) free_irq(sh->irq, NULL);
    else reg_IRQ[sh->irq]--;
    if (SD(sh)->channel == 0) {
	if (sh->io_port && sh->n_io_port)
	    release_region(sh->io_port, sh->n_io_port);
    }
    return(TRUE);
}
#endif

void IncStat(Scsi_Pointer *SCp, uint Increment)
{
    SCp->ptr+=Increment; 
    if ((SCp->this_residual-=Increment)==0)
    {
	if ((--SCp->buffers_residual)==0) SCp->Status=FALSE;
	else
	{
	    SCp->buffer++;
	    SCp->ptr=SCp->buffer->address;
	    SCp->this_residual=SCp->buffer->length;
	}
    }
}

void eata_pio_int_handler(int irq, void *dev_id, struct pt_regs * regs);

void do_eata_pio_int_handler(int irq, void *dev_id, struct pt_regs * regs)
{
    unsigned long flags;

    spin_lock_irqsave(&io_request_lock, flags);
    eata_pio_int_handler(irq, dev_id, regs);
    spin_unlock_irqrestore(&io_request_lock, flags);
}

void eata_pio_int_handler(int irq, void *dev_id, struct pt_regs * regs)
{
    uint eata_stat = 0xfffff;
    Scsi_Cmnd *cmd;
    hostdata *hd;
    struct eata_ccb *cp;
    uint base;
    ulong flags;
    uint x,z;
    struct Scsi_Host *sh;
    ushort zwickel=0;
    unchar stat,odd;
    
    save_flags(flags);
    cli();
    
    for (x = 1, sh = first_HBA; x <= registered_HBAs; x++, sh = SD(sh)->prev) {
	if (sh->irq != irq)
	    continue;
	if (inb((uint)sh->base + HA_RSTATUS) & HA_SBUSY)
	    continue;
	
	int_counter++;
	
	hd=SD(sh);
	
	cp = &hd->ccb[0];
	cmd = cp->cmd;
	base = (uint) cmd->host->base;
	
	do
	{
	    stat=inb(base+HA_RSTATUS);
	    if (stat&HA_SDRQ) {
		if (cp->DataIn)
		{
		    z=256; odd=FALSE;
		    while ((cmd->SCp.Status)&&((z>0)||(odd)))
		    {
			if (odd) 
			{ 
			    *(cmd->SCp.ptr)=zwickel>>8; 
			    IncStat(&cmd->SCp,1);
			    odd=FALSE;
			}
			x=min_t(unsigned int,z,cmd->SCp.this_residual/2);
			insw(base+HA_RDATA,cmd->SCp.ptr,x);
			z-=x; 
			IncStat(&cmd->SCp,2*x);
			if ((z>0)&&(cmd->SCp.this_residual==1))
			{
			    zwickel=inw(base+HA_RDATA); 
			    *(cmd->SCp.ptr)=zwickel&0xff;
			    IncStat(&cmd->SCp,1); z--; 
			    odd=TRUE;
			}
		    }
		    while (z>0) {
			zwickel=inw(base+HA_RDATA); 
			z--;
		    } 
		}
		else /* cp->DataOut */
		{
		    odd=FALSE; z=256;
		    while ((cmd->SCp.Status)&&((z>0)||(odd)))
		    {
			if (odd)
			{
			    zwickel+=*(cmd->SCp.ptr)<<8; 
			    IncStat(&cmd->SCp,1);
			    outw(zwickel,base+HA_RDATA); 
			    z--; 
			    odd=FALSE; 
			}
			x=min_t(unsigned int,z,cmd->SCp.this_residual/2);
			outsw(base+HA_RDATA,cmd->SCp.ptr,x);
			z-=x; 
			IncStat(&cmd->SCp,2*x);
			if ((z>0)&&(cmd->SCp.this_residual==1))
			{
			    zwickel=*(cmd->SCp.ptr); 
			    zwickel&=0xff;
			    IncStat(&cmd->SCp,1); 
			    odd=TRUE;
			}  
		    }
		    while (z>0||odd) {
			outw(zwickel,base+HA_RDATA); 
			z--; 
			odd=FALSE;
		    }
		}
	    }
	}
	while ((stat&HA_SDRQ)||((stat&HA_SMORE)&&hd->moresupport));
	
	/* terminate handler if HBA goes busy again, i.e. transfers
	 * more data */
	
	if (stat&HA_SBUSY) break;
	
	/* OK, this is quite stupid, but I haven't found any correct
	 * way to get HBA&SCSI status so far */
	
	if (!(inb(base+HA_RSTATUS)&HA_SERROR))
	{
	    cmd->result=(DID_OK<<16); 
	    hd->devflags|=(1<<cp->cp_id);
	}
	else if (hd->devflags&1<<cp->cp_id) 
	    cmd->result=(DID_OK<<16)+0x02;
	else cmd->result=(DID_NO_CONNECT<<16);
	
	if (cp->status == LOCKED) {
	    cp->status = FREE;
	    eata_stat = inb(base + HA_RSTATUS);
	    printk(KERN_NOTICE "eata_pio: int_handler, freeing locked "
                   "queueslot\n");
	    DBG(DBG_INTR&&DBG_DELAY,DELAY(1));
	    restore_flags(flags);
	    return;
	}
	
#if DBG_INTR2
	if (stat != 0x50) 
	    printk(KERN_DEBUG "stat: %#.2x, result: %#.8x\n", stat, 
                   cmd->result); 
	DBG(DBG_INTR&&DBG_DELAY,DELAY(1));
#endif
	
	cp->status = FREE;   /* now we can release the slot  */
	
	restore_flags(flags);
	cmd->scsi_done(cmd);
	save_flags(flags);
	cli();
    }
    restore_flags(flags);
    
    return;
}

inline uint eata_pio_send_command(uint base, unchar command)
{
    uint loop = HZ/2;
    
    while (inb(base + HA_RSTATUS) & HA_SBUSY)
	if (--loop == 0)
	    return(TRUE);

    /* Enable interrupts for HBA.  It is not the best way to do it at this
     * place, but I hope that it doesn't interfere with the IDE driver 
     * initialization this way */

    outb(HA_CTRL_8HEADS,base+HA_CTRLREG);
    
    outb(command, base + HA_WCOMMAND);
    return(FALSE);
}

int eata_pio_queue(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
    uint x, y;
    long flags;
    uint base;
    
    hostdata *hd;
    struct Scsi_Host *sh;
    struct eata_ccb *cp;
    
    save_flags(flags);
    cli();
    
    queue_counter++;
    
    hd = HD(cmd);
    sh = cmd->host;
    base = (uint) sh->base;
    
    /* use only slot 0, as 2001 can handle only one cmd at a time */
    
    y = x = 0;
    
    if (hd->ccb[y].status!=FREE) { 
	
	DBG(DBG_QUEUE, printk(KERN_EMERG "can_queue %d, x %d, y %d\n",
                              sh->can_queue,x,y));
#if DEBUG_EATA
	panic(KERN_EMERG "eata_pio: run out of queue slots cmdno:%ld "
              "intrno: %ld\n", queue_counter, int_counter);
#else
	panic(KERN_EMERG "eata_pio: run out of queue slots....\n");
#endif
    }
    
    cp = &hd->ccb[y];
    
    memset(cp, 0, sizeof(struct eata_ccb));
    memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
	
    cp->status = USED;      /* claim free slot */

    DBG(DBG_QUEUE, printk(KERN_DEBUG "eata_pio_queue pid %ld, target: %x, lun:"
                          " %x, y %d\n", cmd->pid, cmd->target, cmd->lun, y));
    DBG(DBG_QUEUE && DBG_DELAY, DELAY(1));
    
    cmd->scsi_done = (void *)done;
    
    switch (cmd->cmnd[0]) {
    case CHANGE_DEFINITION: case COMPARE:         case COPY:
    case COPY_VERIFY:       case LOG_SELECT:      case MODE_SELECT:
    case MODE_SELECT_10:    case SEND_DIAGNOSTIC: case WRITE_BUFFER:
    case FORMAT_UNIT:       case REASSIGN_BLOCKS: case RESERVE:
    case SEARCH_EQUAL:      case SEARCH_HIGH:     case SEARCH_LOW:
    case WRITE_6:           case WRITE_10:        case WRITE_VERIFY:
    case UPDATE_BLOCK:      case WRITE_LONG:      case WRITE_SAME:      
    case SEARCH_HIGH_12:    case SEARCH_EQUAL_12: case SEARCH_LOW_12:
    case WRITE_12:          case WRITE_VERIFY_12: case SET_WINDOW: 
    case MEDIUM_SCAN:       case SEND_VOLUME_TAG:            
    case 0xea:      /* alternate number for WRITE LONG */
	cp->DataOut = TRUE; /* Output mode */
	break;
    case TEST_UNIT_READY:
    default:
	cp->DataIn = TRUE;  /* Input mode  */
    }
    
    cp->Interpret = (cmd->target == hd->hostid);
    cp->cp_datalen = htonl((ulong)cmd->request_bufflen);
    cp->Auto_Req_Sen = FALSE;
    cp->cp_reqDMA = htonl(0);
    cp->reqlen = 0;
    
    cp->cp_id = cmd->target;
    cp->cp_lun = cmd->lun;
    cp->cp_dispri = FALSE;
    cp->cp_identify = TRUE;
    memcpy(cp->cp_cdb, cmd->cmnd, COMMAND_SIZE(*cmd->cmnd));
    
    cp->cp_statDMA = htonl(0);
    
    cp->cp_viraddr = cp;
    cp->cmd = cmd;
    cmd->host_scribble = (char *)&hd->ccb[y];   
    
    if (cmd->use_sg == 0)
    { 
	cmd->SCp.buffers_residual=1;
	cmd->SCp.ptr = cmd->request_buffer;
	cmd->SCp.this_residual = cmd->request_bufflen;
	cmd->SCp.buffer = NULL;
    } else {
	cmd->SCp.buffer = cmd->request_buffer;
	cmd->SCp.buffers_residual = cmd->use_sg;
	cmd->SCp.ptr = cmd->SCp.buffer->address;
	cmd->SCp.this_residual = cmd->SCp.buffer->length;
    }
    cmd->SCp.Status = (cmd->SCp.this_residual != 0);  /* TRUE as long as bytes 
                                                       * are to transfer */ 
    
    if (eata_pio_send_command(base, EATA_CMD_PIO_SEND_CP)) 
    {
	cmd->result = DID_BUS_BUSY << 16;
	printk(KERN_NOTICE "eata_pio_queue target %d, pid %ld, HBA busy, "
               "returning DID_BUS_BUSY, done.\n", cmd->target, cmd->pid);
        done(cmd);
        cp->status = FREE;      
        restore_flags(flags);
	return (0);
    }
    while (!(inb(base + HA_RSTATUS) & HA_SDRQ));
    outsw(base + HA_RDATA, cp, hd->cplen);
    outb(EATA_CMD_PIO_TRUNC, base + HA_WCOMMAND);
    for (x = 0; x < hd->cppadlen; x++) outw(0, base + HA_RDATA);
    
    DBG(DBG_QUEUE,printk(KERN_DEBUG "Queued base %#.4lx pid: %ld target: %x "
                         "lun: %x slot %d irq %d\n", (long)sh->base, cmd->pid, 
			 cmd->target, cmd->lun, y, sh->irq));
    DBG(DBG_QUEUE && DBG_DELAY, DELAY(1));
    
    restore_flags(flags);
    return (0);
}

int eata_pio_abort(Scsi_Cmnd * cmd)
{
    ulong flags;
    uint loop = HZ;
    
    save_flags(flags);
    cli();
    
    DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio_abort called pid: %ld "
                           "target: %x lun: %x reason %x\n", cmd->pid, 
                           cmd->target, cmd->lun, cmd->abort_reason));
    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
    
    
    while (inb((uint)(cmd->host->base) + HA_RAUXSTAT) & HA_ABUSY)
	if (--loop == 0) {
	    printk(KERN_WARNING "eata_pio: abort, timeout error.\n");
	    restore_flags(flags);
	    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	    return (SCSI_ABORT_ERROR);
	}
    if (CD(cmd)->status == FREE) {
	DBG(DBG_ABNORM, printk(KERN_WARNING "Returning: SCSI_ABORT_NOT_RUNNING\n")); 
	restore_flags(flags);
	return (SCSI_ABORT_NOT_RUNNING);
    }
    if (CD(cmd)->status == USED) {
	DBG(DBG_ABNORM, printk(KERN_WARNING "Returning: SCSI_ABORT_BUSY\n"));
	restore_flags(flags);
	return (SCSI_ABORT_BUSY);  /* SNOOZE */ 
    }
    if (CD(cmd)->status == RESET) {
	restore_flags(flags);
	printk(KERN_WARNING "eata_pio: abort, command reset error.\n");
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_ABORT_ERROR);
    }
    if (CD(cmd)->status == LOCKED) {
	restore_flags(flags);
	DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio: abort, queue slot "
                               "locked.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_ABORT_NOT_RUNNING);
    }
    restore_flags(flags);
    panic("eata_pio: abort: invalid slot status\n");
}

int eata_pio_reset(Scsi_Cmnd * cmd, unsigned int dummy)
{
    uint x, time, limit = 0;
    ulong flags;
    unchar success = FALSE;
    Scsi_Cmnd *sp; 
    
    save_flags(flags);
    cli();
    DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio_reset called pid:%ld target:"
                           " %x lun: %x reason %x\n", cmd->pid, cmd->target, 
                           cmd->lun, cmd->abort_reason));

    if (HD(cmd)->state == RESET) {
	printk(KERN_WARNING "eata_pio_reset: exit, already in reset.\n");
	restore_flags(flags);
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_RESET_ERROR);
    }
    
    /* force all slots to be free */
    
    for (x = 0; x < cmd->host->can_queue; x++) {
	
	if (HD(cmd)->ccb[x].status == FREE) 
	    continue;
	
	sp = HD(cmd)->ccb[x].cmd;
	HD(cmd)->ccb[x].status = RESET;
	printk(KERN_WARNING "eata_pio_reset: slot %d in reset, pid %ld.\n", x,
               sp->pid);
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	
	if (sp == NULL)
	    panic("eata_pio_reset: slot %d, sp==NULL.\n", x);
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
    }
    
    /* hard reset the HBA  */
    outb(EATA_CMD_RESET, (uint) cmd->host->base+HA_WCOMMAND);
    
    DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio_reset: board reset done.\n"));
    HD(cmd)->state = RESET;
    
    time = jiffies;
    while (time_before(jiffies, time + 3 * HZ) && limit++ < 10000000);
    
    DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio_reset: interrupts disabled, "
                           "loops %d.\n", limit));
    DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
    
    for (x = 0; x < cmd->host->can_queue; x++) {
	
	/* Skip slots already set free by interrupt */
	if (HD(cmd)->ccb[x].status != RESET)
	    continue;
	
	sp = HD(cmd)->ccb[x].cmd;
	sp->result = DID_RESET << 16;
	
	/* This mailbox is terminated */
	printk(KERN_WARNING "eata_pio_reset: reset ccb %d.\n",x);
	HD(cmd)->ccb[x].status = FREE;
	
	restore_flags(flags);
	sp->scsi_done(sp);
	cli();
    }
    
    HD(cmd)->state = FALSE;
    restore_flags(flags);
    
    if (success) { /* hmmm... */
	DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio_reset: exit, success.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_RESET_SUCCESS);
    } else {
	DBG(DBG_ABNORM, printk(KERN_WARNING "eata_pio_reset: exit, wakeup.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DELAY(1));
	return (SCSI_RESET_PUNT);
    }
}

char * get_pio_board_data(ulong base, uint irq, uint id, ulong cplen, ushort cppadlen)
{
    struct eata_ccb cp;
    static char buff[256];
    int z;
    
    memset(&cp, 0, sizeof(struct eata_ccb));
    memset(buff, 0, sizeof(buff));
    
    cp.DataIn = TRUE;     
    cp.Interpret = TRUE;   /* Interpret command */
    
    cp.cp_datalen = htonl(254);  
    cp.cp_dataDMA = htonl(0);
    
    cp.cp_id = id;
    cp.cp_lun = 0;
    
    cp.cp_cdb[0] = INQUIRY;
    cp.cp_cdb[1] = 0;
    cp.cp_cdb[2] = 0;
    cp.cp_cdb[3] = 0;
    cp.cp_cdb[4] = 254;
    cp.cp_cdb[5] = 0;
    
    if (eata_pio_send_command((uint) base, EATA_CMD_PIO_SEND_CP)) 
        return (NULL);
    while (!(inb(base + HA_RSTATUS) & HA_SDRQ));
    outsw(base + HA_RDATA, &cp, cplen);
    outb(EATA_CMD_PIO_TRUNC, base + HA_WCOMMAND);
    for (z = 0; z < cppadlen; z++) outw(0, base + HA_RDATA);
    
    while (inb(base + HA_RSTATUS) & HA_SBUSY);
    if (inb(base + HA_RSTATUS) & HA_SERROR)
	return (NULL);
    else if (!(inb(base + HA_RSTATUS) & HA_SDRQ))
	return (NULL);
    else
    {
	insw(base+HA_RDATA, &buff, 127);
	while (inb(base + HA_RSTATUS)&HA_SDRQ) inw(base + HA_RDATA);
	return (buff);
    }
}

int get_pio_conf_PIO(u32 base, struct get_conf *buf)
{
    ulong loop = HZ/2;
    int z;
    ushort *p;
    
    if(check_region(base, 9))  
	return (FALSE);
    
    memset(buf, 0, sizeof(struct get_conf));
    
    while (inb(base + HA_RSTATUS) & HA_SBUSY)
	if (--loop == 0) 
	    return (FALSE);
    
    DBG(DBG_PIO && DBG_PROBE,
	printk(KERN_DEBUG "Issuing PIO READ CONFIG to HBA at %#x\n", base));
    eata_pio_send_command(base, EATA_CMD_PIO_READ_CONFIG);

    loop = HZ/2;
    for (p = (ushort *) buf; 
	 (long)p <= ((long)buf + (sizeof(struct get_conf) / 2)); p++) {
	while (!(inb(base + HA_RSTATUS) & HA_SDRQ))
	    if (--loop == 0)
		return (FALSE);

	loop = HZ/2;
	*p = inw(base + HA_RDATA);
    }
    if (!(inb(base + HA_RSTATUS) & HA_SERROR)) {            /* Error ? */
	if (htonl(EATA_SIGNATURE) == buf->signature) {
	    DBG(DBG_PIO&&DBG_PROBE, printk(KERN_NOTICE "EATA Controller found "
                                           "at %#4x EATA Level: %x\n", base, 
					   (uint) (buf->version)));
	    
	    while (inb(base + HA_RSTATUS) & HA_SDRQ) 
		inw(base + HA_RDATA);
	    if(ALLOW_DMA_BOARDS == FALSE) {
		for (z = 0; z < MAXISA; z++)
		    if (base == ISAbases[z]) {
			buf->IRQ = ISAirqs[z]; 
			break;
		    }
	    }
	    return (TRUE);
	} 
    } else {
	DBG(DBG_PROBE, printk("eata_dma: get_conf_PIO, error during transfer "
			      "for HBA at %x\n", base));
    }
    return (FALSE);
}

void print_pio_config(struct get_conf *gc)
{
    printk("Please check values: (read config data)\n");
    printk("LEN: %d ver:%d OCS:%d TAR:%d TRNXFR:%d MORES:%d\n",
	   (uint) ntohl(gc->len), gc->version,
	   gc->OCS_enabled, gc->TAR_support, gc->TRNXFR, gc->MORE_support);
    printk("HAAV:%d SCSIID0:%d ID1:%d ID2:%d QUEUE:%d SG:%d SEC:%d\n",
	   gc->HAA_valid, gc->scsi_id[3], gc->scsi_id[2],
	   gc->scsi_id[1], ntohs(gc->queuesiz), ntohs(gc->SGsiz), gc->SECOND);
    printk("IRQ:%d IRQT:%d FORCADR:%d MCH:%d RIDQ:%d\n",
	   gc->IRQ, gc->IRQ_TR, gc->FORCADR, 
	   gc->MAX_CHAN, gc->ID_qest);
    DBG(DPT_DEBUG, DELAY(14));
}

static uint print_selftest(uint base)
{
    unchar buffer[512];
#ifdef VERBOSE_SETUP
    int z;
#endif
    
    printk("eata_pio: executing controller self test & setup...\n");
    while (inb(base + HA_RSTATUS) & HA_SBUSY);
    outb(EATA_CMD_PIO_SETUPTEST, base + HA_WCOMMAND);
    do {
	while (inb(base + HA_RSTATUS) & HA_SBUSY)
	    /* nothing */ ;
	if (inb(base + HA_RSTATUS) & HA_SDRQ)
	{
	    insw(base + HA_RDATA, &buffer, 256);
#ifdef VERBOSE_SETUP
	    /* no beeps please... */
	    for (z = 0; z < 511 && buffer[z]; z++)
		if (buffer[z] != 7) printk("%c", buffer[z]);
#endif
	}
    } while (inb(base+HA_RSTATUS) & (HA_SBUSY|HA_SDRQ));
    
    return (!(inb(base+HA_RSTATUS) & HA_SERROR)); 
}

int register_pio_HBA(long base, struct get_conf *gc, Scsi_Host_Template * tpnt)
{
    ulong size = 0;
    char *buff;
    ulong cplen;
    ushort cppadlen;
    struct Scsi_Host *sh;
    hostdata *hd;
    
    DBG(DBG_REGISTER, print_pio_config(gc));
    
    if (gc->DMA_support == TRUE) {
	printk("HBA at %#.4lx supports DMA. Please use EATA-DMA driver.\n",base);
	if(ALLOW_DMA_BOARDS == FALSE)
	    return (FALSE);
    }
    
    if ((buff = get_pio_board_data((uint)base, gc->IRQ, gc->scsi_id[3], 
			       cplen   =(htonl(gc->cplen   )+1)/2, 
			       cppadlen=(htons(gc->cppadlen)+1)/2)) == NULL)
    {
	printk("HBA at %#lx didn't react on INQUIRY. Sorry.\n", (ulong) base);
	return (FALSE);
    }
    
    if (print_selftest(base) == FALSE && ALLOW_DMA_BOARDS == FALSE)
    {
	printk("HBA at %#lx failed while performing self test & setup.\n", 
	       (ulong) base);
	return (FALSE);
    }
    
    if (!reg_IRQ[gc->IRQ]) {    /* Interrupt already registered ? */
	if (!request_irq(gc->IRQ, do_eata_pio_int_handler, SA_INTERRUPT, 
			 "EATA-PIO", NULL)){
	    reg_IRQ[gc->IRQ]++;
	    if (!gc->IRQ_TR)
		reg_IRQL[gc->IRQ] = TRUE;   /* IRQ is edge triggered */
	} else {
	    printk("Couldn't allocate IRQ %d, Sorry.\n", gc->IRQ);
	    return (FALSE);
	}
    } else {            /* More than one HBA on this IRQ */
	if (reg_IRQL[gc->IRQ] == TRUE) {
	    printk("Can't support more than one HBA on this IRQ,\n"
		   "  if the IRQ is edge triggered. Sorry.\n");
	    return (FALSE);
	} else
	    reg_IRQ[gc->IRQ]++;
    }
    
    request_region(base, 8, "eata_pio");
    
    size = sizeof(hostdata) + (sizeof(struct eata_ccb) * ntohs(gc->queuesiz));
    
    sh = scsi_register(tpnt, size);
    if(sh == NULL)
    {
    	release_region(base, 8);
    	return FALSE;
    }
    
    hd = SD(sh);                   
    
    memset(hd->ccb, 0, (sizeof(struct eata_ccb) * ntohs(gc->queuesiz)));
    memset(hd->reads, 0, sizeof(ulong) * 26); 
    
    strncpy(SD(sh)->vendor, &buff[8], 8);
    SD(sh)->vendor[8] = 0;
    strncpy(SD(sh)->name, &buff[16], 17);
    SD(sh)->name[17] = 0;
    SD(sh)->revision[0] = buff[32];
    SD(sh)->revision[1] = buff[33];
    SD(sh)->revision[2] = buff[34];
    SD(sh)->revision[3] = '.';
    SD(sh)->revision[4] = buff[35];
    SD(sh)->revision[5] = 0;

    switch (ntohl(gc->len)) {
    case 0x1c:
	SD(sh)->EATA_revision = 'a';
	break;
    case 0x1e:
	SD(sh)->EATA_revision = 'b';
	break;
    case 0x22:
	SD(sh)->EATA_revision = 'c';
	break;
    case 0x24:
	SD(sh)->EATA_revision = 'z';		
    default:
	SD(sh)->EATA_revision = '?';
    }

    if(ntohl(gc->len) >= 0x22) {
	if (gc->is_PCI == TRUE)
	    hd->bustype = IS_PCI;
	else if (gc->is_EISA == TRUE)
	    hd->bustype = IS_EISA;
	else
	    hd->bustype = IS_ISA;
    } else {
	if (buff[21] == '4')
	    hd->bustype = IS_PCI;
	else if (buff[21] == '2')
	    hd->bustype = IS_EISA;
	else
	    hd->bustype = IS_ISA;
    }
  
    SD(sh)->cplen=cplen;
    SD(sh)->cppadlen=cppadlen;
    SD(sh)->hostid=gc->scsi_id[3];
    SD(sh)->devflags=1<<gc->scsi_id[3];
    SD(sh)->moresupport=gc->MORE_support;
    sh->unique_id = base;
    sh->base = base;
    sh->io_port = base;
    sh->n_io_port = 8;
    sh->irq = gc->IRQ;
    sh->dma_channel = PIO;
    sh->this_id = gc->scsi_id[3];
    sh->can_queue = 1;
    sh->cmd_per_lun = 1;
    sh->sg_tablesize = SG_ALL;
    
    hd->channel = 0;
    
    sh->max_id = 8;
    sh->max_lun = 8;

    if (gc->SECOND)
	hd->primary = FALSE;
    else
	hd->primary = TRUE;
    
    sh->unchecked_isa_dma = FALSE; /* We can only do PIO */
    
    hd->next = NULL;    /* build a linked list of all HBAs */
    hd->prev = last_HBA;
    if(hd->prev != NULL)
	SD(hd->prev)->next = sh;
    last_HBA = sh;
    if (first_HBA == NULL)
	first_HBA = sh;
    registered_HBAs++;
    return (1);
}

void find_pio_ISA(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
    int i;
    
    for (i = 0; i < MAXISA; i++) {  
	if (ISAbases[i]) {  
	    if (get_pio_conf_PIO(ISAbases[i], buf) == TRUE){
		register_pio_HBA(ISAbases[i], buf, tpnt);
	    }
	    ISAbases[i] = 0;
	}
    }
    return;
}

void find_pio_EISA(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
    u32 base;
    int i;

#if CHECKPAL
    u8 pal1, pal2, pal3;
#endif

    for (i = 0; i < MAXEISA; i++) {
	if (EISAbases[i] == TRUE) { /* Still a possibility ?          */

	    base = 0x1c88 + (i * 0x1000);
#if CHECKPAL
	    pal1 = inb((u16)base - 8);
	    pal2 = inb((u16)base - 7);
	    pal3 = inb((u16)base - 6);

	    if (((pal1 == 0x12) && (pal2 == 0x14)) ||
		((pal1 == 0x38) && (pal2 == 0xa3) && (pal3 == 0x82)) ||
		((pal1 == 0x06) && (pal2 == 0x94) && (pal3 == 0x24))) {
		DBG(DBG_PROBE, printk(KERN_NOTICE "EISA EATA id tags found: "
                                      "%x %x %x \n",
				      (int)pal1, (int)pal2, (int)pal3));
#endif
		if (get_pio_conf_PIO(base, buf) == TRUE) {
		    DBG(DBG_PROBE && DBG_EISA, print_pio_config(buf));
		    if (buf->IRQ) {
			register_pio_HBA(base, buf, tpnt);
		    } else
			printk(KERN_NOTICE "eata_dma: No valid IRQ. HBA "
                               "removed from list\n");
		}
		/* Nothing found here so we take it from the list */
		EISAbases[i] = 0;
#if CHECKPAL
	    }
#endif
	}
    }
    return;
}

void find_pio_PCI(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
#ifndef CONFIG_PCI
    printk("eata_dma: kernel PCI support not enabled. Skipping scan for PCI HBAs.\n");
#else
    struct pci_dev *dev = NULL; 
    u32 base, x;

    while ((dev = pci_find_device(PCI_VENDOR_ID_DPT, PCI_DEVICE_ID_DPT, dev)) != NULL) {
	    DBG(DBG_PROBE && DBG_PCI, 
		printk("eata_pio: find_PCI, HBA at %s\n", dev->name));
	    if (pci_enable_device(dev))
	    	continue;
	    pci_set_master(dev);
	    base = pci_resource_flags(dev, 0);
	    if (base & IORESOURCE_MEM) {
		printk("eata_pio: invalid base address of device %s\n", dev->name);
		continue;
	    }
	    base = pci_resource_start(dev, 0);
            /* EISA tag there ? */
	    if ((inb(base) == 0x12) && (inb(base + 1) == 0x14))
		continue;   /* Jep, it's forced, so move on  */
	    base += 0x10;   /* Now, THIS is the real address */
	    if (base != 0x1f8) {
		/* We didn't find it in the primary search */
		if (get_pio_conf_PIO(base, buf) == TRUE) {
		    if (buf->FORCADR)   /* If the address is forced */
			continue;       /* we'll find it later      */
		    
		    /* OK. We made it till here, so we can go now  
		     * and register it. We  only have to check and 
		     * eventually remove it from the EISA and ISA list 
		     */
		    
		    register_pio_HBA(base, buf, tpnt);
		    
		    if (base < 0x1000) {
			for (x = 0; x < MAXISA; ++x) {
			    if (ISAbases[x] == base) {
				ISAbases[x] = 0;
				break;
			    }
			}
		    } else if ((base & 0x0fff) == 0x0c88) {
			x = (base >> 12) & 0x0f;
			EISAbases[x] = 0;
		    }
		} 
#if CHECK_BLINK
		else if (check_blink_state(base) == TRUE) {
		    printk("eata_pio: HBA is in BLINK state.\n"
			   "Consult your HBAs manual to correct this.\n");
		}
#endif
	    }
	}
#endif /* #ifndef CONFIG_PCI */
}


int eata_pio_detect(Scsi_Host_Template * tpnt)
{
    struct Scsi_Host *HBA_ptr;
    struct get_conf gc;
    int i;
    
    DBG((DBG_PROBE && DBG_DELAY) || DPT_DEBUG,
	printk("Using lots of delays to let you read the debugging output\n"));
    
    tpnt->proc_name = "eata_pio";

    find_pio_PCI(&gc, tpnt);

    find_pio_EISA(&gc, tpnt);

    find_pio_ISA(&gc, tpnt);
    
    for (i = 0; i <= MAXIRQ; i++)
	if (reg_IRQ[i])
	    request_irq(i, do_eata_pio_int_handler, SA_INTERRUPT, "EATA-PIO", NULL);
    
    HBA_ptr = first_HBA;
  
    if (registered_HBAs != 0) {
	printk("EATA (Extended Attachment) PIO driver version: %d.%d%s\n"
	       "(c) 1993-95 Michael Neuffer, neuffer@goofy.zdv.uni-mainz.de\n"
	       "            Alfred Arnold,   a.arnold@kfa-juelich.de\n"
	       "This release only supports DASD devices (harddisks)\n",
	       VER_MAJOR, VER_MINOR, VER_SUB);
	
	printk("Registered HBAs:\n");
	printk("HBA no. Boardtype: Revis: EATA: Bus: BaseIO: IRQ: Ch: ID: Pr:"
               " QS: SG: CPL:\n");
	for (i = 1; i <= registered_HBAs; i++) {
	    printk("scsi%-2d: %.10s v%s 2.0%c  %s %#.4x   %2d   %d   %d   %c"
                   "  %2d  %2d  %2d\n", 
		   HBA_ptr->host_no, SD(HBA_ptr)->name, SD(HBA_ptr)->revision,
		   SD(HBA_ptr)->EATA_revision, (SD(HBA_ptr)->bustype == 'P')?
		   "PCI ":(SD(HBA_ptr)->bustype == 'E')?"EISA":"ISA ",
		   (uint) HBA_ptr->base, HBA_ptr->irq, SD(HBA_ptr)->channel, 
                   HBA_ptr->this_id, (SD(HBA_ptr)->primary == TRUE)?'Y':'N', 
		   HBA_ptr->can_queue, HBA_ptr->sg_tablesize, 
                   HBA_ptr->cmd_per_lun);
	    HBA_ptr = SD(HBA_ptr)->next;
	}
    }
    DBG(DPT_DEBUG,DELAY(12));
    
    return (registered_HBAs);
}

/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = EATA_PIO;

#include "scsi_module.c"
MODULE_LICENSE("GPL");

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
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
