/* $Id: b1dma.c,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 * 
 * Common module for AVM B1 cards that support dma with AMCC
 * 
 * Copyright 2000 by Carsten Paeth <calle@calle.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>
#include <asm/io.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include "capilli.h"
#include "avmcard.h"
#include "capicmd.h"
#include "capiutil.h"

static char *revision = "$Revision: 1.1.4.1 $";

/* ------------------------------------------------------------- */

MODULE_DESCRIPTION("CAPI4Linux: DMA support for active AVM cards");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");

static int suppress_pollack = 0;
MODULE_PARM(suppress_pollack, "0-1i");

/* ------------------------------------------------------------- */

static void b1dma_dispatch_tx(avmcard *card);

/* ------------------------------------------------------------- */

/* S5933 */

#define	AMCC_RXPTR	0x24
#define	AMCC_RXLEN	0x28
#define	AMCC_TXPTR	0x2c
#define	AMCC_TXLEN	0x30

#define	AMCC_INTCSR	0x38
#	define EN_READ_TC_INT		0x00008000L
#	define EN_WRITE_TC_INT		0x00004000L
#	define EN_TX_TC_INT		EN_READ_TC_INT
#	define EN_RX_TC_INT		EN_WRITE_TC_INT
#	define AVM_FLAG			0x30000000L

#	define ANY_S5933_INT		0x00800000L
#	define	READ_TC_INT		0x00080000L
#	define WRITE_TC_INT		0x00040000L
#	define	TX_TC_INT		READ_TC_INT
#	define	RX_TC_INT		WRITE_TC_INT
#	define MASTER_ABORT_INT		0x00100000L
#	define TARGET_ABORT_INT		0x00200000L
#	define BUS_MASTER_INT		0x00200000L
#	define ALL_INT			0x000C0000L

#define	AMCC_MCSR	0x3c
#	define A2P_HI_PRIORITY		0x00000100L
#	define EN_A2P_TRANSFERS		0x00000400L
#	define P2A_HI_PRIORITY		0x00001000L
#	define EN_P2A_TRANSFERS		0x00004000L
#	define RESET_A2P_FLAGS		0x04000000L
#	define RESET_P2A_FLAGS		0x02000000L

/* ------------------------------------------------------------- */

#define b1dmaoutmeml(addr, value)	writel(value, addr)
#define b1dmainmeml(addr)	readl(addr)
#define b1dmaoutmemw(addr, value)	writew(value, addr)
#define b1dmainmemw(addr)	readw(addr)
#define b1dmaoutmemb(addr, value)	writeb(value, addr)
#define b1dmainmemb(addr)	readb(addr)

/* ------------------------------------------------------------- */

static inline int b1dma_tx_empty(unsigned int port)
{
	return inb(port + 0x03) & 0x1;
}

static inline int b1dma_rx_full(unsigned int port)
{
	return inb(port + 0x02) & 0x1;
}

static int b1dma_tolink(avmcard *card, void *buf, unsigned int len)
{
	unsigned long stop = jiffies + 1 * HZ;	/* maximum wait time 1 sec */
	unsigned char *s = (unsigned char *)buf;
	while (len--) {
		while (   !b1dma_tx_empty(card->port)
		       && time_before(jiffies, stop));
		if (!b1dma_tx_empty(card->port)) 
			return -1;
	        t1outp(card->port, 0x01, *s++);
	}
	return 0;
}

static int b1dma_fromlink(avmcard *card, void *buf, unsigned int len)
{
	unsigned long stop = jiffies + 1 * HZ;	/* maximum wait time 1 sec */
	unsigned char *s = (unsigned char *)buf;
	while (len--) {
		while (   !b1dma_rx_full(card->port)
		       && time_before(jiffies, stop));
		if (!b1dma_rx_full(card->port)) 
			return -1;
	        *s++ = t1inp(card->port, 0x00);
	}
	return 0;
}

static int WriteReg(avmcard *card, __u32 reg, __u8 val)
{
	__u8 cmd = 0x00;
	if (   b1dma_tolink(card, &cmd, 1) == 0
	    && b1dma_tolink(card, &reg, 4) == 0) {
		__u32 tmp = val;
		return b1dma_tolink(card, &tmp, 4);
	}
	return -1;
}

