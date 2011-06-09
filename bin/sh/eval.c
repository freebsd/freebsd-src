/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)eval.c	8.9 (Berkeley) 6/8/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h> /* For WIFSIGNALED(status) */
#include <errno.h>

/*
 * Evaluate a command.
 */

#include "shell.h"
#include "nodes.h"
#include "syntax.h"
#include "expand.h"
#include "parser.h"
#include "jobs.h"
#include "eval.h"
#include "builtins.h"
#include "options.h"
#include "exec.h"
#include "redir.h"
#include "input.h"
#include "output.h"
#include "trap.h"
#include "var.h"
#include "memalloc.h"
#include "error.h"
#include "show.h"
#include "mystring.h"
#ifndef NO_HISTORY
#include "myhistedit.h"
#endif


int evalskip;			/* set if we are skipping commands */
static int skipcount;		/* number of levels to skip */
MKINIT int loopnest;		/* current loop nesting level */
int funcnest;			/* depth of function calls */
static int builtin_flags;	/* evalcommand flags for builtins */


char *commandname;
struct strlist *cmdenviron;
int exitstatus;			/* exit status of last command */
int oexitstatus;		/* saved exit status */


static void evalloop(union node *, int);
static void evalfor(union node *, int);
static void evalcase(union node *, int);
static void evalsubshell(union node *, int);
static void evalredir(union node *, int);
static void expredir(union node *);
static void evalpipe(union node *);
static int is_valid_fast_cmdsubst(union node *n);
static void evalcommand(union node *, int, struct backcmd *);
static void prehash(union node *);


/*
 * Called to reset things after an exception.
 */

#ifdef mkinit
INCLUDE "eval.h"

RESET {
	evalskip = 0;
	loopnest = 0;
	funcnest = 0;
}
#endif



/*
 * The eval command.
 */

int
evalcmd(int argc, char **argv)
{
        char *p;
        char *concat;
        char **ap;

        if (argc > 1) {
                p = argv[1];
                if (argc > 2) {
                        STARTSTACKSTR(concat);
                        ap = argv + 2;
                        for (;;) {
                                STPUTS(p, concat);
                                if ((p = *ap++) == NULL)
                                        break;
                                STPUTC(' ', concat);
                        }
                        STPUTC('\0', concat);
                        p = grabstackstr(concat);
                }
                evalstring(p, builtin_flags & EV_TESTED);
        } else
                exitstatus = 0;
        return exitstatus;
}


/*
 * Execute a command or commands contained in a string.
 */

void
evalstring(char *s, int flags)
{
	union node *n;
	struct stackmark smark;
	int flags_exit;
	int any;

	flags_exit = flags & EV_EXIT;
	flags &= ~EV_EXIT;
	any = 0;
	setstackmark(&smark);
	setinputstring(s, 1);
	while ((n = parsecmd(0)) != NEOF) {
		if (n != NULL && !nflag) {
			if (flags_exit && preadateof())
				evaltree(n, flags | EV_EXIT);
			else
				evaltree(n, flags);
			any = 1;
		}
		popstackmark(&smark);
	}
	popfile();
	popstackmark(&smark);
	if (!any)
		exitstatus = 0;
	if (flags_exit)
		exraise(EXEXIT);
}


/*
 * Evaluate a parse tree.  The value is left in the global variable
 * exitstatus.
 */

