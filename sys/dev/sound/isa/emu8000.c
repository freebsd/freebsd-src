/*
 * Low level EMU8000 chip driver for FreeBSD. This handles io against
 * /dev/midi, the midi {in, out}put event queues and the event/message
 * operation to the EMU8000 chip.
 * 
 * (C) 1999 Seigo Tanimura
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
 * $FreeBSD$
 * 
 */

#include <dev/sound/midi/midi.h>

#include <isa/isavar.h>

static devclass_t midi_devclass;

#ifndef DDB
#undef DDB
#define DDB(x)
#endif /* DDB */

/* These are the specs of EMU8000. */
#define EMU8K_MAXVOICE 32
#define EMU8K_MAXINFO 256

#define EMU8K_IDX_DATA0  0
#define EMU8K_IDX_DATA1  1
#define EMU8K_IDX_DATA2  1
#define EMU8K_IDX_DATA3  2
#define EMU8K_IDX_PTR    2

#define EMU8K_PORT_DATA0 0
#define EMU8K_PORT_DATA1 0
#define EMU8K_PORT_DATA2 2
#define EMU8K_PORT_DATA3 0
#define EMU8K_PORT_PTR   2

#define EMU8K_DRAM_RAM   0x200000
#define EMU8K_DRAM_MAX   0xffffe0

/* And some convinient macros. */
#define EMU8K_DMA_LEFT  0x00
#define EMU8K_DMA_RIGHT 0x01
#define EMU8K_DMA_LR    0x01
#define EMU8K_DMA_READ  0x00
#define EMU8K_DMA_WRITE 0x02
#define EMU8K_DMA_RW    0x02
#define EMU8K_DMA_MASK  0x03

/* The followings are the init array for EMU8000, originally in ADIP. */

/* Set 1 */
static u_short init1_1[32] =
{
	0x03ff, 0x0030, 0x07ff, 0x0130, 0x0bff, 0x0230, 0x0fff, 0x0330,
	0x13ff, 0x0430, 0x17ff, 0x0530, 0x1bff, 0x0630, 0x1fff, 0x0730,
	0x23ff, 0x0830, 0x27ff, 0x0930, 0x2bff, 0x0a30, 0x2fff, 0x0b30,
	0x33ff, 0x0c30, 0x37ff, 0x0d30, 0x3bff, 0x0e30, 0x3fff, 0x0f30,
};

static u_short init1_2[32] =
{
	0x43ff, 0x0030, 0x47ff, 0x0130, 0x4bff, 0x0230, 0x4fff, 0x0330,
	0x53ff, 0x0430, 0x57ff, 0x0530, 0x5bff, 0x0630, 0x5fff, 0x0730,
	0x63ff, 0x0830, 0x67ff, 0x0930, 0x6bff, 0x0a30, 0x6fff, 0x0b30,
	0x73ff, 0x0c30, 0x77ff, 0x0d30, 0x7bff, 0x0e30, 0x7fff, 0x0f30,
};

static u_short init1_3[32] =
{
	0x83ff, 0x0030, 0x87ff, 0x0130, 0x8bff, 0x0230, 0x8fff, 0x0330,
	0x93ff, 0x0430, 0x97ff, 0x0530, 0x9bff, 0x0630, 0x9fff, 0x0730,
	0xa3ff, 0x0830, 0xa7ff, 0x0930, 0xabff, 0x0a30, 0xafff, 0x0b30,
	0xb3ff, 0x0c30, 0xb7ff, 0x0d30, 0xbbff, 0x0e30, 0xbfff, 0x0f30,
};

static u_short init1_4[32] =
{
	0xc3ff, 0x0030, 0xc7ff, 0x0130, 0xcbff, 0x0230, 0xcfff, 0x0330,
	0xd3ff, 0x0430, 0xd7ff, 0x0530, 0xdbff, 0x0630, 0xdfff, 0x0730,
	0xe3ff, 0x0830, 0xe7ff, 0x0930, 0xebff, 0x0a30, 0xefff, 0x0b30,
	0xf3ff, 0x0c30, 0xf7ff, 0x0d30, 0xfbff, 0x0e30, 0xffff, 0x0f30,
};

/* Set 2 */

static u_short init2_1[32] =
{
	0x03ff, 0x8030, 0x07ff, 0x8130, 0x0bff, 0x8230, 0x0fff, 0x8330,
	0x13ff, 0x8430, 0x17ff, 0x8530, 0x1bff, 0x8630, 0x1fff, 0x8730,
	0x23ff, 0x8830, 0x27ff, 0x8930, 0x2bff, 0x8a30, 0x2fff, 0x8b30,
	0x33ff, 0x8c30, 0x37ff, 0x8d30, 0x3bff, 0x8e30, 0x3fff, 0x8f30,
};

static u_short init2_2[32] =
{
	0x43ff, 0x8030, 0x47ff, 0x8130, 0x4bff, 0x8230, 0x4fff, 0x8330,
	0x53ff, 0x8430, 0x57ff, 0x8530, 0x5bff, 0x8630, 0x5fff, 0x8730,
	0x63ff, 0x8830, 0x67ff, 0x8930, 0x6bff, 0x8a30, 0x6fff, 0x8b30,
	0x73ff, 0x8c30, 0x77ff, 0x8d30, 0x7bff, 0x8e30, 0x7fff, 0x8f30,
};

static u_short init2_3[32] =
{
	0x83ff, 0x8030, 0x87ff, 0x8130, 0x8bff, 0x8230, 0x8fff, 0x8330,
	0x93ff, 0x8430, 0x97ff, 0x8530, 0x9bff, 0x8630, 0x9fff, 0x8730,
	0xa3ff, 0x8830, 0xa7ff, 0x8930, 0xabff, 0x8a30, 0xafff, 0x8b30,
	0xb3ff, 0x8c30, 0xb7ff, 0x8d30, 0xbbff, 0x8e30, 0xbfff, 0x8f30,
};

static u_short init2_4[32] =
{
	0xc3ff, 0x8030, 0xc7ff, 0x8130, 0xcbff, 0x8230, 0xcfff, 0x8330,
	0xd3ff, 0x8430, 0xd7ff, 0x8530, 0xdbff, 0x8630, 0xdfff, 0x8730,
	0xe3ff, 0x8830, 0xe7ff, 0x8930, 0xebff, 0x8a30, 0xefff, 0x8b30,
	0xf3ff, 0x8c30, 0xf7ff, 0x8d30, 0xfbff, 0x8e30, 0xffff, 0x8f30,
};

/* Set 3 */

static u_short init3_1[32] =
{
	0x0C10, 0x8470, 0x14FE, 0xB488, 0x167F, 0xA470, 0x18E7, 0x84B5,
	0x1B6E, 0x842A, 0x1F1D, 0x852A, 0x0DA3, 0x8F7C, 0x167E, 0xF254,
	0x0000, 0x842A, 0x0001, 0x852A, 0x18E6, 0x8BAA, 0x1B6D, 0xF234,
	0x229F, 0x8429, 0x2746, 0x8529, 0x1F1C, 0x86E7, 0x229E, 0xF224,
};

static u_short init3_2[32] =
{
	0x0DA4, 0x8429, 0x2C29, 0x8529, 0x2745, 0x87F6, 0x2C28, 0xF254,
	0x383B, 0x8428, 0x320F, 0x8528, 0x320E, 0x8F02, 0x1341, 0xF264,
	0x3EB6, 0x8428, 0x3EB9, 0x8528, 0x383A, 0x8FA9, 0x3EB5, 0xF294,
	0x3EB7, 0x8474, 0x3EBA, 0x8575, 0x3EB8, 0xC4C3, 0x3EBB, 0xC5C3,
};

static u_short init3_3[32] =
{
	0x0000, 0xA404, 0x0001, 0xA504, 0x141F, 0x8671, 0x14FD, 0x8287,
	0x3EBC, 0xE610, 0x3EC8, 0x8C7B, 0x031A, 0x87E6, 0x3EC8, 0x86F7,
	0x3EC0, 0x821E, 0x3EBE, 0xD208, 0x3EBD, 0x821F, 0x3ECA, 0x8386,
	0x3EC1, 0x8C03, 0x3EC9, 0x831E, 0x3ECA, 0x8C4C, 0x3EBF, 0x8C55,
};

static u_short init3_4[32] =
{
	0x3EC9, 0xC208, 0x3EC4, 0xBC84, 0x3EC8, 0x8EAD, 0x3EC8, 0xD308,
	0x3EC2, 0x8F7E, 0x3ECB, 0x8219, 0x3ECB, 0xD26E, 0x3EC5, 0x831F,
	0x3EC6, 0xC308, 0x3EC3, 0xB2FF, 0x3EC9, 0x8265, 0x3EC9, 0x8319,
	0x1342, 0xD36E, 0x3EC7, 0xB3FF, 0x0000, 0x8365, 0x1420, 0x9570,
};

