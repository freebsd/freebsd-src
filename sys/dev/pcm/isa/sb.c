/*
 * sound/sb_dsp.c
 * 
 * driver for the SoundBlaster and clones.
 * 
 * Copyright 1997 Luigi Rizzo.
 *
 * Derived from files in the Voxware 3.5 distribution,
 * Copyright by Hannu Savolainen 1994, under the same copyright
 * conditions.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

/*
 * use this as a template file for board-specific drivers.
 * The next two lines (and the final #endif) are in all drivers:
 */

#include <i386/isa/snd/sound.h>
#if NPCM > 0

/*
 * Begin with the board-specific include files...
 */

#define __SB_MIXER_C__	/* XXX warning... */
#include  <i386/isa/snd/sbcard.h>

/*
 * then prototypes of functions which go in the snddev_info
 * (usually static, unless they are shared by other modules)...
 */

static	int sb_probe(struct isa_device *dev);
static	int sb_attach(struct isa_device *dev);

static	d_open_t	sb_dsp_open;
static	d_close_t	sb_dsp_close;
static	d_ioctl_t	sb_dsp_ioctl;
static	irq_proc_t	sbintr;
static	snd_callback_t	sb_callback;

/*
 * and prototypes for other private functions defined in this module.
 */

static	void sb_dsp_init(snddev_info *d, struct isa_device *dev);
static	void sb_mix_init(snddev_info *d);
static int sb_mixer_set(snddev_info *d, int dev, int value);
static int dsp_speed(snddev_info *d);
static void sb_mixer_reset(snddev_info *d);

u_int sb_get_byte(int io_base);

/*
 * Then put here the descriptors for the various boards supported
 * by this module, properly initialized.
 */

snddev_info sb_op_desc = {
    "basic soundblaster",

    SNDCARD_SB,
    sb_probe,
    sb_attach,

    sb_dsp_open,
    sb_dsp_close /* sb_close */,
    NULL /* use generic sndread */,
    NULL /* use generic sndwrite */,
    sb_dsp_ioctl,
    sndpoll,

    sbintr,
    sb_callback,

    DSP_BUFFSIZE,	/* bufsize */

    AFMT_STEREO | AFMT_U8,		/* audio format */

} ;

/*
 * Then the file continues with the body of all functions
 * directly referenced in the descriptor.
 */

/*
 * the probe routine for the SoundBlaster only consists in
 * resetting the dsp and testing if it is there.
 * Version detection etc. will be done at attach time.
 *
 * Remember, ISA probe routines are supposed to return the
 * size of io space used.
 */

static int
sb_probe(struct isa_device *dev)
{
    bzero(&pcm_info[dev->id_unit], sizeof(pcm_info[dev->id_unit]) );
    if (dev->id_iobase == -1) {
	dev->id_iobase = 0x220;
	printf("sb_probe: no address supplied, try defaults (0x220,0x240)\n");
        if (snd_conflict(dev->id_iobase))
	    dev->id_iobase = 0x240;
    }
    if (snd_conflict(dev->id_iobase))
	return 0 ;

    if (sb_reset_dsp(dev->id_iobase))
	return 16 ; /* the SB uses 16 registers... */
    else
	return 0;
}

static int
sb_attach(struct isa_device *dev)
{
    snddev_info *d = &pcm_info[dev->id_unit] ;

    dev->id_alive = 16 ; /* number of io ports */
    /* should be already set but just in case... */
    sb_dsp_init(d, dev);
    return 0 ;
}

/*
 * here are the main routines from the switches.
 */

static int
sb_dsp_open(dev_t dev, int flags, int mode, struct proc * p)
{
    snddev_info *d;
    int unit ;

    dev = minor(dev);
    unit = dev >> 4 ;
    d = &pcm_info[unit] ;

    DEB(printf("<%s>%d : open\n", d->name, unit));

    if (d->flags & SND_F_BUSY) {
	printf("<%s>%d open: device busy\n", d->name, unit);
	return EBUSY ;
    }

    d->wsel.si_pid = 0;
    d->wsel.si_flags = 0;

    d->rsel.si_pid = 0;
    d->rsel.si_flags = 0;

    d->flags = 0 ;
    d->bd_flags &= ~BD_F_HISPEED ;

    switch ( dev & 0xf ) {
    case SND_DEV_DSP16 :
	if ((d->audio_fmt & AFMT_S16_LE) == 0) {
	    printf("sorry, 16-bit not supported on SB %d.%02d\n",
		(d->bd_id >>8) & 0xff, d->bd_id & 0xff);
	    return ENXIO;
	}
	d->play_fmt = d->rec_fmt = AFMT_S16_LE ;
	break;
    case SND_DEV_AUDIO :
	d->play_fmt = d->rec_fmt = AFMT_MU_LAW ;
	break ;
    case SND_DEV_DSP :
	d->play_fmt = d->rec_fmt = AFMT_U8 ;
	break ;
    }

    d->flags |= SND_F_BUSY ;
    d->play_speed = d->rec_speed = DSP_DEFAULT_SPEED ;

    if (flags & O_NONBLOCK)
	d->flags |= SND_F_NBIO ;

    sb_reset_dsp(d->io_base);
    ask_init(d);

    return 0;
}

