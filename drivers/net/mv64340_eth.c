/*
 * drivers/net/mv64340_eth.c - Driver for MV64340X ethernet ports
 * Copyright (C) 2002 Matthew Dharm <mdharm@momenco.com>
 *
 * Based on the 64360 driver from:
 * Copyright (C) 2002 rabeeh@galileo.co.il
 *
 * Copyright (C) 2003 PMC-Sierra, Inc.,
 *      written by Manish Lachwani (lachwani@pmc-sierra.com)
 *
 * Copyright (C) 2003 Ralf Baechle <ralf@linux-mips.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/pci.h>
#include <asm/smp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ip.h>

#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/types.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include "mv64340_eth.h"

/*************************************************************************
**************************************************************************
**************************************************************************
*  The first part is the high level driver of the gigE ethernet ports.   *
**************************************************************************
**************************************************************************
*************************************************************************/

/* Definition for configuring driver */
#undef MV64340_RX_QUEUE_FILL_ON_TASK

/* Constants */
#define EXTRA_BYTES 32
#define WRAP       ETH_HLEN + 2 + 4 + 16
#define BUFFER_MTU dev->mtu + WRAP
#define INT_CAUSE_UNMASK_ALL		0x0007ffff
#define INT_CAUSE_UNMASK_ALL_EXT	0x0011ffff
#ifdef MV64340_RX_FILL_ON_TASK
#define INT_CAUSE_MASK_ALL		0x00000000
#define INT_CAUSE_CHECK_BITS		INT_CAUSE_UNMASK_ALL
#define INT_CAUSE_CHECK_BITS_EXT	INT_CAUSE_UNMASK_ALL_EXT
#endif

/* Static function declarations */
static int mv64340_eth_real_open(struct net_device *);
static int mv64340_eth_real_stop(struct net_device *);
static int mv64340_eth_change_mtu(struct net_device *, int);
static struct net_device_stats *mv64340_eth_get_stats(struct net_device *);
static void eth_port_init_mac_tables(ETH_PORT eth_port_num);
#ifdef MV64340_NAPI
static int mv64340_poll(struct net_device *dev, int *budget);
#endif

unsigned char prom_mac_addr_base[6];
unsigned long mv64340_sram_base;

/************************************************** 
 * Helper functions - used inside the driver only *
 **************************************************/

static inline void __netif_rx_complete(struct net_device *dev)
{
        if (!test_bit(__LINK_STATE_RX_SCHED, &dev->state)) BUG();
        list_del(&dev->poll_list);
        clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
}

static void *mv64340_eth_malloc_ring(unsigned int size)
{
	dma_addr_t dma_handle;
	void *result;
	/* Assumes allocated buffer is cache line alligned */
	result = pci_alloc_consistent(NULL, size, &dma_handle);
	memset(result, 0, size);
	return result;
}

/**********************************************************************
 * mv64340_eth_change_mtu
 *								       
 * Changes MTU (maximum transfer unit) of the gigabit ethenret port
 *
 * Input : pointer to ethernet interface network device structure
 *         new mtu size 
 * Output : 0 upon success, -EINVAL upon failure
 **********************************************************************/
static int mv64340_eth_change_mtu(struct net_device *dev, int new_mtu)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	unsigned long flags;
	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	spin_lock_irqsave(&port_private->lock, flags);

	if ((new_mtu > 9500) || (new_mtu < 64)) {
		spin_unlock_irqrestore(&port_private->lock, flags);
		return -EINVAL;
	}

	dev->mtu = new_mtu;
	/* 
	 * Stop then re-open the interface. This will allocate RX skb's with
	 * the new MTU.
	 * There is a possible danger that the open will not successed, due
	 * to memory is full, which might fail the open function.
	 */
	if (netif_running(dev)) {
		if (mv64340_eth_real_stop(dev))
			printk(KERN_ERR
			       "%s: Fatal error on stopping device\n",
			       dev->name);
		if (mv64340_eth_real_open(dev))
			printk(KERN_ERR
			       "%s: Fatal error on opening device\n",
			       dev->name);
	}

	spin_unlock_irqrestore(&port_private->lock, flags);
	return 0;
}
/**********************************************************************
 * mv64340_eth_rx_task
 *								       
 * Fills / refills RX queue on a certain gigabit ethernet port
 *
 * Input : pointer to ethernet interface network device structure
 * Output : N/A
 **********************************************************************/
static void mv64340_eth_rx_task(void *data)
{
	struct net_device *dev = (struct net_device *) data;
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	PKT_INFO pkt_info;
	struct sk_buff *skb;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	if (test_and_set_bit(0, &port_private->rx_task_busy)) {
		panic("%s: Error in test_set_bit / clear_bit\n", dev->name);
	}
	
	while (port_private->rx_ring_skbs < (port_private->rx_ring_size - 5)) {
		/* The +8 for buffer allignment and another 32 byte extra */

		skb = dev_alloc_skb(BUFFER_MTU + 8 + EXTRA_BYTES);
		if (!skb)
			/* Better luck next time */
			break;
		port_private->rx_ring_skbs++;
		pkt_info.cmd_sts = ETH_RX_ENABLE_INTERRUPT;
		pkt_info.byte_cnt = dev->mtu + ETH_HLEN + 4 + 2 + EXTRA_BYTES;
		/* Allign buffer to 8 bytes */
		if (pkt_info.byte_cnt & ~0x7) {
			pkt_info.byte_cnt &= ~0x7;
			pkt_info.byte_cnt += 8;
		}
		pkt_info.buf_ptr =
		    pci_map_single(0, skb->data,
				   dev->mtu + ETH_HLEN + 4 + 2 + EXTRA_BYTES,
				   PCI_DMA_FROMDEVICE);
		pkt_info.return_info = skb;
		if (eth_rx_return_buff(ethernet_private, &pkt_info) != ETH_OK) {
			printk(KERN_ERR
			       "%s: Error allocating RX Ring\n", dev->name);
			break;
		}
		skb_reserve(skb, 2);
	}
	clear_bit(0, &port_private->rx_task_busy);
	/*
	 * If RX ring is empty of SKB, set a timer to try allocating
	 * again in a later time .
	 */
	if ((port_private->rx_ring_skbs == 0) &&
	    (port_private->rx_timer_flag == 0)) {
		printk(KERN_INFO "%s: Rx ring is empty\n", dev->name);
		/* After 100mSec */
		port_private->timeout.expires = jiffies + (HZ / 10);
		add_timer(&port_private->timeout);
		port_private->rx_timer_flag = 1;
	}
#if MV64340_RX_QUEUE_FILL_ON_TASK
	else {
		/* Return interrupts */
		MV_WRITE(MV64340_ETH_INTERRUPT_MASK_REG(port_num),
			 INT_CAUSE_UNMASK_ALL);
	}
#endif
}

/**********************************************************************
 * mv64340_eth_rx_task_timer_wrapper
 *								       
 * Timer routine to wake up RX queue filling task. This function is
 * used only in case the RX queue is empty, and all alloc_skb has
 * failed (due to out of memory event).
 *
 * Input : pointer to ethernet interface network device structure
 * Output : N/A
 **********************************************************************/
static void mv64340_eth_rx_task_timer_wrapper(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;

	port_private->rx_timer_flag = 0;
	mv64340_eth_rx_task((void *) data);
}


/**********************************************************************
 * mv64340_eth_update_mac_address
 *								       
 * Update the MAC address of the port in the address table
 *
 * Input : pointer to ethernet interface network device structure
 * Output : N/A
 **********************************************************************/
static void mv64340_eth_update_mac_address(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private = dev->priv;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;

	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	eth_port_init_mac_tables(port_num);
	memcpy(ethernet_private->port_mac_addr, dev->dev_addr, 6);
	eth_port_uc_addr_set(port_num, ethernet_private->port_mac_addr);
}

/**********************************************************************
 * mv64340_eth_set_rx_mode
 *								       
 * Change from promiscuos to regular rx mode
 *
 * Input : pointer to ethernet interface network device structure
 * Output : N/A
 **********************************************************************/
static void mv64340_eth_set_rx_mode(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private = (ETH_PORT_INFO *) dev->priv;

	if (dev->flags & IFF_PROMISC) {
		ethernet_set_config_reg
		    (ethernet_private->port_num,
		     ethernet_get_config_reg(ethernet_private->port_num) |
		     ETH_UNICAST_PROMISCUOUS_MODE);
	} else {
		ethernet_set_config_reg
		    (ethernet_private->port_num,
		     ethernet_get_config_reg(ethernet_private->port_num) &
		     ~(unsigned int) ETH_UNICAST_PROMISCUOUS_MODE);
	}
}


/**********************************************************************
 * mv64340_eth_set_mac_address
 *								       
 * Change the interface's mac address.
 * No special hardware thing should be done because interface is always
 * put in promiscuous mode.
 *
 * Input : pointer to ethernet interface network device structure and
 *         a pointer to the designated entry to be added to the cache.
 * Output : zero upon success, negative upon failure
 **********************************************************************/
static int mv64340_eth_set_mac_address(struct net_device *dev, void *addr)
{
	int i;

	for (i = 0; i < 6; i++)
		/* +2 is for the offset of the HW addr type */
		dev->dev_addr[i] = ((unsigned char *) addr)[i + 2];
	mv64340_eth_update_mac_address(dev);
	return 0;
}

/**********************************************************************
 * mv64340_eth_tx_timeout
 *								       
 * Called upon a timeout on transmitting a packet
 *
 * Input : pointer to ethernet interface network device structure.
 * Output : N/A
 **********************************************************************/
static void mv64340_eth_tx_timeout(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private = dev->priv;
	printk(KERN_INFO "%s: TX timeout  ", dev->name);
	printk(KERN_INFO "Resetting card \n");

	/* Do the reset outside of interrupt context */
        schedule_task(&ethernet_private->tx_timeout_task);
}

/**********************************************************************
 * mv64340_eth_tx_timeout_task
 *
 * Actual routine to reset the adapter when a timeout on Tx has occurred
 **********************************************************************/
static void mv64340_eth_tx_timeout_task(struct net_device *dev)
{
        ETH_PORT_INFO *ethernet_private = dev->priv;

        netif_device_detach(dev);
        eth_port_reset(ethernet_private->port_num);
        eth_port_start(ethernet_private);
        netif_device_attach(dev);
}

/**********************************************************************
 * mv64340_eth_free_tx_queue
 *
 * Input : dev - a pointer to the required interface
 *
 * Output : 0 if was able to release skb , nonzero otherwise
 **********************************************************************/

static int mv64340_eth_free_tx_queue(struct net_device *dev,
			      unsigned int eth_int_cause_ext)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	PKT_INFO pkt_info;
	int released = 1;
	struct net_device_stats *stats;

	spin_lock(&(port_private->lock));
	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;
	stats = &port_private->stats;

	/* Check only queue 0 */
	if (eth_int_cause_ext & (BIT0 | BIT8))
		while (eth_tx_return_desc(ethernet_private, &pkt_info) ==
		       ETH_OK) {
			if (pkt_info.cmd_sts & BIT0) {
				printk("%s: Error in TX\n", dev->name);
				stats->tx_errors++;
			}
			/* 
			 * If return_info is different than 0, release the skb.
			 * The case where return_info is not 0 is only in case
			 * when transmitted a scatter/gather packet, where only
			 * last skb releases the whole chain.
			 */
			if (pkt_info.return_info) {
				dev_kfree_skb_irq((struct sk_buff *)
						  pkt_info.return_info);
				released = 0;
				if (skb_shinfo(pkt_info.return_info)->nr_frags)
					pci_unmap_page(0, pkt_info.buf_ptr,
						pkt_info.byte_cnt,
						PCI_DMA_TODEVICE);

				if (port_private->tx_ring_skbs != 1)
					port_private->tx_ring_skbs--;
			}
			else 
				pci_unmap_page(0, pkt_info.buf_ptr,
						pkt_info.byte_cnt,
						PCI_DMA_TODEVICE);

			/* 
			 * Decrement the number of outstanding skbs counter on the
			 * TX queue.
			 */
			if (port_private->tx_ring_skbs == 0)
				panic
				    ("ERROR - TX outstanding SKBs counter is corrupted\n");

		}

	spin_unlock(&(port_private->lock));
	return released;
}

/**********************************************************************
 * mv64340_eth_receive
 *
 * This function is forward packets that are received from the port's
 * queues toward kernel core or FastRoute them to another interface.
 *
 * Input : dev - a pointer to the required interface
 *         max - maximum number to receive (0 means unlimted)
 *
 * Output : number of served packets
 **********************************************************************/
#ifdef MV64340_NAPI
static int mv64340_eth_receive_queue(struct net_device *dev, unsigned int max,
								int budget)
#else
static int mv64340_eth_receive_queue(struct net_device *dev, unsigned int max)
#endif
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	PKT_INFO pkt_info;
	struct sk_buff *skb;
	unsigned int received_packets = 0;
	struct net_device_stats *stats;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;
	stats = &port_private->stats;