static __u8 ReadReg(avmcard *card, __u32 reg)
{
	__u8 cmd = 0x01;
	if (   b1dma_tolink(card, &cmd, 1) == 0
	    && b1dma_tolink(card, &reg, 4) == 0) {
		__u32 tmp;
		if (b1dma_fromlink(card, &tmp, 4) == 0)
			return (__u8)tmp;
	}
	return 0xff;
}

/* ------------------------------------------------------------- */

static inline void _put_byte(void **pp, __u8 val)
{
	__u8 *s = *pp;
	*s++ = val;
	*pp = s;
}

static inline void _put_word(void **pp, __u32 val)
{
	__u8 *s = *pp;
	*s++ = val & 0xff;
	*s++ = (val >> 8) & 0xff;
	*s++ = (val >> 16) & 0xff;
	*s++ = (val >> 24) & 0xff;
	*pp = s;
}

static inline void _put_slice(void **pp, unsigned char *dp, unsigned int len)
{
	unsigned i = len;
	_put_word(pp, i);
	while (i-- > 0)
		_put_byte(pp, *dp++);
}

static inline __u8 _get_byte(void **pp)
{
	__u8 *s = *pp;
	__u8 val;
	val = *s++;
	*pp = s;
	return val;
}

static inline __u32 _get_word(void **pp)
{
	__u8 *s = *pp;
	__u32 val;
	val = *s++;
	val |= (*s++ << 8);
	val |= (*s++ << 16);
	val |= (*s++ << 24);
	*pp = s;
	return val;
}

static inline __u32 _get_slice(void **pp, unsigned char *dp)
{
	unsigned int len, i;

	len = i = _get_word(pp);
	while (i-- > 0) *dp++ = _get_byte(pp);
	return len;
}

/* ------------------------------------------------------------- */

void b1dma_reset(avmcard *card)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	card->csr = 0x0;
	b1dmaoutmeml(card->mbase+AMCC_INTCSR, card->csr);
	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0);
	b1dmaoutmeml(card->mbase+AMCC_RXLEN, 0);
	b1dmaoutmeml(card->mbase+AMCC_TXLEN, 0);

	t1outp(card->port, 0x10, 0x00);
	t1outp(card->port, 0x07, 0x00);

	restore_flags(flags);

	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0);
	mdelay(10);
	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0x0f000000); /* reset all */
	mdelay(10);
	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0);
	if (card->cardtype == avm_t1pci)
		mdelay(42);
	else
		mdelay(10);
}

/* ------------------------------------------------------------- */

int b1dma_detect(avmcard *card)
{
	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0);
	mdelay(10);
	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0x0f000000); /* reset all */
	mdelay(10);
	b1dmaoutmeml(card->mbase+AMCC_MCSR, 0);
	mdelay(42);

	b1dmaoutmeml(card->mbase+AMCC_RXLEN, 0);
	b1dmaoutmeml(card->mbase+AMCC_TXLEN, 0);
	card->csr = 0x0;
	b1dmaoutmeml(card->mbase+AMCC_INTCSR, card->csr);

	if (b1dmainmeml(card->mbase+AMCC_MCSR) != 0x000000E6)
		return 1;

	b1dmaoutmeml(card->mbase+AMCC_RXPTR, 0xffffffff);
	b1dmaoutmeml(card->mbase+AMCC_TXPTR, 0xffffffff);
	if (   b1dmainmeml(card->mbase+AMCC_RXPTR) != 0xfffffffc
	    || b1dmainmeml(card->mbase+AMCC_TXPTR) != 0xfffffffc)
		return 2;

	b1dmaoutmeml(card->mbase+AMCC_RXPTR, 0x0);
	b1dmaoutmeml(card->mbase+AMCC_TXPTR, 0x0);
	if (   b1dmainmeml(card->mbase+AMCC_RXPTR) != 0x0
	    || b1dmainmeml(card->mbase+AMCC_TXPTR) != 0x0)
		return 3;

	t1outp(card->port, 0x10, 0x00);
	t1outp(card->port, 0x07, 0x00);
	
	t1outp(card->port, 0x02, 0x02);
	t1outp(card->port, 0x03, 0x02);

	if (   (t1inp(card->port, 0x02) & 0xFE) != 0x02
	    || t1inp(card->port, 0x3) != 0x03)
		return 4;

	t1outp(card->port, 0x02, 0x00);
	t1outp(card->port, 0x03, 0x00);

	if (   (t1inp(card->port, 0x02) & 0xFE) != 0x00
	    || t1inp(card->port, 0x3) != 0x01)
		return 5;

	return 0;
}