void
evaltree(union node *n, int flags)
{
	int do_etest;
	union node *next;

	do_etest = 0;
	if (n == NULL) {
		TRACE(("evaltree(NULL) called\n"));
		exitstatus = 0;
		goto out;
	}
	do {
		next = NULL;
#ifndef NO_HISTORY
		displayhist = 1;	/* show history substitutions done with fc */
#endif
		TRACE(("evaltree(%p: %d) called\n", (void *)n, n->type));
		switch (n->type) {
		case NSEMI:
			evaltree(n->nbinary.ch1, flags & ~EV_EXIT);
			if (evalskip)
				goto out;
			next = n->nbinary.ch2;
			break;
		case NAND:
			evaltree(n->nbinary.ch1, EV_TESTED);
			if (evalskip || exitstatus != 0) {
				goto out;
			}
			next = n->nbinary.ch2;
			break;
		case NOR:
			evaltree(n->nbinary.ch1, EV_TESTED);
			if (evalskip || exitstatus == 0)
				goto out;
			next = n->nbinary.ch2;
			break;
		case NREDIR:
			evalredir(n, flags);
			break;
		case NSUBSHELL:
			evalsubshell(n, flags);
			do_etest = !(flags & EV_TESTED);
			break;
		case NBACKGND:
			evalsubshell(n, flags);
			break;
		case NIF: {
			evaltree(n->nif.test, EV_TESTED);
			if (evalskip)
				goto out;
			if (exitstatus == 0)
				next = n->nif.ifpart;
			else if (n->nif.elsepart)
				next = n->nif.elsepart;
			else
				exitstatus = 0;
			break;
		}
		case NWHILE:
		case NUNTIL:
			evalloop(n, flags & ~EV_EXIT);
			break;
		case NFOR:
			evalfor(n, flags & ~EV_EXIT);
			break;
		case NCASE:
			evalcase(n, flags);
			break;
		case NDEFUN:
			defun(n->narg.text, n->narg.next);
			exitstatus = 0;
			break;
		case NNOT:
			evaltree(n->nnot.com, EV_TESTED);
			exitstatus = !exitstatus;
			break;

		case NPIPE:
			evalpipe(n);
			do_etest = !(flags & EV_TESTED);
			break;
		case NCMD:
			evalcommand(n, flags, (struct backcmd *)NULL);
			do_etest = !(flags & EV_TESTED);
			break;
		default:
			out1fmt("Node type = %d\n", n->type);
			flushout(&output);
			break;
		}
		n = next;
	} while (n != NULL);
out:
	if (pendingsigs)
		dotrap();
	if (eflag && exitstatus != 0 && do_etest)
		exitshell(exitstatus);
	if (flags & EV_EXIT)
		exraise(EXEXIT);
}


static void
evalloop(union node *n, int flags)
{
	int status;

	loopnest++;
	status = 0;
	for (;;) {
		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip) {
skipping:	  if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			if (evalskip == SKIPFUNC || evalskip == SKIPFILE)
				status = exitstatus;
			break;
		}
		if (n->type == NWHILE) {
			if (exitstatus != 0)
				break;
		} else {
			if (exitstatus == 0)
				break;
		}
		evaltree(n->nbinary.ch2, flags);
		status = exitstatus;
		if (evalskip)
			goto skipping;
	}
	loopnest--;
	exitstatus = status;
}



static void
evalfor(union node *n, int flags)
{
	struct arglist arglist;
	union node *argp;
	struct strlist *sp;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	for (argp = n->nfor.args ; argp ; argp = argp->narg.next) {
		oexitstatus = exitstatus;
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);
		if (evalskip)
			goto out;
	}
	*arglist.lastp = NULL;

	exitstatus = 0;
	loopnest++;
	for (sp = arglist.list ; sp ; sp = sp->next) {
		setvar(n->nfor.var, sp->text, 0);
		evaltree(n->nfor.body, flags);
		if (evalskip) {
			if (evalskip == SKIPCONT && --skipcount <= 0) {
				evalskip = 0;
				continue;
			}
			if (evalskip == SKIPBREAK && --skipcount <= 0)
				evalskip = 0;
			break;
		}
	}
	loopnest--;
out:
	popstackmark(&smark);
}



static void
evalcase(union node *n, int flags)
{
	union node *cp;
	union node *patp;
	struct arglist arglist;
	struct stackmark smark;

	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	oexitstatus = exitstatus;
	exitstatus = 0;
	expandarg(n->ncase.expr, &arglist, EXP_TILDE);
	for (cp = n->ncase.cases ; cp && evalskip == 0 ; cp = cp->nclist.next) {
		for (patp = cp->nclist.pattern ; patp ; patp = patp->narg.next) {
			if (casematch(patp, arglist.list->text)) {
				if (evalskip == 0) {
					evaltree(cp->nclist.body, flags);
				}
				goto out;
			}
		}
	}
out:
	popstackmark(&smark);
}



/*
 * Kick off a subshell to evaluate a tree.
 */

