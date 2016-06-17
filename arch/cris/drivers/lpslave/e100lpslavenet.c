/* $Id: e100lpslavenet.c,v 1.5 2002/04/22 11:47:24 johana Exp $
 *
 * e100lpslavenet.c: A network driver for the ETRAX 100LX slave controller.
 *
 * Copyright (c) 1998-2001 Axis Communications AB.
 *
 * The outline of this driver comes from skeleton.c.
 *
 * $Log: e100lpslavenet.c,v $
 * Revision 1.5  2002/04/22 11:47:24  johana
 * Fix according to 2.4.19-pre7. time_after/time_before and
 * missing end of comment.
 * The patch has a typo for ethernet.c in e100_clear_network_leds(),
 *  that is fixed here.
 *
 * Revision 1.4  2001/06/21 16:55:26  olof
 * Minimized par port setup time to gain bandwidth
 *
 * Revision 1.3  2001/06/21 15:49:02  olof
 * Removed setting of default MAC address
 *
 * Revision 1.2  2001/06/11 15:39:52  olof
 * Clean up and sync with ethernet.c rev 1.16. Increased reset time of slave.
 *
 * Revision 1.1  2001/06/06 08:56:26  olof
 * Added support for slave Etrax defined by CONFIG_ETRAX_ETHERNET_LPSLAVE
 *
 */

#include <linux/config.h>

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/svinto.h>     /* DMA and register descriptions */
#include <asm/io.h>         /* LED_* I/O functions */
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include "e100lpslave.h"

/* #define ETHDEBUG */
#define D(x)

/*
 * The name of the card. Is used for messages and in the requests for
 * io regions, irqs and dma channels
 */

static const char* cardname = "Etrax 100LX ethernet slave controller";

/* A default ethernet address. Highlevel SW will set the real one later */

static struct sockaddr default_mac = {
	0,
        { 0x00, 0x40, 0x8C, 0xCD, 0x00, 0x00 }
};

/* Information that need to be kept for each board. */
struct net_local {
	struct net_device_stats stats;

	/* Tx control lock.  This protects the transmit buffer ring
	 * state along with the "tx full" state of the driver.  This
	 * means all netif_queue flow control actions are protected
	 * by this lock as well.
	 */
	spinlock_t lock;
};

/* Dma descriptors etc. */

#define RX_BUF_SIZE 32768
#define ETHER_HEAD_LEN      14

#define PAR0_ECP_IRQ_NBR    4

#define RX_DESC_BUF_SIZE   256
#define NBR_OF_RX_DESC     (RX_BUF_SIZE / \
			    RX_DESC_BUF_SIZE)

/* Size of slave etrax boot image */
#define ETRAX_PAR_BOOT_LENGTH 784

static etrax_dma_descr *myNextRxDesc;  /* Points to the next descriptor to
					  to be processed */
static etrax_dma_descr *myLastRxDesc;  /* The last processed descriptor */
static etrax_dma_descr *myPrevRxDesc;  /* The descriptor right before myNextRxDesc */

static unsigned char RxBuf[RX_BUF_SIZE];

static etrax_dma_descr RxDescList[NBR_OF_RX_DESC] __attribute__ ((aligned(4)));
static etrax_dma_descr TxDescList[3] __attribute__ ((aligned(4)));
                       /* host command, data, bogus ECP command */

static struct sk_buff *tx_skb;

/* Index to functions, as function prototypes. */

static int etrax_ethernet_lpslave_init(struct net_device *dev);

static int e100_open(struct net_device *dev);
static int e100_set_mac_address(struct net_device *dev, void *addr);
static int e100_send_packet(struct sk_buff *skb, struct net_device *dev);
static void e100rx_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void e100tx_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void ecp_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void e100_rx(struct net_device *dev);
static int e100_close(struct net_device *dev);
static struct net_device_stats *e100_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static void e100_hardware_send_packet(unsigned long hostcmd, char *buf, int length);
static void update_rx_stats(struct net_device_stats *);
static void update_tx_stats(struct net_device_stats *);
static void e100_reset_tranceiver(void);

static void boot_slave(unsigned char *code);

#ifdef ETHDEBUG
static void dump_parport_status(void);
#endif

#define tx_done(dev) (*R_DMA_CH0_CMD == 0)

static unsigned long host_command;
extern unsigned char e100lpslaveprog;

