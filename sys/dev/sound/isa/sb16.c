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
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#if NPCM > 0

#include "sbc.h"

#define __SB_MIXER_C__	/* XXX warning... */
#include  <dev/sound/isa/sb.h>
#include  <dev/sound/chip.h>

/* channel interface */
static void *sbchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int sbchan_setdir(void *data, int dir);
static int sbchan_setformat(void *data, u_int32_t format);
static int sbchan_setspeed(void *data, u_int32_t speed);
static int sbchan_setblocksize(void *data, u_int32_t blocksize);
static int sbchan_trigger(void *data, int go);
static int sbchan_getptr(void *data);
static pcmchan_caps *sbchan_getcaps(void *data);

/* channel interface for ESS */
#ifdef notyet
static void *esschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
#endif
static int esschan_setdir(void *data, int dir);
static int esschan_setformat(void *data, u_int32_t format);
static int esschan_setspeed(void *data, u_int32_t speed);
static int esschan_setblocksize(void *data, u_int32_t blocksize);
static int esschan_trigger(void *data, int go);
static int esschan_getptr(void *data);
static pcmchan_caps *esschan_getcaps(void *data);
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

static pcmchan_caps sb16_playcaps = {
	5000, 45000,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcmchan_caps sb16_reccaps = {
	5000, 45000,
	AFMT_STEREO | AFMT_U8,
	AFMT_STEREO | AFMT_U8
};

static pcmchan_caps ess_playcaps = {
	5000, 49000,
	AFMT_STEREO | AFMT_U8 | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcmchan_caps ess_reccaps = {
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

static pcm_channel ess_chantemplate = {
	sbchan_init,
	esschan_setdir,
	esschan_setformat,
	esschan_setspeed,
	esschan_setblocksize,
	esschan_trigger,
	esschan_getptr,
	esschan_getcaps,
};
#define PLAIN_SB16(x) ((((x)->bd_flags) & (BD_F_SB16|BD_F_SB16X)) == BD_F_SB16)

struct sb_info;

struct sb_chinfo {
	struct sb_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir;
	u_int32_t fmt;
	int ess_dma_started;
};

struct sb_info {
    	struct resource *io_base;	/* I/O address for the board */
    	int		     io_rid;
    	struct resource *irq;
    	int		     irq_rid;
    	struct resource *drq1; /* play */
    	int		     drq1_rid;
    	struct resource *drq2; /* rec */
    	int		     drq2_rid;
    	bus_dma_tag_t    parent_dmat;

    	int dma16, dma8;
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
static int ess_write(struct sb_info *sb, u_char reg, int val);
static int ess_read(struct sb_info *sb, u_char reg);

/*
 * in the SB, there is a set of indirect "mixer" registers with
 * address at offset 4, data at offset 5
 */
static void sb_setmixer(struct sb_info *sb, u_int port, u_int value);
static int sb_getmixer(struct sb_info *sb, u_int port);

static void sb_intr(void *arg);
static void ess_intr(void *arg);
static int sb_init(device_t dev, struct sb_info *sb);
static int sb_reset_dsp(struct sb_info *sb);

static int sb_format(struct sb_chinfo *ch, u_int32_t format);
static int sb_speed(struct sb_chinfo *ch, int speed);
static int sb_start(struct sb_chinfo *ch);
static int sb_stop(struct sb_chinfo *ch);

static int ess_format(struct sb_chinfo *ch, u_int32_t format);
static int ess_speed(struct sb_chinfo *ch, int speed);
static int ess_start(struct sb_chinfo *ch);
static int ess_stop(struct sb_chinfo *ch);
static int ess_abort(struct sb_chinfo *ch);
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
ess_write(struct sb_info *sb, u_char reg, int val)
{
    	return sb_cmd1(sb, reg, val);
}

static int
ess_read(struct sb_info *sb, u_char reg)
{
    	return (sb_cmd(sb, 0xc0) && sb_cmd(sb, reg))? sb_get_byte(sb) : 0xffff;
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
    	if (sb->bd_flags & BD_F_ESS) sb_cmd(sb, 0xc6);
    	return 0;
}

static void
sb_release_resources(struct sb_info *sb, device_t dev)
{
    	/* should we bus_teardown_intr here? */
    	if (sb->irq) {
		bus_release_resource(dev, SYS_RES_IRQ, sb->irq_rid, sb->irq);
		sb->irq = 0;
    	}
    	if (sb->drq1) {
		bus_release_resource(dev, SYS_RES_DRQ, sb->drq1_rid, sb->drq1);
		sb->drq1 = 0;
    	}
    	if (sb->drq2) {
		bus_release_resource(dev, SYS_RES_DRQ, sb->drq2_rid, sb->drq2);
		sb->drq2 = 0;
    	}
    	if (sb->io_base) {
		bus_release_resource(dev, SYS_RES_IOPORT, sb->io_rid,
				     sb->io_base);
		sb->io_base = 0;
    	}
    	free(sb, M_DEVBUF);
}

static int
sb_alloc_resources(struct sb_info *sb, device_t dev)
{
	if (!sb->io_base)
    		sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT,
						 &sb->io_rid, 0, ~0, 1,
						 RF_ACTIVE);
	if (!sb->irq)
    		sb->irq = bus_alloc_resource(dev, SYS_RES_IRQ,
					     &sb->irq_rid, 0, ~0, 1,
					     RF_ACTIVE);
	if (!sb->drq1)
    		sb->drq1 = bus_alloc_resource(dev, SYS_RES_DRQ,
					      &sb->drq1_rid, 0, ~0, 1,
					      RF_ACTIVE);
	if (!sb->drq2 && sb->drq2_rid > 0)
        	sb->drq2 = bus_alloc_resource(dev, SYS_RES_DRQ,
					      &sb->drq2_rid, 0, ~0, 1,
					      RF_ACTIVE);

    	if (sb->io_base && sb->drq1 && sb->irq) {
		sb->dma8 = rman_get_start(sb->drq1);
		isa_dma_acquire(sb->dma8);
		isa_dmainit(sb->dma8, DSP_BUFFSIZE);

		if (sb->drq2) {
			sb->dma16 = rman_get_start(sb->drq2);
			isa_dma_acquire(sb->dma16);
			isa_dmainit(sb->dma16, DSP_BUFFSIZE);
		} else sb->dma16 = sb->dma8;

		if (sb->dma8 > sb->dma16) {
			int tmp = sb->dma16;
			sb->dma16 = sb->dma8;
			sb->dma8 = tmp;
		}
		return 0;
	} else return ENXIO;
}

static int
sb_identify_board(device_t dev, struct sb_info *sb)
{
    	char *fmt = NULL;
    	static char buf[64];
	int essver = 0;

    	sb_cmd(sb, DSP_CMD_GETVER);	/* Get version */
    	sb->bd_id = (sb_get_byte(sb) << 8) | sb_get_byte(sb);

    	switch (sb->bd_id >> 8) {
    	case 1: /* old sound blaster has nothing... */
    	case 2:
		fmt = "SoundBlaster %d.%d" ; /* default */
		break;

    	case 3:
		fmt = "SoundBlaster Pro %d.%d";
		if (sb->bd_id == 0x301) {
	    		int rev;

	    		/* Try to detect ESS chips. */
	    		sb_cmd(sb, DSP_CMD_GETID); /* Return ident. bytes. */
	    		essver = (sb_get_byte(sb) << 8) | sb_get_byte(sb);
	    		rev = essver & 0x000f;
	    		essver &= 0xfff0;
	    		if (essver == 0x4880) {
				/* the ESS488 can be treated as an SBPRO */
				fmt = "SoundBlaster Pro (ESS488 rev %d)";
	    		} else if (essver == 0x6880) {
				if (rev < 8) fmt = "ESS688 rev %d";
				else fmt = "ESS1868 rev %d";
	        		sb->bd_flags |= BD_F_ESS;
	    		} else return ENXIO;
	    		sb->bd_id &= 0xff00;
	    		sb->bd_id |= ((essver & 0xf000) >> 8) | rev;
		}
		break;

    	case 4:
		sb->bd_flags |= BD_F_SB16;
		if (sb->bd_flags & BD_F_SB16X) fmt = "SB16 ViBRA16X %d.%d";
        	else fmt = "SoundBlaster 16 %d.%d";
		break;

    	default:
		device_printf(dev, "failed to get SB version (%x)\n",
			      sb->bd_id);
		return ENXIO;
    	}
    	if (essver) snprintf(buf, sizeof buf, fmt, sb->bd_id & 0x000f);
	else snprintf(buf, sizeof buf, fmt, sb->bd_id >> 8, sb->bd_id & 0xff);
    	device_set_desc_copy(dev, buf);
    	return sb_reset_dsp(sb);
}

static int
sb_init(device_t dev, struct sb_info *sb)
{
    	int x, irq;

    	sb->bd_flags &= ~BD_F_MIX_MASK;
    	/* do various initializations depending on board id. */
    	switch (sb->bd_id >> 8) {
    	case 1: /* old sound blaster has nothing... */
		break;

    	case 2:
		sb->bd_flags |= BD_F_DUP_MIDI;
		if (sb->bd_id > 0x200) sb->bd_flags |= BD_F_MIX_CT1335;
		break;

    	case 3:
		sb->bd_flags |= BD_F_DUP_MIDI | BD_F_MIX_CT1345;
		break;

    	case 4:
    		sb->bd_flags |= BD_F_SB16 | BD_F_MIX_CT1745;
		if (sb->dma16 != sb->dma8) sb->bd_flags |= BD_F_DUPLEX;

		/* soft irq/dma configuration */
		x = -1;
		irq = rman_get_start(sb->irq);
		if      (irq == 5) x = 2;
		else if (irq == 7) x = 4;
		else if (irq == 9) x = 1;
		else if (irq == 10) x = 8;
		if (x == -1) device_printf(dev,
					   "bad irq %d (5/7/9/10 valid)\n",
					   irq);
		else sb_setmixer(sb, IRQ_NR, x);
		sb_setmixer(sb, DMA_NR, (1 << sb->dma16) | (1 << sb->dma8));
		break;
    	}
    	return 0;
}

static int
sb_probe(device_t dev)
{
    	snddev_info *d = device_get_softc(dev);
    	struct sb_info *sb;
    	int allocated, i;
    	int error;

    	if (isa_get_vendorid(dev)) return ENXIO; /* not yet */

    	device_set_desc(dev, "SoundBlaster");
    	bzero(d, sizeof *d);
    	sb = (struct sb_info *)malloc(sizeof *sb, M_DEVBUF, M_NOWAIT);
    	if (!sb) return ENXIO;
    	bzero(sb, sizeof *sb);

    	allocated = 0;
    	sb->io_rid = 0;
    	sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT, &sb->io_rid,
				    	0, ~0, 16, RF_ACTIVE);
    	if (!sb->io_base) {
		BVDDB(printf("sb_probe: no addr, trying (0x220, 0x240)\n"));
		allocated = 1;
		sb->io_rid = 0;
		sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT,
						 &sb->io_rid, 0x220, 0x22f,
						 16, RF_ACTIVE);
		if (!sb->io_base) {
		    	sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT,
							 &sb->io_rid, 0x240,
							 0x24f, 16, RF_ACTIVE);
		}
    	}
    	if (!sb->io_base) return ENXIO;

    	error = sb_reset_dsp(sb);
    	if (error) goto no;
    	error = sb_identify_board(dev, sb);
    	if (error) goto no;
no:
    	i = sb->io_rid;
    	sb_release_resources(sb, dev);
    	if (allocated) bus_delete_resource(dev, SYS_RES_IOPORT, i);
    	return error;
}

