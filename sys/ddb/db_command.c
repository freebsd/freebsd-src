/*-
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Command dispatcher.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/watchdog.h>

#include <ddb/ddb.h>
#include <ddb/db_command.h>
#include <ddb/db_lex.h>
#include <ddb/db_output.h>

#include <machine/cpu.h>
#include <machine/setjmp.h>

/*
 * Exported global variables
 */
boolean_t	db_cmd_loop_done;
db_addr_t	db_dot;
db_addr_t	db_last_addr;
db_addr_t	db_prev;
db_addr_t	db_next;

SET_DECLARE(db_cmd_set, struct command);
SET_DECLARE(db_show_cmd_set, struct command);

static db_cmdfcn_t	db_fncall;
static db_cmdfcn_t	db_gdb;
static db_cmdfcn_t	db_halt;
static db_cmdfcn_t	db_kill;
static db_cmdfcn_t	db_reset;
static db_cmdfcn_t	db_stack_trace;
static db_cmdfcn_t	db_stack_trace_all;
static db_cmdfcn_t	db_watchdog;

/*
 * 'show' commands
 */

static struct command db_show_all_cmds[] = {
	{ "procs",	db_ps,			0,	0 },
	{ (char *)0 }
};

static struct command_table db_show_all_table = {
	db_show_all_cmds
};

static struct command db_show_cmds[] = {
	{ "all",	0,			0,	&db_show_all_table },
	{ "registers",	db_show_regs,		0,	0 },
	{ "breaks",	db_listbreak_cmd, 	0,	0 },
	{ "threads",	db_show_threads,	0,	0 },
	{ (char *)0, }
};

static struct command_table db_show_table = {
	db_show_cmds,
	SET_BEGIN(db_show_cmd_set),
	SET_LIMIT(db_show_cmd_set)
};
	
static struct command db_commands[] = {
	{ "print",	db_print_cmd,		0,	0 },
	{ "p",		db_print_cmd,		0,	0 },
	{ "examine",	db_examine_cmd,		CS_SET_DOT, 0 },
	{ "x",		db_examine_cmd,		CS_SET_DOT, 0 },
	{ "search",	db_search_cmd,		CS_OWN|CS_SET_DOT, 0 },
	{ "set",	db_set_cmd,		CS_OWN,	0 },
	{ "write",	db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "w",		db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "delete",	db_delete_cmd,		0,	0 },
	{ "d",		db_delete_cmd,		0,	0 },
	{ "break",	db_breakpoint_cmd,	0,	0 },
	{ "b",		db_breakpoint_cmd,	0,	0 },
	{ "dwatch",	db_deletewatch_cmd,	0,	0 },
	{ "watch",	db_watchpoint_cmd,	CS_MORE,0 },
	{ "dhwatch",	db_deletehwatch_cmd,	0,      0 },
	{ "hwatch",	db_hwatchpoint_cmd,	0,      0 },
	{ "step",	db_single_step_cmd,	0,	0 },
	{ "s",		db_single_step_cmd,	0,	0 },
	{ "continue",	db_continue_cmd,	0,	0 },
	{ "c",		db_continue_cmd,	0,	0 },
	{ "until",	db_trace_until_call_cmd,0,	0 },
	{ "next",	db_trace_until_matching_cmd,0,	0 },
	{ "match",	db_trace_until_matching_cmd,0,	0 },
	{ "trace",	db_stack_trace,		CS_OWN,	0 },
	{ "t",		db_stack_trace,		CS_OWN,	0 },
	{ "alltrace",	db_stack_trace_all,	0,	0 },
	{ "where",	db_stack_trace,		CS_OWN,	0 },
	{ "bt",		db_stack_trace,		CS_OWN,	0 },
	{ "call",	db_fncall,		CS_OWN,	0 },
	{ "show",	0,			0,	&db_show_table },
	{ "ps",		db_ps,			0,	0 },
	{ "gdb",	db_gdb,			0,	0 },
	{ "halt",	db_halt,		0,	0 },
	{ "reboot",	db_reset,		0,	0 },
	{ "reset",	db_reset,		0,	0 },
	{ "kill",	db_kill,		CS_OWN,	0 },
	{ "watchdog",	db_watchdog,		0,	0 },
	{ "thread",	db_set_thread,		CS_OWN,	0 },
	{ (char *)0, }
};

static struct command_table db_command_table = {
	db_commands,
	SET_BEGIN(db_cmd_set),
	SET_LIMIT(db_cmd_set)
};

