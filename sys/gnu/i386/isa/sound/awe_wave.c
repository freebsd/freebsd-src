/*
 * sound/awe_wave.c
 *
 * The low level driver for the AWE32/Sound Blaster 32 wave table synth.
 *   version 0.4.2c; Oct. 7, 1997
 *
 * Copyright (C) 1996,1997 Takashi Iwai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef __FreeBSD__
#  include <gnu/i386/isa/sound/awe_config.h>
#else
#  include "awe_config.h"
#endif

/*----------------------------------------------------------------*/

#ifdef CONFIG_AWE32_SYNTH

#ifdef __FreeBSD__
#  include <gnu/i386/isa/sound/awe_hw.h>
#  include <gnu/i386/isa/sound/awe_version.h>
#  include <gnu/i386/isa/sound/awe_voice.h>
#else
#  include "awe_hw.h"
#  include "awe_version.h"
#  include <linux/awe_voice.h>
#endif

#ifdef AWE_HAS_GUS_COMPATIBILITY
/* include finetune table */

#ifdef __FreeBSD__
#  ifdef AWE_OBSOLETE_VOXWARE
#    define SEQUENCER_C
#  endif
#  include <i386/isa/sound/tuning.h>
#else
#   ifdef AWE_OBSOLETE_VOXWARE
#     include "tuning.h"
#   else
#     include "../tuning.h"
#   endif
#endif

#ifdef linux
#  include <linux/ultrasound.h>
#elif defined(__FreeBSD__)
#  include <machine/ultrasound.h>
#endif

#endif /* AWE_HAS_GUS_COMPATIBILITY */


/*----------------------------------------------------------------
 * debug message
 *----------------------------------------------------------------*/

static int debug_mode = 0;
#ifdef AWE_DEBUG_ON
#define DEBUG(LVL,XXX)	{if (debug_mode > LVL) { XXX; }}
#define ERRMSG(XXX)	{if (debug_mode) { XXX; }}
#define FATALERR(XXX)	XXX
#else
#define DEBUG(LVL,XXX) /**/
#define ERRMSG(XXX)	XXX
#define FATALERR(XXX)	XXX
#endif

/*----------------------------------------------------------------
 * bank and voice record
 *----------------------------------------------------------------*/

/* soundfont record */
typedef struct _sf_list {
	unsigned short sf_id;
	unsigned short type;
	int num_info;		/* current info table index */
	int num_sample;		/* current sample table index */
	int mem_ptr;		/* current word byte pointer */
	int infos;
	int samples;
	/*char name[AWE_PATCH_NAME_LEN];*/
} sf_list;

/* bank record */
typedef struct _awe_voice_list {
	int next;	/* linked list with same sf_id */
	unsigned char bank, instr;	/* preset number information */
	char type, disabled;	/* type=normal/mapped, disabled=boolean */
	awe_voice_info v;	/* voice information */
	int next_instr;	/* preset table list */
	int next_bank;	/* preset table list */
} awe_voice_list;

/* voice list type */
#define V_ST_NORMAL	0
#define V_ST_MAPPED	1

typedef struct _awe_sample_list {
	int next;	/* linked list with same sf_id */
	awe_sample_info v;	/* sample information */
} awe_sample_list;

/* sample and information table */
static int current_sf_id = 0;
static int locked_sf_id = 0;
static int max_sfs;
static sf_list *sflists = NULL;

#define awe_free_mem_ptr() (current_sf_id <= 0 ? 0 : sflists[current_sf_id-1].mem_ptr)
#define awe_free_info() (current_sf_id <= 0 ? 0 : sflists[current_sf_id-1].num_info)
#define awe_free_sample() (current_sf_id <= 0 ? 0 : sflists[current_sf_id-1].num_sample)

static int max_samples;
static awe_sample_list *samples = NULL;

static int max_infos;
static awe_voice_list *infos = NULL;


#define AWE_MAX_PRESETS		256
#define AWE_DEFAULT_PRESET	0
#define AWE_DEFAULT_BANK	0
#define AWE_DEFAULT_DRUM	0
#define AWE_DRUM_BANK		128

#define MAX_LAYERS	AWE_MAX_VOICES

/* preset table index */
static int preset_table[AWE_MAX_PRESETS];

/*----------------------------------------------------------------
 * voice table
 *----------------------------------------------------------------*/

/* effects table */
typedef	struct FX_Rec { /* channel effects */
	unsigned char flags[AWE_FX_END];
	short val[AWE_FX_END];
} FX_Rec;


/* channel parameters */
typedef struct _awe_chan_info {
	int channel;		/* channel number */
	int bank;		/* current tone bank */
	int instr;		/* current program */
	int bender;		/* midi pitchbend (-8192 - 8192) */
	int bender_range;	/* midi bender range (x100) */
	int panning;		/* panning (0-127) */
	int main_vol;		/* channel volume (0-127) */
	int expression_vol;	/* midi expression (0-127) */
	int chan_press;		/* channel pressure */
	int vrec;		/* instrument list */
	int def_vrec;		/* default instrument list */
	int sustained;		/* sustain status in MIDI */
	FX_Rec fx;		/* effects */
	FX_Rec fx_layer[MAX_LAYERS]; /* layer effects */
} awe_chan_info;

/* voice parameters */
typedef struct _voice_info {
	int state;
#define AWE_ST_OFF		(1<<0)	/* no sound */
#define AWE_ST_ON		(1<<1)	/* playing */
#define AWE_ST_STANDBY		(1<<2)	/* stand by for playing */
#define AWE_ST_SUSTAINED	(1<<3)	/* sustained */
#define AWE_ST_MARK		(1<<4)	/* marked for allocation */
#define AWE_ST_DRAM		(1<<5)	/* DRAM read/write */
#define AWE_ST_FM		(1<<6)	/* reserved for FM */
#define AWE_ST_RELEASED		(1<<7)	/* released */

	int ch;			/* midi channel */
	int key;		/* internal key for search */
	int layer;		/* layer number (for channel mode only) */
	int time;		/* allocated time */
	awe_chan_info	*cinfo;	/* channel info */

	int note;		/* midi key (0-127) */
	int velocity;		/* midi velocity (0-127) */
	int sostenuto;		/* sostenuto on/off */
	awe_voice_info *sample;	/* assigned voice */

	/* EMU8000 parameters */
	int apitch;		/* pitch parameter */
	int avol;		/* volume parameter */
	int apan;		/* panning parameter */
} voice_info;

/* voice information */
static voice_info *voices;

#define IS_NO_SOUND(v)	(voices[v].state & (AWE_ST_OFF|AWE_ST_RELEASED|AWE_ST_STANDBY|AWE_ST_SUSTAINED))
#define IS_NO_EFFECT(v)	(voices[v].state != AWE_ST_ON)
#define IS_PLAYING(v)	(voices[v].state & (AWE_ST_ON|AWE_ST_SUSTAINED|AWE_ST_RELEASED))
#define IS_EMPTY(v)	(voices[v].state & (AWE_ST_OFF|AWE_ST_MARK|AWE_ST_DRAM|AWE_ST_FM))


/* MIDI channel effects information (for hw control) */
static awe_chan_info *channels;


/*----------------------------------------------------------------
 * global variables
 *----------------------------------------------------------------*/

#ifndef AWE_DEFAULT_BASE_ADDR
#define AWE_DEFAULT_BASE_ADDR	0	/* autodetect */
#endif

#ifndef AWE_DEFAULT_MEM_SIZE
#define AWE_DEFAULT_MEM_SIZE	0	/* autodetect */
#endif

/* awe32 base address (overwritten at initialization) */
static int awe_base = AWE_DEFAULT_BASE_ADDR;
/* memory byte size */
static int awe_mem_size = AWE_DEFAULT_MEM_SIZE;
/* DRAM start offset */
static int awe_mem_start = AWE_DRAM_OFFSET;

/* maximum channels for playing */
static int awe_max_voices = AWE_MAX_VOICES;

static int patch_opened = 0;		/* sample already loaded? */

static int reverb_mode = 4;		/* reverb mode */
static int chorus_mode = 2;		/* chorus mode */
static short init_atten = AWE_DEFAULT_ATTENUATION; /* 12dB below */

static int awe_present = FALSE;		/* awe device present? */
static int awe_busy = FALSE;		/* awe device opened? */

#define DEFAULT_DRUM_FLAGS	((1 << 9) | (1 << 25))
#define IS_DRUM_CHANNEL(c)	(drum_flags & (1 << (c)))
#define DRUM_CHANNEL_ON(c)	(drum_flags |= (1 << (c)))
#define DRUM_CHANNEL_OFF(c)	(drum_flags &= ~(1 << (c)))
static unsigned int drum_flags = DEFAULT_DRUM_FLAGS; /* channel flags */

static int playing_mode = AWE_PLAY_INDIRECT;
#define SINGLE_LAYER_MODE()	(playing_mode == AWE_PLAY_INDIRECT || playing_mode == AWE_PLAY_DIRECT)
#define MULTI_LAYER_MODE()	(playing_mode == AWE_PLAY_MULTI || playing_mode == AWE_PLAY_MULTI2)

static int current_alloc_time = 0;	/* voice allocation index for channel mode */

static struct MiscModeDef {
	int value;
	int init_each_time;
} misc_modes_default[AWE_MD_END] = {
	{0,0}, {0,0}, /* <-- not used */
	{AWE_VERSION_NUMBER, FALSE},
	{TRUE, TRUE}, /* exclusive */
	{TRUE, TRUE}, /* realpan */
	{AWE_DEFAULT_BANK, TRUE}, /* gusbank */
	{FALSE, TRUE}, /* keep effect */
	{AWE_DEFAULT_ATTENUATION, FALSE}, /* zero_atten */
	{FALSE, TRUE}, /* chn_prior */
	{AWE_DEFAULT_MOD_SENSE, TRUE}, /* modwheel sense */
	{AWE_DEFAULT_PRESET, TRUE}, /* def_preset */
	{AWE_DEFAULT_BANK, TRUE}, /* def_bank */
	{AWE_DEFAULT_DRUM, TRUE}, /* def_drum */
	{FALSE, TRUE}, /* toggle_drum_bank */
};

static int misc_modes[AWE_MD_END];

static int awe_bass_level = 5;
static int awe_treble_level = 9;


static struct synth_info awe_info = {
	"AWE32 Synth",		/* name */
	0,			/* device */
	SYNTH_TYPE_SAMPLE,	/* synth_type */
	SAMPLE_TYPE_AWE32,	/* synth_subtype */
	0,			/* perc_mode (obsolete) */
	AWE_MAX_VOICES,		/* nr_voices */
	0,			/* nr_drums (obsolete) */
	AWE_MAX_INFOS		/* instr_bank_size */
};


static struct voice_alloc_info *voice_alloc;	/* set at initialization */


/*----------------------------------------------------------------
 * function prototypes
 *----------------------------------------------------------------*/

#if defined(linux) && !defined(AWE_OBSOLETE_VOXWARE)
static int awe_check_port(void);
static void awe_request_region(void);
static void awe_release_region(void);
#endif

static void awe_reset_samples(void);
/* emu8000 chip i/o access */
static void awe_poke(unsigned short cmd, unsigned short port, unsigned short data);
static void awe_poke_dw(unsigned short cmd, unsigned short port, unsigned int data);
static unsigned short awe_peek(unsigned short cmd, unsigned short port);
static unsigned int awe_peek_dw(unsigned short cmd, unsigned short port);
static void awe_wait(unsigned short delay);

/* initialize emu8000 chip */
static void awe_initialize(void);

/* set voice parameters */
static void awe_init_misc_modes(int init_all);
static void awe_init_voice_info(awe_voice_info *vp);
static void awe_init_voice_parm(awe_voice_parm *pp);
#ifdef AWE_HAS_GUS_COMPATIBILITY
static int freq_to_note(int freq);
static int calc_rate_offset(int Hz);
/*static int calc_parm_delay(int msec);*/
static int calc_parm_hold(int msec);
static int calc_parm_attack(int msec);
static int calc_parm_decay(int msec);
static int calc_parm_search(int msec, short *table);
#endif

/* turn on/off note */
static void awe_note_on(int voice);
static void awe_note_off(int voice);
static void awe_terminate(int voice);
static void awe_exclusive_off(int voice);
static void awe_note_off_all(int do_sustain);

/* calculate voice parameters */
typedef void (*fx_affect_func)(int voice, int forced);
static void awe_set_pitch(int voice, int forced);
static void awe_set_voice_pitch(int voice, int forced);
static void awe_set_volume(int voice, int forced);
static void awe_set_voice_vol(int voice, int forced);
static void awe_set_pan(int voice, int forced);
static void awe_fx_fmmod(int voice, int forced);
static void awe_fx_tremfrq(int voice, int forced);
static void awe_fx_fm2frq2(int voice, int forced);
static void awe_fx_filterQ(int voice, int forced);
static void awe_calc_pitch(int voice);
#ifdef AWE_HAS_GUS_COMPATIBILITY
static void awe_calc_pitch_from_freq(int voice, int freq);
#endif
static void awe_calc_volume(int voice);
static void awe_voice_init(int voice, int init_all);
static void awe_channel_init(int ch, int init_all);
static void awe_fx_init(int ch);

/* sequencer interface */
static int awe_open(int dev, int mode);
static void awe_close(int dev);
static int awe_ioctl(int dev, unsigned int cmd, caddr_t arg);
static int awe_kill_note(int dev, int voice, int note, int velocity);
static int awe_start_note(int dev, int v, int note_num, int volume);
static int awe_set_instr(int dev, int voice, int instr_no);
static int awe_set_instr_2(int dev, int voice, int instr_no);
static void awe_reset(int dev);
static void awe_hw_control(int dev, unsigned char *event);
static int awe_load_patch(int dev, int format, const char *addr,
			  int offs, int count, int pmgr_flag);
static void awe_aftertouch(int dev, int voice, int pressure);
static void awe_controller(int dev, int voice, int ctrl_num, int value);
static void awe_panning(int dev, int voice, int value);
static void awe_volume_method(int dev, int mode);
#ifndef AWE_NO_PATCHMGR
static int awe_patchmgr(int dev, struct patmgr_info *rec);
#endif
static void awe_bender(int dev, int voice, int value);
static int awe_alloc(int dev, int chn, int note, struct voice_alloc_info *alloc);
static void awe_setup_voice(int dev, int voice, int chn);

/* hardware controls */
#ifdef AWE_HAS_GUS_COMPATIBILITY
static void awe_hw_gus_control(int dev, int cmd, unsigned char *event);
#endif
static void awe_hw_awe_control(int dev, int cmd, unsigned char *event);
static void awe_voice_change(int voice, fx_affect_func func);
static void awe_sostenuto_on(int voice, int forced);
static void awe_sustain_off(int voice, int forced);
static void awe_terminate_and_init(int voice, int forced);

/* voice search */
static int awe_search_instr(int bank, int preset);
static int awe_search_multi_voices(int rec, int note, int velocity, awe_voice_info **vlist);
static void awe_alloc_multi_voices(int ch, int note, int velocity, int key);
static void awe_alloc_one_voice(int voice, int note, int velocity);
static int awe_clear_voice(void);

/* load / remove patches */
static int awe_open_patch(awe_patch_info *patch, const char *addr, int count);
static int awe_close_patch(awe_patch_info *patch, const char *addr, int count);
static int awe_unload_patch(awe_patch_info *patch, const char *addr, int count);
static int awe_load_info(awe_patch_info *patch, const char *addr, int count);
static int awe_load_data(awe_patch_info *patch, const char *addr, int count);
static int awe_replace_data(awe_patch_info *patch, const char *addr, int count);
static int awe_load_map(awe_patch_info *patch, const char *addr, int count);
#ifdef AWE_HAS_GUS_COMPATIBILITY
static int awe_load_guspatch(const char *addr, int offs, int size, int pmgr_flag);
#endif
static int check_patch_opened(int type, char *name);
static int awe_write_wave_data(const char *addr, int offset, awe_sample_info *sp, int channels);
static void add_sf_info(int rec);
static void add_sf_sample(int rec);
static void purge_old_list(int rec, int next);
static void add_info_list(int rec);
static void awe_remove_samples(int sf_id);
static void rebuild_preset_list(void);
static short awe_set_sample(awe_voice_info *vp);

/* lowlevel functions */
static void awe_init_audio(void);
static void awe_init_dma(void);
static void awe_init_array(void);
static void awe_send_array(unsigned short *data);
static void awe_tweak_voice(int voice);
static void awe_tweak(void);
static void awe_init_fm(void);
static int awe_open_dram_for_write(int offset, int channels);
static void awe_open_dram_for_check(void);
static void awe_close_dram(void);
static void awe_write_dram(unsigned short c);
static int awe_detect_base(int addr);
static int awe_detect(void);
static int awe_check_dram(void);
static int awe_load_chorus_fx(awe_patch_info *patch, const char *addr, int count);
static void awe_set_chorus_mode(int mode);
static int awe_load_reverb_fx(awe_patch_info *patch, const char *addr, int count);
static void awe_set_reverb_mode(int mode);
static void awe_equalizer(int bass, int treble);
#ifdef CONFIG_AWE32_MIXER
static int awe_mixer_ioctl(int dev, unsigned int cmd, caddr_t arg);
#endif

/* define macros for compatibility */
#ifdef __FreeBSD__
#  include <gnu/i386/isa/sound/awe_compat.h>
#else
#  include "awe_compat.h"
#endif

/*----------------------------------------------------------------
 * synth operation table
 *----------------------------------------------------------------*/

static struct synth_operations awe_operations =
{
#ifdef AWE_OSS38
	"EMU8K",
#endif
	&awe_info,
	0,
	SYNTH_TYPE_SAMPLE,
	SAMPLE_TYPE_AWE32,
	awe_open,
	awe_close,
	awe_ioctl,
	awe_kill_note,
	awe_start_note,
	awe_set_instr_2,
	awe_reset,
	awe_hw_control,
	awe_load_patch,
	awe_aftertouch,
	awe_controller,
	awe_panning,
	awe_volume_method,
#ifndef AWE_NO_PATCHMGR
	awe_patchmgr,
#endif
	awe_bender,
	awe_alloc,
	awe_setup_voice
};

#ifdef CONFIG_AWE32_MIXER
static struct mixer_operations awe_mixer_operations = {
#ifndef __FreeBSD__
	"AWE32",
#endif
	"AWE32 Equalizer",
	awe_mixer_ioctl,
};
#endif


/*================================================================
 * attach / unload interface
 *================================================================*/

#ifdef AWE_OBSOLETE_VOXWARE
#define ATTACH_DECL	static
#else
#define ATTACH_DECL	/**/
#endif

#if defined(__FreeBSD__) && !defined(AWE_OBSOLETE_VOXWARE)
#  define ATTACH_RET
void attach_awe(struct address_info *hw_config)
#else
#  define ATTACH_RET ret
ATTACH_DECL
int attach_awe(void)
#endif
{
    int ret = 0;

	/* check presence of AWE32 card */
	if (! awe_detect()) {
		printk("AWE32: not detected\n");
		return ATTACH_RET;
	}

	/* check AWE32 ports are available */
	if (awe_check_port()) {
		printk("AWE32: I/O area already used.\n");
		return ATTACH_RET;
	}

	/* set buffers to NULL */
	voices = NULL;
	channels = NULL;
	sflists = NULL;
	samples = NULL;
	infos = NULL;

	/* voice & channel info */
	voices = (voice_info*)my_malloc(AWE_MAX_VOICES * sizeof(voice_info));
	channels = (awe_chan_info*)my_malloc(AWE_MAX_CHANNELS * sizeof(awe_chan_info));

	if (voices == NULL || channels == NULL) {
		my_free(voices);
		my_free(channels);
		printk("AWE32: can't allocate sample tables\n");
		return ATTACH_RET;
	}

	/* allocate sample tables */
	INIT_TABLE(sflists, max_sfs, AWE_MAX_SF_LISTS, sf_list);
	INIT_TABLE(samples, max_samples, AWE_MAX_SAMPLES, awe_sample_list);
	INIT_TABLE(infos, max_infos, AWE_MAX_INFOS, awe_voice_list);

	if (num_synths >= MAX_SYNTH_DEV)
		printk("AWE32 Error: too many synthesizers\n");
	else {
		voice_alloc = &awe_operations.alloc;
		voice_alloc->max_voice = awe_max_voices;
		synth_devs[num_synths++] = &awe_operations;
	}

#ifdef CONFIG_AWE32_MIXER
	if (num_mixers < MAX_MIXER_DEV) {
		mixer_devs[num_mixers++] = &awe_mixer_operations;
	}
#endif

	/* reserve I/O ports for awedrv */
	awe_request_region();

	/* clear all samples */
	awe_reset_samples();

	/* intialize AWE32 hardware */
	awe_initialize();

	sprintf(awe_info.name, "AWE32-%s (RAM%dk)",
		AWEDRV_VERSION, awe_mem_size/1024);
#ifdef __FreeBSD__
	printk("awe0: <SoundBlaster EMU8000 MIDI (RAM%dk)>", awe_mem_size/1024);
#elif defined(AWE_DEBUG_ON)
	printk("%s\n", awe_info.name);
#endif

	/* set default values */
	awe_init_misc_modes(TRUE);

	/* set reverb & chorus modes */
	awe_set_reverb_mode(reverb_mode);
	awe_set_chorus_mode(chorus_mode);

	awe_present = TRUE;

    ret = 1;
    return ATTACH_RET;
}


