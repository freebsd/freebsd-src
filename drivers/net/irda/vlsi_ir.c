/*********************************************************************
 *
 *	vlsi_ir.c:	VLSI82C147 PCI IrDA controller driver for Linux
 *
 *	Version:	0.3a, Nov 10, 2001
 *
 *	Copyright (c) 2001 Martin Diehl
 *
 *	This program is free software; you can redistribute it and/or 
 *	modify it under the terms of the GNU General Public License as 
 *	published by the Free Software Foundation; either version 2 of 
 *	the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License 
 *	along with this program; if not, write to the Free Software 
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *	MA 02111-1307 USA
 *
 ********************************************************************/

#include <linux/module.h>
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <net/irda/irda.h>
#include <net/irda/irda_device.h>
#include <net/irda/wrapper.h>
#include <net/irda/irlap.h>

#include <net/irda/vlsi_ir.h>


/********************************************************/


MODULE_DESCRIPTION("IrDA SIR/MIR/FIR driver for VLSI 82C147");
MODULE_AUTHOR("Martin Diehl <info@mdiehl.de>");
MODULE_LICENSE("GPL");


static /* const */ char drivername[] = "vlsi_ir";


#define PCI_CLASS_WIRELESS_IRDA 0x0d00

static struct pci_device_id vlsi_irda_table [] __devinitdata = { {

	class:          PCI_CLASS_WIRELESS_IRDA << 8,
	vendor:         PCI_VENDOR_ID_VLSI,
	device:         PCI_DEVICE_ID_VLSI_82C147,
	}, { /* all zeroes */ }
};

MODULE_DEVICE_TABLE(pci, vlsi_irda_table);


/********************************************************/


MODULE_PARM(clksrc, "i");
MODULE_PARM_DESC(clksrc, "clock input source selection");

/*	clksrc: which clock source to be used
 *		0: auto - try PLL, fallback to 40MHz XCLK
 *		1: on-chip 48MHz PLL
 *		2: external 48MHz XCLK
 *		3: external 40MHz XCLK (HP OB-800)
 */

static int clksrc = 0;			/* default is 0(auto) */


MODULE_PARM(ringsize, "1-2i");
MODULE_PARM_DESC(ringsize, "TX, RX ring descriptor size");

/*	ringsize: size of the tx and rx descriptor rings
 *		independent for tx and rx
 *		specify as ringsize=tx[,rx]
 *		allowed values: 4, 8, 16, 32, 64
 *		Due to the IrDA 1.x max. allowed window size=7,
 *		there should be no gain when using rings larger than 8
 */

static int ringsize[] = {8,8};		/* default is tx=rx=8 */


MODULE_PARM(sirpulse, "i");
MODULE_PARM_DESC(sirpulse, "SIR pulse width tuning");

/*	sirpulse: tuning of the SIR pulse width within IrPHY 1.3 limits
 *		0: very short, 1.5us (exception: 6us at 2.4 kbaud)
 *		1: nominal 3/16 bittime width
 *	note: IrDA compliant peer devices should be happy regardless
 *		which one is used. Primary goal is to save some power
 *		on the sender's side - at 9.6kbaud for example the short
 *		pulse width saves more than 90% of the transmitted IR power.
 */

static int sirpulse = 1;		/* default is 3/16 bittime */


MODULE_PARM(qos_mtt_bits, "i");
MODULE_PARM_DESC(qos_mtt_bits, "IrLAP bitfield representing min-turn-time");

/*	qos_mtt_bits: encoded min-turn-time value we require the peer device
 *		 to use before transmitting to us. "Type 1" (per-station)
 *		 bitfield according to IrLAP definition (section 6.6.8)
 *		 The HP HDLS-1100 requires 1 msec - don't even know
 *		 if this is the one which is used by my OB800
 */

static int qos_mtt_bits = 0x04;		/* default is 1 ms */


/********************************************************/


/* some helpers for operations on ring descriptors */


static inline int rd_is_active(struct vlsi_ring *r, unsigned i)
{
	return ((r->hw[i].rd_status & RD_STAT_ACTIVE) != 0);
}

static inline void rd_activate(struct vlsi_ring *r, unsigned i)
{
	r->hw[i].rd_status |= RD_STAT_ACTIVE;
}

static inline void rd_set_addr_status(struct vlsi_ring *r, unsigned i, dma_addr_t a, u8 s)
{
	struct ring_descr *rd = r->hw +i;

	/* ordering is important for two reasons:
	 *  - overlayed: writing addr overwrites status
	 *  - we want to write status last so we have valid address in
	 *    case status has RD_STAT_ACTIVE set
	 */

	if ((a & ~DMA_MASK_MSTRPAGE) != MSTRPAGE_VALUE)
		BUG();

	a &= DMA_MASK_MSTRPAGE;  /* clear highbyte to make sure we won't write
				  * to status - just in case MSTRPAGE_VALUE!=0
				  */
	rd->rd_addr = a;
	wmb();
	rd->rd_status = s;	 /* potentially passes ownership to the hardware */
}

static inline void rd_set_status(struct vlsi_ring *r, unsigned i, u8 s)
{
	r->hw[i].rd_status = s;
}

static inline void rd_set_count(struct vlsi_ring *r, unsigned i, u16 c)
{
	r->hw[i].rd_count = c;
}

