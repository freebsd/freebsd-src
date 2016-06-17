/* 
 *  sym53c416.c
 *  Low-level SCSI driver for sym53c416 chip.
 *  Copyright (C) 1998 Lieven Willems (lw_linux@hotmail.com)
 * 
 *  Changes : 
 * 
 *  Marcelo Tosatti <marcelo@conectiva.com.br> : Added io_request_lock locking
 *  Alan Cox <alan@redhat.com> : Cleaned up code formatting
 *				 Fixed an irq locking bug
 *				 Added ISAPnP support
 * 
 *  LILO command line usage: sym53c416=<PORTBASE>[,<IRQ>]
 *
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/blk.h>
#include <linux/version.h>
#include <linux/isapnp.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#include "sym53c416.h"

#define VERSION_STRING        "Version 1.0.0-ac"

#define TC_LOW       0x00     /* Transfer counter low        */
#define TC_MID       0x01     /* Transfer counter mid        */
#define SCSI_FIFO    0x02     /* SCSI FIFO register          */
#define COMMAND_REG  0x03     /* Command Register            */
#define STATUS_REG   0x04     /* Status Register (READ)      */
#define DEST_BUS_ID  0x04     /* Destination Bus ID (WRITE)  */
#define INT_REG      0x05     /* Interrupt Register (READ)   */
#define TOM          0x05     /* Time out multiplier (WRITE) */
#define STP          0x06     /* Synchronous Transfer period */
#define SYNC_OFFSET  0x07     /* Synchronous Offset          */
#define CONF_REG_1   0x08     /* Configuration register 1    */
#define CONF_REG_2   0x0B     /* Configuration register 2    */
#define CONF_REG_3   0x0C     /* Configuration register 3    */
#define CONF_REG_4   0x0D     /* Configuration register 4    */
#define TC_HIGH      0x0E     /* Transfer counter high       */
#define PIO_FIFO_1   0x10     /* PIO FIFO register 1         */
#define PIO_FIFO_2   0x11     /* PIO FIFO register 2         */
#define PIO_FIFO_3   0x12     /* PIO FIFO register 3         */
#define PIO_FIFO_4   0x13     /* PIO FIFO register 4         */
#define PIO_FIFO_CNT 0x14     /* PIO FIFO count              */
#define PIO_INT_REG  0x15     /* PIO interrupt register      */
#define CONF_REG_5   0x16     /* Configuration register 5    */
#define FEATURE_EN   0x1D     /* Feature Enable register     */

/* Configuration register 1 entries: */
/* Bits 2-0: SCSI ID of host adapter */
#define SCM    0x80                     /* Slow Cable Mode              */
#define SRID   0x40                     /* SCSI Reset Interrupt Disable */
#define PTM    0x20                     /* Parity Test Mode             */
#define EPC    0x10                     /* Enable Parity Checking       */
#define CTME   0x08                     /* Special Test Mode            */

/* Configuration register 2 entries: */
#define FE     0x40                     /* Features Enable              */
#define SCSI2  0x08                     /* SCSI 2 Enable                */
#define TBPA   0x04                     /* Target Bad Parity Abort      */

/* Configuration register 3 entries: */
#define IDMRC  0x80                     /* ID Message Reserved Check    */
#define QTE    0x40                     /* Queue Tag Enable             */
#define CDB10  0x20                     /* Command Descriptor Block 10  */
#define FSCSI  0x10                     /* FastSCSI                     */
#define FCLK   0x08                     /* FastClock                    */

/* Configuration register 4 entries: */
#define RBS    0x08                     /* Register bank select         */
#define EAN    0x04                     /* Enable Active Negotiation    */

/* Configuration register 5 entries: */
#define LPSR   0x80                     /* Lower Power SCSI Reset       */
#define IE     0x20                     /* Interrupt Enable             */
#define LPM    0x02                     /* Low Power Mode               */
#define WSE0   0x01                     /* 0WS Enable                   */

/* Interrupt register entries: */
#define SRST   0x80                     /* SCSI Reset                   */
#define ILCMD  0x40                     /* Illegal Command              */
#define DIS    0x20                     /* Disconnect                   */
#define BS     0x10                     /* Bus Service                  */
#define FC     0x08                     /* Function Complete            */
#define RESEL  0x04                     /* Reselected                   */
#define SI     0x03                     /* Selection Interrupt          */