static int
sb_doattach(device_t dev, struct sb_info *sb)
{
    	snddev_info *d = device_get_softc(dev);
    	void *ih;
    	int error;
    	char status[SND_STATUSLEN];

    	sb->irq_rid = 0;
    	sb->drq1_rid = 0;
    	sb->drq2_rid = 1;
    	if (sb_alloc_resources(sb, dev)) goto no;
    	error = sb_reset_dsp(sb);
    	if (error) goto no;
    	error = sb_identify_board(dev, sb);
    	if (error) goto no;

    	sb_init(dev, sb);
    	mixer_init(d, &sb_mixer, sb);
	if (sb->bd_flags & BD_F_ESS)
		bus_setup_intr(dev, sb->irq, INTR_TYPE_TTY, ess_intr, sb, &ih);
	else
		bus_setup_intr(dev, sb->irq, INTR_TYPE_TTY, sb_intr, sb, &ih);

    	if (sb->bd_flags & BD_F_SB16)
		pcm_setflags(dev, pcm_getflags(dev) | SD_F_EVILSB16);
    	if (sb->dma16 == sb->dma8)
		pcm_setflags(dev, pcm_getflags(dev) | SD_F_SIMPLEX);
    	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
			/*lowaddr*/BUS_SPACE_MAXADDR_24BIT,
			/*highaddr*/BUS_SPACE_MAXADDR,
			/*filter*/NULL, /*filterarg*/NULL,
			/*maxsize*/DSP_BUFFSIZE, /*nsegments*/1,
			/*maxsegz*/0x3ffff,
			/*flags*/0, &sb->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto no;
    	}

    	snprintf(status, SND_STATUSLEN, "at io 0x%lx irq %ld drq %d",
    	     	rman_get_start(sb->io_base), rman_get_start(sb->irq),
		sb->dma8);
    	if (sb->dma16 != sb->dma8) snprintf(status + strlen(status),
    		SND_STATUSLEN - strlen(status), ":%d", sb->dma16);

    	if (pcm_register(dev, sb, 1, 1)) goto no;
	if (sb->bd_flags & BD_F_ESS) {
		pcm_addchan(dev, PCMDIR_REC, &ess_chantemplate, sb);
		pcm_addchan(dev, PCMDIR_PLAY, &ess_chantemplate, sb);
	} else {
		pcm_addchan(dev, PCMDIR_REC, &sb_chantemplate, sb);
		pcm_addchan(dev, PCMDIR_PLAY, &sb_chantemplate, sb);
	}
    	pcm_setstatus(dev, status);

    	return 0;

