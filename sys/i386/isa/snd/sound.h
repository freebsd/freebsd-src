/*
 * sound.h
 *
 * include file for kernel sources, sound driver.
 * 
 * Copyright by Hannu Savolainen 1995
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "pcm.h"
#if NPCM > 0

/*
 * first, include kernel header files.
 */

#ifndef _OS_H_
#define _OS_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioccom.h>

#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/proc.h>

#include <sys/kernel.h> /* for DATA_SET */

#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <i386/isa/isa_device.h>

#include <machine/clock.h>	/* for DELAY */

typedef void    (irq_proc_t) (int irq);

#endif	/* _OS_H_ */

/*      
 * descriptor of a dma buffer. See dmabuf.c for documentation.
 * (rp,rl) and (fp,fl) identify the READY and FREE regions of the
 * buffer. (dp,dl) identify the region currently used by the DMA.
 * Only dmabuf.c should test the value of dl. The reason is that
 * in auto-dma mode looking at dl alone does not make much sense,
 * since the transfer potentially spans the entire buffer.
 */     
        
typedef struct _snd_dbuf {
        char *buf;
        int     bufsize ;
        volatile int dp, rp, fp;
        volatile int dl, rl, fl;
	volatile int dl0; /* value used last time in dl */
	int int_count;
} snd_dbuf ;

/*
 * descriptor of audio operations ...
 *
 */
typedef struct _snddev_info snddev_info ;
typedef int (snd_callback_t)(snddev_info *d, int reason);

struct _snddev_info {

    /*
     * the first part of the descriptor is filled up from a
     * template.
     */
    char name[64];

    int type ;

    int (*probe)(struct isa_device * dev);
    int (*attach)(struct isa_device * dev) ;
    d_open_t *open ;
    d_close_t *close ;
    d_read_t *read ;
    d_write_t *write ;
    d_ioctl_t *ioctl ;
    d_select_t *select ;
    irq_proc_t  *isr ;
    snd_callback_t *callback;

    int     bufsize;        /* space used for buffers */

    u_long  audio_fmt ;     /* supported audio formats */


    /*
     * combinations of the following flags are used as second argument in
     * the callback from the dma module to the device-specific routines.
     */

#define SND_CB_RD       0x100   /* read callback */
#define SND_CB_WR       0x200   /* write callback */
#define SND_CB_REASON_MASK      0xff
#define SND_CB_START    0x01   /* start dma op */
#define SND_CB_RESTART  0x02   /* restart dma op */
#define SND_CB_STOP     0x03   /* stop dma op */
#define SND_CB_ABORT    0x04   /* abort dma op */
#define SND_CB_INIT     0x05   /* init board parameters */
	/* init can only be called with int enabled and
	 * no pending DMA activity.
	 */

    /*
     * whereas from here, parameters are set at runtime.
     */

    int     io_base ;	/* primary I/O address for the board */
    int     alt_base ; /* some codecs are accessible as SB+WSS... */
    int     conf_base ; /* and the opti931 also has a config space */
    int     mix_base ; /* base for the mixer... */
    int     midi_base ; /* base for the midi */
    int     synth_base ; /* base for the synth */

    int     irq ;
    int     dma1, dma2 ;  /* dma2=dma1 for half-duplex cards */

    int bd_id ;     /* used to hold board-id info, eg. sb version,
		     * mss codec type, etc. etc.
		     */

    snd_dbuf dbuf_out, dbuf_in;

    int     status_ptr;     /* used to implement sndstat */

        /*
         * these parameters describe the operation of the board.
         * Generic things like busy flag, speed, etc are here.
         */

    volatile u_long  flags ;     /* 32 bits, used for various purposes. */

    /*
     * we have separate flags for read and write, although in some
     * cases this is probably not necessary (e.g. because we cannot
     * know how many processes are using the device, we cannot
     * distinguish if open, close, abort are for a write or for a
     * read).
     */

    /*
     * the following flag is used by open-close routines
     * to mark the status of the device.
     */
#define SND_F_BUSY              0x0001  /* has been opened 	*/
    /*
     * Only the last close for a device will propagate to the driver.
     * Unfortunately, voxware uses 3 different minor numbers
     * (dsp, dsp16 and audio) to access the same unit. So, if
     * we want to support multiple opens and still keep track of
     * what is happening, we also need a separate flag for each minor
     * number. These are below...
     */
#define	SND_F_BUSY_AUDIO	0x10000000
#define	SND_F_BUSY_DSP		0x20000000
#define	SND_F_BUSY_DSP16	0x40000000
#define	SND_F_BUSY_ANY		0x70000000
    /*
     * the next two are used to allow only one pending operation of
     * each type.
     */
#define SND_F_READING           0x0004  /* have a pending read */
#define SND_F_WRITING           0x0008  /* have a pending write */
    /*
     * these mark pending DMA operations. When you have pending dma ops,
     * you might get interrupts, so some manipulations of the
     * descriptors must be done with interrupts blocked.
     */
#define SND_F_RD_DMA            0x0010  /* read-dma active */
#define SND_F_WR_DMA            0x0020  /* write-dma active */

#define	SND_F_PENDING_IN	(SND_F_READING | SND_F_RD_DMA)
#define	SND_F_PENDING_OUT	(SND_F_WRITING | SND_F_WR_DMA)
#define	SND_F_PENDING_IO	(SND_F_PENDING_IN | SND_F_PENDING_OUT)