#ifdef MV64340_NAPI
	while ((eth_port_receive(ethernet_private, &pkt_info) == ETH_OK) 
		&&
		budget > 0) {
#else
	while ((--max)
	       && (eth_port_receive(ethernet_private, &pkt_info) ==
		   ETH_OK)) {
#endif
		port_private->rx_ring_skbs--;
		received_packets++;
#ifdef MV64340_NAPI
		budget--;
#endif
		/* Update statistics. Note byte count includes 4 byte CRC count */
		stats->rx_packets++;
		stats->rx_bytes += pkt_info.byte_cnt;
		skb = (struct sk_buff *) pkt_info.return_info;
		/*
		 * In case received a packet without first / last bits on OR the error
		 * summary bit is on, the packets needs to be dropeed.
		 */
		if (
		    ((pkt_info.cmd_sts
		      & (ETH_RX_FIRST_DESC | ETH_RX_LAST_DESC)) !=
		     (ETH_RX_FIRST_DESC | ETH_RX_LAST_DESC))
		    || (pkt_info.cmd_sts & ETH_ERROR_SUMMARY)) {
			stats->rx_dropped++;
			if (
			    (pkt_info.cmd_sts & (ETH_RX_FIRST_DESC |
						 ETH_RX_LAST_DESC)) !=
			    (ETH_RX_FIRST_DESC | ETH_RX_LAST_DESC)) {
				if (net_ratelimit())
					printk(KERN_ERR
					       "%s: Received packet spread on multiple"
					       " descriptors\n",
					       dev->name);
			}
			if (pkt_info.cmd_sts & ETH_ERROR_SUMMARY) {
				stats->rx_errors++;
			}
			dev_kfree_skb_irq(skb);
		} else {
			struct ethhdr *eth_h;
			struct iphdr *ip_h;

			/*
			 * The -4 is for the CRC in the trailer of the
			 * received packet
			 */
			skb_put(skb, pkt_info.byte_cnt - 4);
			skb->dev = dev;

			eth_h = (struct ethhdr *) skb->data;
			ip_h = (struct iphdr *) (skb->data + ETH_HLEN);
			if (pkt_info.cmd_sts & ETH_LAYER_4_CHECKSUM_OK) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				skb->csum = htons((pkt_info.cmd_sts
							& 0x0007fff8) >> 3);
			}
			else 
				skb->ip_summed = CHECKSUM_NONE;
			skb->protocol = eth_type_trans(skb, dev);
#ifdef MV64340_NAPI
			netif_receive_skb(skb);
#else
			netif_rx(skb);
#endif
		}
	}

	return received_packets;
}

/**********************************************************************
 * mv64340_eth_int_handler
 *
 * Main interrupt handler for the gigbit ethernet ports
 *
 * Input : irq - irq number (not used)
 *         dev_id - a pointer to the required interface's data structure
 *         regs   - not used
 * Output : N/A
 **********************************************************************/

static void mv64340_eth_int_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	u32 eth_int_cause = 0, eth_int_cause_ext = 0;
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	/* Read interrupt cause registers */
	eth_int_cause =
	    (MV_READ_DATA(MV64340_ETH_INTERRUPT_CAUSE_REG(port_num)) & 
			INT_CAUSE_UNMASK_ALL);

	if (eth_int_cause & BIT1) 
		eth_int_cause_ext =
		    (MV_READ_DATA(MV64340_ETH_INTERRUPT_CAUSE_EXTEND_REG(port_num)) &
				INT_CAUSE_UNMASK_ALL_EXT);
	else
		eth_int_cause_ext = 0;

#ifdef MV64340_NAPI
	if (!(eth_int_cause & 0x0007fffd)) {
	/* Dont ack the Rx interrupt */
#endif
		/*
	 	 * Clear specific ethernet port intrerrupt registers by acknowleding
	 	 * relevant bits.
   	 	 */
		MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_REG(port_num),
			 ~eth_int_cause);
		if (eth_int_cause_ext != 0x0)
			MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_EXTEND_REG(port_num),
				 ~eth_int_cause_ext);

		/* UDP change : We may need this */
                 if (eth_int_cause_ext & 0x0000ffff) {
                        if (mv64340_eth_free_tx_queue(dev, eth_int_cause_ext) == 0) {
                                if (netif_queue_stopped(dev) &&
				    (dev->flags & IFF_RUNNING) &&
				    (MV64340_TX_QUEUE_SIZE > port_private->tx_ring_skbs + 1)) {
                                         netif_wake_queue(dev);
                                }
                        }
                 }
#ifdef MV64340_NAPI
	}
	else {
		if (netif_rx_schedule_prep(dev)) {
			/* Mask all the interrupts */
			MV_WRITE(MV64340_ETH_INTERRUPT_MASK_REG(port_num),0);
			MV_WRITE(MV64340_ETH_INTERRUPT_EXTEND_MASK_REG(port_num), 0);
			__netif_rx_schedule(dev);
		}
#else
		unsigned int total_received = 0;
		
		if (eth_int_cause & (BIT2 | BIT11)) {
			total_received +=
			    mv64340_eth_receive_queue(dev, 0);
		}
		/*
		 * After forwarded received packets to upper layer, add a task in an
		 * interrupts enabled context that refills the RX ring with skb's.
		 */
#if MV64340_RX_QUEUE_FILL_ON_TASK
		/* Unmask all interrupts on ethernet port */
		MV_WRITE(MV64340_ETH_INTERRUPT_MASK_REG(port_num),
			 INT_CAUSE_MASK_ALL);
		queue_task(&port_private->rx_task, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
#else
		port_private->rx_task.routine(dev);
#endif
#endif
	}
	/* PHY status changed */
	if (eth_int_cause_ext & (BIT16 | BIT20)) {
		unsigned int phy_reg_data;

		/* Check Link status on ethernet port */
		eth_port_read_smi_reg(port_num, 1, &phy_reg_data);
		if (!(phy_reg_data & 0x20)) {
			netif_stop_queue(dev);
			dev->flags &= ~(IFF_RUNNING);
		} else {
			netif_wake_queue(dev);
			dev->flags |= (IFF_RUNNING);

	/*
	 * Start all TX queues on ethernet port. This is good in case of
	 * previous packets where not transmitted, due to link down and
	 * this command re-enables all TX queues.
	 * Note that it is possible to get a TX resource error interrupt
	 * after issuing this, since not all TX queues are enabled,
	 * or has anything to send.
	 */
			MV_WRITE(MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG(port_num), 1);
		}
	}

        /*
         * If no real interrupt occured, exit.
         * This can happen when using gigE interrupt coalescing mechanism.
         */
        if ((eth_int_cause == 0x0) && (eth_int_cause_ext == 0x0)) {
                return;
        }

	return;
}

/**********************************************************************
 * mv64340_eth_open
 *
 * This function is called when openning the network device. The function
 * should initialize all the hardware, initialize cyclic Rx/Tx
 * descriptors chain and buffers and allocate an IRQ to the network
 * device.
 *
 * Input : a pointer to the network device structure
 *
 * Output : zero of success , nonzero if fails.
 **********************************************************************/

static int mv64340_eth_open(struct net_device *dev)
{
	int retval;
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	spin_lock_irq(&(port_private->lock));

	retval =
	    request_irq(ETH_PORT0_IRQ_NUM + port_num, mv64340_eth_int_handler,
			(SA_INTERRUPT | SA_SAMPLE_RANDOM), dev->name, dev);

	if (retval != 0) {
		printk(KERN_ERR "Can not assign IRQ number to MV64340_eth%d\n",
		       port_num);
		return -1;
	} else {
		dev->irq = ETH_PORT0_IRQ_NUM + port_num;
	}

	if (mv64340_eth_real_open(dev)) {
		printk("%s: Error opening interface\n", dev->name);
		free_irq(dev->irq, dev);
		spin_unlock_irq(&ethernet_priv->lock);
		return -EBUSY;
	}
	MOD_INC_USE_COUNT;
	spin_unlock_irq(&ethernet_priv->lock);
	return 0;
}

/* Helper function for mv64340_eth_open */
static int mv64340_eth_real_open(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	u32 phy_reg_data;
	unsigned int size;
	int i;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	/* Stop RX Queues */
	MV_WRITE(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG(port_num),
		 0x0000ff00);

	/* Clear the ethernet port interrupts */
	MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_REG(port_num), 0);
	MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_EXTEND_REG(port_num), 0);

	/* Unmask RX buffer and TX end interrupt */
	MV_WRITE(MV64340_ETH_INTERRUPT_MASK_REG(port_num),
		 INT_CAUSE_UNMASK_ALL);

	/* Unmask phy and link status changes interrupts */
	MV_WRITE(MV64340_ETH_INTERRUPT_EXTEND_MASK_REG(port_num),
		 INT_CAUSE_UNMASK_ALL_EXT);

	/* Set the MAC Address */
	memcpy(ethernet_private->port_mac_addr, dev->dev_addr, 6);

	eth_port_init(ethernet_private);

	/* Set rx_task pointers */
	port_private->rx_task.sync = 0;
	port_private->rx_task.routine = mv64340_eth_rx_task;
	port_private->rx_task.data = dev;

	memset(&port_private->timeout, 0, sizeof(struct timer_list));
	port_private->timeout.function = mv64340_eth_rx_task_timer_wrapper;
	port_private->timeout.data = (unsigned long) dev;

	port_private->rx_task_busy = 0;
	port_private->rx_timer_flag = 0;

	/* Allocate TX ring */
	port_private->tx_ring_skbs = 0;
	port_private->tx_ring_size = MV64340_TX_QUEUE_SIZE;
	size = port_private->tx_ring_size * sizeof(ETH_TX_DESC);
	ethernet_private->tx_desc_area_size = size;

	/* Assumes allocated ring is 16 bytes alligned */
	ethernet_private->p_tx_desc_area =
	    (ETH_TX_DESC *) mv64340_sram_base;
	if (!ethernet_private->p_tx_desc_area) {
		printk(KERN_ERR
		       "%s: Cannot allocate Tx Ring (size %d bytes)\n",
		       dev->name, size);
		return -ENOMEM;
	}
	memset((void *) ethernet_private->p_tx_desc_area, 0,
	       ethernet_private->tx_desc_area_size);

	/* Dummy will be replaced upon real tx */
	ether_init_tx_desc_ring(ethernet_private,
				port_private->tx_ring_size,
				(unsigned long)ethernet_private->p_tx_desc_area);

	/* Allocate RX ring */
	/* Meantime RX Ring are fixed - but must be configurable by user */
	port_private->rx_ring_size = MV64340_RX_QUEUE_SIZE;
	port_private->rx_ring_skbs = 0;
	size = port_private->rx_ring_size * sizeof(ETH_RX_DESC);
	ethernet_private->rx_desc_area_size = size;

	/* Assumes allocated ring is 16 bytes alligned */
	ethernet_private->p_rx_desc_area =
	    (ETH_RX_DESC *) mv64340_eth_malloc_ring(size);
	if (!ethernet_private->p_rx_desc_area) {
		printk(KERN_ERR
		       "%s: Cannot allocate Rx ring (size %d bytes)\n",
		       dev->name, size);
		printk(KERN_ERR
		       "%s: Freeing previously allocated TX queues...",
		       dev->name);
		pci_free_consistent(0, ethernet_private->tx_desc_area_size,
				    (void *)
				    ethernet_private->p_tx_desc_area,
				    virt_to_bus(ethernet_private->
							    p_tx_desc_area));
		return -ENOMEM;
	}
	memset((void *) ethernet_private->p_rx_desc_area, 0,
	       ethernet_private->rx_desc_area_size);
	if (
	    (ether_init_rx_desc_ring(ethernet_private, port_private->rx_ring_size, 1536,	/* Dummy, will be replaced later */
				     (unsigned long) ethernet_private->p_rx_desc_area,
				     0)) == false)
		panic("%s: Error initializing RX Ring\n", dev->name);

	/* Fill RX ring with skb's */
	mv64340_eth_rx_task(dev);

	eth_port_start(ethernet_private);

	/* Interrupt Coalescing */

#ifdef MV64340_COAL
	port_private->rx_int_coal =
		eth_port_set_rx_coal (port_num, 133000000, MV64340_RX_COAL);
#endif

	port_private->tx_int_coal =
		eth_port_set_tx_coal (port_num, 133000000, MV64340_TX_COAL);  

	/* Increase the Rx side buffer size */

	MV_WRITE (MV64340_ETH_PORT_SERIAL_CONTROL_REG(port_num), (0x5 << 17) |
			(MV_READ_DATA (MV64340_ETH_PORT_SERIAL_CONTROL_REG(port_num))
					& 0xfff1ffff));

	/* Check Link status on phy */
	eth_port_read_smi_reg(port_num, 1, &phy_reg_data);
	if (!(phy_reg_data & 0x20)) {
		netif_stop_queue(dev);
		dev->flags &= ~(IFF_RUNNING);
	} else {
		netif_start_queue(dev);
		dev->flags |= (IFF_RUNNING);
	}

	return 0;
}

