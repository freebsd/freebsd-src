/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
 * Copyright 1997,1998 Luigi Rizzo.
 *
 * Derived from files in the Voxware 3.5 distribution,
 * Copyright by Hannu Savolainen 1994, under the same copyright
 * conditions.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/sound/isa/sb.c,v 1.50.2.2 2000/07/19 21:18:15 cg Exp $
 */

#include <dev/sound/pcm/sound.h>

#define __SB_MIXER_C__	/* XXX warning... */
#include  <dev/sound/isa/sb.h>
#include  <dev/sound/chip.h>

#define PLAIN_SB16(x) ((((x)->bd_flags) & (BD_F_SB16|BD_F_SB16X)) == BD_F_SB16)

/* channel interface */
static void *sbchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int sbchan_setdir(void *data, int dir);
static int sbchan_setformat(void *data, u_int32_t format);
static int sbchan_setspeed(void *data, u_int32_t speed);
static int sbchan_setblocksize(void *data, u_int32_t blocksize);
static int sbchan_trigger(void *data, int go);
static int sbchan_getptr(void *data);
static pcmchan_caps *sbchan_getcaps(void *data);

static pcmchan_caps sb_playcaps = {
	4000, 22050,
	AFMT_U8,
	AFMT_U8
};

static pcmchan_caps sb_reccaps = {
	4000, 13000,
	AFMT_U8,
	AFMT_U8
};

static pcmchan_caps sbpro_playcaps = {
	4000, 45000,
	AFMT_STEREO | AFMT_U8,
	AFMT_STEREO | AFMT_U8
};

static pcmchan_caps sbpro_reccaps = {
	4000, 15000,
	AFMT_STEREO | AFMT_U8,
	AFMT_STEREO | AFMT_U8
};