#ifdef AWE_DYNAMIC_BUFFER
static void free_tables(void)
{
	my_free(sflists);
	sflists = NULL; max_sfs = 0;
	my_free(samples);
	samples = NULL; max_samples = 0;
	my_free(infos);
	infos = NULL; max_infos = 0;
}
#else
#define free_buffers() /**/
#endif


#ifdef linux
ATTACH_DECL
void unload_awe(void)
{
	if (awe_present) {
		awe_reset_samples();
		awe_release_region();
		my_free(voices);
		my_free(channels);
		free_tables();
		awe_present = FALSE;
	}
}
#endif


/*----------------------------------------------------------------
 * old type interface
 *----------------------------------------------------------------*/

#ifdef AWE_OBSOLETE_VOXWARE

#ifdef __FreeBSD__
long attach_awe_obsolete(long mem_start, struct address_info *hw_config)
#else
int attach_awe_obsolete(int mem_start, struct address_info *hw_config)
#endif
{
	my_malloc_init(mem_start);
	if (! attach_awe())
		return 0;
	return my_malloc_memptr();
}

int probe_awe_obsolete(struct address_info *hw_config)
{
	return 1;
	/*return awe_detect();*/
}

#else
#if defined(__FreeBSD__ )
int probe_awe(struct address_info *hw_config)
{
	return 1;
}
#endif
#endif /* AWE_OBSOLETE_VOXWARE */


/*================================================================
 * clear sample tables 
 *================================================================*/

static void
awe_reset_samples(void)
{
	int i;

	/* free all bank tables */
	for (i = 0; i < AWE_MAX_PRESETS; i++)
		preset_table[i] = -1;

	free_tables();

	current_sf_id = 0;
	locked_sf_id = 0;
	patch_opened = 0;
}


/*================================================================
 * EMU register access
 *================================================================*/

/* select a given AWE32 pointer */
static int awe_cur_cmd = -1;
#define awe_set_cmd(cmd) \
if (awe_cur_cmd != cmd) { OUTW(cmd, awe_base + 0x802); awe_cur_cmd = cmd; }
#define awe_port(port)		(awe_base - 0x620 + port)

/* write 16bit data */
INLINE static void
awe_poke(unsigned short cmd, unsigned short port, unsigned short data)
{
	awe_set_cmd(cmd);
	OUTW(data, awe_port(port));
}

/* write 32bit data */
INLINE static void
awe_poke_dw(unsigned short cmd, unsigned short port, unsigned int data)
{
	awe_set_cmd(cmd);
	OUTW(data, awe_port(port));		/* write lower 16 bits */
	OUTW(data >> 16, awe_port(port)+2);	/* write higher 16 bits */
}

/* read 16bit data */
INLINE static unsigned short
awe_peek(unsigned short cmd, unsigned short port)
{
	unsigned short k;
	awe_set_cmd(cmd);
	k = inw(awe_port(port));
	return k;
}

/* read 32bit data */
INLINE static unsigned int
awe_peek_dw(unsigned short cmd, unsigned short port)
{
	unsigned int k1, k2;
	awe_set_cmd(cmd);
	k1 = inw(awe_port(port));
	k2 = inw(awe_port(port)+2);
	k1 |= k2 << 16;
	return k1;
}

/* wait delay number of AWE32 44100Hz clocks */
static void
awe_wait(unsigned short delay)
{
	unsigned short clock, target;
	unsigned short port = awe_port(AWE_WC_Port);
	int counter;
  
	/* sample counter */
	awe_set_cmd(AWE_WC_Cmd);
	clock = (unsigned short)inw(port);
	target = clock + delay;
	counter = 0;
	if (target < clock) {
		for (; (unsigned short)inw(port) > target; counter++)
			if (counter > 65536)
				break;
	}
	for (; (unsigned short)inw(port) < target; counter++)
		if (counter > 65536)
			break;
}

/* write a word data */
INLINE static void
awe_write_dram(unsigned short c)
{
	awe_poke(AWE_SMLD, c);
}


#if defined(linux) && !defined(AWE_OBSOLETE_VOXWARE)

/*================================================================
 * port check / request
 *  0x620-622, 0xA20-A22, 0xE20-E22
 *================================================================*/

static int
awe_check_port(void)
{
	return (check_region(awe_port(Data0), 4) ||
		check_region(awe_port(Data1), 4) ||
		check_region(awe_port(Data3), 4));
}

static void
awe_request_region(void)
{
	request_region(awe_port(Data0), 4, "sound driver (AWE32)");
	request_region(awe_port(Data1), 4, "sound driver (AWE32)");
	request_region(awe_port(Data3), 4, "sound driver (AWE32)");
}

static void
awe_release_region(void)
{
	release_region(awe_port(Data0), 4);
	release_region(awe_port(Data1), 4);
	release_region(awe_port(Data3), 4);
}

#endif /* !AWE_OBSOLETE_VOXWARE */


/*================================================================
 * AWE32 initialization
 *================================================================*/
static void
awe_initialize(void)
{
	DEBUG(0,printk("AWE32: initializing..\n"));

	/* initialize hardware configuration */
	awe_poke(AWE_HWCF1, 0x0059);
	awe_poke(AWE_HWCF2, 0x0020);

	/* disable audio; this seems to reduce a clicking noise a bit.. */
	awe_poke(AWE_HWCF3, 0);

	/* initialize audio channels */
	awe_init_audio();

	/* initialize DMA */
	awe_init_dma();

	/* initialize init array */
	awe_init_array();

	/* check DRAM memory size */
	awe_mem_size = awe_check_dram();

	/* initialize the FM section of the AWE32 */
	awe_init_fm();

	/* set up voice envelopes */
	awe_tweak();

	/* enable audio */
	awe_poke(AWE_HWCF3, 0x0004);

	/* set equalizer */
	awe_equalizer(5, 9);
}


/*================================================================
 * AWE32 voice parameters
 *================================================================*/

/* initialize voice_info record */
static void
awe_init_voice_info(awe_voice_info *vp)
{
	vp->sf_id = 0; /* normal mode */
	vp->sample = 0;
	vp->rate_offset = 0;

	vp->start = 0;
	vp->end = 0;
	vp->loopstart = 0;
	vp->loopend = 0;
	vp->mode = 0;
	vp->root = 60;
	vp->tune = 0;
	vp->low = 0;
	vp->high = 127;
	vp->vellow = 0;
	vp->velhigh = 127;

	vp->fixkey = -1;
	vp->fixvel = -1;
	vp->fixpan = -1;
	vp->pan = -1;

	vp->exclusiveClass = 0;
	vp->amplitude = 127;
	vp->attenuation = 0;
	vp->scaleTuning = 100;

	awe_init_voice_parm(&vp->parm);
}

/* initialize voice_parm record:
 * Env1/2: delay=0, attack=0, hold=0, sustain=0, decay=0, release=0.
 * Vibrato and Tremolo effects are zero.
 * Cutoff is maximum.
 * Chorus and Reverb effects are zero.
 */
static void
awe_init_voice_parm(awe_voice_parm *pp)
{
	pp->moddelay = 0x8000;
	pp->modatkhld = 0x7f7f;
	pp->moddcysus = 0x7f7f;
	pp->modrelease = 0x807f;
	pp->modkeyhold = 0;
	pp->modkeydecay = 0;

	pp->voldelay = 0x8000;
	pp->volatkhld = 0x7f7f;
	pp->voldcysus = 0x7f7f;
	pp->volrelease = 0x807f;
	pp->volkeyhold = 0;
	pp->volkeydecay = 0;

	pp->lfo1delay = 0x8000;
	pp->lfo2delay = 0x8000;
	pp->pefe = 0;

	pp->fmmod = 0;
	pp->tremfrq = 0;
	pp->fm2frq2 = 0;

	pp->cutoff = 0xff;
	pp->filterQ = 0;

	pp->chorus = 0;
	pp->reverb = 0;
}	


#ifdef AWE_HAS_GUS_COMPATIBILITY

/* convert frequency mHz to abstract cents (= midi key * 100) */
static int
freq_to_note(int mHz)
{
	/* abscents = log(mHz/8176) / log(2) * 1200 */
	unsigned int max_val = (unsigned int)0xffffffff / 10000;
	int i, times;
	unsigned int base;
	unsigned int freq;
	int note, tune;

	if (mHz == 0)
		return 0;
	if (mHz < 0)
		return 12799; /* maximum */

	freq = mHz;
	note = 0;
	for (base = 8176 * 2; freq >= base; base *= 2) {
		note += 12;
		if (note >= 128) /* over maximum */
			return 12799;
	}
	base /= 2;

	/* to avoid overflow... */
	times = 10000;
	while (freq > max_val) {
		max_val *= 10;
		times /= 10;
		base /= 10;
	}

	freq = freq * times / base;
	for (i = 0; i < 12; i++) {
		if (freq < semitone_tuning[i+1])
			break;
		note++;
	}

	tune = 0;
	freq = freq * 10000 / semitone_tuning[i];
	for (i = 0; i < 100; i++) {
		if (freq < cent_tuning[i+1])
			break;
		tune++;
	}

	return note * 100 + tune;
}


/* convert Hz to AWE32 rate offset:
 * sample pitch offset for the specified sample rate
 * rate=44100 is no offset, each 4096 is 1 octave (twice).
 * eg, when rate is 22050, this offset becomes -4096.
 */
static int
calc_rate_offset(int Hz)
{
	/* offset = log(Hz / 44100) / log(2) * 4096 */
	int freq, base, i;

	/* maybe smaller than max (44100Hz) */
	if (Hz <= 0 || Hz >= 44100) return 0;

	base = 0;
	for (freq = Hz * 2; freq < 44100; freq *= 2)
		base++;
	base *= 1200;

	freq = 44100 * 10000 / (freq/2);
	for (i = 0; i < 12; i++) {
		if (freq < semitone_tuning[i+1])
			break;
		base += 100;
	}
	freq = freq * 10000 / semitone_tuning[i];
	for (i = 0; i < 100; i++) {
		if (freq < cent_tuning[i+1])
			break;
		base++;
	}
	return -base * 4096 / 1200;
}


/*----------------------------------------------------------------
 * convert envelope time parameter to AWE32 raw parameter
 *----------------------------------------------------------------*/

/* attack & decay/release time table (msec) */
static short attack_time_tbl[128] = {
32767, 11878, 5939, 3959, 2969, 2375, 1979, 1696, 1484, 1319, 1187, 1079, 989, 913, 848, 791, 742,
 698, 659, 625, 593, 565, 539, 516, 494, 475, 456, 439, 424, 409, 395, 383, 371,
 359, 344, 330, 316, 302, 290, 277, 266, 255, 244, 233, 224, 214, 205, 196, 188,
 180, 173, 165, 158, 152, 145, 139, 133, 127, 122, 117, 112, 107, 103, 98, 94,
 90, 86, 83, 79, 76, 73, 69, 67, 64, 61, 58, 56, 54, 51, 49, 47,
 45, 43, 41, 39, 38, 36, 35, 33, 32, 30, 29, 28, 27, 25, 24, 23,
 22, 21, 20, 20, 19, 18, 17, 16, 16, 15, 14, 14, 13, 13, 12, 11,
 11, 10, 10, 10, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 0,
};

static short decay_time_tbl[128] = {
32767, 32766, 4589, 4400, 4219, 4045, 3879, 3719, 3566, 3419, 3279, 3144, 3014, 2890, 2771, 2657,
 2548, 2443, 2343, 2246, 2154, 2065, 1980, 1899, 1820, 1746, 1674, 1605, 1539, 1475, 1415, 1356,
 1301, 1247, 1196, 1146, 1099, 1054, 1011, 969, 929, 891, 854, 819, 785, 753, 722, 692,
 664, 636, 610, 585, 561, 538, 516, 494, 474, 455, 436, 418, 401, 384, 368, 353,
 339, 325, 311, 298, 286, 274, 263, 252, 242, 232, 222, 213, 204, 196, 188, 180,
 173, 166, 159, 152, 146, 140, 134, 129, 123, 118, 113, 109, 104, 100, 96, 92,
 88, 84, 81, 77, 74, 71, 68, 65, 63, 60, 58, 55, 53, 51, 49, 47,
 45, 43, 41, 39, 38, 36, 35, 33, 32, 30, 29, 28, 27, 26, 25, 24,
};

/*
static int
calc_parm_delay(int msec)
{
	return (0x8000 - msec * 1000 / 725);
}
*/

/* delay time = 0x8000 - msec/92 */
static int
calc_parm_hold(int msec)
{
	int val = (0x7f * 92 - msec) / 92;
	if (val < 1) val = 1;
	if (val > 127) val = 127;
	return val;
}

/* attack time: search from time table */
static int
calc_parm_attack(int msec)
{
	return calc_parm_search(msec, attack_time_tbl);
}

/* decay/release time: search from time table */
static int
calc_parm_decay(int msec)
{
	return calc_parm_search(msec, decay_time_tbl);
}

/* search an index for specified time from given time table */
static int
calc_parm_search(int msec, short *table)
{
	int left = 1, right = 127, mid;
	while (left < right) {
		mid = (left + right) / 2;
		if (msec < (int)table[mid])
			left = mid + 1;
		else
			right = mid;
	}
	return left;
}
#endif /* AWE_HAS_GUS_COMPATIBILITY */


/*================================================================
 * effects table
 *================================================================*/

/* set an effect value */
#define FX_FLAG_OFF	0
#define FX_FLAG_SET	1
#define FX_FLAG_ADD	2

#define FX_SET(rec,type,value) \
	((rec)->flags[type] = FX_FLAG_SET, (rec)->val[type] = (value))
#define FX_ADD(rec,type,value) \
	((rec)->flags[type] = FX_FLAG_ADD, (rec)->val[type] = (value))
#define FX_UNSET(rec,type) \
	((rec)->flags[type] = FX_FLAG_OFF, (rec)->val[type] = 0)

/* check the effect value is set */
#define FX_ON(rec,type)	((rec)->flags[type])

#define PARM_BYTE	0
#define PARM_WORD	1

static struct PARM_DEFS {
	int type;	/* byte or word */
	int low, high;	/* value range */
	fx_affect_func realtime;	/* realtime paramater change */
} parm_defs[] = {
	{PARM_WORD, 0, 0x8000, NULL},	/* env1 delay */
	{PARM_BYTE, 1, 0x7f, NULL},	/* env1 attack */
	{PARM_BYTE, 0, 0x7e, NULL},	/* env1 hold */
	{PARM_BYTE, 1, 0x7f, NULL},	/* env1 decay */
	{PARM_BYTE, 1, 0x7f, NULL},	/* env1 release */
	{PARM_BYTE, 0, 0x7f, NULL},	/* env1 sustain */
	{PARM_BYTE, 0, 0xff, NULL},	/* env1 pitch */
	{PARM_BYTE, 0, 0xff, NULL},	/* env1 cutoff */

	{PARM_WORD, 0, 0x8000, NULL},	/* env2 delay */
	{PARM_BYTE, 1, 0x7f, NULL},	/* env2 attack */
	{PARM_BYTE, 0, 0x7e, NULL},	/* env2 hold */
	{PARM_BYTE, 1, 0x7f, NULL},	/* env2 decay */
	{PARM_BYTE, 1, 0x7f, NULL},	/* env2 release */
	{PARM_BYTE, 0, 0x7f, NULL},	/* env2 sustain */

	{PARM_WORD, 0, 0x8000, NULL},	/* lfo1 delay */
	{PARM_BYTE, 0, 0xff, awe_fx_tremfrq},	/* lfo1 freq */
	{PARM_BYTE, 0, 0x7f, awe_fx_tremfrq},	/* lfo1 volume (positive only)*/
	{PARM_BYTE, 0, 0x7f, awe_fx_fmmod},	/* lfo1 pitch (positive only)*/
	{PARM_BYTE, 0, 0xff, awe_fx_fmmod},	/* lfo1 cutoff (positive only)*/

	{PARM_WORD, 0, 0x8000, NULL},	/* lfo2 delay */
	{PARM_BYTE, 0, 0xff, awe_fx_fm2frq2},	/* lfo2 freq */
	{PARM_BYTE, 0, 0x7f, awe_fx_fm2frq2},	/* lfo2 pitch (positive only)*/

	{PARM_WORD, 0, 0xffff, awe_set_voice_pitch},	/* initial pitch */
	{PARM_BYTE, 0, 0xff, NULL},	/* chorus */
	{PARM_BYTE, 0, 0xff, NULL},	/* reverb */
	{PARM_BYTE, 0, 0xff, awe_set_volume},	/* initial cutoff */
	{PARM_BYTE, 0, 15, awe_fx_filterQ},	/* initial resonance */

	{PARM_WORD, 0, 0xffff, NULL},	/* sample start */
	{PARM_WORD, 0, 0xffff, NULL},	/* loop start */
	{PARM_WORD, 0, 0xffff, NULL},	/* loop end */
	{PARM_WORD, 0, 0xffff, NULL},	/* coarse sample start */
	{PARM_WORD, 0, 0xffff, NULL},	/* coarse loop start */
	{PARM_WORD, 0, 0xffff, NULL},	/* coarse loop end */
	{PARM_BYTE, 0, 0xff, awe_set_volume},	/* initial attenuation */
};


static unsigned char
FX_BYTE(FX_Rec *rec, FX_Rec *lay, int type, unsigned char value)
{
	int effect = 0;
	int on = 0;
	if (lay && (on = FX_ON(lay, type)) != 0)
		effect = lay->val[type];
	if (!on && (on = FX_ON(rec, type)) != 0)
		effect = rec->val[type];
	if (on == FX_FLAG_ADD)
		effect += (int)value;
	if (on) {
		if (effect < parm_defs[type].low)
			effect = parm_defs[type].low;
		else if (effect > parm_defs[type].high)
			effect = parm_defs[type].high;
		return (unsigned char)effect;
	}
	return value;
}

/* get word effect value */
static unsigned short
FX_WORD(FX_Rec *rec, FX_Rec *lay, int type, unsigned short value)
{
	int effect = 0;
	int on = 0;
	if (lay && (on = FX_ON(lay, type)) != 0)
		effect = lay->val[type];
	if (!on && (on = FX_ON(rec, type)) != 0)
		effect = rec->val[type];
	if (on == FX_FLAG_ADD)
		effect += (int)value;
	if (on) {
		if (effect < parm_defs[type].low)
			effect = parm_defs[type].low;
		else if (effect > parm_defs[type].high)
			effect = parm_defs[type].high;
		return (unsigned short)effect;
	}
	return value;
}

/* get word (upper=type1/lower=type2) effect value */
static unsigned short
FX_COMB(FX_Rec *rec, FX_Rec *lay, int type1, int type2, unsigned short value)
{
	unsigned short tmp;
	tmp = FX_BYTE(rec, lay, type1, (unsigned char)(value >> 8));
	tmp <<= 8;
	tmp |= FX_BYTE(rec, lay, type2, (unsigned char)(value & 0xff));
	return tmp;
}

