/*
 * SCSI low-level driver for the 53c94 SCSI bus adaptor found
 * on Power Macintosh computers, controlling the external SCSI chain.
 * We assume the 53c94 is connected to a DBDMA (descriptor-based DMA)
 * controller.
 *
 * Paul Mackerras, August 1996.
 * Copyright (C) 1996 Paul Mackerras.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/blk.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/spinlock.h>
#include <asm/dbdma.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/system.h>

#include "scsi.h"
#include "hosts.h"
#include "mac53c94.h"

enum fsc_phase {
	idle,
	selecting,
	dataing,
	completing,
	busfreeing,
};

struct fsc_state {
	volatile struct	mac53c94_regs *regs;
	int	intr;
	volatile struct	dbdma_regs *dma;
	int	dmaintr;
	int	clk_freq;
	struct	Scsi_Host *host;
	struct	fsc_state *next;
	Scsi_Cmnd *request_q;
	Scsi_Cmnd *request_qtail;
	Scsi_Cmnd *current_req;		/* req we're currently working on */
	enum fsc_phase phase;		/* what we're currently trying to do */
	struct dbdma_cmd *dma_cmds;	/* space for dbdma commands, aligned */
	void	*dma_cmd_space;
};

static struct fsc_state *all_53c94s;

static void mac53c94_init(struct fsc_state *);
static void mac53c94_start(struct fsc_state *);
static void mac53c94_interrupt(int, void *, struct pt_regs *);
static void do_mac53c94_interrupt(int, void *, struct pt_regs *);
static void cmd_done(struct fsc_state *, int result);
static void set_dma_cmds(struct fsc_state *, Scsi_Cmnd *);
static int data_goes_out(Scsi_Cmnd *);

int
mac53c94_detect(Scsi_Host_Template *tp)
{
	struct device_node *node;
	int nfscs;
	struct fsc_state *state, **prev_statep;
	struct Scsi_Host *host;
	void *dma_cmd_space;
	unsigned char *clkprop;
	int proplen;

	nfscs = 0;
	prev_statep = &all_53c94s;
	for (node = find_devices("53c94"); node != 0; node = node->next) {
		if (node->n_addrs != 2 || node->n_intrs != 2)
			panic("53c94: expected 2 addrs and intrs (got %d/%d)",
			      node->n_addrs, node->n_intrs);
		host = scsi_register(tp, sizeof(struct fsc_state));
		if (host == NULL)
			break;
		host->unique_id = nfscs;
#ifndef MODULE
		note_scsi_host(node, host);
#endif

		state = (struct fsc_state *) host->hostdata;
		if (state == 0)
			panic("no 53c94 state");
		state->host = host;
		state->regs = (volatile struct mac53c94_regs *)
			ioremap(node->addrs[0].address, 0x1000);
		state->intr = node->intrs[0].line;
		state->dma = (volatile struct dbdma_regs *)
			ioremap(node->addrs[1].address, 0x1000);
		state->dmaintr = node->intrs[1].line;

		clkprop = get_property(node, "clock-frequency", &proplen);
		if (clkprop == NULL || proplen != sizeof(int)) {
			printk(KERN_ERR "%s: can't get clock frequency\n",
			       node->full_name);
			state->clk_freq = 25000000;
		} else
			state->clk_freq = *(int *)clkprop;

		/* Space for dma command list: +1 for stop command,
		   +1 to allow for aligning. */
		dma_cmd_space = kmalloc((host->sg_tablesize + 2) *
					sizeof(struct dbdma_cmd), GFP_KERNEL);
		if (dma_cmd_space == 0)
			panic("53c94: couldn't allocate dma command space");
		state->dma_cmds = (struct dbdma_cmd *)
			DBDMA_ALIGN(dma_cmd_space);
		memset(state->dma_cmds, 0, (host->sg_tablesize + 1)
		       * sizeof(struct dbdma_cmd));
		state->dma_cmd_space = dma_cmd_space;

		*prev_statep = state;
		prev_statep = &state->next;

		if (request_irq(state->intr, do_mac53c94_interrupt, 0,
				"53C94", state)) {
			printk(KERN_ERR "mac53C94: can't get irq %d\n", state->intr);
		}

		mac53c94_init(state);

		++nfscs;
	}
	return nfscs;
}

int
mac53c94_release(struct Scsi_Host *host)
{
	struct fsc_state *fp = (struct fsc_state *) host->hostdata;

	if (fp == 0)
		return 0;
	if (fp->regs)
		iounmap((void *) fp->regs);
	if (fp->dma)
		iounmap((void *) fp->dma);
	kfree(fp->dma_cmd_space);
	free_irq(fp->intr, fp);
	return 0;
}

