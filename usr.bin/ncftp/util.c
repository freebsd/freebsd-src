/* Util.c */

/*  $RCSfile: util.c,v $
 *  $Revision: 14020.13 $
 *  $Date: 93/05/23 09:38:13 $
 */

#include "sys.h"

#include <errno.h>
#include <ctype.h>
#include <pwd.h>

#ifndef NO_VARARGS
#	ifdef NO_STDARGH
#		include <varargs.h>
#	else
#		include <stdarg.h>
#	endif
#endif

#ifdef READLINE
#	include <readline/readline.h>
#endif /* READLINE */

#ifdef GETLINE
#	include <getline.h>
#endif

#include "util.h"
#include "cmds.h"
#include "main.h"
#include "ftp.h"
#include "ftprc.h"
#include "defaults.h"
#include "copyright.h"

/* Util.c globals */
int					Opterr = 1;			/* if error message should be printed */
int					Optind = 1;			/* index into parent argv vector */
int					Optopt;				/* character checked for validity */
char				*Optarg;			/* argument associated with option */
char				*Optplace = EMSG;	/* saved position in an arg */

/* Util.c externs */
extern int			toatty, fromatty;
extern int			verbose, doingInitMacro;
extern string		prompt2;
extern char			*line, *margv[];
extern int			margc;
extern int			debug, mprompt, activemcmd;
extern string		progname;
extern struct cmd	cmdtab[];
extern struct userinfo uinfo;

#ifndef NO_VARARGS
/*VARARGS*/
#ifdef NO_STDARGH
void dbprintf(va_alist)
	va_dcl
#else
void dbprintf(char *fmt0, ...)
#endif
{
	va_list ap;
	char *fmt;

#ifdef NO_STDARGH
	va_start(ap);
	fmt = va_arg(ap, char *);
#else
	va_start(ap, fmt0);
	fmt = fmt0;
#endif

	if (debug) {
		(void) fprintf(DB_STREAM, "#DB# ");
		(void) vfprintf(DB_STREAM, fmt, ap);
		(void) fflush(DB_STREAM);
	}
	va_end(ap);
}	/* dbprintf */

#endif	/* have varargs */




/*
 * Concatenate src on the end of dst.  The resulting string will have at most
 * n-1 characters, not counting the NUL terminator which is always appended
 * unlike strncat.  The other big difference is that strncpy uses n as the
 * max number of characters _appended_, while this routine uses n to limit
 * the overall length of dst.
 */
char *_Strncat(char *dst, char *src, register size_t n)
{
	register size_t i;
	register char *d, *s;

	if (n != 0 && ((i = strlen(dst)) < (n - 1))) {
		d = dst + i;
		s = src;
		/* If they specified a maximum of n characters, use n - 1 chars to
		 * hold the copy, and the last character in the array as a NUL.
		 * This is the difference between the regular strncpy routine.
		 * strncpy doesn't guarantee that your new string will have a
		 * NUL terminator, but this routine does.
		 */
		for (++i; i<n; i++) {
			if ((*d++ = *s++) == 0) {
				/* Pad with zeros. */
				for (; i<n; i++)
					*d++ = 0;
				return dst;
			}
		}
		/* If we get here, then we have a full string, with n - 1 characters,
		 * so now we NUL terminate it and go home.
		 */
		*d = 0;
	}
	return (dst);
}	/* _Strncat */


/*
 * Copy src to dst, truncating or null-padding to always copy n-1 bytes.
 * Return dst.
 */
char *_Strncpy(char *dst, char *src, register size_t n)
{
	register char *d;
	register char *s;
	register size_t i;

	d = dst;
	*d = 0;
	if (n != 0) {
		s = src;
		/* If they specified a maximum of n characters, use n - 1 chars to
		 * hold the copy, and the last character in the array as a NUL.
		 * This is the difference between the regular strncpy routine.
		 * strncpy doesn't guarantee that your new string will have a
		 * NUL terminator, but this routine does.
		 */
		for (i=1; i<n; i++) {
			if ((*d++ = *s++) == 0) {
				/* Pad with zeros. */
				for (; i<n; i++)
					*d++ = 0;
				return dst;
			}
		}
		/* If we get here, then we have a full string, with n - 1 characters,
		 * so now we NUL terminate it and go home.
		 */
		*d = 0;
	}
	return (dst);
}	/* _Strncpy */



