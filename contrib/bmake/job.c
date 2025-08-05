/*	$NetBSD: job.c,v 1.519 2025/08/04 15:40:39 sjg Exp $	*/

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

/*
 * Create child processes and collect their output.
 *
 * Interface:
 *	Job_Init	Initialize this module and make the .BEGIN target.
 *
 *	Job_End		Clean up any memory used.
 *
 *	Job_Make	Start making the given target.
 *
 *	Job_CatchChildren
 *			Handle the termination of any children.
 *
 *	Job_CatchOutput
 *			Print any output the child processes have produced.
 *
 *	Job_ParseShell	Given a special dependency line with target '.SHELL',
 *			define the shell that is used for the creation
 *			commands in jobs mode.
 *
 *	Job_MakeDotEnd	Make the .END target. Must only be called when the
 *			job table is empty.
 *
 *	Job_AbortAll	Kill all currently running jobs, in an emergency.
 *
 *	Job_CheckCommands
 *			Add fallback commands to a target, if necessary.
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
#ifdef USE_META
# include "meta.h"
#endif
#include "pathnames.h"
#include "trace.h"

/*	"@(#)job.c	8.2 (Berkeley) 3/19/94"	*/
MAKE_RCSID("$NetBSD: job.c,v 1.519 2025/08/04 15:40:39 sjg Exp $");


#ifdef USE_SELECT
/*
 * Emulate poll() in terms of select().  This is not a complete
 * emulation but it is sufficient for make's purposes.
 */

#define poll emul_poll
#define pollfd emul_pollfd

struct emul_pollfd {
	int fd;
	short events;
	short revents;
};

#define POLLIN		0x0001
#define POLLOUT		0x0004

int emul_poll(struct pollfd *, int, int);
#endif

struct pollfd;


enum JobStatus {
	JOB_ST_FREE,		/* Job is available */
	JOB_ST_SET_UP,		/* Job is allocated but otherwise invalid */
	JOB_ST_RUNNING,		/* Job is running, pid valid */
	JOB_ST_FINISHED		/* Job is done (i.e. after SIGCHLD) */
};

static const char JobStatus_Name[][9] = {
	"free",
	"set-up",
	"running",
	"finished",
};

/*
 * A Job manages the shell commands that are run to create a single target.
 * Each job is run in a separate subprocess by a shell.  Several jobs can run
 * in parallel.
 *
 * The shell commands for the target are written to a temporary file,
 * then the shell is run with the temporary file as stdin, and the output
 * of that shell is captured via a pipe.
 *
 * When a job is finished, Make_Update updates all parents of the node
 * that was just remade, marking them as ready to be made next if all
 * other dependencies are finished as well.
 */
struct Job {
	/* The process ID of the shell running the commands */
	int pid;

	/* The target the child is making */
	GNode *node;

	/*
	 * If one of the shell commands is "...", all following commands are
	 * delayed until the .END node is made.  This list node points to the
	 * first of these commands, if any.
	 */
	StringListNode *tailCmds;

	/* This is where the shell commands go. */
	FILE *cmdFILE;

	int exit_status;	/* from wait4() in signal handler */

	enum JobStatus status;

	bool suspended;

	/* Ignore non-zero exits */
	bool ignerr;
	/* Output the command before or instead of running it. */
	bool echo;
	/* Target is a special one. */
	bool special;

	int inPipe;		/* Pipe for reading output from job */
	int outPipe;		/* Pipe for writing control commands */
	struct pollfd *inPollfd; /* pollfd associated with inPipe */

#define JOB_BUFSIZE	1024
	/* Buffer for storing the output of the job, line by line. */
	char outBuf[JOB_BUFSIZE + 1];
	size_t outBufLen;

#ifdef USE_META
	struct BuildMon bm;
#endif
};


/*
 * A shell defines how the commands are run.  All commands for a target are
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
 * errOn, errOff and runChkTmpl.
 *
 * In case a shell doesn't have error control, echoTmpl is a printf template
 * for echoing the command, should echoing be on; runIgnTmpl is another
 * printf template for executing the command while ignoring the return
 * status. Finally runChkTmpl is a printf template for running the command and
 * causing the shell to exit on error. If any of these strings are empty when
 * hasErrCtl is false, the command will be executed anyway as is, and if it
 * causes an error, so be it. Any templates set up to echo the command will
 * escape any '$ ` \ "' characters in the command string to avoid unwanted
 * shell code injection, the escaped command is safe to use in double quotes.
 *
 * The command-line flags "echo" and "exit" also control the behavior.  The
 * "echo" flag causes the shell to start echoing commands right away.  The
 * "exit" flag causes the shell to exit when an error is detected in one of
 * the commands.
 */
typedef struct Shell {

	/*
	 * The name of the shell. For Bourne and C shells, this is used only
	 * to find the shell description when used as the single source of a
	 * .SHELL target. For user-defined shells, this is the full path of
	 * the shell.
	 */
	const char *name;

	bool hasEchoCtl;	/* whether both echoOff and echoOn are there */
	const char *echoOff;	/* command to turn echoing off */
	const char *echoOn;	/* command to turn echoing back on */
	const char *noPrint;	/* text to skip when printing output from the
				 * shell. This is usually the same as echoOff */
	size_t noPrintLen;	/* length of noPrint command */

	bool hasErrCtl;		/* whether error checking can be controlled
				 * for individual commands */
	const char *errOn;	/* command to turn on error checking */
	const char *errOff;	/* command to turn off error checking */

	const char *echoTmpl;	/* template to echo a command */
	const char *runIgnTmpl;	/* template to run a command without error
				 * checking */
	const char *runChkTmpl;	/* template to run a command with error
				 * checking */

	/*
	 * A string literal that results in a newline character when it
	 * occurs outside of any 'quote' or "quote" characters.
	 */
	const char *newline;
	char commentChar;	/* character used by shell for comment lines */

	const char *echoFlag;	/* shell flag to echo commands */
	const char *errFlag;	/* shell flag to exit on error */
} Shell;

typedef struct CommandFlags {
	/* Whether to echo the command before or instead of running it. */
	bool echo;

	/* Run the command even in -n or -N mode. */
	bool always;

	/*
	 * true if we turned error checking off before writing the command to
	 * the commands file and need to turn it back on
	 */
	bool ignerr;
} CommandFlags;

/*
 * Write shell commands to a file.
 *
 * TODO: keep track of whether commands are echoed.
 * TODO: keep track of whether error checking is active.
 */
typedef struct ShellWriter {
	FILE *f;

	/* we've sent 'set -x' */
	bool xtraced;

} ShellWriter;

/*
 * FreeBSD: traditionally .MAKE is not required to
 * pass jobs queue to sub-makes.
 * Use .MAKE.ALWAYS_PASS_JOB_QUEUE=no to disable.
 */
#define MAKE_ALWAYS_PASS_JOB_QUEUE "${.MAKE.ALWAYS_PASS_JOB_QUEUE:U}"
static bool Always_pass_job_queue = true;
/*
 * FreeBSD: aborting entire parallel make isn't always
 * desired. When doing tinderbox for example, failure of
 * one architecture should not stop all.
 * We still want to bail on interrupt though.
 */
#define MAKE_JOB_ERROR_TOKEN "${MAKE_JOB_ERROR_TOKEN:U}"
static bool Job_error_token = true;

/* error handling variables */
static int job_errors = 0;	/* number of errors reported */
static enum {			/* Why is the make aborting? */
	ABORT_NONE,
	ABORT_ERROR,		/* Aborted because of an error */
	ABORT_INTERRUPT,	/* Aborted because it was interrupted */
	ABORT_WAIT		/* Waiting for jobs to finish */
} aborting = ABORT_NONE;
#define JOB_TOKENS "+EI+"	/* Token to requeue for each abort state */

static const char aborting_name[][6] = { "NONE", "ERROR", "INTR", "WAIT" };

/* Tracks the number of tokens currently "out" to build jobs. */
int jobTokensRunning = 0;

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
#else
#define DEFSHELL_INDEX_SH     0
#define DEFSHELL_INDEX_KSH    1
#define DEFSHELL_INDEX_CSH    2
#endif

#ifndef DEFSHELL_INDEX
#define DEFSHELL_INDEX 0	/* DEFSHELL_INDEX_CUSTOM or DEFSHELL_INDEX_SH */
#endif

