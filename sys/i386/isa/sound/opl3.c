/*
 * linux/kernel/chr_drv/sound/opl3.c
 * 
 * A low level driver for Yamaha YM3812 and OPL-3 -chips
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

/* Major improvements to the FM handling 30AUG92 by Rob Hooft, */
/* hooft@chem.ruu.nl */

#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_YM3812)

#include "opl3.h"

#define MAX_VOICE	18
#define OFFS_4OP	11	/* Definitions for the operators OP3 and OP4
				 * begin here */

static int      opl3_enabled = 0;
static int      left_address = 0x388, right_address = 0x388, both_address = 0;

static int      nr_voices = 9;
static int      logical_voices[MAX_VOICE] =
{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};

struct voice_info
  {
    unsigned char   keyon_byte;
    long            bender;
    long            bender_range;
    unsigned long   orig_freq;
    unsigned long   current_freq;
    int             mode;
  };

static struct voice_info voices[MAX_VOICE];

typedef struct sbi_instrument instr_array[SBFM_MAXINSTR];
static instr_array instrmap;
static struct sbi_instrument *active_instrument[MAX_VOICE] =
{NULL};

static struct synth_info fm_info =
{"AdLib", 0, SYNTH_TYPE_FM, FM_TYPE_ADLIB, 0, 9, 0, SBFM_MAXINSTR, 0};

static int      already_initialized = 0;

static int      opl3_ok = 0;
static int      opl3_busy = 0;
static int      fm_model = 0;	/* 0=no fm, 1=mono, 2=SB Pro 1, 3=SB Pro 2	 */

static int      store_instr (int instr_no, struct sbi_instrument *instr);
static void     freq_to_fnum (int freq, int *block, int *fnum);
static void     opl3_command (int io_addr, const unsigned char addr, const unsigned char val);
static int      opl3_kill_note (int dev, int voice, int velocity);
static unsigned char connection_mask = 0x00;

void
enable_opl3_mode (int left, int right, int both)
{
  opl3_enabled = 1;
  left_address = left;
  right_address = right;
  both_address = both;
  fm_info.capabilities = SYNTH_CAP_OPL3;
  fm_info.synth_subtype = FM_TYPE_OPL3;
}

static void
enter_4op_mode (void)
{
  int             i;
  static int      voices_4op[MAX_VOICE] =
  {0, 1, 2, 9, 10, 11, 6, 7, 8, 15, 16, 17};

  connection_mask = 0x3f;
  opl3_command (right_address, CONNECTION_SELECT_REGISTER, 0x3f);	/* Select all 4-OP
									 * voices */
  for (i = 0; i < 3; i++)
    physical_voices[i].voice_mode = 4;
  for (i = 3; i < 6; i++)
    physical_voices[i].voice_mode = 0;

  for (i = 9; i < 12; i++)
    physical_voices[i].voice_mode = 4;
  for (i = 12; i < 15; i++)
    physical_voices[i].voice_mode = 0;

  for (i = 0; i < 12; i++)
    logical_voices[i] = voices_4op[i];
  nr_voices = 6;
}

static int
opl3_ioctl (int dev,
	    unsigned int cmd, unsigned int arg)
{
  switch (cmd)
    {

    case SNDCTL_FM_LOAD_INSTR:
      {
	struct sbi_instrument ins;

	IOCTL_FROM_USER ((char *) &ins, (char *) arg, 0, sizeof (ins));

	if (ins.channel < 0 || ins.channel >= SBFM_MAXINSTR)
	  {
	    printk ("FM Error: Invalid instrument number %d\n", ins.channel);
	    return RET_ERROR (EINVAL);
	  }

	pmgr_inform (dev, PM_E_PATCH_LOADED, ins.channel, 0, 0, 0);
	return store_instr (ins.channel, &ins);
      }
      break;

    case SNDCTL_SYNTH_INFO:
      fm_info.nr_voices = nr_voices;

      IOCTL_TO_USER ((char *) arg, 0, &fm_info, sizeof (fm_info));
      return 0;
      break;

    case SNDCTL_SYNTH_MEMAVL:
      return 0x7fffffff;
      break;

    case SNDCTL_FM_4OP_ENABLE:
      if (opl3_enabled)
	enter_4op_mode ();
      return 0;
      break;

    default:
      return RET_ERROR (EINVAL);
    }

}