/* Converts any uppercase characters in the string to lowercase.
 * Never would have guessed that, huh?
 */
void StrLCase(char *dst)
{
	register char *cp;

	for (cp=dst; *cp != '\0'; cp++)
		if (isupper((int) *cp))
			*cp = (char) tolower(*cp);
}




char *Strpcpy(char *dst, char *src)
{
	while ((*dst++ = *src++) != '\0')
		;
	return (--dst);	/* return current value of dst, NOT original value! */
}	/* Strpcpy */



/*
 * malloc's a copy of oldstr.
 */
char *NewString(char *oldstr)
{
	size_t howLong;
	char *newstr;

	howLong = strlen(oldstr);
	if ((newstr = malloc(howLong + 1)) != NULL)
		(void) strcpy(newstr, oldstr);
	return newstr;
}	/* NewString */





void Getopt_Reset(void)
{
	Optind = 1;
	Optplace = "";
}	/* Getopt_Reset */

static char *NextOption(char *ostr)
{
	if ((Optopt = (int) *Optplace++) == (int) ':')
		return 0;
	return index(ostr, Optopt);
}

int Getopt(int nargc, char **nargv, char *ostr)
{
	register char *oli;				   /* Option letter list index */

	if (!*Optplace) {					   /* update scanning pointer */
		if (Optind >= nargc || *(Optplace = nargv[Optind]) != '-')
			return (EOF);
		if (Optplace[1] && *++Optplace == '-') {	/* found "--" */
			++Optind;
			return (EOF);
		}
	}								   /* Option letter okay? */
	oli = NextOption(ostr);
	if (oli == NULL) {
		if (!*Optplace)
			++Optind;
		if (Opterr) {
			(void) fprintf(stderr, "%s%s%c\n", *nargv, ": illegal option -- ", Optopt);
			return(BADCH);
		}
	}
	if (*++oli != ':') {			   /* don't need argument */
		Optarg = NULL;
		if (!*Optplace)
			++Optind;
	} else {						   /* need an argument */
		if (*Optplace)					   /* no white space */
			Optarg = Optplace;
		else if (nargc <= ++Optind) {  /* no arg */
			Optplace = EMSG;
			if (Opterr) {
				(void) fprintf(stderr, "%s%s%c\n", *nargv, ": option requires an argument -- ", Optopt);
				return(BADCH);
			}
		} else						   /* white space */
			Optarg = nargv[Optind];
		Optplace = EMSG;
		++Optind;
	}
	return (Optopt);				   /* dump back Option letter */
}									   /* Getopt */





/*
 * Converts an ls date, in either the "Feb  4  1992" or "Jan 16 13:42"
 * format to a time_t.
 */
