/*
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
/*
 *
 * Ported to the new Audio Driver by Luigi Rizzo:
 * (C) 1999 Seigo Tanimura
 *
 * This is the OPL2/3/4 chip driver for FreeBSD, based on the Luigi Sound Driver.
 * This handles io against /dev/midi, the midi {in, out}put event queues
 * and the event/message operation to the OPL chip.
 *
 * $FreeBSD$
 *
 */

#include <dev/sound/midi/midi.h>
#include <dev/sound/chip.h>

#include <isa/isavar.h>

static devclass_t midi_devclass;

#ifndef DDB
#undef DDB
#define DDB(x)
#endif /* DDB */

/*
 *	The OPL-3 mode is switched on by writing 0x01, to the offset 5
 *	of the right side.
 *
 *	Another special register at the right side is at offset 4. It contains
 *	a bit mask defining which voices are used as 4 OP voices.
 *
 *	The percussive mode is implemented in the left side only.
 *
 *	With the above exeptions the both sides can be operated independently.
 *	
 *	A 4 OP voice can be created by setting the corresponding
 *	bit at offset 4 of the right side.
 *
 *	For example setting the rightmost bit (0x01) changes the
 *	first voice on the right side to the 4 OP mode. The fourth
 *	voice is made inaccessible.
 *
 *	If a voice is set to the 2 OP mode, it works like 2 OP modes
 *	of the original YM3812 (AdLib). In addition the voice can 
 *	be connected the left, right or both stereo channels. It can
 *	even be left unconnected. This works with 4 OP voices also.
 *
 *	The stereo connection bits are located in the FEEDBACK_CONNECTION
 *	register of the voice (0xC0-0xC8). In 4 OP voices these bits are
 *	in the second half of the voice.
 */

/*
 *	Register numbers for the global registers
 */

#define TEST_REGISTER				0x01
#define   ENABLE_WAVE_SELECT		0x20

#define TIMER1_REGISTER				0x02
#define TIMER2_REGISTER				0x03
#define TIMER_CONTROL_REGISTER			0x04	/* Left side */
#define   IRQ_RESET			0x80
#define   TIMER1_MASK			0x40
#define   TIMER2_MASK			0x20
#define   TIMER1_START			0x01
#define   TIMER2_START			0x02

#define CONNECTION_SELECT_REGISTER		0x04	/* Right side */
#define   RIGHT_4OP_0			0x01
#define   RIGHT_4OP_1			0x02
#define   RIGHT_4OP_2			0x04
#define   LEFT_4OP_0			0x08
#define   LEFT_4OP_1			0x10
#define   LEFT_4OP_2			0x20

#define OPL3_MODE_REGISTER			0x05	/* Right side */
#define   OPL3_ENABLE			0x01
#define   OPL4_ENABLE			0x02

#define KBD_SPLIT_REGISTER			0x08	/* Left side */
#define   COMPOSITE_SINE_WAVE_MODE	0x80		/* Don't use with OPL-3? */
#define   KEYBOARD_SPLIT		0x40

#define PERCUSSION_REGISTER			0xbd	/* Left side only */
#define   TREMOLO_DEPTH			0x80
#define   VIBRATO_DEPTH			0x40
#define	  PERCUSSION_ENABLE		0x20
#define   BASSDRUM_ON			0x10
#define   SNAREDRUM_ON			0x08
#define   TOMTOM_ON			0x04
#define   CYMBAL_ON			0x02
#define   HIHAT_ON			0x01

/*
 *	Offsets to the register banks for operators. To get the
 *	register number just add the operator offset to the bank offset
 *
 *	AM/VIB/EG/KSR/Multiple (0x20 to 0x35)
 */
#define AM_VIB					0x20
#define   TREMOLO_ON			0x80
#define   VIBRATO_ON			0x40
#define   SUSTAIN_ON			0x20
#define   KSR				0x10 	/* Key scaling rate */
#define   MULTIPLE_MASK		0x0f	/* Frequency multiplier */

/*
 *	KSL/Total level (0x40 to 0x55)
 */
#define KSL_LEVEL				0x40
#define   KSL_MASK			0xc0	/* Envelope scaling bits */
#define   TOTAL_LEVEL_MASK		0x3f	/* Strength (volume) of OP */

/*
 *	Attack / Decay rate (0x60 to 0x75)
 */
#define ATTACK_DECAY				0x60
#define   ATTACK_MASK			0xf0
#define   DECAY_MASK			0x0f

/*
 * Sustain level / Release rate (0x80 to 0x95)
 */
#define SUSTAIN_RELEASE				0x80
#define   SUSTAIN_MASK			0xf0
#define   RELEASE_MASK			0x0f

/*
 * Wave select (0xE0 to 0xF5)
 */
#define WAVE_SELECT			0xe0

/*
 *	Offsets to the register banks for voices. Just add to the
 *	voice number to get the register number.
 *
 *	F-Number low bits (0xA0 to 0xA8).
 */
#define FNUM_LOW				0xa0

/*
 *	F-number high bits / Key on / Block (octave) (0xB0 to 0xB8)
 */
#define KEYON_BLOCK					0xb0
#define	  KEYON_BIT				0x20
#define	  BLOCKNUM_MASK				0x1c
#define   FNUM_HIGH_MASK			0x03

/*
 *	Feedback / Connection (0xc0 to 0xc8)
 *
 *	These registers have two new bits when the OPL-3 mode
 *	is selected. These bits controls connecting the voice
 *	to the stereo channels. For 4 OP voices this bit is
 *	defined in the second half of the voice (add 3 to the
 *	register offset).
 *
 *	For 4 OP voices the connection bit is used in the
 *	both halfs (gives 4 ways to connect the operators).
 */
#define FEEDBACK_CONNECTION				0xc0
#define   FEEDBACK_MASK				0x0e	/* Valid just for 1st OP of a voice */
#define   CONNECTION_BIT			0x01
/*
 *	In the 4 OP mode there is four possible configurations how the
 *	operators can be connected together (in 2 OP modes there is just
 *	AM or FM). The 4 OP connection mode is defined by the rightmost
 *	bit of the FEEDBACK_CONNECTION (0xC0-0xC8) on the both halfs.
 *
 *	First half	Second half	Mode
 *
 *					 +---+
 *					 v   |
 *	0		0		>+-1-+--2--3--4-->
 *
 *
 *					
 *					 +---+
 *					 |   |
 *	0		1		>+-1-+--2-+
 *						  |->
 *					>--3----4-+
 *					
 *					 +---+
 *					 |   |
 *	1		0		>+-1-+-----+
 *						   |->
 *					>--2--3--4-+
 *
 *					 +---+
 *					 |   |
 *	1		1		>+-1-+--+
 *						|
 *					>--2--3-+->
 *						|
 *					>--4----+
 */
#define   STEREO_BITS				0x30	/* OPL-3 only */
#define     VOICE_TO_LEFT		0x10
#define     VOICE_TO_RIGHT		0x20

/*
 * 	Definition table for the physical voices
 */

struct physical_voice_info {
	unsigned char voice_num;
	unsigned char voice_mode; /* 0=unavailable, 2=2 OP, 4=4 OP */
	int ch; /* channel (left=USE_LEFT, right=USE_RIGHT) */
	unsigned char op[4]; /* Operator offsets */
};

/*
 *	There is 18 possible 2 OP voices
 *	(9 in the left and 9 in the right).
 *	The first OP is the modulator and 2nd is the carrier.
 *
 *	The first three voices in the both sides may be connected
 *	with another voice to a 4 OP voice. For example voice 0
 *	can be connected with voice 3. The operators of voice 3 are
 *	used as operators 3 and 4 of the new 4 OP voice.
 *	In this case the 2 OP voice number 0 is the 'first half' and
 *	voice 3 is the second.
 */

