/*-
 * Copyright (c) 2003,2004 Joseph Koshy
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
 *
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
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/* Operation modes */

#define	FLAG_HAS_PID			0x00000001
#define	FLAG_HAS_WAIT_INTERVAL		0x00000002
#define	FLAG_HAS_LOG_FILE		0x00000004
#define	FLAG_HAS_PROCESS		0x00000008
#define	FLAG_USING_SAMPLING		0x00000010
#define	FLAG_USING_COUNTING		0x00000020
#define	FLAG_USING_PROCESS_PMC		0x00000040

#define	DEFAULT_SAMPLE_COUNT		65536
#define	DEFAULT_WAIT_INTERVAL		5.0
#define	DEFAULT_DISPLAY_HEIGHT		23
#define	DEFAULT_LOGFILE_NAME		"pmcstat.out"

#define	PRINT_HEADER_PREFIX		"# "
#define	READPIPEFD			0
#define	WRITEPIPEFD			1
#define	NPIPEFD				2

struct pmcstat_ev {
	STAILQ_ENTRY(pmcstat_ev) ev_next;
	char	       *ev_spec;  /* event specification */
	char	       *ev_name;  /* (derived) event name */
	enum pmc_mode	ev_mode;  /* desired mode */
	int		ev_count; /* associated count if in sampling mode */
	int		ev_cpu;	  /* specific cpu if requested */
	int		ev_descendants; /* attach to descendants */
	int		ev_cumulative;  /* show cumulative counts */
	int		ev_fieldwidth;  /* print width */
	int		ev_fieldskip;   /* #leading spaces */
	pmc_value_t	ev_saved; /* saved value for incremental counts */
	pmc_id_t	ev_pmcid; /* allocated ID */
};

struct pmcstat_args {
	int	pa_flags;
	pid_t	pa_pid;
	FILE   *pa_outputfile;
	FILE   *pa_logfile;
	double  pa_interval;
	int	pa_argc;
	char  **pa_argv;
	STAILQ_HEAD(, pmcstat_ev) pa_head;
} args;

int	pmcstat_interrupt = 0;
int	pmcstat_displayheight = DEFAULT_DISPLAY_HEIGHT;
int	pmcstat_pipefd[NPIPEFD];
int	pmcstat_kq;

/* Function prototypes */
void pmcstat_cleanup(struct pmcstat_args *_a);
void pmcstat_print_counters(struct pmcstat_args *_a);
void pmcstat_print_headers(struct pmcstat_args *_a);
void pmcstat_print_pmcs(struct pmcstat_args *_a);
void pmcstat_setup_process(struct pmcstat_args *_a);
void pmcstat_show_usage(void);
void pmcstat_start_pmcs(struct pmcstat_args *_a);
void pmcstat_start_process(struct pmcstat_args *_a);


/*
 * cleanup
 */

void
pmcstat_cleanup(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev, *tmp;

	/* de-configure the log file if present. */
	if (a->pa_flags & FLAG_USING_SAMPLING) {
		(void) pmc_configure_logfile(-1);
		(void) fclose(a->pa_logfile);
	}

	/* release allocated PMCs. */
	STAILQ_FOREACH_SAFE(ev, &a->pa_head, ev_next, tmp)
	    if (ev->ev_pmcid != PMC_ID_INVALID) {
		if (pmc_release(ev->ev_pmcid) < 0)
			err(EX_OSERR, "ERROR: cannot release pmc "
			    "%d \"%s\"", ev->ev_pmcid, ev->ev_name);
		free(ev->ev_name);
		free(ev->ev_spec);
		STAILQ_REMOVE(&a->pa_head, ev, pmcstat_ev, ev_next);
		free(ev);
	    }
}

