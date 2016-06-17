/*********************************************************************
 *                
 * Filename:      smc-ircc.c
 * Version:       0.4
 * Description:   Driver for the SMC Infrared Communications Controller
 * Status:        Experimental.
 * Author:        Thomas Davis (tadavis@jps.net)
 * Created at:    
 * Modified at:   Tue Feb 22 10:05:06 2000
 * Modified by:   Dag Brattli <dag@brattli.net>
 * Modified at:   Tue Jun 26 2001
 * Modified by:   Stefani Seibold <stefani@seibold.net>
 * Modified at:   Thur Apr 18 2002
 * Modified by:   Jeff Snyder <je4d@pobox.com>
 * 
 *     Copyright (c) 2001      Stefani Seibold
 *     Copyright (c) 1999-2001 Dag Brattli
 *     Copyright (c) 1998-1999 Thomas Davis, 
 *     All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *
 *     SIO's: all SIO documentet by SMC (June, 2001)
 *     Applicable Models :	Fujitsu Lifebook 635t, Sony PCG-505TX,
 *     				Dell Inspiron 8000
 *
 ********************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/serial_reg.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>

#include <linux/pm.h>

#include <net/irda/wrapper.h>
#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irda_device.h>
#include <net/irda/smc-ircc.h>
#include <net/irda/irport.h>

struct smc_chip {
	char *name;
	u16 flags;
	u8 devid;
	u8 rev;
};
typedef struct smc_chip smc_chip_t;

static const char *driver_name = "smc-ircc";

#define	DIM(x)	(sizeof(x)/(sizeof(*(x))))

#define CHIP_IO_EXTENT 8

static struct ircc_cb *dev_self[] = { NULL, NULL};

/* Some prototypes */
static int  ircc_open(unsigned int iobase, unsigned int board_addr);
static int  ircc_dma_receive(struct ircc_cb *self, int iobase); 
static void ircc_dma_receive_complete(struct ircc_cb *self, int iobase);
static int  ircc_hard_xmit(struct sk_buff *skb, struct net_device *dev);
static void ircc_dma_xmit(struct ircc_cb *self, int iobase, int bofs);
static void ircc_change_speed(void *priv, u32 speed);
static void ircc_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int  ircc_net_open(struct net_device *dev);
static int  ircc_net_close(struct net_device *dev);
static int  ircc_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data);

#define	KEY55_1	0	/* SuperIO Configuration mode with Key <0x55> */
#define	KEY55_2	1	/* SuperIO Configuration mode with Key <0x55,0x55> */
#define	NoIRDA	2	/* SuperIO Chip has no IRDA Port */
#define	SIR	0	/* SuperIO Chip has only slow IRDA */
#define	FIR	4	/* SuperIO Chip has fast IRDA */
#define	SERx4	8	/* SuperIO Chip supports 115,2 KBaud * 4=460,8 KBaud */

/* These are the currently known SMC SuperIO chipsets */
static smc_chip_t __initdata fdc_chips_flat[]=
{
	/* Base address 0x3f0 or 0x370 */
	{ "37C44",	KEY55_1|NoIRDA,		0x00, 0x00 }, /* This chip can not detected */
	{ "37C665GT",	KEY55_2|NoIRDA,		0x65, 0x01 },
	{ "37C665GT",	KEY55_2|NoIRDA,		0x66, 0x01 },
	{ "37C669",	KEY55_2|SIR|SERx4,	0x03, 0x02 },
	{ "37C669",	KEY55_2|SIR|SERx4,	0x04, 0x02 }, /* ID? */
	{ "37C78",	KEY55_2|NoIRDA,		0x78, 0x00 },
	{ "37N769",	KEY55_1|FIR|SERx4,	0x28, 0x00 },
	{ "37N869",	KEY55_1|FIR|SERx4,	0x29, 0x00 },
	{ NULL }
};

static smc_chip_t __initdata fdc_chips_paged[]=
{
	/* Base address 0x3f0 or 0x370 */
	{ "37B72X",	KEY55_1|SIR|SERx4,	0x4c, 0x00 },
	{ "37B77X",	KEY55_1|SIR|SERx4,	0x43, 0x00 },
	{ "37B78X",	KEY55_1|SIR|SERx4,	0x44, 0x00 },
	{ "37B80X",	KEY55_1|SIR|SERx4,	0x42, 0x00 },
	{ "37C67X",	KEY55_1|FIR|SERx4,	0x40, 0x00 },
	{ "37C93X",	KEY55_2|SIR|SERx4,	0x02, 0x01 },
	{ "37C93XAPM",	KEY55_1|SIR|SERx4,	0x30, 0x01 },
	{ "37C93XFR",	KEY55_2|FIR|SERx4,	0x03, 0x01 },
	{ "37M707",	KEY55_1|SIR|SERx4,	0x42, 0x00 },
	{ "37M81X",	KEY55_1|SIR|SERx4,	0x4d, 0x00 },
	{ "37N958FR",	KEY55_1|FIR|SERx4,	0x09, 0x04 },
	{ "37N971",	KEY55_1|FIR|SERx4,	0x0a, 0x00 },
	{ "37N972",	KEY55_1|FIR|SERx4,	0x0b, 0x00 },
	{ NULL }
};