#define USE_LEFT	0
#define USE_RIGHT	1

static struct physical_voice_info pv_map[18] =
{
/*       No Mode Side		OP1	OP2	OP3   OP4	*/
/*	---------------------------------------------------	*/
	{ 0,  2, USE_LEFT,	{0x00,	0x03,	0x08, 0x0b}},
	{ 1,  2, USE_LEFT,	{0x01,	0x04,	0x09, 0x0c}},
	{ 2,  2, USE_LEFT,	{0x02,	0x05,	0x0a, 0x0d}},

	{ 3,  2, USE_LEFT,	{0x08,	0x0b,	0x00, 0x00}},
	{ 4,  2, USE_LEFT,	{0x09,	0x0c,	0x00, 0x00}},
	{ 5,  2, USE_LEFT,	{0x0a,	0x0d,	0x00, 0x00}},

	{ 6,  2, USE_LEFT,	{0x10,	0x13,	0x00, 0x00}}, /* Used by percussive voices */
	{ 7,  2, USE_LEFT,	{0x11,	0x14,	0x00, 0x00}}, /* if the percussive mode */
	{ 8,  2, USE_LEFT,	{0x12,	0x15,	0x00, 0x00}}, /* is selected */

	{ 0,  2, USE_RIGHT,	{0x00,	0x03,	0x08, 0x0b}},
	{ 1,  2, USE_RIGHT,	{0x01,	0x04,	0x09, 0x0c}},
	{ 2,  2, USE_RIGHT,	{0x02,	0x05,	0x0a, 0x0d}},

	{ 3,  2, USE_RIGHT,	{0x08,	0x0b,	0x00, 0x00}},
	{ 4,  2, USE_RIGHT,	{0x09,	0x0c,	0x00, 0x00}},
	{ 5,  2, USE_RIGHT,	{0x0a,	0x0d,	0x00, 0x00}},

	{ 6,  2, USE_RIGHT,	{0x10,	0x13,	0x00, 0x00}},
	{ 7,  2, USE_RIGHT,	{0x11,	0x14,	0x00, 0x00}},
	{ 8,  2, USE_RIGHT,	{0x12,	0x15,	0x00, 0x00}}
};

/* These are the tuning parameters. */
static unsigned short semitone_tuning[24] = 
{
/*   0 */ 10000, 10595, 11225, 11892, 12599, 13348, 14142, 14983, 
/*   8 */ 15874, 16818, 17818, 18877, 20000, 21189, 22449, 23784, 
/*  16 */ 25198, 26697, 28284, 29966, 31748, 33636, 35636, 37755
};

static unsigned short cent_tuning[100] =
{
/*   0 */ 10000, 10006, 10012, 10017, 10023, 10029, 10035, 10041, 
/*   8 */ 10046, 10052, 10058, 10064, 10070, 10075, 10081, 10087, 
/*  16 */ 10093, 10099, 10105, 10110, 10116, 10122, 10128, 10134, 
/*  24 */ 10140, 10145, 10151, 10157, 10163, 10169, 10175, 10181, 
/*  32 */ 10187, 10192, 10198, 10204, 10210, 10216, 10222, 10228, 
/*  40 */ 10234, 10240, 10246, 10251, 10257, 10263, 10269, 10275, 
/*  48 */ 10281, 10287, 10293, 10299, 10305, 10311, 10317, 10323, 
/*  56 */ 10329, 10335, 10341, 10347, 10353, 10359, 10365, 10371, 
/*  64 */ 10377, 10383, 10389, 10395, 10401, 10407, 10413, 10419, 
/*  72 */ 10425, 10431, 10437, 10443, 10449, 10455, 10461, 10467, 
/*  80 */ 10473, 10479, 10485, 10491, 10497, 10503, 10509, 10515, 
/*  88 */ 10521, 10528, 10534, 10540, 10546, 10552, 10558, 10564, 
/*  96 */ 10570, 10576, 10582, 10589
};

/*
 * The next table looks magical, but it certainly is not. Its values have
 * been calculated as table[i]=8*log(i/64)/log(2) with an obvious exception
 * for i=0. This log-table converts a linear volume-scaling (0..127) to a
 * logarithmic scaling as present in the FM-synthesizer chips. so :    Volume
 * 64 =  0 db = relative volume  0 and:    Volume 32 = -6 db = relative
 * volume -8 it was implemented as a table because it is only 128 bytes and
 * it saves a lot of log() calculations. (RH)
 */
static char         opl_volumetable[128] =
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

#define MAX_VOICE 18
#define OFFS_4OP 11
#define SBFM_MAXINSTR 256

/* These are the OPL Models. */
#define MODEL_NONE 0
#define MODEL_OPL2 2
#define MODEL_OPL3 3
#define MODEL_OPL4 4

/* These are the OPL Voice modes. */
#define VOICE_NONE 0
#define VOICE_2OP 2
#define VOICE_4OP 4

/* PnP IDs */
static struct isa_pnp_id opl_ids[] = {
	{0x01200001, "@H@2001 FM Synthesizer"},	/* @H@2001 */
	{0x01100001, "@H@1001 FM Synthesizer"},	/* @H@1001 */
#if notdef
	/* TODO: write bridge drivers for these devices. */
	{0x0000630e, "CSC0000 FM Synthesizer"},	/* CSC0000 */
	{0x68187316, "ESS1868 FM Synthesizer"},	/* ESS1868 */
	{0x79187316, "ESS1879 FM Synthesizer"},	/* ESS1879 */
	{0x2100a865, "YMH0021 FM Synthesizer"},	/* YMH0021 */
	{0x80719304, "ADS7180 FM Synthesizer"},	/* ADS7180 */
	{0x0300561e, "GRV0003 FM Synthesizer"},	/* GRV0003 */
#endif /* notdef */
};

/* These are the default io bases. */
static int opl_defaultiobase[] = {
	0x388,
	0x380,
};

/* These are the per-voice information. */
struct voice_info {
	u_char   keyon_byte;
	long            bender;
	long            bender_range;
	u_long   orig_freq;
	u_long   current_freq;
	int             volume;
	int             mode;
};

/* These are the synthesizer and the midi device information. */
static struct synth_info opl_synthinfo = {
	"OPL FM Synthesizer",
	0,
	SYNTH_TYPE_FM,
	FM_TYPE_ADLIB,
	0,
	9,
	0,
	SBFM_MAXINSTR,
	0,
};

static struct midi_info opl_midiinfo = {
	"OPL FM Synthesizer",
	0,
	0,
	0,
};

/*
 * These functions goes into oplsynthdev_op_desc.
 */
static mdsy_killnote_t opl_killnote;
static mdsy_setinstr_t opl_setinstr;
static mdsy_startnote_t opl_startnote;
static mdsy_reset_t opl_reset;
static mdsy_hwcontrol_t opl_hwcontrol;
static mdsy_loadpatch_t opl_loadpatch;
static mdsy_panning_t opl_panning;
static mdsy_aftertouch_t opl_aftertouch;
static mdsy_controller_t opl_controller;
static mdsy_patchmgr_t opl_patchmgr;
static mdsy_bender_t opl_bender;
static mdsy_allocvoice_t opl_allocvoice;
static mdsy_setupvoice_t opl_setupvoice;
static mdsy_sendsysex_t opl_sendsysex;
static mdsy_prefixcmd_t opl_prefixcmd;
static mdsy_volumemethod_t opl_volumemethod;