static pcmchan_caps sb16_hcaps = {
	5000, 45000,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcmchan_caps sb16_lcaps = {
	5000, 45000,
	AFMT_STEREO | AFMT_U8,
	AFMT_STEREO | AFMT_U8
};

static pcmchan_caps sb16x_caps = {
	5000, 49000,
	AFMT_STEREO | AFMT_U8 | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcm_channel sb_chantemplate = {
	sbchan_init,
	sbchan_setdir,
	sbchan_setformat,
	sbchan_setspeed,
	sbchan_setblocksize,
	sbchan_trigger,
	sbchan_getptr,
	sbchan_getcaps,
};

struct sb_info;

struct sb_chinfo {
	struct sb_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir;
	u_int32_t fmt, spd;
};

struct sb_info {
    	struct resource *io_base;	/* I/O address for the board */
    	struct resource *irq;
   	struct resource *drq1;
    	struct resource *drq2;
    	bus_dma_tag_t parent_dmat;

    	int bd_id;
    	u_long bd_flags;       /* board-specific flags */
    	struct sb_chinfo pch, rch;
};

static int sb_rd(struct sb_info *sb, int reg);
static void sb_wr(struct sb_info *sb, int reg, u_int8_t val);
static int sb_dspready(struct sb_info *sb);
static int sb_cmd(struct sb_info *sb, u_char val);
static int sb_cmd1(struct sb_info *sb, u_char cmd, int val);
static int sb_cmd2(struct sb_info *sb, u_char cmd, int val);
static u_int sb_get_byte(struct sb_info *sb);
static void sb_setmixer(struct sb_info *sb, u_int port, u_int value);
static int sb_getmixer(struct sb_info *sb, u_int port);
static int sb_reset_dsp(struct sb_info *sb);

static void sb_intr(void *arg);
static int sb_speed(struct sb_chinfo *ch);
static int sb_start(struct sb_chinfo *ch);
static int sb_stop(struct sb_chinfo *ch);

static int sbmix_init(snd_mixer *m);
static int sbmix_set(snd_mixer *m, unsigned dev, unsigned left, unsigned right);
static int sbmix_setrecsrc(snd_mixer *m, u_int32_t src);

static snd_mixer sb_mixer = {
    "SoundBlaster mixer",
    sbmix_init,
    sbmix_set,
    sbmix_setrecsrc,
};

static devclass_t pcm_devclass;

/*
 * Common code for the midi and pcm functions
 *
 * sb_cmd write a single byte to the CMD port.
 * sb_cmd1 write a CMD + 1 byte arg
 * sb_cmd2 write a CMD + 2 byte arg
 * sb_get_byte returns a single byte from the DSP data port
 *
 * ess_write is actually sb_cmd1
 * ess_read access ext. regs via sb_cmd(0xc0, reg) followed by sb_get_byte
 */

static int
port_rd(struct resource *port, int off)
{
	return bus_space_read_1(rman_get_bustag(port),
				rman_get_bushandle(port),
				off);
}

static void
port_wr(struct resource *port, int off, u_int8_t data)
{
	return bus_space_write_1(rman_get_bustag(port),
				 rman_get_bushandle(port),
				 off, data);
}

static int
sb_rd(struct sb_info *sb, int reg)
{
	return port_rd(sb->io_base, reg);
}

static void
sb_wr(struct sb_info *sb, int reg, u_int8_t val)
{
	port_wr(sb->io_base, reg, val);
}

static int
sb_dspready(struct sb_info *sb)
{
	return ((sb_rd(sb, SBDSP_STATUS) & 0x80) == 0);
}

static int
sb_dspwr(struct sb_info *sb, u_char val)
{
    	int  i;

    	for (i = 0; i < 1000; i++) {
		if (sb_dspready(sb)) {
	    		sb_wr(sb, SBDSP_CMD, val);
	    		return 1;
		}
		if (i > 10) DELAY((i > 100)? 1000 : 10);
    	}
    	printf("sb_dspwr(0x%02x) timed out.\n", val);
    	return 0;
}

static int
sb_cmd(struct sb_info *sb, u_char val)
{
#if 0
	printf("sb_cmd: %x\n", val);
#endif
    	return sb_dspwr(sb, val);
}

static int
sb_cmd1(struct sb_info *sb, u_char cmd, int val)
{
#if 0
    	printf("sb_cmd1: %x, %x\n", cmd, val);
#endif
    	if (sb_dspwr(sb, cmd)) {
		return sb_dspwr(sb, val & 0xff);
    	} else return 0;
}

static int
sb_cmd2(struct sb_info *sb, u_char cmd, int val)
{
#if 0
    	printf("sb_cmd2: %x, %x\n", cmd, val);
#endif
    	if (sb_dspwr(sb, cmd)) {
		return sb_dspwr(sb, val & 0xff) &&
		       sb_dspwr(sb, (val >> 8) & 0xff);
    	} else return 0;
}

/*
 * in the SB, there is a set of indirect "mixer" registers with
 * address at offset 4, data at offset 5
 */
static void
sb_setmixer(struct sb_info *sb, u_int port, u_int value)
{
    	u_long   flags;

    	flags = spltty();
    	sb_wr(sb, SB_MIX_ADDR, (u_char) (port & 0xff)); /* Select register */
    	DELAY(10);
    	sb_wr(sb, SB_MIX_DATA, (u_char) (value & 0xff));
    	DELAY(10);
    	splx(flags);
}

static int
sb_getmixer(struct sb_info *sb, u_int port)
{
    	int val;
    	u_long flags;

    	flags = spltty();
    	sb_wr(sb, SB_MIX_ADDR, (u_char) (port & 0xff)); /* Select register */
    	DELAY(10);
    	val = sb_rd(sb, SB_MIX_DATA);
    	DELAY(10);
    	splx(flags);

    	return val;
}

static u_int
sb_get_byte(struct sb_info *sb)
{
    	int i;

    	for (i = 1000; i > 0; i--) {
		if (sb_rd(sb, DSP_DATA_AVAIL) & 0x80)
			return sb_rd(sb, DSP_READ);
		else
			DELAY(20);
    	}
    	return 0xffff;
}

static int
sb_reset_dsp(struct sb_info *sb)
{
    	sb_wr(sb, SBDSP_RST, 3);
    	DELAY(100);
    	sb_wr(sb, SBDSP_RST, 0);
    	if (sb_get_byte(sb) != 0xAA) {
        	DEB(printf("sb_reset_dsp 0x%lx failed\n",
			   rman_get_start(d->io_base)));
		return ENXIO;	/* Sorry */
    	}
    	if (sb->bd_flags & BD_F_ESS)
		sb_cmd(sb, 0xc6);
    	return 0;
}

static void
sb_release_resources(struct sb_info *sb, device_t dev)
{
    	/* should we bus_teardown_intr here? */
    	if (sb->irq) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sb->irq);
		sb->irq = 0;
    	}
    	if (sb->drq1) {
		bus_release_resource(dev, SYS_RES_DRQ, 0, sb->drq1);
		sb->drq1 = 0;
    	}
    	if (sb->drq2) {
		bus_release_resource(dev, SYS_RES_DRQ, 1, sb->drq2);
		sb->drq2 = 0;
    	}
    	if (sb->io_base) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sb->io_base);
		sb->io_base = 0;
    	}
    	free(sb, M_DEVBUF);
}