/* Status Register Entries: */
#define SCI    0x80                     /* SCSI Core Int                */
#define GE     0x40                     /* Gross Error                  */
#define PE     0x20                     /* Parity Error                 */
#define TC     0x10                     /* Terminal Count               */
#define VGC    0x08                     /* Valid Group Code             */
#define PHBITS 0x07                     /* Phase bits                   */

/* PIO Interrupt Register Entries: */
#define SCI    0x80                     /* SCSI Core Int                */
#define PFI    0x40                     /* PIO FIFO Interrupt           */
#define FULL   0x20                     /* PIO FIFO Full                */
#define EMPTY  0x10                     /* PIO FIFO Empty               */
#define CE     0x08                     /* Collision Error              */
#define OUE    0x04                     /* Overflow / Underflow error   */
#define FIE    0x02                     /* Full Interrupt Enable        */
#define EIE    0x01                     /* Empty Interrupt Enable       */

/* SYM53C416 SCSI phases (lower 3 bits of SYM53C416_STATUS_REG) */
#define PHASE_DATA_OUT    0x00
#define PHASE_DATA_IN     0x01
#define PHASE_COMMAND     0x02
#define PHASE_STATUS      0x03
#define PHASE_RESERVED_1  0x04
#define PHASE_RESERVED_2  0x05
#define PHASE_MESSAGE_OUT 0x06
#define PHASE_MESSAGE_IN  0x07

/* SYM53C416 core commands */
#define NOOP                      0x00
#define FLUSH_FIFO                0x01
#define RESET_CHIP                0x02
#define RESET_SCSI_BUS            0x03
#define DISABLE_SEL_RESEL         0x45
#define RESEL_SEQ                 0x40
#define SEL_WITHOUT_ATN_SEQ       0x41
#define SEL_WITH_ATN_SEQ          0x42
#define SEL_WITH_ATN_AND_STOP_SEQ 0x43
#define ENABLE_SEL_RESEL          0x44
#define SEL_WITH_ATN3_SEQ         0x46
#define RESEL3_SEQ                0x47
#define SND_MSG                   0x20
#define SND_STAT                  0x21
#define SND_DATA                  0x22
#define DISCONNECT_SEQ            0x23
#define TERMINATE_SEQ             0x24
#define TARGET_COMM_COMPLETE_SEQ  0x25
#define DISCONN                   0x27
#define RECV_MSG_SEQ              0x28
#define RECV_CMD                  0x29
#define RECV_DATA                 0x2A
#define RECV_CMD_SEQ              0x2B
#define TARGET_ABORT_PIO          0x04
#define TRANSFER_INFORMATION      0x10
#define INIT_COMM_COMPLETE_SEQ    0x11
#define MSG_ACCEPTED              0x12
#define TRANSFER_PAD              0x18
#define SET_ATN                   0x1A
#define RESET_ATN                 0x1B
#define ILLEGAL                   0xFF

#define PIO_MODE                  0x80

#define IO_RANGE 0x20         /* 0x00 - 0x1F                   */
#define ID       "sym53c416"	/* Attention: copied to the sym53c416.h */
#define PIO_SIZE 128          /* Size of PIO fifo is 128 bytes */

#define READ_TIMEOUT              150
#define WRITE_TIMEOUT             150

#ifdef MODULE

#define sym53c416_base sym53c416
#define sym53c416_base_1 sym53c416_1
#define sym53c416_base_2 sym53c416_2
#define sym53c416_base_3 sym53c416_3

static unsigned int sym53c416_base[2] = {0,0};
static unsigned int sym53c416_base_1[2] = {0,0};
static unsigned int sym53c416_base_2[2] = {0,0};
static unsigned int sym53c416_base_3[2] = {0,0};

#endif

/* #define DEBUG */

/* Macro for debugging purposes */

#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

#define MAXHOSTS 4

enum phases
{
	idle,
	data_out,
	data_in,
	command_ph,
	status_ph,
	message_out,
	message_in
};

typedef struct
{
	int base;
	int irq;
	int scsi_id;
} host;

static host hosts[MAXHOSTS] = {
                       {0, 0, SYM53C416_SCSI_ID},
                       {0, 0, SYM53C416_SCSI_ID},
                       {0, 0, SYM53C416_SCSI_ID},
                       {0, 0, SYM53C416_SCSI_ID}
                       };