    /*
     * flag used to mark a pending close.
     */
#define SND_F_CLOSING           0x0040  /* a pending close */

    /*
     * if user has not set block size, then make it adaptive
     * (0.25s, or the perhaps last read/write ?)
     */
#define	SND_F_HAS_SIZE		0x0080	/* user set block size */
    /*
     * assorted flags related to operating mode.
     */
#define SND_F_STEREO            0x0100	/* doing stereo */
#define SND_F_NBIO              0x0200	/* do non-blocking i/o */

    /*
     * the user requested ulaw, but the board does not support it
     * natively, so a (software) format conversion is necessary.
     * The kernel is not really the place to do this, but since
     * many applications expect to use /dev/audio , we do it for
     * portability.
     */
#define SND_F_XLAT8             0x0400  /* u-law <--> 8-bit unsigned */
#define SND_F_XLAT16            0x0800  /* u-law <--> 16-bit signed */

    /*
     * these flags mark a pending abort on a r/w operation.
     */
#define SND_F_ABORTING          0x1000  /* a pending abort */

    /*
     * this is used to mark that board initialization is needed, e.g.
     * because of a change in sampling rate, format, etc. -- It will
     * be done at the next convenient time.
     */
#define SND_F_INIT              0x4000  /* changed parameters. need init */
#define SND_F_AUTO_DMA          0x8000  /* use auto-dma */

    u_long  bd_flags;       /* board-specific flags */
    int     play_speed, rec_speed;

    int     play_blocksize, rec_blocksize;  /* blocksize for io and dma ops */
    u_long  play_fmt, rec_fmt ;      /* current audio format */

    /*
     * mixer parameters
     */
    u_long  mix_devs;	/* existing devices for mixer */
    u_long  mix_rec_devs;	/* possible recording sources */
    u_long  mix_recsrc;	/* current recording source(s) */
    u_short mix_levels[32];

    struct selinfo wsel, rsel, esel ;
    u_long	interrupts;	/* counter of interrupts */
    void    *device_data ;	/* just in case it is needed...*/
} ;

/*
 * then ioctls and other stuff
 */

#define NPCM_MAX	8	/* Number of supported devices */

/*
 * Supported card ID numbers (were in soundcard.h...)
 */

#define SNDCARD_ADLIB		1
#define SNDCARD_SB		2
#define SNDCARD_PAS		3
#define SNDCARD_GUS		4
#define SNDCARD_MPU401		5
#define SNDCARD_SB16		6
#define SNDCARD_SB16MIDI	7
#define SNDCARD_UART6850	8
#define SNDCARD_GUS16		9
#define SNDCARD_MSS		10
#define SNDCARD_PSS     	11
#define SNDCARD_SSCAPE		12
#define SNDCARD_PSS_MPU     	13
#define SNDCARD_PSS_MSS     	14
#define SNDCARD_SSCAPE_MSS	15
#define SNDCARD_TRXPRO		16
#define SNDCARD_TRXPRO_SB	17
#define SNDCARD_TRXPRO_MPU	18
#define SNDCARD_MAD16		19
#define SNDCARD_MAD16_MPU	20
#define SNDCARD_CS4232		21
#define SNDCARD_CS4232_MPU	22
#define SNDCARD_MAUI		23
#define SNDCARD_PSEUDO_MSS	24	/* MSS without WSS regs.*/
#define SNDCARD_AWE32           25

/*
 * values used in bd_id for the mss boards
 */
#define MD_AD1848	0x91
#define MD_AD1845	0x92
#define MD_CS4248	0xA1
#define MD_CS4231	0xA2
#define MD_CS4231A	0xA3
#define MD_CS4232	0xA4
#define MD_CS4232A	0xA5
#define MD_CS4236	0xA6
#define	MD_OPTI931	0xB1

/*
 * TODO: add some card classes rather than specific types.
 */
#include <i386/isa/snd/soundcard.h>

/*
 * many variables should be reduced to a range. Here define a macro
 */

#define RANGE(var, low, high) (var) = \
	((var)<(low)?(low) : (var)>(high)?(high) : (var))

/*
 * finally, all default parameters
 */
#define DSP_BUFFSIZE 65536 /* XXX */

#if 1 		/* prepare for pnp support! */
#include "pnp.h"
#if NPNP > 0
#include <i386/isa/pnp.h>	/* XXX pnp support */
#endif
#endif

/*
 * Minor numbers for the sound driver.
 *
 * Unfortunately Creative called the codec chip of SB as a DSP. For this
 * reason the /dev/dsp is reserved for digitized audio use. There is a
 * device for true DSP processors but it will be called something else.
 * In v3.0 it's /dev/sndproc but this could be a temporary solution.
 */