static smc_chip_t __initdata lpc_chips_flat[]=
{
	/* Base address 0x2E or 0x4E */
	{ "47N227",	KEY55_1|FIR|SERx4,	0x5a, 0x00 },
	{ "47N267",	KEY55_1|FIR|SERx4,	0x5e, 0x00 },
	{ NULL }
};

static smc_chip_t __initdata lpc_chips_paged[]=
{
	/* Base address 0x2E or 0x4E */
	{ "47B27X",	KEY55_1|SIR|SERx4,	0x51, 0x00 },
	{ "47B37X",	KEY55_1|SIR|SERx4,	0x52, 0x00 },
	{ "47M10X",	KEY55_1|SIR|SERx4,	0x59, 0x00 },
	{ "47M120",	KEY55_1|NoIRDA|SERx4,	0x5c, 0x00 },
	{ "47M13X",	KEY55_1|SIR|SERx4,	0x59, 0x00 },
	{ "47M14X",	KEY55_1|SIR|SERx4,	0x5f, 0x00 },
	{ "47N252",	KEY55_1|FIR|SERx4,	0x0e, 0x00 },
	{ "47S42X",	KEY55_1|SIR|SERx4,	0x57, 0x00 },
	{ NULL }
};

static int ircc_irq=255;
static int ircc_dma=255;
static int ircc_fir=0;
static int ircc_sir=0;
static int ircc_cfg=0;

static unsigned short	dev_count=0;

static inline void register_bank(int iobase, int bank)
{
        outb(((inb(iobase+IRCC_MASTER) & 0xf0) | (bank & 0x07)),
               iobase+IRCC_MASTER);
}

static int __init smc_access(unsigned short cfg_base,unsigned char reg)
{
	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	outb(reg, cfg_base);

	if (inb(cfg_base)!=reg)
		return -1;

	return 0;
}

static const smc_chip_t * __init smc_probe(unsigned short cfg_base,u8 reg,const smc_chip_t *chip,char *type)
{
	u8 devid,xdevid,rev; 

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	/* Leave configuration */

	outb(0xaa, cfg_base);

	if (inb(cfg_base)==0xaa)	/* not a smc superio chip */
		return NULL;

	outb(reg, cfg_base);

	xdevid=inb(cfg_base+1);

	/* Enter configuration */

	outb(0x55, cfg_base);

	if (smc_access(cfg_base,0x55))	/* send second key and check */
		return NULL;

	/* probe device ID */

	if (smc_access(cfg_base,reg))
		return NULL;

	devid=inb(cfg_base+1);

	if (devid==0)			/* typical value for unused port */
		return NULL;

	if (devid==0xff)		/* typical value for unused port */
		return NULL;

	/* probe revision ID */

	if (smc_access(cfg_base,reg+1))
		return NULL;

	rev=inb(cfg_base+1);

	if (rev>=128)			/* i think this will make no sense */
		return NULL;

	if (devid==xdevid)		/* protection against false positives */        
		return NULL;

	/* Check for expected device ID; are there others? */

	while(chip->devid!=devid) {

		chip++;

		if (chip->name==NULL)
			return NULL;
	}
	if (chip->rev>rev)
		return NULL;

	MESSAGE("found SMC SuperIO Chip (devid=0x%02x rev=%02X base=0x%04x): %s%s\n",devid,rev,cfg_base,type,chip->name);
	
	if (chip->flags&NoIRDA)
		MESSAGE("chipset does not support IRDA\n");

	return chip;
}

/*
 * Function smc_superio_flat (chip, base, type)
 *
 *    Try get configuration of a smc SuperIO chip with flat register model
 *
 */
static int __init smc_superio_flat(const smc_chip_t *chips, unsigned short cfg_base, char *type)
{
	unsigned short fir_io;
	unsigned short sir_io;
	u8 mode;
	int ret = -ENODEV;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	if (smc_probe(cfg_base,0xD,chips,type)==NULL)
		return ret;

	outb(0x0c, cfg_base);

	mode = inb(cfg_base+1);
	mode = (mode & 0x38) >> 3;
		
	/* Value for IR port */
	if (mode && mode < 4) {
		/* SIR iobase */
		outb(0x25, cfg_base);
		sir_io = inb(cfg_base+1) << 2;

	       	/* FIR iobase */
		outb(0x2b, cfg_base);
		fir_io = inb(cfg_base+1) << 3;

		if (fir_io) {
			if (ircc_open(fir_io, sir_io) == 0)
				ret=0; 
		}
	}
	
	/* Exit configuration */
	outb(0xaa, cfg_base);

	return ret;
}

