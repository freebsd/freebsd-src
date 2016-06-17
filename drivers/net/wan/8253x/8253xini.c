/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/pgtable.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include "Reg9050.h"
#include "8253xctl.h"
#include "ring.h"
#include "8253x.h"
#include "crc32dcl.h"
#include "8253xmcs.h"
#include "sp502.h"

/* card names */

char *aura_functionality[] =
{
	"NR",
	"AO",
	"NA",
	"UN"
};

char *board_type[] =
{
	"unknown",
	"1020P",
	"1520P",
	"2020P",
	"2520P",
	"4020P",
	"4520P",
	"8020P",
	"8520P",
	"SUNSE",
	"WANMS",
	"1020C",
	"1520C",
	"2020C",
	"2520C",
	"4020C",
	"4520C",
	"8020C",
	"8520C",
};

unsigned int sab8253x_rebootflag = 0;

AURAXX20PARAMS AuraXX20DriverParams; /* loaded at startup */
				/* from variables below */
SAB_BOARD *AuraBoardRoot = NULL; /* The order of this list is not important */
SAB_CHIP  *AuraChipRoot = NULL;	/* chips grouped by board chip0 before chip1 */
SAB_PORT  *AuraPortRoot = NULL;	/* ports grouped by board and by chip, chip0, chip1, etc */
AURA_CIM  *AuraCimRoot = NULL;	/* only used for deallocating the cim structures, etc */
/* CIM stands for Communications Interface Module -- the G.Link logic provided by the Altera parts. */

/* Arrays of lists of boards of each type on a given interrupt */
SAB_BOARD *AuraBoardESCC2IrqRoot[NUMINTS]; 
SAB_BOARD *AuraBoardESCC8IrqRoot[NUMINTS];
SAB_BOARD *AuraBoardMCSIrqRoot[NUMINTS];

unsigned int NumSab8253xPorts = 0;

unsigned BD1020Pcounter = 0;	/* keep count of each board */
unsigned BD1520Pcounter = 0;	/* may change to just keeping count */
unsigned BD2020Pcounter = 0;	/* of the total number of boards */
unsigned BD2520Pcounter = 0;
unsigned BD4020Pcounter = 0;
unsigned BD4520Pcounter = 0;
unsigned BD8020Pcounter = 0;
unsigned BD8520Pcounter = 0;

unsigned BD1020CPcounter = 0;	/* keep count of each board */
unsigned BD1520CPcounter = 0;	/* may change to just keeping count */
unsigned BD2020CPcounter = 0;	/* of the total number of boards */
unsigned BD2520CPcounter = 0;
unsigned BD4020CPcounter = 0;
unsigned BD4520CPcounter = 0;
unsigned BD8020CPcounter = 0;
unsigned BD8520CPcounter = 0;

unsigned BDMCScounter = 0;


static int auraXX20n_debug = 0;	/* turns on lots of */
				/* debugging messages*/
static char* auraXX20n_name = 0;/* set net dev name on command line */
static char *sab8253xc_name = "sab8253xc";
static int sab8253xc_major = 0;
int sab8253xn_listsize = 32; /* recommend descriptor list size */
int sab8253xn_rbufsize = RXSIZE; /* recommend rbuf list size */
int sab8253xt_listsize = 256; /* recommend descriptor list size */
int sab8253xt_rbufsize = 32; /* recommend rbuf list size for tty */
int sab8253xs_listsize = 64; /* recommend descriptor list size */
int sab8253xs_rbufsize = RXSIZE; /* recommend rbuf list size */
int sab8253xc_listsize = 64; /* recommend descriptor list size */
int sab8253xc_rbufsize = RXSIZE; /* recommend rbuf list size for tty */
int xx20_minorstart = 128;
int sab8253x_vendor_id = PCI_VENDOR_ID_AURORATECH;
int sab8253x_cpci_device_id = PCI_DEVICE_ID_AURORATECH_CPCI;
int sab8253x_wmcs_device_id = PCI_DEVICE_ID_AURORATECH_WANMS;
int sab8253x_mpac_device_id = PCI_DEVICE_ID_AURORATECH_MULTI;
int sab8253x_default_sp502_mode = SP502_RS232_MODE;

MODULE_PARM(sab8253x_vendor_id, "i");
MODULE_PARM(sab8253x_cpci_device_id, "i");
MODULE_PARM(sab8253x_wmcs_device_id, "i");
MODULE_PARM(sab8253x_mpac_device_id, "i");
MODULE_PARM(sab8253x_default_sp502_mode, "i");

MODULE_PARM(xx20_minorstart, "i");
MODULE_PARM(sab8253xc_major, "i");
MODULE_PARM(auraXX20n_debug, "i");
MODULE_PARM(auraXX20n_name, "s"); /* this and the following for sync */
MODULE_PARM(sab8253xn_listsize, "i"); /* network driver */
MODULE_PARM(sab8253xn_rbufsize, "i"); /* network driver */
MODULE_PARM(sab8253xt_listsize, "i"); /* tty driver */
MODULE_PARM(sab8253xt_rbufsize, "i"); /* tty driver */
MODULE_PARM(sab8253xc_listsize, "i"); /* network driver */
MODULE_PARM(sab8253xc_rbufsize, "i"); /* network driver */
MODULE_PARM(sab8253xs_listsize, "i"); /* tty driver */
MODULE_PARM(sab8253xs_rbufsize, "i"); /* tty driver */
MODULE_PARM(sab8253xc_name, "s"); 

struct pci_dev   *XX20lastpdev = NULL; /* just used for finding all PCI devices */
static SAB_BOARD *find_ati_multiport_card(void); /* actually implemented */
static SAB_BOARD *find_ati_cpci_card(void); /* to be done */
static SAB_BOARD *find_ati_wanms_card(void); /* to be done */

#if (!defined(MODULE)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0))
				/* unpleasantness for 2.2 kernels
				 * and probe illogic */

				/* The LRP project is still working on
				   2.2.* kernels but I suspect
				   that initially we will see more
				   purchases for complete Linux
				   machines using 2.4.*, LRP
				   machines tend to be underpowered
				   and have a paucity of PCI slots
				*/

unsigned int do_probe = 1;
#endif

				/* One could argue that these could be in  */
				/* the 8253xnet.c file but they are fairly */
				/* intimately involved with initialization.*/
struct net_device *Sab8253xRoot = NULL;

struct net_device auraXX20n_prototype = /* used for the network device */
{
	"8253x0",			
	0, 0, 0, 0,
	0x000,
	-1, /* bad irq */
	0, 0, 0,
	NULL,
	sab8253xn_init /* network driver initialization */
};

struct file_operations sab8253xc_fops =
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0))
	NULL,
#endif
	NULL,			/* llseek */
	sab8253xc_read,		/* read */
	sab8253xc_write,	/* write */
	NULL,			/* readdir */
	sab8253xc_poll,		/* poll */
	sab8253xc_ioctl,	/* ioctl */
	NULL,			/* mmap */
	sab8253xc_open,		/* open */
	NULL,			/* flush */
	sab8253xc_release,	/* release */
	NULL,			/* fsync */
	sab8253xc_fasync,	/* fasync */
	NULL,			/* check_media_change */
	NULL,			/* revalidate */
	NULL			/* lock */
};


/* A few function defined in this file */
/* These functions are basically functionality */
/* independent -- they are used with asynchronous ttys */
/* synchronous ttys, the network device and the */
/* raw character device */

				/* used for reading and writing ports
				   readw and writew require some reprogramming
				   of the PLX9050
				*/
static unsigned char aura_readb(struct sab_port *port, unsigned char *reg);
static unsigned char wmsaura_readb(struct sab_port *port, unsigned char *reg);
static unsigned short aura_readw(struct sab_port *port, unsigned short *reg);
static unsigned short wmsaura_readw(struct sab_port *port, unsigned short *reg);
static void aura_writeb(struct sab_port *port, unsigned char *reg,unsigned char val);
static void wmsaura_writeb(struct sab_port *port, unsigned char *reg,unsigned char val);
static void aura_writew(struct sab_port *port, unsigned short *reg,unsigned short val);
static void wmsaura_writew(struct sab_port *port, unsigned short *reg,unsigned short val);

static void aura_readfifo(struct sab_port *port, unsigned char *buf, unsigned int nbytes);
static void aura_writefifo(struct sab_port *port);

static void wmsaura_readfifo(struct sab_port *port, unsigned char *buf, unsigned int nbytes);
static void wmsaura_writefifo(struct sab_port *port);

/* function definitions */

/* [124]X20 type cards */
static void DisableESCC2Interrupts(SAB_CHIP *chipptr) /* in processing ports may have to disable ints */
{
	struct sab82532_async_wr_regs *regs;
	
	regs = chipptr->c_regs;
	writeb(0xff,&regs->pim);	/* All interrupts off */
				/* Note that regs/->c_regs
				   is set to base reg
				   address, thus taking
				   address or regs->pim
				   gets the address of
				   the PIM register/int mask */
}

static SAB_CHIP* CreateESCC2(SAB_BOARD *bptr, unsigned int offset)
{
	SAB_CHIP *cptr;
	struct sab82532_async_wr_regs *regs;
	
	printk(KERN_ALERT 
	       "auraXX20n: creating ESCC2 structure on board %p at offset %x.\n",
	       bptr, offset);
	
	cptr = (SAB_CHIP*) kmalloc(sizeof(SAB_CHIP), GFP_KERNEL);
	if(cptr == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: Failed to create ESCC2 structure on board %p at offset %x.\n",
		       bptr, offset);
		return NULL;
	}
	memset(cptr, 0, sizeof(SAB_CHIP));
	cptr->chip_type = ESCC2;
	cptr->c_board = bptr;
	cptr->c_cim = NULL;
	cptr->c_chipno = (offset ? 1 : 0); /* first or second chip on board */
	cptr->c_revision = 
		(readb(((char *)bptr->virtbaseaddress2) + offset + SAB85232_REG_VSTR) &
		 SAB82532_VSTR_VN_MASK);
	cptr->c_nports = 2;
	cptr->c_portbase = NULL;
	cptr->next = AuraChipRoot;	/* chips are added to chiplist in reverse order */
	AuraChipRoot = cptr;
	cptr->next_by_board = bptr->board_chipbase; /* likewise for the local board chiplist */
	bptr->board_chipbase = cptr;
	printk(KERN_ALERT "auraXX20n: chip %d on board %p is revision %d.\n",
	       cptr->c_chipno, bptr, cptr->c_revision);
	
	/* lets set up the generic parallel
	 * port register which is used to
	 * control signaling and other stuff*/
	/*
	 * SAB82532 (Aurora) 1 8-bit parallel port
	 * To summarize the use of the parallel port:
	 *                    RS-232
	 *  A       B        I/O     descr
	 * P0      P4      output  TxClk ctrl
	 * P1      P5      output  DTR
	 * P2      P6      input   DSR
	 * P3      P7      output  485 control
	 *
	 */
	/*
	 * Configuring the parallel port 
	 */
	
	regs = (struct sab82532_async_wr_regs *)(((char *)bptr->virtbaseaddress2) + offset);
	
	DEBUGPRINT((KERN_ALERT "Writing 0x44 to 0x%p + 0x%x for chip %d\n",
		    regs, SAB82532_REG_PCR, cptr->c_chipno));
	
	writeb(0x44,&regs->pcr);  /* 2 input bits */
	writeb(0xff,&regs->pim);/* All interrupts off */
	writeb(0x33,&regs->pvr); /* Txclk and DTR low  */
	
	cptr->c_regs = (void*) regs;
	cptr->int_disable = DisableESCC2Interrupts;
	return cptr;
}

static void CreateESCC2Port(SAB_CHIP *cptr, unsigned int portno, unsigned int function)
{
	SAB_BOARD *bptr;
	SAB_PORT  *pptr;
	extern void sab8253x_setup_ttyport(struct sab_port *p_port) ;
	
	++NumSab8253xPorts;
	bptr = cptr->c_board;
	pptr = (SAB_PORT*) kmalloc(sizeof(SAB_PORT), GFP_KERNEL);
	if(pptr == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: Failed to create ESCC2 port structure on chip %p on board %p.\n",
		       cptr, bptr);
		return;
	}
	memset(pptr, 0, sizeof(SAB_PORT));
	DEBUGPRINT
		((KERN_ALERT "Setting up port %d, chipno %d for %s type board number %d.\n", 
		  portno, cptr->c_chipno, board_type[bptr->b_type],bptr->board_number));
	pptr->portno = portno;
	pptr->chip=cptr;
	pptr->board=bptr;
	pptr->open_type=OPEN_NOT;
	pptr->is_console=0;
	pptr->regs=
		(union sab82532_regs *)
		(((unsigned int)cptr->c_regs) +
		 (portno * SAB82532_REG_SIZE));
	pptr->type = cptr->c_revision;
	pptr->function = function;
	
	/* Simpify reading */