static int host_index = 0;
static char info[120];
static Scsi_Cmnd *current_command = NULL;
static int fastpio = 1;

static int probeaddrs[] = {0x200, 0x220, 0x240, 0};

static void sym53c416_set_transfer_counter(int base, unsigned int len)
{
	/* Program Transfer Counter */
	outb(len & 0x0000FF, base + TC_LOW);
	outb((len & 0x00FF00) >> 8, base + TC_MID);
	outb((len & 0xFF0000) >> 16, base + TC_HIGH);
}

/* Returns the number of bytes read */
static __inline__ unsigned int sym53c416_read(int base, unsigned char *buffer, unsigned int len)
{
	unsigned int orig_len = len;
	unsigned long flags = 0;
	unsigned int bytes_left;
	int i;
	int timeout = READ_TIMEOUT;

	/* Do transfer */
	save_flags(flags);
	cli();
	while(len && timeout)
	{
		bytes_left = inb(base + PIO_FIFO_CNT); /* Number of bytes in the PIO FIFO */
		if(fastpio && bytes_left > 3)
		{
			insl(base + PIO_FIFO_1, buffer, bytes_left >> 2);
			buffer += bytes_left & 0xFC;
			len -= bytes_left & 0xFC;
		}
		else if(bytes_left > 0)
		{
			len -= bytes_left;
			for(; bytes_left > 0; bytes_left--)
				*(buffer++) = inb(base + PIO_FIFO_1);
		}
		else
		{
			i = jiffies + timeout;
			restore_flags(flags);
			while(jiffies < i && (inb(base + PIO_INT_REG) & EMPTY) && timeout)
				if(inb(base + PIO_INT_REG) & SCI)
					timeout = 0;
			save_flags(flags);
			cli();
			if(inb(base + PIO_INT_REG) & EMPTY)
				timeout = 0;
		}
	}
	restore_flags(flags);
	return orig_len - len;
}

/* Returns the number of bytes written */
static __inline__ unsigned int sym53c416_write(int base, unsigned char *buffer, unsigned int len)
{
	unsigned int orig_len = len;
	unsigned long flags = 0;
	unsigned int bufferfree;
	unsigned int i;
	unsigned int timeout = WRITE_TIMEOUT;

	/* Do transfer */
	save_flags(flags);
	cli();
	while(len && timeout)
	{
		bufferfree = PIO_SIZE - inb(base + PIO_FIFO_CNT);
		if(bufferfree > len)
			bufferfree = len;
		if(fastpio && bufferfree > 3)
		{
			outsl(base + PIO_FIFO_1, buffer, bufferfree >> 2);
			buffer += bufferfree & 0xFC;
			len -= bufferfree & 0xFC;
		}
		else if(bufferfree > 0)
		{
			len -= bufferfree;
			for(; bufferfree > 0; bufferfree--)
				outb(*(buffer++), base + PIO_FIFO_1);
		}
		else
		{
			i = jiffies + timeout;
			restore_flags(flags);
			while(jiffies < i && (inb(base + PIO_INT_REG) & FULL) && timeout)
				;
			save_flags(flags);
			cli();
			if(inb(base + PIO_INT_REG) & FULL)
				timeout = 0;
		}
	}
	restore_flags(flags);
	return orig_len - len;
}

