/*
   SCCS: @(#) auvoxware.c 11.4 95/04/14 
*/
/*-------------------------------------------------------------------------

Copyright (C) 1995 The Santa Cruz Operation, Inc.
All Rights Reserved.

Permission to use, copy, modify and distribute this software
for any purpose is hereby granted without fee, provided that the 
above copyright notice and this notice appear in all copies
and that both the copyright notice and this notice appear in
supporting documentation.  SCO makes no representations about
the suitability of this software for any purpose.  It is provided
"AS IS" without express or implied warranty.

SCO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  
IN NO EVENT SHALL SCO BE LIABLE FOR ANY SPECIAL, INDIRECT, 
PUNITIVE, CONSEQUENTIAL OR INCIDENTAL DAMAGES OR ANY DAMAGES 
WHATSOEVER RESULTING FROM LOSS OF USE, LOSS OF DATA OR LOSS OF
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER 
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR 
PERFORMANCE OF THIS SOFTWARE.

-------------------------------------------------------------------------*/
/*
   AUVoxConfig additions (sysseh@devetir.qld.gov.au)
   96-01-15
	Put the following  keywords in -
		minrate		-	Minimum sampling rate
		maxrate		-	Maximum sampling rate
		fragsize	-	The fragment size
		minfrags	-	Minimum number of frags in queue
		maxfrags	-	Maximum fragments in queue
		wordsize	-	8 or 16 bit samples
		device		-	What device file to use
		numchans	-	Mono (1) or stereo (2)
		debug		-	Output messages during operation
		verbose		-	Be chatty about config
		inputsection	-	Next lot of specs are for input
		outputsection	-	Next specs are for output
		end		-	End an input or output section
*/
/*
   SCO Modification History:
   S005, 24-Apr-95, shawnm@sco.com
	base # of driver buffer fragments on data rate
   S004, 12-Apr-95, shawnm@sco.com
	finish integration of ausco.c, fix setitimer calls
   S003, 28-Mar-95, shawnm@sco.com, sysseh@devetir.qld.gov.au
	incorporate patch for stereo/mono mixing from Stephen Hocking
   S002, 21-Mar-95, shawnm@sco.com
	incorporate signal handling and audio block/unblock from ausco.c
   S001, 21-Mar-95, shawnm@sco.com, sysseh@devetir.qld.gov.au
	SYSSEH incorporate parts of patch from Stephen Hocking
*/
/*
 * Copyright 1993 Network Computing Devices, Inc. Copyright (C) Siemens
 * Nixdorf Informationssysteme AG 1993
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc.  or
 * Siemens Nixdorf Informationssysteme AG not be used in advertising or
 * publicity pertaining to distribution of this software without specific,
 * written prior permission.
 * 
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC. AND
 * SIEMENS NIXDORF INFORMATIONSSYSTEME AG DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK COMPUTING DEVICES, INC. NOR
 * SIEMENS NIXDORF INFORMATIONSSYSTEME AG BE LIABLE FOR ANY DAMAGES
 * WHATSOEVER, INCLUDING SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOSS OF USE, DATA, OR PROFITS, EVEN IF ADVISED OF THE
 * POSSIBILITY THEREOF, AND REGARDLESS OF WHETHER IN AN ACTION IN CONTRACT,
 * TORT OR NEGLIGENCE, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * 
 * $NCDId: @(#)auvoxware.c,v 1.10 1996/04/24 17:04:19 greg Exp $
 * 
 * Copyright (C) Siemens Nixdorf Informationssysteme AG 1993 All rights reserved
 */

/*
 * Originally from the merge of auvoxware by Amancio Hasty (hasty@netcom.com)
 * & auvoxsvr4 by Stephen Hocking (sysseh@devetir.qld.gov.au).
 * 16bit fixes and Linux patches supplied by Christian
 * Schlichtherle (s_schli@ira.uka.de).
 *
 * BUGS:
 * - When the soundcard can do only 8 bit recording, "aurecord" records
 *   twice as long as it should. Is this our fault?
 *
 * TODO:
 * - Adapt the buffer sizes to the current sampling rate,
 *   so that we can record/play high quality audio samples without
 *   swallows/pauses.
 *   Note that setting the buffer size to a fixed maximum will not work,
 *   because it causes playing at slow sample rate to pause. :-(
 *   I already tried to do this, but it seems that the rest of the server
 *   code doesn't recognize the changed buffer sizes. Any help in doing
 *   this is welcome!
 *   [chris]
 * - Support a second input channel for stereo sampling,
 *   so that microphone sampling is done on the mono channel
 *   while line sampling is done on the stereo channel.
 *   [chris]
 *
 * CHANGELOG:
 * - 94/7/2:
 *   Completely rewrote this file. Features:
 *   + Makes use of two sound cards if available.
 *     So you can concurrently record and play samples.
 *   + Tested to work with all combinations of 8/16 bit, mono/stereo
 *     sound card sampling modes.
 *   + Uses a stereo input channel if hardware supports this.
 *   + Can play stereo samples on mono sound cards (but who cares?).
 *   + Always uses the highest possible audio quality, i.e. 8/16 bit and
 *     mono/stereo parameters are fixed while only the sampling rate is
 *     variable. This reduces swallows and pauses to the (currently)
 *     unavoidable minimum while consuming a little bit more cpu time.
 *   + Format conversion stuff is pushed back to the rest of the server code.
 *     Only mono/stereo conversion is done here.
 *   + Debugging output uses indentation.
 *   [chris]
 */

