#define AUTOSENSE
/*#define PSEUDO_DMA*/

/*
 * Oak Generic NCR5380 driver
 *
 * Copyright 1995, Russell King
 *
 * ALPHA RELEASE 1.
 *
 * For more information, please consult
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */

/*
 * Options :
 *
 * PARITY - enable parity checking.  Not supported.
 *
 * SCSI2 - enable support for SCSI-II tagged queueing.  Untested.
 *
 * USLEEP - enable support for devices that don't disconnect.  Untested.
 */

/*
 * $Log: oak.c,v $
 * Revision 1.3  1998/05/03 20:45:37  alan
 * ARM SCSI update. This adds the eesox driver and massively updates the
 * Cumana driver. The folks who bought cumana arent anal retentive all
 * docs are secret weenies so now there are docs ..
 *
 * Revision 1.2  1998/03/08 05:49:48  davem
 * Merge to 2.1.89
 *
 * Revision 1.1  1998/02/23 02:45:27  davem
 * Merge to 2.1.88
 *
 */

#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/blk.h>
#include <linux/init.h>

#include <asm/ecard.h>
#include <asm/io.h>
#include <asm/system.h>

#include "../../scsi/scsi.h"
#include "../../scsi/hosts.h"
#include "../../scsi/constants.h"

#define OAKSCSI_PUBLIC_RELEASE 1

#define NCR5380_read(reg)		oakscsi_read(_instance, reg)
#define NCR5380_write(reg, value)	oakscsi_write(_instance, reg, value)
#define do_NCR5380_intr			do_oakscsi_intr
#define NCR5380_queue_command		oakscsi_queue_command
#define NCR5380_abort			oakscsi_abort
#define NCR5380_reset			oakscsi_reset
#define NCR5380_proc_info		oakscsi_proc_info

int NCR5380_proc_info(char *buffer, char **start, off_t offset,
		      int length, int hostno, int inout);

#define NCR5380_implementation_fields \
	int port, ctrl

#define NCR5380_local_declare() \
        struct Scsi_Host *_instance

#define NCR5380_setup(instance) \
        _instance = instance

#define BOARD_NORMAL	0
#define BOARD_NCR53C400	1

#include "../../scsi/NCR5380.h"

#undef START_DMA_INITIATOR_RECEIVE_REG
#define START_DMA_INITIATOR_RECEIVE_REG (7 + 128)

static const card_ids oakscsi_cids[] = {
	{ MANU_OAK, PROD_OAK_SCSI },
	{ 0xffff, 0xffff }
};

#define OAK_ADDRESS(card) (ecard_address((card), ECARD_MEMC, 0))
#define OAK_IRQ(card)	  (IRQ_NONE)
/*
 * Function : int oakscsi_detect(Scsi_Host_Template * tpnt)
 *
 * Purpose : initializes oak NCR5380 driver based on the
 *	command line / compile time port and irq definitions.
 *
 * Inputs : tpnt - template for this SCSI adapter.
 *
 * Returns : 1 if a host adapter was found, 0 if not.
 *
 */
static struct expansion_card *ecs[4];

int oakscsi_detect(Scsi_Host_Template * tpnt)
{
    int count = 0;
    struct Scsi_Host *instance;

    tpnt->proc_name = "oakscsi";

    memset (ecs, 0, sizeof (ecs));

    ecard_startfind ();

    while(1) {
        if ((ecs[count] = ecard_find(0, oakscsi_cids)) == NULL)
            break;

        instance = scsi_register (tpnt, sizeof(struct NCR5380_hostdata));
        instance->io_port = OAK_ADDRESS(ecs[count]);
        instance->irq = OAK_IRQ(ecs[count]);

	NCR5380_init(instance, 0);
	ecard_claim(ecs[count]);

	instance->n_io_port = 255;
	request_region (instance->io_port, instance->n_io_port, "Oak SCSI");

	if (instance->irq != IRQ_NONE)
	    if (request_irq(instance->irq, do_oakscsi_intr, SA_INTERRUPT, "Oak SCSI", NULL)) {
		printk("scsi%d: IRQ%d not free, interrupts disabled\n",
		    instance->host_no, instance->irq);
		instance->irq = IRQ_NONE;
	    }

	if (instance->irq != IRQ_NONE) {
	    printk("scsi%d: eek! Interrupts enabled, but I don't think\n", instance->host_no);
	    printk("scsi%d: that the board had an interrupt!\n", instance->host_no);
	}

	printk("scsi%d: at port %lX irq", instance->host_no, instance->io_port);
	if (instance->irq == IRQ_NONE)
	    printk ("s disabled");
	else
	    printk (" %d", instance->irq);
	printk(" options CAN_QUEUE=%d  CMD_PER_LUN=%d release=%d",
	    tpnt->can_queue, tpnt->cmd_per_lun, OAKSCSI_PUBLIC_RELEASE);
	printk("\nscsi%d:", instance->host_no);
	NCR5380_print_options(instance);
	printk("\n");

	++count;
    }
#ifdef MODULE
    if(count == 0)
        printk("No oak scsi devices found\n");
#endif
    return count;
}

