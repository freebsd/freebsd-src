/*
 * rlversion -- print out readline's version number
 */

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include "posixstat.h"

#include "readline.h"

main()
{
	printf ("%s\n", rl_library_version ? rl_library_version : "unknown");
	exit (0);
}