/* address offset */
static int
FX_OFFSET(FX_Rec *rec, FX_Rec *lay, int lo, int hi, int mode)
{
	int addr = 0;
	if (lay && FX_ON(lay, hi))
		addr = (short)lay->val[hi];
	else if (FX_ON(rec, hi))
		addr = (short)rec->val[hi];
	addr = addr << 15;
	if (lay && FX_ON(lay, lo))
		addr += (short)lay->val[lo];
	else if (FX_ON(rec, lo))
		addr += (short)rec->val[lo];
	if (!(mode & AWE_SAMPLE_8BITS))
		addr /= 2;
	return addr;
}


/*================================================================
 * turn on/off sample
 *================================================================*/

static void
awe_note_on(int voice)
{
	unsigned int temp;
	int addr;
	awe_voice_info *vp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	/* A voice sample must assigned before calling */
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;

	/* channel to be silent and idle */
	awe_poke(AWE_DCYSUSV(voice), 0x0080);
	awe_poke(AWE_VTFT(voice), 0);
	awe_poke(AWE_CVCF(voice), 0);
	awe_poke(AWE_PTRX(voice), 0);
	awe_poke(AWE_CPF(voice), 0);

	/* modulation & volume envelope */
	awe_poke(AWE_ENVVAL(voice),
		 FX_WORD(fx, fx_lay, AWE_FX_ENV1_DELAY, vp->parm.moddelay));
	awe_poke(AWE_ATKHLD(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_ENV1_HOLD, AWE_FX_ENV1_ATTACK,
			 vp->parm.modatkhld));
	awe_poke(AWE_DCYSUS(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_ENV1_SUSTAIN, AWE_FX_ENV1_DECAY,
			  vp->parm.moddcysus));
	awe_poke(AWE_ENVVOL(voice),
		 FX_WORD(fx, fx_lay, AWE_FX_ENV2_DELAY, vp->parm.voldelay));
	awe_poke(AWE_ATKHLDV(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_ENV2_HOLD, AWE_FX_ENV2_ATTACK,
			 vp->parm.volatkhld));
	/* decay/sustain parameter for volume envelope must be set at last */

	/* pitch offset */
	awe_set_pitch(voice, TRUE);

	/* cutoff and volume */
	awe_set_volume(voice, TRUE);

	/* modulation envelope heights */
	awe_poke(AWE_PEFE(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_ENV1_PITCH, AWE_FX_ENV1_CUTOFF,
			 vp->parm.pefe));

	/* lfo1/2 delay */
	awe_poke(AWE_LFO1VAL(voice),
		 FX_WORD(fx, fx_lay, AWE_FX_LFO1_DELAY, vp->parm.lfo1delay));
	awe_poke(AWE_LFO2VAL(voice),
		 FX_WORD(fx, fx_lay, AWE_FX_LFO2_DELAY, vp->parm.lfo2delay));

	/* lfo1 pitch & cutoff shift */
	awe_fx_fmmod(voice, TRUE);
	/* lfo1 volume & freq */
	awe_fx_tremfrq(voice, TRUE);
	/* lfo2 pitch & freq */
	awe_fx_fm2frq2(voice, TRUE);
	/* pan & loop start */
	awe_set_pan(voice, TRUE);

	/* chorus & loop end (chorus 8bit, MSB) */
	addr = vp->loopend - 1;
	addr += FX_OFFSET(fx, fx_lay, AWE_FX_LOOP_END,
			  AWE_FX_COARSE_LOOP_END, vp->mode);
	temp = FX_BYTE(fx, fx_lay, AWE_FX_CHORUS, vp->parm.chorus);
	temp = (temp <<24) | (unsigned int)addr;
	awe_poke_dw(AWE_CSL(voice), temp);
	DEBUG(4,printk("AWE32: [-- loopend=%x/%x]\n", vp->loopend, addr));

	/* Q & current address (Q 4bit value, MSB) */
	addr = vp->start - 1;
	addr += FX_OFFSET(fx, fx_lay, AWE_FX_SAMPLE_START,
			  AWE_FX_COARSE_SAMPLE_START, vp->mode);
	temp = FX_BYTE(fx, fx_lay, AWE_FX_FILTERQ, vp->parm.filterQ);
	temp = (temp<<28) | (unsigned int)addr;
	awe_poke_dw(AWE_CCCA(voice), temp);
	DEBUG(4,printk("AWE32: [-- startaddr=%x/%x]\n", vp->start, addr));

	/* reset volume */
	awe_poke_dw(AWE_VTFT(voice), 0x0000FFFF);
	awe_poke_dw(AWE_CVCF(voice), 0x0000FFFF);

	/* turn on envelope */
	awe_poke(AWE_DCYSUSV(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_ENV2_SUSTAIN, AWE_FX_ENV2_DECAY,
			  vp->parm.voldcysus));
	/* set reverb */
	temp = FX_BYTE(fx, fx_lay, AWE_FX_REVERB, vp->parm.reverb);
	temp = (awe_peek_dw(AWE_PTRX(voice)) & 0xffff0000) | (temp<<8);
	awe_poke_dw(AWE_PTRX(voice), temp);
	awe_poke_dw(AWE_CPF(voice), 0x40000000);

	voices[voice].state = AWE_ST_ON;

	/* clear voice position for the next note on this channel */
	if (SINGLE_LAYER_MODE()) {
		FX_UNSET(fx, AWE_FX_SAMPLE_START);
		FX_UNSET(fx, AWE_FX_COARSE_SAMPLE_START);
	}
}


/* turn off the voice */
static void
awe_note_off(int voice)
{
	awe_voice_info *vp;
	unsigned short tmp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if ((vp = voices[voice].sample) == NULL) {
		voices[voice].state = AWE_ST_OFF;
		return;
	}

	tmp = 0x8000 | FX_BYTE(fx, fx_lay, AWE_FX_ENV1_RELEASE,
			       (unsigned char)vp->parm.modrelease);
	awe_poke(AWE_DCYSUS(voice), tmp);
	tmp = 0x8000 | FX_BYTE(fx, fx_lay, AWE_FX_ENV2_RELEASE,
			       (unsigned char)vp->parm.volrelease);
	awe_poke(AWE_DCYSUSV(voice), tmp);
	voices[voice].state = AWE_ST_RELEASED;
}

/* force to terminate the voice (no releasing echo) */
static void
awe_terminate(int voice)
{
	awe_poke(AWE_DCYSUSV(voice), 0x807F);
	awe_tweak_voice(voice);
	voices[voice].state = AWE_ST_OFF;
}

/* turn off other voices with the same exclusive class (for drums) */
static void
awe_exclusive_off(int voice)
{
	int i, exclass;

	if (voices[voice].sample == NULL)
		return;
	if ((exclass = voices[voice].sample->exclusiveClass) == 0)
		return;	/* not exclusive */

	/* turn off voices with the same class */
	for (i = 0; i < awe_max_voices; i++) {
		if (i != voice && IS_PLAYING(i) &&
		    voices[i].sample && voices[i].ch == voices[voice].ch &&
		    voices[i].sample->exclusiveClass == exclass) {
			DEBUG(4,printk("AWE32: [exoff(%d)]\n", i));
			awe_terminate(i);
			awe_voice_init(i, TRUE);
		}
	}
}


/*================================================================
 * change the parameters of an audible voice
 *================================================================*/

/* change pitch */
static void
awe_set_pitch(int voice, int forced)
{
	if (IS_NO_EFFECT(voice) && !forced) return;
	awe_poke(AWE_IP(voice), voices[voice].apitch);
	DEBUG(3,printk("AWE32: [-- pitch=%x]\n", voices[voice].apitch));
}

/* calculate & change pitch */
static void
awe_set_voice_pitch(int voice, int forced)
{
	awe_calc_pitch(voice);
	awe_set_pitch(voice, forced);
}

/* change volume & cutoff */
static void
awe_set_volume(int voice, int forced)
{
	awe_voice_info *vp;
	unsigned short tmp2;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if (!IS_PLAYING(voice) && !forced) return;
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;

	tmp2 = FX_BYTE(fx, fx_lay, AWE_FX_CUTOFF, vp->parm.cutoff);
	tmp2 = (tmp2 << 8);
	tmp2 |= FX_BYTE(fx, fx_lay, AWE_FX_ATTEN,
			(unsigned char)voices[voice].avol);
	awe_poke(AWE_IFATN(voice), tmp2);
}

/* calculate & change volume */
static void
awe_set_voice_vol(int voice, int forced)
{
	if (IS_EMPTY(voice))
		return;
	awe_calc_volume(voice);
	awe_set_volume(voice, forced);
}


/* change pan; this could make a click noise.. */
static void
awe_set_pan(int voice, int forced)
{
	unsigned int temp;
	int addr;
	awe_voice_info *vp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if (IS_NO_EFFECT(voice) && !forced) return;
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;

	/* pan & loop start (pan 8bit, MSB, 0:right, 0xff:left) */
	if (vp->fixpan > 0)	/* 0-127 */
		temp = 255 - (int)vp->fixpan * 2;
	else {
		int pos = 0;
		if (vp->pan >= 0) /* 0-127 */
			pos = (int)vp->pan * 2 - 128;
		pos += voices[voice].cinfo->panning; /* -128 - 127 */
		pos = 127 - pos;
		if (pos < 0)
			temp = 0;
		else if (pos > 255)
			temp = 255;
		else
			temp = pos;
	}
	if (forced || temp != voices[voice].apan) {
		addr = vp->loopstart - 1;
		addr += FX_OFFSET(fx, fx_lay, AWE_FX_LOOP_START,
				  AWE_FX_COARSE_LOOP_START, vp->mode);
		temp = (temp<<24) | (unsigned int)addr;
		awe_poke_dw(AWE_PSST(voice), temp);
		voices[voice].apan = temp;
		DEBUG(4,printk("AWE32: [-- loopstart=%x/%x]\n", vp->loopstart, addr));
	}
}

/* effects change during playing */
static void
awe_fx_fmmod(int voice, int forced)
{
	awe_voice_info *vp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if (IS_NO_EFFECT(voice) && !forced) return;
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;
	awe_poke(AWE_FMMOD(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_LFO1_PITCH, AWE_FX_LFO1_CUTOFF,
			 vp->parm.fmmod));
}

/* set tremolo (lfo1) volume & frequency */
static void
awe_fx_tremfrq(int voice, int forced)
{
	awe_voice_info *vp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if (IS_NO_EFFECT(voice) && !forced) return;
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;
	awe_poke(AWE_TREMFRQ(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_LFO1_VOLUME, AWE_FX_LFO1_FREQ,
			 vp->parm.tremfrq));
}

/* set lfo2 pitch & frequency */
static void
awe_fx_fm2frq2(int voice, int forced)
{
	awe_voice_info *vp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if (IS_NO_EFFECT(voice) && !forced) return;
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;
	awe_poke(AWE_FM2FRQ2(voice),
		 FX_COMB(fx, fx_lay, AWE_FX_LFO2_PITCH, AWE_FX_LFO2_FREQ,
			 vp->parm.fm2frq2));
}


/* Q & current address (Q 4bit value, MSB) */
static void
awe_fx_filterQ(int voice, int forced)
{
	unsigned int addr;
	awe_voice_info *vp;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	if (IS_NO_EFFECT(voice) && !forced) return;
	if ((vp = voices[voice].sample) == NULL || vp->index < 0)
		return;

	addr = awe_peek_dw(AWE_CCCA(voice)) & 0xffffff;
	addr |= (FX_BYTE(fx, fx_lay, AWE_FX_FILTERQ, vp->parm.filterQ) << 28);
	awe_poke_dw(AWE_CCCA(voice), addr);
}

/*================================================================
 * calculate pitch offset
 *----------------------------------------------------------------
 * 0xE000 is no pitch offset at 44100Hz sample.
 * Every 4096 is one octave.
 *================================================================*/

static void
awe_calc_pitch(int voice)
{
	voice_info *vp = &voices[voice];
	awe_voice_info *ap;
	awe_chan_info *cp = voices[voice].cinfo;
	int offset;

	/* search voice information */
	if ((ap = vp->sample) == NULL)
			return;
	if (ap->index < 0) {
		DEBUG(3,printk("AWE32: set sample (%d)\n", ap->sample));
		if (awe_set_sample(ap) < 0)
			return;
	}

	/* calculate offset */
	if (ap->fixkey >= 0) {
		DEBUG(3,printk("AWE32: p-> fixkey(%d) tune(%d)\n", ap->fixkey, ap->tune));
		offset = (ap->fixkey - ap->root) * 4096 / 12;
	} else {
		DEBUG(3,printk("AWE32: p(%d)-> root(%d) tune(%d)\n", vp->note, ap->root, ap->tune));
		offset = (vp->note - ap->root) * 4096 / 12;
		DEBUG(4,printk("AWE32: p-> ofs=%d\n", offset));
	}
	offset = (offset * ap->scaleTuning) / 100;
	DEBUG(4,printk("AWE32: p-> scale* ofs=%d\n", offset));
	offset += ap->tune * 4096 / 1200;
	DEBUG(4,printk("AWE32: p-> tune+ ofs=%d\n", offset));
	if (cp->bender != 0) {
		DEBUG(3,printk("AWE32: p-> bend(%d) %d\n", voice, cp->bender));
		/* (819200: 1 semitone) ==> (4096: 12 semitones) */
		offset += cp->bender * cp->bender_range / 2400;
	}

	/* add initial pitch correction */
	if (FX_ON(&cp->fx_layer[vp->layer], AWE_FX_INIT_PITCH))
		offset += cp->fx_layer[vp->layer].val[AWE_FX_INIT_PITCH];
	else if (FX_ON(&cp->fx, AWE_FX_INIT_PITCH))
		offset += cp->fx.val[AWE_FX_INIT_PITCH];

	/* 0xe000: root pitch */
	vp->apitch = 0xe000 + ap->rate_offset + offset;
	DEBUG(4,printk("AWE32: p-> sum aofs=%x, rate_ofs=%d\n", vp->apitch, ap->rate_offset));
	if (vp->apitch > 0xffff)
		vp->apitch = 0xffff;
	if (vp->apitch < 0)
		vp->apitch = 0;
}


#ifdef AWE_HAS_GUS_COMPATIBILITY
/* calculate MIDI key and semitone from the specified frequency */
static void
awe_calc_pitch_from_freq(int voice, int freq)
{
	voice_info *vp = &voices[voice];
	awe_voice_info *ap;
	FX_Rec *fx = &voices[voice].cinfo->fx;
	FX_Rec *fx_lay = NULL;
	int offset;
	int note;

	if (voices[voice].layer < MAX_LAYERS)
		fx_lay = &voices[voice].cinfo->fx_layer[voices[voice].layer];

	/* search voice information */
	if ((ap = vp->sample) == NULL)
		return;
	if (ap->index < 0) {
		DEBUG(3,printk("AWE32: set sample (%d)\n", ap->sample));
		if (awe_set_sample(ap) < 0)
			return;
	}
	note = freq_to_note(freq);
	offset = (note - ap->root * 100 + ap->tune) * 4096 / 1200;
	offset = (offset * ap->scaleTuning) / 100;
	if (fx_lay && FX_ON(fx_lay, AWE_FX_INIT_PITCH))
		offset += fx_lay->val[AWE_FX_INIT_PITCH];
	else if (FX_ON(fx, AWE_FX_INIT_PITCH))
		offset += fx->val[AWE_FX_INIT_PITCH];
	vp->apitch = 0xe000 + ap->rate_offset + offset;
	if (vp->apitch > 0xffff)
		vp->apitch = 0xffff;
	if (vp->apitch < 0)
		vp->apitch = 0;
}
#endif /* AWE_HAS_GUS_COMPATIBILITY */


/*================================================================
 * calculate volume attenuation
 *----------------------------------------------------------------
 * Voice volume is controlled by volume attenuation parameter.
 * So volume becomes maximum when avol is 0 (no attenuation), and
 * minimum when 255 (-96dB or silence).
 *================================================================*/

static int vol_table[128] = {
	255,111,95,86,79,74,70,66,63,61,58,56,54,52,50,49,
	47,46,45,43,42,41,40,39,38,37,36,35,34,34,33,32,
	31,31,30,29,29,28,27,27,26,26,25,24,24,23,23,22,
	22,21,21,21,20,20,19,19,18,18,18,17,17,16,16,16,
	15,15,15,14,14,14,13,13,13,12,12,12,11,11,11,10,
	10,10,10,9,9,9,8,8,8,8,7,7,7,7,6,6,
	6,6,5,5,5,5,5,4,4,4,4,3,3,3,3,3,
	2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0,
};

static void
awe_calc_volume(int voice)
{
	voice_info *vp = &voices[voice];
	awe_voice_info *ap;
	awe_chan_info *cp = voices[voice].cinfo;
	int vol;

	/* search voice information */
	if ((ap = vp->sample) == NULL)
		return;

	ap = vp->sample;
	if (ap->index < 0) {
		DEBUG(3,printk("AWE32: set sample (%d)\n", ap->sample));
		if (awe_set_sample(ap) < 0)
			return;
	}
	
	/* 0 - 127 */
	vol = (vp->velocity * cp->main_vol * cp->expression_vol) / (127*127);
	vol = vol * ap->amplitude / 127;

	if (vol < 0) vol = 0;
	if (vol > 127) vol = 127;

	/* calc to attenuation */
	vol = vol_table[vol];
	vol = vol + (int)ap->attenuation + init_atten;
	if (vol > 255) vol = 255;

	vp->avol = vol;
	DEBUG(3,printk("AWE32: [-- voice(%d) vol=%x]\n", voice, vol));
}


/* set sostenuto on */
static void awe_sostenuto_on(int voice, int forced)
{
	if (IS_NO_EFFECT(voice) && !forced) return;
	voices[voice].sostenuto = 127;
}


/* drop sustain */
static void awe_sustain_off(int voice, int forced)
{
	if (voices[voice].state == AWE_ST_SUSTAINED) {
		awe_note_off(voice);
		awe_fx_init(voices[voice].ch);
		awe_voice_init(voice, FALSE);
	}
}


/* terminate and initialize voice */
static void awe_terminate_and_init(int voice, int forced)
{
	awe_terminate(voice);
	awe_fx_init(voices[voice].ch);
	awe_voice_init(voice, TRUE);
}


/*================================================================
 * synth operation routines
 *================================================================*/

#define AWE_VOICE_KEY(v)	(0x8000 | (v))
#define AWE_CHAN_KEY(c,n)	(((c) << 8) | ((n) + 1))
#define KEY_CHAN_MATCH(key,c)	(((key) >> 8) == (c))

/* initialize the voice */
static void
awe_voice_init(int voice, int init_all)
{
	voice_info *vp = &voices[voice];

	/* reset voice search key */
	if (playing_mode == AWE_PLAY_DIRECT)
		vp->key = AWE_VOICE_KEY(voice);
	else
		vp->key = 0;

	/* clear voice mapping */
	voice_alloc->map[voice] = 0;

	/* touch the timing flag */
	vp->time = current_alloc_time;

	/* initialize other parameters if necessary */
	if (init_all) {
		vp->note = -1;
		vp->velocity = 0;
		vp->sostenuto = 0;

		vp->sample = NULL;
		vp->cinfo = &channels[voice];
		vp->ch = voice;
		vp->state = AWE_ST_OFF;

		/* emu8000 parameters */
		vp->apitch = 0;
		vp->avol = 255;
		vp->apan = -1;
	}
}

/* clear effects */
static void awe_fx_init(int ch)
{
	if (SINGLE_LAYER_MODE() && !misc_modes[AWE_MD_KEEP_EFFECT]) {
		BZERO(&channels[ch].fx, sizeof(channels[ch].fx));
		BZERO(&channels[ch].fx_layer, sizeof(&channels[ch].fx_layer));
	}
}

/* initialize channel info */
static void awe_channel_init(int ch, int init_all)
{
	awe_chan_info *cp = &channels[ch];
	cp->channel = ch;
	if (init_all) {
		cp->panning = 0; /* zero center */
		cp->bender_range = 200; /* sense * 100 */
		cp->main_vol = 127;
		if (MULTI_LAYER_MODE() && IS_DRUM_CHANNEL(ch)) {
			cp->instr = misc_modes[AWE_MD_DEF_DRUM];
			cp->bank = AWE_DRUM_BANK;
		} else {
			cp->instr = misc_modes[AWE_MD_DEF_PRESET];
			cp->bank = misc_modes[AWE_MD_DEF_BANK];
		}
		cp->vrec = -1;
		cp->def_vrec = -1;
	}

	cp->bender = 0; /* zero tune skew */
	cp->expression_vol = 127;
	cp->chan_press = 0;
	cp->sustained = 0;

	if (! misc_modes[AWE_MD_KEEP_EFFECT]) {
		BZERO(&cp->fx, sizeof(cp->fx));
		BZERO(&cp->fx_layer, sizeof(cp->fx_layer));
	}
}


