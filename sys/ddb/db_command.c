/*
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
 *
 * $FreeBSD$
 */

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Command dispatcher.
 */
#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/cons.h>

#include <ddb/ddb.h>
#include <ddb/db_command.h>
#include <ddb/db_lex.h>
#include <ddb/db_output.h>

#include <machine/md_var.h>
#include <machine/setjmp.h>

/*
 * Exported global variables
 */
boolean_t	db_cmd_loop_done;
db_addr_t	db_dot;
jmp_buf		db_jmpbuf;
db_addr_t	db_last_addr;
db_addr_t	db_prev;
db_addr_t	db_next;

SET_DECLARE(db_cmd_set, struct command);
SET_DECLARE(db_show_cmd_set, struct command);

static db_cmdfcn_t	db_fncall;
static db_cmdfcn_t	db_gdb;
static db_cmdfcn_t	db_kill;
static db_cmdfcn_t	db_reset;

/* XXX this is actually forward-static. */
extern struct command	db_show_cmds[];

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

static void	db_cmd_list __P((struct command *table,
				 struct command **aux_tablep,
				 struct command **aux_tablep_end));
static int	db_cmd_search __P((char *name, struct command *table,
				   struct command **aux_tablep,
				   struct command **aux_tablep_end,
				   struct command **cmdp));
static void	db_command __P((struct command **last_cmdp,
				struct command *cmd_table,
				struct command **aux_cmd_tablep,
				struct command **aux_cmd_tablep_end));

/*
 * Search for command prefix.
 */
static int
db_cmd_search(name, table, aux_tablep, aux_tablep_end, cmdp)
	char *		name;
	struct command	*table;
	struct command	**aux_tablep;
	struct command	**aux_tablep_end;
	struct command	**cmdp;	/* out */
{
	struct command	*cmd;
	struct command	**aux_cmdp;
	int		result = CMD_NONE;

	for (cmd = table; cmd->name != 0; cmd++) {
	    register char *lp;
	    register char *rp;
	    register int  c;

	    lp = name;
	    rp = cmd->name;
	    while ((c = *lp) == *rp) {
		if (c == 0) {
		    /* complete match */
		    *cmdp = cmd;
		    return (CMD_UNIQUE);
		}
		lp++;
		rp++;
	    }
	    if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (result == CMD_FOUND) {
		    result = CMD_AMBIGUOUS;
		    /* but keep looking for a full match -
		       this lets us match single letters */
		}
		else {
		    *cmdp = cmd;
		    result = CMD_FOUND;
		}
	    }
	}
	if (result == CMD_NONE && aux_tablep != 0)
	    /* XXX repeat too much code. */
	    for (aux_cmdp = aux_tablep; aux_cmdp < aux_tablep_end; aux_cmdp++) {
		register char *lp;
		register char *rp;
		register int  c;

		lp = name;
		rp = (*aux_cmdp)->name;
		while ((c = *lp) == *rp) {
		    if (c == 0) {
			/* complete match */
			*cmdp = *aux_cmdp;
			return (CMD_UNIQUE);
		    }
		    lp++;
		    rp++;
		}
		if (c == 0) {
		    /* end of name, not end of command -
		       partial match */
		    if (result == CMD_FOUND) {
			result = CMD_AMBIGUOUS;
			/* but keep looking for a full match -
			   this lets us match single letters */
		    }
		    else {
			*cmdp = *aux_cmdp;
			result = CMD_FOUND;
		    }
		}
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
db_cmd_list(table, aux_tablep, aux_tablep_end)
	struct command *table;
	struct command **aux_tablep;
	struct command **aux_tablep_end;
{
	register struct command *cmd;
	register struct command **aux_cmdp;

	for (cmd = table; cmd->name != 0; cmd++) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	}
	if (aux_tablep == 0)
	    return;
	for (aux_cmdp = aux_tablep; aux_cmdp < aux_tablep_end; aux_cmdp++) {
	    db_printf("%-12s", (*aux_cmdp)->name);
	    db_end_line();
	}
}

static void
db_command(last_cmdp, cmd_table, aux_cmd_tablep, aux_cmd_tablep_end)
	struct command	**last_cmdp;	/* IN_OUT */
	struct command	*cmd_table;
	struct command	**aux_cmd_tablep;
	struct command	**aux_cmd_tablep_end;
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
				       aux_cmd_tablep,
				       aux_cmd_tablep_end,
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
			db_cmd_list(cmd_table, aux_cmd_tablep, aux_cmd_tablep_end);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != 0) {
		    /* XXX usually no more aux's. */
		    aux_cmd_tablep = 0;
		    if (cmd_table == db_show_cmds)
			aux_cmd_tablep = SET_BEGIN(db_show_cmd_set);
			aux_cmd_tablep_end = SET_LIMIT(db_show_cmd_set);

		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table, aux_cmd_tablep, aux_cmd_tablep_end);
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
	    (*cmd->fcn)(addr, have_addr, count, modif);

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
 * 'show' commands
 */

