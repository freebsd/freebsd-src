/*
 * File Name:
 *   skfddi.c
 *
 * Copyright Information:
 *   Copyright SysKonnect 1998,1999.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The information in this file is provided "AS IS" without warranty.
 *
 * Abstract:
 *   A Linux device driver supporting the SysKonnect FDDI PCI controller
 *   familie.
 *
 * Maintainers:
 *   CG    Christoph Goos (cgoos@syskonnect.de)
 *
 * Contributors:
 *   DM    David S. Miller
 *
 * Address all question to:
 *   linux@syskonnect.de
 *
 * The technical manual for the adapters is available from SysKonnect's
 * web pages: www.syskonnect.com
 * Goto "Support" and search Knowledge Base for "manual".
 *
 * Driver Architecture:
 *   The driver architecture is based on the DEC FDDI driver by
 *   Lawrence V. Stefani and several ethernet drivers.
 *   I also used an existing Windows NT miniport driver.
 *   All hardware dependant fuctions are handled by the SysKonnect
 *   Hardware Module.
 *   The only headerfiles that are directly related to this source
 *   are skfddi.c, h/types.h, h/osdef1st.h, h/targetos.h.
 *   The others belong to the SysKonnect FDDI Hardware Module and
 *   should better not be changed.
 * NOTE:
 *   Compiling this driver produces some warnings, but I did not fix
 *   this, because the Hardware Module source is used for different
 *   drivers, and fixing it for Linux might bring problems on other
 *   projects. To keep the source common for all those drivers (and
 *   thus simplify fixes to it), please do not clean it up!
 *
 * Modification History:
 *              Date            Name    Description
 *              02-Mar-98       CG	Created.
 *
 *		10-Mar-99	CG	Support for 2.2.x added.
 *		25-Mar-99	CG	Corrected IRQ routing for SMP (APIC)
 *		26-Oct-99	CG	Fixed compilation error on 2.2.13
 *		12-Nov-99	CG	Source code release
 *		22-Nov-99	CG	Included in kernel source.
 *		07-May-00	DM	64 bit fixes, new dma interface
 *		06-May-02	ML	Structure fixes
 *
 * Compilation options (-Dxxx):
 *              DRIVERDEBUG     print lots of messages to log file
 *              DUMPPACKETS     print received/transmitted packets to logfile
 * 
 * Tested cpu architectures:
 *	- i386
 *	- sparc64
 */

/* Version information string - should be updated prior to */
/* each new release!!! */
#define VERSION		"2.07"

static const char *boot_msg = 
	"SysKonnect FDDI PCI Adapter driver v" VERSION " for\n"
	"  SK-55xx/SK-58xx adapters (SK-NET FDDI-FP/UP/LP)";

/* Include files */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>	// isdigit

#include <linux/netdevice.h>
#include <linux/fddidevice.h>
#include <linux/skbuff.h>

#include	"h/types.h"
#undef ADDR			// undo Linux definition
#include	"h/skfbi.h"
#include	"h/fddi.h"
#include	"h/smc.h"
#include	"h/smtstate.h"


// Define global routines
int skfp_probe(struct net_device *dev);


// Define module-wide (static) routines
static struct net_device *alloc_device(struct net_device *dev, u_long iobase);
static struct net_device *insert_device(struct net_device *dev,
				    int (*init) (struct net_device *));
static int fddi_dev_index(unsigned char *s);
static void init_dev(struct net_device *dev, u_long iobase);
static void link_modules(struct net_device *dev, struct net_device *tmp);
static int skfp_driver_init(struct net_device *dev);
static int skfp_open(struct net_device *dev);
static int skfp_close(struct net_device *dev);
static void skfp_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static struct net_device_stats *skfp_ctl_get_stats(struct net_device *dev);
static void skfp_ctl_set_multicast_list(struct net_device *dev);
static void skfp_ctl_set_multicast_list_wo_lock(struct net_device *dev);
static int skfp_ctl_set_mac_address(struct net_device *dev, void *addr);
static int skfp_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int skfp_send_pkt(struct sk_buff *skb, struct net_device *dev);
static void send_queued_packets(struct s_smc *smc);
static void CheckSourceAddress(unsigned char *frame, unsigned char *hw_addr);
static void ResetAdapter(struct s_smc *smc);


// Functions needed by the hardware module
void *mac_drv_get_space(struct s_smc *smc, u_int size);
void *mac_drv_get_desc_mem(struct s_smc *smc, u_int size);
unsigned long mac_drv_virt2phys(struct s_smc *smc, void *virt);
unsigned long dma_master(struct s_smc *smc, void *virt, int len, int flag);
void dma_complete(struct s_smc *smc, volatile union s_fp_descr *descr,
		  int flag);
void mac_drv_tx_complete(struct s_smc *smc, volatile struct s_smt_fp_txd *txd);
void llc_restart_tx(struct s_smc *smc);
void mac_drv_rx_complete(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count, int len);
void mac_drv_requeue_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count);
void mac_drv_fill_rxd(struct s_smc *smc);
void mac_drv_clear_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
		       int frag_count);
int mac_drv_rx_init(struct s_smc *smc, int len, int fc, char *look_ahead,
		    int la_len);
void smt_timer_poll(struct s_smc *smc);
void ring_status_indication(struct s_smc *smc, u_long status);
unsigned long smt_get_time(void);
void smt_stat_counter(struct s_smc *smc, int stat);
void cfm_state_change(struct s_smc *smc, int c_state);
void ecm_state_change(struct s_smc *smc, int e_state);
void pcm_state_change(struct s_smc *smc, int plc, int p_state);
void rmt_state_change(struct s_smc *smc, int r_state);
void drv_reset_indication(struct s_smc *smc);
void dump_data(unsigned char *Data, int length);


// External functions from the hardware module
extern u_int mac_drv_check_space();
extern void read_address(struct s_smc *smc, u_char * mac_addr);
extern void card_stop(struct s_smc *smc);
extern int mac_drv_init(struct s_smc *smc);
extern void hwm_tx_frag(struct s_smc *smc, char far * virt, u_long phys,
			int len, int frame_status);
extern int hwm_tx_init(struct s_smc *smc, u_char fc, int frag_count,
		       int frame_len, int frame_status);
extern int init_smt(struct s_smc *smc, u_char * mac_addr);
extern void fddi_isr(struct s_smc *smc);
extern void hwm_rx_frag(struct s_smc *smc, char far * virt, u_long phys,
			int len, int frame_status);
extern void mac_drv_rx_mode(struct s_smc *smc, int mode);
extern void mac_drv_clear_tx_queue(struct s_smc *smc);
extern void mac_drv_clear_rx_queue(struct s_smc *smc);
extern void mac_clear_multicast(struct s_smc *smc);
extern void enable_tx_irq(struct s_smc *smc, u_short queue);
extern void mac_drv_clear_txd(struct s_smc *smc);

static struct pci_device_id skfddi_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_SK, PCI_DEVICE_ID_SK_FP, PCI_ANY_ID, PCI_ANY_ID, },
	{ }			/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, skfddi_pci_tbl);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mirko Lindner <mlindner@syskonnect.de>");

// Define module-wide (static) variables

static int num_boards;	/* total number of adapters configured */
static int num_fddi;
static int autoprobed;

#ifdef MODULE
int init_module(void);
void cleanup_module(void);
static struct net_device *unlink_modules(struct net_device *p);
static int loading_module = 1;
#else
static int loading_module;
#endif				// MODULE

#ifdef DRIVERDEBUG
#define PRINTK(s, args...) printk(s, ## args)
#else
#define PRINTK(s, args...)
#endif				// DRIVERDEBUG

#define PRIV(dev) (&(((struct s_smc *)dev->priv)->os))

/*
 * ==============
 * = skfp_probe =
 * ==============
 *   
 * Overview:
 *   Probes for supported FDDI PCI controllers
 *  
 * Returns:
 *   Condition code
 *       
 * Arguments:
 *   dev - pointer to device information
 *
 * Functional Description:
 *   This routine is called by the OS for each FDDI device name (fddi0,
 *   fddi1,...,fddi6, fddi7) specified in drivers/net/Space.c.
 *   If loaded as a module, it will detect and initialize all 
 *   adapters the first time it is called.
 *
 *   Let's say that skfp_probe() is getting called to initialize fddi0.
 *   Furthermore, let's say there are three supported controllers in the
 *   system.  Before skfp_probe() leaves, devices fddi0, fddi1, and fddi2
 *   will be initialized and a global flag will be set to indicate that
 *   skfp_probe() has already been called.
 *
 *   However...the OS doesn't know that we've already initialized
 *   devices fddi1 and fddi2 so skfp_probe() gets called again and again
 *   until it reaches the end of the device list for FDDI (presently,
 *   fddi7).  It's important that the driver "pretend" to probe for
 *   devices fddi1 and fddi2 and return success.  Devices fddi3
 *   through fddi7 will return failure since they weren't initialized.
 *
 *   This algorithm seems to work for the time being.  As other FDDI
 *   drivers are written for Linux, a more generic approach (perhaps
 *   similar to the Ethernet card approach) may need to be implemented.
 *   
 * Return Codes:
 *   0           - This device (fddi0, fddi1, etc) configured successfully
 *   -ENODEV - No devices present, or no SysKonnect FDDI PCI device
 *                         present for this device name
 *
 *
 * Side Effects:
 *   Device structures for FDDI adapters (fddi0, fddi1, etc) are
 *   initialized and the board resources are read and stored in
 *   the device structure.
 */
