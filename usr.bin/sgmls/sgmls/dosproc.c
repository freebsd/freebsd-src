/* dosproc.c -

   MS-DOS implementation of run_process().

     Written by James Clark (jjc@jclark.com).
*/

#include "config.h"

#ifdef SUPPORT_SUBDOC

#include "std.h"
#include "entity.h"
#include "appl.h"

#include <process.h>

int run_process(argv)
char **argv;
{
     int ret;
     fflush(stdout);
     fflush(stderr);
     ret = spawnvp(P_WAIT, argv[0], argv);
     if (ret < 0)
	  appl_error(E_EXEC, argv[0], strerror(errno));
     return ret;
}

#endif /* SUPPORT_SUBDOC */

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
