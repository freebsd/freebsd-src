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
 * @(#)job.c	8.2 (Berkeley) 3/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef OLD_JOKE
#define	OLD_JOKE 0
#endif /* OLD_JOKE */

/*-
 * job.c --
 *	handle the creation etc. of our child processes.
 *
 * Interface:
 *	Job_Make	Start the creation of the given target.
 *
 *	Job_CatchChildren
 *			Check for and handle the termination of any children.
 *			This must be called reasonably frequently to keep the
 *			whole make going at a decent clip, since job table
 *			entries aren't removed until their process is caught
 *			this way. Its single argument is TRUE if the function
 *			should block waiting for a child to terminate.
 *
 *	Job_CatchOutput	Print any output our children have produced. Should
 *			also be called fairly frequently to keep the user
 *			informed of what's going on. If no output is waiting,
 *			it will block for a time given by the SEL_* constants,
 *			below, or until output is ready.
 *
 *	Job_Init	Called to intialize this module. in addition, any
 *			commands attached to the .BEGIN target are executed
 *			before this function returns. Hence, the makefile must
 *			have been parsed before this function is called.
 *
 *	Job_Full	Return TRUE if the job table is filled.
 *
 *	Job_Empty	Return TRUE if the job table is completely empty.
 *
 *	Job_ParseShell	Given the line following a .SHELL target, parse the
 *			line as a shell specification. Returns FAILURE if the
 *			spec was incorrect.
 *
 *	Job_Finish	Perform any final processing which needs doing. This
 *			includes the execution of any commands which have
 *			been/were attached to the .END target. It should only
 *			be called when the job table is empty.
 *
 *	Job_AbortAll	Abort all currently running jobs. It doesn't handle
 *			output or do anything for the jobs, just kills them.
 *			It should only be called in an emergency, as it were.
 *
 *	Job_CheckCommands
 *			Verify that the commands for a target are ok. Provide
 *			them if necessary and possible.
 *
 *	Job_Touch	Update a target without really updating it.
 *
 *	Job_Wait	Wait for all currently-running jobs to finish.
 */

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#ifdef USE_KQUEUE
#include <sys/event.h>
#endif
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>

#include "arch.h"
#include "buf.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "GNode.h"
#include "job.h"
#include "make.h"
#include "parse.h"
#include "pathnames.h"
#include "str.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

#define	TMPPAT	"/tmp/makeXXXXXXXXXX"

#ifndef USE_KQUEUE
/*
 * The SEL_ constants determine the maximum amount of time spent in select
 * before coming out to see if a child has finished. SEL_SEC is the number of
 * seconds and SEL_USEC is the number of micro-seconds
 */
#define	SEL_SEC		2
#define	SEL_USEC	0
#endif /* !USE_KQUEUE */

/*
 * Job Table definitions.
 *
 * The job "table" is kept as a linked Lst in 'jobs', with the number of
 * active jobs maintained in the 'nJobs' variable. At no time will this
 * exceed the value of 'maxJobs', initialized by the Job_Init function.
 *
 * When a job is finished, the Make_Update function is called on each of the
 * parents of the node which was just remade. This takes care of the upward
 * traversal of the dependency graph.
 */
#define	JOB_BUFSIZE	1024
typedef struct Job {
	pid_t		pid;	/* The child's process ID */

	struct GNode	*node;	/* The target the child is making */

	/*
	 * A LstNode for the first command to be saved after the job completes.
	 * This is NULL if there was no "..." in the job's commands.
	 */
	LstNode		*tailCmds;

	/*
	 * An FILE* for writing out the commands. This is only
	 * used before the job is actually started.
	 */
	FILE		*cmdFILE;

	/*
	 * A word of flags which determine how the module handles errors,
	 * echoing, etc. for the job
	 */
	short		flags;	/* Flags to control treatment of job */
#define	JOB_IGNERR	0x001	/* Ignore non-zero exits */
#define	JOB_SILENT	0x002	/* no output */
#define	JOB_SPECIAL	0x004	/* Target is a special one. i.e. run it locally
				 * if we can't export it and maxLocal is 0 */
#define	JOB_IGNDOTS	0x008  	/* Ignore "..." lines when processing
				 * commands */
#define	JOB_FIRST	0x020	/* Job is first job for the node */
#define	JOB_RESTART	0x080	/* Job needs to be completely restarted */
#define	JOB_RESUME	0x100	/* Job needs to be resumed b/c it stopped,
				 * for some reason */
#define	JOB_CONTINUING	0x200	/* We are in the process of resuming this job.
				 * Used to avoid infinite recursion between
				 * JobFinish and JobRestart */

	/* union for handling shell's output */
	union {
		/*
		 * This part is used when usePipes is true.
 		 * The output is being caught via a pipe and the descriptors
		 * of our pipe, an array in which output is line buffered and
		 * the current position in that buffer are all maintained for
		 * each job.
		 */
		struct {
			/*
			 * Input side of pipe associated with
			 * job's output channel
			 */
			int	op_inPipe;

			/*
			 * Output side of pipe associated with job's
			 * output channel
			 */
			int	op_outPipe;

			/*
			 * Buffer for storing the output of the
			 * job, line by line
			 */
			char	op_outBuf[JOB_BUFSIZE + 1];

			/* Current position in op_outBuf */
			int	op_curPos;
		}	o_pipe;

		/*
		 * If usePipes is false the output is routed to a temporary
		 * file and all that is kept is the name of the file and the
		 * descriptor open to the file.
		 */
		struct {
			/* Name of file to which shell output was rerouted */
			char	of_outFile[sizeof(TMPPAT)];

			/*
			 * Stream open to the output file. Used to funnel all
			 * from a single job to one file while still allowing
			 * multiple shell invocations
			 */
			int	of_outFd;
		}	o_file;

	}       output;	    /* Data for tracking a shell's output */

	TAILQ_ENTRY(Job) link;	/* list link */
} Job;

#define	outPipe	  	output.o_pipe.op_outPipe
#define	inPipe	  	output.o_pipe.op_inPipe
#define	outBuf		output.o_pipe.op_outBuf
#define	curPos		output.o_pipe.op_curPos
#define	outFile		output.o_file.of_outFile
#define	outFd	  	output.o_file.of_outFd

TAILQ_HEAD(JobList, Job);

/*
 * Shell Specifications:
 *
 * Some special stuff goes on if a shell doesn't have error control. In such
 * a case, errCheck becomes a printf template for echoing the command,
 * should echoing be on and ignErr becomes another printf template for
 * executing the command while ignoring the return status. If either of these
 * strings is empty when hasErrCtl is FALSE, the command will be executed
 * anyway as is and if it causes an error, so be it.
 */
#define	DEF_SHELL_STRUCT(TAG, CONST)					\
struct TAG {								\
	/*								\
	 * the name of the shell. For Bourne and C shells, this is used	\
	 * only to find the shell description when used as the single	\
	 * source of a .SHELL target. For user-defined shells, this is	\
	 * the full path of the shell.					\
	 */								\
	CONST char	*name;						\
									\
	/* True if both echoOff and echoOn defined */			\
	Boolean		hasEchoCtl;					\
									\
	CONST char	*echoOff;	/* command to turn off echo */	\
	CONST char	*echoOn;	/* command to turn it back on */\
									\
	/*								\
	 * What the shell prints, and its length, when given the	\
	 * echo-off command. This line will not be printed when		\
	 * received from the shell. This is usually the command which	\
	 * was executed to turn off echoing				\
	 */								\
	CONST char	*noPrint;					\
									\
	/* set if can control error checking for individual commands */	\
	Boolean		hasErrCtl;					\
									\
	/* string to turn error checking on */				\
	CONST char	*errCheck;					\
									\
	/* string to turn off error checking */				\
	CONST char	*ignErr;					\
									\
	CONST char	*echo;	/* command line flag: echo commands */	\
	CONST char	*exit;	/* command line flag: exit on error */	\
}

DEF_SHELL_STRUCT(Shell,);
DEF_SHELL_STRUCT(CShell, const);

/*
 * error handling variables
 */
static int	errors = 0;	/* number of errors reported */
static int	aborting = 0;	/* why is the make aborting? */
#define	ABORT_ERROR	1	/* Because of an error */
#define	ABORT_INTERRUPT	2	/* Because it was interrupted */
#define	ABORT_WAIT	3	/* Waiting for jobs to finish */

/*
 * XXX: Avoid SunOS bug... FILENO() is fp->_file, and file
 * is a char! So when we go above 127 we turn negative!
 */
#define	FILENO(a) ((unsigned)fileno(a))

/*
 * post-make command processing. The node postCommands is really just the
 * .END target but we keep it around to avoid having to search for it
 * all the time.
 */
static GNode	*postCommands;

/*
 * The number of commands actually printed for a target. Should this
 * number be 0, no shell will be executed.
 */
static int	numCommands;

/*
 * Return values from JobStart.
 */
#define	JOB_RUNNING	0	/* Job is running */
#define	JOB_ERROR 	1	/* Error in starting the job */
#define	JOB_FINISHED	2	/* The job is already finished */
#define	JOB_STOPPED	3	/* The job is stopped */

/*
 * Descriptions for various shells.
 */
static const struct CShell shells[] = {
	/*
	 * CSH description. The csh can do echo control by playing
	 * with the setting of the 'echo' shell variable. Sadly,
	 * however, it is unable to do error control nicely.
	 */
	{
		"csh",
		TRUE, "unset verbose", "set verbose", "unset verbose",
		FALSE, "echo \"%s\"\n", "csh -c \"%s || exit 0\"",
		"v", "e",
	},
	/*
	 * SH description. Echo control is also possible and, under
	 * sun UNIX anyway, one can even control error checking.
	 */
	{
		"sh",
		TRUE, "set -", "set -v", "set -",
		TRUE, "set -e", "set +e",
#ifdef OLDBOURNESHELL
		FALSE, "echo \"%s\"\n", "sh -c '%s || exit 0'\n",
#endif
		"v", "e",
	},
	/*
	 * KSH description. The Korn shell has a superset of
	 * the Bourne shell's functionality.
	 */
	{
		"ksh",
		TRUE, "set -", "set -v", "set -",
		TRUE, "set -e", "set +e",
		"v", "e",
	},
};

/*
 * This is the shell to which we pass all commands in the Makefile.
 * It is set by the Job_ParseShell function.
 */
static struct Shell *commandShell = NULL;
char		*shellPath = NULL;	/* full pathname of executable image */
char		*shellName = NULL;	/* last component of shell */

int		maxJobs;	/* The most children we can run at once */
static int	nJobs;		/* The number of children currently running */

/* The structures that describe them */
static struct JobList jobs = TAILQ_HEAD_INITIALIZER(jobs);

static Boolean	jobFull;    	/* Flag to tell when the job table is full. It
				 * is set TRUE when (1) the total number of
				 * running jobs equals the maximum allowed */
#ifdef USE_KQUEUE
static int	kqfd;		/* File descriptor obtained by kqueue() */
#else
static fd_set  	outputs;    	/* Set of descriptors of pipes connected to
				 * the output channels of children */
#endif

static GNode   	*lastNode;	/* The node for which output was most recently
				 * produced. */
static const char *targFmt;   	/* Format string to use to head output from a
				 * job when it's not the most-recent job heard
				 * from */

#define	TARG_FMT  "--- %s ---\n" /* Default format */
#define	MESSAGE(fp, gn) \
	 fprintf(fp, targFmt, gn->name);

/*
 * When JobStart attempts to run a job but isn't allowed to
 * or when Job_CatchChildren detects a job that has
 * been stopped somehow, the job is placed on the stoppedJobs queue to be run
 * when the next job finishes.
 *
 * Lst of Job structures describing jobs that were stopped due to
 * concurrency limits or externally
 */
static struct JobList stoppedJobs = TAILQ_HEAD_INITIALIZER(stoppedJobs);

static int	fifoFd;		/* Fd of our job fifo */
static char	fifoName[] = "/tmp/make_fifo_XXXXXXXXX";
static int	fifoMaster;

static sig_atomic_t interrupted;


#if defined(USE_PGRP) && defined(SYSV)
# define KILL(pid, sig)		killpg(-(pid), (sig))
#else
# if defined(USE_PGRP)
#  define KILL(pid, sig)	killpg((pid), (sig))
# else
#  define KILL(pid, sig)	kill((pid), (sig))
# endif
#endif

/*
 * Grmpf... There is no way to set bits of the wait structure
 * anymore with the stupid W*() macros. I liked the union wait
 * stuff much more. So, we devise our own macros... This is
 * really ugly, use dramamine sparingly. You have been warned.
 */
#define	W_SETMASKED(st, val, fun)				\
	{							\
		int sh = (int)~0;				\
		int mask = fun(sh);				\
								\
		for (sh = 0; ((mask >> sh) & 1) == 0; sh++)	\
			continue;				\
		*(st) = (*(st) & ~mask) | ((val) << sh);	\
	}

#define	W_SETTERMSIG(st, val) W_SETMASKED(st, val, WTERMSIG)
#define	W_SETEXITSTATUS(st, val) W_SETMASKED(st, val, WEXITSTATUS)

static void JobRestart(Job *);
static int JobStart(GNode *, int, Job *);
static void JobDoOutput(Job *, Boolean);
static struct Shell *JobMatchShell(const char *);
static void JobInterrupt(int, int);
static void JobRestartJobs(void);

/**
 * JobCatchSignal
 *	Got a signal. Set global variables and hope that someone will
 *	handle it.
 */
static void
JobCatchSig(int signo)
{

	interrupted = signo;
}

/**
 * JobPassSig --
 *	Pass a signal on to all local jobs if
 *	USE_PGRP is defined, then die ourselves.
 *
 * Side Effects:
 *	We die by the same signal.
 */
static void
JobPassSig(int signo)
{
	Job	*job;
	sigset_t nmask, omask;
	struct sigaction act;

	sigemptyset(&nmask);
	sigaddset(&nmask, signo);
	sigprocmask(SIG_SETMASK, &nmask, &omask);

	DEBUGF(JOB, ("JobPassSig(%d) called.\n", signo));
	TAILQ_FOREACH(job, &jobs, link) {
		DEBUGF(JOB, ("JobPassSig passing signal %d to child %jd.\n",
		    signo, (intmax_t)job->pid));
		KILL(job->pid, signo);
	}

	/*
	 * Deal with proper cleanup based on the signal received. We only run
	 * the .INTERRUPT target if the signal was in fact an interrupt.
	 * The other three termination signals are more of a "get out *now*"
	 * command.
	 */
	if (signo == SIGINT) {
		JobInterrupt(TRUE, signo);
	} else if (signo == SIGHUP || signo == SIGTERM || signo == SIGQUIT) {
		JobInterrupt(FALSE, signo);
	}

	/*
	 * Leave gracefully if SIGQUIT, rather than core dumping.
	 */
	if (signo == SIGQUIT) {
		signo = SIGINT;
	}

	/*
	 * Send ourselves the signal now we've given the message to everyone
	 * else. Note we block everything else possible while we're getting
	 * the signal. This ensures that all our jobs get continued when we
	 * wake up before we take any other signal.
	 * XXX this comment seems wrong.
	 */
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(signo, &act, NULL);

	DEBUGF(JOB, ("JobPassSig passing signal to self, mask = %x.\n",
	    ~0 & ~(1 << (signo - 1))));
	signal(signo, SIG_DFL);

	KILL(getpid(), signo);

	signo = SIGCONT;
	TAILQ_FOREACH(job, &jobs, link) {
		DEBUGF(JOB, ("JobPassSig passing signal %d to child %jd.\n",
		    signo, (intmax_t)job->pid));
		KILL(job->pid, signo);
	}

	sigprocmask(SIG_SETMASK, &omask, NULL);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	act.sa_handler = JobPassSig;
	sigaction(signo, &act, NULL);
}

/**
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
 *	was reached. These commands are later attached to the postCommands
 *	node and executed by Job_Finish when all things are done.
 *	This function is called from JobStart via LST_FOREACH.
 *
 * Results:
 *	Always 0, unless the command was "..."
 *
 * Side Effects:
 *	If the command begins with a '-' and the shell has no error control,
 *	the JOB_IGNERR flag is set in the job descriptor.
 *	If the command is "..." and we're not ignoring such things,
 *	tailCmds is set to the successor node of the cmd.
 *	numCommands is incremented if the command is actually printed.
 */
static int
JobPrintCommand(char *cmd, Job *job)
{
	Boolean	noSpecials;	/* true if we shouldn't worry about
				 * inserting special commands into
				 * the input stream. */
	Boolean	shutUp = FALSE;	/* true if we put a no echo command
				 * into the command file */
	Boolean	errOff = FALSE;	/* true if we turned error checking
				 * off before printing the command
				 * and need to turn it back on */
	const char *cmdTemplate;/* Template to use when printing the command */
	char	*cmdStart;	/* Start of expanded command */
	LstNode	*cmdNode;	/* Node for replacing the command */

	noSpecials = (noExecute && !(job->node->type & OP_MAKE));

	if (strcmp(cmd, "...") == 0) {
		job->node->type |= OP_SAVE_CMDS;
		if ((job->flags & JOB_IGNDOTS) == 0) {
			job->tailCmds =
			    Lst_Succ(Lst_Member(&job->node->commands, cmd));
			return (1);
		}
		return (0);
	}

#define	DBPRINTF(fmt, arg)			\
	DEBUGF(JOB, (fmt, arg));		\
	fprintf(job->cmdFILE, fmt, arg);	\
	fflush(job->cmdFILE);

	numCommands += 1;

	/*
	 * For debugging, we replace each command with the result of expanding
	 * the variables in the command.
	 */
	cmdNode = Lst_Member(&job->node->commands, cmd);

	cmd = Buf_Peel(Var_Subst(cmd, job->node, FALSE));
	cmdStart = cmd;

	Lst_Replace(cmdNode, cmdStart);

	cmdTemplate = "%s\n";

	/*
	 * Check for leading @', -' or +'s to control echoing, error checking,
	 * and execution on -n.
	 */
	while (*cmd == '@' || *cmd == '-' || *cmd == '+') {
		switch (*cmd) {

		  case '@':
			shutUp = DEBUG(LOUD) ? FALSE : TRUE;
			break;

		  case '-':
			errOff = TRUE;
			break;

		  case '+':
			if (noSpecials) {
				/*
				 * We're not actually exececuting anything...
				 * but this one needs to be - use compat mode
				 * just for it.
				 */
				Compat_RunCommand(cmd, job->node);
				return (0);
			}
			break;
		}
		cmd++;
	}

	while (isspace((unsigned char)*cmd))
		cmd++;

	if (shutUp) {
		if (!(job->flags & JOB_SILENT) && !noSpecials &&
		    commandShell->hasEchoCtl) {
			DBPRINTF("%s\n", commandShell->echoOff);
		} else {
			shutUp = FALSE;
		}
	}

	if (errOff) {
		if (!(job->flags & JOB_IGNERR) && !noSpecials) {
			if (commandShell->hasErrCtl) {
				/*
				 * We don't want the error-control commands
				 * showing up either, so we turn off echoing
				 * while executing them. We could put another
				 * field in the shell structure to tell
				 * JobDoOutput to look for this string too,
				 * but why make it any more complex than
				 * it already is?
				 */
				if (!(job->flags & JOB_SILENT) && !shutUp &&
				    commandShell->hasEchoCtl) {
					DBPRINTF("%s\n", commandShell->echoOff);
					DBPRINTF("%s\n", commandShell->ignErr);
					DBPRINTF("%s\n", commandShell->echoOn);
				} else {
					DBPRINTF("%s\n", commandShell->ignErr);
				}
			} else if (commandShell->ignErr &&
			    *commandShell->ignErr != '\0') {
				/*
				 * The shell has no error control, so we need to
				 * be weird to get it to ignore any errors from
				 * the command. If echoing is turned on, we turn
				 * it off and use the errCheck template to echo
				 * the command. Leave echoing off so the user
				 * doesn't see the weirdness we go through to
				 * ignore errors. Set cmdTemplate to use the
				 * weirdness instead of the simple "%s\n"
				 * template.
				 */
				if (!(job->flags & JOB_SILENT) && !shutUp &&
				    commandShell->hasEchoCtl) {
					DBPRINTF("%s\n", commandShell->echoOff);
					DBPRINTF(commandShell->errCheck, cmd);
					shutUp = TRUE;
				}
				cmdTemplate = commandShell->ignErr;
				/*
				 * The error ignoration (hee hee) is already
				 * taken care of by the ignErr template, so
				 * pretend error checking is still on.
				*/
				errOff = FALSE;
			} else {
				errOff = FALSE;
			}
		} else {
			errOff = FALSE;
		}
	}

	DBPRINTF(cmdTemplate, cmd);

	if (errOff) {
		/*
		 * If echoing is already off, there's no point in issuing the
		 * echoOff command. Otherwise we issue it and pretend it was on
		 * for the whole command...
		 */
		if (!shutUp && !(job->flags & JOB_SILENT) &&
		    commandShell->hasEchoCtl) {
			DBPRINTF("%s\n", commandShell->echoOff);
			shutUp = TRUE;
		}
		DBPRINTF("%s\n", commandShell->errCheck);
	}
	if (shutUp) {
		DBPRINTF("%s\n", commandShell->echoOn);
	}
	return (0);
}

/**
 * JobClose --
 *	Called to close both input and output pipes when a job is finished.
 *
 * Side Effects:
 *	The file descriptors associated with the job are closed.
 */
static void
JobClose(Job *job)
{

	if (usePipes) {
#if !defined(USE_KQUEUE)
		FD_CLR(job->inPipe, &outputs);
#endif
		if (job->outPipe != job->inPipe) {
			close(job->outPipe);
		}
		JobDoOutput(job, TRUE);
		close(job->inPipe);
	} else {
		close(job->outFd);
		JobDoOutput(job, TRUE);
	}
}

/**
 * JobFinish  --
 *	Do final processing for the given job including updating
 *	parents and starting new jobs as available/necessary. Note
 *	that we pay no attention to the JOB_IGNERR flag here.
 *	This is because when we're called because of a noexecute flag
 *	or something, jstat.w_status is 0 and when called from
 *	Job_CatchChildren, the status is zeroed if it s/b ignored.
 *
 * Side Effects:
 *	Some nodes may be put on the toBeMade queue.
 *	Final commands for the job are placed on postCommands.
 *
 *	If we got an error and are aborting (aborting == ABORT_ERROR) and
 *	the job list is now empty, we are done for the day.
 *	If we recognized an error (errors !=0), we set the aborting flag
 *	to ABORT_ERROR so no more jobs will be started.
 */
static void
JobFinish(Job *job, int *status)
{
	Boolean	done;
	LstNode	*ln;

	if ((WIFEXITED(*status) && WEXITSTATUS(*status) != 0 &&
	    !(job->flags & JOB_IGNERR)) ||
	    (WIFSIGNALED(*status) && WTERMSIG(*status) != SIGCONT)) {
		/*
		 * If it exited non-zero and either we're doing things our
		 * way or we're not ignoring errors, the job is finished.
		 * Similarly, if the shell died because of a signal
		 * the job is also finished. In these cases, finish out the
		 * job's output before printing the exit status...
		 */
		JobClose(job);
		if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
			fclose(job->cmdFILE);
		}
		done = TRUE;

	} else if (WIFEXITED(*status)) {
		/*
		 * Deal with ignored errors in -B mode. We need to print a
		 * message telling of the ignored error as well as setting
		 * status.w_status to 0 so the next command gets run. To do
		 * this, we set done to be TRUE if in -B mode and the job
		 * exited non-zero.
		 */
		done = WEXITSTATUS(*status) != 0;

		/*
		 * Old comment said: "Note we don't want to close down any of
		 * the streams until we know we're at the end." But we do.
		 * Otherwise when are we going to print the rest of the stuff?
		 */
		JobClose(job);
	} else {
		/*
		 * No need to close things down or anything.
		 */
		done = FALSE;
	}

	if (done || WIFSTOPPED(*status) ||
	    (WIFSIGNALED(*status) && WTERMSIG(*status) == SIGCONT) ||
	    DEBUG(JOB)) {
		FILE	*out;

		if (compatMake && !usePipes && (job->flags & JOB_IGNERR)) {
			/*
			 * If output is going to a file and this job is ignoring
			 * errors, arrange to have the exit status sent to the
			 * output file as well.
			 */
			out = fdopen(job->outFd, "w");
			if (out == NULL)
				Punt("Cannot fdopen");
		} else {
			out = stdout;
		}

		if (WIFEXITED(*status)) {
			DEBUGF(JOB, ("Process %jd exited.\n",
			    (intmax_t)job->pid));
			if (WEXITSTATUS(*status) != 0) {
				if (usePipes && job->node != lastNode) {
					MESSAGE(out, job->node);
					lastNode = job->node;
				}
				fprintf(out, "*** Error code %d%s\n",
				    WEXITSTATUS(*status),
				    (job->flags & JOB_IGNERR) ?
				    "(ignored)" : "");

				if (job->flags & JOB_IGNERR) {
					*status = 0;
				}
			} else if (DEBUG(JOB)) {
				if (usePipes && job->node != lastNode) {
					MESSAGE(out, job->node);
					lastNode = job->node;
				}
				fprintf(out, "*** Completed successfully\n");
			}

		} else if (WIFSTOPPED(*status)) {
			DEBUGF(JOB, ("Process %jd stopped.\n",
			    (intmax_t)job->pid));
			if (usePipes && job->node != lastNode) {
				MESSAGE(out, job->node);
				lastNode = job->node;
			}
			fprintf(out, "*** Stopped -- signal %d\n",
			WSTOPSIG(*status));
			job->flags |= JOB_RESUME;
			TAILQ_INSERT_TAIL(&stoppedJobs, job, link);
			fflush(out);
			return;

		} else if (WTERMSIG(*status) == SIGCONT) {
			/*
			 * If the beastie has continued, shift the Job from
			 * the stopped list to the running one (or re-stop it
			 * if concurrency is exceeded) and go and get another
			 * child.
			 */
			if (job->flags & (JOB_RESUME | JOB_RESTART)) {
				if (usePipes && job->node != lastNode) {
					MESSAGE(out, job->node);
					lastNode = job->node;
				}
				fprintf(out, "*** Continued\n");
			}
			if (!(job->flags & JOB_CONTINUING)) {
				DEBUGF(JOB, ("Warning: process %jd was not "
				    "continuing.\n", (intmax_t)job->pid));
#ifdef notdef
				/*
				 * We don't really want to restart a job from
				 * scratch just because it continued, especially
				 * not without killing the continuing process!
				 *  That's why this is ifdef'ed out.
				 * FD - 9/17/90
				 */
				JobRestart(job);
#endif
			}
			job->flags &= ~JOB_CONTINUING;
			TAILQ_INSERT_TAIL(&jobs, job, link);
			nJobs += 1;
			DEBUGF(JOB, ("Process %jd is continuing locally.\n",
			    (intmax_t)job->pid));
			if (nJobs == maxJobs) {
				jobFull = TRUE;
				DEBUGF(JOB, ("Job queue is full.\n"));
			}
			fflush(out);
			return;

		} else {
			if (usePipes && job->node != lastNode) {
				MESSAGE(out, job->node);
				lastNode = job->node;
			}
			fprintf(out, "*** Signal %d\n", WTERMSIG(*status));
		}

		fflush(out);
	}

	/*
	 * Now handle the -B-mode stuff. If the beast still isn't finished,
	 * try and restart the job on the next command. If JobStart says it's
	 * ok, it's ok. If there's an error, this puppy is done.
	 */
	if (compatMake && WIFEXITED(*status) &&
	    Lst_Succ(job->node->compat_command) != NULL) {
		switch (JobStart(job->node, job->flags & JOB_IGNDOTS, job)) {
		  case JOB_RUNNING:
			done = FALSE;
			break;
		  case JOB_ERROR:
			done = TRUE;
			W_SETEXITSTATUS(status, 1);
			break;
		  case JOB_FINISHED:
			/*
			 * If we got back a JOB_FINISHED code, JobStart has
			 * already called Make_Update and freed the job
			 * descriptor. We set done to false here to avoid fake
			 * cycles and double frees. JobStart needs to do the
			 * update so we can proceed up the graph when given
			 * the -n flag..
			 */
			done = FALSE;
			break;
		  default:
			break;
		}
	} else {
		done = TRUE;
	}

	if (done && aborting != ABORT_ERROR &&
	    aborting != ABORT_INTERRUPT && *status == 0) {
		/*
		 * As long as we aren't aborting and the job didn't return a
		 * non-zero status that we shouldn't ignore, we call
		 * Make_Update to update the parents. In addition, any saved
		 * commands for the node are placed on the .END target.
		 */
		for (ln = job->tailCmds; ln != NULL; ln = LST_NEXT(ln)) {
			Lst_AtEnd(&postCommands->commands,
			    Buf_Peel(
				Var_Subst(Lst_Datum(ln), job->node, FALSE)));
		}

		job->node->made = MADE;
		Make_Update(job->node);
		free(job);

	} else if (*status != 0) {
		errors += 1;
		free(job);
	}

	JobRestartJobs();

	/*
	 * Set aborting if any error.
	 */
	if (errors && !keepgoing && aborting != ABORT_INTERRUPT) {
		/*
		 * If we found any errors in this batch of children and the -k
		 * flag wasn't given, we set the aborting flag so no more jobs
		 * get started.
		 */
		aborting = ABORT_ERROR;
	}

	if (aborting == ABORT_ERROR && Job_Empty()) {
		/*
		 * If we are aborting and the job table is now empty, we finish.
		 */
		Finish(errors);
	}
}

