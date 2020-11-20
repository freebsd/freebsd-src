/*	$NetBSD: job.c,v 1.326 2020/11/16 18:28:27 rillig Exp $	*/

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
 * job.c --
 *	handle the creation etc. of our child processes.
 *
 * Interface:
 *	Job_Init	Called to initialize this module. In addition,
 *			any commands attached to the .BEGIN target
 *			are executed before this function returns.
 *			Hence, the makefiles must have been parsed
 *			before this function is called.
 *
 *	Job_End		Clean up any memory used.
 *
 *	Job_Make	Start the creation of the given target.
 *
 *	Job_CatchChildren
 *			Check for and handle the termination of any
 *			children. This must be called reasonably
 *			frequently to keep the whole make going at
 *			a decent clip, since job table entries aren't
 *			removed until their process is caught this way.
 *
 *	Job_CatchOutput
 *			Print any output our children have produced.
 *			Should also be called fairly frequently to
 *			keep the user informed of what's going on.
 *			If no output is waiting, it will block for
 *			a time given by the SEL_* constants, below,
 *			or until output is ready.
 *
 *	Job_ParseShell	Given the line following a .SHELL target, parse
 *			the line as a shell specification. Returns
 *			FALSE if the spec was incorrect.
 *
 *	Job_Finish	Perform any final processing which needs doing.
 *			This includes the execution of any commands
 *			which have been/were attached to the .END
 *			target. It should only be called when the
 *			job table is empty.
 *
 *	Job_AbortAll	Abort all currently running jobs. It doesn't
 *			handle output or do anything for the jobs,
 *			just kills them. It should only be called in
 *			an emergency.
 *
 *	Job_CheckCommands
 *			Verify that the commands for a target are
 *			ok. Provide them if necessary and possible.
 *
 *	Job_Touch	Update a target without really updating it.
 *
 *	Job_Wait	Wait for all currently-running jobs to finish.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include "wait.h"

#include <errno.h>
#if !defined(USE_SELECT) && defined(HAVE_POLL_H)
#include <poll.h>
#else
#ifndef USE_SELECT			/* no poll.h */
# define USE_SELECT
#endif
#if defined(HAVE_SYS_SELECT_H)
# include <sys/select.h>
#endif
#endif
#include <signal.h>
#include <utime.h>
#if defined(HAVE_SYS_SOCKET_H)
# include <sys/socket.h>
#endif

#include "make.h"
#include "dir.h"
#include "job.h"
#include "pathnames.h"
#include "trace.h"

/*	"@(#)job.c	8.2 (Berkeley) 3/19/94"	*/
MAKE_RCSID("$NetBSD: job.c,v 1.326 2020/11/16 18:28:27 rillig Exp $");

/* A shell defines how the commands are run.  All commands for a target are
 * written into a single file, which is then given to the shell to execute
 * the commands from it.  The commands are written to the file using a few
 * templates for echo control and error control.
 *
 * The name of the shell is the basename for the predefined shells, such as
 * "sh", "csh", "bash".  For custom shells, it is the full pathname, and its
 * basename is used to select the type of shell; the longest match wins.
 * So /usr/pkg/bin/bash has type sh, /usr/local/bin/tcsh has type csh.
 *
 * The echoing of command lines is controlled using hasEchoCtl, echoOff,
 * echoOn, noPrint and noPrintLen.  When echoOff is executed by the shell, it
 * still outputs something, but this something is not interesting, therefore
 * it is filtered out using noPrint and noPrintLen.
 *
 * The error checking for individual commands is controlled using hasErrCtl,
 * errOnOrEcho, errOffOrExecIgnore and errExit.
 *
 * If a shell doesn't have error control, errOnOrEcho becomes a printf template
 * for echoing the command, should echoing be on; errOffOrExecIgnore becomes
 * another printf template for executing the command while ignoring the return
 * status. Finally errExit is a printf template for running the command and
 * causing the shell to exit on error. If any of these strings are empty when
 * hasErrCtl is FALSE, the command will be executed anyway as is, and if it
 * causes an error, so be it. Any templates set up to echo the command will
 * escape any '$ ` \ "' characters in the command string to avoid common
 * problems with echo "%s\n" as a template.
 *
 * The command-line flags "echo" and "exit" also control the behavior.  The
 * "echo" flag causes the shell to start echoing commands right away.  The
 * "exit" flag causes the shell to exit when an error is detected in one of
 * the commands.
 */
typedef struct Shell {

    /* The name of the shell. For Bourne and C shells, this is used only to
     * find the shell description when used as the single source of a .SHELL
     * target. For user-defined shells, this is the full path of the shell. */
    const char *name;

    Boolean hasEchoCtl;		/* True if both echoOff and echoOn defined */
    const char *echoOff;	/* command to turn off echo */
    const char *echoOn;		/* command to turn it back on again */
    const char *noPrint;	/* text to skip when printing output from
				 * shell. This is usually the same as echoOff */
    size_t noPrintLen;		/* length of noPrint command */

    Boolean hasErrCtl;		/* set if can control error checking for
				 * individual commands */
    /* XXX: split into errOn and echoCmd */
    const char *errOnOrEcho;	/* template to turn on error checking */
    /* XXX: split into errOff and execIgnore */
    const char *errOffOrExecIgnore; /* template to turn off error checking */
    const char *errExit;	/* template to use for testing exit code */

    /* string literal that results in a newline character when it appears
     * outside of any 'quote' or "quote" characters */
    const char *newline;
    char commentChar;		/* character used by shell for comment lines */

    /*
     * command-line flags
     */
    const char *echo;		/* echo commands */
    const char *exit;		/* exit on error */
} Shell;

/*
 * FreeBSD: traditionally .MAKE is not required to
 * pass jobs queue to sub-makes.
 * Use .MAKE.ALWAYS_PASS_JOB_QUEUE=no to disable.
 */
#define MAKE_ALWAYS_PASS_JOB_QUEUE ".MAKE.ALWAYS_PASS_JOB_QUEUE"
static int Always_pass_job_queue = TRUE;
/*
 * FreeBSD: aborting entire parallel make isn't always
 * desired. When doing tinderbox for example, failure of
 * one architecture should not stop all.
 * We still want to bail on interrupt though.
 */
#define MAKE_JOB_ERROR_TOKEN "MAKE_JOB_ERROR_TOKEN"
static int Job_error_token = TRUE;

/*
 * error handling variables
 */
static int errors = 0;		/* number of errors reported */
typedef enum AbortReason {	/* why is the make aborting? */
    ABORT_NONE,
    ABORT_ERROR,		/* Because of an error */
    ABORT_INTERRUPT,		/* Because it was interrupted */
    ABORT_WAIT			/* Waiting for jobs to finish */
} AbortReason;
static AbortReason aborting = ABORT_NONE;
#define JOB_TOKENS	"+EI+"	/* Token to requeue for each abort state */

/*
 * this tracks the number of tokens currently "out" to build jobs.
 */
int jobTokensRunning = 0;

/* The number of commands actually printed to the shell commands file for
 * the current job.  Should this number be 0, no shell will be executed. */
static int numCommands;

typedef enum JobStartResult {
    JOB_RUNNING,		/* Job is running */
    JOB_ERROR,			/* Error in starting the job */
    JOB_FINISHED		/* The job is already finished */
} JobStartResult;

/*
 * Descriptions for various shells.
 *
 * The build environment may set DEFSHELL_INDEX to one of
 * DEFSHELL_INDEX_SH, DEFSHELL_INDEX_KSH, or DEFSHELL_INDEX_CSH, to
 * select one of the predefined shells as the default shell.
 *
 * Alternatively, the build environment may set DEFSHELL_CUSTOM to the
 * name or the full path of a sh-compatible shell, which will be used as
 * the default shell.
 *
 * ".SHELL" lines in Makefiles can choose the default shell from the
 * set defined here, or add additional shells.
 */

#ifdef DEFSHELL_CUSTOM
#define DEFSHELL_INDEX_CUSTOM 0
#define DEFSHELL_INDEX_SH     1
#define DEFSHELL_INDEX_KSH    2
#define DEFSHELL_INDEX_CSH    3
#else /* !DEFSHELL_CUSTOM */
#define DEFSHELL_INDEX_SH     0
#define DEFSHELL_INDEX_KSH    1
#define DEFSHELL_INDEX_CSH    2
#endif /* !DEFSHELL_CUSTOM */

#ifndef DEFSHELL_INDEX
#define DEFSHELL_INDEX 0	/* DEFSHELL_INDEX_CUSTOM or DEFSHELL_INDEX_SH */
#endif /* !DEFSHELL_INDEX */

static Shell    shells[] = {
#ifdef DEFSHELL_CUSTOM
    /*
     * An sh-compatible shell with a non-standard name.
     *
     * Keep this in sync with the "sh" description below, but avoid
     * non-portable features that might not be supplied by all
     * sh-compatible shells.
     */
{
    DEFSHELL_CUSTOM,		/* .name */
    FALSE,			/* .hasEchoCtl */
    "",				/* .echoOff */
    "",				/* .echoOn */
    "",				/* .noPrint */
    0,				/* .noPrintLen */
    FALSE,			/* .hasErrCtl */
    "echo \"%s\"\n",		/* .errOnOrEcho */
    "%s\n",			/* .errOffOrExecIgnore */
    "{ %s \n} || exit $?\n",	/* .errExit */
    "'\n'",			/* .newline */
    '#',			/* .commentChar */
    "",				/* .echo */
    "",				/* .exit */
},
#endif /* DEFSHELL_CUSTOM */
    /*
     * SH description. Echo control is also possible and, under
     * sun UNIX anyway, one can even control error checking.
     */
{
    "sh",			/* .name */
    FALSE,			/* .hasEchoCtl */
    "",				/* .echoOff */
    "",				/* .echoOn */
    "",				/* .noPrint */
    0,				/* .noPrintLen */
    FALSE,			/* .hasErrCtl */
    "echo \"%s\"\n", 		/* .errOnOrEcho */
    "%s\n",			/* .errOffOrExecIgnore */
    "{ %s \n} || exit $?\n",	/* .errExit */
    "'\n'",			/* .newline */
    '#',			/* .commentChar*/
#if defined(MAKE_NATIVE) && defined(__NetBSD__)
    "q",			/* .echo */
#else
    "",				/* .echo */
#endif
    "",				/* .exit */
},
    /*
     * KSH description.
     */
{
    "ksh",			/* .name */
    TRUE,			/* .hasEchoCtl */
    "set +v",			/* .echoOff */
    "set -v",			/* .echoOn */
    "set +v",			/* .noPrint */
    6,				/* .noPrintLen */
    FALSE,			/* .hasErrCtl */
    "echo \"%s\"\n",		/* .errOnOrEcho */
    "%s\n",			/* .errOffOrExecIgnore */
    "{ %s \n} || exit $?\n",	/* .errExit */
    "'\n'",			/* .newline */
    '#',			/* .commentChar */
    "v",			/* .echo */
    "",				/* .exit */
},
    /*
     * CSH description. The csh can do echo control by playing
     * with the setting of the 'echo' shell variable. Sadly,
     * however, it is unable to do error control nicely.
     */
{
    "csh",			/* .name */
    TRUE,			/* .hasEchoCtl */
    "unset verbose",		/* .echoOff */
    "set verbose",		/* .echoOn */
    "unset verbose", 		/* .noPrint */
    13,				/* .noPrintLen */
    FALSE, 			/* .hasErrCtl */
    "echo \"%s\"\n", 		/* .errOnOrEcho */
    /* XXX: Mismatch between errOn and execIgnore */
    "csh -c \"%s || exit 0\"\n", /* .errOffOrExecIgnore */
    "", 			/* .errExit */
    "'\\\n'",			/* .newline */
    '#',			/* .commentChar */
    "v", 			/* .echo */
    "e",			/* .exit */
}
};