#define PVR  pptr->regs->async_write.pvr
#define PCR  pptr->regs->async_write.pcr
#define PIM  pptr->regs->async_write.pim
#define ISR0 pptr->regs->async_read.isr0
#define IMR0 pptr->regs->async_write.imr0
#define IMR1 pptr->regs->async_write.imr1
#define PIS  pptr->regs->async_read.pis
#define VSTR pptr->regs->async_read.vstr
#define STAR pptr->regs->async_read.star
#define MODE pptr->regs->async_read.mode
	
	pptr->irq = bptr->b_irq;
	if(portno == 0) 
	{ /* Port A .... */
		pptr->dsr.reg=(unsigned char *)&(PVR);
		pptr->dsr.mask=0x04;
		pptr->dsr.irq=PIS_IDX;
		pptr->dsr.irqmask=0x04;
		
		pptr->dtr.reg=(unsigned char *)&(PVR);
		pptr->dtr.mask=0x02;
		
		pptr->txclkdir.reg=(unsigned char *)&(PVR);
		pptr->txclkdir.mask=0x01;
	} 
	else 
	{ /* Port B */
		pptr->dsr.reg=(unsigned char *)&(PVR);
		pptr->dsr.mask=0x40;
		pptr->dsr.irq=PIS_IDX;
		pptr->dsr.irqmask=0x40;
		
		pptr->dtr.reg=(unsigned char *)&(PVR);
		pptr->dtr.mask=0x20;
		
		pptr->txclkdir.reg=(unsigned char *)&(PVR);
		pptr->txclkdir.mask=0x10;
	}
	pptr->dsr.inverted=1;
	pptr->dsr.cnst = 0;
	pptr->dtr.inverted=1;
	pptr->dtr.cnst = 0;
	pptr->txclkdir.inverted=1;
	
	pptr ->dcd.reg =(unsigned char *) &(VSTR);
	
	DEBUGPRINT((KERN_ALERT "cd register set to 0x%p\n", pptr ->dcd.reg));
	
	pptr->dcd.mask = SAB82532_VSTR_CD;
	pptr->dcd.inverted = 1;
	pptr->dcd.irq=ISR0_IDX;
	pptr->dcd.irqmask=SAB82532_ISR0_CDSC;
	pptr->dcd.cnst = 0;
	
	pptr->cts.reg = (unsigned char *)&(STAR);
	pptr->cts.mask = SAB82532_STAR_CTS;
	pptr->cts.inverted = 0;
	pptr->cts.irq=ISR1_IDX;
	pptr->cts.irqmask=SAB82532_ISR1_CSC;
	pptr->cts.cnst = 0;
	
	pptr->rts.reg = (unsigned char *)&(MODE);
	pptr->rts.mask = SAB82532_MODE_FRTS;
	pptr->rts.inverted = 1;
	pptr->rts.cnst = SAB82532_MODE_RTS;
	
	
	/* Set the read and write function */
	pptr->readbyte=aura_readb;
	pptr->readword=aura_readw;
	pptr->writebyte=aura_writeb;
	pptr->writeword=aura_writew;
	pptr->readfifo=aura_readfifo;
	pptr->writefifo=aura_writefifo;
	
	sab8253x_setup_ttyport(pptr);	/* asynchronous */
	/* ttys are default, basic */
	/* initialization, everything */
	/* else works as a modification */
	/* thereof */
	
	pptr->next = AuraPortRoot;
	AuraPortRoot = pptr;
	pptr->next_by_chip = cptr->c_portbase;
	cptr->c_portbase = pptr;
	pptr->next_by_board = bptr->board_portbase;
	bptr->board_portbase = pptr;
}

/* 8x20 type functions */

static void DisableESCC8Interrupts(SAB_CHIP *chipptr)
{
	unsigned int regbase;		/* a lot more to do for ESCC8 */
	
	regbase = (unsigned int) chipptr->c_regs;
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_A); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_B); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_C); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_D); /* All interrupts off */
}

static SAB_CHIP* CreateESCC8(SAB_BOARD *bptr, unsigned int offset)
{
	SAB_CHIP *cptr;
	unsigned int regbase;
	
	printk(KERN_ALERT 
	       "auraXX20n: creating ESCC8 structure on board %p at offset %x.\n",
	       bptr, offset);
	
	cptr = (SAB_CHIP*) kmalloc(sizeof(SAB_CHIP), GFP_KERNEL);
	if(cptr == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: Failed to create ESCC8 structure on board %p at offset %x.\n",
		       bptr, offset);
		return NULL;
	}
	memset(cptr, 0, sizeof(SAB_CHIP));
	cptr->chip_type = ESCC8;
	cptr->c_board = bptr;
	cptr->c_cim = NULL;
	cptr->c_chipno = (offset ? 1 : 0); /* no card actually has 2 ESCC8s on it */
	cptr->c_revision = 
		(readb(((char *)bptr->virtbaseaddress2) + offset + SAB85232_REG_VSTR) & 
		 SAB82532_VSTR_VN_MASK);
	cptr->c_nports = 8;
	cptr->c_portbase = NULL;	/* used for the list of ports associated
					   with this chip
					*/
	cptr->next = AuraChipRoot;
	AuraChipRoot = cptr;
	cptr->next_by_board = bptr->board_chipbase;
	bptr->board_chipbase = cptr;
	printk(KERN_ALERT "auraXX20n: chip %d on board %p is revision %d.\n",
	       cptr->c_chipno, bptr, cptr->c_revision);
	
	/* lets set up the generic parallel
	 * port register which is used to
	 * control signaling and other stuff*/
	
	/* SAB82538 4 8-bits parallel ports
	 * To summarize the use of the parallel port:
	 *                    RS-232
	 * Parallel port A -- TxClkdir control	(output) ports 0 - 7
	 * Parallel port B -- DTR		(output) ports 0 - 7
	 * Parallel port C -- DSR		(input)  ports 0 - 7
	 * Parallel port D -- driver power down 	(output) drivers 0 - 3
	 *
	 * Note port D is not used on recent boards
	 */
	
	regbase = (unsigned int)(((char *)bptr->virtbaseaddress2) + offset);
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port A (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_A, SAB82538_REG_PIM_A, SAB82538_REG_PVR_A));
	
	/* Configuring Parallel Port A  (Clkdir)*/
	writeb(0x0,((unsigned char *)regbase) + SAB82538_REG_PCR_A);  /* All output bits */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_A); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PVR_A);  /* All low */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port B (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_B,SAB82538_REG_PIM_B,SAB82538_REG_PVR_B));
	
	writeb(0x0,((unsigned char *)regbase) + SAB82538_REG_PCR_B);  /* All output bits */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_B); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PVR_B);  /* All low */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port C (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_C, SAB82538_REG_PIM_C, SAB82538_REG_PVR_C));
	
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PCR_C);  /* All intput bits */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_C); /* All interrupts off */
	/* don't set port value register on input register */
	
	/* Configuring Parallel Port D */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port D (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_D, SAB82538_REG_PIM_D, SAB82538_REG_PVR_D));
	writeb(0x0f,((unsigned char *)regbase) + SAB82538_REG_PCR_D);  /* 4 input  bits */
	writeb(0xff,((unsigned char *)regbase) + SAB82538_REG_PIM_D); /* All interrupts off */
	/* don't set port value register on input register */
	
	/* The priority rotation thing */
	
	DEBUGPRINT((KERN_ALERT "Setting IVA (0x%x +  0x%x = 0x%x\n", regbase,
		    SAB82532_REG_IVA, regbase + SAB82532_REG_IVA));
	
	writeb(SAB82538_IVA_ROT, ((unsigned char *)regbase) + SAB82532_REG_IVA); 
	
	cptr->c_regs = (void*) regbase;
	cptr->int_disable = DisableESCC8Interrupts;
	return cptr;
}

static void DisableESCC8InterruptsFromCIM(SAB_CHIP *chipptr)
{
	unsigned int regbase;		/* a lot more to do for ESCC8 */
	
	regbase = (unsigned int) chipptr->c_regs;
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_A)); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_B)); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_C)); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_D)); /* All interrupts off */
}

static void CreateESCC8Port(SAB_CHIP *cptr, unsigned int portno, unsigned int function)
{
	SAB_BOARD *bptr;
	SAB_PORT  *pptr;
	extern void sab8253x_setup_ttyport(struct sab_port *p_port) ;
	
	++NumSab8253xPorts;
	bptr = cptr->c_board;
	pptr = (SAB_PORT*) kmalloc(sizeof(SAB_PORT), GFP_KERNEL);
	if(pptr == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: Failed to create ESCC2 port structure on chip %p on board %p.\n",
		       cptr, bptr);
		return;
	}
	memset(pptr, 0, sizeof(SAB_PORT));
	DEBUGPRINT
		((KERN_ALERT "Setting up port %d, chipno %d for %s type board number %d.\n", 
		  portno, cptr->c_chipno, board_type[bptr->b_type],bptr->board_number));
	pptr->portno = portno;
	pptr->chip=cptr;
	pptr->board=bptr;
	pptr->open_type=OPEN_NOT;
	pptr->is_console=0;
	pptr->regs=
		(union sab82532_regs *)
		(((unsigned int)cptr->c_regs) +
		 (portno * SAB82538_REG_SIZE));
	pptr->type = cptr->c_revision;
	pptr->function = function;
	
	pptr->irq = bptr->b_irq;
	
	pptr->dsr.reg = ((unsigned char *)cptr->c_regs) + SAB82538_REG_PVR_C; 
	pptr->dsr.mask = 0x1 << portno;
	pptr->dsr.inverted = 1;
	pptr->dsr.irq=PIS_IDX;	/* need to check this constant */
	pptr->dsr.irqmask=0x1 << portno;
	pptr->dsr.cnst = 0;
	
	pptr->txclkdir.reg = ((unsigned char *)cptr->c_regs) + SAB82538_REG_PVR_A;
	pptr->txclkdir.mask = 0x1 << portno;
	/* NOTE: Early 8 ports  boards had different tx clkdir sense */
	pptr->txclkdir.inverted = 1;
	
	pptr->dtr.reg = ((unsigned char *)cptr->c_regs) + SAB82538_REG_PVR_B;
	pptr->dtr.mask = 0x1 << portno;
	pptr->dtr.inverted = 1;
	pptr->dtr.cnst = 0;
	
	pptr ->dcd.reg = (unsigned char *)&(VSTR);
	
	DEBUGPRINT((KERN_ALERT "cd register set to 0x%p\n", pptr ->dcd.reg));
	
	pptr->dcd.mask = SAB82532_VSTR_CD;
	pptr->dcd.inverted = 1;
	pptr->dcd.irq=ISR0_IDX;
	pptr->dcd.irqmask=SAB82532_ISR0_CDSC;
	pptr->dcd.cnst = 0;
	
	pptr->cts.reg = (unsigned char *)&(STAR);
	pptr->cts.mask = SAB82532_STAR_CTS;
	pptr->cts.inverted = 0;
	pptr->cts.irq=ISR1_IDX;
	pptr->cts.irqmask=SAB82532_ISR1_CSC;
	pptr->cts.cnst = 0;
	
	pptr->rts.reg = (unsigned char *)&(MODE);
	pptr->rts.mask = SAB82532_MODE_FRTS;
	pptr->rts.inverted = 1;
	pptr->rts.cnst = SAB82532_MODE_RTS;
	
	
	/* Set the read and write function */
	pptr->readbyte=aura_readb;
	pptr->readword=aura_readw;
	pptr->writebyte=aura_writeb;
	pptr->writeword=aura_writew;
	pptr->readfifo=aura_readfifo;
	pptr->writefifo=aura_writefifo;
	
	sab8253x_setup_ttyport(pptr);	/* asynchronous */
	/* ttys are default, basic */
	/* initialization, everything */
	/* else works as a modification */
	/* thereof */
	
	pptr->next = AuraPortRoot;
	AuraPortRoot = pptr;
	pptr->next_by_chip = cptr->c_portbase;
	cptr->c_portbase = pptr;
	pptr->next_by_board = bptr->board_portbase;
	bptr->board_portbase = pptr;
}

/* Multichannel server functions */

