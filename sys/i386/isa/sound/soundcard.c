/*
 * sound/386bsd/soundcard.c
 * 
 * Soundcard driver for 386BSD.
 * 
 * Copyright by Hannu Savolainen 1993
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */
#include "opt_devfs.h"

#include <i386/isa/sound/sound_config.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */

#if NSND > 0	/* from "snd.h" */
#include <vm/vm.h>
#include <vm/pmap.h>
#include <sys/mman.h>

#include <i386/isa/isa_device.h>


/*
**  Register definitions for DMA controller 1 (channels 0..3):
*/
#define	DMA1_CHN(c)	(IO_DMA1 + 1*(2*(c)))	/* addr reg for channel c */
#define	DMA1_SMSK	(IO_DMA1 + 1*10)	/* single mask register */
#define	DMA1_MODE	(IO_DMA1 + 1*11)	/* mode register */
#define	DMA1_FFC	(IO_DMA1 + 1*12)	/* clear first/last FF */

/*
**  Register definitions for DMA controller 2 (channels 4..7):
*/
#define	DMA2_CHN(c)	(IO_DMA2 + 2*(2*(c)))	/* addr reg for channel c */
#define	DMA2_SMSK	(IO_DMA2 + 2*10)	/* single mask register */
#define	DMA2_MODE	(IO_DMA2 + 2*11)	/* mode register */
#define	DMA2_FFC	(IO_DMA2 + 2*12)	/* clear first/last FF */


#define FIX_RETURN(ret) {if ((ret)<0) return -(ret); else return 0;}

static int      soundcards_installed = 0; /* Number of installed soundcards */
static int      soundcard_configured = 0;

static struct fileinfo files[SND_NDEVS];
struct selinfo  selinfo[SND_NDEVS >> 4];

int
MIDIbuf_poll (int dev, struct fileinfo *file, int events, select_table * wait);

int
audio_poll(int dev, struct fileinfo * file, int events, select_table * wait);

int
sequencer_poll (int dev, struct fileinfo *file, int events, select_table * wait);

static int sndprobe    __P((struct isa_device *));
static int sndattach   __P((struct isa_device *));
static int sndmmap __P((dev_t dev, int offset, int nprot ));

static d_open_t sndopen;
static d_close_t sndclose;
static d_ioctl_t sndioctl;
static d_read_t sndread;
static d_write_t sndwrite;
static d_poll_t sndpoll;

static char     driver_name[] = "snd";

#define CDEV_MAJOR 30
static struct cdevsw snd_cdevsw = {
	sndopen, sndclose, sndread, sndwrite,
	sndioctl, nxstop, nxreset, nxdevtotty,
	sndpoll, sndmmap, nxstrategy, driver_name,
	NULL, -1,
};




static void     sound_mem_init(void);

/*
 * for each "device XXX" entry in the config file, we have
 * a struct isa_driver which is linked into isa_devtab_null[]
 *
 * XXX It is a bit stupid to call the generic routine so many times and
 * switch then to the specific one, but the alternative way would be
 * to replicate some code in the probe/attach routines.
 */

struct isa_driver opldriver = {sndprobe, sndattach, "opl"};
struct isa_driver trixdriver = {sndprobe, sndattach, "trix"};
struct isa_driver trixsbdriver = {sndprobe, sndattach, "trixsb"};
struct isa_driver sbdriver = {sndprobe, sndattach, "sb"};
struct isa_driver sbxvidriver = {sndprobe, sndattach, "sbxvi"};
struct isa_driver sbmididriver = {sndprobe, sndattach, "sbmidi"};
struct isa_driver awedriver    = {sndprobe, sndattach, "awe"};
struct isa_driver pasdriver = {sndprobe, sndattach, "pas"};
struct isa_driver mpudriver = {sndprobe, sndattach, "mpu"};
struct isa_driver gusdriver = {sndprobe, sndattach, "gus"};
struct isa_driver gusxvidriver = {sndprobe, sndattach, "gusxvi"};
struct isa_driver gusmaxdriver = {sndprobe, sndattach, "gusmax"};
struct isa_driver uartdriver = {sndprobe, sndattach, "uart"};
struct isa_driver mssdriver = {sndprobe, sndattach, "mss"};
struct isa_driver cssdriver = {sndprobe, sndattach, "css"};
struct isa_driver sscapedriver = {sndprobe, sndattach, "sscape"};
struct isa_driver sscape_mssdriver = {sndprobe, sndattach, "sscape_mss"};

short ipri_to_irq(u_short ipri);

u_long
get_time(void)
{
    struct timeval  timecopy;

    getmicrotime(&timecopy);
    return timecopy.tv_usec / (1000000 / hz) +
		(u_long) timecopy.tv_sec * hz;
}