/*
 * This driver uses PAR0 to recevice data from slave ETRAX and PAR1 to boot
 * and send data to slave ETRAX.
 * Used ETRAX100 DMAchannels with corresponding IRQ:
 * PAR0 RX : DMA3 - IRQ 19
 * PAR1 TX : DMA4 - IRQ 20
 * IRQ 4 is used to detect ECP commands from slave ETRAX
 *
 * NOTE! PAR0 and PAR1 shares DMA and IRQ numbers with SER2 and SER3 
 */


/*
 * Check for a network adaptor of this type, and return '0' if one exists.
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 */
static int __init
etrax_ethernet_lpslave_init(struct net_device *dev)
{
	int i;
	int anOffset = 0;

	printk("Etrax/100 lpslave ethernet driver v0.3, (c) 1999 Axis Communications AB\n");

	dev->base_addr = 2;

	printk("%s initialized\n", dev->name);

	/* make Linux aware of the new hardware  */

	if (!dev) {
		printk(KERN_WARNING "%s: dev == NULL. Should this happen?\n",
                       cardname);
		dev = init_etherdev(dev, sizeof(struct net_local));
		if (!dev)
			panic("init_etherdev failed\n");
	}

	/* setup generic handlers and stuff in the dev struct */

	ether_setup(dev);

	/* make room for the local structure containing stats etc */

	dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct net_local));

	/* now setup our etrax specific stuff */

	dev->irq = DMA3_RX_IRQ_NBR; /* we really use DMATX as well... */
        dev->dma = PAR0_RX_DMA_NBR;

	/* fill in our handlers so the network layer can talk to us in the future */

	dev->open               = e100_open;
	dev->hard_start_xmit    = e100_send_packet;
	dev->stop               = e100_close;
	dev->get_stats          = e100_get_stats;
	dev->set_multicast_list = set_multicast_list;
	dev->set_mac_address    = e100_set_mac_address;

	/* Initialise the list of Etrax DMA-descriptors */

	/* Initialise receive descriptors */

	for(i = 0; i < (NBR_OF_RX_DESC - 1); i++) {
		RxDescList[i].ctrl   = 0;
		RxDescList[i].sw_len = RX_DESC_BUF_SIZE;
		RxDescList[i].next   = virt_to_phys(&RxDescList[i + 1]);
		RxDescList[i].buf    = virt_to_phys(RxBuf + anOffset);
		RxDescList[i].status = 0;
		RxDescList[i].hw_len = 0;
		anOffset += RX_DESC_BUF_SIZE;
	}

	RxDescList[i].ctrl   = d_eol;
	RxDescList[i].sw_len = RX_DESC_BUF_SIZE;
	RxDescList[i].next   = virt_to_phys(&RxDescList[0]);
	RxDescList[i].buf    = virt_to_phys(RxBuf + anOffset);
	RxDescList[i].status = 0;
	RxDescList[i].hw_len = 0;

	/* Initialise initial pointers */

	myNextRxDesc = &RxDescList[0];
	myLastRxDesc = &RxDescList[NBR_OF_RX_DESC - 1];
	myPrevRxDesc = &RxDescList[NBR_OF_RX_DESC - 1];

        /* setup some TX descriptor data */

	TxDescList[0].sw_len = 4;
	TxDescList[0].ctrl = 0;
	TxDescList[0].buf = virt_to_phys(&host_command);
	TxDescList[0].next = virt_to_phys(&TxDescList[1]);

	return 0;
}

/* set MAC address of the interface. called from the core after a
 * SIOCSIFADDR ioctl, and from the bootup above.
 */

static int
e100_set_mac_address(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	int i;

	/* remember it */

        memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	/* Write it to the hardware.
	 * Note the way the address is wrapped:
	 * *R_NETWORK_SA_0 = a0_0 | (a0_1 << 8) | (a0_2 << 16) | (a0_3 << 24);
	 * *R_NETWORK_SA_1 = a0_4 | (a0_5 << 8);
	 */

        tx_skb = 0;
	e100_hardware_send_packet(HOST_CMD_SETMAC, dev->dev_addr, 6);

	/* show it in the log as well */

	printk("%s: changed MAC to ", dev->name);

	for (i = 0; i < 5; i++)
		printk("%02X:", dev->dev_addr[i]);

	printk("%02X\n", dev->dev_addr[i]);

	return 0;
}

/*
 * Open/initialize the board. This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 */

