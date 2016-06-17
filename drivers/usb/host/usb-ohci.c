/*
 * URB OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2001 David Brownell <dbrownell@users.sourceforge.net>
 * 
 * [ Initialisation is based on Linus'  ]
 * [ uhci code and gregs ohci fragments ]
 * [ (C) Copyright 1999 Linus Torvalds  ]
 * [ (C) Copyright 1999 Gregory P. Smith]
 * 
 * 
 * History:
 *
 * 2002/10/22 OHCI_USB_OPER for ALi lockup in IBM i1200 (ALEX <thchou@ali>)
 * 2002/03/08 interrupt unlink fix (Matt Hughes), better cleanup on
 *	load failure (Matthew Frederickson)
 * 2002/01/20 async unlink fixes:  return -EINPROGRESS (per spec) and
 *	make interrupt unlink-in-completion work (db)
 * 2001/09/19 USB_ZERO_PACKET support (Jean Tourrilhes)
 * 2001/07/17 power management and pmac cleanup (Benjamin Herrenschmidt)
 * 2001/03/24 td/ed hashing to remove bus_to_virt (Steve Longerbeam);
 	pci_map_single (db)
 * 2001/03/21 td and dev/ed allocation uses new pci_pool API (db)
 * 2001/03/07 hcca allocation uses pci_alloc_consistent (Steve Longerbeam)
 *
 * 2000/09/26 fixed races in removing the private portion of the urb
 * 2000/09/07 disable bulk and control lists when unlinking the last
 *	endpoint descriptor in order to avoid unrecoverable errors on
 *	the Lucent chips. (rwc@sgi)
 * 2000/08/29 use bandwidth claiming hooks (thanks Randy!), fix some
 *	urb unlink probs, indentation fixes
 * 2000/08/11 various oops fixes mostly affecting iso and cleanup from
 *	device unplugs.
 * 2000/06/28 use PCI hotplug framework, for better power management
 *	and for Cardbus support (David Brownell)
 * 2000/earlier:  fixes for NEC/Lucent chips; suspend/resume handling
 *	when the controller loses power; handle UE; cleanup; ...
 *
 * v5.2 1999/12/07 URB 3rd preview, 
 * v5.1 1999/11/30 URB 2nd preview, cpia, (usb-scsi)
 * v5.0 1999/11/22 URB Technical preview, Paul Mackerras powerbook susp/resume 
 * 	i386: HUB, Keyboard, Mouse, Printer 
 *
 * v4.3 1999/10/27 multiple HCs, bulk_request
 * v4.2 1999/09/05 ISO API alpha, new dev alloc, neg Error-codes
 * v4.1 1999/08/27 Randy Dunlap's - ISO API first impl.
 * v4.0 1999/08/18 
 * v3.0 1999/06/25 
 * v2.1 1999/05/09  code clean up
 * v2.0 1999/05/04 
 * v1.0 1999/04/27 initial release
 */
 
#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>  /* for in_interrupt() */
#undef DEBUG
#include <linux/usb.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>

#define OHCI_USE_NPS		// force NoPowerSwitching mode
// #define OHCI_VERBOSE_DEBUG	/* not always helpful */

#include "usb-ohci.h"

#include "../hcd.h"

#ifdef CONFIG_PMAC_PBOOK
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/pci-bridge.h>
#ifndef CONFIG_PM
#define CONFIG_PM
#endif
#endif


/*
 * Version Information
 */
#define DRIVER_VERSION "v5.3"
#define DRIVER_AUTHOR "Roman Weissgaerber <weissg@vienna.at>, David Brownell"
#define DRIVER_DESC "USB OHCI Host Controller Driver"

/* For initializing controller (mask in an HCFS mode too) */
#define	OHCI_CONTROL_INIT \
	(OHCI_CTRL_CBSR & 0x3) | OHCI_CTRL_IE | OHCI_CTRL_PLE

#define OHCI_UNLINK_TIMEOUT	(HZ / 10)

/*-------------------------------------------------------------------------*/

/* AMD-756 (D2 rev) reports corrupt register contents in some cases.
 * The erratum (#4) description is incorrect.  AMD's workaround waits
 * till some bits (mostly reserved) are clear; ok for all revs.
 */
#define read_roothub(hc, register, mask) ({ \
	u32 temp = readl (&hc->regs->roothub.register); \
	if (hc->flags & OHCI_QUIRK_AMD756) \
		while (temp & mask) \
			temp = readl (&hc->regs->roothub.register); \
	temp; })

static u32 roothub_a (struct ohci *hc)
	{ return read_roothub (hc, a, 0xfc0fe000); }
static inline u32 roothub_b (struct ohci *hc)
	{ return readl (&hc->regs->roothub.b); }
static inline u32 roothub_status (struct ohci *hc)
	{ return readl (&hc->regs->roothub.status); }
static u32 roothub_portstatus (struct ohci *hc, int i)
	{ return read_roothub (hc, portstatus [i], 0xffe0fce0); }


/*-------------------------------------------------------------------------*
 * URB support functions 
 *-------------------------------------------------------------------------*/ 

static void ohci_complete_add(struct ohci *ohci, struct urb *urb)
{

	if (urb->hcpriv != NULL) {
		printk("completing with non-null priv!\n");
		return;
	}

	if (ohci->complete_tail == NULL) {
		ohci->complete_head = urb;
		ohci->complete_tail = urb;
	} else {
		ohci->complete_head->hcpriv = urb;
		ohci->complete_tail = urb;
	}
}

static inline struct urb *ohci_complete_get(struct ohci *ohci)
{
	struct urb *urb;

	if ((urb = ohci->complete_head) == NULL)
		return NULL;
	if (urb == ohci->complete_tail) {
		ohci->complete_tail = NULL;
		ohci->complete_head = NULL;
	} else {
		ohci->complete_head = urb->hcpriv;
	}
	urb->hcpriv = NULL;
	return urb;
}

static inline void ohci_complete(struct ohci *ohci)
{
	struct urb *urb;

	spin_lock(&ohci->ohci_lock);
	while ((urb = ohci_complete_get(ohci)) != NULL) {
		spin_unlock(&ohci->ohci_lock);
		if (urb->dev) {
			usb_dec_dev_use (urb->dev);
			urb->dev = NULL;
		}
		if (urb->complete)
			(*urb->complete)(urb);
		spin_lock(&ohci->ohci_lock);
	}
	spin_unlock(&ohci->ohci_lock);
}
 
/* free HCD-private data associated with this URB */

static void urb_free_priv (struct ohci *hc, urb_priv_t * urb_priv)
{
	int		i;
	int		last = urb_priv->length - 1;
	int		len;
	int		dir;
	struct td	*td;

	if (last >= 0) {

		/* ISOC, BULK, INTR data buffer starts at td 0 
		 * CTRL setup starts at td 0 */
		td = urb_priv->td [0];

		len = td->urb->transfer_buffer_length,
		dir = usb_pipeout (td->urb->pipe)
					? PCI_DMA_TODEVICE
					: PCI_DMA_FROMDEVICE;

		/* unmap CTRL URB setup */
		if (usb_pipecontrol (td->urb->pipe)) {
			pci_unmap_single (hc->ohci_dev, 
					td->data_dma, 8, PCI_DMA_TODEVICE);
			
			/* CTRL data buffer starts at td 1 if len > 0 */
			if (len && last > 0)
				td = urb_priv->td [1]; 		
		} 

		/* unmap data buffer */
		if (len && td->data_dma)
			pci_unmap_single (hc->ohci_dev, td->data_dma, len, dir);
		
		for (i = 0; i <= last; i++) {
			td = urb_priv->td [i];
			if (td)
				td_free (hc, td);
		}
	}

	kfree (urb_priv);
}
 
static void urb_rm_priv_locked (struct urb * urb) 
{
	urb_priv_t * urb_priv = urb->hcpriv;
	
	if (urb_priv) {
		urb->hcpriv = NULL;

#ifdef	DO_TIMEOUTS
		if (urb->timeout) {
			list_del (&urb->urb_list);
			urb->timeout -= jiffies;
		}
#endif

		/* Release int/iso bandwidth */
		if (urb->bandwidth) {
			switch (usb_pipetype(urb->pipe)) {
			case PIPE_INTERRUPT:
				usb_release_bandwidth (urb->dev, urb, 0);
				break;
			case PIPE_ISOCHRONOUS:
				usb_release_bandwidth (urb->dev, urb, 1);
				break;
			default:
				break;
			}
		}

		urb_free_priv ((struct ohci *)urb->dev->bus->hcpriv, urb_priv);
	} else {
		if (urb->dev != NULL) {
			err ("Non-null dev at rm_priv time");
			// urb->dev = NULL;
		}
	}
}

/*-------------------------------------------------------------------------*/
 
#ifdef DEBUG
static int sohci_get_current_frame_number (struct usb_device * dev);

/* debug| print the main components of an URB     
 * small: 0) header + data packets 1) just header */
 
static void urb_print (struct urb * urb, char * str, int small)
{
	unsigned int pipe= urb->pipe;
	
	if (!urb->dev || !urb->dev->bus) {
		dbg("%s URB: no dev", str);
		return;
	}
	
#ifndef	OHCI_VERBOSE_DEBUG
	if (urb->status != 0)
#endif
	dbg("%s URB:[%4x] dev:%2d,ep:%2d-%c,type:%s,flags:%4x,len:%d/%d,stat:%d(%x)", 
			str,
		 	sohci_get_current_frame_number (urb->dev), 
		 	usb_pipedevice (pipe),
		 	usb_pipeendpoint (pipe), 
		 	usb_pipeout (pipe)? 'O': 'I',
		 	usb_pipetype (pipe) < 2? (usb_pipeint (pipe)? "INTR": "ISOC"):
		 		(usb_pipecontrol (pipe)? "CTRL": "BULK"),
		 	urb->transfer_flags, 
		 	urb->actual_length, 
		 	urb->transfer_buffer_length,
		 	urb->status, urb->status);
#ifdef	OHCI_VERBOSE_DEBUG
	if (!small) {
		int i, len;

		if (usb_pipecontrol (pipe)) {
			printk (KERN_DEBUG __FILE__ ": cmd(8):");
			for (i = 0; i < 8 ; i++) 
				printk (" %02x", ((__u8 *) urb->setup_packet) [i]);
			printk ("\n");
		}
		if (urb->transfer_buffer_length > 0 && urb->transfer_buffer) {
			printk (KERN_DEBUG __FILE__ ": data(%d/%d):", 
				urb->actual_length, 
				urb->transfer_buffer_length);
			len = usb_pipeout (pipe)? 
						urb->transfer_buffer_length: urb->actual_length;
			for (i = 0; i < 16 && i < len; i++) 
				printk (" %02x", ((__u8 *) urb->transfer_buffer) [i]);
			printk ("%s stat:%d\n", i < len? "...": "", urb->status);
		}
	} 
#endif
}

/* just for debugging; prints non-empty branches of the int ed tree inclusive iso eds*/
void ep_print_int_eds (ohci_t * ohci, char * str) {
	int i, j;
	 __u32 * ed_p;
	for (i= 0; i < 32; i++) {
		j = 5;
		ed_p = &(ohci->hcca->int_table [i]);
		if (*ed_p == 0)
		    continue;
		printk (KERN_DEBUG __FILE__ ": %s branch int %2d(%2x):", str, i, i);
		while (*ed_p != 0 && j--) {
			ed_t *ed = dma_to_ed (ohci, le32_to_cpup(ed_p));
			printk (" ed: %4x;", ed->hwINFO);
			ed_p = &ed->hwNextED;
		}
		printk ("\n");
	}
}


static void ohci_dump_intr_mask (char *label, __u32 mask)
{
	dbg ("%s: 0x%08x%s%s%s%s%s%s%s%s%s",
		label,
		mask,
		(mask & OHCI_INTR_MIE) ? " MIE" : "",
		(mask & OHCI_INTR_OC) ? " OC" : "",
		(mask & OHCI_INTR_RHSC) ? " RHSC" : "",
		(mask & OHCI_INTR_FNO) ? " FNO" : "",
		(mask & OHCI_INTR_UE) ? " UE" : "",
		(mask & OHCI_INTR_RD) ? " RD" : "",
		(mask & OHCI_INTR_SF) ? " SF" : "",
		(mask & OHCI_INTR_WDH) ? " WDH" : "",
		(mask & OHCI_INTR_SO) ? " SO" : ""
		);
}

static void maybe_print_eds (char *label, __u32 value)
{
	if (value)
		dbg ("%s %08x", label, value);
}

static char *hcfs2string (int state)
{
	switch (state) {
		case OHCI_USB_RESET:	return "reset";
		case OHCI_USB_RESUME:	return "resume";
		case OHCI_USB_OPER:	return "operational";
		case OHCI_USB_SUSPEND:	return "suspend";
	}
	return "?";
}

