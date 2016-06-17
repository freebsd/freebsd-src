

/*
 *	sm_sbc.c  -- soundcard radio modem driver soundblaster hardware driver
 *
 *	Copyright (C) 1996  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/ioport.h>
#include <linux/soundmodem.h>
#include <linux/delay.h>
#include "sm.h"
#include "smdma.h"

/* --------------------------------------------------------------------- */

/*
 * currently this module is supposed to support both module styles, i.e.
 * the old one present up to about 2.1.9, and the new one functioning
 * starting with 2.1.21. The reason is I have a kit allowing to compile
 * this module also under 2.0.x which was requested by several people.
 * This will go in 2.2
 */
#include <linux/version.h>

#include <asm/uaccess.h>

/* --------------------------------------------------------------------- */

struct sc_state_sbc {
	unsigned char revhi, revlo;
	unsigned char fmt[2];
	unsigned int sr[2];
};

#define SCSTATE ((struct sc_state_sbc *)(&sm->hw))

/* --------------------------------------------------------------------- */
/* 
 * the sbc converter's registers 
 */
#define DSP_RESET(iobase)        (iobase+0x6)
#define DSP_READ_DATA(iobase)    (iobase+0xa)
#define DSP_WRITE_DATA(iobase)   (iobase+0xc)
#define DSP_WRITE_STATUS(iobase) (iobase+0xc)
#define DSP_DATA_AVAIL(iobase)   (iobase+0xe)
#define DSP_MIXER_ADDR(iobase)   (iobase+0x4)
#define DSP_MIXER_DATA(iobase)   (iobase+0x5)
#define DSP_INTACK_16BIT(iobase) (iobase+0xf)
#define SBC_EXTENT               16

/* --------------------------------------------------------------------- */
/*
 * SBC commands
 */
#define SBC_OUTPUT             0x14
#define SBC_INPUT              0x24
#define SBC_BLOCKSIZE          0x48
#define SBC_HI_OUTPUT          0x91 
#define SBC_HI_INPUT           0x99 
#define SBC_LO_OUTPUT_AUTOINIT 0x1c
#define SBC_LO_INPUT_AUTOINIT  0x2c
#define SBC_HI_OUTPUT_AUTOINIT 0x90 
#define SBC_HI_INPUT_AUTOINIT  0x98
#define SBC_IMMED_INT          0xf2
#define SBC_GET_REVISION       0xe1
#define ESS_GET_REVISION       0xe7
#define SBC_SPEAKER_ON         0xd1
#define SBC_SPEAKER_OFF        0xd3
#define SBC_DMA_ON             0xd0
#define SBC_DMA_OFF            0xd4
#define SBC_SAMPLE_RATE        0x40
#define SBC_SAMPLE_RATE_OUT    0x41
#define SBC_SAMPLE_RATE_IN     0x42
#define SBC_MONO_8BIT          0xa0
#define SBC_MONO_16BIT         0xa4
#define SBC_STEREO_8BIT        0xa8
#define SBC_STEREO_16BIT       0xac

#define SBC4_OUT8_AI           0xc6
#define SBC4_IN8_AI            0xce
#define SBC4_MODE_UNS_MONO     0x00
#define SBC4_MODE_SIGN_MONO    0x10

#define SBC4_OUT16_AI          0xb6
#define SBC4_IN16_AI           0xbe

/* --------------------------------------------------------------------- */

static int inline reset_dsp(struct net_device *dev)
{
	int i;

	outb(1, DSP_RESET(dev->base_addr));
	udelay(300);
	outb(0, DSP_RESET(dev->base_addr));
	for (i = 0; i < 0xffff; i++)
		if (inb(DSP_DATA_AVAIL(dev->base_addr)) & 0x80)
			if (inb(DSP_READ_DATA(dev->base_addr)) == 0xaa)
				return 1;
	return 0;
}

/* --------------------------------------------------------------------- */

static void inline write_dsp(struct net_device *dev, unsigned char data)
{
	int i;
	
	for (i = 0; i < 0xffff; i++)
		if (!(inb(DSP_WRITE_STATUS(dev->base_addr)) & 0x80)) {
			outb(data, DSP_WRITE_DATA(dev->base_addr));
			return;
		}
}

/* --------------------------------------------------------------------- */

