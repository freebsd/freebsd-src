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

#define SND_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define SND_DEV_MIDIN	2	/* Raw midi access */
#define SND_DEV_MUSIC	8	/* /dev/music, level 2 interface */

#define MIDIDEV_MODE 0x2000

/* Length of a sequencer event. */
#define EV_SZ 8
#define IEV_SZ 8

/* Lookup modes */
#define LOOKUP_EXIST	(0)
#define LOOKUP_OPEN	(1)
#define LOOKUP_CLOSE	(2)

/*
 * These functions goes into seq_op_desc to get called
 * from sound.c.
 */

static midi_intr_t seq_intr;
static midi_callback_t seq_callback;

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
	seqdev_info	*devinfo;		/* sequencer device information */

	/* Flags (protected by flag_mtx of mididev_info) */
	int		fflags;			/* Access mode */
	int		queueout_pending;	/* Pending for the output queue */
	int		seq_mode;		/* Sequencer mode */

	/* Timer counters */
	u_long		seq_time;		/* The beggining time of this sequence */
	u_long		prev_event_time;	/* The time of the previous event output */
	u_long		prev_input_time;	/* The time of the previous event input */
	u_long		prev_wakeup_time;	/* The time of the previous wakeup */
	struct callout	timeout_ch;		/* Timer callout handler */
	long		timer_current;		/* Current timer value */
	int		timer_running;		/* State of timer */
	int		pending_timer;		/* Timer change operation */
	int		pre_event_timeout;	/* Time to wait event input */

	/* Devices */
	TAILQ_HEAD(,_mididev_info)	midi_open;	/* Midi devices opened by this sequencer. */
	timerdev_info	*timer;			/* A timer device for /dev/music */

	/*
	 * XXX not sure to which category these belong.
	 * (and some might be no-op)
	 */
	int		output_threshould;	/* Sequence output threshould */
	snd_sync_parm	sync_parm;		/* AIOSYNC parameter set */
	struct thread	*sync_thread;		/* AIOSYNCing thread */
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
};


static TAILQ_HEAD(,_seqdev_info)	seq_info;
/* Mutex to protect seq_info and nseq. */
static struct mtx			seqinfo_mtx;
/* total number of sequencers */
static u_long				nseq;
static dev_t				seq_alias = NODEV;
static dev_t				music_alias = NODEV;

SYSCTL_NODE(_hw_midi, OID_AUTO, seq, CTLFLAG_RD, 0, "Midi sequencer");

int					seq_debug;
SYSCTL_INT(_hw_midi_seq, OID_AUTO, debug, CTLFLAG_RW, &seq_debug, 0, "");

static midi_cmdtab	cmdtab_seqevent[] = {
	{SEQ_NOTEOFF,		"SEQ_NOTEOFF"},
	{SEQ_NOTEON,		"SEQ_NOTEON"},
	{SEQ_WAIT,		"SEQ_WAIT"},
	{SEQ_PGMCHANGE,		"SEQ_PGMCHANGE"},
	{SEQ_SYNCTIMER,		"SEQ_SYNCTIMER"},
	{SEQ_MIDIPUTC,		"SEQ_MIDIPUTC"},
	{SEQ_DRUMON,		"SEQ_DRUMON"},
	{SEQ_DRUMOFF,		"SEQ_DRUMOFF"},
	{SEQ_ECHO,		"SEQ_ECHO"},
	{SEQ_AFTERTOUCH,	"SEQ_AFTERTOUCH"},
	{SEQ_CONTROLLER,	"SEQ_CONTROLLER"},
	{SEQ_BALANCE,		"SEQ_BALANCE"},
	{SEQ_VOLMODE,		"SEQ_VOLMODE"},
	{SEQ_FULLSIZE,		"SEQ_FULLSIZE"},
	{SEQ_PRIVATE,		"SEQ_PRIVATE"},
	{SEQ_EXTENDED,		"SEQ_EXTENDED"},
	{EV_SEQ_LOCAL,		"EV_SEQ_LOCAL"},
	{EV_TIMING,		"EV_TIMING"},
	{EV_CHN_COMMON,		"EV_CHN_COMMON"},
	{EV_CHN_VOICE,		"EV_CHN_VOICE"},
	{EV_SYSEX,		"EV_SYSEX"},
	{-1,			NULL},
};

midi_cmdtab	cmdtab_seqioctl[] = {
	{SNDCTL_SEQ_RESET,	"SNDCTL_SEQ_RESET"},
	{SNDCTL_SEQ_SYNC,	"SNDCTL_SEQ_SYNC"},
	{SNDCTL_SYNTH_INFO,	"SNDCTL_SYNTH_INFO"},
	{SNDCTL_SEQ_CTRLRATE,	"SNDCTL_SEQ_CTRLRATE"},
	{SNDCTL_SEQ_GETOUTCOUNT,	"SNDCTL_SEQ_GETOUTCOUNT"},
	{SNDCTL_SEQ_GETINCOUNT,	"SNDCTL_SEQ_GETINCOUNT"},
	{SNDCTL_SEQ_PERCMODE,	"SNDCTL_SEQ_PERCMODE"},
	{SNDCTL_FM_LOAD_INSTR,	"SNDCTL_FM_LOAD_INSTR"},
	{SNDCTL_SEQ_TESTMIDI,	"SNDCTL_SEQ_TESTMIDI"},
	{SNDCTL_SEQ_RESETSAMPLES,	"SNDCTL_SEQ_RESETSAMPLES"},
	{SNDCTL_SEQ_NRSYNTHS,	"SNDCTL_SEQ_NRSYNTHS"},
	{SNDCTL_SEQ_NRMIDIS,	"SNDCTL_SEQ_NRMIDIS"},
	{SNDCTL_MIDI_INFO,	"SNDCTL_MIDI_INFO"},
	{SNDCTL_SEQ_THRESHOLD,	"SNDCTL_SEQ_THRESHOLD"},
	{SNDCTL_SYNTH_MEMAVL,	"SNDCTL_SYNTH_MEMAVL"},
	{SNDCTL_FM_4OP_ENABLE,	"SNDCTL_FM_4OP_ENABLE"},
	{SNDCTL_PMGR_ACCESS,	"SNDCTL_PMGR_ACCESS"},
	{SNDCTL_SEQ_PANIC,	"SNDCTL_SEQ_PANIC"},
	{SNDCTL_SEQ_OUTOFBAND,	"SNDCTL_SEQ_OUTOFBAND"},
	{SNDCTL_TMR_TIMEBASE,	"SNDCTL_TMR_TIMEBASE"},
	{SNDCTL_TMR_START,	"SNDCTL_TMR_START"},
	{SNDCTL_TMR_STOP,	"SNDCTL_TMR_STOP"},
	{SNDCTL_TMR_CONTINUE,	"SNDCTL_TMR_CONTINUE"},
	{SNDCTL_TMR_TEMPO,	"SNDCTL_TMR_TEMPO"},
	{SNDCTL_TMR_SOURCE,	"SNDCTL_TMR_SOURCE"},
	{SNDCTL_TMR_METRONOME,	"SNDCTL_TMR_METRONOME"},
	{SNDCTL_TMR_SELECT,	"SNDCTL_TMR_SELECT"},
	{SNDCTL_MIDI_PRETIME,	"SNDCTL_MIDI_PRETIME"},
	{AIONWRITE,		"AIONWRITE"},
	{AIOGSIZE,		"AIOGSIZE"},
	{AIOSSIZE,		"AIOSSIZE"},
	{AIOGFMT,		"AIOGFMT"},
	{AIOSFMT,		"AIOSFMT"},
	{AIOGMIX,		"AIOGMIX"},
	{AIOSMIX,		"AIOSMIX"},
	{AIOSTOP,		"AIOSTOP"},
	{AIOSYNC,		"AIOSYNC"},
	{AIOGCAP,		"AIOGCAP"},
	{-1,			NULL},
};

midi_cmdtab	cmdtab_timer[] = {
	{TMR_WAIT_REL,	"TMR_WAIT_REL"},
	{TMR_WAIT_ABS,	"TMR_WAIT_ABS"},
	{TMR_STOP,	"TMR_STOP"},
	{TMR_START,	"TMR_START"},
	{TMR_CONTINUE,	"TMR_CONTINUE"},
	{TMR_TEMPO,	"TMR_TEMPO"},
	{TMR_ECHO,	"TMR_ECHO"},
	{TMR_CLOCK,	"TMR_CLOCK"},
	{TMR_SPP,	"TMR_SPP"},
	{TMR_TIMESIG,	"TMR_TIMESIG"},
	{-1,		NULL},
};

static midi_cmdtab	cmdtab_seqcv[] = {
	{MIDI_NOTEOFF,		"MIDI_NOTEOFF"},
	{MIDI_NOTEON,		"MIDI_NOTEON"},
	{MIDI_KEY_PRESSURE,	"MIDI_KEY_PRESSURE"},
	{-1,			NULL},
};

static midi_cmdtab	cmdtab_seqccmn[] = {
	{MIDI_CTL_CHANGE,	"MIDI_CTL_CHANGE"},
	{MIDI_PGM_CHANGE,	"MIDI_PGM_CHANGE"},
	{MIDI_CHN_PRESSURE,	"MIDI_CHN_PRESSURE"},
	{MIDI_PITCH_BEND,	"MIDI_PITCH_BEND"},
	{MIDI_SYSTEM_PREFIX,	"MIDI_SYSTEM_PREFIX"},
	{-1,			NULL},
};


