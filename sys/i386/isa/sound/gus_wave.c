/*
 * sound/gus_wave.c
 * 
 * Driver for the Gravis UltraSound wave table synth.
 * 
 * Copyright by Hannu Savolainen 1993, 1994
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
 * $FreeBSD$
 */

#include <stddef.h>

#include <i386/isa/sound/sound_config.h>
#include <i386/isa/sound/ultrasound.h>
#include <i386/isa/sound/gus_hw.h>
#include <i386/isa/sound/iwdefs.h>
#include <machine/clock.h>

/* PnP stuff */
#define GUS_PNP_ID 0x100561e

#define MAX_CARDS 8
#define MAX_GUS_PNP 12


/* Static ports */
#define PADDRESS	0x279
#define PWRITE_DATA	0xa79
#define SET_CSN			0x06
#define PSTATUS			0x05

/* PnP Registers.  Write to ADDRESS and then use WRITE/READ_DATA */
#define SET_RD_DATA		0x00
#define SERIAL_ISOLATION	0x01
#define WAKE			0x03

#if defined(CONFIG_GUS)

static IWAVE iw;
#define 	ENTER_CRITICAL

#define 	LEAVE_CRITICAL

#define MAX_SAMPLE	150
#define MAX_PATCH	256


static u_int gus_pnp_found[MAX_GUS_PNP] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

struct voice_info {
	u_long   orig_freq;
	u_long   current_freq;
	u_long   mode;
	int             bender;
	int             bender_range;
	int             panning;
	int             midi_volume;
	u_int    initial_volume;
	u_int    current_volume;
	int             loop_irq_mode, loop_irq_parm;
#define LMODE_FINISH		1
#define LMODE_PCM		2
#define LMODE_PCM_STOP		3
	int             volume_irq_mode, volume_irq_parm;
#define VMODE_HALT		1
#define VMODE_ENVELOPE		2
#define VMODE_START_NOTE	3

	int             env_phase;
	u_char   env_rate[6];
	u_char   env_offset[6];

	/*
	 * Volume computation parameters for gus_adagio_vol()
	 */
	int             main_vol, expression_vol, patch_vol;

	/* Variables for "Ultraclick" removal */
	int             dev_pending, note_pending, volume_pending, sample_pending;
	char            kill_pending;
	long            offset_pending;

};

static struct voice_alloc_info *voice_alloc;

extern int      gus_base;
extern int      gus_irq, gus_dma;
static int      gus_dma2 = -1;
static int      dual_dma_mode = 0;
static long     gus_mem_size = 0;
static long     free_mem_ptr = 0;
static int      gus_no_dma = 0;
static int      nr_voices = 0;
static int      gus_devnum = 0;
static int      volume_base, volume_scale, volume_method;
static int      gus_recmask = SOUND_MASK_MIC;
static int      recording_active = 0;
static int      only_read_access = 0;
static int      only_8_bits = 0;

int             gus_wave_volume = 60;
static int      gus_pcm_volume = 80;
int             have_gus_max = 0;
static int      gus_line_vol = 100, gus_mic_vol = 0;
static u_char mix_image = 0x00;

int             gus_timer_enabled = 0;
/*
 * Current version of this driver doesn't allow synth and PCM functions at
 * the same time. The active_device specifies the active driver
 */
static int      active_device = 0;

#define GUS_DEV_WAVE		1	/* Wave table synth */
#define GUS_DEV_PCM_DONE	2	/* PCM device, transfer done */
#define GUS_DEV_PCM_CONTINUE	3	/* PCM device, transfer done ch. 1/2 */

static int      gus_sampling_speed;
static int      gus_sampling_channels;
static int      gus_sampling_bits;

static int     *dram_sleeper = NULL;
static volatile struct snd_wait dram_sleep_flag =
{0};

/*
 * Variables and buffers for PCM output
 */
#define MAX_PCM_BUFFERS		(32*MAX_REALTIME_FACTOR)	/* Don't change */

static int      pcm_bsize, pcm_nblk, pcm_banksize;
static int      pcm_datasize[MAX_PCM_BUFFERS];
static volatile int pcm_head, pcm_tail, pcm_qlen;
static volatile int pcm_active;
static volatile int dma_active;
static int      pcm_opened = 0;
static int      pcm_current_dev;
static int      pcm_current_block;
static u_long pcm_current_buf;
static int      pcm_current_count;
static int      pcm_current_intrflag;

extern sound_os_info *gus_osp;

static struct voice_info voices[32];

static int      freq_div_table[] =
{
	44100,			/* 14 */
	41160,			/* 15 */
	38587,			/* 16 */
	36317,			/* 17 */
	34300,			/* 18 */
	32494,			/* 19 */
	30870,			/* 20 */
	29400,			/* 21 */
	28063,			/* 22 */
	26843,			/* 23 */
	25725,			/* 24 */
	24696,			/* 25 */
	23746,			/* 26 */
	22866,			/* 27 */
	22050,			/* 28 */
	21289,			/* 29 */
	20580,			/* 30 */
	19916,			/* 31 */
	19293			/* 32 */
};

static struct patch_info *samples;
static struct patch_info *dbg_samples;
static int                dbg_samplep;

static long     sample_ptrs[MAX_SAMPLE + 1];
static int      sample_map[32];
static int      free_sample;
static int      mixer_type = 0;


static int      patch_table[MAX_PATCH];
static int      patch_map[32];

static struct synth_info gus_info =
{"Gravis UltraSound", 0, SYNTH_TYPE_SAMPLE, SAMPLE_TYPE_GUS, 0, 16, 0, MAX_PATCH};

static void     gus_default_mixer_init(void);

static int      guswave_start_note2(int dev, int voice, int note_num, int volume);
static void     gus_poke(long addr, u_char data);
static void     compute_and_set_volume(int voice, int volume, int ramp_time);
extern u_short gus_adagio_vol(int vel, int mainv, int xpn, int voicev);
extern u_short gus_linear_vol(int vol, int mainvol);
static void     compute_volume(int voice, int volume);
static void     do_volume_irq(int voice);
static void     set_input_volumes(void);
static void     gus_tmr_install(int io_base);

static void     SEND(int d, int r);
static int      get_serial(int rd_port, u_char *data);
static void     send_Initiation_LFSR(void);
static int      isolation_protocol(int rd_port);


#define	INSTANT_RAMP		-1	/* Instant change. No ramping */
#define FAST_RAMP		0	/* Fastest possible ramp */


/* Crystal Select */
#define CODEC_XTAL2			0x01	/* 16.9344 crystal */
#define CODEC_XTAL1			0x00	/* 24.576 crystal */
/************************************************************************/

/************************************************************************/
/* Definitions for CONFIG_1 register                                    */
#define CODEC_CFIG1I_DEFAULT    0x03 | 0x8
#define CODEC_CAPTURE_PIO	0x80	/* Capture PIO enable */
#define CODEC_PLAYBACK_PIO	0x40	/* Playback PIO enable */
#define CODEC_AUTOCALIB		0x08	/* auto calibrate */
#define CODEC_SINGLE_DMA	0x04	/* Use single DMA channel */
#define CODEC_RE		0x02	/* Capture enable */
#define CODEC_PE		0x01	/* playback enable */
/************************************************************************/

/************************************************************************/
/* Definitions for CONFIG_2 register                                    */
#define CODEC_CFIG2I_DEFAULT    0x81
#define CODEC_OFVS 0x80		/* Output Full Scale Voltage */
#define CODEC_TE   0x40		/* Timer Enable */
#define CODEC_RSCD 0x20		/* Recors Sample Counter Disable */
#define CODEC_PSCD 0x10		/* Playback Sample Counter Disable */
#define CODEC_DAOF 0x01		/* D/A Ouput Force Enable */
/************************************************************************/

/************************************************************************/
/* Definitions for CONFIG_3 register                                    */
/* #define CODEC_CFIG3I_DEFAULT    0xe0  0x02 when synth DACs are working */

#define CODEC_CFIG3I_DEFAULT    0xc0	/* 0x02 when synth DACs are working */
#define CODEC_RPIE    0x80	/* Record FIFO IRQ Enable */
#define CODEC_PPIE    0x40	/* Playback FIFO IRQ Enable */
#define CODEC_FT_MASK 0x30	/* FIFO Threshold Select */
#define CODEC_PVFM    0x04	/* Playback Variable Frequency Mode */
#define CODEC_SYNA    0x02	/* AUX1/Synth Signal Select */
/************************************************************************/

/************************************************************************/
/* Definitions for EXTERNAL_CONTROL register                            */
#define CODEC_CEXTI_DEFAULT     0x00
#define CODEC_IRQ_ENABLE	0x02	/* interrupt enable */
#define CODEC_GPOUT1		0x80	/* external control #1 */
#define CODEC_GPOUT0		0x40	/* external control #0 */
/************************************************************************/

/************************************************************************/
/* Definitions for MODE_SELECT_ID register                              */
#define CODEC_MODE_DEFAULT 0x40
#define CODEC_MODE_MASK  0x60
#define CODEC_ID_BIT4    0x80
#define CODEC_ID_BIT3_0  0x0F
/************************************************************************/
#define CONFIG_1            0x09
#define EXTERNAL_CONTROL    0x0a/* Pin control */
#define STATUS_2            0x0b/* Test and initialization */
#define MODE_SELECT_ID      0x0c/* Miscellaneaous information */
#define LOOPBACK            0x0d/* Digital Mix */
#define UPPER_PLAY_COUNT    0x0e/* Playback Upper Base Count */
#define LOWER_PLAY_COUNT    0x0f/* Playback Lower Base Count */
#define CONFIG_2            0x10
#define CONFIG_3            0x11


#define IWL_CODEC_OUT(reg, val) \
    { outb(iwl_codec_base, reg); outb(iwl_codec_data, val); }

#define IWL_CODEC_IN(reg, val) \
    { outb(iwl_codec_base, reg); val = inb(iwl_codec_data); }


static u_char   gus_look8(int reg);

static void     gus_write16(int reg, u_int data);

static u_short  gus_read16(int reg);

static void     gus_write_addr(int reg, u_long address, int is16bit);
static void     IwaveLineLevel(char level, char index);
static void     IwaveInputSource(BYTE index, BYTE source);
static void     IwavePnpGetCfg(void);
static void     IwavePnpDevice(BYTE dev);
static void     IwavePnpSetCfg(void);
static void     IwavePnpKey(void);
static BYTE     IwavePnpIsol(PORT * pnpread);
static void     IwavePnpPeek(PORT pnprdp, WORD bytes, BYTE * data);
static void     IwavePnpActivate(BYTE dev, BYTE bool);
static void     IwavePnpWake(BYTE csn);
static BYTE     IwavePnpPing(DWORD VendorID);
static WORD     IwaveMemSize(void);
static BYTE     IwaveMemPeek(ADDRESS addr);
static void     IwaveMemPoke(ADDRESS addr, BYTE datum);
static void     IwaveMemCfg(DWORD * lpbanks);
static void     IwaveCodecIrq(BYTE mode);
static WORD     IwaveRegPeek(DWORD reg_mnem);
				
static void     IwaveRegPoke(DWORD reg_mnem, WORD datum);
static void     IwaveCodecMode(char mode);
static void     IwaveLineMute(BYTE mute, BYTE inx);
static void     Iwaveinitcodec(void);
int             IwaveOpen(char voices, char mode, struct address_info * hw);


static void
reset_sample_memory(void)
{
    int             i;

    for (i = 0; i <= MAX_SAMPLE; i++)
	sample_ptrs[i] = -1;
    for (i = 0; i < 32; i++)
	sample_map[i] = -1;
    for (i = 0; i < 32; i++)
	patch_map[i] = -1;

    gus_poke(0, 0);		/* Put a silent sample to the beginning */
    gus_poke(1, 0);
    free_mem_ptr = 2;

    free_sample = 0;

    for (i = 0; i < MAX_PATCH; i++)
	patch_table[i] = -1;
}

void
gus_delay(void)
{
    int             i;

    for (i = 0; i < 7; i++)
	inb(u_DRAMIO);
}

static void
gus_poke(long addr, u_char data)
{				/* Writes a byte to the DRAM */
    u_long   flags;

    flags = splhigh();
    outb(u_Command, 0x43);
    outb(u_DataLo, addr & 0xff);
    outb(u_DataHi, (addr >> 8) & 0xff);

    outb(u_Command, 0x44);
    outb(u_DataHi, (addr >> 16) & 0xff);
    outb(u_DRAMIO, data);
    splx(flags);
}

static u_char
gus_peek(long addr)
{				/* Reads a byte from the DRAM */
    u_long   flags;
    u_char   tmp;

    flags = splhigh();
    outb(u_Command, 0x43);
    outb(u_DataLo, addr & 0xff);
    outb(u_DataHi, (addr >> 8) & 0xff);

    outb(u_Command, 0x44);
    outb(u_DataHi, (addr >> 16) & 0xff);
    tmp = inb(u_DRAMIO);
    splx(flags);

    return tmp;
}

void
gus_write8(int reg, u_int data)
{				/* Writes to an indirect register (8 bit) */
    u_long   flags;

    flags = splhigh();
    outb(u_Command, reg);
    outb(u_DataHi, (u_char) (data & 0xff));
    splx(flags);
}

u_char
gus_read8(int reg)
{	/* Reads from an indirect register (8 bit). Offset 0x80. */
    u_long   flags;
    u_char   val;

    flags = splhigh();
    outb(u_Command, reg | 0x80);
    val = inb(u_DataHi);
    splx(flags);

    return val;
}

static u_char
gus_look8(int reg)
{	/* Reads from an indirect register (8 bit). No additional offset. */
    u_long   flags;
    u_char   val;

    flags = splhigh();
    outb(u_Command, reg);
    val = inb(u_DataHi);
    splx(flags);

    return val;
}

static void
gus_write16(int reg, u_int data)
{			/* Writes to an indirect register (16 bit) */
    u_long   flags;

    flags = splhigh();

    outb(u_Command, reg);

    outb(u_DataLo, (u_char) (data & 0xff));
    outb(u_DataHi, (u_char) ((data >> 8) & 0xff));

    splx(flags);
}

static u_short
gus_read16(int reg)
{		/* Reads from an indirect register (16 bit). Offset 0x80. */
    u_long   flags;
    u_char   hi, lo;

    flags = splhigh();

    outb(u_Command, reg | 0x80);

    lo = inb(u_DataLo);
    hi = inb(u_DataHi);

    splx(flags);

    return ((hi << 8) & 0xff00) | lo;
}

static void
gus_write_addr(int reg, u_long address, int is16bit)
{				/* Writes an 24 bit memory address */
    u_long   hold_address;
    u_long   flags;

    flags = splhigh();
    if (is16bit) {
	/*
	 * Special processing required for 16 bit patches
	 */

	hold_address = address;
	address = address >> 1;
	address &= 0x0001ffffL;
	address |= (hold_address & 0x000c0000L);
    }
    gus_write16(reg, (u_short) ((address >> 7) & 0xffff));
    gus_write16(reg + 1, (u_short) ((address << 9) & 0xffff));
    /*
     * Could writing twice fix problems with GUS_VOICE_POS() ? Lets try...
     */
    gus_delay();
    gus_write16(reg, (u_short) ((address >> 7) & 0xffff));
    gus_write16(reg + 1, (u_short) ((address << 9) & 0xffff));
    splx(flags);
}

static void
gus_select_voice(int voice)
{
    if (voice < 0 || voice > 31)
	return;

    outb(u_Voice, voice);
}

static void
gus_select_max_voices(int nvoices)
{
    if (nvoices < 14)
	nvoices = 14;
    if (nvoices > 32)
	nvoices = 32;

    voice_alloc->max_voice = nr_voices = nvoices;

    gus_write8(0x0e, (nvoices - 1) | 0xc0);
}

static void
gus_voice_on(u_int mode)
{
    gus_write8(0x00, (u_char) (mode & 0xfc));
    gus_delay();
    gus_write8(0x00, (u_char) (mode & 0xfc));
}

static void
gus_voice_off(void)
{
    gus_write8(0x00, gus_read8(0x00) | 0x03);
}

static void
gus_voice_mode(u_int m)
{
    u_char   mode = (u_char) (m & 0xff);

    gus_write8(0x00, (gus_read8(0x00) & 0x03) |
	   (mode & 0xfc));	/* Don't touch last two bits */
    gus_delay();
    gus_write8(0x00, (gus_read8(0x00) & 0x03) | (mode & 0xfc));
}

static void
gus_voice_freq(u_long freq)
{
    u_long   divisor = freq_div_table[nr_voices - 14];
    u_short  fc;

    fc = (u_short) (((freq << 9) + (divisor >> 1)) / divisor);
    fc = fc << 1;

    gus_write16(0x01, fc);
}

static void
gus_voice_volume(u_int vol)
{
	gus_write8(0x0d, 0x03);	/* Stop ramp before setting volume */
	gus_write16(0x09, (u_short) (vol << 4));
}

static void
gus_voice_balance(u_int balance)
{
	gus_write8(0x0c, (u_char) (balance & 0xff));
}

static void
gus_ramp_range(u_int low, u_int high)
{
	gus_write8(0x07, (u_char) ((low >> 4) & 0xff));
	gus_write8(0x08, (u_char) ((high >> 4) & 0xff));
}

static void
gus_ramp_rate(u_int scale, u_int rate)
{
	gus_write8(0x06, (u_char) (((scale & 0x03) << 6) | (rate & 0x3f)));
}

static void
gus_rampon(u_int m)
{
	u_char   mode = (u_char) (m & 0xff);

	gus_write8(0x0d, mode & 0xfc);
	gus_delay();
	gus_write8(0x0d, mode & 0xfc);
}