int t1pci_detect(avmcard *card)
{
	int ret;

	if ((ret = b1dma_detect(card)) != 0)
		return ret;
	
	/* Transputer test */
	
	if (   WriteReg(card, 0x80001000, 0x11) != 0
	    || WriteReg(card, 0x80101000, 0x22) != 0
	    || WriteReg(card, 0x80201000, 0x33) != 0
	    || WriteReg(card, 0x80301000, 0x44) != 0)
		return 6;

	if (   ReadReg(card, 0x80001000) != 0x11
	    || ReadReg(card, 0x80101000) != 0x22
	    || ReadReg(card, 0x80201000) != 0x33
	    || ReadReg(card, 0x80301000) != 0x44)
		return 7;

	if (   WriteReg(card, 0x80001000, 0x55) != 0
	    || WriteReg(card, 0x80101000, 0x66) != 0
	    || WriteReg(card, 0x80201000, 0x77) != 0
	    || WriteReg(card, 0x80301000, 0x88) != 0)
		return 8;

	if (   ReadReg(card, 0x80001000) != 0x55
	    || ReadReg(card, 0x80101000) != 0x66
	    || ReadReg(card, 0x80201000) != 0x77
	    || ReadReg(card, 0x80301000) != 0x88)
		return 9;

	return 0;
}

int b1pciv4_detect(avmcard *card)
{
	int ret, i;

	if ((ret = b1dma_detect(card)) != 0)
		return ret;
	
	for (i=0; i < 5 ; i++) {
		if (WriteReg(card, 0x80A00000, 0x21) != 0)
			return 6;
		if ((ReadReg(card, 0x80A00000) & 0x01) != 0x01)
			return 7;
	}
	for (i=0; i < 5 ; i++) {
		if (WriteReg(card, 0x80A00000, 0x20) != 0)
			return 8;
		if ((ReadReg(card, 0x80A00000) & 0x01) != 0x00)
			return 9;
	}
	
	return 0;
}

/* ------------------------------------------------------------- */

static void b1dma_dispatch_tx(avmcard *card)
{
	avmcard_dmainfo *dma = card->dma;
	unsigned long flags;
	struct sk_buff *skb;
	__u8 cmd, subcmd;
	__u16 len;
	__u32 txlen;
	int inint;
	void *p;
	
	save_flags(flags);
	cli();

	inint = card->interrupt;

	if (card->csr & EN_TX_TC_INT) { /* tx busy */
	        restore_flags(flags);
		return;
	}

	skb = skb_dequeue(&dma->send_queue);
	if (!skb) {
#ifdef CONFIG_B1DMA_DEBUG
		printk(KERN_DEBUG "tx(%d): underrun\n", inint);
#endif
	        restore_flags(flags);
		return;
	}

	len = CAPIMSG_LEN(skb->data);

	if (len) {
		cmd = CAPIMSG_COMMAND(skb->data);
		subcmd = CAPIMSG_SUBCOMMAND(skb->data);

		p = dma->sendbuf;

		if (CAPICMD(cmd, subcmd) == CAPI_DATA_B3_REQ) {
			__u16 dlen = CAPIMSG_DATALEN(skb->data);
			_put_byte(&p, SEND_DATA_B3_REQ);
			_put_slice(&p, skb->data, len);
			_put_slice(&p, skb->data + len, dlen);
		} else {
			_put_byte(&p, SEND_MESSAGE);
			_put_slice(&p, skb->data, len);
		}
		txlen = (__u8 *)p - (__u8 *)dma->sendbuf;
#ifdef CONFIG_B1DMA_DEBUG
		printk(KERN_DEBUG "tx(%d): put msg len=%d\n",
				inint, txlen);
#endif
	} else {
		txlen = skb->len-2;
#ifdef CONFIG_B1DMA_POLLDEBUG
		if (skb->data[2] == SEND_POLLACK)
			printk(KERN_INFO "%s: send ack\n", card->name);
#endif
#ifdef CONFIG_B1DMA_DEBUG
		printk(KERN_DEBUG "tx(%d): put 0x%x len=%d\n",
				inint, skb->data[2], txlen);
#endif
		memcpy(dma->sendbuf, skb->data+2, skb->len-2);
	}
	txlen = (txlen + 3) & ~3;

	b1dmaoutmeml(card->mbase+AMCC_TXPTR, virt_to_phys(dma->sendbuf));
	b1dmaoutmeml(card->mbase+AMCC_TXLEN, txlen);

	card->csr |= EN_TX_TC_INT;

	if (!inint)
		b1dmaoutmeml(card->mbase+AMCC_INTCSR, card->csr);

	restore_flags(flags);
	dev_kfree_skb_any(skb);
}