static int inline read_dsp(struct net_device *dev, unsigned char *data)
{
	int i;

	if (!data)
		return 0;
	for (i = 0; i < 0xffff; i++) 
		if (inb(DSP_DATA_AVAIL(dev->base_addr)) & 0x80) {
			*data = inb(DSP_READ_DATA(dev->base_addr));
			return 1;
		}
	return 0;
}

/* --------------------------------------------------------------------- */

static int config_resources(struct net_device *dev, struct sm_state *sm, int fdx)
{
	unsigned char irqreg = 0, dmareg = 0, realirq, realdma;
	unsigned long flags;

	switch (dev->irq) {
	case 2:
	case 9:
		irqreg |= 0x01;
		break;

	case 5:
		irqreg |= 0x02;
		break;

	case 7:
		irqreg |= 0x04;
		break;

	case 10:
		irqreg |= 0x08;
		break;
		
	default:
		return -ENODEV;
	}

	switch (dev->dma) {
	case 0:
		dmareg |= 0x01;
		break;

	case 1:
		dmareg |= 0x02;
		break;

	case 3:
		dmareg |= 0x08;
		break;

	default:
		return -ENODEV;
	}
		
	if (fdx) {
		switch (sm->hdrv.ptt_out.dma2) {
		case 5:
			dmareg |= 0x20;
			break;
			
		case 6:
			dmareg |= 0x40;
			break;
			
		case 7:
			dmareg |= 0x80;
			break;
			
		default:
			return -ENODEV;
		}
	}
	save_flags(flags);
	cli();
	outb(0x80, DSP_MIXER_ADDR(dev->base_addr));
	outb(irqreg, DSP_MIXER_DATA(dev->base_addr));
	realirq = inb(DSP_MIXER_DATA(dev->base_addr));
	outb(0x81, DSP_MIXER_ADDR(dev->base_addr));
	outb(dmareg, DSP_MIXER_DATA(dev->base_addr));
	realdma = inb(DSP_MIXER_DATA(dev->base_addr));
	restore_flags(flags);
	if ((~realirq) & irqreg || (~realdma) & dmareg) {
		printk(KERN_ERR "%s: sbc resource registers cannot be set; PnP device "
		       "and IRQ/DMA specified wrongly?\n", sm_drvname);
		return -EINVAL;
	}
	return 0;
}

/* --------------------------------------------------------------------- */

static void inline sbc_int_ack_8bit(struct net_device *dev)
{
	inb(DSP_DATA_AVAIL(dev->base_addr));
}

/* --------------------------------------------------------------------- */

static void inline sbc_int_ack_16bit(struct net_device *dev)
{
	inb(DSP_INTACK_16BIT(dev->base_addr));
}

/* --------------------------------------------------------------------- */

static void setup_dma_dsp(struct net_device *dev, struct sm_state *sm, int send)
{
        unsigned long flags;
        static const unsigned char sbcmode[2][2] = {
		{ SBC_LO_INPUT_AUTOINIT, SBC_LO_OUTPUT_AUTOINIT }, 
		{ SBC_HI_INPUT_AUTOINIT, SBC_HI_OUTPUT_AUTOINIT }
	};
	static const unsigned char sbc4mode[2] = { SBC4_IN8_AI, SBC4_OUT8_AI };
	static const unsigned char sbcskr[2] = { SBC_SPEAKER_OFF, SBC_SPEAKER_ON };
	unsigned int nsamps;

	send = !!send;
        if (!reset_dsp(dev)) {
                printk(KERN_ERR "%s: sbc: cannot reset sb dsp\n", sm_drvname);
                return;
        }
        save_flags(flags);
        cli();
        sbc_int_ack_8bit(dev);
        write_dsp(dev, SBC_SAMPLE_RATE); /* set sampling rate */
        write_dsp(dev, SCSTATE->fmt[send]);
        write_dsp(dev, sbcskr[send]); 
	nsamps = dma_setup(sm, send, dev->dma) - 1;
        sbc_int_ack_8bit(dev);
	if (SCSTATE->revhi >= 4) {
		write_dsp(dev, sbc4mode[send]);
		write_dsp(dev, SBC4_MODE_UNS_MONO);
		write_dsp(dev, nsamps & 0xff);
		write_dsp(dev, nsamps >> 8);
	} else {
		write_dsp(dev, SBC_BLOCKSIZE);
		write_dsp(dev, nsamps & 0xff);
		write_dsp(dev, nsamps >> 8);
		write_dsp(dev, sbcmode[SCSTATE->fmt[send] >= 180][send]);
		/* hispeed mode if sample rate > 13kHz */
	}
        restore_flags(flags);
}