/**
 * Job_Touch
 *	Touch the given target. Called by JobStart when the -t flag was
 *	given.  Prints messages unless told to be silent.
 *
 * Side Effects:
 *	The data modification of the file is changed. In addition, if the
 *	file did not exist, it is created.
 */
void
Job_Touch(GNode *gn, Boolean silent)
{
	int	streamID;	/* ID of stream opened to do the touch */
	struct utimbuf times;	/* Times for utime() call */

	if (gn->type & (OP_JOIN | OP_USE | OP_EXEC | OP_OPTIONAL)) {
		/*
		 * .JOIN, .USE, .ZEROTIME and .OPTIONAL targets are "virtual"
		 * targets and, as such, shouldn't really be created.
		 */
		return;
	}

	if (!silent) {
		fprintf(stdout, "touch %s\n", gn->name);
		fflush(stdout);
	}

	if (noExecute) {
		return;
	}

	if (gn->type & OP_ARCHV) {
		Arch_Touch(gn);
	} else if (gn->type & OP_LIB) {
		Arch_TouchLib(gn);
	} else {
		char	*file = gn->path ? gn->path : gn->name;

		times.actime = times.modtime = now;
		if (utime(file, &times) < 0) {
			streamID = open(file, O_RDWR | O_CREAT, 0666);

			if (streamID >= 0) {
				char	c;

				/*
				 * Read and write a byte to the file to change
				 * the modification time, then close the file.
				 */
				if (read(streamID, &c, 1) == 1) {
					lseek(streamID, (off_t)0, SEEK_SET);
					write(streamID, &c, 1);
				}

				close(streamID);
			} else {
				fprintf(stdout, "*** couldn't touch %s: %s",
				    file, strerror(errno));
				fflush(stdout);
			}
		}
	}
}