int
opl3_detect (int ioaddr)
{
  /*
   * This function returns 1 if the FM chicp is present at the given I/O port
   * The detection algorithm plays with the timer built in the FM chip and
   * looks for a change in the status register.
   * 
   * Note! The timers of the FM chip are not connected to AdLib (and compatible)
   * boards.
   * 
   * Note2! The chip is initialized if detected.
   */

  unsigned char   stat1, stat2;
  int             i;

  if (already_initialized)
    {
      return 0;			/* Do avoid duplicate initializations */
    }

  opl3_command (ioaddr, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);	/* Reset timers 1 and 2 */
  opl3_command (ioaddr, TIMER_CONTROL_REGISTER, IRQ_RESET);	/* Reset the IRQ of FM
								 * chicp */

  stat1 = INB (ioaddr);		/* Read status register */

  if ((stat1 & 0xE0) != 0x00)
    {
      return 0;			/* Should be 0x00	 */
    }

  opl3_command (ioaddr, TIMER1_REGISTER, 0xff);	/* Set timer 1 to 0xff */
  opl3_command (ioaddr, TIMER_CONTROL_REGISTER,
		TIMER2_MASK | TIMER1_START);	/* Unmask and start timer 1 */

  /*
   * Now we have to delay at least 80 msec
   */

  for (i = 0; i < 50; i++)
    tenmicrosec ();		/* To be sure */

  stat2 = INB (ioaddr);		/* Read status after timers have expired */

  /* Stop the timers */

  opl3_command (ioaddr, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);	/* Reset timers 1 and 2 */
  opl3_command (ioaddr, TIMER_CONTROL_REGISTER, IRQ_RESET);	/* Reset the IRQ of FM
								 * chicp */

  if ((stat2 & 0xE0) != 0xc0)
    {
      return 0;			/* There is no YM3812 */
    }

  /* There is a FM chicp in this address. Now set some default values. */

  for (i = 0; i < 9; i++)
    opl3_command (ioaddr, KEYON_BLOCK + i, 0);	/* Note off */

  opl3_command (ioaddr, TEST_REGISTER, ENABLE_WAVE_SELECT);
  opl3_command (ioaddr, PERCUSSION_REGISTER, 0x00);	/* Melodic mode. */

  return 1;
}

static int
opl3_kill_note (int dev, int voice, int velocity)
{
  struct physical_voice_info *map;

  if (voice < 0 || voice >= nr_voices)
    return 0;

  map = &physical_voices[logical_voices[voice]];

  DEB (printk ("Kill note %d\n", voice));

  if (map->voice_mode == 0)
    return 0;

  opl3_command (map->ioaddr, KEYON_BLOCK + map->voice_num, voices[voice].keyon_byte & ~0x20);

  voices[voice].keyon_byte = 0;
  voices[voice].bender = 0;
  voices[voice].bender_range = 200;	/* 200 cents = 2 semitones */
  voices[voice].orig_freq = 0;
  voices[voice].current_freq = 0;
  voices[voice].mode = 0;

  return 0;
}

#define HIHAT			0
#define CYMBAL			1
#define TOMTOM			2
#define SNARE			3
#define BDRUM			4
#define UNDEFINED		TOMTOM
#define DEFAULT			TOMTOM

static int
store_instr (int instr_no, struct sbi_instrument *instr)
{

  if (instr->key != FM_PATCH && (instr->key != OPL3_PATCH || !opl3_enabled))
    printk ("FM warning: Invalid patch format field (key) 0x%04x\n", instr->key);
  memcpy ((char *) &(instrmap[instr_no]), (char *) instr, sizeof (*instr));

  return 0;
}

static int
opl3_set_instr (int dev, int voice, int instr_no)
{
  if (voice < 0 || voice >= nr_voices)
    return 0;

  if (instr_no < 0 || instr_no >= SBFM_MAXINSTR)
    return 0;

  active_instrument[voice] = &instrmap[instr_no];
  return 0;
}