static int
e100_open(struct net_device *dev)
{
	unsigned long flags;

	/* configure the PAR0 (RX) and PAR1 (TX) ports
	 *
	 * perror is nAckReverse, which must be 1 at the TX side,
         * and 0 at the RX side
         *
	 * select is XFlag, which must be 1 at both sides
	 */
#ifdef ETHDEBUG        
        printk("Setting up PAR ports\n");
#endif
        *R_PAR0_CONFIG =
          /* We do not have an external buffer, don't care */
		IO_STATE(R_PAR0_CONFIG, ioe,     noninv)    |
          /* Not connected, don't care */
		IO_STATE(R_PAR0_CONFIG, iseli,   noninv)    |
          /* iautofd is not inverted, noninv */
		IO_STATE(R_PAR0_CONFIG, iautofd, noninv)    |
          /* Not used in reverse direction, don't care */
		IO_STATE(R_PAR0_CONFIG, istrb,   noninv)    |
          /* Not connected, don't care */
		IO_STATE(R_PAR0_CONFIG, iinit,   noninv)    |
          /* perror is GND and reverse wants 0, noninv */
		IO_STATE(R_PAR0_CONFIG, iperr,   noninv)    |
          /* ack is not inverted, noninv */
		IO_STATE(R_PAR0_CONFIG, iack,    noninv)    |
          /* busy is not inverted, noninv */
		IO_STATE(R_PAR0_CONFIG, ibusy,   noninv)    |
          /* fault is not inverted, noninv */
		IO_STATE(R_PAR0_CONFIG, ifault,  noninv)    |
          /* select is Vcc and we want 1, noninv */
		IO_STATE(R_PAR0_CONFIG, isel,    noninv)    |
          /* We will run dma, enable */
		IO_STATE(R_PAR0_CONFIG, dma, enable)        |
          /* No run length encoding, disable */
		IO_STATE(R_PAR0_CONFIG, rle_in, disable)    |
          /* No run length encoding, disable */
		IO_STATE(R_PAR0_CONFIG, rle_out, disable)   |
          /* Enable parallel port */
		IO_STATE(R_PAR0_CONFIG, enable, on)         |
          /* Force mode regardless of pin status */
		IO_STATE(R_PAR0_CONFIG, force, on)          |
          /* We want ECP forward mode since PAR0 is RX */
		IO_STATE(R_PAR0_CONFIG, mode, ecp_rev);       

        *R_PAR1_CONFIG =
          /* We do not have an external buffer, don't care */
		IO_STATE(R_PAR1_CONFIG, ioe,     noninv)    |
          
          /* Not connected, don't care */
		IO_STATE(R_PAR1_CONFIG, iseli,   noninv)    |
          
          /* HostAck must indicate data cycle, noninv */
		IO_STATE(R_PAR1_CONFIG, iautofd, noninv)    |
          
          /* HostClk has no external inverter, noninv */
		IO_STATE(R_PAR1_CONFIG, istrb,   noninv)    |
          
          /* Not connected, don't care */
		IO_STATE(R_PAR1_CONFIG, iinit,   noninv)    |
          
          /* nAckReverse must be 1 in forward mode but is grounded, inv */ 
		IO_STATE(R_PAR1_CONFIG, iperr,   inv)       |
          
          /* PeriphClk must be 1 in forward mode, noninv */
		IO_STATE(R_PAR1_CONFIG, iack,    noninv)    |
          
          /* PeriphAck has no external inverter, noninv */
		IO_STATE(R_PAR1_CONFIG, ibusy,   noninv)    |
          
          /* nPerihpRequest has no external inverter, noniv */
		IO_STATE(R_PAR1_CONFIG, ifault,  noninv)    |
          
          /* Select is VCC and we want 1, noninv */
		IO_STATE(R_PAR1_CONFIG, isel,    noninv)    |
          
          /* No EPP mode, disable */
                IO_STATE(R_PAR1_CONFIG, ext_mode, disable)  |
          
          /* We will run dma, enable */
                IO_STATE(R_PAR1_CONFIG, dma, enable)        |
          
          /* No run length encoding, disable */
		IO_STATE(R_PAR1_CONFIG, rle_in, disable)    |
          
          /* No run length encoding, disable */
		IO_STATE(R_PAR1_CONFIG, rle_out, disable)   |
          
          /* Enable parallel port */
		IO_STATE(R_PAR1_CONFIG, enable, on)         |
          
          /* Force mode regardless of pin status */
		IO_STATE(R_PAR1_CONFIG, force, on)          |
          
          /* We want ECP forward mode since PAR1 is TX */
	 	IO_STATE(R_PAR1_CONFIG, mode, ecp_fwd);        

        /* Setup time of value * 160 + 20 ns == 20 ns below */
        *R_PAR1_DELAY = IO_FIELD(R_PAR1_DELAY, setup, 0);  

        *R_PAR1_CTRL = 0;

        while ((((*R_PAR1_STATUS)&0xE000) >> 13) != 5); /* Wait for ECP_FWD mode */
#ifdef ETHDEBUG
        dump_parport_status();
#endif
        
        /* make sure ECP irq is acked when we enable it below */

	(void)*R_PAR0_STATUS_DATA;
	(void)*R_PAR1_STATUS_DATA;

	/* Reset and wait for the DMA channels */

        RESET_DMA(4); /* PAR1_TX_DMA_NBR */
	RESET_DMA(3); /* PAR0_RX_DMA_NBR */
	WAIT_DMA(4);  
	WAIT_DMA(3);
        
        /* boot the slave Etrax, by sending code on PAR1.
	 * do this before we start up the IRQ handlers and stuff,
	 * beacuse we simply poll for completion in boot_slave.
	 */
        
	boot_slave(&e100lpslaveprog);

	/* allocate the irq corresponding to the receiving DMA */

	if (request_irq(DMA3_RX_IRQ_NBR, e100rx_interrupt, 0,
			cardname, (void *)dev)) {
          printk("Failed to allocate DMA3_RX_IRQ_NBR\n");
		goto grace_exit;
	}

	/* allocate the irq corresponding to the transmitting DMA */

	if (request_irq(DMA4_TX_IRQ_NBR, e100tx_interrupt, 0,
			cardname, (void *)dev)) {
          printk("Failed to allocate DMA4_TX_IRQ_NBR\n");
          goto grace_exit;
	}
        
        /* allocate the irq used for detecting ECP commands on the RX port (PAR0) */

	if (request_irq(PAR0_ECP_IRQ_NBR, ecp_interrupt, 0,
			cardname, (void *)dev)) {
          printk("Failed to allocate PAR0_ECP_IRQ_NBR\n");
          grace_exit:          
                free_irq(PAR0_ECP_IRQ_NBR, (void *)dev);
                free_irq(DMA4_TX_IRQ_NBR, (void *)dev);
		free_irq(DMA3_RX_IRQ_NBR, (void *)dev);
                
		return -EAGAIN;
	}

#if 0
        /* We are not allocating DMA since DMA4 is reserved for 'cascading'
         * and will always fail with the current dma.c
         */
        
	/*
	 * Always allocate the DMA channels after the IRQ,
	 * and clean up on failure.
	 */

	if(request_dma(PAR0_RX_DMA_NBR, cardname)) {
          printk("Failed to allocate PAR0_RX_DMA_NBR\n");
		goto grace_exit;
	}

	if(request_dma(PAR1_TX_DMA_NBR, cardname)) {
          printk("Failed to allocate PAR1_TX_DMA_NBR\n");
	grace_exit:
		/* this will cause some 'trying to free free irq' but what the heck... */

		free_dma(PAR1_TX_DMA_NBR);
                free_dma(PAR0_RX_DMA_NBR);
                free_irq(PAR0_ECP_IRQ_NBR, (void *)dev);
                free_irq(DMA4_TX_IRQ_NBR, (void *)dev);
		free_irq(DMA3_RX_IRQ_NBR, (void *)dev);
		
		return -EAGAIN;
	}
#endif
        
#ifdef ETHDEBUG
        printk("Par port IRQ and DMA allocated\n");
#endif
	save_flags(flags);
	cli();

	/* enable the irq's for PAR0/1 DMA */

	*R_IRQ_MASK2_SET =
		IO_STATE(R_IRQ_MASK2_SET, dma3_eop, set) |
		IO_STATE(R_IRQ_MASK2_SET, dma4_descr, set);

        *R_IRQ_MASK0_SET =
		IO_STATE(R_IRQ_MASK0_SET, par0_ecp_cmd, set);

	tx_skb = 0;

	/* make sure the irqs are cleared */

	*R_DMA_CH3_CLR_INTR = IO_STATE(R_DMA_CH3_CLR_INTR, clr_eop, do);
	*R_DMA_CH4_CLR_INTR = IO_STATE(R_DMA_CH4_CLR_INTR, clr_descr, do);

        /* Write the MAC address to the slave HW */
	udelay(5000);
	e100_hardware_send_packet(HOST_CMD_SETMAC, dev->dev_addr, 6);
        
	/* make sure the rec and transmit error counters are cleared */

	(void)*R_REC_COUNTERS;  /* dummy read */
	(void)*R_TR_COUNTERS;   /* dummy read */

	/* start the receiving DMA channel so we can receive packets from now on */

	*R_DMA_CH3_FIRST = virt_to_phys(myNextRxDesc);
	*R_DMA_CH3_CMD = IO_STATE(R_DMA_CH3_CMD, cmd, start);

	restore_flags(flags);
	
	/* We are now ready to accept transmit requeusts from
	 * the queueing layer of the networking.
	 */
#ifdef ETHDEBUG
        printk("Starting slave network transmit queue\n");
#endif
	netif_start_queue(dev);

	return 0;
}

