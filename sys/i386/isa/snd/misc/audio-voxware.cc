/*
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * Full Duplex audio module for the new sound driver and full duplex
 * cards. Luigi Rizzo, from original sources supplied by Amancio Hasty.
 *
 * This includes some enhancements:
 * - the audio device to use can be in the AUDIODEV env. variable.
 *   It can be either a unit number or a full pathname;
 * - use whatever format is available from the card (included split
 *   format e.g. for the sb16);
 * - limit the maximum size of the playout queue to approx 4 frames;
 *   this is necessary if the write channel is slower than expected;
 *   the fix is based on two new ioctls, AIOGCAP and AIONWRITE,
 *   but the code should compile with the old driver as well.
 */

#include <osfcn.h>
#include <machine/soundcard.h>
#include "audio.h"
#include "mulaw.h"
#include "Tcl.h"

#define ULAW_ZERO 0x7f

extern const u_char lintomulawX[];

class VoxWare : public Audio {
    public:
	VoxWare();
	virtual int FrameReady();
	virtual u_char* Read();
	virtual	void Write(u_char *);
	virtual void SetRGain(int);
	virtual void SetPGain(int);
	virtual void OutputPort(int);
	virtual void InputPort(int);
	virtual void Obtain();
	virtual void Release();
	virtual void RMute();
	virtual void RUnmute();
	virtual int HalfDuplex() const;
    protected:
	int ext_fd; /* source for external file */

	u_char* readbuf;
	u_short *s16_buf;

	int play_fmt ;
#if defined(AIOGCAP) /* new sound driver */
	int rec_fmt ; /* the sb16 has split format... */
	snd_capabilities soundcaps;
#endif
};

static class VoxWareMatcher : public Matcher {
public:
    VoxWareMatcher() : Matcher("audio") {}
    TclObject* match(const char* fmt) {
	if (strcmp(fmt, "voxware") == 0)
	    return (new VoxWare);
	return (0);
    }
} linux_audio_matcher;

VoxWare::VoxWare()
{
    readbuf = new u_char[blksize];
    s16_buf = new u_short[blksize];

    memset(readbuf, ULAW_ZERO, blksize);

    ext_fd = -1 ; /* no external audio */
    iports = 4; /* number of input ports */
}

void
VoxWare::Obtain()
{
    char *thedev;
    char buf[64];
    int d = -1;

    if (HaveAudio())
	abort();
    thedev=getenv("AUDIODEV");
    if (thedev==NULL)
	thedev="/dev/audio";
    else if ( thedev[0] >= '0' && thedev[0] <= '9' ) {
	d = atoi(thedev);
	sprintf(buf,"/dev/audio%d", d);
	thedev = buf ;
    }
    fd = open(thedev, O_RDWR );
    thedev=getenv("MIXERDEV");
    if (thedev == NULL)
	if (d < 0)
	    thedev = "/dev/mixer";
	else {
	    sprintf(buf,"/dev/mixer%d", d);
	    thedev = buf ;
	}
	    
    if (fd >= 0) {
	int i = -1 ;
#if defined(AIOGCAP) /* new sound driver */
	i = ioctl(fd, AIOGCAP, &soundcaps);
	if (i == 0) {
	    snd_chan_param pa;
	    struct snd_size sz;

	    pa.play_rate = pa.rec_rate = 8000 ;
	    pa.play_format = pa.rec_format = AFMT_MU_LAW ;
	    switch (soundcaps.formats & (AFMT_FULLDUPLEX | AFMT_WEIRD)) {
	    case AFMT_FULLDUPLEX :
		/*
		 * this entry for cards with decent full duplex. Use s16
		 * preferably (some are broken in ulaw) or ulaw or u8 otherwise.
		 */
		if (soundcaps.formats & AFMT_S16_LE)
		    pa.play_format = pa.rec_format = AFMT_S16_LE ;
		else if (soundcaps.formats & AFMT_MU_LAW)
		    pa.play_format = pa.rec_format = AFMT_MU_LAW ;
		else if (soundcaps.formats & AFMT_U8)
		    pa.play_format = pa.rec_format = AFMT_U8 ;
		else {
		    printf("sorry, no supported formats\n");
		    close(fd);
		    fd = -1 ;
		    return;
		}
		break ;
	    case AFMT_FULLDUPLEX | AFMT_WEIRD :
		/* this is the sb16... */
		if (soundcaps.formats & AFMT_S16_LE) {
		    pa.play_format = AFMT_U8 ;
		    pa.rec_format = AFMT_S16_LE;
		} else {
		    printf("sorry, no supported formats\n");
		    close(fd);
		    fd = -1 ;
		    return;
		}
		break ;
	    default :
		printf("sorry don't know how to deal with this card\n");
		close (fd);
		fd = -1;
		break;
	    }
	    ioctl(fd, AIOSFMT, &pa);
	    play_fmt = pa.play_format ;
	    rec_fmt = pa.rec_format ;
	    sz.play_size = (play_fmt == AFMT_S16_LE) ? 2*blksize : blksize; 
	    sz.rec_size = (rec_fmt == AFMT_S16_LE) ? 2*blksize : blksize; 
	    ioctl(fd, AIOSSIZE, &sz);
	} else
#endif
	{ /* setup code for old voxware driver */
	}
	Audio::Obtain();
    }
}

