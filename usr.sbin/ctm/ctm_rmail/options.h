/*
 * Macros for processing command arguments.
 *
 * Conforms closely to the command option requirements of intro(1) in System V
 * and intro(C) in Xenix.
 *
 * A command consists of: cmdname [ options ] [ cmdarguments ]
 *
 * Options consist of a leading dash '-' and a flag letter.  An argument may
 * follow optionally preceded by white space.
 * Options without arguments may be grouped behind a single dash.
 * A dash on its own is interpreted as the end of the options and is retained
 * as a command argument.
 * A double dash '--' is interpreted as the end of the options and is discarded.
 *
 * For example:
 *	zap -xz -f flame -q34 -- -x
 *
 * where zap.c contains the following in main():
 *
 *	OPTIONS("[-xz] [-q queue-id] [-f dump-file] user")
 *	    FLAG('x', xecute)
 *	    FLAG('z', zot)
 *	    STRING('f', file)
 *		fp = fopen(file, "w");
 *	    NUMBER('q', queue)
 *	    ENDOPTS
 *
 * Results in:
 *	xecute = 1
 *	zot = 1
 *	file = "flame"
 *	fp = fopen("flame", "w")
 *	queue = 34
 *	argc = 2
 *	argv[0] = "zap"
 *	argv[1] = "-x"
 *
 * Should the user enter unknown flags or leave out required arguments,
 * the message:
 *
 *	Usage: zap [-xz] [-q queue-id] [-f dump-file] user
 *
 * will be printed.  This message can be printed by calling pusage(), or
 * usage().  usage() will also cause program termination with exit code 1.
 *
 * Author: Stephen McKay, February 1991
 *
 * Based on recollection of the original options.h produced at the University
 * of Queensland by Ross Patterson (and possibly others).
 */

static char *O_usage;
static char *O_name;
extern long atol();

void
pusage()
    {
    /*
     * Avoid gratuitously loading stdio.
     */
    write(2, "Usage: ", 7);
    write(2, O_name, strlen(O_name));
    write(2, " ", 1);
    write(2, O_usage, strlen(O_usage));
    write(2, "\n", 1);
    }

#define usage()		(pusage(), exit(1))

#define OPTIONS(usage_msg)		\
    {					\
    char O_cont;			\
    O_usage = (usage_msg);		\
    O_name = argv[0];			\
    while (*++argv && **argv == '-')	\
	{				\
	if ((*argv)[1] == '\0')		\
	    break;			\
	argc--;				\
	if ((*argv)[1] == '-' && (*argv)[2] == '\0') \
	    {				\
	    argv++;			\
	    break;			\
	    }				\
	O_cont = 1;			\
	while (O_cont)			\
	    switch (*++*argv)		\
		{			\
		default:		\
		case '-':		\
		    usage();		\
		case '\0':		\
		    O_cont = 0;

#define	FLAG(x,flag)			\
		    break;		\
		case (x):		\
		    (flag) = 1;

#define	CHAR(x,ch)			\
		    break;		\
		case (x):		\
		    O_cont = 0;		\
		    if (*++*argv == '\0' && (--argc, *++argv == 0)) \
			usage();	\
		    (ch) = **argv;

#define	NUMBER(x,n)			\
		    break;		\
		case (x):		\
		    O_cont = 0;		\
		    if (*++*argv == '\0' && (--argc, *++argv == 0)) \
			usage();	\
		    (n) = atol(*argv);

#define	STRING(x,str)			\
		    break;		\
		case (x):		\
		    O_cont = 0;		\
		    if (*++*argv == '\0' && (--argc, *++argv == 0)) \
			usage();	\
		    (str) = *argv;

#define	SUFFIX(x,str)			\
		    break;		\
		case (x):		\
		    (str) = ++*argv;	\
		    O_cont = 0;

#define ENDOPTS				\
		    break;		\
		}			\
	}				\
    *--argv = O_name;			\
    }