static SAB_CHIP* CreateESCC8fromCIM(SAB_BOARD *bptr, AURA_CIM *cim, unsigned int chipno)
{
	SAB_CHIP *cptr;
	unsigned int regbase;
	
	printk(KERN_ALERT 
	       "auraXX20n: creating ESCC8 %d structure on board %p from cim %p.\n",
	       chipno, bptr, cim);
	
	cptr = (SAB_CHIP*) kmalloc(sizeof(SAB_CHIP), GFP_KERNEL);
	if(cptr == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: Failed to create ESCC8 structure %d on board %p at from cim %p.\n",
		       chipno, bptr, cim);
		return NULL;
	}
	
	memset(cptr, 0, sizeof(SAB_CHIP));
	cptr->chip_type = ESCC8;
	cptr->c_board = bptr;
	cptr->c_cim = cim;
	cptr->c_chipno = chipno;
	cptr->c_revision = 
		(readb((unsigned char *) (bptr->CIMCMD_REG +
					  (CIMCMD_RDREGB | (((chipno*8) << 6) | SAB85232_REG_VSTR))))
		 & SAB82532_VSTR_VN_MASK);
	cptr->c_nports = 8;
	cptr->c_portbase = NULL;	/* used for the list of ports associated
					   with this chip
					*/
	cptr->next = AuraChipRoot;
	AuraChipRoot = cptr;
	cptr->next_by_board = bptr->board_chipbase;
	bptr->board_chipbase = cptr;
	
	cptr->next_by_cim = cim->ci_chipbase;
	cim->ci_chipbase = cptr;
	
	printk(KERN_ALERT "auraXX20n: chip %d on board %p is revision %d.\n",
	       cptr->c_chipno, bptr, cptr->c_revision);
	
	/* lets set up the generic parallel
	 * port register which is used to
	 * control signaling and other stuff*/
	
	/* SAB82538 4 8-bits parallel ports
	 * To summarize the use of the parallel port:
	 *                    RS-232
	 * Parallel port A -- TxClkdir control	(output) ports 0 - 7
	 * Parallel port B -- DTR		(output) ports 0 - 7
	 * Parallel port C -- DSR		(input)  ports 0 - 7
	 * Parallel port D -- driver power down 	(output) drivers 0 - 3
	 *
	 * Note port D is not used on recent boards
	 */
	
	regbase = (unsigned int)
		(bptr->CIMCMD_REG + (0 | (((chipno*8) << 6) | 0)));	/* need to add in RDB/WRB cmd bits
									 * and reg offset (> 32) */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port A (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_A, SAB82538_REG_PIM_A, SAB82538_REG_PVR_A));
	
	/* Configuring Parallel Port A  (Clkdir)*/
	writeb(0x00,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PCR_A));  /* All output bits */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_A)); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PVR_A));  /* All low */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port B (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_B,SAB82538_REG_PIM_B,SAB82538_REG_PVR_B));
	
	writeb(0x00,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PCR_B));  /* All output bits */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_B)); /* All interrupts off */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PVR_B));  /* All low */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port C (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_C, SAB82538_REG_PIM_C, SAB82538_REG_PVR_C));
	
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PCR_C));  /* All intput bits */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_C)); /* All interrupts off */
	/* don't set port value register on input register */
	
	/* Configuring Parallel Port D */
	
	DEBUGPRINT((KERN_ALERT "Setting up parallel port D (0x%x), 0x%x, 0x%x, 0x%x\n", regbase,
		    SAB82538_REG_PCR_D, SAB82538_REG_PIM_D, SAB82538_REG_PVR_D));
	writeb(0x0f,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PCR_D));  /* 4 input  bits */
	writeb(0xff,((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82538_REG_PIM_D)); /* All interrupts off */
	/* don't set port value register on input register */
	
	/* The priority rotation thing */
	
	DEBUGPRINT((KERN_ALERT "Setting IVA (0x%x +  0x%x = 0x%x\n", regbase,
		    SAB82532_REG_IVA, regbase + SAB82532_REG_IVA));
	
	writeb(SAB82538_IVA_ROT, ((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82532_REG_IVA)); 
	writeb(0, ((unsigned char *)regbase) + (CIMCMD_WRREGB | SAB82532_REG_IPC)); 
	
	cptr->c_regs = (void*) regbase;
	cptr->int_disable = DisableESCC8InterruptsFromCIM;
	return cptr;
}

static void CreateESCC8PortWithCIM(SAB_CHIP *cptr, unsigned int portno, 
				   AURA_CIM *cim, unsigned flag)
{
	SAB_BOARD *bptr;
	SAB_PORT  *pptr;
	extern void sab8253x_setup_ttyport(struct sab_port *p_port) ;
	
	++NumSab8253xPorts;
	bptr = cptr->c_board;
	pptr = (SAB_PORT*) kmalloc(sizeof(SAB_PORT), GFP_KERNEL);
	if(pptr == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: Failed to create ESCC2 port structure on chip %p on board %p.\n",
		       cptr, bptr);
		return;
	}
	memset(pptr, 0, sizeof(SAB_PORT));
	DEBUGPRINT
		((KERN_ALERT "Setting up port %d, chipno %d for %s type board number %d.\n", 
		  portno, cptr->c_chipno, board_type[bptr->b_type],bptr->board_number));
	pptr->portno = portno;
	pptr->chip=cptr;
	pptr->board=bptr;
	pptr->open_type=OPEN_NOT;
	pptr->is_console=0;
	pptr->regs=
		(union sab82532_regs *)
		(((unsigned int)cptr->c_regs) +
		 (portno << 6));		/* addressing is different when there is a cim */
	pptr->type = cptr->c_revision;
	pptr->function = (((cim->ci_flags & CIM_SYNC) || flag) ? FUNCTION_NR : 
			  FUNCTION_AO);
	
	pptr->irq = bptr->b_irq;
	
	pptr->dsr.reg = ((unsigned char *)cptr->c_regs) + SAB82538_REG_PVR_C; 
	pptr->dsr.mask = 0x1 << portno;
	pptr->dsr.inverted = 1;
	pptr->dsr.irq=PIS_IDX;	/* need to check this constant */
	pptr->dsr.irqmask=0x1 << portno;
	pptr->dsr.cnst = 0;
	
	pptr->txclkdir.reg = ((unsigned char *)cptr->c_regs) + SAB82538_REG_PVR_A;
	pptr->txclkdir.mask = 0x1 << portno;
	/* NOTE: Early 8 ports  boards had different tx clkdir sense */
	pptr->txclkdir.inverted = 1;
	
	pptr->dtr.reg = ((unsigned char *)cptr->c_regs) + SAB82538_REG_PVR_B;
	pptr->dtr.mask = 0x1 << portno;
	pptr->dtr.inverted = 1;
	pptr->dtr.cnst = 0;
	
	pptr->dcd.reg = ((unsigned char *)pptr->regs) + SAB85232_REG_VSTR;
	
	DEBUGPRINT((KERN_ALERT "cd register set to 0x%p\n", pptr->dcd.reg));
	
	pptr->dcd.mask = SAB82532_VSTR_CD;
	pptr->dcd.inverted = 1;
	pptr->dcd.irq=ISR0_IDX;
	pptr->dcd.irqmask=SAB82532_ISR0_CDSC;
	pptr->dcd.cnst = 0;
	
	pptr->cts.reg = (unsigned char *)&(STAR);
	pptr->cts.mask = SAB82532_STAR_CTS;
	pptr->cts.inverted = 0;
	pptr->cts.irq=ISR1_IDX;
	pptr->cts.irqmask=SAB82532_ISR1_CSC;
	pptr->cts.cnst = 0;
	
	pptr->rts.reg = (unsigned char *)&(MODE);
	pptr->rts.mask = SAB82532_MODE_FRTS;
	pptr->rts.inverted = 1;
	pptr->rts.cnst = SAB82532_MODE_RTS;
	
	
	/* Set the read and write function */
	pptr->readbyte=wmsaura_readb;
	pptr->readword=wmsaura_readw;
	pptr->writebyte=wmsaura_writeb;
	pptr->writeword=wmsaura_writew;
	pptr->readfifo=wmsaura_readfifo;
	pptr->writefifo=wmsaura_writefifo;
	
	sab8253x_setup_ttyport(pptr);	/* asynchronous */
	/* ttys are default, basic */
	/* initialization, everything */
	/* else works as a modification */
	/* thereof */
	
	pptr->next = AuraPortRoot;
	AuraPortRoot = pptr;
	
	pptr->next_by_chip = cptr->c_portbase;
	cptr->c_portbase = pptr;
	
	pptr->next_by_board = bptr->board_portbase;
	bptr->board_portbase = pptr;
	
	pptr->next_by_cim = cim->ci_portbase;
	cim->ci_portbase = pptr;
}

static void CreateCIMs(SAB_BOARD *bptr)
{
	unsigned int cimnum;
	unsigned char *wrcsr;
	unsigned char *rdcsr;
	unsigned char tmp;
	AURA_CIM *cim;
	unsigned short intrmask;
	
	for(intrmask = 0, cimnum = 0; cimnum < MAX_NCIMS; ++cimnum)
	{
		intrmask >>= 2;
		
		/*
		 * The hardware is mapped.  Try writing to CIM CSR.
		 */
		wrcsr = bptr->CIMCMD_REG +
			(CIMCMD_WRCIMCSR | (cimnum << CIMCMD_CIMSHIFT));
		rdcsr = bptr->CIMCMD_REG +
			(CIMCMD_RDCIMCSR | (cimnum << CIMCMD_CIMSHIFT));
		
		/* Try to write an 0xff */
		writeb((unsigned char) 0xff, (unsigned char *) wrcsr);
		/* and read it back */
		tmp = (unsigned char) readb((unsigned char *) rdcsr);
		DEBUGPRINT((KERN_ALERT 
			    "aura wan mcs: wrcsr %p rdcsr %p cim %d 0xff readback: 0x%x.\n",
			    (void*) wrcsr, (void*) rdcsr, cimnum, tmp));
		
		/* make sure it's really all ones. */
		if ((tmp & CIMCMD_CIMCSR_TESTMASK) != CIMCMD_CIMCSR_TESTMASK) 
		{
			printk(KERN_ALERT 
			       "aura wan mcs: not found -- wrcsr %p rdcsr %p cim %d 0xff readback: 0x%x.\n",
			       (void*) wrcsr, (void*) rdcsr, cimnum, tmp);
			continue;
		}
		
		/* Try to write a zero */
		writeb((unsigned char) 0, (unsigned char*) wrcsr);
		/* and read it back */
		tmp = (unsigned char) readb((unsigned char *) rdcsr);
		DEBUGPRINT((KERN_ALERT 
			    "aura wan mcs: wrcsr %p rdcsr %p cim %d 0x0 readback: 0x%x.\n",
			    (void*) wrcsr, (void*) rdcsr, cimnum, tmp));
		
		/* make sure it's really zero. */
		if ((tmp & CIMCMD_CIMCSR_TESTMASK) != 0) 
		{
			printk(KERN_ALERT 
			       "aura wan mcs: not found -- wrcsr %p rdcsr %p cim %d 0x0 readback: 0x%x.\n",
			       (void*) wrcsr, (void*) rdcsr, cimnum, tmp);
			continue;
		}
		cim = (AURA_CIM*) kmalloc(sizeof(AURA_CIM), GFP_KERNEL);
		if(cim == NULL)
		{
			printk(KERN_ALERT 
			       "aura wan mcs: unable to allocate memory, board %p, cim %d.\n", 
			       bptr, cimnum); 
			continue;
		}
		cim->ci_num = cimnum;
		cim->ci_board = bptr;
		cim->ci_chipbase = NULL;
		cim->ci_portbase = NULL;
		cim->ci_nports = CIM_NPORTS;
		cim->ci_port0lbl = cimnum * CIM_NPORTS;
		if (mcs_ciminit(bptr, cim) == FALSE) 
		{
			kfree(cim);
			continue;
		}
		intrmask |= 0xc0;	/* turn on the high two bits
					 * a little obscure, borrowed
					 * from solaris driver 0th cim
					 * gets lowest two bits*/
		cim->next = AuraCimRoot;
		AuraCimRoot = cim;
		cim->next_by_mcs = bptr->b_cimbase;
		bptr->b_cimbase = cim;
		printk(KERN_ALERT 
		       "aura wan mcs: Created cim %d type %d on board %p.\n",
		       cim->ci_num, cim->ci_type, bptr);
	}
	bptr->b_intrmask = intrmask;
}

/* put the chips on the boards */

