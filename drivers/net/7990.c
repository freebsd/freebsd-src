/* 
 * 7990.c -- LANCE ethernet IC generic routines. 
 * This is an attempt to separate out the bits of various ethernet
 * drivers that are common because they all use the AMD 7990 LANCE 
 * (Local Area Network Controller for Ethernet) chip.
 *
 * Copyright (C) 05/1998 Peter Maydell <pmaydell@chiark.greenend.org.uk>
 *
 * Most of this stuff was obtained by looking at other LANCE drivers,
 * in particular a2065.[ch]. The AMD C-LANCE datasheet was also helpful.
 * NB: this was made easy by the fact that Jes Sorensen had cleaned up
 * most of a2025 and sunlance with the aim of merging them, so the 
 * common code was pretty obvious.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/pgtable.h>
#include <linux/errno.h>

/* Used for the temporal inet entries and routing */
#include <linux/socket.h>
#include <linux/route.h>

#include <linux/dio.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "7990.h"

/* Lossage Factor Nine, Mr Sulu. */
#define WRITERAP(x) (lp->writerap(lp,x))
#define WRITERDP(x) (lp->writerdp(lp,x))
#define READRDP() (lp->readrdp(lp))
/* These used to be ll->rap = x, ll->rdp = x, and (ll->rdp). Sigh. 
 * If you want to switch them back then 
 * #define DECLARE_LL volatile struct lance_regs *ll = lp->ll
 */
#define DECLARE_LL /* nothing to declare */

/* debugging output macros, various flavours */
/* #define TEST_HITS */
#ifdef UNDEF
#define PRINT_RINGS() \
do { \
        int t; \
        for (t=0; t < RX_RING_SIZE; t++) { \
                printk("R%d: @(%02X %04X) len %04X, mblen %04X, bits %02X\n",\
                       t, ib->brx_ring[t].rmd1_hadr, ib->brx_ring[t].rmd0,\
                       ib->brx_ring[t].length,\
                       ib->brx_ring[t].mblength, ib->brx_ring[t].rmd1_bits);\
        }\
        for (t=0; t < TX_RING_SIZE; t++) { \
                printk("T%d: @(%02X %04X) len %04X, misc %04X, bits %02X\n",\
                       t, ib->btx_ring[t].tmd1_hadr, ib->btx_ring[t].tmd0,\
                       ib->btx_ring[t].length,\
                       ib->btx_ring[t].misc, ib->btx_ring[t].tmd1_bits);\
        }\
} while (0) 
#else
#define PRINT_RINGS()
#endif        

/* Load the CSR registers. The LANCE has to be STOPped when we do this! */
static void load_csrs (struct lance_private *lp)
{
        volatile struct lance_init_block *aib = lp->lance_init_block;
        int leptr;
        DECLARE_LL;

        leptr = LANCE_ADDR (aib);

        WRITERAP(LE_CSR1);                        /* load address of init block */
        WRITERDP(leptr & 0xFFFF);
        WRITERAP(LE_CSR2);
        WRITERDP(leptr >> 16);
        WRITERAP(LE_CSR3);
        WRITERDP(lp->busmaster_regval);           /* set byteswap/ALEctrl/byte ctrl */

        /* Point back to csr0 */
        WRITERAP(LE_CSR0);
}

