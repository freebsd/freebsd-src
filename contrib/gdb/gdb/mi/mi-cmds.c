/* MI Command Set.
   Copyright 2000, 2001 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions (a Red Hat company).

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "top.h"
#include "mi-cmds.h"
#include "gdb_string.h"

extern void _initialize_mi_cmds (void);
struct mi_cmd;
static struct mi_cmd **lookup_table (const char *command);
static void build_table (struct mi_cmd *commands);


struct mi_cmd mi_cmds[] =
{
  {"break-after", "ignore %s", 0},
  {"break-catch", 0, 0},
  {"break-commands", 0, 0},
  {"break-condition", "cond %s", 0},
  {"break-delete", "delete breakpoint %s", 0},
  {"break-disable", "disable breakpoint %s", 0},
  {"break-enable", "enable breakpoint %s", 0},
  {"break-info", "info break %s", 0},
  {"break-insert", 0, 0, mi_cmd_break_insert},
  {"break-list", "info break", 0},
  {"break-watch", 0, 0, mi_cmd_break_watch},
  {"data-disassemble", 0, 0, mi_cmd_disassemble},
  {"data-evaluate-expression", 0, 0, mi_cmd_data_evaluate_expression},
  {"data-list-changed-registers", 0, 0, mi_cmd_data_list_changed_registers},
  {"data-list-register-names", 0, 0, mi_cmd_data_list_register_names},
  {"data-list-register-values", 0, 0, mi_cmd_data_list_register_values},
  {"data-read-memory", 0, 0, mi_cmd_data_read_memory},
  {"data-write-memory", 0, 0, mi_cmd_data_write_memory},
  {"data-write-register-values", 0, 0, mi_cmd_data_write_register_values},
  {"display-delete", 0, 0},
  {"display-disable", 0, 0},
  {"display-enable", 0, 0},
  {"display-insert", 0, 0},
  {"display-list", 0, 0},
  {"environment-cd", "cd %s", 0},
  {"environment-directory", "dir %s", 0},
  {"environment-path", "path %s", 0},
  {"environment-pwd", "pwd", 0},
  {"exec-abort", 0, 0},
  {"exec-arguments", "set args %s", 0},
  {"exec-continue", 0, mi_cmd_exec_continue},
  {"exec-finish", 0, mi_cmd_exec_finish},
  {"exec-interrupt", 0, mi_cmd_exec_interrupt},
  {"exec-next", 0, mi_cmd_exec_next},
  {"exec-next-instruction", 0, mi_cmd_exec_next_instruction},
  {"exec-return", 0, mi_cmd_exec_return},
  {"exec-run", 0, mi_cmd_exec_run},
  {"exec-show-arguments", 0, 0},
  {"exec-signal", 0, 0},
  {"exec-step", 0, mi_cmd_exec_step},
  {"exec-step-instruction", 0, mi_cmd_exec_step_instruction},
  {"exec-until", 0, mi_cmd_exec_until},
  {"file-clear", 0, 0},
  {"file-exec-and-symbols", "file %s", 0},
  {"file-exec-file", "exec-file %s", 0},
  {"file-list-exec-sections", 0, 0},
  {"file-list-exec-source-files", 0, 0},
  {"file-list-shared-libraries", 0, 0},
  {"file-list-symbol-files", 0, 0},
  {"file-symbol-file", "symbol-file %s", 0},
  {"gdb-complete", 0, 0},
  {"gdb-exit", 0, 0, mi_cmd_gdb_exit},
  {"gdb-set", "set %s", 0},
  {"gdb-show", "show %s", 0},
  {"gdb-source", 0, 0},
  {"gdb-version", "show version", 0},
  {"kod-info", 0, 0},
  {"kod-list", 0, 0},
  {"kod-list-object-types", 0, 0},
  {"kod-show", 0, 0},
  {"overlay-auto", 0, 0},
  {"overlay-list-mapping-state", 0, 0},
  {"overlay-list-overlays", 0, 0},
  {"overlay-map", 0, 0},
  {"overlay-off", 0, 0},
  {"overlay-on", 0, 0},
  {"overlay-unmap", 0, 0},
  {"signal-handle", 0, 0},
  {"signal-list-handle-actions", 0, 0},
  {"signal-list-signal-types", 0, 0},
  {"stack-info-depth", 0, 0, mi_cmd_stack_info_depth},
  {"stack-info-frame", 0, 0},
  {"stack-list-arguments", 0, 0, mi_cmd_stack_list_args},
  {"stack-list-exception-handlers", 0, 0},
  {"stack-list-frames", 0, 0, mi_cmd_stack_list_frames},
  {"stack-list-locals", 0, 0, mi_cmd_stack_list_locals},
  {"stack-select-frame", 0, 0, mi_cmd_stack_select_frame},
  {"symbol-info-address", 0, 0},
  {"symbol-info-file", 0, 0},
  {"symbol-info-function", 0, 0},
  {"symbol-info-line", 0, 0},
  {"symbol-info-symbol", 0, 0},
  {"symbol-list-functions", 0, 0},
  {"symbol-list-types", 0, 0},
  {"symbol-list-variables", 0, 0},
  {"symbol-locate", 0, 0},
  {"symbol-type", 0, 0},
  {"target-attach", 0, 0},
  {"target-compare-sections", 0, 0},
  {"target-detach", "detach", 0},
  {"target-download", 0, mi_cmd_target_download},
  {"target-exec-status", 0, 0},
  {"target-list-available-targets", 0, 0},
  {"target-list-current-targets", 0, 0},
  {"target-list-parameters", 0, 0},
  {"target-select", 0, mi_cmd_target_select},
  {"thread-info", 0, 0},
  {"thread-list-all-threads", 0, 0},
  {"thread-list-ids", 0, 0, mi_cmd_thread_list_ids},
  {"thread-select", 0, 0, mi_cmd_thread_select},
  {"trace-actions", 0, 0},
  {"trace-delete", 0, 0},
  {"trace-disable", 0, 0},
  {"trace-dump", 0, 0},
  {"trace-enable", 0, 0},
  {"trace-exists", 0, 0},
  {"trace-find", 0, 0},
  {"trace-frame-number", 0, 0},
  {"trace-info", 0, 0},
  {"trace-insert", 0, 0},
  {"trace-list", 0, 0},
  {"trace-pass-count", 0, 0},
  {"trace-save", 0, 0},
  {"trace-start", 0, 0},
  {"trace-stop", 0, 0},
  {"var-assign", 0, 0, mi_cmd_var_assign},
  {"var-create", 0, 0, mi_cmd_var_create},
  {"var-delete", 0, 0, mi_cmd_var_delete},
  {"var-evaluate-expression", 0, 0, mi_cmd_var_evaluate_expression},
  {"var-info-expression", 0, 0, mi_cmd_var_info_expression},
  {"var-info-num-children", 0, 0, mi_cmd_var_info_num_children},
  {"var-info-type", 0, 0, mi_cmd_var_info_type},
  {"var-list-children", 0, 0, mi_cmd_var_list_children},
  {"var-set-format", 0, 0, mi_cmd_var_set_format},
  {"var-show-attributes", 0, 0, mi_cmd_var_show_attributes},
  {"var-show-format", 0, 0, mi_cmd_var_show_format},
  {"var-update", 0, 0, mi_cmd_var_update},
  {0,}
};

/* Pointer to the mi command table (built at run time) */