static void SetupAllChips(SAB_BOARD *bptr)
{				/* note that port ordering */
				/* is important in chip setup */
				/* the open routine walks the */
				/* port list for sync and async */
				/* ttys */
	SAB_CHIP *chip;
	AURA_CIM *cim;
	unsigned int chipno;
	
	switch(bptr->b_type)
	{
	case BD_1020P:
	case BD_1020CP:
		/* setup 1 ESCC2 */
		chip = CreateESCC2(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_NA);
			CreateESCC2Port(chip, 0, FUNCTION_AO);
		}
		
		break;
	case BD_1520P:
	case BD_1520CP:
		/* setup 1 ESCC2 */
		chip = CreateESCC2(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_NA);
			CreateESCC2Port(chip, 0, FUNCTION_NR);
		}      
		break;
	case BD_2020P:
	case BD_2020CP:
		/* setup 1 ESCC2 */
		chip = CreateESCC2(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_AO);
			CreateESCC2Port(chip, 0, FUNCTION_AO);
		}      
		break;
	case BD_2520P:
	case BD_2520CP:
		/* setup 1 ESCC2 */
		chip = CreateESCC2(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_NR);
			CreateESCC2Port(chip, 0, FUNCTION_NR);
		}      
		break;
	case BD_4020P:		
	case BD_4020CP:		/* do chips in reverCse
				   order so that they
				   are on lists in forward
				   order
				*/
				/* setup 2 ESCC2 */
		chip = CreateESCC2(bptr, AURORA_4X20_CHIP_OFFSET);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_AO);
			CreateESCC2Port(chip, 0, FUNCTION_AO);
		}
		chip = CreateESCC2(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_AO);
			CreateESCC2Port(chip, 0, FUNCTION_AO);
		}
		break;
	case BD_4520P:
	case BD_4520CP:
		/* setup 2 ESCC2 */
		chip = CreateESCC2(bptr, AURORA_4X20_CHIP_OFFSET);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_NR);
			CreateESCC2Port(chip, 0, FUNCTION_NR);
		}
		chip = CreateESCC2(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC2Port(chip, 1, FUNCTION_NR);
			CreateESCC2Port(chip, 0, FUNCTION_NR);
		}
		break;
	case BD_8020P:
	case BD_8020CP:
		/* setup 1 ESCC8 */
		chip = CreateESCC8(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC8Port(chip, 7, FUNCTION_AO);
			CreateESCC8Port(chip, 6, FUNCTION_AO);
			CreateESCC8Port(chip, 5, FUNCTION_AO);
			CreateESCC8Port(chip, 4, FUNCTION_AO);
			CreateESCC8Port(chip, 3, FUNCTION_AO);
			CreateESCC8Port(chip, 2, FUNCTION_AO);
			CreateESCC8Port(chip, 1, FUNCTION_AO);
			CreateESCC8Port(chip, 0, FUNCTION_AO);
		}
		break;
	case BD_8520P:
	case BD_8520CP:
		/* setup 1 ESCC8 */
		chip = CreateESCC8(bptr, 0);
		if(chip != NULL)
		{
			CreateESCC8Port(chip, 7, FUNCTION_NR);
			CreateESCC8Port(chip, 6, FUNCTION_NR);
			CreateESCC8Port(chip, 5, FUNCTION_NR);
			CreateESCC8Port(chip, 4, FUNCTION_NR);
			CreateESCC8Port(chip, 3, FUNCTION_NR);
			CreateESCC8Port(chip, 2, FUNCTION_NR);
			CreateESCC8Port(chip, 1, FUNCTION_NR);
			CreateESCC8Port(chip, 0, FUNCTION_NR);
		}
		break;
		
	case BD_WANMCS:
		CreateCIMs(bptr);
		for(chipno = 7, cim = bptr->b_cimbase; 
		    cim != NULL; cim = cim->next_by_mcs)
		{
			chip = CreateESCC8fromCIM(bptr, cim, chipno--);
			if(chip != NULL)
			{
				CreateESCC8PortWithCIM(chip, 7, cim, 0);
				CreateESCC8PortWithCIM(chip, 6, cim, 0);
				CreateESCC8PortWithCIM(chip, 5, cim, 0);
				CreateESCC8PortWithCIM(chip, 4, cim, 0);
				CreateESCC8PortWithCIM(chip, 3, cim, 0);
				CreateESCC8PortWithCIM(chip, 2, cim, 0);
				CreateESCC8PortWithCIM(chip, 1, cim, 0);
				CreateESCC8PortWithCIM(chip, 0, cim, 0);
			}
			chip = CreateESCC8fromCIM(bptr, cim, chipno--);
			if(chip != NULL)
			{
				CreateESCC8PortWithCIM(chip, 7, cim, 0);
				CreateESCC8PortWithCIM(chip, 6, cim, 0);
				CreateESCC8PortWithCIM(chip, 5, cim, 0);
				CreateESCC8PortWithCIM(chip, 4, cim, 0);
				CreateESCC8PortWithCIM(chip, 3, cim, 0);
				CreateESCC8PortWithCIM(chip, 2, cim, 0);
				CreateESCC8PortWithCIM(chip, 1, cim, 0);
				CreateESCC8PortWithCIM(chip, 0, cim, 1);
			}
		}
		break;
		
	default:
		printk(KERN_ALERT "auraXX20n: unable to set up chip for board %p.\n", bptr);
		break;
	}
}

/* finding the cards by PCI device type */

static SAB_BOARD* find_ati_cpci_card(void)
{
	struct pci_dev *pdev;
	unsigned char bus;
	unsigned char devfn;
	unsigned char pci_latency;
	unsigned short pci_command;
	SAB_BOARD *bptr;
	unsigned control;
	unsigned does_sync;
	unsigned use_1port;
	
	printk(KERN_ALERT "auraXX20n: finding ati cpci cards.\n");
	
	bptr = (SAB_BOARD*)kmalloc(sizeof(SAB_BOARD), GFP_KERNEL);
	if(bptr == NULL)
	{
		printk(KERN_ALERT "auraXX20n: could not allocate board memory!\n");
		return 0;
	}
	memset(bptr, 0, sizeof(SAB_BOARD));
	
	if(!pcibios_present())
	{
		printk(KERN_ALERT "auraXX20n: system does not support PCI bus.\n");
		kfree(bptr);
		return 0;
	}
	DEBUGPRINT((KERN_ALERT "auraXX20n: System supports PCI bus.\n"));
	
 CPCIRESTART:
	if(pdev = pci_find_device(sab8253x_vendor_id, sab8253x_cpci_device_id, XX20lastpdev),
	   pdev == NULL)
	{
		printk(KERN_ALERT "auraXX20n: could not find cpci card.\n");
		kfree(bptr);
		return 0;
	}
	
	DEBUGPRINT((KERN_ALERT "auraXX20n: found multiport CPCI serial card.\n"));
	
	XX20lastpdev = pdev;
	DEBUGPRINT((KERN_ALERT "auraXX20n: found ATI PLX 9050, %p.\n", pdev));
	bptr->b_dev = *pdev;
	
	/* the Solaris and model linux drivers
	 * comment that there are problems with
	 * getting the length via PCI operations
	 * seems to work for 2.4
	 */
	bptr->length0 = (unsigned int) pci_resource_len(pdev, 0);
	bptr->length1 = (unsigned int) pci_resource_len(pdev, 1);
	bptr->length2 = (unsigned int) pci_resource_len(pdev, 2);
	bptr->b_irq = pdev->irq;
	
	
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 0 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 0), bptr->length0));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 1 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 1), bptr->length1));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 2 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 2),
		    bptr->length2));
	
	DEBUGPRINT((KERN_ALERT "auraXX20n: interrupt is %i.\n", pdev->irq));
	bus = pdev->bus->number;
	devfn = pdev->devfn;
	DEBUGPRINT((KERN_ALERT "auraXX20n: bus is %x, slot is %x.\n", bus, PCI_SLOT(devfn)));
	pcibios_read_config_word(bus, devfn, PCI_COMMAND, &pci_command);
#if 0
	/* The Aurora card does not act as a PCI master
	 * ugh!!
	 */
	new_command = pci_command | PCI_COMMAND_MASTER;
	if(pci_command != new_command)
	{
		DEBUGPRINT((KERN_ALERT 
			    "auraXX20n: the PCI BIOS has not enabled this device!"
			    " Updating PCI command %4.4x->%4.4x.\n", 
			    pci_command,
			    new_command));
		pcibios_write_config_word(bus, devfn, PCI_COMMAND, 
					  new_command);
	}
	else
	{
		DEBUGPRINT
			((KERN_ALERT 
			  "auraXX20n: the PCI BIOS has enabled this device as master!\n"));
	}
#endif
	if((pci_command & PCI_COMMAND_MASTER) != PCI_COMMAND_MASTER)
	{
		DEBUGPRINT((KERN_ALERT "auraXX20n: Aurora card is not a bus master.\n"));
	}
	
	pcibios_read_config_byte(bus, devfn, PCI_LATENCY_TIMER, 
				 &pci_latency);
	if (pci_latency < 32) 
	{
		DEBUGPRINT
			((KERN_ALERT
			  "auraXX20n: PCI latency timer (CFLT) is low at %i.\n", pci_latency));
		/* may need to change the latency */
#if 0
		pcibios_write_config_byte(bus, devfn, PCI_LATENCY_TIMER, 32);
#endif
	} 
	else 
	{
		DEBUGPRINT((KERN_ALERT
			    "auraXX20n: PCI latency timer (CFLT) is %#x.\n", 
			    pci_latency));
	}
	bptr->virtbaseaddress0 = ioremap_nocache(pci_base_address(pdev, 0), 
						 bptr->length0);
	if(bptr->virtbaseaddress0 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 0));
		
		goto CPCIRESTART;
	}
	
	bptr->b_bridge = (PLX9050*) bptr->virtbaseaddress0; /* MAKE SURE INTS ARE OFF */
	writel(PLX_INT_OFF, &(bptr->b_bridge->intr));
	
	printk
		(KERN_ALERT
		 "auraXX20n: remapped physical address %p to virtual address %p.\n",
		 (void*) pci_base_address(pdev, 0), (void*) bptr->virtbaseaddress0);
	
	dump_ati_adapter_registers((unsigned int*) bptr->virtbaseaddress0, bptr->length0);
	
	if(*(unsigned int*)bptr->virtbaseaddress0 == -1) /* XP7 problem? */
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to access PLX 9050 registers at %p.\n", 
		       (void*)bptr->virtbaseaddress0);
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		goto CPCIRESTART;
	}
	
	bptr->virtbaseaddress2 = ioremap_nocache(pci_base_address(pdev, 2),
						 bptr->length2);
	if(bptr->virtbaseaddress2 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 2));
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		goto CPCIRESTART;
	}
	
	DEBUGPRINT
		((KERN_ALERT
		  "auraXX20n: remapped physical address %p to virtual address %p.\n",
		  (void*) pci_base_address(pdev, 2), (void*) bptr->virtbaseaddress2));
	
	/* we get clockrate from serial eeprom */
	if (!plx9050_eprom_read(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				(unsigned short*) bptr->b_eprom,
				(unsigned char) 0, EPROM9050_SIZE))
	{
		printk(KERN_ALERT "auraXX20n: Could not read serial eprom.\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto CPCIRESTART;
	}
	
	printk(KERN_ALERT "auraXX20n: dumping serial eprom.\n");
	dump_ati_adapter_registers((unsigned int*) bptr->b_eprom, 2 * EPROM9050_SIZE);  
	
	if(*(unsigned int*)bptr->b_eprom != PCIMEMVALIDCPCI) /* bridge problem? */
	{
		printk(KERN_ALERT "auraXX20n: unable to access valid serial eprom data.\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto CPCIRESTART;
	}
	
	if(((unsigned short*) bptr->b_eprom)[EPROMPREFETCHOFFSET] & PREFETCHBIT)
	{
		++sab8253x_rebootflag;
		printk(KERN_ALERT "8253x: eeprom programmed for prefetchable memory resources; must reprogram!!\n");
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WENCMD, NM93_WENADDR, 0);
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WRITECMD, 
				  9,
				  (((unsigned short*) bptr->b_eprom)[EPROMPREFETCHOFFSET] & (~PREFETCHBIT)));
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WDSCMD, NM93_WDSADDR, 0);
	}
	/* get SYNC and ONEPORT values */
	
	control = readl(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl);	
	/* note we use the actual address
	 * of the control register in
	 * memory
	 */
	
	if(control & AURORA_MULTI_SYNCBIT)
	{
		does_sync = 0;
	}
	else
	{
		does_sync = 1;
	}
	
	if(control & AURORA_MULTI_1PORTBIT)
	{
		use_1port = 1;
	}
	else
	{
		use_1port = 0;
	}
	
	
	/* Figure out the board */
	switch(bptr->length2) 
	{
	case AURORA_4X20_SIZE:
		if(does_sync) 
		{
			bptr->b_type = BD_4520CP;
			bptr->b_nchips = 2;
			bptr->b_nports = 4;
			bptr->b_flags = BD_SYNC;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD4520CPcounter; /* keep track of boardnumber for naming devices */
			++BD4520CPcounter;
			printk(KERN_ALERT "auraXX20n: Found Saturn 4520CP.\n");
		} 
		else 
		{
			bptr->b_type = BD_4020CP;
			bptr->b_nchips = 2;
			bptr->b_nports = 4;
			bptr->b_flags = 0x0;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD4020CPcounter;
			++BD4020CPcounter;
			printk(KERN_ALERT "auraXX20n: Found Apollo 4020CP.\n");
		}
		break;
	case AURORA_8X20_SIZE:
		if(does_sync) 
		{ 
			bptr->b_type = BD_8520CP;
			bptr->b_nchips = 1;
			bptr->b_nports = 8;
			bptr->b_flags = BD_SYNC;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD8520CPcounter;
			++BD8520CPcounter;
			printk(KERN_ALERT "auraXX20n: Found Saturn 8520CP.\n");
		} 
		else 
		{
			bptr->b_type = BD_8020CP;
			bptr->b_nchips = 1;
			bptr->b_nports = 8;
			bptr->b_flags = 0x0;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD8020CPcounter;
			++BD8020CPcounter;
			printk(KERN_ALERT "auraXX20n: Found Apollo 8020CP.\n");
		}
		break;
	case AURORA_2X20_SIZE:
		if(does_sync) 
		{
			if(use_1port) 
			{
				bptr->b_type = BD_1520CP;
				printk(KERN_ALERT "auraXX20n: Found Saturn 1520CP.\n");
				bptr->b_nchips = 1;
				bptr->b_nports = 1;
				bptr->b_flags = BD_SYNC;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD1520CPcounter;
				++BD1520CPcounter;
				printk(KERN_ALERT "auraXX20n: Found Saturn 1520CP.\n");
			} 
			else 
			{
				bptr->b_type = BD_2520CP;
				bptr->b_nchips = 1;
				bptr->b_nports = 2;
				bptr->b_flags = BD_SYNC;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD2520CPcounter;
				++BD2520CPcounter;
				printk(KERN_ALERT "auraXX20n: Found Saturn 2520CP.\n");
			}
		}
		else
		{
			if(use_1port) 
			{
				bptr->b_type = BD_1020CP;
				bptr->b_nchips = 1;
				bptr->b_nports = 1;
				bptr->b_flags = 0x0;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD1020CPcounter;
				++BD1020CPcounter;
				printk(KERN_ALERT "auraXX20n: Found Apollo 1020CP.\n");
			} 
			else 
			{
				bptr->b_type = BD_2020CP;
				bptr->b_nchips = 1;
				bptr->b_nports = 2;
				bptr->b_flags = 0x0;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD2020CPcounter;
				++BD2020CPcounter;
				printk(KERN_ALERT "auraXX20n: Found Apollo 2020CP.\n");
			}
		}
		break;
	default:
		printk(KERN_ALERT "Error: Board could not be identified\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto CPCIRESTART;
	}
	
	/* Let's get the clockrate right -- ugh!*/
	
	bptr->b_clkspeed = bptr->b_eprom[AURORA_MULTI_EPROM_CLKLSW/2];
	
	if(bptr->b_clkspeed == -1)	/* misprogrammed -- ugh. */
	{
		switch(bptr->b_type)
		{
		case BD_8520CP:
		case BD_8020CP:
			bptr->b_clkspeed = AURORA_MULTI_CLKSPEED/4;
			break;
		default:
			bptr->b_clkspeed = AURORA_MULTI_CLKSPEED;
			break;
			
		}
		printk(KERN_ALERT "auraXX20n:  UNKNOWN CLOCKSPEED -- ASSUMING %ld.\n", bptr->b_clkspeed);
		
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WENCMD, NM93_WENADDR, 0);
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WRITECMD, 
				  54, (unsigned short) bptr->b_clkspeed);
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WRITECMD, 
				  55, (unsigned short) (bptr->b_clkspeed >> 16));
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WDSCMD, NM93_WDSADDR, 0);
	}
	
	return bptr;
}

