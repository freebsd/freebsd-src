/*
 * The sequencer personality manager.
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
 * $FreeBSD$
 *
 */

/*
 * This is the newmidi sequencer driver. This driver handles io against
 * /dev/sequencer, midi input and output event queues and event transmittion
 * to and from a midi device or synthesizer.
 */

#include <dev/sound/midi/midi.h>
#include <dev/sound/midi/sequencer.h>

#ifndef DDB
#define DDB(x)
#endif /* DDB */

#define SND_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define SND_DEV_MIDIN	2	/* Raw midi access */
#define SND_DEV_SEQ2	8	/* /dev/sequencer, level 2 interface */

#define MIDIDEV_MODE 0x2000

/* Length of a sequencer event. */
#define EV_SZ 8
#define IEV_SZ 8

/* Return value from seq_playevent and the helpers. */
enum {
	MORE,
	TIMERARMED,
	QUEUEFULL
};

/*
 * These functions goes into seq_op_desc to get called
 * from sound.c.
 */

static midi_intr_t seq_intr;
static seq_callback_t seq_callback;

/* These are the entries to the sequencer driver. */
static d_open_t seq_open;
static d_close_t seq_close;
static d_ioctl_t seq_ioctl;
static d_read_t seq_read;
static d_write_t seq_write;
static d_poll_t seq_poll;

/*
 * This is the device descriptor for the midi sequencer.
 */
seqdev_info seq_op_desc = {
	"midi sequencer",

	0,

	seq_open,
	seq_close,
	seq_read,
	seq_write,
	seq_ioctl,
	seq_poll,

	seq_callback,

	SEQ_BUFFSIZE, /* Queue Length */

	0, /* XXX This is not an *audio* device! */
};

/* Here is the parameter structure per a device. */
struct seq_softc {
	seqdev_info *devinfo; /* sequencer device information */

	int fflags; /* Access mode */

	u_long seq_time; /* The beggining time of this sequence */
	u_long prev_event_time; /* The time of the previous event output */
	u_long prev_input_time; /* The time of the previous event input */
	u_long prev_wakeup_time; /* The time of the previous wakeup */
	struct callout_handle timeout_ch; /* Timer callout handler */
	long timer_current; /* Current timer value */
	int timer_running; /* State of timer */
	int midi_open[NMIDI_MAX]; /* State of midi devices. */
	int pending_timer; /* Timer change operation */
	int output_threshould; /* Sequence output threshould */
	int pre_event_timeout; /* Time to wait event input */
	int queueout_pending; /* Pending for the output queue */
	snd_sync_parm sync_parm; /* AIOSYNC parameter set */
	struct proc *sync_proc; /* AIOSYNCing process */
};

typedef struct seq_softc *sc_p;

static d_open_t seqopen;
static d_close_t seqclose;
static d_ioctl_t seqioctl;
static d_read_t seqread;
static d_write_t seqwrite;
static d_poll_t seqpoll;

