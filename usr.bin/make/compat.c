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
 *
 * @(#)compat.c	8.2 (Berkeley) 3/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * compat.c --
 *	The routines in this file implement the full-compatibility
 *	mode of PMake. Most of the special functionality of PMake
 *	is available in this mode. Things not supported:
 *	    - different shells.
 *	    - friendly variable substitution.
 *
 * Interface:
 *	Compat_Run	    Initialize things for this module and recreate
 *	    	  	    thems as need creatin'
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "compat.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "GNode.h"
#include "job.h"
#include "make.h"
#include "str.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/*
 * The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.
 */
static char 	    meta[256];

static GNode	    *curTarg = NULL;
static GNode	    *ENDNode;
static sig_atomic_t interrupted;

static const char *const sh_builtin[] = {
	"alias", "cd", "eval", "exec", "exit", "read", "set", "ulimit",
	"unalias", "umask", "unset", "wait", ":", NULL};

static void
CompatInit(void)
{
	const char	*cp;	/* Pointer to string of shell meta-characters */

	for (cp = "#=|^(){};&<>*?[]:$`\\\n"; *cp != '\0'; cp++) {
		meta[(unsigned char)*cp] = 1;
	}
	/*
	 * The null character serves as a sentinel in the string.
	 */
	meta[0] = 1;
}

/*
 * Interrupt handler - set flag and defer handling to the main code
 */
static void
CompatCatchSig(int signo)
{

	interrupted = signo;
}

/*-
 *-----------------------------------------------------------------------
 * CompatInterrupt --
 *	Interrupt the creation of the current target and remove it if
 *	it ain't precious.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The target is removed and the process exits. If .INTERRUPT exists,
 *	its commands are run first WITH INTERRUPTS IGNORED..
 *
 *-----------------------------------------------------------------------
 */
static void
CompatInterrupt(int signo)
{
	GNode		*gn;
	sigset_t	nmask, omask;
	LstNode		*ln;

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGINT);
	sigaddset(&nmask, SIGTERM);
	sigaddset(&nmask, SIGHUP);
	sigaddset(&nmask, SIGQUIT);
	sigprocmask(SIG_SETMASK, &nmask, &omask);

	/* prevent recursion in evaluation of .INTERRUPT */
	interrupted = 0;

	if (curTarg != NULL && !Targ_Precious(curTarg)) {
		char	  *p1;
		char 	  *file = Var_Value(TARGET, curTarg, &p1);

		if (!noExecute && eunlink(file) != -1) {
			printf("*** %s removed\n", file);
		}
		free(p1);
	}

	/*
	 * Run .INTERRUPT only if hit with interrupt signal
	 */
	if (signo == SIGINT) {
		gn = Targ_FindNode(".INTERRUPT", TARG_NOCREATE);
		if (gn != NULL) {
			LST_FOREACH(ln, &gn->commands) {
				if (Compat_RunCommand(Lst_Datum(ln), gn))
					break;
			}
		}
	}

	sigprocmask(SIG_SETMASK, &omask, NULL);

	if (signo == SIGQUIT)
		exit(signo);
	signal(signo, SIG_DFL);
	kill(getpid(), signo);
}

/*-
 *-----------------------------------------------------------------------
 * shellneed --
 *
 * Results:
 *	Returns 1 if a specified line must be executed by the shell,
 *	and 0 if it can be run via execve.
 *
 * Side Effects:
 *	Uses brk_string so destroys the contents of argv.
 *
 *-----------------------------------------------------------------------
 */