static Shell shells[] = {
#ifdef DEFSHELL_CUSTOM
    /*
     * An sh-compatible shell with a non-standard name.
     *
     * Keep this in sync with the "sh" description below, but avoid
     * non-portable features that might not be supplied by all
     * sh-compatible shells.
     */
    {
	DEFSHELL_CUSTOM,	/* .name */
	false,			/* .hasEchoCtl */
	"",			/* .echoOff */
	"",			/* .echoOn */
	"",			/* .noPrint */
	0,			/* .noPrintLen */
	false,			/* .hasErrCtl */
	"",			/* .errOn */
	"",			/* .errOff */
	"echo \"%s\"\n",	/* .echoTmpl */
	"%s\n",			/* .runIgnTmpl */
	"{ %s \n} || exit $?\n", /* .runChkTmpl */
	"'\n'",			/* .newline */
	'#',			/* .commentChar */
	"",			/* .echoFlag */
	"",			/* .errFlag */
    },
#endif /* DEFSHELL_CUSTOM */
    /*
     * SH description. Echo control is also possible and, under
     * sun UNIX anyway, one can even control error checking.
     */
    {
	"sh",			/* .name */
	false,			/* .hasEchoCtl */
	"",			/* .echoOff */
	"",			/* .echoOn */
	"",			/* .noPrint */
	0,			/* .noPrintLen */
	false,			/* .hasErrCtl */
	"",			/* .errOn */
	"",			/* .errOff */
	"echo \"%s\"\n",	/* .echoTmpl */
	"%s\n",			/* .runIgnTmpl */
	"{ %s \n} || exit $?\n", /* .runChkTmpl */
	"'\n'",			/* .newline */
	'#',			/* .commentChar*/
#if defined(MAKE_NATIVE) && defined(__NetBSD__)
	/* XXX: -q is not really echoFlag, it's more like noEchoInSysFlag. */
	"q",			/* .echoFlag */
#else
	"",			/* .echoFlag */
#endif
	"",			/* .errFlag */
    },
    /*
     * KSH description.
     */
    {
	"ksh",			/* .name */
	true,			/* .hasEchoCtl */
	"set +v",		/* .echoOff */
	"set -v",		/* .echoOn */
	"set +v",		/* .noPrint */
	6,			/* .noPrintLen */
	false,			/* .hasErrCtl */
	"",			/* .errOn */
	"",			/* .errOff */
	"echo \"%s\"\n",	/* .echoTmpl */
	"%s\n",			/* .runIgnTmpl */
	"{ %s \n} || exit $?\n", /* .runChkTmpl */
	"'\n'",			/* .newline */
	'#',			/* .commentChar */
	"v",			/* .echoFlag */
	"",			/* .errFlag */
    },
    /*
     * CSH description. The csh can do echo control by playing
     * with the setting of the 'echo' shell variable. Sadly,
     * however, it is unable to do error control nicely.
     */
    {
	"csh",			/* .name */
	true,			/* .hasEchoCtl */
	"unset verbose",	/* .echoOff */
	"set verbose",		/* .echoOn */
	"unset verbose",	/* .noPrint */
	13,			/* .noPrintLen */
	false,			/* .hasErrCtl */
	"",			/* .errOn */
	"",			/* .errOff */
	"echo \"%s\"\n",	/* .echoTmpl */
	"csh -c \"%s || exit 0\"\n", /* .runIgnTmpl */
	"",			/* .runChkTmpl */
	"'\\\n'",		/* .newline */
	'#',			/* .commentChar */
	"v",			/* .echoFlag */
	"e",			/* .errFlag */
    }
};

/*
 * This is the shell to which we pass all commands in the Makefile.
 * It is set by the Job_ParseShell function.
 */
static Shell *shell = &shells[DEFSHELL_INDEX];
char *shellPath;		/* full pathname of executable image */
const char *shellName = NULL;	/* last component of shellPath */
char *shellErrFlag = NULL;
static char *shell_freeIt = NULL; /* Allocated memory for custom .SHELL */


static Job *job_table;		/* The structures that describe them */
static Job *job_table_end;	/* job_table + maxJobs */
static bool wantToken;
static bool lurking_children = false;
static bool make_suspended = false; /* Whether we've seen a SIGTSTP (etc) */

/*
 * Set of descriptors of pipes connected to
 * the output channels of children
 */
static struct pollfd *fds = NULL;
static Job **jobByFdIndex = NULL;
static nfds_t fdsLen = 0;
static void watchfd(Job *);
static void clearfd(Job *);

static char *targPrefix = NULL;	/* To identify a job change in the output. */

static Job tokenPoolJob;	/* token wait pseudo-job */

static Job childExitJob;	/* child exit pseudo-job */
#define CEJ_CHILD_EXITED '.'
#define CEJ_RESUME_JOBS 'R'

enum {
	npseudojobs = 2		/* number of pseudo-jobs */
};

static sigset_t caught_signals;	/* Set of signals we handle */
static volatile sig_atomic_t caught_sigchld;

static void CollectOutput(Job *, bool);
static void JobInterrupt(bool, int) MAKE_ATTR_DEAD;
static void JobSigReset(void);

static void
SwitchOutputTo(GNode *gn)
{
	/* The node for which output was most recently produced. */
	static GNode *lastNode = NULL;

	if (gn == lastNode)
		return;
	lastNode = gn;

	if (opts.maxJobs != 1 && targPrefix != NULL && targPrefix[0] != '\0')
		(void)fprintf(stdout, "%s %s ---\n", targPrefix, gn->name);
}

static unsigned
nfds_per_job(void)
{
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
	if (useMeta)
		return 2;
#endif
	return 1;
}

void
Job_FlagsToString(const Job *job, char *buf, size_t bufsize)
{
	snprintf(buf, bufsize, "%c%c%c",
	    job->ignerr ? 'i' : '-',
	    !job->echo ? 's' : '-',
	    job->special ? 'S' : '-');
}

#ifdef USE_META
struct BuildMon *
Job_BuildMon(Job *job)
{
	return &job->bm;
}
#endif

GNode *
Job_Node(Job *job)
{
	return job->node;
}

int
Job_Pid(Job *job)
{
	return job->pid;
}

static void
JobTable_Dump(const char *where)
{
	const Job *job;
	char flags[4];

	debug_printf("%s, job table:\n", where);
	for (job = job_table; job < job_table_end; job++) {
		Job_FlagsToString(job, flags, sizeof flags);
		debug_printf("job %d, status %s, flags %s, pid %d\n",
		    (int)(job - job_table), JobStatus_Name[job->status],
		    flags, job->pid);
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
	if (GNode_IsPrecious(gn))
		return;
	if (opts.noExecute)
		return;

	file = GNode_Path(gn);
	if (unlink_file(file) == 0)
		Error("*** %s removed", file);
}

/* Lock the jobs table and the jobs therein. */
static void
JobsTable_Lock(sigset_t *omaskp)
{
	if (sigprocmask(SIG_BLOCK, &caught_signals, omaskp) != 0)
		Punt("JobsTable_Lock: %s", strerror(errno));
}

/* Unlock the jobs table and the jobs therein. */
static void
JobsTable_Unlock(sigset_t *omaskp)
{
	(void)sigprocmask(SIG_SETMASK, omaskp, NULL);
}

static void
SetNonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		Punt("SetNonblocking.get: %s", strerror(errno));
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) == -1)
		Punt("SetNonblocking.set: %s", strerror(errno));
}

static void
SetCloseOnExec(int fd)
{
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		Punt("SetCloseOnExec: %s", strerror(errno));
}

static void
JobCreatePipe(Job *job, int minfd)
{
	int i;
	int pipe_fds[2];

	if (pipe(pipe_fds) == -1)
		Punt("JobCreatePipe: %s", strerror(errno));

	for (i = 0; i < 2; i++) {
		/* Avoid using low-numbered fds */
		int fd = fcntl(pipe_fds[i], F_DUPFD, minfd);
		if (fd != -1) {
			close(pipe_fds[i]);
			pipe_fds[i] = fd;
		}
	}

	job->inPipe = pipe_fds[0];
	job->outPipe = pipe_fds[1];

	SetCloseOnExec(job->inPipe);
	SetCloseOnExec(job->outPipe);

	/*
	 * We mark the input side of the pipe non-blocking; we poll(2) the
	 * pipe when we're waiting for a job token, but we might lose the
	 * race for the token when a new one becomes available, so the read
	 * from the pipe should not block.
	 */
	SetNonblocking(job->inPipe);
}

/* Pass the signal to each running job. */
static void
JobCondPassSig(int signo)
{
	Job *job;

	DEBUG1(JOB, "JobCondPassSig: signal %d\n", signo);

	for (job = job_table; job < job_table_end; job++) {
		if (job->status != JOB_ST_RUNNING)
			continue;
		DEBUG2(JOB, "JobCondPassSig passing signal %d to pid %d\n",
		    signo, job->pid);
		KILLPG(job->pid, signo);
	}
}

static void
WriteOrDie(int fd, char ch)
{
	if (write(fd, &ch, 1) != 1)
		execDie("write", "child");
}

static void
HandleSIGCHLD(int signo MAKE_ATTR_UNUSED)
{
	caught_sigchld = 1;
	/* Wake up from poll(). */
	WriteOrDie(childExitJob.outPipe, CEJ_CHILD_EXITED);
}

static void
HandleSIGCONT(int signo MAKE_ATTR_UNUSED)
{
	WriteOrDie(childExitJob.outPipe, CEJ_RESUME_JOBS);
}

/*
 * Pass a signal on to all jobs, then resend to ourselves.
 * We die by the same signal.
 */
MAKE_ATTR_DEAD static void
JobPassSig_int(int signo)
{
	/* Run .INTERRUPT target then exit */
	JobInterrupt(true, signo);
}

/*
 * Pass a signal on to all jobs, then resend to ourselves.
 * We die by the same signal.
 */
MAKE_ATTR_DEAD static void
JobPassSig_term(int signo)
{
	/* Dont run .INTERRUPT target then exit */
	JobInterrupt(false, signo);
}