int skfp_probe(struct net_device *dev)
{
	int i;			/* used in for loops */
	struct pci_dev *pdev = NULL;	/* PCI device structure */
#ifndef MEM_MAPPED_IO
	u16 port;		/* temporary I/O (port) address */
	int port_len;		/* length of port address range (in bytes) */
#else
	unsigned long port;
#endif
	u16 command;	/* PCI Configuration space Command register val */
	struct s_smc *smc;	/* board pointer */
	struct net_device *tmp = dev;
	u8 first_dev_used = 0;
	u16 SubSysId;

	PRINTK(KERN_INFO "entering skfp_probe\n");

	/*
	 * Verify whether we're going through skfp_probe() again
	 *
	 * If so, see if we're going through for a subsequent fddi device that
	 * we've already initialized.  If we are, return success (0).  If not,
	 * return failure (-ENODEV).
	 */

	if (autoprobed) {
		PRINTK(KERN_INFO "Already entered skfp_probe\n");
		if (dev != NULL) {
			if ((strncmp(dev->name, "fddi", 4) == 0) &&
			    (dev->base_addr != 0)) {
				return (0);
			}
			return (-ENODEV);
		}
	}
	autoprobed = 1;		/* set global flag */

	printk("%s\n", boot_msg);

	/* Scan for Syskonnect FDDI PCI controllers */
	if (!pci_present()) {	/* is PCI BIOS even present? */
		printk("no PCI BIOS present\n");
		return (-ENODEV);
	}
	for (i = 0; i < SKFP_MAX_NUM_BOARDS; i++) {	// scan for PCI cards
		PRINTK(KERN_INFO "Check device %d\n", i);
		if ((pdev=pci_find_device(PCI_VENDOR_ID_SK, PCI_DEVICE_ID_SK_FP,
			pdev)) == 0) {
			break;
		}
		if (pci_enable_device(pdev))
			continue;

#ifndef MEM_MAPPED_IO
		/* Verify that I/O enable bit is set (PCI slot is enabled) */
		pci_read_config_word(pdev, PCI_COMMAND, &command);
		if ((command & PCI_COMMAND_IO) == 0) {
			PRINTK("I/O enable bit not set!");
			PRINTK(" Verify that slot is enabled\n");
			continue;
		}

		/* Turn off memory mapped space and enable mastering */

		PRINTK(KERN_INFO "Command Reg: %04x\n", command);
		command |= PCI_COMMAND_MASTER;
		command &= ~PCI_COMMAND_MEMORY;
		pci_write_config_word(pdev, PCI_COMMAND, command);

		/* Read I/O base address from PCI Configuration Space */

		pci_read_config_word(pdev, PCI_BASE_ADDRESS_1, &port);
		port &= PCI_BASE_ADDRESS_IO_MASK; // clear I/O bit (bit 0)

		/* Verify port address range is not already being used */

		port_len = FP_IO_LEN;
		if (check_region(port, port_len) != 0) {
			printk("I/O range allocated to adapter");
			printk(" (0x%X-0x%X) is already being used!\n", port,
			       (port + port_len - 1));
			continue;
		}
#else
		/* Verify that MEM enable bit is set (PCI slot is enabled) */
		pci_read_config_word(pdev, PCI_COMMAND, &command);
		if ((command & PCI_COMMAND_MEMORY) == 0) {
			PRINTK("MEMORY-I/O enable bit not set!");
			PRINTK(" Verify that slot is enabled\n");
			continue;
		}

		/* Turn off IO mapped space and enable mastering */

		PRINTK(KERN_INFO "Command Reg: %04x\n", command);
		command |= PCI_COMMAND_MASTER;
		command &= ~PCI_COMMAND_IO;
		pci_write_config_word(pdev, PCI_COMMAND, command);

		port = pci_resource_start(pdev, 0);

		port = (unsigned long)ioremap(port, 0x4000);
		if (!port){
			printk("skfp:  Unable to map MEMORY register, "
			"FDDI adapter will be disabled.\n");
			break;
		}
#endif

		if ((!loading_module) || first_dev_used) {
			/* Allocate a device structure for this adapter */
			tmp = alloc_device(dev, port);
		}
		first_dev_used = 1;	// only significant first time

		pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &SubSysId);

		if (tmp != NULL) {
			if (loading_module)
				link_modules(dev, tmp);
			dev = tmp;
			init_dev(dev, port);
			dev->irq = pdev->irq;

			/* Initialize board structure with bus-specific info */

			smc = (struct s_smc *) dev->priv;
			smc->os.dev = dev;
			smc->os.bus_type = SK_BUS_TYPE_PCI;
			smc->os.pdev = *pdev;
			smc->os.QueueSkb = MAX_TX_QUEUE_LEN;
			smc->os.MaxFrameSize = MAX_FRAME_SIZE;
			smc->os.dev = dev;
			smc->hw.slot = -1;
			smc->os.ResetRequested = FALSE;
			skb_queue_head_init(&smc->os.SendSkbQueue);

			if (skfp_driver_init(dev) == 0) {
				// only increment global board 
				// count on success
				num_boards++;
				request_region(dev->base_addr,
					       FP_IO_LEN, dev->name);
				if ((SubSysId & 0xff00) == 0x5500 ||
					(SubSysId & 0xff00) == 0x5800) {
				printk("%s: SysKonnect FDDI PCI adapter"
				       " found (SK-%04X)\n", dev->name,
					SubSysId);
				} else {
				printk("%s: FDDI PCI adapter found\n",
					dev->name);
				}
			} else {
				kfree(dev);
				i = SKFP_MAX_NUM_BOARDS;	// stop search

			}

		}		// if (dev != NULL)

	}			// for SKFP_MAX_NUM_BOARDS

	/*
	 * If we're at this point we're going through skfp_probe() for the
	 * first time. Return success (0) if we've initialized 1 or more
	 * boards. Otherwise, return failure (-ENODEV).
	 */

	if (num_boards > 0)
		return (0);
	else {
		printk("no SysKonnect FDDI adapter found\n");
		return (-ENODEV);
	}
}				// skfp_probe


/************************
 *
 * Search the entire 'fddi' device list for a fixed probe. If a match isn't
 * found then check for an autoprobe or unused device location. If they
 * are not available then insert a new device structure at the end of
 * the current list.
 *
 ************************/
static struct net_device *alloc_device(struct net_device *dev, u_long iobase)
{
	struct net_device *adev = NULL;
	int fixed = 0, new_dev = 0;

	PRINTK(KERN_INFO "entering alloc_device\n");
	if (!dev)
		return dev;

	num_fddi = fddi_dev_index(dev->name);
	if (loading_module) {
		num_fddi++;
		dev = insert_device(dev, skfp_probe);
		return dev;
	}
	while (1) {
		if (((dev->base_addr == NO_ADDRESS) ||
		     (dev->base_addr == 0)) && !adev) {
			adev = dev;
		} else if ((dev->priv == NULL) && (dev->base_addr == iobase)) {
			fixed = 1;
		} else {
			if (dev->next == NULL) {
				new_dev = 1;
			} else if (strncmp(dev->next->name, "fddi", 4) != 0) {
				new_dev = 1;
			}
		}
		if ((dev->next == NULL) || new_dev || fixed)
			break;
		dev = dev->next;
		num_fddi++;
	}			// while (1)

	if (adev && !fixed) {
		dev = adev;
		num_fddi = fddi_dev_index(dev->name);
		new_dev = 0;
	}
	if (((dev->next == NULL) && ((dev->base_addr != NO_ADDRESS) &&
				     (dev->base_addr != 0)) && !fixed) ||
	    new_dev) {
		num_fddi++;	/* New device */
		dev = insert_device(dev, skfp_probe);
	}
	if (dev) {
		if (!dev->priv) {
			/* Allocate space for private board structure */
			dev->priv = (void *) kmalloc(sizeof(struct s_smc),
						     GFP_KERNEL);
			if (dev->priv == NULL) {
				printk("%s: Could not allocate memory for",
					dev->name);
				printk(" private board structure!\n");
				return (NULL);
			}
			/* clear structure */
			memset(dev->priv, 0, sizeof(struct s_smc));
		}
	}
	return dev;
}				// alloc_device



/************************
 *
 * Initialize device structure
 *
 ************************/
static void init_dev(struct net_device *dev, u_long iobase)
{
	/* Initialize new device structure */

	dev->rmem_end = 0;	/* shared memory isn't used */
	dev->rmem_start = 0;	/* shared memory isn't used */
	dev->mem_end = 0;	/* shared memory isn't used */
	dev->mem_start = 0;	/* shared memory isn't used */
	dev->base_addr = iobase;	/* save port (I/O) base address */
	dev->if_port = 0;	/* not applicable to FDDI adapters */
	dev->dma = 0;		/* Bus Master DMA doesn't require channel */
	dev->irq = 0;

	netif_start_queue(dev);

	dev->get_stats = &skfp_ctl_get_stats;
	dev->open = &skfp_open;
	dev->stop = &skfp_close;
	dev->hard_start_xmit = &skfp_send_pkt;
	dev->hard_header = NULL;	/* set in fddi_setup() */
	dev->rebuild_header = NULL;	/* set in fddi_setup() */
	dev->set_multicast_list = &skfp_ctl_set_multicast_list;
	dev->set_mac_address = &skfp_ctl_set_mac_address;
	dev->do_ioctl = &skfp_ioctl;
	dev->set_config = NULL;	/* not supported for now &&& */
	dev->header_cache_update = NULL;	/* not supported */
	dev->change_mtu = NULL;	/* set in fddi_setup() */

	/* Initialize remaining device structure information */
	fddi_setup(dev);
}				// init_device


/************************
 *
 * If at end of fddi device list and can't use current entry, malloc
 * one up. If memory could not be allocated, print an error message.
 *
************************/
static struct net_device *insert_device(struct net_device *dev,
				    int (*init) (struct net_device *))
{
	struct net_device *new;
	int len;

	PRINTK(KERN_INFO "entering insert_device\n");
	len = sizeof(struct net_device) + sizeof(struct s_smc);
	new = (struct net_device *) kmalloc(len, GFP_KERNEL);
	if (new == NULL) {
		printk("fddi%d: Device not initialised, insufficient memory\n",
		       num_fddi);
		return NULL;
	} else {
		memset((char *) new, 0, len);
		new->priv = (struct s_smc *) (new + 1);
		new->init = init;	/* initialisation routine */
		if (!loading_module) {
			new->next = dev->next;
			dev->next = new;
		}
		/* create new device name */
		if (num_fddi > 999) {
			sprintf(new->name, "fddi????");
		} else {
			sprintf(new->name, "fddi%d", num_fddi);
		}
	}
	return new;
}				// insert_device


/************************
 *
 * Get the number of a "fddiX" string
 *
 ************************/
static int fddi_dev_index(unsigned char *s)
{
	int i = 0, j = 0;

	for (; *s; s++) {
		if (isdigit(*s)) {
			j = 1;
			i = (i * 10) + (*s - '0');
		} else if (j)
			break;
	}
	return i;
}				// fddi_dev_index


/************************
 *
 * Used if loaded as module only. Link the device structures
 * together. Needed to release them all at unload.
 *
************************/
static void link_modules(struct net_device *dev, struct net_device *tmp)
{
	struct net_device *p = dev;

	if (p) {
		while (((struct s_smc *) (p->priv))->os.next_module) {
			p = ((struct s_smc *) (p->priv))->os.next_module;
		}

		if (dev != tmp) {
			((struct s_smc *) (p->priv))->os.next_module = tmp;
		} else {
			((struct s_smc *) (p->priv))->os.next_module = NULL;
		}
	}
	return;
}				// link_modules



/*
 * ====================
 * = skfp_driver_init =
 * ====================
 *   
 * Overview:
 *   Initializes remaining adapter board structure information
 *   and makes sure adapter is in a safe state prior to skfp_open().
 *  
 * Returns:
 *   Condition code
 *       
 * Arguments:
 *   dev - pointer to device information
 *
 * Functional Description:
 *   This function allocates additional resources such as the host memory
 *   blocks needed by the adapter.
 *   The adapter is also reset. The OS must call skfp_open() to open 
 *   the adapter and bring it on-line.
 *
 * Return Codes:
 *    0 - initialization succeeded
 *   -1 - initialization failed
 */
