/*-
 * Copyright (c) 2003-2005, Joseph Koshy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/event.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pmc.h>
#include <pmclog.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/*
 * A given invocation of pmcstat(8) can manage multiple PMCs of both
 * the system-wide and per-process variety.  Each of these could be in
 * 'counting mode' or in 'sampling mode'.
 *
 * For 'counting mode' PMCs, pmcstat(8) will periodically issue a
 * pmc_read() at the configured time interval and print out the value
 * of the requested PMCs.
 *
 * For 'sampling mode' PMCs it can log to a file for offline analysis,
 * or can analyse sampling data "on the fly", either by converting
 * samples to printed textual form or by creating gprof(1) compatible
 * profiles, one per program executed.  When creating gprof(1)
 * profiles it can optionally merge entries from multiple processes
 * for a given executable into a single profile file.
 */

/* Operation modes */

#define	FLAG_HAS_PID			0x00000001
#define	FLAG_HAS_WAIT_INTERVAL		0x00000002
#define	FLAG_HAS_LOG_FILE		0x00000004
#define	FLAG_HAS_PROCESS		0x00000008
#define	FLAG_HAS_SAMPLING_PMCS		0x00000010
#define	FLAG_HAS_COUNTING_PMCS		0x00000020
#define	FLAG_HAS_PROCESS_PMCS		0x00000040
#define	FLAG_HAS_SYSTEM_PMCS		0x00000080
#define	FLAG_HAS_PIPE			0x00000100
#define	FLAG_PROCESS_LOGFILE		0x00000200
#define	FLAG_DO_GPROF			0x00000400
#define	FLAG_DO_GPROF_MERGED		0x00000800

#define	DEFAULT_SAMPLE_COUNT		65536
#define	DEFAULT_WAIT_INTERVAL		5.0
#define	DEFAULT_DISPLAY_HEIGHT		23
#define	DEFAULT_BUFFER_SIZE		4096

#define	WRITELOG_MAGIC			0xA55AA55A
#define	PRINT_HEADER_PREFIX		"# "
#define	READPIPEFD			0
#define	WRITEPIPEFD			1
#define	NPIPEFD				2

enum pmcstat_state {
	PMCSTAT_FINISHED = 0,
	PMCSTAT_EXITING  = 1,
	PMCSTAT_RUNNING  = 2
};

struct pmcstat_ev {
	STAILQ_ENTRY(pmcstat_ev) ev_next;
	char	       *ev_spec;  /* event specification */
	char	       *ev_name;  /* (derived) event name */
	enum pmc_mode	ev_mode;  /* desired mode */
	int		ev_count; /* associated count if in sampling mode */
	int		ev_cpu;	  /* specific cpu if requested */
	int		ev_flags; /* PMC_F_* */
	int		ev_cumulative;  /* show cumulative counts */
	int		ev_fieldwidth;  /* print width */
	int		ev_fieldskip;   /* #leading spaces */
	pmc_value_t	ev_saved; /* saved value for incremental counts */
	pmc_id_t	ev_pmcid; /* allocated ID */
};

struct pmcstat_args {
	int	pa_required;
	int	pa_flags;
	pid_t	pa_pid;
	FILE	*pa_outputfile;
	FILE	*pa_logfile;
	void	*pa_logparser;
	char	*pa_outputdir;
	double	pa_interval;
	int	pa_argc;
	char	**pa_argv;
	STAILQ_HEAD(, pmcstat_ev) pa_head;
} args;

int	pmcstat_interrupt = 0;
int	pmcstat_displayheight = DEFAULT_DISPLAY_HEIGHT;
int	pmcstat_pipefd[NPIPEFD];
int	pmcstat_kq;

/* Function prototypes */
void	pmcstat_cleanup(struct pmcstat_args *_a);
int	pmcstat_close_log(struct pmcstat_args *_a);
void	pmcstat_print_counters(struct pmcstat_args *_a);
void	pmcstat_print_headers(struct pmcstat_args *_a);
void	pmcstat_print_pmcs(struct pmcstat_args *_a);
void	pmcstat_setup_process(struct pmcstat_args *_a);
void	pmcstat_show_usage(void);
void	pmcstat_start_pmcs(struct pmcstat_args *_a);
void	pmcstat_start_process(struct pmcstat_args *_a);
void	pmcstat_process_log(struct pmcstat_args *_a);
int	pmcstat_print_log(struct pmcstat_args *_a);

#define	PMCSTAT_PRINT_LOG(A,T,...) do {					\
		fprintf((A)->pa_outputfile, T "\t" __VA_ARGS__);	\
		fprintf((A)->pa_outputfile, "\n");			\
	} while (0)

/*
 * cleanup
 */