static int
sb_alloc_resources(struct sb_info *sb, device_t dev)
{
	int rid;

	rid = 0;
	if (!sb->io_base)
    		sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT,
						 &rid, 0, ~0, 1,
						 RF_ACTIVE);
	rid = 0;
	if (!sb->irq)
    		sb->irq = bus_alloc_resource(dev, SYS_RES_IRQ,
					     &rid, 0, ~0, 1,
					     RF_ACTIVE);
	rid = 0;
	if (!sb->drq1)
    		sb->drq1 = bus_alloc_resource(dev, SYS_RES_DRQ,
					      &rid, 0, ~0, 1,
					      RF_ACTIVE);
	rid = 1;
	if (!sb->drq2)
        	sb->drq2 = bus_alloc_resource(dev, SYS_RES_DRQ,
					      &rid, 0, ~0, 1,
					      RF_ACTIVE);

    	if (sb->io_base && sb->drq1 && sb->irq) {
		int bs = DSP_BUFFSIZE;

		isa_dma_acquire(rman_get_start(sb->drq1));
		isa_dmainit(rman_get_start(sb->drq1), bs);

		if (sb->drq2) {
			isa_dma_acquire(rman_get_start(sb->drq2));
			isa_dmainit(rman_get_start(sb->drq2), bs);
		}

		return 0;
	} else return ENXIO;
}

static void
sb16_swap(void *v, int dir)
{
	struct sb_info *sb = v;
	int pb = sb->pch.buffer->dl;
	int rb = sb->rch.buffer->dl;
	int pc = sb->pch.buffer->chan;
	int rc = sb->rch.buffer->chan;
	int swp = 0;

	if (!pb && !rb) {
		if (dir == PCMDIR_PLAY && pc < 4)
			swp = 1;
		else
			if (dir == PCMDIR_REC && rc < 4)
				swp = 1;
	if (swp) {
			int t;

			t = sb->pch.buffer->chan;
			sb->pch.buffer->chan = sb->rch.buffer->chan;
			sb->rch.buffer->chan = t;
			sb->pch.buffer->dir = ISADMA_WRITE;
			sb->rch.buffer->dir = ISADMA_READ;
		}
	}
}