/**
 * Job_CheckCommands
 *	Make sure the given node has all the commands it needs.
 *
 * Results:
 *	TRUE if the commands list is/was ok.
 *
 * Side Effects:
 *	The node will have commands from the .DEFAULT rule added to it
 *	if it needs them.
 */
Boolean
Job_CheckCommands(GNode *gn, void (*abortProc)(const char *, ...))
{

	if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->commands) &&
	    (gn->type & OP_LIB) == 0) {
		/*
		 * No commands. Look for .DEFAULT rule from which we might infer
		 * commands.
		 */
		if (DEFAULT != NULL && !Lst_IsEmpty(&DEFAULT->commands)) {
			char *p1;
			/*
			 * Make only looks for a .DEFAULT if the node was
			 * never the target of an operator, so that's what we
			 * do too. If a .DEFAULT was given, we substitute its
			 * commands for gn's commands and set the IMPSRC
			 * variable to be the target's name The DEFAULT node
			 * acts like a transformation rule, in that gn also
			 * inherits any attributes or sources attached to
			 * .DEFAULT itself.
			 */
			Make_HandleUse(DEFAULT, gn);
			Var_Set(IMPSRC, Var_Value(TARGET, gn, &p1), gn);
			free(p1);

		} else if (Dir_MTime(gn) == 0) {
			/*
			 * The node wasn't the target of an operator we have
			 * no .DEFAULT rule to go on and the target doesn't
			 * already exist. There's nothing more we can do for
			 * this branch. If the -k flag wasn't given, we stop
			 * in our tracks, otherwise we just don't update
			 * this node's parents so they never get examined.
			 */
			static const char msg[] =
			    "make: don't know how to make";

			if (gn->type & OP_OPTIONAL) {
				fprintf(stdout, "%s %s(ignored)\n",
				    msg, gn->name);
				fflush(stdout);
			} else if (keepgoing) {
				fprintf(stdout, "%s %s(continuing)\n",
				    msg, gn->name);
				fflush(stdout);
				return (FALSE);
			} else {
#if OLD_JOKE
				if (strcmp(gn->name,"love") == 0)
					(*abortProc)("Not war.");
				else
#endif
					(*abortProc)("%s %s. Stop",
					    msg, gn->name);
				return (FALSE);
			}
		}
	}
	return (TRUE);
}