static void
gus_ramp_mode(u_int m)
{
	u_char   mode = (u_char) (m & 0xff);

	gus_write8(0x0d, (gus_read8(0x0d) & 0x03) |
		   (mode & 0xfc));	/* Leave the last 2 bits alone */
	gus_delay();
	gus_write8(0x0d, (gus_read8(0x0d) & 0x03) | (mode & 0xfc));
}

static void
gus_rampoff(void)
{
	gus_write8(0x0d, 0x03);
}

static void
gus_set_voice_pos(int voice, long position)
{
	int             sample_no;

	if ((sample_no = sample_map[voice]) != -1) {
		if (position < samples[sample_no].len) {
			if (voices[voice].volume_irq_mode == VMODE_START_NOTE)
				voices[voice].offset_pending = position;
			else
				gus_write_addr(0x0a, sample_ptrs[sample_no] + position,
				    samples[sample_no].mode & WAVE_16_BITS);
		}
	}
}

static void
gus_voice_init(int voice)
{
	u_long   flags;

	flags = splhigh();
	gus_select_voice(voice);
	gus_voice_volume(0);
	gus_voice_off();
	gus_write_addr(0x0a, 0, 0);	/* Set current position to 0 */
	gus_write8(0x00, 0x03);	/* Voice off */
	gus_write8(0x0d, 0x03);	/* Ramping off */
	voice_alloc->map[voice] = 0;
	voice_alloc->alloc_times[voice] = 0;
	splx(flags);

}

static void
gus_voice_init2(int voice)
{
	voices[voice].panning = 0;
	voices[voice].mode = 0;
	voices[voice].orig_freq = 20000;
	voices[voice].current_freq = 20000;
	voices[voice].bender = 0;
	voices[voice].bender_range = 200;
	voices[voice].initial_volume = 0;
	voices[voice].current_volume = 0;
	voices[voice].loop_irq_mode = 0;
	voices[voice].loop_irq_parm = 0;
	voices[voice].volume_irq_mode = 0;
	voices[voice].volume_irq_parm = 0;
	voices[voice].env_phase = 0;
	voices[voice].main_vol = 127;
	voices[voice].patch_vol = 127;
	voices[voice].expression_vol = 127;
	voices[voice].sample_pending = -1;
}

static void
step_envelope(int voice)
{
    u_int        vol, prev_vol, phase;
    u_char   rate;
    long int        flags;

    if (voices[voice].mode & WAVE_SUSTAIN_ON && voices[voice].env_phase == 2) {
	flags = splhigh();
	gus_select_voice(voice);
	gus_rampoff();
	splx(flags);
	return;
	/*
	 * Sustain phase begins. Continue envelope after receiving
	 * note off.
	 */
    }
    if (voices[voice].env_phase >= 5) {	/* Envelope finished. Shoot
					 * the voice down */
	gus_voice_init(voice);
	return;
    }
    prev_vol = voices[voice].current_volume;
    phase = ++voices[voice].env_phase;
    compute_volume(voice, voices[voice].midi_volume);
    vol = voices[voice].initial_volume * voices[voice].env_offset[phase] / 255;
    rate = voices[voice].env_rate[phase];

    flags = splhigh();
    gus_select_voice(voice);
    gus_voice_volume(prev_vol);
    gus_write8(0x06, rate);	/* Ramping rate */

    voices[voice].volume_irq_mode = VMODE_ENVELOPE;

    if (((vol - prev_vol) / 64) == 0) {	/* No significant volume
					 * change */
	splx(flags);
	step_envelope(voice);	/* Continue the envelope on the next
				 * step */
	return;
    }
    if (vol > prev_vol) {
	if (vol >= (4096 - 64))
	    vol = 4096 - 65;
	gus_ramp_range(0, vol);
	gus_rampon(0x20);	/* Increasing volume, with IRQ */
    } else {
	if (vol <= 64)
	    vol = 65;
	gus_ramp_range(vol, 4030);
	gus_rampon(0x60);	/* Decreasing volume, with IRQ */
    }
    voices[voice].current_volume = vol;
    splx(flags);
}

static void
init_envelope(int voice)
{
    voices[voice].env_phase = -1;
    voices[voice].current_volume = 64;

    step_envelope(voice);
}

static void
start_release(int voice, long int flags)
{
    if (gus_read8(0x00) & 0x03)
	return;		/* Voice already stopped */

    voices[voice].env_phase = 2;	/* Will be incremented by
				     * step_envelope */

    voices[voice].current_volume =
    voices[voice].initial_volume =
	gus_read16(0x09) >> 4;	/* Get current volume */

    voices[voice].mode &= ~WAVE_SUSTAIN_ON;
    gus_rampoff();
    splx(flags);
    step_envelope(voice);
}

static void
gus_voice_fade(int voice)
{
    int             instr_no = sample_map[voice], is16bits;
    long int        flags;

    flags = splhigh();
    gus_select_voice(voice);

    if (instr_no < 0 || instr_no > MAX_SAMPLE) {
	gus_write8(0x00, 0x03);	/* Hard stop */
	voice_alloc->map[voice] = 0;
	splx(flags);
	return;
    }
    is16bits = (samples[instr_no].mode & WAVE_16_BITS) ? 1 : 0;	/* 8 or 16 bits */

    if (voices[voice].mode & WAVE_ENVELOPES) {
	start_release(voice, flags);
	return;
    }
    /*
     * Ramp the volume down but not too quickly.
     */
    if ((int) (gus_read16(0x09) >> 4) < 100) {	/* Get current volume */
	gus_voice_off();
	gus_rampoff();
	gus_voice_init(voice);
	return;
    }
    gus_ramp_range(65, 4030);
    gus_ramp_rate(2, 4);
    gus_rampon(0x40 | 0x20);/* Down, once, with IRQ */
    voices[voice].volume_irq_mode = VMODE_HALT;
    splx(flags);
}

static void
gus_reset(void)
{
    int             i;

    gus_select_max_voices(24);
    volume_base = 3071;
    volume_scale = 4;
    volume_method = VOL_METHOD_ADAGIO;

    for (i = 0; i < 32; i++) {
	gus_voice_init(i);	/* Turn voice off */
	gus_voice_init2(i);
    }

    inb(u_Status);		/* Touch the status register */

    gus_look8(0x41);	/* Clear any pending DMA IRQs */
    gus_look8(0x49);	/* Clear any pending sample IRQs */

    gus_read8(0x0f);	/* Clear pending IRQs */

}

static void
gus_initialize(void)
{
    u_long   flags;
    u_char   dma_image, irq_image, tmp;

    static u_char gus_irq_map[16] =
	{0, 0, 0, 3, 0, 2, 0, 4, 0, 1, 0, 5, 6, 0, 0, 7};

    static u_char gus_dma_map[8] =
	{0, 1, 0, 2, 0, 3, 4, 5};

    flags = splhigh();
    gus_write8(0x4c, 0);	/* Reset GF1 */
    gus_delay();
    gus_delay();

    gus_write8(0x4c, 1);	/* Release Reset */
    gus_delay();
    gus_delay();

    /*
     * Clear all interrupts
     */

    gus_write8(0x41, 0);	/* DMA control */
    gus_write8(0x45, 0);	/* Timer control */
    gus_write8(0x49, 0);	/* Sample control */

    gus_select_max_voices(24);

    inb(u_Status);		/* Touch the status register */

    gus_look8(0x41);	/* Clear any pending DMA IRQs */
    gus_look8(0x49);	/* Clear any pending sample IRQs */
    gus_read8(0x0f);	/* Clear pending IRQs */

    gus_reset();		/* Resets all voices */

    gus_look8(0x41);	/* Clear any pending DMA IRQs */
    gus_look8(0x49);	/* Clear any pending sample IRQs */
    gus_read8(0x0f);	/* Clear pending IRQs */

    gus_write8(0x4c, 7);	/* Master reset | DAC enable | IRQ enable */

    /*
     * Set up for Digital ASIC
     */

    outb(gus_base + 0x0f, 0x05);

    mix_image |= 0x02;	/* Disable line out (for a moment) */
    outb(u_Mixer, mix_image);

    outb(u_IRQDMAControl, 0x00);

    outb(gus_base + 0x0f, 0x00);

    /*
     * Now set up the DMA and IRQ interface
     * 
     * The GUS supports two IRQs and two DMAs.
     * 
     * Just one DMA channel is used. This prevents simultaneous ADC and DAC.
     * Adding this support requires significant changes to the dmabuf.c,
     * dsp.c and audio.c also.
     */

    irq_image = 0;
    tmp = gus_irq_map[gus_irq];
    if (!tmp)
	printf("Warning! GUS IRQ not selected\n");
    irq_image |= tmp;
    irq_image |= 0x40;	/* Combine IRQ1 (GF1) and IRQ2 (Midi) */

    dual_dma_mode = 1;
    if (gus_dma2 == gus_dma || gus_dma2 == -1) {
	dual_dma_mode = 0;
	dma_image = 0x40;	/* Combine DMA1 (DRAM) and IRQ2 (ADC) */

	tmp = gus_dma_map[gus_dma];
	if (!tmp)
	    printf("Warning! GUS DMA not selected\n");

	dma_image |= tmp;
    } else
	/* Setup dual DMA channel mode for GUS MAX */
    {
	dma_image = gus_dma_map[gus_dma];
	if (!dma_image)
	    printf("Warning! GUS DMA not selected\n");

	tmp = gus_dma_map[gus_dma2] << 3;
	if (!tmp) {
	    printf("Warning! Invalid GUS MAX DMA\n");
	    tmp = 0x40;	/* Combine DMA channels */
	    dual_dma_mode = 0;
	}
	dma_image |= tmp;
    }

    /*
     * For some reason the IRQ and DMA addresses must be written twice
     */

    /*
     * Doing it first time
     */

    outb(u_Mixer, mix_image);	/* Select DMA control */
    outb(u_IRQDMAControl, dma_image | 0x80);	/* Set DMA address */

    outb(u_Mixer, mix_image | 0x40);	/* Select IRQ control */
    outb(u_IRQDMAControl, irq_image);	/* Set IRQ address */

    /*
     * Doing it second time
     */

    outb(u_Mixer, mix_image);	/* Select DMA control */
    outb(u_IRQDMAControl, dma_image);	/* Set DMA address */

    outb(u_Mixer, mix_image | 0x40);	/* Select IRQ control */
    outb(u_IRQDMAControl, irq_image);	/* Set IRQ address */

    gus_select_voice(0);	/* This disables writes to IRQ/DMA reg */

    mix_image &= ~0x02;	/* Enable line out */
    mix_image |= 0x08;	/* Enable IRQ */
    outb(u_Mixer, mix_image);	/* Turn mixer channels on Note! Mic
				 * in is left off. */

    gus_select_voice(0);	/* This disables writes to IRQ/DMA reg */

    gusintr(0);		/* Serve pending interrupts */
    splx(flags);
}

int
gus_wave_detect(int baseaddr)
{
	u_long   i;
	u_long   loc;
	gus_base = baseaddr;

	gus_write8(0x4c, 0);	/* Reset GF1 */
	gus_delay();
	gus_delay();

	gus_write8(0x4c, 1);	/* Release Reset */
	gus_delay();
	gus_delay();

	/* See if there is first block there.... */
	gus_poke(0L, 0xaa);
	if (gus_peek(0L) != 0xaa)
		return (0);

	/* Now zero it out so that I can check for mirroring .. */
	gus_poke(0L, 0x00);
	for (i = 1L; i < 1024L; i++) {
		int             n, failed;

		/* check for mirroring ... */
		if (gus_peek(0L) != 0)
			break;
		loc = i << 10;

		for (n = loc - 1, failed = 0; n <= loc; n++) {
			gus_poke(loc, 0xaa);
			if (gus_peek(loc) != 0xaa)
				failed = 1;

			gus_poke(loc, 0x55);
			if (gus_peek(loc) != 0x55)
				failed = 1;
		}

		if (failed)
			break;
	}
	gus_mem_size = i << 10;
	return 1;
}

static int
guswave_ioctl(int dev,
	      u_int cmd, ioctl_arg arg)
{

    switch (cmd) {
    case SNDCTL_SYNTH_INFO:
	gus_info.nr_voices = nr_voices;
	bcopy(&gus_info, &(((char *) arg)[0]), sizeof(gus_info));
	return 0;
	break;

    case SNDCTL_SEQ_RESETSAMPLES:
	reset_sample_memory();
	return 0;
	break;

    case SNDCTL_SEQ_PERCMODE:
	return 0;
	break;

    case SNDCTL_SYNTH_MEMAVL:
	return gus_mem_size - free_mem_ptr - 32;

    default:
	return -(EINVAL);
    }
}

static int
guswave_set_instr(int dev, int voice, int instr_no)
{
	int             sample_no;

	if (instr_no < 0 || instr_no > MAX_PATCH)
		return -(EINVAL);

	if (voice < 0 || voice > 31)
		return -(EINVAL);

	if (voices[voice].volume_irq_mode == VMODE_START_NOTE) {
		voices[voice].sample_pending = instr_no;
		return 0;
	}
	sample_no = patch_table[instr_no];
	patch_map[voice] = -1;

	if (sample_no < 0) {
		printf("GUS: Undefined patch %d for voice %d\n", instr_no, voice);
		return -(EINVAL);	/* Patch not defined */
	}
	if (sample_ptrs[sample_no] == -1) {	/* Sample not loaded */
		printf("GUS: Sample #%d not loaded for patch %d (voice %d)\n",
		       sample_no, instr_no, voice);
		return -(EINVAL);
	}
	sample_map[voice] = sample_no;
	patch_map[voice] = instr_no;
	return 0;
}

static int
guswave_kill_note(int dev, int voice, int note, int velocity)
{
    u_long   flags;

    flags = splhigh();
    /* voice_alloc->map[voice] = 0xffff; */
    if (voices[voice].volume_irq_mode == VMODE_START_NOTE) {
	voices[voice].kill_pending = 1;
	splx(flags);
    } else {
	splx(flags);
	gus_voice_fade(voice);
    }

    splx(flags);
    return 0;
}

static void
guswave_aftertouch(int dev, int voice, int pressure)
{
}

static void
guswave_panning(int dev, int voice, int value)
{
    if (voice >= 0 || voice < 32)
	voices[voice].panning = value;
}

static void
guswave_volume_method(int dev, int mode)
{
    if (mode == VOL_METHOD_LINEAR || mode == VOL_METHOD_ADAGIO)
	volume_method = mode;
}

static void
compute_volume(int voice, int volume)
{
    if (volume < 128)
	voices[voice].midi_volume = volume;

    switch (volume_method) {
    case VOL_METHOD_ADAGIO:
	voices[voice].initial_volume =
	    gus_adagio_vol(voices[voice].midi_volume, voices[voice].main_vol,
		   voices[voice].expression_vol, voices[voice].patch_vol);
	break;

    case VOL_METHOD_LINEAR:/* Totally ignores patch-volume and expression */
	voices[voice].initial_volume =
	    gus_linear_vol(volume, voices[voice].main_vol);
	break;

    default:
	voices[voice].initial_volume = volume_base +
	    (voices[voice].midi_volume * volume_scale);
    }

    if (voices[voice].initial_volume > 4030)
	voices[voice].initial_volume = 4030;
}

static void
compute_and_set_volume(int voice, int volume, int ramp_time)
{
    int             curr, target, rate;
    u_long   flags;

    compute_volume(voice, volume);
    voices[voice].current_volume = voices[voice].initial_volume;

    flags = splhigh();
    /*
     * CAUTION! Interrupts disabled. Enable them before returning
     */

    gus_select_voice(voice);

    curr = gus_read16(0x09) >> 4;
    target = voices[voice].initial_volume;

    if (ramp_time == INSTANT_RAMP) {
	gus_rampoff();
	gus_voice_volume(target);
	splx(flags);
	return;
    }
    if (ramp_time == FAST_RAMP)
	rate = 63;
    else
	rate = 16;
    gus_ramp_rate(0, rate);

    if ((target - curr) / 64 == 0) {	/* Close enough to target. */
	gus_rampoff();
	gus_voice_volume(target);
	splx(flags);
	return;
    }
    if (target > curr) {
	if (target > (4095 - 65))
	    target = 4095 - 65;
	gus_ramp_range(curr, target);
	gus_rampon(0x00);	/* Ramp up, once, no IRQ */
    } else {
	if (target < 65)
	    target = 65;

	gus_ramp_range(target, curr);
	gus_rampon(0x40);	/* Ramp down, once, no irq */
    }
    splx(flags);
}

static void
dynamic_volume_change(int voice)
{
	u_char   status;
	u_long   flags;

	flags = splhigh();
	gus_select_voice(voice);
	status = gus_read8(0x00);	/* Get voice status */
	splx(flags);

	if (status & 0x03)
		return;		/* Voice was not running */

	if (!(voices[voice].mode & WAVE_ENVELOPES)) {
		compute_and_set_volume(voice, voices[voice].midi_volume, 1);
		return;
	}
	/*
	 * Voice is running and has envelopes.
	 */

	flags = splhigh();
	gus_select_voice(voice);
	status = gus_read8(0x0d);	/* Ramping status */
	splx(flags);

	if (status & 0x03) {	/* Sustain phase? */
		compute_and_set_volume(voice, voices[voice].midi_volume, 1);
		return;
	}
	if (voices[voice].env_phase < 0)
		return;

	compute_volume(voice, voices[voice].midi_volume);

}

