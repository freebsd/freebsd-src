/* 
 * Driver for Intel I82092AA PCI-PCMCIA bridge.
 *
 * (C) 2001-2003 Red Hat, Inc.
 *
 * Author: Arjan Van De Ven <arjanv@redhat.com> 
 * Loosely based on i82365.c from the pcmcia-cs package
 *
 * $Id: i82092.c,v 1.16 2003/04/15 16:36:42 dwmw2 Exp $
 */

#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/tqueue.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>

#include <asm/system.h>
#include <asm/io.h>

#include "i82092aa.h"
#include "i82365.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Red Hat, Inc. - Arjan Van De Ven <arjanv@redhat.com>");
MODULE_DESCRIPTION("Socket driver for Intel i82092AA PCI-PCMCIA bridge");

/* Extra i82092-specific register */
#define I365_CPAGE 0x26

/* PCI core routines */
static struct pci_device_id i82092aa_pci_ids[] = {
	{
	      vendor:PCI_VENDOR_ID_INTEL,
	      device:PCI_DEVICE_ID_INTEL_82092AA_0,
	      subvendor:PCI_ANY_ID,
	      subdevice:PCI_ANY_ID,
	      class: 0, class_mask:0,

	 },
	 {} 
};

static struct pci_driver i82092aa_pci_drv = {
	name:           "i82092aa",
	id_table:       i82092aa_pci_ids,
	probe:          i82092aa_pci_probe,
	remove:         __devexit_p(i82092aa_pci_remove),
	suspend:        NULL,
	resume:         NULL 
};


/* the pccard structure and its functions */
static struct pccard_operations i82092aa_operations = {
	init: 		 	i82092aa_init,
	suspend:	   	i82092aa_suspend,
	register_callback: 	i82092aa_register_callback,
	inquire_socket:		i82092aa_inquire_socket,   
	get_status:		i82092aa_get_status,
	get_socket:		i82092aa_get_socket,
	set_socket:		i82092aa_set_socket,
	get_io_map:		i82092aa_get_io_map,
	set_io_map:		i82092aa_set_io_map,
	get_mem_map:		i82092aa_get_mem_map,
	set_mem_map:		i82092aa_set_mem_map,
	proc_setup:		i82092aa_proc_setup,
};

/* The card can do upto 4 sockets, allocate a structure for each of them */

struct socket_info {
	int	card_state; 	/*  0 = no socket,
				    1 = empty socket, 
				    2 = card but not initialized,
				    3 = operational card */
	unsigned long io_base; 	/* base io address of the socket */
	socket_cap_t cap;

	unsigned int pending_events; /* Pending events on this interface */
	
	void	(*handler)(void *info, u_int events); 
				/* callback to the driver of the card */
	void	*info;		/* to be passed to the handler */
	
	struct pci_dev *dev;	/* The PCI device for the socket */
};

#define MAX_SOCKETS 4
static struct socket_info sockets[MAX_SOCKETS];
static int socket_count;  /* shortcut */

int membase = -1;
int isa_setup;

MODULE_PARM(membase, "i");
MODULE_PARM(isa_setup, "i");

