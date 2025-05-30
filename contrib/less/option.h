/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


#define END_OPTION_STRING       ('$')

/*
 * Types of options.
 */
#define O_BOOL            01      /* Boolean option: 0 or 1 */
#define O_TRIPLE          02      /* Triple-valued option: 0, 1 or 2 */
#define O_NUMBER          04      /* Numeric option */
#define O_STRING          010     /* String-valued option */
#define O_NOVAR           020     /* No associated variable */
#define O_REPAINT         040     /* Repaint screen after toggling option */
#define O_NO_TOGGLE       0100    /* Option cannot be toggled with "-" cmd */
#define O_HL_REPAINT      0200    /* Repaint hilites after toggling option */
#define O_NO_QUERY        0400    /* Option cannot be queried with "_" cmd */
#define O_INIT_HANDLER    01000   /* Call option handler function at startup */
#define O_UNSUPPORTED     02000   /* Option is unsupported via LESS_UNSUPPORT */

#define OTYPE           (O_BOOL|O_TRIPLE|O_NUMBER|O_STRING|O_NOVAR)

#define OLETTER_NONE    '\1'     /* Invalid option letter */

/*
 * Argument to a handling function tells what type of activity:
 */
#define INIT    0       /* Initialization (from command line) */
#define QUERY   1       /* Query (from _ or - command) */
#define TOGGLE  2       /* Change value (from - command) */

/* Flag to toggle_option to specify how to "toggle" */
#define OPT_NO_TOGGLE   0
#define OPT_TOGGLE      1
#define OPT_UNSET       2
#define OPT_SET         3
#define OPT_NO_PROMPT   0100

/* Error code from findopt_name */
#define OPT_AMBIG       1

struct optname
{
        constant char *oname;   /* Long (GNU-style) option name */
        struct optname *onext;  /* List of synonymous option names */
};

#define OPTNAME_MAX     32      /* Max length of long option name */

struct loption
{
        char oletter;           /* The controlling letter (a-z) */
        struct optname *onames; /* Long (GNU-style) option name */
        int otype;              /* Type of the option */
        int odefault;           /* Default value */
        int *ovar;              /* Pointer to the associated variable */
        void (*ofunc)(int, constant char*); /* Pointer to special handling function */
        constant char *odesc[3]; /* Description of each value */
};