/*
 * The next table looks magical, but it certainly is not. Its values have
 * been calculated as table[i]=8*log(i/64)/log(2) with an obvious exception
 * for i=0. This log-table converts a linear volume-scaling (0..127) to a
 * logarithmic scaling as present in the FM-synthesizer chips. so :    Volume
 * 64 =  0 db = relative volume  0 and:    Volume 32 = -6 db = relative
 * volume -8 it was implemented as a table because it is only 128 bytes and
 * it saves a lot of log() calculations. (RH)
 */
char            fm_volume_table[128] =
{-64, -48, -40, -35, -32, -29, -27, -26,	/* 0 -   7 */
 -24, -23, -21, -20, -19, -18, -18, -17,	/* 8 -  15 */
 -16, -15, -15, -14, -13, -13, -12, -12,	/* 16 -  23 */
 -11, -11, -10, -10, -10, -9, -9, -8,	/* 24 -  31 */
 -8, -8, -7, -7, -7, -6, -6, -6,/* 32 -  39 */
 -5, -5, -5, -5, -4, -4, -4, -4,/* 40 -  47 */
 -3, -3, -3, -3, -2, -2, -2, -2,/* 48 -  55 */
 -2, -1, -1, -1, -1, 0, 0, 0,	/* 56 -  63 */
 0, 0, 0, 1, 1, 1, 1, 1,	/* 64 -  71 */
 1, 2, 2, 2, 2, 2, 2, 2,	/* 72 -  79 */
 3, 3, 3, 3, 3, 3, 3, 4,	/* 80 -  87 */
 4, 4, 4, 4, 4, 4, 4, 5,	/* 88 -  95 */
 5, 5, 5, 5, 5, 5, 5, 5,	/* 96 - 103 */
 6, 6, 6, 6, 6, 6, 6, 6,	/* 104 - 111 */
 6, 7, 7, 7, 7, 7, 7, 7,	/* 112 - 119 */
 7, 7, 7, 8, 8, 8, 8, 8};	/* 120 - 127 */

static void
calc_vol (unsigned char *regbyte, int volume)
{
  int             level = (~*regbyte & 0x3f);

  if (level)
    level += fm_volume_table[volume];

  if (level > 0x3f)
    level = 0x3f;
  if (level < 0)
    level = 0;

  *regbyte = (*regbyte & 0xc0) | (~level & 0x3f);
}

static void
set_voice_volume (int voice, int volume)
{
  unsigned char   vol1, vol2, vol3, vol4;
  struct sbi_instrument *instr;
  struct physical_voice_info *map;

  if (voice < 0 || voice >= nr_voices)
    return;

  map = &physical_voices[logical_voices[voice]];

  instr = active_instrument[voice];

  if (!instr)
    instr = &instrmap[0];

  if (instr->channel < 0)
    return;

  if (voices[voice].mode == 0)
    return;

  if (voices[voice].mode == 2)
    {				/* 2 OP voice */

      vol1 = instr->operators[2];
      vol2 = instr->operators[3];

      if ((instr->operators[10] & 0x01))
	{			/* Additive synthesis	 */
	  calc_vol (&vol1, volume);
	  calc_vol (&vol2, volume);
	}
      else
	{			/* FM synthesis */
	  calc_vol (&vol2, volume);
	}

      opl3_command (map->ioaddr, KSL_LEVEL + map->op[0], vol1);	/* Modulator volume */
      opl3_command (map->ioaddr, KSL_LEVEL + map->op[1], vol2);	/* Carrier volume */
    }
  else
    {				/* 4 OP voice */
      int             connection;

      vol1 = instr->operators[2];
      vol2 = instr->operators[3];
      vol3 = instr->operators[OFFS_4OP + 2];
      vol4 = instr->operators[OFFS_4OP + 3];

      /*
       * The connection method for 4 OP voices is defined by the rightmost
       * bits at the offsets 10 and 10+OFFS_4OP
       */

      connection = ((instr->operators[10] & 0x01) << 1) | (instr->operators[10 + OFFS_4OP] & 0x01);

      switch (connection)
	{
	case 0:
	  calc_vol (&vol4, volume);	/* Just the OP 4 is carrier */
	  break;

	case 1:
	  calc_vol (&vol2, volume);
	  calc_vol (&vol4, volume);
	  break;

	case 2:
	  calc_vol (&vol1, volume);
	  calc_vol (&vol4, volume);
	  break;

	case 3:
	  calc_vol (&vol1, volume);
	  calc_vol (&vol3, volume);
	  calc_vol (&vol4, volume);
	  break;

	default:/* Why ?? */ ;
	}

      opl3_command (map->ioaddr, KSL_LEVEL + map->op[0], vol1);
      opl3_command (map->ioaddr, KSL_LEVEL + map->op[1], vol2);
      opl3_command (map->ioaddr, KSL_LEVEL + map->op[2], vol3);
      opl3_command (map->ioaddr, KSL_LEVEL + map->op[3], vol4);
    }
}