static int
shellneed(char *cmd)
{
	char **av;
	const char *const *p;
	int ac;

	av = brk_string(cmd, &ac, TRUE);
	for (p = sh_builtin; *p != 0; p++)
		if (strcmp(av[1], *p) == 0)
			return (1);
	return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Compat_RunCommand --
 *	Execute the next command for a target. If the command returns an
 *	error, the node's made field is set to ERROR and creation stops.
 *	The node from which the command came is also given.
 *
 * Results:
 *	0 if the command succeeded, 1 if an error occurred.
 *
 * Side Effects:
 *	The node's 'made' field may be set to ERROR.
 *
 *-----------------------------------------------------------------------
 */
int
Compat_RunCommand(char *cmd, GNode *gn)
{
	char	*cmdStart;	/* Start of expanded command */
	char	*cp;
	Boolean	silent;		/* Don't print command */
	Boolean	doit;		/* Execute even in -n */
	Boolean	errCheck;	/* Check errors */
	int	reason;		/* Reason for child's death */
	int	status;		/* Description of child's death */
	int	cpid;		/* Child actually found */
	ReturnStatus rstat;	/* Status of fork */
	LstNode	*cmdNode;	/* Node where current command is located */
	char	**av;		/* Argument vector for thing to exec */
	char	*cmd_save;	/* saved cmd */

	/*
	 * Avoid clobbered variable warnings by forcing the compiler
	 * to ``unregister'' variables
	 */
#if __GNUC__
	(void)&av;
	(void)&errCheck;
#endif
	silent = gn->type & OP_SILENT;
	errCheck = !(gn->type & OP_IGNORE);
	doit = FALSE;

	cmdNode = Lst_Member(&gn->commands, cmd);
	cmdStart = Buf_Peel(Var_Subst(NULL, cmd, gn, FALSE));

	/*
	 * brk_string will return an argv with a NULL in av[0], thus causing
	 * execvp to choke and die horribly. Besides, how can we execute a null
	 * command? In any case, we warn the user that the command expanded to
	 * nothing (is this the right thing to do?).
	 */
	if (*cmdStart == '\0') {
		free(cmdStart);
		Error("%s expands to empty string", cmd);
		return (0);
	} else {
		cmd = cmdStart;
	}
	Lst_Replace(cmdNode, cmdStart);

	if ((gn->type & OP_SAVE_CMDS) && (gn != ENDNode)) {
		Lst_AtEnd(&ENDNode->commands, cmdStart);
		return (0);
	} else if (strcmp(cmdStart, "...") == 0) {
		gn->type |= OP_SAVE_CMDS;
		return (0);
	}

	while (*cmd == '@' || *cmd == '-' || *cmd == '+') {
		switch (*cmd) {

		  case '@':
			silent = DEBUG(LOUD) ? FALSE : TRUE;
			break;

		  case '-':
			errCheck = FALSE;
			break;

		case '+':
			doit = TRUE;
			if (!meta[0])		/* we came here from jobs */
				CompatInit();
			break;
		}
		cmd++;
	}

	while (isspace((unsigned char)*cmd))
		cmd++;

	/*
	 * Search for meta characters in the command. If there are no meta
	 * characters, there's no need to execute a shell to execute the
	 * command.
	 */
	for (cp = cmd; !meta[(unsigned char)*cp]; cp++)
		continue;

	/*
	 * Print the command before echoing if we're not supposed to be quiet
	 * for this one. We also print the command if -n given, but not if '+'.
	 */
	if (!silent || (noExecute && !doit)) {
		printf("%s\n", cmd);
		fflush(stdout);
	}

	/*
	 * If we're not supposed to execute any commands, this is as far as
	 * we go...
	 */
	if (!doit && noExecute) {
		return (0);
	}

	if (*cp != '\0') {
		/*
		 * If *cp isn't the null character, we hit a "meta" character
		 * and need to pass the command off to the shell. We give the
		 * shell the -e flag as well as -c if it's supposed to exit
		 * when it hits an error.
		 */
		static char	*shargv[4];

		shargv[0] = shellPath;
		shargv[1] = (errCheck ? "-ec" : "-c");
		shargv[2] = cmd;
		shargv[3] = NULL;
		av = shargv;

	} else if (shellneed(cmd)) {
		/*
		 * This command must be passed by the shell for other reasons..
		 * or.. possibly not at all.
		 */
		static char	*shargv[4];

		shargv[0] = shellPath;
		shargv[1] = (errCheck ? "-ec" : "-c");
		shargv[2] = cmd;
		shargv[3] = NULL;
		av = shargv;

	} else {
		/*
		 * No meta-characters, so no need to exec a shell. Break the
		 * command into words to form an argument vector we can execute.
		 * brk_string sticks our name in av[0], so we have to
		 * skip over it...
		 */
		av = brk_string(cmd, NULL, TRUE);
		av += 1;
	}

	/*
	 * Fork and execute the single command. If the fork fails, we abort.
	 */
	cpid = vfork();
	if (cpid < 0) {
		Fatal("Could not fork");
	}
	if (cpid == 0) {
		execvp(av[0], av);
		write(STDERR_FILENO, av[0], strlen(av[0]));
		write(STDERR_FILENO, ":", 1);
		write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
		write(STDERR_FILENO, "\n", 1);
		_exit(1);
	}

	/*
	 * we need to print out the command associated with this Gnode in
	 * Targ_PrintCmd from Targ_PrintGraph when debugging at level g2,
	 * in main(), Fatal() and DieHorribly(), therefore do not free it
	 * when debugging.
	 */
	if (!DEBUG(GRAPH2)) {
		free(cmdStart);
		Lst_Replace(cmdNode, cmd_save);
	}

	/*
	 * The child is off and running. Now all we can do is wait...
	 */
	while (1) {
		while ((rstat = wait(&reason)) != cpid) {
			if (interrupted || (rstat == -1 && errno != EINTR)) {
				break;
			}
		}
		if (interrupted)
			CompatInterrupt(interrupted);

		if (rstat > -1) {
			if (WIFSTOPPED(reason)) {
				status = WSTOPSIG(reason);	/* stopped */
			} else if (WIFEXITED(reason)) {
				status = WEXITSTATUS(reason);	/* exited */
				if (status != 0) {
					printf("*** Error code %d", status);
				}
			} else {
				status = WTERMSIG(reason);	/* signaled */
				printf("*** Signal %d", status);
			}

			if (!WIFEXITED(reason) || status != 0) {
				if (errCheck) {
					gn->made = ERROR;
					if (keepgoing) {
						/*
						 * Abort the current target,
						 * but let others continue.
						 */
						printf(" (continuing)\n");
					}
				} else {
					/*
					 * Continue executing commands for this
					 * target. If we return 0, this will
					 * happen...
					 */
					printf(" (ignored)\n");
					status = 0;
				}
			}
			break;
		} else {
			Fatal("error in wait: %d", rstat);
			/*NOTREACHED*/
		}
	}

	return (status);
}

/*-
 *-----------------------------------------------------------------------
 * CompatMake --
 *	Make a target, given the parent, to abort if necessary.
 *
 * Results:
 *	0
 *
 * Side Effects:
 *	If an error is detected and not being ignored, the process exits.
 *
 *-----------------------------------------------------------------------
 */
static int
CompatMake(void *gnp, void *pgnp)
{
	GNode	*gn = gnp;
	GNode	*pgn = pgnp;
	LstNode	*ln;

	if (gn->type & OP_USE) {
		Make_HandleUse(gn, pgn);

	} else if (gn->made == UNMADE) {
		/*
		 * First mark ourselves to be made, then apply whatever
		 * transformations the suffix module thinks are necessary.
		 * Once that's done, we can descend and make all our children.
		 * If any of them has an error but the -k flag was given, our
		 * 'make' field will be set FALSE again. This is our signal to
		 * not attempt to do anything but abort our parent as well.
		 */
		gn->make = TRUE;
		gn->made = BEINGMADE;
		Suff_FindDeps(gn);
		Lst_ForEach(&gn->children, CompatMake, gn);
		if (!gn->make) {
			gn->made = ABORTED;
			pgn->make = FALSE;
			return (0);
		}

		if (Lst_Member(&gn->iParents, pgn) != NULL) {
			char *p1;
			Var_Set(IMPSRC, Var_Value(TARGET, gn, &p1), pgn);
			free(p1);
		}

		/*
		 * All the children were made ok. Now cmtime contains the
		 * modification time of the newest child, we need to find out
		 * if we exist and when we were modified last. The criteria for
		 * datedness are defined by the Make_OODate function.
		 */
		DEBUGF(MAKE, ("Examining %s...", gn->name));
		if (!Make_OODate(gn)) {
			gn->made = UPTODATE;
			DEBUGF(MAKE, ("up-to-date.\n"));
			return (0);
		} else {
			DEBUGF(MAKE, ("out-of-date.\n"));
		}

		/*
		 * If the user is just seeing if something is out-of-date,
		 * exit now to tell him/her "yes".
		 */
		if (queryFlag) {
			exit(1);
		}

		/*
		 * We need to be re-made. We also have to make sure we've got
		 * a $? variable. To be nice, we also define the $> variable
		 * using Make_DoAllVar().
		 */
		Make_DoAllVar(gn);

		/*
		 * Alter our type to tell if errors should be ignored or things
		 * should not be printed so Compat_RunCommand knows what to do.
		 */
		if (Targ_Ignore(gn)) {
			gn->type |= OP_IGNORE;
		}
		if (Targ_Silent(gn)) {
			gn->type |= OP_SILENT;
		}

		if (Job_CheckCommands(gn, Fatal)) {
			/*
			 * Our commands are ok, but we still have to worry
			 * about the -t flag...
			 */
			if (!touchFlag) {
				curTarg = gn;
				LST_FOREACH(ln, &gn->commands) {
					if (Compat_RunCommand(Lst_Datum(ln),
					    gn))
						break;
				}
				curTarg = NULL;
			} else {
				Job_Touch(gn, gn->type & OP_SILENT);
			}
		} else {
			gn->made = ERROR;
		}

		if (gn->made != ERROR) {
			/*
			 * If the node was made successfully, mark it so, update
			 * its modification time and timestamp all its parents.
			 * Note that for .ZEROTIME targets, the timestamping
			 * isn't done. This is to keep its state from affecting
			 * that of its parent.
			 */
			gn->made = MADE;
#ifndef RECHECK
			/*
			 * We can't re-stat the thing, but we can at least take
			 * care of rules where a target depends on a source that
			 * actually creates the target, but only if it has
			 * changed, e.g.
			 *
			 * parse.h : parse.o
			 *
			 * parse.o : parse.y
			 *  	yacc -d parse.y
			 *  	cc -c y.tab.c
			 *  	mv y.tab.o parse.o
			 *  	cmp -s y.tab.h parse.h || mv y.tab.h parse.h
			 *
			 * In this case, if the definitions produced by yacc
			 * haven't changed from before, parse.h won't have been
			 * updated and gn->mtime will reflect the current
			 * modification time for parse.h. This is something of a
			 * kludge, I admit, but it's a useful one..
			 *
			 * XXX: People like to use a rule like
			 *
			 * FRC:
			 *
			 * To force things that depend on FRC to be made, so we
			 * have to check for gn->children being empty as well...
			 */
			if (!Lst_IsEmpty(&gn->commands) ||
			    Lst_IsEmpty(&gn->children)) {
				gn->mtime = now;
			}
#else
			/*
			 * This is what Make does and it's actually a good
			 * thing, as it allows rules like
			 *
			 *	cmp -s y.tab.h parse.h || cp y.tab.h parse.h
			 *
			 * to function as intended. Unfortunately, thanks to
			 * the stateless nature of NFS (and the speed of this
			 * program), there are times when the modification time
			 * of a file created on a remote machine will not be
			 * modified before the stat() implied by the Dir_MTime
			 * occurs, thus leading us to believe that the file
			 * is unchanged, wreaking havoc with files that depend
			 * on this one.
			 *
			 * I have decided it is better to make too much than to
			 * make too little, so this stuff is commented out
			 * unless you're sure it's ok.
			 * -- ardeb 1/12/88
			 */
			if (noExecute || Dir_MTime(gn) == 0) {
				gn->mtime = now;
			}
			if (gn->cmtime > gn->mtime)
				gn->mtime = gn->cmtime;
			DEBUGF(MAKE, ("update time: %s\n",
			    Targ_FmtTime(gn->mtime)));
#endif
			if (!(gn->type & OP_EXEC)) {
				pgn->childMade = TRUE;
				Make_TimeStamp(pgn, gn);
			}

		} else if (keepgoing) {
			pgn->make = FALSE;

		} else {
			char *p1;

			printf("\n\nStop in %s.\n",
			    Var_Value(".CURDIR", gn, &p1));
			free(p1);
			exit(1);
		}
	} else if (gn->made == ERROR) {
		/*
		 * Already had an error when making this beastie. Tell the
		 * parent to abort.
		 */
		pgn->make = FALSE;
	} else {
		if (Lst_Member(&gn->iParents, pgn) != NULL) {
			char *p1;
			Var_Set(IMPSRC, Var_Value(TARGET, gn, &p1), pgn);
			free(p1);
		}
		switch(gn->made) {
		  case BEINGMADE:
			Error("Graph cycles through %s\n", gn->name);
			gn->made = ERROR;
			pgn->make = FALSE;
			break;
		  case MADE:
			if ((gn->type & OP_EXEC) == 0) {
			    pgn->childMade = TRUE;
			    Make_TimeStamp(pgn, gn);
			}
			break;
		  case UPTODATE:
			if ((gn->type & OP_EXEC) == 0) {
			    Make_TimeStamp(pgn, gn);
			}
			break;
		  default:
			break;
		}
	}

	return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Compat_Run --
 *	Start making again, given a list of target nodes.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Guess what?
 *
 *-----------------------------------------------------------------------
 */
void
Compat_Run(Lst *targs)
{
	GNode	*gn = NULL;	/* Current root target */
	int	errors;		/* Number of targets not remade due to errors */
	LstNode	*ln;

	CompatInit();
	Shell_Init();		/* Set up shell. */

	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		signal(SIGINT, CompatCatchSig);
	}
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
		signal(SIGTERM, CompatCatchSig);
	}
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
		signal(SIGHUP, CompatCatchSig);
	}
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN) {
		signal(SIGQUIT, CompatCatchSig);
	}

	ENDNode = Targ_FindNode(".END", TARG_CREATE);
	/*
	 * If the user has defined a .BEGIN target, execute the commands
	 * attached to it.
	*/
	if (!queryFlag) {
		gn = Targ_FindNode(".BEGIN", TARG_NOCREATE);
		if (gn != NULL) {
			LST_FOREACH(ln, &gn->commands) {
				if (Compat_RunCommand(Lst_Datum(ln), gn))
					break;
			}
			if (gn->made == ERROR) {
				printf("\n\nStop.\n");
				exit(1);
			}
		}
	}

	/*
	 * For each entry in the list of targets to create, call CompatMake on
	 * it to create the thing. CompatMake will leave the 'made' field of gn
	 * in one of several states:
	 *	UPTODATE  gn was already up-to-date
	 *	MADE	  gn was recreated successfully
	 *	ERROR	  An error occurred while gn was being created
	 *	ABORTED	  gn was not remade because one of its inferiors
	 *		  could not be made due to errors.
	 */
	errors = 0;
	while (!Lst_IsEmpty(targs)) {
		gn = Lst_DeQueue(targs);
		CompatMake(gn, gn);

		if (gn->made == UPTODATE) {
			printf("`%s' is up to date.\n", gn->name);
		} else if (gn->made == ABORTED) {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
			errors += 1;
		}
	}

	/*
	 * If the user has defined a .END target, run its commands.
	 */
	if (errors == 0) {
		LST_FOREACH(ln, &ENDNode->commands) {
			if (Compat_RunCommand(Lst_Datum(ln), gn))
				break;
		}
	}
}
