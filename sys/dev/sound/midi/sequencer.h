/*
 * Include file for midi sequencer driver.
 * 
 * Copyright by Seigo Tanimura 1999.
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
 * $FreeBSD$
 *
 */

/*
 * first, include kernel header files.
 */

#ifndef _SEQUENCER_H_
#define _SEQUENCER_H_

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
#include <sys/condvar.h>
#include <machine/clock.h>	/* for DELAY */
#include <sys/soundcard.h>

#include <dev/sound/midi/timer.h>

#define SEQ_CDEV_MAJOR MIDI_CDEV_MAJOR

/*
 * the following assumes that FreeBSD 3.X uses poll(2) instead of select(2).
 * This change dates to late 1997.
 */
#include <sys/poll.h>
#define d_select_t d_poll_t

/* Return value from seq_playevent and timer event handers. */
enum {
	MORE,
	TIMERARMED,
	QUEUEFULL
};

typedef struct _seqdev_info seqdev_info;

/*
 * The order of mutex lock (from the first to the last)
 *
 * 1. sequencer flags, queues, timer and device list
 * 2. midi synth voice and channel
 * 3. midi synth status
 * 4. generic midi flags and queues
 * 5. midi device
 */

/*
 * descriptor of sequencer operations ...
 *
 */

struct _seqdev_info {

	/*
	 * the first part of the descriptor is filled up from a
	 * template.
	 */
	char name[64];

	int type ;

	d_open_t *open;
	d_close_t *close;
	d_read_t *read;
	d_write_t *write;
	d_ioctl_t *ioctl;
	d_poll_t *poll;
	midi_callback_t *callback;

	/*
	 * combinations of the following flags are used as second argument in
	 * the callback from the dma module to the device-specific routines.
	 */

#define SEQ_CB_RD       0x100   /* read callback */
#define SEQ_CB_WR       0x200   /* write callback */
#define SEQ_CB_REASON_MASK      0xff
#define SEQ_CB_START    0x01   /* start dma op */
#define SEQ_CB_STOP     0x03   /* stop dma op */
#define SEQ_CB_ABORT    0x04   /* abort dma op */
#define SEQ_CB_INIT     0x05   /* init board parameters */

	/*
	 * callback extensions
	 */
#define SEQ_CB_DMADONE         0x10
#define SEQ_CB_DMAUPDATE       0x11
#define SEQ_CB_DMASTOP         0x12

	/* init can only be called with int enabled and
	 * no pending DMA activity.
	 */

	/*
	 * whereas from here, parameters are set at runtime.
	 * io_base == 0 means that the board is not configured.
	 */

	int unit; /* unit number of the device */
	void *softc; /* softc for a device */

	int bd_id ;     /* used to hold board-id info, eg. sb version,
			 * mss codec type, etc. etc.
			 */

	struct mtx flagqueue_mtx; /* Mutex to protect flags and queues */
	struct cv insync_cv; /* Conditional variable for sync */

	/* Queues */
	midi_dbuf midi_dbuf_in; /* midi input event/message queue */
	midi_dbuf midi_dbuf_out; /* midi output event/message queue */
	

        /*
         * these parameters describe the operation of the board.
         * Generic things like busy flag, speed, etc are here.
         */

	/* Flags */
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
#define SEQ_F_BUSY              0x0001  /* has been opened 	*/
	/*
	 * the next two are used to allow only one pending operation of
	 * each type.
	 */
#define SEQ_F_READING           0x0004  /* have a pending read */
#define SEQ_F_WRITING           0x0008  /* have a pending write */

	/*
	 * flag used to mark a pending close.
	 */
#define SEQ_F_CLOSING           0x0040  /* a pending close */

	/*
	 * if user has not set block size, then make it adaptive
	 * (0.25s, or the perhaps last read/write ?)
	 */
#define	SEQ_F_HAS_SIZE		0x0080	/* user set block size */
	/*
	 * assorted flags related to operating mode.
	 */
#define SEQ_F_STEREO            0x0100	/* doing stereo */
#define SEQ_F_NBIO              0x0200	/* do non-blocking i/o */

	/*
	 * these flags mark a pending abort on a r/w operation.
	 */
#define SEQ_F_ABORTING          0x1000  /* a pending abort */

	/*
	 * this is used to mark that board initialization is needed, e.g.
	 * because of a change in sampling rate, format, etc. -- It will
	 * be done at the next convenient time.
	 */
#define SEQ_F_INIT              0x4000  /* changed parameters. need init */

#define SEQ_F_INSYNC            0x8000  /* a pending sync */

	int     play_blocksize, rec_blocksize;  /* blocksize for io and dma ops */

#define swsel midi_dbuf_out.sel
#define srsel midi_dbuf_in.sel
	u_long	interrupts;	/* counter of interrupts */
	u_long	magic;
#define	MAGIC(unit) ( 0xa4d10de0 + unit )
	void    *device_data ;	/* just in case it is needed...*/

	/* The tailq entry of the next sequencer device. */
	TAILQ_ENTRY(_seqdev_info) sd_link;
};


/*
 * then ioctls and other stuff
 */
#define NSEQ_MAX	16	/* Number of supported devices */

/*
 * many variables should be reduced to a range. Here define a macro
 */

#define RANGE(var, low, high) (var) = \
((var)<(low)?(low) : (var)>(high)?(high) : (var))

/*
 * finally, all default parameters
 */
#define SEQ_BUFFSIZE (1024) /* XXX */

#define MIDI_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define MIDI_DEV_MUSIC	8	/* Sequencer output /dev/music (FM
				   synthesizer and MIDI output) */

#ifdef _KERNEL

extern midi_cmdtab	cmdtab_seqioctl[];
extern midi_cmdtab	cmdtab_timer[];

void	seq_timer(void *arg);
int	seq_copytoinput(void *arg, u_char *event, int len);

SYSCTL_DECL(_hw_midi_seq);

extern int	seq_debug;
#define SEQ_DEBUG(x)			\
	do {				\
		if (seq_debug) {	\
			(x);		\
		}			\
	} while(0)

#endif /* _KERNEL */


#endif /* _SEQUENCER_H_ */