static int
sb_dsp_close(dev_t dev, int flags, int mode, struct proc * p)
{
    int unit;
    snddev_info *d;
    u_long s;

    dev = minor(dev);
    unit = dev >> 4 ;
    d = &pcm_info[unit] ;

    s = spltty();
    d->flags |= SND_F_CLOSING ;
    splx(s);
    snd_flush(d);

    sb_cmd(d->io_base, DSP_CMD_SPKOFF ); /* XXX useless ? */

    d->flags = 0 ;
    return 0 ;
}

static int
sb_dsp_ioctl(dev_t dev, int cmd, caddr_t arg, int mode, struct proc * p)
{
    int unit;
    snddev_info *d;

    dev = minor(dev);
    unit = dev >> 4 ;
    d = &pcm_info[unit] ;

    /*
     * handle mixer calls first. Reads are in the default handler,
     * so do not bother about them.
     */
    if ( (cmd & MIXER_WRITE(0)) == MIXER_WRITE(0) )
	return sb_mixer_set(d, cmd & 0xff, *(int *)arg) ;

    /*
     * for the remaining functions, use the default handler.
     */

    return ENOSYS ;
}

static void
sbintr(int unit)
{
    snddev_info *d = &pcm_info[unit];
    int reason = 3, c=1, io_base = d->io_base;

    DEB(printf("got sbintr for unit %d, flags 0x%08lx\n", unit, d->flags));

    /*
     * SB < 4.0 is half duplex and has only 1 bit for int source,
     * so we fake it. SB 4.x (SB16) has the int source in a separate
     * register.
     */
again:
    if (d->bd_flags & BD_F_SB16) {
	c = sb_getmixer(io_base, IRQ_STAT);
	/* this tells us if the source is 8-bit or 16-bit dma. We
	 * have to check the io channel to map it to read or write...
	 */
	reason = 0 ;
	if ( c & 1 ) { /* 8-bit dma */
	    if (d->dma1 < 4)
		reason |= 1;
	    if (d->dma2 < 4)
		reason |= 2;
	}
	if ( c & 2 ) { /* 16-bit dma */
	    if (d->dma1 >= 4)
		reason |= 1;
	    if (d->dma2 >= 4)
		reason |= 2;
	}
    }
    /* XXX previous location of ack... */
    DEB(printf("sbintr, flags 0x%08lx reason %d\n", d->flags, reason));
    if ( d->dbuf_out.dl && (reason & 1) )
	dsp_wrintr(d);
    if ( d->dbuf_in.dl && (reason & 2) )
	dsp_rdintr(d);

    if ( c & 2 )
	inb(DSP_DATA_AVL16); /* 16-bit int ack */
    if (c & 1)
	inb(DSP_DATA_AVAIL);	/* 8-bit int ack */

    /*
     * the sb16 might have multiple sources etc.
     */
    if (d->bd_flags & BD_F_SB16 && (c & 3) )
	goto again;
}

/*
 * device-specific function called back from the dma module.
 * The reason of the callback is the second argument.
 * NOTE: during operations, some ioctl can be done to change
 * settings (e.g. speed, channels, format), and the default
 * ioctl handler will just record the change and set the
 * flag SND_F_INIT. The callback routine is in charge of applying
 * the changes at the next convenient time (typically, at the
 * start of operations). For full duplex devices, in some cases the
 * init requires both channels to be idle.
 */