/*
 * Function smc_superio_paged (chip, base, type)
 *
 *    Try  get configuration of a smc SuperIO chip with paged register model
 *
 */
static int __init smc_superio_paged(const smc_chip_t *chips, unsigned short cfg_base, char *type)
{
	unsigned short fir_io;
	unsigned short sir_io;
	int ret = -ENODEV;
	
	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	if (smc_probe(cfg_base,0x20,chips,type)==NULL)
		return ret;
	
	/* Select logical device (UART2) */
	outb(0x07, cfg_base);
	outb(0x05, cfg_base + 1);
		
	/* SIR iobase */
	outb(0x60, cfg_base);
	sir_io  = inb(cfg_base + 1) << 8;
	outb(0x61, cfg_base);
	sir_io |= inb(cfg_base + 1);
		
	/* Read FIR base */
	outb(0x62, cfg_base);
	fir_io = inb(cfg_base + 1) << 8;
	outb(0x63, cfg_base);
	fir_io |= inb(cfg_base + 1);
	outb(0x2b, cfg_base); /* ??? */

	if (fir_io) {
		if (ircc_open(fir_io, sir_io) == 0)
			ret=0; 
	}
	
	/* Exit configuration */
	outb(0xaa, cfg_base);

	return ret;
}

static int __init smc_superio_fdc(unsigned short cfg_base)
{
	if (check_region(cfg_base, 2) < 0) {
		IRDA_DEBUG(0, "%s: can't get cfg_base of 0x%03x\n",
			__FUNCTION__, cfg_base);
		return -1;
	}

	if (!smc_superio_flat(fdc_chips_flat,cfg_base,"FDC")||!smc_superio_paged(fdc_chips_paged,cfg_base,"FDC"))
		return 0;

	return -1;
}

static int __init smc_superio_lpc(unsigned short cfg_base)
{
#if 0
	if (check_region(cfg_base, 2) < 0) {
		IRDA_DEBUG(0, "%s: can't get cfg_base of 0x%03x\n",
			__FUNCTION__, cfg_base);
		return -1;
	}
#endif

	if (!smc_superio_flat(lpc_chips_flat,cfg_base,"LPC")||!smc_superio_paged(lpc_chips_paged,cfg_base,"LPC"))
		return 0;

	return -1;
}

/*
 * Function ircc_init ()
 *
 *    Initialize chip. Just try to find out how many chips we are dealing with
 *    and where they are
 */
int __init ircc_init(void)
{
	int ret=-ENODEV;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	dev_count=0;

	if ((ircc_fir>0)&&(ircc_sir>0)) {
	        MESSAGE(" Overriding FIR address 0x%04x\n", ircc_fir);
		MESSAGE(" Overriding SIR address 0x%04x\n", ircc_sir);

		if (ircc_open(ircc_fir, ircc_sir) == 0)
			return 0;

		return -ENODEV;
	}

	/* try user provided configuration register base address */
	if (ircc_cfg>0) {
	        MESSAGE(" Overriding configuration address 0x%04x\n", ircc_cfg);
		if (!smc_superio_fdc(ircc_cfg))
			ret=0;
	}

	/* Trys to open for all the SMC chipsets we know about */

	IRDA_DEBUG(0, "%s Try to open all known SMC chipsets\n", __FUNCTION__);

	if (!smc_superio_fdc(0x3f0))
		ret=0;
	if (!smc_superio_fdc(0x370))
		ret=0;
	if (!smc_superio_fdc(0xe0))
		ret=0;
	if (!smc_superio_lpc(0x2e))
		ret=0;
	if (!smc_superio_lpc(0x4e))
		ret=0;

	return ret;
}

/*
 * Function ircc_open (iobase, irq)
 *
 *    Try to open driver instance
 *
 */