/* This is the shell to which we pass all commands in the Makefile.
 * It is set by the Job_ParseShell function. */
static Shell *commandShell = &shells[DEFSHELL_INDEX];
const char *shellPath = NULL;	/* full pathname of executable image */
const char *shellName = NULL;	/* last component of shellPath */
char *shellErrFlag = NULL;
static char *shellArgv = NULL;	/* Custom shell args */


static Job *job_table;		/* The structures that describe them */
static Job *job_table_end;	/* job_table + maxJobs */
static unsigned int wantToken;	/* we want a token */
static Boolean lurking_children = FALSE;
static Boolean make_suspended = FALSE;	/* Whether we've seen a SIGTSTP (etc) */

/*
 * Set of descriptors of pipes connected to
 * the output channels of children
 */
static struct pollfd *fds = NULL;
static Job **jobfds = NULL;
static nfds_t nfds = 0;
static void watchfd(Job *);
static void clearfd(Job *);
static int readyfd(Job *);

static GNode *lastNode;		/* The node for which output was most recently
				 * produced. */
static char *targPrefix = NULL; /* What we print at the start of TARG_FMT */
static Job tokenWaitJob;	/* token wait pseudo-job */

static Job childExitJob;	/* child exit pseudo-job */
#define	CHILD_EXIT	"."
#define	DO_JOB_RESUME	"R"

enum { npseudojobs = 2 };	/* number of pseudo-jobs */

#define TARG_FMT  "%s %s ---\n" /* Default format */
#define MESSAGE(fp, gn) \
	if (opts.maxJobs != 1 && targPrefix && *targPrefix) \
	    (void)fprintf(fp, TARG_FMT, targPrefix, gn->name)

static sigset_t caught_signals;	/* Set of signals we handle */

static void JobDoOutput(Job *, Boolean);
static void JobInterrupt(int, int) MAKE_ATTR_DEAD;
static void JobRestartJobs(void);
static void JobSigReset(void);

static unsigned
nfds_per_job(void)
{
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
    if (useMeta)
	return 2;
#endif
    return 1;
}

static void
job_table_dump(const char *where)
{
    Job *job;

    debug_printf("job table @ %s\n", where);
    for (job = job_table; job < job_table_end; job++) {
	debug_printf("job %d, status %d, flags %d, pid %d\n",
		     (int)(job - job_table), job->status, job->flags, job->pid);
    }
}

/*
 * Delete the target of a failed, interrupted, or otherwise
 * unsuccessful job unless inhibited by .PRECIOUS.
 */
static void
JobDeleteTarget(GNode *gn)
{
    const char *file;

    if (gn->type & OP_JOIN)
	return;
    if (gn->type & OP_PHONY)
	return;
    if (Targ_Precious(gn))
	return;
    if (opts.noExecute)
	return;

    file = GNode_Path(gn);
    if (eunlink(file) != -1)
	Error("*** %s removed", file);
}

/*
 * JobSigLock/JobSigUnlock
 *
 * Signal lock routines to get exclusive access. Currently used to
 * protect `jobs' and `stoppedJobs' list manipulations.
 */
static void JobSigLock(sigset_t *omaskp)
{
	if (sigprocmask(SIG_BLOCK, &caught_signals, omaskp) != 0) {
		Punt("JobSigLock: sigprocmask: %s", strerror(errno));
		sigemptyset(omaskp);
	}
}

static void JobSigUnlock(sigset_t *omaskp)
{
	(void)sigprocmask(SIG_SETMASK, omaskp, NULL);
}

static void
JobCreatePipe(Job *job, int minfd)
{
    int i, fd, flags;
    int pipe_fds[2];

    if (pipe(pipe_fds) == -1)
	Punt("Cannot create pipe: %s", strerror(errno));

    for (i = 0; i < 2; i++) {
	/* Avoid using low numbered fds */
	fd = fcntl(pipe_fds[i], F_DUPFD, minfd);
	if (fd != -1) {
	    close(pipe_fds[i]);
	    pipe_fds[i] = fd;
	}
    }

    job->inPipe = pipe_fds[0];
    job->outPipe = pipe_fds[1];

    /* Set close-on-exec flag for both */
    if (fcntl(job->inPipe, F_SETFD, FD_CLOEXEC) == -1)
	Punt("Cannot set close-on-exec: %s", strerror(errno));
    if (fcntl(job->outPipe, F_SETFD, FD_CLOEXEC) == -1)
	Punt("Cannot set close-on-exec: %s", strerror(errno));

    /*
     * We mark the input side of the pipe non-blocking; we poll(2) the
     * pipe when we're waiting for a job token, but we might lose the
     * race for the token when a new one becomes available, so the read
     * from the pipe should not block.
     */
    flags = fcntl(job->inPipe, F_GETFL, 0);
    if (flags == -1)
	Punt("Cannot get flags: %s", strerror(errno));
    flags |= O_NONBLOCK;
    if (fcntl(job->inPipe, F_SETFL, flags) == -1)
	Punt("Cannot set flags: %s", strerror(errno));
}

/* Pass the signal to each running job. */
static void
JobCondPassSig(int signo)
{
    Job *job;

    DEBUG1(JOB, "JobCondPassSig(%d) called.\n", signo);

    for (job = job_table; job < job_table_end; job++) {
	if (job->status != JOB_ST_RUNNING)
	    continue;
	DEBUG2(JOB, "JobCondPassSig passing signal %d to child %d.\n",
	       signo, job->pid);
	KILLPG(job->pid, signo);
    }
}

/* SIGCHLD handler.
 *
 * Sends a token on the child exit pipe to wake us up from select()/poll(). */
static void
JobChildSig(int signo MAKE_ATTR_UNUSED)
{
    while (write(childExitJob.outPipe, CHILD_EXIT, 1) == -1 && errno == EAGAIN)
	continue;
}


/* Resume all stopped jobs. */
static void
JobContinueSig(int signo MAKE_ATTR_UNUSED)
{
    /*
     * Defer sending SIGCONT to our stopped children until we return
     * from the signal handler.
     */
    while (write(childExitJob.outPipe, DO_JOB_RESUME, 1) == -1 &&
	errno == EAGAIN)
	continue;
}

/* Pass a signal on to all jobs, then resend to ourselves.
 * We die by the same signal. */
MAKE_ATTR_DEAD static void
JobPassSig_int(int signo)
{
    /* Run .INTERRUPT target then exit */
    JobInterrupt(TRUE, signo);
}

/* Pass a signal on to all jobs, then resend to ourselves.
 * We die by the same signal. */
MAKE_ATTR_DEAD static void
JobPassSig_term(int signo)
{
    /* Dont run .INTERRUPT target then exit */
    JobInterrupt(FALSE, signo);
}

static void
JobPassSig_suspend(int signo)
{
    sigset_t nmask, omask;
    struct sigaction act;

    /* Suppress job started/continued messages */
    make_suspended = TRUE;

    /* Pass the signal onto every job */
    JobCondPassSig(signo);

    /*
     * Send ourselves the signal now we've given the message to everyone else.
     * Note we block everything else possible while we're getting the signal.
     * This ensures that all our jobs get continued when we wake up before
     * we take any other signal.
     */
    sigfillset(&nmask);
    sigdelset(&nmask, signo);
    (void)sigprocmask(SIG_SETMASK, &nmask, &omask);

    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    (void)sigaction(signo, &act, NULL);

    if (DEBUG(JOB))
	debug_printf("JobPassSig passing signal %d to self.\n", signo);

    (void)kill(getpid(), signo);

    /*
     * We've been continued.
     *
     * A whole host of signals continue to happen!
     * SIGCHLD for any processes that actually suspended themselves.
     * SIGCHLD for any processes that exited while we were alseep.
     * The SIGCONT that actually caused us to wakeup.
     *
     * Since we defer passing the SIGCONT on to our children until
     * the main processing loop, we can be sure that all the SIGCHLD
     * events will have happened by then - and that the waitpid() will
     * collect the child 'suspended' events.
     * For correct sequencing we just need to ensure we process the
     * waitpid() before passing on the SIGCONT.
     *
     * In any case nothing else is needed here.
     */

    /* Restore handler and signal mask */
    act.sa_handler = JobPassSig_suspend;
    (void)sigaction(signo, &act, NULL);
    (void)sigprocmask(SIG_SETMASK, &omask, NULL);
}

static Job *
JobFindPid(int pid, JobStatus status, Boolean isJobs)
{
    Job *job;

    for (job = job_table; job < job_table_end; job++) {
	if (job->status == status && job->pid == pid)
	    return job;
    }
    if (DEBUG(JOB) && isJobs)
	job_table_dump("no pid");
    return NULL;
}

/* Parse leading '@', '-' and '+', which control the exact execution mode. */
static void
ParseRunOptions(
	char **pp,
	Boolean *out_shutUp, Boolean *out_errOff, Boolean *out_runAlways)
{
    char *p = *pp;
    *out_shutUp = FALSE;
    *out_errOff = FALSE;
    *out_runAlways = FALSE;

    for (;;) {
	if (*p == '@')
	    *out_shutUp = !DEBUG(LOUD);
	else if (*p == '-')
	    *out_errOff = TRUE;
	else if (*p == '+')
	    *out_runAlways = TRUE;
	else
	    break;
	p++;
    }

    pp_skip_whitespace(&p);

    *pp = p;
}

/* Escape a string for a double-quoted string literal in sh, csh and ksh. */
static char *
EscapeShellDblQuot(const char *cmd)
{
    size_t i, j;

    /* Worst that could happen is every char needs escaping. */
    char *esc = bmake_malloc(strlen(cmd) * 2 + 1);
    for (i = 0, j = 0; cmd[i] != '\0'; i++, j++) {
	if (cmd[i] == '$' || cmd[i] == '`' || cmd[i] == '\\' || cmd[i] == '"')
	    esc[j++] = '\\';
	esc[j] = cmd[i];
    }
    esc[j] = '\0';

    return esc;
}

