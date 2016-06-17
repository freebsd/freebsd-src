/*
 *  linux/drivers/net/irda/sa1100_ir.c
 *
 *  Copyright (C) 2000-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Infra-red driver for the StrongARM SA1100 embedded microprocessor
 *
 *  Note that we don't have to worry about the SA1111's DMA bugs in here,
 *  so we use the straight forward pci_map_* functions with a null pointer.
 *  IMHO we should really be using our own machine specific set.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/rtnetlink.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/pm.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/wrapper.h>
#include <net/irda/irda_device.h>

#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

#include <asm/arch/assabet.h>

#ifndef CONFIG_SA1100_H3600
#define clr_h3600_egpio(x)	do { } while (0)
#define set_h3600_egpio(x)	do { } while (0)
#endif

#ifndef GPIO_IRDA_FIR
#define GPIO_IRDA_FIR		(0)
#endif

#ifndef GPIO_IRDA_POWER
#define GPIO_IRDA_POWER		(0)
#endif

static int power_level = 3;
static int tx_lpm = 0;

/*
 * Our netdevice.  There is only ever one of these.
 */
static struct net_device *netdev;

struct sa1100_irda {
	unsigned char		hscr0;
	unsigned char		utcr4;
	unsigned char		power;
	unsigned char		open;

	int			speed;
	int			newspeed;

	struct sk_buff		*txskb;
	struct sk_buff		*rxskb;
	dma_addr_t		txbuf_dma;
	dma_addr_t		rxbuf_dma;
	int			txdma;
	int			rxdma;

	struct net_device_stats	stats;
	struct irlap_cb		*irlap;
	struct pm_dev		*pmdev;
	struct qos_info		qos;

	iobuff_t		tx_buff;
	iobuff_t		rx_buff;
};

#define IS_FIR(si)		((si)->speed >= 4000000)

#define HPSIR_MAX_RXLEN		2047

/*
 * Allocate and map the receive buffer, unless it is already allocated.
 */
static int sa1100_irda_rx_alloc(struct sa1100_irda *si)
{
	if (si->rxskb)
		return 0;

	si->rxskb = alloc_skb(HPSIR_MAX_RXLEN + 1, GFP_ATOMIC);

	if (!si->rxskb) {
		printk(KERN_ERR "sa1100_ir: out of memory for RX SKB\n");
		return -ENOMEM;
	}

	/*
	 * Align any IP headers that may be contained
	 * within the frame.
	 */
	skb_reserve(si->rxskb, 1);

	si->rxbuf_dma = pci_map_single(NULL, si->rxskb->data,
					HPSIR_MAX_RXLEN,
					PCI_DMA_FROMDEVICE);
	return 0;
}

/*
 * We want to get here as soon as possible, and get the receiver setup.
 * We use the existing buffer.
 */
static void sa1100_irda_rx_dma_start(struct sa1100_irda *si)
{
	if (!si->rxskb) {
		printk(KERN_ERR "sa1100_ir: rx buffer went missing\n");
		return;
	}

	/*
	 * First empty receive FIFO
	 */
	Ser2HSCR0 = si->hscr0 | HSCR0_HSSP;

	/*
	 * Enable the DMA, receiver and receive interrupt.
	 */
	sa1100_dma_flush_all(si->rxdma);
	sa1100_dma_queue_buffer(si->rxdma, NULL, si->rxbuf_dma, HPSIR_MAX_RXLEN);
	Ser2HSCR0 = si->hscr0 | HSCR0_HSSP | HSCR0_RXE;
}

/*
 * Set the IrDA communications speed.
 */