static int __init ircc_open(unsigned int fir_base, unsigned int sir_base)
{
	struct ircc_cb *self;
        struct irport_cb *irport;
	unsigned char low, high, chip, config, dma, irq, version;


	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	if (check_region(fir_base, CHIP_IO_EXTENT) < 0) {
		IRDA_DEBUG(0, "%s: can't get fir_base of 0x%03x\n",
			__FUNCTION__, fir_base);
		return -ENODEV;
	}
#if POSSIBLE_USED_BY_SERIAL_DRIVER
	if (check_region(sir_base, CHIP_IO_EXTENT) < 0) {
		IRDA_DEBUG(0, "%s: can't get sir_base of 0x%03x\n",
			__FUNCTION__, sir_base);
		return -ENODEV;
	}
#endif

	register_bank(fir_base, 3);

	high    = inb(fir_base+IRCC_ID_HIGH);
	low     = inb(fir_base+IRCC_ID_LOW);
	chip    = inb(fir_base+IRCC_CHIP_ID);
	version = inb(fir_base+IRCC_VERSION);
	config  = inb(fir_base+IRCC_INTERFACE);

	irq     = config >> 4 & 0x0f;
	dma     = config & 0x0f;

        if (high != 0x10 || low != 0xb8 || (chip != 0xf1 && chip != 0xf2)) { 
	        IRDA_DEBUG(0, "%s(), addr 0x%04x - no device found!\n", 
	        	__FUNCTION__, fir_base);
		return -ENODEV;
	}
	MESSAGE("SMC IrDA Controller found\n IrCC version %d.%d, "
		"firport 0x%03x, sirport 0x%03x dma=%d, irq=%d\n",
		chip & 0x0f, version, fir_base, sir_base, dma, irq);

	if (dev_count>DIM(dev_self)) {
	        IRDA_DEBUG(0, "%s(), to many devices!\n", __FUNCTION__);
		return -ENOMEM;
	}

	/*
	 *  Allocate new instance of the driver
	 */
	self = kmalloc(sizeof(struct ircc_cb), GFP_KERNEL);
	if (self == NULL) {
		ERROR("%s, Can't allocate memory for control block!\n",
                      driver_name);
		return -ENOMEM;
	}
	memset(self, 0, sizeof(struct ircc_cb));
	spin_lock_init(&self->lock);

	/* Max DMA buffer size needed = (data_size + 6) * (window_size) + 6; */
	self->rx_buff.truesize = 4000; 
	self->tx_buff.truesize = 4000;

	self->rx_buff.head = (u8 *) kmalloc(self->rx_buff.truesize,
					      GFP_KERNEL|GFP_DMA);
	if (self->rx_buff.head == NULL) {
		ERROR("%s, Can't allocate memory for receive buffer!\n",
                      driver_name);
		kfree(self);
		return -ENOMEM;
	}

	self->tx_buff.head = (u8 *) kmalloc(self->tx_buff.truesize, 
					      GFP_KERNEL|GFP_DMA);
	if (self->tx_buff.head == NULL) {
		ERROR("%s, Can't allocate memory for transmit buffer!\n",
                      driver_name);
		kfree(self->rx_buff.head);
		kfree(self);
		return -ENOMEM;
	}

	irport = irport_open(dev_count, sir_base, irq);
	if (!irport) {
		kfree(self->tx_buff.head);
		kfree(self->rx_buff.head);
		kfree(self);
		return -ENODEV;
	}

	memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	memset(self->tx_buff.head, 0, self->tx_buff.truesize);
   
	/* Need to store self somewhere */
	dev_self[dev_count++] = self;

	/* Steal the network device from irport */
	self->netdev = irport->netdev;
	self->irport = irport;

	irport->priv = self;

	/* Initialize IO */
	self->io           = &irport->io;
	self->io->fir_base  = fir_base;
        self->io->sir_base  = sir_base;	/* Used by irport */
        self->io->fir_ext   = CHIP_IO_EXTENT;
        self->io->sir_ext   = 8;		/* Used by irport */

	if (ircc_irq < 255) {
		if (ircc_irq!=irq)
			MESSAGE("%s, Overriding IRQ - chip says %d, using %d\n",
				driver_name, irq, ircc_irq);
		self->io->irq = ircc_irq;
	}
	else
		self->io->irq = irq;
	if (ircc_dma < 255) {
		if (ircc_dma!=dma)
			MESSAGE("%s, Overriding DMA - chip says %d, using %d\n",
				driver_name, dma, ircc_dma);
		self->io->dma = ircc_dma;
	}
	else
		self->io->dma = dma;

	request_region(self->io->fir_base, CHIP_IO_EXTENT, driver_name);

	/* Initialize QoS for this device */
	irda_init_max_qos_capabilies(&irport->qos);
	
	/* The only value we must override it the baudrate */
	irport->qos.baud_rate.bits = IR_9600|IR_19200|IR_38400|IR_57600|
		IR_115200|IR_576000|IR_1152000|(IR_4000000 << 8);

	irport->qos.min_turn_time.bits = 0x07;
	irport->qos.window_size.bits = 0x01;
	irda_qos_bits_to_value(&irport->qos);

	irport->flags = IFF_FIR|IFF_MIR|IFF_SIR|IFF_DMA|IFF_PIO;
	

	self->rx_buff.in_frame = FALSE;
	self->rx_buff.state = OUTSIDE_FRAME;
	self->tx_buff.data = self->tx_buff.head;
	self->rx_buff.data = self->rx_buff.head;
	
	/* Override the speed change function, since we must control it now */
	irport->change_speed = &ircc_change_speed;
	irport->interrupt    = &ircc_interrupt;
	self->netdev->open   = &ircc_net_open;
	self->netdev->stop   = &ircc_net_close;

	irport_start(self->irport);

        self->pmdev = pm_register(PM_SYS_DEV, PM_SYS_IRDA, ircc_pmproc);
        if (self->pmdev)
                self->pmdev->data = self;

	/* Power on device */

	outb(0x00, fir_base+IRCC_MASTER);

	return 0;
}