/* --------------------------------------------------------------------- */

static void sbc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct sm_state *sm = (struct sm_state *)dev->priv;
	unsigned int curfrag;

	if (!dev || !sm || sm->hdrv.magic != HDLCDRV_MAGIC)
		return;
	cli();
 	sbc_int_ack_8bit(dev);
	disable_dma(dev->dma);
	clear_dma_ff(dev->dma);
	dma_ptr(sm, sm->dma.ptt_cnt > 0, dev->dma, &curfrag);
	enable_dma(dev->dma);
	sm_int_freq(sm);
	sti();
	if (sm->dma.ptt_cnt <= 0) {
		dma_receive(sm, curfrag);
		hdlcdrv_arbitrate(dev, &sm->hdrv);
		if (hdlcdrv_ptt(&sm->hdrv)) {
			/* starting to transmit */
			disable_dma(dev->dma);
			hdlcdrv_transmitter(dev, &sm->hdrv); /* prefill HDLC buffer */
			dma_start_transmit(sm);
			setup_dma_dsp(dev, sm, 1);
			dma_transmit(sm);
		}
	} else if (dma_end_transmit(sm, curfrag)) {
		/* stopping transmission */
		disable_dma(dev->dma);
		sti();
		dma_init_receive(sm);
		setup_dma_dsp(dev, sm, 0);
        } else
		dma_transmit(sm);
	sm_output_status(sm);
	hdlcdrv_transmitter(dev, &sm->hdrv);
	hdlcdrv_receiver(dev, &sm->hdrv);

}

/* --------------------------------------------------------------------- */

static int sbc_open(struct net_device *dev, struct sm_state *sm) 
{
	int err;
	unsigned int dmasz, u;

	if (sizeof(sm->m) < sizeof(struct sc_state_sbc)) {
		printk(KERN_ERR "sm sbc: sbc state too big: %d > %d\n", 
		       sizeof(struct sc_state_sbc), sizeof(sm->m));
		return -ENODEV;
	}
	if (!dev || !sm)
		return -ENXIO;
	if (dev->base_addr <= 0 || dev->base_addr > 0x1000-SBC_EXTENT || 
	    dev->irq < 2 || dev->irq > 15 || dev->dma > 3)
		return -ENXIO;
	if (check_region(dev->base_addr, SBC_EXTENT))
		return -EACCES;
	/*
	 * check if a card is available
	 */
	if (!reset_dsp(dev)) {
		printk(KERN_ERR "%s: sbc: no card at io address 0x%lx\n",
		       sm_drvname, dev->base_addr);
		return -ENODEV;
	}
	write_dsp(dev, SBC_GET_REVISION);
	if (!read_dsp(dev, &SCSTATE->revhi) || 
	    !read_dsp(dev, &SCSTATE->revlo))
		return -ENODEV;
	printk(KERN_INFO "%s: SoundBlaster DSP revision %d.%d\n", sm_drvname, 
	       SCSTATE->revhi, SCSTATE->revlo);
	if (SCSTATE->revhi < 2) {
		printk(KERN_ERR "%s: your card is an antiquity, at least DSP "
		       "rev 2.00 required\n", sm_drvname);
		return -ENODEV;
	}
	if (SCSTATE->revhi < 3 && 
	    (SCSTATE->fmt[0] >= 180 || SCSTATE->fmt[1] >= 180)) {
		printk(KERN_ERR "%s: sbc io 0x%lx: DSP rev %d.%02d too "
		       "old, at least 3.00 required\n", sm_drvname,
		       dev->base_addr, SCSTATE->revhi, SCSTATE->revlo);
		return -ENODEV;
	}
	if (SCSTATE->revhi >= 4 && 
	    (err = config_resources(dev, sm, 0))) {
		printk(KERN_ERR "%s: invalid IRQ and/or DMA specified\n", sm_drvname);
		return err;
	}
	/*
	 * initialize some variables
	 */
	dma_init_receive(sm);
	dmasz = (NUM_FRAGMENTS + 1) * sm->dma.ifragsz;
	u = NUM_FRAGMENTS * sm->dma.ofragsz;
	if (u > dmasz)
		dmasz = u;
	if (!(sm->dma.ibuf = sm->dma.obuf = kmalloc(dmasz, GFP_KERNEL | GFP_DMA)))
		return -ENOMEM;
	dma_init_transmit(sm);
	dma_init_receive(sm);

	memset(&sm->m, 0, sizeof(sm->m));
	memset(&sm->d, 0, sizeof(sm->d));
	if (sm->mode_tx->init)
		sm->mode_tx->init(sm);
	if (sm->mode_rx->init)
		sm->mode_rx->init(sm);

	if (request_dma(dev->dma, sm->hwdrv->hw_name)) {
		kfree(sm->dma.obuf);
		return -EBUSY;
	}
	if (request_irq(dev->irq, sbc_interrupt, SA_INTERRUPT, 
			sm->hwdrv->hw_name, dev)) {
		free_dma(dev->dma);
		kfree(sm->dma.obuf);
		return -EBUSY;
	}
	request_region(dev->base_addr, SBC_EXTENT, sm->hwdrv->hw_name);
	setup_dma_dsp(dev, sm, 0);
	return 0;
}

