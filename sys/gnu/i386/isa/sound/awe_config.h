/*
 * sound/awe_config.h
 *
 * Configuration of AWE32 sound driver
 *   version 0.4.2; Sep. 15, 1997
 *
 * Copyright (C) 1996 Takashi Iwai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef AWE_CONFIG_H_DEF
#define AWE_CONFIG_H_DEF

/*----------------------------------------------------------------
 * system configuration
 *----------------------------------------------------------------*/

/* if you're using obsolete VoxWare 3.0.x on Linux 1.2.x (or FreeBSD),
 * define the following line.
 */
#undef AWE_OBSOLETE_VOXWARE

#ifdef __FreeBSD__
#  define AWE_OBSOLETE_VOXWARE
#endif

/* if you're using OSS-Lite on Linux 2.1.6 or later, define the
 * following line.
 */
#undef AWE_NEW_KERNEL_INTERFACE

/* if you have lowlevel.h in the lowlevel directory (OSS-Lite), define
 * the following line.
 */
#undef HAS_LOWLEVEL_H

/* if your system doesn't support patch manager (OSS 3.7 or newer),
 * define the following line.
 */
#undef AWE_NO_PATCHMGR
 
/* if your system has an additional parameter (OSS 3.8b5 or newer),
 * define this.
 */
#undef AWE_OSS38

/*----------------------------------------------------------------
 * AWE32 card configuration:
 * uncomment the following lines only when auto detection doesn't
 * work properly on your machine.
 *----------------------------------------------------------------*/

/*#define AWE_DEFAULT_BASE_ADDR	0x620*/	/* base port address */
/*#define AWE_DEFAULT_MEM_SIZE	512*/	/* kbytes */


/*----------------------------------------------------------------
 * maximum size of soundfont list table:
 * you usually don't need to touch this value.
 *----------------------------------------------------------------*/

#define AWE_MAX_SF_LISTS 16


/*----------------------------------------------------------------
 * chunk size of sample and voice tables:
 * you usually don't need to touch these values.
 *----------------------------------------------------------------*/

#define AWE_MAX_SAMPLES 400
#define AWE_MAX_INFOS 800


/*----------------------------------------------------------------
 * chorus & reverb effects send for FM chip: from 0 to 0xff
 * larger numbers often cause weird sounds.
 *----------------------------------------------------------------*/

#define DEF_FM_CHORUS_DEPTH	0x10
#define DEF_FM_REVERB_DEPTH	0x10


/*----------------------------------------------------------------*
 * other compile conditions
 *----------------------------------------------------------------*/

/* initialize FM passthrough even without extended RAM */
#undef AWE_ALWAYS_INIT_FM

/* debug on */
#define AWE_DEBUG_ON

/* GUS compatible mode */
#define AWE_HAS_GUS_COMPATIBILITY

/* accept all notes/sounds off controls */
#define AWE_ACCEPT_ALL_SOUNDS_CONTROL

/* add mixer control of emu8000 equalizer */
#define CONFIG_AWE32_MIXER

/* look up voices according to MIDI channel priority */
#define AWE_LOOKUP_MIDI_PRIORITY

/*----------------------------------------------------------------*/

/* reading configuration of sound driver */

#ifdef AWE_OBSOLETE_VOXWARE

#ifdef __FreeBSD__
#  include <i386/isa/sound/sound_config.h>
#else
#  include "sound_config.h"
#endif

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_AWE32)
#define CONFIG_AWE32_SYNTH
#endif

#else /* AWE_OBSOLETE_VOXWARE */

#ifdef HAS_LOWLEVEL_H
#include "lowlevel.h"
#endif

#include "../sound_config.h"

#endif /* AWE_OBSOLETE_VOXWARE */


#endif  /* AWE_CONFIG_H_DEF */