// dump control and status registers
static void ohci_dump_status (ohci_t *controller)
{
	struct ohci_regs	*regs = controller->regs;
	__u32			temp;

	temp = readl (&regs->revision) & 0xff;
	if (temp != 0x10)
		dbg ("spec %d.%d", (temp >> 4), (temp & 0x0f));

	temp = readl (&regs->control);
	dbg ("control: 0x%08x%s%s%s HCFS=%s%s%s%s%s CBSR=%d", temp,
		(temp & OHCI_CTRL_RWE) ? " RWE" : "",
		(temp & OHCI_CTRL_RWC) ? " RWC" : "",
		(temp & OHCI_CTRL_IR) ? " IR" : "",
		hcfs2string (temp & OHCI_CTRL_HCFS),
		(temp & OHCI_CTRL_BLE) ? " BLE" : "",
		(temp & OHCI_CTRL_CLE) ? " CLE" : "",
		(temp & OHCI_CTRL_IE) ? " IE" : "",
		(temp & OHCI_CTRL_PLE) ? " PLE" : "",
		temp & OHCI_CTRL_CBSR
		);

	temp = readl (&regs->cmdstatus);
	dbg ("cmdstatus: 0x%08x SOC=%d%s%s%s%s", temp,
		(temp & OHCI_SOC) >> 16,
		(temp & OHCI_OCR) ? " OCR" : "",
		(temp & OHCI_BLF) ? " BLF" : "",
		(temp & OHCI_CLF) ? " CLF" : "",
		(temp & OHCI_HCR) ? " HCR" : ""
		);

	ohci_dump_intr_mask ("intrstatus", readl (&regs->intrstatus));
	ohci_dump_intr_mask ("intrenable", readl (&regs->intrenable));
	// intrdisable always same as intrenable
	// ohci_dump_intr_mask ("intrdisable", readl (&regs->intrdisable));

	maybe_print_eds ("ed_periodcurrent", readl (&regs->ed_periodcurrent));

	maybe_print_eds ("ed_controlhead", readl (&regs->ed_controlhead));
	maybe_print_eds ("ed_controlcurrent", readl (&regs->ed_controlcurrent));

	maybe_print_eds ("ed_bulkhead", readl (&regs->ed_bulkhead));
	maybe_print_eds ("ed_bulkcurrent", readl (&regs->ed_bulkcurrent));

	maybe_print_eds ("donehead", readl (&regs->donehead));
}

static void ohci_dump_roothub (ohci_t *controller, int verbose)
{
	__u32			temp, ndp, i;

	temp = roothub_a (controller);
	if (temp == ~(u32)0)
		return;
	ndp = (temp & RH_A_NDP);

	if (verbose) {
		dbg ("roothub.a: %08x POTPGT=%d%s%s%s%s%s NDP=%d", temp,
			((temp & RH_A_POTPGT) >> 24) & 0xff,
			(temp & RH_A_NOCP) ? " NOCP" : "",
			(temp & RH_A_OCPM) ? " OCPM" : "",
			(temp & RH_A_DT) ? " DT" : "",
			(temp & RH_A_NPS) ? " NPS" : "",
			(temp & RH_A_PSM) ? " PSM" : "",
			ndp
			);
		temp = roothub_b (controller);
		dbg ("roothub.b: %08x PPCM=%04x DR=%04x",
			temp,
			(temp & RH_B_PPCM) >> 16,
			(temp & RH_B_DR)
			);
		temp = roothub_status (controller);
		dbg ("roothub.status: %08x%s%s%s%s%s%s",
			temp,
			(temp & RH_HS_CRWE) ? " CRWE" : "",
			(temp & RH_HS_OCIC) ? " OCIC" : "",
			(temp & RH_HS_LPSC) ? " LPSC" : "",
			(temp & RH_HS_DRWE) ? " DRWE" : "",
			(temp & RH_HS_OCI) ? " OCI" : "",
			(temp & RH_HS_LPS) ? " LPS" : ""
			);
	}
	
	for (i = 0; i < ndp; i++) {
		temp = roothub_portstatus (controller, i);
		dbg ("roothub.portstatus [%d] = 0x%08x%s%s%s%s%s%s%s%s%s%s%s%s",
			i,
			temp,
			(temp & RH_PS_PRSC) ? " PRSC" : "",
			(temp & RH_PS_OCIC) ? " OCIC" : "",
			(temp & RH_PS_PSSC) ? " PSSC" : "",
			(temp & RH_PS_PESC) ? " PESC" : "",
			(temp & RH_PS_CSC) ? " CSC" : "",

			(temp & RH_PS_LSDA) ? " LSDA" : "",
			(temp & RH_PS_PPS) ? " PPS" : "",
			(temp & RH_PS_PRS) ? " PRS" : "",
			(temp & RH_PS_POCI) ? " POCI" : "",
			(temp & RH_PS_PSS) ? " PSS" : "",

			(temp & RH_PS_PES) ? " PES" : "",
			(temp & RH_PS_CCS) ? " CCS" : ""
			);
	}
}

static void ohci_dump (ohci_t *controller, int verbose)
{
	dbg ("OHCI controller usb-%s state", controller->ohci_dev->slot_name);

	// dumps some of the state we know about
	ohci_dump_status (controller);
	if (verbose)
		ep_print_int_eds (controller, "hcca");
	dbg ("hcca frame #%04x", controller->hcca->frame_no);
	ohci_dump_roothub (controller, 1);
}


#endif

/*-------------------------------------------------------------------------*
 * Interface functions (URB)
 *-------------------------------------------------------------------------*/

/* return a request to the completion handler */
 
static int sohci_return_urb (struct ohci *hc, struct urb * urb)
{
	urb_priv_t * urb_priv = urb->hcpriv;
	struct urb * urbt;
	int i;
	
	if (!urb_priv)
		return -1; /* urb already unlinked */

	/* just to be sure */
	if (!urb->complete) {
		urb_rm_priv_locked (urb);
		ohci_complete_add(hc, urb);	/* Just usb_dec_dev_use */
		return -1;
	}
	
#ifdef DEBUG
	urb_print (urb, "RET", usb_pipeout (urb->pipe));
#endif

	switch (usb_pipetype (urb->pipe)) {
  		case PIPE_INTERRUPT:
			pci_unmap_single (hc->ohci_dev,
				urb_priv->td [0]->data_dma,
				urb->transfer_buffer_length,
				usb_pipeout (urb->pipe)
					? PCI_DMA_TODEVICE
					: PCI_DMA_FROMDEVICE);
			if (urb->interval) {
				urb->complete (urb);

				/* implicitly requeued */
				urb->actual_length = 0;
				urb->status = -EINPROGRESS;
				td_submit_urb (urb);
			} else {
				urb_rm_priv_locked (urb);
				ohci_complete_add(hc, urb);
			}
  			break;
  			
		case PIPE_ISOCHRONOUS:
			for (urbt = urb->next; urbt && (urbt != urb); urbt = urbt->next);
			if (urbt) { /* send the reply and requeue URB */	
				pci_unmap_single (hc->ohci_dev,
					urb_priv->td [0]->data_dma,
					urb->transfer_buffer_length,
					usb_pipeout (urb->pipe)
						? PCI_DMA_TODEVICE
						: PCI_DMA_FROMDEVICE);
				urb->complete (urb);
				urb->actual_length = 0;
  				urb->status = USB_ST_URB_PENDING;
  				urb->start_frame = urb_priv->ed->last_iso + 1;
  				if (urb_priv->state != URB_DEL) {
  					for (i = 0; i < urb->number_of_packets; i++) {
  						urb->iso_frame_desc[i].actual_length = 0;
  						urb->iso_frame_desc[i].status = -EXDEV;
  					}
  					td_submit_urb (urb);
  				}

  			} else { /* unlink URB, call complete */
				urb_rm_priv_locked (urb);
				ohci_complete_add(hc, urb);
			}		
			break;
  				
		case PIPE_BULK:
		case PIPE_CONTROL: /* unlink URB, call complete */
			urb_rm_priv_locked (urb);
			ohci_complete_add(hc, urb);
			break;
	}
	return 0;
}

/*-------------------------------------------------------------------------*/

/* get a transfer request */
 
