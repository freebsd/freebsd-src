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
 * ttytab.c
 * Everything that has to do with the getty table.
 * This includes starting child precesses.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <ttyent.h>
#include <syslog.h>
#include <stdio.h>


#include "init.h"
#include "prototypes.h"
#include "libutil.h"


#ifdef __gcc__
# define alloca __builtin_alloca
#endif

int		checkstatus	= DEF_CHECKSTATUS;
int		checktime	= DEF_CHECKTIME;
int		def_waittimes[] = {0,0,0,30,60,300,1800};
int		*waittimes	= def_waittimes;
int		nwaittimes	= 7;


/*
 * FREE_TTY
 * remove a ttytab entry from the list
 */
ttytab_t *
free_tty(tab, tt)
ttytab_t	*tab, *tt;
{
char		**s;


	/* Delete the entry from utmp, make log to wtmp */
	if (logout(tt->name))
		logwtmp(tt->name, "", "");

	/* unlink it from the list first, in case of unexpected signal */
	if (tt->next)
		tt->next->prev = tt->prev;
	if (tt->prev)
		tt->prev->next = tt->next;
	else
		tab = tt->next;

	/* free the associated memory */
	if (tt->name) free(tt->name);
	if (tt->type) free(tt->type);
	if (tt->argv) {
		for (s = tt->argv; *s; s++)
			free(*s);
		free(tt->argv);
	}
	free(tt);

	return(tab);
}


/*
 * ENT_TO_TAB
 * create or change a ttytab entry based on the information in a ttyent struct
 */
ttytab_t *
ent_to_tab(ent, tt, tab, flags)
const struct ttyent	*ent;
ttytab_t		*tab, *tt;
int			flags;
{
char		*argstr;


	/* Do we really need this entry ? */
	if (ent->ty_status & TTY_ON == 0)
		return(tab);

	Debug(4, "Processing tty entry %s", ent->ty_name);
	/* Allocate a ttytab entry, if not already there */
	if (!tt) {
		tt = malloc(sizeof(*tt));
		if (!tt)
			goto kaaplotz;
		tt->name	= (char *)0;
		tt->argv	= (char **)0;
		tt->type	= (char *)0;
		tt->intflags	= INIT_NEW;
		tt->pid		= 0;
		tt->failcount	= 0;
		tt->starttime	= (time_t)0;
		tt->next	= tab;
		tt->prev	= (ttytab_t *)0;
		if (tab)
		    tab->prev	= tt;
		tab		= tt;
		Debug(5, "Creating new ttytab entry");
	} else
		Debug(5, "Reusing existing ttytab entry");


	tt->intflags |= (flags | INIT_SEEN);


	/* fill each field in from the ttyent, if it has changed */
	if (strcmp(ent->ty_name, tt->name)) {
		Debug(5, "name differs: old=\"%s\" new=\"%s\"", tt->name, ent->ty_name);
		if (tt->name) free(tt->name);
		tt->name = newstring(ent->ty_name);
		tt->intflags |= INIT_CHANGED;
	}
	if (flags & INIT_NODEV)
		argstr = ent->ty_getty;
	else {
		if (!(argstr = alloca(strlen(ent->ty_getty) +strlen(tt->name) +2)))
			goto kaaplotz;
/* 		sprintf(argstr, "%s %s", ent->ty_getty, tt->name); */
		strcpy(argstr, ent->ty_getty);
		strcat(argstr, " ");
		strcat(argstr, tt->name);
	}
	if (argv_changed(argstr, (const char **)tt->argv)) {
		if (tt->argv) {
			char	**s;

			for (s = tt->argv; *s; s++)
				free(*s);
			free(tt->argv);
		}
		tt->argv = string_to_argv(argstr, (int *)0, (char **)0);
		tt->intflags |= INIT_CHANGED;
		Debug(5, "argv differs.");
	}
	if (strcmp(ent->ty_type, tt->type)) {
		if (tt->type) free(tt->type);
		tt->type = newstring(ent->ty_type);
		tt->intflags |= INIT_CHANGED;
		Debug(5, "type differs.");
	}

	if (!(tt->intflags & INIT_CHANGED)) {
		Debug(5, "entry unchanged.");
	} else
		Debug(5, "entry has been changed.");
	return(tab);


kaaplotz:
	syslog(LOG_ERR, "Out of memory in ent_to_tab");
	callout(retrytime, CO_ENT2TAB, NULL);
	return(tab);
}



/*
 * ARGV_CHANGED
 * compares a string and an argv
 */
int
argv_changed(string, argv)
const char	*string;
const char	**argv;
{
const char	*ss;
const char	**sa;


	if (!argv)
		return(1);
	for (ss = string, sa = argv; *sa; sa++) {
		register int	len = strlen(*sa);
		if (strncmp(*sa, ss, len))
			return(1);
		ss += len;
		if ((*ss != ' ') && (*ss != '\t') && (*ss))
			return(1);
		while ((*ss == ' ') || (*ss == '\t')) ss++;
	}
	if (*ss)		/* String longer than argv ? */
		return(1);
	else
		return(0);
}


/*
 * STRING_TO_ARGV
 * breaks up a string into words. Speration characters are SPACE and TAB,
 * unless prepended by a backslash \ or within double quotes ".
 * Quotes and backslash can be escaped by an additional \.
 * If errtext is non-NULL, it is set to point to an error message.
 */
