/* for FreeBSD */
/*
 * $Id: local.h,v 1.11 1994/11/01 17:26:50 ache Exp
 */

#define DSP_BUFFSIZE 65536
#define SELECTED_SOUND_OPTIONS  0xffffffff
#define SOUND_CONFIG_DATE "Sun Feb 5 14:38:12 EST 1995"
#define SOUND_CONFIG_BY "freebsd-hackers"
#define SOUND_CONFIG_HOST "freefall"
#define SOUND_CONFIG_DOMAIN "cdrom.com"

/* determine if sound code should be compiled */
#include "snd.h"
#if NSND > 0
#define KERNEL_SOUNDCARD
#endif

#define ALLOW_SELECT

/* PSS code does not work */
#ifndef EXCLUDE_PSS
#define EXCLUDE_PSS
#endif

#include "gus.h"
#if NGUS == 0 && !defined(EXCLUDE_GUS)
#define EXCLUDE_GUS
#endif

#include "gusxvi.h"
#if NGUSXVI == 0 && !defined(EXCLUDE_GUS16)
#define EXCLUDE_GUS16
#endif

#include "mss.h"
#if NMSS == 0 && !defined(EXCLUDE_MSS)
#define EXCLUDE_MSS
#endif

#include "trix.h"
#if NTRIX == 0 && !defined(EXCLUDE_TRIX)
#define EXCLUDE_TRIX
#endif

#include "sscape.h"
#if NSSCAPE == 0 && !defined(EXCLUDE_SSCAPE)
#define EXCLUDE_SSCAPE
#endif

#if NGUS == 0 && !defined(EXCLUDE_GUSMAX)
# define EXCLUDE_GUSMAX
# if defined(EXCLUDE_GUS16) && defined(EXCLUDE_MSS) && !defined(EXCLUDE_AD1848)
#  define EXCLUDE_AD1848
# endif
#else
# define GUSMAX_MIXER
#endif

#include <sb.h>
#if NSB == 0 && !defined(EXCLUDE_SB)
#define EXCLUDE_SB
#endif

#include "sbxvi.h"
#if NSBXVI == 0 && !defined(EXCLUDE_SB16)
#define EXCLUDE_SB16
#endif

#include "sbmidi.h"
#if NSBMIDI == 0 && !defined(EXCLUDE_SB16MIDI)
#define EXCLUDE_SB16MIDI
#endif

#include <pas.h>
#if NPAS == 0 && !defined(EXCLUDE_PAS)
#define EXCLUDE_PAS
#endif

#include "mpu.h"
#if NMPU == 0 && !defined(EXCLUDE_MPU401)
#define EXCLUDE_MPU401
#endif

#include "opl.h"
#if NOPL == 0 && !defined(EXCLUDE_YM3812)
#define EXCLUDE_YM3812
#endif

#include "uart.h"
#if NUART == 0 && !defined(EXCLUDE_UART6850)
#define EXCLUDE_UART6850
#endif

/* nothing but a sequencer (Adlib/OPL) ? */
#if NGUS == 0 && NSB == 0 && NSBMIDI == 0 && NPAS == 0 && NMPU == 0 && \
    NUART == 0 && NMSS == 0
#ifndef EXCLUDE_MIDI
#define EXCLUDE_MIDI
#endif
#ifndef EXCLUDE_AUDIO
#define EXCLUDE_AUDIO
#endif
#endif

/* nothing but a Midi (MPU/UART) ? */
#if NGUS == 0 && NSB == 0 && NSBMIDI == 0 && NPAS == 0 && NOPL == 0 && \
    NMSS == 0
/* MPU depends on sequencer timer */
#if NMPU == 0 && !defined(EXCLUDE_SEQUENCER)
#define EXCLUDE_SEQUENCER
#endif
#ifndef EXCLUDE_AUDIO
#define EXCLUDE_AUDIO
#endif
#endif