static int
sb_callback(snddev_info *d, int reason)
{
    int rd = reason & SND_CB_RD ;
    int l = (rd) ? d->dbuf_in.dl : d->dbuf_out.dl ;

    switch (reason & SND_CB_REASON_MASK) {
    case SND_CB_INIT : /* called with int enabled and no pending io */
	dsp_speed(d);
	snd_set_blocksize(d);
	if (d->play_fmt & AFMT_MU_LAW)
	    d->flags |= SND_F_XLAT8 ;
	else
	    d->flags &= ~SND_F_XLAT8 ;
	reset_dbuf(& (d->dbuf_in), SND_CHAN_RD );
	reset_dbuf(& (d->dbuf_out), SND_CHAN_WR );
	return 1;
	break;

    case SND_CB_START : /* called with int disabled */
	if (d->bd_flags & BD_F_SB16) {
	    /* the SB16 can do full duplex using one 16-bit channel
	     * and one 8-bit channel. It needs to be programmed to
	     * use split format though.
	     * We use the following algorithm:
	     * 1. check which direction(s) are active;
	     * 2. check if we should swap dma channels
	     * 3. check if we can do the swap.
	     */
	    int swap = 1 ; /* default... */

	    if (rd) {
		if (d->flags & SND_F_WRITING || d->dbuf_out.dl)
		    swap = 0;
		if (d->rec_fmt == AFMT_S16_LE && d->dma2 >=4)
		    swap = 0;
		if (d->rec_fmt != AFMT_S16_LE && d->dma2 <4)
		    swap = 0;
	    } else {
		if (d->flags & SND_F_READING || d->dbuf_in.dl)
		    swap = 0;
		if (d->play_fmt == AFMT_S16_LE && d->dma1 >=4)
		    swap = 0;
		if (d->play_fmt != AFMT_S16_LE && d->dma1 <4)
		    swap = 0;
	    }
		
	    if (swap) {
	        int c = d->dma2 ;
		d->dma2 = d->dma1;
		d->dma1 = c ;
		reset_dbuf(& (d->dbuf_in), SND_CHAN_RD );
		reset_dbuf(& (d->dbuf_out), SND_CHAN_WR );
		DEB(printf("START dma chan: play %d, rec %d\n",
		    d->dma1, d->dma2));
	    }
	}
	if (!rd)
	    sb_cmd(d->io_base, DSP_CMD_SPKON);

	if (d->bd_flags & BD_F_SB16) {
	    u_char c, c1 ;

	    if (rd) {
		c = ((d->dma2 > 3) ? DSP_DMA16 : DSP_DMA8) |
			DSP_F16_AUTO |
			DSP_F16_FIFO_ON | DSP_F16_ADC ;
		c1 = (d->rec_fmt == AFMT_U8) ? 0 : DSP_F16_SIGNED ;
		if (d->rec_fmt == AFMT_MU_LAW) c1 = 0 ;
		if (d->rec_fmt == AFMT_S16_LE)
		    l /= 2 ;
	    } else {
		c = ((d->dma1 > 3) ? DSP_DMA16 : DSP_DMA8) |
			DSP_F16_AUTO |
			DSP_F16_FIFO_ON | DSP_F16_DAC ;
		c1 = (d->play_fmt == AFMT_U8) ? 0 : DSP_F16_SIGNED ;
		if (d->play_fmt == AFMT_MU_LAW) c1 = 0 ;
		if (d->play_fmt == AFMT_S16_LE)
		    l /= 2 ;
	    }
 
	    if (d->flags & SND_F_STEREO)
		c1 |= DSP_F16_STEREO ;

	    sb_cmd(d->io_base, c );
	    sb_cmd3(d->io_base, c1 , l - 1) ;
	} else {
	    /* code for the SB2 and SB3 */
	    u_char c ;
	    if (d->bd_flags & BD_F_HISPEED)
		c = (rd) ? DSP_CMD_HSADC_AUTO : DSP_CMD_HSDAC_AUTO ;
	    else
		c = (rd) ? DSP_CMD_ADC8_AUTO : DSP_CMD_DAC8_AUTO ;
	    sb_cmd3(d->io_base, c , l - 1) ;
	}
	break;

    case SND_CB_ABORT : /* XXX */
    case SND_CB_STOP :
	{
	    int cmd = DSP_CMD_DMAPAUSE_8 ; /* default: halt 8 bit chan */
	    if (d->bd_flags & BD_F_SB16) {
		if ( (rd && d->dbuf_in.chan>4) || (!rd && d->dbuf_out.chan>4) )
		    cmd = DSP_CMD_DMAPAUSE_16 ;
	    }
	    if (d->bd_flags & BD_F_HISPEED) {
		sb_reset_dsp(d->io_base);
		d->flags |= SND_F_INIT ;
	    } else {
		sb_cmd(d->io_base, cmd); /* pause dma. */
	       /*
		* This seems to have the side effect of blocking the other
		* side as well so I have to re-enable it :(
		*/
		if ( (rd && d->dbuf_out.dl) ||
		     (!rd && d->dbuf_in.dl) )
		    sb_cmd(d->io_base, cmd == DSP_CMD_DMAPAUSE_8 ?
			0xd6 : 0xd4); /* continue other dma */
	    }
	}
	DEB( sb_cmd(d->io_base, DSP_CMD_SPKOFF) ); /* speaker off */
	break ;

    }
    return 0 ;
}

