/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/queue.h>			/* LIST_* */
#include <sys/kernel.h>			/* SYSINIT */

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

/*
 * Extern variables to set the address and size of the symtab and strtab.
 * Most users should use db_fetch_symtab in order to set them from the
 * boot loader provided values.
 */
extern vm_offset_t ksymtab, kstrtab, ksymtab_size, ksymtab_relbase;

/* Command tables contain a list of commands. */
struct db_command;
LIST_HEAD(db_command_table, db_command);

#define	_DB_TABLE_NAME(table)	db_##table##_table

#define	DB_DEFINE_TABLE(parent, name, table)				\
	struct db_command_table _DB_TABLE_NAME(table) =			\
	    LIST_HEAD_INITIALIZER(_DB_TABLE_NAME(table));		\
	_DB_SET(parent, name, NULL, 0, &_DB_TABLE_NAME(table))

#define	DB_DECLARE_TABLE(table)						\
	extern struct db_command_table _DB_TABLE_NAME(table)

/*
 * Builtin command tables:
 * - cmd: Top-level command table; a list of these is displayed
 *   by typing 'help' at the debugger prompt.
 * - show: Sub-commands of 'show'
 * - show_all: Sub-commands of 'show all'
 * - show_active: Sub-commands of 'show active'
 */
DB_DECLARE_TABLE(cmd);
DB_DECLARE_TABLE(show);
DB_DECLARE_TABLE(show_all);
DB_DECLARE_TABLE(show_active);

/*
 * Type signature for a function implementing a ddb command.
 */
typedef void db_cmdfcn_t(db_expr_t addr, bool have_addr, db_expr_t count,
	    char *modif);

/*
 * Command table entry.
 */
struct db_command {
	char *name;		/* command name */
	db_cmdfcn_t *fcn;	/* function to call */
	int flag;
#define	CS_OWN		0x1	/* non-standard syntax */
#define	CS_MORE		0x2	/* standard syntax, but may have other words
				 * at end */
#define	CS_SET_DOT	0x100	/* set dot after command */
#define	DB_CMD_MEMSAFE	0x1000	/* Command does not allow reads or writes to
				 * arbitrary memory. */
#define	DB_MAC1		0x10000	/* For MAC policy use */
#define	DB_MAC2		0x20000
	struct db_command_table *more; /* another level of command */
	LIST_ENTRY(db_command) next; /* next entry in the command table */
	void *mac_priv;		/* For MAC policy use */
};

/*
 * Arrange for the specified ddb command to be defined and
 * bound to the specified function.  Commands can be defined
 * in modules in which case they will be available only when
 * the module is loaded.
 */
#define	_DB_SET(_table, _name, _func, _flag, _more)		\
static struct db_command db_##_table##_##_name##_cmd = {	\
	.name	= __STRING(_name),				\
	.fcn	= _func,					\
	.flag	= _flag,					\
	.more	= _more						\
};								\
								\