static int
sndmmap( dev_t dev, int offset, int nprot )
{
	int		unit;
	struct dma_buffparms * dmap;

	dev = minor(dev) >> 4;
	if (dev > 0 ) return (-1);

	dmap =	audio_devs[dev]->dmap_out;

	if (nprot & PROT_EXEC)
		return( -1 );
	dmap->mapping_flags |= DMA_MAP_MAPPED ;
	return( i386_btop(vtophys(dmap->raw_buf) + offset) );
}


static int
sndread(dev_t dev, struct uio * buf, int flag)
{
    int             count = buf->uio_resid;

    dev = minor(dev);
    FIX_RETURN(sound_read_sw(dev, &files[dev], buf, count));
}


static int
sndwrite(dev_t dev, struct uio * buf, int flag)
{
    int             count = buf->uio_resid;

    dev = minor(dev);
    FIX_RETURN(sound_write_sw(dev, &files[dev], buf, count));
}

static int
sndopen(dev_t dev, int flags, int mode, struct proc * p)
{
    int             retval;
    struct fileinfo tmp_file;

    dev = minor(dev);
    if (!soundcard_configured && dev) {
	printf("SoundCard Error: soundcard system has not been configured\n");
	return ENODEV ;
    }
    tmp_file.mode = 0;

    if (flags & FREAD && flags & FWRITE)
	tmp_file.mode = OPEN_READWRITE;
    else if (flags & FREAD)
	tmp_file.mode = OPEN_READ;
    else if (flags & FWRITE)
	tmp_file.mode = OPEN_WRITE;

    selinfo[dev >> 4].si_pid = 0;
    selinfo[dev >> 4].si_flags = 0;
    if ((retval = sound_open_sw(dev, &tmp_file)) < 0)
	FIX_RETURN(retval);

    bcopy((char *) &tmp_file, (char *) &files[dev], sizeof(tmp_file));

    FIX_RETURN(retval);
}


static int
sndclose(dev_t dev, int flags, int mode, struct proc * p)
{
    dev = minor(dev);
    sound_release_sw(dev, &files[dev]);

    return 0 ;
}

static int
sndioctl(dev_t dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
    dev = minor(dev);
    FIX_RETURN(sound_ioctl_sw(dev, &files[dev], cmd, arg));
}

int
sndpoll(dev_t dev, int events, struct proc * p)
{
    dev = minor(dev);
    dev = minor(dev);

    /* printf ("snd_select(dev=%d, rw=%d, pid=%d)\n", dev, rw, p->p_pid); */
#ifdef ALLOW_POLL
    switch (dev & 0x0f) {
#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
	return sequencer_poll(dev, &files[dev], events, p);
	break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
	return MIDIbuf_poll(dev, &files[dev], events, p);
	break;
#endif

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:

	return audio_poll(dev, &files[dev], events, p);
	break;
#endif

    default:
	return 0;
    }

#endif	/* ALLOW_POLL */
    DEB(printf("sound_ioctl(dev=%d, cmd=0x%x, arg=0x%x)\n", dev, cmd, arg));

    return 0 ;
}

/* XXX this should become ffs(ipri), perhaps -1 lr 970705 */
short
ipri_to_irq(u_short ipri)
{
    /*
     * Converts the ipri (bitmask) to the corresponding irq number
     */
    int             irq;

    for (irq = 0; irq < 16; irq++)
	if (ipri == (1 << irq))
	    return irq;

    return -1;		/* Invalid argument */
}

static int
driver_to_voxunit(struct isa_driver * driver)
{
    /*
     * converts a sound driver pointer into the equivalent VoxWare device
     * unit number
     */
    if (driver == &opldriver)
	return (SNDCARD_ADLIB);
    else if (driver == &sbdriver)
	return (SNDCARD_SB);
    else if (driver == &pasdriver)
	return (SNDCARD_PAS);
    else if (driver == &gusdriver)
	return (SNDCARD_GUS);
    else if (driver == &mpudriver)
	return (SNDCARD_MPU401);
    else if (driver == &sbxvidriver)
	return (SNDCARD_SB16);
    else if (driver == &sbmididriver)
	return (SNDCARD_SB16MIDI);
    else if(driver == &awedriver)
	return(SNDCARD_AWE32);
    else if (driver == &uartdriver)
	return (SNDCARD_UART6850);
    else if (driver == &gusdriver)
	return (SNDCARD_GUS16);
    else if (driver == &mssdriver)
	return (SNDCARD_MSS);
    else if (driver == &cssdriver)
	return (SNDCARD_CS4232);
    else if (driver == &sscapedriver)
	return(SNDCARD_SSCAPE);
    else if (driver == &sscape_mssdriver)
	return(SNDCARD_SSCAPE_MSS);
    else if (driver == &trixdriver)
	return (SNDCARD_TRXPRO);
    else if (driver == &trixsbdriver)
	return (SNDCARD_TRXPRO_SB);
    else
	return (0);
}

