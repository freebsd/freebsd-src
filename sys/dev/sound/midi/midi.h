/*
 * Include file for midi driver.
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

#ifndef _MIDI_H_
#define _MIDI_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioccom.h>

#include <sys/filio.h>
#include <sys/lock.h>
#include <sys/sockio.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/proc.h>

#include <sys/kernel.h> /* for DATA_SET */

#include <sys/module.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/clock.h>	/* for DELAY */
#include <machine/resource.h>
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>	/* for DELAY */
#include <sys/soundcard.h>
#include <sys/rman.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/mutex.h>

#include <dev/sound/midi/miditypes.h>
#include <dev/sound/midi/midibuf.h>
#include <dev/sound/midi/midisynth.h>

#define MIDI_CDEV_MAJOR 30

/*#define MIDI_OUTOFGIANT*/

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
 * descriptor of midi operations ...
 *
 */

struct _mididev_info {

	/*
	 * the first part of the descriptor is filled up from a
	 * template.
	 */
	char name[64];

	int type;

	d_open_t *open;
	d_close_t *close;
	d_ioctl_t *ioctl;
	midi_callback_t *callback;

	/*
	 * combinations of the following flags are used as second argument in
	 * the callback from the dma module to the device-specific routines.
	 */

#define MIDI_CB_RD       0x100   /* read callback */
#define MIDI_CB_WR       0x200   /* write callback */
#define MIDI_CB_REASON_MASK      0xff
#define MIDI_CB_START    0x01   /* start dma op */
#define MIDI_CB_STOP     0x03   /* stop dma op */
#define MIDI_CB_ABORT    0x04   /* abort dma op */
#define MIDI_CB_INIT     0x05   /* init board parameters */

	/*
	 * callback extensions
	 */
#define MIDI_CB_DMADONE         0x10
#define MIDI_CB_DMAUPDATE       0x11
#define MIDI_CB_DMASTOP         0x12

	/* init can only be called with int enabled and
	 * no pending DMA activity.
	 */

	/*
	 * whereas from here, parameters are set at runtime.
	 * resources are stored in the softc of the device,
	 * not in the common structure.
	 */

	int unit; /* unit number of the device */
	void *softc; /* softc for the device */
	device_t dev; /* device_t for the device */

	int bd_id ;     /* used to hold board-id info, eg. sb version,
			 * mss codec type, etc. etc.
			 */

	struct mtx flagqueue_mtx; /* Mutex to protect flags and queues */

	/* Queues */
	midi_dbuf midi_dbuf_in; /* midi input event/message queue */
	midi_dbuf midi_dbuf_out; /* midi output event/message queue */
	midi_dbuf midi_dbuf_passthru; /* midi passthru event/message queue */

        /*
         * these parameters describe the operation of the board.
         * Generic things like busy flag, speed, etc are here.
         */

	/* Flags */
	volatile u_long  flags ;     /* 32 bits, used for various purposes. */
	int fflags; /* file flag */

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
#define MIDI_F_BUSY              0x0001  /* has been opened 	*/
	/*
	 * the next two are used to allow only one pending operation of
	 * each type.
	 */
#define MIDI_F_READING           0x0004  /* have a pending read */
#define MIDI_F_WRITING           0x0008  /* have a pending write */

	/*
	 * flag used to mark a pending close.
	 */
#define MIDI_F_CLOSING           0x0040  /* a pending close */

	/*
	 * if user has not set block size, then make it adaptive
	 * (0.25s, or the perhaps last read/write ?)
	 */
#define	MIDI_F_HAS_SIZE		0x0080	/* user set block size */
	/*
	 * assorted flags related to operating mode.
	 */
#define MIDI_F_STEREO            0x0100	/* doing stereo */
#define MIDI_F_NBIO              0x0200	/* do non-blocking i/o */
#define MIDI_F_PASSTHRU          0x0400 /* pass received data to output port */

	/*
	 * these flags mark a pending abort on a r/w operation.
	 */
#define MIDI_F_ABORTING          0x1000  /* a pending abort */

	/*
	 * this is used to mark that board initialization is needed, e.g.
	 * because of a change in sampling rate, format, etc. -- It will
	 * be done at the next convenient time.
	 */
#define MIDI_F_INIT              0x4000  /* changed parameters. need init */

	int     play_blocksize, rec_blocksize;  /* blocksize for io and dma ops */

#define mwsel midi_dbuf_out.sel
#define mrsel midi_dbuf_in.sel
	u_long	interrupts;	/* counter of interrupts */
	u_long	magic;
#define	MAGIC(unit) ( 0xa4d10de0 + unit )
	void    *device_data ;	/* just in case it is needed...*/

	midi_intr_t	*intr;	/* interrupt handler of the upper layer (ie sequencer) */
	void		*intrarg;	/* argument to interrupt handler */

	/* The following is the interface from a midi sequencer to a midi device. */
	synthdev_info synth;

	/* This is the status message to display via /dev/midistat */
	char midistat[128];

	/* The tailq entry of the next midi device. */
	TAILQ_ENTRY(_mididev_info) md_link;

	/* The tailq entry of the next midi device opened by a sequencer. */
	TAILQ_ENTRY(_mididev_info) md_linkseq;
} ;

/*
 * then ioctls and other stuff
 */

#define NMIDI_MAX	16	/* Number of supported devices */

/*
 * many variables should be reduced to a range. Here define a macro
 */

#define RANGE(var, low, high) (var) = \
((var)<(low)?(low) : (var)>(high)?(high) : (var))

/*
 * convert dev_t to unit and dev
 */
#define MIDIMINOR(x)       (minor(x))
#define MIDIUNIT(x)        ((MIDIMINOR(x) & 0x000000f0) >> 4)
#define MIDIDEV(x)         (MIDIMINOR(x) & 0x0000000f)
#define MIDIMKMINOR(u, d)  (((u) & 0x0f) << 4 | ((d) & 0x0f))
#define MIDIMKDEV(m, u, d) (makedev((m), MIDIMKMINOR((u), (d))))

/*
 * see if the device is configured
 */
#define MIDICONFED(x) ((x)->ioctl != NULL)

/*
 * finally, all default parameters
 */
#define MIDI_BUFFSIZE (1024) /* XXX */

/*
 * some macros for debugging purposes
 * DDB/DEB to enable/disable debugging stuff
 * BVDDB   to enable debugging when bootverbose
 */
#define DDB(x)	x	/* XXX */
#define BVDDB(x) if (bootverbose) x

#ifndef DEB
#define DEB(x)
#endif

/* This is the generic midi drvier initializer. */
	int midiinit(mididev_info *d, device_t dev);

/* This provides an access to the mididev_info. */
	mididev_info *get_mididev_info(dev_t i_dev, int *unit);
	mididev_info *get_mididev_info_unit(int unit);
	mididev_info *create_mididev_info_unit(int type, mididev_info *mdinf, synthdev_info *syninf);
	int mididev_info_number(void);
#define MDT_MIDI	(0)
#define MDT_SYNTH	(1)

/* These are the generic methods for a midi driver. */
	d_open_t midi_open;
	d_close_t midi_close;
	d_ioctl_t midi_ioctl;
	d_read_t midi_read;
	d_write_t midi_write;
	d_poll_t midi_poll;

/* Common interrupt handler */
void midi_intr(mididev_info *);

/* Sync output */
int midi_sync(mididev_info *);

/*
 * Minor numbers for the midi driver.
 */

#define MIDI_DEV_MIDIN	2	/* Raw midi access */
#define MIDI_DEV_STATUS	15	/* /dev/midistat */

#endif	/* _MIDI_H_ */
