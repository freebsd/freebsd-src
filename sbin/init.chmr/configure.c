/*
 * Copyright (c) 1993 Christoph M. Robitschko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christoph M. Robitschko
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * configure.c
 * the main part of the configuration subsystem.
 */

#ifdef CONFIGURE

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ttyent.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <syslog.h>
#include <stdio.h>

#include "init.h"
#include "prototypes.h"
#include "cf_defs.h"



/* Prototypes for local functions: */
static int	evaluate_line(char *, int, int, char **, int);
static int	parseline(char *, int, int, char **);


char	*fgetln(FILE *, size_t *);			/* XXX */


extern const struct Command	Commands[];
extern const int		NCommands;



/*
 * CONFIGURE
 * read a configuration file
 */
void
configure(filename)
char		*filename;
{
static int	ncalled = 0;
FILE		*cf;
char		*fline, *line, *s, *errmsg;
int		lineno;
char		**cline;
int		argc;
size_t		len;


	Debug (1, "Configuring from file %s", filename);
	if (++ncalled > 50) {
		syslog(LOG_ERR, "include loop in configuration files !");
		ncalled --;
		return;
	}

	cf = fopen(filename, "r");
	if (!cf) {
	    Debug(1, "Could not open config file %s: %m", filename);
	    ncalled --;
	    return;
	}
	Debug(1, "Config file %s opened.", filename);

	if ((line = (char *) malloc(1)) == (char *) 0) {
		syslog(LOG_ERR, "%s: %s", filename, strerror(errno));
		ncalled --;
		return;
	}

	lineno = 0;
	while ((fline = fgetln(cf, &len))) {
		lineno ++;
		if (*fline == '#')
			continue;		/* Skip comment line */
		else if (fline[len - 1] == '\n')
			--len;
		if ((line = (char *) realloc(line, len + 1)) == (char *) 0) {
			syslog(LOG_ERR, "%s: %s", filename, strerror(errno));
			break;
		}
		bcopy(fline, line, len);
		line[len] = '\0';
		for (s = line; *s; s++)
			if ((*s != ' ') && (*s != '\t'))
				break;
		if (!*s)
			continue;		/* Skip empty line */
		cline = string_to_argv(line, &argc, &errmsg);
		if (errmsg)
			syslog(LOG_ERR, "%s line %d: %s", filename, lineno, errmsg);
		if (cline && argc > 0)
			(void)parseline(filename, lineno, argc, cline);
	}
	free(line);
	fclose(cf);
	ncalled --;
}


/*
 * Local support functions
 */

/* PARSELINE -- the obvious */
static int
parseline(configfile, linenumber, argc, argv)
char		*configfile;
int		linenumber;
int		argc;
char		**argv;
{
int		f, level;
int		nmatch, wmatch;
char		comline[1024];		/* XXX should test for overflow */


	comline[0] = '\0';
	/* Search through the Commands list;
	 * Each argv[level] is matched against a list of Commands of the same level,
	 * which must be subcommands of argv[level-1] if level > 0.

	 * There are two types of Matches: T_EX 'Exact match' means that argv[level]
	 * is an abbreviation of Commands[f].name, case ignored.
	 * T_STR, T_NUM etc match always (the validity of argv[level] is checked
	 * when it is evaluated, later), unless there has already been an exact
	 * match for the same argv[level].

	 * Note that the search is not aborted after a match is found, but
	 * continues up to the end of the list or the end of the scope of the
	 * previous (sub)command (i.e. if Commands[f].level < level).
	 * At that point, exactly one match must have been found. The search
	 * is then restarted for the next level, beginning at the previous match.
	 */

	for (f=nmatch=wmatch=level=0; f<NCommands; f++) {
		if (((Commands[f].level & LEVEL) == level) && 
		     (((Commands[f].type != T_EX) && (!nmatch)) ||	/* Values match always, unless there was an exact match previously */
		      (!strCcmp(argv[level], Commands[f].name))))	/* Exact match */
				 nmatch++, wmatch = f;

		if ((f == NCommands -1) || ((Commands[f].level & LEVEL) < level))
		    switch (nmatch) {
		    	case 0:	syslog(LOG_ERR, "%s line %d: unknown word \"%s\"", configfile, linenumber, argv[level]);
		    		return(-1);
			default:syslog(LOG_ERR, "%s line %d: \"%s\" is ambiguous", configfile, linenumber, argv[level]);
				return(-1);
		    	case 1: if (level) strcat (comline, " ");
		    		strcat (comline, Commands[wmatch].name);
		    		if (Commands[wmatch].level & SUB) {
		    		    if (++level >= argc) {
		    		    	syslog(LOG_ERR, "%s line %d: required parameter missing to command \"%s\"", configfile, linenumber, comline);
		    		    	return (-1);
		    		    }
		    		    f = wmatch;
		    		    nmatch = 0;
		    		} else {	/* XXX OPT not yet implemented */
		    		    if (level +1 != argc) {
		    		    	syslog(LOG_ERR, "%s line %d: too many arguments to command \"%s\"", configfile, linenumber, comline);
		    		    	return(-1);
		    		    }
				Debug(3, "%s line %d: OK \"%s\"", configfile, linenumber, comline);
				return(evaluate_line(configfile, linenumber, argc, argv, wmatch));
		    		}
		    		break;
		    }
		    /* NOTREACHED */
	}
	return(-1);	/* to stop gcc from complaining */
}



