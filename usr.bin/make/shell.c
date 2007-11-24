/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "pathnames.h"
#include "shell.h"
#include "util.h"

/*
 * Descriptions for various shells. What the list of builtins should contain
 * is debatable: either all builtins or only those which may specified on
 * a single line without use of meta-characters. For correct makefiles that
 * contain only correct command lines there is no difference. But if a command
 * line, for example, is: 'if -foo bar' and there is an executable named 'if'
 * in the path, the first possibility would execute that 'if' while in the
 * second case the shell would give an error. Histerically only a small
 * subset of the builtins and no reserved words where given in the list which
 * corresponds roughly to the first variant. So go with this but add missing
 * words.
 */
#define	CSH_BUILTINS						\
	"alias cd eval exec exit read set ulimit unalias "	\
	"umask unset wait"

#define	SH_BUILTINS						\
	"alias cd eval exec exit read set ulimit unalias "	\
	"umask unset wait"

#define	CSH_META	"#=|^(){};&<>*?[]:$`\\@\n"
#define	SH_META		"#=|^(){};&<>*?[]:$`\\\n"

static const char *const shells_init[] = {
	/*
	 * CSH description. The csh can do echo control by playing
	 * with the setting of the 'echo' shell variable. Sadly,
	 * however, it is unable to do error control nicely.
	 */
	"name=csh path='" PATH_DEFSHELLDIR "/csh' "
	"quiet='unset verbose' echo='set verbose' filter='unset verbose' "
	"hasErrCtl=N check='echo \"%s\"\n' ignore='csh -c \"%s || exit 0\"' "
	"echoFlag=v errFlag=e "
	"meta='" CSH_META "' builtins='" CSH_BUILTINS "'",

	/*
	 * SH description. Echo control is also possible and, under
	 * sun UNIX anyway, one can even control error checking.
	 */
	"name=sh path='" PATH_DEFSHELLDIR "/sh' "
	"quiet='set -' echo='set -v' filter='set -' "
	"hasErrCtl=Y check='set -e' ignore='set +e' "
	"echoFlag=v errFlag=e "
	"meta='" SH_META "' builtins='" SH_BUILTINS "'",

	/*
	 * KSH description. The Korn shell has a superset of
	 * the Bourne shell's functionality. There are probably builtins
	 * missing here.
	 */
	"name=ksh path='" PATH_DEFSHELLDIR "/ksh' "
	"quiet='set -' echo='set -v' filter='set -' "
	"hasErrCtl=Y check='set -e' ignore='set +e' "
	"echoFlag=v errFlag=e "
	"meta='" SH_META "' builtins='" SH_BUILTINS "' unsetenv=T",

	NULL
};

/*
 * This is the shell to which we pass all commands in the Makefile.
 * It is set by the Job_ParseShell function.
 */
struct Shell *commandShell;

/*
 * This is the list of all known shells.
 */
static struct Shells shells = TAILQ_HEAD_INITIALIZER(shells);

void ShellDump(const struct Shell *) __unused;

/**
 * Helper function for sorting the builtin list alphabetically.
 */
static int
sort_builtins(const void *p1, const void *p2)
{

	return (strcmp(*(const char* const*)p1, *(const char* const*)p2));
}

/**
 * Free a shell structure and all associated strings.
 */
static void
ShellFree(struct Shell *sh)
{

	if (sh != NULL) {
		free(sh->name);
		free(sh->path);
		free(sh->echoOff);
		free(sh->echoOn);
		free(sh->noPrint);
		free(sh->errCheck);
		free(sh->ignErr);
		free(sh->echo);
		free(sh->exit);
		ArgArray_Done(&sh->builtins);
		free(sh->meta);
		free(sh);
	}
}

/**
 * Dump a shell specification to stderr.
 */
void
ShellDump(const struct Shell *sh)
{
	int i;

	fprintf(stderr, "Shell %p:\n", sh);
	fprintf(stderr, "  name='%s' path='%s'\n", sh->name, sh->path);
	fprintf(stderr, "  hasEchoCtl=%d echoOff='%s' echoOn='%s'\n",
	    sh->hasEchoCtl, sh->echoOff, sh->echoOn);
	fprintf(stderr, "  noPrint='%s'\n", sh->noPrint);
	fprintf(stderr, "  hasErrCtl=%d errCheck='%s' ignErr='%s'\n",
	    sh->hasErrCtl, sh->errCheck, sh->ignErr);
	fprintf(stderr, "  echo='%s' exit='%s'\n", sh->echo, sh->exit);
	fprintf(stderr, "  builtins=%d\n", sh->builtins.argc - 1);
	for (i = 1; i < sh->builtins.argc; i++)
		fprintf(stderr, " '%s'", sh->builtins.argv[i]);
	fprintf(stderr, "\n  meta='%s'\n", sh->meta);
	fprintf(stderr, "  unsetenv=%d\n", sh->unsetenv);
}