static void 
e100_reset_tranceiver(void)
{
  /* To do: Reboot and setup slave Etrax */
}

/* Called by upper layers if they decide it took too long to complete
 * sending a packet - we need to reset and stuff.
 */

static void
e100_tx_timeout(struct net_device *dev)
{
	struct net_local *np = (struct net_local *)dev->priv;

	printk(KERN_WARNING "%s: transmit timed out, %s?\n", dev->name,
	       tx_done(dev) ? "IRQ problem" : "network cable problem");
	
	/* remember we got an error */
	
	np->stats.tx_errors++; 
	
	/* reset the TX DMA in case it has hung on something */
	
	RESET_DMA(4);
	WAIT_DMA(4);
	
	/* Reset the tranceiver. */
	
	e100_reset_tranceiver();
	
	/* and get rid of the packet that never got an interrupt */
	
	dev_kfree_skb(tx_skb);
	tx_skb = 0;
	
	/* tell the upper layers we're ok again */
	
	netif_wake_queue(dev);
}


/* This will only be invoked if the driver is _not_ in XOFF state.
 * What this means is that we need not check it, and that this
 * invariant will hold if we make sure that the netif_*_queue()
 * calls are done at the proper times.
 */

static int
e100_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *np = (struct net_local *)dev->priv;
	int length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	unsigned char *buf = skb->data;
	