/* Set 4 */

static u_short init4_1[32] =
{
	0x0C10, 0x8470, 0x14FE, 0xB488, 0x167F, 0xA470, 0x18E7, 0x84B5,
	0x1B6E, 0x842A, 0x1F1D, 0x852A, 0x0DA3, 0x0F7C, 0x167E, 0x7254,
	0x0000, 0x842A, 0x0001, 0x852A, 0x18E6, 0x0BAA, 0x1B6D, 0x7234,
	0x229F, 0x8429, 0x2746, 0x8529, 0x1F1C, 0x06E7, 0x229E, 0x7224,
};

static u_short init4_2[32] =
{
	0x0DA4, 0x8429, 0x2C29, 0x8529, 0x2745, 0x07F6, 0x2C28, 0x7254,
	0x383B, 0x8428, 0x320F, 0x8528, 0x320E, 0x0F02, 0x1341, 0x7264,
	0x3EB6, 0x8428, 0x3EB9, 0x8528, 0x383A, 0x0FA9, 0x3EB5, 0x7294,
	0x3EB7, 0x8474, 0x3EBA, 0x8575, 0x3EB8, 0x44C3, 0x3EBB, 0x45C3,
};

static u_short init4_3[32] =
{
	0x0000, 0xA404, 0x0001, 0xA504, 0x141F, 0x0671, 0x14FD, 0x0287,
	0x3EBC, 0xE610, 0x3EC8, 0x0C7B, 0x031A, 0x07E6, 0x3EC8, 0x86F7,
	0x3EC0, 0x821E, 0x3EBE, 0xD208, 0x3EBD, 0x021F, 0x3ECA, 0x0386,
	0x3EC1, 0x0C03, 0x3EC9, 0x031E, 0x3ECA, 0x8C4C, 0x3EBF, 0x0C55,
};

static u_short init4_4[32] =
{
	0x3EC9, 0xC208, 0x3EC4, 0xBC84, 0x3EC8, 0x0EAD, 0x3EC8, 0xD308,
	0x3EC2, 0x8F7E, 0x3ECB, 0x0219, 0x3ECB, 0xD26E, 0x3EC5, 0x031F,
	0x3EC6, 0xC308, 0x3EC3, 0x32FF, 0x3EC9, 0x0265, 0x3EC9, 0x8319,
	0x1342, 0xD36E, 0x3EC7, 0x33FF, 0x0000, 0x8365, 0x1420, 0x9570,
};

/* The followings are the register, the channel and the port for the EMU8000 registers. */
struct _emu_register {
	int reg; /* Register */
	int index; /* Index */
	int port; /* Port */
	int chn; /* Channel */
	int size; /* Size, 0 == word, 1 == double word */
};

#define EMU8K_CHN_ANY (-1)

static struct _emu_register emu_regs[] =
{
	/* Reg,           Index,             Port,       Channel, Size */
	{    0, EMU8K_IDX_DATA0, EMU8K_PORT_DATA0, EMU8K_CHN_ANY,    1}, /* CPF */
	{    1, EMU8K_IDX_DATA0, EMU8K_PORT_DATA0, EMU8K_CHN_ANY,    1}, /* PTRX */
	{    2, EMU8K_IDX_DATA0, EMU8K_PORT_DATA0, EMU8K_CHN_ANY,    1}, /* CVCF */
	{    3, EMU8K_IDX_DATA0, EMU8K_PORT_DATA0, EMU8K_CHN_ANY,    1}, /* VTFT */
	{    6, EMU8K_IDX_DATA0, EMU8K_PORT_DATA0, EMU8K_CHN_ANY,    1}, /* PSST */
	{    7, EMU8K_IDX_DATA0, EMU8K_PORT_DATA0, EMU8K_CHN_ANY,    1}, /* CSL */
	{    0, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    1}, /* CCCA */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,             9,    1}, /* HWCF4 */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            10,    1}, /* HWCF5 */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            13,    1}, /* HWCF6 */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            20,    1}, /* SMALR */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            21,    1}, /* SMARR */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            22,    1}, /* SMALW */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            23,    1}, /* SMARW */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            26,    0}, /* SMLD */
	{    1, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2,            26,    0}, /* SMRD */
	{    1, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2,            27,    0}, /* WC */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            29,    0}, /* HWCF1 */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            30,    0}, /* HWCF2 */
	{    1, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1,            31,    0}, /* HWCF3 */
	{    2, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    0}, /* INIT1 */
	{    2, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2, EMU8K_CHN_ANY,    0}, /* INIT2 */
	{    3, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    0}, /* INIT3 */
	{    3, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2, EMU8K_CHN_ANY,    0}, /* INIT4 */
	{    4, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    0}, /* ENVVOL */
	{    5, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    0}, /* DCYSUSV */
	{    6, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    0}, /* ENVVAL */
	{    7, EMU8K_IDX_DATA1, EMU8K_PORT_DATA1, EMU8K_CHN_ANY,    0}, /* DCYSUS */
	{    4, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2, EMU8K_CHN_ANY,    0}, /* ATKHLDV */
	{    5, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2, EMU8K_CHN_ANY,    0}, /* LFO1VAL */
	{    6, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2, EMU8K_CHN_ANY,    0}, /* ATKHLD */
	{    7, EMU8K_IDX_DATA2, EMU8K_PORT_DATA2, EMU8K_CHN_ANY,    0}, /* LFO2VAL */
	{    0, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3, EMU8K_CHN_ANY,    0}, /* IP */
	{    1, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3, EMU8K_CHN_ANY,    0}, /* IFATN */
	{    2, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3, EMU8K_CHN_ANY,    0}, /* PEFE */
	{    3, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3, EMU8K_CHN_ANY,    0}, /* FMMOD */
	{    4, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3, EMU8K_CHN_ANY,    0}, /* TREMFRQ */
	{    5, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3, EMU8K_CHN_ANY,    0}, /* FM2FRQ2 */
	{    7, EMU8K_IDX_DATA3, EMU8K_PORT_DATA3,             0,    0}, /* PROBE */
};

/* These are the EMU8000 register names. */
enum {
	EMU8K_CPF = 0,
	EMU8K_PTRX,
	EMU8K_CVCF,
	EMU8K_VTFT,
	EMU8K_PSST,
	EMU8K_CSL,
	EMU8K_CCCA,
	EMU8K_HWCF4,
	EMU8K_HWCF5,
	EMU8K_HWCF6,
	EMU8K_SMALR,
	EMU8K_SMARR,
	EMU8K_SMALW,
	EMU8K_SMARW,
	EMU8K_SMLD,
	EMU8K_SMRD,
	EMU8K_WC,
	EMU8K_HWCF1,
	EMU8K_HWCF2,
	EMU8K_HWCF3,
	EMU8K_INIT1,
	EMU8K_INIT2,
	EMU8K_INIT3,
	EMU8K_INIT4,
	EMU8K_ENVVOL,
	EMU8K_DCYSUSV,
	EMU8K_ENVVAL,
	EMU8K_DCYSUS,
	EMU8K_ATKHLDV,
	EMU8K_LFO1VAL,
	EMU8K_ATKHLD,
	EMU8K_LFO2VAL,
	EMU8K_IP,
	EMU8K_IFATN,
	EMU8K_PEFE,
	EMU8K_FMMOD,
	EMU8K_TREMFRQ,
	EMU8K_FM2FRQ2,
	EMU8K_PROBE,
	EMU8K_REGLAST, /* keep this! */
};
#define EMU8K_REGNUM (EMU8K_REGLAST)

/* These are the synthesizer and the midi device information. */
static struct synth_info emu_synthinfo = {
	"EMU8000 Wavetable Synth",
	0,
	SYNTH_TYPE_SAMPLE,
	SAMPLE_TYPE_AWE32,
	0,
	EMU8K_MAXVOICE,
	0,
	EMU8K_MAXINFO,
	0,
};

static struct midi_info emu_midiinfo = {
	"EMU8000 Wavetable Synth",
	0,
	0,
	0,
};

#if notyet
/*
 * These functions goes into emusynthdev_op_desc.
 */