static int sa1100_irda_set_speed(struct sa1100_irda *si, int speed)
{
	unsigned long flags;
	int brd, ret = -EINVAL;

	switch (speed) {
	case 9600:	case 19200:	case 38400:
	case 57600:	case 115200:
		brd = 3686400 / (16 * speed) - 1;

		/*
		 * Stop the receive DMA.
		 */
		if (IS_FIR(si))
			sa1100_dma_stop(si->rxdma);

		local_irq_save(flags);

		Ser2UTCR3 = 0;
		Ser2HSCR0 = HSCR0_UART;

		Ser2UTCR1 = brd >> 8;
		Ser2UTCR2 = brd;

		/*
		 * Clear status register
		 */
		Ser2UTSR0 = UTSR0_REB | UTSR0_RBB | UTSR0_RID;
		Ser2UTCR3 = UTCR3_RIE | UTCR3_RXE | UTCR3_TXE;

		if (machine_is_assabet())
			ASSABET_BCR_clear(ASSABET_BCR_IRDA_FSEL);
		if (machine_is_h3600())
			clr_h3600_egpio(EGPIO_H3600_IR_FSEL);
		if (machine_is_yopy())
			PPSR &= ~GPIO_IRDA_FIR;

		si->speed = speed;

		local_irq_restore(flags);
		ret = 0;
		break;

	case 4000000:
		save_flags(flags);
		cli();

		si->hscr0 = 0;

		Ser2HSSR0 = 0xff;
		Ser2HSCR0 = si->hscr0 | HSCR0_HSSP;
		Ser2UTCR3 = 0;

		si->speed = speed;

		if (machine_is_assabet())
			ASSABET_BCR_set(ASSABET_BCR_IRDA_FSEL);
		if (machine_is_h3600())
			set_h3600_egpio(EGPIO_H3600_IR_FSEL);
		if (machine_is_yopy())
			PPSR |= GPIO_IRDA_FIR;

		sa1100_irda_rx_alloc(si);
		sa1100_irda_rx_dma_start(si);

		restore_flags(flags);

		break;

	default:
		break;
	}

	return ret;
}

/*
 * This sets the IRDA power level on the Assabet.
 */
static inline int
sa1100_irda_set_power_assabet(struct sa1100_irda *si, unsigned int state)
{
	static unsigned int bcr_state[4] = {
		ASSABET_BCR_IRDA_MD0,
		ASSABET_BCR_IRDA_MD1|ASSABET_BCR_IRDA_MD0,
		ASSABET_BCR_IRDA_MD1,
		0
	};

	if (state < 4) {
		state = bcr_state[state];
		ASSABET_BCR_clear(state ^ (ASSABET_BCR_IRDA_MD1|
					   ASSABET_BCR_IRDA_MD0));
		ASSABET_BCR_set(state);
	}
	return 0;
}

/*
 * This turns the IRDA power on or off on the Compaq H3600
 */
static inline int
sa1100_irda_set_power_h3600(struct sa1100_irda *si, unsigned int state)
{
	if (state)
		set_h3600_egpio(EGPIO_H3600_IR_ON);
	else
		clr_h3600_egpio(EGPIO_H3600_IR_ON);
	return 0;
}

/*
 * This turns the IRDA power on or off on the Yopy
 */
static inline int
sa1100_irda_set_power_yopy(struct sa1100_irda *si, unsigned int state)
{
	if (state)
		PPSR &= ~GPIO_IRDA_POWER;
	else
		PPSR |= GPIO_IRDA_POWER;
	return 0;
}

/*
 * Control the power state of the IrDA transmitter.
 * State:
 *  0 - off
 *  1 - short range, lowest power
 *  2 - medium range, medium power
 *  3 - maximum range, high power
 *
 * Currently, only assabet is known to support this.
 */
static int
__sa1100_irda_set_power(struct sa1100_irda *si, unsigned int state)
{
	int ret = 0;

	if (machine_is_assabet())
		ret = sa1100_irda_set_power_assabet(si, state);
	if (machine_is_h3600())
		ret = sa1100_irda_set_power_h3600(si, state);
	if (machine_is_yopy())
		ret = sa1100_irda_set_power_yopy(si, state);

	return ret;
}

static inline int
sa1100_set_power(struct sa1100_irda *si, unsigned int state)
{
	int ret;

	ret = __sa1100_irda_set_power(si, state);
	if (ret == 0)
		si->power = state;

	return ret;
}