/* change the voice parameters; voice = channel */
static void awe_voice_change(int voice, fx_affect_func func)
{
	int i; 
	switch (playing_mode) {
	case AWE_PLAY_DIRECT:
		func(voice, FALSE);
		break;
	case AWE_PLAY_INDIRECT:
		for (i = 0; i < awe_max_voices; i++)
			if (voices[i].key == AWE_VOICE_KEY(voice))
				func(i, FALSE);
		break;
	default:
		for (i = 0; i < awe_max_voices; i++)
			if (KEY_CHAN_MATCH(voices[i].key, voice))
				func(i, FALSE);
		break;
	}
}


/*----------------------------------------------------------------
 * device open / close
 *----------------------------------------------------------------*/

/* open device:
 *   reset status of all voices, and clear sample position flag
 */
static int
awe_open(int dev, int mode)
{
	if (awe_busy)
		return RET_ERROR(EBUSY);

	awe_busy = TRUE;

	/* set default mode */
	awe_init_misc_modes(FALSE);
	init_atten = misc_modes[AWE_MD_ZERO_ATTEN];
	drum_flags = DEFAULT_DRUM_FLAGS;
	playing_mode = AWE_PLAY_INDIRECT;

	/* reset voices & channels */
	awe_reset(dev);

	patch_opened = 0;

	return 0;
}


/* close device:
 *   reset all voices again (terminate sounds)
 */
static void
awe_close(int dev)
{
	awe_reset(dev);
	awe_busy = FALSE;
}


/* set miscellaneous mode parameters
 */
static void
awe_init_misc_modes(int init_all)
{
	int i;
	for (i = 0; i < AWE_MD_END; i++) {
		if (init_all || misc_modes_default[i].init_each_time)
			misc_modes[i] = misc_modes_default[i].value;
	}
}


/* sequencer I/O control:
 */
static int
awe_ioctl(int dev, unsigned int cmd, caddr_t arg)
{
	switch (cmd) {
	case SNDCTL_SYNTH_INFO:
		if (playing_mode == AWE_PLAY_DIRECT)
			awe_info.nr_voices = awe_max_voices;
		else
			awe_info.nr_voices = AWE_MAX_CHANNELS;
		IOCTL_TO_USER((char*)arg, 0, &awe_info, sizeof(awe_info));
		return 0;
		break;

	case SNDCTL_SEQ_RESETSAMPLES:
		awe_reset_samples();
		awe_reset(dev);
		return 0;
		break;

	case SNDCTL_SEQ_PERCMODE:
		/* what's this? */
		return 0;
		break;

	case SNDCTL_SYNTH_MEMAVL:
		return awe_mem_size - awe_free_mem_ptr() * 2;

	default:
		printk("AWE32: unsupported ioctl %d\n", cmd);
		return RET_ERROR(EINVAL);
	}
}


static int voice_in_range(int voice)
{
	if (playing_mode == AWE_PLAY_DIRECT) {
		if (voice < 0 || voice >= awe_max_voices)
			return FALSE;
	} else {
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return FALSE;
	}
	return TRUE;
}

static void release_voice(int voice, int do_sustain)
{
	if (IS_NO_SOUND(voice))
		return;
	if (do_sustain && (voices[voice].cinfo->sustained == 127 ||
			    voices[voice].sostenuto == 127))
		voices[voice].state = AWE_ST_SUSTAINED;
	else {
		awe_note_off(voice);
		awe_fx_init(voices[voice].ch);
		awe_voice_init(voice, FALSE);
	}
}

/* release all notes */
static void awe_note_off_all(int do_sustain)
{
	int i;
	for (i = 0; i < awe_max_voices; i++)
		release_voice(i, do_sustain);
}

/* kill a voice:
 *   not terminate, just release the voice.
 */
static int
awe_kill_note(int dev, int voice, int note, int velocity)
{
	int i, v2, key;

	DEBUG(2,printk("AWE32: [off(%d) nt=%d vl=%d]\n", voice, note, velocity));
	if (! voice_in_range(voice))
		return RET_ERROR(EINVAL);

	switch (playing_mode) {
	case AWE_PLAY_DIRECT:
	case AWE_PLAY_INDIRECT:
		key = AWE_VOICE_KEY(voice);
		break;

	case AWE_PLAY_MULTI2:
		v2 = voice_alloc->map[voice] >> 8;
		voice_alloc->map[voice] = 0;
		voice = v2;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return RET_ERROR(EINVAL);
		/* continue to below */
	default:
		key = AWE_CHAN_KEY(voice, note);
		break;
	}

	for (i = 0; i < awe_max_voices; i++) {
		if (voices[i].key == key)
			release_voice(i, TRUE);
	}
	return 0;
}


static void start_or_volume_change(int voice, int velocity)
{
	voices[voice].velocity = velocity;
	awe_calc_volume(voice);
	if (voices[voice].state == AWE_ST_STANDBY)
		awe_note_on(voice);
	else if (voices[voice].state == AWE_ST_ON)
		awe_set_volume(voice, FALSE);
}

static void set_and_start_voice(int voice, int state)
{
	/* calculate pitch & volume parameters */
	voices[voice].state = state;
	awe_calc_pitch(voice);
	awe_calc_volume(voice);
	if (state == AWE_ST_ON)
		awe_note_on(voice);
}

/* start a voice:
 *   if note is 255, identical with aftertouch function.
 *   Otherwise, start a voice with specified not and volume.
 */
static int
awe_start_note(int dev, int voice, int note, int velocity)
{
	int i, key, state, volonly;

	DEBUG(2,printk("AWE32: [on(%d) nt=%d vl=%d]\n", voice, note, velocity));
	if (! voice_in_range(voice))
		return RET_ERROR(EINVAL);
	    
	if (velocity == 0)
		state = AWE_ST_STANDBY; /* stand by for playing */
	else
		state = AWE_ST_ON;	/* really play */
	volonly = FALSE;

	switch (playing_mode) {
	case AWE_PLAY_DIRECT:
	case AWE_PLAY_INDIRECT:
		key = AWE_VOICE_KEY(voice);
		if (note == 255)
			volonly = TRUE;
		break;

	case AWE_PLAY_MULTI2:
		voice = voice_alloc->map[voice] >> 8;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return RET_ERROR(EINVAL);
		/* continue to below */
	default:
		if (note >= 128) { /* key volume mode */
			note -= 128;
			volonly = TRUE;
		}
		key = AWE_CHAN_KEY(voice, note);
		break;
	}

	/* dynamic volume change */
	if (volonly) {
		for (i = 0; i < awe_max_voices; i++) {
			if (voices[i].key == key)
				start_or_volume_change(i, velocity);
		}
		return 0;
	}

	/* if the same note still playing, stop it */
	for (i = 0; i < awe_max_voices; i++)
		if (voices[i].key == key) {
			if (voices[i].state == AWE_ST_ON) {
				awe_note_off(i);
				awe_voice_init(i, FALSE);
			} else if (voices[i].state == AWE_ST_STANDBY)
				awe_voice_init(i, TRUE);
		}

	/* allocate voices */
	if (playing_mode == AWE_PLAY_DIRECT)
		awe_alloc_one_voice(voice, note, velocity);
	else
		awe_alloc_multi_voices(voice, note, velocity, key);

	/* turn off other voices exlusively (for drums) */
	for (i = 0; i < awe_max_voices; i++)
		if (voices[i].key == key)
			awe_exclusive_off(i);

	/* set up pitch and volume parameters */
	for (i = 0; i < awe_max_voices; i++) {
		if (voices[i].key == key && voices[i].state == AWE_ST_OFF)
			set_and_start_voice(i, state);
	}

	return 0;
}


/* search instrument from preset table with the specified bank */
static int
awe_search_instr(int bank, int preset)
{
	int i;

	for (i = preset_table[preset]; i >= 0; i = infos[i].next_bank) {
		if (infos[i].bank == bank)
			return i;
	}
	return -1;
}


/* assign the instrument to a voice */
static int
awe_set_instr_2(int dev, int voice, int instr_no)
{
	if (playing_mode == AWE_PLAY_MULTI2) {
		voice = voice_alloc->map[voice] >> 8;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return RET_ERROR(EINVAL);
	}
	return awe_set_instr(dev, voice, instr_no);
}

/* assign the instrument to a channel; voice is the channel number */
static int
awe_set_instr(int dev, int voice, int instr_no)
{
	awe_chan_info *cinfo;
	int def_bank;

	if (! voice_in_range(voice))
		return RET_ERROR(EINVAL);

	if (instr_no < 0 || instr_no >= AWE_MAX_PRESETS)
		return RET_ERROR(EINVAL);

	cinfo = &channels[voice];

	if (MULTI_LAYER_MODE() && IS_DRUM_CHANNEL(voice))
		def_bank = AWE_DRUM_BANK; /* always search drumset */
	else
		def_bank = cinfo->bank;

	cinfo->vrec = -1;
	cinfo->def_vrec = -1;
	cinfo->vrec = awe_search_instr(def_bank, instr_no);
	if (def_bank == AWE_DRUM_BANK)	/* search default drumset */
		cinfo->def_vrec = awe_search_instr(def_bank, misc_modes[AWE_MD_DEF_DRUM]);
	else	/* search default preset */
		cinfo->def_vrec = awe_search_instr(misc_modes[AWE_MD_DEF_BANK], instr_no);

	if (cinfo->vrec < 0 && cinfo->def_vrec < 0) {
		DEBUG(1,printk("AWE32 Warning: can't find instrument %d\n", instr_no));
	}

	cinfo->instr = instr_no;

	return 0;
}


/* reset all voices; terminate sounds and initialize parameters */
static void
awe_reset(int dev)
{
	int i;
	current_alloc_time = 0;
	/* don't turn off voice 31 and 32.  they are used also for FM voices */
	for (i = 0; i < awe_max_voices; i++) {
		awe_terminate(i);
		awe_voice_init(i, TRUE);
	}
	for (i = 0; i < AWE_MAX_CHANNELS; i++)
		awe_channel_init(i, TRUE);
	for (i = 0; i < 16; i++) {
		awe_operations.chn_info[i].controllers[CTL_MAIN_VOLUME] = 127;
		awe_operations.chn_info[i].controllers[CTL_EXPRESSION] = 127;
	}
	awe_init_fm();
	awe_tweak();
}


/* hardware specific control:
 *   GUS specific and AWE32 specific controls are available.
 */
static void
awe_hw_control(int dev, unsigned char *event)
{
	int cmd = event[2];
	if (cmd & _AWE_MODE_FLAG)
		awe_hw_awe_control(dev, cmd & _AWE_MODE_VALUE_MASK, event);
#ifdef AWE_HAS_GUS_COMPATIBILITY
	else
		awe_hw_gus_control(dev, cmd & _AWE_MODE_VALUE_MASK, event);
#endif
}


#ifdef AWE_HAS_GUS_COMPATIBILITY

/* GUS compatible controls */
static void
awe_hw_gus_control(int dev, int cmd, unsigned char *event)
{
	int voice, i, key;
	unsigned short p1;
	short p2;
	int plong;

	if (MULTI_LAYER_MODE())
		return;
	if (cmd == _GUS_NUMVOICES)
		return;

	voice = event[3];
	if (! voice_in_range(voice))
		return;

	p1 = *(unsigned short *) &event[4];
	p2 = *(short *) &event[6];
	plong = *(int*) &event[4];

	switch (cmd) {
	case _GUS_VOICESAMPLE:
		awe_set_instr(dev, voice, p1);
		return;

	case _GUS_VOICEBALA:
		/* 0 to 15 --> -128 to 127 */
		awe_panning(dev, voice, ((int)p1 << 4) - 128);
		return;

	case _GUS_VOICEVOL:
	case _GUS_VOICEVOL2:
		/* not supported yet */
		return;

	case _GUS_RAMPRANGE:
	case _GUS_RAMPRATE:
	case _GUS_RAMPMODE:
	case _GUS_RAMPON:
	case _GUS_RAMPOFF:
		/* volume ramping not supported */
		return;

	case _GUS_VOLUME_SCALE:
		return;

	case _GUS_VOICE_POS:
		FX_SET(&channels[voice].fx, AWE_FX_SAMPLE_START,
		       (short)(plong & 0x7fff));
		FX_SET(&channels[voice].fx, AWE_FX_COARSE_SAMPLE_START,
		       (plong >> 15) & 0xffff);
		return;
	}

	key = AWE_VOICE_KEY(voice);
	for (i = 0; i < awe_max_voices; i++) {
		if (voices[i].key == key) {
			switch (cmd) {
			case _GUS_VOICEON:
				awe_note_on(i);
				break;

			case _GUS_VOICEOFF:
				awe_terminate(i);
				awe_fx_init(voices[i].ch);
				awe_voice_init(i, TRUE);
				break;

			case _GUS_VOICEFADE:
				awe_note_off(i);
				awe_fx_init(voices[i].ch);
				awe_voice_init(i, FALSE);
				break;

			case _GUS_VOICEFREQ:
				awe_calc_pitch_from_freq(i, plong);
				break;
			}
		}
	}
}

#endif


/* AWE32 specific controls */
static void
awe_hw_awe_control(int dev, int cmd, unsigned char *event)
{
	int voice;
	unsigned short p1;
	short p2;
	awe_chan_info *cinfo;
	FX_Rec *fx;
	int i;

	voice = event[3];
	if (! voice_in_range(voice))
		return;

	if (playing_mode == AWE_PLAY_MULTI2) {
		voice = voice_alloc->map[voice] >> 8;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return;
	}

	p1 = *(unsigned short *) &event[4];
	p2 = *(short *) &event[6];
	cinfo = &channels[voice];

	switch (cmd) {
	case _AWE_DEBUG_MODE:
		debug_mode = p1;
		printk("AWE32: debug mode = %d\n", debug_mode);
		break;
	case _AWE_REVERB_MODE:
		awe_set_reverb_mode(p1);
		break;

	case _AWE_CHORUS_MODE:
		awe_set_chorus_mode(p1);
		break;
		      
	case _AWE_REMOVE_LAST_SAMPLES:
		DEBUG(0,printk("AWE32: remove last samples\n"));
		if (locked_sf_id > 0)
			awe_remove_samples(locked_sf_id);
		break;

	case _AWE_INITIALIZE_CHIP:
		awe_initialize();
		break;

	case _AWE_SEND_EFFECT:
		fx = &cinfo->fx;
		i = FX_FLAG_SET;
		if (p1 >= 0x100) {
			int layer = (p1 >> 8);
			if (layer >= 0 && layer < MAX_LAYERS)
				fx = &cinfo->fx_layer[layer];
			p1 &= 0xff;
		}
		if (p1 & 0x40) i = FX_FLAG_OFF;
		if (p1 & 0x80) i = FX_FLAG_ADD;
		p1 &= 0x3f;
		if (p1 < AWE_FX_END) {
			DEBUG(0,printk("AWE32: effects (%d) %d %d\n", voice, p1, p2));
			if (i == FX_FLAG_SET)
				FX_SET(fx, p1, p2);
			else if (i == FX_FLAG_ADD)
				FX_ADD(fx, p1, p2);
			else
				FX_UNSET(fx, p1);
			if (i != FX_FLAG_OFF && parm_defs[p1].realtime) {
				DEBUG(0,printk("AWE32: fx_realtime (%d)\n", voice));
				awe_voice_change(voice, parm_defs[p1].realtime);
			}
		}
		break;

	case _AWE_RESET_CHANNEL:
		awe_channel_init(voice, !p1);
		break;
		
	case _AWE_TERMINATE_ALL:
		awe_reset(0);
		break;

	case _AWE_TERMINATE_CHANNEL:
		awe_voice_change(voice, awe_terminate_and_init);
		break;

	case _AWE_RELEASE_ALL:
		awe_note_off_all(FALSE);
		break;
	case _AWE_NOTEOFF_ALL:
		awe_note_off_all(TRUE);
		break;

	case _AWE_INITIAL_VOLUME:
		DEBUG(0,printk("AWE32: init attenuation %d\n", p1));
		if (p2 == 0) /* absolute value */
			init_atten = (short)p1;
		else /* relative value */
			init_atten = misc_modes[AWE_MD_ZERO_ATTEN] + (short)p1;
		if (init_atten < 0) init_atten = 0;
		for (i = 0; i < awe_max_voices; i++)
			awe_set_voice_vol(i, TRUE);
		break;

	case _AWE_CHN_PRESSURE:
		cinfo->chan_press = p1;
		p1 = p1 * misc_modes[AWE_MD_MOD_SENSE] / 1200;
		FX_ADD(&cinfo->fx, AWE_FX_LFO1_PITCH, p1);
		awe_voice_change(voice, awe_fx_fmmod);
		FX_ADD(&cinfo->fx, AWE_FX_LFO2_PITCH, p1);
		awe_voice_change(voice, awe_fx_fm2frq2);
		break;

	case _AWE_CHANNEL_MODE:
		DEBUG(0,printk("AWE32: channel mode = %d\n", p1));
		playing_mode = p1;
		awe_reset(0);
		break;

	case _AWE_DRUM_CHANNELS:
		DEBUG(0,printk("AWE32: drum flags = %x\n", p1));
		drum_flags = *(unsigned int*)&event[4];
		break;

	case _AWE_MISC_MODE:
		DEBUG(0,printk("AWE32: misc mode = %d %d\n", p1, p2));
		if (p1 > AWE_MD_VERSION && p1 < AWE_MD_END)
			misc_modes[p1] = p2;
		break;

	case _AWE_EQUALIZER:
		awe_equalizer((int)p1, (int)p2);
		break;

	default:
		DEBUG(0,printk("AWE32: hw control cmd=%d voice=%d\n", cmd, voice));
		break;
	}
}


/* voice pressure change */
static void
awe_aftertouch(int dev, int voice, int pressure)
{
	int note;

	DEBUG(2,printk("AWE32: [after(%d) %d]\n", voice, pressure));
	if (! voice_in_range(voice))
		return;

	switch (playing_mode) {
	case AWE_PLAY_DIRECT:
	case AWE_PLAY_INDIRECT:
		awe_start_note(dev, voice, 255, pressure);
		break;
	case AWE_PLAY_MULTI2:
		note = (voice_alloc->map[voice] & 0xff) - 1;
		awe_start_note(dev, voice, note + 0x80, pressure);
		break;
	}
}


