#define AUTOSENSE
#define PSEUDO_DMA

/*
 * Generic Generic NCR5380 driver
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
 * $Log: cumana_1.c,v $
 * Revision 1.3  1998/05/03 20:45:32  alan
 * ARM SCSI update. This adds the eesox driver and massively updates the
 * Cumana driver. The folks who bought cumana arent anal retentive all
 * docs are secret weenies so now there are docs ..
 *
 * Revision 1.2  1998/03/08 05:49:46  davem
 * Merge to 2.1.89
 *
 * Revision 1.1  1998/02/23 02:45:22  davem
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
#include <asm/irq.h>
#include <asm/system.h>

#include "../../scsi/scsi.h"
#include "../../scsi/hosts.h"
#include "../../scsi/constants.h"

#include <scsi/scsicam.h>

#define CUMANASCSI_PUBLIC_RELEASE 1

static const card_ids cumanascsi_cids[] = {
	{ MANU_CUMANA, PROD_CUMANA_SCSI_1 },
	{ 0xffff, 0xffff }
};

#define NCR5380_implementation_fields \
    int port, ctrl

#define NCR5380_local_declare() \
        struct Scsi_Host *_instance

#define NCR5380_setup(instance) \
        _instance = instance

#define NCR5380_read(reg) cumanascsi_read(_instance, reg)
#define NCR5380_write(reg, value) cumanascsi_write(_instance, reg, value)

#define do_NCR5380_intr do_cumanascsi_intr
#define NCR5380_queue_command cumanascsi_queue_command
#define NCR5380_abort cumanascsi_abort
#define NCR5380_reset cumanascsi_reset
#define NCR5380_proc_info cumanascsi_proc_info

int NCR5380_proc_info(char *buffer, char **start, off_t offset,
		      int length, int hostno, int inout);

#define BOARD_NORMAL	0
#define BOARD_NCR53C400	1

#include "../../scsi/NCR5380.h"

/*
 * Function : cumanascsi_setup(char *str, int *ints)
 *
 * Purpose : LILO command line initialization of the overrides array,
 *
 * Inputs : str - unused, ints - array of integer parameters with ints[0]
 *	equal to the number of ints.
 *
 */

void cumanascsi_setup(char *str, int *ints)
{
}

#define CUMANA_ADDRESS(card) (ecard_address((card), ECARD_IOC, ECARD_SLOW) + 0x800)
#define CUMANA_IRQ(card)     ((card)->irq)
/*
 * Function : int cumanascsi_detect(Scsi_Host_Template * tpnt)
 *
 * Purpose : initializes cumana NCR5380 driver based on the
 *	command line / compile time port and irq definitions.
 *
 * Inputs : tpnt - template for this SCSI adapter.
 *
 * Returns : 1 if a host adapter was found, 0 if not.
 *
 */
static struct expansion_card *ecs[4];
 
int cumanascsi_detect(Scsi_Host_Template * tpnt)
{
    int count = 0;
    struct Scsi_Host *instance;

    tpnt->proc_name = "CumanaSCSI-1";

    memset (ecs, 0, sizeof (ecs));

    while(1) {
    	if((ecs[count] = ecard_find(0, cumanascsi_cids)) == NULL)
    		break;

        instance = scsi_register (tpnt, sizeof(struct NCR5380_hostdata));
        instance->io_port = CUMANA_ADDRESS(ecs[count]);
	instance->irq = CUMANA_IRQ(ecs[count]);

	NCR5380_init(instance, 0);
	ecard_claim(ecs[count]);

	instance->n_io_port = 255;
	request_region (instance->io_port, instance->n_io_port, "CumanaSCSI-1");

        ((struct NCR5380_hostdata *)instance->hostdata)->ctrl = 0;
        outb(0x00, instance->io_port - 577);

	if (instance->irq != IRQ_NONE)
	    if (request_irq(instance->irq, do_cumanascsi_intr, SA_INTERRUPT, "CumanaSCSI-1", NULL)) {
		printk("scsi%d: IRQ%d not free, interrupts disabled\n",
		    instance->host_no, instance->irq);
		instance->irq = IRQ_NONE;
	    }

	if (instance->irq == IRQ_NONE) {
	    printk("scsi%d: interrupts not enabled. for better interactive performance,\n", instance->host_no);
	    printk("scsi%d: please jumper the board for a free IRQ.\n", instance->host_no);
	}

	printk("scsi%d: at port %lX irq", instance->host_no, instance->io_port);
	if (instance->irq == IRQ_NONE)
	    printk ("s disabled");
	else
	    printk (" %d", instance->irq);
	printk(" options CAN_QUEUE=%d CMD_PER_LUN=%d release=%d",
	    tpnt->can_queue, tpnt->cmd_per_lun, CUMANASCSI_PUBLIC_RELEASE);
	printk("\nscsi%d:", instance->host_no);
	NCR5380_print_options(instance);
	printk("\n");

	++count;
    }
    return count;
}