int
mac53c94_queue(Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *))
{
	unsigned long flags;
	struct fsc_state *state;

#if 0
	if (data_goes_out(cmd)) {
		int i;
		printk(KERN_DEBUG "mac53c94_queue %p: command is", cmd);
		for (i = 0; i < cmd->cmd_len; ++i)
			printk(" %.2x", cmd->cmnd[i]);
		printk("\n" KERN_DEBUG "use_sg=%d request_bufflen=%d request_buffer=%p\n",
		       cmd->use_sg, cmd->request_bufflen, cmd->request_buffer);
	}
#endif

	cmd->scsi_done = done;
	cmd->host_scribble = NULL;

	state = (struct fsc_state *) cmd->host->hostdata;

	save_flags(flags);
	cli();
	if (state->request_q == NULL)
		state->request_q = cmd;
	else
		state->request_qtail->host_scribble = (void *) cmd;
	state->request_qtail = cmd;

	if (state->phase == idle)
		mac53c94_start(state);

	restore_flags(flags);
	return 0;
}

int
mac53c94_abort(Scsi_Cmnd *cmd)
{
	return SCSI_ABORT_SNOOZE;
}

int
mac53c94_reset(Scsi_Cmnd *cmd, unsigned how)
{
	struct fsc_state *state = (struct fsc_state *) cmd->host->hostdata;
	volatile struct mac53c94_regs *regs = state->regs;
	volatile struct dbdma_regs *dma = state->dma;
	unsigned long flags;

	save_flags(flags);
	cli();
	st_le32(&dma->control, (RUN|PAUSE|FLUSH|WAKE) << 16);
	regs->command = CMD_SCSI_RESET;	/* assert RST */
	eieio();
	udelay(100);			/* leave it on for a while (>= 25us) */
	regs->command = CMD_RESET;
	eieio();
	udelay(20);
	mac53c94_init(state);
	regs->command = CMD_NOP;
	eieio();
	restore_flags(flags);
	return SCSI_RESET_PENDING;
}

int
mac53c94_command(Scsi_Cmnd *cmd)
{
	printk(KERN_DEBUG "whoops... mac53c94_command called\n");
	return -1;
}

static void
mac53c94_init(struct fsc_state *state)
{
	volatile struct mac53c94_regs *regs = state->regs;
	volatile struct dbdma_regs *dma = state->dma;
	int x;

	regs->config1 = state->host->this_id | CF1_PAR_ENABLE;
	regs->sel_timeout = TIMO_VAL(250);	/* 250ms */
	regs->clk_factor = CLKF_VAL(state->clk_freq);
	regs->config2 = CF2_FEATURE_EN;
	regs->config3 = 0;
	regs->sync_period = 0;
	regs->sync_offset = 0;
	eieio();
	x = regs->interrupt;
	st_le32(&dma->control, (RUN|PAUSE|FLUSH|WAKE) << 16);
}

/*
 * Start the next command for a 53C94.
 * Should be called with interrupts disabled.
 */
static void
mac53c94_start(struct fsc_state *state)
{
	Scsi_Cmnd *cmd;
	volatile struct mac53c94_regs *regs = state->regs;
	int i;

	if (state->phase != idle || state->current_req != NULL)
		panic("inappropriate mac53c94_start (state=%p)", state);
	if (state->request_q == NULL)
		return;
	state->current_req = cmd = state->request_q;
	state->request_q = (Scsi_Cmnd *) cmd->host_scribble;

	/* Off we go */
	regs->count_lo = 0;
	regs->count_mid = 0;
	regs->count_hi = 0;
	eieio();
	regs->command = CMD_NOP + CMD_DMA_MODE;
	udelay(1);
	eieio();
	regs->command = CMD_FLUSH;
	udelay(1);
	eieio();
	regs->dest_id = cmd->target;
	regs->sync_period = 0;
	regs->sync_offset = 0;
	eieio();

	/* load the command into the FIFO */
	for (i = 0; i < cmd->cmd_len; ++i) {
		regs->fifo = cmd->cmnd[i];
		eieio();
	}

	/* do select without ATN XXX */
	regs->command = CMD_SELECT;
	state->phase = selecting;

	if (cmd->use_sg > 0 || cmd->request_bufflen != 0)
		set_dma_cmds(state, cmd);
}

static void
do_mac53c94_interrupt(int irq, void *dev_id, struct pt_regs *ptregs)
{
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);
	mac53c94_interrupt(irq, dev_id, ptregs);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void