#include <stdio.h>
#include "config.h"

FILE	*yyin;

static char *searchpath = CONFSEARCHPATH;

#if defined(DEBUGDSPOUT) || defined(DEBUGDSPIN)
int dspin, dspout;
#endif

#  define PRMSG(x, a, b) \
  if (doDebug) { \
    fprintf(stderr, "auvoxware.c:  %*s", debug_msg_indentation, ""); \
    fprintf(stderr, (x), (a), (b)); \
    fflush(stderr); \
  } 
#  define IDENTMSG (debug_msg_indentation += 2)
#  define UNIDENTMSG (debug_msg_indentation -= 2)

static int doDebug = 0;
int beVerbose = 0;
static int debug_msg_indentation = 0;

#include <errno.h>
#include "dixstruct.h"		/* for RESTYPE */
#include "os.h"			/* for xalloc/xfree and NULL */
#include <fcntl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <assert.h>

#ifdef __FreeBSD__
#include <machine/soundcard.h>
#include <machine/pcaudioio.h>
#else
#include <sys/soundcard.h>
#endif

#include <audio/audio.h>
#include <audio/Aproto.h>
#include "au.h"

#ifdef sco
static AuBool   scoAudioBlocked = AuFalse;
#endif /* sco */

static AuBool   processFlowEnabled;

#define	SERVER_CLIENT		0

#define MIN_MINIBUF_SAMPLES	256
#define MAX_MINIBUF_SAMPLES     1024     /* Must be a power of 2 */

#define auDefaultInputGain	AuFixedPointFromSum(50, 0)
#define auDefaultOutputGain	AuFixedPointFromSum(50, 0)

#define	PhysicalOneTrackBufferSize \
    PAD4(auMinibufSamples * auNativeBytesPerSample * 1)
#define	PhysicalTwoTrackBufferSize \
    PAD4(auMinibufSamples * auNativeBytesPerSample * 2)

/* VOXware sound driver mixer control variables */

static int      Letsplay;
static int      level[100];
static int      mixerfd;	/* The mixer device */
static int      devmask = 0;	/* Bitmask for supported mixer devices */
static int      recsrc = 0;	/* Currently selected recording sources */
static int      recmask = 0;	/* Supported recording sources */
static int      stereodevs = 0;	/* Channels supporting stereo */

static char    *labels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;

/* end of VOXware driver mixer control variables */

int  ParseError = 0;		/* Did we fail to parse conf. file? */

SndStat *confStat;

SndStat sndStatIn =
{
	-1,
	16,
	2,
	0,
	4000,
	44100,
	256,
	3,
	32,
	"/dev/dsp",
	0
}, sndStatOut =
{
	-1,
	16,
	2,
	0,
	4000,
	44100,
	256,
	3,
	32,
	"/dev/dsp",
	0
};

static AuUint8 *auOutputMono,
               *auOutputStereo,
               *auInput;

static ComponentPtr monoInputDevice,
                    stereoInputDevice,
                    monoOutputDevice,
                    stereoOutputDevice;

extern AuInt32  auMinibufSamples;


#define auPhysicalOutputChangableMask AuCompDeviceGainMask

#define auPhysicalOutputValueMask \
  (AuCompCommonAllMasks \
   | AuCompDeviceMinSampleRateMask \
   | AuCompDeviceMaxSampleRateMask \
   | AuCompDeviceGainMask \
   | AuCompDeviceLocationMask \
   | AuCompDeviceChildrenMask)

#define auPhysicalInputChangableMask \
  (AuCompDeviceGainMask | AuCompDeviceLineModeMask)

#define auPhysicalInputValueMask \
  (AuCompCommonAllMasks \
   | AuCompDeviceMinSampleRateMask \
   | AuCompDeviceMaxSampleRateMask \
   | AuCompDeviceLocationMask \
   | AuCompDeviceGainMask \
   | AuCompDeviceChildrenMask)

static void     setPhysicalOutputGain();
static void     setPhysicalInputGainAndLineMode();

void setDebugOn ()
{
  doDebug = 1;
}

void setVerboseOn ()
{
  beVerbose = 1;
}

#ifdef sco

AuBlock
AuBlockAudio()
{
	scoAudioBlocked = AuTrue;
	return 0;
}

void
AuUnBlockAudio(AuBlock id)
{
	scoAudioBlocked = AuFalse;
}

#endif /* sco */

static int createServerComponents(auServerDeviceListSize,
				  auServerBucketListSize,
				  auServerRadioListSize,
				  auServerMinRate,
				  auServerMaxRate)
