/*
 * Network device driver for the BMAC ethernet controller on
 * Apple Powermacs.  Assumes it's under a DBDMA controller.
 *
 * Copyright (C) 1998 Randy Gobbel.
 *
 * May 1999, Al Viro: proper release of /proc/net/bmac entry, switched to
 * dynamic procfs inode.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>
#include <asm/prom.h>
#include <asm/dbdma.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/irq.h>
#ifdef CONFIG_PMAC_PBOOK
#include <linux/adb.h>
#include <linux/pmu.h>
#endif /* CONFIG_PMAC_PBOOK */
#include "bmac.h"

#define trunc_page(x)	((void *)(((unsigned long)(x)) & ~((unsigned long)(PAGE_SIZE - 1))))
#define round_page(x)	trunc_page(((unsigned long)(x)) + ((unsigned long)(PAGE_SIZE - 1)))

/*
 * CRC polynomial - used in working out multicast filter bits.
 */
#define ENET_CRCPOLY 0x04c11db7

/* switch to use multicast code lifted from sunhme driver */
#define SUNHME_MULTICAST

#define N_RX_RING	64
#define N_TX_RING	32
#define MAX_TX_ACTIVE	1
#define ETHERCRC	4
#define ETHERMINPACKET	64
#define ETHERMTU	1500
#define RX_BUFLEN	(ETHERMTU + 14 + ETHERCRC + 2)
#define TX_TIMEOUT	HZ	/* 1 second */

/* Bits in transmit DMA status */
#define TX_DMA_ERR	0x80

#define XXDEBUG(args)

struct bmac_data {
	/* volatile struct bmac *bmac; */
	struct sk_buff_head *queue;
	volatile struct dbdma_regs *tx_dma;
	int tx_dma_intr;
	volatile struct dbdma_regs *rx_dma;
	int rx_dma_intr;
	volatile struct dbdma_cmd *tx_cmds;	/* xmit dma command list */
	volatile struct dbdma_cmd *rx_cmds;	/* recv dma command list */
	struct device_node *node;
	struct sk_buff *rx_bufs[N_RX_RING];
	int rx_fill;
	int rx_empty;
	struct sk_buff *tx_bufs[N_TX_RING];
	int tx_fill;
	int tx_empty;
	unsigned char tx_fullup;
	struct net_device_stats stats;
	struct timer_list tx_timeout;
	int timeout_active;
	int sleeping;
	int opened;
	int is_bmac_plus;
	u32 device_id;
	unsigned short hash_use_count[64];
	unsigned short hash_table_mask[4];
	struct net_device *next_bmac;
};

typedef struct bmac_reg_entry {
	char *name;
	unsigned short reg_offset;
} bmac_reg_entry_t;

#define N_REG_ENTRIES 31

static bmac_reg_entry_t reg_entries[N_REG_ENTRIES] = {
	{"MEMADD", MEMADD},
	{"MEMDATAHI", MEMDATAHI},
	{"MEMDATALO", MEMDATALO},
	{"TXPNTR", TXPNTR},
	{"RXPNTR", RXPNTR},
	{"IPG1", IPG1},
	{"IPG2", IPG2},
	{"ALIMIT", ALIMIT},
	{"SLOT", SLOT},
	{"PALEN", PALEN},
	{"PAPAT", PAPAT},
	{"TXSFD", TXSFD},
	{"JAM", JAM},
	{"TXCFG", TXCFG},
	{"TXMAX", TXMAX},
	{"TXMIN", TXMIN},
	{"PAREG", PAREG},
	{"DCNT", DCNT},
	{"NCCNT", NCCNT},
	{"NTCNT", NTCNT},
	{"EXCNT", EXCNT},
	{"LTCNT", LTCNT},
	{"TXSM", TXSM},
	{"RXCFG", RXCFG},
	{"RXMAX", RXMAX},
	{"RXMIN", RXMIN},
	{"FRCNT", FRCNT},
	{"AECNT", AECNT},
	{"FECNT", FECNT},
	{"RXSM", RXSM},
	{"RXCV", RXCV}
};

static struct net_device *bmac_devs;
static unsigned char *bmac_emergency_rxbuf;

#ifdef CONFIG_PMAC_PBOOK
static int bmac_sleep_notify(struct pmu_sleep_notifier *self, int when);
static struct pmu_sleep_notifier bmac_sleep_notifier = {
	bmac_sleep_notify, SLEEP_LEVEL_NET,
};
#endif

/*
 * Number of bytes of private data per BMAC: allow enough for
 * the rx and tx dma commands plus a branch dma command each,
 * and another 16 bytes to allow us to align the dma command
 * buffers on a 16 byte boundary.
 */
#define PRIV_BYTES	(sizeof(struct bmac_data) \
	+ (N_RX_RING + N_TX_RING + 4) * sizeof(struct dbdma_cmd) \
	+ sizeof(struct sk_buff_head))