static int __init i82092aa_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	unsigned char configbyte;
	struct pci_dev *parent;
	int i;
	
	enter("i82092aa_pci_probe");
	
	if (pci_enable_device(dev))
		return -EIO;

	/* Since we have no memory BARs some firmware we may not
	   have had PCI_COMMAND_MEM enabled, yet the device needs
	   it. */
	pci_read_config_byte(dev, PCI_COMMAND, &configbyte);
	if (!(configbyte | PCI_COMMAND_MEMORY)) {
		dprintk(KERN_DEBUG "Enabling PCI_COMMAND_MEMORY\n");
		configbyte |= PCI_COMMAND_MEMORY;
		pci_write_config_byte(dev, PCI_COMMAND, configbyte);
	}

	pci_read_config_byte(dev, 0x40, &configbyte);  /* PCI Configuration Control */
	switch(configbyte&6) {
		case 0:
			printk(KERN_INFO "i82092aa: configured as a 2 socket device.\n");
			socket_count = 2;
			break;
		case 2:
			printk(KERN_INFO "i82092aa: configured as a 1 socket device.\n");
			socket_count = 1;
			break;
		case 4:
		case 6:
			printk(KERN_INFO "i82092aa: configured as a 4 socket device.\n");
			socket_count = 4;
			break;
			
		default:
			printk(KERN_ERR "i82092aa: Oops, you did something we didn't think of.\n");
			return -EIO;
			break;
	}
	
	if (membase == -1) {
		for (i = 0; i < 4; i++) {
			if ((dev->bus->resource[i]->flags & (IORESOURCE_MEM|IORESOURCE_READONLY|IORESOURCE_CACHEABLE|IORESOURCE_SHADOWABLE))
			    == IORESOURCE_MEM) {
				membase = dev->bus->resource[i]->start >> 24;
				goto mem_ok;
			}
		}
		printk(KERN_WARNING "No suitable memory range for i82092aa found\n");
		return -ENOSPC;
	}
 mem_ok:
	if (membase)
		printk(KERN_NOTICE "i82092 memory base address set to 0x%02x000000\n", membase);

	/* If we're down the end of the PCI bus chain where ISA cycles don't get sent, then
	   only 1/4 of the I/O address space is going to be usable, unless we make sure that
	   the NO_ISA bit in the Bridge Control register of all upstream busses is cleared.
	   Since some PCMCIA cards (most serial ports, for example) will decode 10 bits and
	   respond only to addresses where bits 8 and 9 are non-zero, we need to do this. */
	for (parent = dev->bus->self; 
	     parent && (parent->class>>8) == PCI_CLASS_BRIDGE_PCI;
	     parent = parent->bus->self) {
		uint16_t brctl;

		if (pci_read_config_word(parent, PCI_BRIDGE_CONTROL, &brctl)) {
			printk(KERN_WARNING "Error reading bridge control word from device %s\n", parent->slot_name);
			continue;
		}

		if (!(brctl & PCI_BRIDGE_CTL_NO_ISA))
			continue;

		if (isa_setup) {
			printk(KERN_NOTICE "PCI bridge %s has NOISA bit set. Clearing to allow full PCMCIA operation\n",
			       parent->slot_name);
			brctl &= ~PCI_BRIDGE_CTL_NO_ISA;
			if (pci_write_config_word(parent, PCI_BRIDGE_CONTROL, brctl))
				printk(KERN_WARNING "Error writing bridge control word from device %s\n", parent->slot_name);
		} else {
			printk(KERN_NOTICE "PCI bridge %s has NOISA bit set. Some I/O addresses for PCMCIA cards will not work.\n",
			       parent->slot_name);
			printk(KERN_NOTICE "Perhaps use 'isa_setup=1' option to i82092.o?\n");
			break;
		}
	}			
				
	for (i = 0;i<socket_count;i++) {
		sockets[i].card_state = 1; /* 1 = present but empty */
		sockets[i].io_base = (dev->resource[0].start & ~1);
		 if (sockets[i].io_base > 0) 
		 	request_region(sockets[i].io_base, 2, "i82092aa");
		 
		
		sockets[i].cap.features |= SS_CAP_PCCARD | SS_CAP_PAGE_REGS;
		sockets[i].cap.map_size = 0x1000;
		sockets[i].cap.irq_mask = 0;
		sockets[i].cap.pci_irq  = dev->irq;

		/* Trick the resource code into doing the right thing... */
		sockets[i].cap.cb_dev = dev;
		
		if (card_present(i)) {
			sockets[i].card_state = 3;
			dprintk(KERN_DEBUG "i82092aa: slot %i is occupied\n",i);
		} else {
			dprintk(KERN_DEBUG "i82092aa: slot %i is vacant\n",i);
		}
	}
		
	/* Now, specifiy that all interrupts are to be done as PCI interrupts */
	configbyte = 0xFF; /* bitmask, one bit per event, 1 = PCI interrupt, 0 = ISA interrupt */
	pci_write_config_byte(dev, 0x50, configbyte); /* PCI Interrupt Routing Register */


	/* Register the interrupt handler */
	dprintk(KERN_DEBUG "Requesting interrupt %i \n",dev->irq);
	if (request_irq(dev->irq, i82092aa_interrupt, SA_SHIRQ, "i82092aa", i82092aa_interrupt)) {
		printk(KERN_ERR "i82092aa: Failed to register IRQ %d, aborting\n", dev->irq);
		return -EIO;
	}
		 
	
	if (register_ss_entry(socket_count, &i82092aa_operations) != 0)
		printk(KERN_NOTICE "i82092aa: register_ss_entry() failed\n");

	leave("i82092aa_pci_probe");
	return 0;
}