static void							\
db##_table##_##_name##_add(void *arg __unused)			\
{								\
	db_command_register(&_DB_TABLE_NAME(_table),		\
	    &db_##_table##_##_name##_cmd);			\
}								\
SYSINIT(db_##_table##_##_name, SI_SUB_KLD, SI_ORDER_ANY,	\
    db##_table##_##_name##_add, NULL);				\
								\
static void							\
db##_table##_##_name##_del(void *arg __unused)			\
{								\
	db_command_unregister(&_DB_TABLE_NAME(_table),		\
	    &db_##_table##_##_name##_cmd);			\
}								\
SYSUNINIT(db_##_table##_##_name, SI_SUB_KLD, SI_ORDER_ANY,	\
    db##_table##_##_name##_del, NULL)

/*
 * Like _DB_SET but also create the function declaration which
 * must be followed immediately by the body; e.g.
 *   DB_TABLE_COMMAND_FLAGS(_cmd, panic, db_panic, 0)
 *   {
 *	...panic implementation...
 *   }
 *
 * This macro is mostly used to define commands placed in one of
 * the ddb command tables; see DB_COMMAND, etc. below.
 */
#define	DB_TABLE_COMMAND_FLAGS(_table, _name, _func, _flag)	\
static db_cmdfcn_t _func;					\
_DB_SET(_table, _name, _func, _flag, NULL);			\
static void							\
_func(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)

#define	DB_TABLE_COMMAND(_table, _name, _func)			\
	DB_TABLE_COMMAND_FLAGS(_table, _name, _func, 0)

/* Wrappers around _DB_SET used for aliases. */
#define	DB_TABLE_ALIAS_FLAGS(_table, _name, _func, _flag)	\
	_DB_SET(_table, _name, _func, _flag, NULL)
#define	DB_TABLE_ALIAS(_table, _name, _func)			\
	DB_TABLE_ALIAS_FLAGS(_table, _name, _func, 0)

#define	DB_COMMAND_FLAGS(cmd_name, func_name, flags) \
	DB_TABLE_COMMAND_FLAGS(cmd, cmd_name, func_name, flags)
#define	DB_COMMAND(cmd_name, func_name) \
	DB_COMMAND_FLAGS(cmd_name, func_name, 0)
#define	DB_ALIAS_FLAGS(alias_name, func_name, flags) \
	DB_TABLE_ALIAS_FLAGS(cmd, alias_name, func_name, flags)
#define	DB_ALIAS(alias_name, func_name) \
	DB_ALIAS_FLAGS(alias_name, func_name, 0)
#define	DB_SHOW_COMMAND_FLAGS(cmd_name, func_name, flags) \
	DB_TABLE_COMMAND_FLAGS(show, cmd_name, func_name, flags)
#define	DB_SHOW_COMMAND(cmd_name, func_name) \
	DB_SHOW_COMMAND_FLAGS(cmd_name, func_name, 0)
#define	DB_SHOW_ALIAS_FLAGS(alias_name, func_name, flags) \
	DB_TABLE_ALIAS_FLAGS(show, alias_name, func_name, flags)
#define	DB_SHOW_ALIAS(alias_name, func_name) \
	DB_SHOW_ALIAS_FLAGS(alias_name, func_name, 0)
#define	DB_SHOW_ALL_COMMAND(cmd_name, func_name)			\
	DB_TABLE_COMMAND_FLAGS(show_all, cmd_name, func_name, DB_CMD_MEMSAFE)
#define	DB_SHOW_ALL_ALIAS(alias_name, func_name)			\
	DB_TABLE_ALIAS_FLAGS(show_all, alias_name, func_name, DB_CMD_MEMSAFE)

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
db_addr_t	db_disasm(db_addr_t loc, bool altfmt);
				/* instruction disassembler */
void		db_error(const char *s) __dead2;
int		db_expression(db_expr_t *valuep);
int		db_getc(void);
int		db_get_variable(db_expr_t *valuep);
void		db_iprintf(const char *,...) __printflike(1, 2);
struct proc	*db_lookup_proc(db_expr_t addr);
struct thread	*db_lookup_thread(db_expr_t addr, bool check_pid);
struct vm_map	*db_map_addr(vm_offset_t);
bool		db_map_current(struct vm_map *);
bool		db_map_equal(struct vm_map *, struct vm_map *);
void		db_md_list_watchpoints(void);
void		db_print_loc_and_inst(db_addr_t loc);
void		db_print_thread(void);
int		db_printf(const char *fmt, ...) __printflike(1, 2);
int		db_read_bytes(vm_offset_t addr, size_t size, char *data);
				/* machine-dependent */
int		db_readline(char *lstart, int lsize);
void		db_restart_at_pc(bool watchpt);
int		db_set_variable(db_expr_t value);
void		db_set_watchpoints(void);
void		db_skip_to_eol(void);
bool		db_stop_at_pc(int type, int code, bool *is_breakpoint,
		    bool *is_watchpoint);
#define		db_strcpy	strcpy
void		db_trace_self(void);
int		db_trace_thread(struct thread *, int);
bool		db_value_of_name(const char *name, db_expr_t *valuep);
bool		db_value_of_name_pcpu(const char *name, db_expr_t *valuep);
bool		db_value_of_name_vnet(const char *name, db_expr_t *valuep);
int		db_write_bytes(vm_offset_t addr, size_t size, char *data);
void		db_command_register(struct db_command_table *,
		    struct db_command *);
void		db_command_unregister(struct db_command_table *,
		    struct db_command *);
int		db_fetch_ksymtab(vm_offset_t ksym_start, vm_offset_t ksym_end,
		    vm_offset_t relbase);

db_cmdfcn_t	db_breakpoint_cmd;
db_cmdfcn_t	db_capture_cmd;
db_cmdfcn_t	db_continue_cmd;
db_cmdfcn_t	db_delete_cmd;
db_cmdfcn_t	db_deletehwatch_cmd;
db_cmdfcn_t	db_deletewatch_cmd;
db_cmdfcn_t	db_examine_cmd;
db_cmdfcn_t	db_findstack_cmd;
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
db_cmdfcn_t	db_pprint_cmd;

#ifdef HAS_HW_BREAKPOINT
void		db_md_list_breakpoints(void);

db_cmdfcn_t	db_deletehbreak_cmd;
db_cmdfcn_t	db_hbreakpoint_cmd;
#endif

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
