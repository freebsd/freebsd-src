/* MI Command Set.
   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.
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

/* Work in progress */

#include "defs.h"
#include "target.h"
#include "inferior.h"
#include "gdb_string.h"
#include "top.h"
#include "gdbthread.h"
#include "mi-cmds.h"
#include "mi-parse.h"
#include "mi-getopt.h"
#include "mi-console.h"
#include "ui-out.h"
#include "mi-out.h"
#include "event-loop.h"
#include "event-top.h"
#include "gdbcore.h"		/* for write_memory() */
#include "value.h"		/* for write_register_bytes() */
#include "regcache.h"
#include "gdb.h"
#include <ctype.h>
#include <sys/time.h>

/* Convenience macro for allocting typesafe memory. */

#undef XMALLOC
#define XMALLOC(TYPE) (TYPE*) xmalloc (sizeof (TYPE))

enum
  {
    FROM_TTY = 0
  };


int mi_debug_p;
struct ui_file *raw_stdout;

/* The token of the last asynchronous command */
static char *last_async_command;
static char *previous_async_command;
static char *mi_error_message;
static char *old_regs;

extern void _initialize_mi_main (void);
static char *mi_input (char *);
static void mi_execute_command (char *cmd, int from_tty);
static enum mi_cmd_result mi_cmd_execute (struct mi_parse *parse);

static void mi_execute_cli_command (const char *cli, char *args);
static enum mi_cmd_result mi_execute_async_cli_command (char *mi, char *args, int from_tty);
static void mi_execute_command_wrapper (char *cmd);

void mi_exec_async_cli_cmd_continuation (struct continuation_arg *arg);

static int register_changed_p (int regnum);
static int get_register (int regnum, int format);
static void mi_load_progress (const char *section_name,
			      unsigned long sent_so_far,
			      unsigned long total_section,
			      unsigned long total_sent,
			      unsigned long grand_total);

/* FIXME: these should go in some .h file, but infcmd.c doesn't have a
   corresponding .h file. These wrappers will be obsolete anyway, once
   we pull the plug on the sanitization. */
extern void interrupt_target_command_wrapper (char *, int);
extern void return_command_wrapper (char *, int);

/* Command implementations. FIXME: Is this libgdb? No.  This is the MI
   layer that calls libgdb.  Any operation used in the below should be
   formalized. */