no:
    	sb_release_resources(sb, dev);
    	return ENXIO;
}

static int
sb_attach(device_t dev)
{
    	struct sb_info *sb;
    	int flags = device_get_flags(dev);

    	if (flags & DV_F_DUAL_DMA) {
        	bus_set_resource(dev, SYS_RES_DRQ, 1,
				 flags & DV_F_DRQ_MASK, 1);
    	}
    	sb = (struct sb_info *)malloc(sizeof *sb, M_DEVBUF, M_NOWAIT);
    	if (!sb) return ENXIO;
    	bzero(sb, sizeof *sb);

    	/* XXX in probe should set io resource to right val instead of this */
    	sb->io_rid = 0;
    	sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT, &sb->io_rid,
				    	0, ~0, 16, RF_ACTIVE);
    	if (!sb->io_base) {
		BVDDB(printf("sb_probe: no addr, trying (0x220, 0x240)\n"));
		sb->io_rid = 0;
		sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT,
						 &sb->io_rid, 0x220, 0x22f,
						 16, RF_ACTIVE);
		if (!sb->io_base) {
	    		sb->io_base = bus_alloc_resource(dev, SYS_RES_IOPORT,
							 &sb->io_rid, 0x240,
							 0x24f, 16, RF_ACTIVE);
		}
    	}
    	if (!sb->io_base) return ENXIO;

    	return sb_doattach(dev, sb);
}