static void __devexit i82092aa_pci_remove(struct pci_dev *dev)
{
	enter("i82092aa_pci_remove");
	
	free_irq(dev->irq, i82092aa_interrupt);

	leave("i82092aa_pci_remove");
}

static spinlock_t port_lock = SPIN_LOCK_UNLOCKED;

/* basic value read/write functions */

static unsigned char indirect_read(int socket, unsigned short reg)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;
	spin_lock_irqsave(&port_lock,flags);
	reg += socket * 0x40;
	port = sockets[socket].io_base;
	outb(reg,port);
	val = inb(port+1);
	spin_unlock_irqrestore(&port_lock,flags);
	return val;
}

static unsigned short indirect_read16(int socket, unsigned short reg)
{
	unsigned long port;
	unsigned short tmp;
	unsigned long flags;
	spin_lock_irqsave(&port_lock,flags);
	reg  = reg + socket * 0x40;
	port = sockets[socket].io_base;
	outb(reg,port);
	tmp = inb(port+1);
	reg++;
	outb(reg,port);
	tmp = tmp | (inb(port+1)<<8);
	spin_unlock_irqrestore(&port_lock,flags);
	return tmp;
}

static void indirect_write(int socket, unsigned short reg, unsigned char value)
{
	unsigned long port;
	unsigned long flags;
	spin_lock_irqsave(&port_lock,flags);
	reg = reg + socket * 0x40;
	port = sockets[socket].io_base; 
	outb(reg,port);
	outb(value,port+1);
	spin_unlock_irqrestore(&port_lock,flags);
}

static void indirect_setbit(int socket, unsigned short reg, unsigned char mask)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;
	spin_lock_irqsave(&port_lock,flags);
	reg = reg + socket * 0x40;
	port = sockets[socket].io_base; 
	outb(reg,port);
	val = inb(port+1);
	val |= mask;
	outb(reg,port);
	outb(val,port+1);
	spin_unlock_irqrestore(&port_lock,flags);
}


static void indirect_resetbit(int socket, unsigned short reg, unsigned char mask)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;
	spin_lock_irqsave(&port_lock,flags);
	reg = reg + socket * 0x40;
	port = sockets[socket].io_base; 
	outb(reg,port);
	val = inb(port+1);
	val &= ~mask;
	outb(reg,port);
	outb(val,port+1);
	spin_unlock_irqrestore(&port_lock,flags);
}

static void indirect_write16(int socket, unsigned short reg, unsigned short value)
{
	unsigned long port;
	unsigned char val;
	unsigned long flags;
	spin_lock_irqsave(&port_lock,flags);
	reg = reg + socket * 0x40;
	port = sockets[socket].io_base; 
	
	outb(reg,port);
	val = value & 255;
	outb(val,port+1);
	
	reg++;
	
	outb(reg,port);
	val = value>>8;
	outb(val,port+1);
	spin_unlock_irqrestore(&port_lock,flags);
}

/* simple helper functions */
/* External clock time, in nanoseconds.  120 ns = 8.33 MHz */
static int cycle_time = 120;

static int to_cycles(int ns)
{
	if (cycle_time!=0)
		return ns/cycle_time;
	else
		return 0;
}
    
static int to_ns(int cycles)
{
	return cycle_time*cycles;
}


/* Interrupt handler functionality */