static mdsy_killnote_t emu_killnote;
static mdsy_setinstr_t emu_setinstr;
static mdsy_startnote_t emu_startnote;
static mdsy_reset_t emu_reset;
static mdsy_hwcontrol_t emu_hwcontrol;
static mdsy_loadpatch_t emu_loadpatch;
static mdsy_panning_t emu_panning;
static mdsy_aftertouch_t emu_aftertouch;
static mdsy_controller_t emu_controller;
static mdsy_patchmgr_t emu_patchmgr;
static mdsy_bender_t emu_bender;
static mdsy_allocvoice_t emu_allocvoice;
static mdsy_setupvoice_t emu_setupvoice;
static mdsy_sendsysex_t emu_sendsysex;
static mdsy_prefixcmd_t emu_prefixcmd;
static mdsy_volumemethod_t emu_volumemethod;

/*
 * This is the synthdev_info for an EMU8000 chip.
 */
static synthdev_info emusynth_op_desc = {
	emu_killnote,
	emu_setinstr,
	emu_startnote,
	emu_reset,
	emu_hwcontrol,
	emu_loadpatch,
	emu_panning,
	emu_aftertouch,
	emu_controller,
	emu_patchmgr,
	emu_bender,
	emu_allocvoice,
	emu_setupvoice,
	emu_sendsysex,
	emu_prefixcmd,
	emu_volumemethod,
};
#endif /* notyet */

/*
 * These functions goes into emu_op_desc to get called
 * from sound.c.
 */

static int emu_probe(device_t dev);
static int emu_attach(device_t dev);
static int emupnp_attach(device_t dev) __unused;

static d_open_t emu_open;
static d_close_t emu_close;
static d_ioctl_t emu_ioctl;
static midi_callback_t emu_callback;

/* These go to mididev_info. */
static mdsy_readraw_t emu_readraw;
static mdsy_writeraw_t emu_writeraw;

/* Here is the parameter structure per a device. */
struct emu_softc {
	device_t dev; /* device information */
	mididev_info *devinfo; /* midi device information */

	struct mtx mtx; /* Mutex to protect a device */

	struct resource *io[3]; /* Base of io port */
	int io_rid[3]; /* Io resource ID */

	u_int dramsize; /* DRAM size */
	struct synth_info synthinfo; /* Synthesizer information */

	int fflags; /* File flags */
};

typedef struct emu_softc *sc_p;

/* These functions are local. */
static u_int emu_dramsize(sc_p scp);
static void emu_allocdmachn(sc_p scp, int chn, int mode);
static void emu_dmaaddress(sc_p scp, int mode, u_int addr);
static void emu_waitstream(sc_p scp, int mode);
static void emu_readblkstream(sc_p scp, int mode, u_short *data, size_t len);
static void emu_writeblkstream(sc_p scp, int mode, u_short *data, size_t len);
static void emu_readstream(sc_p scp, int mode, u_short *data);
static void emu_writestream(sc_p scp, int mode, u_short data);
static void emu_releasedmachn(sc_p scp, int chn, int mode);
static void emu_delay(sc_p scp, short n);
static void emu_readcpf(sc_p scp, int chn, u_int *cp, u_int *f) __unused;
static void emu_writecpf(sc_p scp, int chn, u_int cp, u_int f);
static void emu_readptrx(sc_p scp, int chn, u_int *pt, u_int *rs, u_int *auxd) __unused;
static void emu_writeptrx(sc_p scp, int chn, u_int pt, u_int rs, u_int auxd);
static void emu_readcvcf(sc_p scp, int chn, u_int *cv, u_int *cf) __unused;
static void emu_writecvcf(sc_p scp, int chn, u_int cv, u_int cf);
static void emu_readvtft(sc_p scp, int chn, u_int *vt, u_int *ft) __unused;
static void emu_writevtft(sc_p scp, int chn, u_int vt, u_int ft);
static void emu_readpsst(sc_p scp, int chn, u_int *pan, u_int *st) __unused;
static void emu_writepsst(sc_p scp, int chn, u_int pan, u_int st);
static void emu_readcsl(sc_p scp, int chn, u_int *cs, u_int *lp) __unused;
static void emu_writecsl(sc_p scp, int chn, u_int cs, u_int lp);
static void emu_readccca(sc_p scp, int chn, u_int *q, u_int *dma, u_int *wr, u_int *right, u_int *ca) __unused;
static void emu_writeccca(sc_p scp, int chn, u_int q, u_int dma, u_int wr, u_int right, u_int ca);
static void emu_readhwcf4(sc_p scp, u_int *val) __unused;
static void emu_writehwcf4(sc_p scp, u_int val);
static void emu_readhwcf5(sc_p scp, u_int *val) __unused;
static void emu_writehwcf5(sc_p scp, u_int val);
static void emu_readhwcf6(sc_p scp, u_int *val) __unused;
static void emu_writehwcf6(sc_p scp, u_int val);
static void emu_readsmalr(sc_p scp, u_int *mt, u_int *smalr);
static void emu_writesmalr(sc_p scp, u_int mt, u_int smalr);
static void emu_readsmarr(sc_p scp, u_int *mt, u_int *smarr);
static void emu_writesmarr(sc_p scp, u_int mt, u_int smarr);
static void emu_readsmalw(sc_p scp, u_int *full, u_int *smalw);
static void emu_writesmalw(sc_p scp, u_int full, u_int smalw);
static void emu_readsmarw(sc_p scp, u_int *full, u_int *smarw);
static void emu_writesmarw(sc_p scp, u_int full, u_int smarw);
static void emu_readsmld(sc_p scp, u_short *smld);
static void emu_writesmld(sc_p scp, u_short smld);
static void emu_readsmrd(sc_p scp, u_short *smrd);
static void emu_writesmrd(sc_p scp, u_short smrd);
static void emu_readwc(sc_p scp, u_int *wc);
static void emu_writewc(sc_p scp, u_int wc) __unused;
static void emu_readhwcf1(sc_p scp, u_int *val);
static void emu_writehwcf1(sc_p scp, u_int val);
static void emu_readhwcf2(sc_p scp, u_int *val);
static void emu_writehwcf2(sc_p scp, u_int val);
static void emu_readhwcf3(sc_p scp, u_int *val) __unused;
static void emu_writehwcf3(sc_p scp, u_int val);
static void emu_readinit1(sc_p scp, int chn, u_int *val) __unused;
static void emu_writeinit1(sc_p scp, int chn, u_int val);
static void emu_readinit2(sc_p scp, int chn, u_int *val) __unused;
static void emu_writeinit2(sc_p scp, int chn, u_int val);
static void emu_readinit3(sc_p scp, int chn, u_int *val) __unused;
static void emu_writeinit3(sc_p scp, int chn, u_int val);
static void emu_readinit4(sc_p scp, int chn, u_int *val) __unused;
static void emu_writeinit4(sc_p scp, int chn, u_int val);
static void emu_readenvvol(sc_p scp, int chn, u_int *envvol) __unused;
static void emu_writeenvvol(sc_p scp, int chn, u_int envvol);
static void emu_readdcysusv(sc_p scp, int chn, u_int *ph1v, u_int *susv, u_int *off, u_int *dcyv) __unused;
static void emu_writedcysusv(sc_p scp, int chn, u_int ph1v, u_int susv, u_int off, u_int dcyv);
static void emu_readenvval(sc_p scp, int chn, u_int *envval) __unused;
static void emu_writeenvval(sc_p scp, int chn, u_int envval);
static void emu_readdcysus(sc_p scp, int chn, u_int *ph1, u_int *sus, u_int *dcy) __unused;
static void emu_writedcysus(sc_p scp, int chn, u_int ph1, u_int sus, u_int dcy);
static void emu_readatkhldv(sc_p scp, int chn, u_int *atkhldv) __unused;
static void emu_writeatkhldv(sc_p scp, int chn, u_int atkhldv);
static void emu_readlfo1val(sc_p scp, int chn, u_int *lfo1val) __unused;
static void emu_writelfo1val(sc_p scp, int chn, u_int lfo1val);
static void emu_readatkhld(sc_p scp, int chn, u_int *atkhld) __unused;
static void emu_writeatkhld(sc_p scp, int chn, u_int atkhld);
static void emu_readlfo2val(sc_p scp, int chn, u_int *lfo2val) __unused;
static void emu_writelfo2val(sc_p scp, int chn, u_int lfo2val);
static void emu_readip(sc_p scp, int chn, u_int *ip) __unused;
static void emu_writeip(sc_p scp, int chn, u_int ip);
static void emu_readifatn(sc_p scp, int chn, u_int *ifc, u_int *atn) __unused;
static void emu_writeifatn(sc_p scp, int chn, u_int ifc, u_int atn);
static void emu_readpefe(sc_p scp, int chn, u_int *pe, u_int *fe) __unused;
static void emu_writepefe(sc_p scp, int chn, u_int pe, u_int fe);
static void emu_readfmmod(sc_p scp, int chn, u_int *fm, u_int *mod) __unused;
static void emu_writefmmod(sc_p scp, int chn, u_int fm, u_int mod);
static void emu_readtremfrq(sc_p scp, int chn, u_int *trem, u_int *frq) __unused;
static void emu_writetremfrq(sc_p scp, int chn, u_int trem, u_int frq);
static void emu_readfm2frq2(sc_p scp, int chn, u_int *fm2, u_int *frq2) __unused;
static void emu_writefm2frq2(sc_p scp, int chn, u_int fm2, u_int frq2);
static void emu_readprobe(sc_p scp, u_int *val);
static void emu_writeprobe(sc_p scp, u_int val) __unused;
static void emu_command(sc_p scp, int reg, int chn, u_long val);
static u_long emu_status(sc_p scp, int reg, int chn);
static int emu_allocres(sc_p scp, device_t dev);
static void emu_releaseres(sc_p scp, device_t dev);

