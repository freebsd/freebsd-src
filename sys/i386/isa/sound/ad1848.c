/*
 * sound/ad1848.c
 * 
 * Modified by Luigi Rizzo (luigi@iet.unipi.it)
 *
 * The low level driver for the AD1848/CS4248 codec chip which is used for
 * example in the MS Sound System.
 * 
 * The CS4231 which is used in the GUS MAX and some other cards is upwards
 * compatible with AD1848 and this driver is able to drive it.
 * 
 * CS4231A and AD1845 are upward compatible with CS4231. However the new
 * features of these chips are different.
 * 
 * CS4232 is a PnP audio chip which contains a CS4231A (and SB, MPU). CS4232A is
 * an improved version of CS4232.
 * 
 * CS4236 is also a PnP audio chip similar to the 4232
 *
 * OPTi931 is another high-end 1848-type chip. It differs in the use
 * of the high 16 registers and configuration stuff. Luckily, being a
 * PnP device, we can avoid black magic to identify the chip and be
 * sure of its identity.
 * 
 * Copyright by Hannu Savolainen 1994, 1995
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
 * Modified: Riccardo Facchetti  24 Mar 1995 - Added the Audio Excel DSP 16
 * initialization routine.
 *
 * $FreeBSD$
 */

#define DEB(x)
#define DEB1(x)
#include <i386/isa/sound/sound_config.h>

#if defined(CONFIG_AD1848)

#include <i386/isa/sound/ad1848_mixer.h>
#include <i386/isa/sound/iwdefs.h>

#if defined(CONFIG_CS4232) 
extern struct isa_driver cssdriver;
#else
extern struct isa_driver mssdriver;
#endif

extern void     IwaveStopDma(BYTE path);

typedef struct {
	int      base;
	int      irq;
	int      dual_dma;	/* 1, when two DMA channels allocated */
	u_char   MCE_bit;
	u_char   saved_regs[16];

	int      speed;
	u_char   speed_bits;
	int      channels;
	int      audio_format;
	u_char   format_bits;

	u_long   xfer_count;
	int      irq_mode;
	int      intr_active;
	int      opened;
	char     *chip_name;
	int      mode;
#define MD_1848		1
#define MD_4231		2
#define MD_4231A	3
#define MD_4236		4
#define MD_1845		5
#define MD_MAXMODE	6

	/* Mixer parameters */
	int      recmask;
	int      supported_devices;
	int      supported_rec_devices;
	u_short  levels[32];
	int      dev_no;
	volatile u_long timer_ticks;
	int      timer_running;
	int      irq_ok;
	sound_os_info  *osp;
}	ad1848_info;

static int      nr_ad1848_devs = 0;
static volatile char irq2dev[17] =
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

static int      timer_installed = -1;
static int      mute_flag = 0;
static char     mixer2codec[MAX_MIXER_DEV] = {0};

static int      ad_format_mask[MD_MAXMODE /* devc->mode */ ] =
{
    /* 0 - none	   */	0,
    /* 1 - AD1848  */	AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW,

    /*
     * AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW  | AFMT_A_LAW | AFMT_U16_LE |
     * AFMT_IMA_ADPCM,
     */

    /* 2 - CS4231  */	AFMT_U8 | AFMT_S16_LE | AFMT_U16_LE,

    /*
     * AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW | AFMT_U16_LE |
     * AFMT_IMA_ADPCM,
     */

    /* 3 - CS4231A */	AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW,
    /* 4 - AD1845 */	AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW,
    /* 5 - CS4236 */	AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW,
};

static ad1848_info dev_info[MAX_AUDIO_DEV];

#define io_Index_Addr(d)	((d)->base)
#define io_Indexed_Data(d)	((d)->base+1)
#define io_Status(d)		((d)->base+2)
#define io_Polled_IO(d)		((d)->base+3)

static int      ad1848_open(int dev, int mode);
static void     ad1848_close(int dev);
static int      ad1848_ioctl(int dev, u_int cmd, ioctl_arg arg, int local);
static void     ad1848_output_block(int dev, u_long buf, int count, int intrflag, int dma_restart);
static void     ad1848_start_input(int dev, u_long buf, int count, int intrflag, int dma_restart);
static int      ad1848_prepare_for_IO(int dev, int bsize, int bcount);
static void     ad1848_reset(int dev);
static void     ad1848_halt(int dev);
static void     ad1848_halt_input(int dev);
static void     ad1848_halt_output(int dev);
static void     ad1848_trigger(int dev, int bits);
static int      ad1848_tmr_install(int dev);
static void     ad1848_tmr_reprogram(int dev);

/*
 * AD_WAIT_INIT waits if we are initializing the board and we cannot modify
 * its settings
 */
#define AD_WAIT_INIT(x) {int t=x; while(t>0 && inb(devc->base) == 0x80) t-- ; }

short ipri_to_irq(u_short ipri);

void
adintr(unit)
{
#if 1
    /* this isn't ideal but should work */
    ad1848_interrupt(-1);
#else
    static short    unit_to_irq[4] = {9, -1, -1, -1};
    struct isa_device *dev;

    if (unit_to_irq[unit] > 0)
	ad1848_interrupt(unit_to_irq[unit]);
    else {
#if defined(CONFIG_CS4232)
	dev = find_isadev(isa_devtab_null, &cssdriver, unit);
#else
	dev = find_isadev(isa_devtab_null, &mssdriver, unit);
#endif
	if (!dev)
	    printf("ad1848: Couldn't determine unit\n");
	else {
	    unit_to_irq[unit] = ipri_to_irq(dev->id_irq);
	    ad1848_interrupt(unit_to_irq[unit]);
	}
    }
#endif
}

static int
ad_read(ad1848_info * devc, int reg)
{
    u_long   flags;
    int             x;

    AD_WAIT_INIT(900000);
    flags = splhigh();
    outb(io_Index_Addr(devc), (u_char) (reg & 0xff) | devc->MCE_bit);
    x = inb(io_Indexed_Data(devc));
    splx(flags);

    return x;
}

static void
ad_write(ad1848_info * devc, int reg, u_char data)
{
    u_long   flags;

    AD_WAIT_INIT(90000);

    flags = splhigh();
    outb(io_Index_Addr(devc), (u_char) (reg & 0xff) | devc->MCE_bit);
    outb(io_Indexed_Data(devc), (u_char) (data & 0xff));
    splx(flags);
}

static void
wait_for_calibration(ad1848_info * devc)
{
    int             timeout = 0;

    /*
     * Wait until the auto calibration process has finished.
     * 
     * 1)       Wait until the chip becomes ready (reads don't return 0x80).
     * 2)       Wait until the ACI bit of I11 gets on and then off.
     */

    AD_WAIT_INIT(100000);
    if (inb(devc->base) & 0x80)
	printf("ad1848: Auto calibration timed out(1).\n");

    timeout = 100;
    while (timeout > 0 && !(ad_read(devc, 11) & 0x20))
	timeout--;
    if (!(ad_read(devc, 11) & 0x20))
	return;

    timeout = 20000;
    while (timeout > 0 && ad_read(devc, 11) & 0x20)
	timeout--;
    if (ad_read(devc, 11) & 0x20)
	printf("ad1848: Auto calibration timed out(3).\n");
}