/*
 * This is the synthdev_info for an OPL3 chip.
 */
static synthdev_info oplsynth_op_desc = {
	opl_killnote,
	opl_setinstr,
	opl_startnote,
	opl_reset,
	opl_hwcontrol,
	opl_loadpatch,
	opl_panning,
	opl_aftertouch,
	opl_controller,
	opl_patchmgr,
	opl_bender,
	opl_allocvoice,
	opl_setupvoice,
	opl_sendsysex,
	opl_prefixcmd,
	opl_volumemethod,
};

/* Here is the parameter structure per a device. */
struct opl_softc {
	device_t dev; /* device information */
	mididev_info *devinfo; /* midi device information */

	struct mtx mtx; /* Mutex to protect the device. */

	struct resource *io; /* Base of io port */
	int io_rid; /* Io resource ID */

	int model; /* OPL model */
	struct synth_info synthinfo; /* Synthesizer information */

	struct sbi_instrument i_map[SBFM_MAXINSTR]; /* Instrument map */
	struct sbi_instrument *act_i[SBFM_MAXINSTR]; /* Active instruments */
	struct physical_voice_info pv_map[MAX_VOICE]; /* Physical voice map */
	int cmask; /* Connection mask */
	int lv_map[MAX_VOICE]; /* Level map */
	struct voice_info voc[MAX_VOICE]; /* Voice information */
};

typedef struct opl_softc *sc_p;

/*
 * These functions goes into opl_op_desc to get called
 * from sound.c.
 */

static int opl_probe(device_t dev);
static int opl_probe1(sc_p scp);
static int opl_attach(device_t dev);
static int oplsbc_probe(device_t dev);
static int oplsbc_attach(device_t dev);

static d_open_t opl_open;
static d_close_t opl_close;
static d_ioctl_t opl_ioctl;
static midi_callback_t opl_callback;

/* These go to snddev_info. */
static mdsy_readraw_t opl_readraw;
static mdsy_writeraw_t opl_writeraw;

/* These functions are local. */
static void opl_command(sc_p scp, int ch, int addr, u_int val);
static int opl_status(sc_p scp);
static void opl_enter4opmode(sc_p scp);
static void opl_storeinstr(sc_p scp, int instr_no, struct sbi_instrument *instr);
static void opl_calcvol(u_char *regbyte, int volume, int main_vol);
static void opl_setvoicevolume(sc_p scp, int voice, int volume, int main_vol);
static void opl_freqtofnum(int freq, int *block, int *fnum);
static int opl_bendpitch(sc_p scp, int voice, int val);
static int opl_notetofreq(int note_num);
static u_long opl_computefinetune(u_long base_freq, int bend, int range);
static int opl_allocres(sc_p scp, device_t dev);
static void opl_releaseres(sc_p scp, device_t dev);

/*
 * This is the device descriptor for the midi device.
 */
static mididev_info opl_op_desc = {
	"OPL FM Synthesizer",

	SNDCARD_OPL,

	opl_open,
	opl_close,
	opl_ioctl,

	opl_callback,

	MIDI_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

/*
 * Here are the main functions to interact to the user process.
 */

static int
opl_probe(device_t dev)
{
	sc_p scp;
	int unit, i;

	/* Check isapnp ids */
	if (isa_get_logicalid(dev) != 0)
		return (ISA_PNP_PROBE(device_get_parent(dev), dev, opl_ids));

	scp = device_get_softc(dev);
	unit = device_get_unit(dev);

	device_set_desc(dev, opl_op_desc.name);
	bzero(scp, sizeof(*scp));

	MIDI_DEBUG(printf("opl%d: probing.\n", unit));

	scp->io_rid = 0;
	scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 4, RF_ACTIVE);
	if (opl_allocres(scp, dev)) {
		/* We try the defaults in opl_defaultiobase. */
		MIDI_DEBUG(printf("opl%d: port is omitted, trying the defaults.\n", unit));
		for (i = 0 ; i < sizeof(opl_defaultiobase) / sizeof(*opl_defaultiobase) ; i++) {
			scp->io_rid = 0;
			scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, opl_defaultiobase[i], opl_defaultiobase[i] + 1, 4, RF_ACTIVE);
			if (scp->io != NULL) {
				if (opl_probe1(scp))
					opl_releaseres(scp, dev);
				else
					break;
			}
		}
		if (scp->io == NULL)
			return (ENXIO);
	} else if(opl_probe1(scp)) {
		opl_releaseres(scp, dev);
		return (ENXIO);
	}

	/* We now have some kind of OPL. */

	MIDI_DEBUG(printf("opl%d: probed.\n", unit));

	return (0);
}