int oakscsi_release (struct Scsi_Host *shpnt)
{
	int i;

	if (shpnt->irq != IRQ_NONE)
		free_irq (shpnt->irq, NULL);
	if (shpnt->io_port)
		release_region (shpnt->io_port, shpnt->n_io_port);

	for (i = 0; i < 4; i++)
		if (shpnt->io_port == OAK_ADDRESS(ecs[i]))
			ecard_release (ecs[i]);
	return 0;
}

const char * oakscsi_info (struct Scsi_Host *spnt) {
    return "";
}

#define STAT(p)   inw(p + 144)
extern void inswb(int from, void *to, int len);

static inline int NCR5380_pwrite(struct Scsi_Host *instance, unsigned char *addr,
              int len)
{
  int iobase = instance->io_port;
printk("writing %p len %d\n",addr, len);
  if(!len) return -1;

  while(1)
  {
    int status;
    while(((status = STAT(iobase)) & 0x100)==0);
  }
}

static inline int NCR5380_pread(struct Scsi_Host *instance, unsigned char *addr,
              int len)
{
  int iobase = instance->io_port;
printk("reading %p len %d\n", addr, len);
  while(len > 0)
  {
    int status, timeout;
    unsigned long b;
    
    timeout = 0x01FFFFFF;
    
    while(((status = STAT(iobase)) & 0x100)==0)
    {
      timeout--;
      if(status & 0x200 || !timeout)
      {
        printk("status = %08X\n",status);
        return 1;
      }
    }
    if(len >= 128)
    {
      inswb(iobase + 136, addr, 128);
      addr += 128;
      len -= 128;
    }
    else
    {
      b = (unsigned long) inw(iobase + 136);
      *addr ++ = b;
      len -= 1;
      if(len)
        *addr ++ = b>>8;
      len -= 1;
    }
  }
  return 0;
}

#define oakscsi_read(instance,reg)	(inb((instance)->io_port + (reg)))
#define oakscsi_write(instance,reg,val)	(outb((val), (instance)->io_port + (reg)))

#undef STAT

#include "../../scsi/NCR5380.c"

static Scsi_Host_Template oakscsi_template = {
	module:		THIS_MODULE,
	proc_info:	oakscsi_proc_info,
	name:		"Oak 16-bit SCSI",
	detect:		oakscsi_detect,
	release:	oakscsi_release,
	info:		oakscsi_info,
	queuecommand:	oakscsi_queue_command,
	abort:		oakscsi_abort,
	reset:		oakscsi_reset,
	can_queue:	16,
	this_id:	7,
	sg_tablesize:	SG_ALL,
	cmd_per_lun:	2,
	use_clustering:	DISABLE_CLUSTERING
};

static int __init oakscsi_init(void)
{
	scsi_register_module(MODULE_SCSI_HA, &oakscsi_template);
	if (oakscsi_template.present)
		return 0;

	scsi_unregister_module(MODULE_SCSI_HA, &oakscsi_template);
	return -ENODEV;
}

static void __exit oakscsi_exit(void)
{
	scsi_unregister_module(MODULE_SCSI_HA, &oakscsi_template);
}

module_init(oakscsi_init);
module_exit(oakscsi_exit);

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("Oak SCSI driver");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