static struct command db_show_all_cmds[] = {
#if 0
	{ "threads",	db_show_all_threads,	0,	0 },
#endif
	{ "procs",	db_ps,			0,	0 },
	{ (char *)0 }
};

static struct command db_show_cmds[] = {
	{ "all",	0,			0,	db_show_all_cmds },
	{ "registers",	db_show_regs,		0,	0 },
	{ "breaks",	db_listbreak_cmd, 	0,	0 },
#if 0
	{ "thread",	db_show_one_thread,	0,	0 },
#endif
#if 0
	{ "port",	ipc_port_print,		0,	0 },
#endif
	{ (char *)0, }
};

static struct command db_command_table[] = {
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
	{ "trace",	db_stack_trace_cmd,	0,	0 },
	{ "call",	db_fncall,		CS_OWN,	0 },
	{ "show",	0,			0,	db_show_cmds },
	{ "ps",		db_ps,			0,	0 },
	{ "gdb",	db_gdb,			0,	0 },
	{ "reset",	db_reset,		0,	0 },
	{ "kill",	db_kill,		CS_OWN,	0 },
	{ (char *)0, }
};

static struct command	*db_last_command = 0;

#if 0
void
db_help_cmd()
{
	struct command *cmd = db_command_table;

	while (cmd->name != 0) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	    cmd++;
	}
}
#endif

/*
 * At least one non-optional command must be implemented using
 * DB_COMMAND() so that db_cmd_set gets created.  Here is one.
 */
DB_COMMAND(panic, db_panic)
{
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

	    (void) setjmp(db_jmpbuf);
	    if (db_print_position() != 0)
		db_printf("\n");

	    db_printf("db> ");
	    (void) db_read_line();

	    db_command(&db_last_command, db_command_table,
		       SET_BEGIN(db_cmd_set), SET_LIMIT(db_cmd_set));
	}
}

void
db_error(s)
	char *s;
{
	if (s)
	    db_printf("%s", s);
	db_flush_lex();
	longjmp(db_jmpbuf, 1);
}


/*
 * Call random function:
 * !expr(arg,arg,arg)
 */
static void
db_fncall(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	db_expr_t	fn_addr;
#define	MAXARGS		11	/* XXX only 10 are passed */
	db_expr_t	args[MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	typedef db_expr_t fcn_10args_t __P((db_expr_t, db_expr_t, db_expr_t,
					    db_expr_t, db_expr_t, db_expr_t,
					    db_expr_t, db_expr_t, db_expr_t,
					    db_expr_t));
	fcn_10args_t	*func;
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function\n");
	    db_flush_lex();
	    return;
	}
	func = (fcn_10args_t *)fn_addr;	/* XXX */

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == MAXARGS) {
			db_printf("Too many arguments\n");
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

	while (nargs < MAXARGS) {
	    args[nargs++] = 0;
	}

	retval = (*func)(args[0], args[1], args[2], args[3], args[4],
			 args[5], args[6], args[7], args[8], args[9] );
	db_printf("%#lr\n", (long)retval);
}

/* Enter GDB remote protocol debugger on the next trap. */

dev_t	   gdbdev = NODEV;
cn_getc_t *gdb_getc;
cn_putc_t *gdb_putc;

static void
db_gdb (dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{

	if (gdbdev == NODEV) {
		db_printf("No gdb port enabled. Set flag 0x80 on desired port\n");
		db_printf("in your configuration file (currently sio only).\n");
		return;
	}
	boothowto ^= RB_GDB;

	db_printf("Next trap will enter %s\n",
		   boothowto & RB_GDB ? "GDB remote protocol mode"
				      : "DDB debugger");
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
	if (sig < 0 || sig > _SIG_MAXSIG)
		DB_ERROR(("Signal number out of range\n"));

	/*
	 * Find the process in question.  allproc_lock is not needed
	 * since we're in DDB.
	 */
	/* sx_slock(&allproc_lock); */
	LIST_FOREACH(p, &allproc, p_list)
	    if (p->p_pid == pid)
		    break;
	/* sx_sunlock(&allproc_lock); */
	if (p == NULL)
		DB_ERROR(("Can't find process with pid %d\n", pid));

	/* If it's already locked, bail; otherwise, do the deed. */
	if (PROC_TRYLOCK(p) == 0)
		DB_ERROR(("Can't lock process with pid %d\n", pid));
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
