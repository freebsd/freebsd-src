/* These few lines are used by FreeBSD (only??). */

#include "snd.h"

#if NSND > 0
#define CONFIGURE_SOUNDCARD
#endif

#define DSP_BUFFSIZE 32768
#define SELECTED_SOUND_OPTIONS	0xffffffff