static int sa1100_irda_startup(struct sa1100_irda *si)
{
	int ret;

	/*
	 * Ensure that the ports for this device are setup correctly.
	 */
	if (machine_is_yopy()) {
		PPDR |= GPIO_IRDA_POWER | GPIO_IRDA_FIR;
		PPSR |= GPIO_IRDA_POWER | GPIO_IRDA_FIR;
		PSDR |= GPIO_IRDA_POWER | GPIO_IRDA_FIR;
	}

	/*
	 * Configure PPC for IRDA - we want to drive TXD2 low.
	 * We also want to drive this pin low during sleep.
	 */
	PPSR &= ~PPC_TXD2;
	PSDR &= ~PPC_TXD2;
	PPDR |= PPC_TXD2;

	/*
	 * Enable HP-SIR modulation, and ensure that the port is disabled.
	 */
	Ser2UTCR3 = 0;
	Ser2HSCR0 = HSCR0_UART;
	Ser2UTCR4 = si->utcr4;
	Ser2UTCR0 = UTCR0_8BitData;
	Ser2HSCR2 = HSCR2_TrDataH | HSCR2_RcDataL;

	/*
	 * Clear status register
	 */
	Ser2UTSR0 = UTSR0_REB | UTSR0_RBB | UTSR0_RID;

	ret = sa1100_irda_set_speed(si, si->speed = 9600);
	if (ret)
		return ret;

	return 0;
}

static void sa1100_irda_shutdown(struct sa1100_irda *si)
{
	/*
	 * Stop all DMA activity.
	 */
	sa1100_dma_stop(si->rxdma);
	sa1100_dma_stop(si->txdma);

	/* Disable the port. */
	Ser2UTCR3 = 0;
	Ser2HSCR0 = 0;
}

#ifdef CONFIG_PM
/*
 * Suspend the IrDA interface.
 */
static int sa1100_irda_suspend(struct net_device *dev, int state)
{
	struct sa1100_irda *si = dev->priv;

	if (si && si->open) {
		/*
		 * Stop the transmit queue
		 */
		netif_device_detach(dev);
		disable_irq(dev->irq);
		sa1100_irda_shutdown(si);
		__sa1100_irda_set_power(si, 0);
	}

	return 0;
}

/*
 * Resume the IrDA interface.
 */
static int sa1100_irda_resume(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;

	if (si && si->open) {
		/*
		 * If we missed a speed change, initialise at the new speed
		 * directly.  It is debatable whether this is actually
		 * required, but in the interests of continuing from where
		 * we left off it is desireable.  The converse argument is
		 * that we should re-negotiate at 9600 baud again.
		 */
		if (si->newspeed) {
			si->speed = si->newspeed;
			si->newspeed = 0;
		}

		sa1100_irda_startup(si);
		__sa1100_irda_set_power(si, si->power);
		enable_irq(dev->irq);

		/*
		 * This automatically wakes up the queue
		 */
		netif_device_attach(dev);
	}

	return 0;
}

