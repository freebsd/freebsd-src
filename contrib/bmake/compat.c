/*	$NetBSD: compat.c,v 1.183 2020/11/15 22:31:03 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
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

/*-
 * compat.c --
 *	The routines in this file implement the full-compatibility
 *	mode of PMake. Most of the special functionality of PMake
 *	is available in this mode. Things not supported:
 *	    - different shells.
 *	    - friendly variable substitution.
 *
 * Interface:
 *	Compat_Run	Initialize things for this module and recreate
 *			thems as need creatin'
 */

#ifdef HAVE_CONFIG_H
# include   "config.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "wait.h"

#include <errno.h>
#include <signal.h>

#include "make.h"
#include "dir.h"
#include "job.h"
#include "metachar.h"
#include "pathnames.h"

/*	"@(#)compat.c	8.2 (Berkeley) 3/19/94"	*/
MAKE_RCSID("$NetBSD: compat.c,v 1.183 2020/11/15 22:31:03 rillig Exp $");

static GNode *curTarg = NULL;
static pid_t compatChild;
static int compatSigno;

/*
 * CompatDeleteTarget -- delete the file of a failed, interrupted, or
 * otherwise duffed target if not inhibited by .PRECIOUS.
 */
static void
CompatDeleteTarget(GNode *gn)
{
    if (gn != NULL && !Targ_Precious(gn)) {
	const char *file = GNode_VarTarget(gn);

	if (!opts.noExecute && eunlink(file) != -1) {
	    Error("*** %s removed", file);
	}
    }
}

/* Interrupt the creation of the current target and remove it if it ain't
 * precious. Then exit.
 *
 * If .INTERRUPT exists, its commands are run first WITH INTERRUPTS IGNORED.
 *
 * XXX: is .PRECIOUS supposed to inhibit .INTERRUPT? I doubt it, but I've
 * left the logic alone for now. - dholland 20160826
 */
static void
CompatInterrupt(int signo)
{
    CompatDeleteTarget(curTarg);

    if (curTarg != NULL && !Targ_Precious(curTarg)) {
	/*
	 * Run .INTERRUPT only if hit with interrupt signal
	 */
	if (signo == SIGINT) {
	    GNode *gn = Targ_FindNode(".INTERRUPT");
	    if (gn != NULL) {
		Compat_Make(gn, gn);
	    }
	}
    }

    if (signo == SIGQUIT)
	_exit(signo);

    /*
     * If there is a child running, pass the signal on.
     * We will exist after it has exited.
     */
    compatSigno = signo;
    if (compatChild > 0) {
	KILLPG(compatChild, signo);
    } else {
	bmake_signal(signo, SIG_DFL);
	kill(myPid, signo);
    }
}

/* Execute the next command for a target. If the command returns an error,
 * the node's made field is set to ERROR and creation stops.
 *
 * Input:
 *	cmdp		Command to execute
 *	gnp		Node from which the command came
 *
 * Results:
 *	0 if the command succeeded, 1 if an error occurred.
 */