static int
sb_doattach(device_t dev, struct sb_info *sb)
{
    	snddev_info *d = device_get_softc(dev);
    	void *ih;
    	char status[SND_STATUSLEN];
	int bs = DSP_BUFFSIZE;

    	if (sb_alloc_resources(sb, dev))
		goto no;
    	if (sb_reset_dsp(sb))
		goto no;
    	mixer_init(d, &sb_mixer, sb);

	bus_setup_intr(dev, sb->irq, INTR_TYPE_TTY, sb_intr, sb, &ih);
    	if ((sb->bd_flags & BD_F_SB16) && !(sb->bd_flags & BD_F_SB16X))
		pcm_setswap(dev, sb16_swap);
    	if (!sb->drq2)
		pcm_setflags(dev, pcm_getflags(dev) | SD_F_SIMPLEX);

    	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
			/*lowaddr*/BUS_SPACE_MAXADDR_24BIT,
			/*highaddr*/BUS_SPACE_MAXADDR,
			/*filter*/NULL, /*filterarg*/NULL,
			/*maxsize*/bs, /*nsegments*/1,
			/*maxsegz*/0x3ffff,
			/*flags*/0, &sb->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto no;
    	}

    	snprintf(status, SND_STATUSLEN, "at io 0x%lx irq %ld drq %ld",
    	     	rman_get_start(sb->io_base), rman_get_start(sb->irq),
		rman_get_start(sb->drq1));
    	if (sb->drq2)
		snprintf(status + strlen(status), SND_STATUSLEN - strlen(status),
			":%ld", rman_get_start(sb->drq2));

    	if (pcm_register(dev, sb, 1, 1))
		goto no;
	pcm_addchan(dev, PCMDIR_REC, &sb_chantemplate, sb);
	pcm_addchan(dev, PCMDIR_PLAY, &sb_chantemplate, sb);
    	pcm_setstatus(dev, status);

    	return 0;

no:
    	sb_release_resources(sb, dev);
    	return ENXIO;
}

static void
sb_intr(void *arg)
{
    	struct sb_info *sb = (struct sb_info *)arg;
    	int reason = 3, c;

    	/*
     	* SB < 4.0 is half duplex and has only 1 bit for int source,
     	* so we fake it. SB 4.x (SB16) has the int source in a separate
     	* register.
     	* The Vibra16X has separate flags for 8 and 16 bit transfers, but
     	* I have no idea how to tell capture from playback interrupts...
     	*/
    	if (sb->bd_flags & BD_F_SB16) {
    		c = sb_getmixer(sb, IRQ_STAT);
    		/* this tells us if the source is 8-bit or 16-bit dma. We
     		* have to check the io channel to map it to read or write...
     		*/
    		reason = 0;
    		if (c & 1) { /* 8-bit dma */
			if (sb->pch.fmt & AFMT_U8)
				reason |= 1;
			if (sb->rch.fmt & AFMT_U8)
				reason |= 2;
    		}
    		if (c & 2) { /* 16-bit dma */
			if (sb->pch.fmt & AFMT_S16_LE)
				reason |= 1;
			if (sb->rch.fmt & AFMT_S16_LE)
				reason |= 2;
    		}
    	} else c = 1;
#if 0
    	printf("sb_intr: reason=%d c=0x%x\n", reason, c);
#endif
    	if ((reason & 1) && (sb->pch.buffer->dl > 0))
		chn_intr(sb->pch.channel);
    	if ((reason & 2) && (sb->rch.buffer->dl > 0))
		chn_intr(sb->rch.channel);
    	if (c & 1)
		sb_rd(sb, DSP_DATA_AVAIL); /* 8-bit int ack */
    	if (c & 2)
		sb_rd(sb, DSP_DATA_AVL16); /* 16-bit int ack */
}