static int sa1100_irda_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	int ret;

	if (!dev->data)
		return -EINVAL;

	switch (rqst) {
	case PM_SUSPEND:
		ret = sa1100_irda_suspend((struct net_device *)dev->data,
					  (int)data);
		break;

	case PM_RESUME:
		ret = sa1100_irda_resume((struct net_device *)dev->data);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

/*
 * HP-SIR format interrupt service routines.
 */
static void sa1100_irda_hpsir_irq(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;
	int status;

	status = Ser2UTSR0;

	/*
	 * Deal with any receive errors first.  The bytes in error may be
	 * the only bytes in the receive FIFO, so we do this first.
	 */
	while (status & UTSR0_EIF) {
		int stat, data;

		stat = Ser2UTSR1;
		data = Ser2UTDR;

		if (stat & (UTSR1_FRE | UTSR1_ROR)) {
			si->stats.rx_errors++;
			if (stat & UTSR1_FRE)
				si->stats.rx_frame_errors++;
			if (stat & UTSR1_ROR)
				si->stats.rx_fifo_errors++;
		} else
			async_unwrap_char(dev, &si->stats, &si->rx_buff, data);

		status = Ser2UTSR0;
	}

	/*
	 * We must clear certain bits.
	 */
	Ser2UTSR0 = status & (UTSR0_RID | UTSR0_RBB | UTSR0_REB);

	if (status & UTSR0_RFS) {
		/*
		 * There are at least 4 bytes in the FIFO.  Read 3 bytes
		 * and leave the rest to the block below.
		 */
		async_unwrap_char(dev, &si->stats, &si->rx_buff, Ser2UTDR);
		async_unwrap_char(dev, &si->stats, &si->rx_buff, Ser2UTDR);
		async_unwrap_char(dev, &si->stats, &si->rx_buff, Ser2UTDR);
	}

	if (status & (UTSR0_RFS | UTSR0_RID)) {
		/*
		 * Fifo contains more than 1 character.
		 */
		do {
			async_unwrap_char(dev, &si->stats, &si->rx_buff,
					  Ser2UTDR);
		} while (Ser2UTSR1 & UTSR1_RNE);

		dev->last_rx = jiffies;
	}

	if (status & UTSR0_TFS && si->tx_buff.len) {
		/*
		 * Transmitter FIFO is not full
		 */
		do {
			Ser2UTDR = *si->tx_buff.data++;
			si->tx_buff.len -= 1;
		} while (Ser2UTSR1 & UTSR1_TNF && si->tx_buff.len);

		if (si->tx_buff.len == 0) {
			si->stats.tx_packets++;
			si->stats.tx_bytes += si->tx_buff.data -
					      si->tx_buff.head;

			/*
			 * We need to ensure that the transmitter has
			 * finished.
			 */
			do
				rmb();
			while (Ser2UTSR1 & UTSR1_TBY);

			/*
			 * Ok, we've finished transmitting.  Now enable
			 * the receiver.  Sometimes we get a receive IRQ
			 * immediately after a transmit...
			 */
			Ser2UTSR0 = UTSR0_REB | UTSR0_RBB | UTSR0_RID;
			Ser2UTCR3 = UTCR3_RIE | UTCR3_RXE | UTCR3_TXE;

			if (si->newspeed) {
				sa1100_irda_set_speed(si, si->newspeed);
				si->newspeed = 0;
			}

			/* I'm hungry! */
			netif_wake_queue(dev);
		}
	}
}

static void sa1100_irda_fir_error(struct sa1100_irda *si, struct net_device *dev)
{
	struct sk_buff *skb = si->rxskb;
	dma_addr_t dma_addr;
	unsigned int len, stat, data;

	if (!skb) {
		printk(KERN_ERR "sa1100_ir: SKB is NULL!\n");
		return;
	}

	/*
	 * Get the current data position.
	 */
	sa1100_dma_get_current(si->rxdma, NULL, &dma_addr);
	len = dma_addr - si->rxbuf_dma;
	pci_unmap_single(NULL, si->rxbuf_dma, len, PCI_DMA_FROMDEVICE);

	do {
		/*
		 * Read Status, and then Data.
		 */
		stat = Ser2HSSR1;
		rmb();
		data = Ser2HSDR;

		if (stat & (HSSR1_CRE | HSSR1_ROR)) {
			si->stats.rx_errors++;
			if (stat & HSSR1_CRE)
				si->stats.rx_crc_errors++;
			if (stat & HSSR1_ROR)
				si->stats.rx_frame_errors++;
		} else
			skb->data[len++] = data;

		/*
		 * If we hit the end of frame, there's
		 * no point in continuing.
		 */
		if (stat & HSSR1_EOF)
			break;
	} while (Ser2HSSR0 & HSSR0_EIF);

	if (stat & HSSR1_EOF) {
		si->rxskb = NULL;

		skb_put(skb, len);
		skb->dev = dev;
		skb->mac.raw = skb->data;
		skb->protocol = htons(ETH_P_IRDA);
		si->stats.rx_packets++;
		si->stats.rx_bytes += len;

		/*
		 * Before we pass the buffer up, allocate a new one.
		 */
		sa1100_irda_rx_alloc(si);

		netif_rx(skb);
	} else {
		/*
		 * Remap the buffer.
		 */
		si->rxbuf_dma = pci_map_single(NULL, si->rxskb->data,
						HPSIR_MAX_RXLEN,
						PCI_DMA_FROMDEVICE);
	}
}

/*
 * FIR format interrupt service routine.  We only have to
 * handle RX events; transmit events go via the TX DMA handler.
 *
 * No matter what, we disable RX, process, and the restart RX.
 */
static void sa1100_irda_fir_irq(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;

	/*
	 * Stop RX DMA
	 */
	sa1100_dma_stop(si->rxdma);

	/*
	 * Framing error - we throw away the packet completely.
	 * Clearing RXE flushes the error conditions and data
	 * from the fifo.
	 */
	if (Ser2HSSR0 & (HSSR0_FRE | HSSR0_RAB)) {
		si->stats.rx_errors++;

		if (Ser2HSSR0 & HSSR0_FRE)
			si->stats.rx_frame_errors++;

		/*
		 * Clear out the DMA...
		 */
		Ser2HSCR0 = si->hscr0 | HSCR0_HSSP;

		/*
		 * Clear selected status bits now, so we
		 * don't miss them next time around.
		 */
		Ser2HSSR0 = HSSR0_FRE | HSSR0_RAB;
	}

	/*
	 * Deal with any receive errors.  The any of the lowest
	 * 8 bytes in the FIFO may contain an error.  We must read
	 * them one by one.  The "error" could even be the end of
	 * packet!
	 */
	if (Ser2HSSR0 & HSSR0_EIF)
		sa1100_irda_fir_error(si, dev);

	/*
	 * No matter what happens, we must restart reception.
	 */
	sa1100_irda_rx_dma_start(si);
}

static void sa1100_irda_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	if (IS_FIR(((struct sa1100_irda *)dev->priv)))
		sa1100_irda_fir_irq(dev);
	else
		sa1100_irda_hpsir_irq(dev);
}

