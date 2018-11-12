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
    char **swap_names;
#ifdef ORDER
    char **order_names;
#endif
    int ncpus;
};

/*
 * the system_info struct is filled in by a machine dependent routine.
 */

#ifdef p_active     /* uw7 define macro p_active */
#define P_ACTIVE p_pactive
#else
#define P_ACTIVE p_active
#endif

struct system_info
{
    int    last_pid;
    double load_avg[NUM_AVERAGES];
    int    p_total;
    int    P_ACTIVE;     /* number of procs considered "active" */
    int    *procstates;
    int    *cpustates;
    int    *memory;
    int    *arc;
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
    int uid;		/* only this uid (unless uid == -1) */
    int wcpu;		/* show weighted cpu */
    int jid;		/* only this jid (unless jid == -1) */
    int jail;		/* show jail ID */
    int swap;		/* show swap usage */
    int kidle;		/* show per-CPU idle threads */
    char *command;	/* only this command (unless == NULL) */
};

/* routines defined by the machine dependent module */

char	*format_header();
char	*format_next_process();
void	 toggle_pcpustats(void);
void	 get_system_info(struct system_info *si);
int	 machine_init(struct statics *statics, char do_unames);
int	 proc_owner(int pid);

/* non-int routines typically used by the machine dependent module */
char	*printable();

#endif /* MACHINE_H */