/*
 * The second part of the file contains all functions specific to
 * the board and (usually) not exported to other modules.
 */

int
sb_reset_dsp(int io_base)
{
    int loopc;

    outb(DSP_RESET, 1);
    DELAY(100);
    outb(DSP_RESET, 0);
    for (loopc = 0; loopc<100 && !(inb(DSP_DATA_AVAIL) & 0x80); loopc++)
	DELAY(30);

    if (inb(DSP_READ) != 0xAA) {
        DEB(printf("sb_reset_dsp 0x%x failed\n", io_base));
	return 0;	/* Sorry */
    }
    return 1;
}

/*
 * only used in sb_attach from here.
 */

static void
sb_dsp_init(snddev_info *d, struct isa_device *dev)
{
    int i, x;
    char *fmt = NULL ;
    int	io_base = dev->id_iobase ;

    d->bd_id = 0 ;

    sb_reset_dsp(io_base);
    sb_cmd(io_base, DSP_CMD_GETVER);	/* Get version */

    for (i = 10000; i; i--) { /* perhaps wait longer on a fast machine ? */
	if (inb(DSP_DATA_AVAIL) & 0x80) { /* wait for Data Ready */
	    if ( (d->bd_id & 0xff00) == 0)
		d->bd_id = inb(DSP_READ) << 8; /* major */
	    else {
		d->bd_id |= inb(DSP_READ); /* minor */
		break;
	    }
	} else
	    DELAY(20);
    }

    /*
     * now do various initializations depending on board id.
     */

    fmt = "SoundBlaster %d.%d" ; /* default */

    switch ( d->bd_id >> 8 ) {
    case 0 :
	printf("\n\nFailed to get SB version (%x) - possible I/O conflict\n\n",
	       inb(DSP_DATA_AVAIL));
	d->bd_id = 0x100;
    case 1 : /* old sound blaster has nothing... */
	break ;

    case 2 :
	d->dma2 = d->dma1 ; /* half duplex */
	d->bd_flags |= BD_F_DUP_MIDI ;

	if (d->bd_id == 0x200)
	    break ; /* no mixer on the 2.0 */
	d->bd_flags &= ~BD_F_MIX_MASK ;
	d->bd_flags |= BD_F_MIX_CT1335 ;

	break ;
    case 4 :
	fmt = "SoundBlaster 16 %d.%d";
	d->audio_fmt |= AFMT_FULLDUPLEX | AFMT_WEIRD | AFMT_S8 | AFMT_S16_LE;
	d->bd_flags |= BD_F_SB16;
	d->bd_flags &= ~BD_F_MIX_MASK ;
	d->bd_flags |= BD_F_MIX_CT1745 ;
	
	/* soft irq/dma configuration */
	x = -1 ;
	if (d->irq == 5) x = 2;
	else if (d->irq == 7) x = 4;
	else if (d->irq == 9) x = 1;
	else if (d->irq == 10) x = 8;
	if (x == -1)
	    printf("<%s>%d: bad irq %d (only 5,7,9,10 allowed)\n",
		d->name, dev->id_unit, d->irq);
	else
	    sb_setmixer(io_base, IRQ_NR, x);

	sb_setmixer(io_base, DMA_NR, (1 << d->dma1) | (1 << d->dma2));
	break ;

    case 3 :
	d->dma2 = d->dma1 ; /* half duplex */
	fmt = "SoundBlaster Pro %d.%d";
	d->bd_flags |= BD_F_DUP_MIDI ;
	d->bd_flags &= ~BD_F_MIX_MASK ;
	d->bd_flags |= BD_F_MIX_CT1345 ;
	if (d->bd_id == 0x301) {
	    int ess_major = 0, ess_minor = 0;

	    /*
	     * Try to detect ESS chips.
	     */

	    sb_cmd(io_base, DSP_CMD_GETID);	/* Return ident. bytes. */

	    for (i = 1000; i; i--) {
		if (inb(DSP_DATA_AVAIL) & 0x80) { /* wait for Data Ready */
		    if (ess_major == 0)
			ess_major = inb(DSP_READ);
		    else {
			ess_minor = inb(DSP_READ);
			break;
		    }
		}
	    }

	    if (ess_major == 0x48 && (ess_minor & 0xf0) == 0x80)
		printf("Hmm... Could this be an ESS488 based card (rev %d)\n",
		   ess_minor & 0x0f);
	    else if (ess_major == 0x68 && (ess_minor & 0xf0) == 0x80)
		printf("Hmm... Could this be an ESS688 based card (rev %d)\n",
		   ess_minor & 0x0f);
	}

	if (d->bd_flags & BD_F_JAZZ16) {
	    if (d->bd_flags & BD_F_JAZZ16_2)
		fmt = "SoundMan Wave %d.%d";
	    else
		fmt = "MV Jazz16 %d.%d";
	    d->audio_fmt |= AFMT_S16_LE;	/* 16 bits */
	}
    }

    sprintf(d->name, fmt, (d->bd_id >> 8) &0xff, d->bd_id & 0xff);

    sb_mix_init(d);
}