void
pmcstat_cleanup(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev, *tmp;

	/* de-configure the log file if present. */
	if (a->pa_flags & FLAG_HAS_LOG_FILE)
		(void) pmc_configure_logfile(-1);

	/* release allocated PMCs. */
	STAILQ_FOREACH_SAFE(ev, &a->pa_head, ev_next, tmp)
	    if (ev->ev_pmcid != PMC_ID_INVALID) {
		if (pmc_release(ev->ev_pmcid) < 0)
			err(EX_OSERR, "ERROR: cannot release pmc "
			    "0x%x \"%s\"", ev->ev_pmcid, ev->ev_name);
		free(ev->ev_name);
		free(ev->ev_spec);
		STAILQ_REMOVE(&a->pa_head, ev, pmcstat_ev, ev_next);
		free(ev);
	    }

	if (a->pa_logparser) {
		pmclog_close(a->pa_logparser);
		a->pa_logparser = NULL;
	}
}

void
pmcstat_start_pmcs(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev;

	STAILQ_FOREACH(ev, &args.pa_head, ev_next) {

	    assert(ev->ev_pmcid != PMC_ID_INVALID);

	    if (pmc_start(ev->ev_pmcid) < 0) {
	        warn("ERROR: Cannot start pmc 0x%x \"%s\"",
		    ev->ev_pmcid, ev->ev_name);
		pmcstat_cleanup(a);
		exit(EX_OSERR);
	    }
	}

}

void
pmcstat_print_headers(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev;
	int c;

	(void) fprintf(a->pa_outputfile, PRINT_HEADER_PREFIX);

	STAILQ_FOREACH(ev, &a->pa_head, ev_next) {
		if (PMC_IS_SAMPLING_MODE(ev->ev_mode))
			continue;

		c = PMC_IS_SYSTEM_MODE(ev->ev_mode) ? 's' : 'p';

		if (ev->ev_fieldskip != 0) {
			(void) fprintf(a->pa_outputfile, "%*s%c/%*s ",
			    ev->ev_fieldskip, "", c,
			    ev->ev_fieldwidth - ev->ev_fieldskip - 2,
			    ev->ev_name);
		} else
			(void) fprintf(a->pa_outputfile, "%c/%*s ",
			    c, ev->ev_fieldwidth - 2, ev->ev_name);
	}

	(void) fflush(a->pa_outputfile);
}

void
pmcstat_print_counters(struct pmcstat_args *a)
{
	int extra_width;
	struct pmcstat_ev *ev;
	pmc_value_t value;

	extra_width = sizeof(PRINT_HEADER_PREFIX) - 1;

	STAILQ_FOREACH(ev, &a->pa_head, ev_next) {

		/* skip sampling mode counters */
		if (PMC_IS_SAMPLING_MODE(ev->ev_mode))
			continue;

		if (pmc_read(ev->ev_pmcid, &value) < 0)
			err(EX_OSERR, "ERROR: Cannot read pmc "
			    "\"%s\"", ev->ev_name);

		(void) fprintf(a->pa_outputfile, "%*ju ",
		    ev->ev_fieldwidth + extra_width, (uintmax_t)
		    ev->ev_cumulative ? value : (value - ev->ev_saved));
		if (ev->ev_cumulative == 0)
			ev->ev_saved = value;
		extra_width = 0;
	}

	(void) fflush(a->pa_outputfile);
}

/*
 * Print output
 */

void
pmcstat_print_pmcs(struct pmcstat_args *a)
{
	static int linecount = 0;

	if (++linecount > pmcstat_displayheight) {
		(void) fprintf(a->pa_outputfile, "\n");
		linecount = 1;
	}

	if (linecount == 1)
		pmcstat_print_headers(a);

	(void) fprintf(a->pa_outputfile, "\n");
	pmcstat_print_counters(a);

	return;
}

/*
 * Do process profiling
 *
 * If a pid was specified, attach each allocated PMC to the target
 * process.  Otherwise, fork a child and attach the PMCs to the child,
 * and have the child exec() the target program.
 */