static void i82092aa_bh(void *dummy)
{
        unsigned int events;
	int i;
                
        for (i=0; i < socket_count; i++) {
        	events = xchg(&(sockets[i].pending_events),0);
        	dprintk("events = %x \n",events);
                if (sockets[i].handler)
                	sockets[i].handler(sockets[i].info, events);
	}
}
                                                                                                                                        

static struct tq_struct i82092aa_task = {
        routine:        i82092aa_bh
};
        

static void i82092aa_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	int i;
	int loopcount = 0;
	
	unsigned int events, active=0;
	
/*	enter("i82092aa_interrupt");*/
	
	while (1) {
		loopcount++;
		if (loopcount>20) {
			printk(KERN_ERR "i82092aa: infinite eventloop in interrupt \n");
			break;
		}
		
		active = 0;
		
		for (i=0;i<socket_count;i++) {
			int csc;
			if (sockets[i].card_state==0) /* Inactive socket, should not happen */
				continue;
			
			csc = indirect_read(i,I365_CSC); /* card status change register */
			
			if ((csc==0) ||  /* no events on this socket */
			   (sockets[i].handler==NULL)) /* no way to handle events */
			   	continue;
			events = 0;
			 
			if (csc & I365_CSC_DETECT) {
				events |= SS_DETECT;
				dprintk("Card detected in socket %i!\n",i);
			 }
			
			if (indirect_read(i,I365_INTCTL) & I365_PC_IOCARD) { 
				/* For IO/CARDS, bit 0 means "read the card" */
				events |= (csc & I365_CSC_STSCHG) ? SS_STSCHG : 0; 
			} else {
				/* Check for battery/ready events */
				events |= (csc & I365_CSC_BVD1) ? SS_BATDEAD : 0;
				events |= (csc & I365_CSC_BVD2) ? SS_BATWARN : 0;
				events |= (csc & I365_CSC_READY) ? SS_READY : 0;
			}
			
			if (events) {
				sockets[i].pending_events |= events;
				schedule_task(&i82092aa_task);
			}
			active |= events;
		}
				
		if (active==0) /* no more events to handle */
			break;				
		
	}
	
/*	leave("i82092aa_interrupt");*/
}



/* socket functions */

static int card_present(int socketno)
{	
	unsigned int val;
	enter("card_present");
	
	if ((socketno<0) || (socketno > MAX_SOCKETS))
		return 0;
	if (sockets[socketno].io_base == 0)
		return 0;

		
	val = indirect_read(socketno, 1); /* Interface status register */
	if ((val&12)==12) {
		leave("card_present 1");
		return 1;
	}
		
	leave("card_present 0");
	return 0;
}

static void set_bridge_state(int sock)
{
	enter("set_bridge_state");
	indirect_write(sock, I365_GBLCTL,0x00);
	indirect_write(sock, I365_GENCTL,0x00);
	
	indirect_setbit(sock, I365_INTCTL,0x08);
	leave("set_bridge_state");
}





      
static int i82092aa_init(unsigned int s)
{
	int i;
        pccard_io_map io = { 0, 0, 0, 0, 1 };
        pccard_mem_map mem = { 0, 0, 0, 0, 0, 0 };
        
        enter("i82092aa_init");
                        
        i82092aa_set_socket(s, &dead_socket);
        for (i = 0; i < 2; i++) {
        	io.map = i;
                i82092aa_set_io_map(s, &io);
	}
        for (i = 0; i < 5; i++) {
        	mem.map = i;
                i82092aa_set_mem_map(s, &mem);
	}
	
	leave("i82092aa_init");
	return 0;
}
                                                                                                                                                                                                                                              
static int i82092aa_suspend(unsigned int sock)
{
	int retval;
	enter("i82092aa_suspend");
        retval =  i82092aa_set_socket(sock, &dead_socket);
        leave("i82092aa_suspend");
        return retval;
}
       
static int i82092aa_register_callback(unsigned int sock, void (*handler)(void *, unsigned int), void * info)
{
	enter("i82092aa_register_callback");
	sockets[sock].handler = handler;
        sockets[sock].info = info;
        if (handler == NULL) {   
        	MOD_DEC_USE_COUNT;   
 	} else {
		MOD_INC_USE_COUNT;
	}
	leave("i82092aa_register_callback");
	return 0;
} /* i82092aa_register_callback */
                                        
