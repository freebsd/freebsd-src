/*
 * sound/opl3.c
 * 
 * A low level driver for Yamaha YM3812 and OPL-3 -chips
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

/*
 * Major improvements to the FM handling 30AUG92 by Rob Hooft,
 */
/*
 * hooft@chem.ruu.nl
 */
#include <i386/isa/sound/sound_config.h>


#if defined(CONFIG_YM3812)

#include <i386/isa/sound/opl3.h>
#include <machine/clock.h>

#define MAX_VOICE	18
#define OFFS_4OP	11

struct voice_info {
	u_char   keyon_byte;
	long            bender;
	long            bender_range;
	u_long   orig_freq;
	u_long   current_freq;
	int             volume;
	int             mode;
};

typedef struct opl_devinfo {
	int             left_io, right_io;
	int             nr_voice;
	int             lv_map[MAX_VOICE];

	struct voice_info voc[MAX_VOICE];
	struct voice_alloc_info *v_alloc;
	struct channel_info *chn_info;

	struct sbi_instrument i_map[SBFM_MAXINSTR];
	struct sbi_instrument *act_i[MAX_VOICE];

	struct synth_info fm_info;

	int             busy;
	int             model;
	u_char   cmask;

	int             is_opl4;
	sound_os_info  *osp;
}
                opl_devinfo;

static struct opl_devinfo *devc = NULL;


static int      detected_model;

static int      store_instr(int instr_no, struct sbi_instrument * instr);
static void     freq_to_fnum(int freq, int *block, int *fnum);
static void     opl3_command(int io_addr, u_int addr, u_int val);
static int      opl3_kill_note(int dev, int voice, int note, int velocity);

void
enable_opl3_mode(int left, int right, int both)
{
	/* NOP */
}

static void
enter_4op_mode(void)
{
    int             i;
    static int      v4op[MAX_VOICE] =
	{0, 1, 2, 9, 10, 11, 6, 7, 8, 15, 16, 17};

    devc->cmask = 0x3f;	/* Connect all possible 4 OP voice operators */
    opl3_command(devc->right_io, CONNECTION_SELECT_REGISTER, 0x3f);

    for (i = 0; i < 3; i++)
	pv_map[i].voice_mode = 4;
    for (i = 3; i < 6; i++)
	pv_map[i].voice_mode = 0;

    for (i = 9; i < 12; i++)
	pv_map[i].voice_mode = 4;
    for (i = 12; i < 15; i++)
	pv_map[i].voice_mode = 0;

    for (i = 0; i < 12; i++)
	devc->lv_map[i] = v4op[i];
    devc->v_alloc->max_voice = devc->nr_voice = 12;
}

static int
opl3_ioctl(int dev,
	   u_int cmd, ioctl_arg arg)
{
    switch (cmd) {

    case SNDCTL_FM_LOAD_INSTR:
	{
	    struct sbi_instrument ins;

	    bcopy(&(((char *) arg)[0]), (char *) &ins, sizeof(ins));

	    if (ins.channel < 0 || ins.channel >= SBFM_MAXINSTR) {
		printf("FM Error: Invalid instrument number %d\n", ins.channel);
		return -(EINVAL);
	    }
	    pmgr_inform(dev, PM_E_PATCH_LOADED, ins.channel, 0, 0, 0);
	    return store_instr(ins.channel, &ins);
	}
	break;

    case SNDCTL_SYNTH_INFO:
	devc->fm_info.nr_voices = (devc->nr_voice == 12) ? 6 : devc->nr_voice;
	bcopy(&devc->fm_info, &(((char *) arg)[0]), sizeof(devc->fm_info));
	return 0;
	break;

    case SNDCTL_SYNTH_MEMAVL:
	return 0x7fffffff;
	break;

    case SNDCTL_FM_4OP_ENABLE:
	if (devc->model == 2)
	    enter_4op_mode();
	return 0;
	break;

    default:
	return -(EINVAL);
    }

}