/* PnP IDs */
static struct isa_pnp_id emu_ids[] = {
	{0x21008c0e, "CTL0021 WaveTable Synthesizer"},	/* CTL0021 */
	{0x22008c0e, "CTL0022 WaveTable Synthesizer"},	/* CTL0022 */
};

/*
 * This is the device descriptor for the midi device.
 */
mididev_info emu_op_desc = {
	"EMU8000 Wavetable Synth",

	SNDCARD_AWE32,

	emu_open,
	emu_close,
	emu_ioctl,
	emu_callback,

	MIDI_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

/*
 * Here are the main functions to interact to the user process.
 */

static int
emu_probe(device_t dev)
{
	sc_p scp;
	int unit;
	u_int probe, hwcf1, hwcf2;

	/* Check isapnp ids */
	if (isa_get_logicalid(dev) != 0)
		return (ISA_PNP_PROBE(device_get_parent(dev), dev, emu_ids));
	/* XXX non-pnp emu? */

	unit = device_get_unit(dev);
	scp = device_get_softc(dev);

	device_set_desc(dev, "EMU8000 Wavetable Synth");
	bzero(scp, sizeof(*scp));

	DEB(printf("emu%d: probing.\n", unit));

	if (emu_allocres(scp, dev)) {
		emu_releaseres(scp, dev);
		return (ENXIO);
	}

	emu_readprobe(scp, &probe);
	emu_readhwcf1(scp, &hwcf1);
	emu_readhwcf2(scp, &hwcf2);
	if ((probe & 0x000f) != 0x000c
	    || (hwcf1 & 0x007e) != 0x0058
	    || (hwcf2 & 0x0003) != 0x0003) {
		emu_releaseres(scp, dev);
		return (ENXIO);
	}

	DEB(printf("emu%d: probed.\n", unit));

	return (0);
}

extern synthdev_info midisynth_op_desc;

static int
emu_attach(device_t dev)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit, i;

	unit = device_get_unit(dev);
	scp = device_get_softc(dev);

	DEB(printf("emu%d: attaching.\n", unit));

	if (emu_allocres(scp, dev)) {
		emu_releaseres(scp, dev);
		return (ENXIO);
	}

	/* EMU8000 needs some initialization processes. */

	/* 1. Write HWCF{1,2}. */
	emu_writehwcf1(scp, 0x0059);
	emu_writehwcf2(scp, 0x0020);

	/* Disable the audio. */
	emu_writehwcf3(scp, 0);

	/* 2. Initialize the channels. */

	/* 2a. Write DCYSUSV. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writedcysusv(scp, i, 0, 0, 1, 0);

	/* 2b. Clear the envelope and sound engine registers. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++) {
		emu_writeenvvol(scp, i, 0);
		emu_writeenvval(scp, i, 0);
		emu_writedcysus(scp, i, 0, 0, 0);
		emu_writeatkhldv(scp, i, 0);
		emu_writelfo1val(scp, i, 0);
		emu_writeatkhld(scp, i, 0);
		emu_writelfo2val(scp, i, 0);
		emu_writeip(scp, i, 0);
		emu_writeifatn(scp, i, 0, 0);
		emu_writepefe(scp, i, 0, 0);
		emu_writefmmod(scp, i, 0, 0);
		emu_writetremfrq(scp, i, 0, 0);
		emu_writefm2frq2(scp, i, 0, 0);
		emu_writeptrx(scp, i, 0, 0, 0);
		emu_writevtft(scp, i, 0, 0);
		emu_writepsst(scp, i, 0, 0);
		emu_writecsl(scp, i, 0, 0);
		emu_writeccca(scp, i, 0, 0, 0, 0, 0);
	}

	/* 2c. Clear the current registers. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++) {
		emu_writecpf(scp, i, 0, 0);
		emu_writecvcf(scp, i, 0, 0);
	}

	/* 3. Initialize the sound memory DMA registers. */
	emu_writesmalr(scp, 0, 0);
	emu_writesmarr(scp, 0, 0);
	emu_writesmalw(scp, 0, 0);
	emu_writesmarw(scp, 0, 0);

	/* 4. Fill the array. */

	/* 4a. Set 1. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit1(scp, i, init1_1[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit2(scp, i, init1_2[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit3(scp, i, init1_3[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit4(scp, i, init1_4[i]);

	/* 4b. Have a rest. */
	emu_delay(scp, 1024); /* 1024 samples. */

	/* 4c. Set 2. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit1(scp, i, init2_1[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit2(scp, i, init2_2[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit3(scp, i, init2_3[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit4(scp, i, init2_4[i]);

	/* 4d. Set 3. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit1(scp, i, init3_1[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit2(scp, i, init3_2[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit3(scp, i, init3_3[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit4(scp, i, init3_4[i]);

	/* 4e. Write to HWCF{4,5,6}. */
	emu_writehwcf4(scp, 0);
	emu_writehwcf5(scp, 0x00000083);
	emu_writehwcf6(scp, 0x00008000);

	/* 4f. Set 4. */
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit1(scp, i, init4_1[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit2(scp, i, init4_2[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit3(scp, i, init4_3[i]);
	for (i = 0 ; i < EMU8K_MAXVOICE ; i++)
		emu_writeinit4(scp, i, init4_4[i]);

	/* 5. Determine the size of DRAM. */
	scp->dev = dev;
	scp->dramsize = emu_dramsize(scp);
	printf("emu%d: DRAM size = %dKB\n", unit, scp->dramsize / 1024);

	/* We have inited the EMU8000. Now work on FM. */

	/* Write parameters for the left channel. */
	emu_writedcysusv(scp, 30, 0, 0, 1, 0);
	emu_writepsst(scp, 30, 0x80, 0xffffe0); /* full left */
	emu_writecsl(scp, 30, 0, 0xfffff8); /* chorus */
	emu_writeptrx(scp, 30, 0, 0, 0); /* reverb */
	emu_writecpf(scp, 30, 0, 0);
	emu_writeccca(scp, 30, 0, 0, 0, 0, 0xffffe3);

	/* Then the right channel. */
	emu_writedcysusv(scp, 31, 0, 0, 1, 0);
	emu_writepsst(scp, 31, 0x80, 0xfffff0); /* full right */
	emu_writecsl(scp, 31, 0, 0xfffff8); /* chorus */
	emu_writeptrx(scp, 31, 0, 0, 0xff); /* reverb */
	emu_writecpf(scp, 31, 0, 0);
	emu_writeccca(scp, 31, 0, 0, 0, 0, 0xfffff3);

	/* Skew volume and cutoff. */
	emu_writevtft(scp, 30, 0x8000, 0xffff);
	emu_writevtft(scp, 31, 0x8000, 0xffff);

	/* Ready to sound. */
	emu_writehwcf3(scp, 0x0004);

	/* Fill the softc for this unit. */
	bcopy(&emu_synthinfo, &scp->synthinfo, sizeof(emu_synthinfo));
	mtx_init(&scp->mtx, "emumid", MTX_DEF);
	scp->devinfo = devinfo = create_mididev_info_unit(MDT_SYNTH, &emu_op_desc, &midisynth_op_desc);

	/* Fill the midi info. */
	devinfo->synth.readraw = emu_readraw;
	devinfo->synth.writeraw = emu_writeraw;
	snprintf(devinfo->midistat, sizeof(devinfo->midistat), "at 0x%x, 0x%x, 0x%x",
		 (u_int)rman_get_start(scp->io[0]), (u_int)rman_get_start(scp->io[1]), (u_int)rman_get_start(scp->io[2]));

	midiinit(devinfo, dev);

	DEB(printf("emu%d: attached.\n", unit));

	return (0);
}

static int
emupnp_attach(device_t dev)
{
	return (emu_attach(dev));
}

static int
emu_open(dev_t i_dev, int flags, int mode, struct thread *td)
{
	return (0);
}

static int
emu_close(dev_t i_dev, int flags, int mode, struct thread *td)
{
	return (0);
}

static int
emu_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
	sc_p scp;
	mididev_info *devinfo;
	int unit;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;

	unit = MIDIUNIT(i_dev);

	DEB(printf("emu%d: ioctlling, cmd 0x%x.\n", unit, (int)cmd));

	devinfo = get_mididev_info(i_dev, &unit);
	if (devinfo == NULL) {
		DEB(printf("emu_ioctl: unit %d is not configured.\n", unit));
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
		return (0);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		if (midiinfo->device != unit)
			return (ENXIO);
		bcopy(&emu_midiinfo, midiinfo, sizeof(emu_midiinfo));
		strcpy(midiinfo->name, scp->synthinfo.name);
		midiinfo->device = unit;
		return (0);
		break;
	case SNDCTL_SYNTH_MEMAVL:
		return 0x7fffffff;
		break;
	default:
		return (ENOSYS);
	}
	/* NOTREACHED */
	return (EINVAL);
}

static int
emu_callback(void *d, int reason)
{
	mididev_info *devinfo;

	devinfo = (mididev_info *)d;

	mtx_assert(&devinfo->flagqueue_mtx, MA_OWNED);

	return (0);
}

static int
emu_readraw(mididev_info *md, u_char *buf, int len, int *lenr, int nonblock)
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
		DEB(printf("emu_readraw: unit %d is not for reading.\n", unit));
		return (EIO);
	}

	/* NOP. */
	*lenr = 0;

	return (0);
}

static int
emu_writeraw(mididev_info *md, u_char *buf, int len, int *lenw, int nonblock)
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
		DEB(printf("emu_writeraw: unit %d is not for writing.\n", unit));
		return (EIO);
	}

	/* NOP. */
	*lenw = 0;

	return (0);
}