static void
guswave_controller(int dev, int voice, int ctrl_num, int value)
{
	u_long   flags;
	u_long   freq;

	if (voice < 0 || voice > 31)
		return;

	switch (ctrl_num) {
	case CTRL_PITCH_BENDER:
		voices[voice].bender = value;

		if (voices[voice].volume_irq_mode != VMODE_START_NOTE) {
			freq = compute_finetune(voices[voice].orig_freq, value,
						voices[voice].bender_range);
			voices[voice].current_freq = freq;

			flags = splhigh();
			gus_select_voice(voice);
			gus_voice_freq(freq);
			splx(flags);
		}
		break;

	case CTRL_PITCH_BENDER_RANGE:
		voices[voice].bender_range = value;
		break;
	case CTL_EXPRESSION:
		value /= 128;
	case CTRL_EXPRESSION:
		if (volume_method == VOL_METHOD_ADAGIO) {
			voices[voice].expression_vol = value;
			if (voices[voice].volume_irq_mode != VMODE_START_NOTE)
				dynamic_volume_change(voice);
		}
		break;

	case CTL_PAN:
		voices[voice].panning = (value * 2) - 128;
		break;

	case CTL_MAIN_VOLUME:
		value = (value * 100) / 16383;

	case CTRL_MAIN_VOLUME:
		voices[voice].main_vol = value;
		if (voices[voice].volume_irq_mode != VMODE_START_NOTE)
			dynamic_volume_change(voice);
		break;

	default:
		break;
	}
}

static int
guswave_start_note2(int dev, int voice, int note_num, int volume)
{
    int             sample, best_sample, best_delta, delta_freq;
    int             is16bits, samplep, patch, pan;
    u_long   note_freq, base_note, freq, flags;
    u_char   mode = 0;

    if (voice < 0 || voice > 31) {
	printf("GUS: Invalid voice\n");
	return -(EINVAL);
    }
    if (note_num == 255) {
	if (voices[voice].mode & WAVE_ENVELOPES) {
	    voices[voice].midi_volume = volume;
	    dynamic_volume_change(voice);
	    return 0;
	}
	compute_and_set_volume(voice, volume, 1);
	return 0;
    }
    if ((patch = patch_map[voice]) == -1)
	return -(EINVAL);
    if ((samplep = patch_table[patch]) == -1)
	return -(EINVAL);
    note_freq = note_to_freq(note_num);

    /*
     * Find a sample within a patch so that the note_freq is between
     * low_note and high_note.
     */
    sample = -1;

    best_sample = samplep;
    best_delta = 1000000;
    while (samplep >= 0 && sample == -1) {
	dbg_samples = samples;
	dbg_samplep = samplep;

	delta_freq = note_freq - samples[samplep].base_note;
	if (delta_freq < 0)
	    delta_freq = -delta_freq;
	if (delta_freq < best_delta) {
	    best_sample = samplep;
	    best_delta = delta_freq;
	}
	if (samples[samplep].low_note <= note_freq &&
		note_freq <= samples[samplep].high_note)
	    sample = samplep;
	else
	    samplep = samples[samplep].key;	/* Follow link */
    }
    if (sample == -1)
	sample = best_sample;

    if (sample == -1) {
	printf("GUS: Patch %d not defined for note %d\n", patch, note_num);
	return 0;	/* Should play default patch ??? */
    }
    is16bits = (samples[sample].mode & WAVE_16_BITS) ? 1 : 0;
    voices[voice].mode = samples[sample].mode;
    voices[voice].patch_vol = samples[sample].volume;

    if (voices[voice].mode & WAVE_ENVELOPES) {
	int             i;

	for (i = 0; i < 6; i++) {
	    voices[voice].env_rate[i] = samples[sample].env_rate[i];
	    voices[voice].env_offset[i] = samples[sample].env_offset[i];
	}
    }
    sample_map[voice] = sample;

    base_note = samples[sample].base_note / 100; /* Try to avoid overflows */
    note_freq /= 100;

    freq = samples[sample].base_freq * note_freq / base_note;

    voices[voice].orig_freq = freq;

    /*
     * Since the pitch bender may have been set before playing the note,
     * we have to calculate the bending now.
     */

    freq = compute_finetune(voices[voice].orig_freq, voices[voice].bender,
			    voices[voice].bender_range);
    voices[voice].current_freq = freq;

    pan = (samples[sample].panning + voices[voice].panning) / 32;
    pan += 7;
    if (pan < 0)
	pan = 0;
    if (pan > 15)
	pan = 15;

    if (samples[sample].mode & WAVE_16_BITS) {
	mode |= 0x04;	/* 16 bits */
	if ((sample_ptrs[sample] >> 18) !=
		((sample_ptrs[sample] + samples[sample].len) >> 18))
	    printf("GUS: Sample address error\n");
    }
    /*
     *    CAUTION!        Interrupts disabled. Don't return before enabling
     */

    flags = splhigh();
    gus_select_voice(voice);
    gus_voice_off();
    gus_rampoff();

    splx(flags);

    if (voices[voice].mode & WAVE_ENVELOPES) {
	compute_volume(voice, volume);
	init_envelope(voice);
    } else {
	compute_and_set_volume(voice, volume, 0);
    }

    flags = splhigh();
    gus_select_voice(voice);

    if (samples[sample].mode & WAVE_LOOP_BACK)
	gus_write_addr(0x0a, sample_ptrs[sample] + samples[sample].len -
		   voices[voice].offset_pending, is16bits);	/* start=end */
    else
	gus_write_addr(0x0a, sample_ptrs[sample] + voices[voice].offset_pending,
		       is16bits);	/* Sample start=begin */

    if (samples[sample].mode & WAVE_LOOPING) {
	mode |= 0x08;

	if (samples[sample].mode & WAVE_BIDIR_LOOP)
	    mode |= 0x10;

	if (samples[sample].mode & WAVE_LOOP_BACK) {
	    gus_write_addr(0x0a,
		    sample_ptrs[sample] + samples[sample].loop_end -
		    voices[voice].offset_pending, is16bits);
	    mode |= 0x40;
	}
	gus_write_addr(0x02, sample_ptrs[sample] + samples[sample].loop_start,
		       is16bits);	/* Loop start location */
	gus_write_addr(0x04, sample_ptrs[sample] + samples[sample].loop_end,
		       is16bits);	/* Loop end location */
    } else {
	mode |= 0x20;	/* Loop IRQ at the end */
	voices[voice].loop_irq_mode = LMODE_FINISH; /* Ramp down at the end */
	voices[voice].loop_irq_parm = 1;
	gus_write_addr(0x02, sample_ptrs[sample],
		       is16bits);	/* Loop start location */
	gus_write_addr(0x04, sample_ptrs[sample] + samples[sample].len - 1,
		       is16bits);	/* Loop end location */
    }
    gus_voice_freq(freq);
    gus_voice_balance(pan);
    gus_voice_on(mode);
    splx(flags);

    return 0;
}

/*
 * New guswave_start_note by Andrew J. Robinson attempts to minimize clicking
 * when the note playing on the voice is changed.  It uses volume ramping.
 */

static int
guswave_start_note(int dev, int voice, int note_num, int volume)
{
    long int        flags;
    int             mode;
    int             ret_val = 0;

    flags = splhigh();
    if (note_num == 255) {
	if (voices[voice].volume_irq_mode == VMODE_START_NOTE) {
	    voices[voice].volume_pending = volume;
	} else {
	    ret_val = guswave_start_note2(dev, voice, note_num, volume);
	}
    } else {
	gus_select_voice(voice);
	mode = gus_read8(0x00);
	if (mode & 0x20)
	    gus_write8(0x00, mode & 0xdf);	/* No interrupt! */

	voices[voice].offset_pending = 0;
	voices[voice].kill_pending = 0;
	voices[voice].volume_irq_mode = 0;
	voices[voice].loop_irq_mode = 0;

	if (voices[voice].sample_pending >= 0) {
	    splx(flags);	/* Run temporarily with interrupts
				 * enabled */
	    guswave_set_instr(voices[voice].dev_pending, voice,
			      voices[voice].sample_pending);
	    voices[voice].sample_pending = -1;
	    flags = splhigh();
	    gus_select_voice(voice);	/* Reselect the voice
					 * (just to be sure) */
	}
	if ((mode & 0x01) || (int) ((gus_read16(0x09) >> 4) < 2065)) {
	    ret_val = guswave_start_note2(dev, voice, note_num, volume);
	} else {
	    voices[voice].dev_pending = dev;
	    voices[voice].note_pending = note_num;
	    voices[voice].volume_pending = volume;
	    voices[voice].volume_irq_mode = VMODE_START_NOTE;

	    gus_rampoff();
	    gus_ramp_range(2000, 4065);
	    gus_ramp_rate(0, 63);	/* Fastest possible rate */
	    gus_rampon(0x20 | 0x40);	/* Ramp down, once, irq */
	}
    }
    splx(flags);
    return ret_val;
}

static void
guswave_reset(int dev)
{
    int             i;

    for (i = 0; i < 32; i++) {
	gus_voice_init(i);
	gus_voice_init2(i);
    }
}

static int
guswave_open(int dev, int mode)
{
    int             err;
    int             otherside = audio_devs[dev]->otherside;

    if (otherside != -1) {
	if (audio_devs[otherside]->busy)
	    return -(EBUSY);
    }
    if (audio_devs[dev]->busy)
	return -(EBUSY);

    gus_initialize();
    voice_alloc->timestamp = 0;

    if ((err = DMAbuf_open_dma(gus_devnum)) < 0) {
	printf("GUS: Loading saples without DMA\n");
	gus_no_dma = 1;	/* Upload samples using PIO */
    } else
	gus_no_dma = 0;

    dram_sleep_flag.aborting = 0;
    dram_sleep_flag.mode = WK_NONE;
    active_device = GUS_DEV_WAVE;

    audio_devs[dev]->busy = 1;
    gus_reset();

    return 0;
}

static void
guswave_close(int dev)
{
    int             otherside = audio_devs[dev]->otherside;

    if (otherside != -1) {
	if (audio_devs[otherside]->busy)
	    return;
    }
    audio_devs[dev]->busy = 0;

    active_device = 0;
    gus_reset();

    if (!gus_no_dma)
	DMAbuf_close_dma(gus_devnum);
}

static int
guswave_load_patch(int dev, int format, snd_rw_buf * addr,
		   int offs, int count, int pmgr_flag)
{
    struct patch_info patch;
    int             instr;
    long            sizeof_patch;

    u_long   blk_size, blk_end, left, src_offs, target;

    sizeof_patch = offsetof(struct patch_info, data); /* Header size */

    if (format != GUS_PATCH) {
	printf("GUS Error: Invalid patch format (key) 0x%x\n", format);
	return -(EINVAL);
    }
    if (count < sizeof_patch) {
	printf("GUS Error: Patch header too short\n");
	return -(EINVAL);
    }
    count -= sizeof_patch;

    if (free_sample >= MAX_SAMPLE) {
	printf("GUS: Sample table full\n");
	return -(ENOSPC);
    }
    /*
     * Copy the header from user space but ignore the first bytes which
     * have been transferred already.
     */

    if (uiomove(&((char *) &patch)[offs], sizeof_patch - offs, addr)) {
	printf("audio: Bad copyin()!\n");
    };

    instr = patch.instr_no;

    if (instr < 0 || instr > MAX_PATCH) {
	printf("GUS: Invalid patch number %d\n", instr);
	return -(EINVAL);
    }
    if (count < patch.len) {
	printf("GUS Warning: Patch record too short (%d<%d)\n",
	       count, (int) patch.len);
	patch.len = count;
    }
    if (patch.len <= 0 || patch.len > gus_mem_size) {
	printf("GUS: Invalid sample length %d\n", (int) patch.len);
	return -(EINVAL);
    }
    if (patch.mode & WAVE_LOOPING) {
	if (patch.loop_start < 0 || patch.loop_start >= patch.len) {
	    printf("GUS: Invalid loop start\n");
	    return -(EINVAL);
	}
	if (patch.loop_end < patch.loop_start || patch.loop_end > patch.len) {
	    printf("GUS: Invalid loop end\n");
	    return -(EINVAL);
	}
    }
    free_mem_ptr = (free_mem_ptr + 31) & ~31;	/* 32 byte alignment */

#define GUS_BANK_SIZE (256*1024)

    if (patch.mode & WAVE_16_BITS) {
	/*
	 * 16 bit samples must fit one 256k bank.
	 */
	if (patch.len >= GUS_BANK_SIZE) {
	    printf("GUS: Sample (16 bit) too long %d\n", (int) patch.len);
	    return -(ENOSPC);
	}
	if ((free_mem_ptr / GUS_BANK_SIZE) !=
		((free_mem_ptr + patch.len) / GUS_BANK_SIZE)) {
	    u_long   tmp_mem =	/* Aligning to 256K */
		    ((free_mem_ptr / GUS_BANK_SIZE) + 1) * GUS_BANK_SIZE;

	    if ((tmp_mem + patch.len) > gus_mem_size)
		return -(ENOSPC);

	    free_mem_ptr = tmp_mem;	/* This leaves unusable memory */
	}
    }
    if ((free_mem_ptr + patch.len) > gus_mem_size)
	return -(ENOSPC);

    sample_ptrs[free_sample] = free_mem_ptr;

    /*
     * Tremolo is not possible with envelopes
     */

    if (patch.mode & WAVE_ENVELOPES)
	patch.mode &= ~WAVE_TREMOLO;

    bcopy(&patch, (char *) &samples[free_sample], sizeof_patch);

    /*
     * Link this_one sample to the list of samples for patch 'instr'.
     */

    samples[free_sample].key = patch_table[instr];
    patch_table[instr] = free_sample;

    /*
     * Use DMA to transfer the wave data to the DRAM
     */

    left = patch.len;
    src_offs = 0;
    target = free_mem_ptr;

    while (left) {		/* Not completely transferred yet */
	/* blk_size = audio_devs[gus_devnum]->buffsize; */
	blk_size = audio_devs[gus_devnum]->dmap_out->bytes_in_use;
	if (blk_size > left)
	    blk_size = left;

	/*
	 * DMA cannot cross 256k bank boundaries. Check for that.
	 */
	blk_end = target + blk_size;

	if ((target >> 18) != (blk_end >> 18)) {	/* Split the block */
	    blk_end &= ~(256 * 1024 - 1);
	    blk_size = blk_end - target;
	}
	if (gus_no_dma) {
	    /*
	     * For some reason the DMA is not possible. We have
	     * to use PIO.
	     */
	    long            i;
	    u_char   data;

	    for (i = 0; i < blk_size; i++) {
		uiomove((char *) &(data), 1, addr);
		if (patch.mode & WAVE_UNSIGNED)
		    if (!(patch.mode & WAVE_16_BITS) || (i & 0x01))
			data ^= 0x80;	/* Convert to signed */
		gus_poke(target + i, data);
	    }
	} else {
	    u_long   address, hold_address;
	    u_char   dma_command;
	    u_long   flags;

	    /*
	     * OK, move now. First in and then out.
	     */

	    if (uiomove(audio_devs[gus_devnum]->dmap_out->raw_buf, blk_size, addr)) {
		printf("audio: Bad copyin()!\n");
	    };

	    flags = splhigh();
	    /******** INTERRUPTS DISABLED NOW ********/
	    gus_write8(0x41, 0);	/* Disable GF1 DMA */
	    DMAbuf_start_dma(gus_devnum,
			 audio_devs[gus_devnum]->dmap_out->raw_buf_phys,
			 blk_size, 1);

	    /*
	     * Set the DRAM address for the wave data
	     */

	    address = target;

	    if (audio_devs[gus_devnum]->dmachan1 > 3) {
		hold_address = address;
		address = address >> 1;
		address &= 0x0001ffffL;
		address |= (hold_address & 0x000c0000L);
	    }
	    gus_write16(0x42, (address >> 4) & 0xffff);	/* DRAM DMA address */

	    /*
	     * Start the DMA transfer
	     */

	    dma_command = 0x21;	/* IRQ enable, DMA start */
	    if (patch.mode & WAVE_UNSIGNED)
		dma_command |= 0x80;	/* Invert MSB */
	    if (patch.mode & WAVE_16_BITS)
		dma_command |= 0x40;	/* 16 bit _DATA_ */
	    if (audio_devs[gus_devnum]->dmachan1 > 3)
		dma_command |= 0x04;	/* 16 bit DMA _channel_ */

	    gus_write8(0x41, dma_command);	/* Lets bo luteet (=bugs) */

	    /*
	     * Sleep here until the DRAM DMA done interrupt is
	     * served
	     */
	    active_device = GUS_DEV_WAVE;


	    {
		int   chn;

		dram_sleep_flag.mode = WK_SLEEP;
		dram_sleeper = &chn;
		DO_SLEEP(chn, dram_sleep_flag, hz);

	    };
	    if ((dram_sleep_flag.mode & WK_TIMEOUT))
		printf("GUS: DMA Transfer timed out\n");
	    splx(flags);
	}

	/*
	 * Now the next part
	 */

	left -= blk_size;
	src_offs += blk_size;
	target += blk_size;

	gus_write8(0x41, 0);	/* Stop DMA */
    }

    free_mem_ptr += patch.len;

    if (!pmgr_flag)
	pmgr_inform(dev, PM_E_PATCH_LOADED, instr, free_sample, 0, 0);
    free_sample++;
    return 0;
}