/**
 * JobExec
 *	Execute the shell for the given job. Called from JobStart and
 *	JobRestart.
 *
 * Side Effects:
 *	A shell is executed, outputs is altered and the Job structure added
 *	to the job table.
 */
static void
JobExec(Job *job, char **argv)
{
	pid_t	    	  cpid;	    	/* ID of new child */

	if (DEBUG(JOB)) {
		int 	  i;

		DEBUGF(JOB, ("Running %s\n", job->node->name));
		DEBUGF(JOB, ("\tCommand: "));
		for (i = 0; argv[i] != NULL; i++) {
			DEBUGF(JOB, ("%s ", argv[i]));
		}
		DEBUGF(JOB, ("\n"));
	}

	/*
	 * Some jobs produce no output and it's disconcerting to have
	 * no feedback of their running (since they produce no output, the
	 * banner with their name in it never appears). This is an attempt to
	 * provide that feedback, even if nothing follows it.
	 */
	if (lastNode != job->node && (job->flags & JOB_FIRST) &&
	    !(job->flags & JOB_SILENT)) {
		MESSAGE(stdout, job->node);
		lastNode = job->node;
	}

	if ((cpid = vfork()) == -1) {
		Punt("Cannot fork");
	}

	if (cpid == 0) {
		/*
		 * Child
		 */
		if (fifoFd >= 0)
			close(fifoFd);

		/*
		 * Must duplicate the input stream down to the child's input and
		 * reset it to the beginning (again). Since the stream was
		 * marked close-on-exec, we must clear that bit in the new
		 * input.
		 */
		if (dup2(FILENO(job->cmdFILE), 0) == -1)
			Punt("Cannot dup2: %s", strerror(errno));
		fcntl(0, F_SETFD, 0);
		lseek(0, (off_t)0, SEEK_SET);

		if (usePipes) {
			/*
			 * Set up the child's output to be routed through the
			 * pipe we've created for it.
			 */
			if (dup2(job->outPipe, 1) == -1)
				Punt("Cannot dup2: %s", strerror(errno));
		} else {
			/*
			 * We're capturing output in a file, so we duplicate the
			 * descriptor to the temporary file into the standard
			 * output.
			 */
			if (dup2(job->outFd, 1) == -1)
				Punt("Cannot dup2: %s", strerror(errno));
		}
		/*
		 * The output channels are marked close on exec. This bit was
		 * duplicated by the dup2 (on some systems), so we have to clear
		 * it before routing the shell's error output to the same place
		 * as its standard output.
		 */
		fcntl(1, F_SETFD, 0);
		if (dup2(1, 2) == -1)
			Punt("Cannot dup2: %s", strerror(errno));

#ifdef USE_PGRP
		/*
		 * We want to switch the child into a different process family
		 * so we can kill it and all its descendants in one fell swoop,
		 * by killing its process family, but not commit suicide.
		 */
# if defined(SYSV)
		setsid();
# else
		setpgid(0, getpid());
# endif
#endif /* USE_PGRP */

		execv(shellPath, argv);

		write(STDERR_FILENO, "Could not execute shell\n",
		    sizeof("Could not execute shell"));
		_exit(1);
	}
	/*
	 * Parent
	 */
	job->pid = cpid;

	if (usePipes && (job->flags & JOB_FIRST)) {
		/*
		 * The first time a job is run for a node, we set the
		 * current position in the buffer to the beginning and
		 * mark another stream to watch in the outputs mask.
		 */
#ifdef USE_KQUEUE
		struct kevent	kev[2];
#endif
		job->curPos = 0;

#if defined(USE_KQUEUE)
		EV_SET(&kev[0], job->inPipe, EVFILT_READ, EV_ADD, 0, 0, job);
		EV_SET(&kev[1], job->pid, EVFILT_PROC,
		    EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, NULL);
		if (kevent(kqfd, kev, 2, NULL, 0, NULL) != 0) {
			/*
			 * kevent() will fail if the job is already
			 * finished
			 */
			if (errno != EINTR && errno != EBADF && errno != ESRCH)
				Punt("kevent: %s", strerror(errno));
		}
#else
		FD_SET(job->inPipe, &outputs);
#endif /* USE_KQUEUE */
	}

	if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
		fclose(job->cmdFILE);
		job->cmdFILE = NULL;
	}

	/*
	 * Now the job is actually running, add it to the table.
	 */
	nJobs += 1;
	TAILQ_INSERT_TAIL(&jobs, job, link);
	if (nJobs == maxJobs) {
		jobFull = TRUE;
	}
}

/**
 * JobMakeArgv
 *	Create the argv needed to execute the shell for a given job.
 */
static void
JobMakeArgv(Job *job, char **argv)
{
	int		argc;
	static char	args[10];	/* For merged arguments */

	argv[0] = shellName;
	argc = 1;

	if ((commandShell->exit && *commandShell->exit != '-') ||
	    (commandShell->echo && *commandShell->echo != '-')) {
		/*
		 * At least one of the flags doesn't have a minus before it, so
		 * merge them together. Have to do this because the *(&(@*#*&#$#
		 * Bourne shell thinks its second argument is a file to source.
		 * Grrrr. Note the ten-character limitation on the combined
		 * arguments.
		 */
		sprintf(args, "-%s%s", (job->flags & JOB_IGNERR) ? "" :
		    commandShell->exit ? commandShell->exit : "",
		    (job->flags & JOB_SILENT) ? "" :
		    commandShell->echo ? commandShell->echo : "");

		if (args[1]) {
			argv[argc] = args;
			argc++;
		}
	} else {
		if (!(job->flags & JOB_IGNERR) && commandShell->exit) {
			argv[argc] = commandShell->exit;
			argc++;
		}
		if (!(job->flags & JOB_SILENT) && commandShell->echo) {
			argv[argc] = commandShell->echo;
			argc++;
		}
	}
	argv[argc] = NULL;
}

/**
 * JobRestart
 *	Restart a job that stopped for some reason. The job must be neither
 *	on the jobs nor on the stoppedJobs list.
 *
 * Side Effects:
 *	jobFull will be set if the job couldn't be run.
 */
static void
JobRestart(Job *job)
{

	if (job->flags & JOB_RESTART) {
		/*
		 * Set up the control arguments to the shell. This is based on
		 * the flags set earlier for this job. If the JOB_IGNERR flag
		 * is clear, the 'exit' flag of the commandShell is used to
		 * cause it to exit upon receiving an error. If the JOB_SILENT
		 * flag is clear, the 'echo' flag of the commandShell is used
		 * to get it to start echoing as soon as it starts
		 * processing commands.
		 */
		char	*argv[4];

		JobMakeArgv(job, argv);

		DEBUGF(JOB, ("Restarting %s...", job->node->name));
		if (nJobs >= maxJobs && !(job->flags & JOB_SPECIAL)) {
			/*
			 * Not allowed to run -- put it back on the hold
			 * queue and mark the table full
			 */
			DEBUGF(JOB, ("holding\n"));
			TAILQ_INSERT_HEAD(&stoppedJobs, job, link);
			jobFull = TRUE;
			DEBUGF(JOB, ("Job queue is full.\n"));
			return;
		} else {
			/*
			 * Job may be run locally.
			 */
			DEBUGF(JOB, ("running locally\n"));
		}
		JobExec(job, argv);

	} else {
		/*
		 * The job has stopped and needs to be restarted.
		 * Why it stopped, we don't know...
		 */
		DEBUGF(JOB, ("Resuming %s...", job->node->name));
		if ((nJobs < maxJobs || ((job->flags & JOB_SPECIAL) &&
		    maxJobs == 0)) && nJobs != maxJobs) {
			/*
			 * If we haven't reached the concurrency limit already
			 * (or the job must be run and maxJobs is 0), it's ok
			 * to resume it.
			 */
			Boolean error;
			int status;

			error = (KILL(job->pid, SIGCONT) != 0);

			if (!error) {
				/*
				 * Make sure the user knows we've continued
				 * the beast and actually put the thing in the
				 * job table.
				 */
				job->flags |= JOB_CONTINUING;
				status = 0;
				W_SETTERMSIG(&status, SIGCONT);
				JobFinish(job, &status);

				job->flags &= ~(JOB_RESUME|JOB_CONTINUING);
				DEBUGF(JOB, ("done\n"));
			} else {
				Error("couldn't resume %s: %s",
				job->node->name, strerror(errno));
				status = 0;
				W_SETEXITSTATUS(&status, 1);
				JobFinish(job, &status);
			}
		} else {
			/*
			* Job cannot be restarted. Mark the table as full and
			* place the job back on the list of stopped jobs.
			*/
			DEBUGF(JOB, ("table full\n"));
			TAILQ_INSERT_HEAD(&stoppedJobs, job, link);
			jobFull = TRUE;
			DEBUGF(JOB, ("Job queue is full.\n"));
		}
	}
}

/**
 * JobStart
 *	Start a target-creation process going for the target described
 *	by the graph node gn.
 *
 * Results:
 *	JOB_ERROR if there was an error in the commands, JOB_FINISHED
 *	if there isn't actually anything left to do for the job and
 *	JOB_RUNNING if the job has been started.
 *
 * Side Effects:
 *	A new Job node is created and added to the list of running
 *	jobs. PMake is forked and a child shell created.
 */