static void
JobPassSig_suspend(int signo)
{
	sigset_t nmask, omask;
	struct sigaction act;

	/* Suppress job started/continued messages */
	make_suspended = true;

	/* Pass the signal onto every job */
	JobCondPassSig(signo);

	/*
	 * Send ourselves the signal now we've given the message to everyone
	 * else. Note we block everything else possible while we're getting
	 * the signal. This ensures that all our jobs get continued when we
	 * wake up before we take any other signal.
	 */
	sigfillset(&nmask);
	sigdelset(&nmask, signo);
	(void)sigprocmask(SIG_SETMASK, &nmask, &omask);

	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(signo, &act, NULL);

	DEBUG1(JOB, "JobPassSig_suspend passing signal %d to self\n", signo);

	(void)kill(getpid(), signo);

	/*
	 * We've been continued.
	 *
	 * A whole host of signals is going to happen!
	 * SIGCHLD for any processes that actually suspended themselves.
	 * SIGCHLD for any processes that exited while we were asleep.
	 * The SIGCONT that actually caused us to wake up.
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
JobFindPid(int pid, enum JobStatus status, bool isJobs)
{
	Job *job;

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == status && job->pid == pid)
			return job;
	}
	if (DEBUG(JOB) && isJobs)
		JobTable_Dump("no pid");
	return NULL;
}

/* Parse leading '@', '-' and '+', which control the exact execution mode. */
static void
ParseCommandFlags(char **pp, CommandFlags *out_cmdFlags)
{
	char *p = *pp;
	out_cmdFlags->echo = true;
	out_cmdFlags->ignerr = false;
	out_cmdFlags->always = false;

	for (;;) {
		if (*p == '@')
			out_cmdFlags->echo = DEBUG(LOUD);
		else if (*p == '-')
			out_cmdFlags->ignerr = true;
		else if (*p == '+')
			out_cmdFlags->always = true;
		else if (!ch_isspace(*p))
			/* Ignore whitespace for compatibility with GNU make */
			break;
		p++;
	}

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
		if (cmd[i] == '$' || cmd[i] == '`' || cmd[i] == '\\' ||
		    cmd[i] == '"')
			esc[j++] = '\\';
		esc[j] = cmd[i];
	}
	esc[j] = '\0';

	return esc;
}

static void
ShellWriter_WriteFmt(ShellWriter *wr, const char *fmt, const char *arg)
{
	DEBUG1(JOB, fmt, arg);

	(void)fprintf(wr->f, fmt, arg);
	if (wr->f == stdout)
		(void)fflush(wr->f);
}

static void
ShellWriter_WriteLine(ShellWriter *wr, const char *line)
{
	ShellWriter_WriteFmt(wr, "%s\n", line);
}

static void
ShellWriter_EchoOff(ShellWriter *wr)
{
	if (shell->hasEchoCtl)
		ShellWriter_WriteLine(wr, shell->echoOff);
}

static void
ShellWriter_EchoCmd(ShellWriter *wr, const char *escCmd)
{
	ShellWriter_WriteFmt(wr, shell->echoTmpl, escCmd);
}

static void
ShellWriter_EchoOn(ShellWriter *wr)
{
	if (shell->hasEchoCtl)
		ShellWriter_WriteLine(wr, shell->echoOn);
}

static void
ShellWriter_TraceOn(ShellWriter *wr)
{
	if (!wr->xtraced) {
		ShellWriter_WriteLine(wr, "set -x");
		wr->xtraced = true;
	}
}

static void
ShellWriter_ErrOff(ShellWriter *wr, bool echo)
{
	if (echo)
		ShellWriter_EchoOff(wr);
	ShellWriter_WriteLine(wr, shell->errOff);
	if (echo)
		ShellWriter_EchoOn(wr);
}

static void
ShellWriter_ErrOn(ShellWriter *wr, bool echo)
{
	if (echo)
		ShellWriter_EchoOff(wr);
	ShellWriter_WriteLine(wr, shell->errOn);
	if (echo)
		ShellWriter_EchoOn(wr);
}

/*
 * The shell has no built-in error control, so emulate error control by
 * enclosing each shell command in a template like "{ %s \n } || exit $?"
 * (configurable per shell).
 */
static void
JobWriteSpecialsEchoCtl(Job *job, ShellWriter *wr, CommandFlags *inout_cmdFlags,
			const char *escCmd, const char **inout_cmdTemplate)
{
	/* XXX: Why is the whole job modified at this point? */
	job->ignerr = true;

	if (job->echo && inout_cmdFlags->echo) {
		ShellWriter_EchoOff(wr);
		ShellWriter_EchoCmd(wr, escCmd);

		/*
		 * Leave echoing off so the user doesn't see the commands
		 * for toggling the error checking.
		 */
		inout_cmdFlags->echo = false;
	}
	*inout_cmdTemplate = shell->runIgnTmpl;

	/*
	 * The template runIgnTmpl already takes care of ignoring errors,
	 * so pretend error checking is still on.
	 * XXX: What effects does this have, and why is it necessary?
	 */
	inout_cmdFlags->ignerr = false;
}

static void
JobWriteSpecials(Job *job, ShellWriter *wr, const char *escCmd, bool run,
		 CommandFlags *inout_cmdFlags, const char **inout_cmdTemplate)
{
	if (!run)
		inout_cmdFlags->ignerr = false;
	else if (shell->hasErrCtl)
		ShellWriter_ErrOff(wr, job->echo && inout_cmdFlags->echo);
	else if (shell->runIgnTmpl != NULL && shell->runIgnTmpl[0] != '\0') {
		JobWriteSpecialsEchoCtl(job, wr, inout_cmdFlags, escCmd,
		    inout_cmdTemplate);
	} else
		inout_cmdFlags->ignerr = false;
}

/*
 * Write a shell command to the job's commands file, to be run later.
 *
 * If the command starts with '@' and neither the -s nor the -n flag was
 * given to make, stick a shell-specific echoOff command in the script.
 *
 * If the command starts with '-' and the shell has no error control (none
 * of the predefined shells has that), ignore errors for the rest of the job.
 *
 * XXX: Why ignore errors for the entire job?  This is documented in the
 * manual page, but without giving a rationale.
 *
 * If the command is just "...", attach all further commands of this job to
 * the .END node instead, see Job_Finish.
 */
static void
JobWriteCommand(Job *job, ShellWriter *wr, StringListNode *ln, const char *ucmd)
{
	bool run;

	CommandFlags cmdFlags;
	/* Template for writing a command to the shell file */
	const char *cmdTemplate;
	char *xcmd;		/* The expanded command */
	char *xcmdStart;
	char *escCmd;		/* xcmd escaped to be used in double quotes */

	run = GNode_ShouldExecute(job->node);

	xcmd = Var_SubstInTarget(ucmd, job->node);
	/* TODO: handle errors */
	xcmdStart = xcmd;

	cmdTemplate = "%s\n";

	ParseCommandFlags(&xcmd, &cmdFlags);

	/* The '+' command flag overrides the -n or -N options. */
	if (cmdFlags.always && !run) {
		/*
		 * We're not actually executing anything...
		 * but this one needs to be - use compat mode just for it.
		 */
		(void)Compat_RunCommand(ucmd, job->node, ln);
		free(xcmdStart);
		return;
	}

	/*
	 * If the shell doesn't have error control, the alternate echoing
	 * will be done (to avoid showing additional error checking code)
	 * and this needs some characters escaped.
	 */
	escCmd = shell->hasErrCtl ? NULL : EscapeShellDblQuot(xcmd);

	if (!cmdFlags.echo) {
		if (job->echo && run && shell->hasEchoCtl)
			ShellWriter_EchoOff(wr);
		else if (shell->hasErrCtl)
			cmdFlags.echo = true;
	}

	if (cmdFlags.ignerr) {
		JobWriteSpecials(job, wr, escCmd, run, &cmdFlags, &cmdTemplate);
	} else {

		/*
		 * If errors are being checked and the shell doesn't have
		 * error control but does supply an runChkTmpl template, then
		 * set up commands to run through it.
		 */

		if (!shell->hasErrCtl && shell->runChkTmpl != NULL &&
		    shell->runChkTmpl[0] != '\0') {
			if (job->echo && cmdFlags.echo) {
				ShellWriter_EchoOff(wr);
				ShellWriter_EchoCmd(wr, escCmd);
				cmdFlags.echo = false;
			}
			/*
			 * If it's a comment line or blank, avoid the possible
			 * syntax error generated by "{\n} || exit $?".
			 */
			cmdTemplate = escCmd[0] == shell->commentChar ||
				      escCmd[0] == '\0'
			    ? shell->runIgnTmpl
			    : shell->runChkTmpl;
			cmdFlags.ignerr = false;
		}
	}

	if (DEBUG(SHELL) && strcmp(shellName, "sh") == 0)
		ShellWriter_TraceOn(wr);

	ShellWriter_WriteFmt(wr, cmdTemplate, xcmd);
	free(xcmdStart);
	free(escCmd);

	if (cmdFlags.ignerr)
		ShellWriter_ErrOn(wr, cmdFlags.echo && job->echo);

	if (!cmdFlags.echo)
		ShellWriter_EchoOn(wr);
}

/*
 * Write all commands to the shell file that is later executed.
 *
 * The special command "..." stops writing and saves the remaining commands
 * to be executed later, when the target '.END' is made.
 *
 * Return whether at least one command was written to the shell file.
 */
static bool
JobWriteCommands(Job *job)
{
	StringListNode *ln;
	bool seen = false;
	ShellWriter wr;

	wr.f = job->cmdFILE;
	wr.xtraced = false;

	for (ln = job->node->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;

		if (strcmp(cmd, "...") == 0) {
			job->node->type |= OP_SAVE_CMDS;
			job->tailCmds = ln->next;
			break;
		}

		JobWriteCommand(job, &wr, ln, ln->datum);
		seen = true;
	}

	return seen;
}