static int i82092aa_inquire_socket(unsigned int sock, socket_cap_t *cap)
{
	enter("i82092aa_inquire_socket");
	*cap = sockets[sock].cap;
	leave("i82092aa_inquire_socket");
	return 0;
} /* i82092aa_inquire_socket */


static int i82092aa_get_status(unsigned int sock, u_int *value)
{
	unsigned int status;
	
	enter("i82092aa_get_status");
	
	status = indirect_read(sock,I365_STATUS); /* Interface Status Register */
	*value = 0;
	
	if ((status & I365_CS_DETECT) == I365_CS_DETECT) {
		*value |= SS_DETECT;
	}
		
	/* IO cards have a different meaning of bits 0,1 */
	/* Also notice the inverse-logic on the bits */
	 if (indirect_read(sock, I365_INTCTL) & I365_PC_IOCARD)	{
	 	/* IO card */
	 	if (!(status & I365_CS_STSCHG))
	 		*value |= SS_STSCHG;
	 } else { /* non I/O card */
	 	if (!(status & I365_CS_BVD1))
	 		*value |= SS_BATDEAD;
	 	if (!(status & I365_CS_BVD2))
	 		*value |= SS_BATWARN;
	 		
	 }
	 
	 if (status & I365_CS_WRPROT)
	 	(*value) |= SS_WRPROT;	/* card is write protected */
	 
	 if (status & I365_CS_READY)
	 	(*value) |= SS_READY;    /* card is not busy */
	 	
	 if (status & I365_CS_POWERON)
	 	(*value) |= SS_POWERON;  /* power is applied to the card */


	leave("i82092aa_get_status");
	return 0;
}


static int i82092aa_get_socket(unsigned int sock, socket_state_t *state) 
{
	unsigned char reg,vcc,vpp;
	
	enter("i82092aa_get_socket");
	state->flags    = 0;
	state->Vcc      = 0;
	state->Vpp      = 0;
	state->io_irq   = 0;
	state->csc_mask = 0;

	/* First the power status of the socket */
	reg = indirect_read(sock,I365_POWER); /* PCTRL - Power Control Register */

	if (reg & I365_PWR_AUTO)
		state->flags |= SS_PWR_AUTO;  /* Automatic Power Switch */
		
	if (reg & I365_PWR_OUT)
		state->flags |= SS_OUTPUT_ENA; /* Output signals are enabled */
		
	vcc = reg & I365_VCC_MASK;    vpp = reg & I365_VPP1_MASK;
	
	if (reg & I365_VCC_5V) { /* Can still be 3.3V, in this case the Vcc value will be overwritten later */
		state->Vcc = 50;
		
		if (vpp == I365_VPP1_5V)
			state->Vpp = 50;
		if (vpp == I365_VPP1_12V)
			state->Vpp = 120;
			
	}
	
	if ((reg & I365_VCC_3V)==I365_VCC_3V)
		state->Vcc = 33;
	
	
	/* Now the IO card, RESET flags and IO interrupt */
	
	reg = indirect_read(sock, I365_INTCTL); /* IGENC, Interrupt and General Control */
	
	if ((reg & I365_PC_RESET)==0)
		state->flags |= SS_RESET;
	if (reg & I365_PC_IOCARD) 
		state->flags |= SS_IOCARD; /* This is an IO card */
	
	/* Set the IRQ number */
	if (sockets[sock].dev!=NULL)
		state->io_irq = sockets[sock].dev->irq;
	
	/* Card status change */
	reg = indirect_read(sock, I365_CSCINT); /* CSCICR, Card Status Change Interrupt Configuration */
	
	if (reg & I365_CSC_DETECT) 
		state->csc_mask |= SS_DETECT; /* Card detect is enabled */
	
	if (state->flags & SS_IOCARD) {/* IO Cards behave different */
		if (reg & I365_CSC_STSCHG)
			state->csc_mask |= SS_STSCHG;
	} else {
		if (reg & I365_CSC_BVD1) 
			state->csc_mask |= SS_BATDEAD;
		if (reg & I365_CSC_BVD2) 
			state->csc_mask |= SS_BATWARN;
		if (reg & I365_CSC_READY) 
			state->csc_mask |= SS_READY;
	}
		
	leave("i82092aa_get_socket");
	return 0;
}