static unsigned char bitrev(unsigned char b);
static void bmac_probe1(struct device_node *bmac, int is_bmac_plus);
static int bmac_open(struct net_device *dev);
static int bmac_close(struct net_device *dev);
static int bmac_transmit_packet(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *bmac_stats(struct net_device *dev);
static void bmac_set_multicast(struct net_device *dev);
static int bmac_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
static void bmac_reset_and_enable(struct net_device *dev);
static void bmac_start_chip(struct net_device *dev);
static void bmac_init_chip(struct net_device *dev);
static void bmac_init_registers(struct net_device *dev);
static void bmac_enable_and_reset_chip(struct net_device *dev);
static int bmac_set_address(struct net_device *dev, void *addr);
static void bmac_misc_intr(int irq, void *dev_id, struct pt_regs *regs);
static void bmac_txdma_intr(int irq, void *dev_id, struct pt_regs *regs);
static void bmac_rxdma_intr(int irq, void *dev_id, struct pt_regs *regs);
static void bmac_set_timeout(struct net_device *dev);
static void bmac_tx_timeout(unsigned long data);
static int bmac_proc_info ( char *buffer, char **start, off_t offset, int length);
static int bmac_output(struct sk_buff *skb, struct net_device *dev);
static void bmac_start(struct net_device *dev);

#define	DBDMA_SET(x)	( ((x) | (x) << 16) )
#define	DBDMA_CLEAR(x)	( (x) << 16)

static inline void
dbdma_st32(volatile unsigned long *a, unsigned long x)
{
	__asm__ volatile( "stwbrx %0,0,%1" : : "r" (x), "r" (a) : "memory");
	return;
}

static inline unsigned long
dbdma_ld32(volatile unsigned long *a)
{
	unsigned long swap;
	__asm__ volatile ("lwbrx %0,0,%1" :  "=r" (swap) : "r" (a));
	return swap;
}

static void
dbdma_continue(volatile struct dbdma_regs *dmap)
{
	dbdma_st32((volatile unsigned long *)&dmap->control,
		   DBDMA_SET(RUN|WAKE) | DBDMA_CLEAR(PAUSE|DEAD));
	eieio();
}

static void
dbdma_reset(volatile struct dbdma_regs *dmap)
{
	dbdma_st32((volatile unsigned long *)&dmap->control,
		   DBDMA_CLEAR(ACTIVE|DEAD|WAKE|FLUSH|PAUSE|RUN));
	eieio();
	while (dbdma_ld32((volatile unsigned long *)&dmap->status) & RUN)
		eieio();
}

static void
dbdma_setcmd(volatile struct dbdma_cmd *cp,
	     unsigned short cmd, unsigned count, unsigned long addr,
	     unsigned long cmd_dep)
{
	out_le16(&cp->command, cmd);
	out_le16(&cp->req_count, count);
	out_le32(&cp->phy_addr, addr);
	out_le32(&cp->cmd_dep, cmd_dep);
	out_le16(&cp->xfer_status, 0);
	out_le16(&cp->res_count, 0);
}

static inline
void bmwrite(struct net_device *dev, unsigned long reg_offset, unsigned data )
{
	out_le16((void *)dev->base_addr + reg_offset, data);
}


static inline
volatile unsigned short bmread(struct net_device *dev, unsigned long reg_offset )
{
	return in_le16((void *)dev->base_addr + reg_offset);
}

static void
bmac_enable_and_reset_chip(struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_regs *rd = bp->rx_dma;
	volatile struct dbdma_regs *td = bp->tx_dma;

	if (rd)
		dbdma_reset(rd);
	if (td)
		dbdma_reset(td);

	pmac_call_feature(PMAC_FTR_BMAC_ENABLE, bp->node, 0, 1);
}

#define MIFDELAY	udelay(10)

static unsigned int
bmac_mif_readbits(struct net_device *dev, int nb)
{
	unsigned int val = 0;

	while (--nb >= 0) {
		bmwrite(dev, MIFCSR, 0);
		MIFDELAY;
		if (bmread(dev, MIFCSR) & 8)
			val |= 1 << nb;
		bmwrite(dev, MIFCSR, 1);
		MIFDELAY;
	}
	bmwrite(dev, MIFCSR, 0);
	MIFDELAY;
	bmwrite(dev, MIFCSR, 1);
	MIFDELAY;
	return val;
}

static void
bmac_mif_writebits(struct net_device *dev, unsigned int val, int nb)
{
	int b;

	while (--nb >= 0) {
		b = (val & (1 << nb))? 6: 4;
		bmwrite(dev, MIFCSR, b);
		MIFDELAY;
		bmwrite(dev, MIFCSR, b|1);
		MIFDELAY;
	}
}

static unsigned int
bmac_mif_read(struct net_device *dev, unsigned int addr)
{
	unsigned int val;

	bmwrite(dev, MIFCSR, 4);
	MIFDELAY;
	bmac_mif_writebits(dev, ~0U, 32);
	bmac_mif_writebits(dev, 6, 4);
	bmac_mif_writebits(dev, addr, 10);
	bmwrite(dev, MIFCSR, 2);
	MIFDELAY;
	bmwrite(dev, MIFCSR, 1);
	MIFDELAY;
	val = bmac_mif_readbits(dev, 17);
	bmwrite(dev, MIFCSR, 4);
	MIFDELAY;
	return val;
}

static void
bmac_mif_write(struct net_device *dev, unsigned int addr, unsigned int val)
{
	bmwrite(dev, MIFCSR, 4);
	MIFDELAY;
	bmac_mif_writebits(dev, ~0U, 32);
	bmac_mif_writebits(dev, 5, 4);
	bmac_mif_writebits(dev, addr, 10);
	bmac_mif_writebits(dev, 2, 2);
	bmac_mif_writebits(dev, val, 16);
	bmac_mif_writebits(dev, 3, 2);
}

static void
bmac_init_registers(struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile unsigned short regValue;
	unsigned short *pWord16;
	int i;

	/* XXDEBUG(("bmac: enter init_registers\n")); */

	bmwrite(dev, RXRST, RxResetValue);
	bmwrite(dev, TXRST, TxResetBit);

	i = 100;
	do {
		--i;
		udelay(10000);
		regValue = bmread(dev, TXRST); /* wait for reset to clear..acknowledge */
	} while ((regValue & TxResetBit) && i > 0);

	if (!bp->is_bmac_plus) {
		regValue = bmread(dev, XCVRIF);
		regValue |= ClkBit | SerialMode | COLActiveLow;
		bmwrite(dev, XCVRIF, regValue);
		udelay(10000);
	}

	bmwrite(dev, RSEED, (unsigned short)0x1968);		

	regValue = bmread(dev, XIFC);
	regValue |= TxOutputEnable;
	bmwrite(dev, XIFC, regValue);

	bmread(dev, PAREG);

	/* set collision counters to 0 */
	bmwrite(dev, NCCNT, 0);
	bmwrite(dev, NTCNT, 0);
	bmwrite(dev, EXCNT, 0);
	bmwrite(dev, LTCNT, 0);

	/* set rx counters to 0 */
	bmwrite(dev, FRCNT, 0);
	bmwrite(dev, LECNT, 0);
	bmwrite(dev, AECNT, 0);
	bmwrite(dev, FECNT, 0);
	bmwrite(dev, RXCV, 0);

	/* set tx fifo information */
	bmwrite(dev, TXTH, 4);	/* 4 octets before tx starts */

	bmwrite(dev, TXFIFOCSR, 0);	/* first disable txFIFO */
	bmwrite(dev, TXFIFOCSR, TxFIFOEnable );

	/* set rx fifo information */
	bmwrite(dev, RXFIFOCSR, 0);	/* first disable rxFIFO */
	bmwrite(dev, RXFIFOCSR, RxFIFOEnable );

	//bmwrite(dev, TXCFG, TxMACEnable);	       	/* TxNeverGiveUp maybe later */
	bmread(dev, STATUS);		/* read it just to clear it */

	/* zero out the chip Hash Filter registers */
	for (i=0; i<4; i++) bp->hash_table_mask[i] = 0;
	bmwrite(dev, BHASH3, bp->hash_table_mask[0]); 	/* bits 15 - 0 */
	bmwrite(dev, BHASH2, bp->hash_table_mask[1]); 	/* bits 31 - 16 */
	bmwrite(dev, BHASH1, bp->hash_table_mask[2]); 	/* bits 47 - 32 */
	bmwrite(dev, BHASH0, bp->hash_table_mask[3]); 	/* bits 63 - 48 */
	
	pWord16 = (unsigned short *)dev->dev_addr;
	bmwrite(dev, MADD0, *pWord16++);
	bmwrite(dev, MADD1, *pWord16++);
	bmwrite(dev, MADD2, *pWord16);

	bmwrite(dev, RXCFG, RxCRCNoStrip | RxHashFilterEnable | RxRejectOwnPackets);

	bmwrite(dev, INTDISABLE, EnableNormal);

	return;
}

#if 0
static void
bmac_disable_interrupts(struct net_device *dev)
{
	bmwrite(dev, INTDISABLE, DisableAll);
}

static void
bmac_enable_interrupts(struct net_device *dev)
{
	bmwrite(dev, INTDISABLE, EnableNormal);
}
#endif


static void
bmac_start_chip(struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_regs *rd = bp->rx_dma;
	unsigned short	oldConfig;

	/* enable rx dma channel */
	dbdma_continue(rd);

	oldConfig = bmread(dev, TXCFG);		
	bmwrite(dev, TXCFG, oldConfig | TxMACEnable );

	/* turn on rx plus any other bits already on (promiscuous possibly) */
	oldConfig = bmread(dev, RXCFG);		
	bmwrite(dev, RXCFG, oldConfig | RxMACEnable );
	udelay(20000);
}

static void
bmac_init_phy(struct net_device *dev)
{
	unsigned int addr;
	struct bmac_data *bp = (struct bmac_data *) dev->priv;

	printk(KERN_DEBUG "phy registers:");
	for (addr = 0; addr < 32; ++addr) {
		if ((addr & 7) == 0)
			printk("\n" KERN_DEBUG);
		printk(" %.4x", bmac_mif_read(dev, addr));
	}
	printk("\n");
	if (bp->is_bmac_plus) {
		unsigned int capable, ctrl;

		ctrl = bmac_mif_read(dev, 0);
		capable = ((bmac_mif_read(dev, 1) & 0xf800) >> 6) | 1;
		if (bmac_mif_read(dev, 4) != capable
		    || (ctrl & 0x1000) == 0) {
			bmac_mif_write(dev, 4, capable);
			bmac_mif_write(dev, 0, 0x1200);
		} else
			bmac_mif_write(dev, 0, 0x1000);
	}
}

static void
bmac_init_chip(struct net_device *dev)
{
	bmac_init_phy(dev);
	bmac_init_registers(dev);
}

#ifdef CONFIG_PMAC_PBOOK
static int
bmac_sleep_notify(struct pmu_sleep_notifier *self, int when)
{
	struct bmac_data *bp;
	unsigned long flags;
	unsigned short config;
	struct net_device* dev = bmac_devs;
	int i;
	
	if (bmac_devs == 0)
		return PBOOK_SLEEP_OK;
		
	bp = (struct bmac_data *) dev->priv;
	
	switch (when) {
	case PBOOK_SLEEP_REQUEST:
		break;
	case PBOOK_SLEEP_REJECT:
		break;
	case PBOOK_SLEEP_NOW:
		netif_device_detach(dev);
		/* prolly should wait for dma to finish & turn off the chip */
		save_flags(flags); cli();
		if (bp->timeout_active) {
			del_timer(&bp->tx_timeout);
			bp->timeout_active = 0;
		}
		disable_irq(dev->irq);
		disable_irq(bp->tx_dma_intr);
		disable_irq(bp->rx_dma_intr);
		bp->sleeping = 1;
		restore_flags(flags);
		if (bp->opened) {
			volatile struct dbdma_regs *rd = bp->rx_dma;
			volatile struct dbdma_regs *td = bp->tx_dma;
			
			config = bmread(dev, RXCFG);
			bmwrite(dev, RXCFG, (config & ~RxMACEnable));
			config = bmread(dev, TXCFG);
			bmwrite(dev, TXCFG, (config & ~TxMACEnable));
			bmwrite(dev, INTDISABLE, DisableAll); /* disable all intrs */
			/* disable rx and tx dma */
			st_le32(&rd->control, DBDMA_CLEAR(RUN|PAUSE|FLUSH|WAKE));	/* clear run bit */
			st_le32(&td->control, DBDMA_CLEAR(RUN|PAUSE|FLUSH|WAKE));	/* clear run bit */
			/* free some skb's */
			for (i=0; i<N_RX_RING; i++) {
				if (bp->rx_bufs[i] != NULL) {
					dev_kfree_skb(bp->rx_bufs[i]);
					bp->rx_bufs[i] = NULL;
				}
			}
			for (i = 0; i<N_TX_RING; i++) {
				if (bp->tx_bufs[i] != NULL) {
					dev_kfree_skb(bp->tx_bufs[i]);
					bp->tx_bufs[i] = NULL;
				}
			}
		}
		pmac_call_feature(PMAC_FTR_BMAC_ENABLE, bp->node, 0, 0);
		break;
	case PBOOK_WAKE:
		/* see if this is enough */
		if (bp->opened)
			bmac_reset_and_enable(dev);
		enable_irq(dev->irq);
		enable_irq(bp->tx_dma_intr);
		enable_irq(bp->rx_dma_intr);
		netif_device_attach(dev);
		break;
	}
	return PBOOK_SLEEP_OK;
}
#endif

static int bmac_set_address(struct net_device *dev, void *addr)
{
	unsigned char *p = addr;
	unsigned short *pWord16;
	unsigned long flags;
	int i;

	XXDEBUG(("bmac: enter set_address\n"));
	save_flags(flags); cli();

	for (i = 0; i < 6; ++i) {
		dev->dev_addr[i] = p[i];
	}
	/* load up the hardware address */
	pWord16  = (unsigned short *)dev->dev_addr;
	bmwrite(dev, MADD0, *pWord16++);
	bmwrite(dev, MADD1, *pWord16++);
	bmwrite(dev, MADD2, *pWord16);

	restore_flags(flags);
	XXDEBUG(("bmac: exit set_address\n"));
	return 0;
}

static inline void bmac_set_timeout(struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	unsigned long flags;

	save_flags(flags);
	cli();
	if (bp->timeout_active)
		del_timer(&bp->tx_timeout);
	bp->tx_timeout.expires = jiffies + TX_TIMEOUT;
	bp->tx_timeout.function = bmac_tx_timeout;
	bp->tx_timeout.data = (unsigned long) dev;
	add_timer(&bp->tx_timeout);
	bp->timeout_active = 1;
	restore_flags(flags);
}

static void
bmac_construct_xmt(struct sk_buff *skb, volatile struct dbdma_cmd *cp)
{
	void *vaddr;
	unsigned long baddr;
	unsigned long len;

	len = skb->len;
	vaddr = skb->data;
	baddr = virt_to_bus(vaddr);

	dbdma_setcmd(cp, (OUTPUT_LAST | INTR_ALWAYS | WAIT_IFCLR), len, baddr, 0);
}

static void
bmac_construct_rxbuff(struct sk_buff *skb, volatile struct dbdma_cmd *cp)
{
	unsigned char *addr = skb? skb->data: bmac_emergency_rxbuf;

	dbdma_setcmd(cp, (INPUT_LAST | INTR_ALWAYS), RX_BUFLEN,
		     virt_to_bus(addr), 0);
}

/* Bit-reverse one byte of an ethernet hardware address. */
static unsigned char
bitrev(unsigned char b)
{
	int d = 0, i;

	for (i = 0; i < 8; ++i, b >>= 1)
		d = (d << 1) | (b & 1);
	return d;
}


static void
bmac_init_tx_ring(struct bmac_data *bp)
{
	volatile struct dbdma_regs *td = bp->tx_dma;

	memset((char *)bp->tx_cmds, 0, (N_TX_RING+1) * sizeof(struct dbdma_cmd));

	bp->tx_empty = 0;
	bp->tx_fill = 0;
	bp->tx_fullup = 0;

	/* put a branch at the end of the tx command list */
	dbdma_setcmd(&bp->tx_cmds[N_TX_RING],
		     (DBDMA_NOP | BR_ALWAYS), 0, 0, virt_to_bus(bp->tx_cmds));

	/* reset tx dma */
	dbdma_reset(td);
	out_le32(&td->wait_sel, 0x00200020);
	out_le32(&td->cmdptr, virt_to_bus(bp->tx_cmds));
}

static int
bmac_init_rx_ring(struct bmac_data *bp)
{
	volatile struct dbdma_regs *rd = bp->rx_dma;
	int i;
	struct sk_buff *skb;

	/* initialize list of sk_buffs for receiving and set up recv dma */
	memset((char *)bp->rx_cmds, 0,
	       (N_RX_RING + 1) * sizeof(struct dbdma_cmd));
	for (i = 0; i < N_RX_RING; i++) {
		if ((skb = bp->rx_bufs[i]) == NULL) {
			bp->rx_bufs[i] = skb = dev_alloc_skb(RX_BUFLEN+2);
			if (skb != NULL)
				skb_reserve(skb, 2);
		}
		bmac_construct_rxbuff(skb, &bp->rx_cmds[i]);
	}

	bp->rx_empty = 0;
	bp->rx_fill = i;

	/* Put a branch back to the beginning of the receive command list */
	dbdma_setcmd(&bp->rx_cmds[N_RX_RING],
		     (DBDMA_NOP | BR_ALWAYS), 0, 0, virt_to_bus(bp->rx_cmds));

	/* start rx dma */
	dbdma_reset(rd);
	out_le32(&rd->cmdptr, virt_to_bus(bp->rx_cmds));

	return 1;
}


static int bmac_transmit_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_regs *td = bp->tx_dma;
	int i;

	/* see if there's a free slot in the tx ring */
	/* XXDEBUG(("bmac_xmit_start: empty=%d fill=%d\n", */
	/* 	     bp->tx_empty, bp->tx_fill)); */
	i = bp->tx_fill + 1;
	if (i >= N_TX_RING)
		i = 0;
	if (i == bp->tx_empty) {
		netif_stop_queue(dev);
		bp->tx_fullup = 1;
		XXDEBUG(("bmac_transmit_packet: tx ring full\n"));
		return -1;		/* can't take it at the moment */
	}

	dbdma_setcmd(&bp->tx_cmds[i], DBDMA_STOP, 0, 0, 0);

	bmac_construct_xmt(skb, &bp->tx_cmds[bp->tx_fill]);

	bp->tx_bufs[bp->tx_fill] = skb;
	bp->tx_fill = i;

	bp->stats.tx_bytes += skb->len;

	dbdma_continue(td);

	return 0;
}