#ifdef ETHDEBUG
        unsigned char *temp_data_ptr = buf;
        int i;
        
	printk("Sending a packet of length %d:\n", length);
	/* dump the first bytes in the packet */
	for(i = 0; i < 8; i++) {
		printk("%d: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n", i * 8,
		       temp_data_ptr[0],temp_data_ptr[1],temp_data_ptr[2],
                       temp_data_ptr[3],temp_data_ptr[4],temp_data_ptr[5],
                       temp_data_ptr[6],temp_data_ptr[7]);
		temp_data_ptr += 8;
	}
#endif
	spin_lock_irq(&np->lock);  /* protect from tx_interrupt */

	tx_skb = skb; /* remember it so we can free it in the tx irq handler later */
	dev->trans_start = jiffies;
	
	e100_hardware_send_packet(HOST_CMD_SENDPACK, buf, length);

	/* this simple TX driver has only one send-descriptor so we're full
	 * directly. If this had a send-ring instead, we would only do this if
	 * the ring got full.
	 */

	netif_stop_queue(dev);
        
	spin_unlock_irq(&np->lock);

	return 0;
}

/*
 * The typical workload of the driver:
 *   Handle the network interface interrupts.
 */

static void
e100rx_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	unsigned long irqbits = *R_IRQ_MASK2_RD;
        
	if(irqbits & IO_STATE(R_IRQ_MASK2_RD, dma3_eop, active)) {

		/* acknowledge the eop interrupt */

		*R_DMA_CH3_CLR_INTR = IO_STATE(R_DMA_CH3_CLR_INTR, clr_eop, do);

		/* check if one or more complete packets were indeed received */

		while(*R_DMA_CH3_FIRST != virt_to_phys(myNextRxDesc)) {
			/* Take out the buffer and give it to the OS, then
			 * allocate a new buffer to put a packet in.
			 */
			e100_rx(dev);
			((struct net_local *)dev->priv)->stats.rx_packets++;
			/* restart/continue on the channel, for safety */
			*R_DMA_CH3_CMD = IO_STATE(R_DMA_CH3_CMD, cmd, restart);
			/* clear dma channel 3 eop/descr irq bits */
			*R_DMA_CH3_CLR_INTR =
				IO_STATE(R_DMA_CH3_CLR_INTR, clr_eop, do) |
				IO_STATE(R_DMA_CH3_CLR_INTR, clr_descr, do);
			
			/* now, we might have gotten another packet
			   so we have to loop back and check if so */
		}
	}
}