/*
 * very dirty: tmp_osp is allocated in sndprobe, and used at the next
 * call in sndattach
 */

static sound_os_info *temp_osp;

/*
 * sndprobe is called for each isa_device. From here, a voxware unit
 * number is determined, and the appropriate probe routine is selected.
 * The parameters from the config line are passed to the hw_config struct.
 */

static int
sndprobe(struct isa_device * dev)
{
    struct address_info hw_config;
    int             unit;

    temp_osp = (sound_os_info *)malloc(sizeof(sound_os_info),
	    M_DEVBUF, M_NOWAIT);
    if (!temp_osp)
	panic("SOUND: Cannot allocate memory\n");

    /*
     * get config info from the kernel config. These may be overridden
     * by the local autoconfiguration routines though (e.g. pnp stuff).
     */

    hw_config.io_base = dev->id_iobase;
    hw_config.irq = ipri_to_irq(dev->id_irq);
    hw_config.dma = dev->id_drq;

    /*
     * misuse the flags field for read dma. Note that, to use 0 as
     * read dma channel, one of the high bits should be set.  lr970705 XXX
     */

    if (dev->id_flags != 0)
	hw_config.dma2 = dev->id_flags & 0x7;
    else
	hw_config.dma2 = -1;

    hw_config.always_detect = 0;
    hw_config.name = NULL;
    hw_config.card_subtype = 0;

    temp_osp->unit = dev->id_unit;
    hw_config.osp = temp_osp;
    unit = driver_to_voxunit(dev->id_driver);

    if (sndtable_probe(unit, &hw_config)) {
	dev->id_iobase = hw_config.io_base;
	dev->id_irq =  hw_config.irq == -1 ? 0 : (1 << hw_config.irq);
	dev->id_drq = hw_config.dma;

	if (hw_config.dma != hw_config.dma2 && ( hw_config.dma2 != -1))
	    dev->id_flags = hw_config.dma2 | 0x100; /* XXX lr */
	else
	    dev->id_flags = 0;
	return TRUE;
    }
    return 0;
}

static int
sndattach(struct isa_device * dev)
{
    int             unit;
    static int      midi_initialized = 0;
    static int      seq_initialized = 0;
    struct address_info hw_config;
    void   *tmp;
    
    unit = driver_to_voxunit(dev->id_driver);
    hw_config.io_base = dev->id_iobase;
    hw_config.irq = ipri_to_irq(dev->id_irq);
    hw_config.dma = dev->id_drq;

    /* misuse the flags field for read dma */
    if (dev->id_flags != 0)
	hw_config.dma2 = dev->id_flags & 0x7;
    else
	hw_config.dma2 = -1;

    hw_config.card_subtype = 0;
    hw_config.osp = temp_osp;

    if (!unit)
	return FALSE;

    if (!(sndtable_init_card(unit, &hw_config))) {	/* init card */
	printf(" <Driver not configured>");
	return FALSE;
    }
    /*
     * Init the high level sound driver
     */

    if (!(soundcards_installed = sndtable_get_cardcount())) {
	DDB(printf("No drivers actually installed\n"));
	return FALSE;	/* No cards detected */
    }
    printf("\n");

#ifdef CONFIG_AUDIO
    if (num_audiodevs) {	/* Audio devices present */
	DMAbuf_init();
	sound_mem_init();
    }
    soundcard_configured = 1;
#endif

    if (num_midis && !midi_initialized)
	midi_initialized = 1;

    if ((num_midis + num_synths) && !seq_initialized) {
	seq_initialized = 1;
	sequencer_init();
    }

    {
	dev_t           dev;

	dev = makedev(CDEV_MAJOR, 0);
	cdevsw_add(&dev, &snd_cdevsw, NULL);
    }
#ifdef DEVFS
#define GID_SND GID_GAMES
#define UID_SND UID_ROOT
#define PERM_SND 0660
    /*
     *	make links to first successfully probed device, don't do it if
     *	duplicate creation of same node failed (ie. bad cookie returned)
     */
    if (dev->id_driver == &opldriver){
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_SEQ,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "sequencer%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "sequencer");
    } else if (dev->id_driver == &mpudriver || 
               dev->id_driver == &sbmididriver ||
	       dev->id_driver == &uartdriver){
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_MIDIN,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "midi%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "midi");
    } else {
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_DSP,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "dsp%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "dsp");
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_DSP16,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "dspW%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "dspW");
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_AUDIO,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "audio%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "audio");
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_CTL,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "mixer%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "mixer");
	tmp = devfs_add_devswf(&snd_cdevsw, (dev->id_unit << 4) | SND_DEV_STATUS,
			 DV_CHR, UID_SND, GID_SND, PERM_SND,
			 "sndstat%r", dev->id_unit);
	if (tmp) devfs_link(tmp, "sndstat");
    }
#endif /* DEVFS */
    return TRUE;
}


