/*
 * snd/sound.c
 * 
 * Main sound driver for FreeBSD. This file provides the main
 * entry points for probe/attach and all i/o demultiplexing, including
 * default routines for generic devices.
 * 
 * (C) 1997 Luigi Rizzo (luigi@iet.unipi.it)
 * 
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * For each card type a template "snddev_info" structure contains
 * all the relevant parameters, both for configuration and runtime.
 *
 * In this file we build tables of pointers to the descriptors for
 * the various supported cards. The generic probe routine scans
 * the table(s) looking for a matching entry, then invokes the
 * board-specific probe routine. If successful, a pointer to the
 * correct snddev_info is stored in snddev_last_probed, for subsequent
 * use in the attach routine. The generic attach routine copies
 * the template to a permanent descriptor (pcm_info[unit] and
 * friends), initializes all generic parameters, and calls the
 * board-specific attach routine.
 *
 * On device calls, the generic routines do the checks on unit and
 * device parameters, then call the board-specific routines if
 * available, or try to perform the task using the default code.
 *
 */

#include "opt_devfs.h"

#include <i386/isa/snd/sound.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */


#if NPCM > 0	/* from "snd.h" */

#define SNDSTAT_BUF_SIZE        4000
static char status_buf[SNDSTAT_BUF_SIZE] ;
static int status_len = 0 ;
static void init_status(snddev_info *d);

static d_open_t sndopen;
static d_close_t sndclose;
static d_ioctl_t sndioctl;
static d_read_t sndread;
static d_write_t sndwrite;
static d_mmap_t sndmmap;

#define CDEV_MAJOR 30
static struct cdevsw snd_cdevsw = {
	sndopen, sndclose, sndread, sndwrite,
	sndioctl, nxstop, nxreset, nxdevtotty,
	sndselect, sndmmap, nxstrategy, "snd",
	NULL, -1,
};

/*
 * descriptors for active devices.
 *
 */
snddev_info pcm_info[NPCM_MAX] ;
snddev_info midi_info[NPCM_MAX] ;
snddev_info synth_info[NPCM_MAX] ;

u_long nsnd = NPCM ;	/* total number of sound devices */

/*
 * the probe routine can only return an int to the upper layer. Hence,
 * it leaves the pointer to the last successfully
 * probed device descriptor in snddev_last_probed
 */
snddev_info *snddev_last_probed = NULL ;

static snddev_info *
generic_snd_probe(struct isa_device * dev, snddev_info **p[], char *s);

/*
 * here are the lists of known cards. Similar cards (e.g. all
 * sb clones, all mss clones, ... are in the same array.
 * All lists of devices of the same type (eg. all pcm, all midi...)
 * are in the same array.
 * Each probe for a device type gets the pointer to the main array
 * and then scans the sublists.
 *
 * XXX should use DATA_SET to create a linker set for sb_devs and other
 * such structures.
 */

extern snddev_info sb_op_desc;
extern snddev_info mss_op_desc;

static snddev_info *sb_devs[] = {	/* all SB clones	 */
    &sb_op_desc,
    NULL,
} ;

static snddev_info *mss_devs[] = {	/* all WSS clones	*/
    &mss_op_desc,
    NULL,
} ;

static snddev_info **pcm_devslist[] = {	/* all pcm devices	*/
    mss_devs,
    sb_devs,
    NULL
} ;


int
pcmprobe(struct isa_device * dev)
{
    bzero(&pcm_info[dev->id_unit], sizeof(pcm_info[dev->id_unit]) );
    return generic_snd_probe(dev, pcm_devslist, "pcm") ? 1 : 0 ;
}

static snddev_info **midi_devslist[] = {/* all midi devices	*/
    NULL
} ;

int
midiprobe(struct isa_device * dev)
{
    bzero(&midi_info[dev->id_unit], sizeof(midi_info[dev->id_unit]) );
    return 0 ;
    return generic_snd_probe(dev, midi_devslist, "midi") ? 1 : 0 ;
}

int
synthprobe(struct isa_device * dev)
{
    bzero(&synth_info[dev->id_unit], sizeof(synth_info[dev->id_unit]) );
    return 0 ;
}

/*
 * this is the generic attach routine
 */