static int
opl3_start_note (int dev, int voice, int note, int volume)
{
  unsigned char   data;
  int             block, fnum, freq, voice_mode;
  struct sbi_instrument *instr;
  struct physical_voice_info *map;

  if (voice < 0 || voice >= nr_voices)
    return 0;

  map = &physical_voices[logical_voices[voice]];

  if (map->voice_mode == 0)
    return 0;

  if (note == 255)		/* Just change the volume */
    {
      set_voice_volume (voice, volume);
      return 0;
    }

  /* Kill previous note before playing */
  opl3_command (map->ioaddr, KSL_LEVEL + map->op[1], 0xff);	/* Carrier volume to min */
  opl3_command (map->ioaddr, KSL_LEVEL + map->op[0], 0xff);	/* Modulator volume to */

  if (map->voice_mode == 4)
    {
      opl3_command (map->ioaddr, KSL_LEVEL + map->op[2], 0xff);
      opl3_command (map->ioaddr, KSL_LEVEL + map->op[3], 0xff);
    }

  opl3_command (map->ioaddr, KEYON_BLOCK + map->voice_num, 0x00);	/* Note off */

  instr = active_instrument[voice];

  if (!instr)
    instr = &instrmap[0];

  if (instr->channel < 0)
    {
      printk (
	       "OPL3: Initializing voice %d with undefined instrument\n",
	       voice);
      return 0;
    }

  if (map->voice_mode == 2 && instr->key == OPL3_PATCH)
    return 0;			/* Cannot play */

  voice_mode = map->voice_mode;

  if (voice_mode == 4)
    {
      int             voice_shift;

      voice_shift = (map->ioaddr == left_address) ? 0 : 3;
      voice_shift += map->voice_num;

      if (instr->key != OPL3_PATCH)	/* Just 2 OP patch */
	{
	  voice_mode = 2;
	  connection_mask &= ~(1 << voice_shift);
	}
      else
	{
	  connection_mask |= (1 << voice_shift);
	}

      opl3_command (right_address, CONNECTION_SELECT_REGISTER, connection_mask);
    }

  /* Set Sound Characteristics */
  opl3_command (map->ioaddr, AM_VIB + map->op[0], instr->operators[0]);
  opl3_command (map->ioaddr, AM_VIB + map->op[1], instr->operators[1]);

  /* Set Attack/Decay */
  opl3_command (map->ioaddr, ATTACK_DECAY + map->op[0], instr->operators[4]);
  opl3_command (map->ioaddr, ATTACK_DECAY + map->op[1], instr->operators[5]);

  /* Set Sustain/Release */
  opl3_command (map->ioaddr, SUSTAIN_RELEASE + map->op[0], instr->operators[6]);
  opl3_command (map->ioaddr, SUSTAIN_RELEASE + map->op[1], instr->operators[7]);

  /* Set Wave Select */
  opl3_command (map->ioaddr, WAVE_SELECT + map->op[0], instr->operators[8]);
  opl3_command (map->ioaddr, WAVE_SELECT + map->op[1], instr->operators[9]);

  /* Set Feedback/Connection */
  /* Connect the voice to both stereo channels */
  opl3_command (map->ioaddr, FEEDBACK_CONNECTION + map->voice_num, instr->operators[10] | 0x30);

  /*
   * If the voice is a 4 OP one, initialize the operators 3 and 4 also
   */

  if (voice_mode == 4)
    {

      /* Set Sound Characteristics */
      opl3_command (map->ioaddr, AM_VIB + map->op[2], instr->operators[OFFS_4OP + 0]);
      opl3_command (map->ioaddr, AM_VIB + map->op[3], instr->operators[OFFS_4OP + 1]);

      /* Set Attack/Decay */
      opl3_command (map->ioaddr, ATTACK_DECAY + map->op[2], instr->operators[OFFS_4OP + 4]);
      opl3_command (map->ioaddr, ATTACK_DECAY + map->op[3], instr->operators[OFFS_4OP + 5]);

      /* Set Sustain/Release */
      opl3_command (map->ioaddr, SUSTAIN_RELEASE + map->op[2], instr->operators[OFFS_4OP + 6]);
      opl3_command (map->ioaddr, SUSTAIN_RELEASE + map->op[3], instr->operators[OFFS_4OP + 7]);

      /* Set Wave Select */
      opl3_command (map->ioaddr, WAVE_SELECT + map->op[2], instr->operators[OFFS_4OP + 8]);
      opl3_command (map->ioaddr, WAVE_SELECT + map->op[3], instr->operators[OFFS_4OP + 9]);

      /* Set Feedback/Connection */
      /* Connect the voice to both stereo channels */
      opl3_command (map->ioaddr, FEEDBACK_CONNECTION + map->voice_num + 3, instr->operators[OFFS_4OP + 10] | 0x30);
    }

  voices[voice].mode = voice_mode;

  set_voice_volume (voice, volume);

  freq = voices[voice].orig_freq = note_to_freq (note) / 1000;

  /*
   * Since the pitch bender may have been set before playing the note, we
   * have to calculate the bending now.
   */

  freq = compute_finetune (voices[voice].orig_freq, voices[voice].bender, voices[voice].bender_range);
  voices[voice].current_freq = freq;

  freq_to_fnum (freq, &block, &fnum);

  /* Play note */

  data = fnum & 0xff;		/* Least significant bits of fnumber */
  opl3_command (map->ioaddr, FNUM_LOW + map->voice_num, data);

  data = 0x20 | ((block & 0x7) << 2) | ((fnum >> 8) & 0x3);
  voices[voice].keyon_byte = data;
  opl3_command (map->ioaddr, KEYON_BLOCK + map->voice_num, data);
  if (voice_mode == 4)
    opl3_command (map->ioaddr, KEYON_BLOCK + map->voice_num + 3, data);

  return 0;
}

