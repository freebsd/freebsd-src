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
 *	@(#)job.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

#ifndef job_h_4678dfd1
#define	job_h_4678dfd1

/*-
 * job.h --
 *	Definitions pertaining to the running of jobs in parallel mode.
 */

#include <stdio.h>

#include "sprite.h"

struct GNode;
struct LstNode;

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
	int		pid;	/* The child's process ID */

	/* Temporary file to use for job */
	char		tfile[sizeof(TMPPAT)];

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
} Job;

#define	outPipe	  	output.o_pipe.op_outPipe
#define	inPipe	  	output.o_pipe.op_inPipe
#define	outBuf		output.o_pipe.op_outBuf
#define	curPos		output.o_pipe.op_curPos
#define	outFile		output.o_file.of_outFile
#define	outFd	  	output.o_file.of_outFd

/*-
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
	int		noPLen;		/* length of noPrint command */	\
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

typedef DEF_SHELL_STRUCT(Shell,) Shell;

extern char *shellPath;
extern char *shellName;
extern int	maxJobs;	/* Number of jobs that may run */

void Shell_Init(void);
void Job_Touch(struct GNode *, Boolean);
Boolean Job_CheckCommands(struct GNode *, void (*abortProc)(const char *, ...));
void Job_CatchChildren(Boolean);
void Job_CatchOutput(int flag);
void Job_Make(struct GNode *);
void Job_Init(int);
Boolean Job_Full(void);
Boolean Job_Empty(void);
ReturnStatus Job_ParseShell(char *);
int Job_Finish(void);
void Job_Wait(void);
void Job_AbortAll(void);

#endif /* job_h_4678dfd1 */