static void
sb_mix_init(snddev_info *d)
{
    switch (d->bd_flags & BD_F_MIX_MASK) {
    case BD_F_MIX_CT1345 : /* SB 3.0 has 1345 mixer */

	d->mix_devs = SBPRO_MIXER_DEVICES ;
	d->mix_rec_devs = SBPRO_RECORDING_DEVICES ;
	d->mix_recsrc = SOUND_MASK_MIC ;

	sb_setmixer(d->io_base, 0, 1 ); /* reset mixer */
	sb_setmixer(d->io_base, MIC_VOL , 0x6 ); /* mic volume max */
	sb_setmixer(d->io_base, RECORD_SRC , 0x0 ); /* mic source */
	sb_setmixer(d->io_base, FM_VOL , 0x0 ); /* no midi */
	break ;

    case BD_F_MIX_CT1745 : /* SB16 mixer ... */

	d->mix_devs = SB16_MIXER_DEVICES ;
	d->mix_rec_devs = SB16_RECORDING_DEVICES ;
	d->mix_recsrc = SOUND_MASK_MIC ;
    }
    sb_mixer_reset(d);
}

/*
 * Common code for the midi and pcm functions
 */

int
sb_cmd(int io_base, u_char val)
{
    int  i;

    for (i = 0; i < 1000 ; i++) {
	if ((inb(DSP_STATUS) & 0x80) == 0) {
	    outb(DSP_COMMAND, val);
	    return 1;
	}
	if (i > 10)
	    DELAY (i > 100 ? 1000 : 10 );
    }

    printf("SoundBlaster: DSP Command(0x%02x) timeout. IRQ conflict ?\n", val);
    return 0;
}

int
sb_cmd3(int io_base, u_char cmd, int val)
{
    if (sb_cmd(io_base, cmd)) {
	sb_cmd(io_base, val & 0xff );
	sb_cmd(io_base, (val>>8) & 0xff );
	return 1 ;
    } else
	return 0;
}

int
sb_cmd2(int io_base, u_char cmd, int val)
{
    if (sb_cmd(io_base, cmd)) {
	sb_cmd(io_base, val & 0xff );
	return 1 ;
    } else
	return 0;
}

void
sb_setmixer(int io_base, u_int port, u_int value)
{
    u_long   flags;
  
    flags = spltty();
    outb(MIXER_ADDR, (u_char) (port & 0xff));   /* Select register */
    DELAY(10);
    outb(MIXER_DATA, (u_char) (value & 0xff));
    DELAY(10); 
    splx(flags);
}

u_int
sb_get_byte(int io_base)
{
    int             i;

    for (i = 1000; i; i--)
	if (inb(DSP_DATA_AVAIL) & 0x80)
	    return inb(DSP_READ);
	else
	    DELAY(20);
    return 0xffff;
}

