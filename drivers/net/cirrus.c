
/*
 * linux/drivers/net/cirrus.c
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * A Cirrus Logic CS8900A driver for Linux
 * based on the cs89x0 driver written by Russell Nelson,
 * Donald Becker, and others.
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

/*
 * At the moment the driver does not support memory mode operation.
 * It is trivial to implement this, but not worth the effort.
 */

/*
 * TODO:
 *
 *   1. If !ready in send_start(), queue buffer and send it in interrupt handler
 *      when we receive a BufEvent with Rdy4Tx, send it again. dangerous!
 *   2. how do we prevent interrupt handler destroying integrity of get_stats()?
 *   3. Change reset code to check status.
 *   4. Implement set_mac_address and remove fake mac address
 *   5. Link status detection stuff
 *   6. Write utility to write EEPROM, do self testing, etc.
 *   7. Implement DMA routines (I need a board w/ DMA support for that)
 *   8. Power management
 *   9. Add support for multiple ethernet chips
 *  10. Add support for other cs89xx chips (need hardware for that)
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "cirrus.h"

/* #define DEBUG */
/* #define FULL_DUPLEX */

#ifdef CONFIG_SA1100_FRODO
#	define CIRRUS_DEFAULT_IO FRODO_ETH_IO + 0x300
#	define CIRRUS_DEFAULT_IRQ FRODO_ETH_IRQ
#elif CONFIG_SA1100_CERF
#	define CIRRUS_DEFAULT_IO CERF_ETH_IO + 0x300
#	define CIRRUS_DEFAULT_IRQ CERF_ETH_IRQ
#elif CONFIG_ARCH_CDB89712
#	define CIRRUS_DEFAULT_IO ETHER_BASE + 0x300
#	define CIRRUS_DEFAULT_IRQ IRQ_EINT3
#else
#	define CIRRUS_DEFAULT_IO	0
#	define CIRRUS_DEFAULT_IRQ	0
#endif	/* #ifdef CONFIG_SA1100_CERF */

typedef struct {
	struct net_device_stats stats;
	u16 txlen;
} cirrus_t;

typedef struct {
	u16 io_base;		/* I/O Base Address			*/
	u16 irq;			/* Interrupt Number			*/
	u16 dma;			/* DMA Channel Numbers		*/
	u32 mem_base;		/* Memory Base Address		*/
	u32 rom_base;		/* Boot PROM Base Address	*/
	u32 rom_mask;		/* Boot PROM Address Mask	*/
	u8 mac[6];			/* Individual Address		*/
} cirrus_eeprom_t;

/*
 * I/O routines
 */

static inline u16 cirrus_read (struct net_device *dev,u16 reg)
{
	outw (reg,dev->base_addr + PP_Address);
	return (inw (dev->base_addr + PP_Data));
}

static inline void cirrus_write (struct net_device *dev,u16 reg,u16 value)
{
	outw (reg,dev->base_addr + PP_Address);
	outw (value,dev->base_addr + PP_Data);
}

static inline void cirrus_set (struct net_device *dev,u16 reg,u16 value)
{
	cirrus_write (dev,reg,cirrus_read (dev,reg) | value);
}

static inline void cirrus_clear (struct net_device *dev,u16 reg,u16 value)
{
	cirrus_write (dev,reg,cirrus_read (dev,reg) & ~value);
}

static inline void cirrus_frame_read (struct net_device *dev,struct sk_buff *skb,u16 length)
{
	insw (dev->base_addr,skb_put (skb,length),(length + 1) / 2);
}

static inline void cirrus_frame_write (struct net_device *dev,struct sk_buff *skb)
{
	outsw (dev->base_addr,skb->data,(skb->len + 1) / 2);
}

/*
 * Debugging functions
 */

#ifdef DEBUG
static inline int printable (int c)
{
	return ((c >= 32 && c <= 126) ||
			(c >= 174 && c <= 223) ||
			(c >= 242 && c <= 243) ||
			(c >= 252 && c <= 253));
}

static void dump16 (struct net_device *dev,const u8 *s,size_t len)
{
	int i;
	char str[128];

	if (!len) return;

	*str = '\0';

	for (i = 0; i < len; i++) {
		if (i && !(i % 4)) strcat (str," ");
		sprintf (str,"%s%.2x ",str,s[i]);
	}

	for ( ; i < 16; i++) {
		if (i && !(i % 4)) strcat (str," ");
		strcat (str,"   ");
	}

	strcat (str," ");
	for (i = 0; i < len; i++) sprintf (str,"%s%c",str,printable (s[i]) ? s[i] : '.');

	printk (KERN_DEBUG "%s:     %s\n",dev->name,str);
}