/* ------------------------------------------------------------- */

static void queue_pollack(avmcard *card)
{
	struct sk_buff *skb;
	void *p;

	skb = alloc_skb(3, GFP_ATOMIC);
	if (!skb) {
		printk(KERN_CRIT "%s: no memory, lost poll ack\n",
					card->name);
		return;
	}
	p = skb->data;
	_put_byte(&p, 0);
	_put_byte(&p, 0);
	_put_byte(&p, SEND_POLLACK);
	skb_put(skb, (__u8 *)p - (__u8 *)skb->data);

	skb_queue_tail(&card->dma->send_queue, skb);
	b1dma_dispatch_tx(card);
}

/* ------------------------------------------------------------- */

static void b1dma_handle_rx(avmcard *card)
{
	avmctrl_info *cinfo = &card->ctrlinfo[0];
	avmcard_dmainfo *dma = card->dma;
	struct capi_ctr *ctrl = cinfo->capi_ctrl;
	struct sk_buff *skb;
	void *p = dma->recvbuf+4;
	__u32 ApplId, MsgLen, DataB3Len, NCCI, WindowSize;
	__u8 b1cmd =  _get_byte(&p);

#ifdef CONFIG_B1DMA_DEBUG
	printk(KERN_DEBUG "rx: 0x%x %lu\n", b1cmd, (unsigned long)dma->recvlen);
#endif
	
	switch (b1cmd) {
	case RECEIVE_DATA_B3_IND:

		ApplId = (unsigned) _get_word(&p);
		MsgLen = _get_slice(&p, card->msgbuf);
		DataB3Len = _get_slice(&p, card->databuf);

		if (MsgLen < 30) { /* not CAPI 64Bit */
			memset(card->msgbuf+MsgLen, 0, 30-MsgLen);
			MsgLen = 30;
			CAPIMSG_SETLEN(card->msgbuf, 30);
		}
		if (!(skb = alloc_skb(DataB3Len+MsgLen, GFP_ATOMIC))) {
			printk(KERN_ERR "%s: incoming packet dropped\n",
					card->name);
		} else {
			memcpy(skb_put(skb, MsgLen), card->msgbuf, MsgLen);
			memcpy(skb_put(skb, DataB3Len), card->databuf, DataB3Len);
			ctrl->handle_capimsg(ctrl, ApplId, skb);
		}
		break;

	case RECEIVE_MESSAGE:

		ApplId = (unsigned) _get_word(&p);
		MsgLen = _get_slice(&p, card->msgbuf);
		if (!(skb = alloc_skb(MsgLen, GFP_ATOMIC))) {
			printk(KERN_ERR "%s: incoming packet dropped\n",
					card->name);
		} else {
			memcpy(skb_put(skb, MsgLen), card->msgbuf, MsgLen);
			ctrl->handle_capimsg(ctrl, ApplId, skb);
		}
		break;

	case RECEIVE_NEW_NCCI:

		ApplId = _get_word(&p);
		NCCI = _get_word(&p);
		WindowSize = _get_word(&p);

		ctrl->new_ncci(ctrl, ApplId, NCCI, WindowSize);

		break;

	case RECEIVE_FREE_NCCI:

		ApplId = _get_word(&p);
		NCCI = _get_word(&p);

		if (NCCI != 0xffffffff)
			ctrl->free_ncci(ctrl, ApplId, NCCI);
		else ctrl->appl_released(ctrl, ApplId);
		break;

	case RECEIVE_START:
#ifdef CONFIG_B1DMA_POLLDEBUG
		printk(KERN_INFO "%s: receive poll\n", card->name);
#endif
		if (!suppress_pollack)
			queue_pollack(card);
		ctrl->resume_output(ctrl);
		break;

	case RECEIVE_STOP:
		ctrl->suspend_output(ctrl);
		break;

	case RECEIVE_INIT:

		cinfo->versionlen = _get_slice(&p, cinfo->versionbuf);
		b1_parse_version(cinfo);
		printk(KERN_INFO "%s: %s-card (%s) now active\n",
		       card->name,
		       cinfo->version[VER_CARDTYPE],
		       cinfo->version[VER_DRIVER]);
		ctrl->ready(ctrl);
		break;

	case RECEIVE_TASK_READY:
		ApplId = (unsigned) _get_word(&p);
		MsgLen = _get_slice(&p, card->msgbuf);
		card->msgbuf[MsgLen] = 0;
		while (    MsgLen > 0
		       && (   card->msgbuf[MsgLen-1] == '\n'
			   || card->msgbuf[MsgLen-1] == '\r')) {
			card->msgbuf[MsgLen-1] = 0;
			MsgLen--;
		}
		printk(KERN_INFO "%s: task %d \"%s\" ready.\n",
				card->name, ApplId, card->msgbuf);
		break;

	case RECEIVE_DEBUGMSG:
		MsgLen = _get_slice(&p, card->msgbuf);
		card->msgbuf[MsgLen] = 0;
		while (    MsgLen > 0
		       && (   card->msgbuf[MsgLen-1] == '\n'
			   || card->msgbuf[MsgLen-1] == '\r')) {
			card->msgbuf[MsgLen-1] = 0;
			MsgLen--;
		}
		printk(KERN_INFO "%s: DEBUG: %s\n", card->name, card->msgbuf);
		break;

	default:
		printk(KERN_ERR "%s: b1dma_interrupt: 0x%x ???\n",
				card->name, b1cmd);
		return;
	}
}