static int
sb_speed(struct sb_chinfo *ch)
{
    	struct sb_info *sb = ch->parent;
    	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
    	int stereo = (ch->fmt & AFMT_STEREO)? 1 : 0;
	int speed = ch->spd;

    	if (sb->bd_flags & BD_F_SB16) {
		RANGE(speed, 5000, 45000);
		sb_cmd(sb, 0x42 - play);
    		sb_cmd(sb, speed >> 8);
		sb_cmd(sb, speed & 0xff);
    	} else {
		u_char tconst;
		int max_speed = 45000, tmp;
        	u_long flags;

    		/* here enforce speed limitations - max 22050 on sb 1.x*/
    		if (sb->bd_id <= 0x200)
			max_speed = 22050;

    		/*
     	 	* SB models earlier than SB Pro have low limit for the
     	 	* input rate. Note that this is only for input, but since
     	 	* we do not support separate values for rec & play....
     	 	*/
		if (!play) {
    			if (sb->bd_id <= 0x200)
				max_speed = 13000;
    			else
				if (sb->bd_id < 0x300)
					max_speed = 15000;
		}
    		RANGE(speed, 4000, max_speed);
    		if (stereo)
			speed <<= 1;

    		/*
     	 	* Now the speed should be valid. Compute the value to be
     	 	* programmed into the board.
     	 	*/
    		if (speed > 22050) { /* High speed mode on 2.01/3.xx */
			tconst = (u_char)
				((65536 - ((256000000 + speed / 2) / speed))
				>> 8);
			sb->bd_flags |= BD_F_HISPEED;
			tmp = 65536 - (tconst << 8);
			speed = (256000000 + tmp / 2) / tmp;
    		} else {
			sb->bd_flags &= ~BD_F_HISPEED;
			tconst = (256 - ((1000000 + speed / 2) / speed)) & 0xff;
			tmp = 256 - tconst;
			speed = (1000000 + tmp / 2) / tmp;
    		}
		flags = spltty();
		sb_cmd1(sb, 0x40, tconst); /* set time constant */
		splx(flags);
    		if (stereo)
			speed >>= 1;
    	}
	ch->spd = speed;
    	return speed;
}

static int
sb_start(struct sb_chinfo *ch)
{
	struct sb_info *sb = ch->parent;
    	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
    	int b16 = (ch->fmt & AFMT_S16_LE)? 1 : 0;
    	int stereo = (ch->fmt & AFMT_STEREO)? 1 : 0;
	int l = ch->buffer->dl;
	int dh = ch->buffer->chan > 3;
	u_char i1, i2;

	if (b16 || dh)
		l >>= 1;
	l--;

	if (play)
		sb_cmd(sb, DSP_CMD_SPKON);

	if (sb->bd_flags & BD_F_SB16) {
	    	i1 = DSP_F16_AUTO | DSP_F16_FIFO_ON;
	        i1 |= play? DSP_F16_DAC : DSP_F16_ADC;
	    	i1 |= (b16 || dh)? DSP_DMA16 : DSP_DMA8;
	    	i2 = (stereo? DSP_F16_STEREO : 0) | (b16? DSP_F16_SIGNED : 0);
	    	sb_cmd(sb, i1);
	    	sb_cmd2(sb, i2, l);
	} else {
	    	if (sb->bd_flags & BD_F_HISPEED)
			i1 = play? 0x90 : 0x98;
	    	else
			i1 = play? 0x1c : 0x2c;
	    	sb_setmixer(sb, 0x0e, stereo? 2 : 0);
	    	sb_cmd2(sb, 0x48, l);
       	    	sb_cmd(sb, i1);
	}
	sb->bd_flags |= BD_F_DMARUN << b16;
	return 0;
}

static int
sb_stop(struct sb_chinfo *ch)
{
	struct sb_info *sb = ch->parent;
    	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
    	int b16 = (ch->fmt & AFMT_S16_LE)? 1 : 0;

    	if (sb->bd_flags & BD_F_HISPEED)
		sb_reset_dsp(sb);
	else {
		sb_cmd(sb, b16? DSP_CMD_DMAPAUSE_16 : DSP_CMD_DMAPAUSE_8);
	       /*
		* The above seems to have the undocumented side effect of
		* blocking the other side as well. If the other
		* channel was active (SB16) I have to re-enable it :(
		*/
		if (sb->bd_flags & (BD_F_DMARUN << (1 - b16)))
			sb_cmd(sb, b16? 0xd4 : 0xd6 );
	}
	if (play)
		sb_cmd(sb, DSP_CMD_SPKOFF); /* speaker off */
	sb->bd_flags &= ~(BD_F_DMARUN << b16);
	return 0;
}