int
sb_getmixer(int io_base, u_int port)
{   
    int             val;
    u_long   flags;
    
    flags = spltty();
    outb(MIXER_ADDR, (u_char) (port & 0xff));   /* Select register */
    DELAY(10);
    val = inb(MIXER_DATA);
    DELAY(10);
    splx(flags);
    
    return val;
}   


/*
 * various utility functions for the DSP
 */

/*
 * dsp_speed updates the speed setting from the descriptor. make sure
 * it is called at spltty().
 * Besides, it takes care of stereo setting.
 */
static int
dsp_speed(snddev_info *d)
{
    u_char   tconst;
    u_long   flags;
    int max_speed = 44100, speed = d->play_speed ;

    if (d->bd_flags & BD_F_SB16) {
	RANGE (speed, 5000, 45000);
	d->play_speed = d->rec_speed = speed ;
	sb_cmd(d->io_base, 0x41);
	sb_cmd(d->io_base, d->play_speed >> 8 );
	sb_cmd(d->io_base, d->play_speed & 0xff );
	sb_cmd(d->io_base, 0x42);
	sb_cmd(d->io_base, d->rec_speed >> 8 );
	sb_cmd(d->io_base, d->rec_speed & 0xff );
	return speed ;
    }
    /*
     * only some models can do stereo, and only if not
     * simultaneously using midi.
     */
    if ( (d->bd_id & 0xff00) < 0x300 || d->bd_flags & BD_F_MIDIBUSY)
	d->flags &= ~SND_F_STEREO;

    /*
     * here enforce speed limitations.
     */
    if (d->bd_id <= 0x200)
	max_speed = 22050; /* max 22050 on SB 1.X */

    /*
     * SB models earlier than SB Pro have low limit for the
     * input rate. Note that this is only for input, but since
     * we do not support separate values for rec & play....
     */
    if (d->bd_id <= 0x200)
	max_speed = 13000;
    else if (d->bd_id < 0x300)
	max_speed = 15000;

    RANGE(speed, 4000, max_speed);

    /*
     * Logitech SoundMan Games and Jazz16 cards can support 44.1kHz
     * stereo
     */
#if !defined (SM_GAMES)
    /*
     * Max. stereo speed is 22050
     */
    if (d->flags & SND_F_STEREO && speed > 22050 && !(d->bd_flags & BD_F_JAZZ16))
	speed = 22050;
#endif

    if (d->flags & SND_F_STEREO)
	speed *= 2;

    /*
     * Now the speed should be valid. Compute the value to be
     * programmed into the board.
     *
     * XXX stereo init is still missing...
     */

    if (speed > 22050) { /* High speed mode on 2.01/3.xx */
	int tmp;

	tconst = (u_char) ((65536 - ((256000000 + speed / 2) / speed)) >> 8) ;
	d->bd_flags |= BD_F_HISPEED ;

	flags = spltty();
	sb_cmd2(d->io_base, DSP_CMD_TCONST, tconst);
	splx(flags);

	tmp = 65536 - (tconst << 8);
	speed = (256000000 + tmp / 2) / tmp;
    } else {
	int             tmp;

	d->bd_flags &= ~BD_F_HISPEED ;
	tconst = (256 - ((1000000 + speed / 2) / speed)) & 0xff;

	flags = spltty();
	sb_cmd2(d->io_base, DSP_CMD_TCONST, tconst);
	splx(flags);

	tmp = 256 - tconst;
	speed = (1000000 + tmp / 2) / tmp;
    }

    if (d->flags & SND_F_STEREO)
	speed /= 2;

    d->play_speed = d->rec_speed = speed;
    return speed;
}

/*
 * mixer support, originally in sb_mixer.c
 */