/*
 * Evaluate_line
 * Execute the action (set a variable, call a function) specified in the
 * Commands entry of the *last* argument on the config line.
 */
/* XXX no support for string values; only last argument evaluated */
static int
evaluate_line(cfgfile, lineno, argc, argv, wmatch)
char		*cfgfile;
int		lineno, argc, wmatch;
char		**argv;
{
char		*s, *arg;
long		tmp, tmp2;


	arg = argv[argc-1];
	if (Commands[wmatch].flags & CFUNC) {		/* Call the function */
		int (* func)() = Commands[wmatch].var;
		return(func(cfgfile, lineno, argc, argv, Commands[wmatch].val));
	}

	switch (Commands[wmatch].type) {
	case T_STR:				/* set var to String */
		s = newstring(arg);
		if (s)			/* XXX ohne Geld ka Musi */
		    *(char **)Commands[wmatch].var = s;
		return(0);		/* XXX Flags handling */
	case T_EX:				/* Copy val to *var */
		tmp = Commands[wmatch].val.intval;
		break;
	case T_INT:				/* Arg is an integer */
		tmp = strtol(arg, &s, 0);
		if ((!*arg) || (*s)) {
			syslog(LOG_ERR, "%s line %d: \"%s\" is not an integer", cfgfile, lineno, arg);
			return(-1);
		}
		break;
	case T_BYTE:				/* Arg is a byte limit */
		tmp = strtol(arg, &s, 0);
		if ((tmp < 0) || (*arg > '9') || (*arg < '0'))
			goto notbyte;
		switch (*s) {
			case '\0':
				tmp *= 1024;
				break;
			case 'm': case 'M':
				tmp *= 1024;
				/* no break */
			case 'k': case 'K':
				tmp *= 1024;
				/* no break */
			case 'b': case 'B':
				if (!*++s)
					break;
			default:
notbyte:
				syslog(LOG_ERR, "%s line %d: \"%s\" is not a valid bytecount", cfgfile, lineno, arg);
				return(-1);
		}
		break;
	case T_TIME:				/* Arg is a time limit */
		tmp = strtol(arg, &s, 0);
		if ((tmp < 0) || (*arg > '9') || (*arg < '0'))
			goto nottime;
		switch (*s) {
			case '\0':
				break;
			case 'h': case 'H':
				tmp *= 60;
				/* no break */
			case 'm': case 'M':
				tmp *= 60;
				/* no break */
			case 's': case 'S':
				if (!*++s)
					break;
			case ':':
				if ((tmp < 0) || (tmp > 59)) goto nottime;
				tmp *= 60;
				if ((*++s < '0') || (*s > '9')) goto nottime;
				tmp2 = strtol(s, &s, 0);
				if ((tmp2 < 0) || (tmp2 > 59)) goto nottime;
				tmp += tmp2;
				if (!*s) break;
				if (*s != ':') goto nottime;
				tmp *= 60;
				if ((*++s < '0') || (*s > '9')) goto nottime;
				tmp2 = strtol(s, &s, 0);
				if ((tmp2 < 0) || (tmp2 > 59)) goto nottime;
				tmp += tmp2;
				if (!*s) break;
				/* no break */
			default:
nottime:
				syslog(LOG_ERR, "%s line %d: \"%s\" is not a valid timespan", cfgfile, lineno, arg);
				return(-1);
		}
		break;
	default:
		syslog(LOG_ERR, "internal error: unknown argument type.", cfgfile, lineno);
		return(-1);
	}

	if (Commands[wmatch].flags & MAXVAL)
		if (tmp > Commands[wmatch].val.intval) {
			syslog(LOG_ERR, "%s line %d: \"%s\" (%d) is bigger than the allowed maximum (%d)", cfgfile, lineno, arg, tmp, Commands[wmatch].val.intval);
			return(-1);
		}
	if (Commands[wmatch].flags & NRAISE)
		if (tmp > *(int *)(Commands[wmatch].var)) {
			syslog(LOG_ERR, "%s line %d: Unable to raise limit beyond the current maximum (%d)", cfgfile, lineno, *(int *)Commands[wmatch].var);
			return(-1);
		}

	if (Commands[wmatch].var)
		*(int *)Commands[wmatch].var = tmp;
	return(0);
}


#endif	/* CONFIGURE */