int
pcmattach(struct isa_device * dev)
{
    snddev_info *d = NULL ;
    struct isa_device *dvp;
    int stat = 0;
    dev_t isadev;
    void *cookie;

    if ( (dev->id_unit >= NPCM_MAX) ||		/* too many devs	*/
	 (snddev_last_probed == NULL) ||	/* last probe failed	*/
	 (snddev_last_probed->attach==NULL) )	/* no attach routine	*/
	return 0 ; /* fail */

    /*
     * default initialization: copy generic parameters for the routine,
     * initialize from the isa_device structure, and allocate memory.
     * If everything succeeds, then call the attach routine for
     * further initialization.
     */
    pcm_info[dev->id_unit] = *snddev_last_probed ;
    d = &pcm_info[dev->id_unit] ;

    d->io_base = dev->id_iobase ;
    d->irq = ffs(dev->id_irq) - 1 ;
    d->dbuf_out.chan = dev->id_drq ;
    if (dev->id_flags != -1 && dev->id_flags & DV_F_DUAL_DMA &&
	    (dev->id_flags & DV_F_DRQ_MASK) != 4 ) /* enable dma2 */
	d->dbuf_in.chan = dev->id_flags & DV_F_DRQ_MASK ;
    else
	d->dbuf_in.chan = d->dbuf_out.chan ;
    /* XXX should also set bd_id from flags ? */
    d->status_ptr = 0;

    /*
     * Allocates memory and initializes the dma structs properly. We
     * use independent buffers for each channel.  For the time being,
     * this is done independently of the dma setting. In future
     * revisions, if we see that we have a single dma, we might decide
     * to use a single buffer to save memory.
     */
    alloc_dbuf( &(d->dbuf_out), d->bufsize );
    alloc_dbuf( &(d->dbuf_in), d->bufsize );

    isa_dma_acquire(d->dbuf_out.chan);
    if (FULL_DUPLEX(d))
	isa_dma_acquire(d->dbuf_in.chan);
    /*
     * initialize standard parameters for the device. This can be
     * overridden by device-specific configurations but better do
     * here the generic things.
     */

    d->play_speed = d->rec_speed = 8000 ;
    d->play_blocksize = d->rec_blocksize = 2048 ;
    d->play_fmt = d->rec_fmt = AFMT_MU_LAW ;

    isadev = makedev(CDEV_MAJOR, 0);
    cdevsw_add(&isadev, &snd_cdevsw, NULL);

#ifdef DEVFS
#define GID_SND GID_GAMES
#define UID_SND UID_ROOT
#define PERM_SND 0660
    /*
     * XXX remember to store the returned tokens if you want to
     * be able to remove the device later
     *
     * Make links to first successfully probed unit.
     * Attempts by later devices to make these links will fail.
     */
    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_DSP,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "dsp%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "dsp");

    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_DSP16,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "dspW%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "dspW");

    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_AUDIO,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "audio%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "audio");

    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_CTL,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "mixer%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "mixer");

    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_STATUS,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "sndstat%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "sndstat");

#if 0 /* these two are still unsupported... */
    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_MIDIN,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "midi%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "midi");

    cookie = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_SYNTH,
	DV_CHR, UID_SND, GID_SND, PERM_SND, "sequencer%n", dev->id_unit);
    if (cookie) devfs_link(cookie, "sequencer");
#endif
#endif /* DEVFS */

    /*
     * should try and find a suitable value for id_id, otherwise
     * the interrupt is not registered and dispatched properly.
     * This is important for PnP devices, where "dev" is built on
     * the fly and many field are not initialized.
     */
    if (dev->id_driver == NULL) {
	dev->id_driver = &pcmdriver ;
	dvp=find_isadev(isa_devtab_tty, &pcmdriver, 0);
	if (dvp)
	    dev->id_id = dvp->id_id;
    }

    d->magic = MAGIC(dev->id_unit); /* debugging... */
    /*
     * and finally, call the device attach routine
     * XXX I should probably use d->attach(dev)
     */
    stat = snddev_last_probed->attach(dev);
#if 0
    /*
     * XXX hooks for synt support. Try probe and attach...
     */
    if (d->synth_base && opl3_probe(dev) ) {
	opl3_attach(dev);
    }
#endif
    snddev_last_probed = NULL ;

    return stat ;
}

int midiattach(struct isa_device * dev) { return 0 ; }
int synthattach(struct isa_device * dev) { return 0 ; }

struct isa_driver pcmdriver = { pcmprobe, pcmattach, "pcm" } ;

struct isa_driver mididriver = { midiprobe, midiattach, "midi" } ;
struct isa_driver synthdriver = { synthprobe, synthattach, "synth" } ;

void
pcmintr(int unit)
{
    DEB(printf("__/\\/ pcmintr -- unit %d\n", unit));
    pcm_info[unit].interrupts++;
    if (pcm_info[unit].isr)
	pcm_info[unit].isr(unit);
#if 0 /* these do not exist at the moment. */
    if (midi_info[unit].isr)
	midi_info[unit].isr(unit);
    if (synth_info[unit].isr)
	synth_info[unit].isr(unit);
#endif
}

static snddev_info *
generic_snd_probe(struct isa_device * dev, snddev_info **p[], char *s)
{
    snddev_info **q ;
    struct isa_device saved_dev ;

    snddev_last_probed = NULL ;

    saved_dev = *dev ; /* the probe routine might alter parameters */

    /*
     * XXX todo: should try to match flags with device type.
     */
    for ( ; p[0] != NULL ; p++ )
	for ( q = *p ; q[0] ; q++ )
	    if (q[0]->probe && q[0]->probe(dev))
		return (snddev_last_probed = q[0]) ;
	    else
		*dev = saved_dev ;

    return NULL ;
}


/*
 * a small utility function which, given a device number, returns
 * a pointer to the associated snddev_info struct, and sets the unit
 * number.
 */
static snddev_info *
get_snddev_info(dev_t dev, int *unit)
{
    int u;
    snddev_info *d = NULL ;

    dev = minor(dev);
    u = dev >> 4 ;
    if (unit)
	*unit = u ;

    if (u >= NPCM_MAX ||
	( pcm_info[u].io_base == 0 && (dev & 0x0f) != SND_DEV_STATUS)) {
	int i;
	for (i = 0 ; i < NPCM_MAX ; i++)
	    if (pcm_info[i].io_base)
		break ;
	if (i != NPCM_MAX) 
	    printf("pcm%d: unit not configured, perhaps you want pcm%d ?\n",
		 u, i);
	else
	    printf("no pcm units configured\b");
	return NULL ;
    }
    switch(dev & 0x0f) {
    case SND_DEV_CTL :	/* /dev/mixer handled by pcm */
    case SND_DEV_STATUS : /* /dev/sndstat handled by pcm */
    case SND_DEV_SNDPROC :	/* /dev/sndproc handled by pcm */
    case SND_DEV_DSP :
    case SND_DEV_DSP16 :
    case SND_DEV_AUDIO :
    case SND_DEV_SEQ : /* XXX when enabled... */
	d = & pcm_info[u] ;
	break ;
    case SND_DEV_SEQ2 :
    case SND_DEV_MIDIN:
    default:
	printf("unsupported subdevice %d\n", dev & 0xf);
	return NULL ;
    }
    return d ;
}