/* --------------------------------------------------------------------- */

static int sbc_close(struct net_device *dev, struct sm_state *sm) 
{
	if (!dev || !sm)
		return -EINVAL;
	/*
	 * disable interrupts
	 */
	disable_dma(dev->dma);
	reset_dsp(dev);	
	free_irq(dev->irq, dev);	
	free_dma(dev->dma);	
	release_region(dev->base_addr, SBC_EXTENT);
	kfree(sm->dma.obuf);
	return 0;
}

/* --------------------------------------------------------------------- */

static int sbc_sethw(struct net_device *dev, struct sm_state *sm, char *mode)
{
	char *cp = strchr(mode, '.');
	const struct modem_tx_info **mtp = sm_modem_tx_table;
	const struct modem_rx_info **mrp;

	if (!strcmp(mode, "off")) {
		sm->mode_tx = NULL;
		sm->mode_rx = NULL;
		return 0;
	}
	if (cp)
		*cp++ = '\0';
	else
		cp = mode;
	for (; *mtp; mtp++) {
		if ((*mtp)->loc_storage > sizeof(sm->m)) {
			printk(KERN_ERR "%s: insufficient storage for modulator %s (%d)\n",
			       sm_drvname, (*mtp)->name, (*mtp)->loc_storage);
			continue;
		}
		if (!(*mtp)->name || strcmp((*mtp)->name, mode))
			continue;
		if ((*mtp)->srate < 5000 || (*mtp)->srate > 44100)
			continue;
		if (!(*mtp)->modulator_u8)
			continue;
		for (mrp = sm_modem_rx_table; *mrp; mrp++) {
			if ((*mrp)->loc_storage > sizeof(sm->d)) {
				printk(KERN_ERR "%s: insufficient storage for demodulator %s (%d)\n",
				       sm_drvname, (*mrp)->name, (*mrp)->loc_storage);
				continue;
			}
			if (!(*mrp)->demodulator_u8)
				continue;
			if ((*mrp)->name && !strcmp((*mrp)->name, cp) &&
			    (*mrp)->srate >= 5000 && (*mrp)->srate <= 44100) {
				sm->mode_tx = *mtp;
				sm->mode_rx = *mrp;
				SCSTATE->fmt[0] = 256-((1000000L+sm->mode_rx->srate/2)/
							 sm->mode_rx->srate);
				SCSTATE->fmt[1] = 256-((1000000L+sm->mode_tx->srate/2)/
							 sm->mode_tx->srate);
				sm->dma.ifragsz = (sm->mode_rx->srate + 50)/100;
				sm->dma.ofragsz = (sm->mode_tx->srate + 50)/100;
				if (sm->dma.ifragsz < sm->mode_rx->overlap)
					sm->dma.ifragsz = sm->mode_rx->overlap;
				sm->dma.i16bit = sm->dma.o16bit = 0;
				return 0;
			}
		}
	}
	return -EINVAL;
}

/* --------------------------------------------------------------------- */