int
opl3_detect(int ioaddr, sound_os_info * osp)
{
    /*
     * This function returns 1 if the FM chip is present at the given
     * I/O port The detection algorithm plays with the timer built in the
     * FM chip and looks for a change in the status register.
     * 
     * Note! The timers of the FM chip are not connected to AdLib (and
     * compatible) boards.
     * 
     * Note2! The chip is initialized if detected.
     */

    u_char   stat1, stat2, signature;
    int             i;

    if (devc != NULL)
	return 0;

    devc = (struct opl_devinfo *) malloc(sizeof(*devc), M_DEVBUF, M_NOWAIT);
    if (!devc)
	panic("SOUND: Cannot allocate memory\n");

    if (devc == NULL) {
	printf("OPL3: Can't allocate memory for device control structure\n");
	return 0;
    }
    devc->osp = osp;

    /* Reset timers 1 and 2 */
    opl3_command(ioaddr, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);

    /* Reset the IRQ of the FM chip */
    opl3_command(ioaddr, TIMER_CONTROL_REGISTER, IRQ_RESET);

    signature = stat1 = inb(ioaddr);	/* Status register */

    if ((stat1 & 0xE0) != 0x00) {
	return 0;	/* Should be 0x00 */
    }
    opl3_command(ioaddr, TIMER1_REGISTER, 0xff);	/* Set timer1 to 0xff */

    opl3_command(ioaddr, TIMER_CONTROL_REGISTER,
		 TIMER2_MASK | TIMER1_START);	/* Unmask and start timer 1 */


    DELAY(150);		 /* Now we have to delay at least 80 usec */

    stat2 = inb(ioaddr);	/* Read status after timers have expired */

    /*
     * Stop the timers
     */

    /* Reset timers 1 and 2 */
    opl3_command(ioaddr, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
    /* Reset the IRQ of the FM chip */
    opl3_command(ioaddr, TIMER_CONTROL_REGISTER, IRQ_RESET);

    if ((stat2 & 0xE0) != 0xc0) {
	return 0;	/* There is no YM3812 */
    }
    /*
     * There is a FM chicp in this address. Detect the type (OPL2 to
     * OPL4)
     */

    if (signature == 0x06) {/* OPL2 */
	detected_model = 2;
    } else if (signature == 0x00) {	/* OPL3 or OPL4 */
	u_char   tmp;

	detected_model = 3;

	/*
	 * Detect availability of OPL4 (_experimental_). Works
	 * propably only after a cold boot. In addition the OPL4 port
	 * of the chip may not be connected to the PC bus at all.
	 */

	opl3_command(ioaddr + 2, OPL3_MODE_REGISTER, 0x00);
	opl3_command(ioaddr + 2, OPL3_MODE_REGISTER, OPL3_ENABLE | OPL4_ENABLE);

	if ((tmp = inb(ioaddr)) == 0x02) {	/* Have a OPL4 */
	    detected_model = 4;
	}
	if (!0) { /* OPL4 port is free */ /* XXX check here lr970711 */
	    int  tmp;

	    outb(ioaddr - 8, 0x02);	/* Select OPL4 ID register */
	    DELAY(10);
	    tmp = inb(ioaddr - 7);	/* Read it */
	    DELAY(10);

	    if (tmp == 0x20) {	/* OPL4 should return 0x20 here */
		detected_model = 4;

		outb(ioaddr - 8, 0xF8);	/* Select OPL4 FM mixer control */
		DELAY(10);
		outb(ioaddr - 7, 0x1B);	/* Write value */
		DELAY(10);
	    } else
		detected_model = 3;
	}
	opl3_command(ioaddr + 2, OPL3_MODE_REGISTER, 0);

    }
    for (i = 0; i < 9; i++)
	opl3_command(ioaddr, KEYON_BLOCK + i, 0);	/* Note off */

    opl3_command(ioaddr, TEST_REGISTER, ENABLE_WAVE_SELECT);
    opl3_command(ioaddr, PERCUSSION_REGISTER, 0x00);	/* Melodic mode. */

    return 1;
}

static int
opl3_kill_note(int dev, int voice, int note, int velocity)
{
    struct physical_voice_info *map;

    if (voice < 0 || voice >= devc->nr_voice)
	return 0;

    devc->v_alloc->map[voice] = 0;

    map = &pv_map[devc->lv_map[voice]];

    DEB(printf("Kill note %d\n", voice));

    if (map->voice_mode == 0)
	return 0;

    opl3_command(map->ioaddr, KEYON_BLOCK + map->voice_num,
		devc->voc[voice].keyon_byte & ~0x20);

    devc->voc[voice].keyon_byte = 0;
    devc->voc[voice].bender = 0;
    devc->voc[voice].volume = 64;
    devc->voc[voice].bender_range = 200;	/* 200 cents = 2 semitones */
    devc->voc[voice].orig_freq = 0;
    devc->voc[voice].current_freq = 0;
    devc->voc[voice].mode = 0;

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
store_instr(int instr_no, struct sbi_instrument * instr)
{

    if (instr->key !=FM_PATCH && (instr->key !=OPL3_PATCH || devc->model != 2))
	printf("FM warning: Invalid patch format field (key) 0x%x\n",
			instr->key);
    bcopy((char *) instr, (char *) &(devc->i_map[instr_no]), sizeof(*instr));

    return 0;
}

static int
opl3_set_instr(int dev, int voice, int instr_no)
{
    if (voice < 0 || voice >= devc->nr_voice)
	return 0;

    if (instr_no < 0 || instr_no >= SBFM_MAXINSTR)
	return 0;

    devc->act_i[voice] = &devc->i_map[instr_no];
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
{
	-64, -48, -40, -35, -32, -29, -27, -26,
	-24, -23, -21, -20, -19, -18, -18, -17,
	-16, -15, -15, -14, -13, -13, -12, -12,
	-11, -11, -10, -10, -10, -9, -9, -8,
	-8, -8, -7, -7, -7, -6, -6, -6,
	-5, -5, -5, -5, -4, -4, -4, -4,
	-3, -3, -3, -3, -2, -2, -2, -2,
	-2, -1, -1, -1, -1, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1,
	1, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 4,
	4, 4, 4, 4, 4, 4, 4, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 8, 8, 8, 8, 8};

static void
calc_vol(u_char *regbyte, int volume, int main_vol)
{
    int             level = (~*regbyte & 0x3f);

    if (main_vol > 127)
	main_vol = 127;

    volume = (volume * main_vol) / 127;

    if (level)
	level += fm_volume_table[volume];

    RANGE (level, 0, 0x3f );

    *regbyte = (*regbyte & 0xc0) | (~level & 0x3f);
}

static void
set_voice_volume(int voice, int volume, int main_vol)
{
    u_char   vol1, vol2, vol3, vol4;
    struct sbi_instrument *instr;
    struct physical_voice_info *map;

    if (voice < 0 || voice >= devc->nr_voice)
	return;

    map = &pv_map[devc->lv_map[voice]];

    instr = devc->act_i[voice];

    if (!instr)
	instr = &devc->i_map[0];

    if (instr->channel < 0)
	return;

    if (devc->voc[voice].mode == 0)
	return;

    if (devc->voc[voice].mode == 2) {

	vol1 = instr->operators[2];
	vol2 = instr->operators[3];

	if ((instr->operators[10] & 0x01)) {
	    calc_vol(&vol1, volume, main_vol);
	}
	calc_vol(&vol2, volume, main_vol);

	opl3_command(map->ioaddr, KSL_LEVEL + map->op[0], vol1);
	opl3_command(map->ioaddr, KSL_LEVEL + map->op[1], vol2);
    } else {		/* 4 OP voice */
	int             connection;

	vol1 = instr->operators[2];
	vol2 = instr->operators[3];
	vol3 = instr->operators[OFFS_4OP + 2];
	vol4 = instr->operators[OFFS_4OP + 3];

	/*
	 * The connection method for 4 OP devc->voc is defined by the
	 * rightmost bits at the offsets 10 and 10+OFFS_4OP
	 */

	connection = ((instr->operators[10] & 0x01) << 1) | (instr->operators[10 + OFFS_4OP] & 0x01);

	switch (connection) {
	case 0:
	    calc_vol(&vol4, volume, main_vol);
	    break;

	case 1:
	    calc_vol(&vol2, volume, main_vol);
	    calc_vol(&vol4, volume, main_vol);
	    break;

	case 2:
	    calc_vol(&vol1, volume, main_vol);
	    calc_vol(&vol4, volume, main_vol);
	    break;

	case 3:
	    calc_vol(&vol1, volume, main_vol);
	    calc_vol(&vol3, volume, main_vol);
	    calc_vol(&vol4, volume, main_vol);
	    break;

	default:;
	}

	opl3_command(map->ioaddr, KSL_LEVEL + map->op[0], vol1);
	opl3_command(map->ioaddr, KSL_LEVEL + map->op[1], vol2);
	opl3_command(map->ioaddr, KSL_LEVEL + map->op[2], vol3);
	opl3_command(map->ioaddr, KSL_LEVEL + map->op[3], vol4);
    }
}

static int
opl3_start_note(int dev, int voice, int note, int volume)
{
    u_char   data, fpc;
    int             block, fnum, freq, voice_mode;
    struct sbi_instrument *instr;
    struct physical_voice_info *map;

    if (voice < 0 || voice >= devc->nr_voice)
	return 0;

    map = &pv_map[devc->lv_map[voice]];

    if (map->voice_mode == 0)
	return 0;

    if (note == 255) {	/* Just change the volume */
	set_voice_volume(voice, volume, devc->voc[voice].volume);
	return 0;
    }
    /*
     * Kill previous note before playing
     */
    opl3_command(map->ioaddr, KSL_LEVEL + map->op[1], 0xff);	/* Carrier volume to min */
    opl3_command(map->ioaddr, KSL_LEVEL + map->op[0], 0xff);	/* Modulator volume to */

    if (map->voice_mode == 4) {
	opl3_command(map->ioaddr, KSL_LEVEL + map->op[2], 0xff);
	opl3_command(map->ioaddr, KSL_LEVEL + map->op[3], 0xff);
    }
    opl3_command(map->ioaddr, KEYON_BLOCK + map->voice_num, 0x00);	/* Note off */

    instr = devc->act_i[voice];

    if (!instr)
	instr = &devc->i_map[0];

    if (instr->channel < 0) {
	printf( "OPL3: Initializing voice %d with undefined instrument\n",
	       voice);
	return 0;
    }
    if (map->voice_mode == 2 && instr->key == OPL3_PATCH)
	return 0;	/* Cannot play */

    voice_mode = map->voice_mode;

    if (voice_mode == 4) {
	int             voice_shift;

	voice_shift = (map->ioaddr == devc->left_io) ? 0 : 3;
	voice_shift += map->voice_num;

	if (instr->key != OPL3_PATCH) {	/* Just 2 OP patch */
	    voice_mode = 2;
	    devc->cmask &= ~(1 << voice_shift);
	} else
	    devc->cmask |= (1 << voice_shift);

	opl3_command(devc->right_io, CONNECTION_SELECT_REGISTER, devc->cmask);
    }
    /*
     * Set Sound Characteristics
     */
    opl3_command(map->ioaddr, AM_VIB + map->op[0], instr->operators[0]);
    opl3_command(map->ioaddr, AM_VIB + map->op[1], instr->operators[1]);

    /*
     * Set Attack/Decay
     */
    opl3_command(map->ioaddr, ATTACK_DECAY + map->op[0], instr->operators[4]);
    opl3_command(map->ioaddr, ATTACK_DECAY + map->op[1], instr->operators[5]);

    /*
     * Set Sustain/Release
     */
    opl3_command(map->ioaddr,SUSTAIN_RELEASE + map->op[0], instr->operators[6]);
    opl3_command(map->ioaddr,SUSTAIN_RELEASE + map->op[1], instr->operators[7]);

    /*
     * Set Wave Select
     */
    opl3_command(map->ioaddr, WAVE_SELECT + map->op[0], instr->operators[8]);
    opl3_command(map->ioaddr, WAVE_SELECT + map->op[1], instr->operators[9]);

    /*
     * Set Feedback/Connection
     */
    fpc = instr->operators[10];
    if (!(fpc & 0x30))
	fpc |= 0x30;	/* Ensure that at least one chn is enabled */
    opl3_command(map->ioaddr, FEEDBACK_CONNECTION + map->voice_num, fpc);

    /*
     * If the voice is a 4 OP one, initialize the operators 3 and 4 also
     */

    if (voice_mode == 4) {

	/*
	 * Set Sound Characteristics
	 */
	opl3_command(map->ioaddr, AM_VIB + map->op[2],
			instr->operators[OFFS_4OP + 0]);
	opl3_command(map->ioaddr, AM_VIB + map->op[3],
			instr->operators[OFFS_4OP + 1]);

	/*
	 * Set Attack/Decay
	 */
	opl3_command(map->ioaddr, ATTACK_DECAY + map->op[2],
			instr->operators[OFFS_4OP + 4]);
	opl3_command(map->ioaddr, ATTACK_DECAY + map->op[3],
			instr->operators[OFFS_4OP + 5]);

	/*
	 * Set Sustain/Release
	 */
	opl3_command(map->ioaddr, SUSTAIN_RELEASE + map->op[2],
			instr->operators[OFFS_4OP + 6]);
	opl3_command(map->ioaddr, SUSTAIN_RELEASE + map->op[3],
			instr->operators[OFFS_4OP + 7]);

	/*
	 * Set Wave Select
	 */
	opl3_command(map->ioaddr, WAVE_SELECT + map->op[2],
			instr->operators[OFFS_4OP + 8]);
	opl3_command(map->ioaddr, WAVE_SELECT + map->op[3],
			instr->operators[OFFS_4OP + 9]);

	/*
	 * Set Feedback/Connection
	 */
	fpc = instr->operators[OFFS_4OP + 10];
	if (!(fpc & 0x30))
	    fpc |= 0x30;	/* Ensure that at least one chn is enabled */
	opl3_command(map->ioaddr,FEEDBACK_CONNECTION + map->voice_num + 3, fpc);
    }
    devc->voc[voice].mode = voice_mode;

    set_voice_volume(voice, volume, devc->voc[voice].volume);

    freq = devc->voc[voice].orig_freq = note_to_freq(note) / 1000;

    /*
     * Since the pitch bender may have been set before playing the note,
     * we have to calculate the bending now.
     */

    freq = compute_finetune(devc->voc[voice].orig_freq,
		devc->voc[voice].bender, devc->voc[voice].bender_range);
    devc->voc[voice].current_freq = freq;

    freq_to_fnum(freq, &block, &fnum);

    /*
     * Play note
     */

    data = fnum & 0xff;	/* Least significant bits of fnumber */
    opl3_command(map->ioaddr, FNUM_LOW + map->voice_num, data);

    data = 0x20 | ((block & 0x7) << 2) | ((fnum >> 8) & 0x3);
    devc->voc[voice].keyon_byte = data;
    opl3_command(map->ioaddr, KEYON_BLOCK + map->voice_num, data);
    if (voice_mode == 4)
	opl3_command(map->ioaddr, KEYON_BLOCK + map->voice_num + 3, data);

    return 0;
}

static void
freq_to_fnum(int freq, int *block, int *fnum)
{
    int             f, octave;

    /*
     * Converts the note frequency to block and fnum values for the FM
     * chip
     */
    /*
     * First try to compute the block -value (octave) where the note
     * belongs
     */

    f = freq;

    octave = 5;

    if (f == 0)
	octave = 0;
    else if (f < 261) {
	while (f < 261) {
	    octave--;
	    f <<= 1;
	}
    } else if (f > 493) {
	while (f > 493) {
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
opl3_command(int io_addr, u_int addr, u_int val)
{
    int             i;

    /*
     * The original 2-OP synth requires a quite long delay after writing
     * to a register. The OPL-3 survives with just two INBs
     */

    outb(io_addr, (u_char) (addr & 0xff));

    if (!devc->model != 2)
	DELAY(10);
    else
	for (i = 0; i < 2; i++)
	    inb(io_addr);

    outb(io_addr + 1, (u_char) (val & 0xff));

    if (devc->model != 2)
	DELAY(30);
    else
	for (i = 0; i < 2; i++)
	    inb(io_addr);
}

static void
opl3_reset(int dev)
{
    int             i;

    for (i = 0; i < 18; i++)
	devc->lv_map[i] = i;

    for (i = 0; i < devc->nr_voice; i++) {
	opl3_command(pv_map[devc->lv_map[i]].ioaddr,
	       KSL_LEVEL + pv_map[devc->lv_map[i]].op[0], 0xff);

	opl3_command(pv_map[devc->lv_map[i]].ioaddr,
	       KSL_LEVEL + pv_map[devc->lv_map[i]].op[1], 0xff);

	if (pv_map[devc->lv_map[i]].voice_mode == 4) {
	    opl3_command(pv_map[devc->lv_map[i]].ioaddr,
		   KSL_LEVEL + pv_map[devc->lv_map[i]].op[2], 0xff);

	    opl3_command(pv_map[devc->lv_map[i]].ioaddr,
		   KSL_LEVEL + pv_map[devc->lv_map[i]].op[3], 0xff);
	}
	opl3_kill_note(dev, i, 0, 64);
    }

    if (devc->model == 2) {
	devc->v_alloc->max_voice = devc->nr_voice = 18;

	for (i = 0; i < 18; i++)
	    pv_map[i].voice_mode = 2;

    }
}

static int
opl3_open(int dev, int mode)
{
    int             i;

    if (devc->busy)
	return -(EBUSY);
    devc->busy = 1;

    devc->v_alloc->max_voice = devc->nr_voice = (devc->model == 2) ? 18 : 9;
    devc->v_alloc->timestamp = 0;

    for (i = 0; i < 18; i++) {
	devc->v_alloc->map[i] = 0;
	devc->v_alloc->alloc_times[i] = 0;
    }

    devc->cmask = 0x00;	/* Just 2 OP mode */
    if (devc->model == 2)
	opl3_command(devc->right_io, CONNECTION_SELECT_REGISTER, devc->cmask);
    return 0;
}

static void
opl3_close(int dev)
{
    devc->busy = 0;
    devc->v_alloc->max_voice = devc->nr_voice = (devc->model == 2) ? 18 : 9;

    devc->fm_info.nr_drums = 0;
    devc->fm_info.perc_mode = 0;

    opl3_reset(dev);
}

static void
opl3_hw_control(int dev, u_char *event)
{
}

static int
opl3_load_patch(int dev, int format, snd_rw_buf * addr,
		int offs, int count, int pmgr_flag)
{
    struct sbi_instrument ins;

    if (count < sizeof(ins)) {
	printf("FM Error: Patch record too short\n");
	return -(EINVAL);
    }
    if (uiomove(&((char *) &ins)[offs], sizeof(ins) - offs, addr)) {
	printf("sb: Bad copyin()!\n");
    };

    if (ins.channel < 0 || ins.channel >= SBFM_MAXINSTR) {
	printf("FM Error: Invalid instrument number %d\n", ins.channel);
	return -(EINVAL);
    }
    ins.key = format;

    return store_instr(ins.channel, &ins);
}

static void
opl3_panning(int dev, int voice, int pressure)
{
}

static void
opl3_volume_method(int dev, int mode)
{
}

#define SET_VIBRATO(cell) { \
      tmp = instr->operators[(cell-1)+(((cell-1)/2)*OFFS_4OP)]; \
      if (pressure > 110) \
	tmp |= 0x40;		/* Vibrato on */ \
      opl3_command (map->ioaddr, AM_VIB + map->op[cell-1], tmp);}

static void
opl3_aftertouch(int dev, int voice, int pressure)
{
    int             tmp;
    struct sbi_instrument *instr;
    struct physical_voice_info *map;

    if (voice < 0 || voice >= devc->nr_voice)
	return;

    map = &pv_map[devc->lv_map[voice]];

    DEB(printf("Aftertouch %d\n", voice));

    if (map->voice_mode == 0)
	return;

    /*
     * Adjust the amount of vibrato depending the pressure
     */

    instr = devc->act_i[voice];

    if (!instr)
	instr = &devc->i_map[0];

    if (devc->voc[voice].mode == 4) {
	int             connection = ((instr->operators[10] & 0x01) << 1) | (instr->operators[10 + OFFS_4OP] & 0x01);

	switch (connection) {
	case 0:
	    SET_VIBRATO(4);
	    break;

	case 1:
	    SET_VIBRATO(2);
	    SET_VIBRATO(4);
	    break;

	case 2:
	    SET_VIBRATO(1);
	    SET_VIBRATO(4);
	    break;

	case 3:
	    SET_VIBRATO(1);
	    SET_VIBRATO(3);
	    SET_VIBRATO(4);
	    break;

	}
	/*
	 * Not implemented yet
	 */
    } else {
	SET_VIBRATO(1);

	if ((instr->operators[10] & 0x01))	/* Additive synthesis */
	    SET_VIBRATO(2);
    }
}

#undef SET_VIBRATO

static void
bend_pitch(int dev, int voice, int value)
{
    u_char   data;
    int             block, fnum, freq;
    struct physical_voice_info *map;

    map = &pv_map[devc->lv_map[voice]];

    if (map->voice_mode == 0)
	return;

    devc->voc[voice].bender = value;
    if (!value)
	return;
    if (!(devc->voc[voice].keyon_byte & 0x20))
	return;		/* Not keyed on */

    freq = compute_finetune(devc->voc[voice].orig_freq, devc->voc[voice].bender, devc->voc[voice].bender_range);
    devc->voc[voice].current_freq = freq;

    freq_to_fnum(freq, &block, &fnum);

    data = fnum & 0xff;	/* Least significant bits of fnumber */
    opl3_command(map->ioaddr, FNUM_LOW + map->voice_num, data);

    data = 0x20 | ((block & 0x7) << 2) | ((fnum >> 8) & 0x3);
	/* KEYON|OCTAVE|MS  bits of f-num */
    devc->voc[voice].keyon_byte = data;
    opl3_command(map->ioaddr, KEYON_BLOCK + map->voice_num, data);
}

static void
opl3_controller(int dev, int voice, int ctrl_num, int value)
{
    if (voice < 0 || voice >= devc->nr_voice)
	return;

    switch (ctrl_num) {
    case CTRL_PITCH_BENDER:
	bend_pitch(dev, voice, value);
	break;

    case CTRL_PITCH_BENDER_RANGE:
	devc->voc[voice].bender_range = value;
	break;

    case CTL_MAIN_VOLUME:
	devc->voc[voice].volume = value / 128;
	break;
    }
}

static int
opl3_patchmgr(int dev, struct patmgr_info * rec)
{
    return -(EINVAL);
}

static void
opl3_bender(int dev, int voice, int value)
{
    if (voice < 0 || voice >= devc->nr_voice)
	return;

    bend_pitch(dev, voice, value - 8192);
}

static int
opl3_alloc_voice(int dev, int chn, int note, struct voice_alloc_info * alloc)
{
    int             i, p, best, first, avail, best_time = 0x7fffffff;
    struct sbi_instrument *instr;
    int             is4op;
    int             instr_no;

    if (chn < 0 || chn > 15)
	instr_no = 0;
    else
	instr_no = devc->chn_info[chn].pgm_num;

    instr = &devc->i_map[instr_no];
    if (instr->channel < 0 ||	/* Instrument not loaded */
	    devc->nr_voice != 12)	/* Not in 4 OP mode */
	is4op = 0;
    else if (devc->nr_voice == 12)	/* 4 OP mode */
	is4op = (instr->key == OPL3_PATCH);
    else
	is4op = 0;

    if (is4op) {
	first = p = 0;
	avail = 6;
    } else {
	if (devc->nr_voice == 12)	/* 4 OP mode. Use the '2 OP
					 * only' operators first */
	    first = p = 6;
	else
	    first = p = 0;
	avail = devc->nr_voice;
    }

    /*
     * Now try to find a free voice
     */
    best = first;

    for (i = 0; i < avail; i++) {
	if (alloc->map[p] == 0) {
	    return p;
	}
	if (alloc->alloc_times[p] < best_time) { /* Find oldest playing note */
	    best_time = alloc->alloc_times[p];
	    best = p;
	}
	p = (p + 1) % avail;
    }

    /*
     * Insert some kind of priority mechanism here.
     */

    if (best < 0)
	best = 0;
    if (best > devc->nr_voice)
	best -= devc->nr_voice;

    return best;		/* All devc->voc in use. Select the first
				 * one. */
}

static void
opl3_setup_voice(int dev, int voice, int chn)
{
    struct channel_info *info =
	&synth_devs[dev]->chn_info[chn];

    opl3_set_instr(dev, voice, info->pgm_num);

    devc->voc[voice].bender = info->bender_value;
    devc->voc[voice].volume = info->controllers[CTL_MAIN_VOLUME];
}

static struct synth_operations opl3_operations =
{
	NULL,
	0,
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
	opl3_volume_method,
	opl3_patchmgr,
	opl3_bender,
	opl3_alloc_voice,
	opl3_setup_voice
};

void
opl3_init(int ioaddr, sound_os_info * osp)
{
    int             i;

    if (num_synths >= MAX_SYNTH_DEV) {
	printf("OPL3 Error: Too many synthesizers\n");
	return ;
    }
    if (devc == NULL) {
	printf("OPL3: Device control structure not initialized.\n");
	return ;
    }
    bzero((char *) devc, sizeof(*devc));
    devc->osp = osp;

    devc->nr_voice = 9;
    strcpy(devc->fm_info.name, "OPL2-");

    devc->fm_info.device = 0;
    devc->fm_info.synth_type = SYNTH_TYPE_FM;
    devc->fm_info.synth_subtype = FM_TYPE_ADLIB;
    devc->fm_info.perc_mode = 0;
    devc->fm_info.nr_voices = 9;
    devc->fm_info.nr_drums = 0;
    devc->fm_info.instr_bank_size = SBFM_MAXINSTR;
    devc->fm_info.capabilities = 0;
    devc->left_io = ioaddr;
    devc->right_io = ioaddr + 2;

    if (detected_model <= 2)
	devc->model = 1;
    else {
	devc->model = 2;
	if (detected_model == 4)
	    devc->is_opl4 = 1;
    }

    opl3_operations.info = &devc->fm_info;

    synth_devs[num_synths++] = &opl3_operations;
    devc->v_alloc = &opl3_operations.alloc;
    devc->chn_info = &opl3_operations.chn_info[0];

    if (devc->model == 2) {
	if (devc->is_opl4)
	    conf_printf2("Yamaha OPL4/OPL3 FM", ioaddr, 0, -1, -1);
	else
	    conf_printf2("Yamaha OPL3 FM", ioaddr, 0, -1, -1);

	devc->v_alloc->max_voice = devc->nr_voice = 18;
	devc->fm_info.nr_drums = 0;
	devc->fm_info.capabilities |= SYNTH_CAP_OPL3;
	strcpy(devc->fm_info.name, "Yamaha OPL-3");

	for (i = 0; i < 18; i++)
	    if (pv_map[i].ioaddr == USE_LEFT)
		pv_map[i].ioaddr = devc->left_io;
	    else
		pv_map[i].ioaddr = devc->right_io;

	    opl3_command(devc->right_io, OPL3_MODE_REGISTER, OPL3_ENABLE);
	    opl3_command(devc->right_io, CONNECTION_SELECT_REGISTER, 0x00);
    } else {
	conf_printf2("Yamaha OPL2 FM", ioaddr, 0, -1, -1);
	devc->v_alloc->max_voice = devc->nr_voice = 9;
	devc->fm_info.nr_drums = 0;

	for (i = 0; i < 18; i++)
	    pv_map[i].ioaddr = devc->left_io;
    };

    for (i = 0; i < SBFM_MAXINSTR; i++)
	devc->i_map[i].channel = -1;

    return ;
}

#endif