/*
 * here are the switches for the main functions. The switches do
 * all necessary checks on the device number to make sure
 * that the device is configured. They also provide some default
 * functionalities so that device-specific drivers have to deal
 * only with special cases.
 */

static int
sndopen(dev_t i_dev, int flags, int mode, struct proc * p)
{
    int dev, unit ;
    snddev_info *d;

    dev = minor(i_dev);
    d = get_snddev_info(dev, &unit);

    DEB(printf("open snd%d subdev %d flags 0x%08x mode 0x%08x\n",
	unit, dev & 0xf, flags, mode));

    if (d == NULL)
	return (ENXIO) ;

    switch(dev & 0x0f) {
    case SND_DEV_SEQ:	/* sequencer. Hack... */
#if 0 /* XXX hook for opl3 support */
	if (d->synth_base)
	    return opl3_open(i_dev, flags, mode, p);
	else
#endif
	    return ENXIO ;

    case SND_DEV_CTL : /* mixer ... */
	return 0 ;	/* always succeed */

    case SND_DEV_STATUS : /* implemented right here */
	init_status(&pcm_info[unit]);
	d->status_ptr = 0 ;
	return 0 ;

    default:
	if (d->open == NULL) {
	    printf("open: unit %d not configured, perhaps you want unit %d ?\n",
		unit, unit+1 );
	    return (ENXIO) ;
	} else
	    return d->open(i_dev, flags, mode, p);
    }
    return ENXIO ;
}

static int
sndclose(dev_t i_dev, int flags, int mode, struct proc * p)
{
    int dev, unit ;
    snddev_info *d;

    dev = minor(i_dev);
    d = get_snddev_info(dev, &unit);

    DEB(printf("close snd%d subdev %d\n", unit, dev & 0xf));

    if (d == NULL)
	return (ENXIO) ;

    switch(dev & 0xf) { /* only those for which close makes sense */
    case SND_DEV_SEQ:
#if 0	/* XXX hook for opl3 support */
	if (d->synth_base)
		return opl3_close(i_dev, flags, mode, p);
	else
#endif
		return ENXIO ;

    case SND_DEV_AUDIO :
    case SND_DEV_DSP :
    case SND_DEV_DSP16 :
	if (d->close)
	    return d->close(i_dev, flags, mode, p);
    }
    return 0 ;
}