static int rxintcount;

static void bmac_rxdma_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_regs *rd = bp->rx_dma;
	volatile struct dbdma_cmd *cp;
	int i, nb, stat;
	struct sk_buff *skb;
	unsigned int residual;
	int last;
	unsigned long flags;

	save_flags(flags); cli();

	if (++rxintcount < 10) {
		XXDEBUG(("bmac_rxdma_intr\n"));
	}

	last = -1;
	i = bp->rx_empty;

	while (1) {
		cp = &bp->rx_cmds[i];
		stat = ld_le16(&cp->xfer_status);
		residual = ld_le16(&cp->res_count);
		if ((stat & ACTIVE) == 0)
			break;
		nb = RX_BUFLEN - residual - 2;
		if (nb < (ETHERMINPACKET - ETHERCRC)) {
			skb = NULL;
			bp->stats.rx_length_errors++;
			bp->stats.rx_errors++;
		} else {
			skb = bp->rx_bufs[i];
			bp->rx_bufs[i] = NULL;
		}
		if (skb != NULL) {
			nb -= ETHERCRC;
			skb_put(skb, nb);
			skb->dev = dev;
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
			dev->last_rx = jiffies;
			++bp->stats.rx_packets;
			bp->stats.rx_bytes += nb;
		} else {
			++bp->stats.rx_dropped;
		}
		dev->last_rx = jiffies;
		if ((skb = bp->rx_bufs[i]) == NULL) {
			bp->rx_bufs[i] = skb = dev_alloc_skb(RX_BUFLEN+2);
			if (skb != NULL)
				skb_reserve(bp->rx_bufs[i], 2);
		}
		bmac_construct_rxbuff(skb, &bp->rx_cmds[i]);
		st_le16(&cp->res_count, 0);
		st_le16(&cp->xfer_status, 0);
		last = i;
		if (++i >= N_RX_RING) i = 0;
	}

	if (last != -1) {
		bp->rx_fill = last;
		bp->rx_empty = i;
	}

	restore_flags(flags);

	dbdma_continue(rd);

	if (rxintcount < 10) {
		XXDEBUG(("bmac_rxdma_intr done\n"));
	}
}