AuUint32 *auServerDeviceListSize,
         *auServerBucketListSize,
         *auServerRadioListSize,
         *auServerMinRate,
         *auServerMaxRate;
{
  AuDeviceID           stereo, mono;
  ComponentPtr         d, *p;
  int                  i;
  AuUint8              formatIn,
                       formatOut;
  AuUint32             bytesPerSampleIn,
                       bytesPerSampleOut;
  static AuBool        initialized = AuFalse;
  extern RESTYPE       auComponentType;
  extern ComponentPtr *auServerDevices,	/* array of devices */
                      *auServerBuckets,	/* array of server owned buckets */
                      *auServerRadios,	/* array of server owned radios */
                       auDevices,	/* list of all devices */
                       auBuckets,	/* list of all buckets */
                       auRadios;	/* list of all radios */
  extern AuUint32      auNumServerDevices,	/* number of devices */
                       auNumActions,	/* number of defined actions */
                       auNumServerBuckets, /* number of server owned buckets */
                       auNumServerRadios; /* number of server owned radios */
  
  PRMSG("createServerComponents(...);\n", 0, 0);
  IDENTMSG;

  *auServerMinRate = aumax(sndStatIn.minSampleRate,
			   sndStatOut.minSampleRate);
  *auServerMaxRate = aumax(sndStatIn.maxSampleRate,
			   sndStatOut.maxSampleRate);

  auNumServerDevices = *auServerDeviceListSize
                     = *auServerBucketListSize
		     = *auServerRadioListSize
		     = 0;

  formatIn = (sndStatIn.wordSize == 16) ? AuFormatLinearSigned16LSB
                                : AuFormatLinearUnsigned8;
  formatOut = (sndStatOut.wordSize == 16) ? AuFormatLinearSigned16LSB
                                  : AuFormatLinearUnsigned8;

  bytesPerSampleIn = sndStatIn.wordSize / 8;
  bytesPerSampleOut = sndStatOut.wordSize / 8;

  AU_ALLOC_DEVICE(d, 1, 0);
  d->id = FakeClientID(SERVER_CLIENT);
  d->changableMask = auPhysicalOutputChangableMask;
  d->valueMask = auPhysicalOutputValueMask;
  d->kind = AuComponentKindPhysicalOutput;
  d->use = AuComponentUseExportMask;
  d->access = AuAccessExportMask | AuAccessListMask;
  d->format = formatOut;
  d->numTracks = 1;
  d->description.type = AuStringLatin1;
  d->description.string = "Mono Channel Output";
  d->description.len = strlen(d->description.string);
  d->minSampleRate = sndStatOut.minSampleRate;
  d->maxSampleRate = sndStatOut.maxSampleRate;
  d->location = AuDeviceLocationCenterMask | AuDeviceLocationInternalMask;
  d->numChildren = 0;
  d->minibuf = auOutputMono;
  d->minibufSize = d->numTracks * bytesPerSampleOut * auMinibufSamples;
  d->physicalDeviceMask = PhysicalOutputMono;
  AU_ADD_DEVICE(d);

  monoOutputDevice = d;

  AU_ALLOC_DEVICE(d, 2, 1);
  d->id = FakeClientID(SERVER_CLIENT);
  d->changableMask = auPhysicalOutputChangableMask;
  d->valueMask = auPhysicalOutputValueMask;
  d->kind = AuComponentKindPhysicalOutput;
  d->use = AuComponentUseExportMask;
  d->access = AuAccessExportMask | AuAccessListMask;
  d->format = formatOut;
  d->numTracks = 2;
  d->description.type = AuStringLatin1;
  d->description.string = "Stereo Channel Output";
  d->description.len = strlen(d->description.string);
  d->minSampleRate = sndStatOut.minSampleRate;
  d->maxSampleRate = sndStatOut.maxSampleRate;
  d->location = AuDeviceLocationCenterMask | AuDeviceLocationInternalMask;
  d->numChildren = 1;
  d->children = (AuID *) ((AuUint8 *) d + PAD4(sizeof(ComponentRec)));
  d->childSwap = (char *) (d->children + d->numChildren);
  d->children[0] = monoOutputDevice->id;
  d->minibuf = auOutputStereo;
  d->minibufSize = d->numTracks * bytesPerSampleOut * auMinibufSamples;
  d->physicalDeviceMask = PhysicalOutputStereo;
  AU_ADD_DEVICE(d);

  stereoOutputDevice = d;

  AU_ALLOC_DEVICE(d, (sndStatIn.isStereo + 1), 0);
  d->id = FakeClientID(SERVER_CLIENT);
  d->changableMask = auPhysicalInputChangableMask;
  d->valueMask = auPhysicalInputValueMask;
  d->kind = AuComponentKindPhysicalInput;
  d->use = AuComponentUseImportMask;
  d->access = AuAccessImportMask | AuAccessListMask;
  d->format = formatIn;
  d->numTracks = sndStatIn.isStereo + 1;
  d->description.type = AuStringLatin1;
  d->description.string = (sndStatIn.isStereo) ? "Stereo Channel Input"
                                              : "Mono Channel Input";
  d->description.len = strlen(d->description.string);
  d->minSampleRate = sndStatOut.minSampleRate;
  d->maxSampleRate = sndStatOut.maxSampleRate;
  d->location = AuDeviceLocationRightMask | AuDeviceLocationLeftMask
                | AuDeviceLocationExternalMask;
  d->numChildren = 0;
  d->gain = auDefaultInputGain;
  d->minibuf = auInput;
  d->minibufSize = d->numTracks * bytesPerSampleIn * auMinibufSamples;
  d->physicalDeviceMask = (sndStatIn.isStereo) ? PhysicalInputStereo
                                              : PhysicalInputMono;
  AU_ADD_DEVICE(d);

  monoInputDevice = d; /* Should have two input devices - FIXME */
  stereoInputDevice = d;

  /* set the array of server devices */
  if (!(auServerDevices = (ComponentPtr *) aualloc(sizeof(ComponentPtr)
						   * auNumServerDevices))) {
    UNIDENTMSG;
    return AuBadAlloc;
  }
  
  p = auServerDevices;
  d = auDevices;
  
  while (d) {
    *p++ = d;
    d = d->next;
  }

  if (!initialized) {
    initialized = AuTrue;
    setPhysicalOutputGain(auDefaultOutputGain);
    setPhysicalInputGainAndLineMode(auDefaultInputGain, 0);
  }

  UNIDENTMSG;

  return AuSuccess;
}