static void sym53c416_intr_handle(int irq, void *dev_id, struct pt_regs *regs)
{
	int base = 0;
	int i;
	unsigned long flags = 0;
	unsigned char status_reg, pio_int_reg, int_reg;
	struct scatterlist *sglist;
	unsigned int sgcount;
	unsigned int tot_trans = 0;

	/* We search the base address of the host adapter which caused the interrupt */
	/* FIXME: should pass dev_id sensibly as hosts[i] */
	for(i = 0; i < host_index && !base; i++)
		if(irq == hosts[i].irq)
			base = hosts[i].base;
	/* If no adapter found, we cannot handle the interrupt. Leave a message */
	/* and continue. This should never happen...                            */
	if(!base)
	{
		printk(KERN_ERR "sym53c416: No host adapter defined for interrupt %d\n", irq);
		return;
	}
	/* Now we have the base address and we can start handling the interrupt */

	spin_lock_irqsave(&io_request_lock,flags);
	status_reg = inb(base + STATUS_REG);
	pio_int_reg = inb(base + PIO_INT_REG);
	int_reg = inb(base + INT_REG);
	spin_unlock_irqrestore(&io_request_lock, flags);

	/* First, we handle error conditions */
	if(int_reg & SCI)         /* SCSI Reset */
	{
		printk(KERN_DEBUG "sym53c416: Reset received\n");
		current_command->SCp.phase = idle;
		current_command->result = DID_RESET << 16;
		spin_lock_irqsave(&io_request_lock, flags);
		current_command->scsi_done(current_command);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if(int_reg & ILCMD)       /* Illegal Command */
	{
		printk(KERN_WARNING "sym53c416: Illegal Command: 0x%02x.\n", inb(base + COMMAND_REG));
		current_command->SCp.phase = idle;
		current_command->result = DID_ERROR << 16;
		spin_lock_irqsave(&io_request_lock, flags);
		current_command->scsi_done(current_command);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if(status_reg & GE)         /* Gross Error */
	{
		printk(KERN_WARNING "sym53c416: Controller reports gross error.\n");
		current_command->SCp.phase = idle;
		current_command->result = DID_ERROR << 16;
		spin_lock_irqsave(&io_request_lock, flags);
		current_command->scsi_done(current_command);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if(status_reg & PE)         /* Parity Error */
	{
		printk(KERN_WARNING "sym53c416:SCSI parity error.\n");
		current_command->SCp.phase = idle;
		current_command->result = DID_PARITY << 16;
		spin_lock_irqsave(&io_request_lock, flags);
		current_command->scsi_done(current_command);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if(pio_int_reg & (CE | OUE))
	{
		printk(KERN_WARNING "sym53c416: PIO interrupt error.\n");
		current_command->SCp.phase = idle;
		current_command->result = DID_ERROR << 16;
		spin_lock_irqsave(&io_request_lock, flags);
		current_command->scsi_done(current_command);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	if(int_reg & DIS)           /* Disconnect */
	{
		if(current_command->SCp.phase != message_in)
			current_command->result = DID_NO_CONNECT << 16;
		else
			current_command->result = (current_command->SCp.Status & 0xFF) | ((current_command->SCp.Message & 0xFF) << 8) | (DID_OK << 16);
		current_command->SCp.phase = idle;
		spin_lock_irqsave(&io_request_lock, flags);
		current_command->scsi_done(current_command);
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	/* Now we handle SCSI phases         */

	switch(status_reg & PHBITS)       /* Filter SCSI phase out of status reg */
	{
		case PHASE_DATA_OUT:
		{
			if(int_reg & BS)
			{
				current_command->SCp.phase = data_out;
				outb(FLUSH_FIFO, base + COMMAND_REG);
				sym53c416_set_transfer_counter(base, current_command->request_bufflen);
				outb(TRANSFER_INFORMATION | PIO_MODE, base + COMMAND_REG);
				if(!current_command->use_sg)
					tot_trans = sym53c416_write(base, current_command->request_buffer, current_command->request_bufflen);
				else
				{
					sgcount = current_command->use_sg;
					sglist = current_command->request_buffer;
					while(sgcount--)
					{
						tot_trans += sym53c416_write(base, sglist->address, sglist->length);
						sglist++;
					}
				}
				if(tot_trans < current_command->underflow)
					printk(KERN_WARNING "sym53c416: Underflow, wrote %d bytes, request for %d bytes.\n", tot_trans, current_command->underflow);
			}
			break;
		}

		case PHASE_DATA_IN:
		{
			if(int_reg & BS)
			{
				current_command->SCp.phase = data_in;
				outb(FLUSH_FIFO, base + COMMAND_REG);
				sym53c416_set_transfer_counter(base, current_command->request_bufflen);
				outb(TRANSFER_INFORMATION | PIO_MODE, base + COMMAND_REG);
				if(!current_command->use_sg)
					tot_trans = sym53c416_read(base, current_command->request_buffer, current_command->request_bufflen);
				else
				{
					sgcount = current_command->use_sg;
					sglist = current_command->request_buffer;
					while(sgcount--)
					{
						tot_trans += sym53c416_read(base, sglist->address, sglist->length);
						sglist++;
					}
				}
				if(tot_trans < current_command->underflow)
					printk(KERN_WARNING "sym53c416: Underflow, read %d bytes, request for %d bytes.\n", tot_trans, current_command->underflow);
			}
			break;
		}

		case PHASE_COMMAND:
		{
			current_command->SCp.phase = command_ph;
			printk(KERN_ERR "sym53c416: Unknown interrupt in command phase.\n");
			break;
		}

		case PHASE_STATUS:
		{
			current_command->SCp.phase = status_ph;
			outb(FLUSH_FIFO, base + COMMAND_REG);
			outb(INIT_COMM_COMPLETE_SEQ, base + COMMAND_REG);
			break;
		}
		
		case PHASE_RESERVED_1:
		case PHASE_RESERVED_2:
		{
			printk(KERN_ERR "sym53c416: Reserved phase occurred.\n");
			break;
		}

		case PHASE_MESSAGE_OUT:
		{
			current_command->SCp.phase = message_out;
			outb(SET_ATN, base + COMMAND_REG);
			outb(MSG_ACCEPTED, base + COMMAND_REG);
			break;
		}

		case PHASE_MESSAGE_IN:
		{
			current_command->SCp.phase = message_in;
			current_command->SCp.Status = inb(base + SCSI_FIFO);
			current_command->SCp.Message = inb(base + SCSI_FIFO);
			if(current_command->SCp.Message == SAVE_POINTERS || current_command->SCp.Message == DISCONNECT)
				outb(SET_ATN, base + COMMAND_REG);
			outb(MSG_ACCEPTED, base + COMMAND_REG);
			break;
		}
	}
}

static void sym53c416_init(int base, int scsi_id)
{
	outb(RESET_CHIP, base + COMMAND_REG);
	outb(NOOP, base + COMMAND_REG);
	outb(0x99, base + TOM); /* Time out of 250 ms */
	outb(0x05, base + STP);
	outb(0x00, base + SYNC_OFFSET);
	outb(EPC | scsi_id, base + CONF_REG_1);
	outb(FE | SCSI2 | TBPA, base + CONF_REG_2);
	outb(IDMRC | QTE | CDB10 | FSCSI | FCLK, base + CONF_REG_3);
	outb(0x83 | EAN, base + CONF_REG_4);
	outb(IE | WSE0, base + CONF_REG_5);
	outb(0, base + FEATURE_EN);
}

static int sym53c416_probeirq(int base, int scsi_id)
{
	int irq, irqs, i;

	/* Clear interrupt register */
	inb(base + INT_REG);
	/* Start probing for irq's */
	irqs = probe_irq_on();
	/* Reinit chip */
	sym53c416_init(base, scsi_id);
	/* Cause interrupt */
	outb(NOOP, base + COMMAND_REG);
	outb(ILLEGAL, base + COMMAND_REG);
	outb(0x07, base + DEST_BUS_ID);
	outb(0x00, base + DEST_BUS_ID);
	/* Wait for interrupt to occur */
	i = jiffies + 20;
	while(i > jiffies && !(inb(base + STATUS_REG) & SCI))
		barrier();
	if(i <= jiffies) /* timed out */
		return 0;
	/* Get occurred irq */
	irq = probe_irq_off(irqs);
	sym53c416_init(base, scsi_id);
	return irq;
}

/* Setup: sym53c416=base,irq */
void sym53c416_setup(char *str, int *ints)
{
	int i;

	if(host_index >= MAXHOSTS)
	{
		printk(KERN_WARNING "sym53c416: Too many hosts defined\n");
		return;
	}
	if(ints[0] < 1 || ints[0] > 2)
	{
		printk(KERN_ERR "sym53c416: Wrong number of parameters:\n");
		printk(KERN_ERR "sym53c416: usage: sym53c416=<base>[,<irq>]\n");
		return;
	}
	for(i = 0; i < host_index && i >= 0; i++)
	        if(hosts[i].base == ints[1])
        		i = -2;
	if(i >= 0)
	{
        	hosts[host_index].base = ints[1];
        	hosts[host_index].irq = (ints[0] == 2)? ints[2] : 0;
        	host_index++;
	}
}

static int sym53c416_test(int base)
{
	outb(RESET_CHIP, base + COMMAND_REG);
	outb(NOOP, base + COMMAND_REG);
	if(inb(base + COMMAND_REG) != NOOP)
		return 0;
	if(!inb(base + TC_HIGH) || inb(base + TC_HIGH) == 0xFF)
		return 0;
	if((inb(base + PIO_INT_REG) & (FULL | EMPTY | CE | OUE | FIE | EIE)) != EMPTY)
		return 0;
	return 1;
}


static struct isapnp_device_id id_table[] = {
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('S','L','I'), ISAPNP_FUNCTION(0x4163), 0 },
	{0}
};

MODULE_DEVICE_TABLE(isapnp, id_table);

void sym53c416_probe(void)
{
	int *base = probeaddrs;
	int ints[2];

	ints[0] = 1;
	for(; *base; base++)
	{
		if(!check_region(*base, IO_RANGE) && sym53c416_test(*base))
		{
			ints[1] = *base;
			sym53c416_setup(NULL, ints);
		}
	}
}

int sym53c416_detect(Scsi_Host_Template *tpnt)
{
	unsigned long flags;
	struct Scsi_Host * shpnt = NULL;
	int i;
	int count;
	struct pci_dev *idev = NULL;
	
#ifdef MODULE
	int ints[3];

	ints[0] = 2;
	if(sym53c416_base)
	{
		ints[1] = sym53c416_base[0];
		ints[2] = sym53c416_base[1];
		sym53c416_setup(NULL, ints);
	}
	if(sym53c416_base_1)
	{
		ints[1] = sym53c416_base_1[0];
		ints[2] = sym53c416_base_1[1];
		sym53c416_setup(NULL, ints);
	}
	if(sym53c416_base_2)
	{
		ints[1] = sym53c416_base_2[0];
		ints[2] = sym53c416_base_2[1];
		sym53c416_setup(NULL, ints);
	}
	if(sym53c416_base_3)
	{
		ints[1] = sym53c416_base_3[0];
		ints[2] = sym53c416_base_3[1];
		sym53c416_setup(NULL, ints);
	}
#endif
	printk(KERN_INFO "sym53c416.c: %s\n", VERSION_STRING);

	while((idev=isapnp_find_dev(NULL, ISAPNP_VENDOR('S','L','I'), 
				ISAPNP_FUNCTION(0x4163), idev))!=NULL)
	{
		int i[3];
		
		if(idev->prepare(idev)<0)
		{
			printk(KERN_WARNING "sym53c416: unable to prepare PnP card.\n");
			continue;
		}
		if(idev->activate(idev)<0)
		{
			printk(KERN_WARNING "sym53c416: unable to activate PnP card.\n");
			continue;
		}
		
		i[0] = 2;
		i[1] = idev->resource[0].start;
		i[2] = idev->irq_resource[0].start;
		
		printk(KERN_INFO "sym53c416: ISAPnP card found and configured at 0x%X, IRQ %d.\n",
			i[1], i[2]);
		sym53c416_setup(NULL, i);
	}
	sym53c416_probe();

	/* Now we register and set up each host adapter found... */
	for(count = 0, i = 0; i < host_index; i++)
	{
		if(!sym53c416_test(hosts[i].base))
			printk(KERN_WARNING "No sym53c416 found at address 0x%03x\n", hosts[i].base);
		else
		{
			if(hosts[i].irq == 0)
			/* We don't have an irq yet, so we should probe for one */
				if((hosts[i].irq = sym53c416_probeirq(hosts[i].base, hosts[i].scsi_id)) == 0)
					printk(KERN_WARNING "IRQ autoprobing failed for sym53c416 at address 0x%03x\n", hosts[i].base);
			if(hosts[i].irq && !check_region(hosts[i].base, IO_RANGE))
			{
				shpnt = scsi_register(tpnt, 0);
				if(shpnt==NULL)
					continue;
				save_flags(flags);
				cli();
				/* FIXME: Request_irq with CLI is not safe */
				/* Request for specified IRQ */
				if(request_irq(hosts[i].irq, sym53c416_intr_handle, 0, ID, NULL))
				{
					restore_flags(flags);
					printk(KERN_ERR "sym53c416: Unable to assign IRQ %d\n", hosts[i].irq);
					scsi_unregister(shpnt);
				}
				else
				{
					/* Inform the kernel of our IO range */
					request_region(hosts[i].base, IO_RANGE, ID);
					shpnt->unique_id = hosts[i].base;
					shpnt->io_port = hosts[i].base;
					shpnt->n_io_port = IO_RANGE;
					shpnt->irq = hosts[i].irq;
					shpnt->this_id = hosts[i].scsi_id;
					sym53c416_init(hosts[i].base, hosts[i].scsi_id);
					count++;
					restore_flags(flags);
				}
			}
		}
	}
	return count;
}

const char *sym53c416_info(struct Scsi_Host *SChost)
{
	int i;
	int base = SChost->io_port;
	int irq = SChost->irq;
	int scsi_id = 0;
	int rev = inb(base + TC_HIGH);

	for(i = 0; i < host_index; i++)
		if(hosts[i].base == base)
			scsi_id = hosts[i].scsi_id;
	sprintf(info, "Symbios Logic 53c416 (rev. %d) at 0x%03x, irq %d, SCSI-ID %d, %s pio", rev, base, irq, scsi_id, (fastpio)? "fast" : "slow");
	return info;
}

int sym53c416_queuecommand(Scsi_Cmnd *SCpnt, void (*done)(Scsi_Cmnd *))
{
	int base;
	unsigned long flags = 0;
	int i;

	/* Store base register as we can have more than one controller in the system */
	base = SCpnt->host->io_port;
	current_command = SCpnt;                  /* set current command                */
	current_command->scsi_done = done;        /* set ptr to done function           */
	current_command->SCp.phase = command_ph;  /* currect phase is the command phase */
	current_command->SCp.Status = 0;
	current_command->SCp.Message = 0;

	save_flags(flags);
	cli();
	outb(SCpnt->target, base + DEST_BUS_ID); /* Set scsi id target        */
	outb(FLUSH_FIFO, base + COMMAND_REG);    /* Flush SCSI and PIO FIFO's */
	/* Write SCSI command into the SCSI fifo */
	for(i = 0; i < SCpnt->cmd_len; i++)
		outb(SCpnt->cmnd[i], base + SCSI_FIFO);
	/* Start selection sequence */
	outb(SEL_WITHOUT_ATN_SEQ, base + COMMAND_REG);
	/* Now an interrupt will be generated which we will catch in out interrupt routine */
	restore_flags(flags);
	return 0;
}

static void internal_done(Scsi_Cmnd *SCpnt)
{
	SCpnt->SCp.Status++;
}

static int sym53c416_command(Scsi_Cmnd *SCpnt)
{
	sym53c416_queuecommand(SCpnt, internal_done);
	SCpnt->SCp.Status = 0;
	while(!SCpnt->SCp.Status)
		barrier();
	return SCpnt->result;
}

static int sym53c416_abort(Scsi_Cmnd *SCpnt)
{
	//printk("sym53c416_abort\n");
	/* We don't know how to abort for the moment */
	return SCSI_ABORT_SNOOZE;
}

static int sym53c416_reset(Scsi_Cmnd *SCpnt, unsigned int reset_flags)
{
	int base;
	int scsi_id = -1;	
	int i;

	//printk("sym53c416_reset\n");
	base = SCpnt->host->io_port;
	/* search scsi_id */
	for(i = 0; i < host_index && scsi_id != -1; i++)
		if(hosts[i].base == base)
			scsi_id = hosts[i].scsi_id;
	outb(RESET_CHIP, base + COMMAND_REG);
	outb(NOOP | PIO_MODE, base + COMMAND_REG);
	outb(RESET_SCSI_BUS, base + COMMAND_REG);
	sym53c416_init(base, scsi_id);
	return SCSI_RESET_PENDING;
}

static int sym53c416_bios_param(Disk *disk, kdev_t dev, int *ip)
{
	int size;

	size = disk->capacity;
	ip[0] = 64;				/* heads                        */
	ip[1] = 32;				/* sectors                      */
	if((ip[2] = size >> 11) > 1024)		/* cylinders, test for big disk */
	{
		ip[0] = 255;			/* heads                        */
		ip[1] = 63;			/* sectors                      */
		ip[2] = size / (255 * 63);	/* cylinders                    */
	}
	return 0;
}

/* Loadable module support */
#ifdef MODULE

MODULE_AUTHOR("Lieven Willems");
MODULE_LICENSE("GPL");

MODULE_PARM(sym53c416, "1-2i");
MODULE_PARM(sym53c416_1, "1-2i");
MODULE_PARM(sym53c416_2, "1-2i");
MODULE_PARM(sym53c416_3, "1-2i");

#endif

static Scsi_Host_Template driver_template = SYM53C416;

#include "scsi_module.c"