static void
evalsubshell(union node *n, int flags)
{
	struct job *jp;
	int backgnd = (n->type == NBACKGND);

	oexitstatus = exitstatus;
	expredir(n->nredir.redirect);
	if ((!backgnd && flags & EV_EXIT && !have_traps()) ||
			forkshell(jp = makejob(n, 1), n, backgnd) == 0) {
		if (backgnd)
			flags &=~ EV_TESTED;
		redirect(n->nredir.redirect, 0);
		evaltree(n->nredir.n, flags | EV_EXIT);	/* never returns */
	} else if (! backgnd) {
		INTOFF;
		exitstatus = waitforjob(jp, (int *)NULL);
		INTON;
	} else
		exitstatus = 0;
}


/*
 * Evaluate a redirected compound command.
 */

static void
evalredir(union node *n, int flags)
{
	struct jmploc jmploc;
	struct jmploc *savehandler;
	volatile int in_redirect = 1;

	oexitstatus = exitstatus;
	expredir(n->nredir.redirect);
	savehandler = handler;
	if (setjmp(jmploc.loc)) {
		int e;

		handler = savehandler;
		e = exception;
		popredir();
		if (e == EXERROR || e == EXEXEC) {
			if (in_redirect) {
				exitstatus = 2;
				return;
			}
		}
		longjmp(handler->loc, 1);
	} else {
		INTOFF;
		handler = &jmploc;
		redirect(n->nredir.redirect, REDIR_PUSH);
		in_redirect = 0;
		INTON;
		evaltree(n->nredir.n, flags);
	}
	INTOFF;
	handler = savehandler;
	popredir();
	INTON;
}


/*
 * Compute the names of the files in a redirection list.
 */

static void
expredir(union node *n)
{
	union node *redir;

	for (redir = n ; redir ; redir = redir->nfile.next) {
		struct arglist fn;
		fn.lastp = &fn.list;
		switch (redir->type) {
		case NFROM:
		case NTO:
		case NFROMTO:
		case NAPPEND:
		case NCLOBBER:
			expandarg(redir->nfile.fname, &fn, EXP_TILDE | EXP_REDIR);
			redir->nfile.expfname = fn.list->text;
			break;
		case NFROMFD:
		case NTOFD:
			if (redir->ndup.vname) {
				expandarg(redir->ndup.vname, &fn, EXP_TILDE | EXP_REDIR);
				fixredir(redir, fn.list->text, 1);
			}
			break;
		}
	}
}



/*
 * Evaluate a pipeline.  All the processes in the pipeline are children
 * of the process creating the pipeline.  (This differs from some versions
 * of the shell, which make the last process in a pipeline the parent
 * of all the rest.)
 */

static void
evalpipe(union node *n)
{
	struct job *jp;
	struct nodelist *lp;
	int pipelen;
	int prevfd;
	int pip[2];

	TRACE(("evalpipe(%p) called\n", (void *)n));
	pipelen = 0;
	for (lp = n->npipe.cmdlist ; lp ; lp = lp->next)
		pipelen++;
	INTOFF;
	jp = makejob(n, pipelen);
	prevfd = -1;
	for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
		prehash(lp->n);
		pip[1] = -1;
		if (lp->next) {
			if (pipe(pip) < 0) {
				close(prevfd);
				error("Pipe call failed: %s", strerror(errno));
			}
		}
		if (forkshell(jp, lp->n, n->npipe.backgnd) == 0) {
			INTON;
			if (prevfd > 0) {
				dup2(prevfd, 0);
				close(prevfd);
			}
			if (pip[1] >= 0) {
				if (!(prevfd >= 0 && pip[0] == 0))
					close(pip[0]);
				if (pip[1] != 1) {
					dup2(pip[1], 1);
					close(pip[1]);
				}
			}
			evaltree(lp->n, EV_EXIT);
		}
		if (prevfd >= 0)
			close(prevfd);
		prevfd = pip[0];
		if (pip[1] != -1)
			close(pip[1]);
	}
	INTON;
	if (n->npipe.backgnd == 0) {
		INTOFF;
		exitstatus = waitforjob(jp, (int *)NULL);
		TRACE(("evalpipe:  job done exit status %d\n", exitstatus));
		INTON;
	} else
		exitstatus = 0;
}