/*
 * TX DMA completion handler.
 */
static void sa1100_irda_txdma_irq(void *id, int len)
{
	struct net_device *dev = id;
	struct sa1100_irda *si = dev->priv;
	struct sk_buff *skb = si->txskb;

	si->txskb = NULL;

	/*
	 * Wait for the transmission to complete.  Unfortunately,
	 * the hardware doesn't give us an interrupt to indicate
	 * "end of frame".
	 */
	do
		rmb();
	while (!(Ser2HSSR0 & HSSR0_TUR) || Ser2HSSR1 & HSSR1_TBY);

	/*
	 * Clear the transmit underrun bit.
	 */
	Ser2HSSR0 = HSSR0_TUR;

	/*
	 * Do we need to change speed?  Note that we're lazy
	 * here - we don't free the old rxskb.  We don't need
	 * to allocate a buffer either.
	 */
	if (si->newspeed) {
		sa1100_irda_set_speed(si, si->newspeed);
		si->newspeed = 0;
	}

	/*
	 * Start reception.  This disables the transmitter for
	 * us.  This will be using the existing RX buffer.
	 */
	sa1100_irda_rx_dma_start(si);

	/*
	 * Account and free the packet.
	 */
	if (skb) {
		pci_unmap_single(NULL, si->txbuf_dma, len, PCI_DMA_TODEVICE);
		si->stats.tx_packets ++;
		si->stats.tx_bytes += len;
		dev_kfree_skb_irq(skb);
	}

	/*
	 * Make sure that the TX queue is available for sending
	 * (for retries).  TX has priority over RX at all times.
	 */
	netif_wake_queue(dev);
}

/*
 * Note that we will never build up a backlog of frames; the protocol is a
 * half duplex protocol which basically means we transmit a frame, we
 * receive a frame, we transmit the next frame etc.
 */