static int
JobStart(GNode *gn, int flags, Job *previous)
{
	Job	*job;		/* new job descriptor */
	char	*argv[4];	/* Argument vector to shell */
	Boolean	cmdsOK;		/* true if the nodes commands were all right */
	Boolean	noExec;		/* Set true if we decide not to run the job */
	int	tfd;		/* File descriptor for temp file */
	LstNode	*ln;
	char	tfile[sizeof(TMPPAT)];

	if (interrupted) {
		JobPassSig(interrupted);
		return (JOB_ERROR);
	}
	if (previous != NULL) {
		previous->flags &= ~(JOB_FIRST | JOB_IGNERR | JOB_SILENT);
		job = previous;
	} else {
		job = emalloc(sizeof(Job));
		flags |= JOB_FIRST;
	}

	job->node = gn;
	job->tailCmds = NULL;

	/*
	 * Set the initial value of the flags for this job based on the global
	 * ones and the node's attributes... Any flags supplied by the caller
	 * are also added to the field.
	 */
	job->flags = 0;
	if (Targ_Ignore(gn)) {
		job->flags |= JOB_IGNERR;
	}
	if (Targ_Silent(gn)) {
		job->flags |= JOB_SILENT;
	}
	job->flags |= flags;

	/*
	 * Check the commands now so any attributes from .DEFAULT have a chance
	 * to migrate to the node.
	 */
	if (!compatMake && (job->flags & JOB_FIRST)) {
		cmdsOK = Job_CheckCommands(gn, Error);
	} else {
		cmdsOK = TRUE;
	}

	/*
	 * If the -n flag wasn't given, we open up OUR (not the child's)
	 * temporary file to stuff commands in it. The thing is rd/wr so we
	 * don't need to reopen it to feed it to the shell. If the -n flag
	 * *was* given, we just set the file to be stdout. Cute, huh?
	 */
	if ((gn->type & OP_MAKE) || (!noExecute && !touchFlag)) {
		/*
		 * We're serious here, but if the commands were bogus, we're
		 * also dead...
		 */
		if (!cmdsOK) {
			DieHorribly();
		}

		strcpy(tfile, TMPPAT);
		if ((tfd = mkstemp(tfile)) == -1)
			Punt("Cannot create temp file: %s", strerror(errno));
		job->cmdFILE = fdopen(tfd, "w+");
		eunlink(tfile);
		if (job->cmdFILE == NULL) {
			close(tfd);
			Punt("Could not open %s", tfile);
		}
		fcntl(FILENO(job->cmdFILE), F_SETFD, 1);
		/*
		 * Send the commands to the command file, flush all its
		 * buffers then rewind and remove the thing.
		 */
		noExec = FALSE;

		/*
		 * Used to be backwards; replace when start doing multiple
		 * commands per shell.
		 */
		if (compatMake) {
			/*
			 * Be compatible: If this is the first time for this
			 * node, verify its commands are ok and open the
			 * commands list for sequential access by later
			 * invocations of JobStart. Once that is done, we take
			 * the next command off the list and print it to the
			 * command file. If the command was an ellipsis, note
			 * that there's nothing more to execute.
			 */
			if (job->flags & JOB_FIRST)
				gn->compat_command = Lst_First(&gn->commands);
			else
				gn->compat_command =
				    Lst_Succ(gn->compat_command);

			if (gn->compat_command == NULL ||
			    JobPrintCommand(Lst_Datum(gn->compat_command), job))
				noExec = TRUE;

			if (noExec && !(job->flags & JOB_FIRST)) {
				/*
				 * If we're not going to execute anything, the
				 * job is done and we need to close down the
				 * various file descriptors we've opened for
				 * output, then call JobDoOutput to catch the
				 * final characters or send the file to the
				 * screen... Note that the i/o streams are only
				 * open if this isn't the first job. Note also
				 * that this could not be done in
				 * Job_CatchChildren b/c it wasn't clear if
				 * there were more commands to execute or not...
				 */
				JobClose(job);
			}
		} else {
			/*
			 * We can do all the commands at once. hooray for sanity
			 */
			numCommands = 0;
			LST_FOREACH(ln, &gn->commands) {
				if (JobPrintCommand(Lst_Datum(ln), job))
					break;
			}

			/*
			 * If we didn't print out any commands to the shell
			 * script, there's not much point in executing the
			 * shell, is there?
			 */
			if (numCommands == 0) {
				noExec = TRUE;
			}
		}

	} else if (noExecute) {
		/*
		 * Not executing anything -- just print all the commands to
		 * stdout in one fell swoop. This will still set up
		 * job->tailCmds correctly.
		 */
		if (lastNode != gn) {
			MESSAGE(stdout, gn);
			lastNode = gn;
		}
		job->cmdFILE = stdout;

		/*
		 * Only print the commands if they're ok, but don't die if
		 * they're not -- just let the user know they're bad and keep
		 * going. It doesn't do any harm in this case and may do
		 * some good.
		 */
		if (cmdsOK) {
			LST_FOREACH(ln, &gn->commands) {
				if (JobPrintCommand(Lst_Datum(ln), job))
					break;
			}
		}
		/*
		* Don't execute the shell, thank you.
		*/
		noExec = TRUE;

	} else {
		/*
		 * Just touch the target and note that no shell should be
		 * executed. Set cmdFILE to stdout to make life easier. Check
		 * the commands, too, but don't die if they're no good -- it
		 * does no harm to keep working up the graph.
		 */
		job->cmdFILE = stdout;
		Job_Touch(gn, job->flags & JOB_SILENT);
		noExec = TRUE;
	}

	/*
	 * If we're not supposed to execute a shell, don't.
	 */
	if (noExec) {
		/*
		 * Unlink and close the command file if we opened one
		 */
		if (job->cmdFILE != stdout) {
			if (job->cmdFILE != NULL)
				fclose(job->cmdFILE);
		} else {
			fflush(stdout);
		}

		/*
		 * We only want to work our way up the graph if we aren't here
		 * because the commands for the job were no good.
		*/
		if (cmdsOK) {
			if (aborting == 0) {
				for (ln = job->tailCmds; ln != NULL;
				    ln = LST_NEXT(ln)) {
					Lst_AtEnd(&postCommands->commands,
					    Buf_Peel(Var_Subst(Lst_Datum(ln),
					    job->node, FALSE)));
				}
				job->node->made = MADE;
				Make_Update(job->node);
			}
			free(job);
			return(JOB_FINISHED);
		} else {
			free(job);
			return(JOB_ERROR);
		}
	} else {
		fflush(job->cmdFILE);
	}

	/*
	 * Set up the control arguments to the shell. This is based on the flags
	 * set earlier for this job.
	 */
	JobMakeArgv(job, argv);

	/*
	 * If we're using pipes to catch output, create the pipe by which we'll
	 * get the shell's output. If we're using files, print out that we're
	 * starting a job and then set up its temporary-file name.
	 */
	if (!compatMake || (job->flags & JOB_FIRST)) {
		if (usePipes) {
			int fd[2];

			if (pipe(fd) == -1)
				Punt("Cannot create pipe: %s", strerror(errno));
			job->inPipe = fd[0];
			job->outPipe = fd[1];
			fcntl(job->inPipe, F_SETFD, 1);
			fcntl(job->outPipe, F_SETFD, 1);
		} else {
			fprintf(stdout, "Remaking `%s'\n", gn->name);
			fflush(stdout);
			strcpy(job->outFile, TMPPAT);
			if ((job->outFd = mkstemp(job->outFile)) == -1)
				Punt("cannot create temp file: %s",
				    strerror(errno));
			fcntl(job->outFd, F_SETFD, 1);
		}
	}

	if (nJobs >= maxJobs && !(job->flags & JOB_SPECIAL) && maxJobs != 0) {
		/*
		 * We've hit the limit of concurrency, so put the job on hold
		 * until some other job finishes. Note that the special jobs
		 * (.BEGIN, .INTERRUPT and .END) may be run even when the
		 * limit has been reached (e.g. when maxJobs == 0).
		 */
		jobFull = TRUE;

		DEBUGF(JOB, ("Can only run job locally.\n"));
		job->flags |= JOB_RESTART;
		TAILQ_INSERT_TAIL(&stoppedJobs, job, link);
	} else {
		if (nJobs >= maxJobs) {
			/*
			 * If we're running this job as a special case
			 * (see above), at least say the table is full.
			 */
			jobFull = TRUE;
			DEBUGF(JOB, ("Local job queue is full.\n"));
		}
		JobExec(job, argv);
	}
	return (JOB_RUNNING);
}

static char *
JobOutput(Job *job, char *cp, char *endp, int msg)
{
	char *ecp;

	if (commandShell->noPrint) {
		ecp = strstr(cp, commandShell->noPrint);
		while (ecp != NULL) {
			if (cp != ecp) {
				*ecp = '\0';
				if (msg && job->node != lastNode) {
					MESSAGE(stdout, job->node);
					lastNode = job->node;
				}
				/*
				 * The only way there wouldn't be a newline
				 * after this line is if it were the last in
				 * the buffer. However, since the non-printable
				 * comes after it, there must be a newline, so
				 * we don't print one.
				 */
				fprintf(stdout, "%s", cp);
				fflush(stdout);
			}
			cp = ecp + strlen(commandShell->noPrint);
			if (cp != endp) {
				/*
				 * Still more to print, look again after
				 * skipping the whitespace following the
				 * non-printable command....
				 */
				cp++;
				while (*cp == ' ' || *cp == '\t' ||
				    *cp == '\n') {
					cp++;
				}
				ecp = strstr(cp, commandShell->noPrint);
			} else {
				return (cp);
			}
		}
	}
	return (cp);
}

/**
 * JobDoOutput
 *	This function is called at different times depending on
 *	whether the user has specified that output is to be collected
 *	via pipes or temporary files. In the former case, we are called
 *	whenever there is something to read on the pipe. We collect more
 *	output from the given job and store it in the job's outBuf. If
 *	this makes up a line, we print it tagged by the job's identifier,
 *	as necessary.
 *	If output has been collected in a temporary file, we open the
 *	file and read it line by line, transfering it to our own
 *	output channel until the file is empty. At which point we
 *	remove the temporary file.
 *	In both cases, however, we keep our figurative eye out for the
 *	'noPrint' line for the shell from which the output came. If
 *	we recognize a line, we don't print it. If the command is not
 *	alone on the line (the character after it is not \0 or \n), we
 *	do print whatever follows it.
 *
 * Side Effects:
 *	curPos may be shifted as may the contents of outBuf.
 */