int
Compat_RunCommand(const char *cmdp, GNode *gn)
{
    char *cmdStart;		/* Start of expanded command */
    char *bp;
    Boolean silent;		/* Don't print command */
    Boolean doIt;		/* Execute even if -n */
    volatile Boolean errCheck;	/* Check errors */
    WAIT_T reason;		/* Reason for child's death */
    int status;			/* Description of child's death */
    pid_t cpid;			/* Child actually found */
    pid_t retstat;		/* Result of wait */
    StringListNode *cmdNode;	/* Node where current command is located */
    const char **volatile av;	/* Argument vector for thing to exec */
    char **volatile mav;	/* Copy of the argument vector for freeing */
    Boolean useShell;		/* TRUE if command should be executed
				 * using a shell */
    const char *volatile cmd = cmdp;

    silent = (gn->type & OP_SILENT) != 0;
    errCheck = !(gn->type & OP_IGNORE);
    doIt = FALSE;

    /* Luckily the commands don't end up in a string pool, otherwise
     * this comparison could match too early, in a dependency using "..."
     * for delayed commands, run in parallel mode, using the same shell
     * command line more than once; see JobPrintCommand.
     * TODO: write a unit-test to protect against this potential bug. */
    cmdNode = Lst_FindDatum(gn->commands, cmd);
    (void)Var_Subst(cmd, gn, VARE_WANTRES, &cmdStart);
    /* TODO: handle errors */

    if (cmdStart[0] == '\0') {
	free(cmdStart);
	return 0;
    }
    cmd = cmdStart;
    LstNode_Set(cmdNode, cmdStart);

    if (gn->type & OP_SAVE_CMDS) {
	GNode *endNode = Targ_GetEndNode();
	if (gn != endNode) {
	    Lst_Append(endNode->commands, cmdStart);
	    return 0;
	}
    }
    if (strcmp(cmdStart, "...") == 0) {
	gn->type |= OP_SAVE_CMDS;
	return 0;
    }

    for (;;) {
	if (*cmd == '@')
	    silent = !DEBUG(LOUD);
	else if (*cmd == '-')
	    errCheck = FALSE;
	else if (*cmd == '+') {
	    doIt = TRUE;
	    if (!shellName)	/* we came here from jobs */
		Shell_Init();
	} else
	    break;
	cmd++;
    }

    while (ch_isspace(*cmd))
	cmd++;

    /*
     * If we did not end up with a command, just skip it.
     */
    if (cmd[0] == '\0')
	return 0;

#if !defined(MAKE_NATIVE)
    /*
     * In a non-native build, the host environment might be weird enough
     * that it's necessary to go through a shell to get the correct
     * behaviour.  Or perhaps the shell has been replaced with something
     * that does extra logging, and that should not be bypassed.
     */
    useShell = TRUE;
#else
    /*
     * Search for meta characters in the command. If there are no meta
     * characters, there's no need to execute a shell to execute the
     * command.
     *
     * Additionally variable assignments and empty commands
     * go to the shell. Therefore treat '=' and ':' like shell
     * meta characters as documented in make(1).
     */

    useShell = needshell(cmd);
#endif

    /*
     * Print the command before echoing if we're not supposed to be quiet for
     * this one. We also print the command if -n given.
     */
    if (!silent || !GNode_ShouldExecute(gn)) {
	printf("%s\n", cmd);
	fflush(stdout);
    }

    /*
     * If we're not supposed to execute any commands, this is as far as
     * we go...
     */
    if (!doIt && !GNode_ShouldExecute(gn))
	return 0;

    DEBUG1(JOB, "Execute: '%s'\n", cmd);

    if (useShell) {
	/*
	 * We need to pass the command off to the shell, typically
	 * because the command contains a "meta" character.
	 */
	static const char *shargv[5];

	/* The following work for any of the builtin shell specs. */
	int shargc = 0;
	shargv[shargc++] = shellPath;
	if (errCheck && shellErrFlag)
	    shargv[shargc++] = shellErrFlag;
	shargv[shargc++] = DEBUG(SHELL) ? "-xc" : "-c";
	shargv[shargc++] = cmd;
	shargv[shargc] = NULL;
	av = shargv;
	bp = NULL;
	mav = NULL;
    } else {
	/*
	 * No meta-characters, so no need to exec a shell. Break the command
	 * into words to form an argument vector we can execute.
	 */
	Words words = Str_Words(cmd, FALSE);
	mav = words.words;
	bp = words.freeIt;
	av = (void *)mav;
    }

#ifdef USE_META
    if (useMeta) {
	meta_compat_start();
    }
#endif

    /*
     * Fork and execute the single command. If the fork fails, we abort.
     */
    compatChild = cpid = vFork();
    if (cpid < 0) {
	Fatal("Could not fork");
    }
    if (cpid == 0) {
	Var_ExportVars();
#ifdef USE_META
	if (useMeta) {
	    meta_compat_child();
	}
#endif
	(void)execvp(av[0], (char *const *)UNCONST(av));
	execDie("exec", av[0]);
    }

    free(mav);
    free(bp);

    /* XXX: Memory management looks suspicious here. */
    /* XXX: Setting a list item to NULL is unexpected. */
    LstNode_SetNull(cmdNode);

#ifdef USE_META
    if (useMeta) {
	meta_compat_parent(cpid);
    }
#endif

    /*
     * The child is off and running. Now all we can do is wait...
     */
    while ((retstat = wait(&reason)) != cpid) {
	if (retstat > 0)
	    JobReapChild(retstat, reason, FALSE); /* not ours? */
	if (retstat == -1 && errno != EINTR) {
	    break;
	}
    }

    if (retstat < 0)
	Fatal("error in wait: %d: %s", retstat, strerror(errno));

    if (WIFSTOPPED(reason)) {
	status = WSTOPSIG(reason);		/* stopped */
    } else if (WIFEXITED(reason)) {
	status = WEXITSTATUS(reason);		/* exited */
#if defined(USE_META) && defined(USE_FILEMON_ONCE)
	if (useMeta) {
	    meta_cmd_finish(NULL);
	}
#endif
	if (status != 0) {
	    if (DEBUG(ERROR)) {
		const char *p = cmd;
		debug_printf("\n*** Failed target:  %s\n*** Failed command: ",
			     gn->name);

		/* Replace runs of whitespace with a single space, to reduce
		 * the amount of whitespace for multi-line command lines. */
		while (*p != '\0') {
		    if (ch_isspace(*p)) {
			debug_printf(" ");
			cpp_skip_whitespace(&p);
		    } else {
			debug_printf("%c", *p);
			p++;
		    }
		}
		debug_printf("\n");
	    }
	    printf("*** Error code %d", status);
	}
    } else {
	status = WTERMSIG(reason);		/* signaled */
	printf("*** Signal %d", status);
    }


    if (!WIFEXITED(reason) || status != 0) {
	if (errCheck) {
#ifdef USE_META
	    if (useMeta) {
		meta_job_error(NULL, gn, 0, status);
	    }
#endif
	    gn->made = ERROR;
	    if (opts.keepgoing) {
		/* Abort the current target, but let others continue. */
		printf(" (continuing)\n");
	    } else {
		printf("\n");
	    }
	    if (deleteOnError)
		CompatDeleteTarget(gn);
	} else {
	    /*
	     * Continue executing commands for this target.
	     * If we return 0, this will happen...
	     */
	    printf(" (ignored)\n");
	    status = 0;
	}
    }

    free(cmdStart);
    compatChild = 0;
    if (compatSigno) {
	bmake_signal(compatSigno, SIG_DFL);
	kill(myPid, compatSigno);
    }

    return status;
}