static int
is_valid_fast_cmdsubst(union node *n)
{
	union node *argp;

	if (n->type != NCMD)
		return 0;
	for (argp = n->ncmd.args ; argp ; argp = argp->narg.next)
		if (expandhassideeffects(argp->narg.text))
			return 0;
	return 1;
}

/*
 * Execute a command inside back quotes.  If it's a builtin command, we
 * want to save its output in a block obtained from malloc.  Otherwise
 * we fork off a subprocess and get the output of the command via a pipe.
 * Should be called with interrupts off.
 */

void
evalbackcmd(union node *n, struct backcmd *result)
{
	int pip[2];
	struct job *jp;
	struct stackmark smark;		/* unnecessary */
	struct jmploc jmploc;
	struct jmploc *savehandler;

	setstackmark(&smark);
	result->fd = -1;
	result->buf = NULL;
	result->nleft = 0;
	result->jp = NULL;
	if (n == NULL) {
		exitstatus = 0;
		goto out;
	}
	if (is_valid_fast_cmdsubst(n)) {
		exitstatus = oexitstatus;
		savehandler = handler;
		if (setjmp(jmploc.loc)) {
			if (exception == EXERROR || exception == EXEXEC)
				exitstatus = 2;
			else if (exception != 0) {
				handler = savehandler;
				longjmp(handler->loc, 1);
			}
		} else {
			handler = &jmploc;
			evalcommand(n, EV_BACKCMD, result);
		}
		handler = savehandler;
	} else {
		exitstatus = 0;
		if (pipe(pip) < 0)
			error("Pipe call failed: %s", strerror(errno));
		jp = makejob(n, 1);
		if (forkshell(jp, n, FORK_NOJOB) == 0) {
			FORCEINTON;
			close(pip[0]);
			if (pip[1] != 1) {
				dup2(pip[1], 1);
				close(pip[1]);
			}
			evaltree(n, EV_EXIT);
		}
		close(pip[1]);
		result->fd = pip[0];
		result->jp = jp;
	}
out:
	popstackmark(&smark);
	TRACE(("evalbackcmd done: fd=%d buf=%p nleft=%d jp=%p\n",
		result->fd, result->buf, result->nleft, result->jp));
}

/*
 * Check if a builtin can safely be executed in the same process,
 * even though it should be in a subshell (command substitution).
 * Note that jobid, jobs, times and trap can show information not
 * available in a child process; this is deliberate.
 * The arguments should already have been expanded.
 */
static int
safe_builtin(int idx, int argc, char **argv)
{
	if (idx == BLTINCMD || idx == COMMANDCMD || idx == ECHOCMD ||
	    idx == FALSECMD || idx == JOBIDCMD || idx == JOBSCMD ||
	    idx == KILLCMD || idx == PRINTFCMD || idx == PWDCMD ||
	    idx == TESTCMD || idx == TIMESCMD || idx == TRUECMD ||
	    idx == TYPECMD)
		return (1);
	if (idx == EXPORTCMD || idx == TRAPCMD || idx == ULIMITCMD ||
	    idx == UMASKCMD)
		return (argc <= 1 || (argc == 2 && argv[1][0] == '-'));
	if (idx == SETCMD)
		return (argc <= 1 || (argc == 2 && (argv[1][0] == '-' ||
		    argv[1][0] == '+') && argv[1][1] == 'o' &&
		    argv[1][2] == '\0'));
	return (0);
}

/*
 * Execute a simple command.
 * Note: This may or may not return if (flags & EV_EXIT).
 */