/* #define to 0 or 1 appropriately */
#define DEBUG_IRING 0
/* Set up the Lance Rx and Tx rings and the init block */
static void lance_init_ring (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;
        volatile struct lance_init_block *ib = lp->init_block;
        volatile struct lance_init_block *aib; /* for LANCE_ADDR computations */
        int leptr;
        int i;

        aib = lp->lance_init_block;

        lp->rx_new = lp->tx_new = 0;
        lp->rx_old = lp->tx_old = 0;

        ib->mode = LE_MO_PROM;                             /* normal, enable Tx & Rx */

        /* Copy the ethernet address to the lance init block
         * Notice that we do a byteswap if we're big endian.
         * [I think this is the right criterion; at least, sunlance,
         * a2065 and atarilance do the byteswap and lance.c (PC) doesn't.
         * However, the datasheet says that the BSWAP bit doesn't affect
         * the init block, so surely it should be low byte first for
         * everybody? Um.] 
         * We could define the ib->physaddr as three 16bit values and
         * use (addr[1] << 8) | addr[0] & co, but this is more efficient.
         */
#ifdef __BIG_ENDIAN
        ib->phys_addr [0] = dev->dev_addr [1];
        ib->phys_addr [1] = dev->dev_addr [0];
        ib->phys_addr [2] = dev->dev_addr [3];
        ib->phys_addr [3] = dev->dev_addr [2];
        ib->phys_addr [4] = dev->dev_addr [5];
        ib->phys_addr [5] = dev->dev_addr [4];
#else
        for (i=0; i<6; i++)
           ib->phys_addr[i] = dev->dev_addr[i];
#endif        

        if (DEBUG_IRING)
                printk ("TX rings:\n");
    
	lp->tx_full = 0;
        /* Setup the Tx ring entries */
        for (i = 0; i < (1<<lp->lance_log_tx_bufs); i++) {
                leptr = LANCE_ADDR(&aib->tx_buf[i][0]);
                ib->btx_ring [i].tmd0      = leptr;
                ib->btx_ring [i].tmd1_hadr = leptr >> 16;
                ib->btx_ring [i].tmd1_bits = 0;
                ib->btx_ring [i].length    = 0xf000; /* The ones required by tmd2 */
                ib->btx_ring [i].misc      = 0;
                if (DEBUG_IRING) 
                   printk ("%d: 0x%8.8x\n", i, leptr);
        }

        /* Setup the Rx ring entries */
        if (DEBUG_IRING)
                printk ("RX rings:\n");
        for (i = 0; i < (1<<lp->lance_log_rx_bufs); i++) {
                leptr = LANCE_ADDR(&aib->rx_buf[i][0]);

                ib->brx_ring [i].rmd0      = leptr;
                ib->brx_ring [i].rmd1_hadr = leptr >> 16;
                ib->brx_ring [i].rmd1_bits = LE_R1_OWN;
                /* 0xf000 == bits that must be one (reserved, presumably) */
                ib->brx_ring [i].length    = -RX_BUFF_SIZE | 0xf000;
                ib->brx_ring [i].mblength  = 0;
                if (DEBUG_IRING)
                        printk ("%d: 0x%8.8x\n", i, leptr);
        }

        /* Setup the initialization block */
    
        /* Setup rx descriptor pointer */
        leptr = LANCE_ADDR(&aib->brx_ring);
        ib->rx_len = (lp->lance_log_rx_bufs << 13) | (leptr >> 16);
        ib->rx_ptr = leptr;
        if (DEBUG_IRING)
                printk ("RX ptr: %8.8x\n", leptr);
    
        /* Setup tx descriptor pointer */
        leptr = LANCE_ADDR(&aib->btx_ring);
        ib->tx_len = (lp->lance_log_tx_bufs << 13) | (leptr >> 16);
        ib->tx_ptr = leptr;
        if (DEBUG_IRING)
                printk ("TX ptr: %8.8x\n", leptr);

        /* Clear the multicast filter */
        ib->filter [0] = 0;
        ib->filter [1] = 0;
        PRINT_RINGS();
}

/* LANCE must be STOPped before we do this, too... */
static int init_restart_lance (struct lance_private *lp)
{
        int i;
        DECLARE_LL;

        WRITERAP(LE_CSR0);
        WRITERDP(LE_C0_INIT);

        /* Need a hook here for sunlance ledma stuff */

        /* Wait for the lance to complete initialization */
        for (i = 0; (i < 100) && !(READRDP() & (LE_C0_ERR | LE_C0_IDON)); i++)
                barrier();
        if ((i == 100) || (READRDP() & LE_C0_ERR)) {
                printk ("LANCE unopened after %d ticks, csr0=%4.4x.\n", i, READRDP());
                return -1;
        }

        /* Clear IDON by writing a "1", enable interrupts and start lance */
        WRITERDP(LE_C0_IDON);
        WRITERDP(LE_C0_INEA | LE_C0_STRT);

        return 0;
}

static int lance_reset (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *)dev->priv;
        int status;
        DECLARE_LL;
    
        /* Stop the lance */
        WRITERAP(LE_CSR0);
        WRITERDP(LE_C0_STOP);

        load_csrs (lp);
        lance_init_ring (dev);
        dev->trans_start = jiffies;
        status = init_restart_lance (lp);
#ifdef DEBUG_DRIVER
        printk ("Lance restart=%d\n", status);