static inline u8 rd_get_status(struct vlsi_ring *r, unsigned i)
{
	return r->hw[i].rd_status;
}

static inline dma_addr_t rd_get_addr(struct vlsi_ring *r, unsigned i)
{
	dma_addr_t	a;

	a = (r->hw[i].rd_addr & DMA_MASK_MSTRPAGE) | (MSTRPAGE_VALUE << 24);
	return a;
}

static inline u16 rd_get_count(struct vlsi_ring *r, unsigned i)
{
	return r->hw[i].rd_count;
}

/* producer advances r->head when descriptor was added for processing by hw */

static inline void ring_put(struct vlsi_ring *r)
{
	r->head = (r->head + 1) & r->mask;
}

/* consumer advances r->tail when descriptor was removed after getting processed by hw */

static inline void ring_get(struct vlsi_ring *r)
{
	r->tail = (r->tail + 1) & r->mask;
}


/********************************************************/

/* the memory required to hold the 2 descriptor rings */

#define RING_AREA_SIZE		(2 * MAX_RING_DESCR * sizeof(struct ring_descr))

/* the memory required to hold the rings' buffer entries */

#define RING_ENTRY_SIZE		(2 * MAX_RING_DESCR * sizeof(struct ring_entry))

/********************************************************/

/* just dump all registers */

static void vlsi_reg_debug(unsigned iobase, const char *s)
{
	int	i;

	mb();
	printk(KERN_DEBUG "%s: ", s);
	for (i = 0; i < 0x20; i++)
		printk("%02x", (unsigned)inb((iobase+i)));
	printk("\n");
}

/********************************************************/


static int vlsi_set_clock(struct pci_dev *pdev)
{
	u8	clkctl, lock;
	int	i, count;

	if (clksrc < 2) { /* auto or PLL: try PLL */
		clkctl = CLKCTL_NO_PD | CLKCTL_CLKSTP;
		pci_write_config_byte(pdev, VLSI_PCI_CLKCTL, clkctl);

		/* procedure to detect PLL lock synchronisation:
		 * after 0.5 msec initial delay we expect to find 3 PLL lock
		 * indications within 10 msec for successful PLL detection.
		 */
		udelay(500);
		count = 0;
		for (i = 500; i <= 10000; i += 50) { /* max 10 msec */
			pci_read_config_byte(pdev, VLSI_PCI_CLKCTL, &lock);
			if (lock&CLKCTL_LOCK) {
				if (++count >= 3)
					break;
			}
			udelay(50);
		}
		if (count < 3) {
			if (clksrc == 1) { /* explicitly asked for PLL hence bail out */
				printk(KERN_ERR "%s: no PLL or failed to lock!\n",
					__FUNCTION__);
				clkctl = CLKCTL_CLKSTP;
				pci_write_config_byte(pdev, VLSI_PCI_CLKCTL, clkctl);
				return -1;
			}
			else			/* was: clksrc=0(auto) */
				clksrc = 3;	/* fallback to 40MHz XCLK (OB800) */

			printk(KERN_INFO "%s: PLL not locked, fallback to clksrc=%d\n",
				__FUNCTION__, clksrc);
		}
		else { /* got successful PLL lock */
			clksrc = 1;
			return 0;
		}
	}

	/* we get here if either no PLL detected in auto-mode or
	   the external clock source was explicitly specified */

	clkctl = CLKCTL_EXTCLK | CLKCTL_CLKSTP;
	if (clksrc == 3)
		clkctl |= CLKCTL_XCKSEL;	
	pci_write_config_byte(pdev, VLSI_PCI_CLKCTL, clkctl);

	/* no way to test for working XCLK */

	return 0;
}


static void vlsi_start_clock(struct pci_dev *pdev)
{
	u8	clkctl;

	printk(KERN_INFO "%s: start clock using %s as input\n", __FUNCTION__,
		(clksrc&2)?((clksrc&1)?"40MHz XCLK":"48MHz XCLK"):"48MHz PLL");
	pci_read_config_byte(pdev, VLSI_PCI_CLKCTL, &clkctl);
	clkctl &= ~CLKCTL_CLKSTP;
	pci_write_config_byte(pdev, VLSI_PCI_CLKCTL, clkctl);
}
			

static void vlsi_stop_clock(struct pci_dev *pdev)
{
	u8	clkctl;

	pci_read_config_byte(pdev, VLSI_PCI_CLKCTL, &clkctl);
	clkctl |= CLKCTL_CLKSTP;
	pci_write_config_byte(pdev, VLSI_PCI_CLKCTL, clkctl);
}
			

static void vlsi_unset_clock(struct pci_dev *pdev)
{
	u8	clkctl;

	pci_read_config_byte(pdev, VLSI_PCI_CLKCTL, &clkctl);
	if (!(clkctl&CLKCTL_CLKSTP))
		/* make sure clock is already stopped */
		vlsi_stop_clock(pdev);

	clkctl &= ~(CLKCTL_EXTCLK | CLKCTL_NO_PD);
	pci_write_config_byte(pdev, VLSI_PCI_CLKCTL, clkctl);
}

/********************************************************/


/* ### FIXME: don't use old virt_to_bus() anymore! */