void
pmcstat_setup_process(struct pmcstat_args *a)
{
	char token;
	struct pmcstat_ev *ev;
	struct kevent kev;

	if (a->pa_flags & FLAG_HAS_PID) {
		STAILQ_FOREACH(ev, &a->pa_head, ev_next)
		    if (pmc_attach(ev->ev_pmcid, a->pa_pid) != 0)
			    err(EX_OSERR, "ERROR: cannot attach pmc \"%s\" to "
				"process %d", ev->ev_name, (int) a->pa_pid);
	} else {

		/*
		 * We need to fork a new process and startup the child
		 * using execvp().  Before doing the exec() the child
		 * process reads its pipe for a token so that the parent
		 * can finish doing its pmc_attach() calls.
		 */
		if (pipe(pmcstat_pipefd) < 0)
			err(EX_OSERR, "ERROR: cannot create pipe");

		switch (a->pa_pid = fork()) {
		case -1:
			err(EX_OSERR, "ERROR: cannot fork");
			/*NOTREACHED*/

		case 0:		/* child */

			/* wait for our parent to signal us */
			(void) close(pmcstat_pipefd[WRITEPIPEFD]);
			if (read(pmcstat_pipefd[READPIPEFD], &token, 1) < 0)
				err(EX_OSERR, "ERROR (child): cannot read "
				    "token");
			(void) close(pmcstat_pipefd[READPIPEFD]);

			/* exec() the program requested */
			execvp(*a->pa_argv, a->pa_argv);
			/* and if that fails, notify the parent */
			kill(getppid(), SIGCHLD);
			err(EX_OSERR, "ERROR: execvp \"%s\" failed",
			    *a->pa_argv);
			/*NOTREACHED*/

		default:	/* parent */

			(void) close(pmcstat_pipefd[READPIPEFD]);

			/* attach all our PMCs to the child */
			STAILQ_FOREACH(ev, &args.pa_head, ev_next)
			    if (PMC_IS_VIRTUAL_MODE(ev->ev_mode) &&
				pmc_attach(ev->ev_pmcid, a->pa_pid) != 0)
				    err(EX_OSERR, "ERROR: cannot attach pmc "
					"\"%s\" to process %d", ev->ev_name,
					(int) a->pa_pid);

		}
	}

	/* Ask to be notified via a kevent when the target process exits */
	EV_SET(&kev, a->pa_pid, EVFILT_PROC, EV_ADD|EV_ONESHOT, NOTE_EXIT, 0,
	    NULL);
	if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: cannot monitor child process %d",
		    a->pa_pid);
	return;
}

void
pmcstat_start_process(struct pmcstat_args *a)
{

	/* nothing to do: target is already running */
	if (a->pa_flags & FLAG_HAS_PID)
		return;

	/* write token to child to state that we are ready */
	if (write(pmcstat_pipefd[WRITEPIPEFD], "+", 1) != 1)
		err(EX_OSERR, "ERROR: write failed");

	(void) close(pmcstat_pipefd[WRITEPIPEFD]);
}


/*
 * Process a log file in offline analysis mode.
 */

void
pmcstat_process_log(struct pmcstat_args *a)
{
	int runstate;

	/*
	 * If gprof style profiles haven't been asked for, just print the
	 * log to the current output file.
	 */
	if ((a->pa_flags & (FLAG_DO_GPROF_MERGED|FLAG_DO_GPROF)) == 0) {
		while ((runstate = pmcstat_print_log(a)) == PMCSTAT_RUNNING)
			;
		return;
	}

	/* convert the log to gprof compatible profiles */
	assert(0);	/* To be implemented */
}

/*
 * Print log entries available in a configured parser.
 */

int
pmcstat_print_log(struct pmcstat_args *a)
{
	struct pmclog_ev ev;

	while (pmclog_read(a->pa_logparser, &ev) == 0) {
		assert(ev.pl_state == PMCLOG_OK);
		switch (ev.pl_type) {
		case PMCLOG_TYPE_CLOSELOG:
			PMCSTAT_PRINT_LOG(a,"close",);
			break;
		case PMCLOG_TYPE_DROPNOTIFY:
			PMCSTAT_PRINT_LOG(a,"drop",);
			break;
		case PMCLOG_TYPE_INITIALIZE:
			PMCSTAT_PRINT_LOG(a,"init","0x%x \"%s\"",
			    ev.pl_u.pl_i.pl_version,
			    pmc_name_of_cputype(ev.pl_u.pl_i.pl_arch));
			break;
		case PMCLOG_TYPE_MAPPINGCHANGE:
			PMCSTAT_PRINT_LOG(a,"mapping","%s %d %p %p \"%s\"",
			    ev.pl_u.pl_m.pl_type == PMCLOG_MAPPING_INSERT ?
			    	"insert" : "delete",
			    ev.pl_u.pl_m.pl_pid,
			    (void *) ev.pl_u.pl_m.pl_start,
			    (void *) ev.pl_u.pl_m.pl_end,
			    ev.pl_u.pl_m.pl_pathname);
			break;
		case PMCLOG_TYPE_PCSAMPLE:
			PMCSTAT_PRINT_LOG(a,"sample","0x%x %d %p",
			    ev.pl_u.pl_s.pl_pmcid,
			    ev.pl_u.pl_s.pl_pid,
			    (void *) ev.pl_u.pl_s.pl_pc);
			break;
		case PMCLOG_TYPE_PMCALLOCATE:
			PMCSTAT_PRINT_LOG(a,"allocate","0x%x \"%s\" 0x%x",
			    ev.pl_u.pl_a.pl_pmcid,
			    ev.pl_u.pl_a.pl_evname,
			    ev.pl_u.pl_a.pl_flags);
			break;
		case PMCLOG_TYPE_PMCATTACH:
			PMCSTAT_PRINT_LOG(a,"attach","0x%x %d \"%s\"",
			    ev.pl_u.pl_t.pl_pmcid,
			    ev.pl_u.pl_t.pl_pid,
			    ev.pl_u.pl_t.pl_pathname);
			break;
		case PMCLOG_TYPE_PMCDETACH:
			PMCSTAT_PRINT_LOG(a,"detach","0x%x %d",
			    ev.pl_u.pl_d.pl_pmcid,
			    ev.pl_u.pl_d.pl_pid);
			break;
		case PMCLOG_TYPE_PROCCSW:
			PMCSTAT_PRINT_LOG(a,"csw","0x%x %d %jd",
			    ev.pl_u.pl_c.pl_pmcid,
			    ev.pl_u.pl_c.pl_pid,
			    ev.pl_u.pl_c.pl_value);
			break;
		case PMCLOG_TYPE_PROCEXEC:
			PMCSTAT_PRINT_LOG(a,"exec","%d \"%s\"",
			    ev.pl_u.pl_x.pl_pid,
			    ev.pl_u.pl_x.pl_pathname);
			break;
		case PMCLOG_TYPE_PROCEXIT:
			PMCSTAT_PRINT_LOG(a,"exitvalue","0x%x %d %jd",
			    ev.pl_u.pl_e.pl_pmcid,
			    ev.pl_u.pl_e.pl_pid,
			    ev.pl_u.pl_e.pl_value);
			break;
		case PMCLOG_TYPE_PROCFORK:
			PMCSTAT_PRINT_LOG(a,"fork","%d %d",
			    ev.pl_u.pl_f.pl_oldpid,
			    ev.pl_u.pl_f.pl_newpid);
			break;
		case PMCLOG_TYPE_USERDATA:
			PMCSTAT_PRINT_LOG(a,"user","0x%x",
			    ev.pl_u.pl_u.pl_userdata);
			break;
		case PMCLOG_TYPE_SYSEXIT:
			PMCSTAT_PRINT_LOG(a,"exit","%d",
			    ev.pl_u.pl_se.pl_pid);
			break;
		default:
			fprintf(a->pa_outputfile, "unknown %d",
			    ev.pl_type);
		}
	}

	if (ev.pl_state == PMCLOG_EOF)
		return PMCSTAT_FINISHED;
	else if (ev.pl_state ==  PMCLOG_REQUIRE_DATA)
		return PMCSTAT_RUNNING;

	err(EX_DATAERR, "ERROR: event parsing failed "
	    "(record %jd, offset 0x%jx)",
	    (uintmax_t) ev.pl_count + 1, ev.pl_offset);
	/*NOTREACHED*/
}