static void
RunCommands(GNode *gn)
{
    StringListNode *ln;
    for (ln = gn->commands->first; ln != NULL; ln = ln->next) {
	const char *cmd = ln->datum;
	if (Compat_RunCommand(cmd, gn) != 0)
	    break;
    }
}

static void
MakeNodes(GNodeList *gnodes, GNode *pgn)
{
    GNodeListNode *ln;
    for (ln = gnodes->first; ln != NULL; ln = ln->next) {
	GNode *cohort = ln->datum;
	Compat_Make(cohort, pgn);
    }
}

/* Make a target.
 *
 * If an error is detected and not being ignored, the process exits.
 *
 * Input:
 *	gn		The node to make
 *	pgn		Parent to abort if necessary
 */
void
Compat_Make(GNode *gn, GNode *pgn)
{
    if (shellName == NULL)	/* we came here from jobs */
	Shell_Init();

    if (gn->made == UNMADE && (gn == pgn || !(pgn->type & OP_MADE))) {
	/*
	 * First mark ourselves to be made, then apply whatever transformations
	 * the suffix module thinks are necessary. Once that's done, we can
	 * descend and make all our children. If any of them has an error
	 * but the -k flag was given, our 'make' field will be set to FALSE
	 * again. This is our signal to not attempt to do anything but abort
	 * our parent as well.
	 */
	gn->flags |= REMAKE;
	gn->made = BEINGMADE;
	if (!(gn->type & OP_MADE))
	    Suff_FindDeps(gn);
	MakeNodes(gn->children, gn);
	if (!(gn->flags & REMAKE)) {
	    gn->made = ABORTED;
	    pgn->flags &= ~(unsigned)REMAKE;
	    goto cohorts;
	}

	if (Lst_FindDatum(gn->implicitParents, pgn) != NULL)
	    Var_Set(IMPSRC, GNode_VarTarget(gn), pgn);

	/*
	 * All the children were made ok. Now youngestChild->mtime contains the
	 * modification time of the newest child, we need to find out if we
	 * exist and when we were modified last. The criteria for datedness
	 * are defined by GNode_IsOODate.
	 */
	DEBUG1(MAKE, "Examining %s...", gn->name);
	if (!GNode_IsOODate(gn)) {
	    gn->made = UPTODATE;
	    DEBUG0(MAKE, "up-to-date.\n");
	    goto cohorts;
	} else
	    DEBUG0(MAKE, "out-of-date.\n");

	/*
	 * If the user is just seeing if something is out-of-date, exit now
	 * to tell him/her "yes".
	 */
	if (opts.queryFlag)
	    exit(1);

	/*
	 * We need to be re-made. We also have to make sure we've got a $?
	 * variable. To be nice, we also define the $> variable using
	 * Make_DoAllVar().
	 */
	Make_DoAllVar(gn);

	/*
	 * Alter our type to tell if errors should be ignored or things
	 * should not be printed so CompatRunCommand knows what to do.
	 */
	if (Targ_Ignore(gn))
	    gn->type |= OP_IGNORE;
	if (Targ_Silent(gn))
	    gn->type |= OP_SILENT;

	if (Job_CheckCommands(gn, Fatal)) {
	    /*
	     * Our commands are ok, but we still have to worry about the -t
	     * flag...
	     */
	    if (!opts.touchFlag || (gn->type & OP_MAKE)) {
		curTarg = gn;
#ifdef USE_META
		if (useMeta && GNode_ShouldExecute(gn)) {
		    meta_job_start(NULL, gn);
		}
#endif
		RunCommands(gn);
		curTarg = NULL;
	    } else {
		Job_Touch(gn, (gn->type & OP_SILENT) != 0);
	    }
	} else {
	    gn->made = ERROR;
	}
#ifdef USE_META
	if (useMeta && GNode_ShouldExecute(gn)) {
	    if (meta_job_finish(NULL) != 0)
		gn->made = ERROR;
	}
#endif

	if (gn->made != ERROR) {
	    /*
	     * If the node was made successfully, mark it so, update
	     * its modification time and timestamp all its parents.
	     * This is to keep its state from affecting that of its parent.
	     */
	    gn->made = MADE;
	    if (Make_Recheck(gn) == 0)
		pgn->flags |= FORCE;
	    if (!(gn->type & OP_EXEC)) {
		pgn->flags |= CHILDMADE;
		GNode_UpdateYoungestChild(pgn, gn);
	    }
	} else if (opts.keepgoing) {
	    pgn->flags &= ~(unsigned)REMAKE;
	} else {
	    PrintOnError(gn, "\nStop.");
	    exit(1);
	}
    } else if (gn->made == ERROR) {
	/* Already had an error when making this. Tell the parent to abort. */
	pgn->flags &= ~(unsigned)REMAKE;
    } else {
	if (Lst_FindDatum(gn->implicitParents, pgn) != NULL) {
	    const char *target = GNode_VarTarget(gn);
	    Var_Set(IMPSRC, target != NULL ? target : "", pgn);
	}
	switch(gn->made) {
	    case BEINGMADE:
		Error("Graph cycles through %s", gn->name);
		gn->made = ERROR;
		pgn->flags &= ~(unsigned)REMAKE;
		break;
	    case MADE:
		if (!(gn->type & OP_EXEC)) {
		    pgn->flags |= CHILDMADE;
		    GNode_UpdateYoungestChild(pgn, gn);
		}
		break;
	    case UPTODATE:
		if (!(gn->type & OP_EXEC))
		    GNode_UpdateYoungestChild(pgn, gn);
		break;
	    default:
		break;
	}
    }

cohorts:
    MakeNodes(gn->cohorts, pgn);
}