unsigned long UnLSDate(char *dstr)
{
#ifdef NO_MKTIME
	return (MDTM_UNKNOWN);
#else
	char *cp = dstr;
	int mon, day, year, hr, min;
	time_t now, mt;
	unsigned long result = MDTM_UNKNOWN;
	struct tm ut, *t;

	switch (*cp++) {
		case 'A':
			mon = (*cp == 'u') ? 7 : 3;
			break;
		case 'D':
			mon = 11;
			break;
		case 'F':
			mon = 1;
			break;
		default:					   /* shut up un-init warning */
		case 'J':
			if (*cp++ == 'u')
				mon = (*cp == 'l') ? 6 : 5;
			else
				mon = 0;
			break;
		case 'M':
			mon = (*++cp == 'r') ? 2 : 4;
			break;
		case 'N':
			mon = 10;
			break;
		case 'O':
			mon = 9;
			break;
		case 'S':
			mon = 8;
	}
	cp = dstr + 4;
	day = 0;
	if (*cp != ' ')
		day = 10 * (*cp - '0');
	cp++;
	day += *cp++ - '0';
	min = 0;
	
	(void) time(&now);
	t = localtime(&now);

	if (*++cp != ' ') {
		/* It's a time, XX:YY, not a year. */
		cp[2] = ' ';
		(void) sscanf(cp, "%d %d", &hr, &min);
		cp[2] = ':';
		year = t->tm_year;
		if (mon > t->tm_mon)
			--year;
	} else {
		hr = min = 0;
		(void) sscanf(cp, "%d", &year);
		year -= 1900;
	}
	/* Copy the whole structure of the 'tm' pointed to by t, so it will
	 * also set all fields we don't specify explicitly to be the same as
	 * they were in t.  That way we copy non-standard fields such as
	 * tm_gmtoff, if it exists or not.
	 */
	ut = *t;
	ut.tm_sec = 1;
	ut.tm_min = min;
	ut.tm_hour = hr;
	ut.tm_mday = day;
	ut.tm_mon = mon;
	ut.tm_year = year;
	ut.tm_wday = ut.tm_yday = 0;
	mt = mktime(&ut);
	if (mt != (time_t) -1)
		result = (unsigned long) mt;
	return (result);
#endif	/* NO_MKTIME */
}	/* UnLSDate */



/*
 * Converts a MDTM date, like "213 19930602204445\n"
 * format to a time_t.
 */
unsigned long UnMDTMDate(char *dstr)
{
#ifdef NO_MKTIME
	return (MDTM_UNKNOWN);
#else
	struct tm ut;
	time_t mt;
	unsigned long result = MDTM_UNKNOWN;

	/* Clear out the whole structure, along with any non-standard fields. */
	bzero((char *)&ut, sizeof (struct tm));

	if (sscanf(dstr, "%*s %04d%02d%02d%02d%02d%02d",
		&ut.tm_year,
		&ut.tm_mon,
		&ut.tm_mday,
		&ut.tm_hour,
		&ut.tm_min,
		&ut.tm_sec) == 6)
	{	
		--ut.tm_mon;
		ut.tm_year -= 1900;
		mt = mktime(&ut);
		if (mt != (time_t) -1)
			result = (unsigned long) mt;
	}
	return result;
#endif	/* NO_MKTIME */
}	/* UnMDTMDate */



void Perror(
#ifdef DB_ERRS
			char *fromProc
			,
#ifdef __LINE__
			int lineNum,
#endif
#endif
			char *msg
			)
{
	extern int errno;

	if (NOT_VQUIET) {
#ifdef sun
	/*
	 * There is a problem in the SunOS headers when compiling with an ANSI
	 * compiler.  The problem is that there are macros in the form of
	 * #define MAC(x) 'x', and this will always be the character x instead
	 * of whatever parameter was passed to MAC.  If we get these errors, it
	 * usually means that you are trying to compile with gcc when you haven't
	 * run the 'fixincludes' script that fixes these macros.  We will ignore
	 * the error, but it means that the echo() function won't work correctly,
	 * and you will see your password echo.
	 */
		if (errno == ENOTTY)
			return;
#endif
		(void) fprintf(stderr, "NcFTP");
#ifdef DB_ERRS
		if (fromProc != NULL)
			(void) fprintf(stderr, "/%s", fromProc);
#ifdef __LINE__
		(void) fprintf(stderr, "/%d", lineNum);
#endif
#endif
		(void) fprintf(stderr, ": ");
		if (msg != NULL)
			(void) fprintf(stderr, "%s (%d): ", msg, errno);
		perror(NULL);
	}
}	/* Perror */




size_t RemoveTrailingNewline(char *cp, int *stripped)
{
	size_t len;
	int nBytesStripped = 0;

    if (cp != NULL) {
		cp += (len = strlen(cp)) - 1;
		if (*cp == '\n') {
			*cp-- = 0;    /* get rid of the newline. */
			nBytesStripped++;
		}
		if (*cp == '\r') { /* no returns either, please. */
			*cp = 0;
			nBytesStripped++;
		}
		if (stripped != NULL)
			*stripped = nBytesStripped;
		return len;
	}
	return (size_t)0;
}	/* RemoveTrailingNewline */



