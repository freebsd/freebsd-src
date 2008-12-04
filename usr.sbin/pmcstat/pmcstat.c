/*-
 * Copyright (c) 2003-2008, Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <pmc.h>
#include <pmclog.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "pmcstat.h"

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
 *
 * pmcstat(8) can also execute a command line and attach PMCs to the
 * resulting child process.  The protocol used is as follows:
 *
 * - parent creates a socketpair for two way communication and
 *   fork()s.
 * - subsequently:
 *
 *   /Parent/				/Child/
 *
 *   - Wait for childs token.
 *					- Sends token.
 *					- Awaits signal to start.
 *  - Attaches PMCs to the child's pid
 *    and starts them. Sets up
 *    monitoring for the child.
 *  - Signals child to start.
 *					- Recieves signal, attempts exec().
 *
 * After this point normal processing can happen.
 */

/* Globals */

int	pmcstat_interrupt = 0;
int	pmcstat_displayheight = DEFAULT_DISPLAY_HEIGHT;
int	pmcstat_sockpair[NSOCKPAIRFD];
int	pmcstat_kq;
kvm_t	*pmcstat_kvm;
struct kinfo_proc *pmcstat_plist;

void
pmcstat_attach_pmcs(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev;
	struct pmcstat_target *pt;
	int count;

	/* Attach all process PMCs to target processes. */
	count = 0;
	STAILQ_FOREACH(ev, &a->pa_events, ev_next) {
		if (PMC_IS_SYSTEM_MODE(ev->ev_mode))
			continue;
		SLIST_FOREACH(pt, &a->pa_targets, pt_next)
			if (pmc_attach(ev->ev_pmcid, pt->pt_pid) == 0)
				count++;
			else if (errno != ESRCH)
				err(EX_OSERR, "ERROR: cannot attach pmc "
				    "\"%s\" to process %d", ev->ev_name,
				    (int) pt->pt_pid);
	}

	if (count == 0)
		errx(EX_DATAERR, "ERROR: No processes were attached to.");
}


void
pmcstat_cleanup(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev, *tmp;

	/* release allocated PMCs. */
	STAILQ_FOREACH_SAFE(ev, &a->pa_events, ev_next, tmp)
	    if (ev->ev_pmcid != PMC_ID_INVALID) {
		if (pmc_stop(ev->ev_pmcid) < 0)
			err(EX_OSERR, "ERROR: cannot stop pmc 0x%x "
			    "\"%s\"", ev->ev_pmcid, ev->ev_name);
		if (pmc_release(ev->ev_pmcid) < 0)
			err(EX_OSERR, "ERROR: cannot release pmc "
			    "0x%x \"%s\"", ev->ev_pmcid, ev->ev_name);
		free(ev->ev_name);
		free(ev->ev_spec);
		STAILQ_REMOVE(&a->pa_events, ev, pmcstat_ev, ev_next);
		free(ev);
	    }

	/* de-configure the log file if present. */
	if (a->pa_flags & (FLAG_HAS_PIPE | FLAG_HAS_OUTPUT_LOGFILE))
		(void) pmc_configure_logfile(-1);

	if (a->pa_logparser) {
		pmclog_close(a->pa_logparser);
		a->pa_logparser = NULL;
	}

	if (a->pa_flags & (FLAG_HAS_PIPE | FLAG_HAS_OUTPUT_LOGFILE))
		pmcstat_shutdown_logging(a);
}

void
pmcstat_clone_event_descriptor(struct pmcstat_args *a, struct pmcstat_ev *ev,
    uint32_t cpumask)
{
	int cpu;
	struct pmcstat_ev *ev_clone;

	while ((cpu = ffs(cpumask)) > 0) {
		cpu--;

		if ((ev_clone = malloc(sizeof(*ev_clone))) == NULL)
			errx(EX_SOFTWARE, "ERROR: Out of memory");
		(void) memset(ev_clone, 0, sizeof(*ev_clone));

		ev_clone->ev_count = ev->ev_count;
		ev_clone->ev_cpu   = cpu;
		ev_clone->ev_cumulative = ev->ev_cumulative;
		ev_clone->ev_flags = ev->ev_flags;
		ev_clone->ev_mode  = ev->ev_mode;
		ev_clone->ev_name  = strdup(ev->ev_name);
		ev_clone->ev_pmcid = ev->ev_pmcid;
		ev_clone->ev_saved = ev->ev_saved;
		ev_clone->ev_spec  = strdup(ev->ev_spec);

		STAILQ_INSERT_TAIL(&a->pa_events, ev_clone, ev_next);

		cpumask &= ~(1 << cpu);
	}
}

