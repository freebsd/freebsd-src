#ifndef CCISS_H
#define CCISS_H

#include <linux/genhd.h>

#include "cciss_cmd.h"


#define NWD		16
#define NWD_SHIFT	4
#define MAX_PART	16

#define IO_OK		0
#define IO_ERROR	1

#define MAJOR_NR COMPAQ_CISS_MAJOR 

struct ctlr_info;
typedef struct ctlr_info ctlr_info_t;

struct access_method {
	void (*submit_command)(ctlr_info_t *h, CommandList_struct *c);
	void (*set_intr_mask)(ctlr_info_t *h, unsigned long val);
	unsigned long (*fifo_full)(ctlr_info_t *h);
	unsigned long (*intr_pending)(ctlr_info_t *h);
	unsigned long (*command_completed)(ctlr_info_t *h);
};
typedef struct _drive_info_struct
{
 	__u32   	LunID;	
	int 		usage_count;
	unsigned int 	nr_blocks;
	int		block_size;
	int 		heads;
	int		sectors;
	int 		cylinders;
	int 		raid_level;
} drive_info_struct;

struct ctlr_info 
{
	int	ctlr;
	int	major;
	char	devname[8];
	char    *product_name;
	char	firm_ver[4]; // Firmware version 
	struct pci_dev *pdev;
	__u32	board_id;
	unsigned long vaddr;
	unsigned long paddr;	
	unsigned long io_mem_addr;
	unsigned long io_mem_length;
	CfgTable_struct *cfgtable;
	int	intr;
	int	interrupts_enabled;
	int 	max_commands;
	int	commands_outstanding;
	int 	max_outstanding; /* Debug */ 
	int	num_luns;
	int 	highest_lun;
	int	usage_count;  /* number of opens all all minor devices */

	// information about each logical volume
	drive_info_struct drv[CISS_MAX_LUN];

	struct access_method access;

	/* queue and queue Info */ 
	CommandList_struct *reqQ;
	CommandList_struct  *cmpQ;
	unsigned int Qdepth;
	unsigned int maxQsinceinit;
	unsigned int maxSG;

	//* pointers to command and error info pool */ 
	CommandList_struct 	*cmd_pool;
	dma_addr_t		cmd_pool_dhandle; 
	ErrorInfo_struct 	*errinfo_pool;
	dma_addr_t		errinfo_pool_dhandle; 
        __u32   		*cmd_pool_bits;
	int			nr_allocs;
	int			nr_frees; 

	// Disk structures we need to pass back
	struct gendisk   gendisk;
	   // indexed by minor numbers
	struct hd_struct hd[256];
	int              sizes[256];
	int              blocksizes[256];
	int              hardsizes[256];
	int busy_configuring;
#ifdef CONFIG_CISS_SCSI_TAPE
	void *scsi_ctlr; /* ptr to structure containing scsi related stuff */
#endif
#ifdef CONFIG_CISS_MONITOR_THREAD
	struct timer_list watchdog;
	struct task_struct *monitor_thread; 
	unsigned int monitor_period;
	unsigned int monitor_deadline;
	unsigned char alive;
	unsigned char monitor_started;
#define CCISS_MIN_PERIOD 10
#define CCISS_MAX_PERIOD 3600 
#define CTLR_IS_ALIVE(h) (h->alive)
#define ASSERT_CTLR_ALIVE(h) {	h->alive = 1; \
				h->monitor_period = 0; \
				h->monitor_started = 0; }
#define MONITOR_STATUS_PATTERN "Status: %s\n"
#define CTLR_STATUS(h) CTLR_IS_ALIVE(h) ? "operational" : "failed"
#define MONITOR_PERIOD_PATTERN "Monitor thread period: %d\n"
#define MONITOR_PERIOD_VALUE(h) (h->monitor_period)
#define MONITOR_DEADLINE_PATTERN "Monitor thread deadline: %d\n"
#define MONITOR_DEADLINE_VALUE(h) (h->monitor_deadline)
#define START_MONITOR_THREAD(h, cmd, count, cciss_monitor, rc) \
	start_monitor_thread(h, cmd, count, cciss_monitor, rc)
#else

#define MONITOR_PERIOD_PATTERN "%s"
#define MONITOR_PERIOD_VALUE(h) ""
#define MONITOR_DEADLINE_PATTERN "%s"
#define MONITOR_DEADLINE_VALUE(h) ""
#define MONITOR_STATUS_PATTERN "%s\n"
#define CTLR_STATUS(h) ""
#define CTLR_IS_ALIVE(h) (1)
#define ASSERT_CTLR_ALIVE(h)
#define START_MONITOR_THREAD(a,b,c,d,rc) (*rc == 0)

#endif
};

/*  Defining the diffent access_menthods */
/*
 * Memory mapped FIFO interface (SMART 53xx cards)
 */
