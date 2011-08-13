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
static void expredir(union node *);
static void evalpipe(union node *);
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

SHELLPROC {
	exitstatus = 0;
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
                                while (*p)
                                        STPUTC(*p++, concat);
                                if ((p = *ap++) == NULL)
                                        break;
                                STPUTC(' ', concat);
                        }
                        STPUTC('\0', concat);
                        p = grabstackstr(concat);
                }
                evalstring(p, builtin_flags & EV_TESTED);
        }
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

	flags_exit = flags & EV_EXIT;
	flags &= ~EV_EXIT;
	setstackmark(&smark);
	setinputstring(s, 1);
	while ((n = parsecmd(0)) != NEOF) {
		if (n != NULL) {
			if (flags_exit && preadateof())
				evaltree(n, flags | EV_EXIT);
			else
				evaltree(n, flags);
		}
		popstackmark(&smark);
	}
	popfile();
	popstackmark(&smark);
	if (flags_exit)
		exitshell(exitstatus);
}


/*
 * Evaluate a parse tree.  The value is left in the global variable
 * exitstatus.
 */

void
evaltree(union node *n, int flags)
{
	int do_etest;

	do_etest = 0;
	if (n == NULL) {
		TRACE(("evaltree(NULL) called\n"));
		exitstatus = 0;
		goto out;
	}
#ifndef NO_HISTORY
	displayhist = 1;	/* show history substitutions done with fc */
#endif
	TRACE(("evaltree(%p: %d) called\n", (void *)n, n->type));
	switch (n->type) {
	case NSEMI:
		evaltree(n->nbinary.ch1, flags & ~EV_EXIT);
		if (evalskip)
			goto out;
		evaltree(n->nbinary.ch2, flags);
		break;
	case NAND:
		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip || exitstatus != 0) {
			goto out;
		}
		evaltree(n->nbinary.ch2, flags);
		break;
	case NOR:
		evaltree(n->nbinary.ch1, EV_TESTED);
		if (evalskip || exitstatus == 0)
			goto out;
		evaltree(n->nbinary.ch2, flags);
		break;
	case NREDIR:
		oexitstatus = exitstatus;
		expredir(n->nredir.redirect);
		redirect(n->nredir.redirect, REDIR_PUSH);
		evaltree(n->nredir.n, flags);
		popredir();
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
			evaltree(n->nif.ifpart, flags);
		else if (n->nif.elsepart)
			evaltree(n->nif.elsepart, flags);
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
out:
	if (pendingsigs)
		dotrap();
	if ((flags & EV_EXIT) || (eflag && exitstatus != 0 && do_etest))
		exitshell(exitstatus);
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
	}
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
		close(pip[1]);
	}
	INTON;
	if (n->npipe.backgnd == 0) {
		INTOFF;
		exitstatus = waitforjob(jp, (int *)NULL);
		TRACE(("evalpipe:  job done exit status %d\n", exitstatus));
		INTON;
	}
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

	setstackmark(&smark);
	result->fd = -1;
	result->buf = NULL;
	result->nleft = 0;
	result->jp = NULL;
	if (n == NULL) {
		exitstatus = 0;
		goto out;
	}
	if (n->type == NCMD) {
		exitstatus = oexitstatus;
		evalcommand(n, EV_BACKCMD, result);
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
 * Execute a simple command.
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
	volatile int e;
	char *lastarg;
	int realstatus;
	int do_clearcmdentry;

	/* First expand the arguments. */
	TRACE(("evalcommand(%p, %d) called\n", (void *)cmd, flags));
	setstackmark(&smark);
	arglist.lastp = &arglist.list;
	varlist.lastp = &varlist.list;
	varflag = 1;
	do_clearcmdentry = 0;
	oexitstatus = exitstatus;
	exitstatus = 0;
	for (argp = cmd->ncmd.args ; argp ; argp = argp->narg.next) {
		char *p = argp->narg.text;
		if (varflag && is_name(*p)) {
			do {
				p++;
			} while (is_in_name(*p));
			if (*p == '=') {
				expandarg(argp, &varlist, EXP_VARTILDE);
				continue;
			}
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
	argv = stalloc(sizeof (char *) * (argc + 1));

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
		const char *p;
		out2str(ps4val());
		for (sp = varlist.list ; sp ; sp = sp->next) {
			if (sep != 0)
				outc(' ', &errout);
			p = sp->text;
			while (*p != '=' && *p != '\0')
				out2c(*p++);
			if (*p != '\0') {
				out2c(*p++);
				out2qstr(p);
			}
			sep = ' ';
		}
		for (sp = arglist.list ; sp ; sp = sp->next) {
			if (sep != 0)
				outc(' ', &errout);
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
		outc('\n', &errout);
		flushout(&errout);
	}

	/* Now locate the command. */
	if (argc == 0) {
		/* Variable assignment(s) without command */
		cmdentry.cmdtype = CMDBUILTIN;
		cmdentry.u.index = BLTINCMD;
		cmdentry.special = 1;
	} else {
		static const char PATH[] = "PATH=";
		char *path = pathval();

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
				clearcmdentry(0);
				do_clearcmdentry = 1;
			}

		find_command(argv[0], &cmdentry, 1, path);
		if (cmdentry.cmdtype == CMDUNKNOWN) {	/* command not found */
			exitstatus = 127;
			flushout(&errout);
			return;
		}
		/* implement the bltin builtin here */
		if (cmdentry.cmdtype == CMDBUILTIN && cmdentry.u.index == BLTINCMD) {
			for (;;) {
				argv++;
				if (--argc == 0)
					break;
				if ((cmdentry.u.index = find_builtin(*argv,
				    &cmdentry.special)) < 0) {
					outfmt(&errout, "%s: not found\n", *argv);
					exitstatus = 127;
					flushout(&errout);
					return;
				}
				if (cmdentry.u.index != BLTINCMD)
					break;
			}
		}
	}

	/* Fork off a child process if necessary. */
	if (cmd->ncmd.backgnd
	 || (cmdentry.cmdtype == CMDNORMAL
	    && ((flags & EV_EXIT) == 0 || have_traps()))
	 || ((flags & EV_BACKCMD) != 0
	    && (cmdentry.cmdtype != CMDBUILTIN
		 || cmdentry.u.index == CDCMD
		 || cmdentry.u.index == DOTCMD
		 || cmdentry.u.index == EVALCMD))
	 || (cmdentry.cmdtype == CMDBUILTIN &&
	    cmdentry.u.index == COMMANDCMD)) {
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
		redirect(cmd->ncmd.redirect, REDIR_PUSH);
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
		INTON;
		savehandler = handler;
		if (setjmp(jmploc.loc)) {
			if (exception == EXSHELLPROC)
				freeparam(&saveparam);
			else {
				freeparam(&shellparam);
				shellparam = saveparam;
			}
			unreffunc(cmdentry.u.func);
			poplocalvars();
			localvars = savelocalvars;
			handler = savehandler;
			longjmp(handler->loc, 1);
		}
		handler = &jmploc;
		for (sp = varlist.list ; sp ; sp = sp->next)
			mklocal(sp->text);
		funcnest++;
		exitstatus = oexitstatus;
		if (flags & EV_TESTED)
			evaltree(getfuncnode(cmdentry.u.func), EV_TESTED);
		else
			evaltree(getfuncnode(cmdentry.u.func), 0);
		funcnest--;
		INTOFF;
		unreffunc(cmdentry.u.func);
		poplocalvars();
		localvars = savelocalvars;
		freeparam(&shellparam);
		shellparam = saveparam;
		handler = savehandler;
		popredir();
		INTON;
		if (evalskip == SKIPFUNC) {
			evalskip = 0;
			skipcount = 0;
		}
		if (flags & EV_EXIT)
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
		}
		savecmdname = commandname;
		cmdenviron = varlist.list;
		e = -1;
		savehandler = handler;
		if (setjmp(jmploc.loc)) {
			e = exception;
			exitstatus = (e == EXINT)? SIGINT+128 : 2;
			goto cmddone;
		}
		handler = &jmploc;
		redirect(cmd->ncmd.redirect, mode);
		if (cmdentry.special)
			listsetvar(cmdenviron);
		commandname = argv[0];
		argptr = argv + 1;
		nextopt_optptr = NULL;		/* initialize nextopt */
		builtin_flags = flags;
		exitstatus = (*builtinfunc[cmdentry.u.index])(argc, argv);
		flushall();
cmddone:
		cmdenviron = NULL;
		out1 = &output;
		out2 = &errout;
		freestdout();
		if (e != EXSHELLPROC) {
			commandname = savecmdname;
			if (flags & EV_EXIT) {
				exitshell(exitstatus);
			}
		}
		handler = savehandler;
		if (e != -1) {
			if ((e != EXERROR && e != EXEXEC)
			    || cmdentry.special)
				exraise(e);
			FORCEINTON;
		}
		if (cmdentry.u.index != EXECCMD)
			popredir();
		if (flags == EV_BACKCMD) {
			backcmd->buf = memout.buf;
			backcmd->nleft = memout.nextc - memout.buf;
			memout.buf = NULL;
		}
	} else {
#ifdef DEBUG
		trputs("normal command:  ");  trargs(argv);
#endif
		clearredir();
		redirect(cmd->ncmd.redirect, 0);
		for (sp = varlist.list ; sp ; sp = sp->next)
			setvareq(sp->text, VEXPORT|VSTACK);
		envp = environment();
		shellexec(argv, envp, pathval(), cmdentry.u.index);
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
	}

out:
	if (lastarg)
		setvar("_", lastarg, 0);
	if (do_clearcmdentry)
		clearcmdentry(0);
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
 * No command given, or a bltin command with no arguments.
 */

int
bltincmd(int argc __unused, char **argv __unused)
{
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
	static char stdpath[] = _PATH_STDPATH;
	struct jmploc loc, *old;
	struct strlist *sp;
	char *path;
	int ch;
	int cmd = -1;

	for (sp = cmdenviron; sp ; sp = sp->next)
		setvareq(sp->text, VEXPORT|VSTACK);
	path = pathval();

	optind = optreset = 1;
	opterr = 0;
	while ((ch = getopt(argc, argv, "pvV")) != -1) {
		switch (ch) {
		case 'p':
			path = stdpath;
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
		return typecmd_impl(2, argv - 1, cmd);
	}
	if (argc != 0) {
		old = handler;
		handler = &loc;
		if (setjmp(handler->loc) == 0)
			shellexec(argv, environment(), path, 0);
		handler = old;
		if (exception == EXEXEC)
			exit(exerrno);
		exraise(exception);
	}

	/*
	 * Do nothing successfully if no command was specified;
	 * ksh also does this.
	 */
	exit(0);
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
