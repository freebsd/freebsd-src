/*
 * Copyright (c) 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	isdnphone - audio operations
 *      ============================
 *
 *	$Id: audio.c,v 1.5 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:52:39 1999]
 *
 *----------------------------------------------------------------------------*/

#include "defs.h"

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
init_audio(char *audiodevice)
{
	int ret;
	int fd;
	u_long fmt = 0;

	snd_chan_param pa;
	struct snd_size sz;
	snd_capabilities soundcaps;
	
	if((fd = open(audiodevice, O_RDWR)) < 0)
	{
		fprintf(stderr, "unable to open %s: %s\n", audiodevice, strerror(errno));
		return(-1);
	}

	ret = ioctl(fd, AIOGCAP, &soundcaps);	

	if(ret == -1)
	{
		fprintf(stderr, "ERROR: ioctl AIOGCAP %s: %s\n", audiodevice, strerror(errno));
		return(-1);
	}

	fmt = soundcaps.formats;

	if((fmt & AFMT_FULLDUPLEX) && (!(fmt & AFMT_WEIRD)))
	{
#ifdef NOTDEF
			if(fmt & AFMT_A_LAW)
			{
				play_fmt = rec_fmt = AFMT_A_LAW;
			}
			else
#endif
			if(fmt & AFMT_MU_LAW)
			{
				play_fmt = rec_fmt = AFMT_MU_LAW;
			}
			else
			{
				printf("sorry, A-law or u-law not supported!\n");
				close(fd);
				return(-1);
			}
	}
	else
	{
		printf("no full-duplex available!\n");
		close (fd);
		return(-1);
	}
	
	pa.play_format = play_fmt;
	pa.rec_format = rec_fmt;
	pa.play_rate = pa.rec_rate = AUDIORATE;

	ret = ioctl(fd, AIOSFMT, &pa);
	
	if(ret == -1)
	{
		fprintf(stderr, "ERROR: ioctl AIOSFMT %s: %s\n", audiodevice, strerror(errno));
		return(-1);
	}

	sz.play_size = BCH_MAX_DATALEN;
	sz.rec_size = BCH_MAX_DATALEN;

	ret = ioctl(fd, AIOSSIZE, &sz);

	if(ret == -1)
	{
		fprintf(stderr, "ERROR: ioctl AIOSSIZE %s: %s\n", audiodevice, strerror(errno));
		return(-1);
	}

	return(fd);
}
	
/*---------------------------------------------------------------------------*
 *	audio device has speech data from microphone
 *---------------------------------------------------------------------------*/
void
audio_hdlr(void)
{
	unsigned char buffer[BCH_MAX_DATALEN];
	int ret;

	ret = read(audiofd, buffer, BCH_MAX_DATALEN);
	
	if(ret < 0)
	{
		fatal("read audio failed: %s", strerror(errno));
	}

	debug("audio_hdlr: read %d bytes\n", ret);

	if(ret > 0)
	{
		telwrite(ret, buffer);
	}
}

/*---------------------------------------------------------------------------*
 *	write audio data to loudspeaker
 *---------------------------------------------------------------------------*/
void
audiowrite(int len, unsigned char *buf)
{
	if((write(audiofd, buf, len)) < 0)
	{
		fatal("write audio failed: %s", strerror(errno));
	}
}

/* EOF */