static void
JobPrintf(Job *job, const char *fmt, const char *arg)
{
    if (DEBUG(JOB))
	debug_printf(fmt, arg);

    (void)fprintf(job->cmdFILE, fmt, arg);
    (void)fflush(job->cmdFILE);
}

static void
JobPrintln(Job *job, const char *line)
{
    JobPrintf(job, "%s\n", line);
}

/*-
 *-----------------------------------------------------------------------
 * JobPrintCommand  --
 *	Put out another command for the given job. If the command starts
 *	with an @ or a - we process it specially. In the former case,
 *	so long as the -s and -n flags weren't given to make, we stick
 *	a shell-specific echoOff command in the script. In the latter,
 *	we ignore errors for the entire job, unless the shell has error
 *	control.
 *	If the command is just "..." we take all future commands for this
 *	job to be commands to be executed once the entire graph has been
 *	made and return non-zero to signal that the end of the commands
 *	was reached. These commands are later attached to the .END
 *	node and executed by Job_End when all things are done.
 *
 * Side Effects:
 *	If the command begins with a '-' and the shell has no error control,
 *	the JOB_IGNERR flag is set in the job descriptor.
 *	numCommands is incremented if the command is actually printed.
 *-----------------------------------------------------------------------
 */
static void
JobPrintCommand(Job *job, char *cmd)
{
    const char *const cmdp = cmd;
    Boolean noSpecials;		/* true if we shouldn't worry about
				 * inserting special commands into
				 * the input stream. */
    Boolean shutUp;		/* true if we put a no echo command
				 * into the command file */
    Boolean errOff;		/* true if we turned error checking
				 * off before printing the command
				 * and need to turn it back on */
    Boolean runAlways;
    const char *cmdTemplate;	/* Template to use when printing the
				 * command */
    char *cmdStart;		/* Start of expanded command */
    char *escCmd = NULL;	/* Command with quotes/backticks escaped */

    noSpecials = !GNode_ShouldExecute(job->node);

    numCommands++;

    Var_Subst(cmd, job->node, VARE_WANTRES, &cmd);
    /* TODO: handle errors */
    cmdStart = cmd;

    cmdTemplate = "%s\n";

    ParseRunOptions(&cmd, &shutUp, &errOff, &runAlways);

    if (runAlways && noSpecials) {
	/*
	 * We're not actually executing anything...
	 * but this one needs to be - use compat mode just for it.
	 */
	Compat_RunCommand(cmdp, job->node);
	free(cmdStart);
	return;
    }

    /*
     * If the shell doesn't have error control the alternate echo'ing will
     * be done (to avoid showing additional error checking code)
     * and this will need the characters '$ ` \ "' escaped
     */

    if (!commandShell->hasErrCtl)
	escCmd = EscapeShellDblQuot(cmd);

    if (shutUp) {
	if (!(job->flags & JOB_SILENT) && !noSpecials &&
	    (commandShell->hasEchoCtl)) {
	    JobPrintln(job, commandShell->echoOff);
	} else {
	    if (commandShell->hasErrCtl)
		shutUp = FALSE;
	}
    }

    if (errOff) {
	if (!noSpecials) {
	    if (commandShell->hasErrCtl) {
		/*
		 * we don't want the error-control commands showing
		 * up either, so we turn off echoing while executing
		 * them. We could put another field in the shell
		 * structure to tell JobDoOutput to look for this
		 * string too, but why make it any more complex than
		 * it already is?
		 */
		if (!(job->flags & JOB_SILENT) && !shutUp &&
		    (commandShell->hasEchoCtl)) {
		    JobPrintln(job, commandShell->echoOff);
		    JobPrintln(job, commandShell->errOffOrExecIgnore);
		    JobPrintln(job,  commandShell->echoOn);
		} else {
		    JobPrintln(job, commandShell->errOffOrExecIgnore);
		}
	    } else if (commandShell->errOffOrExecIgnore &&
		       commandShell->errOffOrExecIgnore[0] != '\0') {
		/*
		 * The shell has no error control, so we need to be
		 * weird to get it to ignore any errors from the command.
		 * If echoing is turned on, we turn it off and use the
		 * errOnOrEcho template to echo the command. Leave echoing
		 * off so the user doesn't see the weirdness we go through
		 * to ignore errors. Set cmdTemplate to use the weirdness
		 * instead of the simple "%s\n" template.
		 */
		job->flags |= JOB_IGNERR;
		if (!(job->flags & JOB_SILENT) && !shutUp) {
		    if (commandShell->hasEchoCtl) {
			JobPrintln(job, commandShell->echoOff);
		    }
		    JobPrintf(job, commandShell->errOnOrEcho, escCmd);
		    shutUp = TRUE;
		} else {
		    if (!shutUp)
			JobPrintf(job, commandShell->errOnOrEcho, escCmd);
		}
		cmdTemplate = commandShell->errOffOrExecIgnore;
		/*
		 * The error ignoration (hee hee) is already taken care
		 * of by the errOffOrExecIgnore template, so pretend error
		 * checking is still on.
		 */
		errOff = FALSE;
	    } else {
		errOff = FALSE;
	    }
	} else {
	    errOff = FALSE;
	}
    } else {

	/*
	 * If errors are being checked and the shell doesn't have error control
	 * but does supply an errExit template, then set up commands to run
	 * through it.
	 */

	if (!commandShell->hasErrCtl && commandShell->errExit &&
	    commandShell->errExit[0] != '\0') {
	    if (!(job->flags & JOB_SILENT) && !shutUp) {
		if (commandShell->hasEchoCtl)
		    JobPrintln(job, commandShell->echoOff);
		JobPrintf(job, commandShell->errOnOrEcho, escCmd);
		shutUp = TRUE;
	    }
	    /* If it's a comment line or blank, treat as an ignored error */
	    if (escCmd[0] == commandShell->commentChar ||
		(escCmd[0] == '\0'))
		cmdTemplate = commandShell->errOffOrExecIgnore;
	    else
		cmdTemplate = commandShell->errExit;
	    errOff = FALSE;
	}
    }

    if (DEBUG(SHELL) && strcmp(shellName, "sh") == 0 &&
	!(job->flags & JOB_TRACED)) {
	JobPrintln(job, "set -x");
	job->flags |= JOB_TRACED;
    }

    JobPrintf(job, cmdTemplate, cmd);
    free(cmdStart);
    free(escCmd);
    if (errOff) {
	/*
	 * If echoing is already off, there's no point in issuing the
	 * echoOff command. Otherwise we issue it and pretend it was on
	 * for the whole command...
	 */
	if (!shutUp && !(job->flags & JOB_SILENT) && commandShell->hasEchoCtl) {
	    JobPrintln(job, commandShell->echoOff);
	    shutUp = TRUE;
	}
	JobPrintln(job, commandShell->errOnOrEcho);
    }
    if (shutUp && commandShell->hasEchoCtl)
	JobPrintln(job, commandShell->echoOn);
}

/* Print all commands to the shell file that is later executed.
 *
 * The special command "..." stops printing and saves the remaining commands
 * to be executed later. */
static void
JobPrintCommands(Job *job)
{
    StringListNode *ln;

    for (ln = job->node->commands->first; ln != NULL; ln = ln->next) {
	const char *cmd = ln->datum;

	if (strcmp(cmd, "...") == 0) {
	    job->node->type |= OP_SAVE_CMDS;
	    job->tailCmds = ln->next;
	    break;
	}

	JobPrintCommand(job, ln->datum);
    }
}

/* Save the delayed commands, to be executed when everything else is done. */
static void
JobSaveCommands(Job *job)
{
    StringListNode *node;

    for (node = job->tailCmds; node != NULL; node = node->next) {
	const char *cmd = node->datum;
	char *expanded_cmd;
	/* XXX: This Var_Subst is only intended to expand the dynamic
	 * variables such as .TARGET, .IMPSRC.  It is not intended to
	 * expand the other variables as well; see deptgt-end.mk. */
	(void)Var_Subst(cmd, job->node, VARE_WANTRES, &expanded_cmd);
	/* TODO: handle errors */
	Lst_Append(Targ_GetEndNode()->commands, expanded_cmd);
    }
}


/* Called to close both input and output pipes when a job is finished. */
static void
JobClosePipes(Job *job)
{
    clearfd(job);
    (void)close(job->outPipe);
    job->outPipe = -1;

    JobDoOutput(job, TRUE);
    (void)close(job->inPipe);
    job->inPipe = -1;
}

/* Do final processing for the given job including updating parent nodes and
 * starting new jobs as available/necessary.
 *
 * Deferred commands for the job are placed on the .END node.
 *
 * If there was a serious error (errors != 0; not an ignored one), no more
 * jobs will be started.
 *
 * Input:
 *	job		job to finish
 *	status		sub-why job went away
 */
static void
JobFinish (Job *job, WAIT_T status)
{
    Boolean done, return_job_token;

    DEBUG3(JOB, "JobFinish: %d [%s], status %d\n",
	   job->pid, job->node->name, status);

    if ((WIFEXITED(status) &&
	 ((WEXITSTATUS(status) != 0 && !(job->flags & JOB_IGNERR)))) ||
	WIFSIGNALED(status))
    {
	/*
	 * If it exited non-zero and either we're doing things our
	 * way or we're not ignoring errors, the job is finished.
	 * Similarly, if the shell died because of a signal
	 * the job is also finished. In these
	 * cases, finish out the job's output before printing the exit
	 * status...
	 */
	JobClosePipes(job);
	if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
	   (void)fclose(job->cmdFILE);
	   job->cmdFILE = NULL;
	}
	done = TRUE;
    } else if (WIFEXITED(status)) {
	/*
	 * Deal with ignored errors in -B mode. We need to print a message
	 * telling of the ignored error as well as to run the next command.
	 *
	 */
	done = WEXITSTATUS(status) != 0;
	JobClosePipes(job);
    } else {
	/*
	 * No need to close things down or anything.
	 */
	done = FALSE;
    }

    if (done) {
	if (WIFEXITED(status)) {
	    DEBUG2(JOB, "Process %d [%s] exited.\n",
		   job->pid, job->node->name);
	    if (WEXITSTATUS(status) != 0) {
		if (job->node != lastNode) {
		    MESSAGE(stdout, job->node);
		    lastNode = job->node;
		}
#ifdef USE_META
		if (useMeta) {
		    meta_job_error(job, job->node, job->flags, WEXITSTATUS(status));
		}
#endif
		if (!shouldDieQuietly(job->node, -1))
		    (void)printf("*** [%s] Error code %d%s\n",
				 job->node->name,
				 WEXITSTATUS(status),
				 (job->flags & JOB_IGNERR) ? " (ignored)" : "");
		if (job->flags & JOB_IGNERR) {
		    WAIT_STATUS(status) = 0;
		} else {
		    if (deleteOnError) {
			JobDeleteTarget(job->node);
		    }
		    PrintOnError(job->node, NULL);
		}
	    } else if (DEBUG(JOB)) {
		if (job->node != lastNode) {
		    MESSAGE(stdout, job->node);
		    lastNode = job->node;
		}
		(void)printf("*** [%s] Completed successfully\n",
				job->node->name);
	    }
	} else {
	    if (job->node != lastNode) {
		MESSAGE(stdout, job->node);
		lastNode = job->node;
	    }
	    (void)printf("*** [%s] Signal %d\n",
			job->node->name, WTERMSIG(status));
	    if (deleteOnError) {
		JobDeleteTarget(job->node);
	    }
	}
	(void)fflush(stdout);
    }