/* We do probe in this function. */
static int
opl_probe1(sc_p scp)
{
	u_char stat1, stat2;

	/* Reset the timers and the interrupt. */
	opl_command(scp, USE_LEFT, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
	opl_command(scp, USE_LEFT, TIMER_CONTROL_REGISTER, IRQ_RESET);

	/* Read the status. */
	stat1 = opl_status(scp);
	if ((stat1 & 0xe0) != 0)
		return (1);

	/* Try firing the timer1. */
	opl_command(scp, USE_LEFT, TIMER1_REGISTER, 0xff); /* Set the timer value. */
	opl_command(scp, USE_LEFT, TIMER_CONTROL_REGISTER, TIMER1_START | TIMER2_MASK); /* Start the timer. */
	DELAY(150); /* Wait for the timer. */

	/* Read the status. */
	stat2 = opl_status(scp);

	/* Reset the timers and the interrupt. */
	opl_command(scp, USE_LEFT, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
	opl_command(scp, USE_LEFT, TIMER_CONTROL_REGISTER, IRQ_RESET);

	if ((stat2 & 0xe0) != 0xc0)
		return (1);

	return (0);
}

static int
oplsbc_probe(device_t dev)
{
	char *s;
	sc_p scp;
	struct sndcard_func *func;

	/* The parent device has already been probed. */

	func = device_get_ivars(dev);
	if (func == NULL || func->func != SCF_SYNTH)
		return (ENXIO);

	s = "SB OPL FM Synthesizer";

	scp = device_get_softc(dev);
	bzero(scp, sizeof(*scp));
	scp->io_rid = 2;
	device_set_desc(dev, s);
	return (0);
}

static int
opl_attach(device_t dev)
{
	sc_p scp;
	mididev_info *devinfo;
	int i, opl4_io, opl4_id;
	struct resource *opl4;
	u_char signature, tmp;

	scp = device_get_softc(dev);

	MIDI_DEBUG(printf("opl: attaching.\n"));

	/* Fill the softc for this unit. */
	scp->dev = dev;

	/* Allocate other resources. */
	if (opl_allocres(scp, dev)) {
		opl_releaseres(scp, dev);
		return (ENXIO);
	}

	/* Detect the OPL type. */
	signature = opl_status(scp);
	if (signature == 0x06)
		scp->model = MODEL_OPL2;
	else {
		/* OPL3 or later, might be OPL4. */

		/* Enable OPL3 and OPL4. */
		opl_command(scp, USE_RIGHT, OPL3_MODE_REGISTER, 0);
		opl_command(scp, USE_RIGHT, OPL3_MODE_REGISTER, OPL3_ENABLE | OPL4_ENABLE);

		tmp = opl_status(scp);
		if (tmp != 0x02)
			scp->model = MODEL_OPL3;
#if notdef
		else {
#endif /* notdef */
			/* Alloc OPL4 ID register. */
			opl4_id = 2;
			opl4_io = rman_get_start(scp->io) - 8;
			opl4 = bus_alloc_resource(dev, SYS_RES_IOPORT, &opl4_id, opl4_io, opl4_io + 1, 2, RF_ACTIVE);
			if (opl4 != NULL) {
				/* Select OPL4 ID register. */
				bus_space_write_1(rman_get_bustag(opl4), rman_get_bushandle(opl4), 0, 0x02);
				DELAY(10);
				tmp = bus_space_read_1(rman_get_bustag(opl4), rman_get_bushandle(opl4), 1);
				DELAY(10);

				if (tmp != 0x20)
					scp->model = MODEL_OPL3;
				else {
					scp->model = MODEL_OPL4;

					/* Select back OPL4 FM mixer control. */
					bus_space_write_1(rman_get_bustag(opl4), rman_get_bushandle(opl4), 0, 0xf8);
					DELAY(10);
					bus_space_write_1(rman_get_bustag(opl4), rman_get_bushandle(opl4), 1, 0x1b);
					DELAY(10);
				}
				bus_release_resource(dev, SYS_RES_IOPORT, opl4_id, opl4);
			}
#if notdef
		}
#endif /* notdef */
		opl_command(scp, USE_RIGHT, OPL3_MODE_REGISTER, 0);
	}

	/* Kill any previous notes. */
	for (i = 0 ; i < 9 ; i++)
		opl_command(scp, USE_RIGHT, KEYON_BLOCK + i, 0);

	/* Select melodic mode. */
	opl_command(scp, USE_LEFT, TEST_REGISTER, ENABLE_WAVE_SELECT);
	opl_command(scp, USE_LEFT, PERCUSSION_REGISTER, 0);

	for (i = 0 ; i < SBFM_MAXINSTR ; i++)
		scp->i_map[i].channel = -1;

	/* Fill the softc. */
	bcopy(&opl_synthinfo, &scp->synthinfo, sizeof(opl_synthinfo));
	snprintf(scp->synthinfo.name, 64, "Yamaha OPL%d FM", scp->model);
	mtx_init(&scp->mtx, "oplmid", MTX_DEF);
	bcopy(pv_map, scp->pv_map, sizeof(pv_map));
	if (scp->model < MODEL_OPL3) { /* OPL2. */
		scp->synthinfo.nr_voices = 9;
		scp->synthinfo.nr_drums = 0;
		for (i = 0 ; i < MAX_VOICE ; i++)
			scp->pv_map[i].ch = USE_LEFT;
	} else { /* OPL3 or later. */
		scp->synthinfo.capabilities |= SYNTH_CAP_OPL3;
		scp->synthinfo.nr_voices = 18;
		scp->synthinfo.nr_drums = 0;
#if notdef
		for (i = 0 ; i < MAX_VOICE ; i++) {
			if (scp->pv_map[i].ch == USE_LEFT)
				scp->pv_map[i].ch = USE_LEFT;
			else
				scp->pv_map[i].ch = USE_RIGHT;
		}
#endif /* notdef */
		opl_command(scp, USE_RIGHT, OPL3_MODE_REGISTER, OPL3_ENABLE);
		opl_command(scp, USE_RIGHT, CONNECTION_SELECT_REGISTER, 0);
	}

	scp->devinfo = devinfo = create_mididev_info_unit(MDT_SYNTH, &opl_op_desc, &oplsynth_op_desc);

	/* Fill the midi info. */
	devinfo->synth.readraw = opl_readraw;
	devinfo->synth.writeraw = opl_writeraw;
	devinfo->synth.alloc.max_voice = scp->synthinfo.nr_voices;
	strcpy(devinfo->name, scp->synthinfo.name);
	snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x", (u_int)rman_get_start(scp->io));

	midiinit(devinfo, dev);

	MIDI_DEBUG(printf("opl: attached.\n"));
	MIDI_DEBUG(printf("opl: the chip is OPL%d.\n", scp->model));

	return (0);
}

static int
oplsbc_attach(device_t dev)
{
	return (opl_attach(dev));
}

static int
opl_open(dev_t i_dev, int flags, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit, i;

	unit = MIDIUNIT(i_dev);

	MIDI_DEBUG(printf("opl%d: opening.\n", unit));

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		MIDI_DEBUG(printf("opl_open: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	mtx_lock(&devinfo->synth.vc_mtx);
	if (scp->model < MODEL_OPL3)
		devinfo->synth.alloc.max_voice = 9;
	else
		devinfo->synth.alloc.max_voice = 18;
	devinfo->synth.alloc.timestamp = 0;
	for (i = 0 ; i < MAX_VOICE ; i++) {
		devinfo->synth.alloc.map[i] = 0;
		devinfo->synth.alloc.alloc_times[i] = 0;
	}
	mtx_unlock(&devinfo->synth.vc_mtx);
	scp->cmask = 0; /* We are in 2 OP mode initially. */
	if (scp->model >= MODEL_OPL3) {
		mtx_lock(&scp->mtx);
		opl_command(scp, USE_RIGHT, CONNECTION_SELECT_REGISTER, scp->cmask);
		mtx_unlock(&scp->mtx);
	}

	MIDI_DEBUG(printf("opl%d: opened.\n", unit));

	return (0);
}

static int
opl_close(dev_t i_dev, int flags, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;

	unit = MIDIUNIT(i_dev);

	MIDI_DEBUG(printf("opl%d: closing.\n", unit));

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		MIDI_DEBUG(printf("opl_close: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	mtx_lock(&devinfo->synth.vc_mtx);
	if (scp->model < MODEL_OPL3)
		devinfo->synth.alloc.max_voice = 9;
	else
		devinfo->synth.alloc.max_voice = 18;
	mtx_unlock(&devinfo->synth.vc_mtx);

	/* Stop the OPL. */
	opl_reset(scp->devinfo);

	MIDI_DEBUG(printf("opl%d: closed.\n", unit));

	return (0);
}

static int
opl_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;
	struct sbi_instrument *ins;

	unit = MIDIUNIT(i_dev);

	MIDI_DEBUG(printf("opl_ioctl: unit %d, cmd %s.\n", unit, midi_cmdname(cmd, cmdtab_midiioctl)));

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		MIDI_DEBUG(printf("opl_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = devinfo->softc;

	switch (cmd) {
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		if (synthinfo->device != unit)
			return (ENXIO);
		bcopy(&scp->synthinfo, synthinfo, sizeof(scp->synthinfo));
		synthinfo->device = unit;
		synthinfo->nr_voices = devinfo->synth.alloc.max_voice;
		if (synthinfo->nr_voices == 12)
			synthinfo->nr_voices = 6;
		return (0);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		if (midiinfo->device != unit)
			return (ENXIO);
		bcopy(&opl_midiinfo, midiinfo, sizeof(opl_midiinfo));
		strcpy(midiinfo->name, scp->synthinfo.name);
		midiinfo->device = unit;
		return (0);
		break;
	case SNDCTL_FM_LOAD_INSTR:
		ins = (struct sbi_instrument *)arg;
		if (ins->channel < 0 || ins->channel >= SBFM_MAXINSTR) {
			printf("opl_ioctl: Instrument number %d is not valid.\n", ins->channel);
			return (EINVAL);
		}
#if notyet
		pmgr_inform(scp, PM_E_PATCH_LOADED, inc->channel, 0, 0, 0);
#endif /* notyet */
		opl_storeinstr(scp, ins->channel, ins);
		return (0);
		break;
	case SNDCTL_SYNTH_MEMAVL:
		*(int *)arg = 0x7fffffff;
		return (0);
		break;
	case SNDCTL_FM_4OP_ENABLE:
		if (scp->model >= MODEL_OPL3)
			opl_enter4opmode(scp);
		return (0);
		break;
	default:
		return (ENOSYS);
	}
	/* NOTREACHED */
	return (EINVAL);
}

static int
opl_callback(void *d, int reason)
{
	int unit;
	sc_p scp;
	mididev_info *devinfo;

	devinfo = (mididev_info *)d;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	if (devinfo == NULL) {
		MIDI_DEBUG(printf("opl_callback: device not configured.\n"));
		return (ENXIO);
	}

	unit = devinfo->unit;
	scp = devinfo->softc;

	MIDI_DEBUG(printf("opl%d: callback, reason 0x%x.\n", unit, reason));

	switch (reason & MIDI_CB_REASON_MASK) {
	case MIDI_CB_START:
		if ((reason & MIDI_CB_RD) != 0 && (devinfo->flags & MIDI_F_READING) == 0)
			/* Begin recording. */
			devinfo->flags |= MIDI_F_READING;
		if ((reason & MIDI_CB_WR) != 0 && (devinfo->flags & MIDI_F_WRITING) == 0)
			/* Start playing. */
			devinfo->flags |= MIDI_F_WRITING;
		break;
	case MIDI_CB_STOP:
	case MIDI_CB_ABORT:
		if ((reason & MIDI_CB_RD) != 0 && (devinfo->flags & MIDI_F_READING) != 0)
			/* Stop recording. */
			devinfo->flags &= ~MIDI_F_READING;
		if ((reason & MIDI_CB_WR) != 0 && (devinfo->flags & MIDI_F_WRITING) != 0)
			/* Stop Playing. */
			devinfo->flags &= ~MIDI_F_WRITING;
		break;
	}

	return (0);
}

static int
opl_readraw(mididev_info *md, u_char *buf, int len, int *lenr, int nonblock)
{
	sc_p scp;
	int unit;

	if (md == NULL)
		return (ENXIO);
	if (lenr == NULL)
		return (EINVAL);

	unit = md->unit;
	scp = md->softc;
	if ((md->fflags & FREAD) == 0) {
		MIDI_DEBUG(printf("opl_readraw: unit %d is not for reading.\n", unit));
		return (EIO);
	}

	/* NOP. */
	*lenr = 0;

	return (0);
}

static int
opl_writeraw(mididev_info *md, u_char *buf, int len, int *lenw, int nonblock)
{
	sc_p scp;
	int unit;

	if (md == NULL)
		return (ENXIO);
	if (lenw == NULL)
		return (EINVAL);

	unit = md->unit;
	scp = md->softc;
	if ((md->fflags & FWRITE) == 0) {
		MIDI_DEBUG(printf("opl_writeraw: unit %d is not for writing.\n", unit));
		return (EIO);
	}

	/* NOP. */
	*lenw = 0;

	return (0);
}

/* The functions below here are the synthesizer interfaces. */

static int
opl_killnote(mididev_info *md, int voice, int note, int vel)
{
	int unit;
	sc_p scp;
	struct physical_voice_info *map;

	scp = md->softc;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: killing a note, voice %d, note %d, vel %d.\n", unit, voice, note, vel));

	if (voice < 0 || voice >= md->synth.alloc.max_voice)
		return (0);

	mtx_lock(&md->synth.vc_mtx);

	md->synth.alloc.map[voice] = 0;
	mtx_lock(&scp->mtx);
	map = &scp->pv_map[scp->lv_map[voice]];

	if (map->voice_mode != VOICE_NONE) {
		opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num, scp->voc[voice].keyon_byte & ~0x20);

		scp->voc[voice].keyon_byte = 0;
		scp->voc[voice].bender = 0;
		scp->voc[voice].volume = 64;
		scp->voc[voice].bender_range = 200;
		scp->voc[voice].orig_freq = 0;
		scp->voc[voice].current_freq = 0;
		scp->voc[voice].mode = 0;
	}

	mtx_unlock(&scp->mtx);
	mtx_unlock(&md->synth.vc_mtx);

	return (0);
}

static int
opl_setinstr(mididev_info *md, int voice, int instr_no)
{
	int unit;
	sc_p scp;

	scp = md->softc;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: setting an instrument, voice %d, instr_no %d.\n", unit, voice, instr_no));


	if (voice < 0 || voice >= md->synth.alloc.max_voice || instr_no < 0 || instr_no >= SBFM_MAXINSTR)
		return (0);

	mtx_lock(&scp->mtx);
	scp->act_i[voice] = &scp->i_map[instr_no];
	mtx_unlock(&scp->mtx);

	return (0);
}

static int
opl_startnote(mididev_info *md, int voice, int note, int volume)
{
	u_char fpc;
	int unit, block, fnum, freq, voice_mode, voice_shift;
	struct sbi_instrument *instr;
	struct physical_voice_info *map;
	sc_p scp;

	scp = md->softc;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: starting a note, voice %d, note %d, volume %d.\n", unit, voice, note, volume));

	if (voice < 0 || voice >= md->synth.alloc.max_voice)
		return (0);

	mtx_lock(&scp->mtx);
	map = &scp->pv_map[scp->lv_map[voice]];
	if (map->voice_mode == VOICE_NONE) {
		mtx_unlock(&scp->mtx);
		return (0);
	}

	if (note == 255) {
		/* Change the volume. */
		opl_setvoicevolume(scp, voice, volume, scp->voc[voice].volume);
		mtx_unlock(&scp->mtx);
		return (0);
	}

	/* Kill the previous note. */
	opl_command(scp, map->ch, KSL_LEVEL + map->op[1], 0xff); /* Carrier volume */
	opl_command(scp, map->ch, KSL_LEVEL + map->op[0], 0xff); /* Modulator volume */
	if (map->voice_mode == VOICE_4OP) {
		opl_command(scp, map->ch, KSL_LEVEL + map->op[3], 0xff); /* Carrier volume */
		opl_command(scp, map->ch, KSL_LEVEL + map->op[2], 0xff); /* Modulator volume */
	}
	opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num, 0); /* Note off. */

	instr = scp->act_i[voice];
	if (instr == NULL)
		instr = &scp->i_map[0];
	if (instr->channel < 0) {
		mtx_unlock(&scp->mtx);
		printf("opl_startnote: the instrument for voice %d is undefined.\n", voice);
		return (0);
	}
	if (map->voice_mode == VOICE_2OP && instr->key == OPL3_PATCH) {
		mtx_unlock(&scp->mtx);
		printf("opl_startnote: the voice mode %d mismatches the key 0x%x.\n", map->voice_mode, instr->key);
		return (0);
	}

	voice_mode = map->voice_mode;
	if (voice_mode == VOICE_4OP) {
		if (map->ch == USE_LEFT)
			voice_shift = 0;
		else
			voice_shift = 3;
		voice_shift += map->voice_num;
		if (instr->key != OPL3_PATCH) {
			voice_mode = VOICE_2OP;
			scp->cmask &= ~(1 << voice_shift);
		} else
			scp->cmask |= 1 << voice_shift;

		opl_command(scp, USE_RIGHT, CONNECTION_SELECT_REGISTER, scp->cmask);
	}

	/* Set the sound characteristics, attack, decay, sustain, release, wave select, feedback, connection. */
	opl_command(scp, map->ch, AM_VIB + map->op[0], instr->operators[0]); /* Sound characteristics. */
	opl_command(scp, map->ch, AM_VIB + map->op[1], instr->operators[1]);
	opl_command(scp, map->ch, ATTACK_DECAY + map->op[0], instr->operators[4]); /* Attack and decay. */
	opl_command(scp, map->ch, ATTACK_DECAY + map->op[1], instr->operators[5]);
	opl_command(scp, map->ch, SUSTAIN_RELEASE + map->op[0], instr->operators[6]); /* Sustain and release. */
	opl_command(scp, map->ch, SUSTAIN_RELEASE + map->op[1], instr->operators[7]);
	opl_command(scp, map->ch, WAVE_SELECT + map->op[0], instr->operators[8]); /* Wave select. */
	opl_command(scp, map->ch, WAVE_SELECT + map->op[1], instr->operators[9]);
	fpc = instr->operators[10];
	if ((fpc & 0x30) == 0)
		fpc |= 0x30; /* So that at least one channel is enabled. */
	opl_command(scp, map->ch, FEEDBACK_CONNECTION + map->voice_num, fpc); /* Feedback and connection. */

	if (voice_mode == VOICE_4OP) {
		/* Do not forget the operators 3 and 4. */
		opl_command(scp, map->ch, AM_VIB + map->op[2], instr->operators[OFFS_4OP + 0]); /* Sound characteristics. */
		opl_command(scp, map->ch, AM_VIB + map->op[3], instr->operators[OFFS_4OP + 1]);
		opl_command(scp, map->ch, ATTACK_DECAY + map->op[2], instr->operators[OFFS_4OP + 4]); /* Attack and decay. */
		opl_command(scp, map->ch, ATTACK_DECAY + map->op[3], instr->operators[OFFS_4OP + 5]);
		opl_command(scp, map->ch, SUSTAIN_RELEASE + map->op[2], instr->operators[OFFS_4OP + 6]); /* Sustain and release. */
		opl_command(scp, map->ch, SUSTAIN_RELEASE + map->op[3], instr->operators[OFFS_4OP + 7]);
		opl_command(scp, map->ch, WAVE_SELECT + map->op[2], instr->operators[OFFS_4OP + 8]); /* Wave select. */
		opl_command(scp, map->ch, WAVE_SELECT + map->op[3], instr->operators[OFFS_4OP + 9]);
		fpc = instr->operators[OFFS_4OP + 10];
		if ((fpc & 0x30) == 0)
			fpc |= 0x30; /* So that at least one channel is enabled. */
		opl_command(scp, map->ch, FEEDBACK_CONNECTION + map->voice_num + 3, fpc); /* Feedback and connection. */
	}
	scp->voc[voice].mode = voice_mode;

	opl_setvoicevolume(scp, voice, volume, scp->voc[voice].volume);

	/* Calcurate the frequency. */
	scp->voc[voice].orig_freq = opl_notetofreq(note) / 1000;
	/* Tune for the pitch bend. */
	freq = scp->voc[voice].current_freq = opl_computefinetune(scp->voc[voice].orig_freq, scp->voc[voice].bender, scp->voc[voice].bender_range);
	opl_freqtofnum(freq, &block, &fnum);

	/* Now we can play the note. */
	opl_command(scp, map->ch, FNUM_LOW + map->voice_num, fnum & 0xff);
	scp->voc[voice].keyon_byte = 0x20 | ((block & 0x07) << 2) | ((fnum >> 8) & 0x03);
	opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num, scp->voc[voice].keyon_byte);
	if (voice_mode == VOICE_4OP)
		opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num + 3, scp->voc[voice].keyon_byte);

	mtx_unlock(&scp->mtx);

	return (0);
}

static int
opl_reset(mididev_info *md)
{
	int unit, i;
	sc_p scp;
	struct physical_voice_info *map;

	scp = md->softc;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: resetting.\n", unit));

	mtx_lock(&md->synth.vc_mtx);
	mtx_lock(&scp->mtx);

	for (i = 0 ; i < MAX_VOICE ; i++)
		scp->lv_map[i] = i;

	for (i = 0 ; i < md->synth.alloc.max_voice ; i++) {
		opl_command(scp, scp->pv_map[scp->lv_map[i]].ch, KSL_LEVEL + scp->pv_map[scp->lv_map[i]].op[0], 0xff);
		opl_command(scp, scp->pv_map[scp->lv_map[i]].ch, KSL_LEVEL + scp->pv_map[scp->lv_map[i]].op[1], 0xff);
		if (scp->pv_map[scp->lv_map[i]].voice_mode == VOICE_4OP) {
			opl_command(scp, scp->pv_map[scp->lv_map[i]].ch, KSL_LEVEL + scp->pv_map[scp->lv_map[i]].op[2], 0xff);
			opl_command(scp, scp->pv_map[scp->lv_map[i]].ch, KSL_LEVEL + scp->pv_map[scp->lv_map[i]].op[3], 0xff);
		}
		/*
		 * opl_killnote(md, i, 0, 64) inline-expanded to avoid
		 * unlocking and relocking mutex unnecessarily.
		 */
		md->synth.alloc.map[i] = 0;
		map = &scp->pv_map[scp->lv_map[i]];

		if (map->voice_mode != VOICE_NONE) {
			opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num, scp->voc[i].keyon_byte & ~0x20);
			
			scp->voc[i].keyon_byte = 0;
			scp->voc[i].bender = 0;
			scp->voc[i].volume = 64;
			scp->voc[i].bender_range = 200;
			scp->voc[i].orig_freq = 0;
			scp->voc[i].current_freq = 0;
			scp->voc[i].mode = 0;
		}
	}

	if (scp->model >= MODEL_OPL3) {
		md->synth.alloc.max_voice = 18;
		for (i = 0 ; i < MAX_VOICE ; i++)
			scp->pv_map[i].voice_mode = VOICE_2OP;
	}

	mtx_unlock(&md->synth.vc_mtx);
	mtx_unlock(&scp->mtx);

	return (0);
}

static int
opl_hwcontrol(mididev_info *md, u_char *event)
{
	/* NOP. */
	return (0);
}

static int
opl_loadpatch(mididev_info *md, int format, struct uio *buf, int offs, int count, int pmgr_flag)
{
	int unit;
	struct sbi_instrument ins;
	sc_p scp;

	scp = md->softc;
	unit = md->unit;

	if (count < sizeof(ins)) {
		printf("opl_loadpatch: The patch record is too short.\n");
		return (EINVAL);
	}
	if (uiomove(&((char *)&ins)[offs], sizeof(ins) - offs, buf) != 0)
		printf("opl_loadpatch: User memory mangled?\n");
	if (ins.channel < 0 || ins.channel >= SBFM_MAXINSTR) {
		printf("opl_loadpatch: Instrument number %d is not valid.\n", ins.channel);
		return (EINVAL);
	}
	ins.key = format;

	opl_storeinstr(scp, ins.channel, &ins);
	return (0);
}

static int
opl_panning(mididev_info *md, int chn, int pan)
{
	/* NOP. */
	return (0);
}

#define SET_VIBRATO(cell) do { \
	int tmp; \
	tmp = instr->operators[(cell-1)+(((cell-1)/2)*OFFS_4OP)]; \
	if (press > 110) \
		tmp |= 0x40;	/* Vibrato on */ \
	opl_command(scp, map->ch, AM_VIB + map->op[cell-1], tmp);} while(0);

static int
opl_aftertouch(mididev_info *md, int voice, int press)
{
	int unit, connection;
	struct sbi_instrument *instr;
	struct physical_voice_info *map;
	sc_p scp;

	scp = md->softc;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: setting the aftertouch, voice %d, press %d.\n", unit, voice, press));

	if (voice < 0 || voice >= md->synth.alloc.max_voice)
		return (0);

	mtx_lock(&scp->mtx);

	map = &scp->pv_map[scp->lv_map[voice]];
	if (map->voice_mode == VOICE_NONE) {
		mtx_unlock(&scp->mtx);
		return (0);
	}

	/* Adjust the vibrato. */
	instr = scp->act_i[voice];
	if (instr == NULL)
		instr = &scp->i_map[0];

	if (scp->voc[voice].mode == VOICE_4OP) {
		connection = ((instr->operators[10] & 0x01) << 1) | (instr->operators[10 + OFFS_4OP] & 0x01);
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
	} else {
		SET_VIBRATO(1);
		if ((instr->operators[10] & 0x01))
			SET_VIBRATO(2);
	}

	mtx_unlock(&scp->mtx);

	return (0);
}

static int
opl_bendpitch(sc_p scp, int voice, int value)
{
	int unit, block, fnum, freq;
	struct physical_voice_info *map;
	mididev_info *md;

	md = scp->devinfo;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: setting the pitch bend, voice %d, value %d.\n", unit, voice, value));

	mtx_lock(&scp->mtx);

	map = &scp->pv_map[scp->lv_map[voice]];
	if (map->voice_mode == 0) {
		mtx_unlock(&scp->mtx);
		return (0);
	}
	scp->voc[voice].bender = value;
	if (value == 0 || (scp->voc[voice].keyon_byte & 0x20) == 0) {
		mtx_unlock(&scp->mtx);
		return (0);
	}

	freq = opl_computefinetune(scp->voc[voice].orig_freq, scp->voc[voice].bender, scp->voc[voice].bender_range);
	scp->voc[voice].current_freq = freq;

	opl_freqtofnum(freq, &block, &fnum);

	opl_command(scp, map->ch, FNUM_LOW + map->voice_num, fnum & 0xff);
	scp->voc[voice].keyon_byte = 0x20 | ((block & 0x07) << 2) | ((fnum >> 8) & 0x03);
	opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num, scp->voc[voice].keyon_byte);
	if (map->voice_mode == VOICE_4OP)
		opl_command(scp, map->ch, KEYON_BLOCK + map->voice_num + 3, scp->voc[voice].keyon_byte);

	mtx_unlock(&scp->mtx);

	return (0);
}