static AuInt32 setTimer(rate)
AuInt32 rate;
{
  AuInt16          timer_ms;
  AuInt32          foo;
  struct itimerval ntval, otval;

  PRMSG("setTimer(rate = %d);\n", rate, 0);
  IDENTMSG;

  /* change timer according to new sample rate */
  if (rate == 0) {		/* Disable timer case */
    ntval.it_value.tv_sec = ntval.it_value.tv_usec = 0;
    ntval.it_interval.tv_sec = ntval.it_interval.tv_usec = 0;
    timer_ms = 0x7fff;
  } else {
    timer_ms = (auMinibufSamples * 500) / rate;
    ntval.it_interval.tv_sec = 0;
    ntval.it_interval.tv_usec = timer_ms * 1000;
    ntval.it_value.tv_sec = 0;
    ntval.it_value.tv_usec = timer_ms *1000 / 2 ;
  }
  foo = setitimer(ITIMER_REAL, &ntval, &otval);

  UNIDENTMSG;

  return timer_ms;
}


#ifdef sco
static void
oneMoreTick()
{
	struct itimerval ntval, otval;
	int foo;

	ntval.it_interval.tv_sec = 0;
	ntval.it_interval.tv_usec = 0;
	ntval.it_value.tv_sec = 0;
	ntval.it_value.tv_usec = 10;
  foo = setitimer(ITIMER_REAL, &ntval, &otval);
}
#endif /* sco */


static void
setFragmentSize(sndStatPtr)
SndStat *sndStatPtr;
{
  int fragarg, i, j;
  int datarate, numfrags;

  datarate = sndStatPtr->curSampleRate;
  if (sndStatPtr->isStereo)
    datarate *= 2;
  if (sndStatPtr->wordSize == 16)
    datarate *= 2;
  datarate /= 2; /* half second */
  numfrags = datarate / MAX_MINIBUF_SAMPLES;
  if (numfrags < sndStatPtr->minFrags)
    numfrags = sndStatPtr->minFrags;
  else if (numfrags > sndStatPtr->maxFrags)
    numfrags = sndStatPtr->maxFrags;

  j = MAX_MINIBUF_SAMPLES;
  for (i = 0; j; i++) /* figure out what power of 2 MAX_MINIBUF_SAMPLES is */
    j = j >> 1;
  fragarg = (numfrags << 16) | i;  /* numfrags of size MAX_MINIBUF_SAMPLES */
  ioctl(sndStatPtr->fd, SNDCTL_DSP_SETFRAGMENT, &fragarg);
}


static AuUint32 setSampleRate(rate)
AuUint32 rate;
{
  int numSamplesIn, numSamplesOut;
  AuBlock l;

  PRMSG("setSampleRate(rate = %d);\n", rate, 0);
  IDENTMSG;

  l = AuBlockAudio();

  if (sndStatOut.curSampleRate != rate) {
    sndStatOut.curSampleRate = rate;

#if defined(SNDCTL_DSP_SETFRAGMENT)
    setFragmentSize(&sndStatOut);
#endif
    ioctl(sndStatOut.fd, SNDCTL_DSP_SYNC, NULL);
    ioctl(sndStatOut.fd, SNDCTL_DSP_SPEED, &(sndStatOut.curSampleRate));
  }

  if (sndStatIn.fd == sndStatOut.fd)
    sndStatIn = sndStatOut;
  else if (sndStatIn.curSampleRate != rate) {
    sndStatIn.curSampleRate = rate;

#if defined(SNDCTL_DSP_SETFRAGMENT)
    setFragmentSize(&sndStatIn);
#endif
    ioctl(sndStatIn.fd, SNDCTL_DSP_SYNC, NULL);
    ioctl(sndStatIn.fd, SNDCTL_DSP_SPEED, &(sndStatIn.curSampleRate));
  }

#if defined(AUDIO_DRAIN)
  if (sndStatOut.isPCSpeaker)
    ioctl (sndStatOut.fd, AUDIO_DRAIN, 0);
#endif

  AuUnBlockAudio(l);

  setTimer(rate);

  UNIDENTMSG;

  return sndStatOut.curSampleRate;
}


static void serverReset()
{
  PRMSG("serverReset();\n", 0, 0);
  IDENTMSG;

  setTimer(0);
#ifndef sco
  signal(SIGALRM, SIG_IGN);
#endif /* sco */

  PRMSG("Audio Synchronisation...", 0, 0);

#if defined(AUDIO_DRAIN)
  if (sndStatOut.isPCSpeaker)
    ioctl(sndStatOut.fd,AUDIO_DRAIN,0);
  else {
#endif

    ioctl(sndStatIn.fd, SNDCTL_DSP_SYNC, NULL);
    if (sndStatOut.fd != sndStatIn.fd)
      ioctl(sndStatOut.fd, SNDCTL_DSP_SYNC, NULL);

#if defined(AUDIO_DRAIN)
  }
#endif

  if (doDebug) {
    fputs(" done.\n", stderr);
    fflush(stderr);
  }
  UNIDENTMSG;
}