/* voice control change */
static void
awe_controller(int dev, int voice, int ctrl_num, int value)
{
	int i;
	awe_chan_info *cinfo;

	if (! voice_in_range(voice))
		return;

	if (playing_mode == AWE_PLAY_MULTI2) {
		voice = voice_alloc->map[voice] >> 8;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return;
	}

	cinfo = &channels[voice];

	switch (ctrl_num) {
	case CTL_BANK_SELECT: /* MIDI control #0 */
		DEBUG(2,printk("AWE32: [bank(%d) %d]\n", voice, value));
		if (MULTI_LAYER_MODE() && IS_DRUM_CHANNEL(voice) &&
		    !misc_modes[AWE_MD_TOGGLE_DRUM_BANK])
			break;
		cinfo->bank = value;
		if (cinfo->bank == AWE_DRUM_BANK)
			DRUM_CHANNEL_ON(cinfo->channel);
		else
			DRUM_CHANNEL_OFF(cinfo->channel);
		awe_set_instr(dev, voice, cinfo->instr);
		break;

	case CTL_MODWHEEL: /* MIDI control #1 */
		DEBUG(2,printk("AWE32: [modwheel(%d) %d]\n", voice, value));
		i = value * misc_modes[AWE_MD_MOD_SENSE] / 1200;
		FX_ADD(&cinfo->fx, AWE_FX_LFO1_PITCH, i);
		awe_voice_change(voice, awe_fx_fmmod);
		FX_ADD(&cinfo->fx, AWE_FX_LFO2_PITCH, i);
		awe_voice_change(voice, awe_fx_fm2frq2);
		break;

	case CTRL_PITCH_BENDER: /* SEQ1 V2 contorl */
		DEBUG(2,printk("AWE32: [bend(%d) %d]\n", voice, value));
		/* zero centered */
		cinfo->bender = value;
		awe_voice_change(voice, awe_set_voice_pitch);
		break;

	case CTRL_PITCH_BENDER_RANGE: /* SEQ1 V2 control */
		DEBUG(2,printk("AWE32: [range(%d) %d]\n", voice, value));
		/* value = sense x 100 */
		cinfo->bender_range = value;
		/* no audible pitch change yet.. */
		break;

	case CTL_EXPRESSION: /* MIDI control #11 */
		if (SINGLE_LAYER_MODE())
			value /= 128;
	case CTRL_EXPRESSION: /* SEQ1 V2 control */
		DEBUG(2,printk("AWE32: [expr(%d) %d]\n", voice, value));
		/* 0 - 127 */
		cinfo->expression_vol = value;
		awe_voice_change(voice, awe_set_voice_vol);
		break;

	case CTL_PAN:	/* MIDI control #10 */
		DEBUG(2,printk("AWE32: [pan(%d) %d]\n", voice, value));
		/* (0-127) -> signed 8bit */
		cinfo->panning = value * 2 - 128;
		if (misc_modes[AWE_MD_REALTIME_PAN])
			awe_voice_change(voice, awe_set_pan);
		break;

	case CTL_MAIN_VOLUME:	/* MIDI control #7 */
		if (SINGLE_LAYER_MODE())
			value = (value * 100) / 16383;
	case CTRL_MAIN_VOLUME:	/* SEQ1 V2 control */
		DEBUG(2,printk("AWE32: [mainvol(%d) %d]\n", voice, value));
		/* 0 - 127 */
		cinfo->main_vol = value;
		awe_voice_change(voice, awe_set_voice_vol);
		break;

	case CTL_EXT_EFF_DEPTH: /* reverb effects: 0-127 */
		DEBUG(2,printk("AWE32: [reverb(%d) %d]\n", voice, value));
		FX_SET(&cinfo->fx, AWE_FX_REVERB, value * 2);
		break;

	case CTL_CHORUS_DEPTH: /* chorus effects: 0-127 */
		DEBUG(2,printk("AWE32: [chorus(%d) %d]\n", voice, value));
		FX_SET(&cinfo->fx, AWE_FX_CHORUS, value * 2);
		break;

#ifdef AWE_ACCEPT_ALL_SOUNDS_CONTROLL
	case 120:  /* all sounds off */
		awe_note_off_all(FALSE);
		break;
	case 123:  /* all notes off */
		awe_note_off_all(TRUE);
		break;
#endif

	case CTL_SUSTAIN: /* MIDI control #64 */
		cinfo->sustained = value;
		if (value != 127)
			awe_voice_change(voice, awe_sustain_off);
		break;

	case CTL_SOSTENUTO: /* MIDI control #66 */
		if (value == 127)
			awe_voice_change(voice, awe_sostenuto_on);
		else
			awe_voice_change(voice, awe_sustain_off);
		break;

	default:
		DEBUG(0,printk("AWE32: [control(%d) ctrl=%d val=%d]\n",
			   voice, ctrl_num, value));
		break;
	}
}


/* voice pan change (value = -128 - 127) */
static void
awe_panning(int dev, int voice, int value)
{
	awe_chan_info *cinfo;

	if (! voice_in_range(voice))
		return;

	if (playing_mode == AWE_PLAY_MULTI2) {
		voice = voice_alloc->map[voice] >> 8;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return;
	}

	cinfo = &channels[voice];
	cinfo->panning = value;
	DEBUG(2,printk("AWE32: [pan(%d) %d]\n", voice, cinfo->panning));
	if (misc_modes[AWE_MD_REALTIME_PAN])
		awe_voice_change(voice, awe_set_pan);
}


/* volume mode change */
static void
awe_volume_method(int dev, int mode)
{
	/* not impremented */
	DEBUG(0,printk("AWE32: [volmethod mode=%d]\n", mode));
}


#ifndef AWE_NO_PATCHMGR
/* patch manager */
static int
awe_patchmgr(int dev, struct patmgr_info *rec)
{
	printk("AWE32 Warning: patch manager control not supported\n");
	return 0;
}
#endif


/* pitch wheel change: 0-16384 */
static void
awe_bender(int dev, int voice, int value)
{
	awe_chan_info *cinfo;

	if (! voice_in_range(voice))
		return;

	if (playing_mode == AWE_PLAY_MULTI2) {
		voice = voice_alloc->map[voice] >> 8;
		if (voice < 0 || voice >= AWE_MAX_CHANNELS)
			return;
	}

	/* convert to zero centered value */
	cinfo = &channels[voice];
	cinfo->bender = value - 8192;
	DEBUG(2,printk("AWE32: [bend(%d) %d]\n", voice, cinfo->bender));
	awe_voice_change(voice, awe_set_voice_pitch);
}


/*----------------------------------------------------------------
 * load a sound patch:
 *   three types of patches are accepted: AWE, GUS, and SYSEX.
 *----------------------------------------------------------------*/

static int
awe_load_patch(int dev, int format, const char *addr,
	       int offs, int count, int pmgr_flag)
{
	awe_patch_info patch;
	int rc = 0;

#ifdef AWE_HAS_GUS_COMPATIBILITY
	if (format == GUS_PATCH) {
		return awe_load_guspatch(addr, offs, count, pmgr_flag);
	} else
#endif
	if (format == SYSEX_PATCH) {
		/* no system exclusive message supported yet */
		return 0;
	} else if (format != AWE_PATCH) {
		printk("AWE32 Error: Invalid patch format (key) 0x%x\n", format);
		return RET_ERROR(EINVAL);
	}
	
	if (count < AWE_PATCH_INFO_SIZE) {
		printk("AWE32 Error: Patch header too short\n");
		return RET_ERROR(EINVAL);
	}
	COPY_FROM_USER(((char*)&patch) + offs, addr, offs, 
		       AWE_PATCH_INFO_SIZE - offs);

	count -= AWE_PATCH_INFO_SIZE;
	if (count < patch.len) {
		printk("AWE32: sample: Patch record too short (%d<%d)\n",
		       count, patch.len);
		return RET_ERROR(EINVAL);
	}
	
	switch (patch.type) {
	case AWE_LOAD_INFO:
		rc = awe_load_info(&patch, addr, count);
		break;
	case AWE_LOAD_DATA:
		rc = awe_load_data(&patch, addr, count);
		break;
	case AWE_OPEN_PATCH:
		rc = awe_open_patch(&patch, addr, count);
		break;
	case AWE_CLOSE_PATCH:
		rc = awe_close_patch(&patch, addr, count);
		break;
	case AWE_UNLOAD_PATCH:
		rc = awe_unload_patch(&patch, addr, count);
		break;
	case AWE_REPLACE_DATA:
		rc = awe_replace_data(&patch, addr, count);
		break;
	case AWE_MAP_PRESET:
		rc = awe_load_map(&patch, addr, count);
		break;
	case AWE_LOAD_CHORUS_FX:
		rc = awe_load_chorus_fx(&patch, addr, count);
		break;
	case AWE_LOAD_REVERB_FX:
		rc = awe_load_reverb_fx(&patch, addr, count);
		break;

	default:
		printk("AWE32 Error: unknown patch format type %d\n",
		       patch.type);
		rc = RET_ERROR(EINVAL);
	}

	return rc;
}


/* create an sflist record */
static int
awe_create_sf(int type, char *name)
{
	sf_list *rec;

	/* terminate sounds */
	awe_reset(0);
	if (current_sf_id >= max_sfs) {
		int newsize = max_sfs + AWE_MAX_SF_LISTS;
		sf_list *newlist = my_realloc(sflists, sizeof(sf_list)*max_sfs,
					      sizeof(sf_list)*newsize);
		if (newlist == NULL)
			return 1;
		sflists = newlist;
		max_sfs = newsize;
	}
	rec = &sflists[current_sf_id];
	rec->sf_id = current_sf_id + 1;
	rec->type = type;
	if (current_sf_id == 0 || (type & AWE_PAT_LOCKED) != 0)
		locked_sf_id = current_sf_id + 1;
	/*
	if (name)
		MEMCPY(rec->name, name, AWE_PATCH_NAME_LEN);
	else
		BZERO(rec->name, AWE_PATCH_NAME_LEN);
	 */
	rec->num_info = awe_free_info();
	rec->num_sample = awe_free_sample();
	rec->mem_ptr = awe_free_mem_ptr();
	rec->infos = -1;
	rec->samples = -1;

	current_sf_id++;
	return 0;
}


/* open patch; create sf list and set opened flag */
static int
awe_open_patch(awe_patch_info *patch, const char *addr, int count)
{
	awe_open_parm parm;
	COPY_FROM_USER(&parm, addr, AWE_PATCH_INFO_SIZE, sizeof(parm));
	if (awe_create_sf(parm.type, parm.name)) {
		printk("AWE32: can't open: failed to alloc new list\n");
		return RET_ERROR(ENOSPC);
	}
	patch_opened = TRUE;
	return current_sf_id;
}

/* check if the patch is already opened */
static int
check_patch_opened(int type, char *name)
{
	if (! patch_opened) {
		if (awe_create_sf(type, name)) {
			printk("AWE32: failed to alloc new list\n");
			return RET_ERROR(ENOSPC);
		}
		patch_opened = TRUE;
		return current_sf_id;
	}
	return current_sf_id;
}

/* close the patch; if no voice is loaded, remove the patch */
static int
awe_close_patch(awe_patch_info *patch, const char *addr, int count)
{
	if (patch_opened && current_sf_id > 0) {
		/* if no voice is loaded, release the current patch */
		if (sflists[current_sf_id-1].infos == -1)
			awe_remove_samples(current_sf_id - 1);
	}
	patch_opened = 0;
	return 0;
}


/* remove the latest patch */
static int
awe_unload_patch(awe_patch_info *patch, const char *addr, int count)
{
	if (current_sf_id > 0)
		awe_remove_samples(current_sf_id - 1);
	return 0;
}

/* allocate voice info list records */
static int alloc_new_info(int nvoices)
{
	int newsize, free_info;
	awe_voice_list *newlist;
	free_info = awe_free_info();
	if (free_info + nvoices >= max_infos) {
		do {
			newsize = max_infos + AWE_MAX_INFOS;
		} while (free_info + nvoices >= newsize);
		newlist = my_realloc(infos, sizeof(awe_voice_list)*max_infos,
				     sizeof(awe_voice_list)*newsize);
		if (newlist == NULL) {
			printk("AWE32: can't alloc info table\n");
			return RET_ERROR(ENOSPC);
		}
		infos = newlist;
		max_infos = newsize;
	}
	return 0;
}

/* allocate sample info list records */
static int alloc_new_sample(void)
{
	int newsize, free_sample;
	awe_sample_list *newlist;
	free_sample = awe_free_sample();
	if (free_sample >= max_samples) {
		newsize = max_samples + AWE_MAX_SAMPLES;
		newlist = my_realloc(samples,
				     sizeof(awe_sample_list)*max_samples,
				     sizeof(awe_sample_list)*newsize);
		if (newlist == NULL) {
			printk("AWE32: can't alloc sample table\n");
			return RET_ERROR(ENOSPC);
		}
		samples = newlist;
		max_samples = newsize;
	}
	return 0;
}

/* load voice map */
static int
awe_load_map(awe_patch_info *patch, const char *addr, int count)
{
	awe_voice_map map;
	awe_voice_list *rec;
	int free_info;

	if (check_patch_opened(AWE_PAT_TYPE_MAP, NULL) < 0)
		return RET_ERROR(ENOSPC);
	if (alloc_new_info(1) < 0)
		return RET_ERROR(ENOSPC);

	COPY_FROM_USER(&map, addr, AWE_PATCH_INFO_SIZE, sizeof(map));
	
	free_info = awe_free_info();
	rec = &infos[free_info];
	rec->bank = map.map_bank;
	rec->instr = map.map_instr;
	rec->type = V_ST_MAPPED;
	rec->disabled = FALSE;
	awe_init_voice_info(&rec->v);
	if (map.map_key >= 0) {
		rec->v.low = map.map_key;
		rec->v.high = map.map_key;
	}
	rec->v.start = map.src_instr;
	rec->v.end = map.src_bank;
	rec->v.fixkey = map.src_key;
	rec->v.sf_id = current_sf_id;
	add_info_list(free_info);
	add_sf_info(free_info);

	return 0;
}

/* load voice information data */
static int
awe_load_info(awe_patch_info *patch, const char *addr, int count)
{
	int offset;
	awe_voice_rec_hdr hdr;
	int i;
	int total_size;

	if (count < AWE_VOICE_REC_SIZE) {
		printk("AWE32 Error: invalid patch info length\n");
		return RET_ERROR(EINVAL);
	}

	offset = AWE_PATCH_INFO_SIZE;
	COPY_FROM_USER((char*)&hdr, addr, offset, AWE_VOICE_REC_SIZE);
	offset += AWE_VOICE_REC_SIZE;

	if (hdr.nvoices <= 0 || hdr.nvoices >= 100) {
		printk("AWE32 Error: Illegal voice number %d\n", hdr.nvoices);
		return RET_ERROR(EINVAL);
	}
	total_size = AWE_VOICE_REC_SIZE + AWE_VOICE_INFO_SIZE * hdr.nvoices;
	if (count < total_size) {
		printk("AWE32 Error: patch length(%d) is smaller than nvoices(%d)\n",
		       count, hdr.nvoices);
		return RET_ERROR(EINVAL);
	}

	if (check_patch_opened(AWE_PAT_TYPE_MISC, NULL) < 0)
		return RET_ERROR(ENOSPC);

#if 0 /* it looks like not so useful.. */
	/* check if the same preset already exists in the info list */
	for (i = sflists[current_sf_id-1].infos; i >= 0; i = infos[i].next) {
		if (infos[i].disabled) continue;
		if (infos[i].bank == hdr.bank && infos[i].instr == hdr.instr) {
			/* in exclusive mode, do skip loading this */
			if (hdr.write_mode == AWE_WR_EXCLUSIVE)
				return 0;
			/* in replace mode, disable the old data */
			else if (hdr.write_mode == AWE_WR_REPLACE)
				infos[i].disabled = TRUE;
		}
	}
	if (hdr.write_mode == AWE_WR_REPLACE)
		rebuild_preset_list();
#endif

	if (alloc_new_info(hdr.nvoices) < 0)
		return RET_ERROR(ENOSPC);

	for (i = 0; i < hdr.nvoices; i++) {
		int rec = awe_free_info();

		infos[rec].bank = hdr.bank;
		infos[rec].instr = hdr.instr;
		infos[rec].type = V_ST_NORMAL;
		infos[rec].disabled = FALSE;

		/* copy awe_voice_info parameters */
		COPY_FROM_USER(&infos[rec].v, addr, offset, AWE_VOICE_INFO_SIZE);
		offset += AWE_VOICE_INFO_SIZE;
		infos[rec].v.sf_id = current_sf_id;
		if (infos[rec].v.mode & AWE_MODE_INIT_PARM)
			awe_init_voice_parm(&infos[rec].v.parm);
		awe_set_sample(&infos[rec].v);
		add_info_list(rec);
		add_sf_info(rec);
	}

	return 0;
}

/* load wave sample data */
static int
awe_load_data(awe_patch_info *patch, const char *addr, int count)
{
	int offset, size;
	int rc, free_sample;
	awe_sample_info *rec;

	if (check_patch_opened(AWE_PAT_TYPE_MISC, NULL) < 0)
		return RET_ERROR(ENOSPC);

	if (alloc_new_sample() < 0)
		return RET_ERROR(ENOSPC);

	free_sample = awe_free_sample();
	rec = &samples[free_sample].v;

	size = (count - AWE_SAMPLE_INFO_SIZE) / 2;
	offset = AWE_PATCH_INFO_SIZE;
	COPY_FROM_USER(rec, addr, offset, AWE_SAMPLE_INFO_SIZE);
	offset += AWE_SAMPLE_INFO_SIZE;
	if (size != rec->size) {
		printk("AWE32: load: sample size differed (%d != %d)\n",
		       rec->size, size);
		return RET_ERROR(EINVAL);
	}
	if (rec->size > 0)
		if ((rc = awe_write_wave_data(addr, offset, rec, -1)) != 0)
			return rc;

	rec->sf_id = current_sf_id;

	add_sf_sample(free_sample);

	return 0;
}


/* replace wave sample data */
static int
awe_replace_data(awe_patch_info *patch, const char *addr, int count)
{
	int offset;
	int size;
	int rc, i;
	int channels;
	awe_sample_info cursmp;
	int save_mem_ptr;

	if (! patch_opened) {
		printk("AWE32: replace: patch not opened\n");
		return RET_ERROR(EINVAL);
	}

	size = (count - AWE_SAMPLE_INFO_SIZE) / 2;
	offset = AWE_PATCH_INFO_SIZE;
	COPY_FROM_USER(&cursmp, addr, offset, AWE_SAMPLE_INFO_SIZE);
	offset += AWE_SAMPLE_INFO_SIZE;
	if (cursmp.size == 0 || size != cursmp.size) {
		printk("AWE32: replace: illegal sample size (%d!=%d)\n",
		       cursmp.size, size);
		return RET_ERROR(EINVAL);
	}
	channels = patch->optarg;
	if (channels <= 0 || channels > AWE_NORMAL_VOICES) {
		printk("AWE32: replace: illegal channels %d\n", channels);
		return RET_ERROR(EINVAL);
	}

	for (i = sflists[current_sf_id-1].samples;
	     i >= 0; i = samples[i].next) {
		if (samples[i].v.sample == cursmp.sample)
			break;
	}
	if (i < 0) {
		printk("AWE32: replace: cannot find existing sample data %d\n",
		       cursmp.sample);
		return RET_ERROR(EINVAL);
	}
		
	if (samples[i].v.size != cursmp.size) {
		printk("AWE32: replace: exiting size differed (%d!=%d)\n",
		       samples[i].v.size, cursmp.size);
		return RET_ERROR(EINVAL);
	}

	save_mem_ptr = awe_free_mem_ptr();
	sflists[current_sf_id-1].mem_ptr = samples[i].v.start - awe_mem_start;
	MEMCPY(&samples[i].v, &cursmp, sizeof(cursmp));
	if ((rc = awe_write_wave_data(addr, offset, &samples[i].v, channels)) != 0)
		return rc;
	sflists[current_sf_id-1].mem_ptr = save_mem_ptr;
	samples[i].v.sf_id = current_sf_id;

	return 0;
}


/*----------------------------------------------------------------*/

static const char *readbuf_addr;
static int readbuf_offs;
static int readbuf_flags;
#ifdef __FreeBSD__
static unsigned short *readbuf_loop;
static int readbuf_loopstart, readbuf_loopend;
#endif

/* initialize read buffer */
static int
readbuf_init(const char *addr, int offset, awe_sample_info *sp)
{
#ifdef __FreeBSD__
	readbuf_loop = NULL;
	readbuf_loopstart = sp->loopstart;
	readbuf_loopend = sp->loopend;
	if (sp->mode_flags & (AWE_SAMPLE_BIDIR_LOOP|AWE_SAMPLE_REVERSE_LOOP)) {
		int looplen = sp->loopend - sp->loopstart;
		readbuf_loop = my_malloc(looplen * 2);
		if (readbuf_loop == NULL) {
			printk("AWE32: can't malloc temp buffer\n");
			return RET_ERROR(ENOSPC);
		}
	}
#endif
	readbuf_addr = addr;
	readbuf_offs = offset;
	readbuf_flags = sp->mode_flags;
	return 0;
}