static int
opl_controller(mididev_info *md, int voice, int ctrlnum, int val)
{
	int unit;
	sc_p scp;

	scp = md->softc;
	unit = md->unit;

	MIDI_DEBUG(printf("opl%d: setting the controller, voice %d, ctrlnum %d, val %d.\n", unit, voice, ctrlnum, val));

	if (voice < 0 || voice >= md->synth.alloc.max_voice)
		return (0);

	switch (ctrlnum) {
	case CTRL_PITCH_BENDER:
		opl_bendpitch(scp, voice, val);
		break;
	case CTRL_PITCH_BENDER_RANGE:
		mtx_lock(&scp->mtx);
		scp->voc[voice].bender_range = val;
		mtx_unlock(&scp->mtx);
		break;
	case CTRL_MAIN_VOLUME:
		mtx_lock(&scp->mtx);
		scp->voc[voice].volume = val / 128;
		mtx_unlock(&scp->mtx);
		break;
	}

	return (0);
}

static int
opl_patchmgr(mididev_info *md, struct patmgr_info *rec)
{
	return (EINVAL);
}

static int
opl_bender(mididev_info *md, int voice, int val)
{
	sc_p scp;

	scp = md->softc;

	if (voice < 0 || voice >= md->synth.alloc.max_voice)
		return (0);

	return opl_bendpitch(scp, voice, val - 8192);
}