static void
guswave_hw_control(int dev, u_char *event)
{
	int             voice, cmd;
	u_short  p1, p2;
	u_long   plong, flags;

	cmd = event[2];
	voice = event[3];
	p1 = *(u_short *) &event[4];
	p2 = *(u_short *) &event[6];
	plong = *(u_long *) &event[4];

	if ((voices[voice].volume_irq_mode == VMODE_START_NOTE) &&
	    (cmd != _GUS_VOICESAMPLE) && (cmd != _GUS_VOICE_POS))
		do_volume_irq(voice);

	switch (cmd) {

	case _GUS_NUMVOICES:
		flags = splhigh();
		gus_select_voice(voice);
		gus_select_max_voices(p1);
		splx(flags);
		break;

	case _GUS_VOICESAMPLE:
		guswave_set_instr(dev, voice, p1);
		break;

	case _GUS_VOICEON:
		flags = splhigh();
		gus_select_voice(voice);
		p1 &= ~0x20;	/* Don't allow interrupts */
		gus_voice_on(p1);
		splx(flags);
		break;

	case _GUS_VOICEOFF:
		flags = splhigh();
		gus_select_voice(voice);
		gus_voice_off();
		splx(flags);
		break;

	case _GUS_VOICEFADE:
		gus_voice_fade(voice);
		break;

	case _GUS_VOICEMODE:
		flags = splhigh();
		gus_select_voice(voice);
		p1 &= ~0x20;	/* Don't allow interrupts */
		gus_voice_mode(p1);
		splx(flags);
		break;

	case _GUS_VOICEBALA:
		flags = splhigh();
		gus_select_voice(voice);
		gus_voice_balance(p1);
		splx(flags);
		break;

	case _GUS_VOICEFREQ:
		flags = splhigh();
		gus_select_voice(voice);
		gus_voice_freq(plong);
		splx(flags);
		break;

	case _GUS_VOICEVOL:
		flags = splhigh();
		gus_select_voice(voice);
		gus_voice_volume(p1);
		splx(flags);
		break;

	case _GUS_VOICEVOL2:	/* Just update the software voice level */
		voices[voice].initial_volume =
			voices[voice].current_volume = p1;
		break;

	case _GUS_RAMPRANGE:
		if (voices[voice].mode & WAVE_ENVELOPES)
			break;	/* NO-NO */
		flags = splhigh();
		gus_select_voice(voice);
		gus_ramp_range(p1, p2);
		splx(flags);
		break;

	case _GUS_RAMPRATE:
		if (voices[voice].mode & WAVE_ENVELOPES)
			break;	/* NJET-NJET */
		flags = splhigh();
		gus_select_voice(voice);
		gus_ramp_rate(p1, p2);
		splx(flags);
		break;

	case _GUS_RAMPMODE:
		if (voices[voice].mode & WAVE_ENVELOPES)
			break;	/* NO-NO */
		flags = splhigh();
		gus_select_voice(voice);
		p1 &= ~0x20;	/* Don't allow interrupts */
		gus_ramp_mode(p1);
		splx(flags);
		break;

	case _GUS_RAMPON:
		if (voices[voice].mode & WAVE_ENVELOPES)
			break;	/* EI-EI */
		flags = splhigh();
		gus_select_voice(voice);
		p1 &= ~0x20;	/* Don't allow interrupts */
		gus_rampon(p1);
		splx(flags);
		break;

	case _GUS_RAMPOFF:
		if (voices[voice].mode & WAVE_ENVELOPES)
			break;	/* NEJ-NEJ */
		flags = splhigh();
		gus_select_voice(voice);
		gus_rampoff();
		splx(flags);
		break;

	case _GUS_VOLUME_SCALE:
		volume_base = p1;
		volume_scale = p2;
		break;

	case _GUS_VOICE_POS:
		flags = splhigh();
		gus_select_voice(voice);
		gus_set_voice_pos(voice, plong);
		splx(flags);
		break;

	default:;
	}
}

static int
gus_sampling_set_speed(int speed)
{

	if (speed <= 0)
		speed = gus_sampling_speed;

	RANGE(speed, 4000, 44100);
	gus_sampling_speed = speed;

	if (only_read_access) {
		/* Compute nearest valid recording speed  and return it */

		speed = (9878400 / (gus_sampling_speed + 2)) / 16;
		speed = (9878400 / (speed * 16)) - 2;
	}
	return speed;
}

static int
gus_sampling_set_channels(int channels)
{
	if (!channels)
		return gus_sampling_channels;
	RANGE(channels, 1, 2);
	gus_sampling_channels = channels;
	return channels;
}

static int
gus_sampling_set_bits(int bits)
{
	if (!bits)
		return gus_sampling_bits;

	if (bits != 8 && bits != 16)
		bits = 8;

	if (only_8_bits)
		bits = 8;

	gus_sampling_bits = bits;
	return bits;
}

static int
gus_sampling_ioctl(int dev, u_int cmd, ioctl_arg arg, int local)
{
	switch (cmd) {
		case SOUND_PCM_WRITE_RATE:
		if (local)
			return gus_sampling_set_speed((int) arg);
		return *(int *) arg = gus_sampling_set_speed((*(int *) arg));
		break;

	case SOUND_PCM_READ_RATE:
		if (local)
			return gus_sampling_speed;
		return *(int *) arg = gus_sampling_speed;
		break;

	case SNDCTL_DSP_STEREO:
		if (local)
			return gus_sampling_set_channels((int) arg + 1) - 1;
		return *(int *) arg = gus_sampling_set_channels((*(int *) arg) + 1) - 1;
		break;

	case SOUND_PCM_WRITE_CHANNELS:
		if (local)
			return gus_sampling_set_channels((int) arg);
		return *(int *) arg = gus_sampling_set_channels((*(int *) arg));
		break;

	case SOUND_PCM_READ_CHANNELS:
		if (local)
			return gus_sampling_channels;
		return *(int *) arg = gus_sampling_channels;
		break;

	case SNDCTL_DSP_SETFMT:
		if (local)
			return gus_sampling_set_bits((int) arg);
		return *(int *) arg = gus_sampling_set_bits((*(int *) arg));
		break;

	case SOUND_PCM_READ_BITS:
		if (local)
			return gus_sampling_bits;
		return *(int *) arg = gus_sampling_bits;

	case SOUND_PCM_WRITE_FILTER:	/* NOT POSSIBLE */
		return *(int *) arg = -(EINVAL);
		break;

	case SOUND_PCM_READ_FILTER:
		return *(int *) arg = -(EINVAL);
		break;

	}
	return -(EINVAL);
}

static void
gus_sampling_reset(int dev)
{
	if (recording_active) {
		gus_write8(0x49, 0x00);	/* Halt recording */
		set_input_volumes();
	}
}

static int
gus_sampling_open(int dev, int mode)
{

    int             otherside = audio_devs[dev]->otherside;
    if (otherside != -1) {
	if (audio_devs[otherside]->busy)
	    return -(EBUSY);
    }
    if (audio_devs[dev]->busy)
	return -(EBUSY);


    gus_initialize();

    active_device = 0;

    gus_reset();
    reset_sample_memory();
    gus_select_max_voices(14);

    pcm_active = 0;
    dma_active = 0;
    pcm_opened = 1;
    audio_devs[dev]->busy = 1;

    if (mode & OPEN_READ) {
	recording_active = 1;
	set_input_volumes();
    }
    only_read_access = !(mode & OPEN_WRITE);
    only_8_bits = mode & OPEN_READ;
    if (only_8_bits)
	audio_devs[dev]->format_mask = AFMT_U8;
    else
	audio_devs[dev]->format_mask = AFMT_U8 | AFMT_S16_LE;

    return 0;
}

static void
gus_sampling_close(int dev)
{
	int             otherside = audio_devs[dev]->otherside;
	audio_devs[dev]->busy = 0;

	if (otherside != -1) {
		if (audio_devs[otherside]->busy)
			return;
	}
	gus_reset();

	pcm_opened = 0;
	active_device = 0;

	if (recording_active) {
		gus_write8(0x49, 0x00);	/* Halt recording */
		set_input_volumes();
	}
	recording_active = 0;
}

static void
gus_sampling_update_volume(void)
{
    u_long   flags;
    int             voice;

    if (pcm_active && pcm_opened)
	for (voice = 0; voice < gus_sampling_channels; voice++) {
	    flags = splhigh();
	    gus_select_voice(voice);
	    gus_rampoff();
	    gus_voice_volume(1530 + (25 * gus_pcm_volume));
	    gus_ramp_range(65, 1530 + (25 * gus_pcm_volume));
	    splx(flags);
	}
}

static void
play_next_pcm_block(void)
{
    u_long   flags;
    int             speed = gus_sampling_speed;
    int             this_one, is16bits, chn;
    u_long   dram_loc;
    u_char   mode[2], ramp_mode[2];

    if (!pcm_qlen)
	return;

    this_one = pcm_head;

    for (chn = 0; chn < gus_sampling_channels; chn++) {
	mode[chn] = 0x00;
	ramp_mode[chn] = 0x03;	/* Ramping and rollover off */

	if (chn == 0) {
	    mode[chn] |= 0x20;	/* Loop IRQ */
	    voices[chn].loop_irq_mode = LMODE_PCM;
	}
	if (gus_sampling_bits != 8) {
	    is16bits = 1;
	    mode[chn] |= 0x04;	/* 16 bit data */
	} else
	    is16bits = 0;

	dram_loc = this_one * pcm_bsize;
	dram_loc += chn * pcm_banksize;

	if (this_one == (pcm_nblk - 1)) {	/* Last fragment of the
						 * DRAM buffer */
	    mode[chn] |= 0x08;	/* Enable loop */
	    ramp_mode[chn] = 0x03;	/* Disable rollover bit */
	} else {
	    if (chn == 0)
		ramp_mode[chn] = 0x04;	/* Enable rollover bit */
	}

	flags = splhigh();
	gus_select_voice(chn);
	gus_voice_freq(speed);

	if (gus_sampling_channels == 1)
	    gus_voice_balance(7);	/* mono */
	else if (chn == 0)
	    gus_voice_balance(0);	/* left */
	else
	    gus_voice_balance(15);	/* right */

	if (!pcm_active) {	/* Playback not already active */
	    /*
	     * The playback was not started yet (or there has
	     * been a pause). Start the voice (again) and ask for
	     * a rollover irq at the end of this_one block. If
	     * this_one one is last of the buffers, use just the
	     * normal loop with irq.
	     */

	    gus_voice_off();
	    gus_rampoff();
	    gus_voice_volume(1530 + (25 * gus_pcm_volume));
	    gus_ramp_range(65, 1530 + (25 * gus_pcm_volume));

	    gus_write_addr(0x0a, dram_loc, is16bits);	/* Starting position */
	    gus_write_addr(0x02, chn * pcm_banksize, is16bits);	/* Loop start */

	    if (chn != 0)
		gus_write_addr(0x04, pcm_banksize + (pcm_bsize * pcm_nblk) - 1,
			   is16bits);	/* Loop end location */
	}
	if (chn == 0)
	    gus_write_addr(0x04, dram_loc + pcm_datasize[this_one] - 1,
			       is16bits);	/* Loop end location */
	else
	    mode[chn] |= 0x08;	/* Enable looping */

	if (pcm_datasize[this_one] != pcm_bsize) {
	    /*
	     * Incompletely filled block. Possibly the last one.
	     */
	    if (chn == 0) {
		mode[chn] &= ~0x08;	/* Disable looping */
		mode[chn] |= 0x20;	/* Enable IRQ at the end */
		voices[0].loop_irq_mode = LMODE_PCM_STOP;
		ramp_mode[chn] = 0x03;	/* No rollover bit */
	    } else {
		gus_write_addr(0x04, dram_loc + pcm_datasize[this_one],
			       is16bits);	/* Loop end location */
		mode[chn] &= ~0x08;	/* Disable looping */
	    }
	}
	splx(flags);
    }

    for (chn = 0; chn < gus_sampling_channels; chn++) {
	flags = splhigh();
	gus_select_voice(chn);
	gus_write8(0x0d, ramp_mode[chn]);
	gus_voice_on(mode[chn]);
	splx(flags);
    }

    pcm_active = 1;
}

static void
gus_transfer_output_block(int dev, u_long buf,
			  int total_count, int intrflag, int chn)
{
	/*
	 * This routine transfers one block of audio data to the DRAM. In
	 * mono mode it's called just once. When in stereo mode, this_one
	 * routine is called once for both channels.
	 * 
	 * The left/mono channel data is transferred to the beginning of dram
	 * and the right data to the area pointed by gus_page_size.
	 */

	int             this_one, count;
	u_long   flags;
	u_char   dma_command;
	u_long   address, hold_address;

	flags = splhigh();

	count = total_count / gus_sampling_channels;

	if (chn == 0) {
		if (pcm_qlen >= pcm_nblk)
			printf("GUS Warning: PCM buffers out of sync\n");

		this_one = pcm_current_block = pcm_tail;
		pcm_qlen++;
		pcm_tail = (pcm_tail + 1) % pcm_nblk;
		pcm_datasize[this_one] = count;
	} else
		this_one = pcm_current_block;

	gus_write8(0x41, 0);	/* Disable GF1 DMA */
	DMAbuf_start_dma(dev, buf + (chn * count), count, 1);

	address = this_one * pcm_bsize;
	address += chn * pcm_banksize;

	if (audio_devs[dev]->dmachan1 > 3) {
		hold_address = address;
		address = address >> 1;
		address &= 0x0001ffffL;
		address |= (hold_address & 0x000c0000L);
	}
	gus_write16(0x42, (address >> 4) & 0xffff);	/* DRAM DMA address */

	dma_command = 0x21;	/* IRQ enable, DMA start */

	if (gus_sampling_bits != 8)
		dma_command |= 0x40;	/* 16 bit _DATA_ */
	else
		dma_command |= 0x80;	/* Invert MSB */

	if (audio_devs[dev]->dmachan1 > 3)
		dma_command |= 0x04;	/* 16 bit DMA channel */

	gus_write8(0x41, dma_command);	/* Kickstart */

	if (chn == (gus_sampling_channels - 1)) {	/* Last channel */
		/*
		 * Last (right or mono) channel data
		 */
		dma_active = 1;	/* DMA started. There is a unacknowledged
				 * buffer */
		active_device = GUS_DEV_PCM_DONE;
		if (!pcm_active && (pcm_qlen > 0 || count < pcm_bsize)) {
			play_next_pcm_block();
		}
	} else {
		/*
		 * Left channel data. The right channel is transferred after
		 * DMA interrupt
		 */
		active_device = GUS_DEV_PCM_CONTINUE;
	}

	splx(flags);
}

static void
gus_sampling_output_block(int dev, u_long buf, int total_count,
			  int intrflag, int restart_dma)
{
	pcm_current_buf = buf;
	pcm_current_count = total_count;
	pcm_current_intrflag = intrflag;
	pcm_current_dev = dev;
	gus_transfer_output_block(dev, buf, total_count, intrflag, 0);
}

static void
gus_sampling_start_input(int dev, u_long buf, int count,
			 int intrflag, int restart_dma)
{
	u_long   flags;
	u_char   mode;

	flags = splhigh();

	DMAbuf_start_dma(dev, buf, count, 0);

	mode = 0xa0;		/* DMA IRQ enabled, invert MSB */

	if (audio_devs[dev]->dmachan2 > 3)
		mode |= 0x04;	/* 16 bit DMA channel */
	if (gus_sampling_channels > 1)
		mode |= 0x02;	/* Stereo */
	mode |= 0x01;		/* DMA enable */

	gus_write8(0x49, mode);

	splx(flags);
}

static int
gus_sampling_prepare_for_input(int dev, int bsize, int bcount)
{
    u_int    rate;

    rate = (9878400 / (gus_sampling_speed + 2)) / 16;

    gus_write8(0x48, rate & 0xff);	/* Set sampling rate */

    if (gus_sampling_bits != 8) {
	printf("GUS Error: 16 bit recording not supported\n");
	return -(EINVAL);
    }
    return 0;
}

static int
gus_sampling_prepare_for_output(int dev, int bsize, int bcount)
{
	int             i;

	long            mem_ptr, mem_size;

	mem_ptr = 0;
	mem_size = gus_mem_size / gus_sampling_channels;

	if (mem_size > (256 * 1024))
		mem_size = 256 * 1024;

	pcm_bsize = bsize / gus_sampling_channels;
	pcm_head = pcm_tail = pcm_qlen = 0;

	pcm_nblk = MAX_PCM_BUFFERS;
	if ((pcm_bsize * pcm_nblk) > mem_size)
		pcm_nblk = mem_size / pcm_bsize;

	for (i = 0; i < pcm_nblk; i++)
		pcm_datasize[i] = 0;

	pcm_banksize = pcm_nblk * pcm_bsize;

	if (gus_sampling_bits != 8 && pcm_banksize == (256 * 1024))
		pcm_nblk--;

	return 0;
}

static int
gus_local_qlen(int dev)
{
	return pcm_qlen;
}

static void
gus_copy_from_user(int dev, char *localbuf, int localoffs,
		   snd_rw_buf * userbuf, int useroffs, int len)
{
	if (gus_sampling_channels == 1) {

		if (uiomove(&localbuf[localoffs], len, userbuf)) {
			printf("audio: Bad copyin()!\n");
		};
	} else if (gus_sampling_bits == 8) {
		int             in_left = useroffs;
		int             in_right = useroffs + 1;
		char           *out_left, *out_right;
		int             i;

		len /= 2;
		localoffs /= 2;
		out_left = &localbuf[localoffs];
		out_right = out_left + pcm_bsize;

		for (i = 0; i < len; i++) {
			uiomove((char *) &(*out_left++), 1, userbuf);
			in_left += 2;
			uiomove((char *) &(*out_right++), 1, userbuf);
			in_right += 2;
		}
	} else {
		int             in_left = useroffs;
		int             in_right = useroffs + 2;
		short          *out_left, *out_right;
		int             i;

		len /= 4;
		localoffs /= 2;

		out_left = (short *) &localbuf[localoffs];
		out_right = out_left + (pcm_bsize / 2);

		for (i = 0; i < len; i++) {
			uiomove((char *) &(*out_left++), 2, userbuf);
			in_left += 2;
			uiomove((char *) &(*out_right++), 2, userbuf);
			in_right += 2;
		}
	}
}

static struct audio_operations gus_sampling_operations =
{
	"Gravis UltraSound",
	NEEDS_RESTART,
	AFMT_U8 | AFMT_S16_LE,
	NULL,
	gus_sampling_open,
	gus_sampling_close,
	gus_sampling_output_block,
	gus_sampling_start_input,
	gus_sampling_ioctl,
	gus_sampling_prepare_for_input,
	gus_sampling_prepare_for_output,
	gus_sampling_reset,
	gus_sampling_reset,
	gus_local_qlen,
	gus_copy_from_user
};