/* Initialize this module and start making.
 *
 * Input:
 *	targs		The target nodes to re-create
 */
void
Compat_Run(GNodeList *targs)
{
    GNode *gn = NULL;		/* Current root target */
    int errors;			/* Number of targets not remade due to errors */

    if (!shellName)
	Shell_Init();

    if (bmake_signal(SIGINT, SIG_IGN) != SIG_IGN)
	bmake_signal(SIGINT, CompatInterrupt);
    if (bmake_signal(SIGTERM, SIG_IGN) != SIG_IGN)
	bmake_signal(SIGTERM, CompatInterrupt);
    if (bmake_signal(SIGHUP, SIG_IGN) != SIG_IGN)
	bmake_signal(SIGHUP, CompatInterrupt);
    if (bmake_signal(SIGQUIT, SIG_IGN) != SIG_IGN)
	bmake_signal(SIGQUIT, CompatInterrupt);

    /* Create the .END node now, to keep the (debug) output of the
     * counter.mk test the same as before 2020-09-23.  This implementation
     * detail probably doesn't matter though. */
    (void)Targ_GetEndNode();
    /*
     * If the user has defined a .BEGIN target, execute the commands attached
     * to it.
     */
    if (!opts.queryFlag) {
	gn = Targ_FindNode(".BEGIN");
	if (gn != NULL) {
	    Compat_Make(gn, gn);
	    if (gn->made == ERROR) {
		PrintOnError(gn, "\nStop.");
		exit(1);
	    }
	}
    }

    /*
     * Expand .USE nodes right now, because they can modify the structure
     * of the tree.
     */
    Make_ExpandUse(targs);

    /*
     * For each entry in the list of targets to create, call Compat_Make on
     * it to create the thing. Compat_Make will leave the 'made' field of gn
     * in one of several states:
     *	    UPTODATE	gn was already up-to-date
     *	    MADE	gn was recreated successfully
     *	    ERROR	An error occurred while gn was being created
     *	    ABORTED	gn was not remade because one of its inferiors
     *			could not be made due to errors.
     */
    errors = 0;
    while (!Lst_IsEmpty(targs)) {
	gn = Lst_Dequeue(targs);
	Compat_Make(gn, gn);

	if (gn->made == UPTODATE) {
	    printf("`%s' is up to date.\n", gn->name);
	} else if (gn->made == ABORTED) {
	    printf("`%s' not remade because of errors.\n", gn->name);
	    errors++;
	}
    }

    /*
     * If the user has defined a .END target, run its commands.
     */
    if (errors == 0) {
	GNode *endNode = Targ_GetEndNode();
	Compat_Make(endNode, endNode);
	/* XXX: Did you mean endNode->made instead of gn->made? */
	if (gn->made == ERROR) {
	    PrintOnError(gn, "\nStop.");
	    exit(1);
	}
    }
}