#ifdef USE_META
    if (useMeta) {
	int meta_status = meta_job_finish(job);
	if (meta_status != 0 && status == 0)
	    status = meta_status;
    }
#endif

    return_job_token = FALSE;

    Trace_Log(JOBEND, job);
    if (!(job->flags & JOB_SPECIAL)) {
	    if (WAIT_STATUS(status) != 0 ||
		(aborting == ABORT_ERROR) || aborting == ABORT_INTERRUPT)
	    return_job_token = TRUE;
    }

    if (aborting != ABORT_ERROR && aborting != ABORT_INTERRUPT &&
	(WAIT_STATUS(status) == 0)) {
	/*
	 * As long as we aren't aborting and the job didn't return a non-zero
	 * status that we shouldn't ignore, we call Make_Update to update
	 * the parents.
	 */
	JobSaveCommands(job);
	job->node->made = MADE;
	if (!(job->flags & JOB_SPECIAL))
	    return_job_token = TRUE;
	Make_Update(job->node);
	job->status = JOB_ST_FREE;
    } else if (WAIT_STATUS(status)) {
	errors++;
	job->status = JOB_ST_FREE;
    }

    if (errors > 0 && !opts.keepgoing && aborting != ABORT_INTERRUPT)
	aborting = ABORT_ERROR;	/* Prevent more jobs from getting started. */

    if (return_job_token)
	Job_TokenReturn();

    if (aborting == ABORT_ERROR && jobTokensRunning == 0)
	Finish(errors);
}

static void
TouchRegular(GNode *gn)
{
    const char *file = GNode_Path(gn);
    struct utimbuf times = { now, now };
    int fd;
    char c;

    if (utime(file, &times) >= 0)
	return;

    fd = open(file, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
	(void)fprintf(stderr, "*** couldn't touch %s: %s\n",
		      file, strerror(errno));
	(void)fflush(stderr);
	return;                /* XXX: What about propagating the error? */
    }

    /* Last resort: update the file's time stamps in the traditional way.
     * XXX: This doesn't work for empty files, which are sometimes used
     * as marker files. */
    if (read(fd, &c, 1) == 1) {
	(void)lseek(fd, 0, SEEK_SET);
	while (write(fd, &c, 1) == -1 && errno == EAGAIN)
	    continue;
    }
    (void)close(fd);		/* XXX: What about propagating the error? */
}

/* Touch the given target. Called by JobStart when the -t flag was given.
 *
 * The modification date of the file is changed.
 * If the file did not exist, it is created. */
void
Job_Touch(GNode *gn, Boolean silent)
{
    if (gn->type & (OP_JOIN|OP_USE|OP_USEBEFORE|OP_EXEC|OP_OPTIONAL|
	OP_SPECIAL|OP_PHONY)) {
	/* These are "virtual" targets and should not really be created. */
	return;
    }

    if (!silent || !GNode_ShouldExecute(gn)) {
	(void)fprintf(stdout, "touch %s\n", gn->name);
	(void)fflush(stdout);
    }

    if (!GNode_ShouldExecute(gn))
	return;

    if (gn->type & OP_ARCHV) {
	Arch_Touch(gn);
	return;
    }

    if (gn->type & OP_LIB) {
	Arch_TouchLib(gn);
	return;
    }

    TouchRegular(gn);
}

/* Make sure the given node has all the commands it needs.
 *
 * The node will have commands from the .DEFAULT rule added to it if it
 * needs them.
 *
 * Input:
 *	gn		The target whose commands need verifying
 *	abortProc	Function to abort with message
 *
 * Results:
 *	TRUE if the commands list is/was ok.
 */
Boolean
Job_CheckCommands(GNode *gn, void (*abortProc)(const char *, ...))
{
    if (GNode_IsTarget(gn))
	return TRUE;
    if (!Lst_IsEmpty(gn->commands))
	return TRUE;
    if ((gn->type & OP_LIB) && !Lst_IsEmpty(gn->children))
	return TRUE;

    /*
     * No commands. Look for .DEFAULT rule from which we might infer
     * commands.
     */
    if (defaultNode != NULL && !Lst_IsEmpty(defaultNode->commands) &&
	!(gn->type & OP_SPECIAL)) {
	/*
	 * The traditional Make only looks for a .DEFAULT if the node was
	 * never the target of an operator, so that's what we do too.
	 *
	 * The .DEFAULT node acts like a transformation rule, in that
	 * gn also inherits any attributes or sources attached to
	 * .DEFAULT itself.
	 */
	Make_HandleUse(defaultNode, gn);
	Var_Set(IMPSRC, GNode_VarTarget(gn), gn);
	return TRUE;
    }

    Dir_UpdateMTime(gn, FALSE);
    if (gn->mtime != 0 || (gn->type & OP_SPECIAL))
	return TRUE;

    /*
     * The node wasn't the target of an operator.  We have no .DEFAULT
     * rule to go on and the target doesn't already exist. There's
     * nothing more we can do for this branch. If the -k flag wasn't
     * given, we stop in our tracks, otherwise we just don't update
     * this node's parents so they never get examined.
     */

    if (gn->flags & FROM_DEPEND) {
	if (!Job_RunTarget(".STALE", gn->fname))
	    fprintf(stdout, "%s: %s, %d: ignoring stale %s for %s\n",
		    progname, gn->fname, gn->lineno, makeDependfile,
		    gn->name);
	return TRUE;
    }

    if (gn->type & OP_OPTIONAL) {
	(void)fprintf(stdout, "%s: don't know how to make %s (%s)\n",
		      progname, gn->name, "ignored");
	(void)fflush(stdout);
	return TRUE;
    }

    if (opts.keepgoing) {
	(void)fprintf(stdout, "%s: don't know how to make %s (%s)\n",
		      progname, gn->name, "continuing");
	(void)fflush(stdout);
	return FALSE;
    }

    abortProc("%s: don't know how to make %s. Stop", progname, gn->name);
    return FALSE;
}

/* Execute the shell for the given job.
 *
 * See Job_CatchOutput for handling the output of the shell. */
static void
JobExec(Job *job, char **argv)
{
    int cpid;			/* ID of new child */
    sigset_t mask;

    job->flags &= ~JOB_TRACED;

    if (DEBUG(JOB)) {
	int i;

	debug_printf("Running %s\n", job->node->name);
	debug_printf("\tCommand: ");
	for (i = 0; argv[i] != NULL; i++) {
	    debug_printf("%s ", argv[i]);
	}
	debug_printf("\n");
    }

    /*
     * Some jobs produce no output and it's disconcerting to have
     * no feedback of their running (since they produce no output, the
     * banner with their name in it never appears). This is an attempt to
     * provide that feedback, even if nothing follows it.
     */
    if ((lastNode != job->node) && !(job->flags & JOB_SILENT)) {
	MESSAGE(stdout, job->node);
	lastNode = job->node;
    }

    /* No interruptions until this job is on the `jobs' list */
    JobSigLock(&mask);

    /* Pre-emptively mark job running, pid still zero though */
    job->status = JOB_ST_RUNNING;

    cpid = vFork();
    if (cpid == -1)
	Punt("Cannot vfork: %s", strerror(errno));

    if (cpid == 0) {
	/* Child */
	sigset_t tmask;

#ifdef USE_META
	if (useMeta) {
	    meta_job_child(job);
	}
#endif
	/*
	 * Reset all signal handlers; this is necessary because we also
	 * need to unblock signals before we exec(2).
	 */
	JobSigReset();

	/* Now unblock signals */
	sigemptyset(&tmask);
	JobSigUnlock(&tmask);

	/*
	 * Must duplicate the input stream down to the child's input and
	 * reset it to the beginning (again). Since the stream was marked
	 * close-on-exec, we must clear that bit in the new input.
	 */
	if (dup2(fileno(job->cmdFILE), 0) == -1)
	    execDie("dup2", "job->cmdFILE");
	if (fcntl(0, F_SETFD, 0) == -1)
	    execDie("fcntl clear close-on-exec", "stdin");
	if (lseek(0, 0, SEEK_SET) == -1)
	    execDie("lseek to 0", "stdin");

	if (Always_pass_job_queue ||
	    (job->node->type & (OP_MAKE | OP_SUBMAKE))) {
	    /*
	     * Pass job token pipe to submakes.
	     */
	    if (fcntl(tokenWaitJob.inPipe, F_SETFD, 0) == -1)
		execDie("clear close-on-exec", "tokenWaitJob.inPipe");
	    if (fcntl(tokenWaitJob.outPipe, F_SETFD, 0) == -1)
		execDie("clear close-on-exec", "tokenWaitJob.outPipe");
	}

	/*
	 * Set up the child's output to be routed through the pipe
	 * we've created for it.
	 */
	if (dup2(job->outPipe, 1) == -1)
	    execDie("dup2", "job->outPipe");

	/*
	 * The output channels are marked close on exec. This bit was
	 * duplicated by the dup2(on some systems), so we have to clear
	 * it before routing the shell's error output to the same place as
	 * its standard output.
	 */
	if (fcntl(1, F_SETFD, 0) == -1)
	    execDie("clear close-on-exec", "stdout");
	if (dup2(1, 2) == -1)
	    execDie("dup2", "1, 2");

	/*
	 * We want to switch the child into a different process family so
	 * we can kill it and all its descendants in one fell swoop,
	 * by killing its process family, but not commit suicide.
	 */
#if defined(HAVE_SETPGID)
	(void)setpgid(0, getpid());
#else
#if defined(HAVE_SETSID)
	/* XXX: dsl - I'm sure this should be setpgrp()... */
	(void)setsid();
#else
	(void)setpgrp(0, getpid());
#endif
#endif

	Var_ExportVars();

	(void)execv(shellPath, argv);
	execDie("exec", shellPath);
    }

    /* Parent, continuing after the child exec */
    job->pid = cpid;

    Trace_Log(JOBSTART, job);

#ifdef USE_META
    if (useMeta) {
	meta_job_parent(job, cpid);
    }
#endif

    /*
     * Set the current position in the buffer to the beginning
     * and mark another stream to watch in the outputs mask
     */
    job->curPos = 0;

    watchfd(job);

    if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
	(void)fclose(job->cmdFILE);
	job->cmdFILE = NULL;
    }

    /*
     * Now the job is actually running, add it to the table.
     */
    if (DEBUG(JOB)) {
	debug_printf("JobExec(%s): pid %d added to jobs table\n",
		     job->node->name, job->pid);
	job_table_dump("job started");
    }
    JobSigUnlock(&mask);
}