static void
guswave_setup_voice(int dev, int voice, int chn)
{
	struct channel_info *info =
	&synth_devs[dev]->chn_info[chn];

	guswave_set_instr(dev, voice, info->pgm_num);

	voices[voice].expression_vol =
		info->controllers[CTL_EXPRESSION];	/* Just msb */
	voices[voice].main_vol =
		(info->controllers[CTL_MAIN_VOLUME] * 100) / 128;
	voices[voice].panning =
		(info->controllers[CTL_PAN] * 2) - 128;
	voices[voice].bender = info->bender_value;
}

static void
guswave_bender(int dev, int voice, int value)
{
	int             freq;
	u_long   flags;

	voices[voice].bender = value - 8192;
	freq = compute_finetune(voices[voice].orig_freq, value - 8192,
				voices[voice].bender_range);
	voices[voice].current_freq = freq;

	flags = splhigh();
	gus_select_voice(voice);
	gus_voice_freq(freq);
	splx(flags);
}

static int
guswave_patchmgr(int dev, struct patmgr_info * rec)
{
	int             i, n;

	switch (rec->command) {
	case PM_GET_DEVTYPE:
		rec->parm1 = PMTYPE_WAVE;
		return 0;
		break;

	case PM_GET_NRPGM:
		rec->parm1 = MAX_PATCH;
		return 0;
		break;

	case PM_GET_PGMMAP:
		rec->parm1 = MAX_PATCH;

		for (i = 0; i < MAX_PATCH; i++) {
			int             ptr = patch_table[i];

			rec->data.data8[i] = 0;

			while (ptr >= 0 && ptr < free_sample) {
				rec->data.data8[i]++;
				ptr = samples[ptr].key;	/* Follow link */
			}
		}
		return 0;
		break;

	case PM_GET_PGM_PATCHES:
		{
			int             ptr = patch_table[rec->parm1];

			n = 0;

			while (ptr >= 0 && ptr < free_sample) {
				rec->data.data32[n++] = ptr;
				ptr = samples[ptr].key;	/* Follow link */
			}
		}
		rec->parm1 = n;
		return 0;
		break;

	case PM_GET_PATCH:
		{
			int             ptr = rec->parm1;
			struct patch_info *pat;

			if (ptr < 0 || ptr >= free_sample)
				return -(EINVAL);

			bcopy((char *) &samples[ptr], rec->data.data8, sizeof(struct patch_info));

			pat = (struct patch_info *) rec->data.data8;

			pat->key = GUS_PATCH;	/* Restore patch type */
			rec->parm1 = sample_ptrs[ptr];	/* DRAM location */
			rec->parm2 = sizeof(struct patch_info);
		}
		return 0;
		break;

	case PM_SET_PATCH:
		{
			int             ptr = rec->parm1;
			struct patch_info *pat;

			if (ptr < 0 || ptr >= free_sample)
				return -(EINVAL);

			pat = (struct patch_info *) rec->data.data8;

			if (pat->len > samples[ptr].len)	/* Cannot expand sample */
				return -(EINVAL);

			pat->key = samples[ptr].key;	/* Ensure the link is
							 * correct */

			bcopy(rec->data.data8, (char *) &samples[ptr], sizeof(struct patch_info));

			pat->key = GUS_PATCH;
		}
		return 0;
		break;

	case PM_READ_PATCH:	/* Returns a block of wave data from the DRAM */
		{
			int             sample = rec->parm1;
			int             n;
			long            offs = rec->parm2;
			int             l = rec->parm3;

			if (sample < 0 || sample >= free_sample)
				return -(EINVAL);

			if (offs < 0 || offs >= samples[sample].len)
				return -(EINVAL);	/* Invalid offset */

			n = samples[sample].len - offs;	/* Num of bytes left */

			if (l > n)
				l = n;

			if (l > sizeof(rec->data.data8))
				l = sizeof(rec->data.data8);

			if (l <= 0)
				return -(EINVAL);	/* Was there a bug? */

			offs += sample_ptrs[sample];	/* Begin offsess +
							 * offset to DRAM */

			for (n = 0; n < l; n++)
				rec->data.data8[n] = gus_peek(offs++);
			rec->parm1 = n;	/* Nr of bytes copied */
		}
		return 0;
		break;

	case PM_WRITE_PATCH:	/* Writes a block of wave data to the DRAM */
		{
			int             sample = rec->parm1;
			int             n;
			long            offs = rec->parm2;
			int             l = rec->parm3;

			if (sample < 0 || sample >= free_sample)
				return -(EINVAL);

			if (offs < 0 || offs >= samples[sample].len)
				return -(EINVAL);	/* Invalid offset */

			n = samples[sample].len - offs;	/* Nr of bytes left */

			if (l > n)
				l = n;

			if (l > sizeof(rec->data.data8))
				l = sizeof(rec->data.data8);

			if (l <= 0)
				return -(EINVAL);	/* Was there a bug? */

			offs += sample_ptrs[sample];	/* Begin offsess +
							 * offset to DRAM */

			for (n = 0; n < l; n++)
				gus_poke(offs++, rec->data.data8[n]);
			rec->parm1 = n;	/* Nr of bytes copied */
		}
		return 0;
		break;

	default:
		return -(EINVAL);
	}
}

static int
guswave_alloc(int dev, int chn, int note, struct voice_alloc_info * alloc)
{
	int             i, p, best = -1, best_time = 0x7fffffff;

	p = alloc->ptr;
	/*
	 * First look for a completely stopped voice
	 */

	for (i = 0; i < alloc->max_voice; i++) {
		if (alloc->map[p] == 0) {
			alloc->ptr = p;
			return p;
		}
		if (alloc->alloc_times[p] < best_time) {
			best = p;
			best_time = alloc->alloc_times[p];
		}
		p = (p + 1) % alloc->max_voice;
	}

	/*
	 * Then look for a releasing voice
	 */

	for (i = 0; i < alloc->max_voice; i++) {
		if (alloc->map[p] == 0xffff) {
			alloc->ptr = p;
			return p;
		}
		p = (p + 1) % alloc->max_voice;
	}

	if (best >= 0)
		p = best;

	alloc->ptr = p;
	return p;
}

static struct synth_operations guswave_operations =
{
	&gus_info,
	0,
	SYNTH_TYPE_SAMPLE,
	SAMPLE_TYPE_GUS,
	guswave_open,
	guswave_close,
	guswave_ioctl,
	guswave_kill_note,
	guswave_start_note,
	guswave_set_instr,
	guswave_reset,
	guswave_hw_control,
	guswave_load_patch,
	guswave_aftertouch,
	guswave_controller,
	guswave_panning,
	guswave_volume_method,
	guswave_patchmgr,
	guswave_bender,
	guswave_alloc,
	guswave_setup_voice
};

static void
set_input_volumes(void)
{
	u_long   flags;
	u_char   mask = 0xff & ~0x06;	/* Just line out enabled */

	if (have_gus_max)	/* Don't disturb GUS MAX */
		return;

	flags = splhigh();

	/*
	 * Enable channels having vol > 10% Note! bit 0x01 means the line in
	 * DISABLED while 0x04 means the mic in ENABLED.
	 */
	if (gus_line_vol > 10)
		mask &= ~0x01;
	if (gus_mic_vol > 10)
		mask |= 0x04;

	if (recording_active) {
		/*
		 * Disable channel, if not selected for recording
		 */
		if (!(gus_recmask & SOUND_MASK_LINE))
			mask |= 0x01;
		if (!(gus_recmask & SOUND_MASK_MIC))
			mask &= ~0x04;
	}
	mix_image &= ~0x07;
	mix_image |= mask & 0x07;
	outb(u_Mixer, mix_image);

	splx(flags);
}

int
gus_default_mixer_ioctl(int dev, u_int cmd, ioctl_arg arg)
{

#define MIX_DEVS	(SOUND_MASK_MIC|SOUND_MASK_LINE| \
			 SOUND_MASK_SYNTH|SOUND_MASK_PCM)

    if (((cmd >> 8) & 0xff) == 'M') {
	if (cmd & IOC_IN)
	    switch (cmd & 0xff) {
	    case SOUND_MIXER_RECSRC:
		gus_recmask = (*(int *) arg) & MIX_DEVS;
		if (!(gus_recmask & (SOUND_MASK_MIC | SOUND_MASK_LINE)))
		    gus_recmask = SOUND_MASK_MIC;
		/*
		 * Note! Input volumes are updated during
		 * next open for recording
		 */
		return *(int *) arg = gus_recmask;
		break;

	    case SOUND_MIXER_MIC:
		{
		    int             vol = (*(int *) arg) & 0xff;

		    if (vol < 0)
			vol = 0;
		    if (vol > 100)
			vol = 100;
		    gus_mic_vol = vol;
		    set_input_volumes();
		    return *(int *) arg = vol | (vol << 8);
		}
		break;

	    case SOUND_MIXER_LINE:
		{
		    int             vol = (*(int *) arg) & 0xff;

		    if (vol < 0)
			vol = 0;
		    if (vol > 100)
			vol = 100;
		    gus_line_vol = vol;
		    set_input_volumes();
		    return *(int *) arg = vol | (vol << 8);
		}
		break;

	    case SOUND_MIXER_PCM:
		    gus_pcm_volume = (*(int *) arg) & 0xff;
		    RANGE (gus_pcm_volume, 0, 100);
		    gus_sampling_update_volume();
		    return *(int *) arg = gus_pcm_volume | (gus_pcm_volume << 8);
		    break;

	    case SOUND_MIXER_SYNTH:
		    {
			    int             voice;

			    gus_wave_volume = (*(int *) arg) & 0xff;

			    RANGE (gus_wave_volume , 0, 100);

			    if (active_device == GUS_DEV_WAVE)
				    for (voice = 0; voice < nr_voices; voice++)
					    dynamic_volume_change(voice);	/* Apply the new vol */

			    return *(int *) arg = gus_wave_volume | (gus_wave_volume << 8);
		    }
		    break;

	    default:
		    return -(EINVAL);
	    }
    else
	    switch (cmd & 0xff) {	/* Return parameters */

	    case SOUND_MIXER_RECSRC:
		    return *(int *) arg = gus_recmask;
		    break;

	    case SOUND_MIXER_DEVMASK:
		    return *(int *) arg = MIX_DEVS;
		    break;

	    case SOUND_MIXER_STEREODEVS:
		    return *(int *) arg = 0;
		    break;

	    case SOUND_MIXER_RECMASK:
		    return *(int *) arg = SOUND_MASK_MIC | SOUND_MASK_LINE;
		    break;

	    case SOUND_MIXER_CAPS:
		    return *(int *) arg = 0;
		    break;

	    case SOUND_MIXER_MIC:
		    return *(int *) arg = gus_mic_vol | (gus_mic_vol << 8);
		    break;

	    case SOUND_MIXER_LINE:
		    return *(int *) arg = gus_line_vol | (gus_line_vol << 8);
		    break;

	    case SOUND_MIXER_PCM:
		    return *(int *) arg = gus_pcm_volume | (gus_pcm_volume << 8);
		    break;

	    case SOUND_MIXER_SYNTH:
		    return *(int *) arg = gus_wave_volume | (gus_wave_volume << 8);
		    break;

	    default:
		    return -(EINVAL);
	    }
} else
    return -(EINVAL);
}

static struct mixer_operations gus_mixer_operations = {"Gravis Ultrasound", gus_default_mixer_ioctl};

static void
gus_default_mixer_init()
{
if (num_mixers < MAX_MIXER_DEV)	/* Don't install if there is another
			     * mixer */
    mixer_devs[num_mixers++] = &gus_mixer_operations;

if (have_gus_max) {
    /*
     * Enable all mixer channels on the GF1 side. Otherwise
     * recording will not be possible using GUS MAX.
     */
    mix_image &= ~0x07;
    mix_image |= 0x04;	/* All channels enabled */
    outb(u_Mixer, mix_image);
}
}

/* start of pnp code */

static void 
SEND(int d, int r)
{
outb(PADDRESS, d);
outb(PWRITE_DATA, r);
}




/*
 * Get the device's serial number.  Returns 1 if the serial is valid.
 */
static int
get_serial(int rd_port, u_char *data)
{
	int             i, bit, valid = 0, sum = 0x6a;

	bzero(data, sizeof(char) * 9);

	for (i = 0; i < 72; i++) {
		bit = inb((rd_port << 2) | 0x3) == 0x55;
		DELAY(250);	/* Delay 250 usec */

		/* Can't Short Circuit the next evaluation, so 'and' is last */
		bit = (inb((rd_port << 2) | 0x3) == 0xaa) && bit;
		DELAY(250);	/* Delay 250 usec */

		valid = valid || bit;

		if (i < 64)
			sum = (sum >> 1) |
				(((sum ^ (sum >> 1) ^ bit) << 7) & 0xff);

		data[i / 8] = (data[i / 8] >> 1) | (bit ? 0x80 : 0);
	}
	valid = valid && (data[8] == sum);

	return valid;
}

static void
send_Initiation_LFSR()
{
	int             cur, i;

	/* Reset the LSFR */
	outb(PADDRESS, 0);
	outb(PADDRESS, 0);

	cur = 0x6a;
	outb(PADDRESS, cur);

	for (i = 1; i < 32; i++) {
		cur = (cur >> 1) | (((cur ^ (cur >> 1)) << 7) & 0xff);
		outb(PADDRESS, cur);
	}
}



static int 
isolation_protocol(int rd_port)
{
	int             csn;
	u_char   data[9];

	send_Initiation_LFSR();

	/* Reset CSN for All Cards */
	SEND(0x02, 0x04);

	for (csn = 1; (csn < MAX_CARDS); csn++) {
		/* Wake up cards without a CSN */

		SEND(WAKE, 0);
		SEND(SET_RD_DATA, rd_port);
		outb(PADDRESS, SERIAL_ISOLATION);
		DELAY(1000);	/* Delay 1 msec */
		if (get_serial(rd_port, data)) {
			printf("Board Vendor ID: %c%c%c%02x%02x",
			       ((data[0] & 0x7c) >> 2) + 64,
			       (((data[0] & 0x03) << 3) | ((data[1] & 0xe0) >> 5)) + 64,
			       (data[1] & 0x1f) + 64, data[2], data[3]);
			printf("     Board Serial Number: %08x\n", *(int *) &(data[4]));

			SEND(SET_CSN, csn);	/* Move this out of this
						 * function XXX */
			outb(PADDRESS, PSTATUS);


			return rd_port;
		} else
			break;
	}

	return 0;
}



/*
 * ########################################################################
 * 
 * FUNCTION : IwaveInputSource
 * 
 * PROFILE: This function allows the calling program to select among any of
 * several possible sources to the ADC's. The possible input sources and
 * their corresponding symbolic constants are: - Line        (LINE_IN) - Aux1
 * (AUX1_IN) - Microphone  (MIC_IN) - Mixer       (MIX_IN)
 * 
 * Set the first argument to either LEFT_SOURCE or RIGHT_SOURCE. Always use the
 * symbolic contants for the arguments.
 * 
 * ########################################################################
 */
static void 
IwaveInputSource(BYTE index, BYTE source)
{
	BYTE            reg;

	ENTER_CRITICAL;
	reg = inb(iw.pcodar) & 0xE0;
	outb(iw.pcodar, reg | index);	/* select register CLICI or CRICI */
	reg = inb(iw.cdatap) & ~MIX_IN;
	source &= MIX_IN;
	outb(iw.cdatap, (BYTE) (reg | source));
	LEAVE_CRITICAL;
}
static void 
IwavePnpGetCfg(void)
{
	WORD            val;


	ENTER_CRITICAL;
	IwavePnpDevice(AUDIO);
	outb(_PIDXR, 0x60);	/* select P2X0HI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get P2XR[9:8] */
	outb(_PIDXR, 0x61);	/* select P2XRLI */
	iw.p2xr = val + (WORD) inb(iw.pnprdp);	/* get P2XR[7:4] */

	outb(_PIDXR, 0x62);	/* select P3X0HI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get P3XR[9:8] */
	outb(_PIDXR, 0x63);	/* select P3X0LI */
	iw.p3xr = val + (WORD) inb(iw.pnprdp);	/* get P3XR[7:3] */

	outb(_PIDXR, 0x64);	/* select PHCAI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get PCODAR[9:8] */
	outb(_PIDXR, 0x65);	/* select PLCAI */
	iw.pcodar = val + (WORD) inb(iw.pnprdp);	/* get PCODAR[7:2] */

	outb(_PIDXR, 0x70);	/* select PUI1SI */
	iw.synth_irq = (WORD) (inb(iw.pnprdp) & 0x0F);	/* Synth IRQ number */

	outb(_PIDXR, 0x72);	/* select PUI2SI */
	iw.midi_irq = (WORD) (inb(iw.pnprdp) & 0x0F);	/* MIDI IRQ number */

	outb(_PIDXR, 0x74);	/* select PUD1SI */
	iw.dma1_chan = inb(iw.pnprdp) & 0x07;	/* DMA1 chan (LMC/Codec Rec) */

	outb(_PIDXR, 0x75);	/* select PUD2SI */
	iw.dma2_chan = inb(iw.pnprdp) & 0x07;	/* DMA2 chan (codec play) */


	IwavePnpDevice(EXT);	/* select external device */
	outb(_PIDXR, 0x60);	/* select PRAHI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get PCDRAR[9:8] */
	outb(_PIDXR, 0x61);	/* select PRALI */
	iw.pcdrar = val + (WORD) inb(iw.pnprdp);	/* get PCDRAR[7:4] */
	outb(_PIDXR, 0x62);	/* select PATAHI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get PATAAR[9:8] */
	outb(_PIDXR, 0x63);	/* select PATALI */
	iw.pataar = val + (WORD) inb(iw.pnprdp);	/* get PATAAR[7:1] */

	outb(_PIDXR, 0x70);	/* select PRISI */
	iw.ext_irq = (WORD) (inb(iw.pnprdp) & 0x0F);	/* Ext Dev IRQ number */

	outb(_PIDXR, 0x74);	/* select PRDSI */
	iw.ext_chan = inb(iw.pnprdp) & 0x07;	/* Ext Dev DMA channel */

	IwavePnpDevice(MPU401);	/* Select MPU401 Device */
	outb(_PIDXR, 0x60);	/* select P401HI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get P401AR[9:8] */
	outb(_PIDXR, 0x61);	/* select P401LI */
	iw.p401ar = val + (WORD) inb(iw.pnprdp);	/* get P401AR[7:1] */

	outb(_PIDXR, 0x70);	/* select PMISI */
	iw.mpu_irq = (WORD) (inb(iw.pnprdp) & 0x0F);	/* MPU401 Dev IRQ number */

	IwavePnpDevice(GAME);	/* Select GAME logical Device */
	outb(_PIDXR, 0x60);	/* select P201HI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get P201AR[9:8] */
	outb(_PIDXR, 0x61);	/* select P201LI */
	iw.p201ar = val + (WORD) inb(iw.pnprdp);	/* get P201AR[7:6] */

	IwavePnpDevice(EMULATION);	/* Select SB and ADLIB Device */
	outb(_PIDXR, 0x60);	/* select P388HI */
	val = ((WORD) inb(iw.pnprdp)) << 8;	/* get P388AR[9:8] */
	outb(_PIDXR, 0x61);	/* select P388LI */
	iw.p388ar = val + inb(iw.pnprdp);	/* get P388AR[7:6] */
	outb(_PIDXR, 0x70);	/* select PSBISI */
	iw.emul_irq = (WORD) (inb(iw.pnprdp) & 0x0F);	/* emulation Dev IRQ
							 * number */
	LEAVE_CRITICAL;
}