#define CDEV_MAJOR SEQ_CDEV_MAJOR
static struct cdevsw seq_cdevsw = {
	/* open */	seqopen,
	/* close */	seqclose,
	/* read */	seqread,
	/* write */	seqwrite,
	/* ioctl */	seqioctl,
	/* poll */	seqpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"midi", /* XXX */
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

seqdev_info seq_info[NSEQ_MAX] ;
static int seq_info_inited;
u_long nseq = NSEQ_MAX;	/* total number of sequencers */

/* The followings are the local function. */
static int seq_init(void);
static int seq_queue(sc_p scp, u_char *note);
static void seq_startplay(sc_p scp);
static int seq_playevent(sc_p scp, u_char *event);
static u_long seq_gettime(void);
static int seq_requesttimer(sc_p scp, int delay);
static void seq_stoptimer(sc_p scp);
static void seq_midiinput(sc_p scp, mididev_info *md);
static int seq_copytoinput(sc_p scp, u_char *event, int len);
static int seq_extended(sc_p scp, u_char *event);
static int seq_chnvoice(sc_p scp, u_char *event);
static int seq_findvoice(mididev_info *md, int chn, int note);
static int seq_allocvoice(mididev_info *md, int chn, int note);
static int seq_chncommon(sc_p scp, u_char *event);
static int seq_timing(sc_p scp, u_char *event);
static int seq_local(sc_p scp, u_char *event);
static int seq_sysex(sc_p scp, u_char *event);
static void seq_timer(void *arg);
static int seq_reset(sc_p scp);
static int seq_openmidi(sc_p scp, mididev_info *md, int flags, int mode, struct proc *p);
static int seq_closemidi(sc_p scp, mididev_info *md, int flags, int mode, struct proc *p);
static void seq_panic(sc_p scp);
static int seq_sync(sc_p scp);

static seqdev_info * get_seqdev_info(dev_t i_dev, int *unit);

/*
 * Here are the main functions to interact to the user process.
 * These are called from snd* functions in sys/i386/isa/snd/sound.c.
 */

static int
seq_init(void)
{
	int unit;
	sc_p scp;
	seqdev_info *devinfo;

	DEB(printf("seq: initing.\n"));

	/* Have we already inited? */
	if (seq_info_inited)
		return (1);

	for (unit = 0 ; unit < nseq ; unit++) {
		/* Allocate the softc. */
		scp = malloc(sizeof(*scp), M_DEVBUF, M_NOWAIT);
		if (scp == (sc_p)NULL) {
			printf("seq%d: softc allocation failed.\n", unit);
			return (1);
		}
		bzero(scp, sizeof(*scp));

		/* Fill the softc and the seq_info for this unit. */
		scp->seq_time = seq_gettime();
		scp->prev_event_time = 0;
		scp->prev_input_time = 0;
		scp->prev_wakeup_time = scp->seq_time;
		callout_handle_init(&scp->timeout_ch);
		scp->timer_current = 0;
		scp->timer_running = 0;
		scp->queueout_pending = 0;

		scp->devinfo = devinfo = &seq_info[unit];
		bcopy(&seq_op_desc, devinfo, sizeof(seq_op_desc));
		devinfo->unit = unit;
		devinfo->softc = scp;
		devinfo->flags = 0;
		devinfo->midi_dbuf_in.unit_size = devinfo->midi_dbuf_out.unit_size = EV_SZ;
		midibuf_init(&devinfo->midi_dbuf_in);
		midibuf_init(&devinfo->midi_dbuf_out);

		make_dev(&seq_cdevsw, MIDIMKMINOR(unit, SND_DEV_SEQ),
			 UID_ROOT, GID_WHEEL, 0666, "sequencer%d", unit);
	}

	/* We have inited. */
	seq_info_inited = 1;

	if (nseq == 1)
		printf("seq0: Midi sequencer.\n");
	else
		printf("seq0-%lu: Midi sequencers.\n", nseq - 1);

	DEB(printf("seq: inited.\n"));

	return (0);
}

int
seq_open(dev_t i_dev, int flags, int mode, struct proc *p)
{
	int unit, s, midiunit;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;

	unit = MIDIUNIT(i_dev);

	DEB(printf("seq%d: opening.\n", unit));

	if (unit >= NSEQ_MAX) {
		DEB(printf("seq_open: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		DEB(printf("seq_open: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	s = splmidi();
	/* Mark this device busy. */
	if ((sd->flags & SEQ_F_BUSY) != 0) {
		splx(s);
		DEB(printf("seq_open: unit %d is busy.\n", unit));
		return (EBUSY);
	}
	sd->flags |= SEQ_F_BUSY;
	sd->flags &= ~(SEQ_F_READING | SEQ_F_WRITING);
	scp->fflags = flags;

	/* Init the queue. */
	midibuf_init(&sd->midi_dbuf_in);
	midibuf_init(&sd->midi_dbuf_out);

	/* Init timestamp. */
	scp->seq_time = seq_gettime();
	scp->prev_event_time = 0;
	scp->prev_input_time = 0;
	scp->prev_wakeup_time = scp->seq_time;

	splx(s);

	/* Open midi devices. */
	for (midiunit = 0 ; midiunit < nmidi + nsynth ; midiunit++) {
		md = &midi_info[midiunit];
		if (MIDICONFED(md))
			seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, p);
	}

	DEB(printf("seq%d: opened.\n", unit));

	return (0);
}

int
seq_close(dev_t i_dev, int flags, int mode, struct proc *p)
{
	int unit, s, i;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;

	unit = MIDIUNIT(i_dev);

	DEB(printf("seq%d: closing.\n", unit));

	if (unit >= NSEQ_MAX) {
		DEB(printf("seq_close: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		DEB(printf("seq_close: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	s = splmidi();

	if (!(sd->flags & MIDI_F_NBIO))
		seq_sync(scp);

	/* Stop the timer. */
	seq_stoptimer(scp);

	/* Reset the sequencer. */
	seq_reset(scp);
	seq_sync(scp);

	/* Clean up the midi device. */
	for (i = 0 ; i < nmidi + nsynth ; i++) {
		md = &midi_info[i];
		if (MIDICONFED(md))
			seq_closemidi(scp, md, scp->fflags, MIDIDEV_MODE, p);
	}

	/* Stop playing and unmark this device busy. */
	sd->flags &= ~(SEQ_F_BUSY | SEQ_F_READING | SEQ_F_WRITING);

	splx(s);

	DEB(printf("seq%d: closed.\n", unit));

	return (0);
}

int
seq_read(dev_t i_dev, struct uio *buf, int flag)
{
	int unit, ret, s, len;
	sc_p scp;
	seqdev_info *sd;

	unit = MIDIUNIT(i_dev);

	/*DEB(printf("seq%d: reading.\n", unit));*/

	if (unit >= NSEQ_MAX) {
		DEB(printf("seq_read: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		DEB(printf("seq_read: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;
	if ((scp->fflags & FREAD) == 0) {
		DEB(printf("seq_read: unit %d is not for reading.\n", unit));
		return (EIO);
	}

	s = splmidi();

	/* Begin recording. */
	sd->callback(sd, SEQ_CB_START | SEQ_CB_RD);

	/* Have we got the data to read? */
	if ((sd->flags & SEQ_F_NBIO) != 0 && sd->midi_dbuf_in.rl == 0)
		ret = EAGAIN;
	else {
		len = buf->uio_resid;
		ret = midibuf_uioread(&sd->midi_dbuf_in, buf, len);
		if (ret < 0)
			ret = -ret;
		else
			ret = 0;
	}
	splx(s);

	return (ret);
}

int
seq_write(dev_t i_dev, struct uio *buf, int flag)
{
	u_char event[EV_SZ], ev_code;
	int unit, count, countorg, midiunit, ev_size, p, ret, s;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;

	unit = MIDIUNIT(i_dev);

	/*DEB(printf("seq%d: writing.\n", unit));*/

	if (unit >= NSEQ_MAX) {
		DEB(printf("seq_write: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		DEB(printf("seq_write: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;
	if ((scp->fflags & FWRITE) == 0) {
		DEB(printf("seq_write: unit %d is not for writing.\n", unit));
		return (EIO);
	}

	p = 0;
	countorg = buf->uio_resid;
	count = countorg;

	s = splmidi();
	/* Begin playing. */
	sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
	splx(s);

	/* Pick up an event. */
	while (count >= 4) {
		if (uiomove((caddr_t)event, 4, buf))
			printf("seq_write: user memory mangled?\n");
		ev_code = event[0];

		/* Have a look at the event code. */
		if (ev_code == SEQ_FULLSIZE) {

			/* A long event, these are the patches/samples for a synthesizer. */
			midiunit = *(u_short *)&event[2];
			if (midiunit < 0 || midiunit >= nmidi + nsynth)
				return (ENXIO);
			md = &midi_info[midiunit];
			if (!MIDICONFED(md))
				return (ENXIO);
			s = splmidi();
			if ((md->flags & MIDI_F_BUSY) == 0
			    && seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) != 0) {
				splx(s);
				return (ENXIO);
			}

			DEB(printf("seq_write: loading a patch to the unit %d.\n", midiunit));

			ret = md->synth.loadpatch(md, *(short *)&event[0], buf, p + 4, count, 0);
			splx(s);
			return (ret);
		}

		if (ev_code >= 128) {

			/* Some sort of an extended event. The size is eight bytes. */
#if notyet
			if (scp->seq_mode == SEQ_2 && ev_code == SEQ_EXTENDED) {
				printf("seq%d: invalid level two event %x.\n", unit, ev_code);
				return (EINVAL);
			}
#endif /* notyet */
			ev_size = 8;

			if (count < ev_size) {
				/* No more data. Start playing now. */
				s = splmidi();
				sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
				splx(s);

				return (0);
			}
			if (uiomove((caddr_t)&event[4], 4, buf))
				printf("seq_write: user memory mangled?\n");
		} else {

			/* Not an extended event. The size is four bytes. */
#if notyet
			if (scp->seq_mode == SEQ_2) {
				printf("seq%d: four byte event in level two mode.\n", unit);
				return (EINVAL);
			}
#endif /* notyet */
			ev_size = 4;
		}
		if (ev_code == SEQ_MIDIPUTC) {
			/* An event passed to the midi device itself. */
			midiunit = event[2];
			if (midiunit < 0 || midiunit >= nmidi + nsynth)
				return (ENXIO);
			md = &midi_info[midiunit];
			if (!MIDICONFED(md))
				return (ENXIO);
			if ((md->flags & MIDI_F_BUSY) == 0
			    && (ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc)) != 0) {
				seq_reset(scp);
				return (ret);
			}
		}

		/*DEB(printf("seq_write: queueing event %d.\n", event[0]));*/
		/* Now we queue the event. */
		switch (seq_queue(scp, event)) {
		case EAGAIN:
			s = splmidi();
			/* The queue is full. Start playing now. */
			sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
			splx(s);
			return (0);
		case EINTR:
			return (EINTR);
		case ERESTART:
			return (ERESTART);
		}
		p += ev_size;
		count -= ev_size;
	}

	/* We have written every single data. Start playing now. */
	s = splmidi();
	sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
	splx(s);

	return (0);
}

int
seq_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc *p)
{
	int unit, midiunit, ret, tmp, s, arg2;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;
	struct patmgr_info *patinfo;
	snd_sync_parm *syncparm;
	struct seq_event_rec *event;
	struct snd_size *sndsize;

	unit = MIDIUNIT(i_dev);

	DEB(printf("seq%d: ioctlling, cmd 0x%x.\n", unit, (int)cmd));

	if (unit >= NSEQ_MAX) {
		DEB(printf("seq_ioctl: unit %d does not exist.\n", unit));
		return (ENXIO);
	}
	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		DEB(printf("seq_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	ret = 0;

	switch (cmd) {

		/*
		 * we start with the new ioctl interface.
		 */
	case AIONWRITE:	/* how many bytes can be written ? */
		*(int *)arg = sd->midi_dbuf_out.fl;
		break;

	case AIOSSIZE:     /* set the current blocksize */
		sndsize = (struct snd_size *)arg;
		if (sndsize->play_size <= sd->midi_dbuf_out.unit_size && sndsize->rec_size <= sd->midi_dbuf_in.unit_size) {
			sd->flags &= ~MIDI_F_HAS_SIZE;
			sd->midi_dbuf_out.blocksize = sd->midi_dbuf_out.unit_size;
			sd->midi_dbuf_in.blocksize = sd->midi_dbuf_in.unit_size;
		}
		else {
			if (sndsize->play_size > sd->midi_dbuf_out.bufsize / 4)
				sndsize->play_size = sd->midi_dbuf_out.bufsize / 4;
			if (sndsize->rec_size > sd->midi_dbuf_in.bufsize / 4)
				sndsize->rec_size = sd->midi_dbuf_in.bufsize / 4;
			/* Round up the size to the multiple of EV_SZ. */
			sd->midi_dbuf_out.blocksize =
			    ((sndsize->play_size + sd->midi_dbuf_out.unit_size - 1)
			     / sd->midi_dbuf_out.unit_size) * sd->midi_dbuf_out.unit_size;
			sd->midi_dbuf_in.blocksize =
			    ((sndsize->rec_size + sd->midi_dbuf_in.unit_size - 1)
			     / sd->midi_dbuf_in.unit_size) * sd->midi_dbuf_in.unit_size;
			sd->flags |= MIDI_F_HAS_SIZE;
		}
		/* FALLTHROUGH */
	case AIOGSIZE:	/* get the current blocksize */
		sndsize = (struct snd_size *)arg;
		sndsize->play_size = sd->midi_dbuf_out.blocksize;
		sndsize->rec_size = sd->midi_dbuf_in.blocksize;

		ret = 0;
		break;

	case AIOSTOP:
		if (*(int *)arg == AIOSYNC_PLAY) {
			s = splmidi();

			/* Stop writing. */
			sd->callback(sd, SEQ_CB_ABORT | SEQ_CB_WR);

			/* Pass the ioctl to the midi devices. */
			for (midiunit = 0 ; midiunit < nmidi + nsynth ; midiunit++) {
				md = &midi_info[midiunit];
				if (MIDICONFED(md) && scp->midi_open[midiunit] && (md->flags & MIDI_F_WRITING) != 0) {
					arg2 = *(int *)arg;
					midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, (caddr_t)&arg2, mode, p);
				}
			}

			*(int *)arg = sd->midi_dbuf_out.rl;
			splx(s);
		}
		else if (*(int *)arg == AIOSYNC_CAPTURE) {
			s = splmidi();

			/* Stop reading. */
			sd->callback(sd, SEQ_CB_ABORT | SEQ_CB_RD);

			/* Pass the ioctl to the midi devices. */
			for (midiunit = 0 ; midiunit < nmidi + nsynth ; midiunit++) {
				md = &midi_info[midiunit];
				if (MIDICONFED(md) && scp->midi_open[midiunit] && (md->flags & MIDI_F_WRITING) != 0) {
					arg2 = *(int *)arg;
					midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, (caddr_t)&arg2, mode, p);
				}
			}

			*(int *)arg = sd->midi_dbuf_in.rl;
			splx(s);
		}

		ret = 0;
		break;

	case AIOSYNC:
		syncparm = (snd_sync_parm *)arg;
		scp->sync_parm = *syncparm;

		/* XXX Should select(2) against us watch the blocksize, or sync_parm? */

		ret = 0;
		break;

	case SNDCTL_TMR_TIMEBASE:
	case SNDCTL_TMR_TEMPO:
	case SNDCTL_TMR_START:
	case SNDCTL_TMR_STOP:
	case SNDCTL_TMR_CONTINUE:
	case SNDCTL_TMR_METRONOME:
	case SNDCTL_TMR_SOURCE:
#if notyet
		if (scp->seq_mode != SEQ_2) {
			ret = EINVAL;
			break;
		}
		ret = tmr->ioctl(tmr_no, cmd, arg);
#endif /* notyet */
		break;
	case SNDCTL_TMR_SELECT:
#if notyet
		if (scp->seq_mode != SEQ_2) {
			ret = EINVAL;
			break;
		}
#endif /* notyet */
		scp->pending_timer = *(int *)arg;
		if (scp->pending_timer < 0 || scp->pending_timer >= /*NTIMER*/1) {
			scp->pending_timer = -1;
			ret = EINVAL;
			break;
		}
		*(int *)arg = scp->pending_timer;
		ret = 0;
		break;
	case SNDCTL_SEQ_PANIC:
		seq_panic(scp);
		ret = 0;
		break;
	case SNDCTL_SEQ_SYNC:
		if (mode == O_RDONLY) {
			ret = 0;
			break;
		}
		ret = seq_sync(scp);
		break;
	case SNDCTL_SEQ_RESET:
		seq_reset(scp);
		ret = 0;
		break;
	case SNDCTL_SEQ_TESTMIDI:
		midiunit = *(int *)arg;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		md = &midi_info[midiunit];
		if (MIDICONFED(md) && !scp->midi_open[midiunit]) {
			ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
			break;
		}
		ret = 0;
		break;
	case SNDCTL_SEQ_GETINCOUNT:
		if (mode == O_WRONLY)
			*(int *)arg = 0;
		else
			*(int *)arg = sd->midi_dbuf_in.rl;
		ret = 0;
		break;
	case SNDCTL_SEQ_GETOUTCOUNT:
		if (mode == O_RDONLY)
			*(int *)arg = 0;
		else
			*(int *)arg = sd->midi_dbuf_out.fl;
		ret = 0;
		break;
	case SNDCTL_SEQ_CTRLRATE:
#if notyet
		if (scp->seq_mode != SEQ_2) {
			ret = tmr->ioctl(tmr_no, cmd, arg);
			break;
		}
#endif /* notyet */
		if (*(int *)arg != 0) {
			ret = EINVAL;
			break;
		}
		*(int *)arg = hz;
		ret = 0;
		break;
	case SNDCTL_SEQ_RESETSAMPLES:
		midiunit = *(int *)arg;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		if (!scp->midi_open[midiunit]) {
			md = &midi_info[midiunit];
			if (MIDICONFED(md)) {
				ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
				if (ret != 0)
					break;
			} else {
				ret = EBUSY;
				break;
			}
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_SEQ_NRSYNTHS:
		*(int *)arg = nmidi + nsynth;
		ret = 0;
		break;
	case SNDCTL_SEQ_NRMIDIS:
		*(int *)arg = nmidi + nsynth;
		ret = 0;
		break;
	case SNDCTL_SYNTH_MEMAVL:
		midiunit = *(int *)arg;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		if (!scp->midi_open[midiunit]) {
			md = &midi_info[midiunit];
			if (MIDICONFED(md)) {
				ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
				if (ret != 0)
					break;
			} else {
				ret = EBUSY;
				break;
			}
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_FM_4OP_ENABLE:
		midiunit = *(int *)arg;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		if (!scp->midi_open[midiunit]) {
			md = &midi_info[midiunit];
			if (MIDICONFED(md)) {
				ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
				if (ret != 0)
					break;
			} else {
				ret = EBUSY;
				break;
			}
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		midiunit = synthinfo->device;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_SEQ_OUTOFBAND:
		event = (struct seq_event_rec *)arg;
		s = splmidi();
		ret = seq_playevent(scp, event->arr);
		splx(s);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		midiunit = midiinfo->device;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_PMGR_IFACE:
		patinfo = (struct patmgr_info *)arg;
		midiunit = patinfo->device;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		if (!scp->midi_open[midiunit]) {
			ret = EBUSY;
			break;
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_PMGR_ACCESS:
		patinfo = (struct patmgr_info *)arg;
		midiunit = patinfo->device;
		if (midiunit >= nmidi + nsynth) {
			ret = ENXIO;
			break;
		}
		if (!scp->midi_open[midiunit]) {
			md = &midi_info[midiunit];
			if (MIDICONFED(md)) {
				ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
				if (ret != 0)
					break;
			} else {
				ret = EBUSY;
				break;
			}
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	case SNDCTL_SEQ_THRESHOLD:
		tmp = *(int *)arg;
		RANGE(tmp, 1, sd->midi_dbuf_out.bufsize - 1);
		scp->output_threshould = tmp;
		ret = 0;
		break;
	case SNDCTL_MIDI_PRETIME:
		tmp = *(int *)arg;
		if (tmp < 0)
			tmp = 0;
		tmp = (hz * tmp) / 10;
		scp->pre_event_timeout = tmp;
		ret = 0;
		break;
	default:
		if (scp->fflags == O_RDONLY) {
			ret = EIO;
			break;
		}
		if (!scp->midi_open[0]) {
			md = &midi_info[0];
			if (MIDICONFED(md)) {
				ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
				if (ret != 0)
					break;
			} else {
				ret = EBUSY;
				break;
			}
		}
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), 0, SND_DEV_MIDIN), cmd, arg, mode, p);
		break;
	}

	return (ret);
}

int
seq_poll(dev_t i_dev, int events, struct proc *p)
{
	int unit, ret, s, lim;
	sc_p scp;
	seqdev_info *sd;

	unit = MIDIUNIT(i_dev);

	DEB(printf("seq%d: polling.\n", unit));

	if (unit >= NSEQ_MAX) {
		DEB(printf("seq_poll: unit %d does not exist.\n", unit));
		return (ENXIO);
	}
	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		DEB(printf("seq_poll: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	ret = 0;
	s = splmidi();

	/* Look up the apropriate queue and select it. */
	if ((events & (POLLOUT | POLLWRNORM)) != 0) {
		/* Start playing. */
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);

		/* Find out the boundary. */
		if ((sd->flags & SEQ_F_HAS_SIZE) != 0)
			lim = sd->midi_dbuf_out.blocksize;
		else
			lim = sd->midi_dbuf_out.unit_size;
		if (sd->midi_dbuf_out.fl < lim)
			/* No enough space, record select. */
			selrecord(p, &sd->midi_dbuf_out.sel);
		else
			/* We can write now. */
			ret |= events & (POLLOUT | POLLWRNORM);
	}
	if ((events & (POLLIN | POLLRDNORM)) != 0) {
		/* Start recording. */
		sd->callback(sd, SEQ_CB_START | SEQ_CB_RD);

		/* Find out the boundary. */
		if ((sd->flags & SEQ_F_HAS_SIZE) != 0)
			lim = sd->midi_dbuf_in.blocksize;
		else
			lim = sd->midi_dbuf_in.unit_size;
		if (sd->midi_dbuf_in.rl < lim)
			/* No data ready, record select. */
			selrecord(p, &sd->midi_dbuf_in.sel);
		else
			/* We can write now. */
			ret |= events & (POLLIN | POLLRDNORM);
	}
	splx(s);

	return (ret);
}

static void
seq_intr(void *p, mididev_info *md)
{
	sc_p scp;
	seqdev_info *sd;

	sd = (seqdev_info *)p;
	scp = sd->softc;

	/* Restart playing if we have the data to output. */
	if (scp->queueout_pending)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
	/* Check the midi device if we are reading. */
	if ((sd->flags & SEQ_F_READING) != 0)
		seq_midiinput(scp, md);
}

static int
seq_callback(seqdev_info *sd, int reason)
{
	int unit;
	sc_p scp;

	/*DEB(printf("seq_callback: reason 0x%x.\n", reason));*/

	if (sd == NULL) {
		DEB(printf("seq_callback: device not configured.\n"));
		return (ENXIO);
	}
	scp = sd->softc;
	unit = sd->unit;

	switch (reason & SEQ_CB_REASON_MASK) {
	case SEQ_CB_START:
		if ((reason & SEQ_CB_RD) != 0 && (sd->flags & SEQ_F_READING) == 0)
			/* Begin recording. */
			sd->flags |= SEQ_F_READING;
		if ((reason & SEQ_CB_WR) != 0 && (sd->flags & SEQ_F_WRITING) == 0)
			/* Start playing. */
			seq_startplay(scp);
		break;
	case SEQ_CB_STOP:
	case SEQ_CB_ABORT:
		if ((reason & SEQ_CB_RD) != 0 && (sd->flags & SEQ_F_READING) != 0) {
			/* Stop the timer. */
			scp->seq_time = seq_gettime();
			scp->prev_input_time = 0;

			/* Stop recording. */
			sd->flags &= ~SEQ_F_READING;
		}
		if ((reason & SEQ_CB_WR) != 0 && (sd->flags & SEQ_F_WRITING) != 0) {
			/* Stop the timer. */
			seq_stoptimer(scp);
			scp->seq_time = seq_gettime();
			scp->prev_event_time = 0;

			/* Stop Playing. */
			sd->flags &= ~SEQ_F_WRITING;
			scp->queueout_pending = 0;
		}
		break;
	}

	return (0);
}

/*
 * The functions below here are the libraries for the above ones.
 */

static int
seq_queue(sc_p scp, u_char *note)
{
	int unit, err, s;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	/*DEB(printf("seq%d: queueing.\n", unit));*/

	s = splmidi();

	/* Start playing if we have some data in the queue. */
	if (sd->midi_dbuf_out.rl >= EV_SZ)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);

	if (sd->midi_dbuf_out.fl < EV_SZ) {
		/* We have no space. Start playing if not yet. */
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
		if ((sd->flags & SEQ_F_NBIO) != 0 && sd->midi_dbuf_out.fl < EV_SZ) {
			/* We would block. */
			splx(s);
			return (EAGAIN);
		} else
			while (sd->midi_dbuf_out.fl < EV_SZ) {
				/* We have no space. Good night. */
				err = tsleep(&sd->midi_dbuf_out.tsleep_out, PRIBIO | PCATCH, "seqque", 0);
				if (err == EINTR)
					sd->callback(sd, SEQ_CB_STOP | SEQ_CB_WR);
				if (err == EINTR || err == ERESTART) {
					splx(s);
					return (err);
				}
			}
	}

	/* We now have enough space to write. */
	err = midibuf_seqwrite(&sd->midi_dbuf_out, note, EV_SZ);

	splx(s);

	if (err < 0)
		err = -err;
	else
		err = 0;

	return (err);
}

static void
seq_startplay(sc_p scp)
{
	int unit;
	u_char event[EV_SZ];
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	/* Dequeue the events to play. */
	while (sd->midi_dbuf_out.rl >= EV_SZ) {

		/* We are playing now. */
		sd->flags |= SEQ_F_WRITING;

		/* We only copy the event, not dequeue. */
		midibuf_seqcopy(&sd->midi_dbuf_out, event, EV_SZ);

		switch (seq_playevent(scp, event)) {
		case TIMERARMED:
			/* Dequeue the event. */
			midibuf_seqread(&sd->midi_dbuf_out, event, EV_SZ);
			/* FALLTHRU */
		case QUEUEFULL:
			/* We cannot play further. */
			return;
		case MORE:
			/* Dequeue the event. */
			midibuf_seqread(&sd->midi_dbuf_out, event, EV_SZ);
			break;
		}
	}

	/* Played every event in the queue. */
	sd->flags &= ~SEQ_F_WRITING;
}

static int
seq_playevent(sc_p scp, u_char *event)
{
	int unit, ret;
	long *delay;
	seqdev_info *sd;
	mididev_info *md;

	sd = scp->devinfo;
	unit = sd->unit;

	md = &midi_info[0];
	if (!MIDICONFED(md))
		return (MORE);

	switch(event[0]) {
	case SEQ_NOTEOFF:
		if ((md->flags & MIDI_F_BUSY) != 0 || seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) == 0)
			if (md->synth.killnote(md, event[1], 255, event[3]) == EAGAIN) {
				ret = QUEUEFULL;
				break;
			}
		ret = MORE;
		break;
	case SEQ_NOTEON:
		if (((md->flags & MIDI_F_BUSY) != 0 || seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) == 0)
		    && (event[4] < 128 || event[4] == 255))
			if (md->synth.startnote(md, event[1], event[2], event[3]) == EAGAIN) {
				ret = QUEUEFULL;
				break;
			}
		ret = MORE;
		break;
	case SEQ_WAIT:

		/* Extract the delay. */
		delay = (long *)event;
		*delay = (*delay >> 8) & 0xffffff;
		if (*delay > 0) {
			/* Arm the timer. */
			sd->flags |= SEQ_F_WRITING;
			scp->prev_event_time = *delay;
			if (seq_requesttimer(scp, *delay)) {
				ret = TIMERARMED;
				break;
			}
		}
		ret = MORE;
		break;
	case SEQ_PGMCHANGE:
		if ((md->flags & MIDI_F_BUSY) != 0 || seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) == 0)
			if (md->synth.setinstr(md, event[1], event[2]) == EAGAIN) {
				ret = QUEUEFULL;
				break;
			}
		ret = MORE;
		break;
	case SEQ_SYNCTIMER:
		/* Reset the timer. */
		scp->seq_time = seq_gettime();
		scp->prev_input_time = 0;
		scp->prev_event_time = 0;
		scp->prev_wakeup_time = scp->seq_time;
		ret = MORE;
		break;
	case SEQ_MIDIPUTC:
		/* Pass through to the midi device. */
		if (event[2] < nmidi + nsynth) {
			md = &midi_info[event[2]];
			if (MIDICONFED(md) && ((md->flags & MIDI_F_BUSY) != 0 || seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) == 0)) {
				if (md->synth.writeraw(md, &event[1], sizeof(event[1]), 1) == EAGAIN) {
					/* The queue was full. Try again later. */
					ret = QUEUEFULL;
					break;
				}
			}
		}
		ret = MORE;
		break;
	case SEQ_ECHO:
		/* Echo this event back. */
		if (seq_copytoinput(scp, event, 4) == EAGAIN) {
			ret = QUEUEFULL;
			break;
		}
		ret = MORE;
		break;
	case SEQ_PRIVATE:
		if (event[1] < nmidi + nsynth) {
			md = &midi_info[event[1]];
			if (MIDICONFED(md) && md->synth.hwcontrol(md, event) == EAGAIN) {
				ret = QUEUEFULL;
				break;
			}
		}
		ret = MORE;
		break;
	case SEQ_EXTENDED:
		ret = seq_extended(scp, event);
		break;
	case EV_CHN_VOICE:
		ret = seq_chnvoice(scp, event);
		break;
	case EV_CHN_COMMON:
		ret = seq_chncommon(scp, event);
		break;
	case EV_TIMING:
		ret = seq_timing(scp, event);
		break;
	case EV_SEQ_LOCAL:
		ret = seq_local(scp, event);
		break;
	case EV_SYSEX:
		ret = seq_sysex(scp, event);
		break;
	default:
		ret = MORE;
		break;
	}

	switch (ret) {
	case QUEUEFULL:
		/*DEB(printf("seq_playevent: the queue is full.\n"));*/
		/* The queue was full. Try again on the interrupt by the midi device. */
		sd->flags |= SEQ_F_WRITING;
		scp->queueout_pending = 1;
		break;
	case TIMERARMED:
		sd->flags |= SEQ_F_WRITING;
		/* FALLTHRU */
	case MORE:
		scp->queueout_pending = 0;
		break;
	}

	return (ret);
}

static u_long
seq_gettime(void)
{
	struct timeval  timecopy;

	getmicrotime(&timecopy);
	return timecopy.tv_usec / (1000000 / hz) + (u_long) timecopy.tv_sec * hz;
}

static int
seq_requesttimer(sc_p scp, int delay)
{
	u_long cur_time, rel_base;

	/*DEB(printf("seq%d: requested timer at delay of %d.\n", unit, delay));*/

	cur_time = seq_gettime();

	if (delay < 0)
		/* Request a new timer. */
		delay = -delay;
	else {
		rel_base = cur_time - scp->seq_time;
		if (delay <= rel_base)
			return 0;
		delay -= rel_base;
	}

#if notdef
	/*
	 * Compensate the delay of midi message transmission.
	 * XXX Do we have to consider the accumulation of errors
	 * less than 1/hz second?
	 */
	delay -= (cur_time - scp->prev_wakeup_time);
	if (delay < 1) {
		printf("sequencer: prev = %lu, cur = %lu, delay = %d, skip sleeping.\n",
			scp->prev_wakeup_time, cur_time, delay);
		seq_stoptimer(scp);
		return 0;
	}
#endif /* notdef */

	scp->timeout_ch = timeout(seq_timer, (void *)scp, delay);
	scp->timer_running = 1;
	return 1;
}

static void
seq_stoptimer(sc_p scp)
{
	/*DEB(printf("seq%d: stopping timer.\n", unit));*/

	if (scp->timer_running) {
		untimeout(seq_timer, (void *)scp, scp->timeout_ch);
		scp->timer_running = 0;
	}
}

static void
seq_midiinput(sc_p scp, mididev_info *md)
{
	int unit, midiunit;
	u_long tstamp;
	u_char event[4];
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	/* Can this midi device interrupt for input? */
	midiunit = md->unit;
	if (scp->midi_open[midiunit]
	    && (md->flags & MIDI_F_READING) != 0
	    && md->intrarg == sd)
		/* Read the input data. */
		while (md->synth.readraw(md, &event[1], sizeof(event[1]), 1) == 0) {
			tstamp = seq_gettime() - scp->seq_time;
			if (tstamp != scp->prev_input_time) {
				/* Insert a wait between events. */
				tstamp = (tstamp << 8) | SEQ_WAIT;
				seq_copytoinput(scp, (u_char *)&tstamp, 4);
				scp->prev_input_time = tstamp;
			}
			event[0] = SEQ_MIDIPUTC;
			event[2] = midiunit;
			event[3] = 0;
			seq_copytoinput(scp, event, sizeof(event));
		}
}

static int
seq_copytoinput(sc_p scp, u_char *event, int len)
{
	seqdev_info *sd;

	sd = scp->devinfo;

	if (midibuf_input_intr(&sd->midi_dbuf_in, event, len) == -EAGAIN)
		return (EAGAIN);

	return (0);
}

static int
seq_extended(sc_p scp, u_char *event)
{
	int unit;
	seqdev_info *sd;
	mididev_info *md;

	sd = scp->devinfo;
	unit = sd->unit;

	if (event[2] >= nmidi + nsynth)
		return (MORE);
	md = &midi_info[event[2]];
	if (!MIDICONFED(md) && (md->flags & MIDI_F_BUSY) == 0 && seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) != 0)
		return (MORE);

	switch (event[1]) {
	case SEQ_NOTEOFF:
		if (md->synth.killnote(md, event[3], event[4], event[5]) == EAGAIN)
			return (QUEUEFULL);
		break;
	case SEQ_NOTEON:
		if (event[4] < 128 || event[4] == 255)
			if (md->synth.startnote(md, event[3], event[4], event[5]) == EAGAIN)
				return (QUEUEFULL);
		break;
	case SEQ_PGMCHANGE:
		if (md->synth.setinstr(md, event[3], event[4]) == EAGAIN)
			return (QUEUEFULL);
		break;
	case SEQ_AFTERTOUCH:
		if (md->synth.aftertouch(md, event[3], event[4]) == EAGAIN)
			return (QUEUEFULL);
		break;
	case SEQ_BALANCE:
		if (md->synth.panning(md, event[3], (char)event[4]) == EAGAIN)
			return (QUEUEFULL);
		break;
	case SEQ_CONTROLLER:
		if (md->synth.controller(md, event[3], event[4], *(short *)&event[5]) == EAGAIN)
			return (QUEUEFULL);
		break;
	case SEQ_VOLMODE:
		if (md->synth.volumemethod != NULL)
			if (md->synth.volumemethod(md, event[3]) == EAGAIN)
				return (QUEUEFULL);
		break;
	}

	return (MORE);
}

static int
seq_chnvoice(sc_p scp, u_char *event)
{
	int voice;
	seqdev_info *sd;
	mididev_info *md;
	u_char dev, cmd, chn, note, parm;

	voice = -1;
	dev = event[1];
	cmd = event[2];
	chn = event[3];
	note = event[4];
	parm = event[5];

	sd = scp->devinfo;

	if (dev >= nmidi + nsynth)
		return (MORE);
	md = &midi_info[dev];
	if (!MIDICONFED(md) && (md->flags & MIDI_F_BUSY) == 0 && seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) != 0)
		return (MORE);

#if notyet
	if (scp->seq_mode == SEQ_2) {
		if (md->synth.allocvoice)
			voice = seq_allocvoice(md, chn, note);
	}
#endif /* notyet */
	switch (cmd) {
	case MIDI_NOTEON:
		if (note < 128 || note == 255) {
#if notyet
			if (voice == -1 && scp->seq_mode == SEQ_2 && md->synth.allocvoice)
				/* This is an internal synthesizer. (FM, GUS, etc) */
				if ((voice = seq_allocvoice(md, chn, note)) == -EAGAIN)
					return (QUEUEFULL);
#endif /* notyet */
			if (voice == -1)
				voice = chn;

#if notyet
			if (scp->seq_mode == SEQ_2 && dev < nmidi + nsynth && chn == 9) {
				/* This channel is a percussion. The note number is the patch number. */
				if (md->synth.setinstr(md, voice, 128 + note) == EAGAIN)
					return (QUEUEFULL);
				note = 60; /* Middle C. */
			}
			if (scp->seq_mode == SEQ_2)
				if (md->synth.setupvoice(md, voice, chn) == EAGAIN)
					return (QUEUEFULL);
#endif /* notyet */
			if (md->synth.startnote(md, voice, note, parm) == EAGAIN)
				return (QUEUEFULL);
		}
		break;
	case MIDI_NOTEOFF:
		if (voice == -1)
			voice = chn;
		if (md->synth.killnote(md, voice, note, parm) == EAGAIN)
			return (QUEUEFULL);
		break;
	case MIDI_KEY_PRESSURE:
		if (voice == -1)
			voice = chn;
		if (md->synth.aftertouch(md, voice, parm) == EAGAIN)
			return (QUEUEFULL);
		break;
	}

	return (MORE);
}

static int
seq_findvoice(mididev_info *md, int chn, int note)
{
	int i;
	u_short key;

	key = (chn << 8) | (note + 1);

	for (i = 0 ; i < md->synth.alloc.max_voice ; i++)
		if (md->synth.alloc.map[i] == key)
			return (i);

	return (-1);
}

static int
seq_allocvoice(mididev_info *md, int chn, int note)
{
	int voice;
	u_short key;

	key = (chn << 8) | (note + 1);

	if ((voice = md->synth.allocvoice(md, chn, note, &md->synth.alloc)) == -EAGAIN)
		return (-EAGAIN);
	md->synth.alloc.map[voice] = key;
	md->synth.alloc.alloc_times[voice] = md->synth.alloc.timestamp++;

	return (voice);
}

static int
seq_chncommon(sc_p scp, u_char *event)
{
	int unit/*, i, val, key*/;
	u_short w14;
	u_char dev, cmd, chn, p1;
	seqdev_info *sd;
	mididev_info *md;

	dev = event[1];
	cmd = event[2];
	chn = event[3];
	p1 = event[4];
	w14 = *(u_short *)&event[6];

	sd = scp->devinfo;
	unit = sd->unit;

	if (dev >= nmidi + nsynth)
		return (MORE);
	md = &midi_info[dev];
	if (!MIDICONFED(md) && (md->flags & MIDI_F_BUSY) == 0 && seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) != 0)
		return (MORE);

	switch (cmd) {
	case MIDI_PGM_CHANGE:
#if notyet
		if (scp->seq_mode == SEQ_2) {
			md->synth.chn_info[chn].pgm_num = p1;
			if (dev < nmidi + nsynth)
				if (md->synth.setinstr(md, chn, p1) == EAGAIN)
					return (QUEUEFULL);
		} else
#endif /* notyet */
			/* For Mode 1. */
			if (md->synth.setinstr(md, chn, p1) == EAGAIN)
				return (QUEUEFULL);
		break;
	case MIDI_CTL_CHANGE:
#if notyet
		if (scp->seq_mode == SEQ_2) {
			if (chn < 16 && p1 < 128) {
				md->synth.chn_info[chn].controllers[p1] = w14 & 0x7f;
				if (p1 < 32)
					/* We have set the MSB, clear the LSB. */
					md->synth.chn_info[chn].controllers[p1 + 32] = 0;
				if (dev < nmidi + nsynth) {
					val = w14 & 0x7f;
					if (p1 < 64) {
						/* Combine the MSB and the LSB. */
						val = ((md->synth.chn_info[chn].controllers[p1 & ~32] & 0x7f) << 7)
						    | (md->synth.chn_info[chn].controllers[p1 | 32] & 0x7f);
						p1 &= ~32;
					}
					/* Handle all of the notes playing on this channel. */
					key = ((int)chn << 8);
					for (i = 0 ; i < md->synth.alloc.max_voice ; i++)
						if ((md->synth.alloc.map[i] & 0xff00) == key)
							if (md->synth.controller(md, i, p1, val) == EAGAIN)
								return (QUEUEFULL);
				} else
					if (md->synth.controller(md, chn, p1, w14) == EAGAIN)
						return (QUEUEFULL);
			}
		} else
#endif /* notyet */
			/* For Mode 1. */
			if (md->synth.controller(md, chn, p1, w14) == EAGAIN)
				return (QUEUEFULL);
		break;
	case MIDI_PITCH_BEND:
#if notyet
		if (scp->seq_mode == SEQ_2) {
			md->synth.chn_info[chn].bender_value = w14;
			if (dev < nmidi + nsynth) {
				/* Handle all of the notes playing on this channel. */
				key = ((int)chn << 8);
				for (i = 0 ; i < md->synth.alloc.max_voice ; i++)
					if ((md->synth.alloc.map[i] & 0xff00) == key)
						if (md->synth.bender(md, i, w14) == EAGAIN)
							return (QUEUEFULL);
			} else
				if (md->synth.bender(md, chn, w14) == EAGAIN)
					return (QUEUEFULL);
		} else
#endif /* notyet */
			/* For Mode 1. */
			if (md->synth.bender(md, chn, w14) == EAGAIN)
				return (QUEUEFULL);
		break;
	}

	return (MORE);
}

static int
seq_timing(sc_p scp, u_char *event)
{
	int unit/*, ret*/;
	long parm;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	parm = *(long *)&event[4];

#if notyet
	if (scp->seq_mode == SEQ_2 && (ret = tmr->event(tmr_no, event)) == TIMERARMED)
		return (ret);
#endif /* notyet */
	switch (event[1]) {
	case TMR_WAIT_REL:
		parm += scp->prev_event_time;
		/* FALLTHRU */
	case TMR_WAIT_ABS:
		if (parm > 0) {
			sd->flags |= SEQ_F_WRITING;
			scp->prev_event_time = parm;
			if (seq_requesttimer(scp, parm))
				return (TIMERARMED);
		}
		break;
	case TMR_START:
		scp->seq_time = seq_gettime();
		scp->prev_input_time = 0;
		scp->prev_event_time = 0;
		scp->prev_wakeup_time = scp->seq_time;
		break;
	case TMR_STOP:
		break;
	case TMR_CONTINUE:
		break;
	case TMR_TEMPO:
		break;
	case TMR_ECHO:
#if notyet
		if (scp->seq_mode == SEQ_2)
			seq_copytoinput(scp, event, 8);
		else {
#endif /* notyet */
			parm = (parm << 8 | SEQ_ECHO);
			seq_copytoinput(scp, (u_char *)&parm, 4);
#if notyet
		}
#endif /* notyet */
		break;
	}

	return (MORE);
}

static int
seq_local(sc_p scp, u_char *event)
{
	int unit;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	switch (event[1]) {
	case LOCL_STARTAUDIO:
#if notyet
		DMAbuf_start_devices(*(u_int *)&event[4]);
#endif /* notyet */
		break;
	}

	return (MORE);
}

static int
seq_sysex(sc_p scp, u_char *event)
{
	int unit, i, l;
	seqdev_info *sd;
	mididev_info *md;

	sd = scp->devinfo;
	unit = sd->unit;

	if (event[1] >= nmidi + nsynth)
		return (MORE);
	md = &midi_info[event[1]];
	if (!MIDICONFED(md) || md->synth.sendsysex == NULL
	    || ((md->flags & MIDI_F_BUSY) == 0 && seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc) != 0))
		return (MORE);

	l = 0;
	for (i = 0 ; i < 6 && event[i + 2] != 0xff ; i++)
		l = i + 1;
	if (l > 0)
		if (md->synth.sendsysex(md, &event[2], l) == EAGAIN)
			return (QUEUEFULL);

	return (MORE);
}

static void
seq_timer(void *arg)
{
	sc_p scp;

	scp = arg;

	/*DEB(printf("seq_timer: timer fired.\n"));*/

	/* Record the current timestamp. */
	scp->prev_wakeup_time = seq_gettime();

	seq_startplay(scp);
}

static int
seq_openmidi(sc_p scp, mididev_info *md, int flags, int mode, struct proc *p)
{
	int midiunit, s, err;

	if (md == NULL || !MIDICONFED(md)) {
		DEB(printf("seq_openmidi: midi device does not exist.\n"));
		return (ENXIO);
	}
	midiunit = md->unit;

	DEB(printf("seq_openmidi: opening midi unit %d.\n", midiunit));

	if (!scp->midi_open[midiunit]) {
		err = midi_open(MIDIMKDEV(MIDI_CDEV_MAJOR, midiunit, SND_DEV_MIDIN), flags, mode, p);
		if (err != 0) {
			printf("seq_openmidi: failed to open midi device %d.\n", midiunit);
			return (err);
		}
		s = splmidi();
		scp->midi_open[midiunit] = 1;
		md->intr = seq_intr;
		md->intrarg = scp->devinfo;
		md->synth.prev_out_status = 0;
		md->synth.sysex_state = 0;
		splx(s);
	}

	return (0);
}

static int
seq_closemidi(sc_p scp, mididev_info *md, int flags, int mode, struct proc *p)
{
	int midiunit, s;

	if (md == NULL || !MIDICONFED(md)) {
		DEB(printf("seq_closemidi: midi device does not exist.\n"));
		return (ENXIO);
	}
	midiunit = md->unit;

	DEB(printf("seq_closemidi: closing midi unit %d.\n", midiunit));

	if (scp->midi_open[midiunit]) {
		midi_close(MIDIMKDEV(MIDI_CDEV_MAJOR, midiunit, SND_DEV_MIDIN), flags, mode, p);
		s = splmidi();
		scp->midi_open[midiunit] = 0;
		md->intr = NULL;
		md->intrarg = NULL;
		splx(s);
	}

	return (0);
}

static void
seq_panic(sc_p scp)
{
	seq_reset(scp);
}

static int
seq_reset(sc_p scp)
{
	int unit, i, s, chn;
	seqdev_info *sd;
	mididev_info *md;
	u_char c[3];

	sd = scp->devinfo;
	unit = sd->unit;

	s = splmidi();

	/* Stop reading and writing. */
	sd->callback(sd, SEQ_CB_ABORT | SEQ_CB_RD | SEQ_CB_WR);

	/* Clear the queues. */
	midibuf_init(&sd->midi_dbuf_in);
	midibuf_init(&sd->midi_dbuf_out);

#if notyet
	/* Reset the synthesizers. */
	for (i = 0 ; i < nmidi + nsynth ; i++) {
		md = &midi_info[i];
		if (MIDICONFED(md) && scp->midi_open[i])
			md->synth.reset(md);
	}
#endif /* notyet */

#if notyet
	if (scp->seq_mode == SEQ_2) {
		for (chn = 0 ; chn < 16 ; chn++)
			for (i = 0 ; i < nmidi + nsynth ; i++)
				if (midi_open[i]) {
					md = &midi_info[i];
					if (!MIDICONFED(md))
						continue;
					if (md->synth.controller(md, chn, 123, 0) == EAGAIN /* All notes off. */
					    || md->synth.controller(md, chn, 121, 0) == EAGAIN /* Reset all controllers. */
					    || md->synth.bender(md, chn, 1 << 13) == EAGAIN) { /* Reset pitch bend. */
						splx(s);
						return (EAGAIN);
					}
				}
		splx(s);
	} else {
#endif /* notyet */
		splx(s);
		for (i = 0 ; i < nmidi + nsynth ; i++) {
			md = &midi_info[i];
			if (!MIDICONFED(md))
				continue;

			/* Send active sensing. */
			c[0] = 0xfe; /* Active Sensing. */
			if (md->synth.writeraw(md, c, 1, 0) == EAGAIN)
				return (EAGAIN);
			/*
			 * We need a sleep to reset a midi device using an active sensing.
			 * SC-88 resets after 420ms...
			 */
			tsleep(md, PRIBIO, "seqrst", 500 * hz / 1000);
			for (chn = 0 ; chn < 16 ; chn++) {
				c[0] = 0xb0 | (chn & 0x0f);
				c[1] = (u_char)0x78; /* All sound off */
				c[2] = (u_char)0;
				if (md->synth.writeraw(md, c, 3, 0) == EAGAIN)
					return (EAGAIN);
				c[1] = (u_char)0x7b; /* All note off */
				if (md->synth.writeraw(md, c, 3, 0) == EAGAIN)
					return (EAGAIN);
				c[1] = (u_char)0x79; /* Reset all controller */
				if (md->synth.writeraw(md, c, 3, 0) == EAGAIN)
					return (EAGAIN);
			}
		}
		for (i = 0 ; i < nmidi + nsynth ; i++){
			md = &midi_info[i];
			if (MIDICONFED(md))
				seq_closemidi(scp, md, scp->fflags, MIDIDEV_MODE, curproc);
		}
#if notyet
	}
#endif /* notyet */

	return (0);
}

static int
seq_sync(sc_p scp)
{
	int unit, s, i;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	s = splmidi();

	if (sd->midi_dbuf_out.rl >= EV_SZ)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);

	while ((sd->flags & SEQ_F_WRITING) != 0 && sd->midi_dbuf_out.rl >= EV_SZ) {
		i = tsleep(&sd->midi_dbuf_out.tsleep_out, PRIBIO | PCATCH, "seqsnc", 0);
		if (i == EINTR)
			sd->callback(sd, SEQ_CB_STOP | SEQ_CB_WR);
		if (i == EINTR || i == ERESTART) {
			splx(s);
			return (i);
		}
	}
	splx(s);

	return (0);
}

/*
 * a small utility function which, given a device number, returns
 * a pointer to the associated seqdev_info struct, and sets the unit
 * number.
 */
static seqdev_info *
get_seqdev_info(dev_t i_dev, int *unit)
{
	int u;
	seqdev_info *d = NULL ;

	if (MIDIDEV(i_dev) != SND_DEV_SEQ && MIDIDEV(i_dev) != SND_DEV_SEQ2)
		return NULL;
	u = MIDIUNIT(i_dev);
	if (unit)
		*unit = u ;

	if (u >= NSEQ_MAX) {
		DEB(printf("get_seqdev_info: unit %d is not configured.\n", u));
		return NULL;
	}
	d = &seq_info[u];

	return d ;
}

/* XXX These functions are actually redundant. */
static int
seqopen(dev_t i_dev, int flags, int mode, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
		return seq_open(i_dev, flags, mode, p);
	}

	return (ENXIO);
}

static int
seqclose(dev_t i_dev, int flags, int mode, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
		return seq_close(i_dev, flags, mode, p);
	}

	return (ENXIO);
}

static int
seqread(dev_t i_dev, struct uio * buf, int flag)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
		return seq_read(i_dev, buf, flag);
	}

	return (ENXIO);
}

static int
seqwrite(dev_t i_dev, struct uio * buf, int flag)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
		return seq_write(i_dev, buf, flag);
	}

	return (ENXIO);
}

static int
seqioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
		return seq_ioctl(i_dev, cmd, arg, mode, p);
	}

	return (ENXIO);
}

static int
seqpoll(dev_t i_dev, int events, struct proc * p)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
		return seq_poll(i_dev, events, p);
	}

	return (ENXIO);
}

static int
seq_modevent(module_t mod, int type, void *data)
{
	int retval;

	retval = 0;

	switch (type) {
	case MOD_LOAD:
		seq_init();
		break;

	case MOD_UNLOAD:
		printf("sequencer: unload not supported yet.\n");
		retval = EOPNOTSUPP;
		break;

	default:
		break;
	}

	return retval;
}

DEV_MODULE(seq, seq_modevent, NULL);