static int sa1100_irda_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;
	int speed = irda_get_next_speed(skb);

	/*
	 * Does this packet contain a request to change the interface
	 * speed?  If so, remember it until we complete the transmission
	 * of this frame.
	 */
	if (speed != si->speed && speed != -1)
		si->newspeed = speed;

	/*
	 * If this is an empty frame, we can bypass a lot.
	 */
	if (skb->len == 0) {
		if (si->newspeed) {
			si->newspeed = 0;
			sa1100_irda_set_speed(si, speed);
		}
		dev_kfree_skb(skb);
		return 0;
	}

	if (!IS_FIR(si)) {
		si->tx_buff.data = si->tx_buff.head;
		si->tx_buff.len  = async_wrap_skb(skb, si->tx_buff.data,
						  si->tx_buff.truesize);

		/*
		 * Set the transmit interrupt enable.  This will fire
		 * off an interrupt immediately.  Note that we disable
		 * the receiver so we won't get spurious characteres
		 * received.
		 */
		Ser2UTCR3 = UTCR3_TIE | UTCR3_TXE;

		dev_kfree_skb(skb);
	} else {
		int mtt = irda_get_mtt(skb);

		/*
		 * We must not be transmitting...
		 */
		if (si->txskb)
			BUG();

		netif_stop_queue(dev);

		si->txskb = skb;
		si->txbuf_dma = pci_map_single(NULL, skb->data,
					 skb->len, PCI_DMA_TODEVICE);

		sa1100_dma_queue_buffer(si->txdma, dev, si->txbuf_dma,
					skb->len);

		/*
		 * If we have a mean turn-around time, impose the specified
		 * specified delay.  We could shorten this by timing from
		 * the point we received the packet.
		 */
		if (mtt)
			udelay(mtt);

		Ser2HSCR0 = si->hscr0 | HSCR0_HSSP | HSCR0_TXE;
	}

	dev->trans_start = jiffies;

	return 0;
}

static int
sa1100_irda_ioctl(struct net_device *dev, struct ifreq *ifreq, int cmd)
{
	struct if_irda_req *rq = (struct if_irda_req *)ifreq;
	struct sa1100_irda *si = dev->priv;
	int ret = -EOPNOTSUPP;

	switch (cmd) {
	case SIOCSBANDWIDTH:
		if (capable(CAP_NET_ADMIN)) {
			/*
			 * We are unable to set the speed if the
			 * device is not running.
			 */
			if (si->open) {
				ret = sa1100_irda_set_speed(si,
						rq->ifr_baudrate);
			} else {
				printk("sa1100_irda_ioctl: SIOCSBANDWIDTH: !netif_running\n");
				ret = 0;
			}
		}
		break;

	case SIOCSMEDIABUSY:
		ret = -EPERM;
		if (capable(CAP_NET_ADMIN)) {
			irda_device_set_media_busy(dev, TRUE);
			ret = 0;
		}
		break;

	case SIOCGRECEIVING:
		rq->ifr_receiving = IS_FIR(si) ? 0
					: si->rx_buff.state != OUTSIDE_FRAME;
		break;

	default:
		break;
	}
		
	return ret;
}

static struct net_device_stats *sa1100_irda_stats(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;
	return &si->stats;
}