static void vlsi_arm_rx(struct vlsi_ring *r)
{
	unsigned	i;
	dma_addr_t	ba;

	for (i = 0; i < r->size; i++) {
		if (r->buf[i].data == NULL)
			BUG();
		ba = virt_to_bus(r->buf[i].data);
		rd_set_addr_status(r, i, ba, RD_STAT_ACTIVE);
	}
}

static int vlsi_alloc_ringbuf(struct vlsi_ring *r)
{
	unsigned	i, j;

	r->head = r->tail = 0;
	r->mask = r->size - 1;
	for (i = 0; i < r->size; i++) {
		r->buf[i].skb = NULL;
		r->buf[i].data = kmalloc(XFER_BUF_SIZE, GFP_KERNEL|GFP_DMA);
		if (r->buf[i].data == NULL) {
			for (j = 0; j < i; j++) {
				kfree(r->buf[j].data);
				r->buf[j].data = NULL;
			}
			return -ENOMEM;
		}
	}
	return 0;
}

static void vlsi_free_ringbuf(struct vlsi_ring *r)
{
	unsigned	i;

	for (i = 0; i < r->size; i++) {
		if (r->buf[i].data == NULL)
			continue;
		if (r->buf[i].skb) {
			dev_kfree_skb(r->buf[i].skb);
			r->buf[i].skb = NULL;
		}
		else
			kfree(r->buf[i].data);
		r->buf[i].data = NULL;
	}
}