char **
string_to_argv(string, rargc, errtext)
const char	*string;
int		*rargc;
char		**errtext;
{
const char	*s;
char		*buf, *t;
int		backslash, inquote;
int		argc, alloc_argc;
char		**argv;
char		**ra;


	Debug(4, "string_to_argv(\"%s\")", string);
	/*
	 * argv is allocated in chunks of ALLOC_ARGV pointers; if it runs
	 * out of space, it is realloc'ed with ALLOC_ARGV more pointers.
	 */
	argc = 0;
	argv = (char **)malloc(ALLOC_ARGV * sizeof(char *));
	if (!argv) return(argv);
	alloc_argc = ALLOC_ARGV;

	if (errtext)
		*errtext = (char *)0;

	for (s = string; *s;) {
		backslash = inquote = 0;
		for (; (*s == ' ') || (*s == '\t'); s++);	/* Skip blanks */
		if (!(*s)) break;
		if (!(buf = (char *)malloc(strlen(s)+1))) goto malloc_fail;
		for (t=buf;; s++) {
			if (backslash) {
				backslash = 0;
				*t++ = *s;
			} else switch (*s) {
				case '\\':
					backslash = 1;
					break;
				case '\"':
					inquote ^= 1;
					break;
				case '\0':
					if (inquote && errtext)
						*errtext = "Unmatched \".";
					goto end_of_word;
				case ' ': case '\t':
					if (!inquote)
						goto end_of_word;
					/* no break */
				default:
					*t++ = *s;
			}
		}

end_of_word:
		*t = '\0';
		if (!(argv[argc] = (char *)realloc(buf, strlen(buf)+1))) goto malloc_fail;

		if (++argc >= alloc_argc) {
			alloc_argc += ALLOC_ARGV;
			if (!(ra = realloc(argv, alloc_argc * sizeof(char **))))
				goto malloc_fail;
			argv = ra;
		}
	}

	/* Terminate argv with a NULL pointer and return */
	argv[argc] = (char *)0;
	if (rargc)
		*rargc = argc;
#ifdef DEBUG
	if (debug > 5) {
		for (ra = argv; *ra; ra++)
			Debug(5, "	\"%s\"", *ra);
		Debug(5, " ");
	}
#endif
	return(argv);

malloc_fail:
	/* free all so-far allocated memory and return a NULL pointer */
	for (; *argv; argv++)
		free(*argv);
	free(argv);
	return((char **)0);
}
		


/*
 * DO_GETTY
 * start a getty process for a ttytab entry
 */
int
do_getty(tt, status)
ttytab_t	*tt;
int		status;
{
int		pid;


	if (!tt)
		return(-1);
	Debug(2, "do_getty for %s", tt->name);

	if (tt->intflags & INIT_FAILSLEEP)
		tt->intflags &= ~INIT_FAILSLEEP;
	else {
	    /* Delete old entry from utmp, make log to wtmp */
	    if (logout(tt->name))
		    logwtmp(tt->name, "", "");

	    if ((checkstatus && status) || (checktime && (time(0) - tt->starttime <= checktime))) {
		if (tt->intflags & INIT_FAILED) {
		    if (++tt->failcount >= nwaittimes)
		    	tt->failcount = nwaittimes;
		} else {
		    tt->intflags |= INIT_FAILED;
	    	tt->failcount = 1;
		}
		if (waittimes[tt->failcount]) {
		    tt->intflags |= INIT_FAILSLEEP;
		    tt->pid = 0;
		    syslog(LOG_WARNING, "getty \"%s\" for %s failed, sleeping", tt->argv[0], tt->name);
		    callout (waittimes[tt->failcount], CO_GETTY, (void *)tt);
		    return(0);
		}
	    } else
		tt->intflags &= ~INIT_FAILED;
	}


#ifdef TESTRUN
	return (0);
#endif /* TESTRUN */

	tt->starttime = time(0);
	blocksig();
	switch ((pid=fork())) {
	    case -1:
		syslog(LOG_ERR, "fork failed for %s: %m", tt->name);
		unblocksig();
		callout(retrytime, CO_FORK, (void *)tt);
		return (-1);
	    case 0:
		signalsforchile();
		iputenv("TERM", tt->type);
		if (tt->intflags & INIT_OPEN) {
			int	fd;
			char	*device = alloca(strlen(tt->name) + sizeof("/dev/"));
/* 			sprintf(device, "/dev/%s", tt->name); */
			strcpy(device, "/dev/");
			strcat(device, tt->name);
			(void)revoke (device);
			closelog();
			fd = open(device, O_RDWR);
			if (fd < 0)
				syslog(LOG_ERR, "open %s failed: %m", device);
			if (login_tty(fd) < 0)
				syslog(LOG_ERR, "login_tty for %s failed: %m", device);
		}
#ifdef CONFIGURE
		setconf();
#endif
		closelog();		/* Necessary, because dup2 fails otherwise (?) */
		if (tt->intflags & INIT_ARG0)
			execve(tt->argv[0], tt->argv +1, ienviron);
		else
			execve(tt->argv[0], tt->argv, ienviron);
		syslog(LOG_ERR, "Exec \"%s\" failed for %s: %m", tt->argv[0], tt->name);
		_exit(1);
	    default:
	    	tt->pid = pid;
	    	unblocksig();
	    	return(0);
	}


}