#define SND_DEV_CTL	0	/* Control port /dev/mixer */
#define SND_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define SND_DEV_MIDIN	2	/* Raw midi access */
#define SND_DEV_DSP	3	/* Digitized voice /dev/dsp */
#define SND_DEV_AUDIO	4	/* Sparc compatible /dev/audio */
#define SND_DEV_DSP16	5	/* Like /dev/dsp but 16 bits/sample */
#define SND_DEV_STATUS	6	/* /dev/sndstat */
	/* #7 not in use now. Was in 2.4. Free for use after v3.0. */
#define SND_DEV_SEQ2	8	/* /dev/sequencer, level 2 interface */
#define SND_DEV_SNDPROC 9	/* /dev/sndproc for programmable devices */
#define SND_DEV_PSS	SND_DEV_SNDPROC

#define DSP_DEFAULT_SPEED	8000

#define ON		1
#define OFF		0


#define SYNTH_MAX_VOICES	32

struct voice_alloc_info {
	int max_voice;
	int used_voices;
	int ptr;		/* For device specific use */
	u_short map[SYNTH_MAX_VOICES]; /* (ch << 8) | (note+1) */
	int timestamp;
	int alloc_times[SYNTH_MAX_VOICES];
};

struct channel_info {
	int pgm_num;
	int bender_value;
	u_char controllers[128];
};

/*
 * mixer description structure and macros
 */

struct mixer_def {
    u_int    regno:7;
    u_int    polarity:1;	/* 1 means reversed */
    u_int    bitoffs:4;
    u_int    nbits:4;
};
typedef struct mixer_def mixer_ent;
typedef struct mixer_def mixer_tab[32][2];

#define MIX_ENT(name, reg_l, pol_l, pos_l, len_l, reg_r, pol_r, pos_r, len_r) \
    {{reg_l, pol_l, pos_l, len_l}, {reg_r, pol_r, pos_r, len_r}}
#define PMIX_ENT(name, reg_l, pos_l, len_l, reg_r, pos_r, len_r) \
    {{reg_l, 0, pos_l, len_l}, {reg_r, 0, pos_r, len_r}}

#define DDB(x)	x	/* XXX */

#ifndef DEB
#define DEB(x)
#endif
#ifndef DDB
#define DDB(x)
#endif

extern snddev_info pcm_info[NPCM_MAX] ;
extern snddev_info midi_info[NPCM_MAX] ;
extern snddev_info synth_info[NPCM_MAX] ;

extern u_long nsnd ;
extern snddev_info *snddev_last_probed;

int pcmprobe(struct isa_device * dev);
int midiprobe(struct isa_device * dev);
int synthprobe(struct isa_device * dev);
int pcmattach(struct isa_device * dev);
int midiattach(struct isa_device * dev);
int synthattach(struct isa_device * dev);

/*
 *      DMA buffer calls
 */

void dsp_wrintr(snddev_info *d);
void dsp_rdintr(snddev_info *d);
int dsp_write_body(snddev_info *d, struct uio *buf);
int dsp_read_body(snddev_info *d, struct uio *buf);
void alloc_dbuf(snd_dbuf *d, int size, int b16);
void reset_dbuf(snd_dbuf *b);
int snd_flush(snddev_info *d);
int snd_sync(snddev_info *d, int chan, int threshold);
int dsp_wrabort(snddev_info *d);
int dsp_rdabort(snddev_info *d);
void dsp_wr_dmaupdate(snddev_info *d);
void dsp_rd_dmaupdate(snddev_info *d);

d_select_t sndselect;

/*
 * library functions (in sound.c)
 */

int ask_init(snddev_info *d);
void translate_bytes(u_char *table, u_char *buff, int n);
void change_bits(mixer_tab *t, u_char *regval, int dev, int chn, int newval);
int snd_conflict(int io_base);
void snd_set_blocksize(snddev_info *d);
int isa_dmastatus1(int channel);
/*
 * routines in ad1848.c and sb_dsp.c which others might use
 */
int mss_detect (struct isa_device *dev);
int sb_cmd (int io_base, u_char cmd);
int sb_cmd2 (int io_base, u_char cmd, int val);
int sb_cmd3 (int io_base, u_char cmd, int val);
int sb_reset_dsp (int io_base);
void sb_setmixer (int io_base, u_int port, u_int value);
int sb_getmixer (int io_base, u_int port);


/*
 * usage of flags in device config entry (config file)
 */

#define DV_F_DRQ_MASK	0x00000007	/* mask for secondary drq */
#define	DV_F_DUAL_DMA	0x00000010	/* set to use secondary dma channel */
#define	DV_F_DEV_MASK	0x0000ff00	/* force device type/class */
#define	DV_F_DEV_SHIFT	8	/* force device type/class */

/*
 * some flags are used in a device-specific manner, so that values can
 * be used multiple times.
 */

#define	DV_F_TRUE_MSS	0x00010000	/* mss _with_ base regs */
    /* almost all modern cards do not have this set of registers,
     * so it is better to make this the default behaviour
     */

#endif