static int vlsi_init_ring(vlsi_irda_dev_t *idev)
{
	char 		 *ringarea;

	ringarea = pci_alloc_consistent(idev->pdev, RING_AREA_SIZE, &idev->busaddr);
	if (!ringarea) {
		printk(KERN_ERR "%s: insufficient memory for descriptor rings\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	memset(ringarea, 0, RING_AREA_SIZE);

#if 0
	printk(KERN_DEBUG "%s: (%d,%d)-ring %p / %p\n", __FUNCTION__,
		ringsize[0], ringsize[1], ringarea, 
		(void *)(unsigned)idev->busaddr);
#endif

	idev->rx_ring.size = ringsize[1];
	idev->rx_ring.hw = (struct ring_descr *)ringarea;
	if (!vlsi_alloc_ringbuf(&idev->rx_ring)) {
		idev->tx_ring.size = ringsize[0];
		idev->tx_ring.hw = idev->rx_ring.hw + MAX_RING_DESCR;
		if (!vlsi_alloc_ringbuf(&idev->tx_ring)) {
			idev->virtaddr = ringarea;
			return 0;
		}
		vlsi_free_ringbuf(&idev->rx_ring);
	}

	pci_free_consistent(idev->pdev, RING_AREA_SIZE,
		ringarea, idev->busaddr);
	printk(KERN_ERR "%s: insufficient memory for ring buffers\n",
		__FUNCTION__);
	return -1;
}



/********************************************************/



static int vlsi_set_baud(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	unsigned long flags;
	u16 nphyctl;
	unsigned iobase; 
	u16 config;
	unsigned mode;
	int	ret;
	int	baudrate;

	baudrate = idev->new_baud;
	iobase = ndev->base_addr;

	printk(KERN_DEBUG "%s: %d -> %d\n", __FUNCTION__, idev->baud, idev->new_baud);

	spin_lock_irqsave(&idev->lock, flags);

	outw(0, iobase+VLSI_PIO_IRENABLE);

	if (baudrate == 4000000) {
		mode = IFF_FIR;
		config = IRCFG_FIR;
		nphyctl = PHYCTL_FIR;
	}
	else if (baudrate == 1152000) {
		mode = IFF_MIR;
		config = IRCFG_MIR | IRCFG_CRC16;
		nphyctl = PHYCTL_MIR(clksrc==3);
	}
	else {
		mode = IFF_SIR;
		config = IRCFG_SIR | IRCFG_SIRFILT | IRCFG_RXANY;
		switch(baudrate) {
			default:
				printk(KERN_ERR "%s: undefined baudrate %d - fallback to 9600!\n",
					__FUNCTION__, baudrate);
				baudrate = 9600;
				/* fallthru */
			case 2400:
			case 9600:
			case 19200:
			case 38400:
			case 57600:
			case 115200:
				nphyctl = PHYCTL_SIR(baudrate,sirpulse,clksrc==3);
				break;
		}
	}

	config |= IRCFG_MSTR | IRCFG_ENRX;

	outw(config, iobase+VLSI_PIO_IRCFG);

	outw(nphyctl, iobase+VLSI_PIO_NPHYCTL);
	wmb();
	outw(IRENABLE_IREN, iobase+VLSI_PIO_IRENABLE);

	/* chip fetches IRCFG on next rising edge of its 8MHz clock */

	mb();
	config = inw(iobase+VLSI_PIO_IRENABLE) & IRENABLE_MASK;

	if (mode == IFF_FIR)
		config ^= IRENABLE_FIR_ON;
	else if (mode == IFF_MIR)
		config ^= (IRENABLE_MIR_ON|IRENABLE_CRC16_ON);
	else
		config ^= IRENABLE_SIR_ON;


	if (config != (IRENABLE_IREN|IRENABLE_ENRXST)) {
		printk(KERN_ERR "%s: failed to set %s mode!\n", __FUNCTION__,
			(mode==IFF_SIR)?"SIR":((mode==IFF_MIR)?"MIR":"FIR"));
		ret = -1;
	}
	else {
		if (inw(iobase+VLSI_PIO_PHYCTL) != nphyctl) {
			printk(KERN_ERR "%s: failed to apply baudrate %d\n",
				__FUNCTION__, baudrate);
			ret = -1;
		}
		else {
			idev->mode = mode;
			idev->baud = baudrate;
			idev->new_baud = 0;
			ret = 0;
		}
	}
	spin_unlock_irqrestore(&idev->lock, flags);

	if (ret)
		vlsi_reg_debug(iobase,__FUNCTION__);

	return ret;
}



static int vlsi_init_chip(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	unsigned	iobase;
	u16 ptr;

	iobase = ndev->base_addr;

	outb(IRINTR_INT_MASK, iobase+VLSI_PIO_IRINTR); /* w/c pending IRQ, disable all INT */

	outw(0, iobase+VLSI_PIO_IRENABLE);	/* disable IrPHY-interface */

	/* disable everything, particularly IRCFG_MSTR - which resets the RING_PTR */

	outw(0, iobase+VLSI_PIO_IRCFG);
	wmb();
	outw(IRENABLE_IREN, iobase+VLSI_PIO_IRENABLE);

	mb();

	outw(0, iobase+VLSI_PIO_IRENABLE);

	outw(MAX_PACKET_LENGTH, iobase+VLSI_PIO_MAXPKT);  /* max possible value=0x0fff */

	outw(BUS_TO_RINGBASE(idev->busaddr), iobase+VLSI_PIO_RINGBASE);

	outw(TX_RX_TO_RINGSIZE(idev->tx_ring.size, idev->rx_ring.size),
		iobase+VLSI_PIO_RINGSIZE);	

	ptr = inw(iobase+VLSI_PIO_RINGPTR);
	idev->rx_ring.head = idev->rx_ring.tail = RINGPTR_GET_RX(ptr);
	idev->tx_ring.head = idev->tx_ring.tail = RINGPTR_GET_TX(ptr);

	outw(IRCFG_MSTR, iobase+VLSI_PIO_IRCFG);		/* ready for memory access */
	wmb();
	outw(IRENABLE_IREN, iobase+VLSI_PIO_IRENABLE);

	mb();

	idev->new_baud = 9600;		/* start with IrPHY using 9600(SIR) mode */
	vlsi_set_baud(ndev);

	outb(IRINTR_INT_MASK, iobase+VLSI_PIO_IRINTR);	/* just in case - w/c pending IRQ's */
	wmb();

	/* DO NOT BLINDLY ENABLE IRINTR_ACTEN!
	 * basically every received pulse fires an ACTIVITY-INT
	 * leading to >>1000 INT's per second instead of few 10
	 */

	outb(IRINTR_RPKTEN|IRINTR_TPKTEN, iobase+VLSI_PIO_IRINTR);
	wmb();

	return 0;
}


/**************************************************************/


static void vlsi_refill_rx(struct vlsi_ring *r)
{
	do {
		if (rd_is_active(r, r->head))
			BUG();
		rd_activate(r, r->head);
		ring_put(r);
	} while (r->head != r->tail);
}


static int vlsi_rx_interrupt(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct vlsi_ring *r;
	int	len;
	u8	status;
	struct sk_buff	*skb;
	int	crclen;

	r = &idev->rx_ring;
	while (!rd_is_active(r, r->tail)) {

		status = rd_get_status(r, r->tail);
		if (status & RX_STAT_ERROR) {
			idev->stats.rx_errors++;
			if (status & RX_STAT_OVER)  
				idev->stats.rx_over_errors++;
			if (status & RX_STAT_LENGTH)  
				idev->stats.rx_length_errors++;
			if (status & RX_STAT_PHYERR)  
				idev->stats.rx_frame_errors++;
			if (status & RX_STAT_CRCERR)  
				idev->stats.rx_crc_errors++;
		}
		else {
			len = rd_get_count(r, r->tail);
			crclen = (idev->mode==IFF_FIR) ? sizeof(u32) : sizeof(u16);
			if (len < crclen)
				printk(KERN_ERR "%s: strange frame (len=%d)\n",
					__FUNCTION__, len);
			else
				len -= crclen;		/* remove trailing CRC */

			skb = dev_alloc_skb(len+1);
			if (skb) {
				skb->dev = ndev;
				skb_reserve(skb,1);
				memcpy(skb_put(skb,len), r->buf[r->tail].data, len);
				idev->stats.rx_packets++;
				idev->stats.rx_bytes += len;
				skb->mac.raw = skb->data;
				skb->protocol = htons(ETH_P_IRDA);
				netif_rx(skb);				
			}
			else {
				idev->stats.rx_dropped++;
				printk(KERN_ERR "%s: rx packet dropped\n", __FUNCTION__);
			}
		}
		rd_set_count(r, r->tail, 0);
		rd_set_status(r, r->tail, 0);
		ring_get(r);
		if (r->tail == r->head) {
			printk(KERN_WARNING "%s: rx ring exhausted\n", __FUNCTION__);
			break;
		}
	}

	do_gettimeofday(&idev->last_rx); /* remember "now" for later mtt delay */

	vlsi_refill_rx(r);

	mb();
	outw(0, ndev->base_addr+VLSI_PIO_PROMPT);

	return 0;
}


static int vlsi_tx_interrupt(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct vlsi_ring	*r;
	unsigned	iobase;
	int	ret;
	u16	config;
	u16	status;

	r = &idev->tx_ring;
	while (!rd_is_active(r, r->tail)) {
		if (r->tail == r->head)
			break;	/* tx ring empty - nothing to send anymore */

		status = rd_get_status(r, r->tail);
		if (status & TX_STAT_UNDRN) {
			idev->stats.tx_errors++;
			idev->stats.tx_fifo_errors++;
		}
		else {
			idev->stats.tx_packets++;
			idev->stats.tx_bytes += rd_get_count(r, r->tail); /* not correct for SIR */
		}
		rd_set_count(r, r->tail, 0);
		rd_set_status(r, r->tail, 0);
		if (r->buf[r->tail].skb) {
			rd_set_addr_status(r, r->tail, 0, 0);
			dev_kfree_skb(r->buf[r->tail].skb);
			r->buf[r->tail].skb = NULL;
			r->buf[r->tail].data = NULL;
		}
		ring_get(r);
	}

	ret = 0;
	iobase = ndev->base_addr;

	if (r->head == r->tail) {	/* tx ring empty: re-enable rx */

		outw(0, iobase+VLSI_PIO_IRENABLE);
		config = inw(iobase+VLSI_PIO_IRCFG);
		mb();
		outw((config & ~IRCFG_ENTX) | IRCFG_ENRX, iobase+VLSI_PIO_IRCFG);
		wmb();
		outw(IRENABLE_IREN, iobase+VLSI_PIO_IRENABLE);
	}
	else
		ret = 1;			/* no speed-change-check */

	mb();
	outw(0, iobase+VLSI_PIO_PROMPT);

	if (netif_queue_stopped(ndev)) {
		netif_wake_queue(ndev);
		printk(KERN_DEBUG "%s: queue awoken\n", __FUNCTION__);
	}
	return ret;
}


#if 0	/* disable ACTIVITY handling for now */

static int vlsi_act_interrupt(struct net_device *ndev)
{
	printk(KERN_DEBUG "%s\n", __FUNCTION__);
	return 0;
}
#endif

static void vlsi_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *ndev = dev_instance;
	vlsi_irda_dev_t *idev = ndev->priv;
	unsigned	iobase;
	u8		irintr;
	int 		boguscount = 32;
	int		no_speed_check = 0;
	unsigned	got_act;
	unsigned long	flags;

	got_act = 0;
	iobase = ndev->base_addr;
	spin_lock_irqsave(&idev->lock,flags);
	do {
		irintr = inb(iobase+VLSI_PIO_IRINTR);
		rmb();
		outb(irintr, iobase+VLSI_PIO_IRINTR); /* acknowledge asap */
		wmb();

		if (!(irintr&=IRINTR_INT_MASK))		/* not our INT - probably shared */
			break;

//		vlsi_reg_debug(iobase,__FUNCTION__);

		if (irintr&IRINTR_RPKTINT)
			no_speed_check |= vlsi_rx_interrupt(ndev);

		if (irintr&IRINTR_TPKTINT)
			no_speed_check |= vlsi_tx_interrupt(ndev);

#if 0	/* disable ACTIVITY handling for now */

		if (got_act  &&  irintr==IRINTR_ACTIVITY) /* nothing new */
			break;

		if ((irintr&IRINTR_ACTIVITY) && !(irintr^IRINTR_ACTIVITY) ) {
			no_speed_check |= vlsi_act_interrupt(ndev);
			got_act = 1;
		}
#endif
		if (irintr & ~(IRINTR_RPKTINT|IRINTR_TPKTINT|IRINTR_ACTIVITY))
			printk(KERN_DEBUG "%s: IRINTR = %02x\n",
				__FUNCTION__, (unsigned)irintr);
			
	} while (--boguscount > 0);
	spin_unlock_irqrestore(&idev->lock,flags);

	if (boguscount <= 0)
		printk(KERN_ERR "%s: too much work in interrupt!\n", __FUNCTION__);

	else if (!no_speed_check) {
		if (idev->new_baud)
			vlsi_set_baud(ndev);
	}
}


/**************************************************************/


/* writing all-zero to the VLSI PCI IO register area seems to prevent
 * some occasional situations where the hardware fails (symptoms are 
 * what appears as stalled tx/rx state machines, i.e. everything ok for
 * receive or transmit but hw makes no progress or is unable to access
 * the bus memory locations).
 * Best place to call this is immediately after/before the internal clock
 * gets started/stopped.
 */

static inline void vlsi_clear_regs(unsigned iobase)
{
	unsigned	i;
	const unsigned	chip_io_extent = 32;

	for (i = 0; i < chip_io_extent; i += sizeof(u16))
		outw(0, iobase + i);
}


static int vlsi_open(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct pci_dev *pdev = idev->pdev;
	int	err;
	char	hwname[32];

	if (pci_request_regions(pdev,drivername)) {
		printk(KERN_ERR "%s: io resource busy\n", __FUNCTION__);
		return -EAGAIN;
	}

	/* under some rare occasions the chip apparently comes up
	 * with IRQ's pending. So we get interrupts invoked much too early
	 * which will immediately kill us again :-(
	 * so we better w/c pending IRQ and disable them all
	 */

	outb(IRINTR_INT_MASK, ndev->base_addr+VLSI_PIO_IRINTR);

	if (request_irq(ndev->irq, vlsi_interrupt, SA_SHIRQ,
			drivername, ndev)) {
		printk(KERN_ERR "%s: couldn't get IRQ: %d\n",
			__FUNCTION__, ndev->irq);
		pci_release_regions(pdev);
		return -EAGAIN;
	}
	printk(KERN_INFO "%s: got resources for %s - irq=%d / io=%04lx\n",
		__FUNCTION__, ndev->name, ndev->irq, ndev->base_addr );

	if (vlsi_set_clock(pdev)) {
		printk(KERN_ERR "%s: no valid clock source\n",
			__FUNCTION__);
		free_irq(ndev->irq,ndev);
		pci_release_regions(pdev);
		return -EIO;
	}

	vlsi_start_clock(pdev);

	vlsi_clear_regs(ndev->base_addr);

	err = vlsi_init_ring(idev);
	if (err) {
		vlsi_unset_clock(pdev);
		free_irq(ndev->irq,ndev);
		pci_release_regions(pdev);
		return err;
	}

	vlsi_init_chip(ndev);

	printk(KERN_INFO "%s: IrPHY setup: %d baud (%s), %s SIR-pulses\n",
		__FUNCTION__, idev->baud, 
		(idev->mode==IFF_SIR)?"SIR":((idev->mode==IFF_MIR)?"MIR":"FIR"),
		(sirpulse)?"3/16 bittime":"short");

	vlsi_arm_rx(&idev->rx_ring);

	do_gettimeofday(&idev->last_rx);  /* first mtt may start from now on */

	sprintf(hwname, "VLSI-FIR @ 0x%04x", (unsigned)ndev->base_addr);
	idev->irlap = irlap_open(ndev,&idev->qos,hwname);

	netif_start_queue(ndev);
	outw(0, ndev->base_addr+VLSI_PIO_PROMPT);	/* kick hw state machine */

	printk(KERN_INFO "%s: device %s operational using (%d,%d) tx,rx-ring\n",
		__FUNCTION__, ndev->name, ringsize[0], ringsize[1]);

	return 0;
}


static int vlsi_close(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct pci_dev *pdev = idev->pdev;
	u8	cmd;
	unsigned iobase;


	iobase = ndev->base_addr;
	netif_stop_queue(ndev);

	if (idev->irlap)
		irlap_close(idev->irlap);
	idev->irlap = NULL;

	outb(IRINTR_INT_MASK, iobase+VLSI_PIO_IRINTR);	/* w/c pending + disable further IRQ */
	wmb();
	outw(0, iobase+VLSI_PIO_IRENABLE);
	outw(0, iobase+VLSI_PIO_IRCFG);			/* disable everything */
	wmb();
	outw(IRENABLE_IREN, iobase+VLSI_PIO_IRENABLE);
	mb();						/* ... from now on */

	outw(0, iobase+VLSI_PIO_IRENABLE);
	wmb();

	vlsi_clear_regs(ndev->base_addr);

	vlsi_stop_clock(pdev);

	vlsi_unset_clock(pdev);

	free_irq(ndev->irq,ndev);

	vlsi_free_ringbuf(&idev->rx_ring);
	vlsi_free_ringbuf(&idev->tx_ring);

	if (idev->busaddr)
		pci_free_consistent(idev->pdev,RING_AREA_SIZE,idev->virtaddr,idev->busaddr);

	idev->virtaddr = NULL;
	idev->busaddr = 0;

	pci_read_config_byte(pdev, PCI_COMMAND, &cmd);
	cmd &= ~PCI_COMMAND_MASTER;
	pci_write_config_byte(pdev, PCI_COMMAND, cmd);

	pci_release_regions(pdev);

	printk(KERN_INFO "%s: device %s stopped\n", __FUNCTION__, ndev->name);

	return 0;
}

static struct net_device_stats * vlsi_get_stats(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;

	return &idev->stats;
}

static int vlsi_hard_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct vlsi_ring	*r;
	unsigned long flags;
	unsigned iobase;
	u8 status;
	u16 config;
	int mtt;
	int len, speed;
	struct timeval  now, ready;


	status = 0;

	speed = irda_get_next_speed(skb);

	if (speed != -1  &&  speed != idev->baud) {
		idev->new_baud = speed;
		if (!skb->len) {
			dev_kfree_skb(skb);
			vlsi_set_baud(ndev);
			return 0;
		}
		status = TX_STAT_CLRENTX;  /* stop tx-ring after this frame */
	}

	if (skb->len == 0) {
		printk(KERN_ERR "%s: blocking 0-size packet???\n",
			__FUNCTION__);
		dev_kfree_skb(skb);
		return 0;
	}

	r = &idev->tx_ring;

	if (rd_is_active(r, r->head))
		BUG();

	if (idev->mode == IFF_SIR) {
		status |= TX_STAT_DISCRC;
		len = async_wrap_skb(skb, r->buf[r->head].data, XFER_BUF_SIZE);
	}
	else {				/* hw deals with MIR/FIR mode */
		len = skb->len;
		memcpy(r->buf[r->head].data, skb->data, len);
	}

	rd_set_count(r, r->head, len);
	rd_set_addr_status(r, r->head, virt_to_bus(r->buf[r->head].data), status);

	/* new entry not yet activated! */

#if 0
	printk(KERN_DEBUG "%s: dump entry %d: %u %02x %08x\n",
		__FUNCTION__, r->head,
		idev->ring_hw[r->head].rd_count,
		(unsigned)idev->ring_hw[r->head].rd_status,
		idev->ring_hw[r->head].rd_addr & 0xffffffff);
	vlsi_reg_debug(iobase,__FUNCTION__);
#endif


	/* let mtt delay pass before we need to acquire the spinlock! */

	if ((mtt = irda_get_mtt(skb)) > 0) {
	
		ready.tv_usec = idev->last_rx.tv_usec + mtt;
		ready.tv_sec = idev->last_rx.tv_sec;
		if (ready.tv_usec >= 1000000) {
			ready.tv_usec -= 1000000;
			ready.tv_sec++;		/* IrLAP 1.1: mtt always < 1 sec */
		}
		for(;;) {
			do_gettimeofday(&now);
			if (now.tv_sec > ready.tv_sec
			    ||  (now.tv_sec==ready.tv_sec && now.tv_usec>=ready.tv_usec))
			    	break;
			udelay(100);
		}
	}

/*
 *	race window ahead, due to concurrent controller processing!
 *
 *	We need to disable IR output in order to switch to TX mode.
 *	Better not do this blindly anytime we want to transmit something
 *	because TX may already run. However the controller may stop TX
 *	at any time when fetching an inactive descriptor or one with
 *	CLR_ENTX set. So we switch on TX only, if TX was not running
 *	_after_ the new descriptor was activated on the ring. This ensures
 *	we will either find TX already stopped or we can be sure, there
 *	will be a TX-complete interrupt even if the chip stopped doing
 *	TX just after we found it still running. The ISR will then find
 *	the non-empty ring and restart TX processing. The enclosing
 *	spinlock is required to get serialization with the ISR right.
 */


	iobase = ndev->base_addr;

	spin_lock_irqsave(&idev->lock,flags);

	rd_activate(r, r->head);
	ring_put(r);

	if (!(inw(iobase+VLSI_PIO_IRENABLE) & IRENABLE_ENTXST)) {
	
		outw(0, iobase+VLSI_PIO_IRENABLE);

		config = inw(iobase+VLSI_PIO_IRCFG);
		rmb();
		outw(config | IRCFG_ENTX, iobase+VLSI_PIO_IRCFG);
		wmb();
		outw(IRENABLE_IREN, iobase+VLSI_PIO_IRENABLE);
		mb();
		outw(0, iobase+VLSI_PIO_PROMPT);
		wmb();
	}

	if (r->head == r->tail) {
		netif_stop_queue(ndev);
		printk(KERN_DEBUG "%s: tx ring full - queue stopped: %d/%d\n",
			__FUNCTION__, r->head, r->tail);
#if 0
		printk(KERN_INFO "%s: dump stalled entry %d: %u %02x %08x\n",
			__FUNCTION__, r->tail,
			r->hw[r->tail].rd_count,
			(unsigned)r->hw[r->tail].rd_status,
			r->hw[r->tail].rd_addr & 0xffffffff);
#endif
		vlsi_reg_debug(iobase,__FUNCTION__);
	}

	spin_unlock_irqrestore(&idev->lock, flags);

	dev_kfree_skb(skb);	

	return 0;
}