static void
ad_mute(ad1848_info * devc)
{
    int             i;
    u_char   prev;

    mute_flag = 1;

    /*
     * Save old register settings and mute output channels
     */
    for (i = 6; i < 8; i++) {
	prev = devc->saved_regs[i] = ad_read(devc, i);
	ad_write(devc, i, prev | 0x80);
    }
}

static void
ad_unmute(ad1848_info * devc)
{
    int             i;

    mute_flag = 0;
    /*
     * Restore back old volume registers (unmute)
     */
    for (i = 6; i < 8; i++)
	ad_write(devc, i, devc->saved_regs[i] & ~0x80);
    }

static void
ad_enter_MCE(ad1848_info * devc)
{
    u_long   flags;

    AD_WAIT_INIT(1000);
    devc->MCE_bit = 0x40;
    flags = splhigh();
    if ( ( inb(io_Index_Addr(devc)) & 0x40) == 0 )
    outb(io_Index_Addr(devc), devc->MCE_bit);
    splx(flags);
}

static void
ad_leave_MCE(ad1848_info * devc)
{
    u_long   flags;
    u_char   prev;

    AD_WAIT_INIT(1000);

    flags = splhigh();

    devc->MCE_bit = 0x00;
    prev = inb(io_Index_Addr(devc));
    /* XXX the next call is redundant ? */
    outb(io_Index_Addr(devc), 0x00);	/* Clear the MCE bit */

    if ((prev & 0x40) == 0) {	/* Not in MCE mode */
	splx(flags);
	return;
    }
    outb(io_Index_Addr(devc), 0x00);	/* Clear the MCE bit */
    wait_for_calibration(devc);
    splx(flags);
}


static int
ad1848_set_recmask(ad1848_info * devc, int mask)
{
    u_char   recdev;
    int             i, n;

    mask &= devc->supported_rec_devices;

    n = 0;
    for (i = 0; i < 32; i++)/* Count selected device bits */
	if (mask & (1 << i))
	    n++;

    if (n == 0)
	mask = SOUND_MASK_MIC;
    else if (n != 1) {	/* Too many devices selected */
	mask &= ~devc->recmask;	/* Filter out active settings */

	n = 0;
	for (i = 0; i < 32; i++)	/* Count selected device bits */
	    if (mask & (1 << i))
		n++;

	    if (n != 1)
		mask = SOUND_MASK_MIC;
    }
    switch (mask) {
    case SOUND_MASK_MIC:
	recdev = 2;
	break;

    case SOUND_MASK_LINE:
    case SOUND_MASK_LINE3:
	recdev = 0;
	break;

    case SOUND_MASK_CD:
    case SOUND_MASK_LINE1:
	recdev = 1;
	break;

    case SOUND_MASK_IMIX:
	recdev = 3;
	break;

    default:
	mask = SOUND_MASK_MIC;
	recdev = 2;
    }

    recdev <<= 6;
    ad_write(devc, 0, (ad_read(devc, 0) & 0x3f) | recdev);
    ad_write(devc, 1, (ad_read(devc, 1) & 0x3f) | recdev);

    devc->recmask = mask;
    return mask;
}

static void
change_bits(u_char *regval, int dev, int chn, int newval)
{
    u_char   mask;
    int             shift;

    if (mix_devices[dev][chn].polarity == 1)	/* Reverse */
	newval = 100 - newval;

    mask = (1 << mix_devices[dev][chn].nbits) - 1;
    shift = mix_devices[dev][chn].bitpos;
    newval = (int) ((newval * mask) + 50) / 100;	/* Scale it */

    *regval &= ~(mask << shift);	/* Clear bits */
    *regval |= (newval & mask) << shift;	/* Set new value */
}

static int
ad1848_mixer_get(ad1848_info * devc, int dev)
{
    if (!((1 << dev) & devc->supported_devices))
	return -(EINVAL);

    return devc->levels[dev];
}

#define CLMICI          0x00781601
#define CRMICI          0x00791701

static int
ad1848_mixer_set(ad1848_info * devc, int dev, int value)
{
    int             left = value & 0x000000ff;
    int             right = (value & 0x0000ff00) >> 8;
    int             retvol;

    int             regoffs;
    u_char   val;
    /* u_char  clci,  crmici,  clmici,  clici,  crici; */

    if (left > 100)
	left = 100;
    if (right > 100)
	right = 100;

    if (mix_devices[dev][RIGHT_CHN].nbits == 0)	/* Mono control */
	right = left;

    retvol = left | (right << 8);

    /* Scale volumes */
    left = mix_cvt[left];
    right = mix_cvt[right];

    /* Scale it again */
    left = mix_cvt[left];
    right = mix_cvt[right];

    if (dev > 31)
	return -(EINVAL);

    if (!(devc->supported_devices & (1 << dev)))
	return -(EINVAL);

    if (mix_devices[dev][LEFT_CHN].nbits == 0)
	return -(EINVAL);

    devc->levels[dev] = retvol;

    /*
     * Set the left channel
     */
    /* IwaveCodecMode(CODEC_MODE3);        Default codec mode  */

    regoffs = mix_devices[dev][LEFT_CHN].regno;
    val = ad_read(devc, regoffs);

    change_bits(&val, dev, LEFT_CHN, left);
    ad_write(devc, regoffs, val);
    devc->saved_regs[regoffs] = val;

    /*
     * Set the right channel
     */

    if (mix_devices[dev][RIGHT_CHN].nbits == 0)
	return retvol;	/* Was just a mono channel */

    regoffs = mix_devices[dev][RIGHT_CHN].regno;
    val = ad_read(devc, regoffs);
    change_bits(&val, dev, RIGHT_CHN, right);
    ad_write(devc, regoffs, val);
    devc->saved_regs[regoffs] = val;

    return retvol;
}

static void
ad1848_mixer_reset(ad1848_info * devc)
{
    int             i;

    devc->recmask = 0;
    if (devc->mode != MD_1848)
	devc->supported_devices = MODE2_MIXER_DEVICES;
    else
	devc->supported_devices = MODE1_MIXER_DEVICES;

    devc->supported_rec_devices = MODE1_REC_DEVICES;

    for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	if (devc->supported_devices & (1 << i))
	    ad1848_mixer_set(devc, i, default_mixer_levels[i]);
    ad1848_set_recmask(devc, SOUND_MASK_MIC);
}