static int
sndread(dev_t i_dev, struct uio * buf, int flag)
{
    int ret, dev, unit;
    snddev_info *d ;
    u_long s;

    dev = minor(i_dev);

    d = get_snddev_info(dev, &unit);
    DEB(printf("read snd%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

    if (d == NULL)
	return ENXIO ;

    if ( (dev & 0x0f) == SND_DEV_STATUS ) {
	int l, c;
	u_char *p;

	l = buf->uio_resid;
	s=spltty();
	c = status_len - d->status_ptr ;
	if (c < 0) /* should not happen! */
	    c = 0 ;
	if (c < l)
	    l = c ;
	p = status_buf + d->status_ptr ;
	d->status_ptr += l ;
	splx(s);
	return uiomove(p, l, buf) ;
    }

    if (d->read)	/* device-specific read */
	return d->read(i_dev, buf, flag);

    /*
     * the generic read routine. device-specific stuff should only
     * be in the dma-handling procedures.
     */
    s = spltty();
    if ( d->flags & SND_F_READING ) {
        /* another reader is in, deny request */
        splx(s);
	DDB(printf("read denied, another reader is in\n"));
	/*
	 * sleep for a while to avoid killing the machine.
	 */
	tsleep( (void *)s, PZERO, "sndar", hz ) ;
        return EBUSY ;
    }
    if ( ! FULL_DUPLEX(d) ) {           /* half duplex */
        if ( d->flags & SND_F_WRITING ) {
            /* another writer is in, deny request */
            splx(s);
	    DDB(printf("read denied, half duplex and a writer is in\n"));
	    tsleep( (void *)s, PZERO, "sndaw", hz ) ;
            return EBUSY ;
        }
        while ( d->dbuf_out.dl ) {
	    /*
	     * we have a pending dma operation, post a read request
	     * and wait for the write to complete.
	     */
            d->flags |= SND_F_READING ;
            DEB(printf("sndread: sleeping waiting for write to end\n"));
            ret = tsleep( (caddr_t)&(d->dbuf_out),
                 PRIBIO | PCATCH , "sndrdw", hz ) ;
            if (ret == ERESTART || ret == EINTR) {
                d->flags &= ~SND_F_READING ;
                splx(s);
                return EINTR ;
            }
        }
    }
    d->flags |= SND_F_READING ;
    splx(s);
    
    return dsp_read_body(d, buf);
}

static int
sndwrite(dev_t i_dev, struct uio * buf, int flag)
{
    int ret, dev, unit;
    snddev_info *d;
    u_long s;

    dev = minor(i_dev);
    d = get_snddev_info(dev, &unit);

    DEB(printf("write snd%d subdev %d flag 0x%08x\n", unit, dev & 0xf, flag));

    if (d == NULL)
	return (ENXIO) ;

    switch( dev & 0x0f) {	/* only writeable devices */
    case SND_DEV_MIDIN:	/* XXX is this writable ? */
    case SND_DEV_SEQ :
    case SND_DEV_SEQ2 :
    case SND_DEV_DSP :
    case SND_DEV_DSP16 :
    case SND_DEV_AUDIO :
	break ;
    default:
	return EPERM ; /* for non-writeable devices ; */
    }
    if (d->write)
	return d->write(i_dev, buf, flag);

    /*
     * Otherwise, use the generic write routine. device-specific
     * stuff should only be in the dma-handling procedures.
     */

    s = spltty();
    if ( d->flags & SND_F_WRITING ) {
        /* another writer is in, deny request */
        splx(s);
	DDB(printf("write denied, another writer is in\n"));
	tsleep( (void *)s, PZERO , "sndaw", hz ) ;
        return EBUSY ;
    }
    if ( ! FULL_DUPLEX(d) ) {           /* half duplex */
        if ( d->flags & SND_F_READING ) {
            /* another reader is in, deny request */
            splx(s);
	    DDB(printf("write denied, half duplex and a reader is in\n"));
	    tsleep( (void *)s, PZERO, "sndar", hz ) ;
            return EBUSY ;
        }
        while ( d->dbuf_in.dl ) {
	    /*
	     * we have a pending read dma. Post a write request
	     * and wait for the read to complete (in fact I could
	     * abort the read dma...
	     */
            d->flags |= SND_F_WRITING ;
            DEB(printf("sndwrite: sleeping waiting for read to end\n"));
            ret = tsleep( (caddr_t)&(d->dbuf_out),
                 PRIBIO | PCATCH , "sndwr", hz ) ;
            if (ret == ERESTART || ret == EINTR) {
                d->flags &= ~SND_F_WRITING ;
                splx(s);
                return EINTR ;
            }
        }
    }
    d->flags |= SND_F_WRITING ;
    splx(s);
    
    return dsp_write_body(d, buf);
}

/*
 * generic sound ioctl. Functions of the default driver can be
 * overridden by the device-specific ioctl call.
 * If a device-specific call returns ENOSYS (Function not implemented),
 * the default driver is called. Otherwise, the returned value
 * is passed up.
 *
 * The default handler, for many parameters, sets the value in the
 * descriptor, sets SND_F_INIT, and calls the callback function with
 * reason INIT. If successful, the callback returns 1 and the caller
 * can update the parameter.
 */

static int
sndioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
    int ret = ENOSYS, dev, unit ;
    snddev_info *d;
    u_long s;

    dev = minor(i_dev);
    d = get_snddev_info(dev, &unit);

    if (d == NULL)
	return (ENXIO) ;

    if ( (dev & 0x0f) == SND_DEV_SEQ ) {
	/* sequencer. Hack... */
#if 0
	if (d->synth_base)
	    return opl3_ioctl(i_dev, cmd, arg, mode, p) ;
	else
#endif
	    return ENXIO ;
    }
    if (d->ioctl)
	ret = d->ioctl(dev, cmd, arg, mode, p);
    if (ret != ENOSYS)
	return ret ;

    /*
     * pass control to the default ioctl handler. Set ret to 0 now.
     */
    ret = 0 ;

    /*
     * The linux ioctl interface for the sound driver has a thousand
     * different calls, and it is unpractical to put the names in
     * the switch().  So we have some tests before for common routines,
     * such as the ones related to the mixer.  But we really ought
     * to redesign the interface!
     *
     * Reading from the mixer just requires to look at the cached
     * copy in d->mix_levels[dev], so this routine should cover
     * practically all needs for mixer reading.
     */
    if ( (cmd & MIXER_READ(0)) == MIXER_READ(0) && (cmd & 0xff) < 32 ) {
	int dev = cmd & 0x1f ;
	if ( d->mix_devs & (1<<dev) ) { /* supported */
	    *(int *)arg = d->mix_levels[dev];
	    return 0 ;
	} else
	    return EINVAL ;
    }

    /*
     * all routines are called with int. blocked. Make sure that
     * ints are re-enabled when calling slow or blocking functions!
     */
    s = spltty();
    switch(cmd) {

    /*
     * we start with the new ioctl interface.
     */
    case AIONWRITE :	/* how many bytes can write ? */
	if (d->dbuf_out.dl)
	    dsp_wr_dmaupdate(&(d->dbuf_out));
	*(int *)arg = d->dbuf_out.fl;
	break;

    case AIOSSIZE :     /* set the current blocksize */
	{
	    struct snd_size *p = (struct snd_size *)arg;
	    if (p->play_size <= 1 && p->rec_size <= 1) { /* means no blocks */
		d->flags &= ~SND_F_HAS_SIZE ;
	    } else {
		RANGE (p->play_size, 40, d->dbuf_out.bufsize /4);
		d->play_blocksize = p->play_size  & ~3 ;
		RANGE (p->rec_size, 40, d->dbuf_in.bufsize /4);
		d->rec_blocksize = p->rec_size  & ~3 ;
		d->flags |= SND_F_HAS_SIZE ;
	    }
	}
	splx(s);
	ask_init(d);
	/* FALLTHROUGH */
    case AIOGSIZE :	/* get the current blocksize */
	{
	    struct snd_size *p = (struct snd_size *)arg;
	    p->play_size = d->play_blocksize ;
	    p->rec_size = d->rec_blocksize ;
	}
	break ;

    case AIOSFMT :
	{
	    snd_chan_param *p = (snd_chan_param *)arg;
	    d->play_speed = p->play_rate;
	    d->rec_speed = p->play_rate; /* XXX one speed allowed */
	    if (p->play_format & SND_F_STEREO)
		d->flags |= SND_F_STEREO ;
	    else
		d->flags &= ~SND_F_STEREO ;
	    d->play_fmt = p->play_format & ~AFMT_STEREO ;
	    d->rec_fmt = p->rec_format & ~AFMT_STEREO ;
	}
	splx(s);
	if (!ask_init(d))
	    break ; /* could not reinit */
	/* FALLTHROUGH */

    case AIOGFMT :
	{
	    snd_chan_param *p = (snd_chan_param *)arg;
	    p->play_rate = d->play_speed;
	    p->rec_rate = d->rec_speed;
	    p->play_format = d->play_fmt;
	    p->rec_format = d->rec_fmt;
	    if (d->flags & SND_F_STEREO) {
		p->play_format |= AFMT_STEREO ;
		p->rec_format |= AFMT_STEREO ;
	    }
	}
	break;

    case AIOGCAP :     /* get capabilities */
	/* this should really be implemented by the driver */
	{
	    snd_capabilities *p = (snd_capabilities *)arg;
	    p->rate_min = 5000;
	    p->rate_max = 48000; /* default */
	    p->bufsize = d->bufsize;
	    p->formats = d->audio_fmt; /* default */
	    p->mixers = 1 ; /* default: one mixer */
	    p->inputs = d->mix_devs ;
	    p->left = p->right = 255 ;
	}
	break ;

    case AIOSTOP:
	if (*(int *)arg == AIOSYNC_PLAY) /* play */
	    *(int *)arg = dsp_wrabort(d, 1 /* restart */);
	else if (*(int *)arg == AIOSYNC_CAPTURE)
	    *(int *)arg = dsp_rdabort(d, 1 /* restart */);
	else {
	    splx(s);
	    printf("AIOSTOP: bad channel 0x%x\n", *(int *)arg);
	    *(int *)arg = 0 ;
	}
	break ;

    case AIOSYNC:
	printf("AIOSYNC chan 0x%03lx pos %d unimplemented\n",
	    ((snd_sync_parm *)arg)->chan,
	    ((snd_sync_parm *)arg)->pos);
	break;
    /*
     * here follow the standard ioctls (filio.h etc.)
     */
    case FIONREAD : /* get # bytes to read */
	if ( d->dbuf_in.dl )
	    dsp_rd_dmaupdate(&(d->dbuf_in));
	*(int *)arg = d->dbuf_in.rl;
	break;

    case FIOASYNC: /*set/clear async i/o */
	printf("FIOASYNC\n");
	break;

    case SNDCTL_DSP_NONBLOCK :
    case FIONBIO : /* set/clear non-blocking i/o */
	if ( *(int *)arg == 0 )
	    d->flags &= ~SND_F_NBIO ;
	else
	    d->flags |= SND_F_NBIO ;
	break ;

    /*
     * Finally, here is the linux-compatible ioctl interface
     */
    case SNDCTL_DSP_GETBLKSIZE:
	*(int *) arg = d->play_blocksize ;
	break ;

    case SNDCTL_DSP_SETBLKSIZE :
	{
	    int t = *(int *)arg;
	    if (t <= 1) { /* means no blocks */
		d->flags &= ~SND_F_HAS_SIZE ;
	    } else {
		RANGE (t, 40, d->dbuf_out.bufsize /4);
		d->play_blocksize =
		d->rec_blocksize = t  & ~3 ; /* align to multiple of 4 */
		d->flags |= SND_F_HAS_SIZE ;
	    }
	}
	splx(s);
	ask_init(d);
	break ;
    case SNDCTL_DSP_RESET:
	DEB(printf("dsp reset\n"));
	dsp_wrabort(d, 1 /* restart */);
	dsp_rdabort(d, 1 /* restart */);
	break ;

    case SNDCTL_DSP_SYNC:
	DEB(printf("dsp sync\n"));
	splx(s);
	snd_sync(d, 1, d->dbuf_out.bufsize - 4); /* DMA does not start with <4 bytes */
	break ;

    case SNDCTL_DSP_SPEED:
	d->play_speed = d->rec_speed = *(int *)arg ;
	splx(s);
	if (ask_init(d))
	    *(int *)arg = d->play_speed ;
	break ;

    case SNDCTL_DSP_STEREO:
	if ( *(int *)arg == 0 )
	    d->flags &= ~SND_F_STEREO ; /* mono */
	else if ( *(int *)arg == 1 )
	    d->flags |= SND_F_STEREO ; /* stereo */
	else {
	    printf("dsp stereo: %d is invalid, assuming 1\n", *(int *)arg );
	    d->flags |= SND_F_STEREO ; /* stereo */
	}
	splx(s);
	if (ask_init(d))
	    *(int *)arg = (d->flags & SND_F_STEREO) ? 1 : 0 ;
	break ;

    case SOUND_PCM_WRITE_CHANNELS:
	if ( *(int *)arg == 1)
	    d->flags &= ~SND_F_STEREO ; /* mono */
	else if ( *(int *)arg == 2)
	    d->flags |= SND_F_STEREO ; /* stereo */
	else {
	    ret = EINVAL ;
	    break ;
	}
	splx(s);
	if (ask_init(d))
	    *(int *)arg = (d->flags & SND_F_STEREO) ? 2 : 1 ;
	break ;

    case SOUND_PCM_READ_RATE:
	*(int *)arg = d->play_speed;
	break ;

    case SOUND_PCM_READ_CHANNELS:
	*(int *)arg = (d->flags & SND_F_STEREO) ? 2 : 1;
	break ;

    case SNDCTL_DSP_GETFMTS:	/* returns a mask of supported fmts */
	*(int *)arg = (int)d->audio_fmt ;
	break ;

    case SNDCTL_DSP_SETFMT:	/* sets _one_ format */
	/*
	 * when some card (SB16) is opened RDONLY or WRONLY,
	 * only one of the fields is set, the other becomes 0.
	 * This makes it possible to select DMA channels at runtime.
	 */
	if (d->play_fmt)
	    d->play_fmt = *(int *)arg ;
	if (d->rec_fmt)
	    d->rec_fmt = *(int *)arg ;
	splx(s);
	if (ask_init(d))
	    *(int *)arg = d->play_fmt ;
	break ;

    case SNDCTL_DSP_SUBDIVIDE:
	/* XXX watch out, this is RW! */
	DEB(printf("SNDCTL_DSP_SUBDIVIDE yet unimplemented\n");)
	break;

    case SNDCTL_DSP_SETFRAGMENT:
	/* XXX watch out, this is RW! */
	DEB(printf("SNDCTL_DSP_SETFRAGMENT 0x%08x\n", *(int *)arg));
	{
	    int bytes, count;
	    bytes = *(int *)arg & 0xffff ;
	    count = ( *(int *)arg >> 16) & 0xffff ;
	    if (bytes > 15)
		bytes = 15 ;
	    bytes = 1 << bytes ;
	    if (bytes <= 1) { /* means no blocks */
		d->flags &= ~SND_F_HAS_SIZE ;
	    } else {
		RANGE (bytes, 40, d->dbuf_out.bufsize /4);
		d->play_blocksize =
		d->rec_blocksize = bytes  & ~3 ; /* align to multiple of 4 */
		d->flags |= SND_F_HAS_SIZE ;
	    }
	    splx(s);
	    ask_init(d);
#if 0
	    /* XXX todo: set the buffer size to the # of fragments */
	    count = d->dbuf_in.bufsize / d->play_blocksize ;
	    bytes = ffs(d->play_blocksize) - 1;
	    /*
	     * don't change arg, since it's fake anyways and some
	     * programs might fail if we do.
	     */
	    *(int *)arg = (count << 16) | bytes ;
#endif
	}
	break ;

    case SNDCTL_DSP_GETISPACE:
	/* return space available in the input queue */
	{
	    audio_buf_info *a = (audio_buf_info *)arg;
	    snd_dbuf *b = &(d->dbuf_in);
	    if (b->dl)
		dsp_rd_dmaupdate( b );
	    a->bytes = d->dbuf_in.fl ;
	    a->fragments = 1 ;
	    a->fragstotal = b->bufsize / d->rec_blocksize ;
	    a->fragsize = d->rec_blocksize ;
	}
	break ;

    case SNDCTL_DSP_GETOSPACE:
	/* return space available in the output queue */
	{
	    audio_buf_info *a = (audio_buf_info *)arg;
	    snd_dbuf *b = &(d->dbuf_out);
	    if (b->dl)
		dsp_wr_dmaupdate( b );
	    a->bytes = d->dbuf_out.fl ;
	    a->fragments = 1 ;
	    a->fragstotal = b->bufsize / d->play_blocksize ;
	    a->fragsize = d->play_blocksize ;
	}
	break ;

    case SNDCTL_DSP_GETIPTR:
	{
	    count_info *a = (count_info *)arg;
	    snd_dbuf *b = &(d->dbuf_in);
	    if (b->dl)
		dsp_rd_dmaupdate( b );
	    a->bytes = b->total;
	    a->blocks = (b->total - b->prev_total +
		    d->rec_blocksize -1 ) / d->rec_blocksize ;
	    a->ptr = b->fp ; /* XXX not sure... */
	    b->prev_total = b->total ;
	}
	break;

    case SNDCTL_DSP_GETOPTR:
	{
	    count_info *a = (count_info *)arg;
	    snd_dbuf *b = &(d->dbuf_out);
	    if (b->dl)
		dsp_wr_dmaupdate( b );
	    a->bytes = b->total;
	    a->blocks = (b->total - b->prev_total
		    /* +d->play_blocksize -1*/ ) / d->play_blocksize ;
	    a->ptr = b->rp ; /* XXX not sure... */
	    b->prev_total = b->total ;
	}
	break;

    case SNDCTL_DSP_GETCAPS :
	*(int *) arg = 0x0 ; /* revision */
	if (FULL_DUPLEX(d))
		*(int *) arg |= DSP_CAP_DUPLEX ;
	*(int *) arg |= DSP_CAP_REALTIME ;
	break ;

    case SOUND_PCM_READ_BITS:
	if (d->play_fmt == AFMT_S16_LE)
	    *(int *) arg = 16 ;
	else
	    *(int *) arg = 8 ;
	break ;

    /*
     * mixer calls
     */

    case SOUND_MIXER_READ_DEVMASK :
    case SOUND_MIXER_READ_CAPS :
    case SOUND_MIXER_READ_STEREODEVS :
	*(int *)arg = d->mix_devs;
	break ;

    case SOUND_MIXER_READ_RECMASK :
	*(int *)arg = d->mix_rec_devs;
	break ;

    case SOUND_MIXER_READ_RECSRC :
	*(int *)arg = d->mix_recsrc ;
	break;

    default:
	DEB(printf("default ioctl snd%d subdev %d fn 0x%08x fail\n",
	    unit, dev & 0xf, cmd));
	ret = EINVAL;
	break ;
    }
    splx(s);
    return ret ;
}

/*
 * we use the name 'select', but the new "poll" interface this is
 * really sndpoll. Second arg for poll is not "rw" but "events"
 */
int
sndselect(dev_t i_dev, int rw, struct proc *p)
{
    int dev, unit, c = 1 /* default: success */ ;
    snddev_info *d ;
    u_long flags;

    dev = minor(i_dev);
    d = get_snddev_info(dev, &unit);
    DEB(printf("sndselect dev 0x%04x rw 0x%08x\n",i_dev, rw));
    if (d == NULL ) /* should not happen! */
	return (ENXIO) ;
    if (d->select == NULL)
        return ( (rw & (POLLIN|POLLOUT|POLLRDNORM|POLLWRNORM)) | POLLHUP);
    else if (d->select != sndselect )
	return d->select(i_dev, rw, p);
    else {
	/* handle it here with the generic code */
	/*
	 * if the user selected a block size, then we want to use the
	 * device as a block device, and select will return ready when
	 * we have a full block.
	 * In all other cases, select will return when 1 byte is ready.
	 */
	int lim = 1;

	int revents = 0 ;
	if (rw & (POLLOUT | POLLWRNORM) ) {
	    if ( d->flags & SND_F_HAS_SIZE )
		lim = d->play_blocksize ;
	    /* XXX fix the test here for half duplex devices */
	    if (1 /* write is compatible with current mode */) {
		flags = spltty();
		if (d->dbuf_out.dl)
		    dsp_wr_dmaupdate(&(d->dbuf_out));
		c = d->dbuf_out.fl ;
		if (c < lim) /* no space available */
		    selrecord(p, & (d->wsel));
		else
		    revents |= rw & (POLLOUT | POLLWRNORM);
		splx(flags);
	    }
        }
        if (rw & (POLLIN | POLLRDNORM)) {
	    if ( d->flags & SND_F_HAS_SIZE )
		lim = d->rec_blocksize ;
	    /* XXX fix the test here */
	    if (1 /* read is compatible with current mode */) {
		flags = spltty();
		if ( d->dbuf_in.dl == 0 ) /* dma idle, restart it */
		    dsp_rdintr(d);
		else
		    dsp_rd_dmaupdate(&(d->dbuf_in));
		c = d->dbuf_in.rl ;
		if (c < lim) /* no data available */
		    selrecord(p, & (d->rsel));
		else
		    revents |= rw & (POLLIN | POLLRDNORM);
		splx(flags);
	    }
	    DEB(printf("sndselect on read: %d >= %d flags 0x%08x\n",
		c, lim, d->flags));
	    return c < lim ? 0 : 1 ;
	}
	return revents;
    }
    return ENXIO ; /* notreached */
}

/*
 * The mmap interface allows access to the play and read buffer,
 * plus the device descriptor.
 * The various blocks are accessible at the following offsets:
 *
 * 0x00000000 ( 0   ) : write buffer ;
 * 0x01000000 (16 MB) : read buffer ;
 * 0x02000000 (32 MB) : device descriptor (dangerous!)
 *
 * WARNING: the mmap routines assume memory areas are aligned. This
 * is true (probably) for the dma buffers, but likely false for the
 * device descriptor. As a consequence, we do not know where it is
 * located in the requested area.
 */
#include <sys/mman.h>
#include <vm/vm.h>      
#include <vm/vm_kern.h> 
#include <vm/vm_param.h>
#include <vm/pmap.h>    
#include <vm/vm_extern.h>

static int
sndmmap(dev_t dev, int offset, int nprot)
{
    snddev_info *d = get_snddev_info(dev, NULL);

    DEB(printf("sndmmap d 0x%p dev 0x%04x ofs 0x%08x nprot 0x%08x\n",
	d, dev, offset, nprot));
    
    if (d == NULL || nprot & PROT_EXEC)
	return -1 ; /* forbidden */

    if (offset >= d->dbuf_out.bufsize && (nprot & PROT_WRITE) )
	return -1 ; /* can only write to the first block */

    if (offset < d->dbuf_out.bufsize)
	return i386_btop(vtophys(d->dbuf_out.buf + offset));
    offset -= 1 << 24;
    if ( (offset >= 0) && (offset < d->dbuf_in.bufsize))
	return i386_btop(vtophys(d->dbuf_in.buf + offset));
    offset -= 1 << 24;
    if ( (offset >= 0) && (offset < 0x2000)) {
	return i386_btop(vtophys( ((int)d & ~0xfff) + offset));
    }
    return -1 ;
}


/*
 * ask_init sets the init flag in the device descriptor, and
 * possibly calls the appropriate callback routine, returning 1
 * if the callback was successful. This enables ioctls handler for
 * rw parameters to read back the updated value.
 * Since the init callback can be slow, ask_init() should be called
 * with interrupts enabled.
 */

int
ask_init(snddev_info *d)
{
    u_long s;

    if ( d->callback == NULL )
	return 0 ;
    s = spltty();
    if ( d->flags & SND_F_PENDING_IO ||
	 d->dbuf_out.dl || d->dbuf_in.dl ) {
	/* cannot do it now, record the request and return */
	d->flags |= SND_F_INIT ;
	splx(s);
	return 0 ;
    } else {
	splx(s);
	d->callback(d, SND_CB_INIT );
	return 1;
    }
}

/*
 * these are the functions for the soundstat device. We copy parameters
 * from the device info structure to static variables, and from there
 * back to the structure when done.
 */

static void
init_status(snddev_info *d)
{
    /*
     * Write the status information to the status_buf and update
     * status_len. There is a limit of SNDSTAT_BUF_SIZE bytes for the data.
     */

    int             i;

    if (status_len != 0) /* only do init once */
	return ;
    sprintf(status_buf,
	"FreeBSD Audio Driver (980215) "  __DATE__ " " __TIME__ "\n"
	"Installed devices:\n");

    for (i = 0; i < NPCM_MAX; i++) {
        if (pcm_info[i].open)
            sprintf(status_buf + strlen(status_buf),
		"pcm%d: <%s> at 0x%x irq %d dma %d:%d\n",
		i, pcm_info[i].name, pcm_info[i].io_base,
		pcm_info[i].irq,
		pcm_info[i].dbuf_out.chan, pcm_info[i].dbuf_in.chan);
        if (midi_info[i].open)
            sprintf(status_buf + strlen(status_buf),
		"midi%d: <%s> at 0x%x irq %d dma %d:%d\n",
		i, midi_info[i].name, midi_info[i].io_base,
		midi_info[i].irq,
		midi_info[i].dbuf_out.chan, midi_info[i].dbuf_in.chan);
        if (pcm_info[i].synth_base) {
	    char *s = "???";
	    switch (pcm_info[i].synth_type) {
	    case 2 : s = "OPL2"; break;
	    case 3 : s = "OPL3"; break;
	    case 4 : s = "OPL4"; break;
	    }

            sprintf(status_buf + strlen(status_buf),
		"sequencer%d: <%s> at 0x%x (not functional)\n",
		i, s, pcm_info[i].synth_base);
	}
    }
    status_len = strlen(status_buf) ;
}

/*
 * finally, some "libraries"
 */

/*
 * isa_dmastatus1() is a wrapper for isa_dmastatus(), which
 * might return -1 or -2 in some cases (errors). Since for the
 * user code it is more comfortable not to check for these cases,
 * negative values are mapped back to 0 (which is reasonable).
 */

int
isa_dmastatus1(int channel)
{
    int r = isa_dmastatus(channel);
    if (r<0) r = 0;
    return r;
}

/*
 * snd_conflict scans already-attached boards to see if
 * the current address is conflicting with one of the already
 * assigned ones. Returns 1 if a conflict is detected.
 */
int
snd_conflict(int io_base)
{
    int i;
    for (i=0; i< NPCM_MAX ; i++) {
        if ( (io_base == pcm_info[i].io_base  ) ||
             (io_base == pcm_info[i].alt_base ) ||
             (io_base == pcm_info[i].conf_base) ||
             (io_base == pcm_info[i].mix_base ) ||
             (io_base == pcm_info[i].midi_base) ||
             (io_base == pcm_info[i].synth_base) ) {
            BVDDB(printf("device at 0x%x already attached as unit %d\n",
                io_base, i);)
            return 1 ;
        }
    }
    return 0;
}

void
snd_set_blocksize(snddev_info *d)
{
    int tmp ;
    /*
     * compute the sample size, and possibly
     * set the blocksize so as to guarantee approx 1/4s
     * between callbacks.
     */
    tmp = 1 ;
    if (d->flags & SND_F_STEREO) tmp += tmp;
    if (d->play_fmt & (AFMT_S16_LE|AFMT_U16_LE)) tmp += tmp;
    d->dbuf_out.sample_size = tmp ;
    tmp = tmp * d->play_speed;
    if ( (d->flags & SND_F_HAS_SIZE) == 0) {
	d->play_blocksize = (tmp / 4) & ~3; /* 0.25s, aligned to 4 */
	RANGE (d->play_blocksize, 1024, (d->bufsize / 4) & ~3);
    }

    tmp = 1 ;
    if (d->flags & SND_F_STEREO) tmp += tmp;
    if (d->rec_fmt & (AFMT_S16_LE|AFMT_U16_LE)) tmp += tmp;
    tmp = tmp * d->rec_speed;
    d->dbuf_in.sample_size = tmp ;
    if ( (d->flags & SND_F_HAS_SIZE) == 0) {
	d->rec_blocksize = (tmp / 4) & ~3; /* 0.25s, aligned to 4 */
	RANGE (d->rec_blocksize, 1024, (d->bufsize / 4) & ~3);
    }
}

/*
 * The various mixers use a variety of bitmasks etc. The Voxware
 * driver had a very nice technique to describe a mixer and interface
 * to it. A table defines, for each channel, which register, bits,
 * offset, polarity to use. This procedure creates the new value
 * using the table and the old value.
 */

void
change_bits(mixer_tab *t, u_char *regval, int dev, int chn, int newval)
{
    u_char mask;
    int shift;

    DEB(printf("ch_bits dev %d ch %d val %d old 0x%02x "
	"r %d p %d bit %d off %d\n",
	dev, chn, newval, *regval,
	(*t)[dev][chn].regno, (*t)[dev][chn].polarity,
	(*t)[dev][chn].nbits, (*t)[dev][chn].bitoffs ) );

    if ( (*t)[dev][chn].polarity == 1)	/* reverse */
	newval = 100 - newval ;

    mask = (1 << (*t)[dev][chn].nbits) - 1;
    newval = (int) ((newval * mask) + 50) / 100; /* Scale it */
    shift = (*t)[dev][chn].bitoffs /*- (*t)[dev][LEFT_CHN].nbits + 1*/;

    *regval &= ~(mask << shift);        /* Filter out the previous value */
    *regval |= (newval & mask) << shift;        /* Set the new value */
}


/*
 * code for translating between U8 and ULAW. Needed to support
 * /dev/audio on the SoundBlaster. Actually, we would also need
 * ulaw -> 16 bits (for the soundblaster as well, when used in
 * full-duplex)
 */

#if 1
void
translate_bytes (u_char *table, u_char *buff, int n)
{
    u_long   i;

    if (n <= 0)
	return;
  
    for (i = 0; i < n; ++i)
	buff[i] = table[buff[i]];
}
#else
/* inline */
void
translate_bytes (const void *table, void *buff, int n)
{     
    if (n > 0) { 
	__asm__ (  "   cld\n"
		   "1: lodsb\n"
		   "   xlatb\n"
		   "   stosb\n"
		   "   loop 1b\n":
	    : "b" ((long) table), "c" (n), "D" ((long) buff), "S" ((long) buff)
	    : "bx", "cx", "di", "si", "ax");
    }
}   

#endif

#endif	/* NPCM > 0 */