void
pmcstat_create_process(struct pmcstat_args *a)
{
	char token;
	pid_t pid;
	struct kevent kev;
	struct pmcstat_target *pt;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pmcstat_sockpair) < 0)
		err(EX_OSERR, "ERROR: cannot create socket pair");

	switch (pid = fork()) {
	case -1:
		err(EX_OSERR, "ERROR: cannot fork");
		/*NOTREACHED*/

	case 0:		/* child */
		(void) close(pmcstat_sockpair[PARENTSOCKET]);

		/* Write a token to tell our parent we've started executing. */
		if (write(pmcstat_sockpair[CHILDSOCKET], "+", 1) != 1)
			err(EX_OSERR, "ERROR (child): cannot write token");

		/* Wait for our parent to signal us to start. */
		if (read(pmcstat_sockpair[CHILDSOCKET], &token, 1) < 0)
			err(EX_OSERR, "ERROR (child): cannot read token");
		(void) close(pmcstat_sockpair[CHILDSOCKET]);

		/* exec() the program requested */
		execvp(*a->pa_argv, a->pa_argv);
		/* and if that fails, notify the parent */
		kill(getppid(), SIGCHLD);
		err(EX_OSERR, "ERROR: execvp \"%s\" failed", *a->pa_argv);
		/*NOTREACHED*/

	default:	/* parent */
		(void) close(pmcstat_sockpair[CHILDSOCKET]);
		break;
	}

	/* Ask to be notified via a kevent when the target process exits. */
	EV_SET(&kev, pid, EVFILT_PROC, EV_ADD|EV_ONESHOT, NOTE_EXIT, 0,
	    NULL);
	if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
		err(EX_OSERR, "ERROR: cannot monitor child process %d", pid);

	if ((pt = malloc(sizeof(*pt))) == NULL)
		errx(EX_SOFTWARE, "ERROR: Out of memory.");

	pt->pt_pid = pid;
	SLIST_INSERT_HEAD(&a->pa_targets, pt, pt_next);

	/* Wait for the child to signal that its ready to go. */
	if (read(pmcstat_sockpair[PARENTSOCKET], &token, 1) < 0)
		err(EX_OSERR, "ERROR (parent): cannot read token");

	return;
}

void
pmcstat_find_targets(struct pmcstat_args *a, const char *spec)
{
	int n, nproc, pid, rv;
	struct pmcstat_target *pt;
	char errbuf[_POSIX2_LINE_MAX], *end;
	static struct kinfo_proc *kp;
	regex_t reg;
	regmatch_t regmatch;

	/* First check if we've been given a process id. */
      	pid = strtol(spec, &end, 0);
	if (end != spec && pid >= 0) {
		if ((pt = malloc(sizeof(*pt))) == NULL)
			goto outofmemory;
		pt->pt_pid = pid;
		SLIST_INSERT_HEAD(&a->pa_targets, pt, pt_next);
		return;
	}

	/* Otherwise treat arg as a regular expression naming processes. */
	if (pmcstat_kvm == NULL) {
		if ((pmcstat_kvm = kvm_openfiles(NULL, "/dev/null", NULL, 0,
		    errbuf)) == NULL)
			err(EX_OSERR, "ERROR: Cannot open kernel \"%s\"",
			    errbuf);
		if ((pmcstat_plist = kvm_getprocs(pmcstat_kvm, KERN_PROC_PROC,
		    0, &nproc)) == NULL)
			err(EX_OSERR, "ERROR: Cannot get process list: %s",
			    kvm_geterr(pmcstat_kvm));
	}

	if ((rv = regcomp(&reg, spec, REG_EXTENDED|REG_NOSUB)) != 0) {
		regerror(rv, &reg, errbuf, sizeof(errbuf));
		err(EX_DATAERR, "ERROR: Failed to compile regex \"%s\": %s",
		    spec, errbuf);
	}

	for (n = 0, kp = pmcstat_plist; n < nproc; n++, kp++) {
		if ((rv = regexec(&reg, kp->ki_comm, 1, &regmatch, 0)) == 0) {
			if ((pt = malloc(sizeof(*pt))) == NULL)
				goto outofmemory;
			pt->pt_pid = kp->ki_pid;
			SLIST_INSERT_HEAD(&a->pa_targets, pt, pt_next);
		} else if (rv != REG_NOMATCH) {
			regerror(rv, &reg, errbuf, sizeof(errbuf));
			errx(EX_SOFTWARE, "ERROR: Regex evalation failed: %s",
			    errbuf);
		}
	}

	regfree(&reg);

	return;

 outofmemory:
	errx(EX_SOFTWARE, "Out of memory.");
	/*NOTREACHED*/
}

uint32_t
pmcstat_get_cpumask(const char *cpuspec)
{
	uint32_t cpumask;
	int cpu;
	const char *s;
	char *end;

	s = cpuspec;
	cpumask = 0ULL;

	do {
		cpu = strtol(s, &end, 0);
		if (cpu < 0 || end == s)
			errx(EX_USAGE, "ERROR: Illegal CPU specification "
			    "\"%s\".", cpuspec);
		cpumask |= (1 << cpu);
		s = end + strspn(end, ", \t");
	} while (*s);

	return (cpumask);
}

void
pmcstat_kill_process(struct pmcstat_args *a)
{
	struct pmcstat_target *pt;

	assert(a->pa_flags & FLAG_HAS_COMMANDLINE);

	/*
	 * If a command line was specified, it would be the very first
	 * in the list, before any other processes specified by -t.
	 */
	pt = SLIST_FIRST(&a->pa_targets);
	assert(pt != NULL);

	if (kill(pt->pt_pid, SIGINT) != 0)
		err(EX_OSERR, "ERROR: cannot signal child process");
}