static void
#ifdef sco
intervalProc(int i)
#else
intervalProc()
#endif /* sco */
{
	extern void     AuProcessData();

#ifndef sco
	signal(SIGALRM, SIG_IGN);

	if (processFlowEnabled)
	{
		AuProcessData();
		signal(SIGALRM, intervalProc);
	}
#else
	if (!scoAudioBlocked && processFlowEnabled)
		AuProcessData();
#endif /* sco */
}

/**
  * Gains are mapped thusly:
  *
  *   Software   s	0 - 100
  *   Hardware   h	0 - 100
**/

static void
setPhysicalOutputGain(gain)
    AuFixedPoint    gain;
{
    AuInt32         g = AuFixedPointIntegralAddend(gain);
    AuInt32         i, gusvolume;


    if (g > 100)
	g = 100;
    if (g < 0)
        g = 0;
    Letsplay = g;
    gusvolume = g | (g << 8);
    if (mixerfd != -1)
	i = ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_PCM), &gusvolume);
}

static          AuFixedPoint
getPhysicalOutputGain()
{
    AuInt16         outputGain;

    outputGain = Letsplay;

    return AuFixedPointFromSum(outputGain, 0);
}

static void
setPhysicalInputGainAndLineMode(gain, lineMode)
    AuFixedPoint    gain;
    AuUint8         lineMode;
{
  AuInt16 g = AuFixedPointIntegralAddend(gain);
  AuInt16 inputAttenuation;
  AuInt16 zero = 0;
  AuInt32 params[4];
  
  if (g < 100)
    inputAttenuation = g;
  else
    inputAttenuation = 100;
  
  inputAttenuation = inputAttenuation << 8 | inputAttenuation;
  
  if (lineMode == AuDeviceLineModeHigh) {
    ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_MIC), &inputAttenuation);
    ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_LINE), &zero);
  }
 
  if (lineMode == AuDeviceLineModeLow) {
    ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_LINE), &inputAttenuation);
    ioctl(mixerfd, MIXER_WRITE(SOUND_MIXER_MIC), &zero);
  }
}

static void enableProcessFlow()
{
  AuUint8        *p;

  PRMSG("enableProcessFlow();\n", 0, 0);

#ifdef sco
    if (!processFlowEnabled)
    {
#endif /* sco */

	processFlowEnabled = AuTrue;

#ifndef sco
	signal(SIGALRM, intervalProc);
#else
	setTimer(sndStatOut.curSampleRate);
#endif /* sco */

#ifdef sco
    }
#endif /* sco */
}

static void disableProcessFlow()
{

#ifndef sco
	signal(SIGALRM, SIG_IGN);
#endif /* sco */

  PRMSG("disableProcessFlow();\n", 0, 0);

#ifdef sco
 if (processFlowEnabled)
 {
#endif /* sco */

  ioctl(sndStatOut.fd, SNDCTL_DSP_SYNC, NULL);
#ifndef sco
  ioctl(sndStatOut.fd, SNDCTL_DSP_SPEED, &sndStatOut.curSampleRate);
#endif /* sco */

  if (sndStatOut.fd != sndStatIn.fd)
  {
    ioctl(sndStatIn.fd, SNDCTL_DSP_SYNC, NULL);
#ifndef sco
    ioctl(sndStatIn.fd, SNDCTL_DSP_SPEED, &sndStatIn.curSampleRate);
#endif /* sco */
  }

#ifdef sco
  oneMoreTick();
#endif

  processFlowEnabled = AuFalse;

#ifdef sco
 }
#endif /* sco */
}


#if defined(__GNUC__) && !defined(linux)
inline
#endif
static void monoToStereoLinearSigned16LSB(numSamples)
AuUint32 numSamples;
{
  AuInt16 *s = (AuInt16*)monoOutputDevice->minibuf;
  AuInt16 *d = (AuInt16*)stereoOutputDevice->minibuf;

  while (numSamples--) {
    *d++ = *s;
    *d++ = *s++;
  }
}

#if defined(__GNUC__) && !defined(linux)
inline
#endif
static void monoToStereoLinearUnsigned8(numSamples)
AuUint32 numSamples;
{
  AuUint8 *s = (AuUint8*)monoOutputDevice->minibuf;
  AuUint8 *d = (AuUint8*)stereoOutputDevice->minibuf;

  while (numSamples--) {
    *d++ = *s;
    *d++ = *s++;
  }
}