/*
 * Function ircc_change_speed (self, baud)
 *
 *    Change the speed of the device
 *
 */
static void ircc_change_speed(void *priv, u32 speed)
{
	int iobase, ir_mode, ctrl, fast; 
	struct ircc_cb *self = (struct ircc_cb *) priv;
	struct net_device *dev;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	ASSERT(self != NULL, return;);

	dev = self->netdev;
	iobase = self->io->fir_base;

	/* Update accounting for new speed */
	self->io->speed = speed;

	outb(IRCC_MASTER_RESET, iobase+IRCC_MASTER);
	outb(0x00, iobase+IRCC_MASTER);

	switch (speed) {
	default:
		IRDA_DEBUG(0, "%s(), unknown baud rate of %d\n", 
			__FUNCTION__, speed);
		/* FALLTHROUGH */
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:		
		ir_mode = IRCC_CFGA_IRDA_SIR_A;
		ctrl = 0;
		fast = 0;
		break;
	case 576000:		
		ir_mode = IRCC_CFGA_IRDA_HDLC;
		ctrl = IRCC_CRC;
		fast = 0;
		IRDA_DEBUG(0, "%s(), handling baud of 576000\n", __FUNCTION__);
		break;
	case 1152000:
		ir_mode = IRCC_CFGA_IRDA_HDLC;
		ctrl = IRCC_1152 | IRCC_CRC;
		fast = 0;
		IRDA_DEBUG(0, "%s(), handling baud of 1152000\n", __FUNCTION__);
		break;
	case 4000000:
		ir_mode = IRCC_CFGA_IRDA_4PPM;
		ctrl = IRCC_CRC;
		fast = IRCC_LCR_A_FAST;
		IRDA_DEBUG(0, "%s(), handling baud of 4000000\n", __FUNCTION__);
		break;
	}
	
	register_bank(iobase, 0);
	outb(0, iobase+IRCC_IER);
	outb(IRCC_MASTER_INT_EN, iobase+IRCC_MASTER);

	/* Make special FIR init if necessary */
	if (speed > 115200) {
		irport_stop(self->irport);

		/* Install FIR transmit handler */
		dev->hard_start_xmit = &ircc_hard_xmit;

		/* 
		 * Don't know why we have to do this, but FIR interrupts 
		 * stops working if we remove it.
		 */
		/* outb(UART_MCR_OUT2, self->io->sir_base + UART_MCR); */

		/* Be ready for incoming frames */
		ircc_dma_receive(self, iobase);
	} else {
		/* Install SIR transmit handler */
		dev->hard_start_xmit = &irport_hard_xmit;
		irport_start(self->irport);
		
	        IRDA_DEBUG(0, "%s(), using irport to change speed to %d\n", __FUNCTION__, speed);
		irport_change_speed(self->irport, speed);
	}	

	register_bank(iobase, 1);
	outb(((inb(iobase+IRCC_SCE_CFGA) & 0x87) | ir_mode), 
	     iobase+IRCC_SCE_CFGA);

#ifdef SMC_669 /* Uses pin 88/89 for Rx/Tx */
	outb(((inb(iobase+IRCC_SCE_CFGB) & 0x3f) | IRCC_CFGB_MUX_COM), 
	     iobase+IRCC_SCE_CFGB);
#else	
	outb(((inb(iobase+IRCC_SCE_CFGB) & 0x3f) | IRCC_CFGB_MUX_IR),
	     iobase+IRCC_SCE_CFGB);
#endif	
	(void) inb(iobase+IRCC_FIFO_THRESHOLD);
	outb(64, iobase+IRCC_FIFO_THRESHOLD);
	
	register_bank(iobase, 4);
	outb((inb(iobase+IRCC_CONTROL) & 0x30) | ctrl, iobase+IRCC_CONTROL);
	
	register_bank(iobase, 0);
	outb(fast, iobase+IRCC_LCR_A);
	
	netif_start_queue(dev);
}

/*
 * Function ircc_hard_xmit (skb, dev)
 *
 *    Transmit the frame!
 *
 */
static int ircc_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct irport_cb *irport;
	struct ircc_cb *self;
	unsigned long flags;
	s32 speed;
	int iobase;
	int mtt;

	irport = (struct irport_cb *) dev->priv;
	self = (struct ircc_cb *) irport->priv;
	ASSERT(self != NULL, return 0;);

	iobase = self->io->fir_base;

	netif_stop_queue(dev);

	/* Check if we need to change the speed after this frame */
	speed = irda_get_next_speed(skb);
	if ((speed != self->io->speed) && (speed != -1)) {
		/* Check for empty frame */
		if (!skb->len) {
			ircc_change_speed(self, speed); 
			dev_kfree_skb(skb);
			return 0;
		} else
			self->new_speed = speed;
	}
	
	spin_lock_irqsave(&self->lock, flags);

	memcpy(self->tx_buff.head, skb->data, skb->len);

	self->tx_buff.len = skb->len;
	self->tx_buff.data = self->tx_buff.head;
	
	mtt = irda_get_mtt(skb);	
	if (mtt) {
		int bofs;

		/* 
		 * Compute how many BOFs (STA or PA's) we need to waste the
		 * min turn time given the speed of the link.
		 */
		bofs = mtt * (self->io->speed / 1000) / 8000;
		if (bofs > 4095)
			bofs = 4095;

		ircc_dma_xmit(self, iobase, bofs);
	} else {
		/* Transmit frame */
		ircc_dma_xmit(self, iobase, 0);
	}
	spin_unlock_irqrestore(&self->lock, flags);
	dev_kfree_skb(skb);

	return 0;
}