static device_method_t sb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sb_probe),
	DEVMETHOD(device_attach,	sb_attach),

	{ 0, 0 }
};

static driver_t sb_driver = {
	"pcm",
	sb_methods,
	sizeof(snddev_info),
};

DRIVER_MODULE(sb, isa, sb_driver, pcm_devclass, 0, 0);

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
			if (sb->pch.fmt & AFMT_U8) reason |= 1;
			if (sb->rch.fmt & AFMT_U8) reason |= 2;
    		}
    		if (c & 2) { /* 16-bit dma */
			if (sb->pch.fmt & AFMT_S16_LE) reason |= 1;
			if (sb->rch.fmt & AFMT_S16_LE) reason |= 2;
    		}
    	} else c = 1;
#if 0
    	printf("sb_intr: reason=%d c=0x%x\n", reason, c);
#endif
    	if ((reason & 1) && (sb->pch.buffer->dl > 0))
		chn_intr(sb->pch.channel);
    	if ((reason & 2) && (sb->rch.buffer->dl > 0))
		chn_intr(sb->rch.channel);
    	if (c & 1) sb_rd(sb, DSP_DATA_AVAIL); /* 8-bit int ack */
    	if (c & 2) sb_rd(sb, DSP_DATA_AVL16); /* 16-bit int ack */
}