/* the transmit dma channel interrupt
 *
 * this is supposed to free the skbuff which was pending during transmission,
 * and inform the kernel that we can send one more buffer
 */

static void
e100tx_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	unsigned long irqbits = *R_IRQ_MASK2_RD;
	struct net_local *np = (struct net_local *)dev->priv;

#ifdef ETHDEBUG
        printk("We got tx interrupt\n");
#endif
	/* check for a dma4_eop interrupt */
	if(irqbits & IO_STATE(R_IRQ_MASK2_RD, dma4_descr, active)) {
		/* This protects us from concurrent execution of
		 * our dev->hard_start_xmit function above.
		 */

		spin_lock(&np->lock);
		
		/* acknowledge the eop interrupt */
                
		*R_DMA_CH4_CLR_INTR = IO_STATE(R_DMA_CH4_CLR_INTR, clr_descr, do);

                /* skip *R_DMA_CH4_FIRST == 0 test since we use d_wait... */
		if(tx_skb) {

			np->stats.tx_bytes += tx_skb->len;
			np->stats.tx_packets++;
			/* dma is ready with the transmission of the data in tx_skb, so now we can release the skb memory */
			dev_kfree_skb_irq(tx_skb);
			tx_skb = 0;
			netif_wake_queue(dev);
		} else {
			printk(KERN_WARNING "%s: tx weird interrupt\n",
                               cardname);
		}

		spin_unlock(&np->lock);
	}
}

static void
ecp_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct net_local *lp = (struct net_local *)dev->priv;
	unsigned long temp, irqbits = *R_IRQ_MASK0_RD;

        /* check for ecp irq */
	if(irqbits & IO_MASK(R_IRQ_MASK0_RD, par0_ecp_cmd)) { 
		/* acknowledge by reading the bit */
		temp = *R_PAR0_STATUS_DATA;
		/* force an EOP on the incoming channel, so we'll get an rx interrupt */
		*R_SET_EOP = IO_STATE(R_SET_EOP, ch3_eop, set);
	}
}

/* We have a good packet(s), get it/them out of the buffers. */
static void
e100_rx(struct net_device *dev)
{
	struct sk_buff *skb;
	int length=0;
	int i;
	struct net_local *np = (struct net_local *)dev->priv;
	struct etrax_dma_descr *mySaveRxDesc = myNextRxDesc;
	unsigned char *skb_data_ptr;

	/* If the packet is broken down in many small packages then merge
	 * count how much space we will need to alloc with skb_alloc() for
	 * it to fit.
	 */

	while (!(myNextRxDesc->status & d_eop)) {
		length += myNextRxDesc->sw_len; /* use sw_len for the first descs */
		myNextRxDesc->status = 0;
		myNextRxDesc = phys_to_virt(myNextRxDesc->next);
	}

	length += myNextRxDesc->hw_len; /* use hw_len for the last descr */

#ifdef ETHDEBUG
	printk("Got a packet of length %d:\n", length);
	/* dump the first bytes in the packet */
	skb_data_ptr = (unsigned char *)phys_to_virt(mySaveRxDesc->buf);
	for(i = 0; i < 8; i++) {
		printk("%d: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n", i * 8,
		       skb_data_ptr[0],skb_data_ptr[1],skb_data_ptr[2],skb_data_ptr[3],
		       skb_data_ptr[4],skb_data_ptr[5],skb_data_ptr[6],skb_data_ptr[7]);
		skb_data_ptr += 8;
	}
#endif

	skb = dev_alloc_skb(length - ETHER_HEAD_LEN);
	if (!skb) {
		np->stats.rx_errors++;
		printk(KERN_NOTICE "%s: Memory squeeze, dropping packet.\n",
		       dev->name);
		return;
	}

	skb_put(skb, length - ETHER_HEAD_LEN);        /* allocate room for the packet body */
	skb_data_ptr = skb_push(skb, ETHER_HEAD_LEN); /* allocate room for the header */

#ifdef ETHDEBUG
	printk("head = 0x%x, data = 0x%x, tail = 0x%x, end = 0x%x\n",
	       skb->head, skb->data, skb->tail, skb->end);
	printk("copying packet to 0x%x.\n", skb_data_ptr);
#endif

	/* this loop can be made using max two memcpy's if optimized */

	while(mySaveRxDesc != myNextRxDesc) {
		memcpy(skb_data_ptr, phys_to_virt(mySaveRxDesc->buf),
		       mySaveRxDesc->sw_len);
		skb_data_ptr += mySaveRxDesc->sw_len;
		mySaveRxDesc = phys_to_virt(mySaveRxDesc->next);
	}

	memcpy(skb_data_ptr, phys_to_virt(mySaveRxDesc->buf),
	       mySaveRxDesc->hw_len);

	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);

	/* Send the packet to the upper layers */

	netif_rx(skb);

	/* Prepare for next packet */

	myNextRxDesc->status = 0;
	myPrevRxDesc = myNextRxDesc;
	myNextRxDesc = phys_to_virt(myNextRxDesc->next);

	myPrevRxDesc->ctrl |= d_eol;
	myLastRxDesc->ctrl &= ~d_eol;
	myLastRxDesc = myPrevRxDesc;

	return;
}

