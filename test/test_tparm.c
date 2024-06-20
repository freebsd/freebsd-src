/****************************************************************************
 * Copyright 2020-2022,2023 Thomas E. Dickey                                *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author: Thomas E. Dickey
 *
 * $Id: test_tparm.c,v 1.39 2023/11/11 01:00:03 tom Exp $
 *
 * Exercise tparm/tiparm, either for all possible capabilities with fixed
 * parameters, or one capability with specific combinations of parameters.
 */
#define USE_TINFO
#include <test.priv.h>

#if NCURSES_XNAMES
#if HAVE_TERM_ENTRY_H
#include <term_entry.h>
#else
#undef NCURSES_XNAMES
#define NCURSES_XNAMES 0
#endif
#endif

#define MAX_PARM 9

#define GrowArray(array,limit,length) \
	    if (length + 2 >= limit) { \
		limit *= 2; \
		array = typeRealloc(char *, limit, array); \
		if (array == 0) { \
		    failed("no memory: " #array); \
		} \
	    }

static GCC_NORETURN void failed(const char *);

static void
failed(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    ExitProgram(EXIT_FAILURE);
}

#if HAVE_TIGETSTR

static int a_opt;
static int p_opt;
static int v_opt;

#if HAVE_TIPARM
static int i_opt;
#endif

#if HAVE_TIPARM_S
static int s_opt;
#endif

/*
 * Total tests (and failures):
 */
static long total_tests;
static long total_fails;

/*
 * Total characters formatted for tputs:
 */
static long total_nulls;
static long total_ctrls;
static long total_print;

static int
output_func(int ch)
{
    if (ch == 0) {
	total_nulls++;
    } else if (ch < 32 || (ch >= 127 && ch < 160)) {
	total_ctrls++;
    } else {
	total_print++;
    }
    return ch;
}

static int
isNumeric(char *source)
{
    char *next = 0;
    long value = strtol(source, &next, 0);
    int result = (next == 0 || next == source || *next != '\0') ? 0 : 1;
    (void) value;
    return result;
}

static int
relevant(const char *name, const char *value)
{
    int code = 1;
    if (VALID_STRING(value)) {
	if (strstr(value, "%p") == 0
	    && strstr(value, "%d") == 0
	    && strstr(value, "%s") == 0
	    && (!p_opt || strstr(value, "$<") == 0)) {
	    if (v_opt > 2)
		printf("? %s noparams\n", name);
	    code = 0;
	}
    } else {
	if (v_opt > 2) {
	    printf("? %s %s\n",
		   (value == ABSENT_STRING)
		   ? "absent"
		   : "cancel",
		   name);
	}
	code = 0;
    }
    return code;
}

static int
increment(long *all_parms, int *num_parms, int len_parms, int end_parms)
{
    int rc = 0;
    int n;

    if (len_parms > MAX_PARM)
	len_parms = MAX_PARM;

    if (end_parms < len_parms) {
	if (all_parms[end_parms]++ >= num_parms[end_parms]) {
	    all_parms[end_parms] = 0;
	    increment(all_parms, num_parms, len_parms, end_parms + 1);
	}
    }
    for (n = 0; n < len_parms; ++n) {
	if (all_parms[n] != 0) {
	    rc = 1;
	    break;
	}
    }
    /* return 1 until the vector resets to all 0's */
    return rc;
}

/* parse the format string to determine which positional parameters
 * are assumed to be strings.
 */
#if HAVE_TISCAN_S
static int
analyze_format(const char *format, int *mask, char **p_is_s)
{
    int arg_count;
    int arg_mask;
    int n;
    if (tiscan_s(&arg_count, &arg_mask, format) == OK) {
	*mask = arg_mask;
	for (n = 0; n < MAX_PARM; ++n) {
	    static char dummy[1];
	    p_is_s[n] = (arg_mask & 1) ? dummy : NULL;
	    arg_mask >>= 1;
	}
    } else {
	*mask = 0;
	arg_count = 0;
	for (n = 0; n < MAX_PARM; ++n) {
	    p_is_s[n] = NULL;
	}
    }
    return arg_count;
}
#elif HAVE__NC_TPARM_ANALYZE
extern int _nc_tparm_analyze(TERMINAL *, const char *, char **, int *);

static int
analyze_format(const char *format, int *mask, char **p_is_s)
{
    int popcount = 0;
    int analyzed = _nc_tparm_analyze(cur_term, format, p_is_s, &popcount);
    int n;
    if (analyzed < popcount) {
	analyzed = popcount;
    }
    *mask = 0;
    for (n = 0; n < MAX_PARM; ++n) {
	if (p_is_s[n])
	    *mask |= (1 << n);
    }
    return analyzed;
}
#else
/* TODO: make this work without direct use of ncurses internals. */
static int
analyze_format(const char *format, int *mask, char **p_is_s)
{
    int n;
    char *filler = strstr(format, "%s");
    *mask = 0;
    for (n = 0; n < MAX_PARM; ++n) {
	p_is_s[n] = filler;
    }
    return n;
}
#endif

#define NumStr(n) use_strings[n] \
 		  ? (long) (my_intptr_t) (number[n] \
		     ? string[n] \
		     : NULL) \
		  : number[n]

#define NS_0(fmt)	fmt
#define NS_1(fmt)	NS_0(fmt), NumStr(0)
#define NS_2(fmt)	NS_1(fmt), NumStr(1)
#define NS_3(fmt)	NS_2(fmt), NumStr(2)
#define NS_4(fmt)	NS_3(fmt), NumStr(3)
#define NS_5(fmt)	NS_4(fmt), NumStr(4)
#define NS_6(fmt)	NS_5(fmt), NumStr(5)
#define NS_7(fmt)	NS_6(fmt), NumStr(6)
#define NS_8(fmt)	NS_7(fmt), NumStr(7)
#define NS_9(fmt)	NS_8(fmt), NumStr(8)

static void
test_tparm(const char *name, const char *format, long *number, char **string)
{
    char *use_strings[MAX_PARM];
    char *result = NULL;
    int nparam;
    int mask;

    nparam = analyze_format(format, &mask, use_strings);
#if HAVE_TIPARM_S
    if (s_opt) {
	switch (nparam) {
	case 0:
	    result = tiparm_s(0, mask, NS_0(format));
	    break;
	case 1:
	    result = tiparm_s(1, mask, NS_1(format));
	    break;
	case 2:
	    result = tiparm_s(2, mask, NS_2(format));
	    break;
	case 3:
	    result = tiparm_s(3, mask, NS_3(format));
	    break;
	case 4:
	    result = tiparm_s(4, mask, NS_4(format));
	    break;
	case 5:
	    result = tiparm_s(5, mask, NS_5(format));
	    break;
	case 6:
	    result = tiparm_s(6, mask, NS_6(format));
	    break;
	case 7:
	    result = tiparm_s(7, mask, NS_7(format));
	    break;
	case 8:
	    result = tiparm_s(8, mask, NS_8(format));
	    break;
	case 9:
	    result = tiparm_s(9, mask, NS_9(format));
	    break;
	}
    } else
#endif
#if HAVE_TIPARM
    if (i_opt) {
	switch (nparam) {
	case 0:
	    result = tiparm(NS_0(format));
	    break;
	case 1:
	    result = tiparm(NS_1(format));
	    break;
	case 2:
	    result = tiparm(NS_2(format));
	    break;
	case 3:
	    result = tiparm(NS_3(format));
	    break;
	case 4:
	    result = tiparm(NS_4(format));
	    break;
	case 5:
	    result = tiparm(NS_5(format));
	    break;
	case 6:
	    result = tiparm(NS_6(format));
	    break;
	case 7:
	    result = tiparm(NS_7(format));
	    break;
	case 8:
	    result = tiparm(NS_8(format));
	    break;
	case 9:
	    result = tiparm(NS_9(format));
	    break;
	}
    } else
#endif
	result = tparm(NS_9(format));
    total_tests++;
    if (result != NULL) {
	tputs(result, 1, output_func);
    } else {
	total_fails++;
    }
    if (v_opt > 1) {
	int n;
	printf(".. %3d =", result != 0 ? (int) strlen(result) : -1);
	for (n = 0; n < nparam; ++n) {
	    if (use_strings[n]) {
		if (number[n]) {
		    printf(" \"%s\"", string[n]);
		} else {
		    printf("  ?");
		}
	    } else {
		printf(" %2ld", number[n]);
	    }
	}
	printf(" %s\n", name);
    }
}

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: test_tparm [options] [capability] [value1 [value2 [...]]]"
	,""
	,"Use tparm/tputs for all distinct combinations of given capability."
	,""
	,USAGE_COMMON
	,"Options:"
	," -T TERM  override $TERM; this may be a comma-separated list or \"-\""
	,"          to read a list from standard-input"
	," -a       test all combinations of parameters"
	,"          [value1...] forms a vector of maximum parameter-values."
#if HAVE_TIPARM
	," -i       test tiparm rather than tparm"
#endif
	," -p       test capabilities with no parameters but having padding"
	," -r NUM   repeat tests NUM times"
#if HAVE_TIPARM_S
	," -s       test tiparm_s rather than tparm"
#endif
	," -v       show values and results"
    };
    unsigned n;
    for (n = 0; n < SIZEOF(msg); ++n) {
	fprintf(stderr, "%s\n", msg[n]);
    }
    ExitProgram(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

#define PLURAL(n) n, (n != 1) ? "s" : ""
#define COLONS(n) (n >= 1) ? ":" : ""

#define NUMFORM "%10ld"
/* *INDENT-OFF* */
VERSION_COMMON()
/* *INDENT-ON* */

int
main(int argc, char *argv[])
{
    int ch;
    int n;
    int r_run, t_run, n_run;
    char *old_term = getenv("TERM");
    int r_opt = 1;
    char *t_opt = 0;

    int std_caps = 0;		/* predefine items in all_caps[] */
    int len_caps = 0;		/* cur # of items in all_caps[] */
    int max_caps = 10;		/* max # of items in all_caps[] */
    char **all_caps = typeCalloc(char *, max_caps);

    long all_parms[10];		/* workspace for "-a" option */

    int len_terms = 0;		/* cur # of items in all_terms[] */
    int max_terms = 10;		/* max # of items in all_terms[] */
    char **all_terms = typeCalloc(char *, max_terms);

    int use_caps;
    int max_name = 10;		/* max # of items in cap_name[] */
    int max_data = 10;		/* max # of items in cap_data[] */
    char **cap_name;
    char **cap_data;

    int len_parms = 0;		/* cur # of items in num_parms[], str_parms[] */
    int max_parms = argc + 10;	/* max # of items in num_parms[], str_parms[] */
    int *num_parms = typeCalloc(int, max_parms);
    char **str_parms = typeCalloc(char *, max_parms);
    long use_parms = 1;

    if (all_caps == 0 || all_terms == 0 || num_parms == 0 || str_parms == 0)
	failed("no memory");

    while ((ch = getopt(argc, argv, OPTS_COMMON "T:aipr:sv")) != -1) {
	switch (ch) {
	case 'T':
	    t_opt = optarg;
	    break;
	case 'a':
	    ++a_opt;
	    break;
#if HAVE_TIPARM
	case 'i':
	    ++i_opt;
	    break;
#endif
	case 'p':
	    ++p_opt;
	    break;
	case 'r':
	    r_opt = atoi(optarg);
	    break;
#if HAVE_TIPARM_S
	case 's':
	    ++s_opt;
	    break;
#endif
	case 'v':
	    ++v_opt;
	    break;
	case OPTS_VERSION:
	    show_version(argv);
	    ExitProgram(EXIT_SUCCESS);
	default:
	    usage(ch == OPTS_USAGE);
	    /* NOTREACHED */
	}
    }

    /*
     * If there is a nonnumeric parameter after the options, use that as the
     * capability name.
     */
    if (optind < argc) {
	if (!isNumeric(argv[optind])) {
	    all_caps[len_caps++] = strdup(argv[optind++]);
	}
    }

    /*
     * Any remaining arguments must be possible parameter values.  If numeric,
     * and "-a" is not set, use those as the actual values for which the
     * capabilities are tested.
     */
    while (optind < argc) {
	if (isNumeric(argv[optind])) {
	    char *dummy = 0;
	    long value = strtol(argv[optind], &dummy, 0);
	    num_parms[len_parms] = (int) value;
	}
	str_parms[len_parms] = argv[optind];
	++optind;
	++len_parms;
    }
    for (n = len_parms; n < max_parms; ++n) {
	static char dummy[1];
	str_parms[n] = dummy;
    }
    if (v_opt) {
	printf("%d parameter%s%s\n", PLURAL(len_parms), COLONS(len_parms));
	if (v_opt > 3) {
	    for (n = 0; n < len_parms; ++n) {
		printf(" %d: %d (%s)\n", n + 1, num_parms[n], str_parms[n]);
	    }
	}
    }

    /*
     * Make a list of values for $TERM.  Accept "-" for standard input to
     * simplify scripting a check of the whole database.
     */
    old_term = strdup((old_term == 0) ? "unknown" : old_term);
    if (t_opt != 0) {
	if (!strcmp(t_opt, "-")) {
	    char buffer[BUFSIZ];
	    while (fgets(buffer, sizeof(buffer) - 1, stdin) != 0) {
		char *s = buffer;
		char *t;
		while (isspace(UChar(s[0])))
		    ++s;
		t = s + strlen(s);
		while (t != s && isspace(UChar(t[-1])))
		    *--t = '\0';
		s = strdup(s);
		if (len_terms + 2 >= max_terms) {
		    max_terms *= 2;
		    all_terms = typeRealloc(char *, max_terms, all_terms);
		    if (all_terms == 0)
			failed("no memory: all_terms");
		}
		all_terms[len_terms++] = s;
	    }
	} else {
	    char *s = t_opt;
	    char *t;
	    while ((t = strtok(s, ",")) != 0) {
		s = 0;
		if (len_terms + 2 >= max_terms) {
		    max_terms *= 2;
		    all_terms = typeRealloc(char *, max_terms, all_terms);
		    if (all_terms == 0)
			failed("no memory: all_terms");
		}
		all_terms[len_terms++] = strdup(t);
	    }
	}
    } else {
	all_terms[len_terms++] = strdup(old_term);
    }
    all_terms[len_terms] = 0;
    if (v_opt) {
	printf("%d term%s:\n", PLURAL(len_terms));
	if (v_opt > 3) {
	    for (n = 0; n < len_terms; ++n) {
		printf(" %d: %s\n", n + 1, all_terms[n]);
	    }
	}
    }

    /*
     * If no capability name was selected, use the predefined list of string
     * capabilities.
     *
     * TODO: To address the "other" systems which do not follow SVr4,
     * just use the output from infocmp on $TERM.
     */
    if (len_caps == 0) {
#if defined(HAVE_CURSES_DATA_BOOLNAMES) || defined(DECL_CURSES_DATA_BOOLNAMES)
	for (n = 0; strnames[n] != 0; ++n) {
	    GrowArray(all_caps, max_caps, len_caps);
	    all_caps[len_caps++] = strdup(strnames[n]);
	}
#else
	all_caps[len_caps++] = strdup("cup");
	all_caps[len_caps++] = strdup("sgr");
#endif
    }
    std_caps = len_caps;
    all_caps[len_caps] = 0;
    if (v_opt) {
	printf("%d name%s%s\n", PLURAL(len_caps), COLONS(len_caps));
	if (v_opt > 3) {
	    for (n = 0; n < len_caps; ++n) {
		printf(" %d: %s\n", n + 1, all_caps[n]);
	    }
	}
    }

    cap_name = typeMalloc(char *, (max_name = 1 + len_caps));
    cap_data = typeMalloc(char *, (max_data = 1 + len_caps));

    if (r_opt <= 0)
	r_opt = 1;

    if (a_opt) {
	for (n = 0; n < max_parms; ++n)
	    if (num_parms[n])
		use_parms *= (num_parms[n] + 1);
    }

    for (r_run = 0; r_run < r_opt; ++r_run) {
	for (t_run = 0; t_run < len_terms; ++t_run) {
	    int errs;

	    if (setupterm(all_terms[t_run], fileno(stdout), &errs) != OK) {
		printf("** skipping %s (errs:%d)\n", all_terms[t_run], errs);
	    }
#if NCURSES_XNAMES
	    len_caps = std_caps;
	    if (cur_term) {
		TERMTYPE *term = (TERMTYPE *) cur_term;
		for (n = STRCOUNT; n < NUM_STRINGS(term); ++n) {
		    GrowArray(all_caps, max_caps, len_caps);
		    GrowArray(cap_name, max_name, len_caps);
		    GrowArray(cap_data, max_data, len_caps);
		    all_caps[len_caps++] = strdup(ExtStrname(term, (int) n, strnames));
		}
	    }
#else
	    (void) std_caps;
#endif

	    /*
	     * Most of the capabilities have no parameters, e.g., they are
	     * function-keys or simple operations such as clear-display.
	     * Ignore those, since they do not really exercise tparm.
	     */
	    use_caps = 0;
	    for (n = 0; n < len_caps; ++n) {
		char *value = tigetstr(all_caps[n]);
		if (relevant(all_caps[n], value)) {
		    cap_name[use_caps] = all_caps[n];
		    cap_data[use_caps] = value;
		    use_caps++;
		}
	    }

	    if (v_opt) {
		printf("[%d:%d] %d paramerized cap%s * %ld test-case%s \"%s\"\n",
		       r_run + 1, r_opt,
		       PLURAL(use_caps),
		       PLURAL(use_parms),
		       all_terms[t_run]);
	    }

	    memset(all_parms, 0, sizeof(all_parms));
	    if (a_opt) {
		/* for each combination of values */
		do {
		    for (n_run = 0; n_run < use_caps; ++n_run) {
			test_tparm(cap_name[n_run],
				   cap_data[n_run],
				   all_parms,
				   str_parms);
		    }
		}
		while (increment(all_parms, num_parms, len_parms, 0));
	    } else {
		/* for the given values */
		for (n_run = 0; n_run < use_caps; ++n_run) {
		    test_tparm(cap_name[n_run],
			       cap_data[n_run],
			       all_parms,
			       str_parms);
		}
	    }
#if NCURSES_XNAMES
	    for (n = std_caps; n < len_caps; ++n) {
		free(all_caps[n]);
	    }
#endif
	    if (cur_term != 0) {
		del_curterm(cur_term);
	    } else {
		printf("? no cur_term\n");
	    }
	}
    }

    printf("Tests:\n");
    printf(NUMFORM " total\n", total_tests);
    if (total_fails)
	printf(NUMFORM " failed\n", total_fails);
    printf("Characters:\n");
    printf(NUMFORM " nulls\n", total_nulls);
    printf(NUMFORM " controls\n", total_ctrls);
    printf(NUMFORM " printable\n", total_print);
    printf(NUMFORM " total\n", total_nulls + total_ctrls + total_print);
#if NO_LEAKS
    for (n = 0; n < std_caps; ++n) {
	free(all_caps[n]);
    }
    free(all_caps);
    free(old_term);
    for (n = 0; n < len_terms; ++n) {
	free(all_terms[n]);
    }
    free(all_terms);
    free(num_parms);
    free(str_parms);
    free(cap_name);
    free(cap_data);
#endif

    ExitProgram(EXIT_SUCCESS);
}

#else /* !HAVE_TIGETSTR */
int
main(void)
{
    failed("This program requires the terminfo functions such as tigetstr");
}
#endif /* HAVE_TIGETSTR */