static int vlsi_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	unsigned long flags;
	u16 fifocnt;
	int ret = 0;

	spin_lock_irqsave(&idev->lock,flags);
	switch (cmd) {
		case SIOCSBANDWIDTH:
			if (!capable(CAP_NET_ADMIN)) {
				ret = -EPERM;
				break;
			}
			idev->new_baud = irq->ifr_baudrate;
			break;
		case SIOCSMEDIABUSY:
			if (!capable(CAP_NET_ADMIN)) {
				ret = -EPERM;
				break;
			}
			irda_device_set_media_busy(ndev, TRUE);
			break;
		case SIOCGRECEIVING:
			/* the best we can do: check whether there are any bytes in rx fifo.
			 * The trustable window (in case some data arrives just afterwards)
			 * may be as short as 1usec or so at 4Mbps - no way for future-telling.
			 */
			fifocnt = inw(ndev->base_addr+VLSI_PIO_RCVBCNT) & RCVBCNT_MASK;
			irq->ifr_receiving = (fifocnt!=0) ? 1 : 0;
			break;
		default:
			printk(KERN_ERR "%s: notsupp - cmd=%04x\n",
				__FUNCTION__, cmd);
			ret = -EOPNOTSUPP;
	}	
	spin_unlock_irqrestore(&idev->lock,flags);
	
	return ret;
}