static void
evalcommand(union node *cmd, int flags, struct backcmd *backcmd)
{
	struct stackmark smark;
	union node *argp;
	struct arglist arglist;
	struct arglist varlist;
	char **argv;
	int argc;
	char **envp;
	int varflag;
	struct strlist *sp;
	int mode;
	int pip[2];
	struct cmdentry cmdentry;
	struct job *jp;
	struct jmploc jmploc;
	struct jmploc *savehandler;
	char *savecmdname;
	struct shparam saveparam;
	struct localvar *savelocalvars;
	struct parsefile *savetopfile;
	volatile int e;
	char *lastarg;
	int realstatus;
	int do_clearcmdentry;
	const char *path = pathval();

	/* First expand the arguments. */
	TRACE(("evalcommand(%p, %d) called\n", (void *)cmd, flags));
	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	varlist.lastp = &varlist.list;
	varflag = 1;
	jp = NULL;
	do_clearcmdentry = 0;
	oexitstatus = exitstatus;
	exitstatus = 0;
	for (argp = cmd->ncmd.args ; argp ; argp = argp->narg.next) {
		if (varflag && isassignment(argp->narg.text)) {
			expandarg(argp, &varlist, EXP_VARTILDE);
			continue;
		}
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);
		varflag = 0;
	}
	*arglist.lastp = NULL;
	*varlist.lastp = NULL;
	expredir(cmd->ncmd.redirect);
	argc = 0;
	for (sp = arglist.list ; sp ; sp = sp->next)
		argc++;
	/* Add one slot at the beginning for tryexec(). */
	argv = stalloc(sizeof (char *) * (argc + 2));
	argv++;

	for (sp = arglist.list ; sp ; sp = sp->next) {
		TRACE(("evalcommand arg: %s\n", sp->text));
		*argv++ = sp->text;
	}
	*argv = NULL;
	lastarg = NULL;
	if (iflag && funcnest == 0 && argc > 0)
		lastarg = argv[-1];
	argv -= argc;

	/* Print the command if xflag is set. */
	if (xflag) {
		char sep = 0;
		const char *p, *ps4;
		ps4 = expandstr(ps4val());
		out2str(ps4 != NULL ? ps4 : ps4val());
		for (sp = varlist.list ; sp ; sp = sp->next) {
			if (sep != 0)
				out2c(' ');
			p = strchr(sp->text, '=');
			if (p != NULL) {
				p++;
				outbin(sp->text, p - sp->text, out2);
				out2qstr(p);
			} else
				out2qstr(sp->text);
			sep = ' ';
		}
		for (sp = arglist.list ; sp ; sp = sp->next) {
			if (sep != 0)
				out2c(' ');
			/* Disambiguate command looking like assignment. */
			if (sp == arglist.list &&
					strchr(sp->text, '=') != NULL &&
					strchr(sp->text, '\'') == NULL) {
				out2c('\'');
				out2str(sp->text);
				out2c('\'');
			} else
				out2qstr(sp->text);
			sep = ' ';
		}
		out2c('\n');
		flushout(&errout);
	}

	/* Now locate the command. */
	if (argc == 0) {
		/* Variable assignment(s) without command */
		cmdentry.cmdtype = CMDBUILTIN;
		cmdentry.u.index = BLTINCMD;
		cmdentry.special = 0;
	} else {
		static const char PATH[] = "PATH=";
		int cmd_flags = 0, bltinonly = 0;

		/*
		 * Modify the command lookup path, if a PATH= assignment
		 * is present
		 */
		for (sp = varlist.list ; sp ; sp = sp->next)
			if (strncmp(sp->text, PATH, sizeof(PATH) - 1) == 0) {
				path = sp->text + sizeof(PATH) - 1;
				/*
				 * On `PATH=... command`, we need to make
				 * sure that the command isn't using the
				 * non-updated hash table of the outer PATH
				 * setting and we need to make sure that
				 * the hash table isn't filled with items
				 * from the temporary setting.
				 *
				 * It would be better to forbit using and
				 * updating the table while this command
				 * runs, by the command finding mechanism
				 * is heavily integrated with hash handling,
				 * so we just delete the hash before and after
				 * the command runs. Partly deleting like
				 * changepatch() does doesn't seem worth the
				 * bookinging effort, since most such runs add
				 * directories in front of the new PATH.
				 */
				clearcmdentry();
				do_clearcmdentry = 1;
			}

		for (;;) {
			if (bltinonly) {
				cmdentry.u.index = find_builtin(*argv, &cmdentry.special);
				if (cmdentry.u.index < 0) {
					cmdentry.u.index = BLTINCMD;
					argv--;
					argc++;
					break;
				}
			} else
				find_command(argv[0], &cmdentry, cmd_flags, path);
			/* implement the bltin and command builtins here */
			if (cmdentry.cmdtype != CMDBUILTIN)
				break;
			if (cmdentry.u.index == BLTINCMD) {
				if (argc == 1)
					break;
				argv++;
				argc--;
				bltinonly = 1;
			} else if (cmdentry.u.index == COMMANDCMD) {
				if (argc == 1)
					break;
				if (!strcmp(argv[1], "-p")) {
					if (argc == 2)
						break;
					if (argv[2][0] == '-') {
						if (strcmp(argv[2], "--"))
							break;
						if (argc == 3)
							break;
						argv += 3;
						argc -= 3;
					} else {
						argv += 2;
						argc -= 2;
					}
					path = _PATH_STDPATH;
					clearcmdentry();
					do_clearcmdentry = 1;
				} else if (!strcmp(argv[1], "--")) {
					if (argc == 2)
						break;
					argv += 2;
					argc -= 2;
				} else if (argv[1][0] == '-')
					break;
				else {
					argv++;
					argc--;
				}
				cmd_flags |= DO_NOFUNC;
				bltinonly = 0;
			} else
				break;
		}
		/*
		 * Special builtins lose their special properties when
		 * called via 'command'.
		 */
		if (cmd_flags & DO_NOFUNC)
			cmdentry.special = 0;
	}

	/* Fork off a child process if necessary. */
	if (cmd->ncmd.backgnd
	 || ((cmdentry.cmdtype == CMDNORMAL || cmdentry.cmdtype == CMDUNKNOWN)
	    && ((flags & EV_EXIT) == 0 || have_traps()))
	 || ((flags & EV_BACKCMD) != 0
	    && (cmdentry.cmdtype != CMDBUILTIN ||
		 !safe_builtin(cmdentry.u.index, argc, argv)))) {
		jp = makejob(cmd, 1);
		mode = cmd->ncmd.backgnd;
		if (flags & EV_BACKCMD) {
			mode = FORK_NOJOB;
			if (pipe(pip) < 0)
				error("Pipe call failed: %s", strerror(errno));
		}
		if (forkshell(jp, cmd, mode) != 0)
			goto parent;	/* at end of routine */
		if (flags & EV_BACKCMD) {
			FORCEINTON;
			close(pip[0]);
			if (pip[1] != 1) {
				dup2(pip[1], 1);
				close(pip[1]);
			}
		}
		flags |= EV_EXIT;
	}

	/* This is the child process if a fork occurred. */
	/* Execute the command. */
	if (cmdentry.cmdtype == CMDFUNCTION) {
#ifdef DEBUG
		trputs("Shell function:  ");  trargs(argv);
#endif
		saveparam = shellparam;
		shellparam.malloc = 0;
		shellparam.reset = 1;
		shellparam.nparam = argc - 1;
		shellparam.p = argv + 1;
		shellparam.optnext = NULL;
		INTOFF;
		savelocalvars = localvars;
		localvars = NULL;
		reffunc(cmdentry.u.func);
		savehandler = handler;
		if (setjmp(jmploc.loc)) {
			freeparam(&shellparam);
			shellparam = saveparam;
			popredir();
			unreffunc(cmdentry.u.func);
			poplocalvars();
			localvars = savelocalvars;
			funcnest--;
			handler = savehandler;
			longjmp(handler->loc, 1);
		}
		handler = &jmploc;
		funcnest++;
		redirect(cmd->ncmd.redirect, REDIR_PUSH);
		INTON;
		for (sp = varlist.list ; sp ; sp = sp->next)
			mklocal(sp->text);
		exitstatus = oexitstatus;
		evaltree(getfuncnode(cmdentry.u.func),
		    flags & (EV_TESTED | EV_EXIT));
		INTOFF;
		unreffunc(cmdentry.u.func);
		poplocalvars();
		localvars = savelocalvars;
		freeparam(&shellparam);
		shellparam = saveparam;
		handler = savehandler;
		funcnest--;
		popredir();
		INTON;
		if (evalskip == SKIPFUNC) {
			evalskip = 0;
			skipcount = 0;
		}
		if (jp)
			exitshell(exitstatus);
	} else if (cmdentry.cmdtype == CMDBUILTIN) {
#ifdef DEBUG
		trputs("builtin command:  ");  trargs(argv);
#endif
		mode = (cmdentry.u.index == EXECCMD)? 0 : REDIR_PUSH;
		if (flags == EV_BACKCMD) {
			memout.nleft = 0;
			memout.nextc = memout.buf;
			memout.bufsize = 64;
			mode |= REDIR_BACKQ;
			cmdentry.special = 0;
		}
		savecmdname = commandname;
		savetopfile = getcurrentfile();
		cmdenviron = varlist.list;
		e = -1;
		savehandler = handler;
		if (setjmp(jmploc.loc)) {
			e = exception;
			if (e == EXINT)
				exitstatus = SIGINT+128;
			else if (e != EXEXIT)
				exitstatus = 2;
			goto cmddone;
		}
		handler = &jmploc;
		redirect(cmd->ncmd.redirect, mode);
		/*
		 * If there is no command word, redirection errors should
		 * not be fatal but assignment errors should.
		 */
		if (argc == 0 && !(flags & EV_BACKCMD))
			cmdentry.special = 1;
		listsetvar(cmdenviron, cmdentry.special ? 0 : VNOSET);
		if (argc > 0)
			bltinsetlocale();
		commandname = argv[0];
		argptr = argv + 1;
		nextopt_optptr = NULL;		/* initialize nextopt */
		builtin_flags = flags;
		exitstatus = (*builtinfunc[cmdentry.u.index])(argc, argv);
		flushall();
cmddone:
		if (argc > 0)
			bltinunsetlocale();
		cmdenviron = NULL;
		out1 = &output;
		out2 = &errout;
		freestdout();
		handler = savehandler;
		commandname = savecmdname;
		if (jp)
			exitshell(exitstatus);
		if (flags == EV_BACKCMD) {
			backcmd->buf = memout.buf;
			backcmd->nleft = memout.nextc - memout.buf;
			memout.buf = NULL;
		}
		if (cmdentry.u.index != EXECCMD)
			popredir();
		if (e != -1) {
			if ((e != EXERROR && e != EXEXEC)
			    || cmdentry.special)
				exraise(e);
			popfilesupto(savetopfile);
			if (flags != EV_BACKCMD)
				FORCEINTON;
		}
	} else {
#ifdef DEBUG
		trputs("normal command:  ");  trargs(argv);
#endif
		redirect(cmd->ncmd.redirect, 0);
		for (sp = varlist.list ; sp ; sp = sp->next)
			setvareq(sp->text, VEXPORT|VSTACK);
		envp = environment();
		shellexec(argv, envp, path, cmdentry.u.index);
		/*NOTREACHED*/
	}
	goto out;