/* ------------------------------------------------------------- */

static void b1dma_handle_interrupt(avmcard *card)
{
	__u32 status = b1dmainmeml(card->mbase+AMCC_INTCSR);
	__u32 newcsr;

	if ((status & ANY_S5933_INT) == 0) 
		return;

        newcsr = card->csr | (status & ALL_INT);
	if (status & TX_TC_INT) newcsr &= ~EN_TX_TC_INT;
	if (status & RX_TC_INT) newcsr &= ~EN_RX_TC_INT;
	b1dmaoutmeml(card->mbase+AMCC_INTCSR, newcsr);

	if ((status & RX_TC_INT) != 0) {
		__u8 *recvbuf = card->dma->recvbuf;
		__u32 rxlen;
	   	if (card->dma->recvlen == 0) {
			card->dma->recvlen = *((__u32 *)recvbuf);
			rxlen = (card->dma->recvlen + 3) & ~3;
			b1dmaoutmeml(card->mbase+AMCC_RXPTR,
					virt_to_phys(recvbuf+4));
			b1dmaoutmeml(card->mbase+AMCC_RXLEN, rxlen);
		} else {
			b1dma_handle_rx(card);
	   		card->dma->recvlen = 0;
			b1dmaoutmeml(card->mbase+AMCC_RXPTR, virt_to_phys(recvbuf));
			b1dmaoutmeml(card->mbase+AMCC_RXLEN, 4);
		}
	}

	if ((status & TX_TC_INT) != 0) {
		card->csr &= ~EN_TX_TC_INT;
	        b1dma_dispatch_tx(card);
	}
	b1dmaoutmeml(card->mbase+AMCC_INTCSR, card->csr);
}