static int sohci_submit_urb (struct urb * urb)
{
	ohci_t * ohci;
	ed_t * ed;
	urb_priv_t * urb_priv;
	unsigned int pipe = urb->pipe;
	int maxps = usb_maxpacket (urb->dev, pipe, usb_pipeout (pipe));
	int i, size = 0;
	unsigned long flags;
	int bustime = 0;
	int mem_flags = GFP_ATOMIC;
	
	if (!urb->dev || !urb->dev->bus)
		return -ENODEV;
	
	if (urb->hcpriv)			/* urb already in use */
		return -EINVAL;

//	if(usb_endpoint_halted (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe))) 
//		return -EPIPE;
	
	usb_inc_dev_use (urb->dev);
	ohci = (ohci_t *) urb->dev->bus->hcpriv;
	
#ifdef DEBUG
	urb_print (urb, "SUB", usb_pipein (pipe));
#endif

	/* handle a request to the virtual root hub */
	if (usb_pipedevice (pipe) == ohci->rh.devnum) 
		return rh_submit_urb (urb);

	spin_lock_irqsave(&ohci->ohci_lock, flags);

	/* when controller's hung, permit only roothub cleanup attempts
	 * such as powering down ports */
	if (ohci->disabled) {
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
		usb_dec_dev_use (urb->dev);	
		return -ESHUTDOWN;
	}

	/* every endpoint has a ed, locate and fill it */
	if (!(ed = ep_add_ed (urb->dev, pipe, urb->interval, 1, mem_flags))) {
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
		usb_dec_dev_use (urb->dev);	
		return -ENOMEM;
	}

	/* for the private part of the URB we need the number of TDs (size) */
	switch (usb_pipetype (pipe)) {
		case PIPE_BULK:	/* one TD for every 4096 Byte */
			size = (urb->transfer_buffer_length - 1) / 4096 + 1;

			/* If the transfer size is multiple of the pipe mtu,
			 * we may need an extra TD to create a empty frame
			 * Jean II */
			if ((urb->transfer_flags & USB_ZERO_PACKET) &&
			    usb_pipeout (pipe) &&
			    (urb->transfer_buffer_length != 0) && 
			    ((urb->transfer_buffer_length % maxps) == 0))
				size++;
			break;
		case PIPE_ISOCHRONOUS: /* number of packets from URB */
			size = urb->number_of_packets;
			if (size <= 0) {
				spin_unlock_irqrestore(&ohci->ohci_lock, flags);
				usb_dec_dev_use (urb->dev);	
				return -EINVAL;
			}
			for (i = 0; i < urb->number_of_packets; i++) {
  				urb->iso_frame_desc[i].actual_length = 0;
  				urb->iso_frame_desc[i].status = -EXDEV;
  			}
			break;
		case PIPE_CONTROL: /* 1 TD for setup, 1 for ACK and 1 for every 4096 B */
			size = (urb->transfer_buffer_length == 0)? 2: 
						(urb->transfer_buffer_length - 1) / 4096 + 3;
			break;
		case PIPE_INTERRUPT: /* one TD */
			size = 1;
			break;
	}

	/* allocate the private part of the URB */
	urb_priv = kmalloc (sizeof (urb_priv_t) + size * sizeof (td_t *), 
							GFP_ATOMIC);
	if (!urb_priv) {
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
		usb_dec_dev_use (urb->dev);	
		return -ENOMEM;
	}
	memset (urb_priv, 0, sizeof (urb_priv_t) + size * sizeof (td_t *));
	
	/* fill the private part of the URB */
	urb_priv->length = size;
	urb_priv->ed = ed;	

	/* allocate the TDs (updating hash chains) */
	for (i = 0; i < size; i++) { 
		urb_priv->td[i] = td_alloc (ohci, SLAB_ATOMIC);
		if (!urb_priv->td[i]) {
			urb_priv->length = i;
			urb_free_priv (ohci, urb_priv);
			spin_unlock_irqrestore(&ohci->ohci_lock, flags);
			usb_dec_dev_use (urb->dev);	
			return -ENOMEM;
		}
	}	

	if (ed->state == ED_NEW || (ed->state & ED_DEL)) {
		urb_free_priv (ohci, urb_priv);
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
		usb_dec_dev_use (urb->dev);	
		return -EINVAL;
	}
	
	/* allocate and claim bandwidth if needed; ISO
	 * needs start frame index if it was't provided.
	 */
	switch (usb_pipetype (pipe)) {
		case PIPE_ISOCHRONOUS:
			if (urb->transfer_flags & USB_ISO_ASAP) { 
				urb->start_frame = ((ed->state == ED_OPER)
					? (ed->last_iso + 1)
					: (le16_to_cpu (ohci->hcca->frame_no) + 10)) & 0xffff;
			}	
			/* FALLTHROUGH */
		case PIPE_INTERRUPT:
			if (urb->bandwidth == 0) {
				bustime = usb_check_bandwidth (urb->dev, urb);
			}
			if (bustime < 0) {
				urb_free_priv (ohci, urb_priv);
				spin_unlock_irqrestore(&ohci->ohci_lock, flags);
				usb_dec_dev_use (urb->dev);	
				return bustime;
			}
			usb_claim_bandwidth (urb->dev, urb, bustime, usb_pipeisoc (urb->pipe));
#ifdef	DO_TIMEOUTS
			urb->timeout = 0;
#endif
	}

	urb->actual_length = 0;
	urb->hcpriv = urb_priv;
	urb->status = USB_ST_URB_PENDING;

	/* link the ed into a chain if is not already */
	if (ed->state != ED_OPER)
		ep_link (ohci, ed);

	/* fill the TDs and link it to the ed */
	td_submit_urb (urb);

#ifdef	DO_TIMEOUTS
	/* maybe add to ordered list of timeouts */
	if (urb->timeout) {
		struct list_head	*entry;

		urb->timeout += jiffies;

		list_for_each (entry, &ohci->timeout_list) {
			struct urb	*next_urb;

			next_urb = list_entry (entry, struct urb, urb_list);
			if (time_after_eq (urb->timeout, next_urb->timeout))
				break;
		}
		list_add (&urb->urb_list, entry);

		/* drive timeouts by SF (messy, but works) */
		writel (OHCI_INTR_SF, &ohci->regs->intrenable);	
		(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
	}
#endif

	spin_unlock_irqrestore(&ohci->ohci_lock, flags);

	return 0;	
}

/*-------------------------------------------------------------------------*/

/* deactivate all TDs and remove the private part of the URB */
/* interrupt callers must use async unlink mode */

static int sohci_unlink_urb (struct urb * urb)
{
	unsigned long flags;
	ohci_t * ohci;
	
	if (!urb) /* just to be sure */ 
		return -EINVAL;
		
	if (!urb->dev || !urb->dev->bus)
		return -ENODEV;

	ohci = (ohci_t *) urb->dev->bus->hcpriv; 

#ifdef DEBUG
	urb_print (urb, "UNLINK", 1);
#endif		  

	/* handle a request to the virtual root hub */
	if (usb_pipedevice (urb->pipe) == ohci->rh.devnum)
		return rh_unlink_urb (urb);

	spin_lock_irqsave(&ohci->ohci_lock, flags);
	if (urb->hcpriv && (urb->status == USB_ST_URB_PENDING)) { 
		if (!ohci->disabled) {
			urb_priv_t  * urb_priv;

			/* interrupt code may not sleep; it must use
			 * async status return to unlink pending urbs.
			 */
			if (!(urb->transfer_flags & USB_ASYNC_UNLINK)
					&& in_interrupt ()) {
				spin_unlock_irqrestore(&ohci->ohci_lock, flags);
				err ("bug in call from %p; use async!",
					__builtin_return_address(0));
				return -EWOULDBLOCK;
			}

			/* flag the urb and its TDs for deletion in some
			 * upcoming SF interrupt delete list processing
			 */
			urb_priv = urb->hcpriv;

			if (!urb_priv || (urb_priv->state == URB_DEL)) {
				spin_unlock_irqrestore(&ohci->ohci_lock, flags);
				return 0;
			}
				
			urb_priv->state = URB_DEL; 
			ep_rm_ed (urb->dev, urb_priv->ed);
			urb_priv->ed->state |= ED_URB_DEL;

			if (!(urb->transfer_flags & USB_ASYNC_UNLINK)) {
				DECLARE_WAIT_QUEUE_HEAD (unlink_wakeup); 
				DECLARE_WAITQUEUE (wait, current);
				int timeout = OHCI_UNLINK_TIMEOUT;

				add_wait_queue (&unlink_wakeup, &wait);
				urb_priv->wait = &unlink_wakeup;
				spin_unlock_irqrestore(&ohci->ohci_lock, flags);

				/* wait until all TDs are deleted */
				set_current_state(TASK_UNINTERRUPTIBLE);
				while (timeout && (urb->status == USB_ST_URB_PENDING)) {
					timeout = schedule_timeout (timeout);
					set_current_state(TASK_UNINTERRUPTIBLE);
				}
				set_current_state(TASK_RUNNING);
				remove_wait_queue (&unlink_wakeup, &wait); 
				if (urb->status == USB_ST_URB_PENDING) {
					err ("unlink URB timeout");
					return -ETIMEDOUT;
				}

				usb_dec_dev_use (urb->dev);
				urb->dev = NULL;
				if (urb->complete)
					urb->complete (urb); 
			} else {
				/* usb_dec_dev_use done in dl_del_list() */
				urb->status = -EINPROGRESS;
				spin_unlock_irqrestore(&ohci->ohci_lock, flags);
				return -EINPROGRESS;
			}
		} else {
			urb_rm_priv_locked (urb);
			spin_unlock_irqrestore(&ohci->ohci_lock, flags);
			usb_dec_dev_use (urb->dev);
			urb->dev = NULL;
			if (urb->transfer_flags & USB_ASYNC_UNLINK) {
				urb->status = -ECONNRESET;
				if (urb->complete)
					urb->complete (urb); 
			} else 
				urb->status = -ENOENT;
		}	
	} else {
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
	}	
	return 0;
}

/*-------------------------------------------------------------------------*/

/* allocate private data space for a usb device */

static int sohci_alloc_dev (struct usb_device *usb_dev)
{
	struct ohci_device * dev;

	dev = dev_alloc ((struct ohci *) usb_dev->bus->hcpriv, ALLOC_FLAGS);
	if (!dev)
		return -ENOMEM;

	usb_dev->hcpriv = dev;
	return 0;
}

/*-------------------------------------------------------------------------*/

/* may be called from interrupt context */
/* frees private data space of usb device */
  
static int sohci_free_dev (struct usb_device * usb_dev)
{
	unsigned long flags;
	int i, cnt = 0;
	ed_t * ed;
	struct ohci_device * dev = usb_to_ohci (usb_dev);
	ohci_t * ohci = usb_dev->bus->hcpriv;
	
	if (!dev)
		return 0;
	
	if (usb_dev->devnum >= 0) {
	
		/* driver disconnects should have unlinked all urbs
		 * (freeing all the TDs, unlinking EDs) but we need
		 * to defend against bugs that prevent that.
		 */
		spin_lock_irqsave(&ohci->ohci_lock, flags);	
		for(i = 0; i < NUM_EDS; i++) {
  			ed = &(dev->ed[i]);
  			if (ed->state != ED_NEW) {
  				if (ed->state == ED_OPER) {
					/* driver on that interface didn't unlink an urb */
					dbg ("driver usb-%s dev %d ed 0x%x unfreed URB",
						ohci->ohci_dev->slot_name, usb_dev->devnum, i);
					ep_unlink (ohci, ed);
				}
  				ep_rm_ed (usb_dev, ed);
  				ed->state = ED_DEL;
  				cnt++;
  			}
  		}
  		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
  		
		/* if the controller is running, tds for those unlinked
		 * urbs get freed by dl_del_list at the next SF interrupt
		 */
		if (cnt > 0) {

			if (ohci->disabled) {
				/* FIXME: Something like this should kick in,
				 * though it's currently an exotic case ...
				 * the controller won't ever be touching
				 * these lists again!!
				dl_del_list (ohci,
					le16_to_cpu (ohci->hcca->frame_no) & 1);
				 */
				warn ("TD leak, %d", cnt);

			} else if (!in_interrupt ()) {
				DECLARE_WAIT_QUEUE_HEAD (freedev_wakeup); 
				DECLARE_WAITQUEUE (wait, current);
				int timeout = OHCI_UNLINK_TIMEOUT;

				/* SF interrupt handler calls dl_del_list */
				add_wait_queue (&freedev_wakeup, &wait);
				dev->wait = &freedev_wakeup;
				set_current_state(TASK_UNINTERRUPTIBLE);
				while (timeout && dev->ed_cnt)
					timeout = schedule_timeout (timeout);
				set_current_state(TASK_RUNNING);
				remove_wait_queue (&freedev_wakeup, &wait);
				if (dev->ed_cnt) {
					err ("free device %d timeout", usb_dev->devnum);
					return -ETIMEDOUT;
				}
			} else {
				/* likely some interface's driver has a refcount bug */
				err ("bus %s devnum %d deletion in interrupt",
					ohci->ohci_dev->slot_name, usb_dev->devnum);
				BUG ();
			}
		}
	}

	/* free device, and associated EDs */
	dev_free (ohci, dev);

	return 0;
}

/*-------------------------------------------------------------------------*/

/* tell us the current USB frame number */

static int sohci_get_current_frame_number (struct usb_device *usb_dev) 
{
	ohci_t * ohci = usb_dev->bus->hcpriv;
	
	return le16_to_cpu (ohci->hcca->frame_no);
}

/*-------------------------------------------------------------------------*/

struct usb_operations sohci_device_operations = {
	sohci_alloc_dev,
	sohci_free_dev,
	sohci_get_current_frame_number,
	sohci_submit_urb,
	sohci_unlink_urb
};

/*-------------------------------------------------------------------------*
 * ED handling functions
 *-------------------------------------------------------------------------*/  
		
/* search for the right branch to insert an interrupt ed into the int tree 
 * do some load ballancing;
 * returns the branch and 
 * sets the interval to interval = 2^integer (ld (interval)) */

static int ep_int_ballance (ohci_t * ohci, int interval, int load)
{
	int i, branch = 0;
   
	/* search for the least loaded interrupt endpoint branch of all 32 branches */
	for (i = 0; i < 32; i++) 
		if (ohci->ohci_int_load [branch] > ohci->ohci_int_load [i]) branch = i; 
  
	branch = branch % interval;
	for (i = branch; i < 32; i += interval) ohci->ohci_int_load [i] += load;

	return branch;
}

/*-------------------------------------------------------------------------*/

/*  2^int( ld (inter)) */

static int ep_2_n_interval (int inter)
{	
	int i;
	for (i = 0; ((inter >> i) > 1 ) && (i < 5); i++); 
	return 1 << i;
}

/*-------------------------------------------------------------------------*/

/* the int tree is a binary tree 
 * in order to process it sequentially the indexes of the branches have to be mapped 
 * the mapping reverses the bits of a word of num_bits length */
 
static int ep_rev (int num_bits, int word)
{
	int i, wout = 0;

	for (i = 0; i < num_bits; i++) wout |= (((word >> i) & 1) << (num_bits - i - 1));
	return wout;
}

/*-------------------------------------------------------------------------*/

/* link an ed into one of the HC chains */

static int ep_link (ohci_t * ohci, ed_t * edi)
{	 
	int int_branch;
	int i;
	int inter;
	int interval;
	int load;
	__u32 * ed_p;
	volatile ed_t * ed = edi;
	
	ed->state = ED_OPER;
	
	switch (ed->type) {
	case PIPE_CONTROL:
		ed->hwNextED = 0;
		if (ohci->ed_controltail == NULL) {
			writel (ed->dma, &ohci->regs->ed_controlhead);
		} else {
			ohci->ed_controltail->hwNextED = cpu_to_le32 (ed->dma);
		}
		ed->ed_prev = ohci->ed_controltail;
		if (!ohci->ed_controltail && !ohci->ed_rm_list[0] &&
			!ohci->ed_rm_list[1] && !ohci->sleeping) {
			ohci->hc_control |= OHCI_CTRL_CLE;
			writel (ohci->hc_control, &ohci->regs->control);
		}
		ohci->ed_controltail = edi;	  
		break;
		
	case PIPE_BULK:
		ed->hwNextED = 0;
		if (ohci->ed_bulktail == NULL) {
			writel (ed->dma, &ohci->regs->ed_bulkhead);
		} else {
			ohci->ed_bulktail->hwNextED = cpu_to_le32 (ed->dma);
		}
		ed->ed_prev = ohci->ed_bulktail;
		if (!ohci->ed_bulktail && !ohci->ed_rm_list[0] &&
			!ohci->ed_rm_list[1] && !ohci->sleeping) {
			ohci->hc_control |= OHCI_CTRL_BLE;
			writel (ohci->hc_control, &ohci->regs->control);
		}
		ohci->ed_bulktail = edi;	  
		break;
		
	case PIPE_INTERRUPT:
		load = ed->int_load;
		interval = ep_2_n_interval (ed->int_period);
		ed->int_interval = interval;
		int_branch = ep_int_ballance (ohci, interval, load);
		ed->int_branch = int_branch;
		
		for (i = 0; i < ep_rev (6, interval); i += inter) {
			inter = 1;
			for (ed_p = &(ohci->hcca->int_table[ep_rev (5, i) + int_branch]); 
				(*ed_p != 0) && ((dma_to_ed (ohci, le32_to_cpup (ed_p)))->int_interval >= interval); 
				ed_p = &((dma_to_ed (ohci, le32_to_cpup (ed_p)))->hwNextED)) 
					inter = ep_rev (6, (dma_to_ed (ohci, le32_to_cpup (ed_p)))->int_interval);
			ed->hwNextED = *ed_p; 
			*ed_p = cpu_to_le32 (ed->dma);
		}
#ifdef DEBUG
		ep_print_int_eds (ohci, "LINK_INT");
#endif
		break;
		
	case PIPE_ISOCHRONOUS:
		ed->hwNextED = 0;
		ed->int_interval = 1;
		if (ohci->ed_isotail != NULL) {
			ohci->ed_isotail->hwNextED = cpu_to_le32 (ed->dma);
			ed->ed_prev = ohci->ed_isotail;
		} else {
			for ( i = 0; i < 32; i += inter) {
				inter = 1;
				for (ed_p = &(ohci->hcca->int_table[ep_rev (5, i)]); 
					*ed_p != 0; 
					ed_p = &((dma_to_ed (ohci, le32_to_cpup (ed_p)))->hwNextED)) 
						inter = ep_rev (6, (dma_to_ed (ohci, le32_to_cpup (ed_p)))->int_interval);
				*ed_p = cpu_to_le32 (ed->dma);	
			}	
			ed->ed_prev = NULL;
		}	
		ohci->ed_isotail = edi;  
#ifdef DEBUG
		ep_print_int_eds (ohci, "LINK_ISO");
#endif
		break;
	}	 	
	return 0;
}

/*-------------------------------------------------------------------------*/

/* scan the periodic table to find and unlink this ED */
static void periodic_unlink (
	struct ohci	*ohci,
	struct ed	*ed,
	unsigned	index,
	unsigned	period
) {
	for (; index < NUM_INTS; index += period) {
		__u32	*ed_p = &ohci->hcca->int_table [index];

		/* ED might have been unlinked through another path */
		while (*ed_p != 0) {
			if ((dma_to_ed (ohci, le32_to_cpup (ed_p))) == ed) {
				*ed_p = ed->hwNextED;		
				break;
			}
			ed_p = & ((dma_to_ed (ohci,
				le32_to_cpup (ed_p)))->hwNextED);
		}
	}	
}

/* unlink an ed from one of the HC chains. 
 * just the link to the ed is unlinked.
 * the link from the ed still points to another operational ed or 0
 * so the HC can eventually finish the processing of the unlinked ed */

static int ep_unlink (ohci_t * ohci, ed_t * ed) 
{
	int i;

	ed->hwINFO |= cpu_to_le32 (OHCI_ED_SKIP);

	switch (ed->type) {
	case PIPE_CONTROL:
		if (ed->ed_prev == NULL) {
			if (!ed->hwNextED) {
				ohci->hc_control &= ~OHCI_CTRL_CLE;
				writel (ohci->hc_control, &ohci->regs->control);
			}
			writel (le32_to_cpup (&ed->hwNextED), &ohci->regs->ed_controlhead);
		} else {
			ed->ed_prev->hwNextED = ed->hwNextED;
		}
		if (ohci->ed_controltail == ed) {
			ohci->ed_controltail = ed->ed_prev;
		} else {
			(dma_to_ed (ohci, le32_to_cpup (&ed->hwNextED)))->ed_prev = ed->ed_prev;
		}
		break;
      
	case PIPE_BULK:
		if (ed->ed_prev == NULL) {
			if (!ed->hwNextED) {
				ohci->hc_control &= ~OHCI_CTRL_BLE;
				writel (ohci->hc_control, &ohci->regs->control);
			}
			writel (le32_to_cpup (&ed->hwNextED), &ohci->regs->ed_bulkhead);
		} else {
			ed->ed_prev->hwNextED = ed->hwNextED;
		}
		if (ohci->ed_bulktail == ed) {
			ohci->ed_bulktail = ed->ed_prev;
		} else {
			(dma_to_ed (ohci, le32_to_cpup (&ed->hwNextED)))->ed_prev = ed->ed_prev;
		}
		break;
      
	case PIPE_INTERRUPT:
		periodic_unlink (ohci, ed, 0, 1);
		for (i = ed->int_branch; i < 32; i += ed->int_interval)
		    ohci->ohci_int_load[i] -= ed->int_load;
#ifdef DEBUG
		ep_print_int_eds (ohci, "UNLINK_INT");
#endif
		break;
		
	case PIPE_ISOCHRONOUS:
		if (ohci->ed_isotail == ed)
			ohci->ed_isotail = ed->ed_prev;
		if (ed->hwNextED != 0) 
		    (dma_to_ed (ohci, le32_to_cpup (&ed->hwNextED)))
		    	->ed_prev = ed->ed_prev;
				    
		if (ed->ed_prev != NULL)
			ed->ed_prev->hwNextED = ed->hwNextED;
		else
			periodic_unlink (ohci, ed, 0, 1);
#ifdef DEBUG
		ep_print_int_eds (ohci, "UNLINK_ISO");
#endif
		break;
	}
	ed->state = ED_UNLINK;
	return 0;
}


/*-------------------------------------------------------------------------*/

/* add/reinit an endpoint; this should be done once at the usb_set_configuration command,
 * but the USB stack is a little bit stateless  so we do it at every transaction
 * if the state of the ed is ED_NEW then a dummy td is added and the state is changed to ED_UNLINK
 * in all other cases the state is left unchanged
 * the ed info fields are setted anyway even though most of them should not change */
 
static ed_t * ep_add_ed (
	struct usb_device * usb_dev,
	unsigned int pipe,
	int interval,
	int load,
	int mem_flags
)
{
   	ohci_t * ohci = usb_dev->bus->hcpriv;
	td_t * td;
	ed_t * ed_ret;
	volatile ed_t * ed; 

	ed = ed_ret = &(usb_to_ohci (usb_dev)->ed[(usb_pipeendpoint (pipe) << 1) | 
			(usb_pipecontrol (pipe)? 0: usb_pipeout (pipe))]);

	if ((ed->state & ED_DEL) || (ed->state & ED_URB_DEL)) {
		/* pending delete request */
		return NULL;
	}
	
	if (ed->state == ED_NEW) {
		ed->hwINFO = cpu_to_le32 (OHCI_ED_SKIP); /* skip ed */
  		/* dummy td; end of td list for ed */
		td = td_alloc (ohci, SLAB_ATOMIC);
		/* hash the ed for later reverse mapping */
 		if (!td || !hash_add_ed (ohci, (ed_t *)ed)) {
			/* out of memory */
		        if (td)
		            td_free(ohci, td);
			return NULL;
		}
		ed->hwTailP = cpu_to_le32 (td->td_dma);
		ed->hwHeadP = ed->hwTailP;	
		ed->state = ED_UNLINK;
		ed->type = usb_pipetype (pipe);
		usb_to_ohci (usb_dev)->ed_cnt++;
	}

	ohci->dev[usb_pipedevice (pipe)] = usb_dev;
	
	ed->hwINFO = cpu_to_le32 (usb_pipedevice (pipe)
			| usb_pipeendpoint (pipe) << 7
			| (usb_pipeisoc (pipe)? 0x8000: 0)
			| (usb_pipecontrol (pipe)? 0: (usb_pipeout (pipe)? 0x800: 0x1000)) 
			| usb_pipeslow (pipe) << 13
			| usb_maxpacket (usb_dev, pipe, usb_pipeout (pipe)) << 16);
  
  	if (ed->type == PIPE_INTERRUPT && ed->state == ED_UNLINK) {
  		ed->int_period = interval;
  		ed->int_load = load;
  	}

	return ed_ret; 
}

/*-------------------------------------------------------------------------*/
 
/* request the removal of an endpoint
 * put the ep on the rm_list and request a stop of the bulk or ctrl list 
 * real removal is done at the next start frame (SF) hardware interrupt */
 
static void ep_rm_ed (struct usb_device * usb_dev, ed_t * ed)
{    
	unsigned int frame;
	ohci_t * ohci = usb_dev->bus->hcpriv;

	if ((ed->state & ED_DEL) || (ed->state & ED_URB_DEL))
		return;
	
	ed->hwINFO |= cpu_to_le32 (OHCI_ED_SKIP);

	if (!ohci->disabled) {
		switch (ed->type) {
			case PIPE_CONTROL: /* stop control list */
				ohci->hc_control &= ~OHCI_CTRL_CLE;
				writel (ohci->hc_control, &ohci->regs->control); 
  				break;
			case PIPE_BULK: /* stop bulk list */
				ohci->hc_control &= ~OHCI_CTRL_BLE;
				writel (ohci->hc_control, &ohci->regs->control); 
				break;
		}
	}

	frame = le16_to_cpu (ohci->hcca->frame_no) & 0x1;
	ed->ed_rm_list = ohci->ed_rm_list[frame];
	ohci->ed_rm_list[frame] = ed;

	if (!ohci->disabled && !ohci->sleeping) {
		/* enable SOF interrupt */
		writel (OHCI_INTR_SF, &ohci->regs->intrstatus);
		writel (OHCI_INTR_SF, &ohci->regs->intrenable);
		(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
	}
}

/*-------------------------------------------------------------------------*
 * TD handling functions
 *-------------------------------------------------------------------------*/

/* enqueue next TD for this URB (OHCI spec 5.2.8.2) */

static void
td_fill (ohci_t * ohci, unsigned int info,
	dma_addr_t data, int len,
	struct urb * urb, int index)
{
	volatile td_t  * td, * td_pt;
	urb_priv_t * urb_priv = urb->hcpriv;
	
	if (index >= urb_priv->length) {
		err("internal OHCI error: TD index > length");
		return;
	}
	
	/* use this td as the next dummy */
	td_pt = urb_priv->td [index];
	td_pt->hwNextTD = 0;

	/* fill the old dummy TD */
	td = urb_priv->td [index] = dma_to_td (ohci,
			le32_to_cpup (&urb_priv->ed->hwTailP) & ~0xf);

	td->ed = urb_priv->ed;
	td->next_dl_td = NULL;
	td->index = index;
	td->urb = urb; 
	td->data_dma = data;
	if (!len)
		data = 0;

	td->hwINFO = cpu_to_le32 (info);
	if ((td->ed->type) == PIPE_ISOCHRONOUS) {
		td->hwCBP = cpu_to_le32 (data & 0xFFFFF000);
		td->ed->last_iso = info & 0xffff;
	} else {
		td->hwCBP = cpu_to_le32 (data); 
	}			
	if (data)
		td->hwBE = cpu_to_le32 (data + len - 1);
	else
		td->hwBE = 0;
	td->hwNextTD = cpu_to_le32 (td_pt->td_dma);
	td->hwPSW [0] = cpu_to_le16 ((data & 0x0FFF) | 0xE000);

	/* append to queue */
	wmb();
	td->ed->hwTailP = td->hwNextTD;
}

/*-------------------------------------------------------------------------*/
 
/* prepare all TDs of a transfer */

static void td_submit_urb (struct urb * urb)
{ 
	urb_priv_t * urb_priv = urb->hcpriv;
	ohci_t * ohci = (ohci_t *) urb->dev->bus->hcpriv;
	dma_addr_t data;
	int data_len = urb->transfer_buffer_length;
	int maxps = usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));
	int cnt = 0; 
	__u32 info = 0;
  	unsigned int toggle = 0;

	/* OHCI handles the DATA-toggles itself, we just use the USB-toggle bits for reseting */
  	if(usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe))) {
  		toggle = TD_T_TOGGLE;
	} else {
  		toggle = TD_T_DATA0;
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe), 1);
	}
	
	urb_priv->td_cnt = 0;

	if (data_len) {
		data = pci_map_single (ohci->ohci_dev,
			urb->transfer_buffer, data_len,
			usb_pipeout (urb->pipe)
				? PCI_DMA_TODEVICE
				: PCI_DMA_FROMDEVICE
			);
	} else
		data = 0;
	
	switch (usb_pipetype (urb->pipe)) {
		case PIPE_BULK:
			info = usb_pipeout (urb->pipe)? 
				TD_CC | TD_DP_OUT : TD_CC | TD_DP_IN ;
			while(data_len > 4096) {		
				td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), data, 4096, urb, cnt);
				data += 4096; data_len -= 4096; cnt++;
			}
			info = usb_pipeout (urb->pipe)?
				TD_CC | TD_DP_OUT : TD_CC | TD_R | TD_DP_IN ;
			td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), data, data_len, urb, cnt);
			cnt++;

			/* If the transfer size is multiple of the pipe mtu,
			 * we may need an extra TD to create a empty frame
			 * Note : another way to check this condition is
			 * to test if(urb_priv->length > cnt) - Jean II */
			if ((urb->transfer_flags & USB_ZERO_PACKET) &&
			    usb_pipeout (urb->pipe) &&
			    (urb->transfer_buffer_length != 0) && 
			    ((urb->transfer_buffer_length % maxps) == 0)) {
				td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), 0, 0, urb, cnt);
				cnt++;
			}

			if (!ohci->sleeping) {
				wmb();
				writel (OHCI_BLF, &ohci->regs->cmdstatus); /* start bulk list */
				(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
			}
			break;

		case PIPE_INTERRUPT:
			info = usb_pipeout (urb->pipe)? 
				TD_CC | TD_DP_OUT | toggle: TD_CC | TD_R | TD_DP_IN | toggle;
			td_fill (ohci, info, data, data_len, urb, cnt++);
			break;

		case PIPE_CONTROL:
			info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
			td_fill (ohci, info,
				pci_map_single (ohci->ohci_dev,
					urb->setup_packet, 8,
					PCI_DMA_TODEVICE),
				8, urb, cnt++); 
			if (data_len > 0) {  
				info = usb_pipeout (urb->pipe)? 
					TD_CC | TD_R | TD_DP_OUT | TD_T_DATA1 : TD_CC | TD_R | TD_DP_IN | TD_T_DATA1;
				/* NOTE:  mishandles transfers >8K, some >4K */
				td_fill (ohci, info, data, data_len, urb, cnt++);  
			} 
			info = usb_pipeout (urb->pipe)? 
 				TD_CC | TD_DP_IN | TD_T_DATA1: TD_CC | TD_DP_OUT | TD_T_DATA1;
			td_fill (ohci, info, data, 0, urb, cnt++);
			if (!ohci->sleeping) {
				wmb();
				writel (OHCI_CLF, &ohci->regs->cmdstatus); /* start Control list */
				(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
			}
			break;

		case PIPE_ISOCHRONOUS:
			for (cnt = 0; cnt < urb->number_of_packets; cnt++) {
				td_fill (ohci, TD_CC|TD_ISO | ((urb->start_frame + cnt) & 0xffff), 
					data + urb->iso_frame_desc[cnt].offset, 
					urb->iso_frame_desc[cnt].length, urb, cnt); 
			}
			break;
	} 
	if (urb_priv->length != cnt) 
		dbg("TD LENGTH %d != CNT %d", urb_priv->length, cnt);
}