/* The followings are the local function. */
static int seq_init(void);
static int seq_initunit(int unit);
static int seq_queue(sc_p scp, u_char *note);
static void seq_startplay(sc_p scp);
static int seq_playevent(sc_p scp, u_char *event);
static u_long seq_gettime(void);
static int seq_requesttimer(sc_p scp, int delay);
static void seq_stoptimer(sc_p scp);
static void seq_midiinput(sc_p scp, mididev_info *md);
static int seq_extended(sc_p scp, u_char *event);
static int seq_chnvoice(sc_p scp, u_char *event);
static int seq_findvoice(mididev_info *md, int chn, int note) __unused;
static int seq_allocvoice(sc_p scp, mididev_info *md, int chn, int note) __unused;
static int seq_chncommon(sc_p scp, u_char *event);
static int seq_timing(sc_p scp, u_char *event);
static int seq_local(sc_p scp, u_char *event);
static int seq_sysex(sc_p scp, u_char *event);
static int seq_reset(sc_p scp);
static int seq_openmidi(sc_p scp, mididev_info *md, int flags, int mode, struct thread *p);
static int seq_closemidi(sc_p scp, mididev_info *md, int flags, int mode, struct thread *p);
static void seq_panic(sc_p scp);
static int seq_sync(sc_p scp);

static seqdev_info *get_seqdev_info(dev_t i_dev, int *unit);
static seqdev_info *get_seqdev_info_unit(int unit);
static seqdev_info *create_seqdev_info_unit(int unit, seqdev_info *seq);
static int lookup_mididev(sc_p scp, int unit, int mode, mididev_info **mdp);
static int lookup_mididev_midi(sc_p scp, int unit, int mode, mididev_info **mdp);
static void seq_clone(void *arg, char *name, int namelen, dev_t *dev);

/*
 * Here are the main functions to interact to the user process.
 * These are called from snd* functions in sys/i386/isa/snd/sound.c.
 */

static int
seq_init(void)
{
	SEQ_DEBUG(printf("seq: initing.\n"));

	mtx_init(&seqinfo_mtx, "seqinf", NULL, MTX_DEF);
	TAILQ_INIT(&seq_info);

	seq_initunit(0);
	EVENTHANDLER_REGISTER(dev_clone, seq_clone, 0, 1000);

	SEQ_DEBUG(printf("seq: inited.\n"));

	return (0);
}

static int
seq_initunit(int unit)
{
	sc_p scp;
	seqdev_info *devinfo;
	dev_t seqdev, musicdev;

	/* Allocate the softc. */
	scp = malloc(sizeof(*scp), M_DEVBUF, M_WAITOK | M_ZERO);
	if (scp == (sc_p)NULL) {
		printf("seq_initunit: unit %d, softc allocation failed.\n", unit);
		return (1);
	}

	/* Fill the softc and the seq_info for this unit. */
	scp->seq_time = seq_gettime();
	scp->prev_event_time = 0;
	scp->prev_input_time = 0;
	scp->prev_wakeup_time = scp->seq_time;
#if defined(MIDI_OUTOFGIANT)
	callout_init(&scp->timeout_ch, 1);
#else
	callout_init(&scp->timeout_ch, 0);
#endif /* MIDI_OUTOFGIANT */
	scp->timer_current = 0;
	scp->timer_running = 0;
	scp->queueout_pending = 0;
	TAILQ_INIT(&scp->midi_open);
	scp->pending_timer = -1;

	scp->devinfo = devinfo = create_seqdev_info_unit(unit, &seq_op_desc);
	devinfo->midi_dbuf_in.unit_size = devinfo->midi_dbuf_out.unit_size = EV_SZ;
	devinfo->softc = scp;
	devinfo->flags = 0;
	mtx_unlock(&devinfo->flagqueue_mtx);

	seqdev = make_dev(&seq_cdevsw, MIDIMKMINOR(unit, SND_DEV_SEQ),
			  UID_ROOT, GID_WHEEL, 0666, "sequencer%d", unit);
	musicdev = make_dev(&seq_cdevsw, MIDIMKMINOR(unit, SND_DEV_MUSIC),
			    UID_ROOT, GID_WHEEL, 0666, "music%d", unit);

	mtx_lock(&seqinfo_mtx);
	if (seq_alias != NODEV) {
		destroy_dev(seq_alias);
		seq_alias = NODEV;
	}
	seq_alias = make_dev_alias(seqdev, "sequencer");
	if (music_alias != NODEV) {
		destroy_dev(music_alias);
		music_alias = NODEV;
	}
	music_alias = make_dev_alias(musicdev, "music");
	mtx_unlock(&seqinfo_mtx);

	if (timerdev_install() != 0)
		printf("seq_initunit: timerdev_install failed.\n");

	return (0);
}