#ifdef GETLINE
extern size_t epromptlen;

/*
 * The Getline library doesn't detect the ANSI escape sequences, so the
 * library would think that a string is longer than actually appears on
 * screen.  This function lets Getline work properly.  This function is
 * intended to fix that problem for the main command prompt only.  If any
 * other prompts want to use ANSI escapes, a (costly) function would have
 * to scan the prompt for all escape sequences.
 */
/*ARGSUSED*/
static size_t MainPromptLen(char *pr)
{
	return (int)epromptlen;
}
#endif

static char *StdioGets(char *promptstr, char *sline, size_t size)
{
	char *cp;

	if (fromatty) {
		/* It's okay to print a prompt if we are redirecting stdout,
		 * as long as stdin is still a tty.  Otherwise, don't print
		 * a prompt at all if stdin is redirected.
		 */
#ifdef CURSES
		tcap_put(promptstr);
#else
		(void) fputs(promptstr, stdout);
#endif
	}
	sline[0] = 0;
	(void) fflush(stdout);	/* for svr4 */
	cp = fgets(sline, (int)(size - 2), stdin);
	(void) RemoveTrailingNewline(sline, NULL);
	return cp;
}	/* StdioGets */


/* Given a prompt string, a destination string, and it's size, return feedback
 * from the user in the destination string, with any trailing newlines
 * stripped.  Returns NULL if EOF encountered.
 */
char *Gets(char *promptstr, char *sline, size_t size)
{
	char *cp, ch;
	string plines;
#ifdef GETLINE
	int ismainprompt = (promptstr == prompt2);
#endif

	if (!fromatty || !toatty) {
		/* Don't worry about a cmdline/history editor if you redirected a
		 * file at me.
		 */
		return (StdioGets(promptstr, sline, size));
	}

	sline[0] = 0;	/* Clear it, in case of an error later. */

	/*
	 * The prompt string may actually be several lines if the user put a
	 * newline in it with the @N option.  In this case we only want to print
	 * the very last line, so the command-line editors won't screw up.  So
	 * now we print all the lines except the last line.
	 */
	cp = rindex(promptstr, '\n');
	if (cp != NULL) {
		ch = *++cp;
		*cp = 0;
		(void) Strncpy(plines, promptstr);
		*cp = ch;
		promptstr = cp;
#ifdef CURSES
	    tcap_put(plines);
#else
		(void) fputs(plines, stdout);
#endif
	}

#ifdef READLINE
	if ((cp = readline(promptstr)) != NULL) {
		(void) _Strncpy(sline, cp, size);
		free(cp);
		(void) RemoveTrailingNewline(cp = sline, NULL);
		if (*cp != 0)	/* Don't add blank lines to history buffer. */
			add_history(cp);
	}
#else	/* READLINE */

#ifdef GETLINE
	if (toatty) {
		if (ismainprompt)
			gl_strwidth(MainPromptLen);
		if ((cp = getline(promptstr)) != NULL) {
			if (*cp == '\0')	/* You hit ^D. */
				return NULL;
			cp = _Strncpy(sline, cp, size);
			(void) RemoveTrailingNewline(cp, NULL);
			if (*cp != '\0') {	/* Don't add blank lines to history buffer. */
				gl_histadd(cp);
			}
		}
		/* Hope your strlen is declared as returning a size_t. */
		gl_strwidth(strlen);
	} else {
		cp = StdioGets(promptstr, sline, size);
	}
#else /* !GETLINE */
	cp = StdioGets(promptstr, sline, size);
#endif /* !GETLINE */
#endif /* !READLINE */
	return cp;
}	/* Gets */




char **re_makeargv(char *promptstr, int *argc)
{
	size_t sz;

	(void) strcat(line, " ");
	sz = strlen(line);
	(void) Gets(promptstr, &line[sz], (size_t) (CMDLINELEN - sz)) ;
	(void) makeargv();
	*argc = margc;
	return (margv);
}	/* re_makeargv */