static struct command	*db_last_command = 0;

/*
 * if 'ed' style: 'dot' is set at start of last item printed,
 * and '+' points to next line.
 * Otherwise: 'dot' points to next item, '..' points to last.
 */
static boolean_t	db_ed_style = TRUE;

/*
 * Utility routine - discard tokens through end-of-line.
 */
void
db_skip_to_eol()
{
	int	t;
	do {
	    t = db_read_token();
	} while (t != tEOL);
}

/*
 * Results of command search.
 */
#define	CMD_UNIQUE	0
#define	CMD_FOUND	1
#define	CMD_NONE	2
#define	CMD_AMBIGUOUS	3
#define	CMD_HELP	4

static void	db_cmd_match(char *name, struct command *cmd,
		    struct command **cmdp, int *resultp);
static void	db_cmd_list(struct command_table *table);
static int	db_cmd_search(char *name, struct command_table *table,
		    struct command **cmdp);
static void	db_command(struct command **last_cmdp,
		    struct command_table *cmd_table);

/*
 * Helper function to match a single command.
 */
static void
db_cmd_match(name, cmd, cmdp, resultp)
	char *		name;
	struct command	*cmd;
	struct command	**cmdp;	/* out */
	int *		resultp;
{
	char *lp, *rp;
	int c;

	lp = name;
	rp = cmd->name;
	while ((c = *lp) == *rp) {
		if (c == 0) {
			/* complete match */
			*cmdp = cmd;
			*resultp = CMD_UNIQUE;
			return;
		}
		lp++;
		rp++;
	}
	if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (*resultp == CMD_FOUND) {
			*resultp = CMD_AMBIGUOUS;
			/* but keep looking for a full match -
			   this lets us match single letters */
		} else {
			*cmdp = cmd;
			*resultp = CMD_FOUND;
		}
	}
}

/*
 * Search for command prefix.
 */
static int
db_cmd_search(name, table, cmdp)
	char *		name;
	struct command_table *table;
	struct command	**cmdp;	/* out */
{
	struct command	*cmd;
	struct command	**aux_cmdp;
	int		result = CMD_NONE;

	for (cmd = table->table; cmd->name != 0; cmd++) {
		db_cmd_match(name, cmd, cmdp, &result);
		if (result == CMD_UNIQUE)
			return (CMD_UNIQUE);
	}
	if (table->aux_tablep != NULL)
		for (aux_cmdp = table->aux_tablep;
		     aux_cmdp < table->aux_tablep_end;
		     aux_cmdp++) {
			db_cmd_match(name, *aux_cmdp, cmdp, &result);
			if (result == CMD_UNIQUE)
				return (CMD_UNIQUE);
		}
	if (result == CMD_NONE) {
		/* check for 'help' */
		if (name[0] == 'h' && name[1] == 'e'
		    && name[2] == 'l' && name[3] == 'p')
			result = CMD_HELP;
	}
	return (result);
}

static void
db_cmd_list(table)
	struct command_table *table;
{
	register struct command *cmd;
	register struct command **aux_cmdp;

	for (cmd = table->table; cmd->name != 0; cmd++) {
	    db_printf("%-12s", cmd->name);
	    db_end_line(12);
	}
	if (table->aux_tablep == NULL)
	    return;
	for (aux_cmdp = table->aux_tablep; aux_cmdp < table->aux_tablep_end;
	     aux_cmdp++) {
	    db_printf("%-12s", (*aux_cmdp)->name);
	    db_end_line(12);
	}
}

static void
db_command(last_cmdp, cmd_table)
	struct command	**last_cmdp;	/* IN_OUT */
	struct command_table *cmd_table;
{
	struct command	*cmd;
	int		t;
	char		modif[TOK_STRING_SIZE];
	db_expr_t	addr, count;
	boolean_t	have_addr = FALSE;
	int		result;