/*
 * The functions below here are the synthesizer interfaces.
 */

/*
 * The functions below here are the libraries for the above ones.
 */

/* Determine the size of DRAM. */
static u_int
emu_dramsize(sc_p scp)
{
	u_int dramsize;
	static u_short magiccode[] = {0x386d, 0xbd2a, 0x73df, 0xf2d8};
	static u_short magiccode2[] = {0x5ef3, 0x2b90, 0xa4c8, 0x6a13};
	u_short buf[sizeof(magiccode) / sizeof(*magiccode)];

	/*
	 * Write the magic code to the bottom of DRAM.
	 * Writing to a wrapped address clobbers the code.
	 */
	emu_allocdmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE);
	emu_dmaaddress(scp, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE, EMU8K_DRAM_RAM);
	emu_writeblkstream(scp, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE, magiccode, sizeof(magiccode) / sizeof(*magiccode));
	emu_releasedmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE);

	for (dramsize = 0 ; dramsize + EMU8K_DRAM_RAM < EMU8K_DRAM_MAX ; ) {

		/* Read the magic code. */
		emu_allocdmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_READ);
		emu_dmaaddress(scp, EMU8K_DMA_LEFT | EMU8K_DMA_READ, EMU8K_DRAM_RAM);
		emu_readblkstream(scp, EMU8K_DMA_LEFT | EMU8K_DMA_READ, buf, sizeof(buf) / sizeof(*buf));
		emu_releasedmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_READ);

		/* Compare the code. */
		if (bcmp(magiccode, buf, sizeof(magiccode)))
			break;

		/* Increase the DRAM size. */
		dramsize += 0x8000;

		/* Try writing a different magic code to dramsize. */
		emu_allocdmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE);
		emu_dmaaddress(scp, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE, dramsize + EMU8K_DRAM_RAM);
		emu_writeblkstream(scp, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE, magiccode2, sizeof(magiccode2) / sizeof(*magiccode2));
		emu_releasedmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_WRITE);

		/* Then read the magic code. */
		emu_allocdmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_READ);
		emu_dmaaddress(scp, EMU8K_DMA_LEFT | EMU8K_DMA_READ, dramsize + EMU8K_DRAM_RAM);
		emu_readblkstream(scp, EMU8K_DMA_LEFT | EMU8K_DMA_READ, buf, sizeof(buf) / sizeof(*buf));
		emu_releasedmachn(scp, 31, EMU8K_DMA_LEFT | EMU8K_DMA_READ);

		/* Compare the code. */
		if (bcmp(magiccode2, buf, sizeof(magiccode2)))
			break;
	}
	if (dramsize + EMU8K_DRAM_RAM > EMU8K_DRAM_MAX)
		dramsize = EMU8K_DRAM_MAX - EMU8K_DRAM_RAM;

	return dramsize * 2; /* dramsize is in words. */
}

/* Allocates a channel to a DMA stream. */
static void
emu_allocdmachn(sc_p scp, int chn, int mode)
{
	/* Turn off the sound, prepare for a DMA stream. */
	emu_writedcysusv(scp, chn, 0, 0, 1, 0);
	emu_writevtft(scp, chn, 0, 0);
	emu_writecvcf(scp, chn, 0, 0);
	emu_writeptrx(scp, chn, 0x4000, 0, 0);
	emu_writecpf(scp, chn, 0x4000, 0);
	emu_writepsst(scp, chn, 0, 0);
	emu_writecsl(scp, chn, 0, 0);

	/* Enter DMA mode. */
	emu_writeccca(scp, chn, 0, 1,
		      ((mode & EMU8K_DMA_WRITE) > 0) ? 1 : 0,
		      ((mode & EMU8K_DMA_RIGHT) > 0) ? 1 : 0,
		      0);
}

/* Programs the initial address to a DMA. */
static void
emu_dmaaddress(sc_p scp, int mode, u_int addr)
{
	/* Wait until the stream comes ready. */
	emu_waitstream(scp, mode);

	switch(mode & EMU8K_DMA_MASK)
	{
	case EMU8K_DMA_LEFT | EMU8K_DMA_READ:
		emu_writesmalr(scp, 0, addr);
		emu_readsmld(scp, NULL); /* Read the stale data. */
		break;
	case EMU8K_DMA_RIGHT | EMU8K_DMA_READ:
		emu_writesmarr(scp, 0, addr);
		emu_readsmrd(scp, NULL); /* Read the stale data. */
		break;
	case EMU8K_DMA_LEFT | EMU8K_DMA_WRITE:
		emu_writesmalw(scp, 0, addr);
		break;
	case EMU8K_DMA_RIGHT | EMU8K_DMA_WRITE:
		emu_writesmarw(scp, 0, addr);
		break;
	}
}

/* Waits until a stream gets ready. */
static void
emu_waitstream(sc_p scp, int mode)
{
	int i;
	u_int busy;

	for (i = 0 ; i < 100000 ; i++) {
		switch(mode & EMU8K_DMA_MASK)
		{
		case EMU8K_DMA_LEFT | EMU8K_DMA_READ:
			emu_readsmalr(scp, &busy, NULL);
			break;
		case EMU8K_DMA_RIGHT | EMU8K_DMA_READ:
			emu_readsmarr(scp, &busy, NULL);
			break;
		case EMU8K_DMA_LEFT | EMU8K_DMA_WRITE:
			emu_readsmalw(scp, &busy, NULL);
			break;
		case EMU8K_DMA_RIGHT | EMU8K_DMA_WRITE:
			emu_readsmarw(scp, &busy, NULL);
			break;
		}
		if (!busy)
			break;
		emu_delay(scp, 1);
	}
	if (busy)
		printf("emu%d: stream data still busy, timed out.\n", device_get_unit(scp->dev));
}

/* Reads a word block from a stream. */
static void
emu_readblkstream(sc_p scp, int mode, u_short *data, size_t len)
{
	while((len--) > 0)
		emu_readstream(scp, mode, data++);
}

/* Writes a word block stream to a stream. */
static void
emu_writeblkstream(sc_p scp, int mode, u_short *data, size_t len)
{
	while((len--) > 0)
		emu_writestream(scp, mode, *(data++));
}

/* Reads a word from a stream. */
static void
emu_readstream(sc_p scp, int mode, u_short *data)
{
	if ((mode & EMU8K_DMA_RW) != EMU8K_DMA_READ)
		return;

	switch(mode & EMU8K_DMA_MASK)
	{
	case EMU8K_DMA_LEFT | EMU8K_DMA_READ:
		emu_readsmld(scp, data);
		break;
	case EMU8K_DMA_RIGHT | EMU8K_DMA_READ:
		emu_readsmrd(scp, data);
		break;
	}
}