parent:	/* parent process gets here (if we forked) */
	if (mode == FORK_FG) {	/* argument to fork */
		INTOFF;
		exitstatus = waitforjob(jp, &realstatus);
		INTON;
		if (iflag && loopnest > 0 && WIFSIGNALED(realstatus)) {
			evalskip = SKIPBREAK;
			skipcount = loopnest;
		}
	} else if (mode == FORK_NOJOB) {
		backcmd->fd = pip[0];
		close(pip[1]);
		backcmd->jp = jp;
	} else
		exitstatus = 0;

out:
	if (lastarg)
		setvar("_", lastarg, 0);
	if (do_clearcmdentry)
		clearcmdentry();
	popstackmark(&smark);
}



/*
 * Search for a command.  This is called before we fork so that the
 * location of the command will be available in the parent as well as
 * the child.  The check for "goodname" is an overly conservative
 * check that the name will not be subject to expansion.
 */

static void
prehash(union node *n)
{
	struct cmdentry entry;

	if (n && n->type == NCMD && n->ncmd.args)
		if (goodname(n->ncmd.args->narg.text))
			find_command(n->ncmd.args->narg.text, &entry, 0,
				     pathval());
}



/*
 * Builtin commands.  Builtin commands whose functions are closely
 * tied to evaluation are implemented here.
 */