static int i82092aa_set_socket(unsigned int sock, socket_state_t *state) 
{
	unsigned char reg;
	
	enter("i82092aa_set_socket");
	
	/* First, set the global controller options */
	
	set_bridge_state(sock);
	
	/* Values for the IGENC register */
	
	reg = 0;
	if (!(state->flags & SS_RESET)) 	/* The reset bit has "inverse" logic */
		reg = reg | I365_PC_RESET;  
	if (state->flags & SS_IOCARD) 
		reg = reg | I365_PC_IOCARD;
		
	indirect_write(sock,I365_INTCTL,reg); /* IGENC, Interrupt and General Control Register */
	
	/* Power registers */
	
	reg = I365_PWR_NORESET; /* default: disable resetdrv on resume */
	
	if (state->flags & SS_PWR_AUTO) {
		dprintk("Auto power\n");
		reg |= I365_PWR_AUTO;	/* automatic power mngmnt */
	}
	if (state->flags & SS_OUTPUT_ENA) {
		dprintk("Power Enabled \n");
		reg |= I365_PWR_OUT;	/* enable power */
	}
	
	switch (state->Vcc) {
		case 0:	
			break;
		case 50: 
			dprintk("setting voltage to Vcc to 5V on socket %i\n",sock);
			reg |= I365_VCC_5V;
			break;
		default:
			printk(KERN_WARNING "i82092aa: i82092aa_set_socket called with invalid VCC power value: %i ", state->Vcc);
			leave("i82092aa_set_socket");
			return -EINVAL;
	}
	
	
	switch (state->Vpp) {
		case 0:	
			dprintk("not setting Vpp on socket %i\n",sock);
			break;
		case 50: 
			dprintk("setting Vpp to 5.0 for socket %i\n",sock);
			reg |= I365_VPP1_5V | I365_VPP2_5V;
			break;
		case 120: 
			dprintk("setting Vpp to 12.0\n");
			reg |= I365_VPP1_12V | I365_VPP2_12V;
			break;
		default:
			printk(KERN_WARNING "i82092aa: i82092aa_set_socket called with invalid VPP power value: %i ", state->Vcc);
			leave("i82092aa_set_socket");
			return -EINVAL;
	}
	
	if (reg != indirect_read(sock,I365_POWER)) /* only write if changed */
		indirect_write(sock,I365_POWER,reg);
		
	/* Enable specific interrupt events */
	
	reg = 0x00;
	if (state->csc_mask & SS_DETECT) {
		reg |= I365_CSC_DETECT;
	}
	if (state->flags & SS_IOCARD) {
		if (state->csc_mask & SS_STSCHG)
			reg |= I365_CSC_STSCHG;
	} else {
		if (state->csc_mask & SS_BATDEAD) 
			reg |= I365_CSC_BVD1;
		if (state->csc_mask & SS_BATWARN) 
			reg |= I365_CSC_BVD2;
		if (state->csc_mask & SS_READY) 
			reg |= I365_CSC_READY; 
		                        
	}
	
	/* now write the value and clear the (probably bogus) pending stuff by doing a dummy read*/
	
	indirect_write(sock,I365_CSCINT,reg);
	(void)indirect_read(sock,I365_CSC);

	leave("i82092aa_set_socket");
	return 0;
}