static int
ad1848_mixer_ioctl(int dev, u_int cmd, ioctl_arg arg)
{
    ad1848_info    *devc;
    int             codec_dev = mixer2codec[dev];

    if (!codec_dev)
	return -(ENXIO);

    codec_dev--;

    devc = (ad1848_info *) audio_devs[codec_dev]->devc;

    if (((cmd >> 8) & 0xff) == 'M') {
	if (cmd & IOC_IN)
	    switch (cmd & 0xff) {
	    case SOUND_MIXER_RECSRC:
		return *(int *) arg = ad1848_set_recmask(devc, (*(int *) arg));
		break;

	    default:
		return *(int *) arg = ad1848_mixer_set(devc, cmd & 0xff, (*(int *) arg));
	    }
	else
	    switch (cmd & 0xff) {	/* Return parameters */

	    case SOUND_MIXER_RECSRC:
		return *(int *) arg = devc->recmask;
		break;

	    case SOUND_MIXER_DEVMASK:
		return *(int *) arg = devc->supported_devices;
		break;

	    case SOUND_MIXER_STEREODEVS:
		return *(int *) arg = devc->supported_devices & ~(SOUND_MASK_SPEAKER | SOUND_MASK_IMIX);
		break;

	    case SOUND_MIXER_RECMASK:
		return *(int *) arg = devc->supported_rec_devices;
		break;

	    case SOUND_MIXER_CAPS:
		return *(int *) arg = SOUND_CAP_EXCL_INPUT;
		break;

	    default:
		return *(int *) arg = ad1848_mixer_get(devc, cmd & 0xff);
	    }
    } else
	return -(EINVAL);
}

static struct audio_operations ad1848_pcm_operations[MAX_AUDIO_DEV] =
{
    {
	"Generic AD1848 codec",
	/* DMA_AUTOMODE | DMA_DUPLEX, */
	DMA_AUTOMODE,
	AFMT_U8,	/* Will be set later */
	NULL,
	ad1848_open,
	ad1848_close,
	ad1848_output_block,
	ad1848_start_input,
	ad1848_ioctl,
	ad1848_prepare_for_IO,
	ad1848_prepare_for_IO,
	ad1848_reset,
	ad1848_halt,
	NULL,
	NULL,
	ad1848_halt_input,
	ad1848_halt_output,
	ad1848_trigger
    }
};

static struct mixer_operations ad1848_mixer_operations =
{
	"AD1848/CS4248/CS4231/CS4236",
	ad1848_mixer_ioctl
};

static int
ad1848_open(int dev, int mode)
{
    ad1848_info    *devc = NULL;
    u_long   flags;
    int             otherside = audio_devs[dev]->otherside;

    if (dev < 0 || dev >= num_audiodevs)
	return -(ENXIO);

    if (otherside != -1) {
	if (audio_devs[otherside]->busy)
	    return -(EBUSY);
    }
    if (audio_devs[dev]->busy)
	return -(EBUSY);

    devc = (ad1848_info *) audio_devs[dev]->devc;

    flags = splhigh();
    if (audio_devs[dev]->busy) {
	splx(flags);
	return -(EBUSY);
    }
    devc->dual_dma = 0;

    if (audio_devs[dev]->flags & DMA_DUPLEX) {
	devc->dual_dma = 1;
    }
    devc->intr_active = 0;
    audio_devs[dev]->busy = 1;
    devc->irq_mode = 0;
    ad1848_trigger(dev, 0);
    splx(flags);
    /*
     * Mute output until the playback really starts. This decreases
     * clicking.
     */
    ad_mute(devc);

    return 0;
}

static void
ad1848_close(int dev)
{
    u_long   flags;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;
    int             otherside = audio_devs[dev]->otherside;

    if (otherside != -1) {
	if (audio_devs[otherside]->busy)
	    return;
    }
    DEB(printf("ad1848_close(void)\n"));

    flags = splhigh();

    ad_mute(devc);

    ad_write(devc, 9, ad_read(devc, 9) & ~0x1);
    outb(io_Status(devc), 0);	/* Clear interrupt status */
    /*
     * ad_write (devc, 15,0); ad_write (devc, 14,0);
     */
    devc->irq_mode &= ~PCM_ENABLE_OUTPUT;

    devc->intr_active = 0;
    ad1848_reset(dev);

    devc->opened = 0;
    devc->irq_mode = 0;
    audio_devs[dev]->busy = 0;
    ad_unmute(devc);
    splx(flags);
}

static int
set_speed(ad1848_info * devc, int arg)
{
    /*
     * The sampling speed is encoded in the least significant nible of
     * I8. The LSB selects the clock source (0=24.576 MHz, 1=16.9344 Mhz)
     * and other three bits select the divisor (indirectly):
     * 
     * The available speeds are in the following table. Keep the speeds in
     * the increasing order.
     */
    typedef struct {
	int             speed;
	u_char   bits;
    } speed_struct;

    static speed_struct speed_table[] = {
	{5510, (0 << 1) | 1},
	{5510, (0 << 1) | 1},
	{6620, (7 << 1) | 1},
	{8000, (0 << 1) | 0},
	{9600, (7 << 1) | 0},
	{11025, (1 << 1) | 1},
	{16000, (1 << 1) | 0},
	{18900, (2 << 1) | 1},
	{22050, (3 << 1) | 1},
	{27420, (2 << 1) | 0},
	{32000, (3 << 1) | 0},
	{33075, (6 << 1) | 1},
	{37800, (4 << 1) | 1},
	{44100, (5 << 1) | 1},
	{48000, (6 << 1) | 0}
    };

    int             i, n, selected = -1;

    n = sizeof(speed_table) / sizeof(speed_struct);

    if (devc->mode == MD_1845) { /* AD1845 has different timer than others */
	RANGE (arg, 4000, 50000) ;

	devc->speed = arg;
	devc->speed_bits = speed_table[selected].bits;
	return devc->speed;
    }
    if (arg < speed_table[0].speed)
	selected = 0;
    if (arg > speed_table[n - 1].speed)
	selected = n - 1;

    for (i = 1 /* really */ ; selected == -1 && i < n; i++)
	if (speed_table[i].speed == arg)
	    selected = i;
	else if (speed_table[i].speed > arg) {
	    int             diff1, diff2;

	    diff1 = arg - speed_table[i - 1].speed;
	    diff2 = speed_table[i].speed - arg;

	    if (diff1 < diff2)
		selected = i - 1;
	    else
		selected = i;
	}
    if (selected == -1) {
	printf("ad1848: Can't find speed???\n");
	selected = 3;
    }
    devc->speed = speed_table[selected].speed;
    devc->speed_bits = speed_table[selected].bits;
    return devc->speed;
}

static int
set_channels(ad1848_info * devc, int arg)
{
    if (arg != 1 && arg != 2)
	return devc->channels;

    devc->channels = arg;
    return arg;
}

static int
set_format(ad1848_info * devc, int arg)
{
    static struct format_tbl {
	int             format;
	u_char   bits;
    } format2bits[] = {
	{ 0, 0 } ,
	{ AFMT_MU_LAW, 1 } ,
	{ AFMT_A_LAW, 3 } ,
	{ AFMT_IMA_ADPCM, 5 } ,
	{ AFMT_U8, 0 } ,
	{ AFMT_S16_LE, 2 } ,
	{ AFMT_S16_BE, 6 } ,
	{ AFMT_S8, 0 } ,
	{ AFMT_U16_LE, 0 } ,
	{ AFMT_U16_BE, 0 }
    };
    int             i, n = sizeof(format2bits) / sizeof(struct format_tbl);


    if (!(arg & ad_format_mask[devc->mode]))
	arg = AFMT_U8;

    devc->audio_format = arg;

    for (i = 0; i < n; i++)
	if (format2bits[i].format == arg) {
	    if ((devc->format_bits = format2bits[i].bits) == 0)
		return devc->audio_format = AFMT_U8;	/* Was not supported */
	    return arg;
	}
    /* Still hanging here. Something must be terribly wrong */
    devc->format_bits = 0;
    return devc->audio_format = AFMT_U8;
}

