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
 *	$Id: ddb.h,v 1.1 1993/11/25 01:30:16 wollman Exp $
 */

/*
 * Necessary declarations for the `ddb' kernel debugger.
 */

#ifndef __h_ddb_ddb
#define __h_ddb_ddb 1

#include "machine/db_machdep.h"		/* type definitions */

/*
 * Global variables...
 */
extern char *esym;
extern unsigned int db_maxoff;
extern int db_inst_count;
extern int db_load_count;
extern int db_store_count;
extern int db_radix;
extern int db_max_width;
extern int db_tab_stop_width;

/*
 * Functions...
 */
extern void 
db_read_bytes(vm_offset_t addr, register int size, register char *data); 
				/* machine-dependent */

extern void
db_write_bytes(vm_offset_t addr, register int size, register char *data);
				/* machine-dependent */

struct vm_map;			/* forward declaration */

extern boolean_t db_map_equal(struct vm_map *, struct vm_map *);
extern boolean_t db_map_current(struct vm_map *);
extern struct vm_map *db_map_addr(vm_offset_t);

#define db_strcpy strcpy
extern int db_expression (db_expr_t *valuep);

typedef void db_cmd_fcn(db_expr_t, int, db_expr_t, char *);

extern db_cmd_fcn db_listbreak_cmd, db_listwatch_cmd, db_show_regs;
extern db_cmd_fcn db_print_cmd, db_examine_cmd, db_set_cmd, db_search_cmd;
extern db_cmd_fcn db_write_cmd, db_delete_cmd, db_breakpoint_cmd;
extern db_cmd_fcn db_deletewatch_cmd, db_watchpoint_cmd;
extern db_cmd_fcn db_single_step_cmd, db_trace_until_call_cmd;
extern db_cmd_fcn db_trace_until_matching_cmd, db_continue_cmd;
extern db_cmd_fcn db_stack_trace_cmd;

extern db_addr_t db_disasm(db_addr_t loc, boolean_t altfmt);
			/* instruction disassembler */

extern int db_value_of_name (char *name, db_expr_t *valuep);
extern int db_get_variable (db_expr_t *valuep);
extern void db_putchar (int c);
extern void db_error (char *s);
extern int db_readline (char *lstart, int lsize);
extern void db_printf (const char *fmt, ...);
extern void db_check_interrupt(void);
extern void db_print_loc_and_inst (db_addr_t loc);

extern void db_clear_watchpoints (void);
extern void db_set_watchpoints (void);

extern void db_restart_at_pc(boolean_t watchpt);
extern boolean_t db_stop_at_pc(boolean_t *is_breakpoint);

extern void db_skip_to_eol (void);
extern void db_single_step (db_regs_t *regs);

extern void db_trap (int type, int code);

extern void kdbprinttrap(int, int);

#endif /* __h_ddb_ddb */