static int sbc_ioctl(struct net_device *dev, struct sm_state *sm, struct ifreq *ifr, 
		     struct hdlcdrv_ioctl *hi, int cmd)
{
	struct sm_ioctl bi;
	unsigned long flags;
	int i;
	
	if (cmd != SIOCDEVPRIVATE)
		return -ENOIOCTLCMD;

	if (hi->cmd == HDLCDRVCTL_MODEMPARMASK)
		return HDLCDRV_PARMASK_IOBASE | HDLCDRV_PARMASK_IRQ | 
			HDLCDRV_PARMASK_DMA | HDLCDRV_PARMASK_SERIOBASE | 
			HDLCDRV_PARMASK_PARIOBASE | HDLCDRV_PARMASK_MIDIIOBASE;

	if (copy_from_user(&bi, ifr->ifr_data, sizeof(bi)))
		return -EFAULT;

	switch (bi.cmd) {
	default:
		return -ENOIOCTLCMD;

	case SMCTL_GETMIXER:
		i = 0;
		bi.data.mix.sample_rate = sm->mode_rx->srate;
		bi.data.mix.bit_rate = sm->hdrv.par.bitrate;
		bi.data.mix.mixer_type = SM_MIXER_INVALID;
		switch (SCSTATE->revhi) {
		case 2:
			bi.data.mix.mixer_type = SM_MIXER_CT1335;
			break;
		case 3:
			bi.data.mix.mixer_type = SM_MIXER_CT1345;
			break;
		case 4:
			bi.data.mix.mixer_type = SM_MIXER_CT1745;
			break;
		}
		if (bi.data.mix.mixer_type != SM_MIXER_INVALID &&
		    bi.data.mix.reg < 0x80) {
			save_flags(flags);
			cli();
			outb(bi.data.mix.reg, DSP_MIXER_ADDR(dev->base_addr));
			bi.data.mix.data = inb(DSP_MIXER_DATA(dev->base_addr));
			restore_flags(flags);
			i = 1;
		}
		if (copy_to_user(ifr->ifr_data, &bi, sizeof(bi)))
			return -EFAULT;
		return i;
		
	case SMCTL_SETMIXER:
		if (!capable(CAP_SYS_RAWIO))
			return -EACCES;
		switch (SCSTATE->revhi) {
		case 2:
			if (bi.data.mix.mixer_type != SM_MIXER_CT1335)
				return -EINVAL;
			break;
		case 3:
			if (bi.data.mix.mixer_type != SM_MIXER_CT1345)
				return -EINVAL;
			break;
		case 4:
			if (bi.data.mix.mixer_type != SM_MIXER_CT1745)
				return -EINVAL;
			break;
		default:
			return -ENODEV;
		}
		if (bi.data.mix.reg >= 0x80)
			return -EACCES;
		save_flags(flags);
		cli();
		outb(bi.data.mix.reg, DSP_MIXER_ADDR(dev->base_addr));
		outb(bi.data.mix.data, DSP_MIXER_DATA(dev->base_addr));
		restore_flags(flags);
		return 0;
		
	}
	if (copy_to_user(ifr->ifr_data, &bi, sizeof(bi)))
		return -EFAULT;
	return 0;

}

/* --------------------------------------------------------------------- */

const struct hardware_info sm_hw_sbc = {
	"sbc", sizeof(struct sc_state_sbc), 
	sbc_open, sbc_close, sbc_ioctl, sbc_sethw
};

/* --------------------------------------------------------------------- */