int
seq_open(dev_t i_dev, int flags, int mode, struct thread *td)
{
	int unit;
	sc_p scp;
	seqdev_info *sd;

	unit = MIDIUNIT(i_dev);

	SEQ_DEBUG(printf("seq_open: unit %d, flags 0x%x.\n", unit, flags));

	if (unit >= NSEQ_MAX) {
		SEQ_DEBUG(printf("seq_open: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_open: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	/* Mark this device busy. */
	mtx_lock(&sd->flagqueue_mtx);
	if ((sd->flags & SEQ_F_BUSY) != 0) {
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_open: unit %d is busy.\n", unit));
		return (EBUSY);
	}
	scp->fflags = flags;
	sd->flags |= SEQ_F_BUSY;
	sd->flags &= ~(SEQ_F_READING | SEQ_F_WRITING);
	if ((scp->fflags & O_NONBLOCK) != 0)
		sd->flags |= SEQ_F_NBIO;
	scp->seq_mode = MIDIDEV(i_dev);

	/* Init the queue. */
	midibuf_clear(&sd->midi_dbuf_in);
	midibuf_clear(&sd->midi_dbuf_out);

	/* Init timestamp. */
	scp->seq_time = seq_gettime();
	scp->prev_event_time = 0;
	scp->prev_input_time = 0;
	scp->prev_wakeup_time = scp->seq_time;

	if (scp->pending_timer != -1) {
		scp->timer = get_timerdev_info_unit(scp->pending_timer);
		scp->pending_timer = -1;
	}
	if (scp->timer == NULL)
		scp->timer = get_timerdev_info();
	if (scp->timer != NULL) {
		scp->timer->seq = scp;
		mtx_unlock(&scp->timer->mtx);
	} else if (scp->seq_mode == SND_DEV_MUSIC) {
		mtx_unlock(&sd->flagqueue_mtx);
		printf("seq_open: no timer available.\n");
		sd->flags &= ~SEQ_F_BUSY;
		return (ENXIO);
	}

	if (scp->seq_mode == SND_DEV_MUSIC)
		scp->timer->open(scp->timer, flags, mode, td);

	/* Begin recording if nonblocking. */
	if ((sd->flags & (SEQ_F_READING | SEQ_F_NBIO)) == SEQ_F_NBIO && (scp->fflags & FREAD) != 0)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_RD);

	mtx_unlock(&sd->flagqueue_mtx);

	SEQ_DEBUG(printf("seq_open: opened, mode %d.\n", scp->seq_mode == SND_DEV_MUSIC ? 2 : 1));

	return (0);
}

int
seq_close(dev_t i_dev, int flags, int mode, struct thread *td)
{
	int unit;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;
	timerdev_info *tmd;

	unit = MIDIUNIT(i_dev);

	SEQ_DEBUG(printf("seq_close: unit %d.\n", unit));

	if (unit >= NSEQ_MAX) {
		SEQ_DEBUG(printf("seq_close: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_close: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	mtx_lock(&sd->flagqueue_mtx);

	if (!(sd->flags & MIDI_F_NBIO))
		seq_sync(scp);

	/* Stop the timer. */
	seq_stoptimer(scp);

	/* Reset the sequencer. */
	seq_reset(scp);
	seq_sync(scp);

	/* Clean up the midi device. */
	TAILQ_FOREACH(md, &scp->midi_open, md_linkseq)
		lookup_mididev(scp, md->unit, LOOKUP_CLOSE, NULL);

	/* Stop playing and unmark this device busy. */
	sd->flags &= ~(SEQ_F_BUSY | SEQ_F_READING | SEQ_F_WRITING | SEQ_F_INSYNC);

	if (scp->seq_mode == SND_DEV_MUSIC)
		scp->timer->close(scp->timer, flags, mode, td);

	if (scp->timer != NULL) {
		tmd = scp->timer;
		mtx_lock(&tmd->mtx);
		scp->timer = NULL;
		tmd->seq = NULL;
		mtx_unlock(&tmd->mtx);
	}

	mtx_unlock(&sd->flagqueue_mtx);

	SEQ_DEBUG(printf("seq_close: closed.\n"));

	return (0);
}

int
seq_read(dev_t i_dev, struct uio *buf, int flag)
{
	int unit, ret, len, lenr;
	sc_p scp;
	seqdev_info *sd;
	u_char *uiobuf;

	unit = MIDIUNIT(i_dev);

	SEQ_DEBUG(printf("seq_read: unit %d, resid %d.\n", unit, buf->uio_resid));

	if (unit >= NSEQ_MAX) {
		SEQ_DEBUG(printf("seq_read: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_read: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;
	if ((scp->fflags & FREAD) == 0) {
		SEQ_DEBUG(printf("seq_read: unit %d is not for reading.\n", unit));
		return (EIO);
	}

	len = buf->uio_resid;
	lenr = 0;

	uiobuf = (u_char *)malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (uiobuf == NULL)
		return (ENOMEM);

	mtx_lock(&sd->flagqueue_mtx);

	/* Begin recording. */
	if ((sd->flags & SEQ_F_READING) == 0)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_RD);

	/* Have we got the data to read? */
	if ((sd->flags & SEQ_F_NBIO) != 0 && sd->midi_dbuf_in.rl == 0)
		ret = EAGAIN;
	else {
		if ((sd->flags & SEQ_F_NBIO) != 0 && len > sd->midi_dbuf_in.rl)
			len = sd->midi_dbuf_in.rl;
		ret = midibuf_seqread(&sd->midi_dbuf_in, uiobuf, len, &lenr,
				      sd->callback, sd, SEQ_CB_START | SEQ_CB_RD,
				      &sd->flagqueue_mtx);
	}

	mtx_unlock(&sd->flagqueue_mtx);

	if (ret == 0 && lenr > 0)
		ret = uiomove(uiobuf, lenr, buf);

	free(uiobuf, M_DEVBUF);

	SEQ_DEBUG(printf("seq_read: ret %d, resid %d.\n", ret, buf->uio_resid));

	return (ret);
}

int
seq_write(dev_t i_dev, struct uio *buf, int flag)
{
	u_char event[EV_SZ], ev_code;
	int unit, count, countorg, midiunit, ev_size, p, ret;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;

	unit = MIDIUNIT(i_dev);

	SEQ_DEBUG(printf("seq_write: unit %d, resid %d.\n", unit, buf->uio_resid));

	if (unit >= NSEQ_MAX) {
		SEQ_DEBUG(printf("seq_write: unit %d does not exist.\n", unit));
		return (ENXIO);
	}

	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_write: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;
	if ((scp->fflags & FWRITE) == 0) {
		SEQ_DEBUG(printf("seq_write: unit %d is not for writing.\n", unit));
		return (EIO);
	}

	p = 0;
	countorg = buf->uio_resid;
	count = countorg;

	/* Pick up an event. */
	while (count >= 4) {
		if (uiomove((caddr_t)event, 4, buf))
			printf("seq_write: user memory mangled?\n");
		ev_code = event[0];
		SEQ_DEBUG(printf("seq_write: unit %d, event %s.\n", unit, midi_cmdname(ev_code, cmdtab_seqevent)));

		/* Have a look at the event code. */
		if (ev_code == SEQ_FULLSIZE) {

			/* A long event, these are the patches/samples for a synthesizer. */
			midiunit = *(u_short *)&event[2];
			mtx_lock(&sd->flagqueue_mtx);
			ret = lookup_mididev(scp, midiunit, LOOKUP_OPEN, &md);
			mtx_unlock(&sd->flagqueue_mtx);
			if (ret != 0)
				return (ret);

			SEQ_DEBUG(printf("seq_write: loading a patch to the unit %d.\n", midiunit));

			ret = md->synth.loadpatch(md, *(short *)&event[0], buf, p + 4, count, 0);
			return (ret);
		}

		if (ev_code >= 128) {

			/* Some sort of an extended event. The size is eight bytes. */
			if (scp->seq_mode == SND_DEV_MUSIC && ev_code == SEQ_EXTENDED) {
				printf("seq_write: invalid level two event %x.\n", ev_code);
				return (EINVAL);
			}
			ev_size = 8;

			if (count < ev_size) {
				/* No more data. Start playing now. */
				mtx_lock(&sd->flagqueue_mtx);
				if ((sd->flags & SEQ_F_WRITING) == 0)
					sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
				mtx_unlock(&sd->flagqueue_mtx);
				buf->uio_resid += 4;

				return (0);
			}
			if (uiomove((caddr_t)&event[4], 4, buf))
				printf("seq_write: user memory mangled?\n");
		} else {

			/* Not an extended event. The size is four bytes. */
			if (scp->seq_mode == SND_DEV_MUSIC) {
				printf("seq_write: four byte event in level two mode.\n");
				return (EINVAL);
			}
			ev_size = 4;
		}
		if (ev_code == SEQ_MIDIPUTC) {
			/* An event passed to the midi device itself. */
			midiunit = event[2];
			mtx_lock(&sd->flagqueue_mtx);
			ret = lookup_mididev_midi(scp, midiunit, LOOKUP_OPEN, &md);
			mtx_unlock(&sd->flagqueue_mtx);
			if (ret != 0)
				return (ret);
		}

		SEQ_DEBUG(printf("seq_write: queueing event %s.\n", midi_cmdname(event[0], cmdtab_seqevent)));
		/* Now we queue the event. */
		mtx_lock(&sd->flagqueue_mtx);
		switch (seq_queue(scp, event)) {
		case EAGAIN:
			/* The queue is full. Start playing now. */
			if ((sd->flags & SEQ_F_WRITING) == 0)
				sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
			mtx_unlock(&sd->flagqueue_mtx);
			buf->uio_resid = count;
			SEQ_DEBUG(printf("seq_write: resid %d.\n", buf->uio_resid));
			if (count < countorg)
				return (0);
			return (EAGAIN);
		case EINTR:
			mtx_unlock(&sd->flagqueue_mtx);
			SEQ_DEBUG(printf("seq_write: resid %d.\n", buf->uio_resid));
			return (EINTR);
		case ERESTART:
			mtx_unlock(&sd->flagqueue_mtx);
			SEQ_DEBUG(printf("seq_write: resid %d.\n", buf->uio_resid));
			return (ERESTART);
		}
		mtx_unlock(&sd->flagqueue_mtx);
		p += ev_size;
		count -= ev_size;
	}

	/* We have written every single data. Start playing now. */
	mtx_lock(&sd->flagqueue_mtx);
	if ((sd->flags & SEQ_F_WRITING) == 0)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
	mtx_unlock(&sd->flagqueue_mtx);

	SEQ_DEBUG(printf("seq_write: resid %d.\n", buf->uio_resid));

	return (0);
}

int
seq_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
	int unit, midiunit, ret, tmp;
	sc_p scp;
	seqdev_info *sd;
	mididev_info *md;
	struct synth_info *synthinfo;
	struct midi_info *midiinfo;
	struct patmgr_info *patinfo;
	struct seq_event_rec *event;
	struct snd_size *sndsize;

	unit = MIDIUNIT(i_dev);

	SEQ_DEBUG(printf("seq_ioctl: unit %d, cmd %s.\n", unit, midi_cmdname(cmd, cmdtab_seqioctl)));

	if (unit >= NSEQ_MAX) {
		SEQ_DEBUG(printf("seq_ioctl: unit %d does not exist.\n", unit));
		return (ENXIO);
	}
	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_ioctl: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	ret = 0;

	switch (cmd) {

		/*
		 * we start with the new ioctl interface.
		 */
	case AIONWRITE:	/* how many bytes can be written ? */
		mtx_lock(&sd->flagqueue_mtx);
		*(int *)arg = sd->midi_dbuf_out.fl;
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: fl %d.\n", *(int *)arg));
		break;

	case AIOSSIZE:     /* set the current blocksize */
		sndsize = (struct snd_size *)arg;
		SEQ_DEBUG(printf("seq_ioctl: play %d, rec %d.\n", sndsize->play_size, sndsize->rec_size));
		mtx_lock(&sd->flagqueue_mtx);
		if (sndsize->play_size <= sd->midi_dbuf_out.unit_size && sndsize->rec_size <= sd->midi_dbuf_in.unit_size) {
			sd->midi_dbuf_out.blocksize = sd->midi_dbuf_out.unit_size;
			sd->midi_dbuf_in.blocksize = sd->midi_dbuf_in.unit_size;
			sndsize->play_size = sd->midi_dbuf_out.blocksize;
			sndsize->rec_size = sd->midi_dbuf_in.blocksize;
			sd->flags &= ~MIDI_F_HAS_SIZE;
			mtx_unlock(&sd->flagqueue_mtx);
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
			sndsize->play_size = sd->midi_dbuf_out.blocksize;
			sndsize->rec_size = sd->midi_dbuf_in.blocksize;
			sd->flags |= MIDI_F_HAS_SIZE;
			mtx_unlock(&sd->flagqueue_mtx);
		}	

		ret = 0;
		break;

	case AIOGSIZE:	/* get the current blocksize */
		sndsize = (struct snd_size *)arg;
		mtx_lock(&sd->flagqueue_mtx);
		sndsize->play_size = sd->midi_dbuf_out.blocksize;
		sndsize->rec_size = sd->midi_dbuf_in.blocksize;
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: play %d, rec %d.\n", sndsize->play_size, sndsize->rec_size));

		ret = 0;
		break;

	case AIOSTOP:
		if (*(int *)arg == AIOSYNC_PLAY) {

			/* Stop writing. */
			mtx_lock(&sd->flagqueue_mtx);
			sd->callback(sd, SEQ_CB_ABORT | SEQ_CB_WR);
			mtx_unlock(&sd->flagqueue_mtx);

			/* Pass the ioctl to the midi devices. */
			TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
				if ((md->flags & MIDI_F_WRITING) != 0)
					midi_ioctl(MIDIMKDEV(major(i_dev), md->unit, SND_DEV_MIDIN), cmd, (caddr_t)arg, mode, td);
			}

			mtx_lock(&sd->flagqueue_mtx);
			*(int *)arg = sd->midi_dbuf_out.rl;
			mtx_unlock(&sd->flagqueue_mtx);
		}
		else if (*(int *)arg == AIOSYNC_CAPTURE) {

			/* Stop reading. */
			mtx_lock(&sd->flagqueue_mtx);
			sd->callback(sd, SEQ_CB_ABORT | SEQ_CB_RD);
			mtx_unlock(&sd->flagqueue_mtx);

			/* Pass the ioctl to the midi devices. */
			TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
				if ((md->flags & MIDI_F_WRITING) != 0)
					midi_ioctl(MIDIMKDEV(major(i_dev), md->unit, SND_DEV_MIDIN), cmd, (caddr_t)arg, mode, td);
			}

			mtx_lock(&sd->flagqueue_mtx);
			*(int *)arg = sd->midi_dbuf_in.rl;
			mtx_unlock(&sd->flagqueue_mtx);
		}

		ret = 0;
		break;

	case AIOSYNC:
		mtx_lock(&sd->flagqueue_mtx);
		scp->sync_parm = *(snd_sync_parm *)arg;
		mtx_unlock(&sd->flagqueue_mtx);

		/* XXX Should select(2) against us watch the blocksize, or sync_parm? */

		ret = 0;
		break;

	case FIONBIO: /* set/clear non-blocking i/o */
		mtx_lock(&sd->flagqueue_mtx);
		if (*(int *)arg == 0)
			sd->flags &= ~SEQ_F_NBIO ;
		else
			sd->flags |= SEQ_F_NBIO ;
		mtx_unlock(&sd->flagqueue_mtx);
		MIDI_DEBUG(printf("seq_ioctl: arg %d.\n", *(int *)arg));
		break ;

	case SNDCTL_TMR_TIMEBASE:
	case SNDCTL_TMR_TEMPO:
	case SNDCTL_TMR_START:
	case SNDCTL_TMR_STOP:
	case SNDCTL_TMR_CONTINUE:
	case SNDCTL_TMR_METRONOME:
	case SNDCTL_TMR_SOURCE:
		mtx_lock(&sd->flagqueue_mtx);
		if (scp->seq_mode != SND_DEV_MUSIC) {
			ret = EINVAL;
			mtx_unlock(&sd->flagqueue_mtx);
			break;
		}
		mtx_unlock(&sd->flagqueue_mtx);
		/* XXX We should adopt am sx to protect scp->timer */
		ret = scp->timer->ioctl(scp->timer, cmd, arg, mode, td);
		break;
	case SNDCTL_TMR_SELECT:
		mtx_lock(&sd->flagqueue_mtx);
		if (scp->seq_mode != SND_DEV_MUSIC) {
			ret = EINVAL;
			mtx_unlock(&sd->flagqueue_mtx);
			break;
		}
		mtx_unlock(&sd->flagqueue_mtx);
		scp->pending_timer = *(int *)arg;
		mtx_lock(&sd->flagqueue_mtx);
		if (scp->pending_timer < 0) {
			scp->pending_timer = -1;
			ret = EINVAL;
			mtx_unlock(&sd->flagqueue_mtx);
			break;
		}
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: new timer %d.\n", *(int *)arg));
		ret = 0;
		break;
	case SNDCTL_SEQ_PANIC:
		mtx_lock(&sd->flagqueue_mtx);
		seq_panic(scp);
		mtx_unlock(&sd->flagqueue_mtx);
		ret = 0;
		break;
	case SNDCTL_SEQ_SYNC:
		if (mode == O_RDONLY) {
			ret = 0;
			break;
		}
		mtx_lock(&scp->devinfo->flagqueue_mtx);
		ret = seq_sync(scp);
		mtx_unlock(&scp->devinfo->flagqueue_mtx);
		break;
	case SNDCTL_SEQ_RESET:
		mtx_lock(&scp->devinfo->flagqueue_mtx);
		seq_reset(scp);
		mtx_unlock(&scp->devinfo->flagqueue_mtx);
		ret = 0;
		break;
	case SNDCTL_SEQ_TESTMIDI:
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev_midi(scp, *(int *)arg, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		break;
	case SNDCTL_SEQ_GETINCOUNT:
		if (mode == O_WRONLY)
			*(int *)arg = 0;
		else {
			mtx_lock(&sd->flagqueue_mtx);
			*(int *)arg = sd->midi_dbuf_in.rl;
			mtx_unlock(&sd->flagqueue_mtx);
			SEQ_DEBUG(printf("seq_ioctl: incount %d.\n", *(int *)arg));
		}
		ret = 0;
		break;
	case SNDCTL_SEQ_GETOUTCOUNT:
		if (mode == O_RDONLY)
			*(int *)arg = 0;
		else {
			mtx_lock(&sd->flagqueue_mtx);
			*(int *)arg = sd->midi_dbuf_out.fl;
			mtx_unlock(&sd->flagqueue_mtx);
			SEQ_DEBUG(printf("seq_ioctl: outcount %d.\n", *(int *)arg));
		}
		ret = 0;
		break;
	case SNDCTL_SEQ_CTRLRATE:
		mtx_lock(&sd->flagqueue_mtx);
		if (scp->seq_mode == SND_DEV_MUSIC) {
			mtx_unlock(&sd->flagqueue_mtx);
			ret = scp->timer->ioctl(scp->timer, cmd, arg, mode, td);
			break;
		}
		mtx_unlock(&sd->flagqueue_mtx);
		if (*(int *)arg != 0) {
			ret = EINVAL;
			break;
		}
		*(int *)arg = hz;
		SEQ_DEBUG(printf("seq_ioctl: ctrlrate %d.\n", *(int *)arg));
		ret = 0;
		break;
	case SNDCTL_SEQ_RESETSAMPLES:
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, *(int *)arg, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), *(int *)arg, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_SEQ_NRSYNTHS:
		mtx_lock(&sd->flagqueue_mtx);
		if (scp->seq_mode == SND_DEV_MUSIC)
			*(int *)arg = mididev_synth_number() + mididev_midi_number();
		else
			*(int *)arg = mididev_synth_number();
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: synths %d.\n", *(int *)arg));
		ret = 0;
		break;
	case SNDCTL_SEQ_NRMIDIS:
		mtx_lock(&sd->flagqueue_mtx);
		if (scp->seq_mode == SND_DEV_MUSIC)
			*(int *)arg = 0;
		else
			*(int *)arg = mididev_midi_number();
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: midis %d.\n", *(int *)arg));
		ret = 0;
		break;
	case SNDCTL_SYNTH_MEMAVL:
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, *(int *)arg, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), *(int *)arg, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_FM_4OP_ENABLE:
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, *(int *)arg, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), *(int *)arg, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_SYNTH_INFO:
		synthinfo = (struct synth_info *)arg;
		midiunit = synthinfo->device;
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, midiunit, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_SEQ_OUTOFBAND:
		event = (struct seq_event_rec *)arg;
		mtx_lock(&sd->flagqueue_mtx);
		ret = seq_playevent(scp, event->arr);
		mtx_unlock(&sd->flagqueue_mtx);
		break;
	case SNDCTL_MIDI_INFO:
		midiinfo = (struct midi_info *)arg;
		midiunit = midiinfo->device;
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev_midi(scp, midiunit, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_PMGR_IFACE:
		patinfo = (struct patmgr_info *)arg;
		midiunit = patinfo->device;
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, midiunit, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_PMGR_ACCESS:
		patinfo = (struct patmgr_info *)arg;
		midiunit = patinfo->device;
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, midiunit, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), midiunit, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	case SNDCTL_SEQ_THRESHOLD:
		mtx_lock(&sd->flagqueue_mtx);
		RANGE(*(int *)arg, 1, sd->midi_dbuf_out.bufsize - 1);
		scp->output_threshould = *(int *)arg;
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: threshold %d.\n", *(int *)arg));
		ret = 0;
		break;
	case SNDCTL_MIDI_PRETIME:
		tmp = *(int *)arg;
		if (tmp < 0)
			tmp = 0;
		mtx_lock(&sd->flagqueue_mtx);
		scp->pre_event_timeout = (hz * tmp) / 10;
		*(int *)arg = scp->pre_event_timeout;
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_ioctl: pretime %d.\n", *(int *)arg));
		ret = 0;
		break;
	default:
		if ((scp->fflags & O_ACCMODE) == FREAD) {
			ret = EIO;
			break;
		}
		mtx_lock(&sd->flagqueue_mtx);
		ret = lookup_mididev(scp, 0, LOOKUP_OPEN, &md);
		mtx_unlock(&sd->flagqueue_mtx);
		if (ret != 0)
			break;
		ret = midi_ioctl(MIDIMKDEV(major(i_dev), 0, SND_DEV_MIDIN), cmd, arg, mode, td);
		break;
	}

	return (ret);
}

int
seq_poll(dev_t i_dev, int events, struct thread *td)
{
	int unit, ret, lim;
	sc_p scp;
	seqdev_info *sd;

	unit = MIDIUNIT(i_dev);

	SEQ_DEBUG(printf("seq_poll: unit %d.\n", unit));

	if (unit >= NSEQ_MAX) {
		SEQ_DEBUG(printf("seq_poll: unit %d does not exist.\n", unit));
		return (ENXIO);
	}
	sd = get_seqdev_info(i_dev, &unit);
	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_poll: unit %d is not configured.\n", unit));
		return (ENXIO);
	}
	scp = sd->softc;

	mtx_lock(&sd->flagqueue_mtx);

	ret = 0;

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
			selrecord(td, &sd->midi_dbuf_out.sel);
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
			selrecord(td, &sd->midi_dbuf_in.sel);
		else
			/* We can write now. */
			ret |= events & (POLLIN | POLLRDNORM);
	}

	mtx_unlock(&sd->flagqueue_mtx);

	return (ret);
}