static void mv64340_eth_free_tx_rings(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num, curr;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	/* Stop Tx Queues */
	MV_WRITE(MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG(port_num),
		 0x0000ff00);

	/* Free TX rings */
	/* Free outstanding skb's on TX rings */
	for (curr = 0;
	     (port_private->tx_ring_skbs) && (curr < MV64340_TX_QUEUE_SIZE);
	     curr++) {
		if (ethernet_private->tx_skb[curr]) {
			dev_kfree_skb(ethernet_private->tx_skb[curr]);
			port_private->tx_ring_skbs--;
		}
	}
	if (port_private->tx_ring_skbs != 0)
		printk
		    ("%s: Error on Tx descriptor free - could not free %d"
		     " descriptors\n", dev->name,
		     port_private->tx_ring_skbs);
	pci_free_consistent(0, ethernet_private->tx_desc_area_size,
			    (void *) ethernet_private->p_tx_desc_area,
			    virt_to_bus(ethernet_private->p_tx_desc_area));
}

static void mv64340_eth_free_rx_rings(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	int curr;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;


	/* Stop RX Queues */
	MV_WRITE(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG(port_num),
		 0x0000ff00);

	/* Free RX rings */
	/* Free preallocated skb's on RX rings */
	for (curr = 0;
		port_private->rx_ring_skbs && (curr < MV64340_RX_QUEUE_SIZE);
		curr++) {
		if (ethernet_private->rx_skb[curr]) {
			dev_kfree_skb(ethernet_private->rx_skb[curr]);
			port_private->rx_ring_skbs--;
		}
	}

	if (port_private->rx_ring_skbs != 0)
		printk(KERN_ERR
		       "%s: Error in freeing Rx Ring. %d skb's still"
		       " stuck in RX Ring - ignoring them\n", dev->name,
		       port_private->rx_ring_skbs);
	pci_free_consistent(0, ethernet_private->rx_desc_area_size,
			    (void *) ethernet_private->p_rx_desc_area,
			    virt_to_bus(ethernet_private->p_rx_desc_area));
}

/**********************************************************************
 * mv64340_eth_stop
 *
 * This function is used when closing the network device. 
 * It updates the hardware, 
 * release all memory that holds buffers and descriptors and release the IRQ.
 * Input : a pointer to the device structure
 * Output : zero if success , nonzero if fails
 *********************************************************************/

static int mv64340_eth_stop(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	spin_lock_irq(&(port_private->lock));

	mv64340_eth_real_stop(dev);

	free_irq(dev->irq, dev);
	MOD_DEC_USE_COUNT;
	spin_unlock_irq(&ethernet_priv->lock);
	return 0;
};

/* Helper function for mv64340_eth_stop */

static int mv64340_eth_real_stop(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	netif_stop_queue(dev);

	mv64340_eth_free_tx_rings(dev);
	mv64340_eth_free_rx_rings(dev);

	eth_port_reset(ethernet_private->port_num);

	/* Disable ethernet port interrupts */
	MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_REG(port_num), 0);
	MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_EXTEND_REG(port_num), 0);

	/* Mask RX buffer and TX end interrupt */
	MV_WRITE(MV64340_ETH_INTERRUPT_MASK_REG(port_num), 0);

	/* Mask phy and link status changes interrupts */
	MV_WRITE(MV64340_ETH_INTERRUPT_EXTEND_MASK_REG(port_num), 0);

	return 0;
}

#ifdef MV64340_NAPI
static void mv64340_tx(struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
        struct mv64340_eth_priv *port_private;
        unsigned int port_num;
        PKT_INFO pkt_info;

	ethernet_private = dev->priv;
	port_private =
            (struct mv64340_eth_priv *) ethernet_private->port_private;

	port_num = port_private->port_num;

	while (eth_tx_return_desc(ethernet_private, &pkt_info) == ETH_OK) {
		if (pkt_info.return_info) {
			dev_kfree_skb_irq((struct sk_buff *)
                                                  pkt_info.return_info);
			if (skb_shinfo(pkt_info.return_info)->nr_frags) 
                                 pci_unmap_page(0, pkt_info.buf_ptr,
                                             pkt_info.byte_cnt,
                                             PCI_DMA_TODEVICE);

                         if (port_private->tx_ring_skbs != 1)
                                  port_private->tx_ring_skbs--;
                } else 
                       pci_unmap_page(0, pkt_info.buf_ptr,
                             pkt_info.byte_cnt,
                             PCI_DMA_TODEVICE);
	}

	if (netif_queue_stopped(dev) &&
		(dev->flags & IFF_RUNNING) &&
                (MV64340_TX_QUEUE_SIZE > port_private->tx_ring_skbs + 1)) {
                       netif_wake_queue(dev);
	}
}

/**********************************************************************
 * mv64340_poll
 *
 * This function is used in case of NAPI
 ***********************************************************************/
static int mv64340_poll(struct net_device *netdev, int *budget)
{
	ETH_PORT_INFO *ethernet_private = netdev->priv;
	struct mv64340_eth_priv *port_private = 
		(struct mv64340_eth_priv *) ethernet_private->port_private;
	int	done = 1, orig_budget, work_done;
	unsigned long	flags;
	unsigned int	port_num = port_private->port_num;

	spin_lock_irqsave(&port_private->lock, flags);

#ifdef MV64340_TX_FAST_REFILL
	if (++ethernet_private->tx_clean_threshold > 5) {
		mv64340_tx(netdev);
		ethernet_private->tx_clean_threshold = 0;
	}
#endif
	if ((u32)(MV_READ_DATA(MV64340_ETH_RX_CURRENT_QUEUE_DESC_PTR_0(port_num)))                                      != (u32)ethernet_private->rx_used_desc_q) {
		orig_budget = *budget;
		if (orig_budget > netdev->quota)
			orig_budget = netdev->quota;
		work_done = mv64340_eth_receive_queue(netdev, 0, orig_budget);
		port_private->rx_task.routine(netdev);
		*budget -= work_done;
		netdev->quota -= work_done;
		if (work_done >= orig_budget)
			done = 0;
	}

	if (done) {
		__netif_rx_complete(netdev);
		MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_REG(port_num),0);
                MV_WRITE(MV64340_ETH_INTERRUPT_CAUSE_EXTEND_REG(port_num),0);
		MV_WRITE(MV64340_ETH_INTERRUPT_MASK_REG(port_num), 
						INT_CAUSE_UNMASK_ALL);
		MV_WRITE(MV64340_ETH_INTERRUPT_EXTEND_MASK_REG(port_num),
				                 INT_CAUSE_UNMASK_ALL_EXT);
	}

	spin_unlock_irqrestore(&port_private->lock, flags);
	return (done ? 0 : 1);
}
#endif

/**********************************************************************
 * mv64340_eth_start_xmit
 *
 * This function is queues a packet in the Tx descriptor for 
 * required port.
 *
 * Input : skb - a pointer to socket buffer
 *         dev - a pointer to the required port
 *
 * Output : zero upon success
 **********************************************************************/

static int mv64340_eth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;
	PKT_INFO pkt_info;
	unsigned long flags;
	ETH_FUNC_RET_STATUS status;
	struct net_device_stats *stats;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;
	stats = &port_private->stats;

	if (netif_queue_stopped(dev)) {
		printk(KERN_ERR
		       "%s: Tried sending packet when interface is stopped\n",
		       dev->name);
		return 1;
	}

	/* This is a hard error, log it. */
	if ((MV64340_TX_QUEUE_SIZE - port_private->tx_ring_skbs) <=
	    (skb_shinfo(skb)->nr_frags + 1)) {
		netif_stop_queue(dev);
		printk(KERN_ERR
		       "%s: Bug in mv64340_eth - Trying to transmit when"
		       " queue full !\n", dev->name);
		return 1;
	}

	/* Paranoid check - this shouldn't happen */
	if (skb == NULL) {
		stats->tx_dropped++;
		return 1;
	}

	spin_lock_irqsave(&port_private->lock, flags);

	/* Update packet info data structure -- DMA owned, first last */
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
	if (!skb_shinfo(skb)->nr_frags || (skb_shinfo(skb)->nr_frags > 3)) {
#endif
		pkt_info.cmd_sts = ETH_TX_ENABLE_INTERRUPT |
	    				ETH_TX_FIRST_DESC | ETH_TX_LAST_DESC;

		pkt_info.byte_cnt = skb->len;
		pkt_info.buf_ptr = pci_map_single
			    (0, skb->data, skb->len, PCI_DMA_TODEVICE);

		pkt_info.return_info = skb;
		status = eth_port_send(ethernet_private, &pkt_info);
		if ((status == ETH_ERROR) || (status == ETH_QUEUE_FULL))
			printk(KERN_ERR "%s: Error on transmitting packet\n",
				       dev->name);
		port_private->tx_ring_skbs++;
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
	} 
	else {
		unsigned int    frag;
		u32		ipheader;

                /* first frag which is skb header */
                pkt_info.byte_cnt = skb_headlen(skb);
                pkt_info.buf_ptr = pci_map_single(0, skb->data,
                                        skb_headlen(skb), PCI_DMA_TODEVICE);
                pkt_info.return_info = 0;
                ipheader =   (skb->nh.iph->ihl << 11);
                pkt_info.cmd_sts = ETH_TX_FIRST_DESC | 
					ETH_GEN_TCP_UDP_CHECKSUM |
					ETH_GEN_IP_V_4_CHECKSUM |
                                        ipheader;
		/* CPU already calculated pseudo header checksum. So, use it */
                pkt_info.l4i_chk = skb->h.th->check;
                status = eth_port_send(ethernet_private, &pkt_info);
		if (status != ETH_OK) {
	                if ((status == ETH_ERROR))
        	                printk(KERN_ERR "%s: Error on transmitting packet\n", dev->name);
	                if (status == ETH_QUEUE_FULL)
        	                printk("Error on Queue Full \n");
                	if (status == ETH_QUEUE_LAST_RESOURCE)
                        	printk("Tx resource error \n");
		}

                /* Check for the remaining frags */
                for (frag = 0; frag < skb_shinfo(skb)->nr_frags; frag++) {
                        skb_frag_t *this_frag = &skb_shinfo(skb)->frags[frag];
                        pkt_info.l4i_chk = 0x0000;
                        pkt_info.cmd_sts = 0x00000000;

                        /* Last Frag enables interrupt and frees the skb */
                        if (frag == (skb_shinfo(skb)->nr_frags - 1)) {
                                pkt_info.cmd_sts |= ETH_TX_ENABLE_INTERRUPT |
                                                        ETH_TX_LAST_DESC;
                                pkt_info.return_info = skb;
                                port_private->tx_ring_skbs++;
                        }
                        else {
                                pkt_info.return_info = 0;
                        }
                        pkt_info.byte_cnt = this_frag->size;
                        if (this_frag->size < 8)
                                printk("%d : \n", skb_shinfo(skb)->nr_frags);

                        pkt_info.buf_ptr = pci_map_page(0,
                                        this_frag->page,
                                        this_frag->page_offset,
                                        this_frag->size,
                                        PCI_DMA_TODEVICE);

                        status = eth_port_send(ethernet_private, &pkt_info);

			if (status != ETH_OK) {
	                        if ((status == ETH_ERROR))
        	                        printk(KERN_ERR "%s: Error on transmitting packet\n", dev->name);

       		                 if (status == ETH_QUEUE_LAST_RESOURCE)
                	                printk("Tx resource error \n");

                        	if (status == ETH_QUEUE_FULL)
                                	printk("Queue is full \n");
			}
                }
        }
#endif

	/* Check if TX queue can handle another skb. If not, then
	 * signal higher layers to stop requesting TX
	 */
	if (MV64340_TX_QUEUE_SIZE <= (port_private->tx_ring_skbs + 1))
		/* 
		 * Stop getting skb's from upper layers.
		 * Getting skb's from upper layers will be enabled again after
		 * packets are released.
		 */
		netif_stop_queue(dev);


	/* Update statistics and start of transmittion time */
	stats->tx_bytes += skb->len;
	stats->tx_packets++;
	dev->trans_start = jiffies;

	spin_unlock_irqrestore(&ethernet_priv->lock, flags);
	return 0;		/* success */
}

/**********************************************************************
 * mv64340_eth_get_stats
 *
 * Returns a pointer to the interface statistics.
 *
 * Input : dev - a pointer to the required interface
 *
 * Output : a pointer to the interface's statistics
 **********************************************************************/

static struct net_device_stats *mv64340_eth_get_stats(struct net_device
						      *dev)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	unsigned int port_num;

	ethernet_private = dev->priv;
	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_num = port_private->port_num;

	return &port_private->stats;
}

/**********************************************************************
 * mv64340_eth_init
 *								       
 * First function called after registering the network device. 
 * It's purpose is to initialize the device as an ethernet device, 
 * fill the structure that was given in registration with pointers
 * to functions, and setting the MAC address of the interface
 *
 * Input : number of port to initialize
 * Output : -ENONMEM if failed , 0 if success
 **********************************************************************/