static int
opl_allocvoice(mididev_info *md, int chn, int note, struct voice_alloc_info *alloc)
{
	int i, p, best, first, avail, best_time, is4op, instr_no;
	struct sbi_instrument *instr;
	sc_p scp;

	scp = md->softc;

	MIDI_DEBUG(printf("opl%d: allocating a voice, chn %d, note %d.\n", md->unit, chn, note));

	best_time = 0x7fffffff;

	mtx_lock(&md->synth.vc_mtx);

	if (chn < 0 || chn >= 15)
		instr_no = 0;
	else
		instr_no = md->synth.chn_info[chn].pgm_num;

	mtx_lock(&scp->mtx);

	instr = &scp->i_map[instr_no];
	if (instr->channel < 0 || md->synth.alloc.max_voice != 12)
		is4op = 0;
	else if (md->synth.alloc.max_voice == 12) {
		if (instr->key == OPL3_PATCH)
			is4op = 1;
		else
			is4op = 0;
	} else
		is4op = 0;

	if (is4op) {
		first = p = 0;
		avail = 6;
	} else {
		if (md->synth.alloc.max_voice == 12)
			first = p = 6;
		else
			first = p = 0;
		avail = md->synth.alloc.max_voice;
	}

	/* Look up a free voice. */
	best = first;

	for (i = 0 ; i < avail ; i++) {
		if (alloc->map[p] == 0)
			return (p);
	}
	if (alloc->alloc_times[p] < best_time) {
		best_time = alloc->alloc_times[p];
		best = p;
	}
	p = (p + 1) % avail;

	if (best < 0)
		best = 0;
	else if (best > md->synth.alloc.max_voice)
		best -= md->synth.alloc.max_voice;

	mtx_unlock(&scp->mtx);
	mtx_unlock(&md->synth.vc_mtx);

	return best;
}