static SAB_BOARD* find_ati_wanms_card(void)   /* wan multichanner server == mcs [ multichannel server] */
{
	struct pci_dev *pdev;
	unsigned char bus;
	unsigned char devfn;
	unsigned char pci_latency;
	unsigned short pci_command;
	SAB_BOARD *bptr;
	int resetresult;
	
	printk(KERN_ALERT "auraXX20n: finding ati mcs cards.\n");
	
	bptr = (SAB_BOARD*)kmalloc(sizeof(SAB_BOARD), GFP_KERNEL);
	if(bptr == NULL)
	{
		printk(KERN_ALERT "auraXX20n: could not allocate board memory!\n");
		return 0;
	}
	memset(bptr, 0, sizeof(SAB_BOARD));
	
	if(!pcibios_present())
	{
		printk(KERN_ALERT "auraXX20n: system does not support PCI bus.\n");
		kfree(bptr);
		return 0;
	}
	DEBUGPRINT((KERN_ALERT "auraXX20n: System supports PCI bus.\n"));
	
 MCSRESTART:
	if(pdev = pci_find_device(sab8253x_vendor_id, sab8253x_wmcs_device_id, XX20lastpdev),
	   pdev == NULL)
	{
		printk(KERN_ALERT "auraXX20n: could not find mcs card.\n");
		kfree(bptr);
		return 0;
	}
	
	DEBUGPRINT((KERN_ALERT "auraXX20n: found mcs card.\n"));
	
	XX20lastpdev = pdev;
	DEBUGPRINT((KERN_ALERT "auraXX20n: found ATI S5920, %p.\n", pdev));
	bptr->b_dev = *pdev;
	
	/* the Solaris and model linux drivers
	 * comment that there are problems with
	 * getting the length via PCI operations
	 * seems to work for 2.4
	 */
	bptr->length0 = (unsigned int) pci_resource_len(pdev, 0); /* AMCC 5920 operation registers
								     includes access to serial eprom */
	bptr->length1 = (unsigned int) pci_resource_len(pdev, 1); /* commands to remote cards */
	bptr->length2 = (unsigned int) pci_resource_len(pdev, 2); /* command to host card */
	bptr->length3 = (unsigned int) pci_resource_len(pdev, 3); /* RFIFO cache */
	bptr->b_irq = pdev->irq;
	
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 0 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 0), bptr->length0));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 1 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 1), bptr->length1));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 2 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 2),
		    bptr->length2));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 3 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 3),
		    bptr->length3));
	
	DEBUGPRINT((KERN_ALERT "auraXX20n: interrupt is %i.\n", pdev->irq));
	bus = pdev->bus->number;
	devfn = pdev->devfn;
	DEBUGPRINT((KERN_ALERT "auraXX20n: bus is %x, slot is %x.\n", bus, PCI_SLOT(devfn)));
	pcibios_read_config_word(bus, devfn, PCI_COMMAND, &pci_command);
	
#if 0
	/* The Aurora card does not act as a PCI master
	 * ugh!!
	 */
	new_command = pci_command | PCI_COMMAND_MASTER;
	if(pci_command != new_command)
	{
		DEBUGPRINT((KERN_ALERT 
			    "auraXX20n: the PCI BIOS has not enabled this device!"
			    " Updating PCI command %4.4x->%4.4x.\n", 
			    pci_command,
			    new_command));
		pcibios_write_config_word(bus, devfn, PCI_COMMAND, 
					  new_command);
	}
	else
	{
		DEBUGPRINT
			((KERN_ALERT 
			  "auraXX20n: the PCI BIOS has enabled this device as master!\n"));
	}
#endif
	
	if((pci_command & PCI_COMMAND_MASTER) != PCI_COMMAND_MASTER)
	{
		DEBUGPRINT((KERN_ALERT "auraXX20n: Aurora card is not a bus master.\n"));
	}
	
	pcibios_read_config_byte(bus, devfn, PCI_LATENCY_TIMER, 
				 &pci_latency);
	if (pci_latency < 32) 
	{
		DEBUGPRINT
			((KERN_ALERT
			  "auraXX20n: PCI latency timer (CFLT) is low at %i.\n", pci_latency));
		/* may need to change the latency */
#if 0
		pcibios_write_config_byte(bus, devfn, PCI_LATENCY_TIMER, 32);
#endif
	} 
	else 
	{
		DEBUGPRINT((KERN_ALERT
			    "auraXX20n: PCI latency timer (CFLT) is %#x.\n", 
			    pci_latency));
	}
	
	bptr->virtbaseaddress0 = ioremap_nocache(pci_base_address(pdev, 0), 
						 bptr->length0);
	if(bptr->virtbaseaddress0 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 0));
		goto MCSRESTART;
	}
	
	bptr->b_bridge = (void*) bptr->virtbaseaddress0; /* b_bridge is not supposed
							    to be used by the AMCC based
							    products -- it is set just
							    in case */
	
	printk(KERN_ALERT
	       "auraXX20n: remapped physical address %p to virtual address %p.\n",
	       (void*) pci_base_address(pdev, 0), (void*) bptr->virtbaseaddress0);
	
	/* unfortunate name -- works for any bridge */
	dump_ati_adapter_registers((unsigned int*) bptr->virtbaseaddress0, bptr->length0);
	
	if(*(unsigned int*)bptr->virtbaseaddress0 == -1) /* XP7 problem? */
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to access AMCC registers at %p.\n", 
		       (void*)bptr->virtbaseaddress0);
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		goto MCSRESTART;		       /* try the next one if any */
	}
	
	writel(AMCC_INT_OFF, (unsigned int*)(bptr->AMCC_REG + AMCC_INTCSR));
	
	bptr->virtbaseaddress1 = ioremap_nocache(pci_base_address(pdev, 1),
						 bptr->length1);
	if(bptr->virtbaseaddress1 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 1));
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		goto MCSRESTART;
	}
	
	DEBUGPRINT
		((KERN_ALERT
		  "auraXX20n: remapped physical address %p to virtual address %p.\n",
		  (void*) pci_base_address(pdev, 1), (void*) bptr->virtbaseaddress1));
	
	/* next address space */
	bptr->virtbaseaddress2 = ioremap_nocache(pci_base_address(pdev, 2),
						 bptr->length2);
	if(bptr->virtbaseaddress2 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 2));
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress1);
		iounmap((void*)bptr->virtbaseaddress1);
		bptr->virtbaseaddress1 = 0;
		goto MCSRESTART;
	}
	
	DEBUGPRINT
		((KERN_ALERT
		  "auraXX20n: remapped physical address %p to virtual address %p.\n",
		  (void*) pci_base_address(pdev, 2), (void*) bptr->virtbaseaddress2));
	
	bptr->virtbaseaddress3 = ioremap_nocache(pci_base_address(pdev, 3),
						 bptr->length3);
	if(bptr->virtbaseaddress3 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 3));
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress1);
		iounmap((void*)bptr->virtbaseaddress1);
		bptr->virtbaseaddress1 = 0;
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress2);
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto MCSRESTART;
	}
	
	DEBUGPRINT
		((KERN_ALERT
		  "auraXX20n: remapped physical address %p to virtual address %p.\n",
		  (void*) pci_base_address(pdev, 3), (void*) bptr->virtbaseaddress3));
	
	bptr->b_type = BD_WANMCS;
	
	resetresult = wanmcs_reset(bptr);
	writel(AMCC_INT_OFF, (unsigned int*)(bptr->AMCC_REG + AMCC_INTCSR));
	
	if(resetresult == FALSE)
	{
		printk(KERN_ALERT "auraXX20n: unable to reset wan mcs %p.\n", bptr);
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress1);
		iounmap((void*)bptr->virtbaseaddress1);
		bptr->virtbaseaddress1 = 0;
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress2);
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress3);
		iounmap((void*)bptr->virtbaseaddress3);
		bptr->virtbaseaddress3 = 0;
		
		goto MCSRESTART;
	}
	
	/* we get clockrate from serial eeprom */
	if (amcc_read_nvram((unsigned char*) bptr->b_eprom,
			    AMCC_NVRAM_SIZE, bptr->AMCC_REG) == FALSE)
	{
		printk(KERN_ALERT "auraXX20n: Could not read serial eprom.\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress1);
		bptr->virtbaseaddress1 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		iounmap((void*)bptr->virtbaseaddress3);
		bptr->virtbaseaddress3 = 0;
		goto MCSRESTART;
	}
	printk(KERN_ALERT "auraXX20n: dumping serial eprom.\n");
	dump_ati_adapter_registers((unsigned int*) bptr->b_eprom, 2 * AMCC_NVRAM_SIZE);  
	if(bptr->b_eprom[AMCC_NVR_VENDEVID] != PCIMEMVALIDWMCS)
	{
		printk(KERN_ALERT "auraXX20: bad serial eprom, board %p.\n", bptr);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress1);
		bptr->virtbaseaddress1 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		iounmap((void*)bptr->virtbaseaddress3);
		bptr->virtbaseaddress3 = 0;
		goto MCSRESTART;
	}
	return bptr;
}