int cumanascsi_release (struct Scsi_Host *shpnt)
{
	int i;

	if (shpnt->irq != IRQ_NONE)
		free_irq (shpnt->irq, NULL);
	if (shpnt->io_port)
		release_region (shpnt->io_port, shpnt->n_io_port);

	for (i = 0; i < 4; i++)
		if (shpnt->io_port == CUMANA_ADDRESS(ecs[i]))
			ecard_release (ecs[i]);
	return 0;
}

const char * cumanascsi_info (struct Scsi_Host *spnt) {
    return "";
}

#ifdef NOT_EFFICIENT
#define CTRL(p,v)     outb(*ctrl = (v), (p) - 577)
#define STAT(p)       inb((p)+1)
#define IN(p)         inb((p))
#define OUT(v,p)      outb((v), (p))
#else
#define CTRL(p,v)	(p[-2308] = (*ctrl = (v)))
#define STAT(p)		(p[4])
#define IN(p)		(*(p))
#define IN2(p)		((unsigned short)(*(volatile unsigned long *)(p)))
#define OUT(v,p)	(*(p) = (v))
#define OUT2(v,p)	(*((volatile unsigned long *)(p)) = (v))
#endif
#define L(v)		(((v)<<16)|((v) & 0x0000ffff))
#define H(v)		(((v)>>16)|((v) & 0xffff0000))

static inline int NCR5380_pwrite(struct Scsi_Host *instance, unsigned char *addr,
              int len)
{
  int *ctrl = &((struct NCR5380_hostdata *)instance->hostdata)->ctrl;
  int oldctrl = *ctrl;
  unsigned long *laddr;
#ifdef NOT_EFFICIENT
  int iobase = instance->io_port;
  int dma_io = iobase & ~(0x3C0000>>2);
#else
  volatile unsigned char *iobase = (unsigned char *)ioaddr(instance->io_port);
  volatile unsigned char *dma_io = (unsigned char *)((int)iobase & ~0x3C0000);
#endif

  if(!len) return 0;

  CTRL(iobase, 0x02);
  laddr = (unsigned long *)addr;
  while(len >= 32)
  {
    int status;
    unsigned long v;
    status = STAT(iobase);
    if(status & 0x80)
      goto end;
    if(!(status & 0x40))
      continue;
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    v=*laddr++; OUT2(L(v),dma_io); OUT2(H(v),dma_io);
    len -= 32;
    if(len == 0)
      break;
  }

  addr = (unsigned char *)laddr;
  CTRL(iobase, 0x12);
  while(len > 0)
  {
    int status;
    status = STAT(iobase);
    if(status & 0x80)
      goto end;
    if(status & 0x40)
    {
      OUT(*addr++, dma_io);
      if(--len == 0)
        break;
    }

    status = STAT(iobase);
    if(status & 0x80)
      goto end;
    if(status & 0x40)
    {
      OUT(*addr++, dma_io);
      if(--len == 0)
        break;
    }
  }
end:
  CTRL(iobase, oldctrl|0x40);
  return len;
}