/*-------------------------------------------------------------------------*
 * Done List handling functions
 *-------------------------------------------------------------------------*/


/* calculate the transfer length and update the urb */

static void dl_transfer_length(td_t * td)
{
	__u32 tdINFO, tdBE, tdCBP;
 	__u16 tdPSW;
 	struct urb * urb = td->urb;
 	urb_priv_t * urb_priv = urb->hcpriv;
	int dlen = 0;
	int cc = 0;
	
	tdINFO = le32_to_cpup (&td->hwINFO);
  	tdBE   = le32_to_cpup (&td->hwBE);
  	tdCBP  = le32_to_cpup (&td->hwCBP);


  	if (tdINFO & TD_ISO) {
 		tdPSW = le16_to_cpu (td->hwPSW[0]);
 		cc = (tdPSW >> 12) & 0xF;
		if (cc < 0xE)  {
			if (usb_pipeout(urb->pipe)) {
				dlen = urb->iso_frame_desc[td->index].length;
			} else {
				dlen = tdPSW & 0x3ff;
			}
			urb->actual_length += dlen;
			urb->iso_frame_desc[td->index].actual_length = dlen;
			if (!(urb->transfer_flags & USB_DISABLE_SPD) && (cc == TD_DATAUNDERRUN))
				cc = TD_CC_NOERROR;
					 
			urb->iso_frame_desc[td->index].status = cc_to_error[cc];
		}
	} else { /* BULK, INT, CONTROL DATA */
		if (!(usb_pipetype (urb->pipe) == PIPE_CONTROL && 
				((td->index == 0) || (td->index == urb_priv->length - 1)))) {
 			if (tdBE != 0) {
 				if (td->hwCBP == 0)
					urb->actual_length += tdBE - td->data_dma + 1;
  				else
					urb->actual_length += tdCBP - td->data_dma;
			}
  		}
  	}
}