/* Writes a word to a stream. */
static void
emu_writestream(sc_p scp, int mode, u_short data)
{
	if ((mode & EMU8K_DMA_RW) != EMU8K_DMA_WRITE)
		return;

	switch(mode & EMU8K_DMA_MASK)
	{
	case EMU8K_DMA_LEFT | EMU8K_DMA_WRITE:
		emu_writesmld(scp, data);
		break;
	case EMU8K_DMA_RIGHT | EMU8K_DMA_WRITE:
		emu_writesmrd(scp, data);
		break;
	}
}

/* Releases a channel from a DMA stream. */
static void
emu_releasedmachn(sc_p scp, int chn, int mode)
{
	/* Wait until the stream comes ready. */
	emu_waitstream(scp, mode);

	/* Leave DMA mode. */
	emu_writeccca(scp, chn, 0, 0, 0, 0, 0);
}

/*
 * Waits cycles.
 * Idea-stolen-from: sys/i386/isa/clock.c:DELAY()
 */
static void
emu_delay(sc_p scp, short n)
{
	int wc_prev, wc, wc_left, wc_delta;

	emu_readwc(scp, &wc_prev);
	wc_left = n;

	while (wc_left > 0) {
		emu_readwc(scp, &wc);
		wc_delta = wc - wc_prev; /* The counter increases. */
		wc_prev = wc;
		if (wc_delta < 0)
			wc_delta += 0xffff;
		wc_left -= wc_delta;
	}
}

/* The followings provide abstract access to the registers. */
#define DECBIT(sts, shift, len) (((sts) >> (shift))) & (0xffffffff >> (32 - len))
#define GENBIT(val, shift, len) (((val) & (0xffffffff >> (32 - len))) << (shift))

/* CPF: Current Pitch and Fractional Address */
static void
emu_readcpf(sc_p scp, int chn, u_int *cp, u_int *f)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_CPF, chn);
	if (cp != NULL)
		*cp = DECBIT(sts, 16, 16);
	if (f != NULL)
		*f = DECBIT(sts, 0, 16);
}

static void
emu_writecpf(sc_p scp, int chn, u_int cp, u_int f)
{
	emu_command(scp, EMU8K_CPF, chn,
		    GENBIT(cp, 16, 16)
		    | GENBIT(f, 0, 16));
}

/* PTRX: Pitch Target, Rvb Send and Aux Byte */
static void
emu_readptrx(sc_p scp, int chn, u_int *pt, u_int *rs, u_int *auxd)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_PTRX, chn);
	if (pt != NULL)
		*pt = DECBIT(sts, 16, 16);
	if (rs != NULL)
		*rs = DECBIT(sts, 8, 8);
	if (auxd != NULL)
		*auxd = DECBIT(sts, 0, 8);
}

static void
emu_writeptrx(sc_p scp, int chn, u_int pt, u_int rs, u_int auxd)
{
	emu_command(scp, EMU8K_PTRX, chn,
		    GENBIT(pt, 16, 16)
		    | GENBIT(rs, 8, 8)
		    | GENBIT(auxd, 0, 8));
}

/* CVCF: Current Volume and Filter Cutoff */
static void
emu_readcvcf(sc_p scp, int chn, u_int *cv, u_int *cf)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_CVCF, chn);
	if (cv != NULL)
		*cv = DECBIT(sts, 16, 16);
	if (cf != NULL)
		*cf = DECBIT(sts, 0, 16);
}

static void
emu_writecvcf(sc_p scp, int chn, u_int cv, u_int cf)
{
	emu_command(scp, EMU8K_CVCF, chn,
		    GENBIT(cv, 16, 16)
		    | GENBIT(cf, 0, 16));
}

/* VTFT: Volume and Filter Cutoff Targets */
static void
emu_readvtft(sc_p scp, int chn, u_int *vt, u_int *ft)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_VTFT, chn);
	if (vt != NULL)
		*vt = DECBIT(sts, 16, 16);
	if (ft != NULL)
		*ft = DECBIT(sts, 0, 16);
}

static void
emu_writevtft(sc_p scp, int chn, u_int vt, u_int ft)
{
	emu_command(scp, EMU8K_VTFT, chn,
		    GENBIT(vt, 16, 16)
		    | GENBIT(ft, 0, 16));
}

/* PSST: Pan Send and Loop Start Address */
static void
emu_readpsst(sc_p scp, int chn, u_int *pan, u_int *st)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_PSST, chn);
	if (pan != NULL)
		*pan = DECBIT(sts, 24, 8);
	if (st != NULL)
		*st = DECBIT(sts, 0, 24);
}

static void
emu_writepsst(sc_p scp, int chn, u_int pan, u_int st)
{
	emu_command(scp, EMU8K_PSST, chn,
		    GENBIT(pan, 24, 8)
		    | GENBIT(st, 0, 24));
}

/* CSL: Chorus Send and Loop End Address */
static void
emu_readcsl(sc_p scp, int chn, u_int *cs, u_int *lp)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_CSL, chn);
	if (cs != NULL)
		*cs = DECBIT(sts, 24, 8);
	if (lp != NULL)
		*lp = DECBIT(sts, 0, 24);
}

static void
emu_writecsl(sc_p scp, int chn, u_int cs, u_int lp)
{
	emu_command(scp, EMU8K_CSL, chn,
		    GENBIT(cs, 24, 8)
		    | GENBIT(lp, 0, 24));
}

/* CCCA: Q, Control Bits and Current Address */
static void
emu_readccca(sc_p scp, int chn, u_int *q, u_int *dma, u_int *wr, u_int *right, u_int *ca)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_CCCA, chn);
	if (q != NULL)
		*q = DECBIT(sts, 28, 4);
	if (dma != NULL)
		*dma = DECBIT(sts, 26, 1);
	if (wr != NULL)
		*wr = DECBIT(sts, 25, 1);
	if (right != NULL)
		*right = DECBIT(sts, 24, 1);
	if (ca != NULL)
		*ca = DECBIT(sts, 0, 24);
}

static void
emu_writeccca(sc_p scp, int chn, u_int q, u_int dma, u_int wr, u_int right, u_int ca)
{
	emu_command(scp, EMU8K_CCCA, chn,
		    GENBIT(q, 28, 4)
		    | GENBIT(dma, 26, 1)
		    | GENBIT(wr, 25, 1)
		    | GENBIT(right, 24, 1)
		    | GENBIT(ca, 0, 24));
}

/* HWCF4: Configuration Double Word 4 */
static void
emu_readhwcf4(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_HWCF4, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writehwcf4(sc_p scp, u_int val)
{
	if (val != 0)
		printf("emu%d: writing value 0x%x to HWCF4.\n", device_get_unit(scp->dev), val);
	emu_command(scp, EMU8K_HWCF4, 0, val);
}

/* HWCF5: Configuration Double Word 5 */
static void
emu_readhwcf5(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_HWCF5, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writehwcf5(sc_p scp, u_int val)
{
	if (val != 0x00000083)
		printf("emu%d: writing value 0x%x to HWCF5.\n", device_get_unit(scp->dev), val);
	emu_command(scp, EMU8K_HWCF5, 0, val);
}

/* HWCF6: Configuration Double Word 6 */
static void
emu_readhwcf6(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_HWCF6, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writehwcf6(sc_p scp, u_int val)
{
	if (val != 0x00008000)
		printf("emu%d: writing value 0x%x to HWCF6.\n", device_get_unit(scp->dev), val);
	emu_command(scp, EMU8K_HWCF6, 0, val);
}

/* SMALR: Sound Memory Address for Left SM Reads */
static void
emu_readsmalr(sc_p scp, u_int *mt, u_int *smalr)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_SMALR, 0);
	if (mt != NULL)
		*mt = DECBIT(sts, 31, 1);
	if (smalr != NULL)
		*smalr = DECBIT(sts, 0, 24);
}

static void
emu_writesmalr(sc_p scp, u_int mt, u_int smalr)
{
	emu_command(scp, EMU8K_SMALR, 0,
		    GENBIT(mt, 31, 1)
		    | GENBIT(smalr, 0, 24));
}

/* SMARR: Sound Memory Address for Right SM Reads */
static void
emu_readsmarr(sc_p scp, u_int *mt, u_int *smarr)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_SMARR, 0);
	if (mt != NULL)
		*mt = DECBIT(sts, 31, 1);
	if (smarr != NULL)
		*smarr = DECBIT(sts, 0, 24);
}