static void
JobDoOutput(Job *job, Boolean finish)
{
	Boolean	gotNL = FALSE;	/* true if got a newline */
	Boolean	fbuf;		/* true if our buffer filled up */
	int	nr;		/* number of bytes read */
	int	i;		/* auxiliary index into outBuf */
	int	max;		/* limit for i (end of current data) */
	int	nRead;		/* (Temporary) number of bytes read */
	FILE	*oFILE;		/* Stream pointer to shell's output file */
	char	inLine[132];

	if (usePipes) {
		/*
		 * Read as many bytes as will fit in the buffer.
		 */
  end_loop:
		gotNL = FALSE;
		fbuf = FALSE;

		nRead = read(job->inPipe, &job->outBuf[job->curPos],
		    JOB_BUFSIZE - job->curPos);
		/*
		 * Check for interrupt here too, because the above read may
		 * block when the child process is stopped. In this case the
		 * interrupt will unblock it (we don't use SA_RESTART).
		 */
		if (interrupted)
			JobPassSig(interrupted);

		if (nRead < 0) {
			DEBUGF(JOB, ("JobDoOutput(piperead)"));
			nr = 0;
		} else {
			nr = nRead;
		}

		/*
		 * If we hit the end-of-file (the job is dead), we must flush
		 * its remaining output, so pretend we read a newline if
		 * there's any output remaining in the buffer.
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
		 * Look for the last newline in the bytes we just got. If there
		 * is one, break out of the loop with 'i' as its index and
		 * gotNL set TRUE.
		*/
		max = job->curPos + nr;
		for (i = job->curPos + nr - 1; i >= job->curPos; i--) {
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
				 * If we've run out of buffer space, we have
				 * no choice but to print the stuff. sigh.
				 */
				fbuf = TRUE;
				i = job->curPos;
			}
		}
		if (gotNL || fbuf) {
			/*
			 * Need to send the output to the screen. Null terminate
			 * it first, overwriting the newline character if there
			 * was one. So long as the line isn't one we should
			 * filter (according to the shell description), we print
			 * the line, preceded by a target banner if this target
			 * isn't the same as the one for which we last printed
			 * something. The rest of the data in the buffer are
			 * then shifted down to the start of the buffer and
			 * curPos is set accordingly.
			 */
			job->outBuf[i] = '\0';
			if (i >= job->curPos) {
				char *cp;

				cp = JobOutput(job, job->outBuf,
				    &job->outBuf[i], FALSE);

				/*
				 * There's still more in that buffer. This time,
				 * though, we know there's no newline at the
				 * end, so we add one of our own free will.
				 */
				if (*cp != '\0') {
					if (job->node != lastNode) {
						MESSAGE(stdout, job->node);
						lastNode = job->node;
					}
					fprintf(stdout, "%s%s", cp,
					    gotNL ? "\n" : "");
					fflush(stdout);
				}
			}
			if (i < max - 1) {
				/* shift the remaining characters down */
				memcpy(job->outBuf, &job->outBuf[i + 1],
				    max - (i + 1));
				job->curPos = max - (i + 1);

			} else {
				/*
				 * We have written everything out, so we just
				 * start over from the start of the buffer.
				 * No copying. No nothing.
				 */
				job->curPos = 0;
			}
		}
		if (finish) {
			/*
			 * If the finish flag is true, we must loop until we hit
			 * end-of-file on the pipe. This is guaranteed to happen
			 * eventually since the other end of the pipe is now
			 * closed (we closed it explicitly and the child has
			 * exited). When we do get an EOF, finish will be set
			 * FALSE and we'll fall through and out.
			 */
			goto end_loop;
		}

	} else {
		/*
		 * We've been called to retrieve the output of the job from the
		 * temporary file where it's been squirreled away. This consists
		 * of opening the file, reading the output line by line, being
		 * sure not to print the noPrint line for the shell we used,
		 * then close and remove the temporary file. Very simple.
		 *
		 * Change to read in blocks and do FindSubString type things
		 * as for pipes? That would allow for "@echo -n..."
		 */
		oFILE = fopen(job->outFile, "r");
		if (oFILE != NULL) {
			fprintf(stdout, "Results of making %s:\n",
			    job->node->name);
			fflush(stdout);

			while (fgets(inLine, sizeof(inLine), oFILE) != NULL) {
				char	*cp, *endp, *oendp;

				cp = inLine;
				oendp = endp = inLine + strlen(inLine);
				if (endp[-1] == '\n') {
					*--endp = '\0';
				}
				cp = JobOutput(job, inLine, endp, FALSE);

				/*
				 * There's still more in that buffer. This time,
				 * though, we know there's no newline at the
				 * end, so we add one of our own free will.
				 */
				fprintf(stdout, "%s", cp);
				fflush(stdout);
				if (endp != oendp) {
					fprintf(stdout, "\n");
					fflush(stdout);
				}
			}
			fclose(oFILE);
			eunlink(job->outFile);
		}
	}
}

/**
 * Job_CatchChildren
 *	Handle the exit of a child. Called from Make_Make.
 *
 * Side Effects:
 *	The job descriptor is removed from the list of children.
 *
 * Notes:
 *	We do waits, blocking or not, according to the wisdom of our
 *	caller, until there are no more children to report. For each
 *	job, call JobFinish to finish things off. This will take care of
 *	putting jobs on the stoppedJobs queue.
 */
void
Job_CatchChildren(Boolean block)
{
	pid_t	pid;	/* pid of dead child */
	Job	*job;	/* job descriptor for dead child */
	int	status;	/* Exit/termination status */

	/*
	 * Don't even bother if we know there's no one around.
	 */
	if (nJobs == 0) {
		return;
	}

	for (;;) {
		pid = waitpid((pid_t)-1, &status,
		    (block ? 0 : WNOHANG) | WUNTRACED);
		if (pid <= 0)
			break;

		DEBUGF(JOB, ("Process %jd exited or stopped.\n",
		    (intmax_t)pid));

		TAILQ_FOREACH(job, &jobs, link) {
			if (job->pid == pid)
				break;
		}

		if (job == NULL) {
			if (WIFSIGNALED(status) &&
			    (WTERMSIG(status) == SIGCONT)) {
				TAILQ_FOREACH(job, &jobs, link) {
					if (job->pid == pid)
						break;
				}
				if (job == NULL) {
					Error("Resumed child (%jd) "
					    "not in table", (intmax_t)pid);
					continue;
				}
				TAILQ_REMOVE(&stoppedJobs, job, link);
			} else {
				Error("Child (%jd) not in table?",
				    (intmax_t)pid);
				continue;
			}
		} else {
			TAILQ_REMOVE(&jobs, job, link);
			nJobs -= 1;
			if (fifoFd >= 0 && maxJobs > 1) {
				write(fifoFd, "+", 1);
				maxJobs--;
				if (nJobs >= maxJobs)
					jobFull = TRUE;
				else
					jobFull = FALSE;
			} else {
				DEBUGF(JOB, ("Job queue is no longer full.\n"));
				jobFull = FALSE;
			}
		}

		JobFinish(job, &status);
	}
	if (interrupted)
		JobPassSig(interrupted);
}

/**
 * Job_CatchOutput
 *	Catch the output from our children, if we're using
 *	pipes do so. Otherwise just block time until we get a
 *	signal(most likely a SIGCHLD) since there's no point in
 *	just spinning when there's nothing to do and the reaping
 *	of a child can wait for a while.
 *
 * Side Effects:
 *	Output is read from pipes if we're piping.
 * -----------------------------------------------------------------------
 */
void
#ifdef USE_KQUEUE
Job_CatchOutput(int flag __unused)
#else
Job_CatchOutput(int flag)
#endif
{
	int		nfds;
#ifdef USE_KQUEUE
#define KEV_SIZE	4
	struct kevent	kev[KEV_SIZE];
	int		i;
#else
	struct timeval	timeout;
	fd_set		readfds;
	Job		*job;
#endif

	fflush(stdout);

	if (usePipes) {
#ifdef USE_KQUEUE
		if ((nfds = kevent(kqfd, NULL, 0, kev, KEV_SIZE, NULL)) == -1) {
			if (errno != EINTR)
				Punt("kevent: %s", strerror(errno));
			if (interrupted)
				JobPassSig(interrupted);
		} else {
			for (i = 0; i < nfds; i++) {
				if (kev[i].flags & EV_ERROR) {
					warnc(kev[i].data, "kevent");
					continue;
				}
				switch (kev[i].filter) {
				  case EVFILT_READ:
					JobDoOutput(kev[i].udata, FALSE);
					break;
				  case EVFILT_PROC:
					/*
					 * Just wake up and let
					 * Job_CatchChildren() collect the
					 * terminated job.
					 */
					break;
				}
			}
		}
#else
		readfds = outputs;
		timeout.tv_sec = SEL_SEC;
		timeout.tv_usec = SEL_USEC;
		if (flag && jobFull && fifoFd >= 0)
			FD_SET(fifoFd, &readfds);

		nfds = select(FD_SETSIZE, &readfds, (fd_set *)NULL,
		    (fd_set *)NULL, &timeout);
		if (nfds <= 0) {
			if (interrupted)
				JobPassSig(interrupted);
			return;
		}
		if (fifoFd >= 0 && FD_ISSET(fifoFd, &readfds)) {
			if (--nfds <= 0)
				return;
		}
		job = TAILQ_FIRST(&jobs);
		while (nfds != 0 && job != NULL) {
			if (FD_ISSET(job->inPipe, &readfds)) {
				JobDoOutput(job, FALSE);
				nfds--;
			}
			job = TAILQ_NEXT(job, link);
		}
#endif /* !USE_KQUEUE */
	}
}

/**
 * Job_Make
 *	Start the creation of a target. Basically a front-end for
 *	JobStart used by the Make module.
 *
 * Side Effects:
 *	Another job is started.
 */
void
Job_Make(GNode *gn)
{

	JobStart(gn, 0, NULL);
}

/**
 * JobCopyShell
 *	Make a new copy of the shell structure including a copy of the strings
 *	in it. This also defaults some fields in case they are NULL.
 *
 * Returns:
 *	The function returns a pointer to the new shell structure.
 */
static struct Shell *
JobCopyShell(const struct Shell *osh)
{
	struct Shell *nsh;

	nsh = emalloc(sizeof(*nsh));
	nsh->name = estrdup(osh->name);

	if (osh->echoOff != NULL)
		nsh->echoOff = estrdup(osh->echoOff);
	else
		nsh->echoOff = NULL;
	if (osh->echoOn != NULL)
		nsh->echoOn = estrdup(osh->echoOn);
	else
		nsh->echoOn = NULL;
	nsh->hasEchoCtl = osh->hasEchoCtl;

	if (osh->noPrint != NULL)
		nsh->noPrint = estrdup(osh->noPrint);
	else
		nsh->noPrint = NULL;

	nsh->hasErrCtl = osh->hasErrCtl;
	if (osh->errCheck == NULL)
		nsh->errCheck = estrdup("");
	else
		nsh->errCheck = estrdup(osh->errCheck);
	if (osh->ignErr == NULL)
		nsh->ignErr = estrdup("%s");
	else
		nsh->ignErr = estrdup(osh->ignErr);

	if (osh->echo == NULL)
		nsh->echo = estrdup("");
	else
		nsh->echo = estrdup(osh->echo);

	if (osh->exit == NULL)
		nsh->exit = estrdup("");
	else
		nsh->exit = estrdup(osh->exit);

	return (nsh);
}

/**
 * JobFreeShell
 *	Free a shell structure and all associated strings.
 */
static void
JobFreeShell(struct Shell *sh)
{

	if (sh != NULL) {
		free(sh->name);
		free(sh->echoOff);
		free(sh->echoOn);
		free(sh->noPrint);
		free(sh->errCheck);
		free(sh->ignErr);
		free(sh->echo);
		free(sh->exit);
		free(sh);
	}
}

void
Shell_Init(void)
{

	if (commandShell == NULL)
		commandShell = JobMatchShell(shells[DEFSHELL].name);

	if (shellPath == NULL) {
		/*
		 * The user didn't specify a shell to use, so we are using the
		 * default one... Both the absolute path and the last component
		 * must be set. The last component is taken from the 'name'
		 * field of the default shell description pointed-to by
		 * commandShell. All default shells are located in
		 * PATH_DEFSHELLDIR.
		 */
		shellName = commandShell->name;
		shellPath = str_concat(PATH_DEFSHELLDIR, shellName,
		    STR_ADDSLASH);
	}
}