static void
seq_intr(void *p, mididev_info *md)
{
	sc_p scp;
	seqdev_info *sd;

	sd = (seqdev_info *)p;
	scp = sd->softc;

	mtx_lock(&sd->flagqueue_mtx);

	/* Restart playing if we have the data to output. */
	if (scp->queueout_pending)
		sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
	/* Check the midi device if we are reading. */
	if ((sd->flags & SEQ_F_READING) != 0)
		seq_midiinput(scp, md);

	mtx_unlock(&sd->flagqueue_mtx);
}

static int
seq_callback(void *d, int reason)
{
	int unit;
	sc_p scp;
	seqdev_info *sd;

	sd = (seqdev_info *)d;

	SEQ_DEBUG(printf("seq_callback: reason 0x%x.\n", reason));

	if (sd == NULL) {
		SEQ_DEBUG(printf("seq_callback: device not configured.\n"));
		return (ENXIO);
	}
	scp = sd->softc;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

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
			/* Stop recording. */
			sd->flags &= ~SEQ_F_READING;
			scp->seq_time = seq_gettime();
			scp->prev_input_time = 0;
		}
		if ((reason & SEQ_CB_WR) != 0 && (sd->flags & SEQ_F_WRITING) != 0) {
			/* Stop Playing. */
			sd->flags &= ~SEQ_F_WRITING;
			scp->queueout_pending = 0;
			scp->seq_time = seq_gettime();
			scp->prev_input_time = 0;

			/* Stop the timer. */
			seq_stoptimer(scp);
		}
	}

	return (0);
}