/*
 * Save the delayed commands (those after '...'), to be executed later in
 * the '.END' node, when everything else is done.
 */
static void
JobSaveCommands(Job *job)
{
	StringListNode *ln;

	for (ln = job->tailCmds; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;
		char *expanded_cmd;
		/*
		 * XXX: This Var_Subst is only intended to expand the dynamic
		 * variables such as .TARGET, .IMPSRC.  It is not intended to
		 * expand the other variables as well; see deptgt-end.mk.
		 */
		expanded_cmd = Var_SubstInTarget(cmd, job->node);
		/* TODO: handle errors */
		Lst_Append(&Targ_GetEndNode()->commands, expanded_cmd);
		Parse_RegisterCommand(expanded_cmd);
	}
}


/* Close both input and output pipes when a job is finished. */
static void
JobClosePipes(Job *job)
{
	clearfd(job);
	(void)close(job->outPipe);
	job->outPipe = -1;

	CollectOutput(job, true);
	(void)close(job->inPipe);
	job->inPipe = -1;
}

static void
DebugFailedJob(const Job *job)
{
	const StringListNode *ln;

	if (!DEBUG(ERROR))
		return;

	debug_printf("\n");
	debug_printf("*** Failed target: %s\n", job->node->name);
	debug_printf("*** In directory: %s\n", curdir);
	debug_printf("*** Failed commands:\n");
	for (ln = job->node->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;
		debug_printf("\t%s\n", cmd);

		if (strchr(cmd, '$') != NULL) {
			char *xcmd = Var_Subst(cmd, job->node, VARE_EVAL);
			debug_printf("\t=> %s\n", xcmd);
			free(xcmd);
		}
	}
}

static void
JobFinishDoneExitedError(Job *job, WAIT_T *inout_status)
{
	SwitchOutputTo(job->node);
#ifdef USE_META
	if (useMeta) {
		meta_job_error(job, job->node,
		    job->ignerr, WEXITSTATUS(*inout_status));
	}
#endif
	if (!shouldDieQuietly(job->node, -1)) {
		DebugFailedJob(job);
		(void)printf("*** [%s] Error code %d%s\n",
		    job->node->name, WEXITSTATUS(*inout_status),
		    job->ignerr ? " (ignored)" : "");
	}

	if (job->ignerr)
		WAIT_STATUS(*inout_status) = 0;
	else {
		if (deleteOnError)
			JobDeleteTarget(job->node);
		PrintOnError(job->node, "\n");
	}
}

static void
JobFinishDoneExited(Job *job, WAIT_T *inout_status)
{
	DEBUG2(JOB, "Target %s, pid %d exited\n",
	    job->node->name, job->pid);

	if (WEXITSTATUS(*inout_status) != 0)
		JobFinishDoneExitedError(job, inout_status);
	else if (DEBUG(JOB)) {
		SwitchOutputTo(job->node);
		(void)printf("Target %s, pid %d exited successfully\n",
		    job->node->name, job->pid);
	}
}

static void
JobFinishDoneSignaled(Job *job, WAIT_T status)
{
	SwitchOutputTo(job->node);
	DebugFailedJob(job);
	(void)printf("*** [%s] Signal %d\n", job->node->name, WTERMSIG(status));
	if (deleteOnError)
		JobDeleteTarget(job->node);
}

static void
JobFinishDone(Job *job, WAIT_T *inout_status)
{
	if (WIFEXITED(*inout_status))
		JobFinishDoneExited(job, inout_status);
	else
		JobFinishDoneSignaled(job, *inout_status);

	(void)fflush(stdout);
}

/*
 * Finish the job, add deferred commands to the .END node, mark the job as
 * free, update parent nodes and start new jobs as available/necessary.
 */
static void
JobFinish (Job *job, WAIT_T status)
{
	bool done, return_job_token;

	DEBUG3(JOB, "JobFinish: target %s, pid %d, status %#x\n",
	    job->node->name, job->pid, status);

	if ((WIFEXITED(status) &&
	     ((WEXITSTATUS(status) != 0 && !job->ignerr))) ||
	    WIFSIGNALED(status)) {
		/* Finished because of an error. */

		JobClosePipes(job);
		if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
			if (fclose(job->cmdFILE) != 0)
				Punt("Cannot write shell script for \"%s\": %s",
				    job->node->name, strerror(errno));
			job->cmdFILE = NULL;
		}
		done = true;

	} else if (WIFEXITED(status)) {
		/*
		 * Deal with ignored errors in -B mode. We need to print a
		 * message telling of the ignored error as well as to run
		 * the next command.
		 */
		done = WEXITSTATUS(status) != 0;

		JobClosePipes(job);

	} else {
		/* No need to close things down or anything. */
		done = false;
	}

	if (done)
		JobFinishDone(job, &status);

#ifdef USE_META
	if (useMeta) {
		int meta_status = meta_job_finish(job);
		if (meta_status != 0 && status == 0)
			status = meta_status;
	}
#endif

	return_job_token = false;

	Trace_Log(JOBEND, job);
	if (!job->special) {
		if (WAIT_STATUS(status) != 0 ||
		    (aborting == ABORT_ERROR) || aborting == ABORT_INTERRUPT)
			return_job_token = true;
	}

	if (aborting != ABORT_ERROR && aborting != ABORT_INTERRUPT &&
	    (WAIT_STATUS(status) == 0)) {
		JobSaveCommands(job);
		job->node->made = MADE;
		if (!job->special)
			return_job_token = true;
		Make_Update(job->node);
		job->status = JOB_ST_FREE;
	} else if (status != 0) {
		job_errors++;
		job->status = JOB_ST_FREE;
	}

	if (job_errors > 0 && !opts.keepgoing && aborting != ABORT_INTERRUPT) {
		/* Prevent more jobs from getting started. */
		aborting = ABORT_ERROR;
	}

	if (return_job_token)
		TokenPool_Return();

	if (aborting == ABORT_ERROR && jobTokensRunning == 0) {
		if (shouldDieQuietly(NULL, -1))
			exit(2);
		Fatal("%d error%s", job_errors, job_errors == 1 ? "" : "s");
	}
}

static void
TouchRegular(GNode *gn)
{
	const char *file = GNode_Path(gn);
	struct utimbuf times;
	int fd;
	char c;

	times.actime = now;
	times.modtime = now;
	if (utime(file, &times) >= 0)
		return;

	fd = open(file, O_RDWR | O_CREAT, 0666);
	if (fd < 0) {
		(void)fprintf(stderr, "*** couldn't touch %s: %s\n",
		    file, strerror(errno));
		(void)fflush(stderr);
		return;		/* XXX: What about propagating the error? */
	}

	/*
	 * Last resort: update the file's time stamps in the traditional way.
	 * XXX: This doesn't work for empty files, which are sometimes used
	 * as marker files.
	 */
	if (read(fd, &c, 1) == 1) {
		(void)lseek(fd, 0, SEEK_SET);
		while (write(fd, &c, 1) == -1 && errno == EAGAIN)
			continue;
	}
	(void)close(fd);	/* XXX: What about propagating the error? */
}

/*
 * Touch the given target. Called by Job_Make when the -t flag was given.
 *
 * The modification date of the file is changed.
 * If the file did not exist, it is created.
 */
void
Job_Touch(GNode *gn, bool echo)
{
	if (gn->type &
	    (OP_JOIN | OP_USE | OP_USEBEFORE | OP_EXEC | OP_OPTIONAL |
	     OP_SPECIAL | OP_PHONY)) {
		/*
		 * These are "virtual" targets and should not really be
		 * created.
		 */
		return;
	}

	if (echo || !GNode_ShouldExecute(gn)) {
		(void)fprintf(stdout, "touch %s\n", gn->name);
		(void)fflush(stdout);
	}

	if (!GNode_ShouldExecute(gn))
		return;

	if (gn->type & OP_ARCHV)
		Arch_Touch(gn);
	else if (gn->type & OP_LIB)
		Arch_TouchLib(gn);
	else
		TouchRegular(gn);
}

/*
 * Make sure the given node has all the commands it needs.
 *
 * The node will have commands from the .DEFAULT rule added to it if it
 * needs them.
 *
 * Input:
 *	gn		The target whose commands need verifying
 *	abortProc	Function to abort with message
 *
 * Results:
 *	true if the commands are ok.
 */
