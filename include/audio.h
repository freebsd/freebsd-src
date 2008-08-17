/*
 * Header file for audio drivers
 */
#include "ntp_types.h"

#define MAXGAIN		255	/* max codec gain */
#define	MONGAIN		127	/* codec monitor gain */

/*
 * Function prototypes
 */
int	audio_init		P((char *, int, int));
int	audio_gain		P((int, int, int));
void	audio_show		P((void));
