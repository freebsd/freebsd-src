/* for FreeBSD */
/*
 * $Id: local.h,v 1.8 1994/08/02 07:40:06 davidg Exp $
 */
#include "snd.h"

#include <param.h>
#include <systm.h>
#include <machine/cpufunc.h>
#include <vm/vm.h>

#if NSND > 0
#define KERNEL_SOUNDCARD
#endif

#define DSP_BUFFSIZE 65536
#define NO_AUTODMA  /* still */
#define SELECTED_SOUND_OPTIONS	0xffffffff
#define SOUND_VERSION_STRING "2.5"
#define SOUND_CONFIG_DATE "Sat Apr 23 07:45:17 MSD 1994"
#define SOUND_CONFIG_BY "ache"
#define SOUND_CONFIG_HOST "dream.demos.su"
#define SOUND_CONFIG_DOMAIN ""