/* initialize the auraXX20 */
static SAB_BOARD* find_ati_multiport_card(void)
{
	struct pci_dev *pdev;
	unsigned char bus;
	unsigned char devfn;
	unsigned char pci_latency;
	unsigned short pci_command;
	SAB_BOARD *bptr;
	unsigned control;
	unsigned does_sync;
	unsigned use_1port;
	
	printk(KERN_ALERT "auraXX20n: finding ati cards.\n");
	
	bptr = (SAB_BOARD*)kmalloc(sizeof(SAB_BOARD), GFP_KERNEL);
	if(bptr == NULL)
	{
		printk(KERN_ALERT "auraXX20n: could not allocate board memory!\n");
		return 0;
	}
	memset(bptr, 0, sizeof(SAB_BOARD));
	
	if(!pcibios_present())
	{
		printk(KERN_ALERT "auraXX20n: system does not support PCI bus.\n");
		kfree(bptr);
		return 0;
	}
	DEBUGPRINT((KERN_ALERT "auraXX20n: System supports PCI bus.\n"));
	
 MULTIPORTRESTART:
	if(pdev = pci_find_device(sab8253x_vendor_id, sab8253x_mpac_device_id, XX20lastpdev),
	   pdev == NULL)
	{
		printk(KERN_ALERT "auraXX20n: could not find multiport card.\n");
		kfree(bptr);
		return 0;
	}
	
	DEBUGPRINT((KERN_ALERT "auraXX20n: found multiport PCI serial card.\n"));
	
	XX20lastpdev = pdev;
	DEBUGPRINT((KERN_ALERT "auraXX20n: found ATI PLX 9050, %p.\n", pdev));
	bptr->b_dev = *pdev;
	
	/* the Solaris and model linux drivers
	 * comment that there are problems with
	 * getting the length via PCI operations
	 * seems to work for 2.4
	 */
	bptr->length0 = (unsigned int) pci_resource_len(pdev, 0);
	bptr->length1 = (unsigned int) pci_resource_len(pdev, 1);
	bptr->length2 = (unsigned int) pci_resource_len(pdev, 2);
	bptr->b_irq = pdev->irq;
	
	
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 0 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 0), bptr->length0));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 1 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 1), bptr->length1));
	DEBUGPRINT((KERN_ALERT 
		    "auraXX20n: base address 2 is %p, len is %x.\n", 
		    (void*) pci_base_address(pdev, 2),
		    bptr->length2));
	
	DEBUGPRINT((KERN_ALERT "auraXX20n: interrupt is %i.\n", pdev->irq));
	bus = pdev->bus->number;
	devfn = pdev->devfn;
	DEBUGPRINT((KERN_ALERT "auraXX20n: bus is %x, slot is %x.\n", bus, PCI_SLOT(devfn)));
	pcibios_read_config_word(bus, devfn, PCI_COMMAND, &pci_command);
#if 0
	/* The Aurora card does not act as a PCI master
	 * ugh!!
	 */
	new_command = pci_command | PCI_COMMAND_MASTER;
	if(pci_command != new_command)
	{
		DEBUGPRINT((KERN_ALERT 
			    "auraXX20n: the PCI BIOS has not enabled this device!"
			    " Updating PCI command %4.4x->%4.4x.\n", 
			    pci_command,
			    new_command));
		pcibios_write_config_word(bus, devfn, PCI_COMMAND, 
					  new_command);
	}
	else
	{
		DEBUGPRINT
			((KERN_ALERT 
			  "auraXX20n: the PCI BIOS has enabled this device as master!\n"));
	}
#endif
	if((pci_command & PCI_COMMAND_MASTER) != PCI_COMMAND_MASTER)
	{
		DEBUGPRINT((KERN_ALERT "auraXX20n: Aurora card is not a bus master.\n"));
	}
	
	pcibios_read_config_byte(bus, devfn, PCI_LATENCY_TIMER, 
				 &pci_latency);
	if (pci_latency < 32) 
	{
		DEBUGPRINT
			((KERN_ALERT
			  "auraXX20n: PCI latency timer (CFLT) is low at %i.\n", pci_latency));
		/* may need to change the latency */
#if 0
		pcibios_write_config_byte(bus, devfn, PCI_LATENCY_TIMER, 32);
#endif
	} 
	else 
	{
		DEBUGPRINT((KERN_ALERT
			    "auraXX20n: PCI latency timer (CFLT) is %#x.\n", 
			    pci_latency));
	}
	bptr->virtbaseaddress0 = ioremap_nocache(pci_base_address(pdev, 0), 
						 bptr->length0);
	if(bptr->virtbaseaddress0 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 0));
		
		goto MULTIPORTRESTART;
	}
	
	bptr->b_bridge = (PLX9050*) bptr->virtbaseaddress0; /* MAKE SURE INTS ARE OFF */
	writel(PLX_INT_OFF, &(bptr->b_bridge->intr));
	
	printk(KERN_ALERT
	       "auraXX20n: remapped physical address %p to virtual address %p.\n",
	       (void*) pci_base_address(pdev, 0), (void*) bptr->virtbaseaddress0);
	
	dump_ati_adapter_registers((unsigned int*) bptr->virtbaseaddress0, bptr->length0);
	
	if(*(unsigned int*)bptr->virtbaseaddress0 == -1) /* XP7 problem? */
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to access PLX 9050 registers at %p.\n", 
		       (void*)bptr->virtbaseaddress0);
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		goto MULTIPORTRESTART;
	}
	
	bptr->virtbaseaddress2 = ioremap_nocache(pci_base_address(pdev, 2),
						 bptr->length2);
	if(bptr->virtbaseaddress2 == NULL)
	{
		printk(KERN_ALERT
		       "auraXX20n: unable to remap physical address %p.\n", 
		       (void*) pci_base_address(pdev, 2));
		printk(KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
		       (void*)bptr->virtbaseaddress0);
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		
		goto MULTIPORTRESTART;
	}
	
	DEBUGPRINT((KERN_ALERT
		    "auraXX20n: remapped physical address %p to virtual address %p.\n",
		    (void*) pci_base_address(pdev, 2), (void*) bptr->virtbaseaddress2));
	
	if (!plx9050_eprom_read(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				(unsigned short*) bptr->b_eprom,
				(unsigned char) 0, EPROM9050_SIZE))
	{
		printk(KERN_ALERT "auraXX20n: Could not read serial eprom.\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto MULTIPORTRESTART;
	}
	
	printk(KERN_ALERT "auraXX20n: dumping serial eprom.\n");
	dump_ati_adapter_registers((unsigned int*) bptr->b_eprom, 2 * EPROM9050_SIZE);  
	
	if(*(unsigned int*)bptr->b_eprom != PCIMEMVALIDMULTI) /* bridge problem? */
	{
		printk(KERN_ALERT "auraXX20n: unable to access valid serial eprom data.\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto MULTIPORTRESTART;
	}
	
	if(((unsigned short*) bptr->b_eprom)[EPROMPREFETCHOFFSET] & PREFETCHBIT)
	{
		++sab8253x_rebootflag;
		printk(KERN_ALERT "8253x: eeprom programmed for prefetchable memory resources; must reprogram!!\n");
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WENCMD, NM93_WENADDR, 0);
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WRITECMD, 
				  9,
				  (((unsigned short*) bptr->b_eprom)[EPROMPREFETCHOFFSET] & (~PREFETCHBIT)));
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WDSCMD, NM93_WDSADDR, 0);
	}
	
	/* get SYNC and ONEPORT values */
	
	control = readl(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl);	
	/* note we use the actual address
	 * of the control register in
	 * memory
	 */
	
	if(control & AURORA_MULTI_SYNCBIT)
	{
		does_sync = 0;
	}
	else
	{
		does_sync = 1;
	}
	
	if(control & AURORA_MULTI_1PORTBIT)
	{
		use_1port = 1;
	}
	else
	{
		use_1port = 0;
	}
	
	
	/* Figure out the board */
	switch(bptr->length2) 
	{
	case AURORA_4X20_SIZE:
		if(does_sync) 
		{
			bptr->b_type = BD_4520P;
			bptr->b_nchips = 2;
			bptr->b_nports = 4;
			bptr->b_flags = BD_SYNC;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD4520Pcounter; /* keep track of boardnumber for naming devices */
			++BD4520Pcounter;
			printk(KERN_ALERT "auraXX20n: Found Saturn 4520P.\n");
		} 
		else 
		{
			bptr->b_type = BD_4020P;
			bptr->b_nchips = 2;
			bptr->b_nports = 4;
			bptr->b_flags = 0x0;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD4020Pcounter;
			++BD4020Pcounter;
			printk(KERN_ALERT "auraXX20n: Found Apollo 4020P.\n");
		}
		break;
	case AURORA_8X20_SIZE:
		if(does_sync) 
		{ 
			bptr->b_type = BD_8520P;
			bptr->b_nchips = 1;
			bptr->b_nports = 8;
			bptr->b_flags = BD_SYNC;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD8520Pcounter;
			++BD8520Pcounter;
			printk(KERN_ALERT "auraXX20n: Found Saturn 8520P.\n");
		} 
		else 
		{
			bptr->b_type = BD_8020P;
			bptr->b_nchips = 1;
			bptr->b_nports = 8;
			bptr->b_flags = 0x0;
			bptr->b_cimbase  =  NULL;
			bptr->board_number = BD8020Pcounter;
			++BD8020Pcounter;
			printk(KERN_ALERT "auraXX20n: Found Apollo 8020P.\n");
		}
		break;
	case AURORA_2X20_SIZE:
		if(does_sync) 
		{
			if(use_1port) 
			{
				bptr->b_type = BD_1520P;
				printk(KERN_ALERT "auraXX20n: Found Saturn 1520P.\n");
				bptr->b_nchips = 1;
				bptr->b_nports = 1;
				bptr->b_flags = BD_SYNC;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD1520Pcounter;
				++BD1520Pcounter;
				printk(KERN_ALERT "auraXX20n: Found Saturn 1520P.\n");
			} 
			else 
			{
				bptr->b_type = BD_2520P;
				bptr->b_nchips = 1;
				bptr->b_nports = 2;
				bptr->b_flags = BD_SYNC;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD2520Pcounter;
				++BD2520Pcounter;
				printk(KERN_ALERT "auraXX20n: Found Saturn 2520P.\n");
			}
		}
		else
		{
			if(use_1port) 
			{
				bptr->b_type = BD_1020P;
				bptr->b_nchips = 1;
				bptr->b_nports = 1;
				bptr->b_flags = 0x0;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD1020Pcounter;
				++BD1020Pcounter;
				printk(KERN_ALERT "auraXX20n: Found Apollo 1020P.\n");
			} 
			else 
			{
				bptr->b_type = BD_2020P;
				bptr->b_nchips = 1;
				bptr->b_nports = 2;
				bptr->b_flags = 0x0;
				bptr->b_cimbase  =  NULL;
				bptr->board_number = BD2020Pcounter;
				++BD2020Pcounter;
				printk(KERN_ALERT "auraXX20n: Found Apollo 2020P.\n");
			}
		}
		break;
	default:
		printk(KERN_ALERT "Error: Board could not be identified\n");
		iounmap((void*)bptr->virtbaseaddress0);
		bptr->virtbaseaddress0 = 0;
		iounmap((void*)bptr->virtbaseaddress2);
		bptr->virtbaseaddress2 = 0;
		
		goto MULTIPORTRESTART;
	}
	/* Let's get the clockrate right -- ugh!*/
	
	bptr->b_clkspeed = bptr->b_eprom[AURORA_MULTI_EPROM_CLKLSW/2];
	
	if(bptr->b_clkspeed == -1)	/* misprogrammed -- ugh. */
	{
		switch(bptr->b_type)
		{
		case BD_8520P:
		case BD_8020P:
			bptr->b_clkspeed = AURORA_MULTI_CLKSPEED/4;
			break;
		default:
			bptr->b_clkspeed = AURORA_MULTI_CLKSPEED;
			break;
			
		}
		printk(KERN_ALERT "auraXX20n:  UNKNOWN CLOCKSPEED -- ASSUMING %ld.\n", bptr->b_clkspeed);
		
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WENCMD, NM93_WENADDR, 0);
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WRITECMD, 
				  54, (unsigned short) bptr->b_clkspeed);
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WRITECMD, 
				  55, (bptr->b_clkspeed >> 16));
		plx9050_eprom_cmd(&((PLX9050*)(bptr->virtbaseaddress0))->ctrl, 
				  NM93_WDSCMD, NM93_WDSADDR, 0);
	}
	
	return bptr;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#ifdef MODULE