/*
 * The functions below here are the libraries for the above ones.
 */

static int
seq_queue(sc_p scp, u_char *note)
{
	int unit, err, lenw;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	SEQ_DEBUG(printf("seq_queue: unit %d.\n", unit));

	if ((sd->flags & SEQ_F_INSYNC) != 0)
		cv_wait(&sd->insync_cv, &sd->flagqueue_mtx);

	if (sd->midi_dbuf_out.fl < EV_SZ) {
		/* We have no space. Start playing if not yet. */
		if ((sd->flags & SEQ_F_WRITING) == 0)
			sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
		if ((sd->flags & SEQ_F_NBIO) != 0 && sd->midi_dbuf_out.fl < EV_SZ)
			/* We would block. */
			return (EAGAIN);
	}

	/* Write to the queue. */
	err = midibuf_seqwrite(&sd->midi_dbuf_out, note, EV_SZ, &lenw,
			       sd->callback, sd, SEQ_CB_START | SEQ_CB_WR,
			       &sd->flagqueue_mtx);

	if (err == 0) {
		/* Start playing if we have some data in the queue. */
		if (sd->midi_dbuf_out.rl >= EV_SZ && ((sd->flags & SEQ_F_WRITING) == 0))
			sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
	}

	return (err);
}

static void
seq_startplay(sc_p scp)
{
	int unit, lenr;
	u_char event[EV_SZ];
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	sd->flags |= SEQ_F_WRITING;

	/* Dequeue the events to play. */
	while (sd->midi_dbuf_out.rl >= EV_SZ) {

		midibuf_seqcopy(&sd->midi_dbuf_out, event, EV_SZ, &lenr,
				NULL, NULL, 0,
				&sd->flagqueue_mtx);

		switch (seq_playevent(scp, event)) {
		case TIMERARMED:
			midibuf_seqdelete(&sd->midi_dbuf_out, EV_SZ, &lenr,
					  NULL, NULL, 0,
					  &sd->flagqueue_mtx);
			return;
		case QUEUEFULL:
			/* We cannot play any further. */
			return;
		case MORE:
			midibuf_seqdelete(&sd->midi_dbuf_out, EV_SZ, &lenr,
					  NULL, NULL, 0,
					  &sd->flagqueue_mtx);
			break;
		}
	}

	/* Played every event in the queue. */
	sd->flags &= ~SEQ_F_WRITING;
}