/*
 * note: HalfDuplex() uses a modified function of the new driver,
 * which will return AFMT_FULLDUPLEX set in SNDCTL_DSP_GETFMTS
 * for full-duplex devices. In the old driver this was 0 so
 * the default is to use half-duplex for them. Note also that I have
 * not tested half-duplex operation.
 */
int
VoxWare::HalfDuplex() const
{
    int i;
    ioctl(fd, SNDCTL_DSP_GETFMTS, &i);
#if 0
    printf("SNDCTL_DSP_GETFMTS returns 0x%08x %s duplex\n",
	i, i & AFMT_FULLDUPLEX ? "full":"half");
#endif
    return (i & AFMT_FULLDUPLEX) ? 0 : 1 ;
}

void VoxWare::Release()
{
    if (HaveAudio()) {
	Audio::Release();
    }
}

void VoxWare::Write(u_char *cp)
{
    int i = blksize, l;
    static int peak = 0;

    if (play_fmt == AFMT_S16_LE) {
	for (i=0; i< blksize; i++)
	    s16_buf[i] = mulawtolin[cp[i]] ;
	cp = (u_char *)s16_buf;
        i = 2 *blksize ;
    }
    else if (play_fmt == AFMT_S8) {
	for (i=0; i< blksize; i++) {
	    int x = mulawtolin[cp[i]] ;
	    x =  (x >> 8 ) & 0xff;
	    cp[i] = (u_char)x ;
	}
	i = blksize ;
    } else if (play_fmt == AFMT_U8) {
	/*
	 * when translating to 8-bit formats, need to implement AGC
	 * to avoid loss of resolution in the conversion.
	 * The peak is multiplied by 2^13
	 */
	for (i=0; i< blksize; i++) {
	    int x = mulawtolin[cp[i]] ;
#if 0 /* AGC -- still not complete... */
	    if (x < 0) x = -x ;
	    if (x > peak) peak =  ( peak*16 + x - peak ) / 16 ;
	    else peak =  ( peak*8192 + x - peak ) / 8192 ;
	    if (peak < 128) peak = 128 ;
	    /* at this point peak is in the range 128..32k
	     * samples can be scaled and clipped consequently.
	     */
	    x = x * 32768/peak ;
	    if (x > 32767) x = 32767;
	    else if (x < -32768) x = -32768;
#endif
	    x =  (x >> 8 ) & 0xff;
	    x = (x ^ 0x80) & 0xff ;
	    cp[i] = (u_char)x ;
	}
	i = blksize ;
    }
#if 0 && defined(AIOGCAP)
    int queued;
    ioctl(fd, AIONWRITE, &queued);
    queued = soundcaps.bufsize - queued ;
    if (play_fmt == AFMT_S16_LE) {
	if (queued > 8*blksize)
	    i -= 8 ;
    } else {
	if (queued > 4*blksize)
	    i -= 4 ;
    }
#endif
    for ( ; i > 0 ; i -= l) {
	l = write(fd, cp, i); 
	cp += l;
    }
}