static void
freq_to_fnum (int freq, int *block, int *fnum)
{
  int             f, octave;

  /* Converts the note frequency to block and fnum values for the FM chip */
  /* First try to compute the block -value (octave) where the note belongs */

  f = freq;

  octave = 5;

  if (f == 0)
    octave = 0;
  else if (f < 261)
    {
      while (f < 261)
	{
	  octave--;
	  f <<= 1;
	}
    }
  else if (f > 493)
    {
      while (f > 493)
	{
	  octave++;
	  f >>= 1;
	}
    }

  if (octave > 7)
    octave = 7;

  *fnum = freq * (1 << (20 - octave)) / 49716;
  *block = octave;
}

static void
opl3_command (int io_addr, const unsigned char addr, const unsigned char val)
{
  int             i;

  /*
   * The original 2-OP synth requires a quite long delay after writing to a
   * register. The OPL-3 survives with just two INBs
   */

  OUTB (addr, io_addr);		/* Select register	 */

  if (!opl3_enabled)
    tenmicrosec ();
  else
    for (i = 0; i < 2; i++)
      INB (io_addr);

  OUTB (val, io_addr + 1);	/* Write to register	 */

  if (!opl3_enabled)
    {
      tenmicrosec ();
      tenmicrosec ();
      tenmicrosec ();
    }
  else
    for (i = 0; i < 2; i++)
      INB (io_addr);
}