#endif
        return status;
}

static int lance_rx (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;
        volatile struct lance_init_block *ib = lp->init_block;
        volatile struct lance_rx_desc *rd;
        unsigned char bits;
        int len = 0;                    /* XXX shut up gcc warnings */
        struct sk_buff *skb = 0;        /* XXX shut up gcc warnings */
#ifdef TEST_HITS
        int i;
#endif
        DECLARE_LL;

#ifdef TEST_HITS
        printk ("[");
        for (i = 0; i < RX_RING_SIZE; i++) {
                if (i == lp->rx_new)
                        printk ("%s",
                                ib->brx_ring [i].rmd1_bits & LE_R1_OWN ? "_" : "X");
                else
                        printk ("%s",
                                ib->brx_ring [i].rmd1_bits & LE_R1_OWN ? "." : "1");
        }
        printk ("]");
#endif
    
        WRITERDP(LE_C0_RINT | LE_C0_INEA);     /* ack Rx int, reenable ints */
        for (rd = &ib->brx_ring [lp->rx_new];     /* For each Rx ring we own... */
             !((bits = rd->rmd1_bits) & LE_R1_OWN);
             rd = &ib->brx_ring [lp->rx_new]) {

                /* We got an incomplete frame? */
                if ((bits & LE_R1_POK) != LE_R1_POK) {
                        lp->stats.rx_over_errors++;
                        lp->stats.rx_errors++;
                        continue;
                } else if (bits & LE_R1_ERR) {
                        /* Count only the end frame as a rx error,
                         * not the beginning
                         */
                        if (bits & LE_R1_BUF) lp->stats.rx_fifo_errors++;
                        if (bits & LE_R1_CRC) lp->stats.rx_crc_errors++;
                        if (bits & LE_R1_OFL) lp->stats.rx_over_errors++;
                        if (bits & LE_R1_FRA) lp->stats.rx_frame_errors++;
                        if (bits & LE_R1_EOP) lp->stats.rx_errors++;
                } else {
                        len = (rd->mblength & 0xfff) - 4;
                        skb = dev_alloc_skb (len+2);

                        if (skb == 0) {
                                printk ("%s: Memory squeeze, deferring packet.\n",
                                        dev->name);
                                lp->stats.rx_dropped++;
                                rd->mblength = 0;
                                rd->rmd1_bits = LE_R1_OWN;
                                lp->rx_new = (lp->rx_new + 1) & lp->rx_ring_mod_mask;
                                return 0;
                        }
            
                        skb->dev = dev;
                        skb_reserve (skb, 2);           /* 16 byte align */
                        skb_put (skb, len);             /* make room */
                        eth_copy_and_sum(skb,
                                         (unsigned char *)&(ib->rx_buf [lp->rx_new][0]),
                                         len, 0);
                        skb->protocol = eth_type_trans (skb, dev);
			netif_rx (skb);
			dev->last_rx = jiffies;
			lp->stats.rx_packets++;
			lp->stats.rx_bytes += len;
                }

                /* Return the packet to the pool */
                rd->mblength = 0;
                rd->rmd1_bits = LE_R1_OWN;
                lp->rx_new = (lp->rx_new + 1) & lp->rx_ring_mod_mask;
        }
        return 0;
}