mac53c94_interrupt(int irq, void *dev_id, struct pt_regs *ptregs)
{
	struct fsc_state *state = (struct fsc_state *) dev_id;
	volatile struct mac53c94_regs *regs = state->regs;
	volatile struct dbdma_regs *dma = state->dma;
	Scsi_Cmnd *cmd = state->current_req;
	int nb, stat, seq, intr;
	static int mac53c94_errors;

	/*
	 * Apparently, reading the interrupt register unlatches
	 * the status and sequence step registers.
	 */
	seq = regs->seqstep;
	stat = regs->status;
	intr = regs->interrupt;

#if 0
	printk(KERN_DEBUG "mac53c94_intr, intr=%x stat=%x seq=%x phase=%d\n",
	       intr, stat, seq, state->phase);
#endif

	if (intr & INTR_RESET) {
		/* SCSI bus was reset */
		printk(KERN_INFO "external SCSI bus reset detected\n");
		regs->command = CMD_NOP;
		st_le32(&dma->control, RUN << 16);	/* stop dma */
		cmd_done(state, DID_RESET << 16);
		return;
	}
	if (intr & INTR_ILL_CMD) {
		printk(KERN_ERR "53c94: illegal cmd, intr=%x stat=%x seq=%x phase=%d\n",
		       intr, stat, seq, state->phase);
		cmd_done(state, DID_ERROR << 16);
		return;
	}
	if (stat & STAT_ERROR) {
#if 0
		/* XXX these seem to be harmless? */
		printk("53c94: bad error, intr=%x stat=%x seq=%x phase=%d\n",
		       intr, stat, seq, state->phase);
#endif
		++mac53c94_errors;
		regs->command = CMD_NOP + CMD_DMA_MODE;
		eieio();
	}
	if (cmd == 0) {
		printk(KERN_DEBUG "53c94: interrupt with no command active?\n");
		return;
	}
	if (stat & STAT_PARITY) {
		printk(KERN_ERR "mac53c94: parity error\n");
		cmd_done(state, DID_PARITY << 16);
		return;
	}
	switch (state->phase) {
	case selecting:
		if (intr & INTR_DISCONNECT) {
			/* selection timed out */
			cmd_done(state, DID_BAD_TARGET << 16);
			return;
		}
		if (intr != INTR_BUS_SERV + INTR_DONE) {
			printk(KERN_DEBUG "got intr %x during selection\n", intr);
			cmd_done(state, DID_ERROR << 16);
			return;
		}
		if ((seq & SS_MASK) != SS_DONE) {
			printk(KERN_DEBUG "seq step %x after command\n", seq);
			cmd_done(state, DID_ERROR << 16);
			return;
		}
		regs->command = CMD_NOP;
		/* set DMA controller going if any data to transfer */
		if ((stat & (STAT_MSG|STAT_CD)) == 0
		    && (cmd->use_sg > 0 || cmd->request_bufflen != 0)) {
			nb = cmd->SCp.this_residual;
			if (nb > 0xfff0)
				nb = 0xfff0;
			cmd->SCp.this_residual -= nb;
			regs->count_lo = nb;
			regs->count_mid = nb >> 8;
			eieio();
			regs->command = CMD_DMA_MODE + CMD_NOP;
			eieio();
			st_le32(&dma->cmdptr, virt_to_phys(state->dma_cmds));
			st_le32(&dma->control, (RUN << 16) | RUN);
			eieio();
			regs->command = CMD_DMA_MODE + CMD_XFER_DATA;
			state->phase = dataing;
			break;
		} else if ((stat & STAT_PHASE) == STAT_CD + STAT_IO) {
			/* up to status phase already */
			regs->command = CMD_I_COMPLETE;
			state->phase = completing;
		} else {
			printk(KERN_DEBUG "in unexpected phase %x after cmd\n",
			       stat & STAT_PHASE);
			cmd_done(state, DID_ERROR << 16);
			return;
		}
		break;

	case dataing:
		if (intr != INTR_BUS_SERV) {
			printk(KERN_DEBUG "got intr %x before status\n", intr);
			cmd_done(state, DID_ERROR << 16);
			return;
		}
		if (cmd->SCp.this_residual != 0
		    && (stat & (STAT_MSG|STAT_CD)) == 0) {
			/* Set up the count regs to transfer more */
			nb = cmd->SCp.this_residual;
			if (nb > 0xfff0)
				nb = 0xfff0;
			cmd->SCp.this_residual -= nb;
			regs->count_lo = nb;
			regs->count_mid = nb >> 8;
			eieio();
			regs->command = CMD_DMA_MODE + CMD_NOP;
			eieio();
			regs->command = CMD_DMA_MODE + CMD_XFER_DATA;
			break;
		}
		if ((stat & STAT_PHASE) != STAT_CD + STAT_IO) {
			printk(KERN_DEBUG "intr %x before data xfer complete\n", intr);
		}
		st_le32(&dma->control, RUN << 16);	/* stop dma */
		/* should check dma status */
		regs->command = CMD_I_COMPLETE;
		state->phase = completing;
		break;
	case completing:
		if (intr != INTR_DONE) {
			printk(KERN_DEBUG "got intr %x on completion\n", intr);
			cmd_done(state, DID_ERROR << 16);
			return;
		}
		cmd->SCp.Status = regs->fifo; eieio();
		cmd->SCp.Message = regs->fifo; eieio();
		cmd->result = 
		regs->command = CMD_ACCEPT_MSG;
		state->phase = busfreeing;
		break;
	case busfreeing:
		if (intr != INTR_DISCONNECT) {
			printk(KERN_DEBUG "got intr %x when expected disconnect\n", intr);
		}
		cmd_done(state, (DID_OK << 16) + (cmd->SCp.Message << 8)
			 + cmd->SCp.Status);
		break;
	default:
		printk(KERN_DEBUG "don't know about phase %d\n", state->phase);
	}
}