/* channel interface */
static void *
sbchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct sb_info *sb = devinfo;
	struct sb_chinfo *ch = (dir == PCMDIR_PLAY)? &sb->pch : &sb->rch;
	int dch, dl, dh;

	ch->parent = sb;
	ch->channel = c;
	ch->buffer = b;
	ch->buffer->bufsize = DSP_BUFFSIZE;
	if (chn_allocbuf(ch->buffer, sb->parent_dmat) == -1)
		return NULL;
	dch = (dir == PCMDIR_PLAY)? 1 : 0;
	if (sb->bd_flags & BD_F_SB16X)
		dch = !dch;
	dl = rman_get_start(sb->drq1);
	dh = sb->drq2? rman_get_start(sb->drq2) : dl;
	ch->buffer->chan = dch? dh : dl;
	return ch;
}

static int
sbchan_setdir(void *data, int dir)
{
	struct sb_chinfo *ch = data;

	ch->dir = dir;
	return 0;
}

static int
sbchan_setformat(void *data, u_int32_t format)
{
	struct sb_chinfo *ch = data;

	ch->fmt = format;
	return 0;
}

static int
sbchan_setspeed(void *data, u_int32_t speed)
{
	struct sb_chinfo *ch = data;

	ch->spd = speed;
	return sb_speed(ch);
}

static int
sbchan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
sbchan_trigger(void *data, int go)
{
	struct sb_chinfo *ch = data;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	buf_isadma(ch->buffer, go);
	if (go == PCMTRIG_START)
		sb_start(ch);
	else
		sb_stop(ch);
	return 0;
}

static int
sbchan_getptr(void *data)
{
	struct sb_chinfo *ch = data;

	return buf_isadmaptr(ch->buffer);
}

static pcmchan_caps *
sbchan_getcaps(void *data)
{
	struct sb_chinfo *ch = data;
	int p = (ch->dir == PCMDIR_PLAY)? 1 : 0;

	if (ch->parent->bd_id < 0x300)
		return p? &sb_playcaps : &sb_reccaps;
	else if (ch->parent->bd_id < 0x400)
		return p? &sbpro_playcaps : &sbpro_reccaps;
	else if (ch->parent->bd_flags & BD_F_SB16X)
		return &sb16x_caps;
	else
		return (ch->buffer->chan >= 4)? &sb16_hcaps : &sb16_lcaps;
}

/************************************************************/

static int
sbmix_init(snd_mixer *m)
{
    	struct sb_info *sb = mix_getdevinfo(m);

    	switch (sb->bd_flags & BD_F_MIX_MASK) {
    	case BD_F_MIX_CT1345: /* SB 3.0 has 1345 mixer */
		mix_setdevs(m, SBPRO_MIXER_DEVICES);
		mix_setrecdevs(m, SBPRO_RECORDING_DEVICES);
		sb_setmixer(sb, 0, 1); /* reset mixer */
		sb_setmixer(sb, MIC_VOL, 0x6); /* mic volume max */
		sb_setmixer(sb, RECORD_SRC, 0x0); /* mic source */
		sb_setmixer(sb, FM_VOL, 0x0); /* no midi */
		break;

    	case BD_F_MIX_CT1745: /* SB16 mixer ... */
		mix_setdevs(m, SB16_MIXER_DEVICES);
		mix_setrecdevs(m, SB16_RECORDING_DEVICES);
		sb_setmixer(sb, 0x3c, 0x1f); /* make all output active */
		sb_setmixer(sb, 0x3d, 0); /* make all inputs-l off */
		sb_setmixer(sb, 0x3e, 0); /* make all inputs-r off */
    	}
    	return 0;
}

static int
sbmix_set(snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
    	struct sb_info *sb = mix_getdevinfo(m);
    	int regoffs;
    	u_char   val;
    	mixer_tab *iomap;

    	switch (sb->bd_flags & BD_F_MIX_MASK) {
    	case BD_F_MIX_CT1345:
		iomap = &sbpro_mix;
		break;

    	case BD_F_MIX_CT1745:
		iomap = &sb16_mix;
		break;

    	default:
        	return -1;
    	}

	/* Change left channel */
    	regoffs = (*iomap)[dev][LEFT_CHN].regno;
    	if (regoffs != 0) {
		val = sb_getmixer(sb, regoffs);
		change_bits(iomap, &val, dev, LEFT_CHN, left);
		sb_setmixer(sb, regoffs, val);
	}

	/* Change right channel */
	regoffs = (*iomap)[dev][RIGHT_CHN].regno;
	if (regoffs != 0) {
		val = sb_getmixer(sb, regoffs); /* Read the new one */
		change_bits(iomap, &val, dev, RIGHT_CHN, right);
		sb_setmixer(sb, regoffs, val);
	} else
		right = left;

    	return left | (right << 8);
}