static int txintcount;

static void bmac_txdma_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_cmd *cp;
	int stat;
	unsigned long flags;

	save_flags(flags); cli();

	if (txintcount++ < 10) {
		XXDEBUG(("bmac_txdma_intr\n"));
	}

	/*     del_timer(&bp->tx_timeout); */
	/*     bp->timeout_active = 0; */

	while (1) {
		cp = &bp->tx_cmds[bp->tx_empty];
		stat = ld_le16(&cp->xfer_status);
		if (txintcount < 10) {
			XXDEBUG(("bmac_txdma_xfer_stat=%#0x\n", stat));
		}
		if (!(stat & ACTIVE)) {
			/*
			 * status field might not have been filled by DBDMA
			 */
			if (cp == bus_to_virt(in_le32(&bp->tx_dma->cmdptr)))
				break;
		}

		if (bp->tx_bufs[bp->tx_empty]) {
			++bp->stats.tx_packets;
			dev_kfree_skb_irq(bp->tx_bufs[bp->tx_empty]);
		}
		bp->tx_bufs[bp->tx_empty] = NULL;
		bp->tx_fullup = 0;
		netif_wake_queue(dev);
		if (++bp->tx_empty >= N_TX_RING)
			bp->tx_empty = 0;
		if (bp->tx_empty == bp->tx_fill)
			break;
	}

	restore_flags(flags);

	if (txintcount < 10) {
		XXDEBUG(("bmac_txdma_intr done->bmac_start\n"));
	}

	bmac_start(dev);
}

static struct net_device_stats *bmac_stats(struct net_device *dev)
{
	struct bmac_data *p = (struct bmac_data *) dev->priv;

	return &p->stats;
}

#ifndef SUNHME_MULTICAST
/* Real fast bit-reversal algorithm, 6-bit values */
static int reverse6[64] = {
	0x0,0x20,0x10,0x30,0x8,0x28,0x18,0x38,
	0x4,0x24,0x14,0x34,0xc,0x2c,0x1c,0x3c,
	0x2,0x22,0x12,0x32,0xa,0x2a,0x1a,0x3a,
	0x6,0x26,0x16,0x36,0xe,0x2e,0x1e,0x3e,
	0x1,0x21,0x11,0x31,0x9,0x29,0x19,0x39,
	0x5,0x25,0x15,0x35,0xd,0x2d,0x1d,0x3d,
	0x3,0x23,0x13,0x33,0xb,0x2b,0x1b,0x3b,
	0x7,0x27,0x17,0x37,0xf,0x2f,0x1f,0x3f
};

static unsigned int
crc416(unsigned int curval, unsigned short nxtval)
{
	register unsigned int counter, cur = curval, next = nxtval;
	register int high_crc_set, low_data_set;

	/* Swap bytes */
	next = ((next & 0x00FF) << 8) | (next >> 8);

	/* Compute bit-by-bit */
	for (counter = 0; counter < 16; ++counter) {
		/* is high CRC bit set? */
		if ((cur & 0x80000000) == 0) high_crc_set = 0;
		else high_crc_set = 1;

		cur = cur << 1;
	
		if ((next & 0x0001) == 0) low_data_set = 0;
		else low_data_set = 1;

		next = next >> 1;
	
		/* do the XOR */
		if (high_crc_set ^ low_data_set) cur = cur ^ ENET_CRCPOLY;
	}
	return cur;
}

static unsigned int
bmac_crc(unsigned short *address)
{	
	unsigned int newcrc;

	XXDEBUG(("bmac_crc: addr=%#04x, %#04x, %#04x\n", *address, address[1], address[2]));
	newcrc = crc416(0xffffffff, *address);	/* address bits 47 - 32 */
	newcrc = crc416(newcrc, address[1]);	/* address bits 31 - 16 */
	newcrc = crc416(newcrc, address[2]);	/* address bits 15 - 0  */

	return(newcrc);
}

/*
 * Add requested mcast addr to BMac's hash table filter.
 *
 */

static void
bmac_addhash(struct bmac_data *bp, unsigned char *addr)
{	
	unsigned int	 crc;
	unsigned short	 mask;

	if (!(*addr)) return;
	crc = bmac_crc((unsigned short *)addr) & 0x3f; /* Big-endian alert! */
	crc = reverse6[crc];	/* Hyperfast bit-reversing algorithm */
	if (bp->hash_use_count[crc]++) return; /* This bit is already set */
	mask = crc % 16;
	mask = (unsigned char)1 << mask;
	bp->hash_use_count[crc/16] |= mask;
}