/* read directly from user buffer */
static unsigned short
readbuf_word(int pos)
{
	unsigned short c;
	/* read from user buffer */
	if (readbuf_flags & AWE_SAMPLE_8BITS) {
		unsigned char cc;
		GET_BYTE_FROM_USER(cc, readbuf_addr, readbuf_offs + pos);
		c = cc << 8; /* convert 8bit -> 16bit */
	} else {
		GET_SHORT_FROM_USER(c, readbuf_addr, readbuf_offs + pos * 2);
	}
	if (readbuf_flags & AWE_SAMPLE_UNSIGNED)
		c ^= 0x8000; /* unsigned -> signed */
#ifdef __FreeBSD__
	/* write on cache for reverse loop */
	if (readbuf_flags & (AWE_SAMPLE_BIDIR_LOOP|AWE_SAMPLE_REVERSE_LOOP)) {
		if (pos >= readbuf_loopstart && pos < readbuf_loopend)
			readbuf_loop[pos - readbuf_loopstart] = c;
	}
#endif
	return c;
}

#ifdef __FreeBSD__
/* read from cache */
static unsigned short
readbuf_word_cache(int pos)
{
	if (pos >= readbuf_loopstart && pos < readbuf_loopend)
		return readbuf_loop[pos - readbuf_loopstart];
	return 0;
}

static void
readbuf_end(void)
{
	if (readbuf_loop) {
		my_free(readbuf_loop);
	}
	readbuf_loop = NULL;
}

#else

#define readbuf_word_cache	readbuf_word
#define readbuf_end()		/**/

#endif

/*----------------------------------------------------------------*/

#define BLANK_LOOP_START	8
#define BLANK_LOOP_END		40
#define BLANK_LOOP_SIZE		48

/* loading onto memory */
static int 
awe_write_wave_data(const char *addr, int offset, awe_sample_info *sp, int channels)
{
	int i, truesize, dram_offset;
	int rc;

	/* be sure loop points start < end */
	if (sp->loopstart > sp->loopend) {
		int tmp = sp->loopstart;
		sp->loopstart = sp->loopend;
		sp->loopend = tmp;
	}

	/* compute true data size to be loaded */
	truesize = sp->size;
	if (sp->mode_flags & AWE_SAMPLE_BIDIR_LOOP)
		truesize += sp->loopend - sp->loopstart;
	if (sp->mode_flags & AWE_SAMPLE_NO_BLANK)
		truesize += BLANK_LOOP_SIZE;
	if (awe_free_mem_ptr() + truesize >= awe_mem_size/2) {
		printk("AWE32 Error: Sample memory full\n");
		return RET_ERROR(ENOSPC);
	}

	/* recalculate address offset */
	sp->end -= sp->start;
	sp->loopstart -= sp->start;
	sp->loopend -= sp->start;

	dram_offset = awe_free_mem_ptr() + awe_mem_start;
	sp->start = dram_offset;
	sp->end += dram_offset;
	sp->loopstart += dram_offset;
	sp->loopend += dram_offset;

	/* set the total size (store onto obsolete checksum value) */
	if (sp->size == 0)
		sp->checksum = 0;
	else
		sp->checksum = truesize;

	if ((rc = awe_open_dram_for_write(dram_offset, channels)) != 0)
		return rc;

	if (readbuf_init(addr, offset, sp) < 0)
		return RET_ERROR(ENOSPC);

	for (i = 0; i < sp->size; i++) {
		unsigned short c;
		c = readbuf_word(i);
		awe_write_dram(c);
		if (i == sp->loopend &&
		    (sp->mode_flags & (AWE_SAMPLE_BIDIR_LOOP|AWE_SAMPLE_REVERSE_LOOP))) {
			int looplen = sp->loopend - sp->loopstart;
			/* copy reverse loop */
			int k;
			for (k = 1; k <= looplen; k++) {
				c = readbuf_word_cache(i - k);
				awe_write_dram(c);
			}
			if (sp->mode_flags & AWE_SAMPLE_BIDIR_LOOP) {
				sp->end += looplen;
			} else {
				sp->start += looplen;
				sp->end += looplen;
			}
		}
	}
	readbuf_end();

	/* if no blank loop is attached in the sample, add it */
	if (sp->mode_flags & AWE_SAMPLE_NO_BLANK) {
		for (i = 0; i < BLANK_LOOP_SIZE; i++)
			awe_write_dram(0);
		if (sp->mode_flags & AWE_SAMPLE_SINGLESHOT) {
			sp->loopstart = sp->end + BLANK_LOOP_START;
			sp->loopend = sp->end + BLANK_LOOP_END;
		}
	}

	sflists[current_sf_id-1].mem_ptr += truesize;
	awe_close_dram();

	/* initialize FM */
	awe_init_fm();

	return 0;
}


/*----------------------------------------------------------------*/

#ifdef AWE_HAS_GUS_COMPATIBILITY

/* calculate GUS envelope time:
 * is this correct?  i have no idea..
 */
static int
calc_gus_envelope_time(int rate, int start, int end)
{
	int r, p, t;
	r = (3 - ((rate >> 6) & 3)) * 3;
	p = rate & 0x3f;
	t = end - start;
	if (t < 0) t = -t;
	if (13 > r)
		t = t << (13 - r);
	else
		t = t >> (r - 13);
	return (t * 10) / (p * 441);
}

#define calc_gus_sustain(val)  (0x7f - vol_table[(val)/2])
#define calc_gus_attenuation(val)	vol_table[(val)/2]

/* load GUS patch */
static int
awe_load_guspatch(const char *addr, int offs, int size, int pmgr_flag)
{
	struct patch_info patch;
	awe_voice_info *rec;
	awe_sample_info *smp;
	int sizeof_patch;
	int note, free_sample, free_info;
	int rc;

	sizeof_patch = (int)((long)&patch.data[0] - (long)&patch); /* header size */
	if (size < sizeof_patch) {
		printk("AWE32 Error: Patch header too short\n");
		return RET_ERROR(EINVAL);
	}
	COPY_FROM_USER(((char*)&patch) + offs, addr, offs, sizeof_patch - offs);
	size -= sizeof_patch;
	if (size < patch.len) {
		printk("AWE32 Warning: Patch record too short (%d<%d)\n",
		       size, patch.len);
		return RET_ERROR(EINVAL);
	}
	if (check_patch_opened(AWE_PAT_TYPE_GUS, NULL) < 0)
		return RET_ERROR(ENOSPC);
	if (alloc_new_sample() < 0)
		return RET_ERROR(ENOSPC);
	if (alloc_new_info(1))
		return RET_ERROR(ENOSPC);

	free_sample = awe_free_sample();
	smp = &samples[free_sample].v;

	smp->sample = free_sample;
	smp->start = 0;
	smp->end = patch.len;
	smp->loopstart = patch.loop_start;
	smp->loopend = patch.loop_end;
	smp->size = patch.len;

	/* set up mode flags */
	smp->mode_flags = 0;
	if (!(patch.mode & WAVE_16_BITS))
		smp->mode_flags |= AWE_SAMPLE_8BITS;
	if (patch.mode & WAVE_UNSIGNED)
		smp->mode_flags |= AWE_SAMPLE_UNSIGNED;
	smp->mode_flags |= AWE_SAMPLE_NO_BLANK;
	if (!(patch.mode & (WAVE_LOOPING|WAVE_BIDIR_LOOP|WAVE_LOOP_BACK)))
		smp->mode_flags |= AWE_SAMPLE_SINGLESHOT;
	if (patch.mode & WAVE_BIDIR_LOOP)
		smp->mode_flags |= AWE_SAMPLE_BIDIR_LOOP;
	if (patch.mode & WAVE_LOOP_BACK)
		smp->mode_flags |= AWE_SAMPLE_REVERSE_LOOP;

	DEBUG(0,printk("AWE32: [sample %d mode %x]\n", patch.instr_no, smp->mode_flags));
	if (patch.mode & WAVE_16_BITS) {
		/* convert to word offsets */
		smp->size /= 2;
		smp->end /= 2;
		smp->loopstart /= 2;
		smp->loopend /= 2;
	}
	smp->checksum_flag = 0;
	smp->checksum = 0;

	if ((rc = awe_write_wave_data(addr, sizeof_patch, smp, -1)) != 0)
		return rc;

	smp->sf_id = current_sf_id;
	add_sf_sample(free_sample);

	/* set up voice info */
	free_info = awe_free_info();
	rec = &infos[free_info].v;
	awe_init_voice_info(rec);
	rec->sample = free_sample; /* the last sample */
	rec->rate_offset = calc_rate_offset(patch.base_freq);
	note = freq_to_note(patch.base_note);
	rec->root = note / 100;
	rec->tune = -(note % 100);
	rec->low = freq_to_note(patch.low_note) / 100;
	rec->high = freq_to_note(patch.high_note) / 100;
	DEBUG(1,printk("AWE32: [gus base offset=%d, note=%d, range=%d-%d(%d-%d)]\n",
		       rec->rate_offset, note,
		       rec->low, rec->high,
	      patch.low_note, patch.high_note));
	/* panning position; -128 - 127 => 0-127 */
	rec->pan = (patch.panning + 128) / 2;

	/* detuning is ignored */
	/* 6points volume envelope */
	if (patch.mode & WAVE_ENVELOPES) {
		int attack, hold, decay, release;
		attack = calc_gus_envelope_time
			(patch.env_rate[0], 0, patch.env_offset[0]);
		hold = calc_gus_envelope_time
			(patch.env_rate[1], patch.env_offset[0],
			 patch.env_offset[1]);
		decay = calc_gus_envelope_time
			(patch.env_rate[2], patch.env_offset[1],
			 patch.env_offset[2]);
		release = calc_gus_envelope_time
			(patch.env_rate[3], patch.env_offset[1],
			 patch.env_offset[4]);
		release += calc_gus_envelope_time
			(patch.env_rate[4], patch.env_offset[3],
			 patch.env_offset[4]);
		release += calc_gus_envelope_time
			(patch.env_rate[5], patch.env_offset[4],
			 patch.env_offset[5]);
		rec->parm.volatkhld = (calc_parm_attack(attack) << 8) |
			calc_parm_hold(hold);
		rec->parm.voldcysus = (calc_gus_sustain(patch.env_offset[2]) << 8) |
			calc_parm_decay(decay);
		rec->parm.volrelease = 0x8000 | calc_parm_decay(release);
		DEBUG(2,printk("AWE32: [gusenv atk=%d, hld=%d, dcy=%d, rel=%d]\n", attack, hold, decay, release));
		rec->attenuation = calc_gus_attenuation(patch.env_offset[0]);
	}

	/* tremolo effect */
	if (patch.mode & WAVE_TREMOLO) {
		int rate = (patch.tremolo_rate * 1000 / 38) / 42;
		rec->parm.tremfrq = ((patch.tremolo_depth / 2) << 8) | rate;
		DEBUG(2,printk("AWE32: [gusenv tremolo rate=%d, dep=%d, tremfrq=%x]\n",
			       patch.tremolo_rate, patch.tremolo_depth,
			       rec->parm.tremfrq));
	}
	/* vibrato effect */
	if (patch.mode & WAVE_VIBRATO) {
		int rate = (patch.vibrato_rate * 1000 / 38) / 42;
		rec->parm.fm2frq2 = ((patch.vibrato_depth / 6) << 8) | rate;
		DEBUG(2,printk("AWE32: [gusenv vibrato rate=%d, dep=%d, tremfrq=%x]\n",
			       patch.tremolo_rate, patch.tremolo_depth,
			       rec->parm.tremfrq));
	}
	
	/* scale_freq, scale_factor, volume, and fractions not implemented */

	/* append to the tail of the list */
	infos[free_info].bank = misc_modes[AWE_MD_GUS_BANK];
	infos[free_info].instr = patch.instr_no;
	infos[free_info].disabled = FALSE;
	infos[free_info].type = V_ST_NORMAL;
	infos[free_info].v.sf_id = current_sf_id;

	add_info_list(free_info);
	add_sf_info(free_info);

	/* set the voice index */
	awe_set_sample(rec);

	return 0;
}

#endif  /* AWE_HAS_GUS_COMPATIBILITY */

/*----------------------------------------------------------------
 * sample and voice list handlers
 *----------------------------------------------------------------*/

/* append this to the sf list */
static void add_sf_info(int rec)
{
	int sf_id = infos[rec].v.sf_id;
	if (sf_id == 0) return;
	sf_id--;
	if (sflists[sf_id].infos < 0)
		sflists[sf_id].infos = rec;
	else {
		int i, prev;
		prev = sflists[sf_id].infos;
		while ((i = infos[prev].next) >= 0)
			prev = i;
		infos[prev].next = rec;
	}
	infos[rec].next = -1;
	sflists[sf_id].num_info++;
}

/* prepend this sample to sf list */
static void add_sf_sample(int rec)
{
	int sf_id = samples[rec].v.sf_id;
	if (sf_id == 0) return;
	sf_id--;
	samples[rec].next = sflists[sf_id].samples;
	sflists[sf_id].samples = rec;
	sflists[sf_id].num_sample++;
}

/* purge the old records which don't belong with the same file id */
static void purge_old_list(int rec, int next)
{
	infos[rec].next_instr = next;
	if (infos[rec].bank == AWE_DRUM_BANK) {
		/* remove samples with the same note range */
		int cur, *prevp = &infos[rec].next_instr;
		int low = infos[rec].v.low;
		int high = infos[rec].v.high;
		for (cur = next; cur >= 0; cur = infos[cur].next_instr) {
			if (infos[cur].v.low == low &&
			    infos[cur].v.high == high &&
			    infos[cur].v.sf_id != infos[rec].v.sf_id)
				*prevp = infos[cur].next_instr;
			prevp = &infos[cur].next_instr;
		}
	} else {
		if (infos[next].v.sf_id != infos[rec].v.sf_id)
			infos[rec].next_instr = -1;
	}
}

/* prepend to top of the preset table */
static void add_info_list(int rec)
{
	int *prevp, cur;
	int instr = infos[rec].instr;
	int bank = infos[rec].bank;

	if (infos[rec].disabled)
		return;

	prevp = &preset_table[instr];
	cur = *prevp;
	while (cur >= 0) {
		/* search the first record with the same bank number */
		if (infos[cur].bank == bank) {
			/* replace the list with the new record */
			infos[rec].next_bank = infos[cur].next_bank;
			*prevp = rec;
			purge_old_list(rec, cur);
			return;
		}
		prevp = &infos[cur].next_bank;
		cur = infos[cur].next_bank;
	}

	/* this is the first bank record.. just add this */
	infos[rec].next_instr = -1;
	infos[rec].next_bank = preset_table[instr];
	preset_table[instr] = rec;
}

/* remove samples later than the specified sf_id */
static void
awe_remove_samples(int sf_id)
{
	if (sf_id <= 0) {
		awe_reset_samples();
		return;
	}
	/* already removed? */
	if (current_sf_id <= sf_id)
		return;

	current_sf_id = sf_id;
	if (locked_sf_id > sf_id)
		locked_sf_id = sf_id;

	rebuild_preset_list();
}

/* rebuild preset search list */
static void rebuild_preset_list(void)
{
	int i, j;

	for (i = 0; i < AWE_MAX_PRESETS; i++)
		preset_table[i] = -1;

	for (i = 0; i < current_sf_id; i++) {
		for (j = sflists[i].infos; j >= 0; j = infos[j].next)
			add_info_list(j);
	}
}

/* search the specified sample */
static short
awe_set_sample(awe_voice_info *vp)
{
	int i;
	vp->index = -1;
	for (i = sflists[vp->sf_id-1].samples; i >= 0; i = samples[i].next) {
		if (samples[i].v.sample == vp->sample) {
			/* set the actual sample offsets */
			vp->start += samples[i].v.start;
			vp->end += samples[i].v.end;
			vp->loopstart += samples[i].v.loopstart;
			vp->loopend += samples[i].v.loopend;
			/* copy mode flags */
			vp->mode = samples[i].v.mode_flags;
			/* set index */
			vp->index = i;
			return i;
		}
	}
	return -1;
}


/*----------------------------------------------------------------
 * voice allocation
 *----------------------------------------------------------------*/

/* look for all voices associated with the specified note & velocity */
static int
awe_search_multi_voices(int rec, int note, int velocity, awe_voice_info **vlist)
{
	int nvoices;

	nvoices = 0;
	for (; rec >= 0; rec = infos[rec].next_instr) {
		if (note >= infos[rec].v.low &&
		    note <= infos[rec].v.high &&
		    velocity >= infos[rec].v.vellow &&
		    velocity <= infos[rec].v.velhigh) {
			vlist[nvoices] = &infos[rec].v;
			if (infos[rec].type == V_ST_MAPPED) /* mapper */
				return -1;
			nvoices++;
			if (nvoices >= AWE_MAX_VOICES)
				break;
		}
	}
	return nvoices;	
}

/* store the voice list from the specified note and velocity.
   if the preset is mapped, seek for the destination preset, and rewrite
   the note number if necessary.
   */
static int
really_alloc_voices(int vrec, int def_vrec, int *note, int velocity, awe_voice_info **vlist, int level)
{
	int nvoices;

	nvoices = awe_search_multi_voices(vrec, *note, velocity, vlist);
	if (nvoices == 0)
		nvoices = awe_search_multi_voices(def_vrec, *note, velocity, vlist);
	if (nvoices < 0) { /* mapping */
		int preset = vlist[0]->start;
		int bank = vlist[0]->end;
		int key = vlist[0]->fixkey;
		if (level > 5) {
			printk("AWE32: too deep mapping level\n");
			return 0;
		}
		vrec = awe_search_instr(bank, preset);
		if (bank == AWE_DRUM_BANK)
			def_vrec = awe_search_instr(bank, 0);
		else
			def_vrec = awe_search_instr(0, preset);
		if (key >= 0)
			*note = key;
		return really_alloc_voices(vrec, def_vrec, note, velocity, vlist, level+1);
	}

	return nvoices;
}

/* allocate voices corresponding note and velocity; supports multiple insts. */
static void
awe_alloc_multi_voices(int ch, int note, int velocity, int key)
{
	int i, v, nvoices;
	awe_voice_info *vlist[AWE_MAX_VOICES];

	if (channels[ch].vrec < 0 && channels[ch].def_vrec < 0)
		awe_set_instr(0, ch, channels[ch].instr);

	/* check the possible voices; note may be changeable if mapped */
	nvoices = really_alloc_voices(channels[ch].vrec, channels[ch].def_vrec,
				      &note, velocity, vlist, 0);

	/* set the voices */
	current_alloc_time++;
	for (i = 0; i < nvoices; i++) {
		v = awe_clear_voice();
		voices[v].key = key;
		voices[v].ch = ch;
		voices[v].note = note;
		voices[v].velocity = velocity;
		voices[v].time = current_alloc_time;
		voices[v].cinfo = &channels[ch];
		voices[v].sample = vlist[i];
		voices[v].state = AWE_ST_MARK;
		voices[v].layer = nvoices - i - 1;  /* in reverse order */
	}

	/* clear the mark in allocated voices */
	for (i = 0; i < awe_max_voices; i++) {
		if (voices[i].state == AWE_ST_MARK)
			voices[i].state = AWE_ST_OFF;
			
	}
}


/* search the best voice from the specified status condition */
static int
search_best_voice(int condition)
{
	int i, time, best;
	best = -1;
	time = current_alloc_time + 1;
	for (i = 0; i < awe_max_voices; i++) {
		if ((voices[i].state & condition) &&
		    (best < 0 || voices[i].time < time)) {
			best = i;
			time = voices[i].time;
		}
	}
	/* clear voice */
	if (best >= 0) {
		if (voices[best].state != AWE_ST_OFF)
			awe_terminate(best);
		awe_voice_init(best, TRUE);
	}

	return best;
}

/* search an empty voice.
   if no empty voice is found, at least terminate a voice
   */
static int
awe_clear_voice(void)
{
	int best;

	/* looking for the oldest empty voice */
	if ((best = search_best_voice(AWE_ST_OFF)) >= 0)
		return best;
	if ((best = search_best_voice(AWE_ST_RELEASED)) >= 0)
		return best;
	/* looking for the oldest sustained voice */
	if ((best = search_best_voice(AWE_ST_SUSTAINED)) >= 0)
		return best;

#ifdef AWE_LOOKUP_MIDI_PRIORITY
	if (MULTI_LAYER_MODE() && misc_modes[AWE_MD_CHN_PRIOR]) {
		int ch = -1;
		int time = current_alloc_time + 1;
		int i;
		/* looking for the voices from high channel (except drum ch) */
		for (i = 0; i < awe_max_voices; i++) {
			if (IS_DRUM_CHANNEL(voices[i].ch)) continue;
			if (voices[i].ch < ch) continue;
			if (voices[i].state != AWE_ST_MARK &&
			    (voices[i].ch > ch || voices[i].time < time)) {
				best = i;
				time = voices[i].time;
				ch = voices[i].ch;
			}
		}
	}
#endif
	if (best < 0)
		best = search_best_voice(~AWE_ST_MARK);

	if (best >= 0)
		return best;

	return 0;
}


