/* for FreeBSD */
/*
 * $Id: local.h,v 1.9 1994/09/27 17:58:19 davidg Exp $
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
#define NO_AUTODMA  /* still */
#define SELECTED_SOUND_OPTIONS	0xffffffff
#define SOUND_VERSION_STRING "2.90-2"
#define SOUND_CONFIG_DATE "Thu Sep 29 15:33:39 PDT 1994"
#define SOUND_CONFIG_BY "swallace"
#define SOUND_CONFIG_HOST "pal-r32-a07b.slip.nts.uci.edu"
#define SOUND_CONFIG_DOMAIN ""