int vlsi_irda_init(struct net_device *ndev)
{
	vlsi_irda_dev_t *idev = ndev->priv;
	struct pci_dev *pdev = idev->pdev;
	u8	byte;


	SET_MODULE_OWNER(ndev);

	ndev->irq = pdev->irq;
	ndev->base_addr = pci_resource_start(pdev,0);

	/* PCI busmastering - see include file for details! */

	if (pci_set_dma_mask(pdev,DMA_MASK_USED_BY_HW)) {
		printk(KERN_ERR "%s: aborting due to PCI BM-DMA address limitations\n",
			__FUNCTION__);
		return -1;
	}
	pci_set_master(pdev);
	pdev->dma_mask = DMA_MASK_MSTRPAGE;
	pci_write_config_byte(pdev, VLSI_PCI_MSTRPAGE, MSTRPAGE_VALUE);

	/* we don't use the legacy UART, disable its address decoding */

	pci_read_config_byte(pdev, VLSI_PCI_IRMISC, &byte);
	byte &= ~(IRMISC_UARTEN | IRMISC_UARTTST);
	pci_write_config_byte(pdev, VLSI_PCI_IRMISC, byte);


	irda_init_max_qos_capabilies(&idev->qos);

	/* the VLSI82C147 does not support 576000! */

	idev->qos.baud_rate.bits = IR_2400 | IR_9600
		| IR_19200 | IR_38400 | IR_57600 | IR_115200
		| IR_1152000 | (IR_4000000 << 8);

	idev->qos.min_turn_time.bits = qos_mtt_bits;

	irda_qos_bits_to_value(&idev->qos);

	irda_device_setup(ndev);

	/* currently no public media definitions for IrDA */

	ndev->flags |= IFF_PORTSEL | IFF_AUTOMEDIA;
	ndev->if_port = IF_PORT_UNKNOWN;
 
	ndev->open	      = vlsi_open;
	ndev->stop	      = vlsi_close;
	ndev->get_stats	      = vlsi_get_stats;
	ndev->hard_start_xmit = vlsi_hard_start_xmit;
	ndev->do_ioctl	      = vlsi_ioctl;

	return 0;
}	