static int
opl_setupvoice(mididev_info *md, int voice, int chn)
{
	struct channel_info *info;
	sc_p scp;

	scp = md->softc;

	MIDI_DEBUG(printf("opl%d: setting up a voice, voice %d, chn %d.\n", md->unit, voice, chn));

	mtx_lock(&md->synth.vc_mtx);

	info = &md->synth.chn_info[chn];

	opl_setinstr(md, voice, info->pgm_num);
	mtx_lock(&scp->mtx);
	scp->voc[voice].bender = info->bender_value;
	scp->voc[voice].volume = info->controllers[CTL_MAIN_VOLUME];
	mtx_unlock(&scp->mtx);

	mtx_lock(&md->synth.vc_mtx);

	return (0);
}

static int
opl_sendsysex(mididev_info *md, u_char *sysex, int len)
{
	/* NOP. */
	return (0);
}

static int
opl_prefixcmd(mididev_info *md, int status)
{
	/* NOP. */
	return (0);
}

static int
opl_volumemethod(mididev_info *md, int mode)
{
	/* NOP. */
	return (0);
}

/*
 * The functions below here are the libraries for the above ones.
 */

/* Writes a command to the OPL chip. */
static void
opl_command(sc_p scp, int ch, int addr, u_int val)
{
	int model;

	MIDI_DEBUG(printf("opl%d: sending a command, addr 0x%x, val 0x%x.\n", scp->devinfo->unit, addr, val));

	model = scp->model;

	/* Write the addr first. */
	bus_space_write_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), ch * 2, (u_char)(addr & 0xff));
	if (model < MODEL_OPL3)
		DELAY(10);
	else {
		bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), ch * 2);
		bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), ch * 2);
	}

	/* Next write the value. */
	bus_space_write_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), ch * 2 + 1, (u_char)(val & 0xff));
	if (model < MODEL_OPL3)
		DELAY(30);
	else {
		bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), ch * 2);
		bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), ch * 2);
	}
}

/* Reads the status of the OPL chip. */
static int
opl_status(sc_p scp)
{
	MIDI_DEBUG(printf("opl%d: reading the status.\n", scp->devinfo->unit));

	return bus_space_read_1(rman_get_bustag(scp->io), rman_get_bushandle(scp->io), 0);
}