/* XXX check what is arg,  (int) or *(int *) lr970705 */
static int
ad1848_ioctl(int dev, u_int cmd, ioctl_arg arg, int local)
{
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

    switch (cmd) {
    case SOUND_PCM_WRITE_RATE:
	if (local)
	    return set_speed(devc, (int) arg);
	return *(int *) arg = set_speed(devc, (*(int *) arg));

    case SOUND_PCM_READ_RATE:
	if (local)
	    return devc->speed;
	return *(int *) arg = devc->speed;

    case SNDCTL_DSP_STEREO:
	if (local)
	    return set_channels(devc, (int) arg + 1) - 1;
	return *(int *) arg = set_channels(devc, (*(int *) arg) + 1) - 1;

    case SOUND_PCM_WRITE_CHANNELS:
	if (local)
	    return set_channels(devc, (int) arg);
	return *(int *) arg = set_channels(devc, (*(int *) arg));

    case SOUND_PCM_READ_CHANNELS:
	if (local)
	    return devc->channels;
	return *(int *) arg = devc->channels;

    case SNDCTL_DSP_SAMPLESIZE:
	if (local)
	    return set_format(devc, (int) arg);
	return *(int *) arg = set_format(devc, (*(int *) arg));

    case SOUND_PCM_READ_BITS:
	if (local)
	    return devc->audio_format;
	return *(int *) arg = devc->audio_format;


    case FIOASYNC:
	if (local)
	    return 1;
	return *(int *) arg = 1;

    case FIONBIO:
	if (local)
	    return 1;
	return *(int *) arg = 1;


    default:;
    }
    return -(EINVAL);
}

static void
ad1848_output_block(int dev, u_long buf, int count, int intrflag, int dma_restart)
{
    u_long   flags, cnt;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

    cnt = count;
    if (devc->audio_format == AFMT_IMA_ADPCM) {
	cnt /= 4;
    } else {
	if (devc->audio_format & (AFMT_S16_LE | AFMT_S16_BE)) /* 16 bit data */
	    cnt >>= 1;
    }
    if (devc->channels > 1)
	cnt >>= 1;
    cnt--;
    if (mute_flag)
	ad_unmute(devc);

    if (    devc->irq_mode & PCM_ENABLE_OUTPUT &&
	    audio_devs[dev]->flags & DMA_AUTOMODE && intrflag &&
	    cnt == devc->xfer_count) {
	devc->irq_mode |= PCM_ENABLE_OUTPUT;
	devc->intr_active = 1;

    }
    flags = splhigh();

    if (dma_restart) {

	DMAbuf_start_dma(dev, buf, count, 1);
    }
    ad_write(devc, 15, (u_char) (cnt & 0xff));
    ad_write(devc, 14, (u_char) ((cnt >> 8) & 0xff));

    devc->xfer_count = cnt;
    devc->irq_mode |= PCM_ENABLE_OUTPUT;
    devc->intr_active = 1;
    splx(flags);
}

static void
ad1848_start_input(int dev, u_long buf, int count,
	int intrflag, int dma_restart)
{
    u_long   flags, cnt;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

    cnt = count;
    if (devc->audio_format == AFMT_IMA_ADPCM)
	cnt /= 4;
    else if (devc->audio_format & (AFMT_S16_LE | AFMT_S16_BE)) /* 16 bit data */
	cnt >>= 1;
    if (devc->channels > 1)
	cnt >>= 1;
    cnt--;

    if (    devc->irq_mode & PCM_ENABLE_INPUT &&
	    audio_devs[dev]->flags & DMA_AUTOMODE && intrflag &&
	    cnt == devc->xfer_count) {
	devc->irq_mode |= PCM_ENABLE_INPUT;
	devc->intr_active = 1;
	return;		/* Auto DMA mode on. No need to react */
    }
    flags = splhigh();

    if (dma_restart) {
	/* ad1848_halt (dev); */
	DMAbuf_start_dma(dev, buf, count, 0);
    }
    if (devc->mode == MD_1848 || !devc->dual_dma) {/* Single DMA chan. mode */
	ad_write(devc, 15, (u_char) (cnt & 0xff));
	ad_write(devc, 14, (u_char) ((cnt >> 8) & 0xff));
    } else { /* Dual DMA channel mode */
	ad_write(devc, 31, (u_char) (cnt & 0xff));
	ad_write(devc, 30, (u_char) ((cnt >> 8) & 0xff));
    }

    /* ad_write (devc, 9, ad_read (devc, 9) | 0x02); *//* Capture enable */
    ad_unmute(devc);

    devc->xfer_count = cnt;
    devc->irq_mode |= PCM_ENABLE_INPUT;
    devc->intr_active = 1;
    splx(flags);
}

static int
ad1848_prepare_for_IO(int dev, int bsize, int bcount)
{
    u_char   fs, old_fs;
    u_long   flags;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

    if (devc->irq_mode)
	return 0;

    fs = devc->speed_bits | (devc->format_bits << 5);

    if (devc->channels > 1)
	fs |= 0x10;
    old_fs = fs;

    flags = splhigh();

    if (devc->mode == MD_1845) {	/* Use alternate speed select regs */
	fs &= 0xf0;	/* Mask off the rate select bits */

	ad_write(devc, 22, (devc->speed >> 8) & 0xff);	/* Speed MSB */
	ad_write(devc, 23, devc->speed & 0xff);	/* Speed LSB */
    }

    ad_enter_MCE(devc);	/* Enables changes to the format select reg */

    ad_write(devc, 8, fs);

    /*
     * Write to I8 starts resyncronization. Wait until it completes.
     */
    AD_WAIT_INIT(10000);

    /*
     * If mode == 2 (CS4231), set I28 also. It's the capture format
     * register.
     */
    if (devc->mode != MD_1848) {
	ad_write(devc, 28, fs);

	/*
	 * Write to I28 starts resyncronization. Wait until it completes.
	 */
	AD_WAIT_INIT(10000);
    }

    ad_write(devc, 9, ad_read(devc, 9) & ~0x08);

    ad_leave_MCE(devc);

    splx(flags);

    devc->xfer_count = 0;
#ifdef CONFIG_SEQUENCER
    if (dev == timer_installed && devc->timer_running)
	if ((fs & 0x01) != (old_fs & 0x01)) {
	    ad1848_tmr_reprogram(dev);
	}
#endif
    return 0;
}

static void
ad1848_reset(int dev)
{
    ad1848_halt(dev);
}