static void
ess_intr(void *arg)
{
    struct sb_info *sb = (struct sb_info *)arg;
    sb_rd(sb, DSP_DATA_AVAIL); /* int ack */
#ifdef notyet
    /*
     * XXX
     * for full-duplex mode:
     * should read port 0x6 to identify where interrupt came from.
     */
#endif
    /*
     * We are transferring data in DSP normal mode,
     * so clear the dl to indicate the DMA is stopped.
     */
    if (sb->pch.buffer->dl > 0) {
	sb->pch.buffer->dl = -1;
	chn_intr(sb->pch.channel);
    }
    if (sb->rch.buffer->dl > 0) {
	sb->rch.buffer->dl = -1;
	chn_intr(sb->rch.channel);
    }
}

static int
sb_format(struct sb_chinfo *ch, u_int32_t format)
{
	ch->fmt = format;
	return 0;
}

static int
sb_speed(struct sb_chinfo *ch, int speed)
{
    	struct sb_info *sb = ch->parent;
    	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
    	int stereo = (ch->fmt & AFMT_STEREO)? 1 : 0;

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
    		if (sb->bd_id <= 0x200) max_speed = 22050;

    		/*
     	 	* SB models earlier than SB Pro have low limit for the
     	 	* input rate. Note that this is only for input, but since
     	 	* we do not support separate values for rec & play....
     	 	*/
		if (!play) {
    			if (sb->bd_id <= 0x200) max_speed = 13000;
    			else if (sb->bd_id < 0x300) max_speed = 15000;
		}
    		RANGE(speed, 4000, max_speed);
    		if (stereo) speed <<= 1;

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
    		if (stereo) speed >>= 1;
    	}
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
	u_char i1, i2 = 0;

	if (b16) l >>= 1;
	l--;
	if (play) sb_cmd(sb, DSP_CMD_SPKON);
	if (sb->bd_flags & BD_F_SB16) {
	    i1 = DSP_F16_AUTO | DSP_F16_FIFO_ON |
	         (play? DSP_F16_DAC : DSP_F16_ADC);
	    i1 |= (b16 && (sb->bd_flags & BD_F_DUPLEX))? DSP_DMA16 : DSP_DMA8;
	    i2 = (stereo? DSP_F16_STEREO : 0) | (b16? DSP_F16_SIGNED : 0);
	    sb_cmd(sb, i1);
	    sb_cmd2(sb, i2, l);
	} else {
	    if (sb->bd_flags & BD_F_HISPEED) i1 = play? 0x90 : 0x98;
	    else i1 = play? 0x1c : 0x2c;
	    sb_setmixer(sb, 0x0e, stereo? 2 : 0);
	    /* an ESS extension -- they can do 16 bits */
	    if (b16) i1 |= 1;
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

    	if (sb->bd_flags & BD_F_HISPEED) sb_reset_dsp(sb);
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
	if (play) sb_cmd(sb, DSP_CMD_SPKOFF); /* speaker off */
	sb->bd_flags &= ~(BD_F_DMARUN << b16);
	return 0;
}