static void
opl3_reset (int dev)
{
  int             i;

  for (i = 0; i < nr_voices; i++)
    {
      opl3_command (physical_voices[logical_voices[i]].ioaddr,
		KSL_LEVEL + physical_voices[logical_voices[i]].op[0], 0xff);	/* OP1 volume to min */

      opl3_command (physical_voices[logical_voices[i]].ioaddr,
		KSL_LEVEL + physical_voices[logical_voices[i]].op[1], 0xff);	/* OP2 volume to min */

      if (physical_voices[logical_voices[i]].voice_mode == 4)	/* 4 OP voice */
	{
	  opl3_command (physical_voices[logical_voices[i]].ioaddr,
		KSL_LEVEL + physical_voices[logical_voices[i]].op[2], 0xff);	/* OP3 volume to min */

	  opl3_command (physical_voices[logical_voices[i]].ioaddr,
		KSL_LEVEL + physical_voices[logical_voices[i]].op[3], 0xff);	/* OP4 volume to min */
	}

      opl3_kill_note (dev, i, 64);
    }

  if (opl3_enabled)
    {
      nr_voices = 18;

      for (i = 0; i < 18; i++)
	logical_voices[i] = i;

      for (i = 0; i < 18; i++)
	physical_voices[i].voice_mode = 2;

    }

}

static int
opl3_open (int dev, int mode)
{
  if (!opl3_ok)
    return RET_ERROR (ENXIO);
  if (opl3_busy)
    return RET_ERROR (EBUSY);
  opl3_busy = 1;

  connection_mask = 0x00;	/* Just 2 OP voices */
  if (opl3_enabled)
    opl3_command (right_address, CONNECTION_SELECT_REGISTER, connection_mask);
  return 0;
}

static void
opl3_close (int dev)
{
  opl3_busy = 0;
  nr_voices = opl3_enabled ? 18 : 9;
  fm_info.nr_drums = 0;
  fm_info.perc_mode = 0;

  opl3_reset (dev);
}

static void
opl3_hw_control (int dev, unsigned char *event)
{
}

static int
opl3_load_patch (int dev, int format, snd_rw_buf * addr,
		 int offs, int count, int pmgr_flag)
{
  struct sbi_instrument ins;

  if (count < sizeof (ins))
    {
      printk ("FM Error: Patch record too short\n");
      return RET_ERROR (EINVAL);
    }

  COPY_FROM_USER (&((char *) &ins)[offs], (char *) addr, offs, sizeof (ins) - offs);

  if (ins.channel < 0 || ins.channel >= SBFM_MAXINSTR)
    {
      printk ("FM Error: Invalid instrument number %d\n", ins.channel);
      return RET_ERROR (EINVAL);
    }
  ins.key = format;

  return store_instr (ins.channel, &ins);
}

static void
opl3_panning (int dev, int voice, int pressure)
{
}

#define SET_VIBRATO(cell) { \
      tmp = instr->operators[(cell-1)+(((cell-1)/2)*OFFS_4OP)]; \
      if (pressure > 110) \
	tmp |= 0x40;		/* Vibrato on */ \
      opl3_command (map->ioaddr, AM_VIB + map->op[cell-1], tmp);}

static void
opl3_aftertouch (int dev, int voice, int pressure)
{
  int             tmp;
  struct sbi_instrument *instr;
  struct physical_voice_info *map;

  if (voice < 0 || voice >= nr_voices)
    return;

  map = &physical_voices[logical_voices[voice]];

  DEB (printk ("Aftertouch %d\n", voice));

  if (map->voice_mode == 0)
    return;

  /*
   * Adjust the amount of vibrato depending the pressure
   */

  instr = active_instrument[voice];

  if (!instr)
    instr = &instrmap[0];

  if (voices[voice].mode == 4)
    {
      int             connection = ((instr->operators[10] & 0x01) << 1) | (instr->operators[10 + OFFS_4OP] & 0x01);

      switch (connection)
	{
	case 0:
	  SET_VIBRATO (4);
	  break;

	case 1:
	  SET_VIBRATO (2);
	  SET_VIBRATO (4);
	  break;

	case 2:
	  SET_VIBRATO (1);
	  SET_VIBRATO (4);
	  break;

	case 3:
	  SET_VIBRATO (1);
	  SET_VIBRATO (3);
	  SET_VIBRATO (4);
	  break;

	}
      /* Not implemented yet */
    }
  else
    {
      SET_VIBRATO (1);

      if ((instr->operators[10] & 0x01))	/* Additive synthesis */
	SET_VIBRATO (2);
    }
}