#define SA5_DOORBELL	0x20
#define SA5_REQUEST_PORT_OFFSET	0x40
#define SA5_REPLY_INTR_MASK_OFFSET	0x34
#define SA5_REPLY_PORT_OFFSET		0x44
#define SA5_INTR_STATUS		0x30
#define SA5_SCRATCHPAD_OFFSET	0xB0

#define SA5_CTCFG_OFFSET	0xB4
#define SA5_CTMEM_OFFSET	0xB8

#define SA5_INTR_OFF		0x08
#define SA5B_INTR_OFF		0x04
#define SA5_INTR_PENDING	0x08
#define SA5B_INTR_PENDING	0x04
#define FIFO_EMPTY		0xffffffff	

#define  CISS_ERROR_BIT		0x02

#define CCISS_INTR_ON 	1 
#define CCISS_INTR_OFF	0
/* 
	Send the command to the hardware 
*/
static void SA5_submit_command( ctlr_info_t *h, CommandList_struct *c) 
{
#ifdef CCISS_DEBUG
	 printk("Sending %x - down to controller\n", c->busaddr );
#endif /* CCISS_DEBUG */ 
         writel(c->busaddr, h->vaddr + SA5_REQUEST_PORT_OFFSET);
	 h->commands_outstanding++;
	 if ( h->commands_outstanding > h->max_outstanding)
		h->max_outstanding = h->commands_outstanding;
}

/*  
 *  This card is the opposite of the other cards.  
 *   0 turns interrupts on... 
 *   0x08 turns them off... 
 */
static void SA5_intr_mask(ctlr_info_t *h, unsigned long val)
{
	if (val) 
	{ /* Turn interrupts on */
		h->interrupts_enabled = 1;
		writel(0, h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
	} else /* Turn them off */
	{
		h->interrupts_enabled = 0;
        	writel( SA5_INTR_OFF, 
			h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
	}
}
/*
 *  This card is the opposite of the other cards.
 *   0 turns interrupts on...
 *   0x04 turns them off...
 */
static void SA5B_intr_mask(ctlr_info_t *h, unsigned long val)
{
        if (val)
        { /* Turn interrupts on */
		h->interrupts_enabled = 1;
                writel(0, h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
        } else /* Turn them off */
        {
		h->interrupts_enabled = 0;
                writel( SA5B_INTR_OFF,
                        h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
        }
}
/*
 *  Returns true if fifo is full.  
 * 
 */ 
static unsigned long SA5_fifo_full(ctlr_info_t *h)
{
	if( h->commands_outstanding >= h->max_commands)
		return(1);
	else 
		return(0);

}
/* 
 *   returns value read from hardware. 
 *     returns FIFO_EMPTY if there is nothing to read 
 */ 
static unsigned long SA5_completed(ctlr_info_t *h)
{
	unsigned long register_value 
		= readl(h->vaddr + SA5_REPLY_PORT_OFFSET);
	if(register_value != FIFO_EMPTY)
	{
		h->commands_outstanding--;
#ifdef CCISS_DEBUG
		printk("cciss:  Read %lx back from board\n", register_value);
#endif /* CCISS_DEBUG */ 
	} 
#ifdef CCISS_DEBUG
	else
	{
		printk("cciss:  FIFO Empty read\n");
	}
#endif 
	return ( register_value); 

}
/*
 *	Returns true if an interrupt is pending.. 
 */
static unsigned long SA5_intr_pending(ctlr_info_t *h)
{
	unsigned long register_value  = 
		readl(h->vaddr + SA5_INTR_STATUS);
#ifdef CCISS_DEBUG
	printk("cciss: intr_pending %lx\n", register_value);
#endif  /* CCISS_DEBUG */
	if( register_value &  SA5_INTR_PENDING) 
		return  1;	
	return 0 ;
}

/*
 *      Returns true if an interrupt is pending..
 */
static unsigned long SA5B_intr_pending(ctlr_info_t *h)
{
        unsigned long register_value  =
                readl(h->vaddr + SA5_INTR_STATUS);
#ifdef CCISS_DEBUG
        printk("cciss: intr_pending %lx\n", register_value);
#endif  /* CCISS_DEBUG */
        if( register_value &  SA5B_INTR_PENDING)
                return  1;
        return 0 ;
}


static struct access_method SA5_access = {
	SA5_submit_command,
	SA5_intr_mask,
	SA5_fifo_full,
	SA5_intr_pending,
	SA5_completed,
};

static struct access_method SA5B_access = {
        SA5_submit_command,
        SA5B_intr_mask,
        SA5_fifo_full,
        SA5B_intr_pending,
        SA5_completed,
};

struct board_type {
	__u32	board_id;
	char	*product_name;
	struct access_method *access;
};
#endif /* CCISS_H */