	t = db_read_token();
	if (t == tEOL) {
	    /* empty line repeats last command, at 'next' */
	    cmd = *last_cmdp;
	    addr = (db_expr_t)db_next;
	    have_addr = FALSE;
	    count = 1;
	    modif[0] = '\0';
	}
	else if (t == tEXCL) {
	    db_fncall((db_expr_t)0, (boolean_t)0, (db_expr_t)0, (char *)0);
	    return;
	}
	else if (t != tIDENT) {
	    db_printf("?\n");
	    db_flush_lex();
	    return;
	}
	else {
	    /*
	     * Search for command
	     */
	    while (cmd_table) {
		result = db_cmd_search(db_tok_string,
				       cmd_table,
				       &cmd);
		switch (result) {
		    case CMD_NONE:
			db_printf("No such command\n");
			db_flush_lex();
			return;
		    case CMD_AMBIGUOUS:
			db_printf("Ambiguous\n");
			db_flush_lex();
			return;
		    case CMD_HELP:
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != NULL) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    }
		}
	    }

	    if ((cmd->flag & CS_OWN) == 0) {
		/*
		 * Standard syntax:
		 * command [/modifier] [addr] [,count]
		 */
		t = db_read_token();
		if (t == tSLASH) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_printf("Bad modifier\n");
			db_flush_lex();
			return;
		    }
		    db_strcpy(modif, db_tok_string);
		}
		else {
		    db_unread_token(t);
		    modif[0] = '\0';
		}

		if (db_expression(&addr)) {
		    db_dot = (db_addr_t) addr;
		    db_last_addr = db_dot;
		    have_addr = TRUE;
		}
		else {
		    addr = (db_expr_t) db_dot;
		    have_addr = FALSE;
		}
		t = db_read_token();
		if (t == tCOMMA) {
		    if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		    }
		}
		else {
		    db_unread_token(t);
		    count = -1;
		}
		if ((cmd->flag & CS_MORE) == 0) {
		    db_skip_to_eol();
		}
	    }
	}
	*last_cmdp = cmd;
	if (cmd != 0) {
	    /*
	     * Execute the command.
	     */
	    db_enable_pager();
	    (*cmd->fcn)(addr, have_addr, count, modif);
	    db_disable_pager();

	    if (cmd->flag & CS_SET_DOT) {
		/*
		 * If command changes dot, set dot to
		 * previous address displayed (if 'ed' style).
		 */
		if (db_ed_style) {
		    db_dot = db_prev;
		}
		else {
		    db_dot = db_next;
		}
	    }
	    else {
		/*
		 * If command does not change dot,
		 * set 'next' location to be the same.
		 */
		db_next = db_dot;
	    }
	}
}

/*
 * At least one non-optional command must be implemented using
 * DB_COMMAND() so that db_cmd_set gets created.  Here is one.
 */
DB_COMMAND(panic, db_panic)
{
	db_disable_pager();
	panic("from debugger");
}

void
db_command_loop()
{
	/*
	 * Initialize 'prev' and 'next' to dot.
	 */
	db_prev = db_dot;
	db_next = db_dot;

	db_cmd_loop_done = 0;
	while (!db_cmd_loop_done) {
	    if (db_print_position() != 0)
		db_printf("\n");

	    db_printf("db> ");
	    (void) db_read_line();

	    db_command(&db_last_command, &db_command_table);
	}
}

void
db_error(s)
	const char *s;
{
	if (s)
	    db_printf("%s", s);
	db_flush_lex();
	kdb_reenter();
}


/*
 * Call random function:
 * !expr(arg,arg,arg)
 */

/* The generic implementation supports a maximum of 10 arguments. */
typedef db_expr_t __db_f(db_expr_t, db_expr_t, db_expr_t, db_expr_t,
    db_expr_t, db_expr_t, db_expr_t, db_expr_t, db_expr_t, db_expr_t);

static __inline int
db_fncall_generic(db_expr_t addr, db_expr_t *rv, int nargs, db_expr_t args[])
{
	__db_f *f = (__db_f *)addr;

	if (nargs > 10) {
		db_printf("Too many arguments (max 10)\n");
		return (0);
	}
	*rv = (*f)(args[0], args[1], args[2], args[3], args[4], args[5],
	    args[6], args[7], args[8], args[9]);
	return (1);
}

static void
db_fncall(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	db_expr_t	fn_addr;
	db_expr_t	args[DB_MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function\n");
	    db_flush_lex();
	    return;
	}

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == DB_MAXARGS) {
			db_printf("Too many arguments (max %d)\n", DB_MAXARGS);
			db_flush_lex();
			return;
		    }
		    if (!db_expression(&args[nargs])) {
			db_printf("Argument missing\n");
			db_flush_lex();
			return;
		    }
		    nargs++;
		}
		db_unread_token(t);
	    }
	    if (db_read_token() != tRPAREN) {
		db_printf("?\n");
		db_flush_lex();
		return;
	    }
	}
	db_skip_to_eol();
	db_disable_pager();

	if (DB_CALL(fn_addr, &retval, nargs, args))
		db_printf("= %#lr\n", (long)retval);
}

