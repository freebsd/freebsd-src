/*-
 * Copyright (c) 2005-2007, Joseph Koshy
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
 *
 * $FreeBSD$
 */

#ifndef	_PMCSTAT_H_
#define	_PMCSTAT_H_

#include <sys/_cpuset.h>

#define	FLAG_HAS_TARGET			0x00000001	/* process target */
#define	FLAG_HAS_WAIT_INTERVAL		0x00000002	/* -w secs */
#define	FLAG_HAS_OUTPUT_LOGFILE		0x00000004	/* -O file or pipe */
#define	FLAG_HAS_COMMANDLINE		0x00000008	/* command */
#define	FLAG_HAS_SAMPLING_PMCS		0x00000010	/* -S or -P */
#define	FLAG_HAS_COUNTING_PMCS		0x00000020	/* -s or -p */
#define	FLAG_HAS_PROCESS_PMCS		0x00000040	/* -P or -p */
#define	FLAG_HAS_SYSTEM_PMCS		0x00000080	/* -S or -s */
#define	FLAG_HAS_PIPE			0x00000100	/* implicit log */
#define	FLAG_READ_LOGFILE		0x00000200	/* -R file */
#define	FLAG_DO_GPROF			0x00000400	/* -g */
#define	FLAG_HAS_SAMPLESDIR		0x00000800	/* -D dir */
#define	FLAG_HAS_KERNELPATH		0x00001000	/* -k kernel */
#define	FLAG_DO_PRINT			0x00002000	/* -o */
#define	FLAG_DO_CALLGRAPHS		0x00004000	/* -G or -F */
#define	FLAG_DO_ANNOTATE		0x00008000	/* -m */
#define	FLAG_DO_TOP			0x00010000	/* -T */
#define	FLAG_DO_ANALYSIS		0x00020000	/* -g or -G or -m or -T */
#define	FLAGS_HAS_CPUMASK		0x00040000	/* -c */

#define	DEFAULT_SAMPLE_COUNT		65536
#define	DEFAULT_WAIT_INTERVAL		5.0
#define	DEFAULT_DISPLAY_HEIGHT		256		/* file virtual height */
#define	DEFAULT_DISPLAY_WIDTH		1024		/* file virtual width */
#define	DEFAULT_BUFFER_SIZE		4096
#define	DEFAULT_CALLGRAPH_DEPTH		4

#define	PRINT_HEADER_PREFIX		"# "
#define	READPIPEFD			0
#define	WRITEPIPEFD			1
#define	NPIPEFD				2

#define	NSOCKPAIRFD			2
#define	PARENTSOCKET			0
#define	CHILDSOCKET			1

#define	PMCSTAT_OPEN_FOR_READ		0
#define	PMCSTAT_OPEN_FOR_WRITE		1
#define	PMCSTAT_DEFAULT_NW_HOST		"localhost"
#define	PMCSTAT_DEFAULT_NW_PORT		"9000"
#define	PMCSTAT_NHASH			256
#define	PMCSTAT_HASH_MASK		0xFF

#define	PMCSTAT_LDD_COMMAND		"/usr/bin/ldd"

#define	PMCSTAT_PRINT_ENTRY(T,...) do {					\
		(void) fprintf(args.pa_printfile, "%-9s", T);		\
		(void) fprintf(args.pa_printfile, " "  __VA_ARGS__);	\
		(void) fprintf(args.pa_printfile, "\n");		\
	} while (0)

#define PMCSTAT_PL_NONE		0
#define PMCSTAT_PL_CALLGRAPH	1
#define PMCSTAT_PL_GPROF	2
#define PMCSTAT_PL_ANNOTATE	3
#define PMCSTAT_PL_CALLTREE	4

#define PMCSTAT_TOP_DELTA 	0
#define PMCSTAT_TOP_ACCUM	1

#define	min(A,B)		((A) < (B) ? (A) : (B))
#define	max(A,B)		((A) > (B) ? (A) : (B))

enum pmcstat_state {
	PMCSTAT_FINISHED = 0,
	PMCSTAT_EXITING  = 1,
	PMCSTAT_RUNNING  = 2
};

struct pmcstat_ev {
	STAILQ_ENTRY(pmcstat_ev) ev_next;
	int		ev_count; /* associated count if in sampling mode */
	uint32_t	ev_cpu;	  /* cpus for this event */
	int		ev_cumulative;  /* show cumulative counts */
	int		ev_flags; /* PMC_F_* */
	int		ev_fieldskip;   /* #leading spaces */
	int		ev_fieldwidth;  /* print width */
	enum pmc_mode	ev_mode;  /* desired mode */
	char	       *ev_name;  /* (derived) event name */
	pmc_id_t	ev_pmcid; /* allocated ID */
	pmc_value_t	ev_saved; /* for incremental counts */
	char	       *ev_spec;  /* event specification */
};

struct pmcstat_target {
	SLIST_ENTRY(pmcstat_target) pt_next;
	pid_t		pt_pid;
};

struct pmcstat_args {
	int	pa_flags;		/* argument flags */
	int	pa_required;		/* required features */
	int	pa_pplugin;		/* pre-processing plugin */
	int	pa_plugin;		/* analysis plugin */
	int	pa_verbosity;		/* verbosity level */
	FILE	*pa_printfile;		/* where to send printed output */
	int	pa_logfd;		/* output log file */
	char	*pa_inputpath;		/* path to input log */
	char	*pa_outputpath;		/* path to output log */
	void	*pa_logparser;		/* log file parser */
	const char	*pa_fsroot;	/* FS root where executables reside */
	char	*pa_kernel;		/* pathname of the kernel */
	const char	*pa_samplesdir;	/* directory for profile files */
	const char	*pa_mapfilename;/* mapfile name */
	FILE	*pa_graphfile;		/* where to send the callgraph */
	int	pa_graphdepth;		/* print depth for callgraphs */
	double	pa_interval;		/* printing interval in seconds */
	cpuset_t	pa_cpumask;	/* filter for CPUs analysed */
	int	pa_ctdumpinstr;		/* dump instructions with calltree */
	int	pa_topmode;		/* delta or accumulative */
	int	pa_toptty;		/* output to tty or file */
	int	pa_topcolor;		/* terminal support color */
	int	pa_mergepmc;		/* merge PMC with same name */
	int	pa_argc;
	char	**pa_argv;
	STAILQ_HEAD(, pmcstat_ev) pa_events;
	SLIST_HEAD(, pmcstat_target) pa_targets;
};

extern int pmcstat_displayheight;	/* current terminal height */
extern int pmcstat_displaywidth;	/* current terminal width */
extern struct pmcstat_args args;	/* command line args */

/* Function prototypes */
void	pmcstat_attach_pmcs(void);
void	pmcstat_cleanup(void);
int	pmcstat_close_log(void);
void	pmcstat_create_process(void);
void	pmcstat_find_targets(const char *_arg);
void	pmcstat_initialize_logging(void);
void	pmcstat_kill_process(void);
int	pmcstat_open_log(const char *_p, int _mode);
void	pmcstat_print_counters(void);
void	pmcstat_print_headers(void);
void	pmcstat_print_pmcs(void);
void	pmcstat_show_usage(void);
void	pmcstat_shutdown_logging(void);
void	pmcstat_start_pmcs(void);
void	pmcstat_start_process(void);
int	pmcstat_process_log(void);
int	pmcstat_keypress_log(void);
void	pmcstat_display_log(void);
void	pmcstat_pluginconfigure_log(char *_opt);
void    pmcstat_topexit(void);

#endif	/* _PMCSTAT_H_ */