static int lance_tx (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;
        volatile struct lance_init_block *ib = lp->init_block;
        volatile struct lance_tx_desc *td;
        int i, j;
        int status;
        DECLARE_LL;

        /* csr0 is 2f3 */
        WRITERDP(LE_C0_TINT | LE_C0_INEA);
        /* csr0 is 73 */

        j = lp->tx_old;
        for (i = j; i != lp->tx_new; i = j) {
                td = &ib->btx_ring [i];

                /* If we hit a packet not owned by us, stop */
                if (td->tmd1_bits & LE_T1_OWN)
                        break;
                
                if (td->tmd1_bits & LE_T1_ERR) {
                        status = td->misc;
            
                        lp->stats.tx_errors++;
                        if (status & LE_T3_RTY)  lp->stats.tx_aborted_errors++;
                        if (status & LE_T3_LCOL) lp->stats.tx_window_errors++;

                        if (status & LE_T3_CLOS) {
                                lp->stats.tx_carrier_errors++;
                                if (lp->auto_select) {
                                        lp->tpe = 1 - lp->tpe;
                                        printk("%s: Carrier Lost, trying %s\n",
                                               dev->name, lp->tpe?"TPE":"AUI");
                                        /* Stop the lance */
                                        WRITERAP(LE_CSR0);
                                        WRITERDP(LE_C0_STOP);
                                        lance_init_ring (dev);
                                        load_csrs (lp);
                                        init_restart_lance (lp);
                                        return 0;
                                }
                        }

                        /* buffer errors and underflows turn off the transmitter */
                        /* Restart the adapter */
                        if (status & (LE_T3_BUF|LE_T3_UFL)) {
                                lp->stats.tx_fifo_errors++;

                                printk ("%s: Tx: ERR_BUF|ERR_UFL, restarting\n",
                                        dev->name);
                                /* Stop the lance */
                                WRITERAP(LE_CSR0);
                                WRITERDP(LE_C0_STOP);
                                lance_init_ring (dev);
                                load_csrs (lp);
                                init_restart_lance (lp);
                                return 0;
                        }
                } else if ((td->tmd1_bits & LE_T1_POK) == LE_T1_POK) {
                        /*
                         * So we don't count the packet more than once.
                         */
                        td->tmd1_bits &= ~(LE_T1_POK);

                        /* One collision before packet was sent. */
                        if (td->tmd1_bits & LE_T1_EONE)
                                lp->stats.collisions++;

                        /* More than one collision, be optimistic. */
                        if (td->tmd1_bits & LE_T1_EMORE)
                                lp->stats.collisions += 2;

                        lp->stats.tx_packets++;
                }
        
                j = (j + 1) & lp->tx_ring_mod_mask;
        }
        lp->tx_old = j;
        WRITERDP(LE_C0_TINT | LE_C0_INEA);
        return 0;
}

static void lance_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
        struct net_device *dev = (struct net_device *)dev_id;
        struct lance_private *lp = (struct lance_private *)dev->priv;
        int csr0;
        DECLARE_LL;

	spin_lock (&lp->devlock);

        WRITERAP(LE_CSR0);              /* LANCE Controller Status */
        csr0 = READRDP();

        PRINT_RINGS();
        
        if (!(csr0 & LE_C0_INTR)) {     /* Check if any interrupt has */
		spin_lock (&lp->devlock);
                return;                 /* been generated by the Lance. */
	}

        /* Acknowledge all the interrupt sources ASAP */
        WRITERDP(csr0 & ~(LE_C0_INEA|LE_C0_TDMD|LE_C0_STOP|LE_C0_STRT|LE_C0_INIT));

        if ((csr0 & LE_C0_ERR)) {
                /* Clear the error condition */
                WRITERDP(LE_C0_BABL|LE_C0_ERR|LE_C0_MISS|LE_C0_INEA);
        }

        if (csr0 & LE_C0_RINT)
                lance_rx (dev);

        if (csr0 & LE_C0_TINT)
                lance_tx (dev);

        /* Log misc errors. */
        if (csr0 & LE_C0_BABL)
                lp->stats.tx_errors++;       /* Tx babble. */
        if (csr0 & LE_C0_MISS)
                lp->stats.rx_errors++;       /* Missed a Rx frame. */
        if (csr0 & LE_C0_MERR) {
                printk("%s: Bus master arbitration failure, status %4.4x.\n", 
                       dev->name, csr0);
                /* Restart the chip. */
                WRITERDP(LE_C0_STRT);
        }

        if (lp->tx_full && netif_queue_stopped(dev) && (TX_BUFFS_AVAIL >= 0)) {
		lp->tx_full = 0;
		netif_wake_queue (dev);
        }
        
        WRITERAP(LE_CSR0);
        WRITERDP(LE_C0_BABL|LE_C0_CERR|LE_C0_MISS|LE_C0_MERR|LE_C0_IDON|LE_C0_INEA);

	spin_unlock (&lp->devlock);
}

int lance_open (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *)dev->priv;
	int res;
        DECLARE_LL;
        
        /* Install the Interrupt handler. Or we could shunt this out to specific drivers? */
        if (request_irq(lp->irq, lance_interrupt, 0, lp->name, dev))
                return -EAGAIN;

        res = lance_reset(dev);
	lp->devlock = SPIN_LOCK_UNLOCKED;
	netif_start_queue (dev);

	return res;
}