static int mv64340_eth_init(int port_num)
{
	ETH_PORT_INFO *ethernet_private;
	struct mv64340_eth_priv *port_private;
	struct net_device *dev;
	int err;

	dev = alloc_etherdev(sizeof(ETH_PORT_INFO));
	if (!dev)
		return -ENOMEM;

	dev->open = mv64340_eth_open;
	dev->stop = mv64340_eth_stop;
	dev->hard_start_xmit = mv64340_eth_start_xmit;
	dev->get_stats = mv64340_eth_get_stats;
	dev->set_mac_address = mv64340_eth_set_mac_address;
	dev->set_multicast_list = mv64340_eth_set_rx_mode;

	/* No need to Tx Timeout */
	dev->tx_timeout = mv64340_eth_tx_timeout;
#ifdef MV64340_NAPI
        dev->poll = mv64340_poll;
        dev->weight = 64;
#endif

	dev->watchdog_timeo = 2 * HZ;
	dev->tx_queue_len = MV64340_TX_QUEUE_SIZE;
	dev->flags &= ~(IFF_RUNNING);
	dev->base_addr = 0;
	dev->change_mtu = &mv64340_eth_change_mtu;

#ifdef MV64340_CHECKSUM_OFFLOAD_TX
#ifdef MAX_SKB_FRAGS
#ifndef CONFIG_JAGUAR_DMALOW
        /*
         * Zero copy can only work if we use Discovery II memory. Else, we will
         * have to map the buffers to ISA memory which is only 16 MB
         */
        dev->features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_HW_CSUM;
#endif
#endif
#endif
	ethernet_private = dev->priv;
	/* Allocate memory for stats data structure and spinlock etc... */
	ethernet_private->port_private = (void *)
	    kmalloc(sizeof(struct mv64340_eth_priv), GFP_KERNEL);
	if (!ethernet_private->port_private) {
		err = -ENOMEM;
		goto out_free_dev;
	}
	memset(ethernet_private->port_private, 0,
	       sizeof(struct mv64340_eth_priv));
	if (port_num == 0)
		ethernet_private->port_num = ETH_0;
	else if (port_num == 1)
		ethernet_private->port_num = ETH_1;
	else if (port_num == 2)
		ethernet_private->port_num = ETH_2;
	else {
		printk(KERN_ERR "%s: Invalid port number\n", dev->name);
		kfree(ethernet_private->port_private);
		err = -ENODEV;
		goto out_free_dev;
	}

	port_private =
	    (struct mv64340_eth_priv *) ethernet_private->port_private;
	port_private->port_num = port_num;

	memset(&port_private->stats, 0, sizeof(struct net_device_stats));

	/* Configure the timeout task */
        INIT_TQUEUE(&ethernet_private->tx_timeout_task,
                        (void (*)(void *))mv64340_eth_tx_timeout_task, dev);

	/* Init spinlock */
	spin_lock_init(&port_private->lock);

	/* set MAC addresses */
	memcpy(dev->dev_addr, prom_mac_addr_base, 6);
	dev->dev_addr[5] += port_num;

	err = register_netdev(dev);
	if (err)
		goto out_free_dev;

	printk(KERN_NOTICE "%s: port %d with MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
		dev->name, port_num,
		dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	if (dev->features & NETIF_F_SG)
		printk("Scatter Gather Enabled  ");

	if (dev->features & NETIF_F_IP_CSUM)
		printk("TX TCP/IP Checksumming Supported  \n");

	printk("RX TCP/UDP Checksum Offload ON, \n");
	printk("TX and RX Interrupt Coalescing ON \n");

#ifdef MV64340_NAPI
	printk("RX NAPI Enabled \n");
#endif

	return 0;

out_free_dev:
	kfree(dev->priv);		/* free_netdev(dev) in 2.6 */
	kfree(dev);

	return err;
}

/**********************************************************************
 * mv64340_init_module
 *
 * Registers the network drivers into the Linux kernel
 *
 * Input : N/A
 *
 * Output : N/A
 **********************************************************************/
static int __init mv64340_init_module(void)
{
	printk(KERN_NOTICE "MV-64340 10/100/1000 Ethernet Driver\n");
#ifdef CONFIG_MV64340_ETH_0
	if (mv64340_eth_init(0)) {
		printk(KERN_ERR
		       "Error registering MV-64360 ethernet port 0\n");
	}
#endif
#ifdef CONFIG_MV64340_ETH_1
	if (mv64340_eth_init(1)) {
		printk(KERN_ERR
		       "Error registering MV-64360 ethernet port 1\n");
	}
#endif
#ifdef CONFIG_MV64340_ETH_2
	if (mv64340_eth_init(2)) {
		printk(KERN_ERR
		       "Error registering MV-64360 ethernet port 2\n");
	}
#endif
	return 0;
}

/**********************************************************************
 * mv64340_cleanup_module
 *
 * Registers the network drivers into the Linux kernel
 *
 * Input : N/A
 *
 * Output : N/A
 **********************************************************************/
static void __init mv64340_cleanup_module(void)
{
	/* Nothing to do here ! it's not a removable module */
}

module_init(mv64340_init_module);
module_exit(mv64340_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rabeeh Khoury, Assaf Hoffman, Matthew Dharm and Manish Lachwani");
MODULE_DESCRIPTION("Ethernet driver for Marvell MV64340");

/*************************************************************************
**************************************************************************
**************************************************************************
*  The second part is the low level driver of the gigE ethernet ports.   *
**************************************************************************
**************************************************************************
*************************************************************************/




/********************************************************************************
 * Marvell's Gigabit Ethernet controller low level driver
 *
 * DESCRIPTION:
 *       This file introduce low level API to Marvell's Gigabit Ethernet
 *		controller. This Gigabit Ethernet Controller driver API controls
 *		1) Operations (i.e. port init, start, reset etc').
 *		2) Data flow (i.e. port send, receive etc').
 *		Each Gigabit Ethernet port is controlled via ETH_PORT_INFO
 *		struct.
 *		This struct includes user configuration information as well as
 *		driver internal data needed for its operations.
 *
 *		Supported Features:  
 *		- This low level driver is OS independent. Allocating memory for
 *		  the descriptor rings and buffers are not within the scope of
 *		  this driver.
 *		- The user is free from Rx/Tx queue managing.
 *		- This low level driver introduce functionality API that enable
 *		  the to operate Marvell's Gigabit Ethernet Controller in a
 *		  convenient way.
 *		- Simple Gigabit Ethernet port operation API.
 *		- Simple Gigabit Ethernet port data flow API.
 *		- Data flow and operation API support per queue functionality.
 *		- Support cached descriptors for better performance.
 *		- Enable access to all four DRAM banks and internal SRAM memory
 *		  spaces.
 *		- PHY access and control API.
 *		- Port control register configuration API.
 *		- Full control over Unicast and Multicast MAC configurations.
 *								   
 *		Operation flow:
 *
 *		Initialization phase
 *		This phase complete the initialization of the ETH_PORT_INFO
 *		struct. 
 *		User information regarding port configuration has to be set
 *		prior to calling the port initialization routine.
 *
 *		In this phase any port Tx/Rx activity is halted, MIB counters
 *		are cleared, PHY address is set according to user parameter and
 *		access to DRAM and internal SRAM memory spaces.
 *
 *		Driver ring initialization
 *		Allocating memory for the descriptor rings and buffers is not 
 *		within the scope of this driver. Thus, the user is required to
 *		allocate memory for the descriptors ring and buffers. Those
 *		memory parameters are used by the Rx and Tx ring initialization
 *		routines in order to curve the descriptor linked list in a form
 *		of a ring.
 *		Note: Pay special attention to alignment issues when using
 *		cached descriptors/buffers. In this phase the driver store
 *		information in the ETH_PORT_INFO struct regarding each queue
 *		ring.
 *
 *		Driver start 
 *		This phase prepares the Ethernet port for Rx and Tx activity.
 *		It uses the information stored in the ETH_PORT_INFO struct to 
 *		initialize the various port registers.
 *
 *		Data flow:
 *		All packet references to/from the driver are done using PKT_INFO
 *		struct.
 *		This struct is a unified struct used with Rx and Tx operations. 
 *		This way the user is not required to be familiar with neither
 *		Tx nor Rx descriptors structures.
 *		The driver's descriptors rings are management by indexes.
 *		Those indexes controls the ring resources and used to indicate
 *		a SW resource error:
 *		'current' 
 *		This index points to the current available resource for use. For 
 *		example in Rx process this index will point to the descriptor  
 *		that will be passed to the user upon calling the receive routine.
 *		In Tx process, this index will point to the descriptor
 *		that will be assigned with the user packet info and transmitted.
 *		'used'    
 *		This index points to the descriptor that need to restore its 
 *		resources. For example in Rx process, using the Rx buffer return
 *		API will attach the buffer returned in packet info to the
 *		descriptor pointed by 'used'. In Tx process, using the Tx
 *		descriptor return will merely return the user packet info with
 *		the command status of  the transmitted buffer pointed by the
 *		'used' index. Nevertheless, it is essential to use this routine
 *		to update the 'used' index.
 *		'first'
 *		This index supports Tx Scatter-Gather. It points to the first 
 *		descriptor of a packet assembled of multiple buffers. For example
 *		when in middle of Such packet we have a Tx resource error the 
 *		'curr' index get the value of 'first' to indicate that the ring 
 *		returned to its state before trying to transmit this packet.
 *
 *		Receive operation:
 *		The eth_port_receive API set the packet information struct,
 *		passed by the caller, with received information from the 
 *		'current' SDMA descriptor. 
 *		It is the user responsibility to return this resource back
 *		to the Rx descriptor ring to enable the reuse of this source.
 *		Return Rx resource is done using the eth_rx_return_buff API.
 *
 *		Transmit operation:
 *		The eth_port_send API supports Scatter-Gather which enables to
 *		send a packet spanned over multiple buffers. This means that
 *		for each packet info structure given by the user and put into
 *		the Tx descriptors ring, will be transmitted only if the 'LAST'
 *		bit will be set in the packet info command status field. This
 *		API also consider restriction regarding buffer alignments and
 *		sizes.
 *		The user must return a Tx resource after ensuring the buffer
 *		has been transmitted to enable the Tx ring indexes to update.
 *
 *		BOARD LAYOUT
 *		This device is on-board.  No jumper diagram is necessary.
 *
 *		EXTERNAL INTERFACE
 *
 *       Prior to calling the initialization routine eth_port_init() the user
 *	 must set the following fields under ETH_PORT_INFO struct:
 *       port_num             User Ethernet port number.
 *       port_mac_addr[6]	    User defined port MAC address.
 *       port_config          User port configuration value.
 *       port_config_extend    User port config extend value.
 *       port_sdma_config      User port SDMA config value.
 *       port_serial_control   User port serial control value.
 *       *port_private        User scratch pad for user specific data structures.
 *		                                                                       
 *       This driver introduce a set of default values:
 *       PORT_CONFIG_VALUE           Default port configuration value
 *       PORT_CONFIG_EXTEND_VALUE    Default port extend configuration value
 *       PORT_SDMA_CONFIG_VALUE      Default sdma control value
 *       PORT_SERIAL_CONTROL_VALUE   Default port serial control value
 *
 *		This driver data flow is done using the PKT_INFO struct which is
 *		a unified struct for Rx and Tx operations:
 *		byte_cnt	Tx/Rx descriptor buffer byte count.
 *		l4i_chk		CPU provided TCP Checksum. For Tx operation only.
 *		cmd_sts		Tx/Rx descriptor command status.
 *		buf_ptr		Tx/Rx descriptor buffer pointer.
 *		return_info	Tx/Rx user resource return information.
 *
 *******************************************************************************/
/* includes */

/* defines */
/* SDMA command macros */
#define ETH_ENABLE_TX_QUEUE(eth_port) \
 MV_WRITE(MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG(eth_port), 1)

#define ETH_DISABLE_TX_QUEUE(eth_port) \
 MV_WRITE(MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG(eth_port), (1 << 8))

#define ETH_ENABLE_RX_QUEUE(rx_queue, eth_port) \
MV_WRITE(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG(eth_port), (1 << rx_queue))

#define ETH_DISABLE_RX_QUEUE(rx_queue, eth_port) \
MV_WRITE(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG(eth_port), (1 << (8 + rx_queue)))

#define LINK_UP_TIMEOUT		100000
#define PHY_BUSY_TIMEOUT	10000000

/* locals */

/* PHY routines */
#ifdef MDD_CUT
static void ethernet_phy_set(ETH_PORT eth_port_num, int phy_addr);
#endif
static int ethernet_phy_get(ETH_PORT eth_port_num);

/* Ethernet Port routines */
static bool eth_port_uc_addr(ETH_PORT eth_port_num,
			     unsigned char uc_nibble, int option);

#ifdef MDD_CUT
static void eth_b_copy(unsigned int src_addr, unsigned int dst_addr,
		       int byte_count);
#endif

/*******************************************************************************
* eth_port_init - Initialize the Ethernet port driver
*
* DESCRIPTION:
*       This function prepares the ethernet port to start its activity:
*       1) Completes the ethernet port driver struct initialization toward port
*           start routine.
*       2) Resets the device to a quiescent state in case of warm reboot.
*       3) Enable SDMA access to all four DRAM banks as well as internal SRAM.
*       4) Clean MAC tables. The reset status of those tables is unknown.
*       5) Set PHY address. 
*       Note: Call this routine prior to eth_port_start routine and after setting
*       user values in the user fields of Ethernet port control struct.
*
* INPUT:
*       ETH_PORT_INFO 	*p_eth_port_ctrl       Ethernet port control struct
*
* OUTPUT:
*       See description.
*
* RETURN:
*       None.
*
*******************************************************************************/
static void eth_port_init(ETH_PORT_INFO * p_eth_port_ctrl)
{
	p_eth_port_ctrl->port_config = PORT_CONFIG_VALUE;
	p_eth_port_ctrl->port_config_extend = PORT_CONFIG_EXTEND_VALUE;
#if defined(__BIG_ENDIAN)
	p_eth_port_ctrl->port_sdma_config = PORT_SDMA_CONFIG_VALUE;
#elif defined(__LITTLE_ENDIAN)
	p_eth_port_ctrl->port_sdma_config = PORT_SDMA_CONFIG_VALUE |
		ETH_BLM_RX_NO_SWAP | ETH_BLM_TX_NO_SWAP;
#else
#error One of __LITTLE_ENDIAN or __BIG_ENDIAN must be defined!
#endif
	p_eth_port_ctrl->port_serial_control = PORT_SERIAL_CONTROL_VALUE;

	p_eth_port_ctrl->port_rx_queue_command = 0;
	p_eth_port_ctrl->port_tx_queue_command = 0;

	p_eth_port_ctrl->rx_resource_err = false;
	p_eth_port_ctrl->tx_resource_err = false;

	eth_port_reset(p_eth_port_ctrl->port_num);

	eth_port_init_mac_tables(p_eth_port_ctrl->port_num);

	ethernet_phy_reset(p_eth_port_ctrl->port_num);
}

/*******************************************************************************
* eth_port_start - Start the Ethernet port activity.
*
* DESCRIPTION:
*       This routine prepares the Ethernet port for Rx and Tx activity:
*       1. Initialize Tx and Rx Current Descriptor Pointer for each queue that
*           has been initialized a descriptor's ring (using ether_init_tx_desc_ring 
*           for Tx and ether_init_rx_desc_ring for Rx)
*       2. Initialize and enable the Ethernet configuration port by writing to 
*           the port's configuration and command registers.
*       3. Initialize and enable the SDMA by writing to the SDMA's 
*    configuration and command registers.
*       After completing these steps, the ethernet port SDMA can starts to 
*       perform Rx and Tx activities.
*
*       Note: Each Rx and Tx queue descriptor's list must be initialized prior  
*       to calling this function (use ether_init_tx_desc_ring for Tx queues and 
*       ether_init_rx_desc_ring for Rx queues).
*
* INPUT:
*       ETH_PORT_INFO 	*p_eth_port_ctrl       Ethernet port control struct
*
* OUTPUT:
*       Ethernet port is ready to receive and transmit.
*
* RETURN:
*       false if the port PHY is not up.
*       true otherwise.
*
*******************************************************************************/
static bool eth_port_start(ETH_PORT_INFO * p_eth_port_ctrl)
{
	int tx_curr_desc;
	int rx_curr_desc;
	unsigned int phy_reg_data;
	ETH_PORT eth_port_num = p_eth_port_ctrl->port_num;

	/* Assignment of Tx CTRP of given queue */
	tx_curr_desc = p_eth_port_ctrl->tx_curr_desc_q;
	MV_WRITE(MV64340_ETH_TX_CURRENT_QUEUE_DESC_PTR_0(eth_port_num),
		 (u32)&(p_eth_port_ctrl->p_tx_desc_area[tx_curr_desc]));

	/* Assignment of Rx CRDP of given queue */
	rx_curr_desc = p_eth_port_ctrl->rx_curr_desc_q;
	MV_WRITE(MV64340_ETH_RX_CURRENT_QUEUE_DESC_PTR_0(eth_port_num),
		 virt_to_bus(&(p_eth_port_ctrl->p_rx_desc_area[rx_curr_desc])));

	/* Add the assigned Ethernet address to the port's address table */
	eth_port_uc_addr_set(p_eth_port_ctrl->port_num,
			     p_eth_port_ctrl->port_mac_addr);

	/* Assign port configuration and command. */
	MV_WRITE(MV64340_ETH_PORT_CONFIG_REG(eth_port_num),
		 p_eth_port_ctrl->port_config);

	MV_WRITE(MV64340_ETH_PORT_CONFIG_EXTEND_REG(eth_port_num),
		 p_eth_port_ctrl->port_config_extend);

	MV_WRITE(MV64340_ETH_PORT_SERIAL_CONTROL_REG(eth_port_num),
		 p_eth_port_ctrl->port_serial_control);

	MV_SET_REG_BITS(MV64340_ETH_PORT_SERIAL_CONTROL_REG(eth_port_num),
			ETH_SERIAL_PORT_ENABLE);

	/* Assign port SDMA configuration */
	MV_WRITE(MV64340_ETH_SDMA_CONFIG_REG(eth_port_num),
		 p_eth_port_ctrl->port_sdma_config);

	/* Enable port Rx. */
	MV_WRITE(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG(eth_port_num),
		 p_eth_port_ctrl->port_rx_queue_command);

	/* Check if link is up */
	eth_port_read_smi_reg(eth_port_num, 1, &phy_reg_data);

	if (!(phy_reg_data & 0x20))
		return false;

	return true;
}

/*******************************************************************************
* eth_port_uc_addr_set - This function Set the port Unicast address.
*
* DESCRIPTION:
*		This function Set the port Ethernet MAC address.
*
* INPUT:
*	ETH_PORT eth_port_num     Port number.
*	char *        p_addr		Address to be set 
*
* OUTPUT:
*	Set MAC address low and high registers. also calls eth_port_uc_addr() 
*       To set the unicast table with the proper information.
*
* RETURN:
*	N/A.
*
*******************************************************************************/
static void eth_port_uc_addr_set(ETH_PORT eth_port_num,
				 unsigned char *p_addr)
{
	unsigned int mac_h;
	unsigned int mac_l;

	mac_l = (p_addr[4] << 8) | (p_addr[5]);
	mac_h = (p_addr[0] << 24) | (p_addr[1] << 16) |
	    (p_addr[2] << 8) | (p_addr[3] << 0);

	MV_WRITE(MV64340_ETH_MAC_ADDR_LOW(eth_port_num), mac_l);
	MV_WRITE(MV64340_ETH_MAC_ADDR_HIGH(eth_port_num), mac_h);

	/* Accept frames of this address */
	eth_port_uc_addr(eth_port_num, p_addr[5], ACCEPT_MAC_ADDR);

	return;
}

/*******************************************************************************
* eth_port_uc_addr - This function Set the port unicast address table
*
* DESCRIPTION:
*	This function locates the proper entry in the Unicast table for the 
*	specified MAC nibble and sets its properties according to function 
*	parameters.
*
* INPUT:
*	ETH_PORT 	eth_port_num      Port number.
*	unsigned char uc_nibble		Unicast MAC Address last nibble. 
*	int 			option      0 = Add, 1 = remove address.
*
* OUTPUT:
*	This function add/removes MAC addresses from the port unicast address
*	table. 
*
* RETURN:
*	true is output succeeded.
*	false if option parameter is invalid.
*
*******************************************************************************/
static bool eth_port_uc_addr(ETH_PORT eth_port_num,
			     unsigned char uc_nibble, int option)
{
	unsigned int unicast_reg;
	unsigned int tbl_offset;
	unsigned int reg_offset;

	/* Locate the Unicast table entry */
	uc_nibble = (0xf & uc_nibble);
	tbl_offset = (uc_nibble / 4) * 4;	/* Register offset from unicast table base */
	reg_offset = uc_nibble % 4;	/* Entry offset within the above register */

	switch (option) {
	case REJECT_MAC_ADDR:
		/* Clear accepts frame bit at specified unicast DA table entry */
		unicast_reg =
		    MV_READ_DATA(
				 (MV64340_ETH_DA_FILTER_UNICAST_TABLE_BASE
				  (eth_port_num) + tbl_offset));

		unicast_reg &= (0x0E << (8 * reg_offset));

		MV_WRITE(
			 (MV64340_ETH_DA_FILTER_UNICAST_TABLE_BASE
			  (eth_port_num) + tbl_offset), unicast_reg);
		break;

	case ACCEPT_MAC_ADDR:
		/* Set accepts frame bit at unicast DA filter table entry */
		unicast_reg =
		    MV_READ_DATA(
				 (MV64340_ETH_DA_FILTER_UNICAST_TABLE_BASE
				  (eth_port_num) + tbl_offset));

		unicast_reg |= (0x01 << (8 * reg_offset));

		MV_WRITE(
			 (MV64340_ETH_DA_FILTER_UNICAST_TABLE_BASE
			  (eth_port_num) + tbl_offset), unicast_reg);

		break;

	default:
		return false;
	}
	return true;
}

/*******************************************************************************
* eth_port_init_mac_tables - Clear all entrance in the UC, SMC and OMC tables
*
* DESCRIPTION:
*       Go through all the DA filter tables (Unicast, Special Multicast & Other 
*       Multicast) and set each entry to 0.
*
* INPUT:
*	ETH_PORT    eth_port_num   Ethernet Port number. See ETH_PORT enum. 
*
* OUTPUT:
*       Multicast and Unicast packets are rejected.
*
* RETURN:
*       None.
*
*******************************************************************************/
static void eth_port_init_mac_tables(ETH_PORT eth_port_num)
{
	int table_index;

	/* Clear DA filter unicast table (Ex_dFUT) */
	for (table_index = 0; table_index <= 0xC; table_index += 4)
		MV_WRITE(
			 (MV64340_ETH_DA_FILTER_UNICAST_TABLE_BASE
			  (eth_port_num) + table_index), 0);

	for (table_index = 0; table_index <= 0xFC; table_index += 4) {
		/* Clear DA filter special multicast table (Ex_dFSMT) */
		MV_WRITE(
			 (MV64340_ETH_DA_FILTER_SPECIAL_MULTICAST_TABLE_BASE
			  (eth_port_num) + table_index), 0);
		/* Clear DA filter other multicast table (Ex_dFOMT) */
		MV_WRITE((MV64340_ETH_DA_FILTER_OTHER_MULTICAST_TABLE_BASE
			  (eth_port_num) + table_index), 0);
	}
}

/*******************************************************************************
* eth_clear_mib_counters - Clear all MIB counters
*
* DESCRIPTION:
*       This function clears all MIB counters of a specific ethernet port.
*       A read from the MIB counter will reset the counter.
*
* INPUT:
*	ETH_PORT    eth_port_num   Ethernet Port number. See ETH_PORT enum. 
*
* OUTPUT:
*       After reading all MIB counters, the counters resets.
*
* RETURN:
*       MIB counter value.
*
*******************************************************************************/
static void eth_clear_mib_counters(ETH_PORT eth_port_num)
{
	int i;
	unsigned int dummy;

	/* Perform dummy reads from MIB counters */
	for (i = ETH_MIB_GOOD_OCTETS_RECEIVED_LOW;
	     i < ETH_MIB_LATE_COLLISION; i += 4)
		dummy =
		    MV_READ_DATA(
				 (MV64340_ETH_MIB_COUNTERS_BASE
				  (eth_port_num) + i));

	return;
}


#ifdef MDD_CUT
/*******************************************************************************
* ethernet_phy_set - Set the ethernet port PHY address.
*
* DESCRIPTION:
*       This routine set the ethernet port PHY address according to given 
*       parameter.
*
* INPUT:
*		ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
*
* OUTPUT:
*       Set PHY Address Register with given PHY address parameter.
*
* RETURN:
*       None.
*
*******************************************************************************/
static void ethernet_phy_set(ETH_PORT eth_port_num, int phy_addr)
{
	unsigned int reg_data;

	reg_data = MV_READ_DATA(MV64340_ETH_PHY_ADDR_REG);

	reg_data &= ~(0x1F << (5 * eth_port_num));
	reg_data |= (phy_addr << (5 * eth_port_num));

	MV_WRITE(MV64340_ETH_PHY_ADDR_REG, reg_data);

	return;
}
#endif

/*******************************************************************************
 * ethernet_phy_get - Get the ethernet port PHY address.
 *
 * DESCRIPTION:
 *       This routine returns the given ethernet port PHY address.
 *
 * INPUT:
 *		ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *
 * OUTPUT:
 *       None.
 *
 * RETURN:
 *       PHY address.
 *
 *******************************************************************************/
static int ethernet_phy_get(ETH_PORT eth_port_num)
{
	unsigned int reg_data;

	reg_data = MV_READ_DATA(MV64340_ETH_PHY_ADDR_REG);

	return ((reg_data >> (5 * eth_port_num)) & 0x1f);
}

/*******************************************************************************
 * ethernet_phy_reset - Reset Ethernet port PHY.
 *
 * DESCRIPTION:
 *       This routine utilize the SMI interface to reset the ethernet port PHY.
 *       The routine waits until the link is up again or link up is timeout.
 *
 * INPUT:
 *	ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *
 * OUTPUT:
 *       The ethernet port PHY renew its link.
 *
 * RETURN:
 *       None.
 *
*******************************************************************************/
static bool ethernet_phy_reset(ETH_PORT eth_port_num)
{
	unsigned int time_out = 50;
	unsigned int phy_reg_data;

	/* Reset the PHY */
	eth_port_read_smi_reg(eth_port_num, 0, &phy_reg_data);
	phy_reg_data |= 0x8000;	/* Set bit 15 to reset the PHY */
	eth_port_write_smi_reg(eth_port_num, 0, phy_reg_data);

	/* Poll on the PHY LINK */
	do {
		eth_port_read_smi_reg(eth_port_num, 1, &phy_reg_data);

		if (time_out-- == 0)
			return false;
	}
	while (!(phy_reg_data & 0x20));

	return true;
}

/*******************************************************************************
 * eth_port_reset - Reset Ethernet port
 *
 * DESCRIPTION:
 * 	This routine resets the chip by aborting any SDMA engine activity and
 *      clearing the MIB counters. The Receiver and the Transmit unit are in 
 *      idle state after this command is performed and the port is disabled.
 *
 * INPUT:
 *	ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *
 * OUTPUT:
 *       Channel activity is halted.
 *
 * RETURN:
 *       None.
 *
 *******************************************************************************/
static void eth_port_reset(ETH_PORT eth_port_num)
{
	unsigned int reg_data;

	/* Stop Tx port activity. Check port Tx activity. */
	reg_data =
	    MV_READ_DATA(MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG(eth_port_num));

	if (reg_data & 0xFF) {
		/* Issue stop command for active channels only */
		MV_WRITE(MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG
			 (eth_port_num), (reg_data << 8));

		/* Wait for all Tx activity to terminate. */
		do {
			/* Check port cause register that all Tx queues are stopped */
			reg_data =
			    MV_READ_DATA
			    (MV64340_ETH_TRANSMIT_QUEUE_COMMAND_REG
			     (eth_port_num));
		}
		while (reg_data & 0xFF);
	}

	/* Stop Rx port activity. Check port Rx activity. */
	reg_data =
	    MV_READ_DATA(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG
			 (eth_port_num));

	if (reg_data & 0xFF) {
		/* Issue stop command for active channels only */
		MV_WRITE(MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG
			 (eth_port_num), (reg_data << 8));

		/* Wait for all Rx activity to terminate. */
		do {
			/* Check port cause register that all Rx queues are stopped */
			reg_data =
			    MV_READ_DATA
			    (MV64340_ETH_RECEIVE_QUEUE_COMMAND_REG
			     (eth_port_num));
		}
		while (reg_data & 0xFF);
	}


	/* Clear all MIB counters */
	eth_clear_mib_counters(eth_port_num);

	/* Reset the Enable bit in the Configuration Register */
	reg_data =
	    MV_READ_DATA(MV64340_ETH_PORT_SERIAL_CONTROL_REG (eth_port_num));
	reg_data &= ~ETH_SERIAL_PORT_ENABLE;
	MV_WRITE(MV64340_ETH_PORT_SERIAL_CONTROL_REG(eth_port_num), reg_data);

	return;
}

/*******************************************************************************
 * ethernet_set_config_reg - Set specified bits in configuration register.
 *
 * DESCRIPTION:
 *       This function sets specified bits in the given ethernet 
 *       configuration register. 
 *
 * INPUT:
 *	ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *      unsigned int    value   32 bit value.
 *
 * OUTPUT:
 *      The set bits in the value parameter are set in the configuration 
 *      register.
 *
 * RETURN:
 *      None.
 *
 *******************************************************************************/
static void ethernet_set_config_reg(ETH_PORT eth_port_num,
				    unsigned int value)
{
	unsigned int eth_config_reg;

	eth_config_reg =
	    MV_READ_DATA(MV64340_ETH_PORT_CONFIG_REG(eth_port_num));
	eth_config_reg |= value;
	MV_WRITE(MV64340_ETH_PORT_CONFIG_REG(eth_port_num),
		 eth_config_reg);

	return;
}

/*******************************************************************************
 * ethernet_get_config_reg - Get the port configuration register
 *
 * DESCRIPTION:
 *       This function returns the configuration register value of the given 
 *       ethernet port.
 *
 * INPUT:
 *	ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *
 * OUTPUT:
 *       None.
 *
 * RETURN:
 *       Port configuration register value.
 *
 *******************************************************************************/
static unsigned int ethernet_get_config_reg(ETH_PORT eth_port_num)
{
	unsigned int eth_config_reg;

	eth_config_reg = MV_READ_DATA(MV64340_ETH_PORT_CONFIG_EXTEND_REG
				      (eth_port_num));
	return eth_config_reg;
}


/*******************************************************************************
 * eth_port_read_smi_reg - Read PHY registers
 *
 * DESCRIPTION:
 *       This routine utilize the SMI interface to interact with the PHY in 
 *       order to perform PHY register read.
 *
 * INPUT:
 *	ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *       unsigned int   phy_reg   PHY register address offset.
 *       unsigned int   *value   Register value buffer.
 *
 * OUTPUT:
 *       Write the value of a specified PHY register into given buffer.
 *
 * RETURN:
 *       false if the PHY is busy or read data is not in valid state.
 *       true otherwise.
 *
 *******************************************************************************/
static bool eth_port_read_smi_reg(ETH_PORT eth_port_num,
				  unsigned int phy_reg,
				  unsigned int *value)
{
	unsigned int reg_value;
	unsigned int time_out = PHY_BUSY_TIMEOUT;
	int phy_addr;

	phy_addr = ethernet_phy_get(eth_port_num);

	/* first check that it is not busy */
	do {
		reg_value = MV_READ_DATA(MV64340_ETH_SMI_REG);
		if (time_out-- == 0) {
			return false;
		}
	}
	while (reg_value & ETH_SMI_BUSY);

	/* not busy */

	MV_WRITE(MV64340_ETH_SMI_REG,
		 (phy_addr << 16) | (phy_reg << 21) | ETH_SMI_OPCODE_READ);

	time_out = PHY_BUSY_TIMEOUT;	/* initialize the time out var again */

	do {
		reg_value = MV_READ_DATA(MV64340_ETH_SMI_REG);
		if (time_out-- == 0) {
			return false;
		}
	}
	while (reg_value & ETH_SMI_READ_VALID);

	/* Wait for the data to update in the SMI register */
	for (time_out = 0; time_out < PHY_BUSY_TIMEOUT; time_out++);

	reg_value = MV_READ_DATA(MV64340_ETH_SMI_REG);

	*value = reg_value & 0xffff;

	return true;
}

/*******************************************************************************
 * eth_port_write_smi_reg - Write to PHY registers
 *
 * DESCRIPTION:
 *       This routine utilize the SMI interface to interact with the PHY in 
 *       order to perform writes to PHY registers.
 *
 * INPUT:
 *	ETH_PORT   eth_port_num   Ethernet Port number. See ETH_PORT enum. 
 *      unsigned int   phy_reg   PHY register address offset.
 *      unsigned int    value   Register value.
 *
 * OUTPUT:
 *      Write the given value to the specified PHY register.
 *
 * RETURN:
 *      false if the PHY is busy.
 *      true otherwise.
 *
 *******************************************************************************/
static bool eth_port_write_smi_reg(ETH_PORT eth_port_num,
				   unsigned int phy_reg,
				   unsigned int value)
{
	unsigned int reg_value;
	unsigned int time_out = PHY_BUSY_TIMEOUT;
	int phy_addr;

	phy_addr = ethernet_phy_get(eth_port_num);

	/* first check that it is not busy */
	do {
		reg_value = MV_READ_DATA(MV64340_ETH_SMI_REG);
		if (time_out-- == 0) {
			return false;
		}
	}
	while (reg_value & ETH_SMI_BUSY);

	/* not busy */
	MV_WRITE(MV64340_ETH_SMI_REG, (phy_addr << 16) | (phy_reg << 21) |
		 ETH_SMI_OPCODE_WRITE | (value & 0xffff));
	return true;
}

/*******************************************************************************
 * ether_init_rx_desc_ring - Curve a Rx chain desc list and buffer in memory.
 *
 * DESCRIPTION:
 *       This function prepares a Rx chained list of descriptors and packet 
 *       buffers in a form of a ring. The routine must be called after port 
 *       initialization routine and before port start routine. 
 *       The Ethernet SDMA engine uses CPU bus addresses to access the various 
 *       devices in the system (i.e. DRAM). This function uses the ethernet 
 *       struct 'virtual to physical' routine (set by the user) to set the ring 
 *       with physical addresses.
 *
 * INPUT:
 *	ETH_PORT_INFO   *p_eth_port_ctrl   Ethernet Port Control srtuct. 
 *      int 			rx_desc_num       Number of Rx descriptors
 *      int 			rx_buff_size      Size of Rx buffer
 *      unsigned int    rx_desc_base_addr  Rx descriptors memory area base addr.
 *      unsigned int    rx_buff_base_addr  Rx buffer memory area base addr.
 *
 * OUTPUT:
 *      The routine updates the Ethernet port control struct with information 
 *      regarding the Rx descriptors and buffers.
 *
 * RETURN:
 *      false if the given descriptors memory area is not aligned according to
 *      Ethernet SDMA specifications.
 *      true otherwise.
 *
 *******************************************************************************/
static bool ether_init_rx_desc_ring(ETH_PORT_INFO * p_eth_port_ctrl,
				    int rx_desc_num,
				    int rx_buff_size,
				    unsigned long rx_desc_base_addr,
				    unsigned long rx_buff_base_addr)
{
	volatile ETH_RX_DESC* p_rx_desc;
	unsigned long buffer_addr;
	int ix;			/* a counter */

	buffer_addr = rx_buff_base_addr;

	p_rx_desc = (ETH_RX_DESC *) rx_desc_base_addr;

	/* Rx desc Must be 4LW aligned (i.e. Descriptor_Address[3:0]=0000). */
	if (rx_buff_base_addr & 0xF)
		return false;

	/* Rx buffers are limited to 64K bytes and Minimum size is 8 bytes  */
	if ((rx_buff_size < 8) || (rx_buff_size > RX_BUFFER_MAX_SIZE))
		return false;

	/* Rx buffers must be 64-bit aligned.       */
	if ((rx_buff_base_addr + rx_buff_size) & 0x7)
		return false;

	/* initialize the Rx descriptors ring */
	for (ix = 0; ix < rx_desc_num; ix++) {
		p_rx_desc[ix].buf_size = rx_buff_size;
		p_rx_desc[ix].byte_cnt = 0x0000;
		p_rx_desc[ix].cmd_sts =
		    ETH_BUFFER_OWNED_BY_DMA | ETH_RX_ENABLE_INTERRUPT;
		p_rx_desc[ix].next_desc_ptr = virt_to_bus(&(p_rx_desc[ix+1]));
		p_rx_desc[ix].buf_ptr = buffer_addr;

		dma_cache_wback_inv((unsigned long)p_rx_desc, sizeof(ETH_RX_DESC));
		p_eth_port_ctrl->rx_skb[ix] = NULL;
		buffer_addr += rx_buff_size;
	}

	/* Closing Rx descriptors ring */
	p_rx_desc[rx_desc_num-1].next_desc_ptr = virt_to_bus(&(p_rx_desc[0]));
	dma_cache_wback_inv((unsigned long)p_rx_desc, sizeof(ETH_RX_DESC));

	/* Save Rx desc pointer to driver struct. */
	p_eth_port_ctrl->rx_curr_desc_q = 0;
	p_eth_port_ctrl->rx_used_desc_q = 0;

	p_eth_port_ctrl->p_rx_desc_area = (ETH_RX_DESC *) rx_desc_base_addr;
	p_eth_port_ctrl->rx_desc_area_size = rx_desc_num * sizeof(ETH_RX_DESC);

	p_eth_port_ctrl->port_rx_queue_command |= 1;

	return true;
}

/*******************************************************************************
 * ether_init_tx_desc_ring - Curve a Tx chain desc list and buffer in memory.
 *
 * DESCRIPTION:
 *       This function prepares a Tx chained list of descriptors and packet 
 *       buffers in a form of a ring. The routine must be called after port 
 *       initialization routine and before port start routine. 
 *       The Ethernet SDMA engine uses CPU bus addresses to access the various 
 *       devices in the system (i.e. DRAM). This function uses the ethernet 
 *       struct 'virtual to physical' routine (set by the user) to set the ring 
 *       with physical addresses.
 *
 * INPUT:
 *	ETH_PORT_INFO   *p_eth_port_ctrl   Ethernet Port Control srtuct. 
 *      int 		tx_desc_num        Number of Tx descriptors
 *      int 		tx_buff_size	   Size of Tx buffer
 *      unsigned int    tx_desc_base_addr  Tx descriptors memory area base addr.
 *
 * OUTPUT:
 *      The routine updates the Ethernet port control struct with information 
 *      regarding the Tx descriptors and buffers.
 *
 * RETURN:
 *      false if the given descriptors memory area is not aligned according to
 *      Ethernet SDMA specifications.
 *      true otherwise.
 *
 *******************************************************************************/
static bool ether_init_tx_desc_ring(ETH_PORT_INFO * p_eth_port_ctrl,
				    int tx_desc_num,
				    unsigned long tx_desc_base_addr)
{

	ETH_TX_DESC *p_tx_desc;
	int ix;			/* a counter */

	/* Tx desc Must be 4LW aligned (i.e. Descriptor_Address[3:0]=0000). */
	if (tx_desc_base_addr & 0xF)
		return false;

	/* save the first desc pointer to link with the last descriptor */
	p_tx_desc = (ETH_TX_DESC *) tx_desc_base_addr;

	/* Initialize the Tx descriptors ring */
	for (ix = 0; ix < tx_desc_num; ix++) {
		p_tx_desc[ix].byte_cnt = 0x0000;
		p_tx_desc[ix].l4i_chk = 0x0000;
		p_tx_desc[ix].cmd_sts = 0x00000000;
		p_tx_desc[ix].next_desc_ptr = (u32)&(p_tx_desc[ix+1]);
		p_tx_desc[ix].buf_ptr = 0x00000000;
		dma_cache_wback_inv((unsigned long)&(p_tx_desc[ix]), sizeof(ETH_TX_DESC));
		p_eth_port_ctrl->tx_skb[ix] = NULL;
	}

	/* Closing Tx descriptors ring */
	p_tx_desc[tx_desc_num-1].next_desc_ptr = (u32)&(p_tx_desc[0]);
	dma_cache_wback_inv((unsigned long)&(p_tx_desc[tx_desc_num-1]),
			sizeof(ETH_TX_DESC));

	/* Set Tx desc pointer in driver struct. */
	p_eth_port_ctrl->tx_curr_desc_q = 0;
	p_eth_port_ctrl->tx_used_desc_q = 0;
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
        p_eth_port_ctrl->tx_first_desc_q = 0;
#endif
	/* Init Tx ring base and size parameters */
	p_eth_port_ctrl->p_tx_desc_area = (ETH_TX_DESC*) tx_desc_base_addr;
	p_eth_port_ctrl->tx_desc_area_size = tx_desc_num * sizeof(ETH_TX_DESC);

	/* Add the queue to the list of Tx queues of this port */
	p_eth_port_ctrl->port_tx_queue_command |= 1;

	return true;
}

/*******************************************************************************
 * eth_port_send - Send an Ethernet packet
 *
 * DESCRIPTION:
 *	This routine send a given packet described by p_pktinfo parameter. It 
 *      supports transmitting of a packet spaned over multiple buffers. The 
 *      routine updates 'curr' and 'first' indexes according to the packet 
 *      segment passed to the routine. In case the packet segment is first, 
 *      the 'first' index is update. In any case, the 'curr' index is updated. 
 *      If the routine get into Tx resource error it assigns 'curr' index as 
 *      'first'. This way the function can abort Tx process of multiple 
 *      descriptors per packet.
 *
 * INPUT:
 *	ETH_PORT_INFO   *p_eth_port_ctrl   Ethernet Port Control srtuct. 
 *	PKT_INFO        *p_pkt_info       User packet buffer.
 *
 * OUTPUT:
 *	Tx ring 'curr' and 'first' indexes are updated. 
 *
 * RETURN:
 *      ETH_QUEUE_FULL in case of Tx resource error.
 *	ETH_ERROR in case the routine can not access Tx desc ring.
 *	ETH_QUEUE_LAST_RESOURCE if the routine uses the last Tx resource.
 *      ETH_OK otherwise.
 *
 *******************************************************************************/
#ifdef  MV64340_CHECKSUM_OFFLOAD_TX
/*
 * Modified to include the first descriptor pointer in case of SG
 */
static ETH_FUNC_RET_STATUS eth_port_send(ETH_PORT_INFO * p_eth_port_ctrl,
                                         PKT_INFO * p_pkt_info)
{
        int tx_desc_curr, tx_desc_used, tx_first_desc, tx_next_desc;
        volatile ETH_TX_DESC* current_descriptor;
        volatile ETH_TX_DESC* first_descriptor;
        u32 command_status, first_chip_ptr;

        /* Do not process Tx ring in case of Tx ring resource error */
        if (p_eth_port_ctrl->tx_resource_err == true)
                return ETH_QUEUE_FULL;

        /* Get the Tx Desc ring indexes */
        tx_desc_curr = p_eth_port_ctrl->tx_curr_desc_q;
        tx_desc_used = p_eth_port_ctrl->tx_used_desc_q;

        current_descriptor = &(p_eth_port_ctrl->p_tx_desc_area[tx_desc_curr]);
        if (current_descriptor == NULL)
                return ETH_ERROR;

        tx_next_desc = (tx_desc_curr + 1) % MV64340_TX_QUEUE_SIZE;
        command_status = p_pkt_info->cmd_sts | ETH_ZERO_PADDING | ETH_GEN_CRC;

        if (command_status & ETH_TX_FIRST_DESC) {
                tx_first_desc = tx_desc_curr;
                p_eth_port_ctrl->tx_first_desc_q = tx_first_desc;

                /* fill first descriptor */
                first_descriptor = &(p_eth_port_ctrl->p_tx_desc_area[tx_desc_curr]);
                first_descriptor->l4i_chk = p_pkt_info->l4i_chk;
                first_descriptor->cmd_sts = command_status;
                first_descriptor->byte_cnt = p_pkt_info->byte_cnt;
                first_descriptor->buf_ptr = p_pkt_info->buf_ptr;
                first_descriptor->next_desc_ptr = (u32)&(p_eth_port_ctrl->p_tx_desc_area[tx_next_desc]);
                dma_cache_wback_inv((unsigned long)first_descriptor, sizeof(ETH_TX_DESC));
		wmb();
        }
        else {
                tx_first_desc = p_eth_port_ctrl->tx_first_desc_q;
                first_descriptor = &(p_eth_port_ctrl->p_tx_desc_area[tx_first_desc]);
                if (first_descriptor == NULL) {
                        printk("First desc is NULL !!\n");
                        return ETH_ERROR;
                }
                if (command_status & ETH_TX_LAST_DESC)
                        current_descriptor->next_desc_ptr = 0x00000000;
                else {
                        command_status |= ETH_BUFFER_OWNED_BY_DMA;
                        current_descriptor->next_desc_ptr = (u32)&(p_eth_port_ctrl->p_tx_desc_area[tx_next_desc]);
                }
        }

        if (p_pkt_info->byte_cnt < 8) {
                printk(" < 8 problem \n");
                return ETH_ERROR;
        }

        current_descriptor->buf_ptr = p_pkt_info->buf_ptr;
        current_descriptor->byte_cnt = p_pkt_info->byte_cnt;
        current_descriptor->l4i_chk = p_pkt_info->l4i_chk;
        current_descriptor->cmd_sts = command_status;
        dma_cache_wback_inv((unsigned long)current_descriptor, sizeof(ETH_TX_DESC));

        p_eth_port_ctrl->tx_skb[tx_desc_curr] =
                (struct sk_buff*)p_pkt_info->return_info;

	dma_cache_wback_inv((unsigned long)p_eth_port_ctrl, sizeof(ETH_PORT_INFO));
        wmb();

        /* Set last desc with DMA ownership and interrupt enable. */
        if (command_status & ETH_TX_LAST_DESC) {
                current_descriptor->cmd_sts = command_status |
                                        ETH_TX_ENABLE_INTERRUPT |
                                        ETH_BUFFER_OWNED_BY_DMA;

                if (!(command_status & ETH_TX_FIRST_DESC) ) {
                        first_descriptor->cmd_sts |= ETH_BUFFER_OWNED_BY_DMA;
                        dma_cache_wback_inv((unsigned long)first_descriptor, sizeof(ETH_TX_DESC));
                }
                dma_cache_wback_inv((unsigned long)current_descriptor, sizeof(ETH_TX_DESC));
                wmb();

                first_chip_ptr = MV_READ_DATA(MV64340_ETH_CURRENT_SERVED_TX_DESC_PTR(p_eth_port_ctrl->port_num));

                /* Apply send command */
                if (first_chip_ptr == 0x00000000) {
                        MV_WRITE(MV64340_ETH_TX_CURRENT_QUEUE_DESC_PTR_0(p_eth_port_ctrl->port_num), (u32)&(p_eth_port_ctrl->p_tx_desc_area[tx_first_desc]));
                }

                ETH_ENABLE_TX_QUEUE(p_eth_port_ctrl->port_num);

           /* Finish Tx packet. Update first desc in case of Tx resource error */
                tx_first_desc = tx_next_desc;
                p_eth_port_ctrl->tx_first_desc_q = tx_first_desc;
        }
        else {
                if (! (command_status & ETH_TX_FIRST_DESC) ) {
                        current_descriptor->cmd_sts = command_status;
                        dma_cache_wback_inv((unsigned long)current_descriptor,
                                                        sizeof(ETH_TX_DESC));
                        wmb();
                }
        }

        /* Check for ring index overlap in the Tx desc ring */
        if (tx_next_desc == tx_desc_used) {
                p_eth_port_ctrl->tx_resource_err = true;
                p_eth_port_ctrl->tx_curr_desc_q = tx_first_desc;
                return ETH_QUEUE_LAST_RESOURCE;
        }

        p_eth_port_ctrl->tx_curr_desc_q = tx_next_desc;
        dma_cache_wback_inv((unsigned long) p_eth_port_ctrl, sizeof(ETH_PORT_INFO));
        wmb();
        return ETH_OK;
}
#else
static ETH_FUNC_RET_STATUS eth_port_send(ETH_PORT_INFO * p_eth_port_ctrl,
					 PKT_INFO * p_pkt_info)
{
	int tx_desc_curr;
	int tx_desc_used;
	volatile ETH_TX_DESC* current_descriptor;
	unsigned int command_status;

	/* Do not process Tx ring in case of Tx ring resource error */
	if (p_eth_port_ctrl->tx_resource_err == true)
		return ETH_QUEUE_FULL;

	/* Get the Tx Desc ring indexes */
	tx_desc_curr = p_eth_port_ctrl->tx_curr_desc_q;
	tx_desc_used = p_eth_port_ctrl->tx_used_desc_q;
	current_descriptor = &(p_eth_port_ctrl->p_tx_desc_area[tx_desc_curr]);

	if (current_descriptor == NULL)
		return ETH_ERROR;

	command_status = p_pkt_info->cmd_sts | ETH_ZERO_PADDING | ETH_GEN_CRC;

/* XXX Is this for real ?!?!? */
	/* Buffers with a payload smaller than 8 bytes must be aligned to a
	 * 64-bit boundary. We use the memory allocated for Tx descriptor.
	 * This memory is located in TX_BUF_OFFSET_IN_DESC offset within the
	 * Tx descriptor. */
	if (p_pkt_info->byte_cnt <= 8) {
		printk(KERN_ERR
		       "You have failed in the < 8 bytes errata - fixme\n");
		return ETH_ERROR;
	}
	current_descriptor->buf_ptr = p_pkt_info->buf_ptr;
	current_descriptor->byte_cnt = p_pkt_info->byte_cnt;
	p_eth_port_ctrl->tx_skb[tx_desc_curr] =
		(struct sk_buff*)p_pkt_info->return_info;

	mb();

	/* Set last desc with DMA ownership and interrupt enable. */
	current_descriptor->cmd_sts = command_status |
	    ETH_BUFFER_OWNED_BY_DMA | ETH_TX_ENABLE_INTERRUPT;

#if 0
	/* Flush CPU pipe */
	dma_cache_wback_inv(current_descriptor, sizeof(ETH_TX_DESC));
	mb();
#endif

	/* Apply send command */
	ETH_ENABLE_TX_QUEUE(p_eth_port_ctrl->port_num);

	/* Finish Tx packet. Update first desc in case of Tx resource error */
	tx_desc_curr = (tx_desc_curr + 1) % MV64340_TX_QUEUE_SIZE;

	/* Update the current descriptor */
 	p_eth_port_ctrl->tx_curr_desc_q = tx_desc_curr;

	/* Check for ring index overlap in the Tx desc ring */
	if (tx_desc_curr == tx_desc_used) {
		p_eth_port_ctrl->tx_resource_err = true;
		return ETH_QUEUE_LAST_RESOURCE;
	}

	return ETH_OK;
}
#endif

/*******************************************************************************
 * eth_tx_return_desc - Free all used Tx descriptors
 *
 * DESCRIPTION:
 *	This routine returns the transmitted packet information to the caller.
 *      It uses the 'first' index to support Tx desc return in case a transmit 
 *      of a packet spanned over multiple buffer still in process.
 *      In case the Tx queue was in "resource error" condition, where there are 
 *      no available Tx resources, the function resets the resource error flag.
 *
 * INPUT:
 *	ETH_PORT_INFO   *p_eth_port_ctrl   Ethernet Port Control srtuct. 
 *	PKT_INFO        *p_pkt_info       User packet buffer.
 *
 * OUTPUT:
 *	Tx ring 'first' and 'used' indexes are updated. 
 *
 * RETURN:
 *	ETH_ERROR in case the routine can not access Tx desc ring.
 *      ETH_RETRY in case there is transmission in process.
 *	ETH_END_OF_JOB if the routine has nothing to release.
 *      ETH_OK otherwise.
 *
 *******************************************************************************/
static ETH_FUNC_RET_STATUS eth_tx_return_desc(ETH_PORT_INFO * p_eth_port_ctrl,
					      PKT_INFO * p_pkt_info)
{
	int tx_desc_used, tx_desc_curr;
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
        int tx_first_desc;
#endif
	volatile ETH_TX_DESC *p_tx_desc_used;
	unsigned int command_status;

	/* Get the Tx Desc ring indexes */
	tx_desc_curr = p_eth_port_ctrl->tx_curr_desc_q;
	tx_desc_used = p_eth_port_ctrl->tx_used_desc_q;
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
        tx_first_desc = p_eth_port_ctrl->tx_first_desc_q;
#endif
	p_tx_desc_used = &(p_eth_port_ctrl->p_tx_desc_area[tx_desc_used]);

	/* XXX Sanity check */
	if (p_tx_desc_used == NULL)
		return ETH_ERROR;

	command_status = p_tx_desc_used->cmd_sts;

	/* Still transmitting... */
#ifndef MV64340_CHECKSUM_OFFLOAD_TX
	if (command_status & (ETH_BUFFER_OWNED_BY_DMA)) {
	  dma_cache_wback_inv((unsigned long)p_tx_desc_used,
				    sizeof(ETH_TX_DESC));
		return ETH_RETRY;
	}
#endif
	/* Stop release. About to overlap the current available Tx descriptor */
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
	if ((tx_desc_used == tx_first_desc) &&
#else
	if ((tx_desc_used == tx_desc_curr) &&
#endif
	    (p_eth_port_ctrl->tx_resource_err == false)) {
	    dma_cache_wback_inv((unsigned long)p_tx_desc_used,
				    sizeof(ETH_TX_DESC));
		return ETH_END_OF_JOB;
	}

	/* Pass the packet information to the caller */
	p_pkt_info->cmd_sts = command_status;
	p_pkt_info->return_info = p_eth_port_ctrl->tx_skb[tx_desc_used];
	p_eth_port_ctrl->tx_skb[tx_desc_used] = NULL;

	/* Update the next descriptor to release. */
	p_eth_port_ctrl->tx_used_desc_q =
		(tx_desc_used + 1) % MV64340_TX_QUEUE_SIZE;

	/* Any Tx return cancels the Tx resource error status */
	if (p_eth_port_ctrl->tx_resource_err == true)
		p_eth_port_ctrl->tx_resource_err = false;

	dma_cache_wback_inv((unsigned long)p_tx_desc_used, sizeof(ETH_TX_DESC));

	return ETH_OK;
}

/*******************************************************************************
 * eth_port_receive - Get received information from Rx ring.
 *
 * DESCRIPTION:
 * 	This routine returns the received data to the caller. There is no 
 *	data copying during routine operation. All information is returned 
 *	using pointer to packet information struct passed from the caller. 
 *      If the routine exhausts	Rx ring resources then the resource error flag 
 *      is set.  
 *
 * INPUT:
 *	ETH_PORT_INFO   *p_eth_port_ctrl   Ethernet Port Control srtuct. 
 *	PKT_INFO        *p_pkt_info       User packet buffer.
 *
 * OUTPUT:
 *	Rx ring current and used indexes are updated. 
 *
 * RETURN:
 *	ETH_ERROR in case the routine can not access Rx desc ring.
 *	ETH_QUEUE_FULL if Rx ring resources are exhausted.
 *	ETH_END_OF_JOB if there is no received data.
 *      ETH_OK otherwise.
 *
 *******************************************************************************/
static ETH_FUNC_RET_STATUS eth_port_receive(ETH_PORT_INFO *
					    p_eth_port_ctrl,
					    PKT_INFO * p_pkt_info)
{
	int rx_curr_desc;
	int rx_next_curr_desc;
	int rx_used_desc;
	unsigned int command_status;
	volatile ETH_RX_DESC*	p_rx_desc;

	/* Do not process Rx ring in case of Rx ring resource error */
	if (p_eth_port_ctrl->rx_resource_err == true)
		return ETH_QUEUE_FULL;

	/* Get the Rx Desc ring 'curr and 'used' indexes */
	rx_curr_desc = p_eth_port_ctrl->rx_curr_desc_q;
	rx_used_desc = p_eth_port_ctrl->rx_used_desc_q;

	p_rx_desc = &(p_eth_port_ctrl->p_rx_desc_area[rx_curr_desc]);

	/* The following parameters are used to save readings from memory */
	command_status = p_rx_desc->cmd_sts;

	/* Nothing to receive... */
	if (command_status & (ETH_BUFFER_OWNED_BY_DMA)) {
		dma_cache_wback_inv((unsigned long)p_rx_desc, sizeof(ETH_RX_DESC));
		return ETH_END_OF_JOB;
	}

	p_pkt_info->byte_cnt = (p_rx_desc->byte_cnt) - RX_BUF_OFFSET;
	p_pkt_info->cmd_sts = command_status;
	p_pkt_info->buf_ptr = (p_rx_desc->buf_ptr) + RX_BUF_OFFSET;
	p_pkt_info->return_info = p_eth_port_ctrl->rx_skb[rx_curr_desc];
	p_pkt_info->l4i_chk = p_rx_desc->buf_size;

	/* Clean the return info field to indicate that the packet has been */
	/* moved to the upper layers                                        */
	p_eth_port_ctrl->rx_skb[rx_curr_desc] = NULL;

	/* Update current index in data structure */
	rx_next_curr_desc = (rx_curr_desc + 1) % MV64340_RX_QUEUE_SIZE;
	p_eth_port_ctrl->rx_curr_desc_q = rx_next_curr_desc;

	/* Rx descriptors exhausted. Set the Rx ring resource error flag */
	if (rx_next_curr_desc == rx_used_desc)
		p_eth_port_ctrl->rx_resource_err = true;

	dma_cache_wback_inv((unsigned long)p_rx_desc, sizeof(ETH_RX_DESC));
	mb();
	return ETH_OK;
}

/*******************************************************************************
 * eth_rx_return_buff - Returns a Rx buffer back to the Rx ring.
 *
 * DESCRIPTION:
 *	This routine returns a Rx buffer back to the Rx ring. It retrieves the 
 *      next 'used' descriptor and attached the returned buffer to it.
 *      In case the Rx ring was in "resource error" condition, where there are 
 *      no available Rx resources, the function resets the resource error flag.
 *
 * INPUT:
 *	ETH_PORT_INFO   *p_eth_port_ctrl   Ethernet Port Control srtuct. 
 *      PKT_INFO        *p_pkt_info       Information on the returned buffer.
 *
 * OUTPUT:
 *	New available Rx resource in Rx descriptor ring.
 *
 * RETURN:
 *	ETH_ERROR in case the routine can not access Rx desc ring.
 *      ETH_OK otherwise.
 *
 *******************************************************************************/
static ETH_FUNC_RET_STATUS eth_rx_return_buff(ETH_PORT_INFO * p_eth_port_ctrl,
					      PKT_INFO * p_pkt_info)
{
	int used_rx_desc;	/* Where to return Rx resource */
	volatile ETH_RX_DESC* p_used_rx_desc;

	/* Get 'used' Rx descriptor */
	used_rx_desc = p_eth_port_ctrl->rx_used_desc_q;
	p_used_rx_desc = &(p_eth_port_ctrl->p_rx_desc_area[used_rx_desc]);

	p_used_rx_desc->buf_ptr = p_pkt_info->buf_ptr;
	p_used_rx_desc->buf_size = p_pkt_info->byte_cnt;
	p_eth_port_ctrl->rx_skb[used_rx_desc] = p_pkt_info->return_info;

	/* Flush the write pipe */
	mb();

	/* Return the descriptor to DMA ownership */
	p_used_rx_desc->cmd_sts =
	    ETH_BUFFER_OWNED_BY_DMA | ETH_RX_ENABLE_INTERRUPT;

	/* Flush descriptor and CPU pipe */
	dma_cache_wback_inv((unsigned long)p_used_rx_desc, sizeof(ETH_RX_DESC));
	mb();

	/* Move the used descriptor pointer to the next descriptor */
	p_eth_port_ctrl->rx_used_desc_q =
		(used_rx_desc + 1) % MV64340_RX_QUEUE_SIZE;

	/* Any Rx return cancels the Rx resource error status */
	if (p_eth_port_ctrl->rx_resource_err == true)
		p_eth_port_ctrl->rx_resource_err = false;

	return ETH_OK;
}

/*******************************************************************************
 * eth_port_set_rx_coal - Sets coalescing interrupt mechanism on RX path
 *
 * DESCRIPTION:
 *	This routine sets the RX coalescing interrupt mechanism parameter.
 *	This parameter is a timeout counter, that counts in 64 t_clk
 *	chunks ; that when timeout event occurs a maskable interrupt
 *	occurs.
 *	The parameter is calculated using the tClk of the MV-643xx chip
 *	, and the required delay of the interrupt in usec.
 *
 * INPUT:
 *	ETH_PORT eth_port_num      Ethernet port number
 *	unsigned int t_clk        t_clk of the MV-643xx chip in HZ units
 *	unsigned int delay       Delay in usec
 *
 * OUTPUT:
 *	Interrupt coalescing mechanism value is set in MV-643xx chip.
 *
 * RETURN:
 *	The interrupt coalescing value set in the gigE port.
 *
 *******************************************************************************/
static unsigned int eth_port_set_rx_coal(ETH_PORT eth_port_num,
					 unsigned int t_clk,
					 unsigned int delay)
{
	unsigned int coal;
	coal = ((t_clk / 1000000) * delay) / 64;
	/* Set RX Coalescing mechanism */
	MV_WRITE(MV64340_ETH_SDMA_CONFIG_REG(eth_port_num),
		 ((coal & 0x3fff) << 8) |
		 (MV_READ_DATA(MV64340_ETH_SDMA_CONFIG_REG(eth_port_num))
		  & 0xffc000ff));
	return coal;
}

/*******************************************************************************
 * eth_port_set_tx_coal - Sets coalescing interrupt mechanism on TX path
 *
 * DESCRIPTION:
 *	This routine sets the TX coalescing interrupt mechanism parameter.
 *	This parameter is a timeout counter, that counts in 64 t_clk
 *	chunks ; that when timeout event occurs a maskable interrupt
 *	occurs.
 *	The parameter is calculated using the t_cLK frequency of the 
 *	MV-643xx chip and the required delay in the interrupt in uSec
 *
 * INPUT:
 *	ETH_PORT eth_port_num      Ethernet port number
 *	unsigned int t_clk        t_clk of the MV-643xx chip in HZ units
 *	unsigned int delay       Delay in uSeconds
 *
 * OUTPUT:
 *	Interrupt coalescing mechanism value is set in MV-643xx chip.
 *
 * RETURN:
 *	The interrupt coalescing value set in the gigE port.
 *
 *******************************************************************************/
static unsigned int eth_port_set_tx_coal(ETH_PORT eth_port_num,
					 unsigned int t_clk,
					 unsigned int delay)
{
	unsigned int coal;
	coal = ((t_clk / 1000000) * delay) / 64;
	/* Set TX Coalescing mechanism */
	MV_WRITE(MV64340_ETH_TX_FIFO_URGENT_THRESHOLD_REG(eth_port_num),
		 coal << 4);
	return coal;
}

#ifdef MDD_CUT
/*******************************************************************************
 * eth_b_copy - Copy bytes from source to destination
 *
 * DESCRIPTION:
 *       This function supports the eight bytes limitation on Tx buffer size. 
 *       The routine will zero eight bytes starting from the destination address
 *       followed by copying bytes from the source address to the destination.
 *
 * INPUT:
 *       unsigned int src_addr    32 bit source address.
 *       unsigned int dst_addr    32 bit destination address.
 *       int        byte_count    Number of bytes to copy.
 *
 * OUTPUT:
 *       See description.
 *
 * RETURN:
 *       None.
 *
 *******************************************************************************/
static void eth_b_copy(unsigned int src_addr, unsigned int dst_addr,
		       int byte_count)
{
	/* Zero the dst_addr area */
	*(unsigned int *) dst_addr = 0x0;

	while (byte_count != 0) {
		*(char *) dst_addr = *(char *) src_addr;
		dst_addr++;
		src_addr++;
		byte_count--;
	}
}
#endif
