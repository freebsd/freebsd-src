/*  $Revision: 1.3 $
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
#include <getarg.h>

#include "editline.h"

static int n_flag	= 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"dry-run", 'n',	arg_flag,	&n_flag,
     "do not run commands", NULL },
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    char	*p;
    int optind = 0;

    setprogname (argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    while ((p = readline("testit> ")) != NULL) {
	(void)printf("\t\t\t|%s|\n", p);
	if (!n_flag) {
	    if (strncmp(p, "cd ", 3) == 0) {
		if (chdir(&p[3]) < 0)
		    perror(&p[3]);
	    } else if (system(p) != 0) {
		perror(p);
	    }
	}
	add_history(p);
	free(p);
    }
    exit(0);
    /* NOTREACHED */
}