#ifndef HAS_GETCWD
extern char *getwd(char *);
#endif

char *get_cwd(char *buf, int size)
{
#ifdef HAS_GETCWD
#	ifdef NO_UNISTDH
#		ifdef GETCWDSIZET
			extern char *getcwd(char *, size_t);
#		else
			extern char *getcwd(char *, int);
#		endif
#	endif
	return (getcwd(buf, size - 1));
#else
#ifndef MAXPATHLEN
#	define MAXPATHLEN (1024)
#endif
	static char *cwdbuf = NULL;

	if (cwdbuf == NULL) {
		cwdbuf = (char *)malloc((size_t) MAXPATHLEN);
		if (cwdbuf == NULL)
			fatal("out of memory for getwd buffer.");
	}
        getwd(cwdbuf);
        return (_Strncpy(buf, cwdbuf, (size_t)size));
#endif
}   /* get_cwd */



int tmp_name(char *str)
{
	(void) strcpy(str, "/tmp/ncftpXXXXXX");
	return (!mktemp(str));
}	/* tmp_name */




char *onoff(int boolf)
{
	return (boolf ? "on" : "off");
}   /* onoff */




int StrToBool(char *s)
{
	int c;
	int result;

    c = tolower(*s);
    result = 0;
    switch (c) {
        case 'f':           /* false */
		case 'n':			/* no */
            break;
        case 'o':           /* test for "off" and "on" */
            c = tolower(s[1]);
            if (c == 'f')
				break;
			/* fall through */
        case 't':           /* true */
		case 'y':			/* yes */
            result = 1;
            break;
        default:            /* 1, 0, -1, other number? */
            if (atoi(s) != 0)
            	result = 1;
    }
    return result;
}   /* StrToBool */




int confirm(char *cmd, char *file)
{
	string str, pr;

	if (!fromatty || (activemcmd && !mprompt) || (doingInitMacro))
		return 1;
	(void) sprintf(pr, "%s %s? ", cmd, file);
	(void) Gets(pr, str, sizeof(str));
	return (*str != 'n' && *str != 'N');
}	/* confirm */



void fatal(char *msg)
{
	(void) fprintf(stderr, "%s: %s\n", progname, msg);
	close_up_shop();
	exit(1);
}	/* fatal */




int UserLoggedIn(void)
{
	static int inited = 0;
	static int parent_pid, stderr_was_tty;

	if (!inited) {
		stderr_was_tty = isatty(2);
		parent_pid = getppid();
		inited++;
	}
	if ((stderr_was_tty && !isatty(2)) || (getppid() != parent_pid))
		return 0;
	return 1;
}	/* UserLoggedIn */




struct cmd *getcmd(char *name)
{
	struct cmd *c, *found;
	int nmatches;
	size_t len;
	char *p;

	found = (struct cmd *)0;
	if (name != NULL) {
		len = strlen(name);
		nmatches = 0;
		for (c = cmdtab; (p = c->c_name) != NULL; c++) {
			if (strcmp(name, p) == 0) {
				/* Exact match. */
				found = c;
				goto xx;
			}
			if (c->c_handler == unimpl)
				continue;
			if (strncmp(name, p, len) == 0) {
				if (++nmatches > 1) {
					found = ((struct cmd *) -1);	
					goto xx;
				}				
				found = c;
			} else if (found != NULL)
				break;
		}
	}
xx:
	return (found);
}	/* getcmd */




void cmd_help(struct cmd *c)
{
	(void) printf("%s: %s.\n",
		c->c_name,
		c->c_help
	);
}	/* cmd_help */




void cmd_usage(struct cmd *c)
{
	if (c->c_usage != NULL)
		(void) printf("Usage: %s%s\n",
			c->c_name,
			c->c_usage
		);
}	/* cmd_usage */