static int
seq_playevent(sc_p scp, u_char *event)
{
	int unit, ret, lenw;
	long *delay;
	seqdev_info *sd;
	mididev_info *md;

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	ret = lookup_mididev(scp, 0, LOOKUP_OPEN, &md);
	if (ret != 0)
		return (MORE);

	SEQ_DEBUG(printf("seq_playevent: unit %d, event %s.\n", sd->unit, midi_cmdname(event[0], cmdtab_seqevent)));

	switch(event[0]) {
	case SEQ_NOTEOFF:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_playevent: chn %d, note %d, vel %d.\n", event[1], event[2], event[3]));
		if (md->synth.killnote(md, event[1], 255, event[3]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			ret = QUEUEFULL;
			break;
		}
		mtx_lock(&sd->flagqueue_mtx);
		ret = MORE;
		break;
	case SEQ_NOTEON:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_playevent: chn %d, note %d, vel %d, aux %d.\n", event[1], event[2], event[3], event[4]));
		if ((event[4] < 128 || event[4] == 255) && md->synth.startnote(md, event[1], event[2], event[3]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			ret = QUEUEFULL;
			break;
		}
		mtx_lock(&sd->flagqueue_mtx);
		ret = MORE;
		break;
	case SEQ_WAIT:

		/* Extract the delay. */
		delay = (long *)event;
		*delay = (*delay >> 8) & 0xffffff;
		SEQ_DEBUG(printf("seq_playevent: delay %ld.\n", *delay));
		if (*delay > 0) {
			/* Arm the timer. */
			sd->flags |= SEQ_F_WRITING;
			if (seq_requesttimer(scp, *delay)) {
				ret = TIMERARMED;
				break;
			}
		}
		ret = MORE;
		break;
	case SEQ_PGMCHANGE:
		SEQ_DEBUG(printf("seq_playevent: chn %d, instr %d.\n", event[1], event[2]));
		mtx_unlock(&sd->flagqueue_mtx);
		if (md->synth.setinstr(md, event[1], event[2]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			ret = QUEUEFULL;
			break;
		}
		mtx_lock(&sd->flagqueue_mtx);
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
		SEQ_DEBUG(printf("seq_playevent: data 0x%02x, unit %d.\n", event[1], event[2]));
		/* Pass through to the midi device. */
		ret = lookup_mididev_midi(scp, event[2], LOOKUP_OPEN, &md);
		if (ret != 0) {
			ret = MORE;
			break;
		}
		mtx_unlock(&sd->flagqueue_mtx);
		if (md->synth.writeraw(md, &event[1], sizeof(event[1]), &lenw, 1) == EAGAIN)
			/* The queue was full. Try again later. */
			ret = QUEUEFULL;
		else
			ret = MORE;
		mtx_lock(&sd->flagqueue_mtx);
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
		ret = lookup_mididev(scp, event[1], LOOKUP_OPEN, &md);
		if (ret != 0) {
			ret = MORE;
			break;
		}
		mtx_unlock(&sd->flagqueue_mtx);
		if (md->synth.hwcontrol(md, event) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			ret = QUEUEFULL;
			break;
		}
		mtx_lock(&sd->flagqueue_mtx);
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
		SEQ_DEBUG(printf("seq_playevent: the queue is full.\n"));
		/* The queue was full. Try again on the interrupt by the midi device. */
		sd->flags |= SEQ_F_WRITING;
		scp->queueout_pending = 1;
		break;
	case TIMERARMED:
		SEQ_DEBUG(printf("seq_playevent: armed timer.\n"));
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

	SEQ_DEBUG(printf("seq_requesttimer: unit %d, delay %d.\n", scp->devinfo->unit, delay));

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	cur_time = seq_gettime();

	scp->prev_event_time = delay;
	if (delay < 0)
		/* Request a new timer. */
		delay = -delay;
	else {
		rel_base = cur_time - scp->seq_time;
		if (delay <= rel_base) {
			seq_stoptimer(scp);
			return 0;
		}
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

	callout_reset(&scp->timeout_ch, delay, seq_timer, (void *)scp);
	scp->timer_running = 1;

	return 1;
}

static void
seq_stoptimer(sc_p scp)
{
	SEQ_DEBUG(printf("seq_stoptimer: unit %d.\n", scp->devinfo->unit));

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	if (scp->timer_running) {
		callout_stop(&scp->timeout_ch);
		scp->timer_running = 0;
	}
}

static void
seq_midiinput(sc_p scp, mididev_info *md)
{
	int unit, midiunit, lenr;
	u_long tstamp;
	u_char event[4];
	seqdev_info *sd;

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	sd = scp->devinfo;
	unit = sd->unit;

	/* Can this midi device interrupt for input? */
	midiunit = md->midiunit;
	if (lookup_mididev_midi(scp, midiunit, LOOKUP_EXIST, NULL) != 0)
		return;

	if ((md->flags & MIDI_F_READING) != 0 && md->intrarg == sd) {
		/* Read the input data. */
		mtx_unlock(&scp->devinfo->flagqueue_mtx);
		while (md->synth.readraw(md, &event[1], sizeof(event[1]), &lenr, 1) == 0) {
			mtx_lock(&scp->devinfo->flagqueue_mtx);
			tstamp = seq_gettime() - scp->seq_time;
			if (tstamp != scp->prev_input_time) {
				/* Insert a wait between events. */
				tstamp = (tstamp << 8) | SEQ_WAIT;
				seq_copytoinput(scp, (u_char *)&tstamp, 4);
				scp->prev_input_time = tstamp;
			}
			bzero(event, sizeof(event));
			event[0] = SEQ_MIDIPUTC;
			event[2] = midiunit;
			event[3] = 0;
			seq_copytoinput(scp, event, sizeof(event));
			mtx_unlock(&scp->devinfo->flagqueue_mtx);
		}
		mtx_lock(&scp->devinfo->flagqueue_mtx);
	}
}

int
seq_copytoinput(void *arg, u_char *event, int len)
{
	int ret, leni;
	sc_p scp;
	seqdev_info *sd;

	scp = arg;
	sd = scp->devinfo;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	if (len != 4 && len != 8)
		return (EINVAL);
	if (scp->seq_mode == SND_DEV_MUSIC && len != 8)
		return (EINVAL);

	ret = midibuf_input_intr(&sd->midi_dbuf_in, event, len, &leni);
	if (ret == EAGAIN)
		ret = 0;

	return (ret);
}

static int
seq_extended(sc_p scp, u_char *event)
{
	int unit;
	seqdev_info *sd;
	mididev_info *md;

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	if (lookup_mididev(scp, event[2], LOOKUP_OPEN, &md) != 0)
		return (MORE);

	SEQ_DEBUG(printf("seq_extended: unit %d, event %s, midiunit %d.\n", unit, midi_cmdname(event[1], cmdtab_seqevent), event[2]));

	switch (event[1]) {
	case SEQ_NOTEOFF:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: chn %d, note %d, vel %d.\n", event[3], event[4], event[5]));
		if (md->synth.killnote(md, event[3], event[4], event[5]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case SEQ_NOTEON:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: chn %d, note %d, vel %d.\n", event[3], event[4], event[5]));
		if ((event[4] < 128 || event[4] == 255) && md->synth.startnote(md, event[3], event[4], event[5]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case SEQ_PGMCHANGE:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: chn %d, instr %d.\n", event[3], event[4]));
		if (md->synth.setinstr(md, event[3], event[4]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case SEQ_AFTERTOUCH:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: chn %d, press %d.\n", event[3], event[4]));
		if (md->synth.aftertouch(md, event[3], event[4]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case SEQ_BALANCE:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: chn %d, pan %d.\n", event[3], event[4]));
		if (md->synth.panning(md, event[3], (char)event[4]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case SEQ_CONTROLLER:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: chn %d, ctrlnum %d, val %d.\n", event[3], event[4], *(short *)&event[5]));
		if (md->synth.controller(md, event[3], event[4], *(short *)&event[5]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case SEQ_VOLMODE:
		mtx_unlock(&sd->flagqueue_mtx);
		SEQ_DEBUG(printf("seq_extended: mode %d.\n", event[3]));
		if (md->synth.volumemethod != NULL && md->synth.volumemethod(md, event[3]) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
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

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	if (lookup_mididev(scp, dev, LOOKUP_OPEN, &md) != 0)
		return (MORE);

	SEQ_DEBUG(printf("seq_chnvoice: unit %d, dev %d, cmd %s, chn %d, note %d, parm %d.\n",
			 sd->unit,
			 dev,
			 midi_cmdname(cmd, cmdtab_seqcv),
			 chn,
			 note,
			 parm));

	if (scp->seq_mode == SND_DEV_MUSIC && md->synth.allocvoice != NULL)
		voice = seq_allocvoice(scp, md, chn, note);
	switch (cmd) {
	case MIDI_NOTEON:
		if (note < 128 || note == 255) {
			if (voice == -1 && scp->seq_mode == SND_DEV_MUSIC && md->synth.allocvoice)
				/* This is an internal synthesizer. (FM, GUS, etc) */
				if ((voice = seq_allocvoice(scp, md, chn, note)) == EAGAIN)
					return (QUEUEFULL);
			if (voice == -1)
				voice = chn;

			if (scp->seq_mode == SND_DEV_MUSIC && chn == 9) {
				/* This channel is a percussion. The note number is the patch number. */
				mtx_unlock(&sd->flagqueue_mtx);
				if (md->synth.setinstr(md, voice, 128 + note) == EAGAIN) {
					mtx_lock(&sd->flagqueue_mtx);
					return (QUEUEFULL);
				}
				mtx_lock(&sd->flagqueue_mtx);

				note = 60; /* Middle C. */
			}
			if (scp->seq_mode == SND_DEV_MUSIC) {
				mtx_unlock(&sd->flagqueue_mtx);
				if (md->synth.setupvoice(md, voice, chn) == EAGAIN) {
					mtx_lock(&sd->flagqueue_mtx);
					return (QUEUEFULL);
				}
				mtx_lock(&sd->flagqueue_mtx);
			}
			mtx_unlock(&sd->flagqueue_mtx);
			if (md->synth.startnote(md, voice, note, parm) == EAGAIN) {
				mtx_lock(&sd->flagqueue_mtx);
				return (QUEUEFULL);
			}
			mtx_lock(&sd->flagqueue_mtx);
		}
		break;
	case MIDI_NOTEOFF:
		if (voice == -1)
			voice = chn;
		mtx_unlock(&sd->flagqueue_mtx);
		if (md->synth.killnote(md, voice, note, parm) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
		break;
	case MIDI_KEY_PRESSURE:
		if (voice == -1)
			voice = chn;
		mtx_unlock(&sd->flagqueue_mtx);
		if (md->synth.aftertouch(md, voice, parm) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
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

	mtx_lock(&md->synth.vc_mtx);
	for (i = 0 ; i < md->synth.alloc.max_voice ; i++)
		if (md->synth.alloc.map[i] == key) {
			mtx_unlock(&md->synth.vc_mtx);
			return (i);
		}
	mtx_unlock(&md->synth.vc_mtx);

	return (-1);
}

static int
seq_allocvoice(sc_p scp, mididev_info *md, int chn, int note)
{
	int voice;
	u_short key;

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	key = (chn << 8) | (note + 1);

	mtx_unlock(&scp->devinfo->flagqueue_mtx);
	if ((voice = md->synth.allocvoice(md, chn, note, &md->synth.alloc)) == EAGAIN) {
		mtx_lock(&scp->devinfo->flagqueue_mtx);
		return (EAGAIN);
	}
	mtx_lock(&scp->devinfo->flagqueue_mtx);

	mtx_lock(&md->synth.vc_mtx);
	md->synth.alloc.map[voice] = key;
	md->synth.alloc.alloc_times[voice] = md->synth.alloc.timestamp++;
	mtx_unlock(&md->synth.vc_mtx);

	return (voice);
}

static int
seq_chncommon(sc_p scp, u_char *event)
{
	int unit, i, val, key;
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

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	if (lookup_mididev(scp, dev, LOOKUP_OPEN, &md) != 0)
		return (MORE);

	SEQ_DEBUG(printf("seq_chnvoice: unit %d, dev %d, cmd %s, chn %d, p1 %d, w14 %d.\n",
			 sd->unit,
			 dev,
			 midi_cmdname(cmd, cmdtab_seqccmn),
			 chn,
			 p1,
			 w14));

	switch (cmd) {
	case MIDI_PGM_CHANGE:
		if (scp->seq_mode == SND_DEV_MUSIC) {
			mtx_lock(&md->synth.vc_mtx);
			md->synth.chn_info[chn].pgm_num = p1;
			mtx_unlock(&md->synth.vc_mtx);
			mtx_unlock(&sd->flagqueue_mtx);
			if (md->midiunit >= 0) {
				if (md->synth.setinstr(md, chn, p1) == EAGAIN) {
					mtx_lock(&sd->flagqueue_mtx);
					return (QUEUEFULL);
				}
			}
			mtx_lock(&sd->flagqueue_mtx);
		} else {
			/* For Mode 1. */
			mtx_unlock(&sd->flagqueue_mtx);
			if (md->synth.setinstr(md, chn, p1) == EAGAIN) {	
				mtx_lock(&sd->flagqueue_mtx);
				return (QUEUEFULL);
			}
			mtx_lock(&sd->flagqueue_mtx);
		}
		break;
	case MIDI_CTL_CHANGE:
		/* mtx_lock(&md->giant); */
		if (scp->seq_mode == SND_DEV_MUSIC) {
			if (chn < 16 && p1 < 128) {
				mtx_lock(&md->synth.vc_mtx);
				md->synth.chn_info[chn].controllers[p1] = w14 & 0x7f;
				if (p1 < 32)
					/* We have set the MSB, clear the LSB. */
					md->synth.chn_info[chn].controllers[p1 + 32] = 0;
				if (md->midiunit >= 0) {
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
						if ((md->synth.alloc.map[i] & 0xff00) == key) {
							mtx_unlock(&md->synth.vc_mtx);
							mtx_unlock(&sd->flagqueue_mtx);
							if (md->synth.controller(md, i, p1, val) == EAGAIN) {
								mtx_lock(&sd->flagqueue_mtx);
								return (QUEUEFULL);
							}
							mtx_lock(&sd->flagqueue_mtx);
							mtx_lock(&md->synth.vc_mtx);
						}
					mtx_unlock(&md->synth.vc_mtx);
				} else {
					mtx_unlock(&md->synth.vc_mtx);
					mtx_unlock(&sd->flagqueue_mtx);
					if (md->synth.controller(md, chn, p1, w14) == EAGAIN) {
						mtx_lock(&sd->flagqueue_mtx);
						return (QUEUEFULL);
					}
					mtx_lock(&sd->flagqueue_mtx);
				}
			}
		} else {
			/* For Mode 1. */
			mtx_unlock(&sd->flagqueue_mtx);
			if (md->synth.controller(md, chn, p1, w14) == EAGAIN) {
				mtx_lock(&sd->flagqueue_mtx);
				return (QUEUEFULL);
			}
			mtx_lock(&sd->flagqueue_mtx);
		}
		break;
	case MIDI_PITCH_BEND:
		if (scp->seq_mode == SND_DEV_MUSIC) {
			mtx_lock(&md->synth.vc_mtx);
			md->synth.chn_info[chn].bender_value = w14;
			if (md->midiunit >= 0) {
				/* Handle all of the notes playing on this channel. */
				key = ((int)chn << 8);
				for (i = 0 ; i < md->synth.alloc.max_voice ; i++)
					if ((md->synth.alloc.map[i] & 0xff00) == key) {
						mtx_unlock(&md->synth.vc_mtx);
						mtx_unlock(&sd->flagqueue_mtx);
						if (md->synth.bender(md, i, w14) == EAGAIN) {
							mtx_lock(&sd->flagqueue_mtx);
							return (QUEUEFULL);
						}
						mtx_lock(&sd->flagqueue_mtx);
					}
			} else {
				mtx_unlock(&md->synth.vc_mtx);
				mtx_unlock(&sd->flagqueue_mtx);
				if (md->synth.bender(md, chn, w14) == EAGAIN) {
					mtx_lock(&sd->flagqueue_mtx);
					return (QUEUEFULL);
				}
				mtx_lock(&sd->flagqueue_mtx);
			}
		} else {
			/* For Mode 1. */
			mtx_unlock(&sd->flagqueue_mtx);
			if (md->synth.bender(md, chn, w14) == EAGAIN) {
				mtx_lock(&sd->flagqueue_mtx);
				return (QUEUEFULL);
			}
			mtx_lock(&sd->flagqueue_mtx);
		}
		break;
	}

	return (MORE);
}

static int
seq_timing(sc_p scp, u_char *event)
{
	int unit, ret;
	long parm;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	parm = *(long *)&event[4];

	if (scp->seq_mode == SND_DEV_MUSIC) {
		ret = scp->timer->event(scp->timer, event);
		if (ret == TIMERARMED)
			sd->flags |= SEQ_F_WRITING;
		return (ret);
	}

	SEQ_DEBUG(printf("seq_timing: unit %d, cmd %s, parm %lu.\n",
			 unit, midi_cmdname(event[1], cmdtab_timer), parm));

	ret = MORE;
	switch (event[1]) {
	case TMR_WAIT_REL:
		parm += scp->prev_event_time;
		/* FALLTHRU */
	case TMR_WAIT_ABS:
		if (parm > 0) {
			sd->flags |= SEQ_F_WRITING;
			if (seq_requesttimer(scp, parm))
				ret = TIMERARMED;
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
		if (scp->seq_mode == SND_DEV_MUSIC)
			seq_copytoinput(scp, event, 8);
		else {
			parm = (parm << 8 | SEQ_ECHO);
			seq_copytoinput(scp, (u_char *)&parm, 4);
		}
		break;
	}

	SEQ_DEBUG(printf("seq_timing: timer %s.\n",
			 ret == TIMERARMED ? "armed" : "not armed"));

	return (ret);
}

static int
seq_local(sc_p scp, u_char *event)
{
	int unit;
	seqdev_info *sd;

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

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

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	if (lookup_mididev(scp, event[1], LOOKUP_OPEN, &md) != 0)
		return (MORE);

	l = 0;
	for (i = 0 ; i < 6 && event[i + 2] != 0xff ; i++)
		l = i + 1;
	if (l > 0) {
		mtx_unlock(&sd->flagqueue_mtx);
		if (md->synth.sendsysex(md, &event[2], l) == EAGAIN) {
			mtx_lock(&sd->flagqueue_mtx);
			return (QUEUEFULL);
		}
		mtx_lock(&sd->flagqueue_mtx);
	}

	return (MORE);
}

void
seq_timer(void *arg)
{
	sc_p scp;
	seqdev_info *sd;

	scp = arg;
	sd = scp->devinfo;

	SEQ_DEBUG(printf("seq_timer: unit %d, timer fired.\n", sd->unit));

	/* Record the current timestamp. */
	mtx_lock(&sd->flagqueue_mtx);

	scp->timer_running = 0;
	scp->prev_wakeup_time = seq_gettime();
	seq_startplay(scp);

	mtx_unlock(&sd->flagqueue_mtx);
}

static int
seq_openmidi(sc_p scp, mididev_info *md, int flags, int mode, struct thread *td)
{
	int midiunit, err, insync, chn;

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	midiunit = md->unit;

	SEQ_DEBUG(printf("seq_openmidi: opening midi unit %d.\n", midiunit));

	err = midi_open(MIDIMKDEV(MIDI_CDEV_MAJOR, midiunit, SND_DEV_MIDIN), flags, mode, td);
	if (err != 0) {
		printf("seq_openmidi: failed to open midi device %d.\n", midiunit);
		return (err);
	}
	mtx_lock(&md->synth.status_mtx);
	mtx_lock(&md->flagqueue_mtx);
	md->intr = seq_intr;
	md->intrarg = scp->devinfo;
	mtx_unlock(&md->flagqueue_mtx);
	md->synth.sysex_state = 0;
	if (scp->seq_mode == SND_DEV_MUSIC) {
		for (chn = 0 ; chn < 16 ; chn++) {
			md->synth.chn_info[chn].pgm_num = 0;
			md->synth.reset(md);
			md->synth.chn_info[chn].bender_value = (1 << 7);
		}
	}
	mtx_unlock(&md->synth.status_mtx);

	insync = 0;
	if ((scp->devinfo->flags & SEQ_F_INSYNC) != 0) {
		insync = 1;
		cv_wait(&scp->devinfo->insync_cv, &scp->devinfo->flagqueue_mtx);
	}

	TAILQ_INSERT_TAIL(&scp->midi_open, md, md_linkseq);

	if (insync)
		cv_broadcast(&scp->devinfo->insync_cv);

	return (0);
}

static int
seq_closemidi(sc_p scp, mididev_info *md, int flags, int mode, struct thread *td)
{
	int midiunit, insync;

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	if (md == NULL || !MIDICONFED(md)) {
		SEQ_DEBUG(printf("seq_closemidi: midi device does not exist.\n"));
		return (ENXIO);
	}
	midiunit = md->unit;

	SEQ_DEBUG(printf("seq_closemidi: closing midi unit %d.\n", midiunit));

	midi_close(MIDIMKDEV(MIDI_CDEV_MAJOR, midiunit, SND_DEV_MIDIN), flags, mode, td);
	mtx_lock(&md->flagqueue_mtx);
	md->intr = NULL;
	md->intrarg = NULL;
	mtx_unlock(&md->flagqueue_mtx);

	insync = 0;
	if ((scp->devinfo->flags & SEQ_F_INSYNC) != 0) {
		insync = 1;
		cv_wait(&scp->devinfo->insync_cv, &scp->devinfo->flagqueue_mtx);
	}

	TAILQ_REMOVE(&scp->midi_open, md, md_linkseq);

	if (insync)
		cv_broadcast(&scp->devinfo->insync_cv);

	return (0);
}

static void
seq_panic(sc_p scp)
{
	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	seq_reset(scp);
}

static int
seq_reset(sc_p scp)
{
	int unit, chn, lenw, ret;
	seqdev_info *sd;
	mididev_info *md;
	u_char c[3];

	sd = scp->devinfo;
	unit = sd->unit;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	SEQ_DEBUG(printf("seq_reset: unit %d.\n", unit));

	if ((sd->flags & SEQ_F_INSYNC) != 0)
		cv_wait(&sd->insync_cv, &sd->flagqueue_mtx);

	/* Stop reading and writing. */
	sd->callback(sd, SEQ_CB_ABORT | SEQ_CB_RD | SEQ_CB_WR);

	/* Clear the queues. */
	midibuf_clear(&sd->midi_dbuf_in);
	midibuf_clear(&sd->midi_dbuf_out);

	/* Reset the synthesizers. */
	TAILQ_FOREACH(md, &scp->midi_open, md_linkseq)
		md->synth.reset(md);

	if (scp->seq_mode == SND_DEV_MUSIC) {
		for (chn = 0 ; chn < 16 ; chn++) {
			TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
				mtx_unlock(&sd->flagqueue_mtx);
				ret = 0;
				if (md->synth.controller(md, chn, 123, 0) == EAGAIN /* All notes off. */
				    || md->synth.controller(md, chn, 121, 0) == EAGAIN /* Reset all controllers. */
				    || md->synth.bender(md, chn, 1 << 13) == EAGAIN) /* Reset pitch bend. */
					ret = EAGAIN;
				mtx_lock(&sd->flagqueue_mtx);
				return (ret);
			}
		}
	} else {
		TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
			for (chn = 0 ; chn < 16 ; chn++) {
				mtx_unlock(&sd->flagqueue_mtx);
				c[0] = 0xb0 | (chn & 0x0f);
				c[1] = (u_char)0x78; /* All sound off */
				c[2] = (u_char)0;
				md->synth.writeraw(md, c, 3, &lenw, 0);
				c[1] = (u_char)0x7b; /* All note off */
				md->synth.writeraw(md, c, 3, &lenw, 0);
				c[1] = (u_char)0x79; /* Reset all controller */
				md->synth.writeraw(md, c, 3, &lenw, 0);
				mtx_lock(&sd->flagqueue_mtx);
			}
		}
		seq_sync(scp);
		TAILQ_FOREACH(md, &scp->midi_open, md_linkseq)
			lookup_mididev(scp, md->unit, LOOKUP_CLOSE, NULL);
	}

	return (0);
}

#define SEQ_SYNC_TIMEOUT 8
static int
seq_sync(sc_p scp)
{
	int i, rl;
	seqdev_info *sd;
	mididev_info *md;

	sd = scp->devinfo;

	mtx_assert(&sd->flagqueue_mtx, MA_OWNED);

	SEQ_DEBUG(printf("seq_sync: unit %d.\n", sd->unit));
	sd->flags |= SEQ_F_INSYNC;

	while (sd->midi_dbuf_out.rl >= EV_SZ) {
		if ((sd->flags & SEQ_F_WRITING) == 0)
			sd->callback(sd, SEQ_CB_START | SEQ_CB_WR);
		rl = sd->midi_dbuf_out.rl;
		i = cv_timedwait_sig(&sd->midi_dbuf_out.cv_out, &sd->flagqueue_mtx, SEQ_SYNC_TIMEOUT * hz);
		if (i == EINTR || i == ERESTART) {
			if (i == EINTR)
				sd->callback(sd, SEQ_CB_STOP | SEQ_CB_WR);
			sd->flags &= ~SEQ_F_INSYNC;
			return (i);
		}
		if (i == EWOULDBLOCK && rl == sd->midi_dbuf_out.rl && !scp->timer_running) {
			/* A queue seems to be stuck up. Give up and clear queues. */
			sd->callback(sd, SEQ_CB_STOP | SEQ_CB_WR);
			midibuf_clear(&sd->midi_dbuf_out);
			TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
				mtx_lock(&md->flagqueue_mtx);
				md->callback(md, MIDI_CB_ABORT | MIDI_CB_WR);
				midibuf_clear(&md->midi_dbuf_out);
				mtx_unlock(&md->flagqueue_mtx);
			}
			break;
		}
	}

	/*
	 * Since syncing a midi device might block, unlock sd->flagqueue_mtx.
	 * Keep sd->midi_dbuf_out from writing by setting SEQ_F_INSYNC.
	 * sd->insync_cv is signalled when sync is finished.
	 */
	mtx_unlock(&sd->flagqueue_mtx);

	TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
		mtx_lock(&md->flagqueue_mtx);
		midi_sync(md);
		mtx_unlock(&md->flagqueue_mtx);
	}

	mtx_lock(&sd->flagqueue_mtx);
	sd->flags &= ~SEQ_F_INSYNC;
	cv_broadcast(&sd->insync_cv);

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

	if (MIDIDEV(i_dev) != SND_DEV_SEQ && MIDIDEV(i_dev) != SND_DEV_MUSIC)
		return NULL;
	u = MIDIUNIT(i_dev);
	if (unit)
		*unit = u ;

	return get_seqdev_info_unit(u);
}

/*
 * a small utility function which, given a unit number, returns
 * a pointer to the associated mididev_info struct.
 */
seqdev_info *
get_seqdev_info_unit(int unit)
{
	seqdev_info *sd;

	mtx_lock(&seqinfo_mtx);
	TAILQ_FOREACH(sd, &seq_info, sd_link) {
		if (sd->unit == unit)
			break;
	}
	mtx_unlock(&seqinfo_mtx);

	return sd;
}

/* Create a new sequencer device info structure. */
seqdev_info *
create_seqdev_info_unit(int unit, seqdev_info *seq)
{
	seqdev_info *sd, *sdnew;

	/* As malloc(9) might block, allocate seqdev_info now. */
	sdnew = malloc(sizeof(seqdev_info), M_DEVBUF, M_WAITOK | M_ZERO);
	if (sdnew == NULL)
		return NULL;
	bcopy(seq, sdnew, sizeof(seqdev_info));
	sdnew->unit = unit;
	midibuf_init(&sdnew->midi_dbuf_in);
	midibuf_init(&sdnew->midi_dbuf_out);
	mtx_init(&sdnew->flagqueue_mtx, "seqflq", NULL, MTX_DEF);
	cv_init(&sdnew->insync_cv, "seqins");

	mtx_lock(&seqinfo_mtx);

	TAILQ_FOREACH(sd, &seq_info, sd_link) {
		if (sd->unit == unit) {
			mtx_unlock(&seqinfo_mtx);
			midibuf_destroy(&sdnew->midi_dbuf_in);
			midibuf_destroy(&sdnew->midi_dbuf_out);
			mtx_destroy(&sdnew->flagqueue_mtx);
			cv_destroy(&sdnew->insync_cv);
			free(sdnew, M_DEVBUF);
			return sd;
		}
	}

	mtx_lock(&sdnew->flagqueue_mtx);
	TAILQ_INSERT_TAIL(&seq_info, sdnew, sd_link);
	nseq++;

	mtx_unlock(&seqinfo_mtx);

	return sdnew;
}

/*
 * Look up a midi device by its unit number opened by this sequencer.
 * If the device is not opened and mode is LOOKUP_OPEN, open the device.
 */
static int
lookup_mididev(sc_p scp, int unit, int mode, mididev_info **mdp)
{
	int ret;
	mididev_info *md;

	if (mdp == NULL)
		mdp = &md;

	*mdp = NULL;

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
		if (scp->seq_mode == SND_DEV_MUSIC ? md->unit == unit : md->synthunit == unit) {
			*mdp = md;
			if (mode == LOOKUP_CLOSE)
				return seq_closemidi(scp, md, scp->fflags, MIDIDEV_MODE, curthread);

			return (md != NULL && MIDICONFED(md)) ? 0 : ENXIO;
		}
	}

	if (mode == LOOKUP_OPEN) {
		if (scp->seq_mode == SND_DEV_MUSIC)
			md = get_mididev_info_unit(unit);
		else
			md = get_mididev_synth_unit(unit);
		if (md != NULL) {
			*mdp = md;
			ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curthread);
			return ret;
		}
	}

	return ENXIO;
}

/*
 * Look up a midi device by its midi unit number opened by this sequencer.
 * If the device is not opened and mode is LOOKUP_OPEN, open the device.
 */
static int
lookup_mididev_midi(sc_p scp, int unit, int mode, mididev_info **mdp)
{
	int ret;
	mididev_info *md;

	if (mdp == NULL)
		mdp = &md;

	*mdp = NULL;

	if (scp->seq_mode == SND_DEV_MUSIC)
		return (ENXIO);

	mtx_assert(&scp->devinfo->flagqueue_mtx, MA_OWNED);

	TAILQ_FOREACH(md, &scp->midi_open, md_linkseq) {
		if (md->midiunit == unit) {
			*mdp = md;
			if (mode == LOOKUP_CLOSE)
				return seq_closemidi(scp, md, scp->fflags, MIDIDEV_MODE, curthread);

			return (md != NULL && MIDICONFED(md)) ? 0 : ENXIO;
		}
	}

	if (mode == LOOKUP_OPEN) {
		md = get_mididev_midi_unit(unit);
		if (md != NULL) {
			*mdp = md;
			ret = seq_openmidi(scp, md, scp->fflags, MIDIDEV_MODE, curthread);
			return ret;
		}
	}

	return ENXIO;
}

/* XXX These functions are actually redundant. */
static int
seqopen(dev_t i_dev, int flags, int mode, struct thread *td)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
	case MIDI_DEV_MUSIC:
		return seq_open(i_dev, flags, mode, td);
	}

	return (ENXIO);
}

static int
seqclose(dev_t i_dev, int flags, int mode, struct thread *td)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
	case MIDI_DEV_MUSIC:
		return seq_close(i_dev, flags, mode, td);
	}

	return (ENXIO);
}

static int
seqread(dev_t i_dev, struct uio * buf, int flag)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
	case MIDI_DEV_MUSIC:
		return seq_read(i_dev, buf, flag);
	}

	return (ENXIO);
}

static int
seqwrite(dev_t i_dev, struct uio * buf, int flag)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
	case MIDI_DEV_MUSIC:
		return seq_write(i_dev, buf, flag);
	}

	return (ENXIO);
}

static int
seqioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
	case MIDI_DEV_MUSIC:
		return seq_ioctl(i_dev, cmd, arg, mode, td);
	}

	return (ENXIO);
}

static int
seqpoll(dev_t i_dev, int events, struct thread *td)
{
	switch (MIDIDEV(i_dev)) {
	case MIDI_DEV_SEQ:
	case MIDI_DEV_MUSIC:
		return seq_poll(i_dev, events, td);
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

static void
seq_clone(arg, name, namelen, dev)
	void *arg;
	char *name;
	int namelen;
	dev_t *dev;
{
	int u;

	if (*dev != NODEV)
		return;
	if (bcmp(name, "sequencer", 9) != 0)
		return;
	if (name[10] != '\0' && name[11] != '\0')
		return;
	u = name[9] - '0';
	if (name[10] != '\0') {
		u *= 10;
		u += name[10] - '0';
	}
	seq_initunit(u);
	*dev = MIDIMKDEV(SEQ_CDEV_MAJOR, u, MIDI_DEV_SEQ);
	return;
}