/**
 * Parse a shell specification line and return the new Shell structure.
 * In case of an error a message is printed and NULL is returned.
 */
static struct Shell *
ShellParseSpec(const char *spec, Boolean *fullSpec)
{
	ArgArray	aa;
	struct Shell	*sh;
	char		*eq;
	char		*keyw;
	int		arg;

	*fullSpec = FALSE;

	sh = emalloc(sizeof(*sh));
	memset(sh, 0, sizeof(*sh));
	ArgArray_Init(&sh->builtins);

	/*
	 * Parse the specification by keyword but skip the first word
	 */
	brk_string(&aa, spec, TRUE);

	for (arg = 1; arg < aa.argc; arg++) {
		/*
		 * Split keyword and value
		 */
		keyw = aa.argv[arg];
		if ((eq = strchr(keyw, '=')) == NULL) {
			Parse_Error(PARSE_FATAL, "missing '=' in shell "
			    "specification keyword '%s'", keyw);
			ArgArray_Done(&aa);
			ShellFree(sh);
			return (NULL);
		}
		*eq++ = '\0';

		if (strcmp(keyw, "path") == 0) {
			free(sh->path);
			sh->path = estrdup(eq);
		} else if (strcmp(keyw, "name") == 0) {
			free(sh->name);
			sh->name = estrdup(eq);
		} else if (strcmp(keyw, "quiet") == 0) {
			free(sh->echoOff);
			sh->echoOff = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "echo") == 0) {
			free(sh->echoOn);
			sh->echoOn = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "filter") == 0) {
			free(sh->noPrint);
			sh->noPrint = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "echoFlag") == 0) {
			free(sh->echo);
			sh->echo = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "errFlag") == 0) {
			free(sh->exit);
			sh->exit = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "hasErrCtl") == 0) {
			sh->hasErrCtl = (*eq == 'Y' || *eq == 'y' ||
			    *eq == 'T' || *eq == 't');
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "check") == 0) {
			free(sh->errCheck);
			sh->errCheck = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "ignore") == 0) {
			free(sh->ignErr);
			sh->ignErr = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "builtins") == 0) {
			ArgArray_Done(&sh->builtins);
			brk_string(&sh->builtins, eq, TRUE);
			qsort(sh->builtins.argv + 1, sh->builtins.argc - 1,
			    sizeof(char *), sort_builtins);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "meta") == 0) {
			free(sh->meta);
			sh->meta = estrdup(eq);
			*fullSpec = TRUE;
		} else if (strcmp(keyw, "unsetenv") == 0) {
			sh->unsetenv = (*eq == 'Y' || *eq == 'y' ||
			    *eq == 'T' || *eq == 't');
			*fullSpec = TRUE;
		} else {
			Parse_Error(PARSE_FATAL, "unknown keyword in shell "
			    "specification '%s'", keyw);
			ArgArray_Done(&aa);
			ShellFree(sh);
			return (NULL);
		}
	}
	ArgArray_Done(&aa);

	/*
	 * Some checks (could be more)
	 */
	if (*fullSpec) {
		if ((sh->echoOn != NULL) ^ (sh->echoOff != NULL)) {
			Parse_Error(PARSE_FATAL, "Shell must have either both "
			    "echoOff and echoOn or none of them");
			ShellFree(sh);
			return (NULL);
		}

		if (sh->echoOn != NULL && sh->echoOff != NULL)
			sh->hasEchoCtl = TRUE;
	}

	return (sh);
}

/**
 * Parse the builtin shell specifications and put them into the shell
 * list. Then select the default shell to be the current shell. This
 * is called from main() before any parsing (including MAKEFLAGS and
 * command line) is done.
 */
void
Shell_Init(void)
{
	int i;
	struct Shell *sh;
	Boolean fullSpec;

	for (i = 0; shells_init[i] != NULL; i++) {
		sh = ShellParseSpec(shells_init[i], &fullSpec);
		TAILQ_INSERT_TAIL(&shells, sh, link);
		if (strcmp(sh->name, DEFSHELLNAME) == 0)
			commandShell = sh;
	}
}

/**
 * Find a matching shell in 'shells' given its final component.
 *
 * Results:
 *	A pointer to a freshly allocated Shell structure with the contents
 *	from static description or NULL if no shell with the given name
 *	is found.
 */