/* utility functions for ESS */
static int
ess_format(struct sb_chinfo *ch, u_int32_t format)
{
	struct sb_info *sb = ch->parent;
	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
	int b16 = (format & AFMT_S16_LE)? 1 : 0;
	int stereo = (format & AFMT_STEREO)? 1 : 0;
	u_char c;
	ch->fmt = format;
	sb_reset_dsp(sb);
	/* normal DMA mode */
	ess_write(sb, 0xb8, play ? 0x00 : 0x0a);
	/* mono/stereo */
	c = (ess_read(sb, 0xa8) & ~0x03) | 1;
	if (!stereo) c++;
	ess_write(sb, 0xa8, c);
	/* demand mode, 4 bytes/xfer */
	ess_write(sb, 0xb9, 2);
	/* setup dac/adc */
	if (play) ess_write(sb, 0xb6, b16? 0x00 : 0x80);
	ess_write(sb, 0xb7, 0x51 | (b16? 0x20 : 0x00));
	ess_write(sb, 0xb7, 0x98 + (b16? 0x24 : 0x00) + (stereo? 0x00 : 0x38));
	/* irq/drq control */
	ess_write(sb, 0xb1, (ess_read(sb, 0xb1) & 0x0f) | 0x50);
	ess_write(sb, 0xb2, (ess_read(sb, 0xb2) & 0x0f) | 0x50);
	return 0;
}

static int
ess_speed(struct sb_chinfo *ch, int speed)
{
	struct sb_info *sb = ch->parent;
	int t;
	RANGE (speed, 5000, 49000);
	if (speed > 22000) {
		t = (795500 + speed / 2) / speed;
		speed = (795500 + t / 2) / t;
		t = (256 - t ) | 0x80;
	} else {
		t = (397700 + speed / 2) / speed;
		speed = (397700 + t / 2) / t;
		t = 128 - t;
	}
	ess_write(sb, 0xa1, t); /* set time constant */
#if 0
	d->play_speed = d->rec_speed = speed;
	speed = (speed * 9 ) / 20;
#endif
	t = 256 - 7160000 / ((speed * 9 / 20) * 82);
	ess_write(sb, 0xa2, t);
	return speed;
}

static int
ess_start(struct sb_chinfo *ch)
{
	struct sb_info *sb = ch->parent;
    	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
	short c = - ch->buffer->dl;
	u_char c1;
	/*
	 * clear bit 0 of register B8h
	 */
#if 1
	c1 = play ? 0x00 : 0x0a;
	ess_write(sb, 0xb8, c1++);
#else
	c1 = ess_read(sb, 0xb8) & 0xfe;
	ess_write(sb, 0xb8, c1++);
#endif
	/*
	 * update ESS Transfer Count Register
	 */
	ess_write(sb, 0xa4, (u_char)((u_short)c & 0xff));
	ess_write(sb, 0xa5, (u_char)(((u_short)c >> 8) & 0xff));
	/*
	 * set bit 0 of register B8h
	 */
	ess_write(sb, 0xb8, c1);
	if (play)
		sb_cmd(sb, DSP_CMD_SPKON);
	return 0;
}

static int
ess_stop(struct sb_chinfo *ch)
{
	struct sb_info *sb = ch->parent;
	/*
	 * no need to send a stop command if the DMA has already stopped.
	 */
	if (ch->buffer->dl > 0) {
		sb_cmd(sb, DSP_CMD_DMAPAUSE_8); /* pause dma. */
	}
	return 0;
}

