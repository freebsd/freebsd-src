/*
 * $FreeBSD$
 */

/*
 *  This file defines the interface between top and the machine-dependent
 *  module.  It is NOT machine dependent and should not need to be changed
 *  for any specific machine.
 */
#ifndef MACHINE_H
#define MACHINE_H

#include "top.h"

/*
 * the statics struct is filled in by machine_init
 */
struct statics
{
    char **procstate_names;
    char **cpustate_names;
    char **memory_names;
    char **arc_names;
    char **carc_names;
    char **swap_names;
    char **order_names;
    int ncpus;
};

/*
 * the system_info struct is filled in by a machine dependent routine.
 */

struct system_info
{
    int    last_pid;
    double load_avg[NUM_AVERAGES];
    int    p_total;
    int    p_pactive;     /* number of procs considered "active" */
    int    *procstates;
    int    *cpustates;
    int    *memory;
    int    *arc;
    int    *carc;
    int    *swap;
    struct timeval boottime;
    int    ncpus;
};

/* cpu_states is an array of percentages * 10.  For example, 
   the (integer) value 105 is 10.5% (or .105).
 */

/*
 * the process_select struct tells get_process_info what processes we
 * are interested in seeing
 */

struct process_select
{
    int idle;		/* show idle processes */
    int self;		/* show self */
    int system;		/* show system processes */
    int thread;		/* show threads */
#define TOP_MAX_UIDS 8
    int uid[TOP_MAX_UIDS];	/* only these uids (unless uid[0] == -1) */
    int wcpu;		/* show weighted cpu */
    int jid;		/* only this jid (unless jid == -1) */
    int jail;		/* show jail ID */
    int swap;		/* show swap usage */
    int kidle;		/* show per-CPU idle threads */
    char *command;	/* only this command (unless == NULL) */
};

/* routines defined by the machine dependent module */

char	*format_header(char *uname_field);
char	*format_next_process(caddr_t handle, char *(*get_userid)(uid_t),
	    int flags);
void	 toggle_pcpustats(void);
void	 get_system_info(struct system_info *si);
int	 machine_init(struct statics *statics);
int	 proc_owner(int pid);

/* non-int routines typically used by the machine dependent module */
extern struct process_select ps;

void *
get_process_info(struct system_info *si, struct process_select *sel,
    int (*compare)(const void *, const void *));

#endif /* MACHINE_H */