static void 
IwavePnpSetCfg(void)
{
	ENTER_CRITICAL;
	IwavePnpDevice(AUDIO);	/* select audio device */
	outb(_PIDXR, 0x60);	/* select P2X0HI */
	outb(_PNPWRP, (BYTE) (iw.p2xr >> 8));	/* set P2XR[9:8] */
	outb(_PIDXR, 0x61);	/* select P2X0LI */
	outb(_PNPWRP, (BYTE) iw.p2xr);	/* set P2XR[7:4] */
	/* P2XR[3:0]=0   */
	outb(_PIDXR, 0x62);	/* select P3X0HI */
	outb(_PNPWRP, (BYTE) (iw.p3xr >> 8));	/* set P3XR[9:8] */
	outb(_PIDXR, 0x63);	/* select P3X0LI */
	outb(_PNPWRP, (BYTE) (iw.p3xr));	/* set P3XR[7:3] */
	/* P3XR[2:0]=0   */
	outb(_PIDXR, 0x64);	/* select PHCAI */
	outb(_PNPWRP, (BYTE) (iw.pcodar >> 8));	/* set PCODAR[9:8] */
	outb(_PIDXR, 0x65);	/* select PLCAI */
	outb(_PNPWRP, (BYTE) iw.pcodar);	/* set PCODAR[7:2] */

	outb(_PIDXR, 0x70);	/* select PUI1SI */
	outb(_PNPWRP, (BYTE) (iw.synth_irq & 0x0F));	/* Synth IRQ number */
	outb(_PIDXR, 0x72);	/* select PUI2SI */
	outb(_PNPWRP, (BYTE) (iw.midi_irq & 0x0F));	/* MIDI IRQ number */

	outb(_PIDXR, 0x74);	/* select PUD1SI */
	outb(_PNPWRP, (BYTE) (iw.dma1_chan & 0x07));	/* DMA channel 1 */
	outb(_PIDXR, 0x75);	/* select PUD2SI */
	outb(_PNPWRP, (BYTE) (iw.dma2_chan & 0x07));	/* DMA channel 2 */

	IwavePnpDevice(EXT);
	outb(_PIDXR, 0x60);	/* select PRAHI */
	outb(_PNPWRP, (BYTE) (iw.pcdrar >> 8));	/* set PCDRAR[9:8] */
	outb(_PIDXR, 0x61);	/* select PRALI */
	outb(_PNPWRP, (BYTE) iw.pcdrar);	/* set PCDRAR[7:3] */
	/* PCDRAR[2:0]=0 */
	outb(_PIDXR, 0x62);	/* select PATAHI */
	outb(_PNPWRP, (BYTE) (iw.pataar >> 8));	/* set PATAAR[9:8] */
	outb(_PIDXR, 0x63);	/* select PATALI */
	outb(_PNPWRP, (BYTE) iw.pataar);	/* set PATAAR[7:1] */
	/* PATAAR[0]=0 */
	outb(_PIDXR, 0x70);	/* select PRISI */
	outb(_PNPWRP, (BYTE) (iw.ext_irq & 0x0F));	/* Ext Dev IRQ number */
	outb(_PIDXR, 0x74);	/* select PRDSI */
	outb(_PNPWRP, (BYTE) (iw.ext_chan & 0x07));	/* Ext Dev DMA channel */

	IwavePnpDevice(GAME);
	outb(_PIDXR, 0x60);	/* select P201HI */
	outb(_PNPWRP, (BYTE) (iw.p201ar >> 8));	/* set P201RAR[9:8] */
	outb(_PIDXR, 0x61);	/* select P201LI */
	outb(_PNPWRP, (BYTE) iw.p201ar);	/* set P201AR[7:6] */

	IwavePnpDevice(EMULATION);
	outb(_PIDXR, 0x60);	/* select P388HI */
	outb(_PNPWRP, (BYTE) (iw.p388ar >> 8));	/* set P388AR[9:8] */
	outb(_PIDXR, 0x61);	/* select P388LI */
	outb(_PNPWRP, (BYTE) iw.p388ar);	/* set P388AR[7:6] */

	outb(_PIDXR, 0x70);	/* select PSBISI */
	outb(_PNPWRP, (BYTE) (iw.emul_irq & 0x0F));	/* emulation IRQ number */

	IwavePnpDevice(MPU401);
	outb(_PIDXR, 0x60);	/* select P401HI */
	outb(_PNPWRP, (BYTE) (iw.p401ar >> 8));	/* set P401AR[9:8] */
	outb(_PIDXR, 0x61);	/* select P401LI */
	outb(_PNPWRP, (BYTE) iw.p401ar);	/* set P401AR[7:1] */

	outb(_PIDXR, 0x70);	/* select PMISI */
	outb(_PNPWRP, (BYTE) (iw.mpu_irq & 0x0F));	/* MPU emulation IRQ
							 * number */
	LEAVE_CRITICAL;
}

/* ######################################################################## */
/* FILE: iwpnp.c */
/* */
/* REMARKS: This file contains the definitions for the InterWave's DDK */
/* functions dedicated to the configuration of the InterWave */
/* PNP logic. */
/* */
/* UPDATE: 4/07/95 */
/* ######################################################################## */
/* */
/* FUNCTION: IwavePnpKey */
/* */
/* PROFILE: This function issues the initiation key that places the PNP */
/* logic into configuration mode. The PNP logic is quiescent at */
/* power up and must be enabled by software. This function will */
/* do 32 I/O writes to the PIDXR (0x0279). The function will */
/* first reset the LFSR to its initial value by a sequence of two */
/* write cycles of 0x00 to PIDXR before issuing the key. */
/* */
/* ######################################################################## */
static void 
IwavePnpKey(void)
{
	/* send_Initiation_LFSR(); */

	BYTE            code = 0x6A;
	BYTE            msb;
	BYTE            i;

	/* ############################################### */
	/* Reset Linear Feedback Shift Reg. */
	/* ############################################### */
	outb(0x279, 0x00);
	outb(0x279, 0x00);

	outb(0x279, code);	/* Initial value */

	for (i = 1; i < 32; i++) {
		msb = ((code & 0x01) ^ ((code & 0x02) >> 1)) << 7;
		code = (code >> 1) | msb;
		outb(0x279, code);
	}

}

static BYTE 
IwavePnpIsol(PORT * pnpread)
{
	int             num_pnp_devs;
	int             rd_port = 0;
	printf("Checking for GUS Plug-n-Play ...\n");

	/* Try various READ_DATA ports from 0x203-0x3ff */
	for (rd_port = 0x80; (rd_port < 0xff); rd_port += 0x10) {
		if (0)
			printf("Trying Read_Port at %x\n",
			       (rd_port << 2) | 0x3);

		num_pnp_devs = isolation_protocol(rd_port);
		if (num_pnp_devs) {
			*pnpread = rd_port << 2 | 0x3;
			break;
		}
	}
	if (!num_pnp_devs) {
		printf("No Plug-n-Play devices were found\n");
		return 0;
	}
	return 1;
}

/* ######################################################################## */
/* */
/* FUNCTION: IwavePnpPeek */
/* */
/* PROFILE: This function will return the number of specified bytes of */
/* resource data from the serial EEPROM. The function will NOT */
/* reset the serial EEPROM logic to allow reading the entire */
/* EEPROM by issuing repeated calls. The caller must supply a */
/* pointer to where the data are to be stored. */
/* It is assumed that the InterWave is not in either "sleep" */
/* or "wait for key" states. Note that on the first call, if */
/* the caller means to read from the beggining of data the */
/* serial EEPROM logic must be reset. For this, the caller */
/* should issue a WAKE[CSN] command */
/* */
/* ######################################################################## */
static void 
IwavePnpPeek(PORT pnprdp, WORD bytes, BYTE * data)
{
	WORD            i;
	BYTE            datum;

	for (i = 1; i <= bytes; i++) {
		outb(_PIDXR, 0x05);	/* select PRESSI */

		while (TRUE) {	/* wait til new data byte is ready */
			if (inb(pnprdp) & PNP_DATA_RDY)
				break;	/* new resource byte ready */
		}
		outb(_PIDXR, 0x04);	/* select PRESDI */
		datum = inb(pnprdp);	/* read resource byte */
		if (data != NULL)
			*(data++) = datum;	/* store it */
	}
}
/* ######################################################################## */
/* */
/* FUNCTION: IwavePnpActivate */
/* */
/* PROFILE: This function will activate or de-activate the audio device */
/* or the external device on the InterWave. Set the "dev" arg */
/* to AUDIO for the audio device or EXT for the external device. */
/* Set "bool" to ON or OFF to turn the device on or off the ISA */
/* bus. Notice that for a logical device to work, it must be */
/* activated. */
/* */
/* ######################################################################## */
static void 
IwavePnpActivate(BYTE dev, BYTE bool)
{
	IwavePnpDevice(dev);	/* select audio device */
	ENTER_CRITICAL;
	outb(_PIDXR, ACTIVATE_DEV);	/* select Activate Register */
	outb(_PNPWRP, bool);	/* write register */
	LEAVE_CRITICAL;

}
/* ######################################################################## */
/* */
/* FUNCTION: IwavePnpDevice */
/* */
/* PROFILE: This function allows the caller to select between five */
/* logical devices available on the InterWave.It is assumed */
/* that the PNP state machine is in configuration mode. */
/* */
/* ######################################################################## */
static void 
IwavePnpDevice(BYTE dev)
{
	ENTER_CRITICAL;
	outb(_PIDXR, _PLDNI);	/* select PLDNI */
	outb(_PNPWRP, dev);	/* write PLDNI */
	LEAVE_CRITICAL;
}
/* ######################################################################## */
/* */
/* FUNCTION: IwavePnpWake */
/* */
/* PROFILE: This function issues a WAKE[CSN] command to the InterWave. If */
/* the CSN matches the PNP state machine will enter the */
/* configuration state. Otherwise it will enter the sleep mode. */
/* */
/* It is assumed that the PNP state machine is not in the */
/* "wait for key" state. */
/* */
/* ######################################################################## */
static void 
IwavePnpWake(BYTE csn)
{
	ENTER_CRITICAL;
	outb(_PIDXR, _PWAKEI);	/* select PWAKEI */
	outb(_PNPWRP, csn);	/* write csn */
	LEAVE_CRITICAL;
}
/* ######################################################################## */
/* */
/*
 * FUNCTION: IwavePnpPing
 */
/* */
/* PROFILE: This function allows the caller to detect an InterWave based */
/* adapter board and will return its asigned CSN so that an */
/* an application can access its PnP interface and determine the */
/* borad's current configuration. In conducting its search for */
/* the InterWave IC, the function will use the first 32 bits of */
/* the Serial Identifier called the vendor ID in the PnP ISA */
/* spec. The last 4 bits in the Vendor ID represent a revision */
/* number for the particular product and will not be included */
/* in the search. The function will return the Vendor ID and the */
/* calling application should check the revision bits to make */
/* sure they are compatible with the board. */
/* */
/* ######################################################################## */
static BYTE 
IwavePnpPing(DWORD VendorID)
{
	BYTE            csn;

	VendorID &= (0xFFFFFFF0);	/* reset 4 least significant bits */
	IwavePnpKey();		/* Key to access PnP Interface */
	while (iw.pnprdp <= 0x23F) {
		for (csn = 1; csn <= 10; csn++) {
			IwavePnpWake(csn);	/* Select card */
			IwavePnpPeek(iw.pnprdp, 4, (BYTE *) & iw.vendor);	/* get vendor ID */


			if (((iw.vendor) & 0xFFFFFFF0) == VendorID) {	/* If IDs match,
									 * InterWave is found */

				outb(_PIDXR, 0x02);	/* Place all cards in
							 * wait-for-key state */
				outb(0x0A79, 0x02);
				return (csn);
			}
		}
		iw.pnprdp += 0x04;
	}
	outb(_PIDXR, 0x02);	/* Place all cards in wait-for-key state */
	outb(0x0A79, 0x02);
	return (FALSE);		/* InterWave IC not found */
}

/* end of pnp code */

static WORD 
IwaveMemSize(void)
{
	BYTE            datum = 0x55;
	ADDRESS         local = 0L;

	outb(iw.igidxr, _LMCI);
	outb(iw.i8dp, inb(iw.i8dp) & 0xFD);	/* DRAM I/O cycles selected */

	while (TRUE) {
		IwaveMemPoke(local, datum);
		IwaveMemPoke(local + 1L, datum + 1);
		if (IwaveMemPeek(local) != datum || IwaveMemPeek(local + 1L) != (datum + 1) || IwaveMemPeek(0L) != 0x55)
			break;
		local += RAM_STEP;
		datum++;
	}
	return ((WORD) (local >> 10));
}

static BYTE 
IwaveMemPeek(ADDRESS addr)
{
	PORT            p3xr;

	p3xr = iw.p3xr;


	outb(iw.igidxr, 0x43);	/* Select LMALI */
	outw(iw.i16dp, (WORD) addr);	/* Lower 16 bits of LM */
	outb(iw.igidxr, 0x44);	/* Select LMAHI */
	outb(iw.i8dp, (BYTE) (addr >> 16));	/* Upper 8 bits of LM */
	return (inb(iw.lmbdr));	/* return byte from LMBDR */
}


static void 
IwaveMemPoke(ADDRESS addr, BYTE datum)
{
	PORT            p3xr;
	p3xr = iw.p3xr;


	outb(iw.igidxr, 0x43);	/* Select LMALI */
	outw(iw.i16dp, (WORD) addr);	/* Lower 16 bits of LM */
	outb(iw.igidxr, 0x44);	/* Select LMAHI */
	outb(iw.i8dp, (BYTE) (addr >> 16));	/* Upper 8 bits of LM */
	outb(iw.lmbdr, datum);	/* Write byte to LMBDR */
}

