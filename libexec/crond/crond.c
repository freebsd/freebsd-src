#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/crond.c,v 1.1.1.1 1993/06/12 14:55:04 rgrimes Exp $";
#endif

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00131
 * --------------------         -----   ----------------------
 *
 * 06 Apr 93	Adam Glass	Fixes so it compiles quitely
 *
 */


#define	MAIN_PROGRAM


#include "cron.h"
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/types.h>
#if defined(BSD)
# include <sys/wait.h>
# include <sys/resource.h>
#endif /*BSD*/

extern int	fork(), unlink();
extern time_t	time();
extern void	exit();
extern unsigned	sleep();

void
usage()
{
	(void) fprintf(stderr, "usage:  %s [-x debugflag[,...]]\n", ProgramName);
	(void) exit(ERROR_EXIT);
}


int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	extern void	set_cron_uid(), be_different(), load_database(),
			set_cron_cwd(), open_logfile();

	static void	cron_tick(), cron_sleep(), cron_sync(),
			sigchld_handler(), parse_args(), run_reboot_jobs();

	auto cron_db	database;

	ProgramName = argv[0];

#if defined(BSD)
	setlinebuf(stdout);
	setlinebuf(stderr);
#endif

	parse_args(argc, argv);

# if DEBUGGING
	/* if there are no debug flags turned on, fork as a daemon should.
	 */
	if (DebugFlags)
	{
		(void) fprintf(stderr, "[%d] crond started\n", getpid());
	}
	else
# endif /*DEBUGGING*/
	{
		switch (fork())
		{
		case -1:
			log_it("CROND",getpid(),"DEATH","can't fork");
			exit(0);
			break;
		case 0:
			/* child process */
			be_different();
			break;
		default:
			/* parent process should just die */
			_exit(0);
		}
	}

#if defined(BSD)
	(void) signal(SIGCHLD, sigchld_handler);
#endif /*BSD*/

#if defined(ATT)
	(void) signal(SIGCLD, SIG_IGN);
#endif /*ATT*/

	acquire_daemonlock();
	set_cron_uid();
	set_cron_cwd();
	database.head = NULL;
	database.tail = NULL;
	database.mtime = (time_t) 0;
	load_database(&database);
	run_reboot_jobs(&database);
	cron_sync();
	while (TRUE)
	{
# if DEBUGGING
		if (!(DebugFlags & DTEST))
# endif /*DEBUGGING*/
			cron_sleep();

		load_database(&database);

		/* do this iteration
		 */
		cron_tick(&database);

		/* sleep 1 minute
		 */
		TargetTime += 60;
	}
}


static void
run_reboot_jobs(db)
	cron_db *db;
{
	extern void		job_add();
	extern int		job_runqueue();
	register user		*u;
	register entry		*e;

	for (u = db->head;  u != NULL;  u = u->next) {
		for (e = u->crontab;  e != NULL;  e = e->next) {
			if (e->flags & WHEN_REBOOT) {
				job_add(e->cmd, u);
			}
		}
	}
	(void) job_runqueue();
}