static void hexdump (struct net_device *dev,const void *ptr,size_t size)
{
	const u8 *s = (u8 *) ptr;
	int i;
	for (i = 0; i < size / 16; i++, s += 16) dump16 (dev,s,16);
	dump16 (dev,s,size % 16);
}

static void dump_packet (struct net_device *dev,struct sk_buff *skb,const char *type)
{
	printk (KERN_INFO "%s: %s %d byte frame %.2x:%.2x:%.2x:%.2x:%.2x:%.2x to %.2x:%.2x:%.2x:%.2x:%.2x:%.2x type %.4x\n",
			dev->name,
			type,
			skb->len,
			skb->data[0],skb->data[1],skb->data[2],skb->data[3],skb->data[4],skb->data[5],
			skb->data[6],skb->data[7],skb->data[8],skb->data[9],skb->data[10],skb->data[11],
			(skb->data[12] << 8) | skb->data[13]);
	if (skb->len < 0x100) hexdump (dev,skb->data,skb->len);
}
#endif	/* #ifdef DEBUG */

/*
 * Driver functions
 */

static void cirrus_receive (struct net_device *dev)
{
	cirrus_t *priv = (cirrus_t *) dev->priv;
	struct sk_buff *skb;
	u16 status,length;

	status = cirrus_read (dev,PP_RxStatus);
	length = cirrus_read (dev,PP_RxLength);

	if (!(status & RxOK)) {
		priv->stats.rx_errors++;
		if ((status & (Runt | Extradata))) priv->stats.rx_length_errors++;
		if ((status & CRCerror)) priv->stats.rx_crc_errors++;
		return;
	}

	if ((skb = dev_alloc_skb (length + 4)) == NULL) {
		priv->stats.rx_dropped++;
		return;
	}

	skb->dev = dev;
	skb_reserve (skb,2);

	cirrus_frame_read (dev,skb,length);

#ifdef DEBUG
	dump_packet (dev,skb,"recv");
#endif	/* #ifdef DEBUG */

	skb->protocol = eth_type_trans (skb,dev);

	netif_rx (skb);
	dev->last_rx = jiffies;

	priv->stats.rx_packets++;
	priv->stats.rx_bytes += length;
}

static int cirrus_send_start (struct sk_buff *skb,struct net_device *dev)
{
	cirrus_t *priv = (cirrus_t *) dev->priv;
	u16 status;

	netif_stop_queue (dev);

	cirrus_write (dev,PP_TxCMD,TxStart (After5));
	cirrus_write (dev,PP_TxLength,skb->len);

	status = cirrus_read (dev,PP_BusST);

	if ((status & TxBidErr)) {
		printk (KERN_WARNING "%s: Invalid frame size %d!\n",dev->name,skb->len);
		priv->stats.tx_errors++;
		priv->stats.tx_aborted_errors++;
		priv->txlen = 0;
		return (1);
	}

	if (!(status & Rdy4TxNOW)) {
		printk (KERN_WARNING "%s: Transmit buffer not free!\n",dev->name);
		priv->stats.tx_errors++;
		priv->txlen = 0;
		/* FIXME: store skb and send it in interrupt handler */
		return (1);
	}

	cirrus_frame_write (dev,skb);

#ifdef DEBUG
	dump_packet (dev,skb,"send");
#endif	/* #ifdef DEBUG */

	dev->trans_start = jiffies;

	dev_kfree_skb (skb);

	priv->txlen = skb->len;

	return (0);
}