static void
sb_set_recsrc(snddev_info *d, int mask)
{
    u_char recdev ;

    mask &= d->mix_rec_devs;
    switch (d->bd_flags & BD_F_MIX_MASK) {
    case BD_F_MIX_CT1345 :
	if (mask == SOUND_MASK_LINE)
	    recdev = 6 ;
	else if (mask == SOUND_MASK_CD)
	    recdev = 2 ;
	else { /* default: mic */
	    mask =  SOUND_MASK_MIC ;
	    recdev = 0 ;
	}
	sb_setmixer(d->io_base, RECORD_SRC,
	    recdev | (sb_getmixer(d->io_base, RECORD_SRC) & ~7 ));
	break ;
    case BD_F_MIX_CT1745 : /* sb16 */
	if (mask == 0)
	    mask = SOUND_MASK_MIC ; /* XXX For compatibility. Bug ? */
	recdev = 0 ;
	if (mask & SOUND_MASK_MIC)
	    recdev |= 1 ;
	if (mask & SOUND_MASK_CD)
	    recdev |= 6 ; /* l+r cd */
	if (mask & SOUND_MASK_LINE)
	    recdev |= 0x18 ; /* l+r line */
	if (mask & SOUND_MASK_SYNTH)
	    recdev |= 0x60 ; /* l+r midi */
	sb_setmixer(d->io_base, SB16_IMASK_L, recdev);
	sb_setmixer(d->io_base, SB16_IMASK_R, recdev);
	/*
	 * since the same volume controls apply to the input and
	 * output sections, the best approach to have a consistent
	 * behaviour among cards would be to disable the output path
	 * on devices which are used to record.
	 * However, since users like to have feedback, we only disable
	 * the mike -- permanently.
	 */
        sb_setmixer(d->io_base, SB16_OMASK, 0x1f & ~1);
	break ;
    }
    d->mix_recsrc = mask;
}

static void
sb_mixer_reset(snddev_info *d)
{
    int             i;

    for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	sb_mixer_set(d, i, levels[i]);
    if (d->bd_flags & BD_F_SB16) {
	sb_setmixer(d->io_base, 0x3c, 0x1f); /* make all output active */
	sb_setmixer(d->io_base, 0x3d, 0); /* make all inputs-l off */
	sb_setmixer(d->io_base, 0x3e, 0); /* make all inputs-r off */
    }
    sb_set_recsrc(d, SOUND_MASK_MIC);
}

static int
sb_mixer_set(snddev_info *d, int dev, int value)
{
    int left = value & 0x000000ff;
    int right = (value & 0x0000ff00) >> 8;
    int regoffs;
    u_char   val;
    mixer_tab *iomap;

#ifdef JAZZ16
    if (d->bd_flags & BD_F_JAZZ16 && d->bd_flags & BD_F_JAZZ16_2)
        return smw_mixer_set(dev, value);
#endif

    if (dev == SOUND_MIXER_RECSRC) {
	sb_set_recsrc(d, value);
	return 0 ;
    }
    if (left > 100)
        left = 100;
    if (right > 100)
        right = 100;

    if (dev > 31)
        return EINVAL ;

    if (!(d->mix_devs & (1 << dev)))      /* Not supported */
        return EINVAL;

    switch ( d->bd_flags & BD_F_MIX_MASK ) {
    default:
	/* mixer unknown, fail... */
	return EINVAL ;/* XXX change this */
    case BD_F_MIX_CT1345 :
	iomap = &sbpro_mix ;
	break;
    case BD_F_MIX_CT1745 :
	iomap = &sb16_mix ;
	break;
    /* XXX how about the SG NX Pro, iomap = sgnxpro_mix */
    }
    regoffs = (*iomap)[dev][LEFT_CHN].regno;
    if (regoffs == 0)
        return EINVAL;

    val = sb_getmixer(d->io_base, regoffs);

    change_bits(iomap, &val, dev, LEFT_CHN, left);

    d->mix_levels[dev] = left | (left << 8);

    if ((*iomap)[dev][RIGHT_CHN].regno != regoffs) {    /* Change register */
        sb_setmixer(d->io_base, regoffs, val);     /* Save the old one */
        regoffs = (*iomap)[dev][RIGHT_CHN].regno;

        if (regoffs == 0)
            return 0 ;  /* Just left channel present */

        val = sb_getmixer(d->io_base, regoffs);    /* Read the new one */
    }
    change_bits(iomap, &val, dev, RIGHT_CHN, right);

    sb_setmixer(d->io_base, regoffs, val);

    d->mix_levels[dev] = left | (right << 8);
    return 0 ; /* ok */
}

/*
 * now support for some PnP boards.
 */

#if NPNP > 0
static char *opti925_probe(u_long csn, u_long vend_id);
static void opti925_attach(u_long csn, u_long vend_id, char *name,
        struct isa_device *dev);

static struct pnp_device opti925 = {
        "opti925",
        opti925_probe,
        opti925_attach,
        &nsnd,  /* use this for all sound cards */
        &tty_imask      /* imask */
};
DATA_SET (pnpdevice_set, opti925);
    