#ifdef CONFIG_AUDIO

static void
alloc_dmap(int dev, int chan, struct dma_buffparms * dmap)
{
    char           *tmpbuf;
    int            i;

    tmpbuf = contigmalloc(audio_devs[dev]->buffsize, M_DEVBUF, M_NOWAIT,
		0ul, 0xfffffful, 1ul, chan & 4 ? 0x20000ul : 0x10000ul);
    if (tmpbuf == NULL)
	printf("soundcard buffer alloc failed \n");

    if (tmpbuf == NULL) {
	printf("snd: Unable to allocate %d bytes of buffer\n",
	       2 * (int) audio_devs[dev]->buffsize);
	return;
    }
    dmap->raw_buf = tmpbuf;
    /*
     * Use virtual address as the physical address, since isa_dmastart
     * performs the phys address computation.
     */

    dmap->raw_buf_phys = (u_long) tmpbuf;
    for (i = 0; i < audio_devs[dev]->buffsize; i++)   *tmpbuf++ = 0x80; 

}

static void
sound_mem_init(void)
{
    int             dev;
    static u_long dsp_init_mask = 0;

    for (dev = 0; dev < num_audiodevs; dev++)	/* Enumerate devices */
	if (!(dsp_init_mask & (1 << dev)))	/* Not already done */
	    if (audio_devs[dev]->dmachan1 >= 0) {
		dsp_init_mask |= (1 << dev);
		audio_devs[dev]->buffsize = DSP_BUFFSIZE;
		/* Now allocate the buffers */
		alloc_dmap(dev, audio_devs[dev]->dmachan1,
			audio_devs[dev]->dmap_out);
		if (audio_devs[dev]->flags & DMA_DUPLEX)
		    alloc_dmap(dev, audio_devs[dev]->dmachan2,
			    audio_devs[dev]->dmap_in);
	    }	/* for dev */
}

#endif


int
snd_ioctl_return(int *addr, int value)
{
    if (value < 0)
	return value;	/* Error */
    suword(addr, value);
    return 0;
}

#define MAX_UNIT 50
typedef void    (*irq_proc_t) (int irq);
static irq_proc_t irq_proc[MAX_UNIT] = {NULL};
static int      irq_irq[MAX_UNIT] = {0};

int
snd_set_irq_handler(int int_lvl, void (*hndlr) (int), sound_os_info * osp)
{
    if (osp->unit >= MAX_UNIT) {
	printf("Sound error: Unit number too high (%d)\n", osp->unit);
	return 0;
    }
    irq_proc[osp->unit] = hndlr;
    irq_irq[osp->unit] = int_lvl;
    return 1;
}

void
sndintr(int unit)
{
    if ( (unit >= MAX_UNIT) || (irq_proc[unit] == NULL) )
	return;

    irq_proc[unit] (irq_irq[unit]);	/* Call the installed handler */
}

void
conf_printf(char *name, struct address_info * hw_config)
{
    if (!trace_init)
	return;

    printf("snd0: <%s> ", name);
#if 0
    if (hw_config->io_base != -1 ) 
    printf("at 0x%03x", hw_config->io_base);

    if (hw_config->irq != -1 )
	printf(" irq %d", hw_config->irq);

    if (hw_config->dma != -1 || hw_config->dma2 != -1) {
	printf(" dma %d", hw_config->dma);
	if (hw_config->dma2 != -1)
	    printf(",%d", hw_config->dma2);
    }
#endif

}

void
conf_printf2(char *name, int base, int irq, int dma, int dma2)
{
    if (!trace_init)
	return;

    printf("snd0: <%s> ", name);
#if 0
    if (hw_config->io_base != -1 ) 
    printf("at 0x%03x", hw_config->io_base);

    if (irq)
	printf(" irq %d", irq);

    if (dma != -1 || dma2 != -1) {
	printf(" dma %d", dma);
	if (dma2 != -1)
	    printf(",%d", dma2);
    }
#endif

}


void tenmicrosec (int j)
{
  int             i, k;
  for (k = 0; k < j/10 ; k++) {
      for (i = 0; i < 16; i++)
	  inb (0x80);
  }
}

#endif	/* NSND > 0 */