static void cirrus_interrupt (int irq,void *id,struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) id;
	cirrus_t *priv;
	u16 status;

	if (dev->priv == NULL) {
		printk (KERN_WARNING "%s: irq %d for unknown device.\n",dev->name,irq);
		return;
	}

	priv = (cirrus_t *) dev->priv;

	while ((status = cirrus_read (dev,PP_ISQ))) {
		switch (RegNum (status)) {
		case RxEvent:
			cirrus_receive (dev);
			break;

		case TxEvent:
			priv->stats.collisions += ColCount (cirrus_read (dev,PP_TxCOL));
			if (!(RegContent (status) & TxOK)) {
				priv->stats.tx_errors++;
				if ((RegContent (status) & Out_of_window)) priv->stats.tx_window_errors++;
				if ((RegContent (status) & Jabber)) priv->stats.tx_aborted_errors++;
				break;
			} else if (priv->txlen) {
				priv->stats.tx_packets++;
				priv->stats.tx_bytes += priv->txlen;
			}
			priv->txlen = 0;
			netif_wake_queue (dev);
			break;

		case BufEvent:
			if ((RegContent (status) & RxMiss)) {
				u16 missed = MissCount (cirrus_read (dev,PP_RxMISS));
				priv->stats.rx_errors += missed;
				priv->stats.rx_missed_errors += missed;
			}
			if ((RegContent (status) & TxUnderrun)) {
				priv->stats.tx_errors++;
				priv->stats.tx_fifo_errors++;
			}
			/* FIXME: if Rdy4Tx, transmit last sent packet (if any) */
			priv->txlen = 0;
			netif_wake_queue (dev);
			break;

		case TxCOL:
			priv->stats.collisions += ColCount (cirrus_read (dev,PP_TxCOL));
			break;

		case RxMISS:
			status = MissCount (cirrus_read (dev,PP_RxMISS));
			priv->stats.rx_errors += status;
			priv->stats.rx_missed_errors += status;
			break;
		}
	}
}

static void cirrus_transmit_timeout (struct net_device *dev)
{
	cirrus_t *priv = (cirrus_t *) dev->priv;
	priv->stats.tx_errors++;
	priv->stats.tx_heartbeat_errors++;
	priv->txlen = 0;
	netif_wake_queue (dev);
}

static int cirrus_start (struct net_device *dev)
{
	int result;

	/* valid ethernet address? */
	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk(KERN_ERR "%s: invalid ethernet MAC address\n",dev->name);
		return (-EINVAL);
	}

	/* install interrupt handler */
	if ((result = request_irq (dev->irq,&cirrus_interrupt,0,dev->name,dev)) < 0) {
		printk (KERN_ERR "%s: could not register interrupt %d\n",dev->name,dev->irq);
		return (result);
	}

	/* enable the ethernet controller */
	cirrus_set (dev,PP_RxCFG,RxOKiE | BufferCRC | CRCerroriE | RuntiE | ExtradataiE);
	cirrus_set (dev,PP_RxCTL,RxOKA | IndividualA | BroadcastA);
	cirrus_set (dev,PP_TxCFG,TxOKiE | Out_of_windowiE | JabberiE);
	cirrus_set (dev,PP_BufCFG,Rdy4TxiE | RxMissiE | TxUnderruniE | TxColOvfiE | MissOvfloiE);
	cirrus_set (dev,PP_LineCTL,SerRxON | SerTxON);
	cirrus_set (dev,PP_BusCTL,EnableRQ);

#ifdef FULL_DUPLEX
	cirrus_set (dev,PP_TestCTL,FDX);
#endif	/* #ifdef FULL_DUPLEX */

	/* start the queue */
	netif_start_queue (dev);

	MOD_INC_USE_COUNT;

	return (0);
}

static int cirrus_stop (struct net_device *dev)
{
	/* disable ethernet controller */
	cirrus_write (dev,PP_BusCTL,0);
	cirrus_write (dev,PP_TestCTL,0);
	cirrus_write (dev,PP_SelfCTL,0);
	cirrus_write (dev,PP_LineCTL,0);
	cirrus_write (dev,PP_BufCFG,0);
	cirrus_write (dev,PP_TxCFG,0);
	cirrus_write (dev,PP_RxCTL,0);
	cirrus_write (dev,PP_RxCFG,0);

	/* uninstall interrupt handler */
	free_irq (dev->irq,dev);

	/* stop the queue */
	netif_stop_queue (dev);

	MOD_DEC_USE_COUNT;

	return (0);
}

static int cirrus_set_mac_address (struct net_device *dev, void *p)
{
	struct sockaddr *addr = (struct sockaddr *)p;
	int i;

	if (netif_running(dev))
		return -EBUSY;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	/* configure MAC address */
	for (i = 0; i < ETH_ALEN; i += 2)
		cirrus_write (dev,PP_IA + i,dev->dev_addr[i] | (dev->dev_addr[i + 1] << 8));

	return 0;
}

static struct net_device_stats *cirrus_get_stats (struct net_device *dev)
{
	cirrus_t *priv = (cirrus_t *) dev->priv;
	return (&priv->stats);
}