static void
cmd_done(struct fsc_state *state, int result)
{
	Scsi_Cmnd *cmd;

	cmd = state->current_req;
	if (cmd != 0) {
		cmd->result = result;
		(*cmd->scsi_done)(cmd);
		state->current_req = NULL;
	}
	state->phase = idle;
	mac53c94_start(state);
}

/*
 * Set up DMA commands for transferring data.
 */
static void
set_dma_cmds(struct fsc_state *state, Scsi_Cmnd *cmd)
{
	int i, dma_cmd, total;
	struct scatterlist *scl;
	struct dbdma_cmd *dcmds;

	dma_cmd = data_goes_out(cmd)? OUTPUT_MORE: INPUT_MORE;
	dcmds = state->dma_cmds;
	if (cmd->use_sg > 0) {
		total = 0;
		scl = (struct scatterlist *) cmd->buffer;
		for (i = 0; i < cmd->use_sg; ++i) {
			if (scl->length > 0xffff)
				panic("mac53c94: scatterlist element >= 64k");
			total += scl->length;
			st_le16(&dcmds->req_count, scl->length);
			st_le16(&dcmds->command, dma_cmd);
			st_le32(&dcmds->phy_addr, virt_to_phys(scl->address));
			dcmds->xfer_status = 0;
			++scl;
			++dcmds;
		}
	} else {
		total = cmd->request_bufflen;
		if (total > 0xffff)
			panic("mac53c94: transfer size >= 64k");
		st_le16(&dcmds->req_count, total);
		st_le32(&dcmds->phy_addr, virt_to_phys(cmd->request_buffer));
		dcmds->xfer_status = 0;
		++dcmds;
	}
	dma_cmd += OUTPUT_LAST - OUTPUT_MORE;
	st_le16(&dcmds[-1].command, dma_cmd);
	st_le16(&dcmds->command, DBDMA_STOP);
	cmd->SCp.this_residual = total;
}

/*
 * Work out whether data will be going out from the host adaptor or into it.
 * (If this information is available from somewhere else in the scsi
 * code, somebody please let me know :-)
 */
static int
data_goes_out(Scsi_Cmnd *cmd)
{
	switch (cmd->cmnd[0]) {
	case CHANGE_DEFINITION: 
	case COMPARE:	  
	case COPY:
	case COPY_VERIFY:	    
	case FORMAT_UNIT:	 
	case LOG_SELECT:
	case MEDIUM_SCAN:	  
	case MODE_SELECT:
	case MODE_SELECT_10:
	case REASSIGN_BLOCKS: 
	case RESERVE:
	case SEARCH_EQUAL:	  
	case SEARCH_EQUAL_12: 
	case SEARCH_HIGH:	 
	case SEARCH_HIGH_12:  
	case SEARCH_LOW:
	case SEARCH_LOW_12:
	case SEND_DIAGNOSTIC: 
	case SEND_VOLUME_TAG:	     
	case SET_WINDOW: 
	case UPDATE_BLOCK:	
	case WRITE_BUFFER:
 	case WRITE_6:	
	case WRITE_10:	
	case WRITE_12:	  
	case WRITE_LONG:	
	case WRITE_LONG_2:      /* alternate code for WRITE_LONG */
	case WRITE_SAME:	
	case WRITE_VERIFY:
	case WRITE_VERIFY_12:
		return 1;
	default:
		return 0;
	}
}

static Scsi_Host_Template driver_template = SCSI_MAC53C94;

#include "scsi_module.c"