static int i82092aa_get_io_map(unsigned int sock, struct pccard_io_map *io)
{
	unsigned char map, ioctl, addr;
	
	enter("i82092aa_get_io_map");
	map = io->map;
	if (map > 1) {
		leave("i82092aa_get_io_map with -EINVAL");
		return -EINVAL;
	}
	
	/* FIXME: How does this fit in with the PCI resource (re)allocation */
	io->start = indirect_read16(sock, I365_IO(map)+I365_W_START);
	io->stop  = indirect_read16(sock, I365_IO(map)+I365_W_START);
	
	ioctl = indirect_read(sock,I365_IOCTL); /* IOCREG: I/O Control Register */
	addr  = indirect_read(sock,I365_ADDRWIN); /* */
	
	io->speed = to_ns(ioctl & I365_IOCTL_WAIT(map)) ? 1 : 0; /* check this out later */
	io->flags = 0;
	
	if (addr & I365_IOCTL_16BIT(map))
		io->flags |= MAP_AUTOSZ;
		
	leave("i82092aa_get_io_map");
	return 0;
}

static int i82092aa_set_io_map(unsigned sock, struct pccard_io_map *io)
{
	unsigned char map, ioctl;
	
	enter("i82092aa_set_io_map");
	
	map = io->map;
	
	/* Check error conditions */	
	if (map > 1) {
		leave("i82092aa_set_io_map with invalid map");
		return -EINVAL;
	}
	if ((io->start > 0xffff) || (io->stop > 0xffff) || (io->stop < io->start)){
		leave("i82092aa_set_io_map with invalid io");
		return -EINVAL;
	}

	/* Turn off the window before changing anything */ 
	if (indirect_read(sock, I365_ADDRWIN) & I365_ENA_IO(map))
		indirect_resetbit(sock, I365_ADDRWIN, I365_ENA_IO(map));

/*	printk("set_io_map: Setting range to %x - %x \n",io->start,io->stop);  */
	
	/* write the new values */
	indirect_write16(sock,I365_IO(map)+I365_W_START,io->start);            	
	indirect_write16(sock,I365_IO(map)+I365_W_STOP,io->stop);            	
	            	
	ioctl = indirect_read(sock,I365_IOCTL) & ~I365_IOCTL_MASK(map);
	
	if (io->flags & (MAP_16BIT|MAP_AUTOSZ))
		ioctl |= I365_IOCTL_16BIT(map);
		
	indirect_write(sock,I365_IOCTL,ioctl);
	
	/* Turn the window back on if needed */
	if (io->flags & MAP_ACTIVE)
		indirect_setbit(sock,I365_ADDRWIN,I365_ENA_IO(map));
			
	leave("i82092aa_set_io_map");	
	return 0;
}

static int i82092aa_get_mem_map(unsigned sock, struct pccard_mem_map *mem)
{
	unsigned short base, i;
	unsigned char map, addr;
	
	enter("i82092aa_get_mem_map");
	
	mem->flags = 0;
	mem->speed = 0;
	map = mem->map;
	if (map > 4) {
		leave("i82092aa_get_mem_map: -EINVAL");
		return -EINVAL;
	}
	
	addr = indirect_read(sock, I365_ADDRWIN);
		
	if (addr & I365_ENA_MEM(map))
		mem->flags |= MAP_ACTIVE;		/* yes this mapping is active */
	
	base = I365_MEM(map); 
	
	/* Find the start address - this register also has mapping info */
	
	i = indirect_read16(sock,base+I365_W_START);
	if (i & I365_MEM_16BIT)
		mem->flags |= MAP_16BIT;
	if (i & I365_MEM_0WS)
		mem->flags |= MAP_0WS;
	
	mem->sys_start = ((unsigned long)(i & 0x0fff) << 12);
	
	/* Find the end address - this register also has speed info */
	i = indirect_read16(sock,base+I365_W_STOP);
	if (i & I365_MEM_WS0)
		mem->speed = 1;
	if (i & I365_MEM_WS1)
		mem->speed += 2;
	mem->speed = to_ns(mem->speed);
	mem->sys_stop = ( (unsigned long)(i & 0x0fff) << 12) + 0x0fff;
	
	/* Find the card start address, also some more MAP attributes */
	
	i = indirect_read16(sock, base+I365_W_OFF);
	if (i & I365_MEM_WRPROT)
		mem->flags |= MAP_WRPROT;
	if (i & I365_MEM_REG)
		mem->flags |= MAP_ATTRIB;
	mem->card_start = ( (unsigned long)(i & 0x3fff)<12) + mem->sys_start;
	mem->card_start &=  0x3ffffff;
	
	dprintk("Card %i is from %lx to %lx \n",sock,mem->sys_start,mem->sys_stop);
	
	leave("i82092aa_get_mem_map");
	return 0;
	
}