bool
Job_CheckCommands(GNode *gn, void (*abortProc)(const char *, ...))
{
	if (GNode_IsTarget(gn))
		return true;
	if (!Lst_IsEmpty(&gn->commands))
		return true;
	if ((gn->type & OP_LIB) && !Lst_IsEmpty(&gn->children))
		return true;

	/*
	 * No commands. Look for .DEFAULT rule from which we might infer
	 * commands.
	 */
	if (defaultNode != NULL && !Lst_IsEmpty(&defaultNode->commands) &&
	    !(gn->type & OP_SPECIAL)) {
		/*
		 * The traditional Make only looks for a .DEFAULT if the node
		 * was never the target of an operator, so that's what we do
		 * too.
		 *
		 * The .DEFAULT node acts like a transformation rule, in that
		 * gn also inherits any attributes or sources attached to
		 * .DEFAULT itself.
		 */
		Make_HandleUse(defaultNode, gn);
		Var_Set(gn, IMPSRC, GNode_VarTarget(gn));
		return true;
	}

	Dir_UpdateMTime(gn, false);
	if (gn->mtime != 0 || (gn->type & OP_SPECIAL))
		return true;

	/*
	 * The node wasn't the target of an operator.  We have no .DEFAULT
	 * rule to go on and the target doesn't already exist. There's
	 * nothing more we can do for this branch. If the -k flag wasn't
	 * given, we stop in our tracks, otherwise we just don't update
	 * this node's parents so they never get examined.
	 */

	if (gn->flags.fromDepend) {
		if (!Job_RunTarget(".STALE", gn->fname))
			fprintf(stdout,
			    "%s: %s:%u: ignoring stale %s for %s\n",
			    progname, gn->fname, gn->lineno, makeDependfile,
			    gn->name);
		return true;
	}

	if (gn->type & OP_OPTIONAL) {
		(void)fprintf(stdout, "%s: don't know how to make %s (%s)\n",
		    progname, gn->name, "ignored");
		(void)fflush(stdout);
		return true;
	}

	if (opts.keepgoing) {
		(void)fprintf(stdout, "%s: don't know how to make %s (%s)\n",
		    progname, gn->name, "continuing");
		(void)fflush(stdout);
		return false;
	}

	abortProc("don't know how to make %s. Stop", gn->name);
	return false;
}

/*
 * Execute the shell for the given job.
 *
 * See Job_CatchOutput for handling the output of the shell.
 */
static void
JobExec(Job *job, char **argv)
{
	int cpid;		/* ID of new child */
	sigset_t mask;

	if (DEBUG(JOB)) {
		int i;

		debug_printf("Running %s\n", job->node->name);
		debug_printf("\tCommand:");
		for (i = 0; argv[i] != NULL; i++) {
			debug_printf(" %s", argv[i]);
		}
		debug_printf("\n");
	}

	/*
	 * Some jobs produce no output, and it's disconcerting to have
	 * no feedback of their running (since they produce no output, the
	 * banner with their name in it never appears). This is an attempt to
	 * provide that feedback, even if nothing follows it.
	 */
	if (job->echo)
		SwitchOutputTo(job->node);

	/* No interruptions until this job is in the jobs table. */
	JobsTable_Lock(&mask);

	/* Pre-emptively mark job running, pid still zero though */
	job->status = JOB_ST_RUNNING;

	Var_ReexportVars(job->node);
	Var_ExportStackTrace(job->node->name, NULL);

	cpid = FORK_FUNCTION();
	if (cpid == -1)
		Punt("fork: %s", strerror(errno));

	if (cpid == 0) {
		/* Child */
		sigset_t tmask;

#ifdef USE_META
		if (useMeta)
			meta_job_child(job);
#endif
		/*
		 * Reset all signal handlers; this is necessary because we
		 * also need to unblock signals before we exec(2).
		 */
		JobSigReset();

		sigemptyset(&tmask);
		JobsTable_Unlock(&tmask);

		if (dup2(fileno(job->cmdFILE), STDIN_FILENO) == -1)
			execDie("dup2", "job->cmdFILE");
		if (fcntl(STDIN_FILENO, F_SETFD, 0) == -1)
			execDie("clear close-on-exec", "stdin");
		if (lseek(STDIN_FILENO, 0, SEEK_SET) == -1)
			execDie("lseek to 0", "stdin");

		if (Always_pass_job_queue ||
		    (job->node->type & (OP_MAKE | OP_SUBMAKE))) {
			/* Pass job token pipe to submakes. */
			if (fcntl(tokenPoolJob.inPipe, F_SETFD, 0) == -1)
				execDie("clear close-on-exec",
				    "tokenPoolJob.inPipe");
			if (fcntl(tokenPoolJob.outPipe, F_SETFD, 0) == -1)
				execDie("clear close-on-exec",
				    "tokenPoolJob.outPipe");
		}

		if (dup2(job->outPipe, STDOUT_FILENO) == -1)
			execDie("dup2", "job->outPipe");

		/*
		 * The output channels are marked close on exec. This bit
		 * was duplicated by dup2 (on some systems), so we have
		 * to clear it before routing the shell's error output to
		 * the same place as its standard output.
		 */
		if (fcntl(STDOUT_FILENO, F_SETFD, 0) == -1)
			execDie("clear close-on-exec", "stdout");
		if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1)
			execDie("dup2", "1, 2");

		/*
		 * We want to switch the child into a different process
		 * family so we can kill it and all its descendants in
		 * one fell swoop, by killing its process family, but not
		 * commit suicide.
		 */
#if defined(HAVE_SETPGID)
		(void)setpgid(0, getpid());
#else
# if defined(HAVE_SETSID)
		/* XXX: dsl - I'm sure this should be setpgrp()... */
		(void)setsid();
# else
		(void)setpgrp(0, getpid());
# endif
#endif

		(void)execv(shellPath, argv);
		execDie("exec", shellPath);
	}

	/* Parent, continuing after the child exec */
	job->pid = cpid;

	Trace_Log(JOBSTART, job);

#ifdef USE_META
	if (useMeta)
		meta_job_parent(job, cpid);
#endif

	job->outBufLen = 0;

	watchfd(job);

	if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
		if (fclose(job->cmdFILE) != 0)
			Punt("Cannot write shell script for \"%s\": %s",
			    job->node->name, strerror(errno));
		job->cmdFILE = NULL;
	}

	if (DEBUG(JOB)) {
		debug_printf(
		    "JobExec: target %s, pid %d added to jobs table\n",
		    job->node->name, job->pid);
		JobTable_Dump("job started");
	}
	JobsTable_Unlock(&mask);
}

static void
BuildArgv(Job *job, char **argv)
{
	int argc;
	static char args[10];

	argv[0] = UNCONST(shellName);
	argc = 1;

	if ((shell->errFlag != NULL && shell->errFlag[0] != '-') ||
	    (shell->echoFlag != NULL && shell->echoFlag[0] != '-')) {
		/*
		 * At least one of the flags doesn't have a minus before it,
		 * so merge them together. Have to do this because the Bourne
		 * shell thinks its second argument is a file to source.
		 * Grrrr. Note the ten-character limitation on the combined
		 * arguments.
		 *
		 * TODO: Research until when the above comments were
		 * practically relevant.
		 */
		(void)snprintf(args, sizeof args, "-%s%s",
		    !job->ignerr && shell->errFlag != NULL
			? shell->errFlag : "",
		    job->echo && shell->echoFlag != NULL
			? shell->echoFlag : "");
		if (args[1] != '\0') {
			argv[argc] = args;
			argc++;
		}
	} else {
		if (!job->ignerr && shell->errFlag != NULL) {
			argv[argc] = UNCONST(shell->errFlag);
			argc++;
		}
		if (job->echo && shell->echoFlag != NULL) {
			argv[argc] = UNCONST(shell->echoFlag);
			argc++;
		}
	}
	argv[argc] = NULL;
}

static void
JobWriteShellCommands(Job *job, GNode *gn, bool *out_run)
{
	char fname[MAXPATHLEN];
	int fd;

	fd = Job_TempFile(NULL, fname, sizeof fname);

	job->cmdFILE = fdopen(fd, "w+");
	if (job->cmdFILE == NULL)
		Punt("Could not fdopen %s", fname);

	(void)fcntl(fd, F_SETFD, FD_CLOEXEC);

#ifdef USE_META
	if (useMeta) {
		meta_job_start(job, gn);
		if (gn->type & OP_SILENT)	/* might have changed */
			job->echo = false;
	}
#endif

	*out_run = JobWriteCommands(job);
}

void
Job_Make(GNode *gn)
{
	Job *job;
	char *argv[10];
	bool cmdsOK;		/* true if the nodes commands were all right */
	bool run;

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == JOB_ST_FREE)
			break;
	}
	if (job >= job_table_end)
		Punt("Job_Make no job slots vacant");

	memset(job, 0, sizeof *job);
	job->node = gn;
	job->tailCmds = NULL;
	job->status = JOB_ST_SET_UP;

	job->special = (gn->type & OP_SPECIAL) != OP_NONE;
	job->ignerr = opts.ignoreErrors || gn->type & OP_IGNORE;
	job->echo = !(opts.silent || gn->type & OP_SILENT);

	/*
	 * Check the commands now so any attributes from .DEFAULT have a
	 * chance to migrate to the node.
	 */
	cmdsOK = Job_CheckCommands(gn, Error);

	job->inPollfd = NULL;

	if (Lst_IsEmpty(&gn->commands)) {
		job->cmdFILE = stdout;
		run = false;

		if (!cmdsOK) {
			PrintOnError(gn, "\n");
			DieHorribly();
		}
	} else if (((gn->type & OP_MAKE) && !opts.noRecursiveExecute) ||
	    (!opts.noExecute && !opts.touch)) {
		int parseErrorsBefore;

		if (!cmdsOK) {
			PrintOnError(gn, "\n");
			DieHorribly();
		}

		parseErrorsBefore = parseErrors;
		JobWriteShellCommands(job, gn, &run);
		if (parseErrors != parseErrorsBefore)
			run = false;
		(void)fflush(job->cmdFILE);
	} else if (!GNode_ShouldExecute(gn)) {
		SwitchOutputTo(gn);
		job->cmdFILE = stdout;
		if (cmdsOK)
			JobWriteCommands(job);
		run = false;
		(void)fflush(job->cmdFILE);
	} else {
		Job_Touch(gn, job->echo);
		run = false;
	}

	if (!run) {
		if (!job->special)
			TokenPool_Return();

		if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
			(void)fclose(job->cmdFILE);
			job->cmdFILE = NULL;
		}

		if (cmdsOK && aborting == ABORT_NONE) {
			JobSaveCommands(job);
			job->node->made = MADE;
			Make_Update(job->node);
		}
		job->status = JOB_ST_FREE;
		return;
	}

	BuildArgv(job, argv);
	JobCreatePipe(job, 3);
	JobExec(job, argv);
}