/**
 * Job_Init
 *	Initialize the process module, given a maximum number of jobs.
 *
 * Side Effects:
 *	lists and counters are initialized
 */
void
Job_Init(int maxproc)
{
	GNode		*begin;	/* node for commands to do at the very start */
	const char	*env;
	struct sigaction sa;

	fifoFd = -1;
	env = getenv("MAKE_JOBS_FIFO");

	if (env == NULL && maxproc > 1) {
		/*
		 * We did not find the environment variable so we are the
		 * leader. Create the fifo, open it, write one char per
		 * allowed job into the pipe.
		 */
		mktemp(fifoName);
		if (!mkfifo(fifoName, 0600)) {
			fifoFd = open(fifoName, O_RDWR | O_NONBLOCK, 0);
			if (fifoFd >= 0) {
				fifoMaster = 1;
				fcntl(fifoFd, F_SETFL, O_NONBLOCK);
				env = fifoName;
				setenv("MAKE_JOBS_FIFO", env, 1);
				while (maxproc-- > 0) {
					write(fifoFd, "+", 1);
				}
				/* The master make does not get a magic token */
				jobFull = TRUE;
				maxJobs = 0;
			} else {
				unlink(fifoName);
				env = NULL;
			}
		}

	} else if (env != NULL) {
		/*
		 * We had the environment variable so we are a slave.
		 * Open fifo and give ourselves a magic token which represents
		 * the token our parent make has grabbed to start his make
		 * process. Otherwise the sub-makes would gobble up tokens and
		 * the proper number of tokens to specify to -j would depend
		 * on the depth of the tree and the order of execution.
		 */
		fifoFd = open(env, O_RDWR, 0);
		if (fifoFd >= 0) {
			fcntl(fifoFd, F_SETFL, O_NONBLOCK);
			maxJobs = 1;
			jobFull = FALSE;
		}
	}
	if (fifoFd <= 0) {
		maxJobs = maxproc;
		jobFull = FALSE;
	} else {
	}
	nJobs = 0;

	aborting = 0;
	errors = 0;

	lastNode = NULL;

	if ((maxJobs == 1 && fifoFd < 0) || beVerbose == 0) {
		/*
		 * If only one job can run at a time, there's no need for a
		 * banner, no is there?
		 */
		targFmt = "";
	} else {
		targFmt = TARG_FMT;
	}

	Shell_Init();

	/*
	 * Catch the four signals that POSIX specifies if they aren't ignored.
	 * JobCatchSignal will just set global variables and hope someone
	 * else is going to handle the interrupt.
	 */
	sa.sa_handler = JobCatchSig;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		sigaction(SIGINT, &sa, NULL);
	}
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
		sigaction(SIGHUP, &sa, NULL);
	}
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN) {
		sigaction(SIGQUIT, &sa, NULL);
	}
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
		sigaction(SIGTERM, &sa, NULL);
	}
	/*
	 * There are additional signals that need to be caught and passed if
	 * either the export system wants to be told directly of signals or if
	 * we're giving each job its own process group (since then it won't get
	 * signals from the terminal driver as we own the terminal)
	 */
#if defined(USE_PGRP)
	if (signal(SIGTSTP, SIG_IGN) != SIG_IGN) {
		sigaction(SIGTSTP, &sa, NULL);
	}
	if (signal(SIGTTOU, SIG_IGN) != SIG_IGN) {
		sigaction(SIGTTOU, &sa, NULL);
	}
	if (signal(SIGTTIN, SIG_IGN) != SIG_IGN) {
		sigaction(SIGTTIN, &sa, NULL);
	}
	if (signal(SIGWINCH, SIG_IGN) != SIG_IGN) {
		sigaction(SIGWINCH, &sa, NULL);
	}
#endif

#ifdef USE_KQUEUE
	if ((kqfd = kqueue()) == -1) {
		Punt("kqueue: %s", strerror(errno));
	}
#endif

	begin = Targ_FindNode(".BEGIN", TARG_NOCREATE);

	if (begin != NULL) {
		JobStart(begin, JOB_SPECIAL, (Job *)NULL);
		while (nJobs) {
			Job_CatchOutput(0);
			Job_CatchChildren(!usePipes);
		}
	}
	postCommands = Targ_FindNode(".END", TARG_CREATE);
}

/**
 * Job_Full
 *	See if the job table is full. It is considered full if it is OR
 *	if we are in the process of aborting OR if we have
 *	reached/exceeded our local quota. This prevents any more jobs
 *	from starting up.
 *
 * Results:
 *	TRUE if the job table is full, FALSE otherwise
 */
Boolean
Job_Full(void)
{
	char c;
	int i;

	if (aborting)
		return (aborting);
	if (fifoFd >= 0 && jobFull) {
		i = read(fifoFd, &c, 1);
		if (i > 0) {
			maxJobs++;
			jobFull = FALSE;
		}
	}
	return (jobFull);
}

/**
 * Job_Empty
 *	See if the job table is empty.  Because the local concurrency may
 *	be set to 0, it is possible for the job table to become empty,
 *	while the list of stoppedJobs remains non-empty. In such a case,
 *	we want to restart as many jobs as we can.
 *
 * Results:
 *	TRUE if it is. FALSE if it ain't.
 */
Boolean
Job_Empty(void)
{
	if (nJobs == 0) {
		if (!TAILQ_EMPTY(&stoppedJobs) && !aborting) {
			/*
			 * The job table is obviously not full if it has no
			 * jobs in it...Try and restart the stopped jobs.
			 */
			jobFull = FALSE;
			JobRestartJobs();
			return (FALSE);
		} else {
			return (TRUE);
		}
	} else {
		return (FALSE);
	}
}

/**
 * JobMatchShell
 *	Find a matching shell in 'shells' given its final component.
 *
 * Results:
 *	A pointer to a freshly allocated Shell structure with a copy
 *	of the static structure or NULL if no shell with the given name
 *	is found.
 */
static struct Shell *
JobMatchShell(const char *name)
{
	const struct CShell	*sh;	      /* Pointer into shells table */
	struct Shell		*nsh;

	for (sh = shells; sh < shells + sizeof(shells)/sizeof(shells[0]); sh++)
		if (strcmp(sh->name, name) == 0)
			break;

	if (sh == shells + sizeof(shells)/sizeof(shells[0]))
		return (NULL);

	/* make a copy */
	nsh = emalloc(sizeof(*nsh));

	nsh->name = estrdup(sh->name);
	nsh->echoOff = estrdup(sh->echoOff);
	nsh->echoOn = estrdup(sh->echoOn);
	nsh->hasEchoCtl = sh->hasEchoCtl;
	nsh->noPrint = estrdup(sh->noPrint);
	nsh->hasErrCtl = sh->hasErrCtl;
	nsh->errCheck = estrdup(sh->errCheck);
	nsh->ignErr = estrdup(sh->ignErr);
	nsh->echo = estrdup(sh->echo);
	nsh->exit = estrdup(sh->exit);

	return (nsh);
}

/**
 * Job_ParseShell
 *	Parse a shell specification and set up commandShell, shellPath
 *	and shellName appropriately.
 *
 * Results:
 *	FAILURE if the specification was incorrect.
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
 *	    name  	    Name of shell.
 *	    path  	    Location of shell. Overrides "name" if given
 *	    quiet 	    Command to turn off echoing.
 *	    echo  	    Command to turn echoing on
 *	    filter	    Result of turning off echoing that shouldn't be
 *	    	  	    printed.
 *	    echoFlag	    Flag to turn echoing on at the start
 *	    errFlag	    Flag to turn error checking on at the start
 *	    hasErrCtl	    True if shell has error checking control
 *	    check 	    Command to turn on error checking if hasErrCtl
 *	    	  	    is TRUE or template of command to echo a command
 *	    	  	    for which error checking is off if hasErrCtl is
 *	    	  	    FALSE.
 *	    ignore	    Command to turn off error checking if hasErrCtl
 *	    	  	    is TRUE or template of command to execute a
 *	    	  	    command so as to ignore any errors it returns if
 *	    	  	    hasErrCtl is FALSE.
 */
ReturnStatus
Job_ParseShell(char *line)
{
	char	**words;
	int	wordCount;
	char	**argv;
	int	argc;
	char	*path;
	char	*eq;
	Boolean	fullSpec = FALSE;
	struct Shell	newShell;
	struct Shell	*sh;

	while (isspace((unsigned char)*line)) {
		line++;
	}
	words = brk_string(line, &wordCount, TRUE);

	memset(&newShell, 0, sizeof(newShell));
	path = NULL;

	/*
	 * Parse the specification by keyword but skip the first word - it
	 * is not set by brk_string.
	 */
	wordCount--;
	words++;

	for (argc = wordCount, argv = words; argc != 0; argc--, argv++) {
		/*
		 * Split keyword and value
		 */
		if ((eq = strchr(*argv, '=')) == NULL) {
			Parse_Error(PARSE_FATAL, "missing '=' in shell "
			    "specification keyword '%s'", *argv);
			return (FAILURE);
		}
		*eq++ = '\0';

		if (strcmp(*argv, "path") == 0) {
			path = eq;
		} else if (strcmp(*argv, "name") == 0) {
			newShell.name = eq;
		} else if (strcmp(*argv, "quiet") == 0) {
			newShell.echoOff = eq;
			fullSpec = TRUE;
		} else if (strcmp(*argv, "echo") == 0) {
			newShell.echoOn = eq;
			fullSpec = TRUE;
		} else if (strcmp(*argv, "filter") == 0) {
			newShell.noPrint = eq;
			fullSpec = TRUE;
		} else if (strcmp(*argv, "echoFlag") == 0) {
			newShell.echo = eq;
			fullSpec = TRUE;
		} else if (strcmp(*argv, "errFlag") == 0) {
			newShell.exit = eq;
			fullSpec = TRUE;
		} else if (strcmp(*argv, "hasErrCtl") == 0) {
			newShell.hasErrCtl = (*eq == 'Y' || *eq == 'y' ||
			    *eq == 'T' || *eq == 't');
			fullSpec = TRUE;
		} else if (strcmp(*argv, "check") == 0) {
			newShell.errCheck = eq;
			fullSpec = TRUE;
		} else if (strcmp(*argv, "ignore") == 0) {
			newShell.ignErr = eq;
			fullSpec = TRUE;
		} else {
			Parse_Error(PARSE_FATAL, "unknown keyword in shell "
			    "specification '%s'", *argv);
			return (FAILURE);
		}
	}

	/*
	 * Some checks (could be more)
	 */
	if (fullSpec) {
		if ((newShell.echoOn != NULL) ^ (newShell.echoOff != NULL))
			Parse_Error(PARSE_FATAL, "Shell must have either both "
			    "echoOff and echoOn or none of them");

		if (newShell.echoOn != NULL && newShell.echoOff)
			newShell.hasEchoCtl = TRUE;
	}

	if (path == NULL) {
		/*
		 * If no path was given, the user wants one of the pre-defined
		 * shells, yes? So we find the one s/he wants with the help of
		 * JobMatchShell and set things up the right way. shellPath
		 * will be set up by Job_Init.
		 */
		if (newShell.name == NULL) {
			Parse_Error(PARSE_FATAL,
			    "Neither path nor name specified");
			return (FAILURE);
		}
		if ((sh = JobMatchShell(newShell.name)) == NULL) {
			Parse_Error(PARSE_FATAL, "%s: no matching shell",
			    newShell.name);
			return (FAILURE);
		}

	} else {
		/*
		 * The user provided a path. If s/he gave nothing else
		 * (fullSpec is FALSE), try and find a matching shell in the
		 * ones we know of. Else we just take the specification at its
		 * word and copy it to a new location. In either case, we need
		 * to record the path the user gave for the shell.
		 */
		free(shellPath);
		shellPath = estrdup(path);
		if (newShell.name == NULL) {
			/* get the base name as the name */
			path = strrchr(path, '/');
			if (path == NULL) {
				path = shellPath;
			} else {
				path += 1;
			}
			newShell.name = path;
		}

		if (!fullSpec) {
			if ((sh = JobMatchShell(newShell.name)) == NULL) {
				Parse_Error(PARSE_FATAL,
				    "%s: no matching shell", newShell.name);
				return (FAILURE);
			}
		} else {
			sh = JobCopyShell(&newShell);
		}
	}

	/* set the new shell */
	JobFreeShell(commandShell);
	commandShell = sh;

	shellName = commandShell->name;

	return (SUCCESS);
}