static void
opl_enter4opmode(sc_p scp)
{
	int i;
	mididev_info *devinfo;
	static int v4op[MAX_VOICE] = {
		0, 1, 2, 9, 10, 11, 6, 7, 8, 15, 16, 17,
	};

	devinfo = scp->devinfo;

	MIDI_DEBUG(printf("opl%d: entering 4 OP mode.\n", devinfo->unit));

	/* Connect all possible 4 OP voice operators. */
	mtx_lock(&devinfo->synth.vc_mtx);
	mtx_lock(&scp->mtx);
	scp->cmask = 0x3f;
	opl_command(scp, USE_RIGHT, CONNECTION_SELECT_REGISTER, scp->cmask);

	for (i = 0 ; i < 3 ; i++)
		scp->pv_map[i].voice_mode = VOICE_4OP;
	for (i = 3 ; i < 6 ; i++)
		scp->pv_map[i].voice_mode = VOICE_NONE;
	for (i = 9 ; i < 12 ; i++)
		scp->pv_map[i].voice_mode = VOICE_4OP;
	for (i = 12 ; i < 15 ; i++)
		scp->pv_map[i].voice_mode = VOICE_NONE;

	for (i = 0 ; i < 12 ; i++)
		scp->lv_map[i] = v4op[i];
	mtx_unlock(&scp->mtx);
	devinfo->synth.alloc.max_voice = 12;
	mtx_unlock(&devinfo->synth.vc_mtx);
}

static void
opl_storeinstr(sc_p scp, int instr_no, struct sbi_instrument *instr)
{
	if (instr->key != FM_PATCH && (instr->key != OPL3_PATCH || scp->model < MODEL_OPL3))
		printf("opl_storeinstr: The patch format field 0x%x is not valid.\n", instr->key);

	bcopy(instr, &scp->i_map[instr_no], sizeof(*instr));
}

static void
opl_calcvol(u_char *regbyte, int volume, int main_vol)
{
	int level;

	level = (~*regbyte & 0x3f);

	if (main_vol > 127)
		main_vol = 127;

	volume = (volume * main_vol) / 127;

	if (level > 0)
		level += opl_volumetable[volume];

	RANGE(level, 0, 0x3f);

	*regbyte = (*regbyte & 0xc0) | (~level & 0x3f);
}

static void
opl_setvoicevolume(sc_p scp, int voice, int volume, int main_vol)
{
	u_char vol1, vol2, vol3, vol4;
	int connection;
	struct sbi_instrument *instr;
	struct physical_voice_info *map;
	mididev_info *devinfo;

	devinfo = scp->devinfo;

	if (voice < 0 || voice >= devinfo->synth.alloc.max_voice)
		return;

	map = &scp->pv_map[scp->lv_map[voice]];
	instr = scp->act_i[voice];
	if (instr == NULL)
		instr = &scp->i_map[0];

	if (instr->channel < 0)
		return;
	if (scp->voc[voice].mode == VOICE_NONE)
		return;
	if (scp->voc[voice].mode == VOICE_2OP) { /* 2 OP mode. */
		vol1 = instr->operators[2];
		vol2 = instr->operators[3];
		if ((instr->operators[10] & 0x01))
			opl_calcvol(&vol1, volume, main_vol);
		opl_calcvol(&vol2, volume, main_vol);
		opl_command(scp, map->ch, KSL_LEVEL + map->op[0], vol1);
		opl_command(scp, map->ch, KSL_LEVEL + map->op[1], vol2);
	} else { /* 4 OP mode. */
		vol1 = instr->operators[2];
		vol2 = instr->operators[3];
		vol3 = instr->operators[OFFS_4OP + 2];
		vol4 = instr->operators[OFFS_4OP + 3];
		connection = ((instr->operators[10] & 0x01) << 1) | (instr->operators[10 + OFFS_4OP] & 0x01);
		switch(connection) {
		case 0:
			opl_calcvol(&vol4, volume, main_vol);
			break;
		case 1:
			opl_calcvol(&vol2, volume, main_vol);
			opl_calcvol(&vol4, volume, main_vol);
			break;
		case 2:
			opl_calcvol(&vol1, volume, main_vol);
			opl_calcvol(&vol4, volume, main_vol);
			break;
		case 3:
			opl_calcvol(&vol1, volume, main_vol);
			opl_calcvol(&vol3, volume, main_vol);
			opl_calcvol(&vol4, volume, main_vol);
			break;
		}
		opl_command(scp, map->ch, KSL_LEVEL + map->op[0], vol1);
		opl_command(scp, map->ch, KSL_LEVEL + map->op[1], vol2);
		opl_command(scp, map->ch, KSL_LEVEL + map->op[2], vol3);
		opl_command(scp, map->ch, KSL_LEVEL + map->op[3], vol4);
	}
}

static void
opl_freqtofnum(int freq, int *block, int *fnum)
{
	int f, octave;

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

static int notes[] =
{
	261632,
	277189,
	293671,
	311132,
	329632,
	349232,
	369998,
	391998,
	415306,
	440000,
	466162,
	493880
};

#define BASE_OCTAVE 5

static int
opl_notetofreq(int note_num)
{
	int note, octave, note_freq;

	octave = note_num / 12;
	note = note_num % 12;

	note_freq = notes[note];

	if (octave < BASE_OCTAVE)
		note_freq >>= (BASE_OCTAVE - octave);
	else if (octave > BASE_OCTAVE)
		note_freq <<= (octave - BASE_OCTAVE);

	return (note_freq);
}

static u_long
opl_computefinetune(u_long base_freq, int bend, int range)
{
	u_long amount;
	int negative, semitones, cents, multiplier;

	if (bend == 0 || range == 0 || base_freq == 0)
		return (base_freq);

	multiplier = 1;

	if (range > 8192)
		range = 8192;

	bend = bend * range / 8192;
	if (bend == 0)
		return (base_freq);

	if (bend < 0) {
		negative = 1;
		bend = -bend;
	}
	else
		negative = 0;
	if (bend > range)
		bend = range;

	while (bend > 2399) {
		multiplier *= 4;
		bend -= 2400;
	}

	semitones = bend / 100;
	cents = bend % 100;

	amount = (u_long)(semitone_tuning[semitones] * multiplier * cent_tuning[cents]) / 10000;

	if (negative)
		return (base_freq * 10000) / amount;
	else
		return (base_freq * amount) / 10000;
}

/* Allocates resources other than IO ports. */
static int
opl_allocres(sc_p scp, device_t dev)
{
	if (scp->io == NULL) {
		scp->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid, 0, ~0, 4, RF_ACTIVE);
		if (scp->io == NULL)
			return (1);
	}

	return (0);
}

/* Releases resources. */
static void
opl_releaseres(sc_p scp, device_t dev)
{
	if (scp->io != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, scp->io_rid, scp->io);
		scp->io = NULL;
	}
}

static device_method_t opl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , opl_probe ),
	DEVMETHOD(device_attach, opl_attach),

	{ 0, 0 },
};

static driver_t opl_driver = {
	"midi",
	opl_methods,
	sizeof(struct opl_softc),
};

DRIVER_MODULE(opl, isa, opl_driver, midi_devclass, 0, 0);

static device_method_t oplsbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , oplsbc_probe ),
	DEVMETHOD(device_attach, oplsbc_attach),

	{ 0, 0 },
};

static driver_t oplsbc_driver = {
	"midi",
	oplsbc_methods,
	sizeof(struct opl_softc),
};

DRIVER_MODULE(oplsbc, sbc, oplsbc_driver, midi_devclass, 0, 0);