static int skfp_driver_init(struct net_device *dev)
{
	struct s_smc *smc = (struct s_smc *) dev->priv;
	skfddi_priv *bp = PRIV(dev);
	u8 val;			/* used for I/O read/writes */

	PRINTK(KERN_INFO "entering skfp_driver_init\n");

	// set the io address in private structures
	bp->base_addr = dev->base_addr;
	smc->hw.iop = dev->base_addr;

	// Get the interrupt level from the PCI Configuration Table
	val = dev->irq;

	smc->hw.irq = val;

	spin_lock_init(&bp->DriverLock);
	
	// Allocate invalid frame
	bp->LocalRxBuffer = pci_alloc_consistent(&bp->pdev, MAX_FRAME_SIZE, &bp->LocalRxBufferDMA);
	if (!bp->LocalRxBuffer) {
		printk("could not allocate mem for ");
		printk("LocalRxBuffer: %d byte\n", MAX_FRAME_SIZE);
		goto fail;
	}

	// Determine the required size of the 'shared' memory area.
	bp->SharedMemSize = mac_drv_check_space();
	PRINTK(KERN_INFO "Memory for HWM: %ld\n", bp->SharedMemSize);
	if (bp->SharedMemSize > 0) {
		bp->SharedMemSize += 16;	// for descriptor alignment

		bp->SharedMemAddr = pci_alloc_consistent(&bp->pdev,
							 bp->SharedMemSize,
							 &bp->SharedMemDMA);
		if (!bp->SharedMemSize) {
			printk("could not allocate mem for ");
			printk("hardware module: %ld byte\n",
			       bp->SharedMemSize);
			goto fail;
		}
		bp->SharedMemHeap = 0;	// Nothing used yet.

	} else {
		bp->SharedMemAddr = NULL;
		bp->SharedMemHeap = 0;
	}			// SharedMemSize > 0

	memset(bp->SharedMemAddr, 0, bp->SharedMemSize);

	card_stop(smc);		// Reset adapter.

	PRINTK(KERN_INFO "mac_drv_init()..\n");
	if (mac_drv_init(smc) != 0) {
		PRINTK(KERN_INFO "mac_drv_init() failed.\n");
		goto fail;
	}
	read_address(smc, NULL);
	PRINTK(KERN_INFO "HW-Addr: %02x %02x %02x %02x %02x %02x\n",
	       smc->hw.fddi_canon_addr.a[0],
	       smc->hw.fddi_canon_addr.a[1],
	       smc->hw.fddi_canon_addr.a[2],
	       smc->hw.fddi_canon_addr.a[3],
	       smc->hw.fddi_canon_addr.a[4],
	       smc->hw.fddi_canon_addr.a[5]);
	memcpy(dev->dev_addr, smc->hw.fddi_canon_addr.a, 6);

	smt_reset_defaults(smc, 0);

	return (0);

fail:
	if (bp->SharedMemAddr) {
		pci_free_consistent(&bp->pdev,
				    bp->SharedMemSize,
				    bp->SharedMemAddr,
				    bp->SharedMemDMA);
		bp->SharedMemAddr = NULL;
	}
	if (bp->LocalRxBuffer) {
		pci_free_consistent(&bp->pdev, MAX_FRAME_SIZE,
				    bp->LocalRxBuffer, bp->LocalRxBufferDMA);
		bp->LocalRxBuffer = NULL;
	}
	return (-1);
}				// skfp_driver_init


/*
 * =============
 * = skfp_open =
 * =============
 *   
 * Overview:
 *   Opens the adapter
 *  
 * Returns:
 *   Condition code
 *       
 * Arguments:
 *   dev - pointer to device information
 *
 * Functional Description:
 *   This function brings the adapter to an operational state.
 *
 * Return Codes:
 *   0           - Adapter was successfully opened
 *   -EAGAIN - Could not register IRQ
 */
static int skfp_open(struct net_device *dev)
{
	struct s_smc *smc = (struct s_smc *) dev->priv;

	PRINTK(KERN_INFO "entering skfp_open\n");
	/* Register IRQ - support shared interrupts by passing device ptr */
	if (request_irq(dev->irq, (void *) skfp_interrupt, SA_SHIRQ,
			dev->name, dev)) {
		printk("%s: Requested IRQ %d is busy\n", dev->name, dev->irq);
		return (-EAGAIN);
	}
	/*
	 * Set current address to factory MAC address
	 *
	 * Note: We've already done this step in skfp_driver_init.
	 *       However, it's possible that a user has set a node
	 *               address override, then closed and reopened the
	 *               adapter.  Unless we reset the device address field
	 *               now, we'll continue to use the existing modified
	 *               address.
	 */
	read_address(smc, NULL);
	memcpy(dev->dev_addr, smc->hw.fddi_canon_addr.a, 6);

	init_smt(smc, NULL);
	smt_online(smc, 1);
	STI_FBI();

	MOD_INC_USE_COUNT;

	/* Clear local multicast address tables */
	mac_clear_multicast(smc);

	/* Disable promiscuous filter settings */
	mac_drv_rx_mode(smc, RX_DISABLE_PROMISC);

	return (0);
}				// skfp_open


/*
 * ==============
 * = skfp_close =
 * ==============
 *   
 * Overview:
 *   Closes the device/module.
 *  
 * Returns:
 *   Condition code
 *       
 * Arguments:
 *   dev - pointer to device information
 *
 * Functional Description:
 *   This routine closes the adapter and brings it to a safe state.
 *   The interrupt service routine is deregistered with the OS.
 *   The adapter can be opened again with another call to skfp_open().
 *
 * Return Codes:
 *   Always return 0.
 *
 * Assumptions:
 *   No further requests for this adapter are made after this routine is
 *   called.  skfp_open() can be called to reset and reinitialize the
 *   adapter.
 */
static int skfp_close(struct net_device *dev)
{
	struct s_smc *smc = (struct s_smc *) dev->priv;
	struct sk_buff *skb;
	skfddi_priv *bp = PRIV(dev);

	CLI_FBI();
	smt_reset_defaults(smc, 1);
	card_stop(smc);
	mac_drv_clear_tx_queue(smc);
	mac_drv_clear_rx_queue(smc);

	netif_stop_queue(dev);
	/* Deregister (free) IRQ */
	free_irq(dev->irq, dev);

	for (;;) {
		skb = skb_dequeue(&bp->SendSkbQueue);
		if (skb == NULL)
			break;
		bp->QueueSkb++;
		dev_kfree_skb(skb);
	}

	MOD_DEC_USE_COUNT;

	return (0);
}				// skfp_close


/*
 * ==================
 * = skfp_interrupt =
 * ==================
 *   
 * Overview:
 *   Interrupt processing routine
 *  
 * Returns:
 *   None
 *       
 * Arguments:
 *   irq        - interrupt vector
 *   dev_id     - pointer to device information
 *       regs   - pointer to registers structure
 *
 * Functional Description:
 *   This routine calls the interrupt processing routine for this adapter.  It
 *   disables and reenables adapter interrupts, as appropriate.  We can support
 *   shared interrupts since the incoming dev_id pointer provides our device
 *   structure context. All the real work is done in the hardware module.
 *
 * Return Codes:
 *   None
 *
 * Assumptions:
 *   The interrupt acknowledgement at the hardware level (eg. ACKing the PIC
 *   on Intel-based systems) is done by the operating system outside this
 *   routine.
 *
 *       System interrupts are enabled through this call.
 *
 * Side Effects:
 *   Interrupts are disabled, then reenabled at the adapter.
 */

void skfp_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct s_smc *smc;	/* private board structure pointer */
	skfddi_priv *bp = PRIV(dev);


	if (dev == NULL) {
		printk("%s: irq %d for unknown device\n", dev->name, irq);
		return;
	}

	smc = (struct s_smc *) dev->priv;

	// IRQs enabled or disabled ?
	if (inpd(ADDR(B0_IMSK)) == 0) {
		// IRQs are disabled: must be shared interrupt
		return;
	}
	// Note: At this point, IRQs are enabled.
	if ((inpd(ISR_A) & smc->hw.is_imask) == 0) {	// IRQ?
		// Adapter did not issue an IRQ: must be shared interrupt
		return;
	}
	CLI_FBI();		// Disable IRQs from our adapter.
	spin_lock(&bp->DriverLock);

	// Call interrupt handler in hardware module (HWM).
	fddi_isr(smc);

	if (smc->os.ResetRequested) {
		ResetAdapter(smc);
		smc->os.ResetRequested = FALSE;
	}
	spin_unlock(&bp->DriverLock);
	STI_FBI();		// Enable IRQs from our adapter.

	return;
}				// skfp_interrupt


/*
 * ======================
 * = skfp_ctl_get_stats =
 * ======================
 *   
 * Overview:
 *   Get statistics for FDDI adapter
 *  
 * Returns:
 *   Pointer to FDDI statistics structure
 *       
 * Arguments:
 *   dev - pointer to device information
 *
 * Functional Description:
 *   Gets current MIB objects from adapter, then
 *   returns FDDI statistics structure as defined
 *   in if_fddi.h.
 *
 *   Note: Since the FDDI statistics structure is
 *   still new and the device structure doesn't
 *   have an FDDI-specific get statistics handler,
 *   we'll return the FDDI statistics structure as
 *   a pointer to an Ethernet statistics structure.
 *   That way, at least the first part of the statistics
 *   structure can be decoded properly.
 *   We'll have to pay attention to this routine as the
 *   device structure becomes more mature and LAN media
 *   independent.
 *
 */
struct net_device_stats *skfp_ctl_get_stats(struct net_device *dev)
{
	struct s_smc *bp = (struct s_smc *) dev->priv;

	/* Fill the bp->stats structure with driver-maintained counters */

	bp->os.MacStat.port_bs_flag[0] = 0x1234;
	bp->os.MacStat.port_bs_flag[1] = 0x5678;
// goos: need to fill out fddi statistic
#if 0
	/* Get FDDI SMT MIB objects */

/* Fill the bp->stats structure with the SMT MIB object values */