static void setup_dma_fdx_dsp(struct net_device *dev, struct sm_state *sm)
{
        unsigned long flags;
	unsigned int isamps, osamps;

        if (!reset_dsp(dev)) {
                printk(KERN_ERR "%s: sbc: cannot reset sb dsp\n", sm_drvname);
                return;
        }
        save_flags(flags);
        cli();
        sbc_int_ack_8bit(dev);
        sbc_int_ack_16bit(dev);
	/* should eventually change to set rates individually by SBC_SAMPLE_RATE_{IN/OUT} */
	write_dsp(dev, SBC_SAMPLE_RATE_IN);
	write_dsp(dev, SCSTATE->sr[0] >> 8);
	write_dsp(dev, SCSTATE->sr[0] & 0xff);
	write_dsp(dev, SBC_SAMPLE_RATE_OUT);
	write_dsp(dev, SCSTATE->sr[1] >> 8);
	write_dsp(dev, SCSTATE->sr[1] & 0xff);
        write_dsp(dev, SBC_SPEAKER_ON);
	if (sm->dma.o16bit) {
		/*
		 * DMA channel 1 (8bit) does input (capture),
		 * DMA channel 2 (16bit) does output (playback)
		 */
		isamps = dma_setup(sm, 0, dev->dma) - 1;
		osamps = dma_setup(sm, 1, sm->hdrv.ptt_out.dma2) - 1;
		sbc_int_ack_8bit(dev);
		sbc_int_ack_16bit(dev);
		write_dsp(dev, SBC4_IN8_AI);
		write_dsp(dev, SBC4_MODE_UNS_MONO);
		write_dsp(dev, isamps & 0xff);
		write_dsp(dev, isamps >> 8);
		write_dsp(dev, SBC4_OUT16_AI);
		write_dsp(dev, SBC4_MODE_SIGN_MONO);
		write_dsp(dev, osamps & 0xff);
		write_dsp(dev, osamps >> 8);
	} else {
		/*
		 * DMA channel 1 (8bit) does output (playback),
		 * DMA channel 2 (16bit) does input (capture)
		 */
		isamps = dma_setup(sm, 0, sm->hdrv.ptt_out.dma2) - 1;
		osamps = dma_setup(sm, 1, dev->dma) - 1;
		sbc_int_ack_8bit(dev);
		sbc_int_ack_16bit(dev);
		write_dsp(dev, SBC4_OUT8_AI);
		write_dsp(dev, SBC4_MODE_UNS_MONO);
		write_dsp(dev, osamps & 0xff);
		write_dsp(dev, osamps >> 8);
		write_dsp(dev, SBC4_IN16_AI);
		write_dsp(dev, SBC4_MODE_SIGN_MONO);
		write_dsp(dev, isamps & 0xff);
		write_dsp(dev, isamps >> 8);
	}
	dma_init_receive(sm);
	dma_init_transmit(sm);
        restore_flags(flags);
}

/* --------------------------------------------------------------------- */

static void sbcfdx_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct sm_state *sm = (struct sm_state *)dev->priv;
	unsigned char intsrc, pbint = 0, captint = 0;
	unsigned int ocfrag, icfrag;
	unsigned long flags;

	if (!dev || !sm || sm->hdrv.magic != HDLCDRV_MAGIC)
		return;
	save_flags(flags);
	cli();
	outb(0x82, DSP_MIXER_ADDR(dev->base_addr));
	intsrc = inb(DSP_MIXER_DATA(dev->base_addr));
	if (intsrc & 0x01) {
		sbc_int_ack_8bit(dev);
		if (sm->dma.o16bit) {
			captint = 1;
			disable_dma(dev->dma);
			clear_dma_ff(dev->dma);
			dma_ptr(sm, 0, dev->dma, &icfrag);
			enable_dma(dev->dma);
		} else {     
			pbint = 1;
			disable_dma(dev->dma);
			clear_dma_ff(dev->dma);
			dma_ptr(sm, 1, dev->dma, &ocfrag);
			enable_dma(dev->dma);
		}
	}
	if (intsrc & 0x02) {
		sbc_int_ack_16bit(dev);
		if (sm->dma.o16bit) {
			pbint = 1;
			disable_dma(sm->hdrv.ptt_out.dma2);
			clear_dma_ff(sm->hdrv.ptt_out.dma2);
			dma_ptr(sm, 1, sm->hdrv.ptt_out.dma2, &ocfrag);
			enable_dma(sm->hdrv.ptt_out.dma2);
		} else {
			captint = 1;
			disable_dma(sm->hdrv.ptt_out.dma2);
			clear_dma_ff(sm->hdrv.ptt_out.dma2);
			dma_ptr(sm, 0, sm->hdrv.ptt_out.dma2, &icfrag);
			enable_dma(sm->hdrv.ptt_out.dma2);
		}
	}
	restore_flags(flags);
	sm_int_freq(sm);
	sti();
	if (pbint) {
		if (dma_end_transmit(sm, ocfrag))
			dma_clear_transmit(sm);
		dma_transmit(sm);
	}
	if (captint) { 
		dma_receive(sm, icfrag);
		hdlcdrv_arbitrate(dev, &sm->hdrv);
	}
	sm_output_status(sm);
	hdlcdrv_transmitter(dev, &sm->hdrv);
	hdlcdrv_receiver(dev, &sm->hdrv);
}