enum mi_cmd_result
mi_cmd_gdb_exit (char *command, char **argv, int argc)
{
  /* We have to print everything right here because we never return */
  if (last_async_command)
    fputs_unfiltered (last_async_command, raw_stdout);
  fputs_unfiltered ("^exit\n", raw_stdout);
  mi_out_put (uiout, raw_stdout);
  /* FIXME: The function called is not yet a formal libgdb function */
  quit_force (NULL, FROM_TTY);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_exec_run (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("run", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_next (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("next", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_next_instruction (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("nexti", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_step (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("step", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_step_instruction (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("stepi", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_finish (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("finish", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_until (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("until", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_return (char *args, int from_tty)
{
  /* This command doesn't really execute the target, it just pops the
     specified number of frames. */
  if (*args)
    /* Call return_command with from_tty argument equal to 0 so as to
       avoid being queried. */
    return_command_wrapper (args, 0);
  else
    /* Call return_command with from_tty argument equal to 0 so as to
       avoid being queried. */
    return_command_wrapper (NULL, 0);

  /* Because we have called return_command with from_tty = 0, we need
     to print the frame here. */
  show_and_print_stack_frame (selected_frame,
			      selected_frame_level,
			      LOC_AND_ADDRESS);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_exec_continue (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("continue", args, from_tty);
}

/* Interrupt the execution of the target. Note how we must play around
   with the token varialbes, in order to display the current token in
   the result of the interrupt command, and the previous execution
   token when the target finally stops. See comments in
   mi_cmd_execute. */
enum mi_cmd_result
mi_cmd_exec_interrupt (char *args, int from_tty)
{
  if (!target_executing)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_exec_interrupt: Inferior not executing.");
      return MI_CMD_ERROR;
    }
  interrupt_target_command_wrapper (args, from_tty);
  if (last_async_command)
    fputs_unfiltered (last_async_command, raw_stdout);
  fputs_unfiltered ("^done", raw_stdout);
  xfree (last_async_command);
  if (previous_async_command)
    last_async_command = xstrdup (previous_async_command);
  xfree (previous_async_command);
  previous_async_command = NULL;
  mi_out_put (uiout, raw_stdout);
  mi_out_rewind (uiout);
  fputs_unfiltered ("\n", raw_stdout);
  return MI_CMD_QUIET;
}

enum mi_cmd_result
mi_cmd_thread_select (char *command, char **argv, int argc)
{
  enum gdb_rc rc;

  if (argc != 1)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_thread_select: USAGE: threadnum.");
      return MI_CMD_ERROR;
    }
  else
    rc = gdb_thread_select (uiout, argv[0]);

  if (rc == GDB_RC_FAIL)
    return MI_CMD_CAUGHT_ERROR;
  else
    return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_thread_list_ids (char *command, char **argv, int argc)
{
  enum gdb_rc rc = MI_CMD_DONE;

  if (argc != 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_thread_list_ids: No arguments required.");
      return MI_CMD_ERROR;
    }
  else
    rc = gdb_list_thread_ids (uiout);

  if (rc == GDB_RC_FAIL)
    return MI_CMD_CAUGHT_ERROR;
  else
    return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_data_list_register_names (char *command, char **argv, int argc)
{
  int regnum, numregs;
  int i;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  ui_out_list_begin (uiout, "register-names");

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    ui_out_field_string (uiout, NULL, "");
	  else
	    ui_out_field_string (uiout, NULL, REGISTER_NAME (regnum));
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 0; i < argc; i++)
    {
      regnum = atoi (argv[i]);
      if (regnum < 0 || regnum >= numregs)
	{
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
      if (REGISTER_NAME (regnum) == NULL
	  || *(REGISTER_NAME (regnum)) == '\0')
	ui_out_field_string (uiout, NULL, "");
      else
	ui_out_field_string (uiout, NULL, REGISTER_NAME (regnum));
    }
  ui_out_list_end (uiout);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_data_list_changed_registers (char *command, char **argv, int argc)
{
  int regnum, numregs, changed;
  int i;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS;

  ui_out_list_begin (uiout, "changed-registers");

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  changed = register_changed_p (regnum);
	  if (changed < 0)
	    {
	      xasprintf (&mi_error_message,
			 "mi_cmd_data_list_changed_registers: Unable to read register contents.");
	      return MI_CMD_ERROR;
	    }
	  else if (changed)
	    ui_out_field_int (uiout, NULL, regnum);
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 0; i < argc; i++)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  changed = register_changed_p (regnum);
	  if (changed < 0)
	    {
	      xasprintf (&mi_error_message,
			 "mi_cmd_data_list_register_change: Unable to read register contents.");
	      return MI_CMD_ERROR;
	    }
	  else if (changed)
	    ui_out_field_int (uiout, NULL, regnum);
	}
      else
	{
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
    }
  ui_out_list_end (uiout);
  return MI_CMD_DONE;
}

static int
register_changed_p (int regnum)
{
  char *raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);

  if (read_relative_register_raw_bytes (regnum, raw_buffer))
    return -1;

  if (memcmp (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	      REGISTER_RAW_SIZE (regnum)) == 0)
    return 0;

  /* Found a changed register. Return 1. */

  memcpy (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	  REGISTER_RAW_SIZE (regnum));

  return 1;
}

/* Return a list of register number and value pairs. The valid
   arguments expected are: a letter indicating the format in which to
   display the registers contents. This can be one of: x (hexadecimal), d
   (decimal), N (natural), t (binary), o (octal), r (raw).  After the
   format argumetn there can be a sequence of numbers, indicating which
   registers to fetch the content of. If the format is the only argument,
   a list of all the registers with their values is returned. */
enum mi_cmd_result
mi_cmd_data_list_register_values (char *command, char **argv, int argc)
{
  int regnum, numregs, format, result;
  int i;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS;

  if (argc == 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_list_register_values: Usage: -data-list-register-values <format> [<regnum1>...<regnumN>]");
      return MI_CMD_ERROR;
    }

  format = (int) argv[0][0];

  if (!target_has_registers)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_list_register_values: No registers.");
      return MI_CMD_ERROR;
    }

  ui_out_list_begin (uiout, "register-values");

  if (argc == 1)		/* No args, beside the format: do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  ui_out_tuple_begin (uiout, NULL);
	  ui_out_field_int (uiout, "number", regnum);
	  result = get_register (regnum, format);
	  if (result == -1)
	    return MI_CMD_ERROR;
	  ui_out_tuple_end (uiout);
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 1; i < argc; i++)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  ui_out_tuple_begin (uiout, NULL);
	  ui_out_field_int (uiout, "number", regnum);
	  result = get_register (regnum, format);
	  if (result == -1)
	    return MI_CMD_ERROR;
	  ui_out_tuple_end (uiout);
	}
      else
	{
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
    }
  ui_out_list_end (uiout);
  return MI_CMD_DONE;
}

/* Output one register's contents in the desired format. */
static int
get_register (int regnum, int format)
{
  char *raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);
  char *virtual_buffer = alloca (MAX_REGISTER_VIRTUAL_SIZE);
  int optim;
  static struct ui_stream *stb = NULL;

  stb = ui_out_stream_new (uiout);

  if (format == 'N')
    format = 0;

  /* read_relative_register_raw_bytes returns a virtual frame pointer
     (FRAME_FP (selected_frame)) if regnum == FP_REGNUM instead
     of the real contents of the register. To get around this,
     use get_saved_register instead. */
  get_saved_register (raw_buffer, &optim, (CORE_ADDR *) NULL, selected_frame,
		      regnum, (enum lval_type *) NULL);
  if (optim)
    {
      xasprintf (&mi_error_message, "Optimized out");
      return -1;
    }

  /* Convert raw data to virtual format if necessary.  */

  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, REGISTER_VIRTUAL_TYPE (regnum),
				   raw_buffer, virtual_buffer);
    }
  else
    memcpy (virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE (regnum));

  if (format == 'r')
    {
      int j;
      char *ptr, buf[1024];

      strcpy (buf, "0x");
      ptr = buf + 2;
      for (j = 0; j < REGISTER_RAW_SIZE (regnum); j++)
	{
	  register int idx = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? j
	  : REGISTER_RAW_SIZE (regnum) - 1 - j;
	  sprintf (ptr, "%02x", (unsigned char) raw_buffer[idx]);
	  ptr += 2;
	}
      ui_out_field_string (uiout, "value", buf);
      /*fputs_filtered (buf, gdb_stdout); */
    }
  else
    {
      val_print (REGISTER_VIRTUAL_TYPE (regnum), virtual_buffer, 0, 0,
		 stb->stream, format, 1, 0, Val_pretty_default);
      ui_out_field_stream (uiout, "value", stb);
      ui_out_stream_delete (stb);
    }
  return 1;
}

/* Write given values into registers. The registers and values are
   given as pairs. The corresponding MI command is 
   -data-write-register-values <format> [<regnum1> <value1>...<regnumN> <valueN>]*/
enum mi_cmd_result
mi_cmd_data_write_register_values (char *command, char **argv, int argc)
{
  int regnum;
  int i;
  int numregs;
  LONGEST value;
  char format;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS;

  if (argc == 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: Usage: -data-write-register-values <format> [<regnum1> <value1>...<regnumN> <valueN>]");
      return MI_CMD_ERROR;
    }

  format = (int) argv[0][0];

  if (!target_has_registers)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: No registers.");
      return MI_CMD_ERROR;
    }

  if (!(argc - 1))
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: No regs and values specified.");
      return MI_CMD_ERROR;
    }

  if ((argc - 1) % 2)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_register_values: Regs and vals are not in pairs.");
      return MI_CMD_ERROR;
    }

  for (i = 1; i < argc; i = i + 2)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  void *buffer;
	  struct cleanup *old_chain;

	  /* Get the value as a number */
	  value = parse_and_eval_address (argv[i + 1]);
	  /* Get the value into an array */
	  buffer = xmalloc (REGISTER_SIZE);
	  old_chain = make_cleanup (xfree, buffer);
	  store_signed_integer (buffer, REGISTER_SIZE, value);
	  /* Write it down */
	  write_register_bytes (REGISTER_BYTE (regnum), buffer, REGISTER_RAW_SIZE (regnum));
	  /* Free the buffer.  */
	  do_cleanups (old_chain);
	}
      else
	{
	  xasprintf (&mi_error_message, "bad register number");
	  return MI_CMD_ERROR;
	}
    }
  return MI_CMD_DONE;
}

#if 0
/*This is commented out because we decided it was not useful. I leave
   it, just in case. ezannoni:1999-12-08 */

/* Assign a value to a variable. The expression argument must be in
   the form A=2 or "A = 2" (I.e. if there are spaces it needs to be
   quoted. */
enum mi_cmd_result
mi_cmd_data_assign (char *command, char **argv, int argc)
{
  struct expression *expr;
  struct cleanup *old_chain;

  if (argc != 1)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_assign: Usage: -data-assign expression");
      return MI_CMD_ERROR;
    }

  /* NOTE what follows is a clone of set_command(). FIXME: ezannoni
     01-12-1999: Need to decide what to do with this for libgdb purposes. */

  expr = parse_expression (argv[0]);
  old_chain = make_cleanup (free_current_contents, &expr);
  evaluate_expression (expr);
  do_cleanups (old_chain);
  return MI_CMD_DONE;
}
#endif

/* Evaluate the value of the argument. The argument is an
   expression. If the expression contains spaces it needs to be
   included in double quotes. */
enum mi_cmd_result
mi_cmd_data_evaluate_expression (char *command, char **argv, int argc)
{
  struct expression *expr;
  struct cleanup *old_chain = NULL;
  struct value *val;
  struct ui_stream *stb = NULL;

  stb = ui_out_stream_new (uiout);

  if (argc != 1)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_evaluate_expression: Usage: -data-evaluate-expression expression");
      return MI_CMD_ERROR;
    }

  expr = parse_expression (argv[0]);

  old_chain = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  /* Print the result of the expression evaluation. */
  val_print (VALUE_TYPE (val), VALUE_CONTENTS (val),
	     VALUE_EMBEDDED_OFFSET (val), VALUE_ADDRESS (val),
	     stb->stream, 0, 0, 0, 0);

  ui_out_field_stream (uiout, "value", stb);
  ui_out_stream_delete (stb);

  do_cleanups (old_chain);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_target_download (char *args, int from_tty)
{
  char *run;
  struct cleanup *old_cleanups = NULL;

  xasprintf (&run, "load %s", args);
  old_cleanups = make_cleanup (xfree, run);
  execute_command (run, from_tty);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

/* Connect to the remote target. */
enum mi_cmd_result
mi_cmd_target_select (char *args, int from_tty)
{
  char *run;
  struct cleanup *old_cleanups = NULL;

  xasprintf (&run, "target %s", args);
  old_cleanups = make_cleanup (xfree, run);

  /* target-select is always synchronous.  once the call has returned
     we know that we are connected. */
  /* NOTE: At present all targets that are connected are also
     (implicitly) talking to a halted target.  In the future this may
     change. */
  execute_command (run, from_tty);

  do_cleanups (old_cleanups);

  /* Issue the completion message here. */
  if (last_async_command)
    fputs_unfiltered (last_async_command, raw_stdout);
  fputs_unfiltered ("^connected", raw_stdout);
  mi_out_put (uiout, raw_stdout);
  mi_out_rewind (uiout);
  fputs_unfiltered ("\n", raw_stdout);
  do_exec_cleanups (ALL_CLEANUPS);
  return MI_CMD_QUIET;
}

/* DATA-MEMORY-READ:

   ADDR: start address of data to be dumped.
   WORD-FORMAT: a char indicating format for the ``word''. See 
   the ``x'' command.
   WORD-SIZE: size of each ``word''; 1,2,4, or 8 bytes
   NR_ROW: Number of rows.
   NR_COL: The number of colums (words per row).
   ASCHAR: (OPTIONAL) Append an ascii character dump to each row.  Use
   ASCHAR for unprintable characters.

   Reads SIZE*NR_ROW*NR_COL bytes starting at ADDR from memory and
   displayes them.  Returns:

   {addr="...",rowN={wordN="..." ,... [,ascii="..."]}, ...}

   Returns: 
   The number of bytes read is SIZE*ROW*COL. */

enum mi_cmd_result
mi_cmd_data_read_memory (char *command, char **argv, int argc)
{
  struct cleanup *cleanups = make_cleanup (null_cleanup, NULL);
  CORE_ADDR addr;
  long total_bytes;
  long nr_cols;
  long nr_rows;
  char word_format;
  struct type *word_type;
  long word_size;
  char word_asize;
  char aschar;
  char *mbuf;
  int nr_bytes;
  long offset = 0;
  int optind = 0;
  char *optarg;
  enum opt
    {
      OFFSET_OPT
    };
  static struct mi_opt opts[] =
  {
    {"o", OFFSET_OPT, 1},
    0
  };

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_data_read_memory", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case OFFSET_OPT:
	  offset = atol (optarg);
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  if (argc < 5 || argc > 6)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: Usage: ADDR WORD-FORMAT WORD-SIZE NR-ROWS NR-COLS [ASCHAR].");
      return MI_CMD_ERROR;
    }

  /* Extract all the arguments. */

  /* Start address of the memory dump. */
  addr = parse_and_eval_address (argv[0]) + offset;
  /* The format character to use when displaying a memory word. See
     the ``x'' command. */
  word_format = argv[1][0];
  /* The size of the memory word. */
  word_size = atol (argv[2]);
  switch (word_size)
    {
    case 1:
      word_type = builtin_type_int8;
      word_asize = 'b';
      break;
    case 2:
      word_type = builtin_type_int16;
      word_asize = 'h';
      break;
    case 4:
      word_type = builtin_type_int32;
      word_asize = 'w';
      break;
    case 8:
      word_type = builtin_type_int64;
      word_asize = 'g';
      break;
    default:
      word_type = builtin_type_int8;
      word_asize = 'b';
    }
  /* The number of rows */
  nr_rows = atol (argv[3]);
  if (nr_rows <= 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: invalid number of rows.");
      return MI_CMD_ERROR;
    }
  /* number of bytes per row. */
  nr_cols = atol (argv[4]);
  if (nr_cols <= 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: invalid number of columns.");
    }
  /* The un-printable character when printing ascii. */
  if (argc == 6)
    aschar = *argv[5];
  else
    aschar = 0;

  /* create a buffer and read it in. */
  total_bytes = word_size * nr_rows * nr_cols;
  mbuf = xcalloc (total_bytes, 1);
  make_cleanup (xfree, mbuf);
  if (mbuf == NULL)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_read_memory: out of memory.");
      return MI_CMD_ERROR;
    }
  nr_bytes = 0;
  while (nr_bytes < total_bytes)
    {
      int error;
      long num = target_read_memory_partial (addr + nr_bytes, mbuf + nr_bytes,
					     total_bytes - nr_bytes,
					     &error);
      if (num <= 0)
	break;
      nr_bytes += num;
    }

  /* output the header information. */
  ui_out_field_core_addr (uiout, "addr", addr);
  ui_out_field_int (uiout, "nr-bytes", nr_bytes);
  ui_out_field_int (uiout, "total-bytes", total_bytes);
  ui_out_field_core_addr (uiout, "next-row", addr + word_size * nr_cols);
  ui_out_field_core_addr (uiout, "prev-row", addr - word_size * nr_cols);
  ui_out_field_core_addr (uiout, "next-page", addr + total_bytes);
  ui_out_field_core_addr (uiout, "prev-page", addr - total_bytes);

  /* Build the result as a two dimentional table. */
  {
    struct ui_stream *stream = ui_out_stream_new (uiout);
    int row;
    int row_byte;
    ui_out_list_begin (uiout, "memory");
    for (row = 0, row_byte = 0;
	 row < nr_rows;
	 row++, row_byte += nr_cols * word_size)
      {
	int col;
	int col_byte;
	ui_out_tuple_begin (uiout, NULL);
	ui_out_field_core_addr (uiout, "addr", addr + row_byte);
	/* ui_out_field_core_addr_symbolic (uiout, "saddr", addr + row_byte); */
	ui_out_list_begin (uiout, "data");
	for (col = 0, col_byte = row_byte;
	     col < nr_cols;
	     col++, col_byte += word_size)
	  {
	    if (col_byte + word_size > nr_bytes)
	      {
		ui_out_field_string (uiout, NULL, "N/A");
	      }
	    else
	      {
		ui_file_rewind (stream->stream);
		print_scalar_formatted (mbuf + col_byte, word_type, word_format,
					word_asize, stream->stream);
		ui_out_field_stream (uiout, NULL, stream);
	      }
	  }
	ui_out_list_end (uiout);
	if (aschar)
	  {
	    int byte;
	    ui_file_rewind (stream->stream);
	    for (byte = row_byte; byte < row_byte + word_size * nr_cols; byte++)
	      {
		if (byte >= nr_bytes)
		  {
		    fputc_unfiltered ('X', stream->stream);
		  }
		else if (mbuf[byte] < 32 || mbuf[byte] > 126)
		  {
		    fputc_unfiltered (aschar, stream->stream);
		  }
		else
		  fputc_unfiltered (mbuf[byte], stream->stream);
	      }
	    ui_out_field_stream (uiout, "ascii", stream);
	  }
	ui_out_tuple_end (uiout);
      }
    ui_out_stream_delete (stream);
    ui_out_list_end (uiout);
  }
  do_cleanups (cleanups);
  return MI_CMD_DONE;
}

/* DATA-MEMORY-WRITE:

   COLUMN_OFFSET: optional argument. Must be preceeded by '-o'. The
   offset from the beginning of the memory grid row where the cell to
   be written is.
   ADDR: start address of the row in the memory grid where the memory
   cell is, if OFFSET_COLUMN is specified. Otherwise, the address of
   the location to write to.
   FORMAT: a char indicating format for the ``word''. See 
   the ``x'' command.
   WORD_SIZE: size of each ``word''; 1,2,4, or 8 bytes
   VALUE: value to be written into the memory address.

   Writes VALUE into ADDR + (COLUMN_OFFSET * WORD_SIZE).

   Prints nothing. */
enum mi_cmd_result
mi_cmd_data_write_memory (char *command, char **argv, int argc)
{
  CORE_ADDR addr;
  char word_format;
  long word_size;
  /* FIXME: ezannoni 2000-02-17 LONGEST could possibly not be big
     enough when using a compiler other than GCC. */
  LONGEST value;
  void *buffer;
  struct cleanup *old_chain;
  long offset = 0;
  int optind = 0;
  char *optarg;
  enum opt
    {
      OFFSET_OPT
    };
  static struct mi_opt opts[] =
  {
    {"o", OFFSET_OPT, 1},
    0
  };

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_data_write_memory", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case OFFSET_OPT:
	  offset = atol (optarg);
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  if (argc != 4)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_data_write_memory: Usage: [-o COLUMN_OFFSET] ADDR FORMAT WORD-SIZE VALUE.");
      return MI_CMD_ERROR;
    }

  /* Extract all the arguments. */
  /* Start address of the memory dump. */
  addr = parse_and_eval_address (argv[0]);
  /* The format character to use when displaying a memory word. See
     the ``x'' command. */
  word_format = argv[1][0];
  /* The size of the memory word. */
  word_size = atol (argv[2]);

  /* Calculate the real address of the write destination. */
  addr += (offset * word_size);

  /* Get the value as a number */
  value = parse_and_eval_address (argv[3]);
  /* Get the value into an array */
  buffer = xmalloc (word_size);
  old_chain = make_cleanup (xfree, buffer);
  store_signed_integer (buffer, word_size, value);
  /* Write it down to memory */
  write_memory (addr, buffer, word_size);
  /* Free the buffer.  */
  do_cleanups (old_chain);

  return MI_CMD_DONE;
}

/* Execute a command within a safe environment.  Return >0 for
   ok. Return <0 for supress prompt.  Return 0 to have the error
   extracted from error_last_message(). */

static int
captured_mi_execute_command (void *data)
{
  struct mi_parse *context = data;
  enum mi_cmd_result rc;

  switch (context->op)
    {

    case MI_COMMAND:
      /* A MI command was read from the input stream */
      if (mi_debug_p)
	/* FIXME: gdb_???? */
	fprintf_unfiltered (raw_stdout, " token=`%s' command=`%s' args=`%s'\n",
			    context->token, context->command, context->args);
      /* FIXME: cagney/1999-09-25: Rather than this convoluted
         condition expression, each function should return an
         indication of what action is required and then switch on
         that. */
      rc = mi_cmd_execute (context);
      if (!target_can_async_p () || !target_executing)
	{
	  /* print the result if there were no errors */
	  if (rc == MI_CMD_DONE)
	    {
	      fputs_unfiltered (context->token, raw_stdout);
	      fputs_unfiltered ("^done", raw_stdout);
	      mi_out_put (uiout, raw_stdout);
	      mi_out_rewind (uiout);
	      fputs_unfiltered ("\n", raw_stdout);
	    }
	  else if (rc == MI_CMD_ERROR)
	    {
	      if (mi_error_message)
		{
		  fputs_unfiltered (context->token, raw_stdout);
		  fputs_unfiltered ("^error,msg=\"", raw_stdout);
		  fputstr_unfiltered (mi_error_message, '"', raw_stdout);
		  xfree (mi_error_message);
		  fputs_unfiltered ("\"\n", raw_stdout);
		}
	      mi_out_rewind (uiout);
	    }
	  else if (rc == MI_CMD_CAUGHT_ERROR)
	    {
	      mi_out_rewind (uiout);
	      return 0;
	    }
	  else
	    mi_out_rewind (uiout);
	}
      else if (sync_execution)
	/* Don't print the prompt. We are executing the target in
	   synchronous mode. */
	return -1;
      break;

    case CLI_COMMAND:
      /* A CLI command was read from the input stream */
      /* This will be removed as soon as we have a complete set of
         mi commands */
      /* echo the command on the console. */
      fprintf_unfiltered (gdb_stdlog, "%s\n", context->command);
      /* FIXME: If the command string has something that looks like 
         a format spec (e.g. %s) we will get a core dump */
      mi_execute_cli_command ("%s", context->command);
      /* print the result */
      /* FIXME: Check for errors here. */
      fputs_unfiltered (context->token, raw_stdout);
      fputs_unfiltered ("^done", raw_stdout);
      mi_out_put (uiout, raw_stdout);
      mi_out_rewind (uiout);
      fputs_unfiltered ("\n", raw_stdout);
      break;

    }
  return 1;
}


void
mi_execute_command (char *cmd, int from_tty)
{
  struct mi_parse *command;

  /* This is to handle EOF (^D). We just quit gdb. */
  /* FIXME: we should call some API function here. */
  if (cmd == 0)
    quit_force (NULL, from_tty);

  command = mi_parse (cmd);

  if (command != NULL)
    {
      /* FIXME: cagney/1999-11-04: Can this use of catch_errors either
         be pushed even further down or even eliminated? */
      int rc = catch_errors (captured_mi_execute_command, command, "",
			     RETURN_MASK_ALL);
      if (rc < 0)
	{
	  /* The command is executing synchronously.  Bail out early
	     suppressing the finished prompt. */
	  mi_parse_free (command);
	  return;
	}
      if (rc == 0)
	{
	  char *msg = error_last_message ();
	  struct cleanup *cleanup = make_cleanup (xfree, msg);
	  /* The command execution failed and error() was called
	     somewhere */
	  fputs_unfiltered (command->token, raw_stdout);
	  fputs_unfiltered ("^error,msg=\"", raw_stdout);
	  fputstr_unfiltered (msg, '"', raw_stdout);
	  fputs_unfiltered ("\"\n", raw_stdout);
	}
      mi_parse_free (command);
    }

  fputs_unfiltered ("(gdb) \n", raw_stdout);
  gdb_flush (raw_stdout);
  /* print any buffered hook code */
  /* ..... */
}

static enum mi_cmd_result
mi_cmd_execute (struct mi_parse *parse)
{
  if (parse->cmd->argv_func != NULL
      || parse->cmd->args_func != NULL)
    {
      /* FIXME: We need to save the token because the command executed
         may be asynchronous and need to print the token again.
         In the future we can pass the token down to the func
         and get rid of the last_async_command */
      /* The problem here is to keep the token around when we launch
         the target, and we want to interrupt it later on.  The
         interrupt command will have its own token, but when the
         target stops, we must display the token corresponding to the
         last execution command given. So we have another string where
         we copy the token (previous_async_command), if this was
         indeed the token of an execution command, and when we stop we
         print that one. This is possible because the interrupt
         command, when over, will copy that token back into the
         default token string (last_async_command). */

      if (target_executing)
	{
	  if (!previous_async_command)
	    previous_async_command = xstrdup (last_async_command);
	  if (strcmp (parse->command, "exec-interrupt"))
	    {
	      fputs_unfiltered (parse->token, raw_stdout);
	      fputs_unfiltered ("^error,msg=\"", raw_stdout);
	      fputs_unfiltered ("Cannot execute command ", raw_stdout);
	      fputstr_unfiltered (parse->command, '"', raw_stdout);
	      fputs_unfiltered (" while target running", raw_stdout);
	      fputs_unfiltered ("\"\n", raw_stdout);
	      return MI_CMD_ERROR;
	    }
	}
      last_async_command = xstrdup (parse->token);
      make_exec_cleanup (free_current_contents, &last_async_command);
      /* FIXME: DELETE THIS! */
      if (parse->cmd->args_func != NULL)
	return parse->cmd->args_func (parse->args, 0 /*from_tty */ );
      return parse->cmd->argv_func (parse->command, parse->argv, parse->argc);
    }
  else if (parse->cmd->cli != 0)
    {
      /* FIXME: DELETE THIS. */
      /* The operation is still implemented by a cli command */
      /* Must be a synchronous one */
      mi_execute_cli_command (parse->cmd->cli, parse->args);
      return MI_CMD_DONE;
    }
  else
    {
      /* FIXME: DELETE THIS. */
      fputs_unfiltered (parse->token, raw_stdout);
      fputs_unfiltered ("^error,msg=\"", raw_stdout);
      fputs_unfiltered ("Undefined mi command: ", raw_stdout);
      fputstr_unfiltered (parse->command, '"', raw_stdout);
      fputs_unfiltered (" (missing implementation)", raw_stdout);
      fputs_unfiltered ("\"\n", raw_stdout);
      return MI_CMD_ERROR;
    }
}

static void
mi_execute_command_wrapper (char *cmd)
{
  mi_execute_command (cmd, stdin == instream);
}

/* FIXME: This is just a hack so we can get some extra commands going.
   We don't want to channel things through the CLI, but call libgdb directly */
/* Use only for synchronous commands */

void
mi_execute_cli_command (const char *cli, char *args)
{
  if (cli != 0)
    {
      struct cleanup *old_cleanups;
      char *run;
      xasprintf (&run, cli, args);
      if (mi_debug_p)
	/* FIXME: gdb_???? */
	fprintf_unfiltered (gdb_stdout, "cli=%s run=%s\n",
			    cli, run);
      old_cleanups = make_cleanup (xfree, run);
      execute_command ( /*ui */ run, 0 /*from_tty */ );
      do_cleanups (old_cleanups);
      return;
    }
}

enum mi_cmd_result
mi_execute_async_cli_command (char *mi, char *args, int from_tty)
{
  struct cleanup *old_cleanups;
  char *run;
  char *async_args;

  if (target_can_async_p ())
    {
      async_args = (char *) xmalloc (strlen (args) + 2);
      make_exec_cleanup (free, async_args);
      strcpy (async_args, args);
      strcat (async_args, "&");
      xasprintf (&run, "%s %s", mi, async_args);
      make_exec_cleanup (free, run);
      add_continuation (mi_exec_async_cli_cmd_continuation, NULL);
      old_cleanups = NULL;
    }
  else
    {
      xasprintf (&run, "%s %s", mi, args);
      old_cleanups = make_cleanup (xfree, run);
    }

  if (!target_can_async_p ())
    {
      /* NOTE: For synchronous targets asynchronous behavour is faked by
         printing out the GDB prompt before we even try to execute the
         command. */
      if (last_async_command)
	fputs_unfiltered (last_async_command, raw_stdout);
      fputs_unfiltered ("^running\n", raw_stdout);
      fputs_unfiltered ("(gdb) \n", raw_stdout);
      gdb_flush (raw_stdout);
    }
  else
    {
      /* FIXME: cagney/1999-11-29: Printing this message before
         calling execute_command is wrong.  It should only be printed
         once gdb has confirmed that it really has managed to send a
         run command to the target. */
      if (last_async_command)
	fputs_unfiltered (last_async_command, raw_stdout);
      fputs_unfiltered ("^running\n", raw_stdout);
    }

  execute_command ( /*ui */ run, 0 /*from_tty */ );

  if (!target_can_async_p ())
    {
      /* Do this before doing any printing.  It would appear that some
         print code leaves garbage around in the buffer. */
      do_cleanups (old_cleanups);
      /* If the target was doing the operation synchronously we fake
         the stopped message. */
      if (last_async_command)
	fputs_unfiltered (last_async_command, raw_stdout);
      fputs_unfiltered ("*stopped", raw_stdout);
      mi_out_put (uiout, raw_stdout);
      mi_out_rewind (uiout);
      fputs_unfiltered ("\n", raw_stdout);
      return MI_CMD_QUIET;
    }
  return MI_CMD_DONE;
}

void
mi_exec_async_cli_cmd_continuation (struct continuation_arg *arg)
{
  if (last_async_command)
    fputs_unfiltered (last_async_command, raw_stdout);
  fputs_unfiltered ("*stopped", raw_stdout);
  mi_out_put (uiout, raw_stdout);
  fputs_unfiltered ("\n", raw_stdout);
  fputs_unfiltered ("(gdb) \n", raw_stdout);
  gdb_flush (raw_stdout);
  do_exec_cleanups (ALL_CLEANUPS);
}

static char *
mi_input (char *buf)
{
  return gdb_readline (NULL);
}

static void
mi_load_progress (const char *section_name,
		  unsigned long sent_so_far,
		  unsigned long total_section,
		  unsigned long total_sent,
		  unsigned long grand_total)
{
  struct timeval time_now, delta, update_threshold;
  static struct timeval last_update;
  static char *previous_sect_name = NULL;
  int new_section;

  if (!interpreter_p || strncmp (interpreter_p, "mi", 2) != 0)
    return;

  update_threshold.tv_sec = 0;
  update_threshold.tv_usec = 500000;
  gettimeofday (&time_now, NULL);

  delta.tv_usec = time_now.tv_usec - last_update.tv_usec;
  delta.tv_sec = time_now.tv_sec - last_update.tv_sec;

  if (delta.tv_usec < 0)
    {
      delta.tv_sec -= 1;
      delta.tv_usec += 1000000;
    }

  new_section = (previous_sect_name ?
		 strcmp (previous_sect_name, section_name) : 1);
  if (new_section)
    {
      xfree (previous_sect_name);
      previous_sect_name = xstrdup (section_name);

      if (last_async_command)
	fputs_unfiltered (last_async_command, raw_stdout);
      fputs_unfiltered ("+download", raw_stdout);
      ui_out_tuple_begin (uiout, NULL);
      ui_out_field_string (uiout, "section", section_name);
      ui_out_field_int (uiout, "section-size", total_section);
      ui_out_field_int (uiout, "total-size", grand_total);
      ui_out_tuple_end (uiout);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      gdb_flush (raw_stdout);
    }

  if (delta.tv_sec >= update_threshold.tv_sec &&
      delta.tv_usec >= update_threshold.tv_usec)
    {
      last_update.tv_sec = time_now.tv_sec;
      last_update.tv_usec = time_now.tv_usec;
      if (last_async_command)
	fputs_unfiltered (last_async_command, raw_stdout);
      fputs_unfiltered ("+download", raw_stdout);
      ui_out_tuple_begin (uiout, NULL);
      ui_out_field_string (uiout, "section", section_name);
      ui_out_field_int (uiout, "section-sent", sent_so_far);
      ui_out_field_int (uiout, "section-size", total_section);
      ui_out_field_int (uiout, "total-sent", total_sent);
      ui_out_field_int (uiout, "total-size", grand_total);
      ui_out_tuple_end (uiout);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      gdb_flush (raw_stdout);
    }
}

static void
mi_command_loop (int mi_version)
{
  /* HACK: Force stdout/stderr to point at the console.  This avoids
     any potential side effects caused by legacy code that is still
     using the TUI / fputs_unfiltered_hook */
  raw_stdout = stdio_fileopen (stdout);
  /* Route normal output through the MIx */
  gdb_stdout = mi_console_file_new (raw_stdout, "~");
  /* Route error and log output through the MI */
  gdb_stderr = mi_console_file_new (raw_stdout, "&");
  gdb_stdlog = gdb_stderr;
  /* Route target output through the MI. */
  gdb_stdtarg = mi_console_file_new (raw_stdout, "@");

  /* HACK: Poke the ui_out table directly.  Should we be creating a
     mi_out object wired up to the above gdb_stdout / gdb_stderr? */
  uiout = mi_out_new (mi_version);

  /* HACK: Override any other interpreter hooks.  We need to create a
     real event table and pass in that. */
  init_ui_hook = 0;
  /* command_loop_hook = 0; */
  print_frame_info_listing_hook = 0;
  query_hook = 0;
  warning_hook = 0;
  create_breakpoint_hook = 0;
  delete_breakpoint_hook = 0;
  modify_breakpoint_hook = 0;
  interactive_hook = 0;
  registers_changed_hook = 0;
  readline_begin_hook = 0;
  readline_hook = 0;
  readline_end_hook = 0;
  register_changed_hook = 0;
  memory_changed_hook = 0;
  context_hook = 0;
  target_wait_hook = 0;
  call_command_hook = 0;
  error_hook = 0;
  error_begin_hook = 0;
  show_load_progress = mi_load_progress;

  /* Turn off 8 bit strings in quoted output.  Any character with the
     high bit set is printed using C's octal format. */
  sevenbit_strings = 1;

  /* Tell the world that we're alive */
  fputs_unfiltered ("(gdb) \n", raw_stdout);
  gdb_flush (raw_stdout);

  if (!event_loop_p)
    simplified_command_loop (mi_input, mi_execute_command);
  else
    start_event_loop ();
}

static void
mi0_command_loop (void)
{
  mi_command_loop (0);
}

static void
mi1_command_loop (void)
{
  mi_command_loop (1);
}

static void
setup_architecture_data (void)
{
  /* don't trust REGISTER_BYTES to be zero. */
  old_regs = xmalloc (REGISTER_BYTES + 1);
  memset (old_regs, 0, REGISTER_BYTES + 1);
}

static void
mi_init_ui (char *arg0)
{
  /* Eventually this will contain code that takes control of the
     console. */
}

void
_initialize_mi_main (void)
{
  if (interpreter_p == NULL)
    return;

  /* If we're _the_ interpreter, take control. */
  if (strcmp (interpreter_p, "mi0") == 0)
    command_loop_hook = mi0_command_loop;
  else if (strcmp (interpreter_p, "mi") == 0
	   || strcmp (interpreter_p, "mi1") == 0)
    command_loop_hook = mi1_command_loop;
  else
    return;

  init_ui_hook = mi_init_ui;
  setup_architecture_data ();
  register_gdbarch_swap (&old_regs, sizeof (old_regs), NULL);
  register_gdbarch_swap (NULL, 0, setup_architecture_data);
  if (event_loop_p)
    {
      /* These overwrite some of the initialization done in
	 _intialize_event_loop. */
      call_readline = gdb_readline2;
      input_handler = mi_execute_command_wrapper;
      add_file_handler (input_fd, stdin_event_handler, 0);
      async_command_editing_p = 0;
    }
  /* FIXME: Should we notify main that we are here as a possible
     interpreter? */
}
