/*-
 * Copyright (c) 1994 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: pcaudioio.h,v 1.6 1996/07/17 20:18:42 joerg Exp $
 */

#ifndef	_MACHINE_PCAUDIOIO_H_
#define	_MACHINE_PCAUDIOIO_H_

#include <sys/ioccom.h>

typedef struct audio_prinfo {
	unsigned	sample_rate;	/* samples per second */
	unsigned	channels;	/* # of channels (interleaved) */
	unsigned	precision;	/* sample size in bits */
	unsigned	encoding;	/* encoding method used */

	unsigned	gain;		/* volume level: 0 - 255 */
	unsigned	port;		/* input/output device */
	unsigned	_fill1[4];

	unsigned	samples;	/* samples played */
	unsigned	eof;		/* ?!? */
	unsigned char	pause;		/* !=0 pause, ==0 continue */
	unsigned char	error;		/* !=0 if overflow/underflow */
	unsigned char	waiting;	/* !=0 if others wants access */
	unsigned char	_fill2[3];

	unsigned char	open; 		/* is device open */
	unsigned char	active;		/* !=0 if sound hardware is active */
} audio_prinfo_t;

typedef struct audio_info {
	audio_prinfo_t	play;
	audio_prinfo_t	record;
	unsigned	monitor_gain;
	unsigned	_fill[4];
} audio_info_t;

#define	AUDIO_ENCODING_ULAW	(1)	/* u-law encoding */
#define	AUDIO_ENCODING_ALAW	(2)	/* A-law encoding */
#define	AUDIO_ENCODING_RAW	(3)	/* linear encoding */

#define	AUDIO_MIN_GAIN		(0)	/* minimum volume value */
#define	AUDIO_MAX_GAIN		(255)	/* maximum volume value */

#define	AUDIO_INITINFO(i)	memset((void*)i, 0xff, sizeof(audio_info_t))

#define	AUDIO_GETINFO		_IOR('A', 1, audio_info_t)
#define	AUDIO_SETINFO		_IOWR('A', 2, audio_info_t)
#define	AUDIO_DRAIN		_IO('A', 3)
#define	AUDIO_FLUSH		_IO('A', 4)

/* compatibility to /dev/audio */
#define AUDIO_COMPAT_DRAIN	_IO('P', 1)
#define AUDIO_COMPAT_FLUSH	_IO('P', 0)

#endif /* !_MACHINE_PCAUDIOIO_H_ */