/* search sample for the specified note & velocity and set it on the voice;
 * note that voice is the voice index (not channel index)
 */
static void
awe_alloc_one_voice(int voice, int note, int velocity)
{
	int ch, nvoices;
	awe_voice_info *vlist[AWE_MAX_VOICES];

	ch = voices[voice].ch;
	if (channels[ch].vrec < 0 && channels[ch].def_vrec < 0)
		awe_set_instr(0, ch, channels[ch].instr);

	nvoices = really_alloc_voices(voices[voice].cinfo->vrec,
				      voices[voice].cinfo->def_vrec,
				      &note, velocity, vlist, 0);
	if (nvoices > 0) {
		voices[voice].time = ++current_alloc_time;
		voices[voice].sample = vlist[0]; /* use the first one */
		voices[voice].layer = 0;
		voices[voice].note = note;
		voices[voice].velocity = velocity;
	}
}


/*----------------------------------------------------------------
 * sequencer2 functions
 *----------------------------------------------------------------*/

/* search an empty voice; used by sequencer2 */
static int
awe_alloc(int dev, int chn, int note, struct voice_alloc_info *alloc)
{
	playing_mode = AWE_PLAY_MULTI2;
	awe_info.nr_voices = AWE_MAX_CHANNELS;
	return awe_clear_voice();
}


/* set up voice; used by sequencer2 */
static void
awe_setup_voice(int dev, int voice, int chn)
{
	struct channel_info *info;
	if (synth_devs[dev] == NULL ||
	    (info = &synth_devs[dev]->chn_info[chn]) == NULL)
		return;

	if (voice < 0 || voice >= awe_max_voices)
		return;

	DEBUG(2,printk("AWE32: [setup(%d) ch=%d]\n", voice, chn));
	channels[chn].expression_vol = info->controllers[CTL_EXPRESSION];
	channels[chn].main_vol = info->controllers[CTL_MAIN_VOLUME];
	channels[chn].panning =
		info->controllers[CTL_PAN] * 2 - 128; /* signed 8bit */
	channels[chn].bender = info->bender_value; /* zero center */
	channels[chn].bank = info->controllers[CTL_BANK_SELECT];
	channels[chn].sustained = info->controllers[CTL_SUSTAIN];
	if (info->controllers[CTL_EXT_EFF_DEPTH]) {
		FX_SET(&channels[chn].fx, AWE_FX_REVERB,
		       info->controllers[CTL_EXT_EFF_DEPTH] * 2);
	}
	if (info->controllers[CTL_CHORUS_DEPTH]) {
		FX_SET(&channels[chn].fx, AWE_FX_CHORUS,
		       info->controllers[CTL_CHORUS_DEPTH] * 2);
	}
	awe_set_instr(dev, chn, info->pgm_num);
}


#ifdef CONFIG_AWE32_MIXER
/*================================================================
 * AWE32 mixer device control
 *================================================================*/

static int
awe_mixer_ioctl(int dev, unsigned int cmd, caddr_t arg)
{
	int i, level;

	if (((cmd >> 8) & 0xff) != 'M')
		return RET_ERROR(EINVAL);

	level = (int)IOCTL_IN(arg);
	level = ((level & 0xff) + (level >> 8)) / 2;
	DEBUG(0,printk("AWEMix: cmd=%x val=%d\n", cmd & 0xff, level));

	if (IO_WRITE_CHECK(cmd)) {
		switch (cmd & 0xff) {
		case SOUND_MIXER_BASS:
			awe_bass_level = level * 12 / 100;
			if (awe_bass_level >= 12)
				awe_bass_level = 11;
			awe_equalizer(awe_bass_level, awe_treble_level);
			break;
		case SOUND_MIXER_TREBLE:
			awe_treble_level = level * 12 / 100;
			if (awe_treble_level >= 12)
				awe_treble_level = 11;
			awe_equalizer(awe_bass_level, awe_treble_level);
			break;
		case SOUND_MIXER_VOLUME:
			level = level * 127 / 100;
			if (level >= 128) level = 127;
			init_atten = vol_table[level];
			for (i = 0; i < awe_max_voices; i++)
				awe_set_voice_vol(i, TRUE);
			break;
		}
	}
	switch (cmd & 0xff) {
	case SOUND_MIXER_BASS:
		level = awe_bass_level * 100 / 24;
		level = (level << 8) | level;
		break;
	case SOUND_MIXER_TREBLE:
		level = awe_treble_level * 100 / 24;
		level = (level << 8) | level;
		break;
	case SOUND_MIXER_VOLUME:
		for (i = 127; i > 0; i--) {
			if (init_atten <= vol_table[i])
				break;
		}
		level = i * 100 / 127;
		level = (level << 8) | level;
		break;
	case SOUND_MIXER_DEVMASK:
		level = SOUND_MASK_BASS|SOUND_MASK_TREBLE|SOUND_MASK_VOLUME;
		break;
	default:
		level = 0;
		break;
	}
	return IOCTL_OUT(arg, level);
}
#endif /* CONFIG_AWE32_MIXER */


/*================================================================
 * initialization of AWE32
 *================================================================*/

/* intiailize audio channels */
static void
awe_init_audio(void)
{
	int ch;

	/* turn off envelope engines */
	for (ch = 0; ch < AWE_MAX_VOICES; ch++) {
		awe_poke(AWE_DCYSUSV(ch), 0x80);
	}
  
	/* reset all other parameters to zero */
	for (ch = 0; ch < AWE_MAX_VOICES; ch++) {
		awe_poke(AWE_ENVVOL(ch), 0);
		awe_poke(AWE_ENVVAL(ch), 0);
		awe_poke(AWE_DCYSUS(ch), 0);
		awe_poke(AWE_ATKHLDV(ch), 0);
		awe_poke(AWE_LFO1VAL(ch), 0);
		awe_poke(AWE_ATKHLD(ch), 0);
		awe_poke(AWE_LFO2VAL(ch), 0);
		awe_poke(AWE_IP(ch), 0);
		awe_poke(AWE_IFATN(ch), 0);
		awe_poke(AWE_PEFE(ch), 0);
		awe_poke(AWE_FMMOD(ch), 0);
		awe_poke(AWE_TREMFRQ(ch), 0);
		awe_poke(AWE_FM2FRQ2(ch), 0);
		awe_poke_dw(AWE_PTRX(ch), 0);
		awe_poke_dw(AWE_VTFT(ch), 0);
		awe_poke_dw(AWE_PSST(ch), 0);
		awe_poke_dw(AWE_CSL(ch), 0);
		awe_poke_dw(AWE_CCCA(ch), 0);
	}

	for (ch = 0; ch < AWE_MAX_VOICES; ch++) {
		awe_poke_dw(AWE_CPF(ch), 0);
		awe_poke_dw(AWE_CVCF(ch), 0);
	}
}


/* initialize DMA address */
static void
awe_init_dma(void)
{
	awe_poke_dw(AWE_SMALR, 0);
	awe_poke_dw(AWE_SMARR, 0);
	awe_poke_dw(AWE_SMALW, 0);
	awe_poke_dw(AWE_SMARW, 0);
}


/* initialization arrays; from ADIP */

static unsigned short init1[128] = {
	0x03ff, 0x0030,  0x07ff, 0x0130, 0x0bff, 0x0230,  0x0fff, 0x0330,
	0x13ff, 0x0430,  0x17ff, 0x0530, 0x1bff, 0x0630,  0x1fff, 0x0730,
	0x23ff, 0x0830,  0x27ff, 0x0930, 0x2bff, 0x0a30,  0x2fff, 0x0b30,
	0x33ff, 0x0c30,  0x37ff, 0x0d30, 0x3bff, 0x0e30,  0x3fff, 0x0f30,

	0x43ff, 0x0030,  0x47ff, 0x0130, 0x4bff, 0x0230,  0x4fff, 0x0330,
	0x53ff, 0x0430,  0x57ff, 0x0530, 0x5bff, 0x0630,  0x5fff, 0x0730,
	0x63ff, 0x0830,  0x67ff, 0x0930, 0x6bff, 0x0a30,  0x6fff, 0x0b30,
	0x73ff, 0x0c30,  0x77ff, 0x0d30, 0x7bff, 0x0e30,  0x7fff, 0x0f30,

	0x83ff, 0x0030,  0x87ff, 0x0130, 0x8bff, 0x0230,  0x8fff, 0x0330,
	0x93ff, 0x0430,  0x97ff, 0x0530, 0x9bff, 0x0630,  0x9fff, 0x0730,
	0xa3ff, 0x0830,  0xa7ff, 0x0930, 0xabff, 0x0a30,  0xafff, 0x0b30,
	0xb3ff, 0x0c30,  0xb7ff, 0x0d30, 0xbbff, 0x0e30,  0xbfff, 0x0f30,

	0xc3ff, 0x0030,  0xc7ff, 0x0130, 0xcbff, 0x0230,  0xcfff, 0x0330,
	0xd3ff, 0x0430,  0xd7ff, 0x0530, 0xdbff, 0x0630,  0xdfff, 0x0730,
	0xe3ff, 0x0830,  0xe7ff, 0x0930, 0xebff, 0x0a30,  0xefff, 0x0b30,
	0xf3ff, 0x0c30,  0xf7ff, 0x0d30, 0xfbff, 0x0e30,  0xffff, 0x0f30,
};

static unsigned short init2[128] = {
	0x03ff, 0x8030, 0x07ff, 0x8130, 0x0bff, 0x8230, 0x0fff, 0x8330,
	0x13ff, 0x8430, 0x17ff, 0x8530, 0x1bff, 0x8630, 0x1fff, 0x8730,
	0x23ff, 0x8830, 0x27ff, 0x8930, 0x2bff, 0x8a30, 0x2fff, 0x8b30,
	0x33ff, 0x8c30, 0x37ff, 0x8d30, 0x3bff, 0x8e30, 0x3fff, 0x8f30,

	0x43ff, 0x8030, 0x47ff, 0x8130, 0x4bff, 0x8230, 0x4fff, 0x8330,
	0x53ff, 0x8430, 0x57ff, 0x8530, 0x5bff, 0x8630, 0x5fff, 0x8730,
	0x63ff, 0x8830, 0x67ff, 0x8930, 0x6bff, 0x8a30, 0x6fff, 0x8b30,
	0x73ff, 0x8c30, 0x77ff, 0x8d30, 0x7bff, 0x8e30, 0x7fff, 0x8f30,

	0x83ff, 0x8030, 0x87ff, 0x8130, 0x8bff, 0x8230, 0x8fff, 0x8330,
	0x93ff, 0x8430, 0x97ff, 0x8530, 0x9bff, 0x8630, 0x9fff, 0x8730,
	0xa3ff, 0x8830, 0xa7ff, 0x8930, 0xabff, 0x8a30, 0xafff, 0x8b30,
	0xb3ff, 0x8c30, 0xb7ff, 0x8d30, 0xbbff, 0x8e30, 0xbfff, 0x8f30,

	0xc3ff, 0x8030, 0xc7ff, 0x8130, 0xcbff, 0x8230, 0xcfff, 0x8330,
	0xd3ff, 0x8430, 0xd7ff, 0x8530, 0xdbff, 0x8630, 0xdfff, 0x8730,
	0xe3ff, 0x8830, 0xe7ff, 0x8930, 0xebff, 0x8a30, 0xefff, 0x8b30,
	0xf3ff, 0x8c30, 0xf7ff, 0x8d30, 0xfbff, 0x8e30, 0xffff, 0x8f30,
};

static unsigned short init3[128] = {
	0x0C10, 0x8470, 0x14FE, 0xB488, 0x167F, 0xA470, 0x18E7, 0x84B5,
	0x1B6E, 0x842A, 0x1F1D, 0x852A, 0x0DA3, 0x8F7C, 0x167E, 0xF254,
	0x0000, 0x842A, 0x0001, 0x852A, 0x18E6, 0x8BAA, 0x1B6D, 0xF234,
	0x229F, 0x8429, 0x2746, 0x8529, 0x1F1C, 0x86E7, 0x229E, 0xF224,

	0x0DA4, 0x8429, 0x2C29, 0x8529, 0x2745, 0x87F6, 0x2C28, 0xF254,
	0x383B, 0x8428, 0x320F, 0x8528, 0x320E, 0x8F02, 0x1341, 0xF264,
	0x3EB6, 0x8428, 0x3EB9, 0x8528, 0x383A, 0x8FA9, 0x3EB5, 0xF294,
	0x3EB7, 0x8474, 0x3EBA, 0x8575, 0x3EB8, 0xC4C3, 0x3EBB, 0xC5C3,

	0x0000, 0xA404, 0x0001, 0xA504, 0x141F, 0x8671, 0x14FD, 0x8287,
	0x3EBC, 0xE610, 0x3EC8, 0x8C7B, 0x031A, 0x87E6, 0x3EC8, 0x86F7,
	0x3EC0, 0x821E, 0x3EBE, 0xD208, 0x3EBD, 0x821F, 0x3ECA, 0x8386,
	0x3EC1, 0x8C03, 0x3EC9, 0x831E, 0x3ECA, 0x8C4C, 0x3EBF, 0x8C55,

	0x3EC9, 0xC208, 0x3EC4, 0xBC84, 0x3EC8, 0x8EAD, 0x3EC8, 0xD308,
	0x3EC2, 0x8F7E, 0x3ECB, 0x8219, 0x3ECB, 0xD26E, 0x3EC5, 0x831F,
	0x3EC6, 0xC308, 0x3EC3, 0xB2FF, 0x3EC9, 0x8265, 0x3EC9, 0x8319,
	0x1342, 0xD36E, 0x3EC7, 0xB3FF, 0x0000, 0x8365, 0x1420, 0x9570,
};

static unsigned short init4[128] = {
	0x0C10, 0x8470, 0x14FE, 0xB488, 0x167F, 0xA470, 0x18E7, 0x84B5,
	0x1B6E, 0x842A, 0x1F1D, 0x852A, 0x0DA3, 0x0F7C, 0x167E, 0x7254,
	0x0000, 0x842A, 0x0001, 0x852A, 0x18E6, 0x0BAA, 0x1B6D, 0x7234,
	0x229F, 0x8429, 0x2746, 0x8529, 0x1F1C, 0x06E7, 0x229E, 0x7224,

	0x0DA4, 0x8429, 0x2C29, 0x8529, 0x2745, 0x07F6, 0x2C28, 0x7254,
	0x383B, 0x8428, 0x320F, 0x8528, 0x320E, 0x0F02, 0x1341, 0x7264,
	0x3EB6, 0x8428, 0x3EB9, 0x8528, 0x383A, 0x0FA9, 0x3EB5, 0x7294,
	0x3EB7, 0x8474, 0x3EBA, 0x8575, 0x3EB8, 0x44C3, 0x3EBB, 0x45C3,

	0x0000, 0xA404, 0x0001, 0xA504, 0x141F, 0x0671, 0x14FD, 0x0287,
	0x3EBC, 0xE610, 0x3EC8, 0x0C7B, 0x031A, 0x07E6, 0x3EC8, 0x86F7,
	0x3EC0, 0x821E, 0x3EBE, 0xD208, 0x3EBD, 0x021F, 0x3ECA, 0x0386,
	0x3EC1, 0x0C03, 0x3EC9, 0x031E, 0x3ECA, 0x8C4C, 0x3EBF, 0x0C55,

	0x3EC9, 0xC208, 0x3EC4, 0xBC84, 0x3EC8, 0x0EAD, 0x3EC8, 0xD308,
	0x3EC2, 0x8F7E, 0x3ECB, 0x0219, 0x3ECB, 0xD26E, 0x3EC5, 0x031F,
	0x3EC6, 0xC308, 0x3EC3, 0x32FF, 0x3EC9, 0x0265, 0x3EC9, 0x8319,
	0x1342, 0xD36E, 0x3EC7, 0x33FF, 0x0000, 0x8365, 0x1420, 0x9570,
};


/* send initialization arrays to start up */
static void
awe_init_array(void)
{
	awe_send_array(init1);
	awe_wait(1024);
	awe_send_array(init2);
	awe_send_array(init3);
	awe_poke_dw(AWE_HWCF4, 0);
	awe_poke_dw(AWE_HWCF5, 0x83);
	awe_poke_dw(AWE_HWCF6, 0x8000);
	awe_send_array(init4);
}

/* send an initialization array */
static void
awe_send_array(unsigned short *data)
{
	int i;
	unsigned short *p;

	p = data;
	for (i = 0; i < AWE_MAX_VOICES; i++, p++)
		awe_poke(AWE_INIT1(i), *p);
	for (i = 0; i < AWE_MAX_VOICES; i++, p++)
		awe_poke(AWE_INIT2(i), *p);
	for (i = 0; i < AWE_MAX_VOICES; i++, p++)
		awe_poke(AWE_INIT3(i), *p);
	for (i = 0; i < AWE_MAX_VOICES; i++, p++)
		awe_poke(AWE_INIT4(i), *p);
}


/*
 * set up awe32 channels to some known state.
 */

/* set the envelope & LFO parameters to the default values; see ADIP */
static void
awe_tweak_voice(int i)
{
	/* set all mod/vol envelope shape to minimum */
	awe_poke(AWE_ENVVOL(i), 0x8000);
	awe_poke(AWE_ENVVAL(i), 0x8000);
	awe_poke(AWE_DCYSUS(i), 0x7F7F);
	awe_poke(AWE_ATKHLDV(i), 0x7F7F);
	awe_poke(AWE_ATKHLD(i), 0x7F7F);
	awe_poke(AWE_PEFE(i), 0);  /* mod envelope height to zero */
	awe_poke(AWE_LFO1VAL(i), 0x8000); /* no delay for LFO1 */
	awe_poke(AWE_LFO2VAL(i), 0x8000);
	awe_poke(AWE_IP(i), 0xE000);	/* no pitch shift */
	awe_poke(AWE_IFATN(i), 0xFF00);	/* volume to minimum */
	awe_poke(AWE_FMMOD(i), 0);
	awe_poke(AWE_TREMFRQ(i), 0);
	awe_poke(AWE_FM2FRQ2(i), 0);
}

static void
awe_tweak(void)
{
	int i;
	/* reset all channels */
	for (i = 0; i < awe_max_voices; i++)
		awe_tweak_voice(i);
}


/*
 *  initializes the FM section of AWE32;
 *   see Vince Vu's unofficial AWE32 programming guide
 */

static void
awe_init_fm(void)
{
#ifndef AWE_ALWAYS_INIT_FM
	/* if no extended memory is on board.. */
	if (awe_mem_size <= 0)
		return;
#endif
	DEBUG(3,printk("AWE32: initializing FM\n"));

	/* Initialize the last two channels for DRAM refresh and producing
	   the reverb and chorus effects for Yamaha OPL-3 synthesizer */

	/* 31: FM left channel, 0xffffe0-0xffffe8 */
	awe_poke(AWE_DCYSUSV(30), 0x80);
	awe_poke_dw(AWE_PSST(30), 0xFFFFFFE0); /* full left */
	awe_poke_dw(AWE_CSL(30), 0x00FFFFE8 |
		    (DEF_FM_CHORUS_DEPTH << 24));
	awe_poke_dw(AWE_PTRX(30), (DEF_FM_REVERB_DEPTH << 8));
	awe_poke_dw(AWE_CPF(30), 0);
	awe_poke_dw(AWE_CCCA(30), 0x00FFFFE3);

	/* 32: FM right channel, 0xfffff0-0xfffff8 */
	awe_poke(AWE_DCYSUSV(31), 0x80);
	awe_poke_dw(AWE_PSST(31), 0x00FFFFF0); /* full right */
	awe_poke_dw(AWE_CSL(31), 0x00FFFFF8 |
		    (DEF_FM_CHORUS_DEPTH << 24));
	awe_poke_dw(AWE_PTRX(31), (DEF_FM_REVERB_DEPTH << 8));
	awe_poke_dw(AWE_CPF(31), 0x8000);
	awe_poke_dw(AWE_CCCA(31), 0x00FFFFF3);

	/* skew volume & cutoff */
	awe_poke_dw(AWE_VTFT(30), 0x8000FFFF);
	awe_poke_dw(AWE_VTFT(31), 0x8000FFFF);

	voices[30].state = AWE_ST_FM;
	voices[31].state = AWE_ST_FM;

	/* change maximum channels to 30 */
	awe_max_voices = AWE_NORMAL_VOICES;
	if (playing_mode == AWE_PLAY_DIRECT)
		awe_info.nr_voices = awe_max_voices;
	else
		awe_info.nr_voices = AWE_MAX_CHANNELS;
	voice_alloc->max_voice = awe_max_voices;
}