#undef SET_VIBRATO

static void
opl3_controller (int dev, int voice, int ctrl_num, int value)
{
  unsigned char   data;
  int             block, fnum, freq;
  struct physical_voice_info *map;

  if (voice < 0 || voice >= nr_voices)
    return;

  map = &physical_voices[logical_voices[voice]];

  if (map->voice_mode == 0)
    return;

  switch (ctrl_num)
    {
    case CTRL_PITCH_BENDER:
      voices[voice].bender = value;
      if (!value)
	return;
      if (!(voices[voice].keyon_byte & 0x20))
	return;			/* Not keyed on */

      freq = compute_finetune (voices[voice].orig_freq, voices[voice].bender, voices[voice].bender_range);
      voices[voice].current_freq = freq;

      freq_to_fnum (freq, &block, &fnum);

      data = fnum & 0xff;	/* Least significant bits of fnumber */
      opl3_command (map->ioaddr, FNUM_LOW + map->voice_num, data);

      data = 0x20 | ((block & 0x7) << 2) | ((fnum >> 8) & 0x3);	/* KEYON|OCTAVE|MS bits
								 * of f-num */
      voices[voice].keyon_byte = data;
      opl3_command (map->ioaddr, KEYON_BLOCK + map->voice_num, data);
      break;

    case CTRL_PITCH_BENDER_RANGE:
      voices[voice].bender_range = value;
      break;
    }
}

static int
opl3_patchmgr (int dev, struct patmgr_info *rec)
{
  return RET_ERROR (EINVAL);
}

static struct synth_operations opl3_operations =
{
  &fm_info,
  SYNTH_TYPE_FM,
  FM_TYPE_ADLIB,
  opl3_open,
  opl3_close,
  opl3_ioctl,
  opl3_kill_note,
  opl3_start_note,
  opl3_set_instr,
  opl3_reset,
  opl3_hw_control,
  opl3_load_patch,
  opl3_aftertouch,
  opl3_controller,
  opl3_panning,
  opl3_patchmgr
};

long
opl3_init (long mem_start)
{
  int             i;

  synth_devs[num_synths++] = &opl3_operations;
  fm_model = 0;
  opl3_ok = 1;
  if (opl3_enabled)
    {
      printk ("snd1: <Yamaha OPL-3 FM>");
      fm_model = 2;
      nr_voices = 18;
      fm_info.nr_drums = 0;
      fm_info.capabilities |= SYNTH_CAP_OPL3;
      strcpy (fm_info.name, "Yamaha OPL-3");

      for (i = 0; i < 18; i++)
	if (physical_voices[i].ioaddr == USE_LEFT)
	  physical_voices[i].ioaddr = left_address;
	else
	  physical_voices[i].ioaddr = right_address;


      opl3_command (right_address, OPL3_MODE_REGISTER, OPL3_ENABLE);	/* Enable OPL-3 mode */
      opl3_command (right_address, CONNECTION_SELECT_REGISTER, 0x00);	/* Select all 2-OP
									 * voices */
    }
  else
    {
      printk ("snd1: <Yamaha 2-OP FM>");
      fm_model = 1;
      nr_voices = 9;
      fm_info.nr_drums = 0;

      for (i = 0; i < 18; i++)
	physical_voices[i].ioaddr = left_address;
    };

  already_initialized = 1;
  for (i = 0; i < SBFM_MAXINSTR; i++)
    instrmap[i].channel = -1;

  printk("\n");

  return mem_start;
}

#endif