/*
 * A simple function that translates most pathnames with ~, ~user, or
 * environment variables as the first item.  It won't do paths with env vars
 * or ~s in the middle of the path, but those are extremely rare.
 */
char *LocalPath(char *path)
{
	longstring orig;
	struct passwd *pw;
	char *firstent;
	char *cp, *dp, *rest;

	(void) Strncpy(orig, path);
	firstent = orig;
	if ((cp = index(orig, '/')) != NULL) {
		if (cp == orig) {
			/* If we got here, the path is actually a full path name,
			 * with the first character as a slash, so just leave it
			 * alone.
			 */
			return (path);
		}
		/* Otherwise we can look at the first word of the path, and
		 * try to expand it, like $HOME/ or ~/, or it is a relative path, 
		 * which is okay since we won't really do anything with it.
		 */
		*cp = 0;
		rest = cp + 1;
		/* 'firstent' now contains the first 'word' in the path. */
	} else {
		/* Path was just a single word, or it is a full path, like:
		 * /usr/tmp/zz, so firstent is just the entire given "path."
		 */
		rest = NULL;
	}
	if (orig[0] == '~') {
		if (orig[1] == 0) {
			firstent = uinfo.homedir;
		} else {
			pw = getpwnam(orig + 1);
			if (pw != NULL)
				firstent = pw->pw_dir;
		}
	} else if (orig[0] == '$') {
		cp = orig + 1;
		dp = orig + strlen(orig) - 1;
		if ((*cp == '(' && *dp == ')') || (*cp == '{' && *dp == '}')) {
			cp++;
			*dp = 0;
		}
		firstent = getenv(cp);
		if (firstent == NULL) {
			(void) fprintf(stderr, "%s: no such environment variable.\n", cp);
			firstent = "badEnvVar";
		}
	}
	if (rest == NULL)
		(void) strcpy(path, firstent);
	else 
		(void) sprintf(path, "%s/%s", firstent, rest);
	return (path);
}	/* LocalPath */



/*
 * A special case, where invisible dot-files that would normally appear in
 * your home directory will appear instead as visible files in your $DOTDIR
 * directory if you have one.
 */

#define LCMP(b) (strncmp(path, (b), (o = sizeof(b) - 1)) == 0)

char *LocalDotPath(char *path)
{
	size_t o;
	longstring s, s2;
    char *cp = getenv("DOTDIR");

	if (cp == NULL) {
		goto aa;
	} else {
		if (*cp != '/' && *cp != '~') {
			/* then maybe they mean relative to $HOME. */
			(void) sprintf(s2, "%s/%s", uinfo.homedir, cp);
			cp = s2;
		}
		if (LCMP("~/.") ||
			LCMP("$HOME/.") ||
			LCMP("$home/.") ||
			LCMP("$(HOME)/.") ||
			LCMP("${HOME}/.")
		) {
			(void) Strncpy(s, path);
			(void) sprintf(path, "%s/%s", cp, s + o);
			cp = path;
		} else {
aa:			cp = LocalPath(path);
		}
	}
	return cp;
}	/* LocalDotPath */

#ifdef NO_STRSTR

/*
 *  The Elm Mail System  -  $Revision: 5.1 $   $State: Exp $
 *
 *			Copyright (c) 1988-1992 USENET Community Trust
 *			Copyright (c) 1986,1987 Dave Taylor
 */

char *strstr(s1, s2)
char *s1, *s2;
{
	int len;
	char *ptr;
	char *tmpptr;

	ptr = NULL;
	len = strlen(s2);

	if ( len <= strlen(s1)) {
	    tmpptr = s1;
	    while ((ptr = index(tmpptr, (int)*s2)) != NULL) {
	        if (strncmp(ptr, s2, len) == 0) {
	            break;
	        }
	        tmpptr = ptr+1;
	    }
	}
	return (ptr);
}

#endif


#ifdef NO_RENAME
int rename(oldname, newname)
const char *oldname, *newname;
{
	return (link(oldname, newname) == 0 ? unlink(oldname) : -1);
}
#endif /*NO_RENAME*/


/* eof Util.c */