static void
bmac_removehash(struct bmac_data *bp, unsigned char *addr)
{	
	unsigned int crc;
	unsigned char mask;

	/* Now, delete the address from the filter copy, as indicated */
	crc = bmac_crc((unsigned short *)addr) & 0x3f; /* Big-endian alert! */
	crc = reverse6[crc];	/* Hyperfast bit-reversing algorithm */
	if (bp->hash_use_count[crc] == 0) return; /* That bit wasn't in use! */
	if (--bp->hash_use_count[crc]) return; /* That bit is still in use */
	mask = crc % 16;
	mask = ((unsigned char)1 << mask) ^ 0xffff; /* To turn off bit */
	bp->hash_table_mask[crc/16] &= mask;
}

/*
 * Sync the adapter with the software copy of the multicast mask
 *  (logical address filter).
 */

static void
bmac_rx_off(struct net_device *dev)
{
	unsigned short rx_cfg;

	rx_cfg = bmread(dev, RXCFG);
	rx_cfg &= ~RxMACEnable;
	bmwrite(dev, RXCFG, rx_cfg);
	do {
		rx_cfg = bmread(dev, RXCFG);
	}  while (rx_cfg & RxMACEnable);
}

unsigned short
bmac_rx_on(struct net_device *dev, int hash_enable, int promisc_enable)
{
	unsigned short rx_cfg;

	rx_cfg = bmread(dev, RXCFG);
	rx_cfg |= RxMACEnable;
	if (hash_enable) rx_cfg |= RxHashFilterEnable;
	else rx_cfg &= ~RxHashFilterEnable;
	if (promisc_enable) rx_cfg |= RxPromiscEnable;
	else rx_cfg &= ~RxPromiscEnable;
	bmwrite(dev, RXRST, RxResetValue);
	bmwrite(dev, RXFIFOCSR, 0);	/* first disable rxFIFO */
	bmwrite(dev, RXFIFOCSR, RxFIFOEnable );
	bmwrite(dev, RXCFG, rx_cfg );
	return rx_cfg;
}

static void
bmac_update_hash_table_mask(struct net_device *dev, struct bmac_data *bp)
{
	bmwrite(dev, BHASH3, bp->hash_table_mask[0]); /* bits 15 - 0 */
	bmwrite(dev, BHASH2, bp->hash_table_mask[1]); /* bits 31 - 16 */
	bmwrite(dev, BHASH1, bp->hash_table_mask[2]); /* bits 47 - 32 */
	bmwrite(dev, BHASH0, bp->hash_table_mask[3]); /* bits 63 - 48 */
}

#if 0
static void
bmac_add_multi(struct net_device *dev,
	       struct bmac_data *bp, unsigned char *addr)
{
	/* XXDEBUG(("bmac: enter bmac_add_multi\n")); */
	bmac_addhash(bp, addr);
	bmac_rx_off(dev);
	bmac_update_hash_table_mask(dev, bp);
	bmac_rx_on(dev, 1, (dev->flags & IFF_PROMISC)? 1 : 0);
	/* XXDEBUG(("bmac: exit bmac_add_multi\n")); */
}

static void
bmac_remove_multi(struct net_device *dev,
		  struct bmac_data *bp, unsigned char *addr)
{
	bmac_removehash(bp, addr);
	bmac_rx_off(dev);
	bmac_update_hash_table_mask(dev, bp);
	bmac_rx_on(dev, 1, (dev->flags & IFF_PROMISC)? 1 : 0);
}
#endif

/* Set or clear the multicast filter for this adaptor.
    num_addrs == -1	Promiscuous mode, receive all packets
    num_addrs == 0	Normal mode, clear multicast list
    num_addrs > 0	Multicast mode, receive normal and MC packets, and do
			best-effort filtering.
 */
static void bmac_set_multicast(struct net_device *dev)
{
	struct dev_mc_list *dmi;
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	int num_addrs = dev->mc_count;
	unsigned short rx_cfg;
	int i;

	if (bp->sleeping)
		return;

	XXDEBUG(("bmac: enter bmac_set_multicast, n_addrs=%d\n", num_addrs));

	if((dev->flags & IFF_ALLMULTI) || (dev->mc_count > 64)) {
		for (i=0; i<4; i++) bp->hash_table_mask[i] = 0xffff;
		bmac_update_hash_table_mask(dev, bp);
		rx_cfg = bmac_rx_on(dev, 1, 0);
		XXDEBUG(("bmac: all multi, rx_cfg=%#08x\n"));
	} else if ((dev->flags & IFF_PROMISC) || (num_addrs < 0)) {
		rx_cfg = bmread(dev, RXCFG);
		rx_cfg |= RxPromiscEnable;
		bmwrite(dev, RXCFG, rx_cfg);
		rx_cfg = bmac_rx_on(dev, 0, 1);
		XXDEBUG(("bmac: promisc mode enabled, rx_cfg=%#08x\n", rx_cfg));
	} else {
		for (i=0; i<4; i++) bp->hash_table_mask[i] = 0;
		for (i=0; i<64; i++) bp->hash_use_count[i] = 0;
		if (num_addrs == 0) {
			rx_cfg = bmac_rx_on(dev, 0, 0);
			XXDEBUG(("bmac: multi disabled, rx_cfg=%#08x\n", rx_cfg));
		} else {
			for (dmi=dev->mc_list; dmi!=NULL; dmi=dmi->next)
				bmac_addhash(bp, dmi->dmi_addr);
			bmac_update_hash_table_mask(dev, bp);
			rx_cfg = bmac_rx_on(dev, 1, 0);
			XXDEBUG(("bmac: multi enabled, rx_cfg=%#08x\n", rx_cfg));
		}
	}
	/* XXDEBUG(("bmac: exit bmac_set_multicast\n")); */
}
#else /* ifdef SUNHME_MULTICAST */

/* The version of set_multicast below was lifted from sunhme.c */

static void bmac_set_multicast(struct net_device *dev)
{
	struct dev_mc_list *dmi = dev->mc_list;
	char *addrs;
	int i;
	unsigned short rx_cfg;
	u32 crc;

	if((dev->flags & IFF_ALLMULTI) || (dev->mc_count > 64)) {
		bmwrite(dev, BHASH0, 0xffff);
		bmwrite(dev, BHASH1, 0xffff);
		bmwrite(dev, BHASH2, 0xffff);
		bmwrite(dev, BHASH3, 0xffff);
	} else if(dev->flags & IFF_PROMISC) {
		rx_cfg = bmread(dev, RXCFG);
		rx_cfg |= RxPromiscEnable;
		bmwrite(dev, RXCFG, rx_cfg);
	} else {
		u16 hash_table[4];
	
		rx_cfg = bmread(dev, RXCFG);
		rx_cfg &= ~RxPromiscEnable;
		bmwrite(dev, RXCFG, rx_cfg);

		for(i = 0; i < 4; i++) hash_table[i] = 0;
	
		for(i = 0; i < dev->mc_count; i++) {
			addrs = dmi->dmi_addr;
			dmi = dmi->next;

			if(!(*addrs & 1))
				continue;

			crc = ether_crc_le(6, addrs);
			crc >>= 26;
			hash_table[crc >> 4] |= 1 << (crc & 0xf);
		}
		bmwrite(dev, BHASH0, hash_table[0]);
		bmwrite(dev, BHASH1, hash_table[1]);
		bmwrite(dev, BHASH2, hash_table[2]);
		bmwrite(dev, BHASH3, hash_table[3]);
	}
}
#endif /* SUNHME_MULTICAST */