static void writePhysicalOutputsMono()
{
  AuBlock l;
  void* buf;
  int bufSize;
  
  if (sndStatOut.isStereo) {
    switch (monoOutputDevice->format) {
    case AuFormatLinearSigned16LSB:
      monoToStereoLinearSigned16LSB(monoOutputDevice->minibufSamples);
      break;

    case AuFormatLinearUnsigned8:
      monoToStereoLinearUnsigned8(monoOutputDevice->minibufSamples);
      break;

    default:
      /* check createServerComponents(...)! */
      assert(0);
    }

    buf = stereoOutputDevice->minibuf;
    bufSize = stereoOutputDevice->bytesPerSample
	      * monoOutputDevice->minibufSamples;
  } else {
    buf = monoOutputDevice->minibuf;
    bufSize = monoOutputDevice->bytesPerSample
              * monoOutputDevice->minibufSamples;
  }
  
  l = AuBlockAudio();
  { int b, w;
    char *p = buf ;
    for (b=bufSize; b>0; b -= w) {
       w = write(sndStatOut.fd,	p, b);
       p += w ;
    }
  }

#ifdef DEBUGDSPOUT
    {
	char tempbuf[80];

	sprintf(tempbuf, "\nwriteMono buf: %d size: %d\n", buf, bufSize);
	write(dspout, tempbuf, strlen(tempbuf));
	write(dspout, buf, bufSize);
    }
#endif /* DEBUGDSPOUT */

  AuUnBlockAudio(l);
}

#if defined(__GNUC__) && !defined(linux)
inline
#endif
static void stereoToMonoLinearSigned16LSB(numSamples)
AuUint32 numSamples;
{
  AuInt16 *s = (AuInt16*)stereoOutputDevice->minibuf;
  AuInt16 *d = (AuInt16*)monoOutputDevice->minibuf;

  while (numSamples--) {
    *d++ = (s[0] + s[1]) / 2;
    s += 2;
  }
}

#if defined(__GNUC__) && !defined(linux)
inline
#endif
static void stereoToMonoLinearUnsigned8(numSamples)
AuUint32 numSamples;
{
  AuUint8 *s = (AuUint8*)stereoOutputDevice->minibuf;
  AuUint8 *d = (AuUint8*)monoOutputDevice->minibuf;

  while (numSamples--) {
    *d++ = (s[0] + s[1]) / 2;
    s += 2;
  }
}

static void writePhysicalOutputsStereo()
{
  AuBlock l;
  void* buf;
  int bufSize;
  
  if (sndStatOut.isStereo) {
    buf = stereoOutputDevice->minibuf;
    bufSize = stereoOutputDevice->bytesPerSample
              * stereoOutputDevice->minibufSamples;
  } else {
    switch (stereoOutputDevice->format) {
    case AuFormatLinearSigned16LSB:
      stereoToMonoLinearSigned16LSB(stereoOutputDevice->minibufSamples);
      break;

    case AuFormatLinearUnsigned8:
      stereoToMonoLinearUnsigned8(stereoOutputDevice->minibufSamples);
      break;

    default:
      /* check createServerComponents(...)! */
      assert(0);
    }

    buf = monoOutputDevice->minibuf;
    bufSize = monoOutputDevice->bytesPerSample
              * stereoOutputDevice->minibufSamples;
  }
  
  l = AuBlockAudio();
  { int b, w;
    char *p = buf ;
    for (b=bufSize; b>0; b -= w) {
       w = write(sndStatOut.fd,	p, b);
       p += w ;
    }
  }

#ifdef DEBUGDSPOUT
    {
	char tempbuf[80];

	sprintf(tempbuf, "\nwriteStereo buf: %d size: %d\n", buf, bufSize);
	write(dspout, tempbuf, strlen(tempbuf));
	write(dspout, buf, bufSize);
    }
#endif /* DEBUGDSPOUT */

  AuUnBlockAudio(l);
}

static void writePhysicalOutputsBoth()
{
  AuInt16 *m = (AuInt16 *) monoOutputDevice->minibuf;
  AuInt16 *p, *s;
  AuUint8 *m8 = (AuUint8 *) monoOutputDevice->minibuf;
  AuUint8 *p8, *s8;
  AuUint32 max = aumax(monoOutputDevice->minibufSamples,
		       stereoOutputDevice->minibufSamples);
  AuUint32 i;

  switch (stereoOutputDevice->format) {
  case AuFormatLinearSigned16LSB:
    p = s = (AuInt16 *) stereoOutputDevice->minibuf;

    for (i = 0; i < max; i++) {
      *p++ = (*m + *s++)   / 2;
      *p++ = (*m++ + *s++) / 2;
    }
    break;

  case AuFormatLinearUnsigned8:
    p8 = s8 = (AuUint8 *) stereoOutputDevice->minibuf;

    for (i = 0; i < max; i++) {
      *p8++ = (*m8 + *s8++) / 2;
      *p8++ = (*m8++ + *s8++) / 2;
    }
    break;

  default:
    assert(0);
  }

  stereoOutputDevice->minibufSamples = max;
  
  writePhysicalOutputsStereo();
}

static void readPhysicalInputs()
{
  AuBlock l;
  char *p = stereoInputDevice->minibuf ;
  int b,r ;

  /* Should make use of two input devices - FIXME */
  l = AuBlockAudio();
  for (b = stereoInputDevice->bytesPerSample * auMinibufSamples;
	b > 0 ; b -= r) {
      r = read(sndStatIn.fd, p, b);
      p += r ;
  }

#ifdef DEBUGDSPIN
    {
	char tempbuf[80];
	sprintf(tempbuf, "\nreadInputs buf: %d size: %d\n",
	    stereoInputDevice->minibuf,
	    stereoInputDevice->bytesPerSample * auMinibufSamples);
	write(dspin, tempbuf, strlen(tempbuf));
	write(dspin, stereoInputDevice->minibuf,
	    stereoInputDevice->bytesPerSample * auMinibufSamples);
    }
#endif /* DEBUGDSPIN */

  AuUnBlockAudio(l);
}