static int
ess_abort(struct sb_chinfo *ch)
{
	struct sb_info *sb = ch->parent;
    	int play = (ch->dir == PCMDIR_PLAY)? 1 : 0;
	if (play) sb_cmd(sb, DSP_CMD_SPKOFF); /* speaker off */
	sb_reset_dsp(sb);
	ess_format(ch, ch->fmt);
	ess_speed(ch, ch->channel->speed);
	return 0;
}

/* channel interface */
static void *
sbchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct sb_info *sb = devinfo;
	struct sb_chinfo *ch = (dir == PCMDIR_PLAY)? &sb->pch : &sb->rch;

	ch->parent = sb;
	ch->channel = c;
	ch->buffer = b;
	ch->buffer->bufsize = DSP_BUFFSIZE;
	if (chn_allocbuf(ch->buffer, sb->parent_dmat) == -1) return NULL;
	ch->buffer->chan = (dir == PCMDIR_PLAY)? sb->dma16 : sb->dma8;
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
	sb_format(ch, format);
	return 0;
}

static int
sbchan_setspeed(void *data, u_int32_t speed)
{
	struct sb_chinfo *ch = data;
	return sb_speed(ch, speed);
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
	buf_isadma(ch->buffer, go);
	if (go == PCMTRIG_START) sb_start(ch); else sb_stop(ch);
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
	if (ch->parent->bd_id <= 0x200)
		return p? &sb_playcaps : &sb_reccaps;
	else if (ch->parent->bd_id >= 0x400)
		return p? &sb16_playcaps : &sb16_reccaps;
	else
		return p? &sbpro_playcaps : &sbpro_reccaps;
}
/* channel interface for ESS18xx */
#ifdef notyet
static void *
esschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	/* the same as sbchan_init()? */
}
#endif

static int
esschan_setdir(void *data, int dir)
{
	struct sb_chinfo *ch = data;
	ch->dir = dir;
	return 0;
}

static int
esschan_setformat(void *data, u_int32_t format)
{
	struct sb_chinfo *ch = data;
	ess_format(ch, format);
	return 0;
}

static int
esschan_setspeed(void *data, u_int32_t speed)
{
	struct sb_chinfo *ch = data;
	return ess_speed(ch, speed);
}

static int
esschan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
esschan_trigger(void *data, int go)
{
	struct sb_chinfo *ch = data;
	switch (go) {
	case PCMTRIG_START:
		if (!ch->ess_dma_started)
			buf_isadma(ch->buffer, go);
		ch->ess_dma_started = 1;
		ess_start(ch);
		break;
	case PCMTRIG_STOP:
		if (ch->buffer->dl >= 0) {
			buf_isadma(ch->buffer, go);
			ch->ess_dma_started = 0;
			ess_stop(ch);
		}
		break;
	case PCMTRIG_ABORT:
	default:
		ch->ess_dma_started = 0;
		ess_abort(ch);
		buf_isadma(ch->buffer, go);
		break;
	}
	return 0;
}

static int
esschan_getptr(void *data)
{
	struct sb_chinfo *ch = data;
	return buf_isadmaptr(ch->buffer);
}