void b1dma_interrupt(int interrupt, void *devptr, struct pt_regs *regs)
{
	avmcard *card;

	card = (avmcard *) devptr;

	if (!card) {
		printk(KERN_WARNING "b1dma: interrupt: wrong device\n");
		return;
	}
	if (card->interrupt) {
		printk(KERN_ERR "%s: reentering interrupt hander\n", card->name);
		return;
	}

	card->interrupt = 1;

	b1dma_handle_interrupt(card);

	card->interrupt = 0;
}

/* ------------------------------------------------------------- */

static int b1dma_loaded(avmcard *card)
{
	unsigned long stop;
	unsigned char ans;
	unsigned long tout = 2;
	unsigned int base = card->port;

	for (stop = jiffies + tout * HZ; time_before(jiffies, stop);) {
		if (b1_tx_empty(base))
			break;
	}
	if (!b1_tx_empty(base)) {
		printk(KERN_ERR "%s: b1dma_loaded: tx err, corrupted t4 file ?\n",
				card->name);
		return 0;
	}
	b1_put_byte(base, SEND_POLLACK);
	for (stop = jiffies + tout * HZ; time_before(jiffies, stop);) {
		if (b1_rx_full(base)) {
			if ((ans = b1_get_byte(base)) == RECEIVE_POLLDWORD) {
				return 1;
			}
			printk(KERN_ERR "%s: b1dma_loaded: got 0x%x, firmware not running in dword mode\n", card->name, ans);
			return 0;
		}
	}
	printk(KERN_ERR "%s: b1dma_loaded: firmware not running\n", card->name);
	return 0;
}

/* ------------------------------------------------------------- */

static void b1dma_send_init(avmcard *card)
{
	struct sk_buff *skb;
	void *p;

	skb = alloc_skb(15, GFP_ATOMIC);
	if (!skb) {
		printk(KERN_CRIT "%s: no memory, lost register appl.\n",
					card->name);
		return;
	}
	p = skb->data;
	_put_byte(&p, 0);
	_put_byte(&p, 0);
	_put_byte(&p, SEND_INIT);
	_put_word(&p, CAPI_MAXAPPL);
	_put_word(&p, AVM_NCCI_PER_CHANNEL*30);
	_put_word(&p, card->cardnr - 1);
	skb_put(skb, (__u8 *)p - (__u8 *)skb->data);

	skb_queue_tail(&card->dma->send_queue, skb);
	b1dma_dispatch_tx(card);
}

int b1dma_load_firmware(struct capi_ctr *ctrl, capiloaddata *data)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	unsigned long flags;
	int retval;

	b1dma_reset(card);

	if ((retval = b1_load_t4file(card, &data->firmware))) {
		b1dma_reset(card);
		printk(KERN_ERR "%s: failed to load t4file!!\n",
					card->name);
		return retval;
	}

	if (data->configuration.len > 0 && data->configuration.data) {
		if ((retval = b1_load_config(card, &data->configuration))) {
			b1dma_reset(card);
			printk(KERN_ERR "%s: failed to load config!!\n",
					card->name);
			return retval;
		}
	}

	if (!b1dma_loaded(card)) {
		b1dma_reset(card);
		printk(KERN_ERR "%s: failed to load t4file.\n", card->name);
		return -EIO;
	}

	save_flags(flags);
	cli();

	card->csr = AVM_FLAG;
	b1dmaoutmeml(card->mbase+AMCC_INTCSR, card->csr);
	b1dmaoutmeml(card->mbase+AMCC_MCSR,
		EN_A2P_TRANSFERS|EN_P2A_TRANSFERS
		|A2P_HI_PRIORITY|P2A_HI_PRIORITY
		|RESET_A2P_FLAGS|RESET_P2A_FLAGS);
	t1outp(card->port, 0x07, 0x30);
	t1outp(card->port, 0x10, 0xF0);

	card->dma->recvlen = 0;
	b1dmaoutmeml(card->mbase+AMCC_RXPTR, virt_to_phys(card->dma->recvbuf));
	b1dmaoutmeml(card->mbase+AMCC_RXLEN, 4);
	card->csr |= EN_RX_TC_INT;
	b1dmaoutmeml(card->mbase+AMCC_INTCSR, card->csr);
	restore_flags(flags);

        b1dma_send_init(card);

	return 0;
}