static void
ad1848_halt(int dev)
{
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;
    u_long   flags;
    int             timeout;

    flags = splhigh();

    ad_mute(devc);

    ad_write(devc, 9, ad_read(devc, 9) & ~0x03);	/* Stop DMA */

    ad_write(devc, 14, 0);	/* Clear DMA counter */
    ad_write(devc, 15, 0);	/* Clear DMA counter */

    if (devc->mode != MD_1848) {
	ad_write(devc, 30, 0);	/* Clear DMA counter */
	ad_write(devc, 31, 0);	/* Clear DMA counter */
    }

    for (timeout = 0; timeout < 1000 && !(inb(io_Status(devc)) & 0x01);
	 timeout++);	/* Wait for interrupt */

    outb(io_Status(devc), 0);	/* Clear interrupt status */

    devc->irq_mode = 0;

    /* DMAbuf_reset_dma (dev); */
    splx(flags);
}

static void
ad1848_halt_input(int dev)
{
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;
    u_long   flags;
    u_char   playing;
    if (devc->mode == MD_1848) {
	ad1848_halt(dev);
	return;
    }
    playing = ad_read(devc, 9);
    if (!(playing & 0x2))
	return;

    flags = splhigh();

    ad_mute(devc);
    ad_write(devc, 9, playing & ~0x02);	/* Stop capture */

    outb(io_Status(devc), 0);	/* Clear interrupt status */
    outb(io_Status(devc), 0);	/* Clear interrupt status */

    devc->irq_mode &= ~PCM_ENABLE_INPUT;

    splx(flags);
}

static void
ad1848_halt_output(int dev)
{
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;
    u_long   flags;
    u_char   playing;

    playing = ad_read(devc, 9);
    if (!(playing & 0x1)) {
	devc->irq_mode &= ~PCM_ENABLE_OUTPUT;
	return;
    }
    /* IwaveStopDma(PLAYBACK);  */
    if (devc->mode == MD_1848) {
	ad1848_halt(dev);
	return;
    }
    flags = splhigh();
    /* ad_mute (devc);  */

    ad_write(devc, 9, playing & ~0x1);
    outb(io_Status(devc), 0);	/* Clear interrupt status */
    /*
     * ad_write (devc, 15,0); ad_write (devc, 14,0);
     */
    devc->irq_mode &= ~PCM_ENABLE_OUTPUT;

    splx(flags);
}

static void
ad1848_trigger(int dev, int state)
{
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;
    u_long   flags;
    u_char   tmp;

    flags = splhigh();
    state &= devc->irq_mode;

    tmp = ad_read(devc, 9) & ~0x03;
    if (state & PCM_ENABLE_INPUT)
	tmp |= 0x02;
    if (state & PCM_ENABLE_OUTPUT) {
	tmp |= 0x01;
    }
    ad_write(devc, 9, tmp);

    splx(flags);
}


int
ad1848_detect(int io_base, int *ad_flags, sound_os_info * osp)
{
    static int last_probe_addr=0, last_result=0; /* to avoid multiple probes*/
    int             i;
    ad1848_info    *devc = &dev_info[nr_ad1848_devs];
    u_char   tmp, tmp1, tmp2 ;

    DDB(printf("ad1848_detect(%x)\n", io_base));
    if (io_base == last_probe_addr)
	return last_result;
    else {
	last_result = 0; /* default value for detect */
	last_probe_addr = io_base ;
    }

    if (ad_flags)
	*ad_flags = 0;

    if (nr_ad1848_devs >= MAX_AUDIO_DEV) {
	DDB(printf("ad1848 detect error - step 0\n"));
	return 0 ;
    }
    devc->base = io_base;
    devc->irq_ok = 0;
    devc->timer_running = 0;
    devc->MCE_bit = 0x40;
    devc->irq = 0;
    devc->opened = 0;
    devc->chip_name = "AD1848";
    devc->mode = MD_1848;	/* AD1848 or CS4248 */
    devc->osp = osp;

    /*
     * Check that the I/O address is in use.
     * 
     * The bit 0x80 of the base I/O port is known to be 0 after the chip has
     * performed its power on initialization. Just assume this has
     * happened before the OS is starting.
     * 
     * If the I/O address is unused, it typically returns 0xff.
     */

    DDB(printf("ad1848_detect() - step A\n"));

    if ((inb(devc->base) & 0x80) != 0x00) {	/* Not a AD1848 */
	DDB(printf("ad1848 detect error - step A,"
		" inb(base) = 0x%02x, want 0XXX.XXXX\n",
		   inb(devc->base)));
	return 0;
    }
    /*
     * Test if it's possible to change contents of the indirect
     * registers. Registers 0 and 1 are ADC volume registers. The bit
     * 0x10 is read only so try to avoid using it.
     */

    DDB(printf("ad1848_detect() - step B, test indirect register\n"));

    ad_write(devc, 0, 0xaa);
    ad_write(devc, 1, 0x45);/* 0x55 with bit 0x10 clear */
    tmp1 = ad_read(devc, 0) ;
    tmp2 = ad_read(devc, 1) ;
    if ( tmp1 != 0xaa || tmp2 != 0x45) {
	DDB(printf("ad1848 detect error - step B (0x%02x/0x%02x) want 0xaa/0x45\n", tmp1, tmp2));
	    return 0;
    }
    DDB(printf("ad1848_detect() - step C\n"));
    ad_write(devc, 0, 0x45);
    ad_write(devc, 1, 0xaa);
    tmp1 = ad_read(devc, 0) ;
    tmp2 = ad_read(devc, 1) ;

    if (tmp1 != 0x45 || tmp2 != 0xaa) {
	DDB(printf("ad1848 detect error - step C (%x/%x)\n", tmp1, tmp2));

	return 0;
    }
    /*
     * The indirect register I12 has some read only bits. Lets try to
     * change them.
     */

    DDB(printf("ad1848_detect() - step D, last 4 bits of I12 readonly\n"));
    tmp = ad_read(devc, 12);
    ad_write(devc, 12, (~tmp) & 0x0f);
    tmp1 = ad_read(devc, 12);

    if ((tmp & 0x0f) != (tmp1 & 0x0f)) {
	DDB(printf("ad1848 detect error - step D, I12 (0x%02x was 0x%02x)\n",
	    tmp1, tmp));
	return 0;
    }

    /*
     * NOTE! Last 4 bits of the reg I12 tell the chip revision.
     *	0x01=RevB
     *  0x0A=RevC. also CS4231/CS4231A and OPTi931
     */


    /*
     * The original AD1848/CS4248 has just 15 indirect registers. This
     * means that I0 and I16 should return the same value (etc.). Ensure
     * that the Mode2 enable bit of I12 is 0. Otherwise this test fails
     * with CS4231.
     */

    DDB(printf("ad1848_detect() - step F\n"));
    ad_write(devc, 12, 0);	/* Mode2=disabled */

    for (i = 0; i < 16; i++)
	if ((tmp1 = ad_read(devc, i)) != (tmp2 = ad_read(devc, i + 16))) {
	    DDB(printf("ad1848 detect warning - step F(I%d/0x%02x/0x%02x)\n",
		i, tmp1, tmp2));
	    /*
	     * note - this seems to fail on the 4232 on I11. So we just break
	     * rather than fail.
	     */
	    break ; /* return 0; */
	}
    /*
     * Try to switch the chip to mode2 (CS4231) by setting the MODE2 bit
     * (0x40). The bit 0x80 is always 1 in CS4248 and CS4231.
     *
     * On the OPTi931, however, I12 is readonly and only contains the
     * chip revision ID (as in the CS4231A). The upper bits return 0.
     */

    DDB(printf("ad1848_detect() - step G\n"));
    ad_write(devc, 12, 0x40);	/* Set mode2, clear 0x80 */

    tmp1 = ad_read(devc, 12);
    if (tmp1 & 0x80) {
	if (ad_flags)
	    *ad_flags |= AD_F_CS4248;

	devc->chip_name = "CS4248"; /* Our best knowledge just now */
    }
    if ((tmp1 & 0xf0) == 0x00) {
	printf("this should be an OPTi931\n");
    } else if ((tmp1 & 0xc0) == 0xC0) {
	/*
	 * The 4231 has bit7=1 always, and bit6 we just set to 1.
	 * We want to check that this is really a CS4231
	 * Verify that setting I0 doesn't change I16.
	 */
	DDB(printf("ad1848_detect() - step H\n"));
	ad_write(devc, 16, 0);	/* Set I16 to known value */

	ad_write(devc, 0, 0x45);
	if ((tmp1 = ad_read(devc, 16)) != 0x45) { /* No change -> CS4231? */

	    ad_write(devc, 0, 0xaa);
	    if ((tmp1 = ad_read(devc, 16)) == 0xaa) {	/* Rotten bits? */
		DDB(printf("ad1848 detect error - step H(%x)\n", tmp1));
		return 0;
	    }
	    /*
	     * Verify that some bits of I25 are read only.
	     */

	    DDB(printf("ad1848_detect() - step I\n"));
	    tmp1 = ad_read(devc, 25);	/* Original bits */
	    ad_write(devc, 25, ~tmp1);	/* Invert all bits */
	    if ((ad_read(devc, 25) & 0xe7) == (tmp1 & 0xe7)) {
		int             id;

		/*
		 * It's at least CS4231
		 */
		devc->chip_name = "CS4231";
		devc->mode = MD_4231;

		/*
		 * It could be an AD1845 or CS4231A as well.
		 * CS4231 and AD1845 report the same revision info in I25
		 * while the CS4231A reports different.
		 */

		DDB(printf("ad1848_detect() - step I\n"));
		id = ad_read(devc, 25) & 0xe7;
		/*
		 * b7-b5 = version number;
		 *	100 : all CS4231
		 *	101 : CS4231A
		 *      
		 * b2-b0 = chip id;
		 */
		switch (id) {

		case 0xa0:
		    devc->chip_name = "CS4231A";
		    devc->mode = MD_4231A;
		    break;

		case 0xa2:
		    devc->chip_name = "CS4232";
		    devc->mode = MD_4231A;
		    break;

		case 0xb2:
		    /* strange: the 4231 data sheet says b4-b3 are XX
		     * so this should be the same as 0xa2
		     */
		    devc->chip_name = "CS4232A";
		    devc->mode = MD_4231A;
		    break;

		case 0x80:
		    /*
		     * It must be a CS4231 or AD1845. The register I23
		     * of CS4231 is undefined and it appears to be read
		     * only. AD1845 uses I23 for setting sample rate.
		     * Assume the chip is AD1845 if I23 is changeable.
		     */

		    tmp = ad_read(devc, 23);

		    ad_write(devc, 23, ~tmp);
		    if (ad_read(devc, 23) != tmp) {	/* AD1845 ? */
			devc->chip_name = "AD1845";
			devc->mode = MD_1845;
		    }
		    ad_write(devc, 23, tmp);	/* Restore */
		    break;

		case 0x83:	/* CS4236 */
		case 0x03:	/* Mutant CS4236 on Intel PR440fx board */
		    devc->chip_name = "CS4236";
		    devc->mode = MD_4236;
		    break;

		default:	/* Assume CS4231 */
		    printf("unknown id 0x%02x, assuming CS4231\n", id);
		    devc->mode = MD_4231;

		}
	    }
	    ad_write(devc, 25, tmp1);	/* Restore bits */

	    DDB(printf("ad1848_detect() - step K\n"));
	}
    }
    DDB(printf("ad1848_detect() - step L\n"));

    if (ad_flags) {
	if (devc->mode != MD_1848)
	    *ad_flags |= AD_F_CS4231;
    }
    DDB(printf("ad1848_detect() - Detected OK\n"));
    return (last_result = 1);
}