static int
sbmix_setrecsrc(snd_mixer *m, u_int32_t src)
{
    	struct sb_info *sb = mix_getdevinfo(m);
    	u_char recdev;

    	switch (sb->bd_flags & BD_F_MIX_MASK) {
    	case BD_F_MIX_CT1345:
		if      (src == SOUND_MASK_LINE)
			recdev = 0x06;
		else if (src == SOUND_MASK_CD)
			recdev = 0x02;
		else { /* default: mic */
	    		src = SOUND_MASK_MIC;
	    		recdev = 0;
		}
		sb_setmixer(sb, RECORD_SRC, recdev |
			    (sb_getmixer(sb, RECORD_SRC) & ~0x07));
		break;

    	case BD_F_MIX_CT1745: /* sb16 */
		recdev = 0;
		if (src & SOUND_MASK_MIC)
			recdev |= 0x01; /* mono mic */
		if (src & SOUND_MASK_CD)
			recdev |= 0x06; /* l+r cd */
		if (src & SOUND_MASK_LINE)
			recdev |= 0x18; /* l+r line */
		if (src & SOUND_MASK_SYNTH)
			recdev |= 0x60; /* l+r midi */
		sb_setmixer(sb, SB16_IMASK_L, recdev);
		sb_setmixer(sb, SB16_IMASK_R, recdev);
		/*
	 	* since the same volume controls apply to the input and
	 	* output sections, the best approach to have a consistent
	 	* behaviour among cards would be to disable the output path
	 	* on devices which are used to record.
	 	* However, since users like to have feedback, we only disable
	 	* the mic -- permanently.
	 	*/
        	sb_setmixer(sb, SB16_OMASK, 0x1f & ~1);
		break;
       	}
    	return src;
}

static int
sbsbc_probe(device_t dev)
{
    	char buf[64];
	uintptr_t func, ver, r, f;

	/* The parent device has already been probed. */
	r = BUS_READ_IVAR(device_get_parent(dev), dev, 0, &func);
	if (func != SCF_PCM)
		return (ENXIO);

	r = BUS_READ_IVAR(device_get_parent(dev), dev, 1, &ver);
	f = (ver & 0xffff0000) >> 16;
	ver &= 0x0000ffff;
	if (f & BD_F_ESS)
		return (ENXIO);

	snprintf(buf, sizeof buf, "SB DSP %d.%02d%s", (int) ver >> 8, (int) ver & 0xff,
		(f & BD_F_SB16X)? " (ViBRA16X)" : "");
    	device_set_desc_copy(dev, buf);

	return 0;
}

static int
sbsbc_attach(device_t dev)
{
    	struct sb_info *sb;
	uintptr_t ver;

    	sb = (struct sb_info *)malloc(sizeof *sb, M_DEVBUF, M_NOWAIT);
    	if (!sb)
		return ENXIO;
    	bzero(sb, sizeof *sb);

	BUS_READ_IVAR(device_get_parent(dev), dev, 1, &ver);
	sb->bd_id = ver & 0x0000ffff;
	sb->bd_flags = (ver & 0xffff0000) >> 16;

    	return sb_doattach(dev, sb);
}

static device_method_t sbsbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sbsbc_probe),
	DEVMETHOD(device_attach,	sbsbc_attach),

	{ 0, 0 }
};

static driver_t sbsbc_driver = {
	"pcm",
	sbsbc_methods,
	sizeof(snddev_info),
};

DRIVER_MODULE(snd_sb, sbc, sbsbc_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_sb, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_sb, 1);




