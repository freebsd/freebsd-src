/*-
 * Copyright (c) 1993, Garrett A. Wollman.
 * Copyright (c) 1993, University of Vermont and State Agricultural College.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $FreeBSD: src/sys/ddb/ddb.h,v 1.43.2.4.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Necessary declarations for the `ddb' kernel debugger.
 */

#ifndef _DDB_DDB_H_
#define	_DDB_DDB_H_

#ifdef SYSCTL_DECL
SYSCTL_DECL(_debug_ddb);
#endif

#include <machine/db_machdep.h>		/* type definitions */

#ifndef DB_MAXARGS
#define	DB_MAXARGS	10
#endif

#ifndef DB_MAXLINE
#define	DB_MAXLINE	120
#endif

#ifndef DB_MAXSCRIPTS
#define	DB_MAXSCRIPTS	8
#endif

#ifndef DB_MAXSCRIPTNAME
#define	DB_MAXSCRIPTNAME	32
#endif

#ifndef DB_MAXSCRIPTLEN
#define	DB_MAXSCRIPTLEN	128
#endif

#ifndef DB_MAXSCRIPTRECURSION
#define	DB_MAXSCRIPTRECURSION	3
#endif

#ifndef DB_CALL
#define	DB_CALL	db_fncall_generic
#else
int	DB_CALL(db_expr_t, db_expr_t *, int, db_expr_t[]);
#endif

typedef void db_cmdfcn_t(db_expr_t addr, boolean_t have_addr, db_expr_t count,
	    char *modif);

#define DB_COMMAND(cmd_name, func_name) \
	DB_FUNC(cmd_name, func_name, db_cmd_set, 0, NULL)
#define DB_SHOW_COMMAND(cmd_name, func_name) \
	DB_FUNC(cmd_name, func_name, db_show_cmd_set, 0, NULL)
#define DB_SHOW_ALL_COMMAND(cmd_name, func_name) \
	DB_FUNC(cmd_name, func_name, db_show_all_cmd_set, 0, NULL)

#define	DB_SET(cmd_name, func_name, set, flag, more)		\
static const struct command __CONCAT(cmd_name,_cmd) = {		\
	__STRING(cmd_name),					\
	func_name,						\
	flag,							\
	more							\
};								\
TEXT_SET(set, __CONCAT(cmd_name,_cmd))

#define DB_FUNC(cmd_name, func_name, set, flag, more)		\
static db_cmdfcn_t	func_name;				\
								\
DB_SET(cmd_name, func_name, set, flag, more);			\
								\
static void							\
func_name(addr, have_addr, count, modif)			\
	db_expr_t addr;						\
	boolean_t have_addr;					\
	db_expr_t count;					\
	char *modif;

extern db_expr_t db_maxoff;
extern int db_indent;
extern int db_inst_count;
extern int db_load_count;
extern int db_store_count;
extern volatile int db_pager_quit;
extern db_expr_t db_radix;
extern db_expr_t db_max_width;
extern db_expr_t db_tab_stop_width;
extern db_expr_t db_lines_per_page;

struct thread;
struct vm_map;

void		db_check_interrupt(void);
void		db_clear_watchpoints(void);
db_addr_t	db_disasm(db_addr_t loc, boolean_t altfmt);
				/* instruction disassembler */
void		db_error(const char *s);
int		db_expression(db_expr_t *valuep);
int		db_get_variable(db_expr_t *valuep);
void		db_iprintf(const char *,...) __printflike(1, 2);
struct proc	*db_lookup_proc(db_expr_t addr);
struct thread	*db_lookup_thread(db_expr_t addr, boolean_t check_pid);
struct vm_map	*db_map_addr(vm_offset_t);
boolean_t	db_map_current(struct vm_map *);
boolean_t	db_map_equal(struct vm_map *, struct vm_map *);
int		db_md_set_watchpoint(db_expr_t addr, db_expr_t size);
int		db_md_clr_watchpoint(db_expr_t addr, db_expr_t size);
void		db_md_list_watchpoints(void);
void		db_print_loc_and_inst(db_addr_t loc);
void		db_print_thread(void);
void		db_printf(const char *fmt, ...) __printflike(1, 2);
int		db_read_bytes(vm_offset_t addr, size_t size, char *data);
				/* machine-dependent */