void b1dma_reset_ctr(struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;

 	b1dma_reset(card);

	memset(cinfo->version, 0, sizeof(cinfo->version));
	ctrl->reseted(ctrl);
}


/* ------------------------------------------------------------- */


void b1dma_register_appl(struct capi_ctr *ctrl,
				__u16 appl,
				capi_register_params *rp)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	struct sk_buff *skb;
	int want = rp->level3cnt;
	int nconn;
	void *p;

	if (want > 0) nconn = want;
	else nconn = ctrl->profile.nbchannel * -want;
	if (nconn == 0) nconn = ctrl->profile.nbchannel;

	skb = alloc_skb(23, GFP_ATOMIC);
	if (!skb) {
		printk(KERN_CRIT "%s: no memory, lost register appl.\n",
					card->name);
		return;
	}
	p = skb->data;
	_put_byte(&p, 0);
	_put_byte(&p, 0);
	_put_byte(&p, SEND_REGISTER);
	_put_word(&p, appl);
	_put_word(&p, 1024 * (nconn+1));
	_put_word(&p, nconn);
	_put_word(&p, rp->datablkcnt);
	_put_word(&p, rp->datablklen);
	skb_put(skb, (__u8 *)p - (__u8 *)skb->data);

	skb_queue_tail(&card->dma->send_queue, skb);
	b1dma_dispatch_tx(card);

	ctrl->appl_registered(ctrl, appl);
}

/* ------------------------------------------------------------- */

void b1dma_release_appl(struct capi_ctr *ctrl, __u16 appl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	struct sk_buff *skb;
	void *p;

	skb = alloc_skb(7, GFP_ATOMIC);
	if (!skb) {
		printk(KERN_CRIT "%s: no memory, lost release appl.\n",
					card->name);
		return;
	}
	p = skb->data;
	_put_byte(&p, 0);
	_put_byte(&p, 0);
	_put_byte(&p, SEND_RELEASE);
	_put_word(&p, appl);

	skb_put(skb, (__u8 *)p - (__u8 *)skb->data);
	skb_queue_tail(&card->dma->send_queue, skb);
	b1dma_dispatch_tx(card);
}

/* ------------------------------------------------------------- */

void b1dma_send_message(struct capi_ctr *ctrl, struct sk_buff *skb)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	skb_queue_tail(&card->dma->send_queue, skb);
	b1dma_dispatch_tx(card);
}

/* ------------------------------------------------------------- */