int init_module(void)		/* all OS */
#else
int auraXX20_probe(struct net_device *devp) /* passed default device structure */
#endif
#else
static int __init auraXX20_probe(void)	/* legacy device initialization 2.4.* */
#endif
{
	SAB_BOARD *boardptr;
	SAB_PORT *portptr;
	struct net_device *dev;
	unsigned int result;
	unsigned int namelength;
	unsigned int portno;
	int intr_val;
	
	int mp_probe_count = 0;	/* multiport count */
	int cp_probe_count = 0;	/* compact pci count */
	int wm_probe_count = 0;	/* wan multiserver count */
	
	printk(KERN_ALERT "aurora interea miseris mortalibus almam extulerat lucem\n");
	printk(KERN_ALERT "        referens opera atque labores\n"); 
	
	memset(AuraBoardESCC8IrqRoot, 0, sizeof(AuraBoardESCC8IrqRoot));
	memset(AuraBoardESCC2IrqRoot, 0, sizeof(AuraBoardESCC2IrqRoot));
	memset(AuraBoardMCSIrqRoot, 0, sizeof(AuraBoardMCSIrqRoot));
	
#if !defined(MODULE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0))
	if(do_probe == 0)
		return -1;			/* only allow to be called one 2.2.* */
	do_probe = 0;
#endif
	
	fn_init_crc_table();		/* used in faking ethernet packets for */
	/* the network driver -- crcs are currently */
	/* not being checked by this software */
	/* but is good to have them in case a frame */
	/* passes through a WAN LAN bridge */
	
	sab8253x_setup_ttydriver();	/* add synchronous tty and synchronous network
					   driver initialization */
	
	AuraBoardRoot = NULL;		/* basic lists */
	AuraChipRoot = NULL;
	AuraPortRoot = NULL;
	NumSab8253xPorts = 0;
	
	AuraXX20DriverParams.debug = auraXX20n_debug;
	AuraXX20DriverParams.listsize = sab8253xn_listsize;
	
	if(auraXX20n_name != 0)
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
		auraXX20n_prototype.name = auraXX20n_name;
#else
		strcpy(auraXX20n_prototype.name, auraXX20n_name);
#endif
	}
	
	/* find all multiport cards */
	XX20lastpdev = NULL;
	while(1)
	{
		boardptr = find_ati_multiport_card();
		if(boardptr == NULL)
		{
			printk(KERN_ALERT 
			       "auraXX20n: found %d AURAXX20 multiport device%s.\n", 
			       mp_probe_count, ((mp_probe_count == 1) ? "" : "s"));
			break;
		}
		boardptr->nextboard = AuraBoardRoot;
		AuraBoardRoot = boardptr;
		printk(KERN_ALERT "auraXX20n: found AURAXX20 multiport device #%d.\n",
		       mp_probe_count);
		++mp_probe_count;
	}
	
	/* find all cpci cards */
	XX20lastpdev = NULL;
	while(1)
	{
		boardptr = find_ati_cpci_card();
		if(boardptr == NULL)
		{
			printk(KERN_ALERT 
			       "auraXX20n: found %d AURAXX20 CPCI device%s.\n", 
			       cp_probe_count, ((cp_probe_count == 1) ? "" : "s"));
			break;
		}
		boardptr->nextboard = AuraBoardRoot;
		AuraBoardRoot = boardptr;
		printk(KERN_ALERT "auraXX20n: found AURAXX20 CPCI device #%d.\n",
		       cp_probe_count);
		++cp_probe_count;
	}
	/* find all WAN MS cards */
	XX20lastpdev = NULL;
	while(1)
	{
		boardptr = find_ati_wanms_card();
		if(boardptr == NULL)
		{
			printk(KERN_ALERT 
			       "auraXX20n: found %d AURAXX20 WANMS device%s.\n", 
			       wm_probe_count, ((wm_probe_count == 1) ? "" : "s"));
			break;
		}
		boardptr->nextboard = AuraBoardRoot;
		AuraBoardRoot = boardptr;
		printk(KERN_ALERT "auraXX20n: found AURAXX20 WANMS device #%d.\n",
		       wm_probe_count);
		++wm_probe_count;
	}
	
	/* Now do the chips! */
	
	for(boardptr = AuraBoardRoot; boardptr != NULL; boardptr = boardptr->nextboard)
	{
		SetupAllChips(boardptr);	/* sets up the ports on the chips */
	}
	
				/* set up global driver structures
				 * for async tty, call out device
				 * for sync tty and for network device
				 */

				/* NOW TURN ON THE PLX INTS */
				/* note all port ints (only receive right now)
				 * are off */

				/* interrupts cannot be turned on by port
				   this seems to be the only sensible place
				   to do it*/

				/* only at this point is the number of
				 * ttys to be created known. */

	if(finish_sab8253x_setup_ttydriver() ==  -1) /* only as many termios are allocated */
		/* as needed */
	{
		return 0;
	}
	for(portno = 0, portptr = AuraPortRoot; portptr != NULL; ++portno, portptr = portptr->next)
	{
		portptr->line = portno;	/* set up the line number == minor dev associated with port */
		portptr->sigmode = sab8253x_default_sp502_mode; 
				/* if we have SP502s let getty work with RS232 by default */
				/* unless overridden in module setup. */
	}
	/* Now lets set up the network devices */
	for(portno = 0, portptr = AuraPortRoot; portptr != NULL; ++portno, portptr = portptr->next)
	{
		
		dev = kmalloc(sizeof(struct net_device), GFP_KERNEL);
		if(!dev)
		{
			break;
		}
		memset(dev, 0, sizeof(struct net_device));
		*dev = auraXX20n_prototype;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
		dev->name = kmalloc(IFNAMSIZ+1, GFP_KERNEL);
		if(!dev->name)
		{
			kfree(dev);
			break;
		}
#endif
		namelength = MIN(strlen(auraXX20n_prototype.name), IFNAMSIZ);
		strcpy(dev->name, auraXX20n_prototype.name);
		sprintf(&dev->name[namelength-1], "%3.3d", portno);
		
#if 1
		current_sab_port = portptr;
#else
		dev->priv = portptr;
#endif
		result = register_netdev(dev);
		if(result)
		{			/* if we run into some internal kernel limit */
			break;
		}
		printk(KERN_ALERT "sab8253xn: found sab8253x network device #%d.\n",
		       portno);
	}
	printk(KERN_ALERT 
	       "sab8253xn: found %d sab8253x network device%s.\n", 
	       portno, ((portno == 1) ? "" : "s"));
	
	/* Now lets set up the character device */
	
	if(sab8253xc_name)
	{
		result = register_chrdev(sab8253xc_major, sab8253xc_name, &sab8253xc_fops);
		if(result < 0)
		{
			sab8253xc_major = result;
			printk(KERN_ALERT "Could not install sab8253xc device.\n");
		}
		else if(result > 0)
		{
			sab8253xc_major = result;
		}
	}
	
	for(boardptr = AuraBoardRoot; boardptr != NULL; boardptr = boardptr->nextboard)
	{				/* let's set up port interrupt lists */
		intr_val = boardptr->b_irq;
		if((intr_val < 0) || (intr_val >= NUMINTS))
		{
			printk(KERN_ALERT "sab8253xn:  bad interrupt %i board %p.\n", intr_val, boardptr);
			continue;
		}
		switch(boardptr->b_type)
		{
		case BD_WANMCS:
			boardptr->next_on_interrupt = AuraBoardMCSIrqRoot[intr_val];
			AuraBoardMCSIrqRoot[intr_val] = boardptr;
			break;
		case BD_8520P:
		case BD_8520CP:
			boardptr->next_on_interrupt = AuraBoardESCC8IrqRoot[intr_val];
			AuraBoardESCC8IrqRoot[intr_val] = boardptr;
			break;
		default:
			boardptr->next_on_interrupt = AuraBoardESCC2IrqRoot[intr_val];
			AuraBoardESCC2IrqRoot[intr_val] = boardptr;
			break;
		}
	}
	
	for(intr_val = 0; intr_val < NUMINTS; ++intr_val) /* trying to install as few int handlers as possible */
	{				/* one for each group of boards on a given irq */
		if((AuraBoardESCC2IrqRoot[intr_val] != NULL) || (AuraBoardESCC8IrqRoot[intr_val] != NULL) ||
		   (AuraBoardMCSIrqRoot[intr_val] != NULL))
		{
			if (request_irq(intr_val, sab8253x_interrupt, SA_SHIRQ,
					"sab8253x", &AuraBoardESCC2IrqRoot[intr_val]) == 0) 
				/* interrupts on perboard basis
				 * cycle through chips and then
				 * ports */
				/* NOTE PLX INTS ARE OFF -- so turn them on */
			{
				for(boardptr = AuraBoardESCC2IrqRoot[intr_val]; boardptr != NULL; 
				    boardptr = boardptr->next_on_interrupt)
				{
					writel(PLX_INT_ON, &(boardptr->b_bridge->intr));
				}
				for(boardptr = AuraBoardESCC8IrqRoot[intr_val]; boardptr != NULL; 
				    boardptr = boardptr->next_on_interrupt)
				{
					writel(PLX_INT_ON, &(boardptr->b_bridge->intr));
				}
				for(boardptr = AuraBoardMCSIrqRoot[intr_val]; boardptr != NULL; 
				    boardptr = boardptr->next_on_interrupt)
				{
					/* write to the MIC csr to reset the PCI interrupt */
					writeb(0, (unsigned char*)(boardptr->MICCMD_REG +  MICCMD_MICCSR));
					
					/* now, write to the CIM interrupt ena to re-enable interrupt generation */
					writeb(0, (unsigned char*)(boardptr->CIMCMD_REG + CIMCMD_WRINTENA));
					
					/* now, activate PCI interrupts */
					writel(AMCC_AOINTPINENA, (unsigned int*)(boardptr->AMCC_REG + AMCC_INTCSR));
				}
			}
			else
			{
				printk(KERN_ALERT "Unable to get interrupt, board set up not complete %i.\n", intr_val);
			}
		}
	}
	
	/* all done!  a lot of work */
	
#if !defined(MODULE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0))
	return -1;			/* otherwise 2.2 probe uses up
					 * a default device structure*/
#else
	return 0;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#ifdef MODULE