/* --------------------------------------------------------------------- */

static int sbcfdx_open(struct net_device *dev, struct sm_state *sm) 
{
	int err;

	if (sizeof(sm->m) < sizeof(struct sc_state_sbc)) {
		printk(KERN_ERR "sm sbc: sbc state too big: %d > %d\n", 
		       sizeof(struct sc_state_sbc), sizeof(sm->m));
		return -ENODEV;
	}
	if (!dev || !sm)
		return -ENXIO;
	if (dev->base_addr <= 0 || dev->base_addr > 0x1000-SBC_EXTENT || 
	    dev->irq < 2 || dev->irq > 15 || dev->dma > 3)
		return -ENXIO;
	if (check_region(dev->base_addr, SBC_EXTENT))
		return -EACCES;
	/*
	 * check if a card is available
	 */
	if (!reset_dsp(dev)) {
		printk(KERN_ERR "%s: sbc: no card at io address 0x%lx\n",
		       sm_drvname, dev->base_addr);
		return -ENODEV;
	}
	write_dsp(dev, SBC_GET_REVISION);
	if (!read_dsp(dev, &SCSTATE->revhi) || 
	    !read_dsp(dev, &SCSTATE->revlo))
		return -ENODEV;
	printk(KERN_INFO "%s: SoundBlaster DSP revision %d.%d\n", sm_drvname, 
	       SCSTATE->revhi, SCSTATE->revlo);
	if (SCSTATE->revhi < 4) {
		printk(KERN_ERR "%s: at least DSP rev 4.00 required\n", sm_drvname);
		return -ENODEV;
	}
	if ((err = config_resources(dev, sm, 1))) {
		printk(KERN_ERR "%s: invalid IRQ and/or DMA specified\n", sm_drvname);
		return err;
	}
	/*
	 * initialize some variables
	 */
	if (!(sm->dma.ibuf = kmalloc(sm->dma.ifragsz * (NUM_FRAGMENTS+1), GFP_KERNEL | GFP_DMA)))
		return -ENOMEM;
	if (!(sm->dma.obuf = kmalloc(sm->dma.ofragsz * NUM_FRAGMENTS, GFP_KERNEL | GFP_DMA))) {
		kfree(sm->dma.ibuf);
		return -ENOMEM;
	}
	dma_init_transmit(sm);
	dma_init_receive(sm);

	memset(&sm->m, 0, sizeof(sm->m));
	memset(&sm->d, 0, sizeof(sm->d));
	if (sm->mode_tx->init)
		sm->mode_tx->init(sm);
	if (sm->mode_rx->init)
		sm->mode_rx->init(sm);

	if (request_dma(dev->dma, sm->hwdrv->hw_name)) {
		kfree(sm->dma.ibuf);
		kfree(sm->dma.obuf);
		return -EBUSY;
	}
	if (request_dma(sm->hdrv.ptt_out.dma2, sm->hwdrv->hw_name)) {
		kfree(sm->dma.ibuf);
		kfree(sm->dma.obuf);
		free_dma(dev->dma);
		return -EBUSY;
	}
	if (request_irq(dev->irq, sbcfdx_interrupt, SA_INTERRUPT, 
			sm->hwdrv->hw_name, dev)) {
		kfree(sm->dma.ibuf);
		kfree(sm->dma.obuf);
		free_dma(dev->dma);
		free_dma(sm->hdrv.ptt_out.dma2);
		return -EBUSY;
	}
	request_region(dev->base_addr, SBC_EXTENT, sm->hwdrv->hw_name);
	setup_dma_fdx_dsp(dev, sm);
	return 0;
}

/* --------------------------------------------------------------------- */

static int sbcfdx_close(struct net_device *dev, struct sm_state *sm) 
{
	if (!dev || !sm)
		return -EINVAL;
	/*
	 * disable interrupts
	 */
	disable_dma(dev->dma);
	disable_dma(sm->hdrv.ptt_out.dma2);
	reset_dsp(dev);	
	free_irq(dev->irq, dev);	
	free_dma(dev->dma);	
	free_dma(sm->hdrv.ptt_out.dma2);	
	release_region(dev->base_addr, SBC_EXTENT);
	kfree(sm->dma.ibuf);
	kfree(sm->dma.obuf);
	return 0;
}