static inline int NCR5380_pread(struct Scsi_Host *instance, unsigned char *addr,
              int len)
{
  int *ctrl = &((struct NCR5380_hostdata *)instance->hostdata)->ctrl;
  int oldctrl = *ctrl;
  unsigned long *laddr;
#ifdef NOT_EFFICIENT
  int iobase = instance->io_port;
  int dma_io = iobase & ~(0x3C0000>>2);
#else
  volatile unsigned char *iobase = (unsigned char *)ioaddr(instance->io_port);
  volatile unsigned char *dma_io = (unsigned char *)((int)iobase & ~0x3C0000);
#endif

  if(!len) return 0;

  CTRL(iobase, 0x00);
  laddr = (unsigned long *)addr;
  while(len >= 32)
  {
    int status;
    status = STAT(iobase);
    if(status & 0x80)
      goto end;
    if(!(status & 0x40))
      continue;
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    *laddr++ = IN2(dma_io)|(IN2(dma_io)<<16);
    len -= 32;
    if(len == 0)
      break;
  }

  addr = (unsigned char *)laddr;
  CTRL(iobase, 0x10);
  while(len > 0)
  {
    int status;
    status = STAT(iobase);
    if(status & 0x80)
      goto end;
    if(status & 0x40)
    {
      *addr++ = IN(dma_io);
      if(--len == 0)
        break;
    }

    status = STAT(iobase);
    if(status & 0x80)
      goto end;
    if(status & 0x40)
    {
      *addr++ = IN(dma_io);
      if(--len == 0)
        break;
    }
  }
end:
  CTRL(iobase, oldctrl|0x40);
  return len;
}

#undef STAT
#undef CTRL
#undef IN
#undef OUT

#define CTRL(p,v) outb(*ctrl = (v), (p) - 577)

static char cumanascsi_read(struct Scsi_Host *instance, int reg)
{
  int iobase = instance->io_port;
  int i;
  int *ctrl = &((struct NCR5380_hostdata *)instance->hostdata)->ctrl;

  CTRL(iobase, 0);
  i = inb(iobase + 64 + reg);
  CTRL(iobase, 0x40);

  return i;
}

static void cumanascsi_write(struct Scsi_Host *instance, int reg, int value)
{
  int iobase = instance->io_port;
  int *ctrl = &((struct NCR5380_hostdata *)instance->hostdata)->ctrl;

  CTRL(iobase, 0);
  outb(value, iobase + 64 + reg);
  CTRL(iobase, 0x40);
}

#undef CTRL

#include "../../scsi/NCR5380.c"

static Scsi_Host_Template cumanascsi_template = {
	module:			THIS_MODULE,
	name:			"Cumana 16-bit SCSI",
	detect:			cumanascsi_detect,
	release:		cumanascsi_release,
	info:			cumanascsi_info,
	queuecommand:		cumanascsi_queue_command,
	abort:			cumanascsi_abort,
	reset:			cumanascsi_reset,
	bios_param:		scsicam_bios_param,
	can_queue:		16,
	this_id:		7,
	sg_tablesize:		SG_ALL,
	cmd_per_lun:		2,
	unchecked_isa_dma:	0,
	use_clustering:		DISABLE_CLUSTERING
};

static int __init cumanascsi_init(void)
{
	scsi_register_module(MODULE_SCSI_HA, &cumanascsi_template);
	if (cumanascsi_template.present)
		return 0;

	scsi_unregister_module(MODULE_SCSI_HA, &cumanascsi_template);
	return -ENODEV;
}

static void __exit cumanascsi_exit(void)
{
	scsi_unregister_module(MODULE_SCSI_HA, &cumanascsi_template);
}

module_init(cumanascsi_init);
module_exit(cumanascsi_exit);

MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