static void
db_halt(db_expr_t dummy, boolean_t dummy2, db_expr_t dummy3, char *dummy4)
{

	cpu_halt();
}

static void
db_kill(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	db_expr_t old_radix, pid, sig;
	struct proc *p;

#define DB_ERROR(f)	do { db_printf f; db_flush_lex(); goto out; } while (0)

	/*
	 * PIDs and signal numbers are typically represented in base
	 * 10, so make that the default here.  It can, of course, be
	 * overridden by specifying a prefix.
	 */
	old_radix = db_radix;
	db_radix = 10;
	/* Retrieve arguments. */
	if (!db_expression(&sig))
		DB_ERROR(("Missing signal number\n"));
	if (!db_expression(&pid))
		DB_ERROR(("Missing process ID\n"));
	db_skip_to_eol();
	if (sig < 1 || sig > _SIG_MAXSIG)
		DB_ERROR(("Signal number out of range\n"));

	/*
	 * Find the process in question.  allproc_lock is not needed
	 * since we're in DDB.
	 */
	/* sx_slock(&allproc_lock); */
	FOREACH_PROC_IN_SYSTEM(p)
	    if (p->p_pid == pid)
		    break;
	/* sx_sunlock(&allproc_lock); */
	if (p == NULL)
		DB_ERROR(("Can't find process with pid %ld\n", (long) pid));

	/* If it's already locked, bail; otherwise, do the deed. */
	if (PROC_TRYLOCK(p) == 0)
		DB_ERROR(("Can't lock process with pid %ld\n", (long) pid));
	else {
		psignal(p, sig);
		PROC_UNLOCK(p);
	}

out:
	db_radix = old_radix;
#undef DB_ERROR
}

static void
db_reset(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{

	cpu_reset();
}

static void
db_watchdog(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	int i;

	/*
	 * XXX: It might make sense to be able to set the watchdog to a
	 * XXX: timeout here so that failure or hang as a result of subsequent
	 * XXX: ddb commands could be recovered by a reset.
	 */

	EVENTHANDLER_INVOKE(watchdog_list, 0, &i);
}

static void
db_gdb(db_expr_t dummy1, boolean_t dummy2, db_expr_t dummy3, char *dummy4)
{

	if (kdb_dbbe_select("gdb") != 0)
		db_printf("The remote GDB backend could not be selected.\n");
	else
		db_printf("Step to enter the remote GDB backend.\n");
}

static void
db_stack_trace(db_expr_t tid, boolean_t hastid, db_expr_t count, char *modif)
{
	struct thread *td;
	db_expr_t radix;
	pid_t pid;
	int t;

	/*
	 * We parse our own arguments. We don't like the default radix.
	 */
	radix = db_radix;
	db_radix = 10;
	hastid = db_expression(&tid);
	t = db_read_token();
	if (t == tCOMMA) {
		if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		}
	} else {
		db_unread_token(t);
		count = -1;
	}
	db_skip_to_eol();
	db_radix = radix;

	if (hastid) {
		td = kdb_thr_lookup((lwpid_t)tid);
		if (td == NULL)
			td = kdb_thr_from_pid((pid_t)tid);
		if (td == NULL) {
			db_printf("Thread %d not found\n", (int)tid);
			return;
		}
	} else
		td = kdb_thread;
	if (td->td_proc != NULL)
		pid = td->td_proc->p_pid;
	else
		pid = -1;
	db_printf("Tracing pid %d tid %ld td %p\n", pid, (long)td->td_tid, td);
	db_trace_thread(td, count);
}

static void
db_stack_trace_all(db_expr_t dummy, boolean_t dummy2, db_expr_t dummy3,
    char *dummy4)
{
	struct proc *p;
	struct thread *td;
	jmp_buf jb;
	void *prev_jb;

	FOREACH_PROC_IN_SYSTEM(p) {
		prev_jb = kdb_jmpbuf(jb);
		if (setjmp(jb) == 0) {
			FOREACH_THREAD_IN_PROC(p, td) {
				db_printf("\nTracing command %s pid %d tid %ld td %p\n",
					  p->p_comm, p->p_pid, (long)td->td_tid, td);
				db_trace_thread(td, -1);
				if (db_pager_quit) {
					kdb_jmpbuf(prev_jb);
					return;
				}
			}
		}
		kdb_jmpbuf(prev_jb);
	}
}
