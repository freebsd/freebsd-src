/*
 * Header file for audio drivers
 */
#include "ntp_types.h"

#define AUDIO_BUFSIZ    160     /* codec buffer size (Solaris only) */

/*
 * Function prototypes
 */
int	audio_init		P((char *));
int	audio_gain		P((int, int));
void	audio_show		P((void));