	memcpy(bp->stats.smt_station_id, &bp->cmd_rsp_virt->smt_mib_get.smt_station_id, sizeof(bp->cmd_rsp_virt->smt_mib_get.smt_station_id));
	bp->stats.smt_op_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_op_version_id;
	bp->stats.smt_hi_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_hi_version_id;
	bp->stats.smt_lo_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_lo_version_id;
	memcpy(bp->stats.smt_user_data, &bp->cmd_rsp_virt->smt_mib_get.smt_user_data, sizeof(bp->cmd_rsp_virt->smt_mib_get.smt_user_data));
	bp->stats.smt_mib_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_mib_version_id;
	bp->stats.smt_mac_cts = bp->cmd_rsp_virt->smt_mib_get.smt_mac_ct;
	bp->stats.smt_non_master_cts = bp->cmd_rsp_virt->smt_mib_get.smt_non_master_ct;
	bp->stats.smt_master_cts = bp->cmd_rsp_virt->smt_mib_get.smt_master_ct;
	bp->stats.smt_available_paths = bp->cmd_rsp_virt->smt_mib_get.smt_available_paths;
	bp->stats.smt_config_capabilities = bp->cmd_rsp_virt->smt_mib_get.smt_config_capabilities;
	bp->stats.smt_config_policy = bp->cmd_rsp_virt->smt_mib_get.smt_config_policy;
	bp->stats.smt_connection_policy = bp->cmd_rsp_virt->smt_mib_get.smt_connection_policy;
	bp->stats.smt_t_notify = bp->cmd_rsp_virt->smt_mib_get.smt_t_notify;
	bp->stats.smt_stat_rpt_policy = bp->cmd_rsp_virt->smt_mib_get.smt_stat_rpt_policy;
	bp->stats.smt_trace_max_expiration = bp->cmd_rsp_virt->smt_mib_get.smt_trace_max_expiration;
	bp->stats.smt_bypass_present = bp->cmd_rsp_virt->smt_mib_get.smt_bypass_present;
	bp->stats.smt_ecm_state = bp->cmd_rsp_virt->smt_mib_get.smt_ecm_state;
	bp->stats.smt_cf_state = bp->cmd_rsp_virt->smt_mib_get.smt_cf_state;
	bp->stats.smt_remote_disconnect_flag = bp->cmd_rsp_virt->smt_mib_get.smt_remote_disconnect_flag;
	bp->stats.smt_station_status = bp->cmd_rsp_virt->smt_mib_get.smt_station_status;
	bp->stats.smt_peer_wrap_flag = bp->cmd_rsp_virt->smt_mib_get.smt_peer_wrap_flag;
	bp->stats.smt_time_stamp = bp->cmd_rsp_virt->smt_mib_get.smt_msg_time_stamp.ls;
	bp->stats.smt_transition_time_stamp = bp->cmd_rsp_virt->smt_mib_get.smt_transition_time_stamp.ls;
	bp->stats.mac_frame_status_functions = bp->cmd_rsp_virt->smt_mib_get.mac_frame_status_functions;
	bp->stats.mac_t_max_capability = bp->cmd_rsp_virt->smt_mib_get.mac_t_max_capability;
	bp->stats.mac_tvx_capability = bp->cmd_rsp_virt->smt_mib_get.mac_tvx_capability;
	bp->stats.mac_available_paths = bp->cmd_rsp_virt->smt_mib_get.mac_available_paths;
	bp->stats.mac_current_path = bp->cmd_rsp_virt->smt_mib_get.mac_current_path;
	memcpy(bp->stats.mac_upstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_upstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_downstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_downstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_old_upstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_old_upstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_old_downstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_old_downstream_nbr, FDDI_K_ALEN);
	bp->stats.mac_dup_address_test = bp->cmd_rsp_virt->smt_mib_get.mac_dup_address_test;
	bp->stats.mac_requested_paths = bp->cmd_rsp_virt->smt_mib_get.mac_requested_paths;
	bp->stats.mac_downstream_port_type = bp->cmd_rsp_virt->smt_mib_get.mac_downstream_port_type;
	memcpy(bp->stats.mac_smt_address, &bp->cmd_rsp_virt->smt_mib_get.mac_smt_address, FDDI_K_ALEN);
	bp->stats.mac_t_req = bp->cmd_rsp_virt->smt_mib_get.mac_t_req;
	bp->stats.mac_t_neg = bp->cmd_rsp_virt->smt_mib_get.mac_t_neg;
	bp->stats.mac_t_max = bp->cmd_rsp_virt->smt_mib_get.mac_t_max;
	bp->stats.mac_tvx_value = bp->cmd_rsp_virt->smt_mib_get.mac_tvx_value;
	bp->stats.mac_frame_error_threshold = bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_threshold;
	bp->stats.mac_frame_error_ratio = bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_ratio;
	bp->stats.mac_rmt_state = bp->cmd_rsp_virt->smt_mib_get.mac_rmt_state;
	bp->stats.mac_da_flag = bp->cmd_rsp_virt->smt_mib_get.mac_da_flag;
	bp->stats.mac_una_da_flag = bp->cmd_rsp_virt->smt_mib_get.mac_unda_flag;
	bp->stats.mac_frame_error_flag = bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_flag;
	bp->stats.mac_ma_unitdata_available = bp->cmd_rsp_virt->smt_mib_get.mac_ma_unitdata_available;
	bp->stats.mac_hardware_present = bp->cmd_rsp_virt->smt_mib_get.mac_hardware_present;
	bp->stats.mac_ma_unitdata_enable = bp->cmd_rsp_virt->smt_mib_get.mac_ma_unitdata_enable;
	bp->stats.path_tvx_lower_bound = bp->cmd_rsp_virt->smt_mib_get.path_tvx_lower_bound;
	bp->stats.path_t_max_lower_bound = bp->cmd_rsp_virt->smt_mib_get.path_t_max_lower_bound;
	bp->stats.path_max_t_req = bp->cmd_rsp_virt->smt_mib_get.path_max_t_req;
	memcpy(bp->stats.path_configuration, &bp->cmd_rsp_virt->smt_mib_get.path_configuration, sizeof(bp->cmd_rsp_virt->smt_mib_get.path_configuration));
	bp->stats.port_my_type[0] = bp->cmd_rsp_virt->smt_mib_get.port_my_type[0];
	bp->stats.port_my_type[1] = bp->cmd_rsp_virt->smt_mib_get.port_my_type[1];
	bp->stats.port_neighbor_type[0] = bp->cmd_rsp_virt->smt_mib_get.port_neighbor_type[0];
	bp->stats.port_neighbor_type[1] = bp->cmd_rsp_virt->smt_mib_get.port_neighbor_type[1];
	bp->stats.port_connection_policies[0] = bp->cmd_rsp_virt->smt_mib_get.port_connection_policies[0];
	bp->stats.port_connection_policies[1] = bp->cmd_rsp_virt->smt_mib_get.port_connection_policies[1];
	bp->stats.port_mac_indicated[0] = bp->cmd_rsp_virt->smt_mib_get.port_mac_indicated[0];
	bp->stats.port_mac_indicated[1] = bp->cmd_rsp_virt->smt_mib_get.port_mac_indicated[1];
	bp->stats.port_current_path[0] = bp->cmd_rsp_virt->smt_mib_get.port_current_path[0];
	bp->stats.port_current_path[1] = bp->cmd_rsp_virt->smt_mib_get.port_current_path[1];
	memcpy(&bp->stats.port_requested_paths[0 * 3], &bp->cmd_rsp_virt->smt_mib_get.port_requested_paths[0], 3);
	memcpy(&bp->stats.port_requested_paths[1 * 3], &bp->cmd_rsp_virt->smt_mib_get.port_requested_paths[1], 3);
	bp->stats.port_mac_placement[0] = bp->cmd_rsp_virt->smt_mib_get.port_mac_placement[0];
	bp->stats.port_mac_placement[1] = bp->cmd_rsp_virt->smt_mib_get.port_mac_placement[1];
	bp->stats.port_available_paths[0] = bp->cmd_rsp_virt->smt_mib_get.port_available_paths[0];
	bp->stats.port_available_paths[1] = bp->cmd_rsp_virt->smt_mib_get.port_available_paths[1];
	bp->stats.port_pmd_class[0] = bp->cmd_rsp_virt->smt_mib_get.port_pmd_class[0];
	bp->stats.port_pmd_class[1] = bp->cmd_rsp_virt->smt_mib_get.port_pmd_class[1];
	bp->stats.port_connection_capabilities[0] = bp->cmd_rsp_virt->smt_mib_get.port_connection_capabilities[0];
	bp->stats.port_connection_capabilities[1] = bp->cmd_rsp_virt->smt_mib_get.port_connection_capabilities[1];
	bp->stats.port_bs_flag[0] = bp->cmd_rsp_virt->smt_mib_get.port_bs_flag[0];
	bp->stats.port_bs_flag[1] = bp->cmd_rsp_virt->smt_mib_get.port_bs_flag[1];
	bp->stats.port_ler_estimate[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_estimate[0];
	bp->stats.port_ler_estimate[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_estimate[1];
	bp->stats.port_ler_cutoff[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_cutoff[0];
	bp->stats.port_ler_cutoff[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_cutoff[1];
	bp->stats.port_ler_alarm[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_alarm[0];
	bp->stats.port_ler_alarm[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_alarm[1];
	bp->stats.port_connect_state[0] = bp->cmd_rsp_virt->smt_mib_get.port_connect_state[0];
	bp->stats.port_connect_state[1] = bp->cmd_rsp_virt->smt_mib_get.port_connect_state[1];
	bp->stats.port_pcm_state[0] = bp->cmd_rsp_virt->smt_mib_get.port_pcm_state[0];
	bp->stats.port_pcm_state[1] = bp->cmd_rsp_virt->smt_mib_get.port_pcm_state[1];
	bp->stats.port_pc_withhold[0] = bp->cmd_rsp_virt->smt_mib_get.port_pc_withhold[0];
	bp->stats.port_pc_withhold[1] = bp->cmd_rsp_virt->smt_mib_get.port_pc_withhold[1];
	bp->stats.port_ler_flag[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_flag[0];
	bp->stats.port_ler_flag[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_flag[1];
	bp->stats.port_hardware_present[0] = bp->cmd_rsp_virt->smt_mib_get.port_hardware_present[0];
	bp->stats.port_hardware_present[1] = bp->cmd_rsp_virt->smt_mib_get.port_hardware_present[1];


	/* Fill the bp->stats structure with the FDDI counter values */

	bp->stats.mac_frame_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.frame_cnt.ls;
	bp->stats.mac_copied_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.copied_cnt.ls;
	bp->stats.mac_transmit_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.transmit_cnt.ls;
	bp->stats.mac_error_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.error_cnt.ls;
	bp->stats.mac_lost_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.lost_cnt.ls;
	bp->stats.port_lct_fail_cts[0] = bp->cmd_rsp_virt->cntrs_get.cntrs.lct_rejects[0].ls;
	bp->stats.port_lct_fail_cts[1] = bp->cmd_rsp_virt->cntrs_get.cntrs.lct_rejects[1].ls;
	bp->stats.port_lem_reject_cts[0] = bp->cmd_rsp_virt->cntrs_get.cntrs.lem_rejects[0].ls;
	bp->stats.port_lem_reject_cts[1] = bp->cmd_rsp_virt->cntrs_get.cntrs.lem_rejects[1].ls;
	bp->stats.port_lem_cts[0] = bp->cmd_rsp_virt->cntrs_get.cntrs.link_errors[0].ls;
	bp->stats.port_lem_cts[1] = bp->cmd_rsp_virt->cntrs_get.cntrs.link_errors[1].ls;

#endif
	return ((struct net_device_stats *) &bp->os.MacStat);
}				// ctl_get_stat


/*
 * ==============================
 * = skfp_ctl_set_multicast_list =
 * ==============================
 *   
 * Overview:
 *   Enable/Disable LLC frame promiscuous mode reception
 *   on the adapter and/or update multicast address table.
 *  
 * Returns:
 *   None
 *       
 * Arguments:
 *   dev - pointer to device information
 *
 * Functional Description:
 *   This function acquires the driver lock and only calls
 *   skfp_ctl_set_multicast_list_wo_lock then.
 *   This routine follows a fairly simple algorithm for setting the
 *   adapter filters and CAM:
 *
 *      if IFF_PROMISC flag is set
 *              enable promiscuous mode
 *      else
 *              disable promiscuous mode
 *              if number of multicast addresses <= max. multicast number
 *                      add mc addresses to adapter table
 *              else
 *                      enable promiscuous mode
 *              update adapter filters
 *
 * Assumptions:
 *   Multicast addresses are presented in canonical (LSB) format.
 *
 * Side Effects:
 *   On-board adapter filters are updated.
 */
static void skfp_ctl_set_multicast_list(struct net_device *dev)
{
	skfddi_priv *bp = PRIV(dev);
	unsigned long Flags;

	spin_lock_irqsave(&bp->DriverLock, Flags);
	skfp_ctl_set_multicast_list_wo_lock(dev);
	spin_unlock_irqrestore(&bp->DriverLock, Flags);
	return;
}				// skfp_ctl_set_multicast_list



static void skfp_ctl_set_multicast_list_wo_lock(struct net_device *dev)
{
	struct s_smc *smc = (struct s_smc *) dev->priv;
	struct dev_mc_list *dmi;	/* ptr to multicast addr entry */
	int i;

	/* Enable promiscuous mode, if necessary */
	if (dev->flags & IFF_PROMISC) {
		mac_drv_rx_mode(smc, RX_ENABLE_PROMISC);
		PRINTK(KERN_INFO "PROMISCUOUS MODE ENABLED\n");
	}
	/* Else, update multicast address table */
	else {
		mac_drv_rx_mode(smc, RX_DISABLE_PROMISC);
		PRINTK(KERN_INFO "PROMISCUOUS MODE DISABLED\n");

		// Reset all MC addresses
		mac_clear_multicast(smc);
		mac_drv_rx_mode(smc, RX_DISABLE_ALLMULTI);

		if (dev->flags & IFF_ALLMULTI) {
			mac_drv_rx_mode(smc, RX_ENABLE_ALLMULTI);
			PRINTK(KERN_INFO "ENABLE ALL MC ADDRESSES\n");
		} else if (dev->mc_count > 0) {
			if (dev->mc_count <= FPMAX_MULTICAST) {
				/* use exact filtering */

				// point to first multicast addr
				dmi = dev->mc_list;

				for (i = 0; i < dev->mc_count; i++) {
					mac_add_multicast(smc,
							  dmi->dmi_addr, 1);
					PRINTK(KERN_INFO "ENABLE MC ADDRESS:");
					PRINTK(" %02x %02x %02x ",
					       dmi->dmi_addr[0],
					       dmi->dmi_addr[1],
					       dmi->dmi_addr[2]);
					PRINTK("%02x %02x %02x\n",
					       dmi->dmi_addr[3],
					       dmi->dmi_addr[4],
					       dmi->dmi_addr[5]);
					dmi = dmi->next;
				}	// for

			} else {	// more MC addresses than HW supports

				mac_drv_rx_mode(smc, RX_ENABLE_ALLMULTI);
				PRINTK(KERN_INFO "ENABLE ALL MC ADDRESSES\n");
			}
		} else {	// no MC addresses

			PRINTK(KERN_INFO "DISABLE ALL MC ADDRESSES\n");
		}

		/* Update adapter filters */
		mac_update_multicast(smc);
	}
	return;
}				// skfp_ctl_set_multicast_list_wo_lock


/*
 * ===========================
 * = skfp_ctl_set_mac_address =
 * ===========================
 *   
 * Overview:
 *   set new mac address on adapter and update dev_addr field in device table.
 *  
 * Returns:
 *   None
 *       
 * Arguments:
 *   dev  - pointer to device information
 *   addr - pointer to sockaddr structure containing unicast address to set
 *
 * Assumptions:
 *   The address pointed to by addr->sa_data is a valid unicast
 *   address and is presented in canonical (LSB) format.
 */
static int skfp_ctl_set_mac_address(struct net_device *dev, void *addr)
{
	struct s_smc *smc = (struct s_smc *) dev->priv;
	struct sockaddr *p_sockaddr = (struct sockaddr *) addr;
	skfddi_priv *bp = (skfddi_priv *) & smc->os;
	unsigned long Flags;


	memcpy(dev->dev_addr, p_sockaddr->sa_data, FDDI_K_ALEN);
	spin_lock_irqsave(&bp->DriverLock, Flags);
	ResetAdapter(smc);
	spin_unlock_irqrestore(&bp->DriverLock, Flags);

	return (0);		/* always return zero */
}				// skfp_ctl_set_mac_address


/*
 * ==============
 * = skfp_ioctl =
 * ==============
 *   
 * Overview:
 *
 * Perform IOCTL call functions here. Some are privileged operations and the
 * effective uid is checked in those cases.
 *  
 * Returns:
 *   status value
 *   0 - success
 *   other - failure
 *       
 * Arguments:
 *   dev  - pointer to device information
 *   rq - pointer to ioctl request structure
 *   cmd - ?
 *
 */


static int skfp_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	skfddi_priv *lp = PRIV(dev);
	struct s_skfp_ioctl ioc;
	int status = 0;

	copy_from_user(&ioc, rq->ifr_data, sizeof(struct s_skfp_ioctl));
	switch (ioc.cmd) {
	case SKFP_GET_STATS:	/* Get the driver statistics */
		ioc.len = sizeof(lp->MacStat);
		copy_to_user(ioc.data, skfp_ctl_get_stats(dev), ioc.len);
		break;
	case SKFP_CLR_STATS:	/* Zero out the driver statistics */
		if (!capable(CAP_NET_ADMIN)) {
			memset(&lp->MacStat, 0, sizeof(lp->MacStat));
		} else {
			status = -EPERM;
		}
		break;
	default:
		printk("ioctl for %s: unknow cmd: %04x\n", dev->name, ioc.cmd);
	}			// switch

	return status;
}				// skfp_ioctl


/*
 * =====================
 * = skfp_send_pkt     =
 * =====================
 *   
 * Overview:
 *   Queues a packet for transmission and try to transmit it.
 *  
 * Returns:
 *   Condition code
 *       
 * Arguments:
 *   skb - pointer to sk_buff to queue for transmission
 *   dev - pointer to device information
 *
 * Functional Description:
 *   Here we assume that an incoming skb transmit request
 *   is contained in a single physically contiguous buffer
 *   in which the virtual address of the start of packet
 *   (skb->data) can be converted to a physical address
 *   by using pci_map_single().
 *
 *   We have an internal queue for packets we can not send 
 *   immediately. Packets in this queue can be given to the 
 *   adapter if transmit buffers are freed.
 *
 *   We can't free the skb until after it's been DMA'd
 *   out by the adapter, so we'll keep it in the driver and
 *   return it in mac_drv_tx_complete.
 *
 * Return Codes:
 *   0 - driver has queued and/or sent packet
 *       1 - caller should requeue the sk_buff for later transmission
 *
 * Assumptions:
 *   The entire packet is stored in one physically
 *   contiguous buffer which is not cached and whose
 *   32-bit physical address can be determined.
 *
 *   It's vital that this routine is NOT reentered for the
 *   same board and that the OS is not in another section of
 *   code (eg. skfp_interrupt) for the same board on a
 *   different thread.
 *
 * Side Effects:
 *   None
 */
static int skfp_send_pkt(struct sk_buff *skb, struct net_device *dev)
{
	skfddi_priv *bp = PRIV(dev);

	PRINTK(KERN_INFO "skfp_send_pkt\n");

	/*
	 * Verify that incoming transmit request is OK
	 *
	 * Note: The packet size check is consistent with other
	 *               Linux device drivers, although the correct packet
	 *               size should be verified before calling the
	 *               transmit routine.
	 */

	if (!(skb->len >= FDDI_K_LLC_ZLEN && skb->len <= FDDI_K_LLC_LEN)) {
		bp->MacStat.tx_errors++;	/* bump error counter */
		// dequeue packets from xmt queue and send them
		netif_start_queue(dev);
		dev_kfree_skb(skb);
		return (0);	/* return "success" */
	}
	if (bp->QueueSkb == 0) {	// return with tbusy set: queue full

		netif_stop_queue(dev);
		return 1;
	}
	bp->QueueSkb--;
	skb_queue_tail(&bp->SendSkbQueue, skb);
	send_queued_packets((struct s_smc *) dev->priv);
	if (bp->QueueSkb == 0) {
		netif_stop_queue(dev);
	}
	dev->trans_start = jiffies;
	return 0;

}				// skfp_send_pkt


/*
 * =======================
 * = send_queued_packets =
 * =======================
 *   
 * Overview:
 *   Send packets from the driver queue as long as there are some and
 *   transmit resources are available.
 *  
 * Returns:
 *   None
 *       
 * Arguments:
 *   smc - pointer to smc (adapter) structure
 *
 * Functional Description:
 *   Take a packet from queue if there is any. If not, then we are done.
 *   Check if there are resources to send the packet. If not, requeue it
 *   and exit. 
 *   Set packet descriptor flags and give packet to adapter.
 *   Check if any send resources can be freed (we do not use the
 *   transmit complete interrupt).
 */
static void send_queued_packets(struct s_smc *smc)
{
	skfddi_priv *bp = (skfddi_priv *) & smc->os;
	struct sk_buff *skb;
	unsigned char fc;
	int queue;
	struct s_smt_fp_txd *txd;	// Current TxD.
	dma_addr_t dma_address;
	unsigned long Flags;

	int frame_status;	// HWM tx frame status.

	PRINTK(KERN_INFO "send queued packets\n");
	for (;;) {
		// send first buffer from queue
		skb = skb_dequeue(&bp->SendSkbQueue);

		if (!skb) {
			PRINTK(KERN_INFO "queue empty\n");
			return;
		}		// queue empty !

		spin_lock_irqsave(&bp->DriverLock, Flags);
		fc = skb->data[0];
		queue = (fc & FC_SYNC_BIT) ? QUEUE_S : QUEUE_A0;
#ifdef ESS
		// Check if the frame may/must be sent as a synchronous frame.

		if ((fc & ~(FC_SYNC_BIT | FC_LLC_PRIOR)) == FC_ASYNC_LLC) {
			// It's an LLC frame.
			if (!smc->ess.sync_bw_available)
				fc &= ~FC_SYNC_BIT; // No bandwidth available.

			else {	// Bandwidth is available.

				if (smc->mib.fddiESSSynchTxMode) {
					// Send as sync. frame.
					fc |= FC_SYNC_BIT;
				}
			}
		}
#endif				// ESS
		frame_status = hwm_tx_init(smc, fc, 1, skb->len, queue);

		if ((frame_status & (LOC_TX | LAN_TX)) == 0) {
			// Unable to send the frame.

			if ((frame_status & RING_DOWN) != 0) {
				// Ring is down.
				PRINTK("Tx attempt while ring down.\n");
			} else if ((frame_status & OUT_OF_TXD) != 0) {
				PRINTK("%s: out of TXDs.\n", bp->dev->name);
			} else {
				PRINTK("%s: out of transmit resources",
					bp->dev->name);
			}

			// Note: We will retry the operation as soon as
			// transmit resources become available.
			skb_queue_head(&bp->SendSkbQueue, skb);
			spin_unlock_irqrestore(&bp->DriverLock, Flags);
			return;	// Packet has been queued.

		}		// if (unable to send frame)

		bp->QueueSkb++;	// one packet less in local queue

		// source address in packet ?
		CheckSourceAddress(skb->data, smc->hw.fddi_canon_addr.a);

		txd = (struct s_smt_fp_txd *) HWM_GET_CURR_TXD(smc, queue);

		dma_address = pci_map_single(&bp->pdev, skb->data,
					     skb->len, PCI_DMA_TODEVICE);
		if (frame_status & LAN_TX) {
			txd->txd_os.skb = skb;			// save skb
			txd->txd_os.dma_addr = dma_address;	// save dma mapping
		}
		hwm_tx_frag(smc, skb->data, dma_address, skb->len,
                      frame_status | FIRST_FRAG | LAST_FRAG | EN_IRQ_EOF);

		if (!(frame_status & LAN_TX)) {		// local only frame
			pci_unmap_single(&bp->pdev, dma_address,
					 skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb_irq(skb);
		}
		spin_unlock_irqrestore(&bp->DriverLock, Flags);
	}			// for

	return;			// never reached

}				// send_queued_packets


/************************
 * 
 * CheckSourceAddress
 *
 * Verify if the source address is set. Insert it if necessary.
 *
 ************************/
void CheckSourceAddress(unsigned char *frame, unsigned char *hw_addr)
{
	unsigned char SRBit;

	if ((((unsigned long) frame[1 + 6]) & ~0x01) != 0) // source routing bit

		return;
	if ((unsigned short) frame[1 + 10] != 0)
		return;
	SRBit = frame[1 + 6] & 0x01;
	memcpy(&frame[1 + 6], hw_addr, 6);
	frame[8] |= SRBit;
}				// CheckSourceAddress


/************************
 *
 *	ResetAdapter
 *
 *	Reset the adapter and bring it back to operational mode.
 * Args
 *	smc - A pointer to the SMT context struct.
 * Out
 *	Nothing.
 *
 ************************/
static void ResetAdapter(struct s_smc *smc)
{

	PRINTK(KERN_INFO "[fddi: ResetAdapter]\n");

	// Stop the adapter.

	card_stop(smc);		// Stop all activity.

	// Clear the transmit and receive descriptor queues.
	mac_drv_clear_tx_queue(smc);
	mac_drv_clear_rx_queue(smc);

	// Restart the adapter.

	smt_reset_defaults(smc, 1);	// Initialize the SMT module.

	init_smt(smc, (smc->os.dev)->dev_addr);	// Initialize the hardware.

	smt_online(smc, 1);	// Insert into the ring again.
	STI_FBI();

	// Restore original receive mode (multicasts, promiscuous, etc.).
	skfp_ctl_set_multicast_list_wo_lock(smc->os.dev);
}				// ResetAdapter


//--------------- functions called by hardware module ----------------

/************************
 *
 *	llc_restart_tx
 *
 *	The hardware driver calls this routine when the transmit complete
 *	interrupt bits (end of frame) for the synchronous or asynchronous
 *	queue is set.
 *
 * NOTE The hardware driver calls this function also if no packets are queued.
 *	The routine must be able to handle this case.
 * Args
 *	smc - A pointer to the SMT context struct.
 * Out
 *	Nothing.
 *
 ************************/
void llc_restart_tx(struct s_smc *smc)
{
	skfddi_priv *bp = (skfddi_priv *) & smc->os;

	PRINTK(KERN_INFO "[llc_restart_tx]\n");

	// Try to send queued packets
	spin_unlock(&bp->DriverLock);
	send_queued_packets(smc);
	spin_lock(&bp->DriverLock);
	netif_start_queue(bp->dev);// system may send again if it was blocked

}				// llc_restart_tx


/************************
 *
 *	mac_drv_get_space
 *
 *	The hardware module calls this function to allocate the memory
 *	for the SMT MBufs if the define MB_OUTSIDE_SMC is specified.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	size - Size of memory in bytes to allocate.
 * Out
 *	!= 0	A pointer to the virtual address of the allocated memory.
 *	== 0	Allocation error.
 *
 ************************/
void *mac_drv_get_space(struct s_smc *smc, unsigned int size)
{
	void *virt;

	PRINTK(KERN_INFO "mac_drv_get_space (%d bytes), ", size);
	virt = (void *) (smc->os.SharedMemAddr + smc->os.SharedMemHeap);

	if ((smc->os.SharedMemHeap + size) > smc->os.SharedMemSize) {
		printk("Unexpected SMT memory size requested: %d\n", size);
		return (NULL);
	}
	smc->os.SharedMemHeap += size;	// Move heap pointer.

	PRINTK(KERN_INFO "mac_drv_get_space end\n");
	PRINTK(KERN_INFO "virt addr: %lx\n", (ulong) virt);
	PRINTK(KERN_INFO "bus  addr: %lx\n", (ulong)
	       (smc->os.SharedMemDMA +
		((char *) virt - (char *)smc->os.SharedMemAddr)));
	return (virt);
}				// mac_drv_get_space


/************************
 *
 *	mac_drv_get_desc_mem
 *
 *	This function is called by the hardware dependent module.
 *	It allocates the memory for the RxD and TxD descriptors.
 *
 *	This memory must be non-cached, non-movable and non-swappable.
 *	This memory should start at a physical page boundary.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	size - Size of memory in bytes to allocate.
 * Out
 *	!= 0	A pointer to the virtual address of the allocated memory.
 *	== 0	Allocation error.
 *
 ************************/
void *mac_drv_get_desc_mem(struct s_smc *smc, unsigned int size)
{

	char *virt;

	PRINTK(KERN_INFO "mac_drv_get_desc_mem\n");

	// Descriptor memory must be aligned on 16-byte boundary.

	virt = mac_drv_get_space(smc, size);

	size = (u_int) (16 - (((unsigned long) virt) & 15UL));
	size = size % 16;

	PRINTK("Allocate %u bytes alignment gap ", size);
	PRINTK("for descriptor memory.\n");

	if (!mac_drv_get_space(smc, size)) {
		printk("fddi: Unable to align descriptor memory.\n");
		return (NULL);
	}
	return (virt + size);
}				// mac_drv_get_desc_mem


/************************
 *
 *	mac_drv_virt2phys
 *
 *	Get the physical address of a given virtual address.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	virt - A (virtual) pointer into our 'shared' memory area.
 * Out
 *	Physical address of the given virtual address.
 *
 ************************/
unsigned long mac_drv_virt2phys(struct s_smc *smc, void *virt)
{
	return (smc->os.SharedMemDMA +
		((char *) virt - (char *)smc->os.SharedMemAddr));
}				// mac_drv_virt2phys


/************************
 *
 *	dma_master
 *
 *	The HWM calls this function, when the driver leads through a DMA
 *	transfer. If the OS-specific module must prepare the system hardware
 *	for the DMA transfer, it should do it in this function.
 *
 *	The hardware module calls this dma_master if it wants to send an SMT
 *	frame.  This means that the virt address passed in here is part of
 *      the 'shared' memory area.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	virt - The virtual address of the data.
 *
 *	len - The length in bytes of the data.
 *
 *	flag - Indicates the transmit direction and the buffer type:
 *		DMA_RD	(0x01)	system RAM ==> adapter buffer memory
 *		DMA_WR	(0x02)	adapter buffer memory ==> system RAM
 *		SMT_BUF (0x80)	SMT buffer
 *
 *	>> NOTE: SMT_BUF and DMA_RD are always set for PCI. <<
 * Out
 *	Returns the pyhsical address for the DMA transfer.
 *
 ************************/
u_long dma_master(struct s_smc * smc, void *virt, int len, int flag)
{
	return (smc->os.SharedMemDMA +
		((char *) virt - (char *)smc->os.SharedMemAddr));
}				// dma_master


/************************
 *
 *	dma_complete
 *
 *	The hardware module calls this routine when it has completed a DMA
 *	transfer. If the operating system dependant module has set up the DMA
 *	channel via dma_master() (e.g. Windows NT or AIX) it should clean up
 *	the DMA channel.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	descr - A pointer to a TxD or RxD, respectively.
 *
 *	flag - Indicates the DMA transfer direction / SMT buffer:
 *		DMA_RD	(0x01)	system RAM ==> adapter buffer memory
 *		DMA_WR	(0x02)	adapter buffer memory ==> system RAM
 *		SMT_BUF (0x80)	SMT buffer (managed by HWM)
 * Out
 *	Nothing.
 *
 ************************/
void dma_complete(struct s_smc *smc, volatile union s_fp_descr *descr, int flag)
{
	/* For TX buffers, there are two cases.  If it is an SMT transmit
	 * buffer, there is nothing to do since we use consistent memory
	 * for the 'shared' memory area.  The other case is for normal
	 * transmit packets given to us by the networking stack, and in
	 * that case we cleanup the PCI DMA mapping in mac_drv_tx_complete
	 * below.
	 *
	 * For RX buffers, we have to unmap dynamic PCI DMA mappings here
	 * because the hardware module is about to potentially look at
	 * the contents of the buffer.  If we did not call the PCI DMA
	 * unmap first, the hardware module could read inconsistent data.
	 */
	if (flag & DMA_WR) {
		skfddi_priv *bp = (skfddi_priv *) & smc->os;
		volatile struct s_smt_fp_rxd *r = &descr->r;

		/* If SKB is NULL, we used the local buffer. */
		if (r->rxd_os.skb && r->rxd_os.dma_addr) {
			int MaxFrameSize = bp->MaxFrameSize;

			pci_unmap_single(&bp->pdev, r->rxd_os.dma_addr,
					 MaxFrameSize, PCI_DMA_FROMDEVICE);
			r->rxd_os.dma_addr = 0;
		}
	}
}				// dma_complete


/************************
 *
 *	mac_drv_tx_complete
 *
 *	Transmit of a packet is complete. Release the tx staging buffer.
 *
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	txd - A pointer to the last TxD which is used by the frame.
 * Out
 *	Returns nothing.
 *
 ************************/
void mac_drv_tx_complete(struct s_smc *smc, volatile struct s_smt_fp_txd *txd)
{
	struct sk_buff *skb;

	PRINTK(KERN_INFO "entering mac_drv_tx_complete\n");
	// Check if this TxD points to a skb

	if (!(skb = txd->txd_os.skb)) {
		PRINTK("TXD with no skb assigned.\n");
		return;
	}
	txd->txd_os.skb = NULL;

	// release the DMA mapping
	pci_unmap_single(&smc->os.pdev, txd->txd_os.dma_addr,
			 skb->len, PCI_DMA_TODEVICE);
	txd->txd_os.dma_addr = 0;

	smc->os.MacStat.tx_packets++;	// Count transmitted packets.
	smc->os.MacStat.tx_bytes+=skb->len;	// Count bytes

	// free the skb
	dev_kfree_skb_irq(skb);

	PRINTK(KERN_INFO "leaving mac_drv_tx_complete\n");
}				// mac_drv_tx_complete


/************************
 *
 * dump packets to logfile
 *
 ************************/
#ifdef DUMPPACKETS
void dump_data(unsigned char *Data, int length)
{
	int i, j;
	unsigned char s[255], sh[10];
	if (length > 64) {
		length = 64;
	}
	printk(KERN_INFO "---Packet start---\n");
	for (i = 0, j = 0; i < length / 8; i++, j += 8)
		printk(KERN_INFO "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		       Data[j + 0], Data[j + 1], Data[j + 2], Data[j + 3],
		       Data[j + 4], Data[j + 5], Data[j + 6], Data[j + 7]);
	strcpy(s, "");
	for (i = 0; i < length % 8; i++) {
		sprintf(sh, "%02x ", Data[j + i]);
		strcat(s, sh);
	}
	printk(KERN_INFO "%s\n", s);
	printk(KERN_INFO "------------------\n");
}				// dump_data
#else
#define dump_data(data,len)
#endif				// DUMPPACKETS

/************************
 *
 *	mac_drv_rx_complete
 *
 *	The hardware module calls this function if an LLC frame is received
 *	in a receive buffer. Also the SMT, NSA, and directed beacon frames
 *	from the network will be passed to the LLC layer by this function
 *	if passing is enabled.
 *
 *	mac_drv_rx_complete forwards the frame to the LLC layer if it should
 *	be received. It also fills the RxD ring with new receive buffers if
 *	some can be queued.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	rxd - A pointer to the first RxD which is used by the receive frame.
 *
 *	frag_count - Count of RxDs used by the received frame.
 *
 *	len - Frame length.
 * Out
 *	Nothing.
 *
 ************************/
void mac_drv_rx_complete(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count, int len)
{
	skfddi_priv *bp = (skfddi_priv *) & smc->os;
	struct sk_buff *skb;
	unsigned char *virt, *cp;
	unsigned short ri;
	u_int RifLength;

	PRINTK(KERN_INFO "entering mac_drv_rx_complete (len=%d)\n", len);
	if (frag_count != 1) {	// This is not allowed to happen.

		printk("fddi: Multi-fragment receive!\n");
		goto RequeueRxd;	// Re-use the given RXD(s).

	}
	skb = rxd->rxd_os.skb;
	if (!skb) {
		PRINTK(KERN_INFO "No skb in rxd\n");
		smc->os.MacStat.rx_errors++;
		goto RequeueRxd;
	}
	virt = skb->data;

	// The DMA mapping was released in dma_complete above.

	dump_data(skb->data, len);

	/*
	 * FDDI Frame format:
	 * +-------+-------+-------+------------+--------+------------+
	 * | FC[1] | DA[6] | SA[6] | RIF[0..18] | LLC[3] | Data[0..n] |
	 * +-------+-------+-------+------------+--------+------------+
	 *
	 * FC = Frame Control
	 * DA = Destination Address
	 * SA = Source Address
	 * RIF = Routing Information Field
	 * LLC = Logical Link Control
	 */

	// Remove Routing Information Field (RIF), if present.

	if ((virt[1 + 6] & FDDI_RII) == 0)
		RifLength = 0;
	else {
		int n;
// goos: RIF removal has still to be tested
		PRINTK(KERN_INFO "RIF found\n");
		// Get RIF length from Routing Control (RC) field.
		cp = virt + FDDI_MAC_HDR_LEN;	// Point behind MAC header.

		ri = ntohs(*((unsigned short *) cp));
		RifLength = ri & FDDI_RCF_LEN_MASK;
		if (len < (int) (FDDI_MAC_HDR_LEN + RifLength)) {
			printk("fddi: Invalid RIF.\n");
			goto RequeueRxd;	// Discard the frame.

		}
		virt[1 + 6] &= ~FDDI_RII;	// Clear RII bit.
		// regions overlap

		virt = cp + RifLength;
		for (n = FDDI_MAC_HDR_LEN; n; n--)
			*--virt = *--cp;
		// adjust sbd->data pointer
		skb_pull(skb, RifLength);
		len -= RifLength;
		RifLength = 0;
	}

	// Count statistics.
	smc->os.MacStat.rx_packets++;	// Count indicated receive packets.
	smc->os.MacStat.rx_bytes+=len;	// Count bytes

	// virt points to header again
	if (virt[1] & 0x01) {	// Check group (multicast) bit.

		smc->os.MacStat.multicast++;
	}

	// deliver frame to system
	rxd->rxd_os.skb = NULL;
	skb_trim(skb, len);
	skb->protocol = fddi_type_trans(skb, bp->dev);
	skb->dev = bp->dev;	/* pass up device pointer */

	netif_rx(skb);
	bp->dev->last_rx = jiffies;

	HWM_RX_CHECK(smc, RX_LOW_WATERMARK);
	return;

      RequeueRxd:
	PRINTK(KERN_INFO "Rx: re-queue RXD.\n");
	mac_drv_requeue_rxd(smc, rxd, frag_count);
	smc->os.MacStat.rx_errors++;	// Count receive packets not indicated.

}				// mac_drv_rx_complete


/************************
 *
 *	mac_drv_requeue_rxd
 *
 *	The hardware module calls this function to request the OS-specific
 *	module to queue the receive buffer(s) represented by the pointer
 *	to the RxD and the frag_count into the receive queue again. This
 *	buffer was filled with an invalid frame or an SMT frame.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	rxd - A pointer to the first RxD which is used by the receive frame.
 *
 *	frag_count - Count of RxDs used by the received frame.
 * Out
 *	Nothing.
 *
 ************************/
void mac_drv_requeue_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count)
{
	volatile struct s_smt_fp_rxd *next_rxd;
	volatile struct s_smt_fp_rxd *src_rxd;
	struct sk_buff *skb;
	int MaxFrameSize;
	unsigned char *v_addr;
	dma_addr_t b_addr;

	if (frag_count != 1)	// This is not allowed to happen.

		printk("fddi: Multi-fragment requeue!\n");

	MaxFrameSize = ((skfddi_priv *) & smc->os)->MaxFrameSize;
	src_rxd = rxd;
	for (; frag_count > 0; frag_count--) {
		next_rxd = src_rxd->rxd_next;
		rxd = HWM_GET_CURR_RXD(smc);

		skb = src_rxd->rxd_os.skb;
		if (skb == NULL) {	// this should not happen

			PRINTK("Requeue with no skb in rxd!\n");
			skb = alloc_skb(MaxFrameSize + 3, GFP_ATOMIC);
			if (skb) {
				// we got a skb
				rxd->rxd_os.skb = skb;
				skb_reserve(skb, 3);
				skb_put(skb, MaxFrameSize);
				v_addr = skb->data;
				b_addr = pci_map_single(&smc->os.pdev,
							v_addr,
							MaxFrameSize,
							PCI_DMA_FROMDEVICE);
				rxd->rxd_os.dma_addr = b_addr;
			} else {
				// no skb available, use local buffer
				PRINTK("Queueing invalid buffer!\n");
				rxd->rxd_os.skb = NULL;
				v_addr = smc->os.LocalRxBuffer;
				b_addr = smc->os.LocalRxBufferDMA;
			}
		} else {
			// we use skb from old rxd
			rxd->rxd_os.skb = skb;
			v_addr = skb->data;
			b_addr = pci_map_single(&smc->os.pdev,
						v_addr,
						MaxFrameSize,
						PCI_DMA_FROMDEVICE);
			rxd->rxd_os.dma_addr = b_addr;
		}
		hwm_rx_frag(smc, v_addr, b_addr, MaxFrameSize,
			    FIRST_FRAG | LAST_FRAG);

		src_rxd = next_rxd;
	}
}				// mac_drv_requeue_rxd


/************************
 *
 *	mac_drv_fill_rxd
 *
 *	The hardware module calls this function at initialization time
 *	to fill the RxD ring with receive buffers. It is also called by
 *	mac_drv_rx_complete if rx_free is large enough to queue some new
 *	receive buffers into the RxD ring. mac_drv_fill_rxd queues new
 *	receive buffers as long as enough RxDs and receive buffers are
 *	available.
 * Args
 *	smc - A pointer to the SMT context struct.
 * Out
 *	Nothing.
 *
 ************************/
void mac_drv_fill_rxd(struct s_smc *smc)
{
	int MaxFrameSize;
	unsigned char *v_addr;
	unsigned long b_addr;
	struct sk_buff *skb;
	volatile struct s_smt_fp_rxd *rxd;

	PRINTK(KERN_INFO "entering mac_drv_fill_rxd\n");

	// Walk through the list of free receive buffers, passing receive
	// buffers to the HWM as long as RXDs are available.

	MaxFrameSize = ((skfddi_priv *) & smc->os)->MaxFrameSize;
	// Check if there is any RXD left.
	while (HWM_GET_RX_FREE(smc) > 0) {
		PRINTK(KERN_INFO ".\n");

		rxd = HWM_GET_CURR_RXD(smc);
		skb = alloc_skb(MaxFrameSize + 3, GFP_ATOMIC);
		if (skb) {
			// we got a skb
			skb_reserve(skb, 3);
			skb_put(skb, MaxFrameSize);
			v_addr = skb->data;
			b_addr = pci_map_single(&smc->os.pdev,
						v_addr,
						MaxFrameSize,
						PCI_DMA_FROMDEVICE);
			rxd->rxd_os.dma_addr = b_addr;
		} else {
			// no skb available, use local buffer
			// System has run out of buffer memory, but we want to
			// keep the receiver running in hope of better times.
			// Multiple descriptors may point to this local buffer,
			// so data in it must be considered invalid.
			PRINTK("Queueing invalid buffer!\n");
			v_addr = smc->os.LocalRxBuffer;
			b_addr = smc->os.LocalRxBufferDMA;
		}

		rxd->rxd_os.skb = skb;

		// Pass receive buffer to HWM.
		hwm_rx_frag(smc, v_addr, b_addr, MaxFrameSize,
			    FIRST_FRAG | LAST_FRAG);
	}
	PRINTK(KERN_INFO "leaving mac_drv_fill_rxd\n");
}				// mac_drv_fill_rxd


/************************
 *
 *	mac_drv_clear_rxd
 *
 *	The hardware module calls this function to release unused
 *	receive buffers.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	rxd - A pointer to the first RxD which is used by the receive buffer.
 *
 *	frag_count - Count of RxDs used by the receive buffer.
 * Out
 *	Nothing.
 *
 ************************/
void mac_drv_clear_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
		       int frag_count)
{

	struct sk_buff *skb;

	PRINTK("entering mac_drv_clear_rxd\n");

	if (frag_count != 1)	// This is not allowed to happen.

		printk("fddi: Multi-fragment clear!\n");

	for (; frag_count > 0; frag_count--) {
		skb = rxd->rxd_os.skb;
		if (skb != NULL) {
			skfddi_priv *bp = (skfddi_priv *) & smc->os;
			int MaxFrameSize = bp->MaxFrameSize;

			pci_unmap_single(&bp->pdev, rxd->rxd_os.dma_addr,
					 MaxFrameSize, PCI_DMA_FROMDEVICE);

			dev_kfree_skb(skb);
			rxd->rxd_os.skb = NULL;
		}
		rxd = rxd->rxd_next;	// Next RXD.

	}
}				// mac_drv_clear_rxd


/************************
 *
 *	mac_drv_rx_init
 *
 *	The hardware module calls this routine when an SMT or NSA frame of the
 *	local SMT should be delivered to the LLC layer.
 *
 *	It is necessary to have this function, because there is no other way to
 *	copy the contents of SMT MBufs into receive buffers.
 *
 *	mac_drv_rx_init allocates the required target memory for this frame,
 *	and receives the frame fragment by fragment by calling mac_drv_rx_frag.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	len - The length (in bytes) of the received frame (FC, DA, SA, Data).
 *
 *	fc - The Frame Control field of the received frame.
 *
 *	look_ahead - A pointer to the lookahead data buffer (may be NULL).
 *
 *	la_len - The length of the lookahead data stored in the lookahead
 *	buffer (may be zero).
 * Out
 *	Always returns zero (0).
 *
 ************************/
int mac_drv_rx_init(struct s_smc *smc, int len, int fc,
		    char *look_ahead, int la_len)
{
	struct sk_buff *skb;

	PRINTK("entering mac_drv_rx_init(len=%d)\n", len);

	// "Received" a SMT or NSA frame of the local SMT.

	if (len != la_len || len < FDDI_MAC_HDR_LEN || !look_ahead) {
		PRINTK("fddi: Discard invalid local SMT frame\n");
		PRINTK("  len=%d, la_len=%d, (ULONG) look_ahead=%08lXh.\n",
		       len, la_len, (unsigned long) look_ahead);
		return (0);
	}
	skb = alloc_skb(len + 3, GFP_ATOMIC);
	if (!skb) {
		PRINTK("fddi: Local SMT: skb memory exhausted.\n");
		return (0);
	}
	skb_reserve(skb, 3);
	skb_put(skb, len);
	memcpy(skb->data, look_ahead, len);

	// deliver frame to system
	skb->protocol = fddi_type_trans(skb, ((skfddi_priv *) & smc->os)->dev);
	skb->dev->last_rx = jiffies;
	netif_rx(skb);

	return (0);
}				// mac_drv_rx_init


/************************
 *
 *	smt_timer_poll
 *
 *	This routine is called periodically by the SMT module to clean up the
 *	driver.
 *
 *	Return any queued frames back to the upper protocol layers if the ring
 *	is down.
 * Args
 *	smc - A pointer to the SMT context struct.
 * Out
 *	Nothing.
 *
 ************************/
void smt_timer_poll(struct s_smc *smc)
{
}				// smt_timer_poll


/************************
 *
 *	ring_status_indication
 *
 *	This function indicates a change of the ring state.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	status - The current ring status.
 * Out
 *	Nothing.
 *
 ************************/
void ring_status_indication(struct s_smc *smc, u_long status)
{
	PRINTK("ring_status_indication( ");
	if (status & RS_RES15)
		PRINTK("RS_RES15 ");
	if (status & RS_HARDERROR)
		PRINTK("RS_HARDERROR ");
	if (status & RS_SOFTERROR)
		PRINTK("RS_SOFTERROR ");
	if (status & RS_BEACON)
		PRINTK("RS_BEACON ");
	if (status & RS_PATHTEST)
		PRINTK("RS_PATHTEST ");
	if (status & RS_SELFTEST)
		PRINTK("RS_SELFTEST ");
	if (status & RS_RES9)
		PRINTK("RS_RES9 ");
	if (status & RS_DISCONNECT)
		PRINTK("RS_DISCONNECT ");
	if (status & RS_RES7)
		PRINTK("RS_RES7 ");
	if (status & RS_DUPADDR)
		PRINTK("RS_DUPADDR ");
	if (status & RS_NORINGOP)
		PRINTK("RS_NORINGOP ");
	if (status & RS_VERSION)
		PRINTK("RS_VERSION ");
	if (status & RS_STUCKBYPASSS)
		PRINTK("RS_STUCKBYPASSS ");
	if (status & RS_EVENT)
		PRINTK("RS_EVENT ");
	if (status & RS_RINGOPCHANGE)
		PRINTK("RS_RINGOPCHANGE ");
	if (status & RS_RES0)
		PRINTK("RS_RES0 ");
	PRINTK("]\n");
}				// ring_status_indication


/************************
 *
 *	smt_get_time
 *
 *	Gets the current time from the system.
 * Args
 *	None.
 * Out
 *	The current time in TICKS_PER_SECOND.
 *
 *	TICKS_PER_SECOND has the unit 'count of timer ticks per second'. It is
 *	defined in "targetos.h". The definition of TICKS_PER_SECOND must comply
 *	to the time returned by smt_get_time().
 *
 ************************/
unsigned long smt_get_time(void)
{
	return jiffies;
}				// smt_get_time


/************************
 *
 *	smt_stat_counter
 *
 *	Status counter update (ring_op, fifo full).
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	stat -	= 0: A ring operational change occurred.
 *		= 1: The FORMAC FIFO buffer is full / FIFO overflow.
 * Out
 *	Nothing.
 *
 ************************/
void smt_stat_counter(struct s_smc *smc, int stat)
{
//      BOOLEAN RingIsUp ;

	PRINTK(KERN_INFO "smt_stat_counter\n");
	switch (stat) {
	case 0:
		PRINTK(KERN_INFO "Ring operational change.\n");
		break;
	case 1:
		PRINTK(KERN_INFO "Receive fifo overflow.\n");
		smc->os.MacStat.rx_errors++;
		break;
	default:
		PRINTK(KERN_INFO "Unknown status (%d).\n", stat);
		break;
	}
}				// smt_stat_counter


/************************
 *
 *	cfm_state_change
 *
 *	Sets CFM state in custom statistics.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	c_state - Possible values are:
 *
 *		EC0_OUT, EC1_IN, EC2_TRACE, EC3_LEAVE, EC4_PATH_TEST,
 *		EC5_INSERT, EC6_CHECK, EC7_DEINSERT
 * Out
 *	Nothing.
 *
 ************************/
void cfm_state_change(struct s_smc *smc, int c_state)
{
#ifdef DRIVERDEBUG
	char *s;

	switch (c_state) {
	case SC0_ISOLATED:
		s = "SC0_ISOLATED";
		break;
	case SC1_WRAP_A:
		s = "SC1_WRAP_A";
		break;
	case SC2_WRAP_B:
		s = "SC2_WRAP_B";
		break;
	case SC4_THRU_A:
		s = "SC4_THRU_A";
		break;
	case SC5_THRU_B:
		s = "SC5_THRU_B";
		break;
	case SC7_WRAP_S:
		s = "SC7_WRAP_S";
		break;
	case SC9_C_WRAP_A:
		s = "SC9_C_WRAP_A";
		break;
	case SC10_C_WRAP_B:
		s = "SC10_C_WRAP_B";
		break;
	case SC11_C_WRAP_S:
		s = "SC11_C_WRAP_S";
		break;
	default:
		PRINTK(KERN_INFO "cfm_state_change: unknown %d\n", c_state);
		return;
	}
	PRINTK(KERN_INFO "cfm_state_change: %s\n", s);
#endif				// DRIVERDEBUG
}				// cfm_state_change


/************************
 *
 *	ecm_state_change
 *
 *	Sets ECM state in custom statistics.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	e_state - Possible values are:
 *
 *		SC0_ISOLATED, SC1_WRAP_A (5), SC2_WRAP_B (6), SC4_THRU_A (12),
 *		SC5_THRU_B (7), SC7_WRAP_S (8)
 * Out
 *	Nothing.
 *
 ************************/
void ecm_state_change(struct s_smc *smc, int e_state)
{
#ifdef DRIVERDEBUG
	char *s;

	switch (e_state) {
	case EC0_OUT:
		s = "EC0_OUT";
		break;
	case EC1_IN:
		s = "EC1_IN";
		break;
	case EC2_TRACE:
		s = "EC2_TRACE";
		break;
	case EC3_LEAVE:
		s = "EC3_LEAVE";
		break;
	case EC4_PATH_TEST:
		s = "EC4_PATH_TEST";
		break;
	case EC5_INSERT:
		s = "EC5_INSERT";
		break;
	case EC6_CHECK:
		s = "EC6_CHECK";
		break;
	case EC7_DEINSERT:
		s = "EC7_DEINSERT";
		break;
	default:
		s = "unknown";
		break;
	}
	PRINTK(KERN_INFO "ecm_state_change: %s\n", s);
#endif				//DRIVERDEBUG
}				// ecm_state_change


/************************
 *
 *	rmt_state_change
 *
 *	Sets RMT state in custom statistics.
 * Args
 *	smc - A pointer to the SMT context struct.
 *
 *	r_state - Possible values are:
 *
 *		RM0_ISOLATED, RM1_NON_OP, RM2_RING_OP, RM3_DETECT,
 *		RM4_NON_OP_DUP, RM5_RING_OP_DUP, RM6_DIRECTED, RM7_TRACE
 * Out
 *	Nothing.
 *
 ************************/
void rmt_state_change(struct s_smc *smc, int r_state)
{
#ifdef DRIVERDEBUG
	char *s;

	switch (r_state) {
	case RM0_ISOLATED:
		s = "RM0_ISOLATED";
		break;
	case RM1_NON_OP:
		s = "RM1_NON_OP - not operational";
		break;
	case RM2_RING_OP:
		s = "RM2_RING_OP - ring operational";
		break;
	case RM3_DETECT:
		s = "RM3_DETECT - detect dupl addresses";
		break;
	case RM4_NON_OP_DUP:
		s = "RM4_NON_OP_DUP - dupl. addr detected";
		break;
	case RM5_RING_OP_DUP:
		s = "RM5_RING_OP_DUP - ring oper. with dupl. addr";
		break;
	case RM6_DIRECTED:
		s = "RM6_DIRECTED - sending directed beacons";
		break;
	case RM7_TRACE:
		s = "RM7_TRACE - trace initiated";
		break;
	default:
		s = "unknown";
		break;
	}
	PRINTK(KERN_INFO "[rmt_state_change: %s]\n", s);
#endif				// DRIVERDEBUG
}				// rmt_state_change


/************************
 *
 *	drv_reset_indication
 *
 *	This function is called by the SMT when it has detected a severe
 *	hardware problem. The driver should perform a reset on the adapter
 *	as soon as possible, but not from within this function.
 * Args
 *	smc - A pointer to the SMT context struct.
 * Out
 *	Nothing.
 *
 ************************/
void drv_reset_indication(struct s_smc *smc)
{
	PRINTK(KERN_INFO "entering drv_reset_indication\n");

	smc->os.ResetRequested = TRUE;	// Set flag.

}				// drv_reset_indication



//--------------- functions for use as a module ----------------

#ifdef MODULE
/************************
 *
 * Note now that module autoprobing is allowed under PCI. The
 * IRQ lines will not be auto-detected; instead I'll rely on the BIOSes
 * to "do the right thing".
 *
 ************************/
#define LP(a) ((struct s_smc*)(a))
static struct net_device *mdev;

/************************
 *
 * init_module
 *
 *  If compiled as a module, find
 *  adapters and initialize them.
 *
 ************************/
int init_module(void)
{
	struct net_device *p;

	PRINTK(KERN_INFO "FDDI init module\n");
	if ((mdev = insert_device(NULL, skfp_probe)) == NULL)
		return -ENOMEM;

	for (p = mdev; p != NULL; p = LP(p->priv)->os.next_module) {
		PRINTK(KERN_INFO "device to register: %s\n", p->name);
		if (register_netdev(p) != 0) {
			printk("skfddi init_module failed\n");
			return -EIO;
		}
	}

	PRINTK(KERN_INFO "+++++ exit with success +++++\n");
	return 0;
}				// init_module

/************************
 *
 * cleanup_module
 *
 *  Release all resources claimed by this module.
 *
 ************************/
void cleanup_module(void)
{
	PRINTK(KERN_INFO "cleanup_module\n");
	while (mdev != NULL) {
		mdev = unlink_modules(mdev);
	}
	return;
}				// cleanup_module


/************************
 *
 * unlink_modules
 *
 *  Unregister devices and release their memory.
 *
 ************************/
static struct net_device *unlink_modules(struct net_device *p)
{
	struct net_device *next = NULL;

	if (p->priv) {		/* Private areas allocated? */
		struct s_smc *lp = (struct s_smc *) p->priv;

		next = lp->os.next_module;

		if (lp->os.SharedMemAddr) {
			pci_free_consistent(&lp->os.pdev,
					    lp->os.SharedMemSize,
					    lp->os.SharedMemAddr,
					    lp->os.SharedMemDMA);
			lp->os.SharedMemAddr = NULL;
		}
		if (lp->os.LocalRxBuffer) {
			pci_free_consistent(&lp->os.pdev,
					    MAX_FRAME_SIZE,
					    lp->os.LocalRxBuffer,
					    lp->os.LocalRxBufferDMA);
			lp->os.LocalRxBuffer = NULL;
		}
		release_region(p->base_addr, 
			(lp->os.bus_type == SK_BUS_TYPE_PCI ? FP_IO_LEN : 0));
	}
	unregister_netdev(p);
	printk("%s: unloaded\n", p->name);
	kfree(p);		/* Free the device structure */

	return next;
}				// unlink_modules


#endif				/* MODULE */