/* --------------------------------------------------------------------- */

static int sbcfdx_sethw(struct net_device *dev, struct sm_state *sm, char *mode)
{
	char *cp = strchr(mode, '.');
	const struct modem_tx_info **mtp = sm_modem_tx_table;
	const struct modem_rx_info **mrp;

	if (!strcmp(mode, "off")) {
		sm->mode_tx = NULL;
		sm->mode_rx = NULL;
		return 0;
	}
	if (cp)
		*cp++ = '\0';
	else
		cp = mode;
	for (; *mtp; mtp++) {
		if ((*mtp)->loc_storage > sizeof(sm->m)) {
			printk(KERN_ERR "%s: insufficient storage for modulator %s (%d)\n",
			       sm_drvname, (*mtp)->name, (*mtp)->loc_storage);
			continue;
		}
		if (!(*mtp)->name || strcmp((*mtp)->name, mode))
			continue;
		if ((*mtp)->srate < 5000 || (*mtp)->srate > 44100)
			continue;
		for (mrp = sm_modem_rx_table; *mrp; mrp++) {
			if ((*mrp)->loc_storage > sizeof(sm->d)) {
				printk(KERN_ERR "%s: insufficient storage for demodulator %s (%d)\n",
				       sm_drvname, (*mrp)->name, (*mrp)->loc_storage);
				continue;
			}
			if ((*mrp)->name && !strcmp((*mrp)->name, cp) &&
			    (*mtp)->srate >= 5000 && (*mtp)->srate <= 44100 &&
			    (*mrp)->srate == (*mtp)->srate) {
				sm->mode_tx = *mtp;
				sm->mode_rx = *mrp;
				SCSTATE->sr[0] = sm->mode_rx->srate;
				SCSTATE->sr[1] = sm->mode_tx->srate;
				sm->dma.ifragsz = (sm->mode_rx->srate + 50)/100;
				sm->dma.ofragsz = (sm->mode_tx->srate + 50)/100;
				if (sm->dma.ifragsz < sm->mode_rx->overlap)
					sm->dma.ifragsz = sm->mode_rx->overlap;
				if (sm->mode_rx->demodulator_s16 && sm->mode_tx->modulator_u8) {
					sm->dma.i16bit = 1;
					sm->dma.o16bit = 0;
					sm->dma.ifragsz <<= 1;
				} else if (sm->mode_rx->demodulator_u8 && sm->mode_tx->modulator_s16) {
					sm->dma.i16bit = 0;
					sm->dma.o16bit = 1;
					sm->dma.ofragsz <<= 1;
				} else {
					printk(KERN_INFO "%s: mode %s or %s unusable\n", sm_drvname, 
					       sm->mode_rx->name, sm->mode_tx->name);
					sm->mode_tx = NULL;
					sm->mode_rx = NULL;
					return -EINVAL;
				}
				return 0;
			}
		}
	}
	return -EINVAL;
}

/* --------------------------------------------------------------------- */

static int sbcfdx_ioctl(struct net_device *dev, struct sm_state *sm, struct ifreq *ifr, 
			struct hdlcdrv_ioctl *hi, int cmd)
{
	if (cmd != SIOCDEVPRIVATE)
		return -ENOIOCTLCMD;

	if (hi->cmd == HDLCDRVCTL_MODEMPARMASK)
		return HDLCDRV_PARMASK_IOBASE | HDLCDRV_PARMASK_IRQ | 
			HDLCDRV_PARMASK_DMA | HDLCDRV_PARMASK_DMA2 | HDLCDRV_PARMASK_SERIOBASE | 
			HDLCDRV_PARMASK_PARIOBASE | HDLCDRV_PARMASK_MIDIIOBASE;

	return sbc_ioctl(dev, sm, ifr, hi, cmd);
}

/* --------------------------------------------------------------------- */

const struct hardware_info sm_hw_sbcfdx = {
	"sbcfdx", sizeof(struct sc_state_sbc), 
	sbcfdx_open, sbcfdx_close, sbcfdx_ioctl, sbcfdx_sethw
};

/* --------------------------------------------------------------------- */