static pcmchan_caps *
esschan_getcaps(void *data)
{
	struct sb_chinfo *ch = data;
	return (ch->dir == PCMDIR_PLAY)? &ess_playcaps : &ess_reccaps;
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
		if (sb->bd_flags & BD_F_ESS)
			iomap = &ess_mix;
		else
			iomap = &sbpro_mix;
		break;

    	case BD_F_MIX_CT1745:
		iomap = &sb16_mix;
		break;

    	default:
        	return -1;
    	/* XXX how about the SG NX Pro, iomap = sgnxpro_mix */
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
		if      (src == SOUND_MASK_LINE) 	recdev = 0x06;
		else if (src == SOUND_MASK_CD) 		recdev = 0x02;
		else { /* default: mic */
	    		src = SOUND_MASK_MIC;
	    		recdev = 0;
		}
		sb_setmixer(sb, RECORD_SRC, recdev |
			    (sb_getmixer(sb, RECORD_SRC) & ~0x07));
		break;

    	case BD_F_MIX_CT1745: /* sb16 */
		recdev = 0;
		if (src & SOUND_MASK_MIC)   recdev |= 0x01; /* mono mic */
		if (src & SOUND_MASK_CD)    recdev |= 0x06; /* l+r cd */
		if (src & SOUND_MASK_LINE)  recdev |= 0x18; /* l+r line */
		if (src & SOUND_MASK_SYNTH) recdev |= 0x60; /* l+r midi */
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

#if NPNP > 0
static int
sbpnp_probe(device_t dev)
{
    	char *s = NULL;
    	u_int32_t logical_id = isa_get_logicalid(dev);

    	switch(logical_id) {
    	case 0x01100000: /* @@@1001 */
    		s = "Avance Asound 110";
		break;

    	case 0x01200000: /* @@@2001 */
        	s = "Avance Logic ALS120";
		break;

    	case 0x68187316: /* ESS1868 */
		s = "ESS1868";
		break;

	case 0x69187316: /* ESS1869 */
	case 0xacb0110e: /* Compaq's Presario 1621 ESS1869 */
		s = "ESS1869";
		break;

	case 0x88187316: /* ESS1888 */
		s = "ESS1888";
		break;
    	}
    	if (s) {
		device_set_desc(dev, s);
		return (0);
    	}
    	return ENXIO;
}

static int
sbpnp_attach(device_t dev)
{
    	struct sb_info *sb;
    	u_int32_t vend_id = isa_get_vendorid(dev);

    	sb = (struct sb_info *)malloc(sizeof *sb, M_DEVBUF, M_NOWAIT);
    	if (!sb) return ENXIO;
    	bzero(sb, sizeof *sb);

    	switch(vend_id) {
    	case 0xf0008c0e:
    	case 0x10019305:
    	case 0x20019305:
		/* XXX add here the vend_id for other vibra16X cards... */
		sb->bd_flags = BD_F_SB16X;
    	}
    	return sb_doattach(dev, sb);
}

static device_method_t sbpnp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sbpnp_probe),
	DEVMETHOD(device_attach,	sbpnp_attach),

	{ 0, 0 }
};

static driver_t sbpnp_driver = {
	"pcm",
	sbpnp_methods,
	sizeof(snddev_info),
};

DRIVER_MODULE(sbpnp, isa, sbpnp_driver, pcm_devclass, 0, 0);

#endif /* NPNP > 0 */

#if NSBC > 0
#define DESCSTR " PCM Audio"
static int
sbsbc_probe(device_t dev)
{
    	char *s = NULL;
	struct sndcard_func *func;

	/* The parent device has already been probed. */

	func = device_get_ivars(dev);
	if (func == NULL || func->func != SCF_PCM)
		return (ENXIO);

	s = "SB PCM Audio";

	device_set_desc(dev, s);
	return 0;
}

static int
sbsbc_attach(device_t dev)
{
    	struct sb_info *sb;
    	u_int32_t vend_id;
	device_t sbc;

	sbc = device_get_parent(dev);
	vend_id = isa_get_vendorid(sbc);
    	sb = (struct sb_info *)malloc(sizeof *sb, M_DEVBUF, M_NOWAIT);
    	if (!sb) return ENXIO;
    	bzero(sb, sizeof *sb);

    	switch(vend_id) {
    	case 0xf0008c0e:
    	case 0x10019305:
    	case 0x20019305:
		/* XXX add here the vend_id for other vibra16X cards... */
		sb->bd_flags = BD_F_SB16X;
    	}
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

DRIVER_MODULE(sbsbc, sbc, sbsbc_driver, pcm_devclass, 0, 0);

#endif /* NSBC > 0 */

#endif /* NPCM > 0 */