/*
 * If the shell has an output filter (which only csh and ksh have by default),
 * print the output of the child process, skipping the noPrint text of the
 * shell.
 *
 * Return the part of the output that the calling function needs to output by
 * itself.
 */
static const char *
PrintFilteredOutput(Job *job, size_t len)
{
	const char *p = job->outBuf, *ep, *endp;

	if (shell->noPrint == NULL || shell->noPrint[0] == '\0')
		return p;

	endp = p + len;
	while ((ep = strstr(p, shell->noPrint)) != NULL && ep < endp) {
		if (ep > p) {
			if (!opts.silent)
				SwitchOutputTo(job->node);
			(void)fwrite(p, 1, (size_t)(ep - p), stdout);
			(void)fflush(stdout);
		}
		p = ep + shell->noPrintLen;
		if (p == endp)
			break;
		p++;		/* skip over the (XXX: assumed) newline */
		cpp_skip_whitespace(&p);
	}
	return p;
}

/*
 * Collect output from the job. Print any complete lines.
 *
 * In the output of the shell, the 'noPrint' lines are removed. If the
 * command is not alone on the line (the character after it is not \0 or
 * \n), we do print whatever follows it.
 *
 * If finish is true, collect all remaining output for the job.
 */
static void
CollectOutput(Job *job, bool finish)
{
	const char *p;
	size_t nr;		/* number of bytes read */
	size_t i;		/* auxiliary index into outBuf */
	size_t max;		/* limit for i (end of current data) */

again:
	nr = (size_t)read(job->inPipe, job->outBuf + job->outBufLen,
	    JOB_BUFSIZE - job->outBufLen);
	if (nr == (size_t)-1) {
		if (errno == EAGAIN)
			return;
		if (DEBUG(JOB))
			perror("CollectOutput(piperead)");
		nr = 0;
	}

	if (nr == 0)
		finish = false;	/* stop looping */

	if (nr == 0 && job->outBufLen > 0) {
		job->outBuf[job->outBufLen] = '\n';
		nr = 1;
	}

	max = job->outBufLen + nr;
	job->outBuf[max] = '\0';

	for (i = job->outBufLen; i < max; i++)
		if (job->outBuf[i] == '\0')
			job->outBuf[i] = ' ';

	for (i = max; i > job->outBufLen; i--)
		if (job->outBuf[i - 1] == '\n')
			break;

	if (i == job->outBufLen) {
		job->outBufLen = max;
		if (max < JOB_BUFSIZE)
			goto unfinished_line;
		i = max;
	}

	p = PrintFilteredOutput(job, i);
	if (*p != '\0') {
		if (!opts.silent)
			SwitchOutputTo(job->node);
#ifdef USE_META
		if (useMeta)
			meta_job_output(job, p, (i < max) ? i : max);
#endif
		(void)fwrite(p, 1, (size_t)(job->outBuf + i - p), stdout);
		(void)fflush(stdout);
	}
	memmove(job->outBuf, job->outBuf + i, max - i);
	job->outBufLen = max - i;

unfinished_line:
	if (finish)
		goto again;
}

static void
JobRun(GNode *target)
{
	/* Don't let these special jobs overlap with other unrelated jobs. */
	Compat_Make(target, target);
	if (GNode_IsError(target)) {
		PrintOnError(target, "\n\nStop.\n");
		exit(1);
	}
}

void
Job_CatchChildren(void)
{
	int pid;
	WAIT_T status;

	if (jobTokensRunning == 0)
		return;
	if (caught_sigchld == 0)
		return;
	caught_sigchld = 0;

	while ((pid = waitpid((pid_t)-1, &status, WNOHANG | WUNTRACED)) > 0) {
		DEBUG2(JOB,
		    "Process with pid %d exited/stopped with status %#x.\n",
		    pid, WAIT_STATUS(status));
		JobReapChild(pid, status, true);
	}
}

/*
 * It is possible that wait[pid]() was called from elsewhere,
 * this lets us reap jobs regardless.
 */
void
JobReapChild(pid_t pid, WAIT_T status, bool isJobs)
{
	Job *job;

	if (jobTokensRunning == 0)
		return;

	job = JobFindPid(pid, JOB_ST_RUNNING, isJobs);
	if (job == NULL) {
		if (isJobs && !lurking_children)
			Error("Child with pid %d and status %#x not in table?",
			    pid, status);
		return;
	}

	if (WIFSTOPPED(status)) {
		DEBUG2(JOB, "Process for target %s, pid %d stopped\n",
		    job->node->name, job->pid);
		if (!make_suspended) {
			switch (WSTOPSIG(status)) {
			case SIGTSTP:
				(void)printf("*** [%s] Suspended\n",
				    job->node->name);
				break;
			case SIGSTOP:
				(void)printf("*** [%s] Stopped\n",
				    job->node->name);
				break;
			default:
				(void)printf("*** [%s] Stopped -- signal %d\n",
				    job->node->name, WSTOPSIG(status));
			}
			job->suspended = true;
		}
		(void)fflush(stdout);
		return;
	}

	job->status = JOB_ST_FINISHED;
	job->exit_status = WAIT_STATUS(status);
	if (WIFEXITED(status))
		job->node->exit_status = WEXITSTATUS(status);

	JobFinish(job, status);
}

static void
Job_Continue(Job *job)
{
	DEBUG1(JOB, "Continuing pid %d\n", job->pid);
	if (job->suspended) {
		(void)printf("*** [%s] Continued\n", job->node->name);
		(void)fflush(stdout);
		job->suspended = false;
	}
	if (KILLPG(job->pid, SIGCONT) != 0)
		DEBUG1(JOB, "Failed to send SIGCONT to pid %d\n", job->pid);
}

static void
ContinueJobs(void)
{
	Job *job;

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == JOB_ST_RUNNING &&
		    (make_suspended || job->suspended))
			Job_Continue(job);
		else if (job->status == JOB_ST_FINISHED)
			JobFinish(job, job->exit_status);
	}
	make_suspended = false;
}

void
Job_CatchOutput(void)
{
	int nready;
	Job *job;
	unsigned i;

	(void)fflush(stdout);

	do {
		/* Maybe skip the job token pipe. */
		nfds_t skip = wantToken ? 0 : 1;
		nready = poll(fds + skip, fdsLen - skip, -1);
	} while (nready < 0 && errno == EINTR);

	if (nready < 0)
		Punt("poll: %s", strerror(errno));

	if (nready > 0 && childExitJob.inPollfd->revents & POLLIN) {
		char token;
		ssize_t count = read(childExitJob.inPipe, &token, 1);
		if (count != 1)
			Punt("childExitJob.read: %s",
			    count == 0 ? "EOF" : strerror(errno));
		if (token == CEJ_RESUME_JOBS)
			ContinueJobs();
		nready--;
	}

	Job_CatchChildren();
	if (nready == 0)
		return;

	for (i = npseudojobs * nfds_per_job(); i < fdsLen; i++) {
		if (fds[i].revents == 0)
			continue;
		job = jobByFdIndex[i];
		if (job->status == JOB_ST_RUNNING)
			CollectOutput(job, false);
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
		/*
		 * With meta mode, we may have activity on the job's filemon
		 * descriptor too, which at the moment is any pollfd other
		 * than job->inPollfd.
		 */
		if (useMeta && job->inPollfd != &fds[i]) {
			if (meta_job_event(job) <= 0)
				fds[i].events = 0;	/* never mind */
		}
#endif
		if (--nready == 0)
			return;
	}
}

static void
InitShellNameAndPath(void)
{
	shellName = shell->name;

#ifdef DEFSHELL_CUSTOM
	if (shellName[0] == '/') {
		shellPath = bmake_strdup(shellName);
		shellName = str_basename(shellPath);
		return;
	}
#endif
#ifdef DEFSHELL_PATH
	shellPath = bmake_strdup(DEFSHELL_PATH);
#else
	shellPath = str_concat3(_PATH_DEFSHELLDIR, "/", shellName);
#endif
}

void
Shell_Init(void)
{
	if (shellPath == NULL)
		InitShellNameAndPath();

	Var_SetWithFlags(SCOPE_CMDLINE, ".SHELL", shellPath,
			 VAR_SET_INTERNAL|VAR_SET_READONLY);
	if (shell->errFlag == NULL)
		shell->errFlag = "";
	if (shell->echoFlag == NULL)
		shell->echoFlag = "";
	if (shell->hasErrCtl && shell->errFlag[0] != '\0') {
		if (shellErrFlag != NULL &&
		    strcmp(shell->errFlag, &shellErrFlag[1]) != 0) {
			free(shellErrFlag);
			shellErrFlag = NULL;
		}
		if (shellErrFlag == NULL)
			shellErrFlag = str_concat2("-", shell->errFlag);
	} else if (shellErrFlag != NULL) {
		free(shellErrFlag);
		shellErrFlag = NULL;
	}
}