static void
noop()
{
}

static void
setWritePhysicalOutputFunction(flow, funct)
    CompiledFlowPtr flow;
    void            (**funct) ();
{
    AuUint32        mask = flow->physicalDeviceMask;

    if ((mask & (PhysicalOutputMono | PhysicalOutputStereo)) ==
	(PhysicalOutputMono | PhysicalOutputStereo))
	*funct = writePhysicalOutputsBoth;
    else if (mask & PhysicalOutputMono)
	*funct = writePhysicalOutputsMono;
    else if (mask & PhysicalOutputStereo)
	*funct = writePhysicalOutputsStereo;
    else
	*funct = noop;
}

/*
 * Setup soundcard at maximum audio quality.
 */

#ifdef __FreeBSD__
#define NO_16_BIT_SAMPLING
#endif

static void setupSoundcard(sndStatPtr)
SndStat* sndStatPtr;
{
  int samplesize;

  PRMSG("setupSoundcard(...);\n", 0, 0);
  IDENTMSG;

  if (beVerbose)
    if (sndStatPtr == &sndStatOut)
      fprintf (stderr, "++ Setting up Output device\n");
    else
      fprintf (stderr, "++ Setting up Input device\n");
	
  if (sndStatPtr->isPCSpeaker) {
    if (beVerbose)
      fprintf(stderr, "+++ Device is a PC speaker\n");
    sndStatPtr->curSampleRate = sndStatPtr->maxSampleRate
      = sndStatPtr->minSampleRate = 8000;
    sndStatPtr->isStereo = 0;
    sndStatPtr->wordSize = 8;
  }
  else {
    if (beVerbose)
      fprintf(stderr, "+++ requesting wordsize of %d, ", sndStatPtr->wordSize);
    if (ioctl(sndStatPtr->fd, SNDCTL_DSP_SAMPLESIZE, &sndStatPtr->wordSize)
        || sndStatPtr->wordSize != 16) {
      sndStatPtr->wordSize = 8;
      ioctl(sndStatPtr->fd, SNDCTL_DSP_SAMPLESIZE, &sndStatPtr->wordSize);
    }
    if (beVerbose)
      fprintf(stderr, "got %d\n", sndStatPtr->wordSize);
  
    if (beVerbose)
      fprintf(stderr, "+++ requesting %d channel(s), ", sndStatPtr->isStereo + 1);
    if (ioctl(sndStatPtr->fd, SNDCTL_DSP_STEREO, &sndStatPtr->isStereo) == -1
        || !sndStatPtr->isStereo) {
      sndStatPtr->isStereo = 0;
      ioctl(sndStatPtr->fd, SNDCTL_DSP_STEREO, &sndStatPtr->isStereo);
    }
    if (beVerbose)
      fprintf(stderr, "got %d channel(s)\n", sndStatPtr->isStereo + 1);

    if (beVerbose)
      fprintf (stderr, "+++ Requesting minimum sample rate of %d, ", sndStatPtr->minSampleRate);
    ioctl(sndStatPtr->fd, SNDCTL_DSP_SPEED, &sndStatPtr->minSampleRate);
    if (beVerbose)
      fprintf (stderr, "got %d\n", sndStatPtr->minSampleRate);
  
    if (beVerbose)
      fprintf (stderr, "+++ Requesting maximum sample rate of %d, ", sndStatPtr->maxSampleRate);
    ioctl(sndStatPtr->fd, SNDCTL_DSP_SPEED, &sndStatPtr->maxSampleRate);
    if (beVerbose)
      fprintf (stderr, "got %d\n", sndStatPtr->maxSampleRate);

    sndStatPtr->curSampleRate = sndStatPtr->maxSampleRate;

  }

#if defined(SNDCTL_DSP_SETFRAGMENT)
    setFragmentSize(sndStatPtr);
#endif

  UNIDENTMSG;
}


#ifdef sco
static AuBool
scoInterrupts()
{
	struct sigaction act;

	act.sa_handler = intervalProc;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);
	if (sigaction(SIGALRM, &act, (struct sigaction *)NULL) == -1)
	{
		ErrorF("sigaction SIGALRM");
		return FALSE;
	}

	return TRUE;
}
#endif /* sco */

/*
*  Find the config file 
*/

static FILE	*openConfigFile (char *path)
{
  static char	buf[1024];
  FILE *config;

  strcat (buf, path);
  strcat (buf, "AUVoxConfig");
  if ((config = fopen (buf, "r")) != NULL)
    return config;
  else
    return NULL;
}