static struct Shell *
ShellMatch(const char *name)
{
	struct Shell	*sh;

	TAILQ_FOREACH(sh, &shells, link)
		if (strcmp(sh->name, name) == 0)
			return (sh);

	return (NULL);
}

/**
 * Parse a shell specification and set up commandShell appropriately.
 *
 * Results:
 *	TRUE if the specification was correct. FALSE otherwise.
 *
 * Side Effects:
 *	commandShell points to a Shell structure.
 *	created from the shell spec).
 *
 * Notes:
 *	A shell specification consists of a .SHELL target, with dependency
 *	operator, followed by a series of blank-separated words. Double
 *	quotes can be used to use blanks in words. A backslash escapes
 *	anything (most notably a double-quote and a space) and
 *	provides the functionality it does in C. Each word consists of
 *	keyword and value separated by an equal sign. There should be no
 *	unnecessary spaces in the word. The keywords are as follows:
 *	    name	    Name of shell.
 *	    path	    Location of shell. Overrides "name" if given
 *	    quiet	    Command to turn off echoing.
 *	    echo	    Command to turn echoing on
 *	    filter	    Result of turning off echoing that shouldn't be
 *			    printed.
 *	    echoFlag	    Flag to turn echoing on at the start
 *	    errFlag	    Flag to turn error checking on at the start
 *	    hasErrCtl	    True if shell has error checking control
 *	    check	    Command to turn on error checking if hasErrCtl
 *			    is TRUE or template of command to echo a command
 *			    for which error checking is off if hasErrCtl is
 *			    FALSE.
 *	    ignore	    Command to turn off error checking if hasErrCtl
 *			    is TRUE or template of command to execute a
 *			    command so as to ignore any errors it returns if
 *			    hasErrCtl is FALSE.
 *	    builtins	    A space separated list of builtins. If one
 *			    of these builtins is detected when make wants
 *			    to execute a command line, the command line is
 *			    handed to the shell. Otherwise make may try to
 *			    execute the command directly. If this list is empty
 *			    it is assumed, that the command must always be
 *			    handed over to the shell.
 *	    meta	    The shell meta characters. If this is not specified
 *			    or empty, commands are alway passed to the shell.
 *			    Otherwise they are not passed when they contain
 *			    neither a meta character nor a builtin command.
 *	    unsetenv	    Unsetenv("ENV") before executing anything.
 */
Boolean
Shell_Parse(const char line[])
{
	Boolean		fullSpec;
	struct Shell	*sh;
	struct Shell	*match;

	/* parse the specification */
	if ((sh = ShellParseSpec(line, &fullSpec)) == NULL)
		return (FALSE);

	if (sh->path == NULL) {
		/*
		 * If no path was given, the user wants one of the pre-defined
		 * shells, yes? So we find the one s/he wants with the help of
		 * JobMatchShell and set things up the right way.
		 */
		if (sh->name == NULL) {
			Parse_Error(PARSE_FATAL,
			    "Neither path nor name specified");
			ShellFree(sh);
			return (FALSE);
		}
		if (fullSpec) {
			/*
			 * XXX May want to merge sh into match. But this
			 * require ShellParseSpec to return information
			 * which attributes actuall have been specified.
			 */
			Parse_Error(PARSE_FATAL, "No path specified");
			ShellFree(sh);
			return (FALSE);
		}
		if ((match = ShellMatch(sh->name)) == NULL) {
			Parse_Error(PARSE_FATAL, "%s: no matching shell",
			    sh->name);
			ShellFree(sh);
			return (FALSE);
		}
		ShellFree(sh);
		commandShell = match;

		return (TRUE);
	}

	/*
	 * The user provided a path. If s/he gave nothing else
	 * (fullSpec is FALSE), try and find a matching shell in the
	 * ones we know of. Else we just take the specification at its
	 * word and copy it to a new location. In either case, we need
	 * to record the path the user gave for the shell.
	 */
	if (sh->name == NULL) {
		/* get the base name as the name */
		if ((sh->name = strrchr(sh->path, '/')) == NULL) {
			sh->name = estrdup(sh->path);
		} else {
			sh->name = estrdup(sh->name + 1);
		}
	}

	if (!fullSpec) {
		if ((match = ShellMatch(sh->name)) == NULL) {
			Parse_Error(PARSE_FATAL,
			    "%s: no matching shell", sh->name);
			ShellFree(sh);
			return (FALSE);
		}

		/* set the patch on the matching shell */
		free(match->path);
		match->path = sh->path;
		sh->path = NULL;

		ShellFree(sh);
		commandShell = match;
		return (TRUE);
	}

	TAILQ_INSERT_HEAD(&shells, sh, link);

	/* set the new shell */
	commandShell = sh;
	return (TRUE);
}