/* Create the argv needed to execute the shell for a given job. */
static void
JobMakeArgv(Job *job, char **argv)
{
    int argc;
    static char args[10];	/* For merged arguments */

    argv[0] = UNCONST(shellName);
    argc = 1;

    if ((commandShell->exit && commandShell->exit[0] != '-') ||
	(commandShell->echo && commandShell->echo[0] != '-'))
    {
	/*
	 * At least one of the flags doesn't have a minus before it, so
	 * merge them together. Have to do this because the *(&(@*#*&#$#
	 * Bourne shell thinks its second argument is a file to source.
	 * Grrrr. Note the ten-character limitation on the combined arguments.
	 */
	(void)snprintf(args, sizeof args, "-%s%s",
		      ((job->flags & JOB_IGNERR) ? "" :
		       (commandShell->exit ? commandShell->exit : "")),
		      ((job->flags & JOB_SILENT) ? "" :
		       (commandShell->echo ? commandShell->echo : "")));

	if (args[1]) {
	    argv[argc] = args;
	    argc++;
	}
    } else {
	if (!(job->flags & JOB_IGNERR) && commandShell->exit) {
	    argv[argc] = UNCONST(commandShell->exit);
	    argc++;
	}
	if (!(job->flags & JOB_SILENT) && commandShell->echo) {
	    argv[argc] = UNCONST(commandShell->echo);
	    argc++;
	}
    }
    argv[argc] = NULL;
}

/*-
 *-----------------------------------------------------------------------
 * JobStart  --
 *	Start a target-creation process going for the target described
 *	by the graph node gn.
 *
 * Input:
 *	gn		target to create
 *	flags		flags for the job to override normal ones.
 *	previous	The previous Job structure for this node, if any.
 *
 * Results:
 *	JOB_ERROR if there was an error in the commands, JOB_FINISHED
 *	if there isn't actually anything left to do for the job and
 *	JOB_RUNNING if the job has been started.
 *
 * Side Effects:
 *	A new Job node is created and added to the list of running
 *	jobs. PMake is forked and a child shell created.
 *
 * NB: The return value is ignored by everyone.
 *-----------------------------------------------------------------------
 */
static JobStartResult
JobStart(GNode *gn, JobFlags flags)
{
    Job *job;			/* new job descriptor */
    char *argv[10];		/* Argument vector to shell */
    Boolean cmdsOK;		/* true if the nodes commands were all right */
    Boolean noExec;		/* Set true if we decide not to run the job */
    int tfd;			/* File descriptor to the temp file */

    for (job = job_table; job < job_table_end; job++) {
	if (job->status == JOB_ST_FREE)
	    break;
    }
    if (job >= job_table_end)
	Punt("JobStart no job slots vacant");

    memset(job, 0, sizeof *job);
    job->node = gn;
    job->tailCmds = NULL;
    job->status = JOB_ST_SET_UP;

    if (gn->type & OP_SPECIAL)
	flags |= JOB_SPECIAL;
    if (Targ_Ignore(gn))
	flags |= JOB_IGNERR;
    if (Targ_Silent(gn))
	flags |= JOB_SILENT;
    job->flags = flags;

    /*
     * Check the commands now so any attributes from .DEFAULT have a chance
     * to migrate to the node
     */
    cmdsOK = Job_CheckCommands(gn, Error);

    job->inPollfd = NULL;
    /*
     * If the -n flag wasn't given, we open up OUR (not the child's)
     * temporary file to stuff commands in it. The thing is rd/wr so we don't
     * need to reopen it to feed it to the shell. If the -n flag *was* given,
     * we just set the file to be stdout. Cute, huh?
     */
    if (((gn->type & OP_MAKE) && !opts.noRecursiveExecute) ||
	(!opts.noExecute && !opts.touchFlag)) {
	/*
	 * tfile is the name of a file into which all shell commands are
	 * put. It is removed before the child shell is executed, unless
	 * DEBUG(SCRIPT) is set.
	 */
	char *tfile;
	sigset_t mask;
	/*
	 * We're serious here, but if the commands were bogus, we're
	 * also dead...
	 */
	if (!cmdsOK) {
	    PrintOnError(gn, NULL);	/* provide some clue */
	    DieHorribly();
	}

	JobSigLock(&mask);
	tfd = mkTempFile(TMPPAT, &tfile);
	if (!DEBUG(SCRIPT))
	    (void)eunlink(tfile);
	JobSigUnlock(&mask);

	job->cmdFILE = fdopen(tfd, "w+");
	if (job->cmdFILE == NULL)
	    Punt("Could not fdopen %s", tfile);

	(void)fcntl(fileno(job->cmdFILE), F_SETFD, FD_CLOEXEC);
	/*
	 * Send the commands to the command file, flush all its buffers then
	 * rewind and remove the thing.
	 */
	noExec = FALSE;

#ifdef USE_META
	if (useMeta) {
	    meta_job_start(job, gn);
	    if (Targ_Silent(gn))	/* might have changed */
		job->flags |= JOB_SILENT;
	}
#endif
	/*
	 * We can do all the commands at once. hooray for sanity
	 */
	numCommands = 0;
	JobPrintCommands(job);

	/*
	 * If we didn't print out any commands to the shell script,
	 * there's not much point in executing the shell, is there?
	 */
	if (numCommands == 0) {
	    noExec = TRUE;
	}

	free(tfile);
    } else if (!GNode_ShouldExecute(gn)) {
	/*
	 * Not executing anything -- just print all the commands to stdout
	 * in one fell swoop. This will still set up job->tailCmds correctly.
	 */
	if (lastNode != gn) {
	    MESSAGE(stdout, gn);
	    lastNode = gn;
	}
	job->cmdFILE = stdout;
	/*
	 * Only print the commands if they're ok, but don't die if they're
	 * not -- just let the user know they're bad and keep going. It
	 * doesn't do any harm in this case and may do some good.
	 */
	if (cmdsOK)
	    JobPrintCommands(job);
	/*
	 * Don't execute the shell, thank you.
	 */
	noExec = TRUE;
    } else {
	/*
	 * Just touch the target and note that no shell should be executed.
	 * Set cmdFILE to stdout to make life easier. Check the commands, too,
	 * but don't die if they're no good -- it does no harm to keep working
	 * up the graph.
	 */
	job->cmdFILE = stdout;
	Job_Touch(gn, job->flags & JOB_SILENT);
	noExec = TRUE;
    }
    /* Just in case it isn't already... */
    (void)fflush(job->cmdFILE);

    /*
     * If we're not supposed to execute a shell, don't.
     */
    if (noExec) {
	if (!(job->flags & JOB_SPECIAL))
	    Job_TokenReturn();
	/*
	 * Unlink and close the command file if we opened one
	 */
	if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
	    (void)fclose(job->cmdFILE);
	    job->cmdFILE = NULL;
	}

	/*
	 * We only want to work our way up the graph if we aren't here because
	 * the commands for the job were no good.
	 */
	if (cmdsOK && aborting == ABORT_NONE) {
	    JobSaveCommands(job);
	    job->node->made = MADE;
	    Make_Update(job->node);
	}
	job->status = JOB_ST_FREE;
	return cmdsOK ? JOB_FINISHED : JOB_ERROR;
    }

    /*
     * Set up the control arguments to the shell. This is based on the flags
     * set earlier for this job.
     */
    JobMakeArgv(job, argv);

    /* Create the pipe by which we'll get the shell's output.  */
    JobCreatePipe(job, 3);

    JobExec(job, argv);
    return JOB_RUNNING;
}

/* Print the output of the shell command, skipping the noPrint command of
 * the shell, if any. */
static char *
JobOutput(Job *job, char *cp, char *endp)
{
    char *ecp;

    if (commandShell->noPrint == NULL || commandShell->noPrint[0] == '\0')
	return cp;

    while ((ecp = strstr(cp, commandShell->noPrint)) != NULL) {
	if (ecp != cp) {
	    *ecp = '\0';
	    /*
	     * The only way there wouldn't be a newline after
	     * this line is if it were the last in the buffer.
	     * however, since the non-printable comes after it,
	     * there must be a newline, so we don't print one.
	     */
	    (void)fprintf(stdout, "%s", cp);
	    (void)fflush(stdout);
	}
	cp = ecp + commandShell->noPrintLen;
	if (cp != endp) {
	    /*
	     * Still more to print, look again after skipping
	     * the whitespace following the non-printable
	     * command....
	     */
	    cp++;
	    pp_skip_whitespace(&cp);
	} else {
	    return cp;
	}
    }
    return cp;
}

/*
 * This function is called whenever there is something to read on the pipe.
 * We collect more output from the given job and store it in the job's
 * outBuf. If this makes up a line, we print it tagged by the job's
 * identifier, as necessary.
 *
 * In the output of the shell, the 'noPrint' lines are removed. If the
 * command is not alone on the line (the character after it is not \0 or
 * \n), we do print whatever follows it.
 *
 * Input:
 *	job		the job whose output needs printing
 *	finish		TRUE if this is the last time we'll be called
 *			for this job
 */
