/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Regular expression library.
 * Define exactly one of the following to be 1:
 * HAVE_POSIX_REGCOMP: POSIX regcomp() and regex.h
 * HAVE_PCRE: PCRE (Perl-compatible regular expression) library
 * HAVE_RE_COMP: BSD re_comp()
 * HAVE_REGCMP: System V regcmp()
 * HAVE_V8_REGCOMP: Henry Spencer V8 regcomp() and regexp.h
 * NO_REGEX: pattern matching is supported, but without metacharacters.
 */
#undef HAVE_POSIX_REGCOMP
#undef HAVE_PCRE
#undef HAVE_RE_COMP
#undef HAVE_REGCMP
#undef HAVE_V8_REGCOMP
#undef NO_REGEX
#undef HAVE_REGEXEC2

/* Define HAVE_VOID if your compiler supports the "void" type. */
#undef HAVE_VOID

/* Define HAVE_CONST if your compiler supports the "const" modifier. */
#undef HAVE_CONST

/* Define HAVE_TIME_T if your system supports the "time_t" type. */
#undef HAVE_TIME_T

/* Define HAVE_STRERROR if you have the strerror() function. */
#undef HAVE_STRERROR

/* Define HAVE_FILENO if you have the fileno() macro. */
#undef HAVE_FILENO

/* Define HAVE_ERRNO if you have the errno variable */
/* Define MUST_DEFINE_ERRNO if you have errno but it is not define 
 * in errno.h */
#undef HAVE_ERRNO
#undef MUST_DEFINE_ERRNO

/* Define HAVE_SYS_ERRLIST if you have the sys_errlist[] variable */
#undef HAVE_SYS_ERRLIST

/* Define HAVE_OSPEED if your termcap library has the ospeed variable */
/* Define MUST_DEFINE_OSPEED if you have ospeed but it is not defined
 * in termcap.h. */
#undef HAVE_OSPEED
#undef MUST_DEFINE_OSPEED

/* Define HAVE_LOCALE if you have locale.h and setlocale. */
#undef HAVE_LOCALE

/* Define HAVE_TERMIOS_FUNCS if you have tcgetattr/tcsetattr */
#undef HAVE_TERMIOS_FUNCS

/* Define HAVE_UPPER_LOWER if you have isupper, islower, toupper, tolower */
#undef HAVE_UPPER_LOWER

/* Define HAVE_SIGSET_T you have the sigset_t type */
#undef HAVE_SIGSET_T

/* Define HAVE_SIGEMPTYSET if you have the sigemptyset macro */
#undef HAVE_SIGEMPTYSET

/* Define EDIT_PGM to your editor. */
#define EDIT_PGM	"vi"