/* handle an urb that is being unlinked */

static void dl_del_urb (ohci_t *ohci, struct urb * urb)
{
	wait_queue_head_t * wait_head = ((urb_priv_t *)(urb->hcpriv))->wait;

	urb_rm_priv_locked (urb);

	if (urb->transfer_flags & USB_ASYNC_UNLINK) {
		urb->status = -ECONNRESET;
		ohci_complete_add(ohci, urb);
	} else {
		urb->status = -ENOENT;

		/* unblock sohci_unlink_urb */
		if (wait_head)
			wake_up (wait_head);
	}
}

/*-------------------------------------------------------------------------*/

/* replies to the request have to be on a FIFO basis so
 * we reverse the reversed done-list */
 
static td_t * dl_reverse_done_list (ohci_t * ohci)
{
	__u32 td_list_hc;
	td_t * td_rev = NULL;
	td_t * td_list = NULL;
  	urb_priv_t * urb_priv = NULL;

	td_list_hc = le32_to_cpup (&ohci->hcca->done_head) & 0xfffffff0;
	ohci->hcca->done_head = 0;
	
	while (td_list_hc) {		
		td_list = dma_to_td (ohci, td_list_hc);

		if (TD_CC_GET (le32_to_cpup (&td_list->hwINFO))) {
			urb_priv = (urb_priv_t *) td_list->urb->hcpriv;
			dbg(" USB-error/status: %x : %p", 
					TD_CC_GET (le32_to_cpup (&td_list->hwINFO)), td_list);
			if (td_list->ed->hwHeadP & cpu_to_le32 (0x1)) {
				if (urb_priv && ((td_list->index + 1) < urb_priv->length)) {
					td_list->ed->hwHeadP = 
						(urb_priv->td[urb_priv->length - 1]->hwNextTD & cpu_to_le32 (0xfffffff0)) |
									(td_list->ed->hwHeadP & cpu_to_le32 (0x2));
					urb_priv->td_cnt += urb_priv->length - td_list->index - 1;
				} else 
					td_list->ed->hwHeadP &= cpu_to_le32 (0xfffffff2);
			}
		}

		td_list->next_dl_td = td_rev;	
		td_rev = td_list;
		td_list_hc = le32_to_cpup (&td_list->hwNextTD) & 0xfffffff0;	
	}	
	return td_list;
}

/*-------------------------------------------------------------------------*/

/* there are some pending requests to remove 
 * - some of the eds (if ed->state & ED_DEL (set by sohci_free_dev)
 * - some URBs/TDs if urb_priv->state == URB_DEL */
 
static void dl_del_list (ohci_t  * ohci, unsigned int frame)
{
	ed_t * ed;
	__u32 edINFO;
	__u32 tdINFO;
	td_t * td = NULL, * td_next = NULL, * tdHeadP = NULL, * tdTailP;
	__u32 * td_p;
	int ctrl = 0, bulk = 0;

	for (ed = ohci->ed_rm_list[frame]; ed != NULL; ed = ed->ed_rm_list) {

		tdTailP = dma_to_td (ohci, le32_to_cpup (&ed->hwTailP) & 0xfffffff0);
		tdHeadP = dma_to_td (ohci, le32_to_cpup (&ed->hwHeadP) & 0xfffffff0);
		edINFO = le32_to_cpup (&ed->hwINFO);
		td_p = &ed->hwHeadP;

		for (td = tdHeadP; td != tdTailP; td = td_next) { 
			struct urb * urb = td->urb;
			urb_priv_t * urb_priv = td->urb->hcpriv;
			
			td_next = dma_to_td (ohci, le32_to_cpup (&td->hwNextTD) & 0xfffffff0);
			if ((urb_priv->state == URB_DEL) || (ed->state & ED_DEL)) {
				tdINFO = le32_to_cpup (&td->hwINFO);
				if (TD_CC_GET (tdINFO) < 0xE)
					dl_transfer_length (td);
				*td_p = td->hwNextTD | (*td_p & cpu_to_le32 (0x3));

				/* URB is done; clean up */
				if (++(urb_priv->td_cnt) == urb_priv->length)
					dl_del_urb (ohci, urb);
			} else {
				td_p = &td->hwNextTD;
			}
		}

		if (ed->state & ED_DEL) { /* set by sohci_free_dev */
			struct ohci_device * dev = usb_to_ohci (ohci->dev[edINFO & 0x7F]);
			td_free (ohci, tdTailP); /* free dummy td */
   	 		ed->hwINFO = cpu_to_le32 (OHCI_ED_SKIP); 
			ed->state = ED_NEW;
			hash_free_ed(ohci, ed);
   	 		/* if all eds are removed wake up sohci_free_dev */
   	 		if (!--dev->ed_cnt) {
				wait_queue_head_t *wait_head = dev->wait;

				dev->wait = 0;
				if (wait_head)
					wake_up (wait_head);
			}
   	 	} else {
   	 		ed->state &= ~ED_URB_DEL;
			tdHeadP = dma_to_td (ohci, le32_to_cpup (&ed->hwHeadP) & 0xfffffff0);

			if (tdHeadP == tdTailP) {
				if (ed->state == ED_OPER)
					ep_unlink(ohci, ed);
			} else
   	 			ed->hwINFO &= ~cpu_to_le32 (OHCI_ED_SKIP);
   	 	}

		switch (ed->type) {
			case PIPE_CONTROL:
				ctrl = 1;
				break;
			case PIPE_BULK:
				bulk = 1;
				break;
		}
   	}
   	
	/* maybe reenable control and bulk lists */ 
	if (!ohci->disabled) {
		if (ctrl) 	/* reset control list */
			writel (0, &ohci->regs->ed_controlcurrent);
		if (bulk)	/* reset bulk list */
			writel (0, &ohci->regs->ed_bulkcurrent);
		if (!ohci->ed_rm_list[!frame] && !ohci->sleeping) {
			if (ohci->ed_controltail)
				ohci->hc_control |= OHCI_CTRL_CLE;
			if (ohci->ed_bulktail)
				ohci->hc_control |= OHCI_CTRL_BLE;
			writel (ohci->hc_control, &ohci->regs->control);   
		}
	}

   	ohci->ed_rm_list[frame] = NULL;
}


  		
/*-------------------------------------------------------------------------*/

/* td done list */

static void dl_done_list (ohci_t * ohci, td_t * td_list)
{
  	td_t * td_list_next = NULL;
	ed_t * ed;
	int cc = 0;
	struct urb * urb;
	urb_priv_t * urb_priv;
 	__u32 tdINFO, edHeadP, edTailP;
 
  	while (td_list) {
   		td_list_next = td_list->next_dl_td;
   		
  		urb = td_list->urb;
  		urb_priv = urb->hcpriv;
  		tdINFO = le32_to_cpup (&td_list->hwINFO);
  		
   		ed = td_list->ed;
   		
   		dl_transfer_length(td_list);
 			
  		/* error code of transfer */
  		cc = TD_CC_GET (tdINFO);
  		if (cc == TD_CC_STALL)
			usb_endpoint_halt(urb->dev,
				usb_pipeendpoint(urb->pipe),
				usb_pipeout(urb->pipe));
  		
  		if (!(urb->transfer_flags & USB_DISABLE_SPD)
				&& (cc == TD_DATAUNDERRUN))
			cc = TD_CC_NOERROR;

  		if (++(urb_priv->td_cnt) == urb_priv->length) {
			if ((ed->state & (ED_OPER | ED_UNLINK))
					&& (urb_priv->state != URB_DEL)) {
  				urb->status = cc_to_error[cc];
  				sohci_return_urb (ohci, urb);
  			} else {
  				dl_del_urb (ohci, urb);
			}
  		}
  		
  		if (ed->state != ED_NEW) { 
  			edHeadP = le32_to_cpup (&ed->hwHeadP) & 0xfffffff0;
  			edTailP = le32_to_cpup (&ed->hwTailP);

			/* unlink eds if they are not busy */
     			if ((edHeadP == edTailP) && (ed->state == ED_OPER)) 
     				ep_unlink (ohci, ed);
     		}	
     	
    		td_list = td_list_next;
  	}  
}




/*-------------------------------------------------------------------------*
 * Virtual Root Hub 
 *-------------------------------------------------------------------------*/
 
/* Device descriptor */
static __u8 root_hub_dev_des[] =
{
	0x12,       /*  __u8  bLength; */
	0x01,       /*  __u8  bDescriptorType; Device */
	0x10,	    /*  __u16 bcdUSB; v1.1 */
	0x01,
	0x09,	    /*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,	    /*  __u8  bDeviceSubClass; */
	0x00,       /*  __u8  bDeviceProtocol; */
	0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
	0x00,       /*  __u16 idVendor; */
	0x00,
	0x00,       /*  __u16 idProduct; */
 	0x00,
	0x00,       /*  __u16 bcdDevice; */
 	0x00,
	0x00,       /*  __u8  iManufacturer; */
	0x02,       /*  __u8  iProduct; */
	0x01,       /*  __u8  iSerialNumber; */
	0x01        /*  __u8  bNumConfigurations; */
};


/* Configuration descriptor */
static __u8 root_hub_config_des[] =
{
	0x09,       /*  __u8  bLength; */
	0x02,       /*  __u8  bDescriptorType; Configuration */
	0x19,       /*  __u16 wTotalLength; */
	0x00,
	0x01,       /*  __u8  bNumInterfaces; */
	0x01,       /*  __u8  bConfigurationValue; */
	0x00,       /*  __u8  iConfiguration; */
	0x40,       /*  __u8  bmAttributes; 
                 Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup, 4..0: resvd */
	0x00,       /*  __u8  MaxPower; */
      
	/* interface */	  
	0x09,       /*  __u8  if_bLength; */
	0x04,       /*  __u8  if_bDescriptorType; Interface */
	0x00,       /*  __u8  if_bInterfaceNumber; */
	0x00,       /*  __u8  if_bAlternateSetting; */
	0x01,       /*  __u8  if_bNumEndpoints; */
	0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,       /*  __u8  if_bInterfaceSubClass; */
	0x00,       /*  __u8  if_bInterfaceProtocol; */
	0x00,       /*  __u8  if_iInterface; */
     
	/* endpoint */
	0x07,       /*  __u8  ep_bLength; */
	0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x03,       /*  __u8  ep_bmAttributes; Interrupt */
 	0x02,       /*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
 	0x00,
	0xff        /*  __u8  ep_bInterval; 255 ms */
};

/* Hub class-specific descriptor is constructed dynamically */


/*-------------------------------------------------------------------------*/

/* prepare Interrupt pipe data; HUB INTERRUPT ENDPOINT */ 
 