/*
 * Function ircc_dma_xmit (self, iobase)
 *
 *    Transmit data using DMA
 *
 */
static void ircc_dma_xmit(struct ircc_cb *self, int iobase, int bofs)
{
	u8 ctrl;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);
#if 0	
	/* Disable Rx */
	register_bank(iobase, 0);
	outb(0x00, iobase+IRCC_LCR_B);
#endif
	register_bank(iobase, 1);
	outb(inb(iobase+IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE, 
	     iobase+IRCC_SCE_CFGB);

	self->io->direction = IO_XMIT;

	/* Set BOF additional count for generating the min turn time */
	register_bank(iobase, 4);
	outb(bofs & 0xff, iobase+IRCC_BOF_COUNT_LO);
	ctrl = inb(iobase+IRCC_CONTROL) & 0xf0;
	outb(ctrl | ((bofs >> 8) & 0x0f), iobase+IRCC_BOF_COUNT_HI);

	/* Set max Tx frame size */
	outb(self->tx_buff.len >> 8, iobase+IRCC_TX_SIZE_HI);
	outb(self->tx_buff.len & 0xff, iobase+IRCC_TX_SIZE_LO);

	/* Setup DMA controller (must be done after enabling chip DMA) */
	setup_dma(self->io->dma, self->tx_buff.data, self->tx_buff.len, 
		  DMA_TX_MODE);

	outb(UART_MCR_OUT2, self->io->sir_base + UART_MCR);
	/* Enable burst mode chip Tx DMA */
	register_bank(iobase, 1);
	outb(inb(iobase+IRCC_SCE_CFGB) | IRCC_CFGB_DMA_ENABLE |
	     IRCC_CFGB_DMA_BURST, iobase+IRCC_SCE_CFGB);

	/* Enable interrupt */
	outb(IRCC_MASTER_INT_EN, iobase+IRCC_MASTER);
	register_bank(iobase, 0);
	outb(IRCC_IER_ACTIVE_FRAME | IRCC_IER_EOM, iobase+IRCC_IER);

	/* Enable transmit */
	outb(IRCC_LCR_B_SCE_TRANSMIT|IRCC_LCR_B_SIP_ENABLE, iobase+IRCC_LCR_B);
}

/*
 * Function ircc_dma_xmit_complete (self)
 *
 *    The transfer of a frame in finished. This function will only be called 
 *    by the interrupt handler
 *
 */
static void ircc_dma_xmit_complete(struct ircc_cb *self, int iobase)
{
	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);
#if 0
	/* Disable Tx */
	register_bank(iobase, 0);
	outb(0x00, iobase+IRCC_LCR_B);
#endif
	register_bank(self->io->fir_base, 1);
	outb(inb(self->io->fir_base+IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE,
	     self->io->fir_base+IRCC_SCE_CFGB);

	/* Check for underrrun! */
	register_bank(iobase, 0);
	if (inb(iobase+IRCC_LSR) & IRCC_LSR_UNDERRUN) {
		self->irport->stats.tx_errors++;
		self->irport->stats.tx_fifo_errors++;

		/* Reset error condition */
		register_bank(iobase, 0);
		outb(IRCC_MASTER_ERROR_RESET, iobase+IRCC_MASTER);
		outb(0x00, iobase+IRCC_MASTER);
	} else {
		self->irport->stats.tx_packets++;
		self->irport->stats.tx_bytes +=  self->tx_buff.len;
	}

	/* Check if it's time to change the speed */
	if (self->new_speed) {
		ircc_change_speed(self, self->new_speed);		
		self->new_speed = 0;
	}

	netif_wake_queue(self->netdev);
}

/*
 * Function ircc_dma_receive (self)
 *
 *    Get ready for receiving a frame. The device will initiate a DMA
 *    if it starts to receive a frame.
 *
 */