/*
 * No command given, a bltin command with no arguments, or a bltin command
 * with an invalid name.
 */

int
bltincmd(int argc, char **argv)
{
	if (argc > 1) {
		out2fmt_flush("%s: not found\n", argv[1]);
		return 127;
	}
	/*
	 * Preserve exitstatus of a previous possible redirection
	 * as POSIX mandates
	 */
	return exitstatus;
}


/*
 * Handle break and continue commands.  Break, continue, and return are
 * all handled by setting the evalskip flag.  The evaluation routines
 * above all check this flag, and if it is set they start skipping
 * commands rather than executing them.  The variable skipcount is
 * the number of loops to break/continue, or the number of function
 * levels to return.  (The latter is always 1.)  It should probably
 * be an error to break out of more loops than exist, but it isn't
 * in the standard shell so we don't make it one here.
 */

int
breakcmd(int argc, char **argv)
{
	int n = argc > 1 ? number(argv[1]) : 1;

	if (n > loopnest)
		n = loopnest;
	if (n > 0) {
		evalskip = (**argv == 'c')? SKIPCONT : SKIPBREAK;
		skipcount = n;
	}
	return 0;
}

/*
 * The `command' command.
 */
int
commandcmd(int argc, char **argv)
{
	const char *path;
	int ch;
	int cmd = -1;

	path = bltinlookup("PATH", 1);

	optind = optreset = 1;
	opterr = 0;
	while ((ch = getopt(argc, argv, "pvV")) != -1) {
		switch (ch) {
		case 'p':
			path = _PATH_STDPATH;
			break;
		case 'v':
			cmd = TYPECMD_SMALLV;
			break;
		case 'V':
			cmd = TYPECMD_BIGV;
			break;
		case '?':
		default:
			error("unknown option: -%c", optopt);
		}
	}
	argc -= optind;
	argv += optind;

	if (cmd != -1) {
		if (argc != 1)
			error("wrong number of arguments");
		return typecmd_impl(2, argv - 1, cmd, path);
	}
	if (argc != 0)
		error("commandcmd bad call");

	/*
	 * Do nothing successfully if no command was specified;
	 * ksh also does this.
	 */
	return 0;
}