static char *
opti925_probe(u_long csn, u_long vend_id)
{   
    if (vend_id == 0x2509143e) {
	struct pnp_cinfo d ;
	read_pnp_parms ( &d , 1 ) ;
	if (d.enable == 0) {
	    printf("This is an OPTi925, but LDN 1 is disabled\n");
	    return NULL;
	}
        return "OPTi925" ;
    }
    return NULL ;
}
    
static void
opti925_attach(u_long csn, u_long vend_id, char *name,
        struct isa_device *dev)
{   
    struct pnp_cinfo d ;
    snddev_info tmp_d ; /* patched copy of the basic snddev_info */
    int the_irq = 0 ; 
    
    tmp_d = sb_op_desc;
    snddev_last_probed = &tmp_d;

    read_pnp_parms ( &d , 3 );  /* disable LDN 3 */
    the_irq = d.irq[0];
    d.port[0] = 0 ;
    d.enable = 0 ;
    write_pnp_parms ( &d , 3 );
    
    read_pnp_parms ( &d , 2 ); /* disable LDN 2 */
    d.port[0] = 0 ;
    d.enable = 0 ;
    write_pnp_parms ( &d , 2 );
 
    read_pnp_parms ( &d , 1 ) ;
    d.irq[0] = the_irq ;
    dev->id_iobase = d.port[0];
    write_pnp_parms ( &d , 1 );
    enable_pnp_card();

    tmp_d.conf_base = d.port[3];

    dev->id_drq = d.drq[0] ; /* primary dma */
    dev->id_irq = (1 << d.irq[0] ) ;
    dev->id_intr = pcmintr ; 
    dev->id_flags = DV_F_DUAL_DMA | (d.drq[1] ) ;

    snddev_last_probed->probe(dev); /* not really necessary but doesn't harm */
    
    pcmattach(dev); 

}

/*
 * A driver for some SB16pnp and compatibles...
 *
 * Avance Asound 100 -- 0x01009305
 * xxx               -- 0x2b008c0e
 *
 */

static char *sb16pnp_probe(u_long csn, u_long vend_id);
static void sb16pnp_attach(u_long csn, u_long vend_id, char *name,
        struct isa_device *dev);

static struct pnp_device sb16pnp = {
        "SB16pnp",
        sb16pnp_probe,
        sb16pnp_attach,
        &nsnd,  /* use this for all sound cards */
        &tty_imask      /* imask */
};
DATA_SET (pnpdevice_set, sb16pnp);
    
static char *
sb16pnp_probe(u_long csn, u_long vend_id)
{   
    char *s = NULL ;

    /*
     * The SB16/AWExx cards seem to differ in the fourth byte of
     * the vendor id, so I have just masked it for the time being...
     * Reported values are:
     * SB16 Value PnP:	0x2b008c0e
     * SB AWE64 PnP:	0x39008c0e 0x9d008c0e 0xc3008c0e
     */
    if ( (vend_id & 0xffffff)  == (0x9d008c0e & 0xffffff) )
	s = "SB16 PnP";
    else if (vend_id == 0x01009305)  
        s = "Avance Asound 100" ;
    if (s) {
	struct pnp_cinfo d; 
	read_pnp_parms(&d, 0); 
	if (d.enable == 0) {
	    printf("This is a %s, but LDN 0 is disabled\n", s);
	    return NULL ;
	}
	return s ;
    }
    return NULL ;
}
    
static void
sb16pnp_attach(u_long csn, u_long vend_id, char *name,
        struct isa_device *dev)
{   
    struct pnp_cinfo d ;
    snddev_info tmp_d ; /* patched copy of the basic snddev_info */
    
    tmp_d = sb_op_desc;
    snddev_last_probed = &tmp_d;

    read_pnp_parms ( &d , 0 ) ;
    d.port[1] = 0 ; /* only the first address is used */
    dev->id_iobase = d.port[0];
    write_pnp_parms ( &d , 0 );
    enable_pnp_card();

    dev->id_drq = d.drq[0] ; /* primary dma */
    dev->id_irq = (1 << d.irq[0] ) ;
    dev->id_intr = pcmintr ; 
    dev->id_flags = DV_F_DUAL_DMA | (d.drq[1] ) ;

    pcm_info[dev->id_unit] = tmp_d;
    snddev_last_probed->probe(dev); /* not really necessary but doesn't harm */
    
    pcmattach(dev); 
}
#endif /* NPNP */    

#endif