static void cirrus_set_receive_mode (struct net_device *dev)
{
	if ((dev->flags & IFF_PROMISC))
		cirrus_set (dev,PP_RxCTL,PromiscuousA);
	else
		cirrus_clear (dev,PP_RxCTL,PromiscuousA);

	if ((dev->flags & IFF_ALLMULTI) && dev->mc_list)
		cirrus_set (dev,PP_RxCTL,MulticastA);
	else
		cirrus_clear (dev,PP_RxCTL,MulticastA);
}

static int cirrus_eeprom_wait (struct net_device *dev)
{
	int i;

	for (i = 0; i < 200; i++) {
		if (!(cirrus_read (dev,PP_SelfST) & SIBUSY))
			return (0);
		udelay (1);
	}

	return (-1);
}

static int cirrus_eeprom_read (struct net_device *dev,u16 *value,u16 offset)
{
	if (cirrus_eeprom_wait (dev) < 0)
		return (-1);

	cirrus_write (dev,PP_EEPROMCommand,offset | EEReadRegister);

	if (cirrus_eeprom_wait (dev) < 0)
		return (-1);

	*value = cirrus_read (dev,PP_EEPROMData);

	return (0);
}

static int cirrus_eeprom (struct net_device *dev,cirrus_eeprom_t *eeprom)
{
	u16 offset,buf[16],*word;
	u8 checksum = 0,*byte;

	if (cirrus_eeprom_read (dev,buf,0) < 0) {
		read_timed_out:
		printk (KERN_DEBUG "%s: EEPROM read timed out\n",dev->name);
		return (-ETIMEDOUT);
	}

	if ((buf[0] >> 8) != 0xa1) {
		printk (KERN_DEBUG "%s: No EEPROM present\n",dev->name);
		return (-ENODEV);
	}

	if ((buf[0] & 0xff) < sizeof (buf)) {
		eeprom_too_small:
		printk (KERN_DEBUG "%s: EEPROM too small\n",dev->name);
		return (-ENODEV);
	}

	for (offset = 1; offset < (buf[0] & 0xff); offset++) {
		if (cirrus_eeprom_read (dev,buf + offset,offset) < 0)
			goto read_timed_out;

		if (buf[offset] == 0xffff)
			goto eeprom_too_small;
	}

	if (buf[1] != 0x2020) {
		printk (KERN_DEBUG "%s: Group Header #1 mismatch\n",dev->name);
		return (-EIO);
	}

	if (buf[5] != 0x502c) {
		printk (KERN_DEBUG "%s: Group Header #2 mismatch\n",dev->name);
		return (-EIO);
	}

	if (buf[12] != 0x2158) {
		printk (KERN_DEBUG "%s: Group Header #3 mismatch\n",dev->name);
		return (-EIO);
	}

	eeprom->io_base = buf[2];
	eeprom->irq = buf[3];
	eeprom->dma = buf[4];
	eeprom->mem_base = (buf[7] << 16) | buf[6];
	eeprom->rom_base = (buf[9] << 16) | buf[8];
	eeprom->rom_mask = (buf[11] << 16) | buf[10];

	word = (u16 *) eeprom->mac;
	for (offset = 0; offset < 3; offset++) word[offset] = buf[13 + offset];

	byte = (u8 *) buf;
	for (offset = 0; offset < sizeof (buf); offset++) checksum += byte[offset];

	if (cirrus_eeprom_read (dev,&offset,0x10) < 0)
		goto read_timed_out;

	if ((offset >> 8) != (u8) (0x100 - checksum)) {
		printk (KERN_DEBUG "%s: Checksum mismatch (expected 0x%.2x, got 0x%.2x instead\n",
				dev->name,
				(u8) (0x100 - checksum),
				offset >> 8);
		return (-EIO);
	}

	return (0);
}

/*
 * Architecture dependant code
 */

#ifdef CONFIG_SA1100_FRODO
static void frodo_reset (struct net_device *dev)
{
	int i;
	volatile u16 value;

	/* reset ethernet controller */
	FRODO_CPLD_ETHERNET |= FRODO_ETH_RESET;
	mdelay (50);
	FRODO_CPLD_ETHERNET &= ~FRODO_ETH_RESET;
	mdelay (50);

	/* we tied SBHE to CHIPSEL, so each memory access ensure the chip is in 16-bit mode */
	for (i = 0; i < 3; i++) value = cirrus_read (dev,0);

	/* FIXME: poll status bit */
}
#endif	/* #ifdef CONFIG_SA1100_FRODO */

/*
 * Driver initialization routines
 */

static int io = 0;
static int irq = 0;