int b1dmactl_read_proc(char *page, char **start, off_t off,
        		int count, int *eof, struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	unsigned long flags;
	__u8 flag;
	int len = 0;
	char *s;
	u_long txaddr, txlen, rxaddr, rxlen, csr;

	len += sprintf(page+len, "%-16s %s\n", "name", card->name);
	len += sprintf(page+len, "%-16s 0x%x\n", "io", card->port);
	len += sprintf(page+len, "%-16s %d\n", "irq", card->irq);
	len += sprintf(page+len, "%-16s 0x%lx\n", "membase", card->membase);
	switch (card->cardtype) {
	case avm_b1isa: s = "B1 ISA"; break;
	case avm_b1pci: s = "B1 PCI"; break;
	case avm_b1pcmcia: s = "B1 PCMCIA"; break;
	case avm_m1: s = "M1"; break;
	case avm_m2: s = "M2"; break;
	case avm_t1isa: s = "T1 ISA (HEMA)"; break;
	case avm_t1pci: s = "T1 PCI"; break;
	case avm_c4: s = "C4"; break;
	case avm_c2: s = "C2"; break;
	default: s = "???"; break;
	}
	len += sprintf(page+len, "%-16s %s\n", "type", s);
	if ((s = cinfo->version[VER_DRIVER]) != 0)
	   len += sprintf(page+len, "%-16s %s\n", "ver_driver", s);
	if ((s = cinfo->version[VER_CARDTYPE]) != 0)
	   len += sprintf(page+len, "%-16s %s\n", "ver_cardtype", s);
	if ((s = cinfo->version[VER_SERIAL]) != 0)
	   len += sprintf(page+len, "%-16s %s\n", "ver_serial", s);

	if (card->cardtype != avm_m1) {
        	flag = ((__u8 *)(ctrl->profile.manu))[3];
        	if (flag)
			len += sprintf(page+len, "%-16s%s%s%s%s%s%s%s\n",
			"protocol",
			(flag & 0x01) ? " DSS1" : "",
			(flag & 0x02) ? " CT1" : "",
			(flag & 0x04) ? " VN3" : "",
			(flag & 0x08) ? " NI1" : "",
			(flag & 0x10) ? " AUSTEL" : "",
			(flag & 0x20) ? " ESS" : "",
			(flag & 0x40) ? " 1TR6" : ""
			);
	}
	if (card->cardtype != avm_m1) {
        	flag = ((__u8 *)(ctrl->profile.manu))[5];
		if (flag)
			len += sprintf(page+len, "%-16s%s%s%s%s\n",
			"linetype",
			(flag & 0x01) ? " point to point" : "",
			(flag & 0x02) ? " point to multipoint" : "",
			(flag & 0x08) ? " leased line without D-channel" : "",
			(flag & 0x04) ? " leased line with D-channel" : ""
			);
	}
	len += sprintf(page+len, "%-16s %s\n", "cardname", cinfo->cardname);

	save_flags(flags);
	cli();

	txaddr = (u_long)phys_to_virt(b1dmainmeml(card->mbase+0x2c));
	txaddr -= (u_long)card->dma->sendbuf;
	txlen  = b1dmainmeml(card->mbase+0x30);

	rxaddr = (u_long)phys_to_virt(b1dmainmeml(card->mbase+0x24));
	rxaddr -= (u_long)card->dma->recvbuf;
	rxlen  = b1dmainmeml(card->mbase+0x28);

	csr  = b1dmainmeml(card->mbase+AMCC_INTCSR);

	restore_flags(flags);

        len += sprintf(page+len, "%-16s 0x%lx\n",
				"csr (cached)", (unsigned long)card->csr);
        len += sprintf(page+len, "%-16s 0x%lx\n",
				"csr", (unsigned long)csr);
        len += sprintf(page+len, "%-16s %lu\n",
				"txoff", (unsigned long)txaddr);
        len += sprintf(page+len, "%-16s %lu\n",
				"txlen", (unsigned long)txlen);
        len += sprintf(page+len, "%-16s %lu\n",
				"rxoff", (unsigned long)rxaddr);
        len += sprintf(page+len, "%-16s %lu\n",
				"rxlen", (unsigned long)rxlen);

	if (off+count >= len)
	   *eof = 1;
	if (len < off)
           return 0;
	*start = page + off;
	return ((count < len-off) ? count : len-off);
}

/* ------------------------------------------------------------- */

EXPORT_SYMBOL(b1dma_reset);
EXPORT_SYMBOL(t1pci_detect);
EXPORT_SYMBOL(b1pciv4_detect);
EXPORT_SYMBOL(b1dma_interrupt);

EXPORT_SYMBOL(b1dma_load_firmware);
EXPORT_SYMBOL(b1dma_reset_ctr);
EXPORT_SYMBOL(b1dma_register_appl);
EXPORT_SYMBOL(b1dma_release_appl);
EXPORT_SYMBOL(b1dma_send_message);
EXPORT_SYMBOL(b1dmactl_read_proc);

int b1dma_init(void)
{
	char *p;
	char rev[32];

	if ((p = strchr(revision, ':')) != 0 && p[1]) {
		strncpy(rev, p + 2, sizeof(rev));
		rev[sizeof(rev)-1] = 0;
		if ((p = strchr(rev, '$')) != 0 && p > rev)
		   *(p-1) = 0;
	} else
		strcpy(rev, "1.0");

	printk(KERN_INFO "b1dma: revision %s\n", rev);

	return 0;
}

void b1dma_exit(void)
{
}

module_init(b1dma_init);
module_exit(b1dma_exit);