static int i82092aa_set_mem_map(unsigned sock, struct pccard_mem_map *mem)
{
	unsigned short base, i;
	unsigned char map;

	enter("i82092aa_set_mem_map");
	
	map = mem->map;
	if (map > 4) {
		leave("i82092aa_set_mem_map: invalid map");
		return -EINVAL;
	}
	
	/* Turn off the window before changing anything */
	if (indirect_read(sock, I365_ADDRWIN) & I365_ENA_MEM(map))
	              indirect_resetbit(sock, I365_ADDRWIN, I365_ENA_MEM(map));

	
	if (!(mem->flags & MAP_ACTIVE))
		return 0;

	if ( (mem->card_start > 0x3ffffff) || (mem->sys_start > mem->sys_stop) ||
	     ((mem->sys_start >> 24) != membase) || ((mem->sys_stop >> 24) != membase) ||
	     (mem->speed > 1000) ) {
		leave("i82092aa_set_mem_map: invalid address / speed");
		printk(KERN_WARNING "invalid mem map for socket %i : %lx to %lx with a start of %x \n",sock,mem->sys_start, mem->sys_stop, mem->card_start);
		return -EINVAL;
	}
	
	                 
	                 
/* 	printk("set_mem_map: Setting map %i range to %x - %x on socket %i, speed is %i, active = %i \n",map, mem->sys_start,mem->sys_stop,sock,mem->speed,mem->flags & MAP_ACTIVE);  */

	/* write the start address */
	base = I365_MEM(map);
	i = (mem->sys_start >> 12) & 0x0fff;
	if (mem->flags & MAP_16BIT) 
		i |= I365_MEM_16BIT;
	if (mem->flags & MAP_0WS)
		i |= I365_MEM_0WS;	
	indirect_write16(sock,base+I365_W_START,i);
		               
	/* write the stop address */
	
	i= (mem->sys_stop >> 12) & 0x0fff;
	switch (to_cycles(mem->speed)) {
		case 0:
			break;
		case 1:
			i |= I365_MEM_WS0;
			break;
		case 2:
			i |= I365_MEM_WS1;
			break;
		default:
			i |= I365_MEM_WS1 | I365_MEM_WS0;
			break;
	}
	
	indirect_write16(sock,base+I365_W_STOP,i);
	
	/* card start */
	
	i = (((mem->card_start - mem->sys_start) >> 12) - (membase << 12)) & 0x3fff;
	if (mem->flags & MAP_WRPROT)
		i |= I365_MEM_WRPROT;
	if (mem->flags & MAP_ATTRIB) {
/*		printk("requesting attribute memory for socket %i\n",sock);*/
		i |= I365_MEM_REG;
	} else {
/*		printk("requesting normal memory for socket %i\n",sock);*/
	}
	indirect_write16(sock,base+I365_W_OFF,i);
	indirect_write(sock, I365_CPAGE, membase);

	/* Enable the window if necessary */
	indirect_setbit(sock, I365_ADDRWIN, I365_ENA_MEM(map));
	            
	leave("i82092aa_set_mem_map");
	return 0;
}

static void i82092aa_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
	
}
/* Module stuff */

static int i82092aa_module_init(void)
{
	enter("i82092aa_module_init");
	pci_register_driver(&i82092aa_pci_drv);
	leave("i82092aa_module_init");
	return 0;
}

static void i82092aa_module_exit(void)
{
	enter("i82092aa_module_exit");
	pci_unregister_driver(&i82092aa_pci_drv);
	unregister_ss_entry(&i82092aa_operations);
	if (sockets[0].io_base>0)
			 release_region(sockets[0].io_base, 2);
	leave("i82092aa_module_exit");
}

module_init(i82092aa_module_init);
module_exit(i82092aa_module_exit);