/* The inverse routine to net_open(). */
static int
e100_close(struct net_device *dev)
{
	struct net_local *np = (struct net_local *)dev->priv;

	printk("Closing %s.\n", dev->name);

	netif_stop_queue(dev);

	*R_IRQ_MASK0_CLR = IO_STATE(R_IRQ_MASK0_CLR, par0_ecp_cmd, clr);

	*R_IRQ_MASK2_CLR =
		IO_STATE(R_IRQ_MASK2_CLR, dma3_eop, clr) |
		IO_STATE(R_IRQ_MASK2_CLR, dma4_descr, clr);	

	/* Stop the receiver and the transmitter */

	RESET_DMA(3);
	RESET_DMA(4);

	/* Flush the Tx and disable Rx here. */

	free_irq(DMA3_RX_IRQ_NBR, (void *)dev);
	free_irq(DMA4_TX_IRQ_NBR, (void *)dev);
	free_irq(PAR0_ECP_IRQ_NBR, (void *)dev);

	free_dma(PAR1_TX_DMA_NBR);
	free_dma(PAR0_RX_DMA_NBR);

	/* Update the statistics here. */

	update_rx_stats(&np->stats);
	update_tx_stats(&np->stats);

	return 0;
}

static void
update_rx_stats(struct net_device_stats *es)
{
	unsigned long r = *R_REC_COUNTERS;
	/* update stats relevant to reception errors */
	es->rx_fifo_errors += r >> 24;            /* fifo overrun */
	es->rx_crc_errors += r & 0xff;            /* crc error */
	es->rx_frame_errors += (r >> 8) & 0xff;   /* alignment error */
	es->rx_length_errors += (r >> 16) & 0xff; /* oversized frames */
}

static void
update_tx_stats(struct net_device_stats *es)
{
	unsigned long r = *R_TR_COUNTERS;
	/* update stats relevant to transmission errors */
	es->collisions += (r & 0xff) + ((r >> 8) & 0xff); /* single_col + multiple_col */
	es->tx_errors += (r >> 24) & 0xff; /* deferred transmit frames */
}

/*
 * Get the current statistics.
 * This may be called with the card open or closed.
 */
static struct net_device_stats *
e100_get_stats(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	update_rx_stats(&lp->stats);
	update_tx_stats(&lp->stats);

	return &lp->stats;
}

/*
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1	Promiscuous mode, receive all packets
 * num_addrs == 0	Normal mode, clear multicast list
 * num_addrs > 0	Multicast mode, receive normal and MC packets,
 *			and do best-effort filtering.
 */
static void
set_multicast_list(struct net_device *dev)
{
  /* To do */
}

