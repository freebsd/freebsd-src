/*
 * sound/sound_switch.c
 * 
 * The system call switch
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

#include <i386/isa/sound/sound_config.h>

#if NSND > 0

#define	SNDSTAT_BUF_SIZE	4000

/*
 * /dev/sndstatus -device
 */
static char    *status_buf = NULL;
static int      status_len, status_ptr;
static int      status_busy = 0;

static int
put_status(char *s)
{
    int             l = strlen(s);

    if (status_len + l >= SNDSTAT_BUF_SIZE)
	return 0;

    bcopy(s, &status_buf[status_len], l);
    status_len += l;

    return 1;
}

/*
 * in principle, we never overflow the buffer. But... if radix=2 ...
 * and if smaller... lr970711
 */

static int
put_status_int(u_int val, int radix)
{
    int             l, v;
    static char     hx[] = "0123456789abcdef";
    char            buf[33]; /* int is 32 bit+null, in base 2 */

    if (radix < 2 || radix > 16)	/* better than panic */
	return put_status("???");

    if (!val)
	return put_status("0");

    l = 0;
    buf[10] = 0;

    while (val) {
	v = val % radix;
	val = val / radix;

	buf[9 - l] = hx[v];
	l++;
    }

    if (status_len + l >= SNDSTAT_BUF_SIZE)
	return 0;

    bcopy(&buf[10 - l], &status_buf[status_len], l);
    status_len += l;

    return 1;
}

static void
init_status(void)
{
    /*
     * Write the status information to the status_buf and update
     * status_len. There is a limit of SNDSTAT_BUF_SIZE bytes for the data.
     * put_status handles this and returns 0 in case of failure. Since
     * it never oveflows the buffer, we do not care to check.
     */

    int             i;

    status_ptr = 0;

#ifdef SOUND_UNAME_A
    put_status("VoxWare Sound Driver:" SOUND_VERSION_STRING
	       " (" SOUND_CONFIG_DATE " " SOUND_CONFIG_BY ",\n"
	       SOUND_UNAME_A ")\n");
#else
    put_status("VoxWare Sound Driver:" SOUND_VERSION_STRING
	       " (" SOUND_CONFIG_DATE " " SOUND_CONFIG_BY "@"
	       SOUND_CONFIG_HOST "." SOUND_CONFIG_DOMAIN ")\n");
#endif

    put_status("Config options: ") ;
    /*   put_status_int(SELECTED_SOUND_OPTIONS, 16) ; */
    put_status("\n\nInstalled drivers: \n") ;

    for (i = 0; i < num_sound_drivers; i++)
	if (sound_drivers[i].card_type != 0) {
	    put_status("Type ") ;
	    put_status_int(sound_drivers[i].card_type, 10);
	    put_status(": ") ;
	    put_status(sound_drivers[i].name) ;
	    put_status("\n") ;
	}
    put_status("\n\nCard config: \n") ;

    for (i = 0; i < num_sound_cards; i++)
	if (snd_installed_cards[i].card_type != 0) {
	    int             drv, tmp;

	    if (!snd_installed_cards[i].enabled)
		put_status("(") ;

	    if ((drv = snd_find_driver(snd_installed_cards[i].card_type)) != -1)
		put_status(sound_drivers[drv].name) ;

	    put_status(" at 0x") ;
	    put_status_int(snd_installed_cards[i].config.io_base, 16);

	    put_status(" irq ") ;
	    tmp = snd_installed_cards[i].config.irq;
	    if (tmp < 0)
		tmp = -tmp;
	    put_status_int(tmp, 10) ;

	    if (snd_installed_cards[i].config.dma != -1) {
		put_status(" drq ") ;
		put_status_int(snd_installed_cards[i].config.dma, 10) ;
		if (snd_installed_cards[i].config.dma2 != -1) {
		    put_status(",") ;
		    put_status_int(snd_installed_cards[i].config.dma2, 10) ;
		}
	    }
	    if (!snd_installed_cards[i].enabled)
		put_status(")") ;

	    put_status("\n") ;
	}
    if (!sound_started) {
	put_status("\n\n***** Sound driver not started *****\n\n");
	return;
    }
#ifndef CONFIG_AUDIO
    put_status("\nAudio devices: NOT ENABLED IN CONFIG\n") ;
#else
    put_status("\nAudio devices:\n") ;

    for (i = 0; i < num_audiodevs; i++) {
	put_status_int(i, 10) ;
	put_status(": ") ;
	put_status(audio_devs[i]->name) ;

	if (audio_devs[i]->flags & DMA_DUPLEX)
	    put_status(" (DUPLEX)") ;

	put_status("\n") ;
    }
#endif

#ifndef CONFIG_SEQUENCER
    put_status("\nSynth devices: NOT ENABLED IN CONFIG\n");
#else
    put_status("\nSynth devices:\n") ;

    for (i = 0; i < num_synths; i++) {
	put_status_int(i, 10) ;
	put_status(": ") ;
	put_status(synth_devs[i]->info->name) ;
	put_status("\n") ;
    }
#endif

#ifndef CONFIG_MIDI
    put_status("\nMidi devices: NOT ENABLED IN CONFIG\n") ;
#else
    put_status("\nMidi devices:\n") ;

    for (i = 0; i < num_midis; i++) {
	put_status_int(i, 10) ;
	put_status(": ") ;
	put_status(midi_devs[i]->info.name) ;
	put_status("\n") ;
    }
#endif

    put_status("\nTimers:\n");

    for (i = 0; i < num_sound_timers; i++) {
	put_status_int(i, 10);
	put_status(": ");
	put_status(sound_timer_devs[i]->info.name);
	put_status("\n");
    }

    put_status("\nMixers:\n");

    for (i = 0; i < num_mixers; i++) {
	put_status_int(i, 10);
	put_status(": ");
	put_status(mixer_devs[i]->name);
	put_status("\n");
    }
}