/* ######################################################################## */
/* */
/* FUNCTION: IwaveMemCfg */
/* */
/* PROFILE : This function determines the amount of DRAM from its */
/* configuration accross all banks. It sets the configuration */
/* into register LMCFI and stores the total amount of DRAM */
/* into iw.size_mem (Kbytes). */
/* */
/* The function first places the IC in enhanced mode to allow */
/* full access to all DRAM locations. Then it selects full */
/* addressing span (LMCFI[3:0]=0x0C). Finally, it determines */
/* the amount of DRAM in each bank and from this the actual */
/* configuration. */
/* */
/* Note that if a configuration other than one indicated in */
/* the manual is implemented, this function will select */
/* full addressing span (LMCFI[3:0]=0xC). */
/* */
/* ######################################################################## */
static void 
IwaveMemCfg(DWORD * lpbanks)
{
	DWORD           bank[4] = {0L, 0L, 0L, 0L};
	DWORD           addr = 0L, base = 0L, cnt = 0L;
	BYTE            i, reg, ram = FALSE;
	WORD            lmcfi;
	/* */
	ENTER_CRITICAL;
	outb(iw.igidxr, 0x99);
	reg = inb(iw.i8dp);	/* image of sgmi */
	outb(iw.igidxr, 0x19);
	outb(iw.i8dp, (BYTE) (reg | 0x01));	/* enable enhaced mode */
	outb(iw.igidxr, _LMCFI);/* select LM Conf Reg */
	lmcfi = inw(iw.i16dp) & 0xFFF0;
	outw(iw.i16dp, lmcfi | 0x000C);	/* max addr span */
	/* */
	/* Clear every RAM_STEPth location */
	/* */
	while (addr < RAM_MAX) {
		IwaveMemPoke(addr, 0x00);
		addr += RAM_STEP;
	}
	/* */
	/* Determine amount of RAM in each bank */
	/* */
	for (i = 0; i < 4; i++) {
		IwaveMemPoke(base, 0xAA);	/* mark start of bank */
		IwaveMemPoke(base + 1L, 0x55);
		if ((IwaveMemPeek(base) == 0xAA) && (IwaveMemPeek(base + 1L) == 0x55))
			ram = TRUE;
		if (ram) {
			while (cnt < BANK_MAX) {
				bank[i] += RAM_STEP;
				cnt += RAM_STEP;
				addr = base + cnt;
				if (IwaveMemPeek(addr) == 0xAA)
					break;
			}
		}
		if (lpbanks != NULL) {
			*lpbanks = bank[i];
			lpbanks++;
		}
		bank[i] = bank[i] >> 10;
		base += BANK_MAX;
		cnt = 0L;
		ram = FALSE;
	}
	/* */
	iw.flags &= ~DRAM_HOLES;
	outb(iw.igidxr, _LMCFI);
	if (bank[0] == 256 && bank[1] == 0 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi);
	else if (bank[0] == 256 && bank[1] == 256 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x01);
	else if (bank[0] == 256 && bank[1] == 256 && bank[2] == 256 && bank[3] == 256)
		outw(iw.i16dp, lmcfi | 0x02);
	else if (bank[0] == 256 && bank[1] == 1024 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x03);
	else if (bank[0] == 256 && bank[1] == 1024 && bank[2] == 1024 && bank[3] == 1024)
		outw(iw.i16dp, lmcfi | 0x04);
	else if (bank[0] == 256 && bank[1] == 256 && bank[2] == 1024 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x05);
	else if (bank[0] == 256 && bank[1] == 256 && bank[2] == 1024 && bank[3] == 1024)
		outw(iw.i16dp, lmcfi | 0x06);
	else if (bank[0] == 1024 && bank[1] == 0 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x07);
	else if (bank[0] == 1024 && bank[1] == 1024 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x08);
	else if (bank[0] == 1024 && bank[1] == 1024 && bank[2] == 1024 && bank[3] == 1024)
		outw(iw.i16dp, lmcfi | 0x09);
	else if (bank[0] == 4096 && bank[1] == 0 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x0A);
	else if (bank[0] == 4096 && bank[1] == 4096 && bank[2] == 0 && bank[3] == 0)
		outw(iw.i16dp, lmcfi | 0x0B);
	else			/* Flag the non-contiguous config of memory */
		iw.flags |= DRAM_HOLES;
	/* */
	outb(iw.igidxr, 0x19);	/* restore sgmi */
	outb(iw.i8dp, reg);
	LEAVE_CRITICAL;
}


/* ######################################################################## */
/**/
/* FUNCTION: IwaveCodecIrq */
/**/
/* PROFILE: This function disables or enables the Codec Interrupts. To */
/* enable interrupts set CEXTI[2] high thus causing all interrupt */
/* sources (CSR3I[6:4]) to pass onto the IRQ pin. To disable */
/* interrupts set CEXTI[1]=0. To enable Code IRQs issue this call: */
/**/
/* IwaveCodecIrq(CODEC_IRQ_ENABLE). To disable IRQs issue the call */
/**/
/* IwaveCodeIrq(~CODEC_IRQ_ENABLE). */
/**/
/* ######################################################################## */
static void 
IwaveCodecIrq(BYTE mode)
{
	BYTE            reg;

	ENTER_CRITICAL;
	reg = inb(iw.pcodar) & 0xE0;
	outb(iw.pcodar, reg | _CSR3I);	/* select CSR3I */
	outb(iw.cdatap, 0x00);	/* clear all interrupts */
	outb(iw.pcodar + 0x02, 0x00);	/* clear CSR1R */
	outb(iw.pcodar, reg | _CEXTI);	/* select CEXTI */
	reg = inb(iw.cdatap);
	if (mode == CODEC_IRQ_ENABLE)	/* enable Codec Irqs */
		outb(iw.cdatap, (BYTE) (reg | CODEC_IRQ_ENABLE));
	else			/* disable Codec Irqs */
		outb(iw.cdatap, (BYTE) (reg & ~CODEC_IRQ_ENABLE));
	LEAVE_CRITICAL;
}


/* ######################################################################### */
/**/
/* FUNCTION: IwaveRegPeek */
/**/
/* PROFILE : This function returns the value stored in any readable */
/* InterWave register. It takes as input a pointer to a */
/* structure containing the addresses of the relocatable I/O */
/* space as well as a register mnemonic. To correctly use this */
/* function, the programmer must use the mnemonics defined in */
/* "iwdefs.h". These mnemonics contain coded information used */
/* by the function to properly access the desired register. */
/**/
/* An attempt to read from a write-only register will return */
/* meaningless data. */
/**/
/* ######################################################################### */
static WORD 
IwaveRegPeek(DWORD reg_mnem)
{
	BYTE            index, val;
	WORD            reg_id, offset;

	offset = (WORD) ((BYTE) reg_mnem);
	reg_id = (WORD) (reg_mnem >> 16);
	index = (BYTE) (reg_mnem >> 8);

	/* ################################################### */
	/* Logic to read registers in P2XR block & GMCR */
	/* ################################################### */

	if (reg_id >= 0x0001 && reg_id <= 0x001A) {	/* UMCR to GMCR */
		if (reg_id <= 0x000E)	/* UMCR to USRR */
			return ((WORD) inb(iw.p2xr + offset));

		if (reg_id == 0x0019)
			return ((WORD) inb(iw.p201ar));

		else {		/* GUS Hidden registers or GMCR */
			BYTE            iveri;

			outb(iw.igidxr, 0x5B);	/* select IVERI */
			iveri = inb(iw.i8dp);	/* read IVERI */
			outb(iw.i8dp, (BYTE) (iveri | 0x09));	/* set IVERI[3,0] */
			if (reg_id == 0x001A) {	/* GMCR */
				val = inb(iw.p3xr);
				outb(iw.i8dp, iveri);	/* restore IVERI */
				return ((WORD) val);
			}
			val = inb(iw.p2xr + 0x0F);	/* read URCR */
			val = (val & 0xF8) | index;	/* value for URCR[2:0] */
			outb(iw.p2xr + 0x0F, val);	/* set URCR[2:0] */

			if (reg_mnem == UDCI || reg_mnem == UICI) {
				val = inb(iw.p2xr);
				if (reg_mnem == UDCI)
					outb(iw.p2xr, (BYTE) (val & 0xBF));
				else
					outb(iw.p2xr, (BYTE) (val | 0x40));
			}
			val = inb(iw.p2xr + 0x0B);
			outb(iw.igidxr, 0x5B);	/* select IVERI */
			outb(iw.i8dp, iveri);	/* restore IVERI */
			return ((WORD) val);	/* read register */
		}
	}
	/* ################################################### */
	/* Logic to read registers in P3XR block */
	/* ################################################### */

	if (reg_id >= 0x001B && reg_id <= 0x005C) {	/* GMSR to LMBDR */
		if (reg_id == 0x005C)	/* LMBDR */
			return ((WORD) inb(iw.lmbdr));

		if (reg_id >= 0x001B && reg_id <= 0x0021)	/* GMSR to I8DP */
			if (offset == 0x04)
				return (inw(iw.i16dp));
			else
				return ((WORD) inb(iw.p3xr + offset));
		else {		/* indexed registers */

			if (reg_id <= 0x003F)
				index |= 0x80;	/* adjust for reading */

			outb(iw.igidxr, index);	/* select register */

			if (offset == 0x04)
				return (inw(iw.i16dp));
			else
				return ((WORD) inb(iw.i8dp));
		}
	}
	/* #################################################### */
	/* Logic to read registers in PCODAR block */
	/* #################################################### */

	if (reg_id >= 0x005D && reg_id <= 0x0081) {	/* CIDXR to CLRCTI */
		if (reg_id <= 0x0061)
			return ((WORD) inb(iw.pcodar + offset));	/* CRDR */

		else {		/* indexed registers */
			BYTE            cidxr;

			cidxr = inb(iw.pcodar);
			cidxr = (cidxr & 0xE0) + index;
			outb(iw.pcodar, cidxr);	/* select register */
			return ((WORD) inb(iw.cdatap));
		}
	}
	/* ##################################################### */
	/* Logic to read the PnP registers */
	/* ##################################################### */
	if (reg_id >= 0x0082 && reg_id <= 0x00B7) {	/* PCSNBR to PMITI */
		if (reg_id == 0x0085)
			return ((WORD) inb(iw.pnprdp));

		if (reg_id < 0x0085)
			return ((WORD) inb((WORD) reg_mnem));

		else {		/* indexed registers */
			if (reg_id >= 0x008E && reg_id <= 0x00B7) {
				outb(0x0279, 0x07);	/* select PLDNI */
				outb(0xA79, (BYTE) offset);	/* select logical dev */
			}
			outb(0x0279, index);	/* select the register */
			return ((WORD) inb(iw.pnprdp));
		}
	}
	return 0;
}
/* ######################################################################### */
/**/
/* FUNCTION: IwaveRegPoke */
/**/
/* PROFILE : This function writes a value to any writable */
/* InterWave register. It takes as input a pointer to a */
/* structure containing the addresses of the relocatable I/O */
/* space as well as a register mnemonic. To correctly use this */
/* function, the programmer must use the mnemonics defined in */
/* "iwdefs.h". These mnemonics contain coded information used */
/* by the function to properly access the desired register. */
/**/
/* This function does not guard against writing to read-only */
/* registers. It is the programmer's responsibility to ensure */
/* that the writes are to valid registers. */
/**/
/* ######################################################################### */
static void 
IwaveRegPoke(DWORD reg_mnem, WORD datum)
{
	BYTE            index;
	BYTE            val;
	WORD            reg_id;
	WORD            offset;

	offset = (WORD) ((BYTE) reg_mnem);
	reg_id = (WORD) (reg_mnem >> 16);
	index = (BYTE) (reg_mnem >> 8);


	/* ####################################################### */
	/* Logic to write to registers in P2XR block */
	/* ####################################################### */
	if (reg_id >= 0x0001 && reg_id <= 0x0019) {	/* UMCR to GGCR */
		if (reg_id <= 0x000E) {	/* UMCR to USRR */
			outb(iw.p2xr + offset, (BYTE) datum);
			return;
		}
		if (reg_id == 0x0019) {
			outb(iw.p201ar, (BYTE) datum);
			return;
		} else {	/* GUS Hidden registers */

			BYTE            iveri;

			outb(iw.igidxr, 0x5B);	/* select IVERI */
			iveri = inb(iw.i8dp);	/* read IVERI */
			outb(iw.i8dp, (BYTE) (iveri | 0x09));	/* set IVERI[3,0] */
			val = inb(iw.p2xr + 0x0F);	/* read URCR */
			val = (val & 0xF8) | index;	/* value for URCR[2:0] */
			outb(iw.p2xr + 0x0F, val);	/* set URCR[2:0] */

			if (reg_mnem == UDCI || reg_mnem == UICI) {
				val = inb(iw.p2xr);	/* read UMCR */
				if (reg_mnem == UDCI)
					outb(iw.p2xr, (BYTE) (val & 0xBF));	/* set UMCR[6]=0 */
				else
					outb(iw.p2xr, (BYTE) (val | 0x40));	/* set UMCR[6]=1 */
			}
			outb(iw.p2xr + 0x0B, (BYTE) datum);	/* write register */
			outb(iw.igidxr, 0x5B);	/* select IVERI */
			outb(iw.i8dp, iveri);	/* restore IVERI */
			return;
		}
	}
	/* ############################################################# */
	/* Logic to write to registers in P3XR block */
	/* ############################################################# */

	if (reg_id >= 0x001A && reg_id <= 0x005C) {	/* GMCR to LMBDR */

		if (reg_id == 0x005C) {	/* LMBDR */
			outb(iw.lmbdr, (BYTE) datum);
			return;
		}
		if (reg_id == 0x001B)	/* GMSR */
			return;

		if (reg_id >= 0x001A && reg_id <= 0x0021)	/* GMCR to I8DP */
			if (offset == 0x04)
				outw(iw.i16dp, datum);
			else
				outb(iw.p3xr + offset, (BYTE) datum);
		else {		/* indexed registers */
			outb(iw.igidxr, index);	/* select register */

			if (offset == 0x04)
				outw(iw.i16dp, datum);
			else
				outb(iw.i8dp, (BYTE) datum);
		}
	}
	/* /################################################### */
	/* Logic to write to registers in PCODAR block */
	/* ################################################### */

	if (reg_id >= 0x005C && reg_id <= 0x0081) {	/* CIDXR to CLRCTI */
		if (reg_id <= 0x0061)
			outb(iw.pcodar + offset, (BYTE) datum);

		else {		/* one of the indexed registers */
			BYTE            cidxr;

			cidxr = inb(iw.pcodar);
			cidxr = (cidxr & 0xE0) + index;
			outb(iw.pcodar, cidxr);	/* select register */
			outb(iw.cdatap, (BYTE) datum);
		}
	}
	/* ###################################################### */
	/* Logic to write to the PnP registers */
	/* ###################################################### */
	if (reg_id >= 0x0082 && reg_id <= 0x00B7) {
		if (reg_id == 0x0085) {
			outb(iw.pnprdp, (BYTE) datum);
			return;
		}
		if (reg_id < 0x0085)
			outb((WORD) reg_mnem, (BYTE) datum);

		else {		/* one of the indexed registers */
			if (reg_id >= 0x008E && reg_id <= 0x00B7) {
				outb(0x0279, 0x07);	/* select PLDNI */
				outb(0xA79, (BYTE) offset);	/* select logical dev */
			}
			outb(0x0279, index);	/* select the register */
			outb(0xA79, (BYTE) datum);
		}
	}
}


static void 
IwaveLineLevel(char level, char index)
{
	char            reg;

	level &= 0x1F;

	ENTER_CRITICAL;
	reg = inb(iw.pcodar) & 0xE0;
	outb(iw.pcodar, reg | index);	/* select register */
	outb(iw.cdatap, (BYTE) ((inb(iw.cdatap) & 0x80) | level));	/* set level */
	LEAVE_CRITICAL;
}

static void 
IwaveCodecMode(char mode)
{
    char            reg;

    ENTER_CRITICAL;
    reg = inb(iw.pcodar) & 0xE0;
    outb(iw.pcodar, reg | _CMODEI);	/* select CMODEI */
    outb(iw.cdatap, mode);
    LEAVE_CRITICAL;
    iw.cmode = mode;
}

static void 
IwaveLineMute(BYTE mute, BYTE inx)
{
    BYTE            reg;

    ENTER_CRITICAL;
    reg = inb(iw.pcodar) & 0xE0;
    outb(iw.pcodar, reg | inx);	/* select register */
    if (mute == ON)
	outb(iw.cdatap, (BYTE) (inb(iw.cdatap) | 0x80));	/* mute */
    else
	outb(iw.cdatap, (BYTE) (inb(iw.cdatap) & 0x7F));	/* unmute */
    LEAVE_CRITICAL;
}

static void 
Iwaveinitcodec()
{

	u_short  iwl_codec_base = iw.pcodar;
	u_short  iwl_codec_data = iw.pcodar + 1;
	u_short  foo;



	/*
	 * Set the CEXTI register foo = CODEC_CEXTI_DEFAULT;
	 * IWL_CODEC_OUT(EXTERNAL_CONTROL, foo);
	 */
	/*
	 * Disable Interrupts iwl_codec_disable_irqs();
	 */

	/* Set the CODEC to Operate in Mode 3 */
	IWL_CODEC_OUT(MODE_SELECT_ID, 0x6C);
	foo = inb(iwl_codec_data);

	/* Set the configuration registers to their default values */
	foo = CODEC_CFIG1I_DEFAULT;
	IWL_CODEC_OUT(CONFIG_1 | CODEC_MCE, foo);
	outb(iwl_codec_base, CONFIG_1);
	foo = CODEC_CFIG2I_DEFAULT;
	IWL_CODEC_OUT(CONFIG_2, foo);

	foo = CODEC_CFIG3I_DEFAULT;
	IWL_CODEC_OUT(CONFIG_3, foo);

}