/*
 * Close a logfile, after first flushing all in-module queued data.
 */

int
pmcstat_close_log(struct pmcstat_args *a)
{
	if (pmc_flush_logfile() < 0 ||
	    pmc_configure_logfile(-1) < 0)
		err(EX_OSERR, "ERROR: logging failed");
	a->pa_flags &= ~FLAG_HAS_LOG_FILE;
	return a->pa_flags & FLAG_HAS_PIPE ? PMCSTAT_EXITING :
	    PMCSTAT_FINISHED;
}

void
pmcstat_show_usage(void)
{
	errx(EX_USAGE,
	    "[options] [commandline]\n"
	    "\t Measure process and/or system performance using hardware\n"
	    "\t performance monitoring counters.\n"
	    "\t Options include:\n"
	    "\t -C\t\t (toggle) show cumulative counts\n"
	    "\t -D path\t create profiles in directory \"path\"\n"
	    "\t -E\t\t (toggle) show counts at process exit\n"
	    "\t -O file\t send log output to \"file\"\n"
	    "\t -P spec\t allocate a process-private sampling PMC\n"
	    "\t -R file\t read events from \"file\"\n"
	    "\t -S spec\t allocate a system-wide sampling PMC\n"
	    "\t -W\t\t (toggle) show counts per context switch\n"
	    "\t -c cpu\t\t set cpu for subsequent system-wide PMCs\n"
	    "\t -d\t\t (toggle) track descendants\n"
	    "\t -g\t\t produce gprof(1) compatible profiles\n"
	    "\t -m\t\t merge gprof(1) profiles for executables\n"
	    "\t -n rate\t set sampling rate\n"
	    "\t -o file\t send print output to \"file\"\n"
	    "\t -p spec\t allocate a process-private counting PMC\n"
	    "\t -s spec\t allocate a system-wide counting PMC\n"
	    "\t -t pid\t\t attach to running process with pid \"pid\"\n"
	    "\t -w secs\t set printing time interval"
	);
}

/*
 * Main
 */