/**************************************************************/

static int __devinit
vlsi_irda_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device	*ndev;
	vlsi_irda_dev_t		*idev;
	int			alloc_size;


	if (pci_enable_device(pdev))
		goto out;

	printk(KERN_INFO "%s: IrDA PCI controller %s detected\n",
		drivername, pdev->name);

	if ( !pci_resource_start(pdev,0)
	     || !(pci_resource_flags(pdev,0) & IORESOURCE_IO) ) {
		printk(KERN_ERR "%s: bar 0 invalid", __FUNCTION__);
		goto out_disable;
	}

	alloc_size = sizeof(*ndev) + sizeof(*idev);

	ndev = (struct net_device *) kmalloc (alloc_size, GFP_KERNEL);
	if (ndev==NULL) {
		printk(KERN_ERR "%s: Unable to allocate device memory.\n",
			__FUNCTION__);
		goto out_disable;
	}

	memset(ndev, 0, alloc_size);

	idev = (vlsi_irda_dev_t *) (ndev + 1);
	ndev->priv = (void *) idev;

	spin_lock_init(&idev->lock);
	idev->pdev = pdev;
	ndev->init = vlsi_irda_init;
	strcpy(ndev->name,"irda%d");
	if (register_netdev(ndev)) {
		printk(KERN_ERR "%s: register_netdev failed\n",
			__FUNCTION__);
		goto out_freedev;
	}
	printk(KERN_INFO "%s: registered device %s\n", drivername, ndev->name);

	pci_set_drvdata(pdev, ndev);

	return 0;