static int rh_send_irq (ohci_t * ohci, void * rh_data, int rh_len)
{
	int num_ports;
	int i;
	int ret;
	int len;

	__u8 data[8];

	num_ports = roothub_a (ohci) & RH_A_NDP; 
	if (num_ports > MAX_ROOT_PORTS) {
		err ("bogus NDP=%d for OHCI usb-%s", num_ports,
			ohci->ohci_dev->slot_name);
		err ("rereads as NDP=%d",
			readl (&ohci->regs->roothub.a) & RH_A_NDP);
		/* retry later; "should not happen" */
		return 0;
	}
	*(__u8 *) data = (roothub_status (ohci) & (RH_HS_LPSC | RH_HS_OCIC))
		? 1: 0;
	ret = *(__u8 *) data;

	for ( i = 0; i < num_ports; i++) {
		*(__u8 *) (data + (i + 1) / 8) |= 
			((roothub_portstatus (ohci, i) &
				(RH_PS_CSC | RH_PS_PESC | RH_PS_PSSC | RH_PS_OCIC | RH_PS_PRSC))
			    ? 1: 0) << ((i + 1) % 8);
		ret += *(__u8 *) (data + (i + 1) / 8);
	}
	len = i/8 + 1;
  
	if (ret > 0) { 
		memcpy(rh_data, data,
		       min_t(unsigned int, len,
			   min_t(unsigned int, rh_len, sizeof(data))));
		return len;
	}
	return 0;
}

/*-------------------------------------------------------------------------*/

/* Virtual Root Hub INTs are polled by this timer every "interval" ms */
 
static void rh_int_timer_do (unsigned long ptr)
{
	int len; 

	struct urb * urb = (struct urb *) ptr;
	ohci_t * ohci = urb->dev->bus->hcpriv;

	if (ohci->disabled)
		return;

	/* ignore timers firing during PM suspend, etc */
	if ((ohci->hc_control & OHCI_CTRL_HCFS) != OHCI_USB_OPER)
		goto out;

	if(ohci->rh.send) { 
		len = rh_send_irq (ohci, urb->transfer_buffer, urb->transfer_buffer_length);
		if (len > 0) {
			urb->actual_length = len;
#ifdef DEBUG
			urb_print (urb, "RET-t(rh)", usb_pipeout (urb->pipe));
#endif
			if (urb->complete)
				urb->complete (urb);
		}
	}
 out:
	rh_init_int_timer (urb);
}

/*-------------------------------------------------------------------------*/

/* Root Hub INTs are polled by this timer */

static int rh_init_int_timer (struct urb * urb) 
{
	ohci_t * ohci = urb->dev->bus->hcpriv;

	ohci->rh.interval = urb->interval;
	init_timer (&ohci->rh.rh_int_timer);
	ohci->rh.rh_int_timer.function = rh_int_timer_do;
	ohci->rh.rh_int_timer.data = (unsigned long) urb;
	ohci->rh.rh_int_timer.expires = 
			jiffies + (HZ * (urb->interval < 30? 30: urb->interval)) / 1000;
	add_timer (&ohci->rh.rh_int_timer);
	
	return 0;
}

/*-------------------------------------------------------------------------*/

#define OK(x) 			len = (x); break
#define WR_RH_STAT(x) 		writel((x), &ohci->regs->roothub.status)
#define WR_RH_PORTSTAT(x) 	writel((x), &ohci->regs->roothub.portstatus[wIndex-1])
#define RD_RH_STAT		roothub_status(ohci)
#define RD_RH_PORTSTAT		roothub_portstatus(ohci,wIndex-1)

/* request to virtual root hub */

static int rh_submit_urb (struct urb * urb)
{
	struct usb_device * usb_dev = urb->dev;
	ohci_t * ohci = usb_dev->bus->hcpriv;
	unsigned int pipe = urb->pipe;
	struct usb_ctrlrequest * cmd = (struct usb_ctrlrequest *) urb->setup_packet;
	void * data = urb->transfer_buffer;
	int leni = urb->transfer_buffer_length;
	int len = 0;
	int status = TD_CC_NOERROR;
	unsigned long flags;

	__u32 datab[4];
	__u8  * data_buf = (__u8 *) datab;
	
 	__u16 bmRType_bReq;
	__u16 wValue; 
	__u16 wIndex;
	__u16 wLength;

	spin_lock_irqsave(&ohci->ohci_lock, flags);

	if (usb_pipeint(pipe)) {
		ohci->rh.urb =  urb;
		ohci->rh.send = 1;
		ohci->rh.interval = urb->interval;
		rh_init_int_timer(urb);
		urb->status = cc_to_error [TD_CC_NOERROR];

		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
		return 0;
	}

	bmRType_bReq  = cmd->bRequestType | (cmd->bRequest << 8);
	wValue        = le16_to_cpu (cmd->wValue);
	wIndex        = le16_to_cpu (cmd->wIndex);
	wLength       = le16_to_cpu (cmd->wLength);

	switch (bmRType_bReq) {
	/* Request Destination:
	   without flags: Device, 
	   RH_INTERFACE: interface, 
	   RH_ENDPOINT: endpoint,
	   RH_CLASS means HUB here, 
	   RH_OTHER | RH_CLASS  almost ever means HUB_PORT here 
	*/
  
		case RH_GET_STATUS: 				 		
				*(__u16 *) data_buf = cpu_to_le16 (1); OK (2);
		case RH_GET_STATUS | RH_INTERFACE: 	 		
				*(__u16 *) data_buf = cpu_to_le16 (0); OK (2);
		case RH_GET_STATUS | RH_ENDPOINT:	 		
				*(__u16 *) data_buf = cpu_to_le16 (0); OK (2);   
		case RH_GET_STATUS | RH_CLASS: 				
				*(__u32 *) data_buf = cpu_to_le32 (
					RD_RH_STAT & ~(RH_HS_CRWE | RH_HS_DRWE));
				OK (4);
		case RH_GET_STATUS | RH_OTHER | RH_CLASS: 	
				*(__u32 *) data_buf = cpu_to_le32 (RD_RH_PORTSTAT); OK (4);

		case RH_CLEAR_FEATURE | RH_ENDPOINT:  
			switch (wValue) {
				case (RH_ENDPOINT_STALL): OK (0);
			}
			break;

		case RH_CLEAR_FEATURE | RH_CLASS:
			switch (wValue) {
				case RH_C_HUB_LOCAL_POWER:
					OK(0);
				case (RH_C_HUB_OVER_CURRENT): 
						WR_RH_STAT(RH_HS_OCIC); OK (0);
			}
			break;
		
		case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
			switch (wValue) {
				case (RH_PORT_ENABLE): 			
						WR_RH_PORTSTAT (RH_PS_CCS ); OK (0);
				case (RH_PORT_SUSPEND):			
						WR_RH_PORTSTAT (RH_PS_POCI); OK (0);
				case (RH_PORT_POWER):			
						WR_RH_PORTSTAT (RH_PS_LSDA); OK (0);
				case (RH_C_PORT_CONNECTION):	
						WR_RH_PORTSTAT (RH_PS_CSC ); OK (0);
				case (RH_C_PORT_ENABLE):		
						WR_RH_PORTSTAT (RH_PS_PESC); OK (0);
				case (RH_C_PORT_SUSPEND):		
						WR_RH_PORTSTAT (RH_PS_PSSC); OK (0);
				case (RH_C_PORT_OVER_CURRENT):	
						WR_RH_PORTSTAT (RH_PS_OCIC); OK (0);
				case (RH_C_PORT_RESET):			
						WR_RH_PORTSTAT (RH_PS_PRSC); OK (0); 
			}
			break;
 
		case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
			switch (wValue) {
				case (RH_PORT_SUSPEND):			
						WR_RH_PORTSTAT (RH_PS_PSS ); OK (0); 
				case (RH_PORT_RESET): /* BUG IN HUP CODE *********/
						if (RD_RH_PORTSTAT & RH_PS_CCS)
						    WR_RH_PORTSTAT (RH_PS_PRS);
						OK (0);
				case (RH_PORT_POWER):			
						WR_RH_PORTSTAT (RH_PS_PPS ); OK (0); 
				case (RH_PORT_ENABLE): /* BUG IN HUP CODE *********/
						if (RD_RH_PORTSTAT & RH_PS_CCS)
						    WR_RH_PORTSTAT (RH_PS_PES );
						OK (0);
			}
			break;

		case RH_SET_ADDRESS: ohci->rh.devnum = wValue; OK(0);

		case RH_GET_DESCRIPTOR:
			switch ((wValue & 0xff00) >> 8) {
				case (0x01): /* device descriptor */
					len = min_t(unsigned int,
						  leni,
						  min_t(unsigned int,
						      sizeof (root_hub_dev_des),
						      wLength));
					data_buf = root_hub_dev_des; OK(len);
				case (0x02): /* configuration descriptor */
					len = min_t(unsigned int,
						  leni,
						  min_t(unsigned int,
						      sizeof (root_hub_config_des),
						      wLength));
					data_buf = root_hub_config_des; OK(len);
				case (0x03): /* string descriptors */
					len = usb_root_hub_string (wValue & 0xff,
						(int)(long) ohci->regs, "OHCI",
						data, wLength);
					if (len > 0) {
						data_buf = data;
						OK(min_t(int, leni, len));
					}
					// else fallthrough
				default: 
					status = TD_CC_STALL;
			}
			break;
		
		case RH_GET_DESCRIPTOR | RH_CLASS:
		    {
			    __u32 temp = roothub_a (ohci);

			    data_buf [0] = 9;		// min length;
			    data_buf [1] = 0x29;
			    data_buf [2] = temp & RH_A_NDP;
			    data_buf [3] = 0;
			    if (temp & RH_A_PSM) 	/* per-port power switching? */
				data_buf [3] |= 0x1;
			    if (temp & RH_A_NOCP)	/* no overcurrent reporting? */
				data_buf [3] |= 0x10;
			    else if (temp & RH_A_OCPM)	/* per-port overcurrent reporting? */
				data_buf [3] |= 0x8;

			    datab [1] = 0;
			    data_buf [5] = (temp & RH_A_POTPGT) >> 24;
			    temp = roothub_b (ohci);
			    data_buf [7] = temp & RH_B_DR;
			    if (data_buf [2] < 7) {
				data_buf [8] = 0xff;
			    } else {
				data_buf [0] += 2;
				data_buf [8] = (temp & RH_B_DR) >> 8;
				data_buf [10] = data_buf [9] = 0xff;
			    }
				
			    len = min_t(unsigned int, leni,
				      min_t(unsigned int, data_buf [0], wLength));
			    OK (len);
			}
 
		case RH_GET_CONFIGURATION: 	*(__u8 *) data_buf = 0x01; OK (1);

		case RH_SET_CONFIGURATION: 	WR_RH_STAT (0x10000); OK (0);

		default: 
			dbg ("unsupported root hub command");
			status = TD_CC_STALL;
	}
	
#ifdef	DEBUG
	// ohci_dump_roothub (ohci, 0);
#endif

	len = min_t(int, len, leni);
	if (data != data_buf)
	    memcpy (data, data_buf, len);
  	urb->actual_length = len;
	urb->status = cc_to_error [status];
	
#ifdef DEBUG
	urb_print (urb, "RET(rh)", usb_pipeout (urb->pipe));
#endif

	urb->hcpriv = NULL;
	spin_unlock_irqrestore(&ohci->ohci_lock, flags);
	usb_dec_dev_use (usb_dev);
	urb->dev = NULL;
	if (urb->complete)
	    	urb->complete (urb);
	return 0;
}

/*-------------------------------------------------------------------------*/

static int rh_unlink_urb (struct urb * urb)
{
	ohci_t * ohci = urb->dev->bus->hcpriv;
	unsigned int flags;
 
	spin_lock_irqsave(&ohci->ohci_lock, flags);
	if (ohci->rh.urb == urb) {
		ohci->rh.send = 0;
		del_timer (&ohci->rh.rh_int_timer);
		ohci->rh.urb = NULL;

		urb->hcpriv = NULL;
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
		usb_dec_dev_use(urb->dev);
		urb->dev = NULL;
		if (urb->transfer_flags & USB_ASYNC_UNLINK) {
			urb->status = -ECONNRESET;
			if (urb->complete)
				urb->complete (urb);
		} else
			urb->status = -ENOENT;
	} else {
		spin_unlock_irqrestore(&ohci->ohci_lock, flags);
	}
	return 0;
}
 
/*-------------------------------------------------------------------------*
 * HC functions
 *-------------------------------------------------------------------------*/

/* reset the HC and BUS */

static int hc_reset (ohci_t * ohci)
{
	int timeout = 30;
	int smm_timeout = 50; /* 0,5 sec */
	 	
#ifndef __hppa__
	/* PA-RISC doesn't have SMM, but PDC might leave IR set */
	if (readl (&ohci->regs->control) & OHCI_CTRL_IR) { /* SMM owns the HC */
		writel (OHCI_OCR, &ohci->regs->cmdstatus); /* request ownership */
		dbg("USB HC TakeOver from SMM");
		while (readl (&ohci->regs->control) & OHCI_CTRL_IR) {
			wait_ms (10);
			if (--smm_timeout == 0) {
				err("USB HC TakeOver failed!");
				return -1;
			}
		}
	}
#endif	
		
	/* Disable HC interrupts */
	writel (OHCI_INTR_MIE, &ohci->regs->intrdisable);

	dbg("USB HC reset_hc usb-%s: ctrl = 0x%x ;",
		ohci->ohci_dev->slot_name,
		readl (&ohci->regs->control));

  	/* Reset USB (needed by some controllers) */
	writel (0, &ohci->regs->control);

	/* Force a state change from USBRESET to USBOPERATIONAL for ALi */
	(void) readl (&ohci->regs->control);	/* PCI posting */
	writel (ohci->hc_control = OHCI_USB_OPER, &ohci->regs->control);

	/* HC Reset requires max 10 ms delay */
	writel (OHCI_HCR,  &ohci->regs->cmdstatus);
	while ((readl (&ohci->regs->cmdstatus) & OHCI_HCR) != 0) {
		if (--timeout == 0) {
			err("USB HC reset timed out!");
			return -1;
		}	
		udelay (1);
	}	 
	return 0;
}

