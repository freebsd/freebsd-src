/* for FreeBSD */
#include "snd.h"

#if NSND > 0
#define KERNEL_SOUNDCARD
#endif

#define DSP_BUFFSIZE 65536
#define SELECTED_SOUND_OPTIONS	0xffffffff
#define SOUND_VERSION_STRING "2.4"
#define SOUND_CONFIG_DATE "Mon Mar 7 23:54:09 PST 1994"
#define SOUND_CONFIG_BY "swallace"
#define SOUND_CONFIG_HOST "freefall.cdrom.com"
#define SOUND_CONFIG_DOMAIN ""