int		db_readline(char *lstart, int lsize);
void		db_restart_at_pc(boolean_t watchpt);
int		db_set_variable(db_expr_t value);
void		db_set_watchpoints(void);
void		db_skip_to_eol(void);
boolean_t	db_stop_at_pc(boolean_t *is_breakpoint);
#define		db_strcpy	strcpy
void		db_trace_self(void);
int		db_trace_thread(struct thread *, int);
int		db_value_of_name(const char *name, db_expr_t *valuep);
int		db_write_bytes(vm_offset_t addr, size_t size, char *data);

db_cmdfcn_t	db_breakpoint_cmd;
db_cmdfcn_t	db_capture_cmd;
db_cmdfcn_t	db_continue_cmd;
db_cmdfcn_t	db_delete_cmd;
db_cmdfcn_t	db_deletehwatch_cmd;
db_cmdfcn_t	db_deletewatch_cmd;
db_cmdfcn_t	db_examine_cmd;
db_cmdfcn_t	db_hwatchpoint_cmd;
db_cmdfcn_t	db_listbreak_cmd;
db_cmdfcn_t	db_scripts_cmd;
db_cmdfcn_t	db_print_cmd;
db_cmdfcn_t	db_ps;
db_cmdfcn_t	db_run_cmd;
db_cmdfcn_t	db_script_cmd;
db_cmdfcn_t	db_search_cmd;
db_cmdfcn_t	db_set_cmd;
db_cmdfcn_t	db_set_thread;
db_cmdfcn_t	db_show_regs;
db_cmdfcn_t	db_show_threads;
db_cmdfcn_t	db_single_step_cmd;
db_cmdfcn_t	db_textdump_cmd;
db_cmdfcn_t	db_trace_until_call_cmd;
db_cmdfcn_t	db_trace_until_matching_cmd;
db_cmdfcn_t	db_unscript_cmd;
db_cmdfcn_t	db_watchpoint_cmd;
db_cmdfcn_t	db_write_cmd;

/*
 * Command table.
 */
struct command;

struct command_table {
	struct command *table;
	struct command **aux_tablep;
	struct command **aux_tablep_end;
};

struct command {
	char *	name;		/* command name */
	db_cmdfcn_t *fcn;	/* function to call */
	int	flag;		/* extra info: */
#define	CS_OWN		0x1	/* non-standard syntax */
#define	CS_MORE		0x2	/* standard syntax, but may have other words
				 * at end */
#define	CS_SET_DOT	0x100	/* set dot after command */
	struct command_table *more; /* another level of command */
};

/*
 * Interface between DDB and the DDB output capture facility.
 */
struct dumperinfo;
void	db_capture_dump(struct dumperinfo *di);
void	db_capture_enterpager(void);
void	db_capture_exitpager(void);
void	db_capture_write(char *buffer, u_int buflen);
void	db_capture_writech(char ch);

/*
 * Interface between DDB  and the script facility.
 */
void	db_script_kdbenter(const char *eventname);	/* KDB enter event. */

/*
 * Interface between DDB and the textdump facility.
 *
 * Text dump blocks are of a fixed size; textdump_block_buffer is a
 * statically allocated buffer that code interacting with textdumps can use
 * to prepare and hold a pending block in when calling writenextblock().
 */
#define	TEXTDUMP_BLOCKSIZE	512
extern char	textdump_block_buffer[TEXTDUMP_BLOCKSIZE];

void	textdump_mkustar(char *block_buffer, const char *filename,
	    u_int size);
void	textdump_restoreoff(off_t offset);
void	textdump_saveoff(off_t *offsetp);
int	textdump_writenextblock(struct dumperinfo *di, char *buffer);

/*
 * Interface between the kernel and textdumps.
 */
extern int	textdump_pending;	/* Call textdump_dumpsys() instead. */
void	textdump_dumpsys(struct dumperinfo *di);

#endif /* !_DDB_DDB_H_ */