static int miscintcount;

static void bmac_misc_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct bmac_data *bp = (struct bmac_data *)dev->priv;
	unsigned int status = bmread(dev, STATUS);
	if (miscintcount++ < 10) {
		XXDEBUG(("bmac_misc_intr\n"));
	}
	/* XXDEBUG(("bmac_misc_intr, status=%#08x\n", status)); */
	/*     bmac_txdma_intr_inner(irq, dev_id, regs); */
	/*   if (status & FrameReceived) bp->stats.rx_dropped++; */
	if (status & RxErrorMask) bp->stats.rx_errors++;
	if (status & RxCRCCntExp) bp->stats.rx_crc_errors++;
	if (status & RxLenCntExp) bp->stats.rx_length_errors++;
	if (status & RxOverFlow) bp->stats.rx_over_errors++;
	if (status & RxAlignCntExp) bp->stats.rx_frame_errors++;

	/*   if (status & FrameSent) bp->stats.tx_dropped++; */
	if (status & TxErrorMask) bp->stats.tx_errors++;
	if (status & TxUnderrun) bp->stats.tx_fifo_errors++;
	if (status & TxNormalCollExp) bp->stats.collisions++;
}

/*
 * Procedure for reading EEPROM
 */
#define SROMAddressLength	5
#define DataInOn		0x0008
#define DataInOff		0x0000
#define Clk			0x0002
#define ChipSelect		0x0001
#define SDIShiftCount		3
#define SD0ShiftCount		2
#define	DelayValue		1000	/* number of microseconds */
#define SROMStartOffset		10	/* this is in words */
#define SROMReadCount		3	/* number of words to read from SROM */
#define SROMAddressBits		6
#define EnetAddressOffset	20

static unsigned char
bmac_clock_out_bit(struct net_device *dev)
{
	unsigned short         data;
	unsigned short         val;

	bmwrite(dev, SROMCSR, ChipSelect | Clk);
	udelay(DelayValue);

	data = bmread(dev, SROMCSR);
	udelay(DelayValue);
	val = (data >> SD0ShiftCount) & 1;

	bmwrite(dev, SROMCSR, ChipSelect);
	udelay(DelayValue);

	return val;
}

static void
bmac_clock_in_bit(struct net_device *dev, unsigned int val)
{
	unsigned short data;

	if (val != 0 && val != 1) return;

	data = (val << SDIShiftCount);
	bmwrite(dev, SROMCSR, data | ChipSelect  );
	udelay(DelayValue);

	bmwrite(dev, SROMCSR, data | ChipSelect | Clk );
	udelay(DelayValue);

	bmwrite(dev, SROMCSR, data | ChipSelect);
	udelay(DelayValue);
}

static void
reset_and_select_srom(struct net_device *dev)
{
	/* first reset */
	bmwrite(dev, SROMCSR, 0);
	udelay(DelayValue);

	/* send it the read command (110) */
	bmac_clock_in_bit(dev, 1);
	bmac_clock_in_bit(dev, 1);
	bmac_clock_in_bit(dev, 0);
}

static unsigned short
read_srom(struct net_device *dev, unsigned int addr, unsigned int addr_len)
{
	unsigned short data, val;
	unsigned int i;

	/* send out the address we want to read from */
	for (i = 0; i < addr_len; i++)	{
		val = addr >> (addr_len-i-1);
		bmac_clock_in_bit(dev, val & 1);
	}

	/* Now read in the 16-bit data */
	data = 0;
	for (i = 0; i < 16; i++)	{
		val = bmac_clock_out_bit(dev);
		data <<= 1;
		data |= val;
	}
	bmwrite(dev, SROMCSR, 0);

	return data;
}

/*
 * It looks like Cogent and SMC use different methods for calculating
 * checksums. What a pain..
 */

static int
bmac_verify_checksum(struct net_device *dev)
{
	unsigned short data, storedCS;

	reset_and_select_srom(dev);
	data = read_srom(dev, 3, SROMAddressBits);
	storedCS = ((data >> 8) & 0x0ff) | ((data << 8) & 0xff00);

	return 0;
}


static void
bmac_get_station_address(struct net_device *dev, unsigned char *ea)
{
	int i;
	unsigned short data;

	for (i = 0; i < 6; i++)	
		{
			reset_and_select_srom(dev);
			data = read_srom(dev, i + EnetAddressOffset/2, SROMAddressBits);
			ea[2*i]   = bitrev(data & 0x0ff);
			ea[2*i+1] = bitrev((data >> 8) & 0x0ff);
		}
}

static void bmac_reset_and_enable(struct net_device *dev)
{
	struct bmac_data *bp = dev->priv;
	unsigned long flags;
	struct sk_buff *skb;
	unsigned char *data;

	save_flags(flags); cli();
	bmac_enable_and_reset_chip(dev);
	bmac_init_tx_ring(bp);
	bmac_init_rx_ring(bp);
	bmac_init_chip(dev);
	bmac_start_chip(dev);
	bmwrite(dev, INTDISABLE, EnableNormal);
	bp->sleeping = 0;
	
	/*
	 * It seems that the bmac can't receive until it's transmitted
	 * a packet.  So we give it a dummy packet to transmit.
	 */
	skb = dev_alloc_skb(ETHERMINPACKET);
	if (skb != NULL) {
		data = skb_put(skb, ETHERMINPACKET);
		memset(data, 0, ETHERMINPACKET);
		memcpy(data, dev->dev_addr, 6);
		memcpy(data+6, dev->dev_addr, 6);
		bmac_transmit_packet(skb, dev);
	}
	restore_flags(flags);
}

static int __init bmac_probe(void)
{
	struct device_node *bmac;

	MOD_INC_USE_COUNT;

	for (bmac = find_devices("bmac"); bmac != 0; bmac = bmac->next)
		bmac_probe1(bmac, 0);
	for (bmac = find_compatible_devices("network", "bmac+"); bmac != 0;
	     bmac = bmac->next)
		bmac_probe1(bmac, 1);

	if (bmac_devs != 0) {
		proc_net_create ("bmac", 0, bmac_proc_info);
#ifdef CONFIG_PMAC_PBOOK
		pmu_register_sleep_notifier(&bmac_sleep_notifier);
#endif
	}

	MOD_DEC_USE_COUNT;

	return bmac_devs? 0: -ENODEV;
}

