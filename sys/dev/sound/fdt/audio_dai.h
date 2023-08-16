/*-
 * Copyright (c) 2019 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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
 */

#ifndef __DAI_H__
#define __DAI_H__

#define	AUDIO_DAI_FORMAT_I2S		0
#define	AUDIO_DAI_FORMAT_RJ		1
#define	AUDIO_DAI_FORMAT_LJ		2
#define	AUDIO_DAI_FORMAT_DSPA		3
#define	AUDIO_DAI_FORMAT_DSPB		4
#define	AUDIO_DAI_FORMAT_AC97		5
#define	AUDIO_DAI_FORMAT_PDM		6

/*
 * Polarity: Normal/Inverted BCLK/Frame
 */
#define	AUDIO_DAI_POLARITY_NB_NF	0
#define	AUDIO_DAI_POLARITY_NB_IF	1
#define	AUDIO_DAI_POLARITY_IB_NF	2
#define	AUDIO_DAI_POLARITY_IB_IF	3
#define	AUDIO_DAI_POLARITY_INVERTED_FRAME(n)	((n) & 0x01)
#define	AUDIO_DAI_POLARITY_INVERTED_BCLK(n)	((n) & 0x2)

#define	AUDIO_DAI_CLOCK_CBM_CFM		0
#define	AUDIO_DAI_CLOCK_CBS_CFM		1
#define	AUDIO_DAI_CLOCK_CBM_CFS		2
#define	AUDIO_DAI_CLOCK_CBS_CFS		3

#define	AUDIO_DAI_CLOCK_IN		0
#define	AUDIO_DAI_CLOCK_OUT		1

#define	AUDIO_DAI_JACK_HP		0
#define	AUDIO_DAI_JACK_MIC		1

/*
 * Signal to audio_soc that chn_intr required
 * for either recording or playback
 */
#define	AUDIO_DAI_REC_INTR		(1 << 1)
#define	AUDIO_DAI_PLAY_INTR		(1 << 0)

#define	AUDIO_DAI_FORMAT(fmt, pol, clk)		(((fmt) << 16) | ((pol) << 8) | (clk))
#define	AUDIO_DAI_FORMAT_FORMAT(format)		(((format) >> 16) & 0xff)
#define	AUDIO_DAI_FORMAT_POLARITY(format)	(((format) >> 8) & 0xff)
#define	AUDIO_DAI_FORMAT_CLOCK(format)		(((format) >> 0) & 0xff)


#endif /* __DAI_H__ */