/**
 * JobInterrupt
 *	Handle the receipt of an interrupt.
 *
 * Side Effects:
 *	All children are killed. Another job will be started if the
 *	.INTERRUPT target was given.
 */
static void
JobInterrupt(int runINTERRUPT, int signo)
{
	Job	*job;		/* job descriptor in that element */
	GNode	*interrupt;	/* the node describing the .INTERRUPT target */

	aborting = ABORT_INTERRUPT;

	TAILQ_FOREACH(job, &jobs, link) {
		if (!Targ_Precious(job->node)) {
			char *file = (job->node->path == NULL ?
			    job->node->name : job->node->path);

			if (!noExecute && eunlink(file) != -1) {
				Error("*** %s removed", file);
			}
		}
		if (job->pid) {
			DEBUGF(JOB, ("JobInterrupt passing signal to child "
			    "%jd.\n", (intmax_t)job->pid));
			KILL(job->pid, signo);
		}
	}

	if (runINTERRUPT && !touchFlag) {
		/*
		 * clear the interrupted flag because we would get an
		 * infinite loop otherwise.
		 */
		interrupted = 0;

		interrupt = Targ_FindNode(".INTERRUPT", TARG_NOCREATE);
		if (interrupt != NULL) {
			ignoreErrors = FALSE;

			JobStart(interrupt, JOB_IGNDOTS, (Job *)NULL);
			while (nJobs) {
				Job_CatchOutput(0);
				Job_CatchChildren(!usePipes);
			}
		}
	}
}

/**
 * Job_Finish
 *	Do final processing such as the running of the commands
 *	attached to the .END target.
 *
 * Results:
 *	Number of errors reported.
 */
int
Job_Finish(void)
{

	if (postCommands != NULL && !Lst_IsEmpty(&postCommands->commands)) {
		if (errors) {
			Error("Errors reported so .END ignored");
		} else {
			JobStart(postCommands, JOB_SPECIAL | JOB_IGNDOTS, NULL);

			while (nJobs) {
				Job_CatchOutput(0);
				Job_CatchChildren(!usePipes);
			}
		}
	}
	if (fifoFd >= 0) {
		close(fifoFd);
		fifoFd = -1;
		if (fifoMaster)
			unlink(fifoName);
	}
	return (errors);
}

/**
 * Job_Wait
 *	Waits for all running jobs to finish and returns. Sets 'aborting'
 *	to ABORT_WAIT to prevent other jobs from starting.
 *
 * Side Effects:
 *	Currently running jobs finish.
 */
void
Job_Wait(void)
{

	aborting = ABORT_WAIT;
	while (nJobs != 0) {
		Job_CatchOutput(0);
		Job_CatchChildren(!usePipes);
	}
	aborting = 0;
}

/**
 * Job_AbortAll
 *	Abort all currently running jobs without handling output or anything.
 *	This function is to be called only in the event of a major
 *	error. Most definitely NOT to be called from JobInterrupt.
 *
 * Side Effects:
 *	All children are killed, not just the firstborn
 */
void
Job_AbortAll(void)
{
	Job	*job;	/* the job descriptor in that element */
	int	foo;

	aborting = ABORT_ERROR;

	if (nJobs) {
		TAILQ_FOREACH(job, &jobs, link) {
			/*
			 * kill the child process with increasingly drastic
			 * signals to make darn sure it's dead.
			 */
			KILL(job->pid, SIGINT);
			KILL(job->pid, SIGKILL);
		}
	}

	/*
	 * Catch as many children as want to report in at first, then give up
	 */
	while (waitpid((pid_t)-1, &foo, WNOHANG) > 0)
		;
}

/**
 * JobRestartJobs
 *	Tries to restart stopped jobs if there are slots available.
 *	Note that this tries to restart them regardless of pending errors.
 *	It's not good to leave stopped jobs lying around!
 *
 * Side Effects:
 *	Resumes(and possibly migrates) jobs.
 */
static void
JobRestartJobs(void)
{
	Job *job;

	while (!jobFull && (job = TAILQ_FIRST(&stoppedJobs)) != NULL) {
		DEBUGF(JOB, ("Job queue is not full. "
		    "Restarting a stopped job.\n"));
		TAILQ_REMOVE(&stoppedJobs, job, link);
		JobRestart(job);
	}
}

/**
 * Cmd_Exec
 *	Execute the command in cmd, and return the output of that command
 *	in a string.
 *
 * Results:
 *	A string containing the output of the command, or the empty string
 *	If error is not NULL, it contains the reason for the command failure
 *
 * Side Effects:
 *	The string must be freed by the caller.
 */
Buffer *
Cmd_Exec(const char *cmd, const char **error)
{
	int	fds[2];	/* Pipe streams */
	int	cpid;	/* Child PID */
	int	pid;	/* PID from wait() */
	int	status;	/* command exit status */
	Buffer	*buf;	/* buffer to store the result */
	ssize_t	rcnt;

	*error = NULL;
	buf = Buf_Init(0);

	if (shellPath == NULL)
		Shell_Init();
	/*
	 * Open a pipe for fetching its output
	 */
	if (pipe(fds) == -1) {
		*error = "Couldn't create pipe for \"%s\"";
		return (buf);
	}

	/*
	 * Fork
	 */
	if ((cpid = vfork()) == -1) {
		*error = "Couldn't exec \"%s\"";
		return (buf);
	}

	if (cpid == 0) {
		char	*args[4];
		/*
		 * Close input side of pipe
		 */
		close(fds[0]);

		/*
		 * Duplicate the output stream to the shell's output, then
		 * shut the extra thing down. Note we don't fetch the error
		 * stream...why not? Why?
		 */
		dup2(fds[1], 1);
		close(fds[1]);


		/* Set up arguments for shell */
		args[0] = shellName;
		args[1] = "-c";
		args[2] = cmd;
		args[3] = NULL;

		execv(shellPath, args);
		_exit(1);
		/* NOTREACHED */

	}
	/*
	 * No need for the writing half
	 */
	close(fds[1]);

	do {
		char	result[BUFSIZ];

		rcnt = read(fds[0], result, sizeof(result));
		if (rcnt != -1)
			Buf_AddBytes(buf, (size_t)rcnt, (Byte *)result);
	} while (rcnt > 0 || (rcnt == -1 && errno == EINTR));

	if (rcnt == -1)
		*error = "Error reading shell's output for \"%s\"";

	/*
	 * Close the input side of the pipe.
	 */
	close(fds[0]);

	/*
	 * Wait for the process to exit.
	 */
	while (((pid = wait(&status)) != cpid) && (pid >= 0))
		continue;

	if (status)
		*error = "\"%s\" returned non-zero status";

	Buf_StripNewlines(buf);

	return (buf);
}


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

/*
 * The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.
 */
static const char *sh_builtin[] = {
	"alias", "cd", "eval", "exec",
	"exit", "read", "set", "ulimit",
	"unalias", "umask", "unset", "wait",
	":", NULL
};

static char 	    meta[256];

static GNode	    *curTarg = NULL;
static GNode	    *ENDNode;

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
	char		**av;
	const char	**p;

	av = brk_string(cmd, NULL, TRUE);
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
	cmdStart = Buf_Peel(Var_Subst(cmd, gn, FALSE));

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
	if ((cpid = vfork()) == -1) {
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
	 * we need to print out the command associated with this
	 * Gnode in Targ_PrintCmd from Targ_PrintGraph when debugging
	 * at level g2, in main(), Fatal() and DieHorribly(),
	 * therefore do not free it when debugging.
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
				/* stopped */
				status = WSTOPSIG(reason);
			} else if (WIFEXITED(reason)) {
				/* exited */
				status = WEXITSTATUS(reason);
				if (status != 0) {
					printf("*** Error code %d",
					    status);
				}
			} else {
				/* signaled */
				status = WTERMSIG(reason);
				printf("*** Signal %d", status);
			}

			if (!WIFEXITED(reason) || status != 0) {
				if (errCheck) {
					gn->made = ERROR;
					if (keepgoing) {
						/*
						 * Abort the current
						 * target, but let
						 * others continue.
						 */
						printf(" (continuing)\n");
					}
				} else {
					/*
					 * Continue executing
					 * commands for this target.
					 * If we return 0, this will
					 * happen...
					 */
					printf(" (ignored)\n");
					status = 0;
				}
			}
			break;
		} else {
			Fatal("error in wait: %d", rstat);
			/* NOTREACHED */
		}
	}

	return (status);
}

/*-
 *-----------------------------------------------------------------------
 * CompatMake --
 *	Make a target, given the parent, to abort if necessary.
 *
 * Side Effects:
 *	If an error is detected and not being ignored, the process exits.
 *
 *-----------------------------------------------------------------------
 */
static int
CompatMake(GNode *gn, GNode *pgn)
{
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
		LST_FOREACH(ln, &gn->children)
			CompatMake(Lst_Datum(ln), gn);
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
	int	error_cnt;		/* Number of targets not remade due to errors */
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
	error_cnt = 0;
	while (!Lst_IsEmpty(targs)) {
		gn = Lst_DeQueue(targs);
		CompatMake(gn, gn);

		if (gn->made == UPTODATE) {
			printf("`%s' is up to date.\n", gn->name);
		} else if (gn->made == ABORTED) {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
			error_cnt += 1;
		}
	}

	/*
	 * If the user has defined a .END target, run its commands.
	 */
	if (error_cnt == 0) {
		LST_FOREACH(ln, &ENDNode->commands) {
			if (Compat_RunCommand(Lst_Datum(ln), gn))
				break;
		}
	}
}