void
pmcstat_start_pmcs(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev;

	STAILQ_FOREACH(ev, &args.pa_head, ev_next) {

	    assert(ev->ev_pmcid != PMC_ID_INVALID);

	    if (pmc_start(ev->ev_pmcid) < 0) {
	        warn("ERROR: Cannot start pmc %d \"%s\"",
		    ev->ev_pmcid, ev->ev_name);
		pmcstat_cleanup(a);
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

		STAILQ_FOREACH(ev, &args.pa_head, ev_next)
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
			execvp(*args.pa_argv, args.pa_argv);
			err(EX_OSERR, "ERROR (child): execvp failed");
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

	/* Ask to be notified via a kevent when the child exits */
	EV_SET(&kev, a->pa_pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, 0);

	if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: cannot monitor process %d",
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

void
pmcstat_show_usage(void)
{
	errx(EX_USAGE,
	    "[options] [commandline]\n"
	    "\t Measure process and/or system performance using hardware\n"
	    "\t performance monitoring counters.\n"
	    "\t Options include:\n"
	    "\t -C\t\t toggle showing cumulative counts\n"
	    "\t -O file\t set sampling log file to \"file\"\n"
	    "\t -P spec\t allocate process-private sampling PMC\n"
	    "\t -S spec\t allocate system-wide sampling PMC\n"
	    "\t -c cpu\t\t set default cpu\n"
	    "\t -d\t\t toggle tracking descendants\n"
	    "\t -n rate\t set sampling rate\n"
	    "\t -o file\t send print output to \"file\"\n"
	    "\t -p spec\t allocate process-private counting PMC\n"
	    "\t -s spec\t allocate system-wide counting PMC\n"
	    "\t -t pid\t attach to running process with pid \"pid\"\n"
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
	int running;
	int do_descendants, use_cumulative_counts;
	pid_t pid;
	char *end;
	struct pmcstat_ev *ev;
	struct pmc_op_getpmcinfo *ppmci;
	struct sigaction sa;
	struct kevent kev;
	struct winsize ws;

	current_cpu 		= 0;
	current_sampling_count  = DEFAULT_SAMPLE_COUNT;
	do_descendants          = 0;
	use_cumulative_counts   = 0;
	args.pa_flags		= 0;
	args.pa_pid		= (pid_t) -1;
	args.pa_logfile		= NULL;
	args.pa_outputfile	= stderr;
	args.pa_interval	= DEFAULT_WAIT_INTERVAL;
	STAILQ_INIT(&args.pa_head);

	ev = NULL;

	while ((option = getopt(argc, argv, "CO:P:S:c:dn:o:p:s:t:w:")) != -1)
		switch (option) {
		case 'C':	/* cumulative values */
			use_cumulative_counts = !use_cumulative_counts;
			break;

		case 'c':	/* CPU */
			current_cpu = strtol(optarg, &end, 0);
			if (*end != '\0' || current_cpu < 0)
				errx(EX_USAGE,
				    "ERROR: Illegal CPU number \"%s\"",
				    optarg);

			break;

		case 'd':	/* toggle descendents */
			do_descendants = !do_descendants;
			break;

		case 'p':	/* process virtual counting PMC */
		case 's':	/* system-wide counting PMC */
		case 'P':	/* process virtual sampling PMC */
		case 'S':	/* system-wide sampling PMC */
			if ((ev = malloc(sizeof(*ev))) == NULL)
				errx(EX_SOFTWARE, "ERROR: Out of memory");

			switch (option) {
			case 'p': ev->ev_mode = PMC_MODE_TC; break;
			case 's': ev->ev_mode = PMC_MODE_SC; break;
			case 'P': ev->ev_mode = PMC_MODE_TS; break;
			case 'S': ev->ev_mode = PMC_MODE_SS; break;
			}

			if (option == 'P' || option == 'p')
				args.pa_flags |= FLAG_USING_PROCESS_PMC;

			if (option == 'P' || option == 'S')
				args.pa_flags |= FLAG_USING_SAMPLING;

			if (option == 'p' || option == 's')
				args.pa_flags |= FLAG_USING_COUNTING;

			ev->ev_spec  = strdup(optarg);

			if (option == 'S' || option == 'P')
				ev->ev_count = current_sampling_count;
			else
				ev->ev_count = -1;

			if (option == 'S' || option == 's')
				ev->ev_cpu = current_cpu;
			else
				ev->ev_cpu = PMC_CPU_ANY;

			ev->ev_descendants = do_descendants;
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

		case 'n':	/* sampling count */
			current_sampling_count = strtol(optarg, &end, 0);
			if (*end != '\0' || current_sampling_count <= 0)
				errx(EX_USAGE,
				    "ERROR: Illegal count value \"%s\"",
				    optarg);
			break;

		case 'o':	/* outputfile */
			if (args.pa_outputfile != NULL)
				(void) fclose(args.pa_outputfile);

			if ((args.pa_outputfile = fopen(optarg, "w")) == NULL)
				errx(EX_OSERR, "ERROR: cannot open \"%s\" for "
				    "writing", optarg);

		case 'O':	/* sampling output */
			if (args.pa_logfile != NULL)
				(void) fclose(args.pa_logfile);

			if ((args.pa_logfile = fopen(optarg, "w")) == NULL)
				errx(EX_OSERR, "ERROR: cannot open \"%s\" for "
				    "writing", optarg);
			break;

		case 't':	/* target pid */
			pid = strtol(optarg, &end, 0);
			if (*end != '\0' || pid <= 0)
				errx(EX_USAGE, "ERROR: Illegal pid value "
				    "\"%s\"", optarg);

			args.pa_flags |= FLAG_HAS_PID;
			args.pa_pid = pid;

			break;

		case 'w':	/* wait interval */
			interval = strtod(optarg, &end);
			if (*end != '\0' || interval <= 0)
				errx(EX_USAGE, "ERROR: Illegal wait interval "
				    "value \"%s\"", optarg);
			args.pa_flags |= FLAG_HAS_WAIT_INTERVAL;
			args.pa_interval = interval;

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

	if (STAILQ_EMPTY(&args.pa_head)) {
		warnx("ERROR: At least one PMC event must be specified");
		pmcstat_show_usage();
	}

	if (argc == 0) {
		if (args.pa_pid == -1) {
			if (args.pa_flags & FLAG_USING_PROCESS_PMC)
				errx(EX_USAGE, "ERROR: the -P or -p options "
				    "require a target process");
		} else if ((args.pa_flags & FLAG_USING_PROCESS_PMC) == 0)
			errx(EX_USAGE,
			    "ERROR: option -t requires a process-mode pmc "
			    "specification");
	} else if (args.pa_pid != -1)
		errx(EX_USAGE,
		    "ERROR: option -t cannot be specified with a command "
		    "name");

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

	if (pmc_pmcinfo(0, &ppmci) < 0)
		err(EX_OSERR, "ERROR: cannot retrieve pmc information");

	assert(ppmci != NULL);

	STAILQ_FOREACH(ev, &args.pa_head, ev_next)
	    if (pmc_allocate(ev->ev_spec, ev->ev_mode,
		    (ev->ev_descendants ? PMC_F_DESCENDANTS : 0),
		    ev->ev_cpu, &ev->ev_pmcid) < 0)
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

	if (args.pa_flags & FLAG_USING_SAMPLING) {

		/*
		 * configure log file
		 */

		if (args.pa_logfile == NULL)
			if ((args.pa_logfile =
				fopen(DEFAULT_LOGFILE_NAME, "w")) == NULL)
				err(EX_CANTCREAT, "ERROR: Cannot open sampling "
				    "log file \"%s\"", DEFAULT_LOGFILE_NAME);

		if (pmc_configure_logfile(fileno(args.pa_logfile)) < 0)
			err(EX_OSERR, "ERROR: Cannot configure sampling "
			    "log");

		STAILQ_FOREACH(ev, &args.pa_head, ev_next)
		    if (PMC_IS_SAMPLING_MODE(ev->ev_mode) &&
			pmc_set(ev->ev_pmcid, ev->ev_count) < 0)
			    err(EX_OSERR, "ERROR: Cannot set sampling count "
				"for PMC \"%s\"", ev->ev_name);
	}

	/* setup a timer for any counting mode PMCs */
	if (args.pa_flags & FLAG_USING_COUNTING) {
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

	running = 1;
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
		case EVFILT_PROC: /* target process exited */
			running = 0;
			/* FALLTHROUGH */

		case EVFILT_TIMER: /* print out counting PMCs */
			pmcstat_print_pmcs(&args);

			if (running == 0) /* final newline */
				(void) fprintf(args.pa_outputfile, "\n");
			break;

		case EVFILT_SIGNAL:
			if (kev.ident == SIGINT) {
				/* pass the signal on to the child process */
				if ((args.pa_flags & FLAG_HAS_PROCESS) &&
				    (args.pa_flags & FLAG_HAS_PID) == 0)
					if (kill(args.pa_pid, SIGINT) != 0)
						err(EX_OSERR, "cannot kill "
						    "child");
				running = 0;
			} else if (kev.ident == SIGWINCH) {
				if (ioctl(fileno(args.pa_outputfile),
					TIOCGWINSZ, &ws) < 0)
				    err(EX_OSERR, "ERROR: Cannot determine "
					"window size");
				pmcstat_displayheight = ws.ws_row - 1;
			} else
				assert(0);

			break;
		}

	} while (running);

	pmcstat_cleanup(&args);

	return 0;
}
