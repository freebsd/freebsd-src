/* for FreeBSD */
/*
 * local.h,v 1.11 1994/11/01 17:26:50 ache Exp
 */
#include "snd.h"

#include <param.h>
#include <systm.h>
#include <machine/cpufunc.h>
#include <vm/vm.h>

#if NSND > 0
#define KERNEL_SOUNDCARD
#endif

#ifndef EXCLUDE_UART6850
#define EXCLUDE_UART6850
#endif
#ifndef EXCLUDE_PSS
#define EXCLUDE_PSS
#endif

#define DSP_BUFFSIZE 65536
#define SELECTED_SOUND_OPTIONS  0xffffffff
#define SOUND_VERSION_STRING "2.90-2"
#define SOUND_CONFIG_DATE "Sun Feb 5 14:38:12 EST 1995"
#define SOUND_CONFIG_BY "smpatel"
#define SOUND_CONFIG_HOST "xi.dorm.umd.edu"
#define SOUND_CONFIG_DOMAIN "dorm.umd.edu"


/* Reversed the VoxWare EXCLUDE options -Sujal Patel (smpatel@wam.umd.edu) */

#ifndef EXCLUDE_PAS
#define EXCLUDE_PAS
#endif
#ifndef EXCLUDE_SB
#define EXCLUDE_SB
#endif
#ifndef EXCLUDE_GUS
#define EXCLUDE_GUS
#endif
#ifndef EXCLUDE_MPU401
#define EXCLUDE_MPU401
#endif
#ifndef EXCLUDE_UART6850
#define EXCLUDE_UART6850
#endif
#ifndef EXCLUDE_PSS
#define EXCLUDE_PSS
#endif
#ifndef EXCLUDE_GUS16
#define EXCLUDE_GUS16
#endif
#ifndef EXCLUDE_GUSMAX
#define EXCLUDE_GUSMAX
#endif
#ifndef EXCLUDE_MSS
#define EXCLUDE_MSS
#endif
#ifndef EXCLUDE_SBPRO
#define EXCLUDE_SBPRO
#endif
#ifndef EXCLUDE_SB16
#define EXCLUDE_SB16
#endif
#ifndef EXCLUDE_YM3812
#define EXCLUDE_YM3812
#endif

#ifdef AUDIO_PAS
#undef EXCLUDE_PAS
#endif
#ifdef AUDIO_SB
#undef EXCLUDE_SB
#endif
#ifdef AUDIO_GUS
#undef EXCLUDE_GUS
#endif
#ifdef AUDIO_MPU401
#undef EXCLUDE_MPU401
#endif
#ifdef AUDIO_UART6850
#undef EXCLUDE_UART6850
#endif
#ifdef AUDIO_PSS
#undef EXCLUDE_PSS
#endif
#ifdef AUDIO_GUS16
#undef EXCLUDE_GUS16
#endif
#ifdef AUDIO_GUSMAX
#undef EXCLUDE_GUSMAX
#endif
#ifdef AUDIO_MSS
#undef EXCLUDE_MSS
#endif
#ifdef AUDIO_SBPRO
#undef EXCLUDE_SBPRO
#undef EXCLUDE_SB
#endif
#ifdef AUDIO_SB16
#undef EXCLUDE_SB16
#undef EXCLUDE_SB
#endif
#ifdef AUDIO_YM3812
#undef EXCLUDE_YM3812
#endif
