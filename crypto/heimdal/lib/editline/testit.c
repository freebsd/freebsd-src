/*  $Revision: 1.2 $
**
**  A "micro-shell" to test editline library.
**  If given any arguments, commands aren't executed.
*/
#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "editline.h"

int
main(int ac, char **av)
{
    char	*p;
    int		doit;

    doit = ac == 1;
    while ((p = readline("testit> ")) != NULL) {
	(void)printf("\t\t\t|%s|\n", p);
	if (doit)
	    if (strncmp(p, "cd ", 3) == 0) {
		if (chdir(&p[3]) < 0)
		    perror(&p[3]);
	    } else if (system(p) != 0) {
		perror(p);
	    }
	add_history(p);
	free(p);
    }
    exit(0);
    /* NOTREACHED */
}