static void __init bmac_probe1(struct device_node *bmac, int is_bmac_plus)
{
	int j, rev, ret;
	struct bmac_data *bp;
	unsigned char *addr;
	struct net_device *dev;
	u32 *deviceid;

	if (bmac->n_addrs != 3 || bmac->n_intrs != 3) {
		printk(KERN_ERR "can't use BMAC %s: need 3 addrs and 3 intrs\n",
		       bmac->full_name);
		return;
	}
	addr = get_property(bmac, "mac-address", NULL);
	if (addr == NULL) {
		addr = get_property(bmac, "local-mac-address", NULL);
		if (addr == NULL) {
			printk(KERN_ERR "Can't get mac-address for BMAC %s\n",
			       bmac->full_name);
			return;
		}
	}

	if (bmac_emergency_rxbuf == NULL) {
		bmac_emergency_rxbuf = kmalloc(RX_BUFLEN, GFP_KERNEL);
		if (bmac_emergency_rxbuf == NULL) {
			printk(KERN_ERR "BMAC: can't allocate emergency RX buffer\n");
			return;
		}
	}

	dev = init_etherdev(NULL, PRIV_BYTES);
	if (!dev) {
		printk(KERN_ERR "init_etherdev failed, out of memory for BMAC %s\n",
		       bmac->full_name);
		return;
	}
	bp = (struct bmac_data *) dev->priv;
	SET_MODULE_OWNER(dev);
	bp->node = bmac;

	if (!request_OF_resource(bmac, 0, " (bmac)")) {
		printk(KERN_ERR "BMAC: can't request IO resource !\n");
		goto err_out;
	}
	if (!request_OF_resource(bmac, 1, " (bmac tx dma)")) {
		printk(KERN_ERR "BMAC: can't request TX DMA resource !\n");
		goto err_out;
	}

	if (!request_OF_resource(bmac, 2, " (bmac rx dma)")) {
		printk(KERN_ERR "BMAC: can't request RX DMA resource !\n");
		goto err_out;
	}
	dev->base_addr = (unsigned long)
		ioremap(bmac->addrs[0].address, bmac->addrs[0].size);
	if (!dev->base_addr)
		goto err_out;
	dev->irq = bmac->intrs[0].line;

	deviceid = (u32 *)get_property(bmac, "device-id", NULL);
	if (deviceid)
		bp->device_id = *deviceid;

	bmac_enable_and_reset_chip(dev);
	bmwrite(dev, INTDISABLE, DisableAll);

	printk(KERN_INFO "%s: BMAC%s at", dev->name, (is_bmac_plus? "+": ""));
	rev = addr[0] == 0 && addr[1] == 0xA0;
	for (j = 0; j < 6; ++j) {
		dev->dev_addr[j] = rev? bitrev(addr[j]): addr[j];
		printk("%c%.2x", (j? ':': ' '), dev->dev_addr[j]);
	}
	XXDEBUG((", base_addr=%#0lx", dev->base_addr));
	printk("\n");

	/* Enable chip without interrupts for now */
	bmac_enable_and_reset_chip(dev);
	bmwrite(dev, INTDISABLE, DisableAll);

	dev->open = bmac_open;
	dev->stop = bmac_close;
	dev->hard_start_xmit = bmac_output;
	dev->get_stats = bmac_stats;
	dev->set_multicast_list = bmac_set_multicast;
	dev->set_mac_address = bmac_set_address;
	dev->do_ioctl = bmac_do_ioctl;

	bmac_get_station_address(dev, addr);
	if (bmac_verify_checksum(dev) != 0)
		goto err_out_iounmap;

	bp->is_bmac_plus = is_bmac_plus;
	bp->tx_dma = (volatile struct dbdma_regs *)
		ioremap(bmac->addrs[1].address, bmac->addrs[1].size);
	if (!bp->tx_dma)
		goto err_out_iounmap;
	bp->tx_dma_intr = bmac->intrs[1].line;
	bp->rx_dma = (volatile struct dbdma_regs *)
		ioremap(bmac->addrs[2].address, bmac->addrs[2].size);
	if (!bp->rx_dma)
		goto err_out_iounmap_tx;
	bp->rx_dma_intr = bmac->intrs[2].line;

	bp->tx_cmds = (volatile struct dbdma_cmd *) DBDMA_ALIGN(bp + 1);
	bp->rx_cmds = bp->tx_cmds + N_TX_RING + 1;

	bp->queue = (struct sk_buff_head *)(bp->rx_cmds + N_RX_RING + 1);
	skb_queue_head_init(bp->queue);

	init_timer(&bp->tx_timeout);
	/*     bp->timeout_active = 0; */

	ret = request_irq(dev->irq, bmac_misc_intr, 0, "BMAC-misc", dev);
	if (ret) {
		printk(KERN_ERR "BMAC: can't get irq %d\n", dev->irq);
		goto err_out_iounmap_rx;
	}
	ret = request_irq(bmac->intrs[1].line, bmac_txdma_intr, 0, "BMAC-txdma", dev);
	if (ret) {
		printk(KERN_ERR "BMAC: can't get irq %d\n", bmac->intrs[1].line);
		goto err_out_irq0;
	}
	ret = request_irq(bmac->intrs[2].line, bmac_rxdma_intr, 0, "BMAC-rxdma", dev);
	if (ret) {
		printk(KERN_ERR "BMAC: can't get irq %d\n", bmac->intrs[2].line);
		goto err_out_irq1;
	}

	/* Mask chip interrupts and disable chip, will be
	 * re-enabled on open()
	 */
	disable_irq(dev->irq);
	pmac_call_feature(PMAC_FTR_BMAC_ENABLE, bp->node, 0, 0);
	
	bp->next_bmac = bmac_devs;
	bmac_devs = dev;
	return;

err_out_irq1:
	free_irq(bmac->intrs[1].line, dev);
err_out_irq0:
	free_irq(dev->irq, dev);
err_out_iounmap_rx:
	iounmap((void *)bp->rx_dma);
err_out_iounmap_tx:
	iounmap((void *)bp->tx_dma);
err_out_iounmap:
	iounmap((void *)dev->base_addr);
err_out:
	if (bp->node) {
		release_OF_resource(bp->node, 0);
		release_OF_resource(bp->node, 1);
		release_OF_resource(bp->node, 2);
		pmac_call_feature(PMAC_FTR_BMAC_ENABLE, bp->node, 0, 0);
	}
	unregister_netdev(dev);
	kfree(dev);
}

static int bmac_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	u32 ethcmd;

	if (get_user(ethcmd, (u32 *)useraddr))
		return -EFAULT;

	switch (ethcmd) {
	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
		strcpy (info.driver, "bmac");
		info.version[0] = '\0';
		snprintf(info.fw_version, 31, "chip id %x", bp->device_id);
		if (copy_to_user (useraddr, &info, sizeof (info)))
			return -EFAULT;
		return 0;
	}

	case ETHTOOL_GSET:
	case ETHTOOL_SSET:
	case ETHTOOL_NWAY_RST:
	case ETHTOOL_GLINK:
	case ETHTOOL_GMSGLVL:
	case ETHTOOL_SMSGLVL:
	default:
		;
	}

	return -EOPNOTSUPP;
}

static int bmac_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	switch(cmd) {
	case SIOCETHTOOL:
		return bmac_ethtool_ioctl(dev, (void *) ifr->ifr_data);

	case SIOCGMIIPHY:
	case SIOCDEVPRIVATE:
	case SIOCGMIIREG:
	case SIOCDEVPRIVATE+1:
	case SIOCSMIIREG:
	case SIOCDEVPRIVATE+2:
	default:
		;
	}
	return -EOPNOTSUPP;
}

static int bmac_open(struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	/* XXDEBUG(("bmac: enter open\n")); */
	/* reset the chip */
	bp->opened = 1;
	bmac_reset_and_enable(dev);
	enable_irq(dev->irq);
	dev->flags |= IFF_RUNNING;
	return 0;
}