static struct mi_cmd **mi_table;

/* A prime large enough to accomodate the entire command table */
enum
  {
    MI_TABLE_SIZE = 227
  };

/* Exported function used to obtain info from the table */
struct mi_cmd *
mi_lookup (const char *command)
{
  return *lookup_table (command);
}

/* stat collecting */
struct mi_cmd_stats
{
  int hit;
  int miss;
  int rehash;
};
struct mi_cmd_stats stats;

/* our lookup function */
static struct mi_cmd **
lookup_table (const char *command)
{
  const char *chp;
  unsigned int index = 0;
  /* compute our hash */
  for (chp = command; *chp; chp++)
    {
      /* some what arbitrary */
      index = ((index << 6) + (unsigned int) *chp) % MI_TABLE_SIZE;
    }
  /* look it up */
  while (1)
    {
      struct mi_cmd **entry = &mi_table[index];
      if ((*entry) == 0)
	{
	  /* not found, return pointer to next free. */
	  stats.miss++;
	  return entry;
	}
      if (strcmp (command, (*entry)->name) == 0)
	{
	  stats.hit++;
	  return entry;		/* found */
	}
      index = (index + 1) % MI_TABLE_SIZE;
      stats.rehash++;
    }
}

static void
build_table (struct mi_cmd *commands)
{
  int nr_rehash = 0;
  int nr_entries = 0;
  struct mi_cmd *command;
  int sizeof_table = sizeof (struct mi_cmd **) * MI_TABLE_SIZE;

  mi_table = xmalloc (sizeof_table);
  memset (mi_table, 0, sizeof_table);
  for (command = commands; command->name != 0; command++)
    {
      struct mi_cmd **entry = lookup_table (command->name);
      if (*entry)
	internal_error (__FILE__, __LINE__,
			"command `%s' appears to be duplicated",
			command->name);
      *entry = command;
      if (0)
	{
	  fprintf_unfiltered (gdb_stdlog, "%-30s %2d\n",
			      command->name, stats.rehash - nr_rehash);
	}
      nr_entries++;
      nr_rehash = stats.rehash;
    }
  if (0)
    {
      fprintf_filtered (gdb_stdlog, "Average %3.1f\n",
			(double) nr_rehash / (double) nr_entries);
    }
}

void
_initialize_mi_cmds (void)
{
  build_table (mi_cmds);
  memset (&stats, 0, sizeof (stats));
}