void
pmcstat_start_pmcs(struct pmcstat_args *a)
{
	struct pmcstat_ev *ev;

	STAILQ_FOREACH(ev, &args.pa_events, ev_next) {

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
	int c, w;

	(void) fprintf(a->pa_printfile, PRINT_HEADER_PREFIX);

	STAILQ_FOREACH(ev, &a->pa_events, ev_next) {
		if (PMC_IS_SAMPLING_MODE(ev->ev_mode))
			continue;

		c = PMC_IS_SYSTEM_MODE(ev->ev_mode) ? 's' : 'p';

		if (ev->ev_fieldskip != 0)
			(void) fprintf(a->pa_printfile, "%*s",
			    ev->ev_fieldskip, "");
		w = ev->ev_fieldwidth - ev->ev_fieldskip - 2;

		if (c == 's')
			(void) fprintf(a->pa_printfile, "s/%02d/%-*s ",
			    ev->ev_cpu, w-3, ev->ev_name);
		else
			(void) fprintf(a->pa_printfile, "p/%*s ", w,
			    ev->ev_name);
	}

	(void) fflush(a->pa_printfile);
}

void
pmcstat_print_counters(struct pmcstat_args *a)
{
	int extra_width;
	struct pmcstat_ev *ev;
	pmc_value_t value;

	extra_width = sizeof(PRINT_HEADER_PREFIX) - 1;

	STAILQ_FOREACH(ev, &a->pa_events, ev_next) {

		/* skip sampling mode counters */
		if (PMC_IS_SAMPLING_MODE(ev->ev_mode))
			continue;

		if (pmc_read(ev->ev_pmcid, &value) < 0)
			err(EX_OSERR, "ERROR: Cannot read pmc "
			    "\"%s\"", ev->ev_name);

		(void) fprintf(a->pa_printfile, "%*ju ",
		    ev->ev_fieldwidth + extra_width,
		    (uintmax_t) ev->ev_cumulative ? value :
		    (value - ev->ev_saved));

		if (ev->ev_cumulative == 0)
			ev->ev_saved = value;
		extra_width = 0;
	}

	(void) fflush(a->pa_printfile);
}

/*
 * Print output
 */

void
pmcstat_print_pmcs(struct pmcstat_args *a)
{
	static int linecount = 0;

	/* check if we need to print a header line */
	if (++linecount > pmcstat_displayheight) {
		(void) fprintf(a->pa_printfile, "\n");
		linecount = 1;
	}
	if (linecount == 1)
		pmcstat_print_headers(a);
	(void) fprintf(a->pa_printfile, "\n");

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
pmcstat_start_process(void)
{
	/* Signal the child to proceed. */
	if (write(pmcstat_sockpair[PARENTSOCKET], "!", 1) != 1)
		err(EX_OSERR, "ERROR (parent): write of token failed");

	(void) close(pmcstat_sockpair[PARENTSOCKET]);
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
	    "\t -G file\t write a system-wide callgraph to \"file\"\n"
	    "\t -M file\t print executable/gmon file map to \"file\"\n"
	    "\t -N\t\t (toggle) capture callchains\n"
	    "\t -O file\t send log output to \"file\"\n"
	    "\t -P spec\t allocate a process-private sampling PMC\n"
	    "\t -R file\t read events from \"file\"\n"
	    "\t -S spec\t allocate a system-wide sampling PMC\n"
	    "\t -W\t\t (toggle) show counts per context switch\n"
	    "\t -c cpu-list\t set cpus for subsequent system-wide PMCs\n"
	    "\t -d\t\t (toggle) track descendants\n"
	    "\t -g\t\t produce gprof(1) compatible profiles\n"
	    "\t -k dir\t\t set the path to the kernel\n"
	    "\t -n rate\t set sampling rate\n"
	    "\t -o file\t send print output to \"file\"\n"
	    "\t -p spec\t allocate a process-private counting PMC\n"
	    "\t -q\t\t suppress verbosity\n"
	    "\t -r fsroot\t specify FS root directory\n"
	    "\t -s spec\t allocate a system-wide counting PMC\n"
	    "\t -t process-spec attach to running processes matching "
		"\"process-spec\"\n"
	    "\t -v\t\t increase verbosity\n"
	    "\t -w secs\t set printing time interval\n"
	    "\t -z depth\t limit callchain display depth"
	);
}

/*
 * Main
 */

int
main(int argc, char **argv)
{
	double interval;
	int option, npmc, ncpu, haltedcpus;
	int c, check_driver_stats, current_cpu, current_sampling_count;
	int do_callchain, do_descendants, do_logproccsw, do_logprocexit;
	int do_print;
	size_t dummy;
	int graphdepth;
	int pipefd[2];
	int use_cumulative_counts;
	uint32_t cpumask;
	char *end, *tmp;
	const char *errmsg, *graphfilename;
	enum pmcstat_state runstate;
	struct pmc_driverstats ds_start, ds_end;
	struct pmcstat_ev *ev;
	struct sigaction sa;
	struct kevent kev;
	struct winsize ws;
	struct stat sb;
	char buffer[PATH_MAX];

	check_driver_stats      = 0;
	current_cpu 		= 0;
	current_sampling_count  = DEFAULT_SAMPLE_COUNT;
	do_callchain		= 1;
	do_descendants          = 0;
	do_logproccsw           = 0;
	do_logprocexit          = 0;
	use_cumulative_counts   = 0;
	graphfilename		= "-";
	args.pa_required	= 0;
	args.pa_flags		= 0;
	args.pa_verbosity	= 1;
	args.pa_logfd		= -1;
	args.pa_fsroot		= "";
	args.pa_kernel		= strdup("/boot/kernel");
	args.pa_samplesdir	= ".";
	args.pa_printfile	= stderr;
	args.pa_graphdepth	= DEFAULT_CALLGRAPH_DEPTH;
	args.pa_graphfile	= NULL;
	args.pa_interval	= DEFAULT_WAIT_INTERVAL;
	args.pa_mapfilename	= NULL;
	args.pa_inputpath	= NULL;
	args.pa_outputpath	= NULL;
	STAILQ_INIT(&args.pa_events);
	SLIST_INIT(&args.pa_targets);
	bzero(&ds_start, sizeof(ds_start));
	bzero(&ds_end, sizeof(ds_end));
	ev = NULL;

	/*
	 * The initial CPU mask specifies all non-halted CPUS in the
	 * system.
	 */
	dummy = sizeof(int);
	if (sysctlbyname("hw.ncpu", &ncpu, &dummy, NULL, 0) < 0)
		err(EX_OSERR, "ERROR: Cannot determine the number of CPUs");
	cpumask = (1 << ncpu) - 1;
	haltedcpus = 0;
	if (ncpu > 1) {
		if (sysctlbyname("machdep.hlt_cpus", &haltedcpus, &dummy,
		    NULL, 0) < 0)
			err(EX_OSERR, "ERROR: Cannot determine which CPUs are "
			    "halted");
		cpumask &= ~haltedcpus;
	}

	while ((option = getopt(argc, argv,
	    "CD:EG:M:NO:P:R:S:Wc:dgk:m:n:o:p:qr:s:t:vw:z:")) != -1)
		switch (option) {
		case 'C':	/* cumulative values */
			use_cumulative_counts = !use_cumulative_counts;
			args.pa_required |= FLAG_HAS_COUNTING_PMCS;
			break;

		case 'c':	/* CPU */

			if (optarg[0] == '*' && optarg[1] == '\0')
				cpumask = ((1 << ncpu) - 1) & ~haltedcpus;
			else
				cpumask = pmcstat_get_cpumask(optarg);

			args.pa_required |= FLAG_HAS_SYSTEM_PMCS;
			break;

		case 'D':
			if (stat(optarg, &sb) < 0)
				err(EX_OSERR, "ERROR: Cannot stat \"%s\"",
				    optarg);
			if (!S_ISDIR(sb.st_mode))
				errx(EX_USAGE, "ERROR: \"%s\" is not a "
				    "directory.", optarg);
			args.pa_samplesdir = optarg;
			args.pa_flags     |= FLAG_HAS_SAMPLESDIR;
			args.pa_required  |= FLAG_DO_GPROF;
			break;

		case 'd':	/* toggle descendents */
			do_descendants = !do_descendants;
			args.pa_required |= FLAG_HAS_PROCESS_PMCS;
			break;

		case 'G':	/* produce a system-wide callgraph */
			args.pa_flags |= FLAG_DO_CALLGRAPHS;
			graphfilename = optarg;
			break;

		case 'g':	/* produce gprof compatible profiles */
			args.pa_flags |= FLAG_DO_GPROF;
			break;

		case 'k':	/* pathname to the kernel */
			free(args.pa_kernel);
			args.pa_kernel = strdup(optarg);
			args.pa_required |= FLAG_DO_ANALYSIS;
			args.pa_flags    |= FLAG_HAS_KERNELPATH;
			break;

		case 'm':
			args.pa_flags |= FLAG_WANTS_MAPPINGS;
			graphfilename = optarg;
			break;

		case 'E':	/* log process exit */
			do_logprocexit = !do_logprocexit;
			args.pa_required |= (FLAG_HAS_PROCESS_PMCS |
			    FLAG_HAS_COUNTING_PMCS | FLAG_HAS_OUTPUT_LOGFILE);
			break;

		case 'M':	/* mapfile */
			args.pa_mapfilename = optarg;
			break;

		case 'N':
			do_callchain = !do_callchain;
			args.pa_required |= FLAG_HAS_SAMPLING_PMCS;
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
				args.pa_required |= (FLAG_HAS_COMMANDLINE |
				    FLAG_HAS_TARGET);
			}

			if (option == 'P' || option == 'S') {
				args.pa_flags |= FLAG_HAS_SAMPLING_PMCS;
				args.pa_required |= (FLAG_HAS_PIPE |
				    FLAG_HAS_OUTPUT_LOGFILE);
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
				ev->ev_cpu = ffs(cpumask) - 1;
			else
				ev->ev_cpu = PMC_CPU_ANY;

			ev->ev_flags = 0;
			if (do_callchain)
				ev->ev_flags |= PMC_F_CALLCHAIN;
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

			STAILQ_INSERT_TAIL(&args.pa_events, ev, ev_next);

			if (option == 's' || option == 'S')
				pmcstat_clone_event_descriptor(&args, ev,
				    cpumask & ~(1 << ev->ev_cpu));

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
			if (args.pa_printfile != NULL)
				(void) fclose(args.pa_printfile);
			if ((args.pa_printfile = fopen(optarg, "w")) == NULL)
				errx(EX_OSERR, "ERROR: cannot open \"%s\" for "
				    "writing.", optarg);
			args.pa_flags |= FLAG_DO_PRINT;
			break;

		case 'O':	/* sampling output */
			if (args.pa_outputpath)
				errx(EX_USAGE, "ERROR: option -O may only be "
				    "specified once.");
			args.pa_outputpath = optarg;
			args.pa_flags |= FLAG_HAS_OUTPUT_LOGFILE;
			break;

		case 'q':	/* quiet mode */
			args.pa_verbosity = 0;
			break;

		case 'r':	/* root FS path */
			args.pa_fsroot = optarg;
			break;

		case 'R':	/* read an existing log file */
			if (args.pa_inputpath != NULL)
				errx(EX_USAGE, "ERROR: option -R may only be "
				    "specified once.");
			args.pa_inputpath = optarg;
			if (args.pa_printfile == stderr)
				args.pa_printfile = stdout;
			args.pa_flags |= FLAG_READ_LOGFILE;
			break;

		case 't':	/* target pid or process name */
			pmcstat_find_targets(&args, optarg);

			args.pa_flags |= FLAG_HAS_TARGET;
			args.pa_required |= FLAG_HAS_PROCESS_PMCS;
			break;

		case 'v':	/* verbose */
			args.pa_verbosity++;
			break;

		case 'w':	/* wait interval */
			interval = strtod(optarg, &end);
			if (*end != '\0' || interval <= 0)
				errx(EX_USAGE, "ERROR: Illegal wait interval "
				    "value \"%s\".", optarg);
			args.pa_flags |= FLAG_HAS_WAIT_INTERVAL;
			args.pa_required |= FLAG_HAS_COUNTING_PMCS;
			args.pa_interval = interval;
			break;

		case 'W':	/* toggle LOG_CSW */
			do_logproccsw = !do_logproccsw;
			args.pa_required |= (FLAG_HAS_PROCESS_PMCS |
			    FLAG_HAS_COUNTING_PMCS | FLAG_HAS_OUTPUT_LOGFILE);
			break;

		case 'z':
			graphdepth = strtod(optarg, &end);
			if (*end != '\0' || graphdepth <= 0)
				errx(EX_USAGE, "ERROR: Illegal callchain "
				    "depth \"%s\".", optarg);
			args.pa_graphdepth = graphdepth;
			args.pa_required |= FLAG_DO_CALLGRAPHS;
			break;

		case '?':
		default:
			pmcstat_show_usage();
			break;

		}

	args.pa_argc = (argc -= optind);
	args.pa_argv = (argv += optind);

	args.pa_cpumask = cpumask; /* For selecting CPUs using -R. */

	if (argc)	/* command line present */
		args.pa_flags |= FLAG_HAS_COMMANDLINE;

	if (args.pa_flags & (FLAG_DO_GPROF | FLAG_DO_CALLGRAPHS |
	    FLAG_WANTS_MAPPINGS))
		args.pa_flags |= FLAG_DO_ANALYSIS;

	/*
	 * Check invocation syntax.
	 */

	/* disallow -O and -R together */
	if (args.pa_outputpath && args.pa_inputpath)
		errx(EX_USAGE, "ERROR: options -O and -R are mutually "
		    "exclusive.");

	/* -m option is allowed with -R only. */
	if (args.pa_flags & FLAG_WANTS_MAPPINGS && args.pa_inputpath == NULL)
		errx(EX_USAGE, "ERROR: option -m requires an input file");

	/* -m option is not allowed combined with -g or -G. */
	if (args.pa_flags & FLAG_WANTS_MAPPINGS &&
	    args.pa_flags & (FLAG_DO_GPROF | FLAG_DO_CALLGRAPHS))
		errx(EX_USAGE, "ERROR: option -m and -g | -G are mutually "
		    "exclusive");

	if (args.pa_flags & FLAG_READ_LOGFILE) {
		errmsg = NULL;
		if (args.pa_flags & FLAG_HAS_COMMANDLINE)
			errmsg = "a command line specification";
		else if (args.pa_flags & FLAG_HAS_TARGET)
			errmsg = "option -t";
		else if (!STAILQ_EMPTY(&args.pa_events))
			errmsg = "a PMC event specification";
		if (errmsg)
			errx(EX_USAGE, "ERROR: option -R may not be used with "
			    "%s.", errmsg);
	} else if (STAILQ_EMPTY(&args.pa_events))
		/* All other uses require a PMC spec. */
		pmcstat_show_usage();

	/* check for -t pid without a process PMC spec */
	if ((args.pa_required & FLAG_HAS_TARGET) &&
	    (args.pa_flags & FLAG_HAS_PROCESS_PMCS) == 0)
		errx(EX_USAGE, "ERROR: option -t requires a process mode PMC "
		    "to be specified.");

	/* check for process-mode options without a command or -t pid */
	if ((args.pa_required & FLAG_HAS_PROCESS_PMCS) &&
	    (args.pa_flags & (FLAG_HAS_COMMANDLINE | FLAG_HAS_TARGET)) == 0)
		errx(EX_USAGE, "ERROR: options -d, -E, -p, -P, and -W require "
		    "a command line or target process.");

	/* check for -p | -P without a target process of some sort */
	if ((args.pa_required & (FLAG_HAS_COMMANDLINE | FLAG_HAS_TARGET)) &&
	    (args.pa_flags & (FLAG_HAS_COMMANDLINE | FLAG_HAS_TARGET)) == 0)
		errx(EX_USAGE, "ERROR: options -P and -p require a "
		    "target process or a command line.");

	/* check for process-mode options without a process-mode PMC */
	if ((args.pa_required & FLAG_HAS_PROCESS_PMCS) &&
	    (args.pa_flags & FLAG_HAS_PROCESS_PMCS) == 0)
		errx(EX_USAGE, "ERROR: options -d, -E, and -W require a "
		    "process mode PMC to be specified.");

	/* check for -c cpu with no system mode PMCs or logfile. */
	if ((args.pa_required & FLAG_HAS_SYSTEM_PMCS) &&
	    (args.pa_flags & FLAG_HAS_SYSTEM_PMCS) == 0 &&
	    (args.pa_flags & FLAG_READ_LOGFILE) == 0)
		errx(EX_USAGE, "ERROR: option -c requires at least one "
		    "system mode PMC to be specified.");

	/* check for counting mode options without a counting PMC */
	if ((args.pa_required & FLAG_HAS_COUNTING_PMCS) &&
	    (args.pa_flags & FLAG_HAS_COUNTING_PMCS) == 0)
		errx(EX_USAGE, "ERROR: options -C, -W, -o and -w require at "
		    "least one counting mode PMC to be specified.");

	/* check for sampling mode options without a sampling PMC spec */
	if ((args.pa_required & FLAG_HAS_SAMPLING_PMCS) &&
	    (args.pa_flags & FLAG_HAS_SAMPLING_PMCS) == 0)
		errx(EX_USAGE, "ERROR: options -N, -n and -O require at "
		    "least one sampling mode PMC to be specified.");

	/* check if -g/-G are being used correctly */
	if ((args.pa_flags & FLAG_DO_ANALYSIS) &&
	    !(args.pa_flags & (FLAG_HAS_SAMPLING_PMCS|FLAG_READ_LOGFILE)))
		errx(EX_USAGE, "ERROR: options -g/-G require sampling PMCs "
		    "or -R to be specified.");

	/* check if -O was spuriously specified */
	if ((args.pa_flags & FLAG_HAS_OUTPUT_LOGFILE) &&
	    (args.pa_required & FLAG_HAS_OUTPUT_LOGFILE) == 0)
		errx(EX_USAGE,
		    "ERROR: option -O is used only with options "
		    "-E, -P, -S and -W.");

	/* -k kernel path require -g/-G or -R */
	if ((args.pa_flags & FLAG_HAS_KERNELPATH) &&
	    (args.pa_flags & FLAG_DO_ANALYSIS) == 0 &&
	    (args.pa_flags & FLAG_READ_LOGFILE) == 0)
	    errx(EX_USAGE, "ERROR: option -k is only used with -g/-R.");

	/* -D only applies to gprof output mode (-g) */
	if ((args.pa_flags & FLAG_HAS_SAMPLESDIR) &&
	    (args.pa_flags & FLAG_DO_GPROF) == 0)
	    errx(EX_USAGE, "ERROR: option -D is only used with -g.");

	/* -M mapfile requires -g or -R */
	if (args.pa_mapfilename != NULL &&
	    (args.pa_flags & FLAG_DO_GPROF) == 0 &&
	    (args.pa_flags & FLAG_READ_LOGFILE) == 0)
	    errx(EX_USAGE, "ERROR: option -M is only used with -g/-R.");

	/*
	 * Disallow textual output of sampling PMCs if counting PMCs
	 * have also been asked for, mostly because the combined output
	 * is difficult to make sense of.
	 */
	if ((args.pa_flags & FLAG_HAS_COUNTING_PMCS) &&
	    (args.pa_flags & FLAG_HAS_SAMPLING_PMCS) &&
	    ((args.pa_flags & FLAG_HAS_OUTPUT_LOGFILE) == 0))
		errx(EX_USAGE, "ERROR: option -O is required if counting and "
		    "sampling PMCs are specified together.");

	/*
	 * Check if "-k kerneldir" was specified, and if whether
	 * 'kerneldir' actually refers to a a file.  If so, use
	 * `dirname path` to determine the kernel directory.
	 */
	if (args.pa_flags & FLAG_HAS_KERNELPATH) {
		(void) snprintf(buffer, sizeof(buffer), "%s%s", args.pa_fsroot,
		    args.pa_kernel);
		if (stat(buffer, &sb) < 0)
			err(EX_OSERR, "ERROR: Cannot locate kernel \"%s\"",
			    buffer);
		if (!S_ISREG(sb.st_mode) && !S_ISDIR(sb.st_mode))
			errx(EX_USAGE, "ERROR: \"%s\": Unsupported file type.",
			    buffer);
		if (!S_ISDIR(sb.st_mode)) {
			tmp = args.pa_kernel;
			args.pa_kernel = strdup(dirname(args.pa_kernel));
			free(tmp);
			(void) snprintf(buffer, sizeof(buffer), "%s%s",
			    args.pa_fsroot, args.pa_kernel);
			if (stat(buffer, &sb) < 0)
				err(EX_OSERR, "ERROR: Cannot stat \"%s\"",
				    buffer);
			if (!S_ISDIR(sb.st_mode))
				errx(EX_USAGE, "ERROR: \"%s\" is not a "
				    "directory.", buffer);
		}
	}

	/*
	 * If we have a callgraph be created, select the outputfile.
	 */
	if (args.pa_flags & FLAG_DO_CALLGRAPHS) {
		if (strcmp(graphfilename, "-") == 0)
		    args.pa_graphfile = args.pa_printfile;
		else {
			args.pa_graphfile = fopen(graphfilename, "w");
			if (args.pa_graphfile == NULL)
				err(EX_OSERR, "ERROR: cannot open \"%s\" "
				    "for writing", graphfilename);
		}
	}
	if (args.pa_flags & FLAG_WANTS_MAPPINGS) {
		args.pa_graphfile = fopen(graphfilename, "w");
		if (args.pa_graphfile == NULL)
			err(EX_OSERR, "ERROR: cannot open \"%s\" for writing",
			    graphfilename);
	}

	/* if we've been asked to process a log file, do that and exit */
	if (args.pa_flags & FLAG_READ_LOGFILE) {
		/*
		 * Print the log in textual form if we haven't been
		 * asked to generate profiling information.
		 */
		if ((args.pa_flags & FLAG_DO_ANALYSIS) == 0)
			args.pa_flags |= FLAG_DO_PRINT;

		pmcstat_initialize_logging(&args);
		args.pa_logfd = pmcstat_open_log(args.pa_inputpath,
		    PMCSTAT_OPEN_FOR_READ);
		if ((args.pa_logparser = pmclog_open(args.pa_logfd)) == NULL)
			err(EX_OSERR, "ERROR: Cannot create parser");
		pmcstat_process_log(&args);
		pmcstat_shutdown_logging(&args);
		exit(EX_OK);
	}

	/* otherwise, we've been asked to collect data */
	if (pmc_init() < 0)
		err(EX_UNAVAILABLE,
		    "ERROR: Initialization of the pmc(3) library failed");

	if ((npmc = pmc_npmc(0)) < 0) /* assume all CPUs are identical */
		err(EX_OSERR, "ERROR: Cannot determine the number of PMCs "
		    "on CPU %d", 0);

	/* Allocate a kqueue */
	if ((pmcstat_kq = kqueue()) < 0)
		err(EX_OSERR, "ERROR: Cannot allocate kqueue");

	/*
	 * Configure the specified log file or setup a default log
	 * consumer via a pipe.
	 */
	if (args.pa_required & FLAG_HAS_OUTPUT_LOGFILE) {
		if (args.pa_outputpath)
			args.pa_logfd = pmcstat_open_log(args.pa_outputpath,
			    PMCSTAT_OPEN_FOR_WRITE);
		else {
			/*
			 * process the log on the fly by reading it in
			 * through a pipe.
			 */
			if (pipe(pipefd) < 0)
				err(EX_OSERR, "ERROR: pipe(2) failed");

			if (fcntl(pipefd[READPIPEFD], F_SETFL, O_NONBLOCK) < 0)
				err(EX_OSERR, "ERROR: fcntl(2) failed");

			EV_SET(&kev, pipefd[READPIPEFD], EVFILT_READ, EV_ADD,
			    0, 0, NULL);

			if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
				err(EX_OSERR, "ERROR: Cannot register kevent");

			args.pa_logfd = pipefd[WRITEPIPEFD];

			args.pa_flags |= (FLAG_HAS_PIPE | FLAG_DO_PRINT);
			args.pa_logparser = pmclog_open(pipefd[READPIPEFD]);
		}

		if (pmc_configure_logfile(args.pa_logfd) < 0)
			err(EX_OSERR, "ERROR: Cannot configure log file");
	}

	/* remember to check for driver errors if we are sampling or logging */
	check_driver_stats = (args.pa_flags & FLAG_HAS_SAMPLING_PMCS) ||
	    (args.pa_flags & FLAG_HAS_OUTPUT_LOGFILE);

	/*
	 * Allocate PMCs.
	 */

	STAILQ_FOREACH(ev, &args.pa_events, ev_next) {
	    if (pmc_allocate(ev->ev_spec, ev->ev_mode,
		    ev->ev_flags, ev->ev_cpu, &ev->ev_pmcid) < 0)
		    err(EX_OSERR, "ERROR: Cannot allocate %s-mode pmc with "
			"specification \"%s\"",
			PMC_IS_SYSTEM_MODE(ev->ev_mode) ? "system" : "process",
			ev->ev_spec);

	    if (PMC_IS_SAMPLING_MODE(ev->ev_mode) &&
		pmc_set(ev->ev_pmcid, ev->ev_count) < 0)
		    err(EX_OSERR, "ERROR: Cannot set sampling count "
			"for PMC \"%s\"", ev->ev_name);
	}

	/* compute printout widths */
	STAILQ_FOREACH(ev, &args.pa_events, ev_next) {
		int counter_width;
		int display_width;
		int header_width;

		(void) pmc_width(ev->ev_pmcid, &counter_width);
		header_width = strlen(ev->ev_name) + 2; /* prefix '%c/' */
		display_width = (int) floor(counter_width / 3.32193) + 1;

		if (PMC_IS_SYSTEM_MODE(ev->ev_mode))
			header_width += 3; /* 2 digit CPU number + '/' */

		if (header_width > display_width) {
			ev->ev_fieldskip = 0;
			ev->ev_fieldwidth = header_width;
		} else {
			ev->ev_fieldskip = display_width -
			    header_width;
			ev->ev_fieldwidth = display_width;
		}
	}

	/*
	 * If our output is being set to a terminal, register a handler
	 * for window size changes.
	 */

	if (isatty(fileno(args.pa_printfile))) {

		if (ioctl(fileno(args.pa_printfile), TIOCGWINSZ, &ws) < 0)
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

	/* setup a timer if we have counting mode PMCs needing to be printed */
	if ((args.pa_flags & FLAG_HAS_COUNTING_PMCS) &&
	    (args.pa_required & FLAG_HAS_OUTPUT_LOGFILE) == 0) {
		EV_SET(&kev, 0, EVFILT_TIMER, EV_ADD, 0,
		    args.pa_interval * 1000, NULL);

		if (kevent(pmcstat_kq, &kev, 1, NULL, 0, NULL) < 0)
			err(EX_OSERR, "ERROR: Cannot register kevent for "
			    "timer");
	}

	/* attach PMCs to the target process, starting it if specified */
	if (args.pa_flags & FLAG_HAS_COMMANDLINE)
		pmcstat_create_process(&args);

	if (check_driver_stats && pmc_get_driver_stats(&ds_start) < 0)
		err(EX_OSERR, "ERROR: Cannot retrieve driver statistics");

	/* Attach process pmcs to the target process. */
	if (args.pa_flags & (FLAG_HAS_TARGET | FLAG_HAS_COMMANDLINE)) {
		if (SLIST_EMPTY(&args.pa_targets))
			errx(EX_DATAERR, "ERROR: No matching target "
			    "processes.");
		if (args.pa_flags & FLAG_HAS_PROCESS_PMCS)
			pmcstat_attach_pmcs(&args);

		if (pmcstat_kvm) {
			kvm_close(pmcstat_kvm);
			pmcstat_kvm = NULL;
		}
	}

	/* start the pmcs */
	pmcstat_start_pmcs(&args);

	/* start the (commandline) process if needed */
	if (args.pa_flags & FLAG_HAS_COMMANDLINE)
		pmcstat_start_process();

	/* initialize logging if printing the configured log */
	if ((args.pa_flags & FLAG_DO_PRINT) &&
	    (args.pa_flags & (FLAG_HAS_PIPE | FLAG_HAS_OUTPUT_LOGFILE)))
		pmcstat_initialize_logging(&args);

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
			if (args.pa_flags & (FLAG_HAS_OUTPUT_LOGFILE |
				FLAG_HAS_PIPE))
				runstate = pmcstat_close_log(&args);
			else
				runstate = PMCSTAT_FINISHED;
			do_print = 1;
			break;

		case EVFILT_READ:  /* log file data is present */
			runstate = pmcstat_process_log(&args);
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
				if (args.pa_flags & (FLAG_HAS_OUTPUT_LOGFILE |
				    FLAG_HAS_PIPE)) {
					runstate = pmcstat_close_log(&args);
					if (args.pa_flags &
					    (FLAG_DO_PRINT|FLAG_DO_ANALYSIS))
						pmcstat_process_log(&args);
				}
				do_print = 1; /* print PMCs at exit */
				runstate = PMCSTAT_FINISHED;
			} else if (kev.ident == SIGINT) {
				/* Kill the child process if we started it */
				if (args.pa_flags & FLAG_HAS_COMMANDLINE)
					pmcstat_kill_process(&args);
				runstate = PMCSTAT_FINISHED;
			} else if (kev.ident == SIGWINCH) {
				if (ioctl(fileno(args.pa_printfile),
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

		if (do_print &&
		    (args.pa_required & FLAG_HAS_OUTPUT_LOGFILE) == 0) {
			pmcstat_print_pmcs(&args);
			if (runstate == PMCSTAT_FINISHED && /* final newline */
			    (args.pa_flags & FLAG_DO_PRINT) == 0)
				(void) fprintf(args.pa_printfile, "\n");
			do_print = 0;
		}

	} while (runstate != PMCSTAT_FINISHED);

	/* flush any pending log entries */
	if (args.pa_flags & (FLAG_HAS_OUTPUT_LOGFILE | FLAG_HAS_PIPE))
		pmc_flush_logfile();

	pmcstat_cleanup(&args);

	free(args.pa_kernel);

	/* check if the driver lost any samples or events */
	if (check_driver_stats) {
		if (pmc_get_driver_stats(&ds_end) < 0)
			err(EX_OSERR, "ERROR: Cannot retrieve driver "
			    "statistics");
		if (ds_start.pm_intr_bufferfull != ds_end.pm_intr_bufferfull &&
		    args.pa_verbosity > 0)
			warnx("WARNING: some samples were dropped.  Please "
			    "consider tuning the \"kern.hwpmc.nsamples\" "
			    "tunable.");
		if (ds_start.pm_buffer_requests_failed !=
		    ds_end.pm_buffer_requests_failed &&
		    args.pa_verbosity > 0)
			warnx("WARNING: some events were discarded.  Please "
			    "consider tuning the \"kern.hwpmc.nbuffers\" "
			    "tunable.");
	}

	exit(EX_OK);
}