void
e100_hardware_send_packet(unsigned long hostcmd, char *buf, int length)
{
  static char bogus_ecp[] = { 42, 42 };
  int i;
  

#ifdef ETHDEBUG
	printk("e100 send pack, buf 0x%x len %d\n", buf, length);
#endif
        
        host_command = hostcmd;

	/* Configure the tx dma descriptor. Desc 0 is already configured.*/

        TxDescList[1].sw_len = length;
	/* bug workaround - etrax100 needs d_wait on the descriptor _before_
	 * a descriptor containing an ECP command
	 */
	TxDescList[1].ctrl = d_wait;
	TxDescList[1].buf = virt_to_phys(buf);
	TxDescList[1].next = virt_to_phys(&TxDescList[2]);

        /* append the ecp dummy descriptor - its only purpose is to 
	 * make the receiver generate an irq due to the ecp command
	 * so the receiver knows where packets end
	 */

	TxDescList[2].sw_len = 1;
	TxDescList[2].ctrl = d_ecp | d_eol | d_int;
	TxDescList[2].buf = virt_to_phys(bogus_ecp);
        

	/* setup the dma channel and start it */

        *R_DMA_CH4_FIRST = virt_to_phys(TxDescList);
	*R_DMA_CH4_CMD = IO_STATE(R_DMA_CH4_CMD, cmd, start);
        
#ifdef ETHDEBUG         
         printk("done\n");
#endif
}

/* send a chunk of code to the slave chip to boot it. */

static void
boot_slave(unsigned char *code)
{
  int i;

#ifdef ETHDEBUG
	printk("  booting slave ETRAX...\n");
#endif
        *R_PORT_PB_DATA = 0x7F; /* Reset slave */
        udelay(15); /* Time enough to reset WAN tranciever */
        *R_PORT_PB_DATA = 0xFF; /* Reset slave */

	/* configure the tx dma data descriptor */

	TxDescList[1].sw_len = ETRAX_PAR_BOOT_LENGTH;
	TxDescList[1].ctrl = d_eol | d_int; 
                                                      
	TxDescList[1].buf = virt_to_phys(code);
	TxDescList[1].next = 0;
        
        /* setup the dma channel and start it */
 	*R_DMA_CH4_FIRST = virt_to_phys(&TxDescList[1]);
	*R_DMA_CH4_CMD = IO_STATE(R_DMA_CH4_CMD, cmd, start);

	/* wait for completion */
	while(!(*R_IRQ_READ2 & IO_MASK(R_IRQ_READ2, dma4_descr)));

	/* ack the irq */

	*R_DMA_CH4_CLR_INTR = IO_STATE(R_DMA_CH4_CLR_INTR, clr_descr, do);

#if 0
        /* manual transfer of boot code - requires dma turned off */
        for (i=0; i<ETRAX_PAR_BOOT_LENGTH; i++)
        {
          printk("  sending byte: %u value: %x\n",i,code[i]);
          while (((*R_PAR1_STATUS)&0x02) == 0); /* Wait while tr_rdy is busy*/
          *R_PAR1_CTRL_DATA = code[i];
        }
#endif

#ifdef ETHDEBUG
	printk("  done\n");
#endif
}

#ifdef ETHDEBUG
/* debug code to check the current status of PAR1 */
static void
dump_parport_status(void)
{
  unsigned long temp;
  
  printk("Parport1 status:\n");
  
  temp = (*R_PAR1_STATUS)&0xE000;
  temp = temp >> 13;
  printk("Reg mode: %u (ecp_fwd(5), ecp_rev(6))\n", temp);
  
  temp = (*R_PAR1_STATUS)&0x1000;
  temp = temp >> 12;
  printk("Reg perr: %u (ecp_rev(0))\n", temp);
  
  temp = (*R_PAR1_STATUS)&0x0800;
  temp = temp >> 11;
  printk("Reg ack: %u (inactive (1), active (0))\n", temp);
  
  temp = (*R_PAR1_STATUS)&0x0400;
  temp = temp >> 10;
  printk("Reg busy: %u (inactive (0), active (1))\n", temp);
  
  temp = (*R_PAR1_STATUS)&0x0200;
  temp = temp >> 9;
  printk("Reg fault: %u (inactive (1), active (0))\n", temp);
  
  temp = (*R_PAR1_STATUS)&0x0100;
  temp = temp >> 8;
  printk("Reg sel: %u (inactive (0), active (1), xflag(1))\n", temp);

  temp = (*R_PAR1_STATUS)&0x02;
  temp = temp >> 1;
  printk("Reg tr_rdy: %u (busy (0), ready (1))\n", temp);

}
#endif /* ETHDEBUG */

static struct net_device dev_etrax_slave_ethernet;

static int
etrax_init_module(void)
{
	struct net_device *d = &dev_etrax_slave_ethernet;

	d->init = etrax_ethernet_lpslave_init;

	if(register_netdev(d) == 0)
		return 0;
	else
		return -ENODEV;
}

module_init(etrax_init_module);