static void
JobDoOutput(Job *job, Boolean finish)
{
    Boolean gotNL = FALSE;	/* true if got a newline */
    Boolean fbuf;		/* true if our buffer filled up */
    size_t nr;			/* number of bytes read */
    size_t i;			/* auxiliary index into outBuf */
    size_t max;			/* limit for i (end of current data) */
    ssize_t nRead;		/* (Temporary) number of bytes read */

    /*
     * Read as many bytes as will fit in the buffer.
     */
again:
    gotNL = FALSE;
    fbuf = FALSE;

    nRead = read(job->inPipe, &job->outBuf[job->curPos],
		 JOB_BUFSIZE - job->curPos);
    if (nRead < 0) {
	if (errno == EAGAIN)
	    return;
	if (DEBUG(JOB)) {
	    perror("JobDoOutput(piperead)");
	}
	nr = 0;
    } else {
	nr = (size_t)nRead;
    }

    /*
     * If we hit the end-of-file (the job is dead), we must flush its
     * remaining output, so pretend we read a newline if there's any
     * output remaining in the buffer.
     * Also clear the 'finish' flag so we stop looping.
     */
    if (nr == 0 && job->curPos != 0) {
	job->outBuf[job->curPos] = '\n';
	nr = 1;
	finish = FALSE;
    } else if (nr == 0) {
	finish = FALSE;
    }

    /*
     * Look for the last newline in the bytes we just got. If there is
     * one, break out of the loop with 'i' as its index and gotNL set
     * TRUE.
     */
    max = job->curPos + nr;
    for (i = job->curPos + nr - 1; i >= job->curPos && i != (size_t)-1; i--) {
	if (job->outBuf[i] == '\n') {
	    gotNL = TRUE;
	    break;
	} else if (job->outBuf[i] == '\0') {
	    /*
	     * Why?
	     */
	    job->outBuf[i] = ' ';
	}
    }

    if (!gotNL) {
	job->curPos += nr;
	if (job->curPos == JOB_BUFSIZE) {
	    /*
	     * If we've run out of buffer space, we have no choice
	     * but to print the stuff. sigh.
	     */
	    fbuf = TRUE;
	    i = job->curPos;
	}
    }
    if (gotNL || fbuf) {
	/*
	 * Need to send the output to the screen. Null terminate it
	 * first, overwriting the newline character if there was one.
	 * So long as the line isn't one we should filter (according
	 * to the shell description), we print the line, preceded
	 * by a target banner if this target isn't the same as the
	 * one for which we last printed something.
	 * The rest of the data in the buffer are then shifted down
	 * to the start of the buffer and curPos is set accordingly.
	 */
	job->outBuf[i] = '\0';
	if (i >= job->curPos) {
	    char *cp;

	    cp = JobOutput(job, job->outBuf, &job->outBuf[i]);

	    /*
	     * There's still more in that thar buffer. This time, though,
	     * we know there's no newline at the end, so we add one of
	     * our own free will.
	     */
	    if (*cp != '\0') {
		if (!opts.beSilent && job->node != lastNode) {
		    MESSAGE(stdout, job->node);
		    lastNode = job->node;
		}
#ifdef USE_META
		if (useMeta) {
		    meta_job_output(job, cp, gotNL ? "\n" : "");
		}
#endif
		(void)fprintf(stdout, "%s%s", cp, gotNL ? "\n" : "");
		(void)fflush(stdout);
	    }
	}
	/*
	 * max is the last offset still in the buffer. Move any remaining
	 * characters to the start of the buffer and update the end marker
	 * curPos.
	 */
	if (i < max) {
	    (void)memmove(job->outBuf, &job->outBuf[i + 1], max - (i + 1));
	    job->curPos = max - (i + 1);
	} else {
	    assert(i == max);
	    job->curPos = 0;
	}
    }
    if (finish) {
	/*
	 * If the finish flag is true, we must loop until we hit
	 * end-of-file on the pipe. This is guaranteed to happen
	 * eventually since the other end of the pipe is now closed
	 * (we closed it explicitly and the child has exited). When
	 * we do get an EOF, finish will be set FALSE and we'll fall
	 * through and out.
	 */
	goto again;
    }
}

static void
JobRun(GNode *targ)
{
#if 0
    /*
     * Unfortunately it is too complicated to run .BEGIN, .END, and
     * .INTERRUPT job in the parallel job module.  As of 2020-09-25,
     * unit-tests/deptgt-end-jobs.mk hangs in an endless loop.
     *
     * Running these jobs in compat mode also guarantees that these
     * jobs do not overlap with other unrelated jobs.
     */
    List *lst = Lst_New();
    Lst_Append(lst, targ);
    (void)Make_Run(lst);
    Lst_Destroy(lst, NULL);
    JobStart(targ, JOB_SPECIAL);
    while (jobTokensRunning) {
	Job_CatchOutput();
    }
#else
    Compat_Make(targ, targ);
    if (targ->made == ERROR) {
	PrintOnError(targ, "\n\nStop.");
	exit(1);
    }
#endif
}

/* Handle the exit of a child. Called from Make_Make.
 *
 * The job descriptor is removed from the list of children.
 *
 * Notes:
 *	We do waits, blocking or not, according to the wisdom of our
 *	caller, until there are no more children to report. For each
 *	job, call JobFinish to finish things off.
 */
void
Job_CatchChildren(void)
{
    int pid;			/* pid of dead child */
    WAIT_T status;		/* Exit/termination status */

    /*
     * Don't even bother if we know there's no one around.
     */
    if (jobTokensRunning == 0)
	return;

    while ((pid = waitpid((pid_t) -1, &status, WNOHANG | WUNTRACED)) > 0) {
	DEBUG2(JOB, "Process %d exited/stopped status %x.\n", pid,
	  WAIT_STATUS(status));
	JobReapChild(pid, status, TRUE);
    }
}

/*
 * It is possible that wait[pid]() was called from elsewhere,
 * this lets us reap jobs regardless.
 */
void
JobReapChild(pid_t pid, WAIT_T status, Boolean isJobs)
{
    Job *job;			/* job descriptor for dead child */

    /*
     * Don't even bother if we know there's no one around.
     */
    if (jobTokensRunning == 0)
	return;

    job = JobFindPid(pid, JOB_ST_RUNNING, isJobs);
    if (job == NULL) {
	if (isJobs) {
	    if (!lurking_children)
		Error("Child (%d) status %x not in table?", pid, status);
	}
	return;				/* not ours */
    }
    if (WIFSTOPPED(status)) {
	DEBUG2(JOB, "Process %d (%s) stopped.\n", job->pid, job->node->name);
	if (!make_suspended) {
	    switch (WSTOPSIG(status)) {
	    case SIGTSTP:
		(void)printf("*** [%s] Suspended\n", job->node->name);
		break;
	    case SIGSTOP:
		(void)printf("*** [%s] Stopped\n", job->node->name);
		break;
	    default:
		(void)printf("*** [%s] Stopped -- signal %d\n",
			     job->node->name, WSTOPSIG(status));
	    }
	    job->suspended = TRUE;
	}
	(void)fflush(stdout);
	return;
    }

    job->status = JOB_ST_FINISHED;
    job->exit_status = WAIT_STATUS(status);

    JobFinish(job, status);
}

/* Catch the output from our children, if we're using pipes do so. Otherwise
 * just block time until we get a signal(most likely a SIGCHLD) since there's
 * no point in just spinning when there's nothing to do and the reaping of a
 * child can wait for a while. */
void
Job_CatchOutput(void)
{
    int nready;
    Job *job;
    unsigned int i;

    (void)fflush(stdout);

    /* The first fd in the list is the job token pipe */
    do {
	nready = poll(fds + 1 - wantToken, nfds - 1 + wantToken, POLL_MSEC);
    } while (nready < 0 && errno == EINTR);

    if (nready < 0)
	Punt("poll: %s", strerror(errno));

    if (nready > 0 && readyfd(&childExitJob)) {
	char token = 0;
	ssize_t count;
	count = read(childExitJob.inPipe, &token, 1);
	switch (count) {
	case 0:
	    Punt("unexpected eof on token pipe");
	case -1:
	    Punt("token pipe read: %s", strerror(errno));
	case 1:
	    if (token == DO_JOB_RESUME[0])
		/* Complete relay requested from our SIGCONT handler */
		JobRestartJobs();
	    break;
	default:
	    abort();
	}
	nready--;
    }

    Job_CatchChildren();
    if (nready == 0)
	return;

    for (i = npseudojobs * nfds_per_job(); i < nfds; i++) {
	if (!fds[i].revents)
	    continue;
	job = jobfds[i];
	if (job->status == JOB_ST_RUNNING)
	    JobDoOutput(job, FALSE);
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
	/*
	 * With meta mode, we may have activity on the job's filemon
	 * descriptor too, which at the moment is any pollfd other than
	 * job->inPollfd.
	 */
	if (useMeta && job->inPollfd != &fds[i]) {
	    if (meta_job_event(job) <= 0) {
		fds[i].events = 0; /* never mind */
	    }
	}
#endif
	if (--nready == 0)
	    return;
    }
}

/* Start the creation of a target. Basically a front-end for JobStart used by
 * the Make module. */
void
Job_Make(GNode *gn)
{
    (void)JobStart(gn, JOB_NONE);
}

void
Shell_Init(void)
{
    if (shellPath == NULL) {
	/*
	 * We are using the default shell, which may be an absolute
	 * path if DEFSHELL_CUSTOM is defined.
	 */
	shellName = commandShell->name;
#ifdef DEFSHELL_CUSTOM
	if (*shellName == '/') {
	    shellPath = shellName;
	    shellName = strrchr(shellPath, '/');
	    shellName++;
	} else
#endif
	shellPath = str_concat3(_PATH_DEFSHELLDIR, "/", shellName);
    }
    Var_SetWithFlags(".SHELL", shellPath, VAR_CMDLINE, VAR_SET_READONLY);
    if (commandShell->exit == NULL) {
	commandShell->exit = "";
    }
    if (commandShell->echo == NULL) {
	commandShell->echo = "";
    }
    if (commandShell->hasErrCtl && commandShell->exit[0] != '\0') {
	if (shellErrFlag &&
	    strcmp(commandShell->exit, &shellErrFlag[1]) != 0) {
	    free(shellErrFlag);
	    shellErrFlag = NULL;
	}
	if (!shellErrFlag) {
	    size_t n = strlen(commandShell->exit) + 2;

	    shellErrFlag = bmake_malloc(n);
	    if (shellErrFlag) {
		snprintf(shellErrFlag, n, "-%s", commandShell->exit);
	    }
	}
    } else if (shellErrFlag) {
	free(shellErrFlag);
	shellErrFlag = NULL;
    }
}

/* Return the string literal that is used in the current command shell
 * to produce a newline character. */
const char *
Shell_GetNewline(void)
{
    return commandShell->newline;
}

void
Job_SetPrefix(void)
{
    if (targPrefix) {
	free(targPrefix);
    } else if (!Var_Exists(MAKE_JOB_PREFIX, VAR_GLOBAL)) {
	Var_Set(MAKE_JOB_PREFIX, "---", VAR_GLOBAL);
    }

    (void)Var_Subst("${" MAKE_JOB_PREFIX "}",
		    VAR_GLOBAL, VARE_WANTRES, &targPrefix);
    /* TODO: handle errors */
}