static void
emu_writesmarr(sc_p scp, u_int mt, u_int smarr)
{
	emu_command(scp, EMU8K_SMARR, 0,
		    GENBIT(mt, 31, 1)
		    | GENBIT(smarr, 0, 24));
}

/* SMALW: Sound Memory Address for Left SM Writes */
static void
emu_readsmalw(sc_p scp, u_int *full, u_int *smalw)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_SMALW, 0);
	if (full != NULL)
		*full = DECBIT(sts, 31, 1);
	if (smalw != NULL)
		*smalw = DECBIT(sts, 0, 24);
}

static void
emu_writesmalw(sc_p scp, u_int full, u_int smalw)
{
	emu_command(scp, EMU8K_SMALW, 0,
		    GENBIT(full, 31, 1)
		    | GENBIT(smalw, 0, 24));
}

/* SMARW: Sound Memory Address for Right SM Writes */
static void
emu_readsmarw(sc_p scp, u_int *full, u_int *smarw)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_SMARW, 0);
	if (full != NULL)
		*full = DECBIT(sts, 31, 1);
	if (smarw != NULL)
		*smarw = DECBIT(sts, 0, 24);
}

static void
emu_writesmarw(sc_p scp, u_int full, u_int smarw)
{
	emu_command(scp, EMU8K_SMARW, 0,
		    GENBIT(full, 31, 1)
		    | GENBIT(smarw, 0, 24));
}

/* SMLD: Sound Memory Left Data */
static void
emu_readsmld(sc_p scp, u_short *smld)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_SMLD, 0);
	if (smld != NULL)
		*smld = sts;
}

static void
emu_writesmld(sc_p scp, u_short smld)
{
	emu_command(scp, EMU8K_SMLD, 0, smld);
}

/* SMRD: Sound Memory Right Data */
static void
emu_readsmrd(sc_p scp, u_short *smrd)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_SMRD, 0);
	if (smrd != NULL)
		*smrd = sts;
}

static void
emu_writesmrd(sc_p scp, u_short smrd)
{
	emu_command(scp, EMU8K_SMRD, 0, smrd);
}

/* WC: Sample COunter */
static void
emu_readwc(sc_p scp, u_int *wc)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_WC, 0);
	if (wc != NULL)
		*wc = sts;
}

static void
emu_writewc(sc_p scp, u_int wc)
{
	emu_command(scp, EMU8K_WC, 0, wc);
}

/* HWCF1: Configuration Double Word 1 */
static void
emu_readhwcf1(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_HWCF1, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writehwcf1(sc_p scp, u_int val)
{
	if (val != 0x0059)
		printf("emu%d: writing value 0x%x to HWCF1.\n", device_get_unit(scp->dev), val);
	emu_command(scp, EMU8K_HWCF1, 0, val);
}

/* HWCF2: Configuration Double Word 2 */
static void
emu_readhwcf2(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_HWCF2, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writehwcf2(sc_p scp, u_int val)
{
	if (val != 0x0020)
		printf("emu%d: writing value 0x%x to HWCF2.\n", device_get_unit(scp->dev), val);
	emu_command(scp, EMU8K_HWCF2, 0, val);
}

/* HWCF3: Configuration Double Word 3 */
static void
emu_readhwcf3(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_HWCF3, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writehwcf3(sc_p scp, u_int val)
{
	if (val != 0x0004 && val != 0)
		printf("emu%d: writing value 0x%x to HWCF3.\n", device_get_unit(scp->dev), val);
	emu_command(scp, EMU8K_HWCF3, 0, val);
}

/* INIT1: Initialization Array 1 */
static void
emu_readinit1(sc_p scp, int chn, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_INIT1, chn);
	if (val != NULL)
		*val = sts;
}

static void
emu_writeinit1(sc_p scp, int chn, u_int val)
{
	emu_command(scp, EMU8K_INIT1, chn, val);
}

/* INIT2: Initialization Array 2 */
static void
emu_readinit2(sc_p scp, int chn, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_INIT2, chn);
	if (val != NULL)
		*val = sts;
}

static void
emu_writeinit2(sc_p scp, int chn, u_int val)
{
	emu_command(scp, EMU8K_INIT2, chn, val);
}

/* INIT3: Initialization Array 3 */
static void
emu_readinit3(sc_p scp, int chn, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_INIT3, chn);
	if (val != NULL)
		*val = sts;
}

static void
emu_writeinit3(sc_p scp, int chn, u_int val)
{
	emu_command(scp, EMU8K_INIT3, chn, val);
}

/* INIT4: Initialization Array 4 */
static void
emu_readinit4(sc_p scp, int chn, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_INIT4, chn);
	if (val != NULL)
		*val = sts;
}

static void
emu_writeinit4(sc_p scp, int chn, u_int val)
{
	emu_command(scp, EMU8K_INIT4, chn, val);
}

/* ENVVOL: Volume Envelope Decay */
static void
emu_readenvvol(sc_p scp, int chn, u_int *envvol)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_ENVVOL, chn);
	if (envvol != NULL)
		*envvol = sts;
}

static void
emu_writeenvvol(sc_p scp, int chn, u_int envvol)
{
	emu_command(scp, EMU8K_ENVVOL, chn, envvol);
}

/* DCYSUSV: Volume Envelope Sustain and Decay */
static void
emu_readdcysusv(sc_p scp, int chn, u_int *ph1v, u_int *susv, u_int *off, u_int *dcyv)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_DCYSUSV, chn);
	if (ph1v != NULL)
		*ph1v = DECBIT(sts, 15, 1);
	if (susv != NULL)
		*susv = DECBIT(sts, 8, 7);
	if (off != NULL)
		*off = DECBIT(sts, 7, 1);
	if (dcyv != NULL)
		*dcyv = DECBIT(sts, 0, 7);
}

static void
emu_writedcysusv(sc_p scp, int chn, u_int ph1v, u_int susv, u_int off, u_int dcyv)
{
	emu_command(scp, EMU8K_DCYSUSV, chn,
		    GENBIT(ph1v, 15, 1)
		    | GENBIT(susv, 8, 7)
		    | GENBIT(off, 7, 1)
		    | GENBIT(dcyv, 0, 7));
}

/* ENVVAL: Modulation Envelope Decay */
static void
emu_readenvval(sc_p scp, int chn, u_int *envval)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_ENVVAL, chn);
	if (envval != NULL)
		*envval = sts;
}

static void
emu_writeenvval(sc_p scp, int chn, u_int envval)
{
	emu_command(scp, EMU8K_ENVVAL, chn, envval);
}

/* DCYSUS: Modulation Envelope Sustain and Decay */
static void
emu_readdcysus(sc_p scp, int chn, u_int *ph1, u_int *sus, u_int *dcy)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_DCYSUS, chn);
	if (ph1 != NULL)
		*ph1 = DECBIT(sts, 15, 1);
	if (sus != NULL)
		*sus = DECBIT(sts, 8, 7);
	if (dcy != NULL)
		*dcy = DECBIT(sts, 0, 7);
}

static void
emu_writedcysus(sc_p scp, int chn, u_int ph1, u_int sus, u_int dcy)
{
	emu_command(scp, EMU8K_DCYSUS, chn,
		    GENBIT(ph1, 15, 1)
		    | GENBIT(sus, 8, 7)
		    | GENBIT(dcy, 0, 7));
}

/* ATKHLDV: Volume Envelope Hold and Attack */
static void
emu_readatkhldv(sc_p scp, int chn, u_int *atkhldv)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_ATKHLDV, chn);
	if (atkhldv != NULL)
		*atkhldv = sts;
}

static void
emu_writeatkhldv(sc_p scp, int chn, u_int atkhldv)
{
	emu_command(scp, EMU8K_ATKHLDV, chn, atkhldv);
}

/* LFO1VAL: LFO #1 Delay */
static void
emu_readlfo1val(sc_p scp, int chn, u_int *lfo1val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_LFO1VAL, chn);
	if (lfo1val != NULL)
		*lfo1val = sts;
}

static void
emu_writelfo1val(sc_p scp, int chn, u_int lfo1val)
{
	emu_command(scp, EMU8K_LFO1VAL, chn, lfo1val);
}

/* ATKHLD: Modulation Envelope Hold and Attack */
static void
emu_readatkhld(sc_p scp, int chn, u_int *atkhld)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_ATKHLD, chn);
	if (atkhld != NULL)
		*atkhld = sts;
}

static void
emu_writeatkhld(sc_p scp, int chn, u_int atkhld)
{
	emu_command(scp, EMU8K_ATKHLD, chn, atkhld);
}