int __init cirrus_probe (struct net_device *dev)
{
	static cirrus_t priv;
	int i,result;
	u16 value;
	cirrus_eeprom_t eeprom;

	printk ("Cirrus Logic CS8900A driver for Linux (V0.02)\n");

	memset (&priv,0,sizeof (cirrus_t));

	ether_setup (dev);

	dev->open               = cirrus_start;
	dev->stop               = cirrus_stop;
	dev->hard_start_xmit    = cirrus_send_start;
	dev->get_stats          = cirrus_get_stats;
	dev->set_multicast_list = cirrus_set_receive_mode;
	dev->set_mac_address	= cirrus_set_mac_address;
	dev->tx_timeout         = cirrus_transmit_timeout;
	dev->watchdog_timeo     = HZ;

	dev->dev_addr[0] = 0x00;
	dev->dev_addr[1] = 0x00;
	dev->dev_addr[2] = 0x00;
	dev->dev_addr[3] = 0x00;
	dev->dev_addr[4] = 0x00;
	dev->dev_addr[5] = 0x00;

	dev->if_port   = IF_PORT_10BASET;
	dev->priv      = (void *) &priv;

	SET_MODULE_OWNER (dev);

	dev->base_addr = CIRRUS_DEFAULT_IO;
	dev->irq = CIRRUS_DEFAULT_IRQ;

	/* module parameters override everything */
	if (io > 0) dev->base_addr = io;
	if (irq > 0) dev->irq = irq;

	if (!dev->base_addr) {
		printk (KERN_ERR
				"%s: No default I/O base address defined. Use io=... or\n"
				"%s: define CIRRUS_DEFAULT_IO for your platform\n",
				dev->name,dev->name);
		return (-EINVAL);
	}

	if (!dev->irq) {
		printk (KERN_ERR
				"%s: No default IRQ number defined. Use irq=... or\n"
				"%s: define CIRRUS_DEFAULT_IRQ for your platform\n",
				dev->name,dev->name);
		return (-EINVAL);
	}

	if ((result = check_region (dev->base_addr,16))) {
		printk (KERN_ERR "%s: can't get I/O port address 0x%lx\n",dev->name,dev->base_addr);
		return (result);
	}

	if (!request_region (dev->base_addr,16,dev->name))
		return -EBUSY;

#ifdef CONFIG_SA1100_FRODO
	frodo_reset (dev);
#endif	/* #ifdef CONFIG_SA1100_FRODO */

	/* if an EEPROM is present, use it's MAC address */
	if (!cirrus_eeprom (dev,&eeprom))
		for (i = 0; i < 6; i++)
			dev->dev_addr[i] = eeprom.mac[i];

	/* verify EISA registration number for Cirrus Logic */
	if ((value = cirrus_read (dev,PP_ProductID)) != EISA_REG_CODE) {
		printk (KERN_ERR "%s: incorrect signature 0x%.4x\n",dev->name,value);
		return (-ENXIO);
	}

	/* verify chip version */
	value = cirrus_read (dev,PP_ProductID + 2);
	if (VERSION (value) != CS8900A) {
		printk (KERN_ERR "%s: unknown chip version 0x%.8x\n",dev->name,VERSION (value));
		return (-ENXIO);
	}
	printk (KERN_INFO "%s: CS8900A rev %c detected\n",dev->name,'B' + REVISION (value) - REV_B);

	/* setup interrupt number */
	cirrus_write (dev,PP_IntNum,0);

	/* configure MAC address */
	for (i = 0; i < ETH_ALEN; i += 2)
		cirrus_write (dev,PP_IA + i,dev->dev_addr[i] | (dev->dev_addr[i + 1] << 8));

	return (0);
}

EXPORT_NO_SYMBOLS;

static struct net_device dev;

static int __init cirrus_init (void)
{
	memset (&dev,0,sizeof (struct net_device));
	dev.init = cirrus_probe;
	return (register_netdev (&dev));
}

static void __exit cirrus_cleanup (void)
{
	release_region (dev.base_addr,16);
	unregister_netdev (&dev);
}

MODULE_AUTHOR ("Abraham van der Merwe <abraham@2d3d.co.za>");
MODULE_DESCRIPTION ("Cirrus Logic CS8900A driver for Linux (V0.02)");
MODULE_LICENSE ("GPL");
MODULE_PARM_DESC (io,"I/O Base Address");
MODULE_PARM (io,"i");
MODULE_PARM_DESC (irq,"IRQ Number");
MODULE_PARM (irq,"i");

module_init (cirrus_init);
module_exit (cirrus_cleanup);