/* Initialize the process module. */
void
Job_Init(void)
{
    Job_SetPrefix();
    /* Allocate space for all the job info */
    job_table = bmake_malloc((size_t)opts.maxJobs * sizeof *job_table);
    memset(job_table, 0, (size_t)opts.maxJobs * sizeof *job_table);
    job_table_end = job_table + opts.maxJobs;
    wantToken =	0;

    aborting = ABORT_NONE;
    errors = 0;

    lastNode = NULL;

    Always_pass_job_queue = GetBooleanVar(MAKE_ALWAYS_PASS_JOB_QUEUE,
				       Always_pass_job_queue);

    Job_error_token = GetBooleanVar(MAKE_JOB_ERROR_TOKEN, Job_error_token);


    /*
     * There is a non-zero chance that we already have children.
     * eg after 'make -f- <<EOF'
     * Since their termination causes a 'Child (pid) not in table' message,
     * Collect the status of any that are already dead, and suppress the
     * error message if there are any undead ones.
     */
    for (;;) {
	int rval, status;
	rval = waitpid((pid_t) -1, &status, WNOHANG);
	if (rval > 0)
	    continue;
	if (rval == 0)
	    lurking_children = TRUE;
	break;
    }

    Shell_Init();

    JobCreatePipe(&childExitJob, 3);

    /* Preallocate enough for the maximum number of jobs.  */
    fds = bmake_malloc(sizeof *fds *
	(npseudojobs + (size_t)opts.maxJobs) * nfds_per_job());
    jobfds = bmake_malloc(sizeof *jobfds *
	(npseudojobs + (size_t)opts.maxJobs) * nfds_per_job());

    /* These are permanent entries and take slots 0 and 1 */
    watchfd(&tokenWaitJob);
    watchfd(&childExitJob);

    sigemptyset(&caught_signals);
    /*
     * Install a SIGCHLD handler.
     */
    (void)bmake_signal(SIGCHLD, JobChildSig);
    sigaddset(&caught_signals, SIGCHLD);

#define ADDSIG(s,h)				\
    if (bmake_signal(s, SIG_IGN) != SIG_IGN) {	\
	sigaddset(&caught_signals, s);		\
	(void)bmake_signal(s, h);		\
    }

    /*
     * Catch the four signals that POSIX specifies if they aren't ignored.
     * JobPassSig will take care of calling JobInterrupt if appropriate.
     */
    ADDSIG(SIGINT, JobPassSig_int)
    ADDSIG(SIGHUP, JobPassSig_term)
    ADDSIG(SIGTERM, JobPassSig_term)
    ADDSIG(SIGQUIT, JobPassSig_term)

    /*
     * There are additional signals that need to be caught and passed if
     * either the export system wants to be told directly of signals or if
     * we're giving each job its own process group (since then it won't get
     * signals from the terminal driver as we own the terminal)
     */
    ADDSIG(SIGTSTP, JobPassSig_suspend)
    ADDSIG(SIGTTOU, JobPassSig_suspend)
    ADDSIG(SIGTTIN, JobPassSig_suspend)
    ADDSIG(SIGWINCH, JobCondPassSig)
    ADDSIG(SIGCONT, JobContinueSig)
#undef ADDSIG

    (void)Job_RunTarget(".BEGIN", NULL);
    /* Create the .END node now, even though no code in the unit tests
     * depends on it.  See also Targ_GetEndNode in Compat_Run. */
    (void)Targ_GetEndNode();
}

static void JobSigReset(void)
{
#define DELSIG(s)					\
    if (sigismember(&caught_signals, s)) {		\
	(void)bmake_signal(s, SIG_DFL);			\
    }

    DELSIG(SIGINT)
    DELSIG(SIGHUP)
    DELSIG(SIGQUIT)
    DELSIG(SIGTERM)
    DELSIG(SIGTSTP)
    DELSIG(SIGTTOU)
    DELSIG(SIGTTIN)
    DELSIG(SIGWINCH)
    DELSIG(SIGCONT)
#undef DELSIG
    (void)bmake_signal(SIGCHLD, SIG_DFL);
}

/* Find a shell in 'shells' given its name, or return NULL. */
static Shell *
FindShellByName(const char *name)
{
    Shell *sh = shells;
    const Shell *shellsEnd = sh + sizeof shells / sizeof shells[0];

    for (sh = shells; sh < shellsEnd; sh++) {
	if (strcmp(name, sh->name) == 0)
		return sh;
    }
    return NULL;
}

/*-
 *-----------------------------------------------------------------------
 * Job_ParseShell --
 *	Parse a shell specification and set up commandShell, shellPath
 *	and shellName appropriately.
 *
 * Input:
 *	line		The shell spec
 *
 * Results:
 *	FALSE if the specification was incorrect.
 *
 * Side Effects:
 *	commandShell points to a Shell structure (either predefined or
 *	created from the shell spec), shellPath is the full path of the
 *	shell described by commandShell, while shellName is just the
 *	final component of shellPath.
 *
 * Notes:
 *	A shell specification consists of a .SHELL target, with dependency
 *	operator, followed by a series of blank-separated words. Double
 *	quotes can be used to use blanks in words. A backslash escapes
 *	anything (most notably a double-quote and a space) and
 *	provides the functionality it does in C. Each word consists of
 *	keyword and value separated by an equal sign. There should be no
 *	unnecessary spaces in the word. The keywords are as follows:
 *	    name	Name of shell.
 *	    path	Location of shell.
 *	    quiet	Command to turn off echoing.
 *	    echo	Command to turn echoing on
 *	    filter	Result of turning off echoing that shouldn't be
 *			printed.
 *	    echoFlag	Flag to turn echoing on at the start
 *	    errFlag	Flag to turn error checking on at the start
 *	    hasErrCtl	True if shell has error checking control
 *	    newline	String literal to represent a newline char
 *	    check	Command to turn on error checking if hasErrCtl
 *			is TRUE or template of command to echo a command
 *			for which error checking is off if hasErrCtl is
 *			FALSE.
 *	    ignore	Command to turn off error checking if hasErrCtl
 *			is TRUE or template of command to execute a
 *			command so as to ignore any errors it returns if
 *			hasErrCtl is FALSE.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Job_ParseShell(char *line)
{
    Words	wordsList;
    char	**words;
    char	**argv;
    size_t	argc;
    char	*path;
    Shell	newShell;
    Boolean	fullSpec = FALSE;
    Shell	*sh;

    pp_skip_whitespace(&line);

    free(shellArgv);

    memset(&newShell, 0, sizeof newShell);

    /*
     * Parse the specification by keyword
     */
    wordsList = Str_Words(line, TRUE);
    words = wordsList.words;
    argc = wordsList.len;
    path = wordsList.freeIt;
    if (words == NULL) {
	Error("Unterminated quoted string [%s]", line);
	return FALSE;
    }
    shellArgv = path;

    for (path = NULL, argv = words; argc != 0; argc--, argv++) {
	char *arg = *argv;
	if (strncmp(arg, "path=", 5) == 0) {
	    path = arg + 5;
	} else if (strncmp(arg, "name=", 5) == 0) {
	    newShell.name = arg + 5;
	} else {
	    if (strncmp(arg, "quiet=", 6) == 0) {
		newShell.echoOff = arg + 6;
	    } else if (strncmp(arg, "echo=", 5) == 0) {
		newShell.echoOn = arg + 5;
	    } else if (strncmp(arg, "filter=", 7) == 0) {
		newShell.noPrint = arg + 7;
		newShell.noPrintLen = strlen(newShell.noPrint);
	    } else if (strncmp(arg, "echoFlag=", 9) == 0) {
		newShell.echo = arg + 9;
	    } else if (strncmp(arg, "errFlag=", 8) == 0) {
		newShell.exit = arg + 8;
	    } else if (strncmp(arg, "hasErrCtl=", 10) == 0) {
		char c = arg[10];
		newShell.hasErrCtl = c == 'Y' || c == 'y' ||
				     c == 'T' || c == 't';
	    } else if (strncmp(arg, "newline=", 8) == 0) {
		newShell.newline = arg + 8;
	    } else if (strncmp(arg, "check=", 6) == 0) {
		newShell.errOnOrEcho = arg + 6;
	    } else if (strncmp(arg, "ignore=", 7) == 0) {
		newShell.errOffOrExecIgnore = arg + 7;
	    } else if (strncmp(arg, "errout=", 7) == 0) {
		newShell.errExit = arg + 7;
	    } else if (strncmp(arg, "comment=", 8) == 0) {
		newShell.commentChar = arg[8];
	    } else {
		Parse_Error(PARSE_FATAL, "Unknown keyword \"%s\"", arg);
		free(words);
		return FALSE;
	    }
	    fullSpec = TRUE;
	}
    }

    if (path == NULL) {
	/*
	 * If no path was given, the user wants one of the pre-defined shells,
	 * yes? So we find the one s/he wants with the help of FindShellByName
	 * and set things up the right way. shellPath will be set up by
	 * Shell_Init.
	 */
	if (newShell.name == NULL) {
	    Parse_Error(PARSE_FATAL, "Neither path nor name specified");
	    free(words);
	    return FALSE;
	} else {
	    if ((sh = FindShellByName(newShell.name)) == NULL) {
		    Parse_Error(PARSE_WARNING, "%s: No matching shell",
				newShell.name);
		    free(words);
		    return FALSE;
	    }
	    commandShell = sh;
	    shellName = newShell.name;
	    if (shellPath) {
		/* Shell_Init has already been called!  Do it again. */
		free(UNCONST(shellPath));
		shellPath = NULL;
		Shell_Init();
	    }
	}
    } else {
	/*
	 * The user provided a path. If s/he gave nothing else (fullSpec is
	 * FALSE), try and find a matching shell in the ones we know of.
	 * Else we just take the specification at its word and copy it
	 * to a new location. In either case, we need to record the
	 * path the user gave for the shell.
	 */
	shellPath = path;
	path = strrchr(path, '/');
	if (path == NULL) {
	    path = UNCONST(shellPath);
	} else {
	    path++;
	}
	if (newShell.name != NULL) {
	    shellName = newShell.name;
	} else {
	    shellName = path;
	}
	if (!fullSpec) {
	    if ((sh = FindShellByName(shellName)) == NULL) {
		    Parse_Error(PARSE_WARNING, "%s: No matching shell",
				shellName);
		    free(words);
		    return FALSE;
	    }
	    commandShell = sh;
	} else {
	    commandShell = bmake_malloc(sizeof *commandShell);
	    *commandShell = newShell;
	}
	/* this will take care of shellErrFlag */
	Shell_Init();
    }

    if (commandShell->echoOn && commandShell->echoOff) {
	commandShell->hasEchoCtl = TRUE;
    }

    if (!commandShell->hasErrCtl) {
	if (commandShell->errOnOrEcho == NULL) {
	    commandShell->errOnOrEcho = "";
	}
	if (commandShell->errOffOrExecIgnore == NULL) {
	    commandShell->errOffOrExecIgnore = "%s\n";
	}
    }

    /*
     * Do not free up the words themselves, since they might be in use by the
     * shell specification.
     */
    free(words);
    return TRUE;
}

/* Handle the receipt of an interrupt.
 *
 * All children are killed. Another job will be started if the .INTERRUPT
 * target is defined.
 *
 * Input:
 *	runINTERRUPT	Non-zero if commands for the .INTERRUPT target
 *			should be executed
 *	signo		signal received
 */
static void
JobInterrupt(int runINTERRUPT, int signo)
{
    Job		*job;		/* job descriptor in that element */
    GNode	*interrupt;	/* the node describing the .INTERRUPT target */
    sigset_t	mask;
    GNode	*gn;

    aborting = ABORT_INTERRUPT;

    JobSigLock(&mask);

    for (job = job_table; job < job_table_end; job++) {
	if (job->status != JOB_ST_RUNNING)
	    continue;

	gn = job->node;

	JobDeleteTarget(gn);
	if (job->pid) {
	    DEBUG2(JOB, "JobInterrupt passing signal %d to child %d.\n",
		   signo, job->pid);
	    KILLPG(job->pid, signo);
	}
    }

    JobSigUnlock(&mask);

    if (runINTERRUPT && !opts.touchFlag) {
	interrupt = Targ_FindNode(".INTERRUPT");
	if (interrupt != NULL) {
	    opts.ignoreErrors = FALSE;
	    JobRun(interrupt);
	}
    }
    Trace_Log(MAKEINTR, NULL);
    exit(signo);
}