static int
read_status(snd_rw_buf * buf, int count)
{
    /*
     * Return at most 'count' bytes from the status_buf.
     */
    int             l, c;

    l = count;
    c = status_len - status_ptr;

    if (l > c)
	l = c;
    if (l <= 0)
	return 0;


    if (uiomove(&status_buf[status_ptr], l, buf)) {
	printf("sb: Bad copyout()!\n");
    };
    status_ptr += l;

    return l;
}

int
sound_read_sw(int dev, struct fileinfo * file, snd_rw_buf * buf, int count)
{
    DEB(printf("sound_read_sw(dev=%d, count=%d)\n", dev, count));

    switch (dev & 0x0f) {
    case SND_DEV_STATUS:
	return read_status(buf, count);
	break;

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
	return audio_read(dev, file, buf, count);
	break;
#endif

#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
	return sequencer_read(dev, file, buf, count);
	break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
	return MIDIbuf_read(dev, file, buf, count);
#endif

    default:
	printf("Sound: Undefined minor device %d\n", dev);
    }

    return -(EPERM);
}

int
sound_write_sw(int dev, struct fileinfo * file, snd_rw_buf * buf, int count)
{

    DEB(printf("sound_write_sw(dev=%d, count=%d)\n", dev, count));

    switch (dev & 0x0f) {

#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
	return sequencer_write(dev, file, buf, count);
	break;
#endif

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
	return audio_write(dev, file, buf, count);
	break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
	return MIDIbuf_write(dev, file, buf, count);
#endif

    default:
	return -(EPERM);
    }

    return count;
}

int
sound_open_sw(int dev, struct fileinfo * file)
{
    int             retval;

    DEB(printf("sound_open_sw(dev=%d)\n", dev));

    if ((dev >= SND_NDEVS) || (dev < 0)) {
	printf("Invalid minor device %d\n", dev);
	return -(ENXIO);
    }
    switch (dev & 0x0f) {
    case SND_DEV_STATUS:
	if (status_busy)
	    return -(EBUSY);
	status_busy = 1;
	if ((status_buf = (char *) malloc(SNDSTAT_BUF_SIZE, M_TEMP, M_WAITOK)) == NULL)
	    return -(EIO);
	status_len = status_ptr = 0;
	init_status();
	break;

    case SND_DEV_CTL:
	return 0;
	break;

#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
	if ((retval = sequencer_open(dev, file)) < 0)
	    return retval;
	break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
	if ((retval = MIDIbuf_open(dev, file)) < 0)
	    return retval;
	break;
#endif

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
	if ((retval = audio_open(dev, file)) < 0)
	    return retval;
	break;
#endif

    default:
	printf("Invalid minor device %d\n", dev);
	return -(ENXIO);
    }

    return 0;
}

void
sound_release_sw(int dev, struct fileinfo * file)
{

    DEB(printf("sound_release_sw(dev=%d)\n", dev));

    switch (dev & 0x0f) {
    case SND_DEV_STATUS:
	if (status_buf)
	    free(status_buf, M_TEMP);
	status_buf = NULL;
	status_busy = 0;
	break;

    case SND_DEV_CTL:
	break;

#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
	sequencer_release(dev, file);
	break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
	MIDIbuf_release(dev, file);
	break;
#endif

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
	audio_release(dev, file);
	break;
#endif

    default:
	printf("Sound error: Releasing unknown device 0x%02x\n", dev);
    }
}

int
sound_ioctl_sw(int dev, struct fileinfo * file, u_int cmd, ioctl_arg arg)
{
    DEB(printf("sound_ioctl_sw(dev=%d, cmd=0x%x, arg=0x%x)\n", dev, cmd, arg));

    if (((cmd >> 8) & 0xff) == 'M' && num_mixers > 0)	/* Mixer ioctl */
	if ((dev & 0x0f) != SND_DEV_CTL) {
	    int             dtype = dev & 0x0f;
	    int             mixdev;

	    switch (dtype) {
#ifdef CONFIG_AUDIO
	    case SND_DEV_DSP:
	    case SND_DEV_DSP16:
	    case SND_DEV_AUDIO:
		mixdev = audio_devs[dev >> 4]->mixer_dev;
		if (mixdev < 0 || mixdev >= num_mixers)
		    return -(ENXIO);
		return mixer_devs[mixdev]->ioctl(mixdev, cmd, arg);
		break;
#endif

	    default:
		return mixer_devs[0]->ioctl(0, cmd, arg);
	    }
	}
    switch (dev & 0x0f) {

    case SND_DEV_CTL:

	if (!num_mixers)
	    return -(ENXIO);

	dev = dev >> 4;

	if (dev >= num_mixers)
	    return -(ENXIO);

	return mixer_devs[dev]->ioctl(dev, cmd, arg);
	    break;

#ifdef CONFIG_SEQUENCER
    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
	return sequencer_ioctl(dev, file, cmd, arg);
	break;
#endif

#ifdef CONFIG_AUDIO
    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
	return audio_ioctl(dev, file, cmd, arg);
	break;
#endif

#ifdef CONFIG_MIDI
    case SND_DEV_MIDIN:
	return MIDIbuf_ioctl(dev, file, cmd, arg);
	break;
#endif

    default:
	return -(EPERM);
	break;
    }

    return -(EPERM);
}

#endif