AuBool AuInitPhysicalDevices()
{
  static AuBool    AL_initialized = AuFalse;
  static AuUint8  *physicalBuffers;
  int             fd;
  AuUint32         physicalBuffersSize;
  extern AuUint8  *auPhysicalOutputBuffers;
  extern AuUint32  auPhysicalOutputBuffersSize;
  extern void      AuProcessData();
#if defined(AUDIO_GETINFO)
  audio_info_t     spkrinf;
#endif

  PRMSG("AuInitPhysicalDevices();\n", 0, 0);
  IDENTMSG;

  /*
   * create the input and output ports
   */
  if (!AL_initialized) {
    int fd;
    AuInt32 i;

    AL_initialized = AuTrue;

    if ((yyin = openConfigFile (CONFSEARCHPATH)) != NULL)
        yyparse();

    if ((fd = open(sndStatOut.device, O_RDWR, 0)) == -1) {
        UNIDENTMSG;
        return AuFalse;
    }
#if defined(AUDIO_GETINFO)
    if (sndStatOut.isPCSpeaker) {
      ioctl(fd, AUDIO_GETINFO, &spkrinf);
      spkrinf.play.encoding = AUDIO_ENCODING_RAW;
      ioctl(fd, AUDIO_SETINFO, &spkrinf);
    }
#endif

#ifdef DEBUGDSPOUT
    dspout = open("/tmp/dspout", O_RDWR | O_CREAT, 00666);
#endif
#ifdef DEBUGDSPIN
    dspin = open("/tmp/dspin", O_RDWR | O_CREAT, 00666);
#endif

    sndStatOut.fd = fd;

    if ((fd = open(sndStatIn.device, O_RDONLY, 0)) != -1)
      sndStatIn.fd = fd;
    else
      sndStatIn.fd = sndStatOut.fd;

    setupSoundcard(&sndStatOut);
    if (sndStatOut.fd != sndStatIn.fd)
      setupSoundcard(&sndStatIn);

    if (!sndStatOut.isPCSpeaker) {
      if ((mixerfd = open("/dev/mixer", O_RDONLY, 0)) == -1) {
        UNIDENTMSG;
        return AuFalse;
      }
    
      if (ioctl(mixerfd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
        close(mixerfd);
        mixerfd = -1;
      } else {
        if (ioctl(mixerfd, SOUND_MIXER_READ_RECMASK, &recmask) == -1) {
	  return AuFalse;
        }
      
        {
          /* Enable all used recording sources ( mic & line ) and
           * control which is active via level setting later. There
           * should be a better way to do this!
           */
          int mask = SOUND_MASK_MIC | SOUND_MASK_LINE; /* enable these */
          mask &= recmask;    /* if supported */
          if (ioctl(mixerfd, SOUND_MIXER_WRITE_RECSRC, &mask) == -1) {
            return AuFalse;
          }
        }

        if (ioctl(mixerfd, SOUND_MIXER_READ_RECSRC, &recsrc) == -1) {
	  UNIDENTMSG;
	  return AuFalse;
        }
      
        if (ioctl(mixerfd, SOUND_MIXER_READ_STEREODEVS, &stereodevs) == -1) {
	  UNIDENTMSG;
	  return AuFalse;
        }
      
        /* get all sound levels */
        for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
	  if ((1 << i) & devmask) {
	  
	    if (ioctl(mixerfd, MIXER_READ(i), &level[i]) == -1) {
	      UNIDENTMSG;
	      return AuFalse;
	    }
	  }
        }
      
      }
    }
  }
  
  if (physicalBuffers)
    aufree(physicalBuffers);

  auMinibufSamples = MAX_MINIBUF_SAMPLES;

  /* the output buffers need to be twice as large for output range checking */
  physicalBuffersSize = 
      PhysicalTwoTrackBufferSize +	/* mono/stereo input */
      PhysicalOneTrackBufferSize * 2 +	/* mono output */
      PhysicalTwoTrackBufferSize * 2;	/* stereo output */
     
  if (!(physicalBuffers = (AuUint8 *) aualloc(physicalBuffersSize))) {
    UNIDENTMSG;
    return AuFalse;
  }
  
  auInput = physicalBuffers;
  auOutputMono = auInput + PhysicalTwoTrackBufferSize;
  auOutputStereo = auOutputMono + 2 * PhysicalOneTrackBufferSize;
  
  auPhysicalOutputBuffers = auOutputMono;
  auPhysicalOutputBuffersSize = physicalBuffersSize -
      PhysicalTwoTrackBufferSize;

  /*
   * Call AuProcessData() in signal handler often enough to drain the
   * input devices and keep the output devices full at the current
   * sample rate.
   */
  
  processFlowEnabled = AuFalse;

#ifdef sco
	if (!scoInterrupts())
	{
		return AuFalse;
	}
#else
  signal(SIGALRM, intervalProc);
#endif /* sco */

  setTimer(0);
  
  AuRegisterCallback(AuCreateServerComponentsCB, createServerComponents);
  AuRegisterCallback(AuSetPhysicalOutputGainCB, setPhysicalOutputGain);
  AuRegisterCallback(AuGetPhysicalOutputGainCB, getPhysicalOutputGain);
  AuRegisterCallback(AuSetPhysicalInputGainAndLineModeCB,
		     setPhysicalInputGainAndLineMode);
  AuRegisterCallback(AuEnableProcessFlowCB, enableProcessFlow);
  AuRegisterCallback(AuDisableProcessFlowCB, disableProcessFlow);
  AuRegisterCallback(AuReadPhysicalInputsCB, readPhysicalInputs);
  AuRegisterCallback(AuSetWritePhysicalOutputFunctionCB,
		     setWritePhysicalOutputFunction);

  AuRegisterCallback(AuSetSampleRateCB, setSampleRate);
  
  /* bogus resource so we can have a cleanup function at server reset */
  AddResource(FakeClientID(SERVER_CLIENT),
	      CreateNewResourceType(serverReset), 0);

  UNIDENTMSG;

  return AuTrue;
}
