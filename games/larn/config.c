/*
 *	config.c	--	This defines the installation dependent variables.
 *                  Some strings are modified later.  ANSI C would
 *                  allow compile time string concatenation, we must
 *                  do runtime concatenation, in main.
 *
 *		Larn is copyrighted 1986 by Noah Morgan.
 */
#include "header.h"
#include "pathnames.h"

/*
 *	All these strings will be appended to in main() to be complete filenames
 */

/* the game save filename */
char savefilename[1024];

/* the logging file */
char logfile[] = _PATH_LOG;

/* the help text file */
char helpfile[] = _PATH_HELP;

/* the score file */
char scorefile[] = _PATH_SCORE;

/* the maze data file */
char larnlevels[] = _PATH_LEVELS;

/* the fortune data file */
char fortfile[] = _PATH_FORTS;

/* the .larnopts filename */
char optsfile[1024] ="/.larnopts";

/* the player id datafile name */
char playerids[] = _PATH_PLAYERIDS;

char diagfile[] ="Diagfile";		/* the diagnostic filename */
char ckpfile[] ="Larn12.0.ckp";		/* the checkpoint filename */
char *password ="pvnert(x)";		/* the wizards password <=32 */
char psname[PSNAMESIZE]="larn";		/* the process name */

#define	WIZID	1
int wisid=0;		/* the user id of the only person who can be wizard */