/* Return the shell string literal that results in a newline character. */
const char *
Shell_GetNewline(void)
{
	return shell->newline;
}

void
Job_SetPrefix(void)
{
	if (targPrefix != NULL)
		free(targPrefix);
	else if (!Var_Exists(SCOPE_GLOBAL, ".MAKE.JOB.PREFIX"))
		Global_Set(".MAKE.JOB.PREFIX", "---");

	targPrefix = Var_Subst("${.MAKE.JOB.PREFIX}",
	    SCOPE_GLOBAL, VARE_EVAL);
	/* TODO: handle errors */
}

static void
AddSig(int sig, SignalProc handler)
{
	if (bmake_signal(sig, SIG_IGN) != SIG_IGN) {
		sigaddset(&caught_signals, sig);
		(void)bmake_signal(sig, handler);
	}
}

void
Job_Init(void)
{
	Job_SetPrefix();

	job_table = bmake_malloc((size_t)opts.maxJobs * sizeof *job_table);
	memset(job_table, 0, (size_t)opts.maxJobs * sizeof *job_table);
	job_table_end = job_table + opts.maxJobs;
	wantToken = false;
	caught_sigchld = 0;

	aborting = ABORT_NONE;
	job_errors = 0;

	Always_pass_job_queue = GetBooleanExpr(MAKE_ALWAYS_PASS_JOB_QUEUE,
	    Always_pass_job_queue);

	Job_error_token = GetBooleanExpr(MAKE_JOB_ERROR_TOKEN, Job_error_token);


	/*
	 * There is a non-zero chance that we already have children,
	 * e.g. after 'make -f- <<EOF'.
	 * Since their termination causes a 'Child (pid) not in table'
	 * message, Collect the status of any that are already dead, and
	 * suppress the error message if there are any undead ones.
	 */
	for (;;) {
		int rval;
		WAIT_T status;

		rval = waitpid((pid_t)-1, &status, WNOHANG);
		if (rval > 0)
			continue;
		if (rval == 0)
			lurking_children = true;
		break;
	}

	Shell_Init();

	JobCreatePipe(&childExitJob, 3);

	{
		size_t nfds = (npseudojobs + (size_t)opts.maxJobs) *
			      nfds_per_job();
		fds = bmake_malloc(sizeof *fds * nfds);
		jobByFdIndex = bmake_malloc(sizeof *jobByFdIndex * nfds);
	}

	/* These are permanent entries and take slots 0 and 1 */
	watchfd(&tokenPoolJob);
	watchfd(&childExitJob);

	sigemptyset(&caught_signals);
	(void)bmake_signal(SIGCHLD, HandleSIGCHLD);
	sigaddset(&caught_signals, SIGCHLD);

	/* Handle the signals specified by POSIX. */
	AddSig(SIGINT, JobPassSig_int);
	AddSig(SIGHUP, JobPassSig_term);
	AddSig(SIGTERM, JobPassSig_term);
	AddSig(SIGQUIT, JobPassSig_term);

	/*
	 * These signals need to be passed to the jobs, as each job has its
	 * own process group and thus the terminal driver doesn't forward the
	 * signals itself.
	 */
	AddSig(SIGTSTP, JobPassSig_suspend);
	AddSig(SIGTTOU, JobPassSig_suspend);
	AddSig(SIGTTIN, JobPassSig_suspend);
	AddSig(SIGWINCH, JobCondPassSig);
	AddSig(SIGCONT, HandleSIGCONT);

	(void)Job_RunTarget(".BEGIN", NULL);
	/* Create the .END node, see Targ_GetEndNode in Compat_MakeAll. */
	(void)Targ_GetEndNode();
}

static void
DelSig(int sig)
{
	if (sigismember(&caught_signals, sig) != 0)
		(void)bmake_signal(sig, SIG_DFL);
}

static void
JobSigReset(void)
{
	DelSig(SIGINT);
	DelSig(SIGHUP);
	DelSig(SIGQUIT);
	DelSig(SIGTERM);
	DelSig(SIGTSTP);
	DelSig(SIGTTOU);
	DelSig(SIGTTIN);
	DelSig(SIGWINCH);
	DelSig(SIGCONT);
	(void)bmake_signal(SIGCHLD, SIG_DFL);
}

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

/*
 * Parse a shell specification and set up 'shell', shellPath and
 * shellName appropriately.
 *
 * Input:
 *	line		The shell spec
 *
 * Results:
 *	Returns false if the specification was incorrect.
 *	If successful, 'shell' is usable, shellPath is the full path of the
 *	shell described by 'shell', and shellName is the final component of
 *	shellPath.
 *
 * Notes:
 *	A shell specification has the form ".SHELL: keyword=value...". Double
 *	quotes can be used to enclose blanks in words. A backslash escapes
 *	anything (most notably a double-quote and a space) and
 *	provides the usual escape sequences from C. There should be no
 *	unnecessary spaces in the word. The keywords are:
 *	    name	Name of shell.
 *	    path	Location of shell.
 *	    quiet	Command to turn off echoing.
 *	    echo	Command to turn echoing on
 *	    filter	The output from the shell command that turns off
 *			echoing, to be filtered from the final output.
 *	    echoFlag	Flag to turn echoing on at the start.
 *	    errFlag	Flag to turn error checking on at the start.
 *	    hasErrCtl	True if the shell has error checking control.
 *	    newline	String literal to represent a newline character.
 *	    check	If hasErrCtl is true: The command to turn on error
 *			checking. If hasErrCtl is false: The template for a
 *			shell command that echoes a command for which error
 *			checking is off.
 *	    ignore	If hasErrCtl is true: The command to turn off error
 *			checking. If hasErrCtl is false: The template for a
 *			shell command that executes a command so as to ignore
 *			any errors it returns.
 */
bool
Job_ParseShell(char *line)
{
	Words wordsList;
	char **words;
	char **argv;
	size_t argc;
	char *path;
	Shell newShell;
	bool fullSpec = false;
	Shell *sh;

	/* XXX: don't use line as an iterator variable */
	pp_skip_whitespace(&line);

	free(shell_freeIt);

	memset(&newShell, 0, sizeof newShell);

	wordsList = Str_Words(line, true);
	words = wordsList.words;
	argc = wordsList.len;
	path = wordsList.freeIt;
	if (words == NULL) {
		Error("Unterminated quoted string [%s]", line);
		return false;
	}
	shell_freeIt = path;

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
				newShell.echoFlag = arg + 9;
			} else if (strncmp(arg, "errFlag=", 8) == 0) {
				newShell.errFlag = arg + 8;
			} else if (strncmp(arg, "hasErrCtl=", 10) == 0) {
				char c = arg[10];
				newShell.hasErrCtl = c == 'Y' || c == 'y' ||
						     c == 'T' || c == 't';
			} else if (strncmp(arg, "newline=", 8) == 0) {
				newShell.newline = arg + 8;
			} else if (strncmp(arg, "check=", 6) == 0) {
				/*
				 * Before 2020-12-10, these two variables had
				 * been a single variable.
				 */
				newShell.errOn = arg + 6;
				newShell.echoTmpl = arg + 6;
			} else if (strncmp(arg, "ignore=", 7) == 0) {
				/*
				 * Before 2020-12-10, these two variables had
				 * been a single variable.
				 */
				newShell.errOff = arg + 7;
				newShell.runIgnTmpl = arg + 7;
			} else if (strncmp(arg, "errout=", 7) == 0) {
				newShell.runChkTmpl = arg + 7;
			} else if (strncmp(arg, "comment=", 8) == 0) {
				newShell.commentChar = arg[8];
			} else {
				Parse_Error(PARSE_FATAL,
				    "Unknown keyword \"%s\"", arg);
				free(words);
				return false;
			}
			fullSpec = true;
		}
	}

	if (path == NULL) {
		if (newShell.name == NULL) {
			Parse_Error(PARSE_FATAL,
			    "Neither path nor name specified");
			free(words);
			return false;
		} else {
			if ((sh = FindShellByName(newShell.name)) == NULL) {
				Parse_Error(PARSE_WARNING,
				    "%s: No matching shell", newShell.name);
				free(words);
				return false;
			}
			shell = sh;
			shellName = newShell.name;
			if (shellPath != NULL) {
				free(shellPath);
				shellPath = NULL;
				Shell_Init();
			}
		}
	} else {
		free(shellPath);
		shellPath = bmake_strdup(path);
		shellName = newShell.name != NULL ? newShell.name
		    : str_basename(path);
		if (!fullSpec) {
			if ((sh = FindShellByName(shellName)) == NULL) {
				Parse_Error(PARSE_WARNING,
				    "%s: No matching shell", shellName);
				free(words);
				return false;
			}
			shell = sh;
		} else {
			shell = bmake_malloc(sizeof *shell);
			*shell = newShell;
		}
		/* This will take care of shellErrFlag. */
		Shell_Init();
	}

	if (shell->echoOn != NULL && shell->echoOff != NULL)
		shell->hasEchoCtl = true;

	if (!shell->hasErrCtl) {
		if (shell->echoTmpl == NULL)
			shell->echoTmpl = "";
		if (shell->runIgnTmpl == NULL)
			shell->runIgnTmpl = "%s\n";
	}

	/*
	 * Do not free up the words themselves, since they may be in use
	 * by the shell specification.
	 */
	free(words);
	return true;
}