/* Do the final processing, i.e. run the commands attached to the .END target.
 *
 * Return the number of errors reported. */
int
Job_Finish(void)
{
    GNode *endNode = Targ_GetEndNode();
    if (!Lst_IsEmpty(endNode->commands) || !Lst_IsEmpty(endNode->children)) {
	if (errors) {
	    Error("Errors reported so .END ignored");
	} else {
	    JobRun(endNode);
	}
    }
    return errors;
}

/* Clean up any memory used by the jobs module. */
void
Job_End(void)
{
#ifdef CLEANUP
    free(shellArgv);
#endif
}

/* Waits for all running jobs to finish and returns.
 * Sets 'aborting' to ABORT_WAIT to prevent other jobs from starting. */
void
Job_Wait(void)
{
    aborting = ABORT_WAIT;
    while (jobTokensRunning != 0) {
	Job_CatchOutput();
    }
    aborting = ABORT_NONE;
}

/* Abort all currently running jobs without handling output or anything.
 * This function is to be called only in the event of a major error.
 * Most definitely NOT to be called from JobInterrupt.
 *
 * All children are killed, not just the firstborn. */
void
Job_AbortAll(void)
{
    Job		*job;	/* the job descriptor in that element */
    WAIT_T	foo;

    aborting = ABORT_ERROR;

    if (jobTokensRunning) {
	for (job = job_table; job < job_table_end; job++) {
	    if (job->status != JOB_ST_RUNNING)
		continue;
	    /*
	     * kill the child process with increasingly drastic signals to make
	     * darn sure it's dead.
	     */
	    KILLPG(job->pid, SIGINT);
	    KILLPG(job->pid, SIGKILL);
	}
    }

    /*
     * Catch as many children as want to report in at first, then give up
     */
    while (waitpid((pid_t) -1, &foo, WNOHANG) > 0)
	continue;
}

/* Tries to restart stopped jobs if there are slots available.
 * Called in process context in response to a SIGCONT. */
static void
JobRestartJobs(void)
{
    Job *job;

    for (job = job_table; job < job_table_end; job++) {
	if (job->status == JOB_ST_RUNNING &&
	    (make_suspended || job->suspended)) {
	    DEBUG1(JOB, "Restarting stopped job pid %d.\n", job->pid);
	    if (job->suspended) {
		    (void)printf("*** [%s] Continued\n", job->node->name);
		    (void)fflush(stdout);
	    }
	    job->suspended = FALSE;
	    if (KILLPG(job->pid, SIGCONT) != 0 && DEBUG(JOB)) {
		debug_printf("Failed to send SIGCONT to %d\n", job->pid);
	    }
	}
	if (job->status == JOB_ST_FINISHED)
	    /* Job exit deferred after calling waitpid() in a signal handler */
	    JobFinish(job, job->exit_status);
    }
    make_suspended = FALSE;
}

static void
watchfd(Job *job)
{
    if (job->inPollfd != NULL)
	Punt("Watching watched job");

    fds[nfds].fd = job->inPipe;
    fds[nfds].events = POLLIN;
    jobfds[nfds] = job;
    job->inPollfd = &fds[nfds];
    nfds++;
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
    if (useMeta) {
	fds[nfds].fd = meta_job_fd(job);
	fds[nfds].events = fds[nfds].fd == -1 ? 0 : POLLIN;
	jobfds[nfds] = job;
	nfds++;
    }
#endif
}

static void
clearfd(Job *job)
{
    size_t i;
    if (job->inPollfd == NULL)
	Punt("Unwatching unwatched job");
    i = (size_t)(job->inPollfd - fds);
    nfds--;
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
    if (useMeta) {
	/*
	 * Sanity check: there should be two fds per job, so the job's
	 * pollfd number should be even.
	 */
	assert(nfds_per_job() == 2);
	if (i % 2)
	    Punt("odd-numbered fd with meta");
	nfds--;
    }
#endif
    /*
     * Move last job in table into hole made by dead job.
     */
    if (nfds != i) {
	fds[i] = fds[nfds];
	jobfds[i] = jobfds[nfds];
	jobfds[i]->inPollfd = &fds[i];
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
	if (useMeta) {
	    fds[i + 1] = fds[nfds + 1];
	    jobfds[i + 1] = jobfds[nfds + 1];
	}
#endif
    }
    job->inPollfd = NULL;
}

static int
readyfd(Job *job)
{
    if (job->inPollfd == NULL)
	Punt("Polling unwatched job");
    return (job->inPollfd->revents & POLLIN) != 0;
}

/* Put a token (back) into the job pipe.
 * This allows a make process to start a build job. */
static void
JobTokenAdd(void)
{
    char tok = JOB_TOKENS[aborting], tok1;

    if (!Job_error_token && aborting == ABORT_ERROR) {
	if (jobTokensRunning == 0)
	    return;
	tok = '+';			/* no error token */
    }

    /* If we are depositing an error token flush everything else */
    while (tok != '+' && read(tokenWaitJob.inPipe, &tok1, 1) == 1)
	continue;

    DEBUG3(JOB, "(%d) aborting %d, deposit token %c\n",
	   getpid(), aborting, tok);
    while (write(tokenWaitJob.outPipe, &tok, 1) == -1 && errno == EAGAIN)
	continue;
}

/* Prep the job token pipe in the root make process. */
void
Job_ServerStart(int max_tokens, int jp_0, int jp_1)
{
    int i;
    char jobarg[64];

    if (jp_0 >= 0 && jp_1 >= 0) {
	/* Pipe passed in from parent */
	tokenWaitJob.inPipe = jp_0;
	tokenWaitJob.outPipe = jp_1;
	(void)fcntl(jp_0, F_SETFD, FD_CLOEXEC);
	(void)fcntl(jp_1, F_SETFD, FD_CLOEXEC);
	return;
    }

    JobCreatePipe(&tokenWaitJob, 15);

    snprintf(jobarg, sizeof jobarg, "%d,%d",
	    tokenWaitJob.inPipe, tokenWaitJob.outPipe);

    Var_Append(MAKEFLAGS, "-J", VAR_GLOBAL);
    Var_Append(MAKEFLAGS, jobarg, VAR_GLOBAL);

    /*
     * Preload the job pipe with one token per job, save the one
     * "extra" token for the primary job.
     *
     * XXX should clip maxJobs against PIPE_BUF -- if max_tokens is
     * larger than the write buffer size of the pipe, we will
     * deadlock here.
     */
    for (i = 1; i < max_tokens; i++)
	JobTokenAdd();
}

/* Return a withdrawn token to the pool. */
void
Job_TokenReturn(void)
{
    jobTokensRunning--;
    if (jobTokensRunning < 0)
	Punt("token botch");
    if (jobTokensRunning || JOB_TOKENS[aborting] != '+')
	JobTokenAdd();
}

/* Attempt to withdraw a token from the pool.
 *
 * If pool is empty, set wantToken so that we wake up when a token is
 * released.
 *
 * Returns TRUE if a token was withdrawn, and FALSE if the pool is currently
 * empty. */
Boolean
Job_TokenWithdraw(void)
{
    char tok, tok1;
    ssize_t count;

    wantToken = 0;
    DEBUG3(JOB, "Job_TokenWithdraw(%d): aborting %d, running %d\n",
	   getpid(), aborting, jobTokensRunning);

    if (aborting != ABORT_NONE || (jobTokensRunning >= opts.maxJobs))
	return FALSE;

    count = read(tokenWaitJob.inPipe, &tok, 1);
    if (count == 0)
	Fatal("eof on job pipe!");
    if (count < 0 && jobTokensRunning != 0) {
	if (errno != EAGAIN) {
	    Fatal("job pipe read: %s", strerror(errno));
	}
	DEBUG1(JOB, "(%d) blocked for token\n", getpid());
	return FALSE;
    }

    if (count == 1 && tok != '+') {
	/* make being abvorted - remove any other job tokens */
	DEBUG2(JOB, "(%d) aborted by token %c\n", getpid(), tok);
	while (read(tokenWaitJob.inPipe, &tok1, 1) == 1)
	    continue;
	/* And put the stopper back */
	while (write(tokenWaitJob.outPipe, &tok, 1) == -1 && errno == EAGAIN)
	    continue;
	if (shouldDieQuietly(NULL, 1))
	    exit(2);
	Fatal("A failure has been detected in another branch of the parallel make");
    }

    if (count == 1 && jobTokensRunning == 0)
	/* We didn't want the token really */
	while (write(tokenWaitJob.outPipe, &tok, 1) == -1 && errno == EAGAIN)
	    continue;

    jobTokensRunning++;
    DEBUG1(JOB, "(%d) withdrew token\n", getpid());
    return TRUE;
}

/* Run the named target if found. If a filename is specified, then set that
 * to the sources.
 *
 * Exits if the target fails. */
Boolean
Job_RunTarget(const char *target, const char *fname) {
    GNode *gn = Targ_FindNode(target);
    if (gn == NULL)
	return FALSE;

    if (fname)
	Var_Set(ALLSRC, fname, gn);

    JobRun(gn);
    if (gn->made == ERROR) {
	PrintOnError(gn, "\n\nStop.");
	exit(1);
    }
    return TRUE;
}

#ifdef USE_SELECT
int
emul_poll(struct pollfd *fd, int nfd, int timeout)
{
    fd_set rfds, wfds;
    int i, maxfd, nselect, npoll;
    struct timeval tv, *tvp;
    long usecs;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    maxfd = -1;
    for (i = 0; i < nfd; i++) {
	fd[i].revents = 0;

	if (fd[i].events & POLLIN)
	    FD_SET(fd[i].fd, &rfds);

	if (fd[i].events & POLLOUT)
	    FD_SET(fd[i].fd, &wfds);

	if (fd[i].fd > maxfd)
	    maxfd = fd[i].fd;
    }

    if (maxfd >= FD_SETSIZE) {
	Punt("Ran out of fd_set slots; "
	     "recompile with a larger FD_SETSIZE.");
    }

    if (timeout < 0) {
	tvp = NULL;
    } else {
	usecs = timeout * 1000;
	tv.tv_sec = usecs / 1000000;
	tv.tv_usec = usecs % 1000000;
	tvp = &tv;
    }

    nselect = select(maxfd + 1, &rfds, &wfds, NULL, tvp);

    if (nselect <= 0)
	return nselect;

    npoll = 0;
    for (i = 0; i < nfd; i++) {
	if (FD_ISSET(fd[i].fd, &rfds))
	    fd[i].revents |= POLLIN;

	if (FD_ISSET(fd[i].fd, &wfds))
	    fd[i].revents |= POLLOUT;

	if (fd[i].revents)
	    npoll++;
    }

    return npoll;
}
#endif /* USE_SELECT */