out_freedev:
	kfree(ndev);
out_disable:
	pci_disable_device(pdev);
out:
	pci_set_drvdata(pdev, NULL);
	return -ENODEV;
}

static void __devexit vlsi_irda_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);

	if (ndev) {
		printk(KERN_INFO "%s: unregister device %s\n",
			drivername, ndev->name);

		unregister_netdev(ndev);
		/* do not free - async completed by unregister_netdev()
		 * ndev->destructor called (if present) when going to free
		 */

	}
	else
		printk(KERN_CRIT "%s: lost netdevice?\n", drivername);
	pci_set_drvdata(pdev, NULL);

	pci_disable_device(pdev);
	printk(KERN_INFO "%s: %s disabled\n", drivername, pdev->name);
}

static int vlsi_irda_suspend(struct pci_dev *pdev, u32 state)
{
	printk(KERN_ERR "%s - %s\n", __FUNCTION__, pdev->name);
	return 0;
}

static int vlsi_irda_resume(struct pci_dev *pdev)
{
	printk(KERN_ERR "%s - %s\n", __FUNCTION__, pdev->name);
	return 0;
}

/*********************************************************/

static struct pci_driver vlsi_irda_driver = {
	name:           drivername,
	id_table:       vlsi_irda_table,
	probe:          vlsi_irda_probe,
	remove:         __devexit_p(vlsi_irda_remove),
	suspend:        vlsi_irda_suspend,
	resume:         vlsi_irda_resume,
};

static int __init vlsi_mod_init(void)
{
	int	i;

	if (clksrc < 0  ||  clksrc > 3) {
		printk(KERN_ERR "%s: invalid clksrc=%d\n", drivername, clksrc);
		return -1;
	}

	for (i = 0; i < 2; i++) {
		switch(ringsize[i]) {
			case 4:
			case 8:
			case 16:
			case 32:
			case 64:
				break;
			default:
				printk(KERN_WARNING "%s: invalid %s ringsize %d",
					drivername, (i)?"rx":"tx", ringsize[i]);
				printk(", using default=8\n");
				ringsize[i] = 8;
				break;
		}
	} 

	sirpulse = !!sirpulse;

	return pci_module_init(&vlsi_irda_driver);
}

static void __exit vlsi_mod_exit(void)
{
	pci_unregister_driver(&vlsi_irda_driver);
}

module_init(vlsi_mod_init);
module_exit(vlsi_mod_exit);