/* cleanup module/free up virtual memory */
/* space*/
void cleanup_module(void)
#endif
#else
void auraXX20_cleanup(void)
#endif
{
	SAB_BOARD *boardptr;
	SAB_CHIP *chipptr;
	SAB_PORT *portptr;
	AURA_CIM *cimptr;
	int intr_val;
	extern void sab8253x_cleanup_ttydriver(void);
	
	printk(KERN_ALERT "auraXX20n: unloading AURAXX20 driver.\n");
	
	sab8253x_cleanup_ttydriver();	/* clean up tty */
	
	/* unallocate and turn off ints */
	for(intr_val = 0; intr_val < NUMINTS; ++intr_val)
	{
		if((AuraBoardESCC2IrqRoot[intr_val] != NULL) || (AuraBoardESCC8IrqRoot[intr_val] != NULL) ||
		   (AuraBoardMCSIrqRoot[intr_val] != NULL))
		{
			for(boardptr = AuraBoardESCC2IrqRoot[intr_val]; boardptr != NULL; 
			    boardptr = boardptr->next_on_interrupt)
			{
				writel(PLX_INT_OFF, &(boardptr->b_bridge->intr));
			}
			for(boardptr = AuraBoardESCC8IrqRoot[intr_val]; boardptr != NULL; 
			    boardptr = boardptr->next_on_interrupt)
			{
				writel(PLX_INT_OFF, &(boardptr->b_bridge->intr));
			}
			for(boardptr = AuraBoardMCSIrqRoot[intr_val]; boardptr != NULL; 
			    boardptr = boardptr->next_on_interrupt)
			{
				writel(AMCC_INT_OFF, (unsigned int*)(boardptr->AMCC_REG + AMCC_INTCSR));
				(void) wanmcs_reset(boardptr);
				writel(AMCC_INT_OFF, (unsigned int*)(boardptr->AMCC_REG + AMCC_INTCSR));
			}
			
			free_irq(intr_val, &AuraBoardESCC2IrqRoot[intr_val]); /* free up board int
									       * note that if two boards
									       * share an int, two int
									       * handlers were registered
									       * 
									       */
		}
	}
	
	/* disable chips and free board memory*/
	while(AuraBoardRoot)
	{
		boardptr = AuraBoardRoot;
		for(chipptr = boardptr->board_chipbase; chipptr != NULL; chipptr = chipptr->next_by_board)
		{
			(*chipptr->int_disable)(chipptr); /* make sure no ints can come int */
		}
		AuraBoardRoot = boardptr->nextboard;
		if(boardptr->b_type == BD_WANMCS)
		{
			if(boardptr->virtbaseaddress0 != 0)
			{
				DEBUGPRINT((KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
					    (void*)boardptr->virtbaseaddress0));
				iounmap((void*)boardptr->virtbaseaddress0);
				boardptr->virtbaseaddress0 = 0;
			}
			
			if(boardptr->virtbaseaddress1 != 0)
			{
				DEBUGPRINT((KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
					    (void*)boardptr->virtbaseaddress1));
				iounmap((void*)boardptr->virtbaseaddress1);
				boardptr->virtbaseaddress1 = 0;
			}
			
			if(boardptr->virtbaseaddress2 != 0)
			{
				DEBUGPRINT((KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
					    (void*)boardptr->virtbaseaddress2));
				iounmap((void*)boardptr->virtbaseaddress2);
				boardptr->virtbaseaddress2 = 0;
			}
			
			if(boardptr->virtbaseaddress3 != 0)
			{
				DEBUGPRINT((KERN_ALERT "auraXX20n: unmapping virtual address %p.\n",
					    (void*)boardptr->virtbaseaddress3));
				iounmap((void*)boardptr->virtbaseaddress3);
				boardptr->virtbaseaddress3 = 0;
			}
			
		}
		else			/* everything but wan multichannel servers */
		{
			if(boardptr->virtbaseaddress0)
			{
				DEBUGPRINT((KERN_ALERT
					    "auraXX20n: unmapping virtual address %p.\n", 
					    (void*)boardptr->virtbaseaddress0));
				iounmap((void*)boardptr->virtbaseaddress0);
				boardptr->virtbaseaddress0 = 0;
			}
			if(boardptr->virtbaseaddress2)
			{
				DEBUGPRINT((KERN_ALERT
					    "auraXX20n: unmapping virtual address %p.\n", 
					    (void*)boardptr->virtbaseaddress2));
				iounmap((void*)boardptr->virtbaseaddress2);
				boardptr->virtbaseaddress2 = 0;
			}
		}
		kfree(boardptr);
	}
	
	while(AuraCimRoot)
	{
		cimptr = AuraCimRoot;
		AuraCimRoot = cimptr->next;
		kfree(cimptr);
	}
	
	
	while(AuraChipRoot)		/* free chip memory */
	{
		chipptr = AuraChipRoot;
		AuraChipRoot = chipptr->next;
		kfree(chipptr);
	}
	
	if(sab8253xc_name && (sab8253xc_major > 0)) /* unregister the chr device */
	{
		unregister_chrdev(sab8253xc_major, sab8253xc_name);
	}
	
	while(Sab8253xRoot)		/* free up network stuff */
	{
		SAB_PORT *priv;
		priv = (SAB_PORT *)Sab8253xRoot->priv;
		unregister_netdev(Sab8253xRoot);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
		kfree(Sab8253xRoot.name);
#endif
		kfree(Sab8253xRoot);
		Sab8253xRoot = priv->next_dev;
	}
	
	while(AuraPortRoot)		/* free up port memory */
	{
		portptr = AuraPortRoot;
		AuraPortRoot = portptr->next;
		if(portptr->dcontrol2.receive)
		{
			kfree(portptr->dcontrol2.receive);
		}
		if(portptr->dcontrol2.transmit)
		{
			kfree(portptr->dcontrol2.transmit);
		}
		kfree(portptr);
	}
}

/*
 * Hardware dependent read and write functions.
 * We have functions to write/read a byte, write/read
 * a word and read and write the FIFO
 */


/***************************************************************************
 * aura_readb:    Function to read a byte on a 4X20P, 8X20P or Sun serial
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg: The address of the register
 *
 *     Return value : The value of the register. 
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/

static unsigned char aura_readb(struct sab_port *port, unsigned char *reg) 
{
	return readb(reg);
}

/***************************************************************************
 * aura_writeb:    Function to write a byte on a 4X20P, 8X20P or Sun serial
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg:  The address of the register
 *                   val:  The value to put into the register
 *
 *     Return value : None
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/

static void aura_writeb(struct sab_port *port, unsigned char *reg,unsigned char val) 
{
	writeb(val,reg);
}

/***************************************************************************
 * aura_readw:    Function to read a word on a 4X20P, 8X20P or Sun serial
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg: The address of the hw memory to access
 *
 *     Return value : The value of the memory area. 
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/
static unsigned short aura_readw(struct sab_port *port, unsigned short *reg) 
{
	return readw(reg);
}

/***************************************************************************
 * aura_writew:    Function to write a word on a 4X20P, 8X20P or Sun serial
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg:  The address of the hw memory to access
 *                   val:  The value to put into the register
 *
 *     Return value : The value of the memory area. 
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/

static void aura_writew(struct sab_port *port, unsigned short *reg,unsigned short val) 
{
	writew(val,reg);
}

/***************************************************************************
 * aura_readfifo:    Function to read the FIFO on a 4X20P, 8X20P or Sun serial
 *                
 *
 *     Parameters   : 
 *                   port:  The port being accessed
 *                   buf:   The address of a buffer where we should put
 *                          what we read
 *                  nbytes: How many chars to read.
 *
 *     Return value : none
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 13 2000, creation
 ***************************************************************************/
static void aura_readfifo(struct sab_port *port, unsigned char *buf, unsigned int nbytes) 
{
	int i;
	unsigned short *wptr = (unsigned short*) buf;
	int nwords = ((nbytes+1)/2);
	for(i = 0; i < nwords; i ++) 
	{
		wptr[i] = readw(((unsigned short *)port->regs));
	}
}

/***************************************************************************
 * aura_writefifo:    Function to write the FIFO on a 4X20P, 8X20P or Sun serial
 *                
 *
 *     Parameters   : 
 *                   port:  The port being accessed
 *
 *     Return value : none
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 13 2000, creation
 ***************************************************************************/
static void aura_writefifo(struct sab_port *port) 
{
	int i,max,maxw;
	unsigned short *wptr;
	unsigned char buffer[32];
	
	if(port->xmit_cnt <= 0)
	{
		return; 
	}
	max= (port->xmit_fifo_size < port->xmit_cnt) ? port->xmit_fifo_size : port->xmit_cnt;
	
	for (i = 0; i < max; i++) 
	{
		buffer[i] = port->xmit_buf[port->xmit_tail++];
		port->xmit_tail &= (SAB8253X_XMIT_SIZE - 1);
		port->icount.tx++;
		port->xmit_cnt--;
	}
	
	maxw = max/2;
	wptr = (unsigned short*) buffer;
	
	for(i = 0; i < maxw; ++i)
	{
		writew(wptr[i], (unsigned short *)port->regs);
	}
	
	if(max & 1)
	{
		writeb(buffer[max-1], (unsigned char*)port->regs);
	}
}

/***************************************************************************
 * wmsaura_readb:    Function to read a byte on a LMS, WMS
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg: The address of the register
 *
 *     Return value : The value of the register. 
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : TO BE IMPLEMENTED 
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/

static unsigned char wmsaura_readb(struct sab_port *port, unsigned char *reg) 
{
	return readb((unsigned char*) (((unsigned int) reg) + CIMCMD_RDREGB));
}

/***************************************************************************
 * wmsaura_writeb:    Function to write a byte on a LMS, WMS
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg:  The address of the register
 *                   val:  The value to put into the register
 *
 *     Return value : None
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : TO BE IMPLEMENTED
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/

static void wmsaura_writeb(struct sab_port *port, unsigned char *reg,unsigned char val) 
{
	writeb(val, (unsigned char*) (((unsigned int) reg) + CIMCMD_WRREGB));
}

/***************************************************************************
 * wmsaura_readw:    Function to read a word on a LMS, WMS
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg: The address of the hw memory to access
 *
 *     Return value : The value of the memory area. 
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : TO BE IMPLEMENTED
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/
static unsigned short wmsaura_readw(struct sab_port *port, unsigned short *reg) 
{
	unsigned short readval;
	unsigned int address;
	address = (unsigned int) reg;
	
	readval =  readb((unsigned char*) (address + CIMCMD_RDREGB));
	++address;
	return (readval | (readb((unsigned char*) (address + CIMCMD_RDREGB)) << 8));
}

/***************************************************************************
 * wmsaura_writew:    Function to write a word on a LMS, WMS
 *                
 *
 *     Parameters   : 
 *                   port: The port being accessed
 *                   reg:  The address of the hw memory to access
 *                   val:  The value to put into the register
 *
 *     Return value : The value of the memory area. 
 *
 *     Prerequisite : The port must have been opened
 *
 *     Remark       : TO BE IMPLEMENTED
 *
 *     Author       : fw
 *
 *     Revision     : Oct 10 2000, creation
 ***************************************************************************/

static void wmsaura_writew(struct sab_port *port, unsigned short *reg, unsigned short val) 
{
	unsigned char vallow;
	unsigned char valhigh;
	unsigned int address;
	
	address = (unsigned int) reg;
	
	vallow = (unsigned char) val;
	valhigh = (unsigned char) (val >> 8);
	
	writeb(vallow, (unsigned char*) (address + CIMCMD_WRREGB));
	++address;
	writeb(valhigh, (unsigned char*) (address + CIMCMD_WRREGB));
}

static void wmsaura_readfifo(struct sab_port *port, unsigned char *buf, unsigned int nbytes) 
{
#ifdef FIFO_DIRECT
	unsigned short fifo[32/2];	/* this array is word aligned
					 * buf may not be word aligned*/
	unsigned int nwords;
	int i;
	int wcount;
	unsigned int address;
	
	if (nbytes == 0) 
	{
		return;
	}
	
	wcount = ((nbytes + 1) >> 1);
	/* Read the thing into the local FIFO and copy it out. */
	address = (unsigned int) port->regs;
	
	for(i = 0; i < wcount; ++i)
	{
		fifo[i] = readw((unsigned short*)(address + CIMCMD_RDFIFOW));
	}
	
	memcpy((unsigned char*) buf, (unsigned char*) &(fifo[0]), (unsigned int) nbytes);
	
#else		/* FIFO_DIRECT */
	unsigned short fifo[32/2];
	int i;
	int wcount;
	SAB_BOARD *bptr;
	unsigned int channel;
	
	if (nbytes == 0) 
	{
		return;
	}
	
	bptr = port->board;
	wcount = ((nbytes + 1) >> 1);
	channel = (((unsigned char*) port->regs) - bptr->CIMCMD_REG); /* should be properly shifted */
	
	/*
	 * Trigger a cache read by writing the nwords - 1 to the
	 *  magic place.
	 */
	
	writeb((unsigned char) wcount, bptr->MICCMD_REG + (MICCMD_CACHETRIG + channel));
	
	/*
	 * Now, read out the contents.
	 */
	
	channel >>= 1;
	
	for(i = 0; i < wcount; ++i)
	{
		fifo[i] = readw((unsigned short*)(bptr->FIFOCACHE_REG + (channel + (i << 1))));
	}
	
	memcpy((unsigned char*) buf, (unsigned char*) &(fifo[0]), (unsigned int) nbytes);
#endif		/* !FIFO_DIRECT */
}

static void wmsaura_writefifo(struct sab_port *port)
{
	unsigned short fifo[32/2];
	unsigned char* fifob = (unsigned char*) fifo;
	int i,max;
	int wcount;
	unsigned int address;
	
	if(port->xmit_cnt <= 0) 
	{
		return; 
	}
	max = (port->xmit_fifo_size < port->xmit_cnt) ? port->xmit_fifo_size:port->xmit_cnt;
	for (i = 0; i < max; i++) 
	{
		fifob[i] = port->xmit_buf[port->xmit_tail++];
		port->xmit_tail &= (SAB8253X_XMIT_SIZE - 1);
		port->icount.tx++;
		port->xmit_cnt--;
	}
	
	wcount = (max >> 1);
	/* Copy from the linear local FIFO into the hardware fifo. */
	address = (unsigned int) port->regs;
	
	for(i = 0; i < wcount; ++i)
	{
		writew(fifo[i], (unsigned short*)(address + CIMCMD_WRFIFOW));
	}
	if(max & 1)			/* odd byte */
	{
		--max;
		writeb(fifob[max], (unsigned short*)(address + CIMCMD_WRFIFOB));
	}
}

module_init(auraXX20_probe);
module_exit(auraXX20_cleanup);
MODULE_DESCRIPTION("Aurora Multiport Multiprotocol Serial Driver");
MODULE_AUTHOR("Joachim Martillo <martillo@telfordtools.com>");