int lance_close (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;
        DECLARE_LL;
        
	netif_stop_queue (dev);

        /* Stop the LANCE */
        WRITERAP(LE_CSR0);
        WRITERDP(LE_C0_STOP);

        free_irq(lp->irq, dev);

        return 0;
}

void lance_tx_timeout(struct net_device *dev)
{
	printk("lance_tx_timeout\n");
	lance_reset(dev);
	dev->trans_start = jiffies;
	netif_wake_queue (dev);
}


int lance_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *)dev->priv;
        volatile struct lance_init_block *ib = lp->init_block;
        int entry, skblen, len;
        static int outs;
	unsigned long flags;
        DECLARE_LL;

        if (!TX_BUFFS_AVAIL)
                return -1;

	netif_stop_queue (dev);

        skblen = skb->len;

#ifdef DEBUG_DRIVER
        /* dump the packet */
        {
                int i;
        
                for (i = 0; i < 64; i++) {
                        if ((i % 16) == 0)
                                printk ("\n");
                        printk ("%2.2x ", skb->data [i]);
                }
        }
#endif
        len = (skblen <= ETH_ZLEN) ? ETH_ZLEN : skblen;
        entry = lp->tx_new & lp->tx_ring_mod_mask;
        ib->btx_ring [entry].length = (-len) | 0xf000;
        ib->btx_ring [entry].misc = 0;
    
    	if(skb->len < ETH_ZLEN)
    		memset((char *)&ib->tx_buf[entry][0], 0, ETH_ZLEN);
        memcpy ((char *)&ib->tx_buf [entry][0], skb->data, skblen);
    
        /* Now, give the packet to the lance */
        ib->btx_ring [entry].tmd1_bits = (LE_T1_POK|LE_T1_OWN);
        lp->tx_new = (lp->tx_new+1) & lp->tx_ring_mod_mask;

        outs++;
        /* Kick the lance: transmit now */
        WRITERDP(LE_C0_INEA | LE_C0_TDMD);
        dev->trans_start = jiffies;
        dev_kfree_skb (skb);
    
	spin_lock_irqsave (&lp->devlock, flags);
        if (TX_BUFFS_AVAIL)
		netif_start_queue (dev);
	else
		lp->tx_full = 1;
	spin_unlock_irqrestore (&lp->devlock, flags);

        return 0;
}

struct net_device_stats *lance_get_stats (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;

        return &lp->stats;
}

/* taken from the depca driver via a2065.c */
static void lance_load_multicast (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;
        volatile struct lance_init_block *ib = lp->init_block;
        volatile u16 *mcast_table = (u16 *)&ib->filter;
        struct dev_mc_list *dmi=dev->mc_list;
        char *addrs;
        int i;
        u32 crc;
        
        /* set all multicast bits */
        if (dev->flags & IFF_ALLMULTI){ 
                ib->filter [0] = 0xffffffff;
                ib->filter [1] = 0xffffffff;
                return;
        }
        /* clear the multicast filter */
        ib->filter [0] = 0;
        ib->filter [1] = 0;

        /* Add addresses */
        for (i = 0; i < dev->mc_count; i++){
                addrs = dmi->dmi_addr;
                dmi   = dmi->next;

                /* multicast address? */
                if (!(*addrs & 1))
                        continue;
                
		crc = ether_crc_le(6, addrs);
                crc = crc >> 26;
                mcast_table [crc >> 4] |= 1 << (crc & 0xf);
        }
        return;
}


void lance_set_multicast (struct net_device *dev)
{
        struct lance_private *lp = (struct lance_private *) dev->priv;
        volatile struct lance_init_block *ib = lp->init_block;
	int stopped;
        DECLARE_LL;

	stopped = netif_queue_stopped(dev);
	if (!stopped)
		netif_stop_queue (dev);

        while (lp->tx_old != lp->tx_new)
                schedule();

        WRITERAP(LE_CSR0);
        WRITERDP(LE_C0_STOP);
        lance_init_ring (dev);

        if (dev->flags & IFF_PROMISC) {
                ib->mode |= LE_MO_PROM;
        } else {
                ib->mode &= ~LE_MO_PROM;
                lance_load_multicast (dev);
        }
        load_csrs (lp);
        init_restart_lance (lp);

	if (!stopped)
		netif_start_queue (dev);
}

MODULE_LICENSE("GPL");