/*
 * After receiving an interrupt signal, terminate all child processes and if
 * necessary make the .INTERRUPT target.
 */
static void
JobInterrupt(bool runINTERRUPT, int signo)
{
	Job *job;
	sigset_t mask;

	aborting = ABORT_INTERRUPT;

	JobsTable_Lock(&mask);

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == JOB_ST_RUNNING && job->pid != 0) {
			DEBUG2(JOB,
			    "JobInterrupt passing signal %d to child %d.\n",
			    signo, job->pid);
			KILLPG(job->pid, signo);
		}
	}

	for (job = job_table; job < job_table_end; job++) {
		if (job->status == JOB_ST_RUNNING && job->pid != 0) {
			int status;
			(void)waitpid(job->pid, &status, 0);
			JobDeleteTarget(job->node);
		}
	}

	JobsTable_Unlock(&mask);

	if (runINTERRUPT && !opts.touch) {
		GNode *dotInterrupt = Targ_FindNode(".INTERRUPT");
		if (dotInterrupt != NULL) {
			opts.ignoreErrors = false;
			JobRun(dotInterrupt);
		}
	}
	Trace_Log(MAKEINTR, NULL);
	exit(signo);		/* XXX: why signo? */
}

/* Make the .END target, returning the number of job-related errors. */
int
Job_MakeDotEnd(void)
{
	GNode *dotEnd = Targ_GetEndNode();
	if (!Lst_IsEmpty(&dotEnd->commands) ||
	    !Lst_IsEmpty(&dotEnd->children)) {
		if (job_errors != 0)
			Error("Errors reported so .END ignored");
		else
			JobRun(dotEnd);
	}
	return job_errors;
}

#ifdef CLEANUP
void
Job_End(void)
{
	free(shell_freeIt);
}
#endif

/* Waits for all running jobs to finish. */
void
Job_Wait(void)
{
	aborting = ABORT_WAIT;	/* Prevent other jobs from starting. */
	while (jobTokensRunning != 0)
		Job_CatchOutput();
	aborting = ABORT_NONE;
}

/*
 * Abort all currently running jobs without handling output or anything.
 * This function is to be called only in the event of a major error.
 * Most definitely NOT to be called from JobInterrupt.
 */
void
Job_AbortAll(void)
{
	Job *job;
	WAIT_T status;

	aborting = ABORT_ERROR;

	if (jobTokensRunning != 0) {
		for (job = job_table; job < job_table_end; job++) {
			if (job->status != JOB_ST_RUNNING)
				continue;
			KILLPG(job->pid, SIGINT);
			KILLPG(job->pid, SIGKILL);
		}
	}

	while (waitpid((pid_t)-1, &status, WNOHANG) > 0)
		continue;
}

static void
watchfd(Job *job)
{
	if (job->inPollfd != NULL)
		Punt("Watching watched job");

	fds[fdsLen].fd = job->inPipe;
	fds[fdsLen].events = POLLIN;
	jobByFdIndex[fdsLen] = job;
	job->inPollfd = &fds[fdsLen];
	fdsLen++;
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
	if (useMeta) {
		fds[fdsLen].fd = meta_job_fd(job);
		fds[fdsLen].events = fds[fdsLen].fd == -1 ? 0 : POLLIN;
		jobByFdIndex[fdsLen] = job;
		fdsLen++;
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
	fdsLen--;
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
	if (useMeta) {
		assert(nfds_per_job() == 2);
		if (i % 2 != 0)
			Punt("odd-numbered fd with meta");
		fdsLen--;
	}
#endif
	/* Move last job in table into hole made by dead job. */
	if (fdsLen != i) {
		fds[i] = fds[fdsLen];
		jobByFdIndex[i] = jobByFdIndex[fdsLen];
		jobByFdIndex[i]->inPollfd = &fds[i];
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
		if (useMeta) {
			fds[i + 1] = fds[fdsLen + 1];
			jobByFdIndex[i + 1] = jobByFdIndex[fdsLen + 1];
		}
#endif
	}
	job->inPollfd = NULL;
}

int
Job_TempFile(const char *pattern, char *tfile, size_t tfile_sz)
{
	int fd;
	sigset_t mask;

	JobsTable_Lock(&mask);
	fd = mkTempFile(pattern, tfile, tfile_sz);
	if (tfile != NULL && !DEBUG(SCRIPT))
		unlink(tfile);
	JobsTable_Unlock(&mask);

	return fd;
}

static void
TokenPool_Write(char tok)
{
	if (write(tokenPoolJob.outPipe, &tok, 1) != 1)
		Punt("Cannot write \"%c\" to the token pool: %s",
		    tok, strerror(errno));
}

/*
 * Put a token (back) into the job token pool.
 * This allows a make process to start a build job.
 */
static void
TokenPool_Add(void)
{
	char tok = JOB_TOKENS[aborting], tok1;

	/*
	 * FreeBSD: do not deposit an error token
	 * unless Job_error_token is true.
	 */
	if (!Job_error_token && aborting == ABORT_ERROR) {
		if (jobTokensRunning == 0)
			return;
		tok = '+';              /* no error token */
	}

	/* If we are depositing an error token, flush everything else. */
	while (tok != '+' && read(tokenPoolJob.inPipe, &tok1, 1) == 1)
		continue;

	DEBUG3(JOB, "TokenPool_Add: pid %d, aborting %s, token %c\n",
	    getpid(), aborting_name[aborting], tok);
	TokenPool_Write(tok);
}

static void
TokenPool_InitClient(int tokenPoolReader, int tokenPoolWriter)
{
	tokenPoolJob.inPipe = tokenPoolReader;
	tokenPoolJob.outPipe = tokenPoolWriter;
	(void)fcntl(tokenPoolReader, F_SETFD, FD_CLOEXEC);
	(void)fcntl(tokenPoolWriter, F_SETFD, FD_CLOEXEC);
}

/* Prepare the job token pipe in the root make process. */
static void
TokenPool_InitServer(int maxJobTokens)
{
	int i;
	char jobarg[64];

	JobCreatePipe(&tokenPoolJob, 15);

	snprintf(jobarg, sizeof jobarg, "%d,%d",
	    tokenPoolJob.inPipe, tokenPoolJob.outPipe);

	Global_Append(MAKEFLAGS, "-J");
	Global_Append(MAKEFLAGS, jobarg);

	/*
	 * Preload the job pipe with one token per job, save the one
	 * "extra" token for the primary job.
	 */
	SetNonblocking(tokenPoolJob.outPipe);
	for (i = 1; i < maxJobTokens; i++)
		TokenPool_Add();
}

void
TokenPool_Init(int maxJobTokens, int tokenPoolReader, int tokenPoolWriter)
{
	if (tokenPoolReader >= 0 && tokenPoolWriter >= 0)
		TokenPool_InitClient(tokenPoolReader, tokenPoolWriter);
	else
		TokenPool_InitServer(maxJobTokens);
}

/* Return a taken token to the pool. */
void
TokenPool_Return(void)
{
	jobTokensRunning--;
	if (jobTokensRunning < 0)
		Punt("token botch");
	if (jobTokensRunning != 0 || JOB_TOKENS[aborting] != '+')
		TokenPool_Add();
}

/*
 * Attempt to take a token from the pool.
 *
 * If the pool is empty, set wantToken so that we wake up when a token is
 * released.
 *
 * Returns true if a token was taken, and false if the pool is currently
 * empty.
 */
bool
TokenPool_Take(void)
{
	char tok, tok1;
	ssize_t count;

	wantToken = false;
	DEBUG3(JOB, "TokenPool_Take: pid %d, aborting %s, running %d\n",
	    getpid(), aborting_name[aborting], jobTokensRunning);

	if (aborting != ABORT_NONE || jobTokensRunning >= opts.maxJobs)
		return false;

	count = read(tokenPoolJob.inPipe, &tok, 1);
	if (count == 0)
		Fatal("eof on job pipe");
	if (count < 0 && jobTokensRunning != 0) {
		if (errno != EAGAIN)
			Fatal("job pipe read: %s", strerror(errno));
		DEBUG1(JOB, "TokenPool_Take: pid %d blocked for token\n",
		    getpid());
		wantToken = true;
		return false;
	}

	if (count == 1 && tok != '+') {
		/* make being aborted - remove any other job tokens */
		DEBUG2(JOB, "TokenPool_Take: pid %d aborted by token %c\n",
		    getpid(), tok);
		while (read(tokenPoolJob.inPipe, &tok1, 1) == 1)
			continue;
		/* And put the stopper back */
		TokenPool_Write(tok);
		if (shouldDieQuietly(NULL, 1)) {
			Job_Wait();
			exit(6);
		}
		Fatal("A failure has been detected "
		      "in another branch of the parallel make");
	}

	if (count == 1 && jobTokensRunning == 0)
		/* We didn't want the token really */
		TokenPool_Write(tok);

	jobTokensRunning++;
	DEBUG1(JOB, "TokenPool_Take: pid %d took a token\n", getpid());
	return true;
}

/* Make the named target if found, exit if the target fails. */
bool
Job_RunTarget(const char *target, const char *fname)
{
	GNode *gn = Targ_FindNode(target);
	if (gn == NULL)
		return false;

	if (fname != NULL)
		Var_Set(gn, ALLSRC, fname);

	JobRun(gn);
	return true;
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
#endif				/* USE_SELECT */