/*
 *  AWE32 DRAM access routines
 */

/* open DRAM write accessing mode */
static int
awe_open_dram_for_write(int offset, int channels)
{
	int vidx[AWE_NORMAL_VOICES];
	int i;

	if (channels < 0 || channels >= AWE_NORMAL_VOICES) {
		channels = AWE_NORMAL_VOICES;
		for (i = 0; i < AWE_NORMAL_VOICES; i++)
			vidx[i] = i;
	} else {
		for (i = 0; i < channels; i++)
			vidx[i] = awe_clear_voice();
	}

	/* use all channels for DMA transfer */
	for (i = 0; i < channels; i++) {
		if (vidx[i] < 0) continue;
		awe_poke(AWE_DCYSUSV(vidx[i]), 0x80);
		awe_poke_dw(AWE_VTFT(vidx[i]), 0);
		awe_poke_dw(AWE_CVCF(vidx[i]), 0);
		awe_poke_dw(AWE_PTRX(vidx[i]), 0x40000000);
		awe_poke_dw(AWE_CPF(vidx[i]), 0x40000000);
		awe_poke_dw(AWE_PSST(vidx[i]), 0);
		awe_poke_dw(AWE_CSL(vidx[i]), 0);
		awe_poke_dw(AWE_CCCA(vidx[i]), 0x06000000);
		voices[vidx[i]].state = AWE_ST_DRAM;
	}
	/* point channels 31 & 32 to ROM samples for DRAM refresh */
	awe_poke_dw(AWE_VTFT(30), 0);
	awe_poke_dw(AWE_PSST(30), 0x1d8);
	awe_poke_dw(AWE_CSL(30), 0x1e0);
	awe_poke_dw(AWE_CCCA(30), 0x1d8);
	awe_poke_dw(AWE_VTFT(31), 0);
	awe_poke_dw(AWE_PSST(31), 0x1d8);
	awe_poke_dw(AWE_CSL(31), 0x1e0);
	awe_poke_dw(AWE_CCCA(31), 0x1d8);
	voices[30].state = AWE_ST_FM;
	voices[31].state = AWE_ST_FM;

	/* if full bit is on, not ready to write on */
	if (awe_peek_dw(AWE_SMALW) & 0x80000000) {
		for (i = 0; i < channels; i++) {
			awe_poke_dw(AWE_CCCA(vidx[i]), 0);
			voices[i].state = AWE_ST_OFF;
		}
		return RET_ERROR(ENOSPC);
	}

	/* set address to write */
	awe_poke_dw(AWE_SMALW, offset);

	return 0;
}

/* open DRAM for RAM size detection */
static void
awe_open_dram_for_check(void)
{
	int i;
	for (i = 0; i < AWE_NORMAL_VOICES; i++) {
		awe_poke(AWE_DCYSUSV(i), 0x80);
		awe_poke_dw(AWE_VTFT(i), 0);
		awe_poke_dw(AWE_CVCF(i), 0);
		awe_poke_dw(AWE_PTRX(i), 0x40000000);
		awe_poke_dw(AWE_CPF(i), 0x40000000);
		awe_poke_dw(AWE_PSST(i), 0);
		awe_poke_dw(AWE_CSL(i), 0);
		if (i & 1) /* DMA write */
			awe_poke_dw(AWE_CCCA(i), 0x06000000);
		else	   /* DMA read */
			awe_poke_dw(AWE_CCCA(i), 0x04000000);
		voices[i].state = AWE_ST_DRAM;
	}
}


/* close dram access */
static void
awe_close_dram(void)
{
	int i;
	/* wait until FULL bit in SMAxW register be false */
	for (i = 0; i < 10000; i++) {
		if (!(awe_peek_dw(AWE_SMALW) & 0x80000000))
			break;
		awe_wait(10);
	}

	for (i = 0; i < AWE_NORMAL_VOICES; i++) {
		if (voices[i].state == AWE_ST_DRAM) {
			awe_poke_dw(AWE_CCCA(i), 0);
			awe_poke(AWE_DCYSUSV(i), 0x807F);
			voices[i].state = AWE_ST_OFF;
		}
	}
}


/*================================================================
 * detect presence of AWE32 and check memory size
 *================================================================*/

/* detect emu8000 chip on the specified address; from VV's guide */

static int
awe_detect_base(int addr)
{
	awe_base = addr;
	if ((awe_peek(AWE_U1) & 0x000F) != 0x000C)
		return 0;
	if ((awe_peek(AWE_HWCF1) & 0x007E) != 0x0058)
		return 0;
	if ((awe_peek(AWE_HWCF2) & 0x0003) != 0x0003)
		return 0;
        DEBUG(0,printk("AWE32 found at %x\n", awe_base));
	return 1;
}
	
static int
awe_detect(void)
{
	int base;
	if (awe_base == 0) {
		for (base = 0x620; base <= 0x680; base += 0x20)
			if (awe_detect_base(base))
				return 1;
		DEBUG(0,printk("AWE32 not found\n"));
		return 0;
	}
	return 1;
}


/*================================================================
 * check dram size on AWE board
 *================================================================*/

/* any three numbers you like */
#define UNIQUE_ID1	0x1234
#define UNIQUE_ID2	0x4321
#define UNIQUE_ID3	0xFFFF

static int
awe_check_dram(void)
{
	if (awe_mem_size > 0) {
		awe_mem_size *= 1024; /* convert to Kbytes */
		return awe_mem_size;
	}

	awe_open_dram_for_check();

	awe_mem_size = 0;

	/* set up unique two id numbers */
	awe_poke_dw(AWE_SMALW, AWE_DRAM_OFFSET);
	awe_poke(AWE_SMLD, UNIQUE_ID1);
	awe_poke(AWE_SMLD, UNIQUE_ID2);

	while (awe_mem_size < AWE_MAX_DRAM_SIZE) {
		awe_wait(2);
		/* read a data on the DRAM start address */
		awe_poke_dw(AWE_SMALR, AWE_DRAM_OFFSET);
		awe_peek(AWE_SMLD); /* discard stale data  */
		if (awe_peek(AWE_SMLD) != UNIQUE_ID1)
			break;
		if (awe_peek(AWE_SMLD) != UNIQUE_ID2)
			break;
		awe_mem_size += 32;  /* increment 32 Kbytes */
		/* Write a unique data on the test address;
		 * if the address is out of range, the data is written on
		 * 0x200000(=AWE_DRAM_OFFSET).  Then the two id words are
		 * broken by this data.
		 */
		awe_poke_dw(AWE_SMALW, AWE_DRAM_OFFSET + awe_mem_size*512L);
		awe_poke(AWE_SMLD, UNIQUE_ID3);
		awe_wait(2);
		/* read a data on the just written DRAM address */
		awe_poke_dw(AWE_SMALR, AWE_DRAM_OFFSET + awe_mem_size*512L);
		awe_peek(AWE_SMLD); /* discard stale data  */
		if (awe_peek(AWE_SMLD) != UNIQUE_ID3)
			break;
	}
	awe_close_dram();

	DEBUG(0,printk("AWE32: %d Kbytes memory detected\n", awe_mem_size));

	/* convert to Kbytes */
	awe_mem_size *= 1024;
	return awe_mem_size;
}


/*================================================================
 * chorus and reverb controls; from VV's guide
 *================================================================*/

/* 5 parameters for each chorus mode; 3 x 16bit, 2 x 32bit */
static char chorus_defined[AWE_CHORUS_NUMBERS];
static awe_chorus_fx_rec chorus_parm[AWE_CHORUS_NUMBERS] = {
	{0xE600, 0x03F6, 0xBC2C ,0x00000000, 0x0000006D}, /* chorus 1 */
	{0xE608, 0x031A, 0xBC6E, 0x00000000, 0x0000017C}, /* chorus 2 */
	{0xE610, 0x031A, 0xBC84, 0x00000000, 0x00000083}, /* chorus 3 */
	{0xE620, 0x0269, 0xBC6E, 0x00000000, 0x0000017C}, /* chorus 4 */
	{0xE680, 0x04D3, 0xBCA6, 0x00000000, 0x0000005B}, /* feedback */
	{0xE6E0, 0x044E, 0xBC37, 0x00000000, 0x00000026}, /* flanger */
	{0xE600, 0x0B06, 0xBC00, 0x0000E000, 0x00000083}, /* short delay */
	{0xE6C0, 0x0B06, 0xBC00, 0x0000E000, 0x00000083}, /* short delay + feedback */
};

static int
awe_load_chorus_fx(awe_patch_info *patch, const char *addr, int count)
{
	if (patch->optarg < AWE_CHORUS_PREDEFINED || patch->optarg >= AWE_CHORUS_NUMBERS) {
		printk("AWE32 Error: illegal chorus mode %d for uploading\n", patch->optarg);
		return RET_ERROR(EINVAL);
	}
	if (count < sizeof(awe_chorus_fx_rec)) {
		printk("AWE32 Error: too short chorus fx parameters\n");
		return RET_ERROR(EINVAL);
	}
	COPY_FROM_USER(&chorus_parm[patch->optarg], addr, AWE_PATCH_INFO_SIZE,
		       sizeof(awe_chorus_fx_rec));
	chorus_defined[patch->optarg] = TRUE;
	return 0;
}

static void
awe_set_chorus_mode(int effect)
{
	if (effect < 0 || effect >= AWE_CHORUS_NUMBERS ||
	    (effect >= AWE_CHORUS_PREDEFINED && !chorus_defined[effect]))
		return;
	awe_poke(AWE_INIT3(9), chorus_parm[effect].feedback);
	awe_poke(AWE_INIT3(12), chorus_parm[effect].delay_offset);
	awe_poke(AWE_INIT4(3), chorus_parm[effect].lfo_depth);
	awe_poke_dw(AWE_HWCF4, chorus_parm[effect].delay);
	awe_poke_dw(AWE_HWCF5, chorus_parm[effect].lfo_freq);
	awe_poke_dw(AWE_HWCF6, 0x8000);
	awe_poke_dw(AWE_HWCF7, 0x0000);
	chorus_mode = effect;
}

/*----------------------------------------------------------------*/

/* reverb mode settings; write the following 28 data of 16 bit length
 *   on the corresponding ports in the reverb_cmds array
 */
static char reverb_defined[AWE_CHORUS_NUMBERS];
static awe_reverb_fx_rec reverb_parm[AWE_REVERB_NUMBERS] = {
{{  /* room 1 */
	0xB488, 0xA450, 0x9550, 0x84B5, 0x383A, 0x3EB5, 0x72F4,
	0x72A4, 0x7254, 0x7204, 0x7204, 0x7204, 0x4416, 0x4516,
	0xA490, 0xA590, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429,
	0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528,
}},
{{  /* room 2 */
	0xB488, 0xA458, 0x9558, 0x84B5, 0x383A, 0x3EB5, 0x7284,
	0x7254, 0x7224, 0x7224, 0x7254, 0x7284, 0x4448, 0x4548,
	0xA440, 0xA540, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429,
	0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528,
}},
{{  /* room 3 */
	0xB488, 0xA460, 0x9560, 0x84B5, 0x383A, 0x3EB5, 0x7284,
	0x7254, 0x7224, 0x7224, 0x7254, 0x7284, 0x4416, 0x4516,
	0xA490, 0xA590, 0x842C, 0x852C, 0x842C, 0x852C, 0x842B,
	0x852B, 0x842B, 0x852B, 0x842A, 0x852A, 0x842A, 0x852A,
}},
{{  /* hall 1 */
	0xB488, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7284,
	0x7254, 0x7224, 0x7224, 0x7254, 0x7284, 0x4448, 0x4548,
	0xA440, 0xA540, 0x842B, 0x852B, 0x842B, 0x852B, 0x842A,
	0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529,
}},
{{  /* hall 2 */
	0xB488, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7254,
	0x7234, 0x7224, 0x7254, 0x7264, 0x7294, 0x44C3, 0x45C3,
	0xA404, 0xA504, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429,
	0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528,
}},
{{  /* plate */
	0xB4FF, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7234,
	0x7234, 0x7234, 0x7234, 0x7234, 0x7234, 0x4448, 0x4548,
	0xA440, 0xA540, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429,
	0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528,
}},
{{  /* delay */
	0xB4FF, 0xA470, 0x9500, 0x84B5, 0x333A, 0x39B5, 0x7204,
	0x7204, 0x7204, 0x7204, 0x7204, 0x72F4, 0x4400, 0x4500,
	0xA4FF, 0xA5FF, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420,
	0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520,
}},
{{  /* panning delay */
	0xB4FF, 0xA490, 0x9590, 0x8474, 0x333A, 0x39B5, 0x7204,
	0x7204, 0x7204, 0x7204, 0x7204, 0x72F4, 0x4400, 0x4500,
	0xA4FF, 0xA5FF, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420,
	0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520,
}},
};

static struct ReverbCmdPair {
	unsigned short cmd, port;
} reverb_cmds[28] = {
  {AWE_INIT1(0x03)}, {AWE_INIT1(0x05)}, {AWE_INIT4(0x1F)}, {AWE_INIT1(0x07)},
  {AWE_INIT2(0x14)}, {AWE_INIT2(0x16)}, {AWE_INIT1(0x0F)}, {AWE_INIT1(0x17)},
  {AWE_INIT1(0x1F)}, {AWE_INIT2(0x07)}, {AWE_INIT2(0x0F)}, {AWE_INIT2(0x17)},
  {AWE_INIT2(0x1D)}, {AWE_INIT2(0x1F)}, {AWE_INIT3(0x01)}, {AWE_INIT3(0x03)},
  {AWE_INIT1(0x09)}, {AWE_INIT1(0x0B)}, {AWE_INIT1(0x11)}, {AWE_INIT1(0x13)},
  {AWE_INIT1(0x19)}, {AWE_INIT1(0x1B)}, {AWE_INIT2(0x01)}, {AWE_INIT2(0x03)},
  {AWE_INIT2(0x09)}, {AWE_INIT2(0x0B)}, {AWE_INIT2(0x11)}, {AWE_INIT2(0x13)},
};

static int
awe_load_reverb_fx(awe_patch_info *patch, const char *addr, int count)
{
	if (patch->optarg < AWE_REVERB_PREDEFINED || patch->optarg >= AWE_REVERB_NUMBERS) {
		printk("AWE32 Error: illegal reverb mode %d for uploading\n", patch->optarg);
		return RET_ERROR(EINVAL);
	}
	if (count < sizeof(awe_reverb_fx_rec)) {
		printk("AWE32 Error: too short reverb fx parameters\n");
		return RET_ERROR(EINVAL);
	}
	COPY_FROM_USER(&reverb_parm[patch->optarg], addr, AWE_PATCH_INFO_SIZE,
		       sizeof(awe_reverb_fx_rec));
	reverb_defined[patch->optarg] = TRUE;
	return 0;
}

static void
awe_set_reverb_mode(int effect)
{
	int i;
	if (effect < 0 || effect >= AWE_REVERB_NUMBERS ||
	    (effect >= AWE_REVERB_PREDEFINED && !reverb_defined[effect]))
		return;
	for (i = 0; i < 28; i++)
		awe_poke(reverb_cmds[i].cmd, reverb_cmds[i].port,
			 reverb_parm[effect].parms[i]);
	reverb_mode = effect;
}

/*================================================================
 * treble/bass equalizer control
 *================================================================*/

static unsigned short bass_parm[12][3] = {
	{0xD26A, 0xD36A, 0x0000}, /* -12 dB */
	{0xD25B, 0xD35B, 0x0000}, /*  -8 */
	{0xD24C, 0xD34C, 0x0000}, /*  -6 */
	{0xD23D, 0xD33D, 0x0000}, /*  -4 */
	{0xD21F, 0xD31F, 0x0000}, /*  -2 */
	{0xC208, 0xC308, 0x0001}, /*   0 (HW default) */
	{0xC219, 0xC319, 0x0001}, /*  +2 */
	{0xC22A, 0xC32A, 0x0001}, /*  +4 */
	{0xC24C, 0xC34C, 0x0001}, /*  +6 */
	{0xC26E, 0xC36E, 0x0001}, /*  +8 */
	{0xC248, 0xC348, 0x0002}, /* +10 */
	{0xC26A, 0xC36A, 0x0002}, /* +12 dB */
};

static unsigned short treble_parm[12][9] = {
	{0x821E, 0xC26A, 0x031E, 0xC36A, 0x021E, 0xD208, 0x831E, 0xD308, 0x0001}, /* -12 dB */
	{0x821E, 0xC25B, 0x031E, 0xC35B, 0x021E, 0xD208, 0x831E, 0xD308, 0x0001},
	{0x821E, 0xC24C, 0x031E, 0xC34C, 0x021E, 0xD208, 0x831E, 0xD308, 0x0001},
	{0x821E, 0xC23D, 0x031E, 0xC33D, 0x021E, 0xD208, 0x831E, 0xD308, 0x0001},
	{0x821E, 0xC21F, 0x031E, 0xC31F, 0x021E, 0xD208, 0x831E, 0xD308, 0x0001},
	{0x821E, 0xD208, 0x031E, 0xD308, 0x021E, 0xD208, 0x831E, 0xD308, 0x0002},
	{0x821E, 0xD208, 0x031E, 0xD308, 0x021D, 0xD219, 0x831D, 0xD319, 0x0002},
	{0x821E, 0xD208, 0x031E, 0xD308, 0x021C, 0xD22A, 0x831C, 0xD32A, 0x0002},
	{0x821E, 0xD208, 0x031E, 0xD308, 0x021A, 0xD24C, 0x831A, 0xD34C, 0x0002},
	{0x821E, 0xD208, 0x031E, 0xD308, 0x0219, 0xD26E, 0x8319, 0xD36E, 0x0002}, /* +8 (HW default) */
	{0x821D, 0xD219, 0x031D, 0xD319, 0x0219, 0xD26E, 0x8319, 0xD36E, 0x0002},
	{0x821C, 0xD22A, 0x031C, 0xD32A, 0x0219, 0xD26E, 0x8319, 0xD36E, 0x0002}, /* +12 dB */
};


/*
 * set Emu8000 digital equalizer; from 0 to 11 [-12dB - 12dB]
 */
static void
awe_equalizer(int bass, int treble)
{
	unsigned short w;

	if (bass < 0 || bass > 11 || treble < 0 || treble > 11)
		return;
	awe_bass_level = bass;
	awe_treble_level = treble;
	awe_poke(AWE_INIT4(0x01), bass_parm[bass][0]);
	awe_poke(AWE_INIT4(0x11), bass_parm[bass][1]);
	awe_poke(AWE_INIT3(0x11), treble_parm[treble][0]);
	awe_poke(AWE_INIT3(0x13), treble_parm[treble][1]);
	awe_poke(AWE_INIT3(0x1B), treble_parm[treble][2]);
	awe_poke(AWE_INIT4(0x07), treble_parm[treble][3]);
	awe_poke(AWE_INIT4(0x0B), treble_parm[treble][4]);
	awe_poke(AWE_INIT4(0x0D), treble_parm[treble][5]);
	awe_poke(AWE_INIT4(0x17), treble_parm[treble][6]);
	awe_poke(AWE_INIT4(0x19), treble_parm[treble][7]);
	w = bass_parm[bass][2] + treble_parm[treble][8];
	awe_poke(AWE_INIT4(0x15), (unsigned short)(w + 0x0262));
	awe_poke(AWE_INIT4(0x1D), (unsigned short)(w + 0x8362));
}


#endif /* CONFIG_AWE32_SYNTH */
