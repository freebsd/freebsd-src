/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*  run, runv, runp, runvp --  execute process and wait for it to exit
 *
 *  Usage:
 *	i = run (file, arg1, arg2, ..., argn, 0);
 *	i = runv (file, arglist);
 *	i = runp (file, arg1, arg2, ..., argn, 0);
 *	i = runvp (file, arglist);
 *
 *  Run, runv, runp and runvp have argument lists exactly like the
 *  corresponding routines, execl, execv, execlp, execvp.  The run
 *  routines perform a fork, then:
 *  IN THE NEW PROCESS, an execl[p] or execv[p] is performed with the
 *  specified arguments.  The process returns with a -1 code if the
 *  exec was not successful.
 *  IN THE PARENT PROCESS, the signals SIGQUIT and SIGINT are disabled,
 *  the process waits until the newly forked process exits, the
 *  signals are restored to their original status, and the return
 *  status of the process is analyzed.
 *  All run routines return:  -1 if the exec failed or if the child was
 *  terminated abnormally; otherwise, the exit code of the child is
 *  returned.
 *
 **********************************************************************
 * HISTORY
 * $Log: run.c,v $
 * Revision 1.1.1.1  1993/08/21  00:46:33  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:17  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.1  89/10/14  19:53:39  rvb
 * Initial revision
 * 
 * Revision 1.2  89/08/03  14:36:46  mja
 * 	Update run() and runp() to use <varargs.h>.
 * 	[89/04/19            mja]
 * 
 * 23-Sep-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Merged old runv and runvp modules.
 *
 * 22-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added check and kill if child process was stopped.
 *
 * 30-Apr-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Adapted for 4.2 BSD UNIX:  Conforms to new signals and wait.
 *
 * 15-July-82 Mike Accetta (mja) and Neal Friedman (naf)
 *				  at Carnegie-Mellon University
 *	Added a return(-1) if vfork fails.  This should only happen
 *	if there are no more processes available.
 *
 * 28-Jan-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added setuid and setgid for system programs' use.
 *
 * 21-Jan-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Changed fork to vfork.
 *
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Created for VAX.  The proper way to fork-and-execute a system
 *	program is now by "runvp" or "runp", with the program name
 *	(rather than an absolute pathname) as the first argument;
 *	that way, the "PATH" variable in the environment does the right
 *	thing.  Too bad execvp and execlp (hence runvp and runp) don't
 *	accept a pathlist as an explicit argument.
 *
 **********************************************************************
 */

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <varargs.h>

static int dorun();

int run (name,va_alist)
char *name;
va_dcl
{
	int val;
	va_list ap;

	va_start(ap);
	val = runv (name,ap);
	va_end(ap);
	return(val);
}

int runv (name,argv)
char *name,**argv;
{
	return (dorun (name, argv, 0));
}

int runp (name,va_alist)
char *name;
va_dcl
{
	int val;
	va_list ap;

	va_start(ap);
	val = runvp (name,ap);
	va_end(ap);
	return (val);
}

int runvp (name,argv)
char *name,**argv;
{
	return (dorun (name, argv, 1));
}

static
int dorun (name,argv,usepath)
char *name,**argv;
int usepath;
{
	int wpid;
	register int pid;
	struct sigvec ignoresig,intsig,quitsig;
	union wait status;
	int execvp(), execv();
	int (*execrtn)() = usepath ? execvp : execv;

	if ((pid = vfork()) == -1)
		return(-1);	/* no more process's, so exit with error */

	if (pid == 0) {			/* child process */
		setgid (getgid());
		setuid (getuid());
		(*execrtn) (name,argv);
		fprintf (stderr,"run: can't exec %s\n",name);
		_exit (0377);
	}

	ignoresig.sv_handler = SIG_IGN;	/* ignore INT and QUIT signals */
	ignoresig.sv_mask = 0;
	ignoresig.sv_onstack = 0;
	sigvec (SIGINT,&ignoresig,&intsig);
	sigvec (SIGQUIT,&ignoresig,&quitsig);
	do {
		wpid = wait3 (&status.w_status, WUNTRACED, 0);
		if (WIFSTOPPED (status)) {
		    kill (0,SIGTSTP);
		    wpid = 0;
		}
	} while (wpid != pid && wpid != -1);
	sigvec (SIGINT,&intsig,0);	/* restore signals */
	sigvec (SIGQUIT,&quitsig,0);

	if (WIFSIGNALED (status) || status.w_retcode == 0377)
		return (-1);

	return (status.w_retcode);
}