int 
IwaveOpen(char voices, char mode, struct address_info * hw)
{

	u_long   flags;
	u_char   tmp;

	flags = splhigh();

	iw.pnprdp = 0;
	if (IwavePnpIsol(&iw.pnprdp)) {

		iw.vendor = GUS_PNP_ID;

		iw.csn = IwavePnpPing(iw.vendor);

		IwavePnpKey();

		IwavePnpWake(iw.csn);

		IwavePnpGetCfg();
		IwavePnpKey();

		IwavePnpWake(iw.csn);
	}
	if (hw->irq > 0) {
		/* I see the user wants to set the GUS PnP */
		/* Okay lets do it */
		iw.csn = 1;
		iw.p2xr = hw->io_base;
		iw.p3xr = hw->io_base + 0x100;
		iw.pcodar = hw->io_base + 0x10c;

		iw.synth_irq = hw->irq;

		iw.midi_irq = hw->irq;

		iw.dma1_chan = hw->dma;

		if (hw->dma2 == -1) {
			iw.dma2_chan = hw->dma;
		} else {
			iw.dma2_chan = hw->dma2;
		}


	} else {

		/* tell the os what we are doing 8) */
		hw->io_base = iw.p2xr;
		hw->irq = iw.synth_irq;
		/*
		 * iw.dma1_chan = 1; iw.dma2_chan = 3 ;
		 */
		hw->dma = iw.dma1_chan;
		hw->dma2 = iw.dma2_chan;


	}

	if (iw.csn > 0 && iw.csn < MAX_GUS_PNP) {
		gus_pnp_found[iw.csn] = hw->io_base;

	}
	iw.cdatap = iw.pcodar + 1;
	iw.csr1r = iw.pcodar + 2;
	iw.cxdr = iw.pcodar + 3;/* CPDR or CRDR */
	iw.gmxr = iw.p3xr;
	iw.gmxdr = iw.p3xr + 1;	/* GMTDR or GMRDR */
	iw.svsr = iw.p3xr + 2;
	iw.igidxr = iw.p3xr + 3;
	iw.i16dp = iw.p3xr + 4;
	iw.i8dp = iw.p3xr + 5;
	iw.lmbdr = iw.p3xr + 7;
	iw.voices = voices;

	if (iw.pnprdp > 0 && iw.csn > 0) {
		IwavePnpSetCfg();
		IwavePnpActivate(AUDIO, ON);
		IwavePnpActivate(EXT, ON);
	}
	/* IwavePnpActivate(EMULATION,ON); */


	/* reset */
	outb(iw.igidxr, _URSTI);/* Pull reset */
	outb(iw.i8dp, 0x00);
	DELAY(1000 * 30);

	outb(iw.i8dp, 0x01);	/* Release reset */
	DELAY(1000 * 30);

	/* end of reset */


	IwaveMemCfg(NULL);


	tmp = IwaveRegPeek(IDECI);

	IwaveRegPoke(IDECI, tmp | 0x18);

	IwaveCodecMode(CODEC_MODE2);	/* Default codec mode  */
	IwaveRegPoke(ICMPTI, 0);

	outb(iw.igidxr, 0x99);
	tmp = inb(iw.i8dp);
	outb(iw.igidxr, 0x19);
	outb(iw.i8dp, tmp);



	IwaveCodecIrq(~CODEC_IRQ_ENABLE);

	Iwaveinitcodec();

	outb(iw.p2xr, 0x0c);	/* Disable line in, mic and line out */

	IwaveRegPoke(CLCI, 0x3f << 2);

	IwaveLineLevel(0, _CLOAI);
	IwaveLineLevel(0, _CROAI);

	IwaveLineMute(OFF, _CLOAI);
	IwaveLineMute(OFF, _CROAI);

	IwaveLineLevel(0, _CLLICI);
	IwaveLineLevel(0, _CRLICI);
	IwaveLineMute(OFF, _CLLICI);
	IwaveLineMute(OFF, _CRLICI);

	IwaveLineLevel(0, _CLDACI);
	IwaveLineLevel(0, _CRDACI);
	IwaveLineMute(ON, _CLDACI);
	IwaveLineMute(ON, _CRDACI);

	IwaveLineLevel(0, _CLLICI);
	IwaveLineLevel(0, _CRLICI);
	IwaveLineMute(ON, _CLLICI);
	IwaveLineMute(ON, _CRLICI);


	IwaveInputSource(LEFT_SOURCE, MIC_IN);
	IwaveInputSource(RIGHT_SOURCE, MIC_IN);

	outb(iw.pcodar, 0x9 | 0x40);
	outb(iw.cdatap, 0);
	IwaveCodecIrq(CODEC_IRQ_ENABLE);
	outb(iw.pcodar, _CFIG3I | 0x20);


	outb(iw.cdatap, 0xC2);	/* Enable Mode 3 IRQs & Synth  */

	outb(iw.igidxr, _URSTI);
	outb(iw.i8dp, GF1_SET | GF1_OUT_ENABLE | GF1_IRQ_ENABLE);
	DELAY(1000 * 30);
	iw.size_mem = IwaveMemSize();	/* Bytes of RAM in this mode */
	outb(iw.p2xr, 0xc);	/* enable output */
	IwaveRegPoke(CLCI, 0x3f << 2);

	IwaveCodecIrq(CODEC_IRQ_ENABLE);
	splx(flags);

	DELAY(1000 * 100);
	IwaveRegPoke(CPDFI, 0);

	return (TRUE);
}


void
gus_wave_init(struct address_info * hw_config)
{
    u_long   flags;
    u_char   val, gus_pnp_seen = 0;
    char           *model_num = "2.4";
    int             gus_type = 0x24;	/* 2.4 */
    int irq = hw_config->irq, dma = hw_config->dma, dma2 = hw_config->dma2;
    int otherside = -1, i;

    if (irq < 0 || irq > 15) {
	printf("ERROR! Invalid IRQ#%d. GUS Disabled", irq);
	return;
    }
    if (dma < 0 || dma > 7) {
	printf("ERROR! Invalid DMA#%d. GUS Disabled", dma);
	return;
    }
    for (i = 0; i < MAX_GUS_PNP; i++) {
	if (gus_pnp_found[i] != 0 && gus_pnp_found[i] == hw_config->io_base)
	    gus_pnp_seen = 1;
    }
#ifdef NOGUSPNP
    gus_pnp_seen = 0;
#endif

    gus_irq = irq;
    gus_dma = dma;
    gus_dma2 = dma2;

    if (gus_dma2 == -1)
	gus_dma2 = dma;

    /*
     * Try to identify the GUS model.
     * 
     * Versions < 3.6 don't have the digital ASIC. Try to probe it first.
     */

    flags = splhigh();
    outb(gus_base + 0x0f, 0x20);
    val = inb(gus_base + 0x0f);
    splx(flags);

    if (val != 0xff && (val & 0x06)) {	/* Should be 0x02?? */
	/*
	 * It has the digital ASIC so the card is at least v3.4. Next
	 * try to detect the true model.
	 */

	val = inb(u_MixSelect);

	/*
	 * Value 255 means pre-3.7 which don't have mixer. Values 5
	 * thru 9 mean v3.7 which has a ICS2101 mixer. 10 and above
	 * is GUS MAX which has the CS4231 codec/mixer.
	 * 
	 */

	if (gus_pnp_seen)
	    val = 66;

	if (val == 255 || val < 5) {
	    model_num = "3.4";
	    gus_type = 0x34;
	} else if (val < 10) {
	    model_num = "3.7";
	    gus_type = 0x37;
	    mixer_type = ICS2101;
	} else {
	    if (gus_pnp_seen)
		model_num = "PNP";
	    else
		model_num = "MAX";

	    gus_type = 0x40;
	    mixer_type = CS4231;
#ifdef CONFIG_GUSMAX
	    {
		u_char   max_config = 0x40;	/* Codec enable */

		if (gus_dma2 == -1)
		    gus_dma2 = gus_dma;

		if (gus_dma > 3)
		    max_config |= 0x10;	/* 16 bit capture DMA */

		if (gus_dma2 > 3)
		    max_config |= 0x20;	/* 16 bit playback DMA */

		max_config |= (gus_base >> 4) & 0x0f;	/* Extract the X from
							 * 2X0 */

		outb(gus_base + 0x106, max_config);	/* UltraMax control */
	    }

	    if (ad1848_detect(gus_base + 0x10c, NULL, hw_config->osp)) {

		gus_mic_vol = gus_line_vol = gus_pcm_volume = 100;
		gus_wave_volume = 90;
		have_gus_max = 1;
		if (gus_pnp_seen) {

		    ad1848_init("GUS PNP", gus_base + 0x10c,
				-irq,
				gus_dma2,	/* Playback DMA */
				gus_dma,	/* Capture DMA */
				1,	/* Share DMA channels with GF1 */
				hw_config->osp);


		} else {
		    ad1848_init("GUS MAX", gus_base + 0x10c,
				-irq,
				gus_dma2,	/* Playback DMA */
				gus_dma,	/* Capture DMA */
				1,	/* Share DMA channels with GF1 */
				hw_config->osp);
		}
		otherside = num_audiodevs - 1;

	    } else
		printf("[Where's the CS4231?]");
#else
	    printf("\n\n\nGUS MAX support was not compiled in!!!\n\n\n\n");
#endif
	}
    } else {
	/*
	 * ASIC not detected so the card must be 2.2 or 2.4. There
	 * could still be the 16-bit/mixer daughter card.
	 */
    }

    if (gus_pnp_seen) {
	snprintf(gus_info.name, sizeof(gus_info.name),
	    "Gravis %s (%dk)", model_num, (int) gus_mem_size / 1024);
    } else {
	snprintf(gus_info.name, sizeof(gus_info.name),
	    "Gravis UltraSound %s (%dk)", model_num, (int) gus_mem_size / 1024);
    }
    conf_printf(gus_info.name, hw_config);

    if (num_synths >= MAX_SYNTH_DEV)
	printf("GUS Error: Too many synthesizers\n");
    else {
	voice_alloc = &guswave_operations.alloc;
	synth_devs[num_synths++] = &guswave_operations;
#ifdef CONFIG_SEQUENCER
	gus_tmr_install(gus_base + 8);
#endif
    }
    samples = (struct patch_info *) malloc((MAX_SAMPLE + 1) * sizeof(*samples), M_DEVBUF, M_NOWAIT);
    if (!samples)
	panic("SOUND: Cannot allocate memory\n");

    reset_sample_memory();

    gus_initialize();

    if (num_audiodevs < MAX_AUDIO_DEV) {
	audio_devs[gus_devnum = num_audiodevs++] = &gus_sampling_operations;
	audio_devs[gus_devnum]->otherside = otherside;
	audio_devs[gus_devnum]->dmachan1 = dma;
	audio_devs[gus_devnum]->dmachan2 = dma2;
	audio_devs[gus_devnum]->buffsize = DSP_BUFFSIZE;
	if (otherside != -1) {
	    /*
	     * glue logic to prevent people from opening the gus
	     * max via the gf1 and the cs4231 side . Only the gf1
	     * or the cs4231 are allowed to be open
	     */

	    audio_devs[otherside]->otherside = gus_devnum;
	}
	if (dma2 != dma && dma2 != -1)
	    audio_devs[gus_devnum]->flags |= DMA_DUPLEX;
    } else
	printf("GUS: Too many PCM devices available\n");

    /*
     * Mixer dependent initialization.
     */

    switch (mixer_type) {
    case ICS2101:
	gus_mic_vol = gus_line_vol = gus_pcm_volume = 100;
	gus_wave_volume = 90;
	ics2101_mixer_init();
	return;

    case CS4231:
	/* Initialized elsewhere (ad1848.c) */
    default:
	gus_default_mixer_init();
	return;
    }
}

static void
do_loop_irq(int voice)
{
	u_char   tmp;
	int             mode, parm;
	u_long   flags;

	flags = splhigh();
	gus_select_voice(voice);

	tmp = gus_read8(0x00);
	tmp &= ~0x20;		/* Disable wave IRQ for this_one voice */
	gus_write8(0x00, tmp);

	if (tmp & 0x03)		/* Voice stopped */
		voice_alloc->map[voice] = 0;

	mode = voices[voice].loop_irq_mode;
	voices[voice].loop_irq_mode = 0;
	parm = voices[voice].loop_irq_parm;

	switch (mode) {

	case LMODE_FINISH:	/* Final loop finished, shoot volume down */

		if ((int) (gus_read16(0x09) >> 4) < 100) {	/* Get current volume */
			gus_voice_off();
			gus_rampoff();
			gus_voice_init(voice);
			break;
		}
		gus_ramp_range(65, 4065);
		gus_ramp_rate(0, 63);	/* Fastest possible rate */
		gus_rampon(0x20 | 0x40);	/* Ramp down, once, irq */
		voices[voice].volume_irq_mode = VMODE_HALT;
		break;

	case LMODE_PCM_STOP:
		pcm_active = 0;	/* Signal to the play_next_pcm_block routine */
	case LMODE_PCM:
		{
			int             flag;	/* 0 or 2 */

			pcm_qlen--;
			pcm_head = (pcm_head + 1) % pcm_nblk;
			if (pcm_qlen && pcm_active) {
				play_next_pcm_block();
			} else {/* Underrun. Just stop the voice */
				gus_select_voice(0);	/* Left channel */
				gus_voice_off();
				gus_rampoff();
				gus_select_voice(1);	/* Right channel */
				gus_voice_off();
				gus_rampoff();
				pcm_active = 0;
			}

			/*
			 * If the queue was full before this interrupt, the
			 * DMA transfer was suspended. Let it continue now.
			 */
			if (dma_active) {
				if (pcm_qlen == 0)
					flag = 1;	/* Underflow */
				else
					flag = 0;
				dma_active = 0;
			} else
				flag = 2;	/* Just notify the dmabuf.c */
			DMAbuf_outputintr(gus_devnum, flag);
		}
		break;

	default:;
	}
	splx(flags);
}

static void
do_volume_irq(int voice)
{
	u_char   tmp;
	int             mode, parm;
	u_long   flags;

	flags = splhigh();

	gus_select_voice(voice);

	tmp = gus_read8(0x0d);
	tmp &= ~0x20;		/* Disable volume ramp IRQ */
	gus_write8(0x0d, tmp);

	mode = voices[voice].volume_irq_mode;
	voices[voice].volume_irq_mode = 0;
	parm = voices[voice].volume_irq_parm;

	switch (mode) {
	case VMODE_HALT:	/* Decay phase finished */
		splx(flags);
		gus_voice_init(voice);
		break;

	case VMODE_ENVELOPE:
		gus_rampoff();
		splx(flags);
		step_envelope(voice);
		break;

	case VMODE_START_NOTE:
		splx(flags);
		guswave_start_note2(voices[voice].dev_pending, voice,
		  voices[voice].note_pending, voices[voice].volume_pending);
		if (voices[voice].kill_pending)
			guswave_kill_note(voices[voice].dev_pending, voice,
					  voices[voice].note_pending, 0);

		if (voices[voice].sample_pending >= 0) {
			guswave_set_instr(voices[voice].dev_pending, voice,
					  voices[voice].sample_pending);
			voices[voice].sample_pending = -1;
		}
		break;

	default:;
	}
}

void
gus_voice_irq(void)
{
	u_long   wave_ignore = 0, volume_ignore = 0;
	u_long   voice_bit;

	u_char   src, voice;

	while (1) {
		src = gus_read8(0x0f);	/* Get source info */
		voice = src & 0x1f;
		src &= 0xc0;

		if (src == (0x80 | 0x40))
			return;	/* No interrupt */

		voice_bit = 1 << voice;

		if (!(src & 0x80))	/* Wave IRQ pending */
			if (!(wave_ignore & voice_bit) && (int) voice < nr_voices) {	/* Not done yet */
				wave_ignore |= voice_bit;
				do_loop_irq(voice);
			}
		if (!(src & 0x40))	/* Volume IRQ pending */
			if (!(volume_ignore & voice_bit) && (int) voice < nr_voices) {	/* Not done yet */
				volume_ignore |= voice_bit;
				do_volume_irq(voice);
			}
	}
}

void
guswave_dma_irq(void)
{
    u_char   status;

    status = gus_look8(0x41);	/* Get DMA IRQ Status */
    if (status & 0x40)	/* DMA interrupt pending */
	switch (active_device) {
	case GUS_DEV_WAVE:
	    if ((dram_sleep_flag.mode & WK_SLEEP)) {
		dram_sleep_flag.mode = WK_WAKEUP;
		wakeup(dram_sleeper);
	    };
	    break;

	case GUS_DEV_PCM_CONTINUE:	/* Left channel data transferred */
	    gus_transfer_output_block(pcm_current_dev, pcm_current_buf,
			  pcm_current_count, pcm_current_intrflag, 1);
	    break;

	case GUS_DEV_PCM_DONE:	/* Right or mono channel data transferred */
	    if (pcm_qlen < pcm_nblk) {
		int             flag = (1 - dma_active) * 2;	/* 0 or 2 */

		if (pcm_qlen == 0)
		    flag = 1;	/* Underrun */
		dma_active = 0;
		DMAbuf_outputintr(gus_devnum, flag);
	    }
	    break;

		default:;
		}

	status = gus_look8(0x49);	/* Get Sampling IRQ Status */
	if (status & 0x40) {	/* Sampling Irq pending */
		DMAbuf_inputintr(gus_devnum);
	}
}

#ifdef CONFIG_SEQUENCER
/*
 * Timer stuff
 */

static volatile int select_addr, data_addr;
static volatile int curr_timer = 0;

void
gus_timer_command(u_int addr, u_int val)
{
    int             i;

    outb(select_addr, (u_char) (addr & 0xff));

    for (i = 0; i < 2; i++)
	inb(select_addr);

    outb(data_addr, (u_char) (val & 0xff));

    for (i = 0; i < 2; i++)
	inb(select_addr);
}

static void
arm_timer(int timer, u_int interval)
{
    curr_timer = timer;

    if (timer == 1) {
	gus_write8(0x46, 256 - interval);	/* Set counter for timer 1 */
	gus_write8(0x45, 0x04);	/* Enable timer 1 IRQ */
	gus_timer_command(0x04, 0x01);	/* Start timer 1 */
    } else {
	gus_write8(0x47, 256 - interval);	/* Set counter for timer 2 */
	gus_write8(0x45, 0x08);	/* Enable timer 2 IRQ */
	gus_timer_command(0x04, 0x02);	/* Start timer 2 */
    }

    gus_timer_enabled = 0;
}

static u_int
gus_tmr_start(int dev, u_int usecs_per_tick)
{
    int             timer_no, resolution;
    int             divisor;

    if (usecs_per_tick > (256 * 80)) {
	timer_no = 2;
	resolution = 320;	/* usec */
    } else {
	timer_no = 1;
	resolution = 80;/* usec */
    }

    divisor = (usecs_per_tick + (resolution / 2)) / resolution;

    arm_timer(timer_no, divisor);

    return divisor * resolution;
}

static void
gus_tmr_disable(int dev)
{
    gus_write8(0x45, 0);	/* Disable both timers */
    gus_timer_enabled = 0;
}

static void
gus_tmr_restart(int dev)
{
    if (curr_timer == 1)
	gus_write8(0x45, 0x04);	/* Start timer 1 again */
    else
	gus_write8(0x45, 0x08);	/* Start timer 2 again */
}

static struct sound_lowlev_timer gus_tmr =
{
	0,
	gus_tmr_start,
	gus_tmr_disable,
	gus_tmr_restart
};

static void
gus_tmr_install(int io_base)
{
    select_addr = io_base;
    data_addr = io_base + 1;

    sound_timer_init(&gus_tmr, "GUS");
}
#endif
#endif