/* LFO2VAL: LFO #2 Delay */
static void
emu_readlfo2val(sc_p scp, int chn, u_int *lfo2val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_LFO2VAL, chn);
	if (lfo2val != NULL)
		*lfo2val = sts;
}

static void
emu_writelfo2val(sc_p scp, int chn, u_int lfo2val)
{
	emu_command(scp, EMU8K_LFO2VAL, chn, lfo2val);
}

/* IP: Initial Pitch */
static void
emu_readip(sc_p scp, int chn, u_int *ip)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_IP, chn);
	if (ip != NULL)
		*ip = sts;
}

static void
emu_writeip(sc_p scp, int chn, u_int ip)
{
	emu_command(scp, EMU8K_IP, chn, ip);
}

/* IFATN: Initial Filter Cutoff and Attenuation */
static void
emu_readifatn(sc_p scp, int chn, u_int *ifc, u_int *atn)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_IFATN, chn);
	if (ifc != NULL)
		*ifc = DECBIT(sts, 8, 8);
	if (atn != NULL)
		*atn = DECBIT(sts, 0, 8);
}

static void
emu_writeifatn(sc_p scp, int chn, u_int ifc, u_int atn)
{
	emu_command(scp, EMU8K_IFATN, chn,
		    GENBIT(ifc, 8, 8)
		    | GENBIT(atn, 0, 8));
}

/* PEFE: Pitch and Filter Envelope Heights */
static void
emu_readpefe(sc_p scp, int chn, u_int *pe, u_int *fe)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_PEFE, chn);
	if (pe != NULL)
		*pe = DECBIT(sts, 8, 8);
	if (fe != NULL)
		*fe = DECBIT(sts, 0, 8);
}

static void
emu_writepefe(sc_p scp, int chn, u_int pe, u_int fe)
{
	emu_command(scp, EMU8K_PEFE, chn,
		    GENBIT(pe, 8, 8)
		    | GENBIT(fe, 0, 8));
}

/* FMMOD: Vibrato and Filter Modulation from LFO #1 */
static void
emu_readfmmod(sc_p scp, int chn, u_int *fm, u_int *mod)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_FMMOD, chn);
	if (fm != NULL)
		*fm = DECBIT(sts, 8, 8);
	if (mod != NULL)
		*mod = DECBIT(sts, 0, 8);
}

static void
emu_writefmmod(sc_p scp, int chn, u_int fm, u_int mod)
{
	emu_command(scp, EMU8K_FMMOD, chn,
		    GENBIT(fm, 8, 8)
		    | GENBIT(mod, 0, 8));
}

/* TREMFRQ: LFO #1 Tremolo Amount and Frequency */
static void
emu_readtremfrq(sc_p scp, int chn, u_int *trem, u_int *frq)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_TREMFRQ, chn);
	if (trem != NULL)
		*trem = DECBIT(sts, 8, 8);
	if (frq != NULL)
		*frq = DECBIT(sts, 0, 8);
}

static void
emu_writetremfrq(sc_p scp, int chn, u_int trem, u_int frq)
{
	emu_command(scp, EMU8K_TREMFRQ, chn,
		    GENBIT(trem, 8, 8)
		    | GENBIT(frq, 0, 8));
}

/* FM2FRQ2: LFO #2 Vibrato Amount and Frequency */
static void
emu_readfm2frq2(sc_p scp, int chn, u_int *fm2, u_int *frq2)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_FM2FRQ2, chn);
	if (fm2 != NULL)
		*fm2 = DECBIT(sts, 8, 8);
	if (frq2 != NULL)
		*frq2 = DECBIT(sts, 0, 8);
}

static void
emu_writefm2frq2(sc_p scp, int chn, u_int fm2, u_int frq2)
{
	emu_command(scp, EMU8K_FM2FRQ2, chn,
		    GENBIT(fm2, 8, 8)
		    | GENBIT(frq2, 0, 8));
}

/* PROBE: Probe Register */
static void
emu_readprobe(sc_p scp, u_int *val)
{
	u_long sts;

	sts = emu_status(scp, EMU8K_PROBE, 0);
	if (val != NULL)
		*val = sts;
}

static void
emu_writeprobe(sc_p scp, u_int val)
{
	emu_command(scp, EMU8K_PROBE, 0, val);
}

/* Writes to a register. */
static void
emu_command(sc_p scp, int reg, int chn, u_long val)
{
	if (chn < 0 || chn >= EMU8K_MAXVOICE || reg < 0 || reg >= EMU8K_REGNUM)
		return;

	/* Override the channel if necessary. */
	if (emu_regs[reg].chn != EMU8K_CHN_ANY)
		chn = emu_regs[reg].chn;

	/* Select the register first. */
	bus_space_write_2(rman_get_bustag(scp->io[EMU8K_IDX_PTR]), rman_get_bushandle(scp->io[EMU8K_IDX_PTR]), EMU8K_PORT_PTR, (chn & 0x1f) | ((emu_regs[reg].reg & 0x07) << 5));

	/* Then we write the data. */
	bus_space_write_2(rman_get_bustag(scp->io[emu_regs[reg].index]), rman_get_bushandle(scp->io[emu_regs[reg].index]), emu_regs[reg].port, val & 0xffff);
	if (emu_regs[reg].size)
		/* double word */
		bus_space_write_2(rman_get_bustag(scp->io[emu_regs[reg].index]), rman_get_bushandle(scp->io[emu_regs[reg].index]), emu_regs[reg].port + 2, (val >> 16) & 0xffff);
}

/* Reads from a register. */
static u_long
emu_status(sc_p scp, int reg, int chn)
{
	u_long status;

	if (chn < 0 || chn >= EMU8K_MAXVOICE || reg < 0 || reg >= EMU8K_REGNUM)
		return (0xffffffff);

	/* Override the channel if necessary. */
	if (emu_regs[reg].chn != EMU8K_CHN_ANY)
		chn = emu_regs[reg].chn;

	/* Select the register first. */
	bus_space_write_2(rman_get_bustag(scp->io[EMU8K_IDX_PTR]), rman_get_bushandle(scp->io[EMU8K_IDX_PTR]), EMU8K_PORT_PTR, (chn & 0x1f) | ((emu_regs[reg].reg & 0x07) << 5));

	/* Then we read the data. */
	status = bus_space_read_2(rman_get_bustag(scp->io[emu_regs[reg].index]), rman_get_bushandle(scp->io[emu_regs[reg].index]), emu_regs[reg].port) & 0xffff;
	if (emu_regs[reg].size)
		/* double word */
		status |= (bus_space_read_2(rman_get_bustag(scp->io[emu_regs[reg].index]), rman_get_bushandle(scp->io[emu_regs[reg].index]), emu_regs[reg].port + 2) & 0xffff) << 16;

	return (status);
}

/* Allocates resources. */
static int
emu_allocres(sc_p scp, device_t dev)
{
	int iobase;

	if (scp->io[0] == NULL) {
		scp->io_rid[0] = 0;
		scp->io[0] = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid[0], 0, ~0, 4, RF_ACTIVE);
	}
	if (scp->io[0] == NULL)
		return (1);
	iobase = rman_get_start(scp->io[0]);
	if (scp->io[1] == NULL) {
		scp->io_rid[1] = 1;
		scp->io[1] = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid[1], iobase + 0x400, iobase + 0x400 + 3, 4, RF_ACTIVE);
	}
	if (scp->io[2] == NULL) {
		scp->io_rid[2] = 2;
		scp->io[2] = bus_alloc_resource(dev, SYS_RES_IOPORT, &scp->io_rid[2], iobase + 0x800, iobase + 0x800 + 3, 4, RF_ACTIVE);
	}

	if (scp->io[0] == NULL || scp->io[1] == NULL || scp->io[2] == NULL) {
		printf("emu_allocres: failed.\n");
		return (1);
	}

	return (0);
}

/* Releases resources. */
static void
emu_releaseres(sc_p scp, device_t dev)
{
	if (scp->io[0] != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, scp->io_rid[0], scp->io[0]);
		scp->io[0] = NULL;
	}
	if (scp->io[1] != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, scp->io_rid[1], scp->io[1]);
		scp->io[1] = NULL;
	}
	if (scp->io[2] != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, scp->io_rid[2], scp->io[2]);
		scp->io[2] = NULL;
	}
}

static device_method_t emu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , emu_probe ),
	DEVMETHOD(device_attach, emu_attach),

	{ 0, 0 },
};

static driver_t emu_driver = {
	"midi",
	emu_methods,
	sizeof(struct emu_softc),
};

DRIVER_MODULE(emu, isa, emu_driver, midi_devclass, 0, 0);