static int ircc_dma_receive(struct ircc_cb *self, int iobase) 
{	
#if 0
	/* Turn off chip DMA */
	register_bank(iobase, 1);
	outb(inb(iobase+IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE, 
	     iobase+IRCC_SCE_CFGB);
#endif
	setup_dma(self->io->dma, self->rx_buff.data, self->rx_buff.truesize, 
		  DMA_RX_MODE);

	/* Set max Rx frame size */
	register_bank(iobase, 4);
	outb((2050 >> 8) & 0x0f, iobase+IRCC_RX_SIZE_HI);
	outb(2050 & 0xff, iobase+IRCC_RX_SIZE_LO);

	self->io->direction = IO_RECV;
	self->rx_buff.data = self->rx_buff.head;

	/* Setup DMA controller */
	
	/* Enable receiver */
	register_bank(iobase, 0);
	outb(IRCC_LCR_B_SCE_RECEIVE | IRCC_LCR_B_SIP_ENABLE, 
	     iobase+IRCC_LCR_B);
	
	/* Enable burst mode chip Rx DMA */
	register_bank(iobase, 1);
	outb(inb(iobase+IRCC_SCE_CFGB) | IRCC_CFGB_DMA_ENABLE | 
	     IRCC_CFGB_DMA_BURST, iobase+IRCC_SCE_CFGB);

	return 0;
}

/*
 * Function ircc_dma_receive_complete (self)
 *
 *    Finished with receiving frames
 *
 */
static void ircc_dma_receive_complete(struct ircc_cb *self, int iobase)
{
	struct sk_buff *skb;
	int len, msgcnt;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);
#if 0
	/* Disable Rx */
	register_bank(iobase, 0);
	outb(0x00, iobase+IRCC_LCR_B);
#endif
	register_bank(iobase, 0);
	msgcnt = inb(iobase+IRCC_LCR_B) & 0x08;

	IRDA_DEBUG(2, "%s: dma count = %d\n",
		__FUNCTION__, get_dma_residue(self->io->dma));

	len = self->rx_buff.truesize - get_dma_residue(self->io->dma);
	
	/* Remove CRC */
	if (self->io->speed < 4000000)
		len -= 2;
	else
		len -= 4;

	if ((len < 2) || (len > 2050)) {
		WARNING("%s(), bogus len=%d\n", __FUNCTION__, len);
		return;
	}
	IRDA_DEBUG(2, "%s: msgcnt = %d, len=%d\n", __FUNCTION__, msgcnt, len);

	skb = dev_alloc_skb(len+1);
	if (!skb)  {
		WARNING("%s(), memory squeeze, dropping frame.\n", __FUNCTION__);
		return;
	}			
	/* Make sure IP header gets aligned */
	skb_reserve(skb, 1); 

	memcpy(skb_put(skb, len), self->rx_buff.data, len);
	self->irport->stats.rx_packets++;
	self->irport->stats.rx_bytes += len;

	skb->dev = self->netdev;
	skb->mac.raw  = skb->data;
	skb->protocol = htons(ETH_P_IRDA);
	netif_rx(skb);
}

/*
 * Function ircc_interrupt (irq, dev_id, regs)
 *
 *    An interrupt from the chip has arrived. Time to do some work
 *
 */
static void ircc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct irport_cb *irport;
	struct ircc_cb *self;
	int iobase, iir;

	if (dev == NULL) {
		printk(KERN_WARNING "%s: irq %d for unknown device.\n", 
		       driver_name, irq);
		return;
	}
	irport = (struct irport_cb *) dev->priv;
	ASSERT(irport != NULL, return;);
	self = (struct ircc_cb *) irport->priv;
	ASSERT(self != NULL, return;);

	/* Check if we should use the SIR interrupt handler */
	if (self->io->speed < 576000) {
		irport_interrupt(irq, dev_id, regs);
		return;
	}
	iobase = self->io->fir_base;

	spin_lock(&self->lock);	

	register_bank(iobase, 0);
	iir = inb(iobase+IRCC_IIR);

	/* Disable interrupts */
	outb(0, iobase+IRCC_IER);

	IRDA_DEBUG(2, "%s(), iir = 0x%02x\n", __FUNCTION__, iir);

	if (iir & IRCC_IIR_EOM) {
		if (self->io->direction == IO_RECV)
			ircc_dma_receive_complete(self, iobase);
		else
			ircc_dma_xmit_complete(self, iobase);
		
		ircc_dma_receive(self, iobase);
	}

	/* Enable interrupts again */
	register_bank(iobase, 0);
	outb(IRCC_IER_ACTIVE_FRAME|IRCC_IER_EOM, iobase+IRCC_IER);

	spin_unlock(&self->lock);
}

#if 0 /* unused */
/*
 * Function ircc_is_receiving (self)
 *
 *    Return TRUE is we are currently receiving a frame
 *
 */
static int ircc_is_receiving(struct ircc_cb *self)
{
	int status = FALSE;
	/* int iobase; */

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	ASSERT(self != NULL, return FALSE;);

	IRDA_DEBUG(0, "%s: dma count = %d\n",
		__FUNCTION__, get_dma_residue(self->io->dma));

	status = (self->rx_buff.state != OUTSIDE_FRAME);
	
	return status;
}
#endif /* unused */

/*
 * Function ircc_net_open (dev)
 *
 *    Start the device
 *
 */