static int bmac_close(struct net_device *dev)
{
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_regs *rd = bp->rx_dma;
	volatile struct dbdma_regs *td = bp->tx_dma;
	unsigned short config;
	int i;

	bp->sleeping = 1;
	dev->flags &= ~(IFF_UP | IFF_RUNNING);

	/* disable rx and tx */
	config = bmread(dev, RXCFG);
	bmwrite(dev, RXCFG, (config & ~RxMACEnable));

	config = bmread(dev, TXCFG);
	bmwrite(dev, TXCFG, (config & ~TxMACEnable));

	bmwrite(dev, INTDISABLE, DisableAll); /* disable all intrs */

	/* disable rx and tx dma */
	st_le32(&rd->control, DBDMA_CLEAR(RUN|PAUSE|FLUSH|WAKE));	/* clear run bit */
	st_le32(&td->control, DBDMA_CLEAR(RUN|PAUSE|FLUSH|WAKE));	/* clear run bit */

	/* free some skb's */
	XXDEBUG(("bmac: free rx bufs\n"));
	for (i=0; i<N_RX_RING; i++) {
		if (bp->rx_bufs[i] != NULL) {
			dev_kfree_skb(bp->rx_bufs[i]);
			bp->rx_bufs[i] = NULL;
		}
	}
	XXDEBUG(("bmac: free tx bufs\n"));
	for (i = 0; i<N_TX_RING; i++) {
		if (bp->tx_bufs[i] != NULL) {
			dev_kfree_skb(bp->tx_bufs[i]);
			bp->tx_bufs[i] = NULL;
		}
	}
	XXDEBUG(("bmac: all bufs freed\n"));

	bp->opened = 0;
	disable_irq(dev->irq);
	pmac_call_feature(PMAC_FTR_BMAC_ENABLE, bp->node, 0, 0);

	return 0;
}

static void
bmac_start(struct net_device *dev)
{
	struct bmac_data *bp = dev->priv;
	int i;
	struct sk_buff *skb;
	unsigned long flags;

	if (bp->sleeping)
		return;
		
	save_flags(flags); cli();
	while (1) {
		i = bp->tx_fill + 1;
		if (i >= N_TX_RING)
			i = 0;
		if (i == bp->tx_empty)
			break;
		skb = skb_dequeue(bp->queue);
		if (skb == NULL)
			break;
		bmac_transmit_packet(skb, dev);
	}
	restore_flags(flags);
}

static int
bmac_output(struct sk_buff *skb, struct net_device *dev)
{
	struct bmac_data *bp = dev->priv;
	skb_queue_tail(bp->queue, skb);
	bmac_start(dev);
	return 0;
}

static void bmac_tx_timeout(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	struct bmac_data *bp = (struct bmac_data *) dev->priv;
	volatile struct dbdma_regs *td = bp->tx_dma;
	volatile struct dbdma_regs *rd = bp->rx_dma;
	volatile struct dbdma_cmd *cp;
	unsigned long flags;
	unsigned short config, oldConfig;
	int i;

	XXDEBUG(("bmac: tx_timeout called\n"));
	save_flags(flags); cli();
	bp->timeout_active = 0;

	/* update various counters */
/*     	bmac_handle_misc_intrs(bp, 0); */

	cp = &bp->tx_cmds[bp->tx_empty];
/*	XXDEBUG((KERN_DEBUG "bmac: tx dmastat=%x %x runt=%d pr=%x fs=%x fc=%x\n", */
/* 	   ld_le32(&td->status), ld_le16(&cp->xfer_status), bp->tx_bad_runt, */
/* 	   mb->pr, mb->xmtfs, mb->fifofc)); */

	/* turn off both tx and rx and reset the chip */
	config = bmread(dev, RXCFG);
	bmwrite(dev, RXCFG, (config & ~RxMACEnable));
	config = bmread(dev, TXCFG);
	bmwrite(dev, TXCFG, (config & ~TxMACEnable));
	out_le32(&td->control, DBDMA_CLEAR(RUN|PAUSE|FLUSH|WAKE|ACTIVE|DEAD));
	printk(KERN_ERR "bmac: transmit timeout - resetting\n");
	bmac_enable_and_reset_chip(dev);

	/* restart rx dma */
	cp = bus_to_virt(ld_le32(&rd->cmdptr));
	out_le32(&rd->control, DBDMA_CLEAR(RUN|PAUSE|FLUSH|WAKE|ACTIVE|DEAD));
	out_le16(&cp->xfer_status, 0);
	out_le32(&rd->cmdptr, virt_to_bus(cp));
	out_le32(&rd->control, DBDMA_SET(RUN|WAKE));

	/* fix up the transmit side */
	XXDEBUG((KERN_DEBUG "bmac: tx empty=%d fill=%d fullup=%d\n",
		 bp->tx_empty, bp->tx_fill, bp->tx_fullup));
	i = bp->tx_empty;
	++bp->stats.tx_errors;
	if (i != bp->tx_fill) {
		dev_kfree_skb(bp->tx_bufs[i]);
		bp->tx_bufs[i] = NULL;
		if (++i >= N_TX_RING) i = 0;
		bp->tx_empty = i;
	}
	bp->tx_fullup = 0;
	netif_wake_queue(dev);
	if (i != bp->tx_fill) {
		cp = &bp->tx_cmds[i];
		out_le16(&cp->xfer_status, 0);
		out_le16(&cp->command, OUTPUT_LAST);
		out_le32(&td->cmdptr, virt_to_bus(cp));
		out_le32(&td->control, DBDMA_SET(RUN));
		/* 	bmac_set_timeout(dev); */
		XXDEBUG((KERN_DEBUG "bmac: starting %d\n", i));
	}

	/* turn it back on */
	oldConfig = bmread(dev, RXCFG);		
	bmwrite(dev, RXCFG, oldConfig | RxMACEnable );
	oldConfig = bmread(dev, TXCFG);		
	bmwrite(dev, TXCFG, oldConfig | TxMACEnable );

	restore_flags(flags);
}

#if 0
static void dump_dbdma(volatile struct dbdma_cmd *cp,int count)
{
	int i,*ip;
	
	for (i=0;i< count;i++) {
		ip = (int*)(cp+i);
	
		printk("dbdma req 0x%x addr 0x%x baddr 0x%x xfer/res 0x%x\n",
		       ld_le32(ip+0),
		       ld_le32(ip+1),
		       ld_le32(ip+2),
		       ld_le32(ip+3));
	}

}
#endif

static int
bmac_proc_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	off_t pos   = 0;
	off_t begin = 0;
	int i;

	if (bmac_devs == NULL)
		return (-ENOSYS);

	len += sprintf(buffer, "BMAC counters & registers\n");

	for (i = 0; i<N_REG_ENTRIES; i++) {
		len += sprintf(buffer + len, "%s: %#08x\n",
			       reg_entries[i].name,
			       bmread(bmac_devs, reg_entries[i].reg_offset));
		pos = begin + len;

		if (pos < offset) {
			len = 0;
			begin = pos;
		}

		if (pos > offset+length) break;
	}

	*start = buffer + (offset - begin);
	len -= (offset - begin);

	if (len > length) len = length;

	return len;
}


MODULE_AUTHOR("Randy Gobbel/Paul Mackerras");
MODULE_DESCRIPTION("PowerMac BMAC ethernet driver.");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;

static void __exit bmac_cleanup (void)
{
	struct bmac_data *bp;
	struct net_device *dev;

	if (bmac_emergency_rxbuf != NULL) {
		kfree(bmac_emergency_rxbuf);
		bmac_emergency_rxbuf = NULL;
	}

	if (bmac_devs == 0)
		return;
#ifdef CONFIG_PMAC_PBOOK
	pmu_unregister_sleep_notifier(&bmac_sleep_notifier);
#endif
	proc_net_remove("bmac");

	do {
		dev = bmac_devs;
		bp = (struct bmac_data *) dev->priv;
		bmac_devs = bp->next_bmac;

		unregister_netdev(dev);

		release_OF_resource(bp->node, 0);
		release_OF_resource(bp->node, 1);
		release_OF_resource(bp->node, 2);
		free_irq(dev->irq, dev);
		free_irq(bp->tx_dma_intr, dev);
		free_irq(bp->rx_dma_intr, dev);

		kfree(dev);
	} while (bmac_devs != NULL);
}

module_init(bmac_probe);
module_exit(bmac_cleanup);