/*-------------------------------------------------------------------------*/

/* Start an OHCI controller, set the BUS operational
 * enable interrupts 
 * connect the virtual root hub */

static int hc_start (ohci_t * ohci)
{
  	__u32 mask;
  	unsigned int fminterval;
  	struct usb_device  * usb_dev;
	struct ohci_device * dev;
	
	ohci->disabled = 1;

	/* Tell the controller where the control and bulk lists are
	 * The lists are empty now. */
	 
	writel (0, &ohci->regs->ed_controlhead);
	writel (0, &ohci->regs->ed_bulkhead);
	
	writel (ohci->hcca_dma, &ohci->regs->hcca); /* a reset clears this */
   
  	fminterval = 0x2edf;
	writel ((fminterval * 9) / 10, &ohci->regs->periodicstart);
	fminterval |= ((((fminterval - 210) * 6) / 7) << 16); 
	writel (fminterval, &ohci->regs->fminterval);	
	writel (0x628, &ohci->regs->lsthresh);

 	/* start controller operations */
 	ohci->hc_control = OHCI_CONTROL_INIT | OHCI_USB_OPER;
	ohci->disabled = 0;
 	writel (ohci->hc_control, &ohci->regs->control);
 
	/* Choose the interrupts we care about now, others later on demand */
	mask = OHCI_INTR_MIE | OHCI_INTR_UE | OHCI_INTR_WDH | OHCI_INTR_SO;
	writel (mask, &ohci->regs->intrenable);
	writel (mask, &ohci->regs->intrstatus);

#ifdef	OHCI_USE_NPS
	if(ohci->flags & OHCI_QUIRK_SUCKYIO)
	{
		/* NSC 87560 at least requires different setup .. */
		writel ((roothub_a (ohci) | RH_A_NOCP) &
			~(RH_A_OCPM | RH_A_POTPGT | RH_A_PSM | RH_A_NPS),
			&ohci->regs->roothub.a);
	}
	else
	{
		/* required for AMD-756 and some Mac platforms */
		writel ((roothub_a (ohci) | RH_A_NPS) & ~RH_A_PSM,
			&ohci->regs->roothub.a);
	}
	writel (RH_HS_LPSC, &ohci->regs->roothub.status);
#endif	/* OHCI_USE_NPS */

	(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */

	// POTPGT delay is bits 24-31, in 2 ms units.
	mdelay ((roothub_a (ohci) >> 23) & 0x1fe);
 
	/* connect the virtual root hub */
	ohci->rh.devnum = 0;
	usb_dev = usb_alloc_dev (NULL, ohci->bus);
	if (!usb_dev) {
	    ohci->disabled = 1;
	    return -ENOMEM;
	}

	dev = usb_to_ohci (usb_dev);
	ohci->bus->root_hub = usb_dev;
	usb_connect (usb_dev);
	if (usb_new_device (usb_dev) != 0) {
		usb_free_dev (usb_dev); 
		ohci->disabled = 1;
		return -ENODEV;
	}
	
	return 0;
}

/*-------------------------------------------------------------------------*/

/* called only from interrupt handler */

static void check_timeouts (struct ohci *ohci)
{
	spin_lock (&ohci->ohci_lock);
	while (!list_empty (&ohci->timeout_list)) {
		struct urb	*urb;

		urb = list_entry (ohci->timeout_list.next, struct urb, urb_list);
		if (time_after (jiffies, urb->timeout))
			break;

		list_del_init (&urb->urb_list);
		if (urb->status != -EINPROGRESS)
			continue;

		urb->transfer_flags |= USB_TIMEOUT_KILLED | USB_ASYNC_UNLINK;
		spin_unlock (&ohci->ohci_lock);

		// outside the interrupt handler (in a timer...)
		// this reference would race interrupts
		sohci_unlink_urb (urb);

		spin_lock (&ohci->ohci_lock);
	}
	spin_unlock (&ohci->ohci_lock);
}


/*-------------------------------------------------------------------------*/

/* an interrupt happens */

static void hc_interrupt (int irq, void * __ohci, struct pt_regs * r)
{
	ohci_t * ohci = __ohci;
	struct ohci_regs * regs = ohci->regs;
 	int ints; 

	spin_lock (&ohci->ohci_lock);

	/* avoid (slow) readl if only WDH happened */
	if ((ohci->hcca->done_head != 0)
			&& !(le32_to_cpup (&ohci->hcca->done_head) & 0x01)) {
		ints =  OHCI_INTR_WDH;

	/* cardbus/... hardware gone before remove() */
	} else if ((ints = readl (&regs->intrstatus)) == ~(u32)0) {
		ohci->disabled++;
		spin_unlock (&ohci->ohci_lock);
		err ("%s device removed!", ohci->ohci_dev->slot_name);
		return;

	/* interrupt for some other device? */
	} else if ((ints &= readl (&regs->intrenable)) == 0) {
		spin_unlock (&ohci->ohci_lock);
		return;
	} 

	// dbg("Interrupt: %x frame: %x", ints, le16_to_cpu (ohci->hcca->frame_no));

	if (ints & OHCI_INTR_UE) {
		ohci->disabled++;
		err ("OHCI Unrecoverable Error, controller usb-%s disabled",
			ohci->ohci_dev->slot_name);
		// e.g. due to PCI Master/Target Abort

#ifdef	DEBUG
		ohci_dump (ohci, 1);
#else
		// FIXME: be optimistic, hope that bug won't repeat often.
		// Make some non-interrupt context restart the controller.
		// Count and limit the retries though; either hardware or
		// software errors can go forever...
#endif
		hc_reset (ohci);
	}
  
	if (ints & OHCI_INTR_WDH) {
		writel (OHCI_INTR_WDH, &regs->intrdisable);	
		(void)readl (&regs->intrdisable); /* PCI posting flush */
		dl_done_list (ohci, dl_reverse_done_list (ohci));
		writel (OHCI_INTR_WDH, &regs->intrenable); 
		(void)readl (&regs->intrdisable); /* PCI posting flush */
	}
  
	if (ints & OHCI_INTR_SO) {
		dbg("USB Schedule overrun");
		writel (OHCI_INTR_SO, &regs->intrenable); 	 
		(void)readl (&regs->intrdisable); /* PCI posting flush */
	}

	// FIXME:  this assumes SOF (1/ms) interrupts don't get lost...
	if (ints & OHCI_INTR_SF) { 
		unsigned int frame = le16_to_cpu (ohci->hcca->frame_no) & 1;
		writel (OHCI_INTR_SF, &regs->intrdisable);	
		(void)readl (&regs->intrdisable); /* PCI posting flush */
		if (ohci->ed_rm_list[!frame] != NULL) {
			dl_del_list (ohci, !frame);
		}
		if (ohci->ed_rm_list[frame] != NULL) {
			writel (OHCI_INTR_SF, &regs->intrenable);	
			(void)readl (&regs->intrdisable); /* PCI posting flush */
		}
	}

	/*
	 * Finally, we are done with trashing about our hardware lists
	 * and other CPUs are allowed in. The festive flipping of the lock
	 * ensues as we struggle with the check_timeouts disaster.
	 */
	spin_unlock (&ohci->ohci_lock);

	if (!list_empty (&ohci->timeout_list)) {
		check_timeouts (ohci);
// FIXME:  enable SF as needed in a timer;
// don't make lots of 1ms interrupts
// On unloaded USB, think 4k ~= 4-5msec
		if (!list_empty (&ohci->timeout_list))
			writel (OHCI_INTR_SF, &regs->intrenable);	
	}

	writel (ints, &regs->intrstatus);
	writel (OHCI_INTR_MIE, &regs->intrenable);	
	(void)readl (&regs->intrdisable); /* PCI posting flush */

	ohci_complete(ohci);
}

/*-------------------------------------------------------------------------*/

/* allocate OHCI */

static ohci_t * __devinit hc_alloc_ohci (struct pci_dev *dev, void * mem_base)
{
	ohci_t * ohci;

	ohci = (ohci_t *) kmalloc (sizeof *ohci, GFP_KERNEL);
	if (!ohci)
		return NULL;
		
	memset (ohci, 0, sizeof (ohci_t));

	ohci->hcca = pci_alloc_consistent (dev, sizeof *ohci->hcca,
			&ohci->hcca_dma);
        if (!ohci->hcca) {
                kfree (ohci);
                return NULL;
        }
        memset (ohci->hcca, 0, sizeof (struct ohci_hcca));

	ohci->disabled = 1;
	ohci->sleeping = 0;
	ohci->irq = -1;
	ohci->regs = mem_base;   

	ohci->ohci_dev = dev;
	pci_set_drvdata(dev, ohci);
 
	INIT_LIST_HEAD (&ohci->timeout_list);
	spin_lock_init(&ohci->ohci_lock);

	ohci->bus = usb_alloc_bus (&sohci_device_operations);
	if (!ohci->bus) {
		pci_set_drvdata (dev, NULL);
		pci_free_consistent (ohci->ohci_dev, sizeof *ohci->hcca,
				ohci->hcca, ohci->hcca_dma);
		kfree (ohci);
		return NULL;
	}
	ohci->bus->bus_name = dev->slot_name;
	ohci->bus->hcpriv = (void *) ohci;

	return ohci;
} 


/*-------------------------------------------------------------------------*/

/* De-allocate all resources.. */

static void hc_release_ohci (ohci_t * ohci)
{	
	dbg ("USB HC release ohci usb-%s", ohci->ohci_dev->slot_name);

	/* disconnect all devices */    
	if (ohci->bus->root_hub)
		usb_disconnect (&ohci->bus->root_hub);

	if (!ohci->disabled)
		hc_reset (ohci);
	
	if (ohci->irq >= 0) {
		free_irq (ohci->irq, ohci);
		ohci->irq = -1;
	}
	pci_set_drvdata(ohci->ohci_dev, NULL);
	if (ohci->bus) {
		if (ohci->bus->busnum != -1)
			usb_deregister_bus (ohci->bus);

		usb_free_bus (ohci->bus);
	}

	ohci_mem_cleanup (ohci);
    
	/* unmap the IO address space */
	iounmap (ohci->regs);

	pci_free_consistent (ohci->ohci_dev, sizeof *ohci->hcca,
		ohci->hcca, ohci->hcca_dma);
	kfree (ohci);
}

/*-------------------------------------------------------------------------*/

/* Increment the module usage count, start the control thread and
 * return success. */

static struct pci_driver ohci_pci_driver;
 
static int __devinit
hc_found_ohci (struct pci_dev *dev, int irq,
	void *mem_base, const struct pci_device_id *id)
{
	ohci_t * ohci;
	char buf[8], *bufp = buf;
	int ret;

#ifndef __sparc__
	sprintf(buf, "%d", irq);
#else
	bufp = __irq_itoa(irq);
#endif
	printk(KERN_INFO __FILE__ ": USB OHCI at membase 0x%lx, IRQ %s\n",
		(unsigned long)	mem_base, bufp);
	printk(KERN_INFO __FILE__ ": usb-%s, %s\n", dev->slot_name, dev->name);
    
	ohci = hc_alloc_ohci (dev, mem_base);
	if (!ohci) {
		return -ENOMEM;
	}
	if ((ret = ohci_mem_init (ohci)) < 0) {
		hc_release_ohci (ohci);
		return ret;
	}
	ohci->flags = id->driver_data;
	
	/* Check for NSC87560. We have to look at the bridge (fn1) to identify
	   the USB (fn2). This quirk might apply to more or even all NSC stuff
	   I don't know.. */
	   
	if(dev->vendor == PCI_VENDOR_ID_NS)
	{
		struct pci_dev *fn1  = pci_find_slot(dev->bus->number, PCI_DEVFN(PCI_SLOT(dev->devfn), 1));
		if(fn1 && fn1->vendor == PCI_VENDOR_ID_NS && fn1->device == PCI_DEVICE_ID_NS_87560_LIO)
			ohci->flags |= OHCI_QUIRK_SUCKYIO;
		
	}
	
	if (ohci->flags & OHCI_QUIRK_SUCKYIO)
		printk (KERN_INFO __FILE__ ": Using NSC SuperIO setup\n");
	if (ohci->flags & OHCI_QUIRK_AMD756)
		printk (KERN_INFO __FILE__ ": AMD756 erratum 4 workaround\n");

	if (hc_reset (ohci) < 0) {
		hc_release_ohci (ohci);
		return -ENODEV;
	}

	/* FIXME this is a second HC reset; why?? */
	writel (ohci->hc_control = OHCI_USB_RESET, &ohci->regs->control);
	(void)readl (&ohci->regs->intrdisable); /* PCI posting flush */
	wait_ms (10);

	usb_register_bus (ohci->bus);
	
	if (request_irq (irq, hc_interrupt, SA_SHIRQ,
			ohci_pci_driver.name, ohci) != 0) {
		err ("request interrupt %s failed", bufp);
		hc_release_ohci (ohci);
		return -EBUSY;
	}
	ohci->irq = irq;     

	if (hc_start (ohci) < 0) {
		err ("can't start usb-%s", dev->slot_name);
		hc_release_ohci (ohci);
		return -EBUSY;
	}

#ifdef	DEBUG
	ohci_dump (ohci, 1);
#endif
	return 0;
}

/*-------------------------------------------------------------------------*/

#ifdef	CONFIG_PM

/* controller died; cleanup debris, then restart */
/* must not be called from interrupt context */

static void hc_restart (ohci_t *ohci)
{
	int temp;
	int i;

	if (ohci->pci_latency)
		pci_write_config_byte (ohci->ohci_dev, PCI_LATENCY_TIMER, ohci->pci_latency);

	ohci->disabled = 1;
	ohci->sleeping = 0;
	if (ohci->bus->root_hub)
		usb_disconnect (&ohci->bus->root_hub);
	
	/* empty the interrupt branches */
	for (i = 0; i < NUM_INTS; i++) ohci->ohci_int_load[i] = 0;
	for (i = 0; i < NUM_INTS; i++) ohci->hcca->int_table[i] = 0;
	
	/* no EDs to remove */
	ohci->ed_rm_list [0] = NULL;
	ohci->ed_rm_list [1] = NULL;

	/* empty control and bulk lists */	 
	ohci->ed_isotail     = NULL;
	ohci->ed_controltail = NULL;
	ohci->ed_bulktail    = NULL;

	if ((temp = hc_reset (ohci)) < 0 || (temp = hc_start (ohci)) < 0) {
		err ("can't restart usb-%s, %d", ohci->ohci_dev->slot_name, temp);
	} else
		dbg ("restart usb-%s completed", ohci->ohci_dev->slot_name);
}

#endif	/* CONFIG_PM */

/*-------------------------------------------------------------------------*/

/* configured so that an OHCI device is always provided */
/* always called with process context; sleeping is OK */

static int __devinit
ohci_pci_probe (struct pci_dev *dev, const struct pci_device_id *id)
{
	unsigned long mem_resource, mem_len;
	void *mem_base;
	int status;

	if (pci_enable_device(dev) < 0)
		return -ENODEV;

        if (!dev->irq) {
        	err("found OHCI device with no IRQ assigned. check BIOS settings!");
		pci_disable_device (dev);
   	        return -ENODEV;
        }
	
	/* we read its hardware registers as memory */
	mem_resource = pci_resource_start(dev, 0);
	mem_len = pci_resource_len(dev, 0);
	if (!request_mem_region (mem_resource, mem_len, ohci_pci_driver.name)) {
		dbg ("controller already in use");
		pci_disable_device (dev);
		return -EBUSY;
	}

	mem_base = ioremap_nocache (mem_resource, mem_len);
	if (!mem_base) {
		err("Error mapping OHCI memory");
		release_mem_region (mem_resource, mem_len);
		pci_disable_device (dev);
		return -EFAULT;
	}

	/* controller writes into our memory */
	pci_set_master (dev);

	status = hc_found_ohci (dev, dev->irq, mem_base, id);
	if (status < 0) {
		iounmap (mem_base);
		release_mem_region (mem_resource, mem_len);
		pci_disable_device (dev);
	}
	return status;
} 

/*-------------------------------------------------------------------------*/

/* may be called from interrupt context [interface spec] */
/* may be called without controller present */
/* may be called with controller, bus, and devices active */

static void __devexit
ohci_pci_remove (struct pci_dev *dev)
{
	ohci_t		*ohci = pci_get_drvdata(dev);

	dbg ("remove %s controller usb-%s%s%s",
		hcfs2string (ohci->hc_control & OHCI_CTRL_HCFS),
		dev->slot_name,
		ohci->disabled ? " (disabled)" : "",
		in_interrupt () ? " in interrupt" : ""
		);
#ifdef	DEBUG
	ohci_dump (ohci, 1);
#endif

	/* don't wake up sleeping controllers, or block in interrupt context */
	if ((ohci->hc_control & OHCI_CTRL_HCFS) != OHCI_USB_OPER || in_interrupt ()) {
		dbg ("controller being disabled");
		ohci->disabled = 1;
	}

	/* on return, USB will always be reset (if present) */
	if (ohci->disabled)
		writel (ohci->hc_control = OHCI_USB_RESET,
			&ohci->regs->control);

	hc_release_ohci (ohci);

	release_mem_region (pci_resource_start (dev, 0), pci_resource_len (dev, 0));
	pci_disable_device (dev);
}


#ifdef	CONFIG_PM

/*-------------------------------------------------------------------------*/

static int
ohci_pci_suspend (struct pci_dev *dev, u32 state)
{
	ohci_t			*ohci = pci_get_drvdata(dev);
	unsigned long		flags;
	u16 cmd;

	if ((ohci->hc_control & OHCI_CTRL_HCFS) != OHCI_USB_OPER) {
		dbg ("can't suspend usb-%s (state is %s)", dev->slot_name,
			hcfs2string (ohci->hc_control & OHCI_CTRL_HCFS));
		return -EIO;
	}

	/* act as if usb suspend can always be used */
	info ("USB suspend: usb-%s", dev->slot_name);
	ohci->sleeping = 1;

	/* First stop processing */
  	spin_lock_irqsave (&ohci->ohci_lock, flags);
	ohci->hc_control &= ~(OHCI_CTRL_PLE|OHCI_CTRL_CLE|OHCI_CTRL_BLE|OHCI_CTRL_IE);
	writel (ohci->hc_control, &ohci->regs->control);
	writel (OHCI_INTR_SF, &ohci->regs->intrstatus);
	(void) readl (&ohci->regs->intrstatus);
  	spin_unlock_irqrestore (&ohci->ohci_lock, flags);

	/* Wait a frame or two */
	mdelay(1);
	if (!readl (&ohci->regs->intrstatus) & OHCI_INTR_SF)
		mdelay (1);
		
#ifdef CONFIG_PMAC_PBOOK
	if (_machine == _MACH_Pmac)
		disable_irq (ohci->irq);
	/* else, 2.4 assumes shared irqs -- don't disable */
#endif
	/* Enable remote wakeup */
	writel (readl(&ohci->regs->intrenable) | OHCI_INTR_RD, &ohci->regs->intrenable);

	/* Suspend chip and let things settle down a bit */
	ohci->hc_control = OHCI_USB_SUSPEND;
	writel (ohci->hc_control, &ohci->regs->control);
	(void) readl (&ohci->regs->control);
	mdelay (500); /* No schedule here ! */
	switch (readl (&ohci->regs->control) & OHCI_CTRL_HCFS) {
		case OHCI_USB_RESET:
			dbg("Bus in reset phase ???");
			break;
		case OHCI_USB_RESUME:
			dbg("Bus in resume phase ???");
			break;
		case OHCI_USB_OPER:
			dbg("Bus in operational phase ???");
			break;
		case OHCI_USB_SUSPEND:
			dbg("Bus suspended");
			break;
	}
	/* In some rare situations, Apple's OHCI have happily trashed
	 * memory during sleep. We disable it's bus master bit during
	 * suspend
	 */
	pci_read_config_word (dev, PCI_COMMAND, &cmd);
	cmd &= ~PCI_COMMAND_MASTER;
	pci_write_config_word (dev, PCI_COMMAND, cmd);
#ifdef CONFIG_PMAC_PBOOK
	{
	   	struct device_node	*of_node;

		/* Disable USB PAD & cell clock */
		of_node = pci_device_to_OF_node (ohci->ohci_dev);
		if (of_node)
			pmac_call_feature(PMAC_FTR_USB_ENABLE, of_node, 0, 0);
	}
#endif
	return 0;
}

/*-------------------------------------------------------------------------*/

static int
ohci_pci_resume (struct pci_dev *dev)
{
	ohci_t		*ohci = pci_get_drvdata(dev);
	int		temp;
	unsigned long	flags;

	/* guard against multiple resumes */
	atomic_inc (&ohci->resume_count);
	if (atomic_read (&ohci->resume_count) != 1) {
		err ("concurrent PCI resumes for usb-%s", dev->slot_name);
		atomic_dec (&ohci->resume_count);
		return 0;
	}

#ifdef CONFIG_PMAC_PBOOK
	{
		struct device_node *of_node;

		/* Re-enable USB PAD & cell clock */
		of_node = pci_device_to_OF_node (ohci->ohci_dev);
		if (of_node)
			pmac_call_feature(PMAC_FTR_USB_ENABLE, of_node, 0, 1);
	}
#endif

	/* did we suspend, or were we powered off? */
	ohci->hc_control = readl (&ohci->regs->control);
	temp = ohci->hc_control & OHCI_CTRL_HCFS;

#ifdef DEBUG
	/* the registers may look crazy here */
	ohci_dump_status (ohci);
#endif

	/* Re-enable bus mastering */
	pci_set_master(ohci->ohci_dev);
	
	switch (temp) {

	case OHCI_USB_RESET:	// lost power
		info ("USB restart: usb-%s", dev->slot_name);
		hc_restart (ohci);
		break;

	case OHCI_USB_SUSPEND:	// host wakeup
	case OHCI_USB_RESUME:	// remote wakeup
		info ("USB continue: usb-%s from %s wakeup", dev->slot_name,
			(temp == OHCI_USB_SUSPEND)
				? "host" : "remote");
		ohci->hc_control = OHCI_USB_RESUME;
		writel (ohci->hc_control, &ohci->regs->control);
		(void) readl (&ohci->regs->control);
		mdelay (20); /* no schedule here ! */
		/* Some controllers (lucent) need a longer delay here */
		mdelay (15);
		temp = readl (&ohci->regs->control);
		temp = ohci->hc_control & OHCI_CTRL_HCFS;
		if (temp != OHCI_USB_RESUME) {
			err ("controller usb-%s won't resume", dev->slot_name);
			ohci->disabled = 1;
			return -EIO;
		}

		/* Some chips likes being resumed first */
		writel (OHCI_USB_OPER, &ohci->regs->control);
		(void) readl (&ohci->regs->control);
		mdelay (3);

		/* Then re-enable operations */
		spin_lock_irqsave (&ohci->ohci_lock, flags);
		ohci->disabled = 0;
		ohci->sleeping = 0;
		ohci->hc_control = OHCI_CONTROL_INIT | OHCI_USB_OPER;
		if (!ohci->ed_rm_list[0] && !ohci->ed_rm_list[1]) {
			if (ohci->ed_controltail)
				ohci->hc_control |= OHCI_CTRL_CLE;
			if (ohci->ed_bulktail)
				ohci->hc_control |= OHCI_CTRL_BLE;
		}
		writel (ohci->hc_control, &ohci->regs->control);
		writel (OHCI_INTR_SF, &ohci->regs->intrstatus);
		writel (OHCI_INTR_SF, &ohci->regs->intrenable);
		/* Check for a pending done list */
		writel (OHCI_INTR_WDH, &ohci->regs->intrdisable);	
		(void) readl (&ohci->regs->intrdisable);
#ifdef CONFIG_PMAC_PBOOK
		if (_machine == _MACH_Pmac)
			enable_irq (ohci->irq);
#endif
		if (ohci->hcca->done_head)
			dl_done_list (ohci, dl_reverse_done_list (ohci));
		writel (OHCI_INTR_WDH, &ohci->regs->intrenable); 
		writel (OHCI_BLF, &ohci->regs->cmdstatus); /* start bulk list */
		writel (OHCI_CLF, &ohci->regs->cmdstatus); /* start Control list */
		spin_unlock_irqrestore (&ohci->ohci_lock, flags);
		break;

	default:
		warn ("odd PCI resume for usb-%s", dev->slot_name);
	}

	/* controller is operational, extra resumes are harmless */
	atomic_dec (&ohci->resume_count);

	return 0;
}

#endif	/* CONFIG_PM */


/*-------------------------------------------------------------------------*/

static const struct pci_device_id __devinitdata ohci_pci_ids [] = { {

	/*
	 * AMD-756 [Viper] USB has a serious erratum when used with
	 * lowspeed devices like mice.
	 */
	vendor:		0x1022,
	device:		0x740c,
	subvendor:	PCI_ANY_ID,
	subdevice:	PCI_ANY_ID,

	driver_data:	OHCI_QUIRK_AMD756,

} , {

	/* handle any USB OHCI controller */
	class: 		((PCI_CLASS_SERIAL_USB << 8) | 0x10),
	class_mask: 	~0,

	/* no matter who makes it */
	vendor:		PCI_ANY_ID,
	device:		PCI_ANY_ID,
	subvendor:	PCI_ANY_ID,
	subdevice:	PCI_ANY_ID,

	}, { /* end: all zeroes */ }
};

MODULE_DEVICE_TABLE (pci, ohci_pci_ids);

static struct pci_driver ohci_pci_driver = {
	name:		"usb-ohci",
	id_table:	&ohci_pci_ids [0],

	probe:		ohci_pci_probe,
	remove:		__devexit_p(ohci_pci_remove),

#ifdef	CONFIG_PM
	suspend:	ohci_pci_suspend,
	resume:		ohci_pci_resume,
#endif	/* PM */
};

 
/*-------------------------------------------------------------------------*/

static int __init ohci_hcd_init (void) 
{
	return pci_module_init (&ohci_pci_driver);
}

/*-------------------------------------------------------------------------*/

static void __exit ohci_hcd_cleanup (void) 
{	
	pci_unregister_driver (&ohci_pci_driver);
}

module_init (ohci_hcd_init);
module_exit (ohci_hcd_cleanup);


MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");