static int ircc_net_open(struct net_device *dev)
{
	struct irport_cb *irport;
	struct ircc_cb *self;
	int iobase;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);
	
	ASSERT(dev != NULL, return -1;);
	irport = (struct irport_cb *) dev->priv;
	self = (struct ircc_cb *) irport->priv;

	ASSERT(self != NULL, return 0;);
	
	iobase = self->io->fir_base;

	irport_net_open(dev); /* irport allocates the irq */

	/*
	 * Always allocate the DMA channel after the IRQ,
	 * and clean up on failure.
	 */
	if (request_dma(self->io->dma, dev->name)) {
		irport_net_close(dev);

		WARNING("%s(), unable to allocate DMA=%d\n", __FUNCTION__, self->io->dma);
		return -EAGAIN;
	}
	
	MOD_INC_USE_COUNT;

	return 0;
}

/*
 * Function ircc_net_close (dev)
 *
 *    Stop the device
 *
 */
static int ircc_net_close(struct net_device *dev)
{
	struct irport_cb *irport;
	struct ircc_cb *self;
	int iobase;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);
	
	ASSERT(dev != NULL, return -1;);
	irport = (struct irport_cb *) dev->priv;
	self = (struct ircc_cb *) irport->priv;
	
	ASSERT(self != NULL, return 0;);
	
	iobase = self->io->fir_base;

	irport_net_close(dev);

	disable_dma(self->io->dma);

	free_dma(self->io->dma);

	MOD_DEC_USE_COUNT;

	return 0;
}

static void ircc_suspend(struct ircc_cb *self)
{
	MESSAGE("%s, Suspending\n", driver_name);

	if (self->io->suspended)
		return;

	ircc_net_close(self->netdev);

	self->io->suspended = 1;
}

static void ircc_wakeup(struct ircc_cb *self)
{
	unsigned long flags;

	if (!self->io->suspended)
		return;

	save_flags(flags);
	cli();

	ircc_net_open(self->netdev);
	
	restore_flags(flags);
	MESSAGE("%s, Waking up\n", driver_name);
}

static int ircc_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data)
{
        struct ircc_cb *self = (struct ircc_cb*) dev->data;
        if (self) {
                switch (rqst) {
                case PM_SUSPEND:
                        ircc_suspend(self);
                        break;
                case PM_RESUME:
                        ircc_wakeup(self);
                        break;
                }
        }
	return 0;
}

#ifdef MODULE

/*
 * Function ircc_close (self)
 *
 *    Close driver instance
 *
 */
#ifdef MODULE
static int __exit ircc_close(struct ircc_cb *self)
{
	int iobase;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	ASSERT(self != NULL, return -1;);

        iobase = self->irport->io.fir_base;

	irport_close(self->irport);

	/* Stop interrupts */
	register_bank(iobase, 0);
	outb(0, iobase+IRCC_IER);
	outb(IRCC_MASTER_RESET, iobase+IRCC_MASTER);
	outb(0x00, iobase+IRCC_MASTER);
#if 0
	/* Reset to SIR mode */
	register_bank(iobase, 1);
        outb(IRCC_CFGA_IRDA_SIR_A|IRCC_CFGA_TX_POLARITY, iobase+IRCC_SCE_CFGA);
        outb(IRCC_CFGB_IR, iobase+IRCC_SCE_CFGB);
#endif
	/* Release the PORT that this driver is using */
	IRDA_DEBUG(0, "%s(), releasing 0x%03x\n", __FUNCTION__, iobase);

	release_region(iobase, CHIP_IO_EXTENT);

	if (self->tx_buff.head)
		kfree(self->tx_buff.head);
	
	if (self->rx_buff.head)
		kfree(self->rx_buff.head);

	kfree(self);

	return 0;
}
#endif /* MODULE */

static int __init smc_init(void)
{
	return ircc_init();
}

static void __exit smc_cleanup(void)
{
	int i;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	for (i=0; i < 2; i++) {
		if (dev_self[i])
			ircc_close(dev_self[i]);
	}
}

module_init(smc_init);
module_exit(smc_cleanup);
 
MODULE_AUTHOR("Thomas Davis <tadavis@jps.net>");
MODULE_DESCRIPTION("SMC IrCC controller driver");
MODULE_LICENSE("GPL");

MODULE_PARM(ircc_dma, "1i");
MODULE_PARM_DESC(ircc_dma, "DMA channel");
MODULE_PARM(ircc_irq, "1i");
MODULE_PARM_DESC(ircc_irq, "IRQ line");
MODULE_PARM(ircc_fir, "1-4i");
MODULE_PARM_DESC(ircc_fir, "FIR Base Address");
MODULE_PARM(ircc_sir, "1-4i");
MODULE_PARM_DESC(ircc_sir, "SIR Base Address");
MODULE_PARM(ircc_cfg, "1-4i");
MODULE_PARM_DESC(ircc_cfg, "Configuration register base address");

#endif /* MODULE */