static void
cron_tick(db)
	cron_db	*db;
{
	extern void		job_add();
	extern char		*env_get();
	extern struct tm	*localtime();
 	register struct tm	*tm = localtime(&TargetTime);
	local int		minute, hour, dom, month, dow;
	register user		*u;
	register entry		*e;

	/* make 0-based values out of these so we can use them as indicies
	 */
	minute = tm->tm_min -FIRST_MINUTE;
	hour = tm->tm_hour -FIRST_HOUR;
	dom = tm->tm_mday -FIRST_DOM;
	month = tm->tm_mon +1 /* 0..11 -> 1..12 */ -FIRST_MONTH;
	dow = tm->tm_wday -FIRST_DOW;

	Debug(DSCH, ("[%d] tick(%d,%d,%d,%d,%d)\n",
		getpid(), minute, hour, dom, month, dow))

	/* the dom/dow situation is odd.  '* * 1,15 * Sun' will run on the
	 * first and fifteenth AND every Sunday;  '* * * * Sun' will run *only*
	 * on Sundays;  '* * 1,15 * *' will run *only* the 1st and 15th.  this
	 * is why we keep 'e->dow_star' and 'e->dom_star'.  yes, it's bizarre.
	 * like many bizarre things, it's the standard.
	 */
	for (u = db->head;  u != NULL;  u = u->next) {
		Debug(DSCH|DEXT, ("user [%s:%d:%d:...]\n",
			env_get(USERENV,u->envp), u->uid, u->gid))
		for (e = u->crontab;  e != NULL;  e = e->next) {
			Debug(DSCH|DEXT, ("entry [%s]\n", e->cmd))
			if (bit_test(e->minute, minute)
			 && bit_test(e->hour, hour)
			 && bit_test(e->month, month)
			 && ( ((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
			      ? (bit_test(e->dow,dow) && bit_test(e->dom,dom))
			      : (bit_test(e->dow,dow) || bit_test(e->dom,dom))
			    )
			   ) {
				job_add(e->cmd, u);
			}
		}
	}
}


/* the task here is to figure out how long it's going to be until :00 of the
 * following minute and initialize TargetTime to this value.  TargetTime
 * will subsequently slide 60 seconds at a time, with correction applied
 * implicitly in cron_sleep().  it would be nice to let crond execute in
 * the "current minute" before going to sleep, but by restarting cron you
 * could then get it to execute a given minute's jobs more than once.
 * instead we have the chance of missing a minute's jobs completely, but
 * that's something sysadmin's know to expect what with crashing computers..
 */
static void
cron_sync()
{
	extern struct tm	*localtime();
 	register struct tm	*tm;

	TargetTime = time((time_t*)0);
	tm = localtime(&TargetTime);
	TargetTime += (60 - tm->tm_sec);
}


static void
cron_sleep()
{
	extern void	do_command();
	extern int	job_runqueue();
	register int	seconds_to_wait;

	do {
		seconds_to_wait = (int) (TargetTime - time((time_t*)0));
		Debug(DSCH, ("[%d] TargetTime=%ld, sec-to-wait=%d\n",
			getpid(), TargetTime, seconds_to_wait))

		/* if we intend to sleep, this means that it's finally
		 * time to empty the job queue (execute it).
		 *
		 * if we run any jobs, we'll probably screw up our timing,
		 * so go recompute.
		 *
		 * note that we depend here on the left-to-right nature
		 * of &&, and the short-circuiting.
		 */
	} while (seconds_to_wait > 0 && job_runqueue());

	if (seconds_to_wait > 0)
	{
		Debug(DSCH, ("[%d] sleeping for %d seconds\n",
			getpid(), seconds_to_wait))
		(void) sleep((unsigned int) seconds_to_wait);
	}
}


#if defined(BSD)
static void
sigchld_handler()
{
	union wait	waiter;
	int		pid;

	for (;;)
	{
		pid = wait3((int *) &waiter, WNOHANG, (struct rusage *)0);
		switch (pid)
		{
		case -1:
			Debug(DPROC,
				("[%d] sigchld...no children\n", getpid()))
			return;
		case 0:
			Debug(DPROC,
				("[%d] sigchld...no dead kids\n", getpid()))
			return;
		default:
			Debug(DPROC,
				("[%d] sigchld...pid #%d died, stat=%d\n",
				getpid(), pid, waiter.w_status))
		}
	}
}
#endif /*BSD*/


static void
parse_args(argc, argv)
	int	argc;
	char	*argv[];
{
	extern	int	optind, getopt();
	extern	void	usage();
	extern	char	*optarg;

	int	argch;

	while (EOF != (argch = getopt(argc, argv, "x:")))
	{
		switch (argch)
		{
		default:
			usage();
		case 'x':
			if (!set_debug_flags(optarg))
				usage();
			break;
		}
	}
}