void
ad1848_init(char *name, int io_base, int irq,
	int dma_playback, int dma_capture, int share_dma, sound_os_info * osp)
{

    /*
     * NOTE! If irq < 0, there is another driver which has allocated the
     * IRQ so that this driver doesn't need to allocate/deallocate it.
     * The actually used IRQ is ABS(irq).
     */

    /*
     * Initial values for the indirect registers of CS4248/AD1848.
     */
    static int      init_values[] = {
	0xa8,	/* MIXOUTL: src:mic, +20dB, gain +12dB */
	0xa8,	/* MIXOUTR: src:mic, +20dB, gain +12dB */
	0x08,	/* CDL Input: mute, +6dB	*/
	0x08,	/* CDR Input: mute, +6dB        */
	0x08,	/* FML Input: mute, +6dB        */
	0x08,	/* FMR Input: mute, +6dB        */
	0x80,	/* DAC-L Input: enable, 0dB	*/
	0x80,	/* DAC-R Input: enable, 0dB     */
	/* 0xa8, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, */
	0x00,	/* 8bit, lin, uns, mono, 8KHz	*/
	0x0c,	/* dma-cap, dma-pb, autocal, single dma, disable cap/pb */
	0x02,	/* int enable */
	0x00,	/* clear error status */
	0x8a,	/* rev. id (low bytes readonly) */
	0x00,
	0x00,	/* playback upper base count */
	0x00,	/* playback lower base count */

	/* Positions 16 to 31 just for CS4231 and newer devices */
	/* I16-I17: alt. feature enable on the 4231, but AUXL Input
	 * on the OPTi931 (where the features are set elsewhere
	 */
	0x81, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    int             i, my_dev;

    ad1848_info    *devc = &dev_info[nr_ad1848_devs];

    if (!ad1848_detect(io_base, NULL, osp))
	return;

    devc->irq = (irq > 0) ? irq : 0;
    devc->opened = 0;
    devc->timer_ticks = 0;
    devc->osp = osp;

    if (nr_ad1848_devs != 0) {
	bcopy((char *) &ad1848_pcm_operations[0],
	      (char *) &ad1848_pcm_operations[nr_ad1848_devs],
	      sizeof(struct audio_operations));
    }
    for (i = 0; i < 16; i++)
	ad_write(devc, i, init_values[i]);

    ad_mute(devc);	/* Initialize some variables */
    ad_unmute(devc);	/* Leave it unmuted now */

    if (devc->mode > MD_1848) {
	if (dma_capture == dma_playback ||
		dma_capture == -1 || dma_playback == -1) {
	    ad_write(devc, 9, ad_read(devc, 9) | 0x04);	/* Single DMA mode */
	    ad1848_pcm_operations[nr_ad1848_devs].flags &= ~DMA_DUPLEX;
	} else {
	    ad_write(devc, 9, ad_read(devc, 9) & ~0x04); /* Dual DMA mode */
	    ad1848_pcm_operations[nr_ad1848_devs].flags |= DMA_DUPLEX;
	}

	ad_write(devc, 12, ad_read(devc, 12) | 0x40);	/* Mode2 = enabled */
	for (i = 16; i < 32; i++)
	    ad_write(devc, i, init_values[i]);

	if (devc->mode == MD_4231A) {
	    /* Enable full * calibration */
	    ad_write(devc, 9, init_values[9] | 0x18);
	}

	if (devc->mode == MD_1845) {
	    /* Alternate freq select enabled */
	    ad_write(devc, 27, init_values[27] | 0x08);
	}
    } else {
	ad1848_pcm_operations[nr_ad1848_devs].flags &= ~DMA_DUPLEX;
	ad_write(devc, 9, ad_read(devc, 9) | 0x04);	/* Single DMA mode */
    }

    outb(io_Status(devc), 0);	/* Clear pending interrupts */

    if (name != NULL && name[0] != 0)
	snprintf(ad1848_pcm_operations[nr_ad1848_devs].name,
	    sizeof(ad1848_pcm_operations[nr_ad1848_devs].name),
		"%s (%s)", name, devc->chip_name);
    else
	snprintf(ad1848_pcm_operations[nr_ad1848_devs].name,
	    sizeof(ad1848_pcm_operations[nr_ad1848_devs].name),
		"Generic audio codec (%s)", devc->chip_name);

    conf_printf2(ad1848_pcm_operations[nr_ad1848_devs].name,
	     devc->base, devc->irq, dma_playback, dma_capture);


    /* ad1848_pcm_operations[nr_ad1848_devs].flags |= DMA_AUTOMODE ; */

    if (num_audiodevs < MAX_AUDIO_DEV) {
	audio_devs[my_dev = num_audiodevs++] =
			&ad1848_pcm_operations[nr_ad1848_devs];
	if (irq > 0) {
	    audio_devs[my_dev]->devc = devc;
	    irq2dev[irq] = my_dev;
	    if (snd_set_irq_handler(devc->irq, ad1848_interrupt, devc->osp)<0) {
		printf("ad1848: IRQ in use\n");
	    }
#ifdef NO_IRQ_TEST
	    if (devc->mode != MD_1848) {
		int      x;
		u_char   tmp = ad_read(devc, 16);

		devc->timer_ticks = 0;

		ad_write(devc, 21, 0x00);	/* Timer msb */
		ad_write(devc, 20, 0x10);	/* Timer lsb */

		ad_write(devc, 16, tmp | 0x40);	/* Enable timer */
		for (x = 0; x < 100000 && devc->timer_ticks == 0; x++);
		ad_write(devc, 16, tmp & ~0x40);	/* Disable timer */

		if (devc->timer_ticks == 0)
		    printf("[IRQ conflict???]");
		else
		    devc->irq_ok = 1;

	    } else
		devc->irq_ok = 1;	/* Couldn't test. assume it's OK */
#else
	    devc->irq_ok = 1;
#endif
	} else if (irq < 0)
	    irq2dev[-irq] = devc->dev_no = my_dev;

	audio_devs[my_dev]->otherside = -1 ;
	audio_devs[my_dev]->flags |= DMA_AUTOMODE;
	audio_devs[my_dev]->dmachan1 = dma_playback;
	audio_devs[my_dev]->dmachan2 = dma_capture;
	audio_devs[my_dev]->buffsize = DSP_BUFFSIZE;
	audio_devs[my_dev]->devc = devc;
	audio_devs[my_dev]->format_mask = ad_format_mask[devc->mode];
	nr_ad1848_devs++;

#ifdef CONFIG_SEQUENCER
	if (devc->mode != MD_1848 && devc->irq_ok)
	    ad1848_tmr_install(my_dev);
#endif

	/*
	 * Toggle the MCE bit. It completes the initialization phase.
	 */

	ad_enter_MCE(devc);	/* In case the bit was off */
	ad_leave_MCE(devc);

	if (num_mixers < MAX_MIXER_DEV) {
	    mixer2codec[num_mixers] = my_dev + 1;
	    audio_devs[my_dev]->mixer_dev = num_mixers;
	    mixer_devs[num_mixers++] = &ad1848_mixer_operations;
	    ad1848_mixer_reset(devc);
	}
    } else
	printf("AD1848: Too many PCM devices available\n");
}

void
ad1848_interrupt(int irq)
{
    u_char   status;
    ad1848_info    *devc;
    int             dev;

    if (irq < 0 || irq > 15)
	dev = -1;
    else
	dev = irq2dev[irq];

    if (dev < 0 || dev >= num_audiodevs) {
	for (irq = 0; irq < 17; irq++)
	    if (irq2dev[irq] != -1)
		break;

	if (irq > 15) {
	    printf("ad1848.c: Bogus interrupt %d\n", irq);
	    return;
	}
	dev = irq2dev[irq];
    }
    devc = (ad1848_info *) audio_devs[dev]->devc;

    status = inb(io_Status(devc));

    if (status & 0x01) {	/* we have an interrupt */
	int    alt_stat = 0xff ;

	if (devc->mode != MD_1848) {
	    /*
	     * high-end devices have full-duplex dma and timer.
	     * the exact reason for the interrupt is in reg. I24.
	     * For old devices, we fake the interrupt bits, and
	     * determine the real reason basing on the device mode.
	     */
	    alt_stat = ad_read(devc, 24);
	    if (alt_stat & 0x40) {	/* Timer interrupt */
		devc->timer_ticks++;
#ifdef CONFIG_SEQUENCER
		if (timer_installed == dev && devc->timer_running)
		    sound_timer_interrupt();
#endif
	    }
	}

	outb(io_Status(devc), 0);	/* Clear interrupt status */

	if (audio_devs[dev]->busy) {

	    if (devc->irq_mode & PCM_ENABLE_OUTPUT && alt_stat & 0x10)
	    DMAbuf_outputintr(dev, 1);

	    if (devc->irq_mode & PCM_ENABLE_INPUT && alt_stat & 0x20)
	    DMAbuf_inputintr(dev);
	}
    }
}

/*
 * Some extra code for the MS Sound System
 */

#ifdef amancio
void
check_opl3(int base, struct address_info * hw_config)
{

    if (!opl3_detect(base, hw_config->osp))
	return;

    opl3_init(0, base, hw_config->osp);
}
#endif

/*
 * this is the probe routine. Note, it is not necessary to
 * go through this for PnP devices, since they are already
 * indentified precisely using their PnP id.
 *
 */

int
probe_mss(struct address_info * hw_config)
{
    u_char   tmp;

    DDB(printf("Entered probe_mss(io 0x%x, type %d)\n",
	    hw_config->io_base, hw_config->card_subtype));

    if (hw_config->card_subtype == 1) {	/* Has no IRQ/DMA registers */
	/* check_opl3(0x388, hw_config); */
	goto probe_ms_end;
    }

#if defined(CONFIG_AEDSP16) && defined(AEDSP16_MSS)
    /*
     * Initialize Audio Excel DSP 16 to MSS: before any operation we must
     * enable MSS I/O ports.
     */
    InitAEDSP16_MSS(hw_config);
#endif

    /*
     * Check if the IO port returns valid signature. The original MS
     * Sound system returns 0x04 while some cards (AudioTriX Pro for
     * example) return 0x00 or 0x0f.
     */

    if ((tmp = inb(hw_config->io_base + 3)) == 0xff) {	/* Bus float */
	DDB(printf("I/O address inactive (%x), force type 1\n", tmp));
	hw_config->card_subtype = 1 ;
	goto probe_ms_end;
    }

    if ((tmp & 0x3f) != 0x04 &&
	(tmp & 0x3f) != 0x0f &&
	(tmp & 0x3f) != 0x00) {
	DDB(printf("No MSS signature detected on port 0x%x (0x%x)\n",
		   hw_config->io_base, inb(hw_config->io_base + 3)));
	return 0;
    }
    if (hw_config->irq > 11) {
	printf("MSS: Bad IRQ %d\n", hw_config->irq);
	return 0;
    }
    if (hw_config->dma != 0 && hw_config->dma != 1 && hw_config->dma != 3) {
	printf("MSS: Bad DMA %d\n", hw_config->dma);
	return 0;
    }
    /*
     * Check that DMA0 is not in use with a 8 bit board.
     */

    if (hw_config->dma == 0 && inb(hw_config->io_base + 3) & 0x80) {
	printf("MSS: Can't use DMA0 with a 8 bit card/slot\n");
	return 0;
    }
    if (hw_config->irq > 7 && hw_config->irq != 9 &&
	    inb(hw_config->io_base + 3) & 0x80) {
	printf("MSS: Can't use IRQ%d with a 8 bit card/slot\n", hw_config->irq);
	return 0;
    }
probe_ms_end:
    return ad1848_detect(hw_config->io_base + 4, NULL, hw_config->osp);
}

void
attach_mss(struct address_info * hw_config)
{

#if 0
    /*
     * XXX do we really need to detect it again ? - lr970712
     */
    if (!ad1848_detect(hw_config->io_base + 4, NULL, hw_config->osp))
	return ;
#endif

    if (hw_config->card_subtype == 1) {	/* Has no IRQ/DMA registers */
	ad1848_init("MS Sound System1", hw_config->io_base + 4,
		    hw_config->irq,
		    hw_config->dma,
		    hw_config->dma2, 0, hw_config->osp);
    } else {
	/*
	 * Set the IRQ and DMA addresses.
	 */
#ifdef PC98
	static char		interrupt_bits[13] = {
	    -1, -1, -1, 0x08, -1, 0x10, -1, -1, -1, -1, 0x18, -1, 0x20
	};
#else
	static char     interrupt_bits[12] = {
	    -1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20
	};
#endif
	static char     dma_bits[4] = {
	    1, 2, 0, 3
	};

	int	config_port = hw_config->io_base + 0;
	int	version_port = hw_config->io_base + 3;
	char	bits = interrupt_bits[hw_config->irq];

	if (bits == -1)
	    return ;

#ifndef PC98
	outb(config_port, bits | 0x40);
	if ((inb(version_port) & 0x40) == 0)
	    printf("[IRQ Conflict?]");
#endif

	/* Write IRQ+DMA setup */
	outb(config_port, bits | dma_bits[hw_config->dma]);

	ad1848_init("MS Sound System0", hw_config->io_base + 4,
	    hw_config->irq,
	    hw_config->dma,
	    hw_config->dma, 0, hw_config->osp);
    }
    return ;
}

/*
 * WSS compatible PnP codec support.
 * XXX I doubt it works now - lr970712
 */

int
probe_pnp_ad1848(struct address_info * hw_config)
{
    return ad1848_detect(hw_config->io_base, NULL, hw_config->osp);
}

void
attach_pnp_ad1848(struct address_info * hw_config)
{

    ad1848_init(hw_config->name, hw_config->io_base,
		hw_config->irq,
		hw_config->dma,
		hw_config->dma2, 0, hw_config->osp);
}

#ifdef CONFIG_SEQUENCER
/*
 * Timer stuff (for /dev/music).
 */

static u_int current_interval = 0;

static u_int
ad1848_tmr_start(int dev, u_int usecs)
{
    u_long   flags;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;
    u_long   xtal_nsecs;	/* nanoseconds per xtal oscillaror tick */
    u_long   divider;

    flags = splhigh();

    /*
     * Length of the timer interval (in nanoseconds) depends on the
     * selected crystal oscillator. Check this from bit 0x01 of I8.
     * 
     * AD1845 has just one oscillator which has cycle time of 10.050 us
     * (when a 24.576 MHz xtal oscillator is used).
     * 
     * Convert requested interval to nanoseconds before computing the timer
     * divider.
     */

    if (devc->mode == MD_1845)
	xtal_nsecs = 10050;
    else if (ad_read(devc, 8) & 0x01)
	xtal_nsecs = 9920;
    else
	xtal_nsecs = 9969;

    divider = (usecs * 1000 + xtal_nsecs / 2) / xtal_nsecs;

    if (divider < 100)	/* Don't allow shorter intervals than about 1ms */
	divider = 100;

    if (divider > 65535)	/* Overflow check */
	divider = 65535;

    ad_write(devc, 21, (divider >> 8) & 0xff);	/* Set upper bits */
    ad_write(devc, 20, divider & 0xff);	/* Set lower bits */
    ad_write(devc, 16, ad_read(devc, 16) | 0x40);	/* Start the timer */
    devc->timer_running = 1;
    splx(flags);

    return current_interval = (divider * xtal_nsecs + 500) / 1000;
}

static void
ad1848_tmr_reprogram(int dev)
{
    /*
     * Audio driver has changed sampling rate so that a different xtal
     * oscillator was selected. We have to reprogram the timer rate.
     */

    ad1848_tmr_start(dev, current_interval);
    sound_timer_syncinterval(current_interval);
}

static void
ad1848_tmr_disable(int dev)
{
    u_long   flags;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

    flags = splhigh();
    ad_write(devc, 16, ad_read(devc, 16) & ~0x40);
    devc->timer_running = 0;
    splx(flags);
}

static void
ad1848_tmr_restart(int dev)
{
    u_long   flags;
    ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

    if (current_interval == 0)
	return;

    flags = splhigh();
    ad_write(devc, 16, ad_read(devc, 16) | 0x40);
    devc->timer_running = 1;
    splx(flags);
}

static struct sound_lowlev_timer ad1848_tmr = {
	0,
	ad1848_tmr_start,
	ad1848_tmr_disable,
	ad1848_tmr_restart
};

static int
ad1848_tmr_install(int dev)
{
    if (timer_installed != -1)
	return 0;	/* Don't install another timer */

    timer_installed = ad1848_tmr.dev = dev;
    sound_timer_init(&ad1848_tmr, audio_devs[dev]->name);

    return 1;
}
#endif
#endif
