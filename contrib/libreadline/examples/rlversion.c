/*
 * rlversion -- print out readline's version number
 */

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include "posixstat.h"

#ifdef READLINE_LIBRARY
#  include "readline.h"
#else
#  include <readline/readline.h>
#endif

main()
{
	printf ("%s\n", rl_library_version ? rl_library_version : "unknown");
	exit (0);
}