int
main(int argc, char **argv)
{
	double interval;
	int option, npmc, ncpu;
	int c, current_cpu, current_sampling_count;
	int do_print, do_descendants;
	int do_logproccsw, do_logprocexit;
	int logfd;
	int pipefd[2];
	int use_cumulative_counts;
	pid_t pid;
	char *end;
	const char *errmsg;
	enum pmcstat_state runstate;
	struct pmcstat_ev *ev;
	struct sigaction sa;
	struct kevent kev;
	struct winsize ws;

	current_cpu 		= 0;
	current_sampling_count  = DEFAULT_SAMPLE_COUNT;
	do_descendants          = 0;
	do_logproccsw           = 0;
	do_logprocexit          = 0;
	use_cumulative_counts   = 0;
	args.pa_required	= 0;
	args.pa_flags		= 0;
	args.pa_pid		= (pid_t) -1;
	args.pa_logfile		= NULL;
	args.pa_outputdir	= NULL;
	args.pa_outputfile	= stderr;
	args.pa_interval	= DEFAULT_WAIT_INTERVAL;
	STAILQ_INIT(&args.pa_head);

	ev = NULL;

	while ((option = getopt(argc, argv, "CD:EO:P:R:S:Wc:dgmn:o:p:s:t:w:"))
	    != -1)
		switch (option) {
		case 'C':	/* cumulative values */
			use_cumulative_counts = !use_cumulative_counts;
			args.pa_required |= FLAG_HAS_COUNTING_PMCS;
			break;

		case 'c':	/* CPU */
			current_cpu = strtol(optarg, &end, 0);
			if (*end != '\0' || current_cpu < 0)
				errx(EX_USAGE,
				    "ERROR: Illegal CPU number \"%s\".",
				    optarg);
			args.pa_required |= FLAG_HAS_SYSTEM_PMCS;
			break;

		case 'd':	/* toggle descendents */
			do_descendants = !do_descendants;
			args.pa_required |= FLAG_HAS_PROCESS_PMCS;
			break;

		case 'D':
			args.pa_outputdir = optarg;
			break;

		case 'g':	/* produce gprof compatible profiles */
			args.pa_flags |= FLAG_DO_GPROF;
			args.pa_required |= FLAG_HAS_SAMPLING_PMCS;
			break;

		case 'm':	/* produce merged profiles */
			args.pa_flags |= FLAG_DO_GPROF_MERGED;
			args.pa_required |= FLAG_HAS_SAMPLING_PMCS;
			break;

		case 'E':	/* log process exit */
			do_logprocexit = !do_logprocexit;
			args.pa_required |= (FLAG_HAS_PROCESS_PMCS |
			    FLAG_HAS_COUNTING_PMCS | FLAG_HAS_LOG_FILE);
			break;

		case 'p':	/* process virtual counting PMC */
		case 's':	/* system-wide counting PMC */
		case 'P':	/* process virtual sampling PMC */
		case 'S':	/* system-wide sampling PMC */
			if ((ev = malloc(sizeof(*ev))) == NULL)
				errx(EX_SOFTWARE, "ERROR: Out of memory.");

			switch (option) {
			case 'p': ev->ev_mode = PMC_MODE_TC; break;
			case 's': ev->ev_mode = PMC_MODE_SC; break;
			case 'P': ev->ev_mode = PMC_MODE_TS; break;
			case 'S': ev->ev_mode = PMC_MODE_SS; break;
			}

			if (option == 'P' || option == 'p') {
				args.pa_flags |= FLAG_HAS_PROCESS_PMCS;
				args.pa_required |= (FLAG_HAS_PROCESS |
				    FLAG_HAS_PID);
			}

			if (option == 'P' || option == 'S') {
				args.pa_flags |= FLAG_HAS_SAMPLING_PMCS;
				args.pa_required |= FLAG_HAS_LOG_FILE;
			}

			if (option == 'p' || option == 's')
				args.pa_flags |= FLAG_HAS_COUNTING_PMCS;

			if (option == 's' || option == 'S')
				args.pa_flags |= FLAG_HAS_SYSTEM_PMCS;

			ev->ev_spec  = strdup(optarg);

			if (option == 'S' || option == 'P')
				ev->ev_count = current_sampling_count;
			else
				ev->ev_count = -1;

			if (option == 'S' || option == 's')
				ev->ev_cpu = current_cpu;
			else
				ev->ev_cpu = PMC_CPU_ANY;

			ev->ev_flags = 0;
			if (do_descendants)
				ev->ev_flags |= PMC_F_DESCENDANTS;
			if (do_logprocexit)
				ev->ev_flags |= PMC_F_LOG_PROCEXIT;
			if (do_logproccsw)
				ev->ev_flags |= PMC_F_LOG_PROCCSW;

			ev->ev_cumulative  = use_cumulative_counts;

			ev->ev_saved = 0LL;
			ev->ev_pmcid = PMC_ID_INVALID;

			/* extract event name */
			c = strcspn(optarg, ", \t");
			ev->ev_name = malloc(c + 1);
			(void) strncpy(ev->ev_name, optarg, c);
			*(ev->ev_name + c) = '\0';

			STAILQ_INSERT_TAIL(&args.pa_head, ev, ev_next);

			break;

		case 'R':	/* read an existing log file */
			if ((logfd = open(optarg, O_RDONLY, 0)) < 0)
				err(EX_OSERR, "ERROR: Cannot open \"%s\" for "
				    "reading", optarg);
			if ((args.pa_logparser = pmclog_open(logfd))
			    == NULL)
				err(EX_OSERR, "ERROR: Cannot create parser");
			args.pa_flags |= FLAG_PROCESS_LOGFILE;
			break;

		case 'n':	/* sampling count */
			current_sampling_count = strtol(optarg, &end, 0);
			if (*end != '\0' || current_sampling_count <= 0)
				errx(EX_USAGE,
				    "ERROR: Illegal count value \"%s\".",
				    optarg);
			args.pa_required |= FLAG_HAS_SAMPLING_PMCS;
			break;

		case 'o':	/* outputfile */
			if (args.pa_outputfile != NULL)
				(void) fclose(args.pa_outputfile);
			if ((args.pa_outputfile = fopen(optarg, "w")) == NULL)
				errx(EX_OSERR, "ERROR: cannot open \"%s\" for "
				    "writing.", optarg);
			args.pa_required |= FLAG_HAS_COUNTING_PMCS;
			break;

		case 'O':	/* sampling output */
			if (args.pa_logfile != NULL)
				errx(EX_OSERR, "ERROR: option -O may only be "
				    "specified once.");
			if ((args.pa_logfile = fopen(optarg, "w")) == NULL)
				errx(EX_OSERR, "ERROR: cannot open \"%s\" for "
				    "writing.", optarg);
			args.pa_flags |= FLAG_HAS_LOG_FILE;
			break;

		case 't':	/* target pid */
			pid = strtol(optarg, &end, 0);
			if (*end != '\0' || pid <= 0)
				errx(EX_USAGE, "ERROR: Illegal pid value "
				    "\"%s\".", optarg);

			args.pa_flags |= FLAG_HAS_PID;
			args.pa_required |= FLAG_HAS_PROCESS_PMCS;
			args.pa_pid = pid;
			break;

		case 'w':	/* wait interval */
			interval = strtod(optarg, &end);
			if (*end != '\0' || interval <= 0)
				errx(EX_USAGE, "ERROR: Illegal wait interval "
				    "value \"%s\".", optarg);
			args.pa_flags |= FLAG_HAS_WAIT_INTERVAL;
			args.pa_interval = interval;
			break;

		case 'W':	/* toggle LOG_CSW */
			do_logproccsw = !do_logproccsw;
			args.pa_required |= (FLAG_HAS_PROCESS_PMCS |
			    FLAG_HAS_COUNTING_PMCS | FLAG_HAS_LOG_FILE);
			break;

		case '?':
		default:
			pmcstat_show_usage();
			break;

		}

	args.pa_argc = (argc -= optind);
	args.pa_argv = (argv += optind);

	if (argc)
		args.pa_flags |= FLAG_HAS_PROCESS;

	/*
	 * Check invocation syntax.
	 */

	if (args.pa_flags & FLAG_PROCESS_LOGFILE) {
		errmsg = NULL;
		if (args.pa_flags & FLAG_HAS_PROCESS)
			errmsg = "a command line specification";
		else if (args.pa_flags & FLAG_HAS_PID)
			errmsg = "option -t";
		else if (!STAILQ_EMPTY(&args.pa_head))
			errmsg = "a PMC event specification";
		if (errmsg)
			errx(EX_USAGE, "ERROR: option -R may not be used with "
			    "%s.", errmsg);
	} else if (STAILQ_EMPTY(&args.pa_head)) {
		warnx("ERROR: At least one PMC event must be specified");
		pmcstat_show_usage();
	}

	/* check for -t pid without a process PMC spec */
	if ((args.pa_required & FLAG_HAS_PID) &&
	    (args.pa_flags & FLAG_HAS_PROCESS_PMCS) == 0)
		errx(EX_USAGE, "ERROR: option -t requires a process mode PMC "
		    "to be specified.");

	/* check for process-mode options without a command or -t pid */
	if ((args.pa_required & FLAG_HAS_PROCESS_PMCS) &&
	    (args.pa_flags & (FLAG_HAS_PROCESS | FLAG_HAS_PID)) == 0)
		errx(EX_USAGE, "ERROR: options -d,-E,-p,-P,-W require a "
		    "command line or target process.");

	/* check for -p | -P without a target process of some sort */
	if ((args.pa_required & (FLAG_HAS_PROCESS | FLAG_HAS_PID)) &&
	    (args.pa_flags & (FLAG_HAS_PROCESS | FLAG_HAS_PID)) == 0)
		errx(EX_USAGE, "ERROR: the -P or -p options require a "
		    "target process or a command line.");

	/* check for process-mode options without a process-mode PMC */
	if ((args.pa_required & FLAG_HAS_PROCESS_PMCS) &&
	    (args.pa_flags & FLAG_HAS_PROCESS_PMCS) == 0)
		errx(EX_USAGE, "ERROR: options -d,-E,-W require a "
		    "process mode PMC to be specified.");

	/* check for -c cpu and not system mode PMCs */
	if ((args.pa_required & FLAG_HAS_SYSTEM_PMCS) &&
	    (args.pa_flags & FLAG_HAS_SYSTEM_PMCS) == 0)
		errx(EX_USAGE, "ERROR: option -c requires at least one "
		    "system mode PMC to be specified.");

	/* check for counting mode options without a counting PMC */
	if ((args.pa_required & FLAG_HAS_COUNTING_PMCS) &&
	    (args.pa_flags & FLAG_HAS_COUNTING_PMCS) == 0)
		errx(EX_USAGE, "ERROR: options -C,-o,-W require at least one "
		    "counting mode PMC to be specified.");

	/* check for sampling mode options without a sampling PMC spec */
	if ((args.pa_required & FLAG_HAS_SAMPLING_PMCS) &&
	    (args.pa_flags & FLAG_HAS_SAMPLING_PMCS) == 0)
		errx(EX_USAGE, "ERROR: options -n,-O require at least one "
		    "sampling mode PMC to be specified.");

	if ((args.pa_flags & (FLAG_HAS_PID | FLAG_HAS_PROCESS)) ==
	    (FLAG_HAS_PID | FLAG_HAS_PROCESS))
		errx(EX_USAGE,
		    "ERROR: option -t cannot be specified with a command "
		    "line.");

	/* check if -O was spuriously specified */
	if ((args.pa_flags & FLAG_HAS_LOG_FILE) &&
	    (args.pa_required & FLAG_HAS_LOG_FILE) == 0)
		errx(EX_USAGE,
		    "ERROR: option -O is used only with options "
		    "-E,-P,-S and -W.");

	/* if we've been asked to process a log file, do that and exit */
	if (args.pa_flags & FLAG_PROCESS_LOGFILE) {
		pmcstat_process_log(&args);
		exit(EX_OK);
	}

	/* otherwise, we've been asked to collect data */
	if (pmc_init() < 0)
		err(EX_UNAVAILABLE,
		    "ERROR: Initialization of the pmc(3) library failed");

	if ((ncpu = pmc_ncpu()) < 0)
		err(EX_OSERR, "ERROR: Cannot determine the number CPUs "
		    "on the system");

	if ((npmc = pmc_npmc(0)) < 0) /* assume all CPUs are identical */
		err(EX_OSERR, "ERROR: Cannot determine the number of PMCs "
		    "on CPU %d", 0);

	/*
	 * Allocate PMCs.
	 */

	STAILQ_FOREACH(ev, &args.pa_head, ev_next)
	    if (pmc_allocate(ev->ev_spec, ev->ev_mode,
		    ev->ev_flags, ev->ev_cpu, &ev->ev_pmcid) < 0)
		    err(EX_OSERR, "ERROR: Cannot allocate %s-mode pmc with "
			"specification \"%s\"",
			PMC_IS_SYSTEM_MODE(ev->ev_mode) ? "system" : "process",
			ev->ev_spec);

	/* compute printout widths */
	STAILQ_FOREACH(ev, &args.pa_head, ev_next) {
		int counter_width;
		int display_width;
		int header_width;

		(void) pmc_width(ev->ev_pmcid, &counter_width);
		header_width = strlen(ev->ev_name) + 2; /* prefix '%c|' */
		display_width = (int) floor(counter_width / 3.32193) + 1;

		if (header_width > display_width) {
			ev->ev_fieldskip = 0;
			ev->ev_fieldwidth = header_width;
		} else {
			ev->ev_fieldskip = display_width -
			    header_width;
			ev->ev_fieldwidth = display_width;
		}
	}

	/* Allocate a kqueue */
	if ((pmcstat_kq = kqueue()) < 0)
		err(EX_OSERR, "ERROR: Cannot allocate kqueue");

	/*
	 * If our output is being set to a terminal, register a handler
	 * for window size changes.
	 */

	if (isatty(fileno(args.pa_outputfile))) {

		if (ioctl(fileno(args.pa_outputfile), TIOCGWINSZ, &ws) < 0)
			err(EX_OSERR, "ERROR: Cannot determine window size");

		pmcstat_displayheight = ws.ws_row - 1;

		EV_SET(&kev, SIGWINCH, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

		if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
			err(EX_OSERR, "ERROR: Cannot register kevent for "
			    "SIGWINCH");
	}

	EV_SET(&kev, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: Cannot register kevent for SIGINT");

	EV_SET(&kev, SIGIO, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: Cannot register kevent for SIGIO");

	/*
	 * An exec() failure of a forked child is signalled by the
	 * child sending the parent a SIGCHLD.  We don't register an
	 * actual signal handler for SIGCHLD, but instead use our
	 * kqueue to pick up the signal.
	 */
	EV_SET(&kev, SIGCHLD, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: Cannot register kevent for SIGCHLD");

	/*
	 * Configure the specified log file or setup a default log
	 * consumer via a pipe.
	 */
	if (args.pa_required & FLAG_HAS_LOG_FILE) {

		if (args.pa_logfile == NULL) {
			if (pipe(pipefd) < 0)
				err(EX_OSERR, "ERROR: pipe(2) failed");

			EV_SET(&kev, pipefd[READPIPEFD], EVFILT_READ, EV_ADD,
			    0, 0, NULL);

			if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
				err(EX_OSERR, "ERROR: Cannot register kevent");

			logfd = pipefd[WRITEPIPEFD];

			args.pa_flags |= (FLAG_HAS_PIPE | FLAG_HAS_LOG_FILE);
			args.pa_logparser = pmclog_open(pipefd[READPIPEFD]);
		} else
			logfd = fileno(args.pa_logfile);

		if (pmc_configure_logfile(logfd) < 0)
			err(EX_OSERR, "ERROR: Cannot configure log file");

		STAILQ_FOREACH(ev, &args.pa_head, ev_next)
		    if (PMC_IS_SAMPLING_MODE(ev->ev_mode) &&
			pmc_set(ev->ev_pmcid, ev->ev_count) < 0)
			    err(EX_OSERR, "ERROR: Cannot set sampling count "
				"for PMC \"%s\"", ev->ev_name);
	}

	/* setup a timer for any counting mode PMCs */
	if (args.pa_flags & FLAG_HAS_COUNTING_PMCS) {
		EV_SET(&kev, 0, EVFILT_TIMER, EV_ADD, 0,
		    args.pa_interval * 1000, NULL);

		if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
			err(EX_OSERR, "ERROR: Cannot register kevent for "
			    "timer");
	}

	/* attach PMCs to the target process, starting it if specified */
	if (args.pa_flags & FLAG_HAS_PROCESS)
		pmcstat_setup_process(&args);

	/* start the pmcs */
	pmcstat_start_pmcs(&args);

	/* start the (commandline) process if needed */
	if (args.pa_flags & FLAG_HAS_PROCESS)
		pmcstat_start_process(&args);

	/* Handle SIGINT using the kqueue loop */
	sa.sa_handler = SIG_IGN;
	sa.sa_flags   = 0;
	(void) sigemptyset(&sa.sa_mask);

	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(EX_OSERR, "ERROR: Cannot install signal handler");

	/*
	 * loop till either the target process (if any) exits, or we
	 * are killed by a SIGINT.
	 */
	runstate = PMCSTAT_RUNNING;
	do_print = 0;
	do {
		if ((c = kevent(pmcstat_kq, NULL, 0, &kev, 1, NULL)) <= 0) {
			if (errno != EINTR)
				err(EX_OSERR, "ERROR: kevent failed");
			else
				continue;
		}

		if (kev.flags & EV_ERROR)
			errc(EX_OSERR, kev.data, "ERROR: kevent failed");

		switch (kev.filter) {
		case EVFILT_PROC:  /* target has exited */
			if (args.pa_flags & FLAG_HAS_LOG_FILE)
				runstate = pmcstat_close_log(&args);
			break;

		case EVFILT_READ:  /* log file data is present */
			runstate = pmcstat_print_log(&args);
			break;

		case EVFILT_SIGNAL:
			if (kev.ident == SIGCHLD) {
				/*
				 * The child process sends us a
				 * SIGCHLD if its exec() failed.  We
				 * wait for it to exit and then exit
				 * ourselves.
				 */
				(void) wait(&c);
				runstate = PMCSTAT_FINISHED;
			} else if (kev.ident == SIGIO) {
				/*
				 * We get a SIGIO if a PMC loses all
				 * of its targets, or if logfile
				 * writes encounter an error.
				 */
				if (args.pa_flags & FLAG_HAS_LOG_FILE)
					runstate = pmcstat_close_log(&args);
				do_print = 1; /* print PMCs at exit */
				runstate = PMCSTAT_FINISHED;
			} else if (kev.ident == SIGINT) {
				/* pass the signal on to the child process */
				if ((args.pa_flags & FLAG_HAS_PROCESS) &&
				    (args.pa_flags & FLAG_HAS_PID) == 0)
					if (kill(args.pa_pid, SIGINT) != 0)
						err(EX_OSERR, "ERROR: cannot "
						    "signal child process");
				runstate = PMCSTAT_FINISHED;
			} else if (kev.ident == SIGWINCH) {
				if (ioctl(fileno(args.pa_outputfile),
					TIOCGWINSZ, &ws) < 0)
				    err(EX_OSERR, "ERROR: Cannot determine "
					"window size");
				pmcstat_displayheight = ws.ws_row - 1;
			} else
				assert(0);

			break;

		case EVFILT_TIMER: /* print out counting PMCs */
			do_print = 1;
			break;

		}

		if (do_print) {
			pmcstat_print_pmcs(&args);
			if (runstate == PMCSTAT_FINISHED) /* final newline */
				(void) fprintf(args.pa_outputfile, "\n");
			do_print = 0;
		}

	} while (runstate != PMCSTAT_FINISHED);

	/* flush any pending log entries */
	if (args.pa_flags & FLAG_HAS_LOG_FILE)
		pmc_flush_logfile();

	pmcstat_cleanup(&args);

	return 0;
}