/*
 * The return command.
 */

int
returncmd(int argc, char **argv)
{
	int ret = argc > 1 ? number(argv[1]) : oexitstatus;

	if (funcnest) {
		evalskip = SKIPFUNC;
		skipcount = 1;
	} else {
		/* skip the rest of the file */
		evalskip = SKIPFILE;
		skipcount = 1;
	}
	return ret;
}


int
falsecmd(int argc __unused, char **argv __unused)
{
	return 1;
}


int
truecmd(int argc __unused, char **argv __unused)
{
	return 0;
}


int
execcmd(int argc, char **argv)
{
	/*
	 * Because we have historically not supported any options,
	 * only treat "--" specially.
	 */
	if (argc > 1 && strcmp(argv[1], "--") == 0)
		argc--, argv++;
	if (argc > 1) {
		struct strlist *sp;

		iflag = 0;		/* exit on error */
		mflag = 0;
		optschanged();
		for (sp = cmdenviron; sp ; sp = sp->next)
			setvareq(sp->text, VEXPORT|VSTACK);
		shellexec(argv + 1, environment(), pathval(), 0);

	}
	return 0;
}


int
timescmd(int argc __unused, char **argv __unused)
{
	struct rusage ru;
	long shumins, shsmins, chumins, chsmins;
	double shusecs, shssecs, chusecs, chssecs;

	if (getrusage(RUSAGE_SELF, &ru) < 0)
		return 1;
	shumins = ru.ru_utime.tv_sec / 60;
	shusecs = ru.ru_utime.tv_sec % 60 + ru.ru_utime.tv_usec / 1000000.;
	shsmins = ru.ru_stime.tv_sec / 60;
	shssecs = ru.ru_stime.tv_sec % 60 + ru.ru_stime.tv_usec / 1000000.;
	if (getrusage(RUSAGE_CHILDREN, &ru) < 0)
		return 1;
	chumins = ru.ru_utime.tv_sec / 60;
	chusecs = ru.ru_utime.tv_sec % 60 + ru.ru_utime.tv_usec / 1000000.;
	chsmins = ru.ru_stime.tv_sec / 60;
	chssecs = ru.ru_stime.tv_sec % 60 + ru.ru_stime.tv_usec / 1000000.;
	out1fmt("%ldm%.3fs %ldm%.3fs\n%ldm%.3fs %ldm%.3fs\n", shumins,
	    shusecs, shsmins, shssecs, chumins, chusecs, chsmins, chssecs);
	return 0;
}