u_char* VoxWare::Read()
{
    u_char* cp;
    int l=0, l0 = blksize,  i = blksize;
    static int smean = 0 ; /* smoothed mean to remove DC */
    static int loops = 20 ;

    cp = readbuf; 

    if (rec_fmt == AFMT_S16_LE) {
	cp = (u_char *)s16_buf;
        l0 = i = 2 *blksize ;
    }
    for ( ; i > 0 ; i -= l ) {
	l = read(fd, cp, i);
        cp += l ;
    }
    if (rec_fmt == AFMT_S16_LE) {
	for (i=0; i< blksize; i++) {
#if 1 /* remove DC component... */
	    int mean = smean >> 13;
	    int dif = ((short) s16_buf[i]) - mean;
	    smean += dif ;
	    readbuf[i] = lintomulawX[ dif & 0x1ffff ] ;
#else
	    readbuf[i] = lintomulaw[ s16_buf[i] ] ;
#endif
	}
    }
    else if (rec_fmt == AFMT_S8) {
	for (i=0; i< blksize; i++)
	    readbuf[i] = lintomulaw[ readbuf[i]<<8 ] ;
    }
    else if (rec_fmt == AFMT_U8) {
	for (i=0; i< blksize; i++)
	    readbuf[i] = lintomulaw[ (readbuf[i]<<8) ^ 0x8000 ] ;
    }
    if (iport == 3) {
	l = read(ext_fd, readbuf, blksize);
	if (l < blksize) {
	    lseek(ext_fd, (off_t) 0, 0);
	    read(ext_fd, readbuf+l, blksize - l);
	}
    }
    return readbuf;
}

/*
 * should check that I HaveAudio() before trying to set gain.
 *
 * In most mixer devices, there is only a master volume control on
 * the capture channel, so the following code does not really work
 * as expected. The only (partial) exception is the MIC line, where
 * there is generally a 20dB boost which can be enabled or not
 * depending on the type of device.
 */
void VoxWare::SetRGain(int level)
{    
    double x = level;
    level = (int) (x/2.56);
    int foo = (level<<8) | level;
    if (!HaveAudio())
	Obtain();
    switch (iport) {
    case 2:
    case 1:
	break;
    case 0:
	if (ioctl(fd, MIXER_WRITE(SOUND_MIXER_MIC), &foo) == -1)
	    printf("failed to set mic volume \n");
	break;
    }
    if (ioctl(fd, MIXER_WRITE(SOUND_MIXER_IGAIN), &foo) == -1)
       printf("failed set input line volume \n");
    rgain = level;
}

void VoxWare::SetPGain(int level)
{
    float x = level;
    level = (int) (x/2.56);
    int foo = (level<<8) | level;
    if (ioctl(fd, MIXER_WRITE(SOUND_MIXER_PCM), &foo) == -1) {
	printf("failed to output level %d \n", level);
    }
    pgain = level;
}

void VoxWare::OutputPort(int p)
{
    oport = p;
}

void VoxWare::InputPort(int p)
{
    int   src = 0;

    if (ext_fd >=0 && p != 3) {
	close(ext_fd);
	ext_fd = -1 ;
    }
	
    switch(p) {
    case 3:
	fprintf(stderr,"input from file %s\n", ext_fname);
	if (ext_fd == -1)
	    ext_fd = open(ext_fname, 0);
	if (ext_fd != -1)
	    lseek(ext_fd, (off_t) 0, 0);
	break; 
    case 2:
	src = 1 << SOUND_MIXER_LINE;
	break;
    case 1: /* cd ... */
	src = 1 << SOUND_MIXER_CD;
	break;
    case 0 :
	src = 1 << SOUND_MIXER_MIC;
	break;
    }
    if ( ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &src) == -1 ) {
	printf("failed to select input \n");
        p = 0;
    }
    iport = p;
}

void VoxWare::RMute()
{
    rmute |= 1;
}

void VoxWare::RUnmute()
{
    rmute &=~ 1;
}

/*
 * FrameReady must return 0 every so often, or the system will keep
 * processing mike data and not other events.
 */
int VoxWare::FrameReady()
{
    int i = 0;
    int lim = blksize;

    ioctl(fd, FIONREAD, &i );
    if (rec_fmt == AFMT_S16_LE) lim = 2*blksize;
    return (i >= lim) ? 1 : 0 ;
}
/*** end of file ***/