static int sa1100_irda_start(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;
	int err;

	MOD_INC_USE_COUNT;

	si->speed = 9600;

	err = request_irq(dev->irq, sa1100_irda_irq, 0, dev->name, dev);
	if (err)
		goto err_irq;

	err = sa1100_request_dma(&si->rxdma, "IrDA receive", DMA_Ser2HSSPRd);
	if (err)
		goto err_rx_dma;

	err = sa1100_request_dma(&si->txdma, "IrDA transmit", DMA_Ser2HSSPWr);
	if (err)
		goto err_tx_dma;

	/*
	 * The interrupt must remain disabled for now.
	 */
	disable_irq(dev->irq);

	/*
	 * Setup the serial port for the specified speed.
	 */
	err = sa1100_irda_startup(si);
	if (err)
		goto err_startup;

	/*
	 * Open a new IrLAP layer instance.
	 */
	si->irlap = irlap_open(dev, &si->qos, "sa1100");
	err = -ENOMEM;
	if (!si->irlap)
		goto err_irlap;

	sa1100_dma_set_callback(si->txdma, sa1100_irda_txdma_irq);

	/*
	 * Now enable the interrupt and start the queue
	 */
	si->open = 1;
	sa1100_set_power(si, power_level); /* low power mode */
	enable_irq(dev->irq);
	netif_start_queue(dev);
	return 0;

err_irlap:
	si->open = 0;
	sa1100_irda_shutdown(si);
err_startup:
	sa1100_free_dma(si->txdma);
err_tx_dma:
	sa1100_free_dma(si->rxdma);
err_rx_dma:
	free_irq(dev->irq, dev);
err_irq:
	MOD_DEC_USE_COUNT;
	return err;
}

static int sa1100_irda_stop(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;

	disable_irq(dev->irq);
	sa1100_irda_shutdown(si);

	/*
	 * If we have been doing DMA receive, make sure we
	 * tidy that up cleanly.
	 */
	if (si->rxskb) {
		pci_unmap_single(NULL, si->rxbuf_dma, HPSIR_MAX_RXLEN,
				 PCI_DMA_FROMDEVICE);
		dev_kfree_skb(si->rxskb);
		si->rxskb = NULL;
	}

	/* Stop IrLAP */
	if (si->irlap) {
		irlap_close(si->irlap);
		si->irlap = NULL;
	}

	netif_stop_queue(dev);
	si->open = 0;

	/*
	 * Free resources
	 */
	sa1100_free_dma(si->txdma);
	sa1100_free_dma(si->rxdma);
	free_irq(dev->irq, dev);

	sa1100_set_power(si, 0);

	MOD_DEC_USE_COUNT;

	return 0;
}

static int sa1100_irda_init_iobuf(iobuff_t *io, int size)
{
	io->head = kmalloc(size, GFP_KERNEL | GFP_DMA);
	if (io->head != NULL) {
		io->truesize = size;
		io->in_frame = FALSE;
		io->state    = OUTSIDE_FRAME;
		io->data     = io->head;
	}
	return io->head ? 0 : -ENOMEM;
}

static int sa1100_irda_net_init(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;
	unsigned int baudrate_mask;
	int err = -ENOMEM;

	si = kmalloc(sizeof(struct sa1100_irda), GFP_KERNEL);
	if (!si)
		goto out;

	memset(si, 0, sizeof(*si));

	/*
	 * Initialise the HP-SIR buffers
	 */
	err = sa1100_irda_init_iobuf(&si->rx_buff, 14384);
	if (err)
		goto out;
	err = sa1100_irda_init_iobuf(&si->tx_buff, 4000);
	if (err)
		goto out_free_rx;

	dev->priv = si;
	dev->hard_start_xmit	= sa1100_irda_hard_xmit;
	dev->open		= sa1100_irda_start;
	dev->stop		= sa1100_irda_stop;
	dev->do_ioctl		= sa1100_irda_ioctl;
	dev->get_stats		= sa1100_irda_stats;

	irda_device_setup(dev);
	irda_init_max_qos_capabilies(&si->qos);

	/*
	 * We support original IRDA up to 115k2. (we don't currently
	 * support 4Mbps).  Min Turn Time set to 1ms or greater.
	 */
	baudrate_mask = IR_9600|IR_19200|IR_38400|IR_57600|IR_115200;
	baudrate_mask |= IR_4000000 << 8;
	si->qos.baud_rate.bits &= baudrate_mask;
	si->qos.min_turn_time.bits = 7;

	irda_qos_bits_to_value(&si->qos);

	si->utcr4 = UTCR4_HPSIR;
	if (tx_lpm)
		si->utcr4 |= UTCR4_Z1_6us;

	/*
	 * Initially enable HP-SIR modulation, and ensure that the port
	 * is disabled.
	 */
	Ser2UTCR3 = 0;
	Ser2UTCR4 = si->utcr4;
	Ser2HSCR0 = HSCR0_UART;

#ifdef CONFIG_PM
	/*
	 * Power-Management is optional.
	 */
	si->pmdev = pm_register(PM_SYS_DEV, PM_SYS_IRDA, sa1100_irda_pmproc);
	if (si->pmdev)
		si->pmdev->data = dev;
#endif

	return 0;

	kfree(si->tx_buff.head);
out_free_rx:
	kfree(si->rx_buff.head);
out:
	kfree(si);

	return err;
}

/*
 * Remove all traces of this driver module from the kernel, so we can't be
 * called.  Note that the device has already been stopped, so we don't have
 * to worry about interrupts or dma.
 */
static void sa1100_irda_net_uninit(struct net_device *dev)
{
	struct sa1100_irda *si = dev->priv;

	dev->hard_start_xmit	= NULL;
	dev->open		= NULL;
	dev->stop		= NULL;
	dev->do_ioctl		= NULL;
	dev->get_stats		= NULL;
	dev->priv		= NULL;

	pm_unregister(si->pmdev);

	kfree(si->tx_buff.head);
	kfree(si->rx_buff.head);
	kfree(si);
}

#ifdef MODULE
static
#endif
int __init sa1100_irda_init(void)
{
	struct net_device *dev;
	int err;

	/*
	 * Limit power level a sensible range.
	 */
	if (power_level < 1)
		power_level = 1;
	if (power_level > 3)
		power_level = 3;

	err = request_mem_region(__PREG(Ser2UTCR0), 0x24, "IrDA") ? 0 : -EBUSY;
	if (err)
		goto err_mem_1;
	err = request_mem_region(__PREG(Ser2HSCR0), 0x1c, "IrDA") ? 0 : -EBUSY;
	if (err)
		goto err_mem_2;
	err = request_mem_region(__PREG(Ser2HSCR2), 0x04, "IrDA") ? 0 : -EBUSY;
	if (err)
		goto err_mem_3;

	rtnl_lock();
	dev = dev_alloc("irda%d", &err);
	if (dev) {
		dev->irq    = IRQ_Ser2ICP;
		dev->init   = sa1100_irda_net_init;
		dev->uninit = sa1100_irda_net_uninit;

		err = register_netdevice(dev);

		if (err)
			kfree(dev);
		else
			netdev = dev;
	}
	rtnl_unlock();

	if (err) {
		release_mem_region(__PREG(Ser2HSCR2), 0x04);
err_mem_3:
		release_mem_region(__PREG(Ser2HSCR0), 0x1c);
err_mem_2:
		release_mem_region(__PREG(Ser2UTCR0), 0x24);
	}
err_mem_1:
	return err;
}

static void __exit sa1100_irda_exit(void)
{
	struct net_device *dev = netdev;

	netdev = NULL;
	if (dev) {
		rtnl_lock();
		unregister_netdevice(dev);
		rtnl_unlock();
	}

	release_mem_region(__PREG(Ser2HSCR2), 0x04);
	release_mem_region(__PREG(Ser2HSCR0), 0x1c);
	release_mem_region(__PREG(Ser2UTCR0), 0x24);

	/*
	 * We now know that the netdevice is no longer in use, and all
	 * references to our driver have been removed.  The only structure
	 * which may still be present is the netdevice, which will get
	 * cleaned up by net/core/dev.c
	 */
}

#ifdef MODULE
module_init(sa1100_irda_init);
module_exit(sa1100_irda_exit);
#endif

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("StrongARM SA1100 IrDA driver");
MODULE_LICENSE("GPL");
MODULE_PARM(power_level, "i");
MODULE_PARM_DESC(power_level, "IrDA power level, 1 (low) to 3 (high)");
MODULE_PARM(tx_lpm, "i");
MODULE_PARM_DESC(tx_lpm, "Enable transmitter low power (1.6us) mode");
EXPORT_NO_SYMBOLS;
