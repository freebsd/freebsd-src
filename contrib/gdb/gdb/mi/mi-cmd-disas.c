/* MI Command Set - disassemble commands.
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

#include "defs.h"
#include "target.h"
#include "value.h"
#include "mi-cmds.h"
#include "mi-getopt.h"
#include "ui-out.h"

static int gdb_dis_asm_read_memory (bfd_vma memaddr, bfd_byte * myaddr, unsigned int len,
				    disassemble_info * info);
static int compare_lines (const PTR mle1p, const PTR mle2p);

/* Disassemble functions. FIXME: these do not really belong here. We
   should get rid of all the duplicate code in gdb that does the same
   thing: disassemble_command() and the gdbtk variation. */

/* This Structure is used in mi_cmd_disassemble.
   We need a different sort of line table from the normal one cuz we can't
   depend upon implicit line-end pc's for lines to do the
   reordering in this function.  */

struct dis_line_entry
  {
    int line;
    CORE_ADDR start_pc;
    CORE_ADDR end_pc;
  };

/* This variable determines where memory used for disassembly is read from. */
int gdb_disassemble_from_exec = -1;

/* This is the memory_read_func for gdb_disassemble when we are
   disassembling from the exec file. */
static int
gdb_dis_asm_read_memory (bfd_vma memaddr, bfd_byte * myaddr,
			 unsigned int len, disassemble_info * info)
{
  extern struct target_ops exec_ops;
  int res;

  errno = 0;
  res = xfer_memory (memaddr, myaddr, len, 0, 0, &exec_ops);

  if (res == len)
    return 0;
  else if (errno == 0)
    return EIO;
  else
    return errno;
}

static int
compare_lines (const PTR mle1p, const PTR mle2p)
{
  struct dis_line_entry *mle1, *mle2;
  int val;

  mle1 = (struct dis_line_entry *) mle1p;
  mle2 = (struct dis_line_entry *) mle2p;

  val = mle1->line - mle2->line;

  if (val != 0)
    return val;

  return mle1->start_pc - mle2->start_pc;
}

/* The arguments to be passed on the command line and parsed here are:

   either:

   START-ADDRESS: address to start the disassembly at.
   END-ADDRESS: address to end the disassembly at.

   or:

   FILENAME: The name of the file where we want disassemble from.
   LINE: The line around which we want to disassemble. It will
   disassemble the function that contins that line.
   HOW_MANY: Number of disassembly lines to display. In mixed mode, it
   is the number of disassembly lines only, not counting the source
   lines.  

   always required:

   MODE: 0 or 1 for disassembly only, or mixed source and disassembly,
   respectively. */

enum mi_cmd_result
mi_cmd_disassemble (char *command, char **argv, int argc)
{
  CORE_ADDR pc;
  CORE_ADDR start;

  int mixed_source_and_assembly;
  int num_displayed;
  static disassemble_info di;
  static int di_initialized;

  struct symtab *s;

  /* To collect the instruction outputted from opcodes. */
  static struct ui_stream *stb = NULL;

  /* parts of the symbolic representation of the address */
  int line;
  int offset;
  int unmapped;
  char *filename = NULL;
  char *name = NULL;

  /* Which options have we processed ... */
  int file_seen = 0;
  int line_seen = 0;
  int num_seen = 0;
  int start_seen = 0;
  int end_seen = 0;

  /* ... and their corresponding value. */
  char *file_string = NULL;
  int line_num = -1;
  int how_many = -1;
  CORE_ADDR low = 0;
  CORE_ADDR high = 0;

  /* Options processing stuff. */
  int optind = 0;
  char *optarg;
  enum opt
    {
      FILE_OPT, LINE_OPT, NUM_OPT, START_OPT, END_OPT
    };
  static struct mi_opt opts[] =
  {
    {"f", FILE_OPT, 1},
    {"l", LINE_OPT, 1},
    {"n", NUM_OPT, 1},
    {"s", START_OPT, 1},
    {"e", END_OPT, 1},
    0
  };

  /* Get the options with their arguments. Keep track of what we
     encountered. */
  while (1)
    {
      int opt = mi_getopt ("mi_cmd_disassemble", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case FILE_OPT:
	  file_string = xstrdup (optarg);
	  file_seen = 1;
	  break;
	case LINE_OPT:
	  line_num = atoi (optarg);
	  line_seen = 1;
	  break;
	case NUM_OPT:
	  how_many = atoi (optarg);
	  num_seen = 1;
	  break;
	case START_OPT:
	  low = parse_and_eval_address (optarg);
	  start_seen = 1;
	  break;
	case END_OPT:
	  high = parse_and_eval_address (optarg);
	  end_seen = 1;
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  /* Allow only filename + linenum (with how_many which is not
     required) OR start_addr + and_addr */

  if (!((line_seen && file_seen && num_seen && !start_seen && !end_seen)
	|| (line_seen && file_seen && !num_seen && !start_seen && !end_seen)
      || (!line_seen && !file_seen && !num_seen && start_seen && end_seen)))
    error ("mi_cmd_disassemble: Usage: ( [-f filename -l linenum [-n howmany]] | [-s startaddr -e endaddr]) [--] mixed_mode.");

  if (argc != 1)
    error ("mi_cmd_disassemble: Usage: [-f filename -l linenum [-n howmany]] [-s startaddr -e endaddr] [--] mixed_mode.");

  mixed_source_and_assembly = atoi (argv[0]);
  if ((mixed_source_and_assembly != 0) && (mixed_source_and_assembly != 1))
    error ("mi_cmd_disassemble: Mixed_mode argument must be 0 or 1.");

  /* We must get the function beginning and end where line_num is
     contained. */

  if (line_seen && file_seen)
    {
      s = lookup_symtab (file_string);
      if (s == NULL)
	error ("mi_cmd_disassemble: Invalid filename.");
      if (!find_line_pc (s, line_num, &start))
	error ("mi_cmd_disassemble: Invalid line number");
      if (find_pc_partial_function (start, NULL, &low, &high) == 0)
	error ("mi_cmd_disassemble: No function contains specified address");
    }

  if (!di_initialized)
    {
      /* We don't add a cleanup for this, because the allocation of
         the stream is done once only for each gdb run, and we need to
         keep it around until the end. Hopefully there won't be any
         errors in the init code below, that make this function bail
         out. */
      stb = ui_out_stream_new (uiout);
      INIT_DISASSEMBLE_INFO_NO_ARCH (di, stb->stream,
				     (fprintf_ftype) fprintf_unfiltered);
      di.flavour = bfd_target_unknown_flavour;
      di.memory_error_func = dis_asm_memory_error;
      di.print_address_func = dis_asm_print_address;
      di_initialized = 1;
    }

  di.mach = TARGET_PRINT_INSN_INFO->mach;
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    di.endian = BFD_ENDIAN_BIG;
  else
    di.endian = BFD_ENDIAN_LITTLE;

  /* If gdb_disassemble_from_exec == -1, then we use the following heuristic to
     determine whether or not to do disassembly from target memory or from the
     exec file:

     If we're debugging a local process, read target memory, instead of the
     exec file.  This makes disassembly of functions in shared libs work
     correctly.  Also, read target memory if we are debugging native threads.

     Else, we're debugging a remote process, and should disassemble from the
     exec file for speed.  However, this is no good if the target modifies its
     code (for relocation, or whatever).
   */

  if (gdb_disassemble_from_exec == -1)
    {
      if (strcmp (target_shortname, "child") == 0
	  || strcmp (target_shortname, "procfs") == 0
	  || strcmp (target_shortname, "vxprocess") == 0
	  || strstr (target_shortname, "-threads") != NULL)
	gdb_disassemble_from_exec = 0;	/* It's a child process, read inferior mem */
      else
	gdb_disassemble_from_exec = 1;	/* It's remote, read the exec file */
    }

  if (gdb_disassemble_from_exec)
    di.read_memory_func = gdb_dis_asm_read_memory;
  else
    di.read_memory_func = dis_asm_read_memory;

  /* If just doing straight assembly, all we need to do is disassemble
     everything between low and high.  If doing mixed source/assembly,
     we've got a totally different path to follow.  */

  if (mixed_source_and_assembly)
    {
      /* Come here for mixed source/assembly */
      /* The idea here is to present a source-O-centric view of a
         function to the user.  This means that things are presented
         in source order, with (possibly) out of order assembly
         immediately following.  */
      struct symtab *symtab;
      struct linetable_entry *le;
      int nlines;
      int newlines;
      struct dis_line_entry *mle;
      struct symtab_and_line sal;
      int i;
      int out_of_order;
      int next_line;

      /* Assume symtab is valid for whole PC range */
      symtab = find_pc_symtab (low);

      if (!symtab || !symtab->linetable)
	goto assembly_only;

      /* First, convert the linetable to a bunch of my_line_entry's.  */

      le = symtab->linetable->item;
      nlines = symtab->linetable->nitems;

      if (nlines <= 0)
	goto assembly_only;

      mle = (struct dis_line_entry *) alloca (nlines * sizeof (struct dis_line_entry));

      out_of_order = 0;

      /* Copy linetable entries for this function into our data
         structure, creating end_pc's and setting out_of_order as
         appropriate.  */

      /* First, skip all the preceding functions.  */

      for (i = 0; i < nlines - 1 && le[i].pc < low; i++);

      /* Now, copy all entries before the end of this function.  */

      newlines = 0;
      for (; i < nlines - 1 && le[i].pc < high; i++)
	{
	  if (le[i].line == le[i + 1].line
	      && le[i].pc == le[i + 1].pc)
	    continue;		/* Ignore duplicates */

	  /* Skip any end-of-function markers.  */
	  if (le[i].line == 0)
	    continue;

	  mle[newlines].line = le[i].line;
	  if (le[i].line > le[i + 1].line)
	    out_of_order = 1;
	  mle[newlines].start_pc = le[i].pc;
	  mle[newlines].end_pc = le[i + 1].pc;
	  newlines++;
	}

      /* If we're on the last line, and it's part of the function,
         then we need to get the end pc in a special way.  */

      if (i == nlines - 1
	  && le[i].pc < high)
	{
	  mle[newlines].line = le[i].line;
	  mle[newlines].start_pc = le[i].pc;
	  sal = find_pc_line (le[i].pc, 0);
	  mle[newlines].end_pc = sal.end;
	  newlines++;
	}

      /* Now, sort mle by line #s (and, then by addresses within
         lines). */

      if (out_of_order)
	qsort (mle, newlines, sizeof (struct dis_line_entry), compare_lines);

      /* Now, for each line entry, emit the specified lines (unless
         they have been emitted before), followed by the assembly code
         for that line.  */

      next_line = 0;		/* Force out first line */
      ui_out_list_begin (uiout, "asm_insns");
      num_displayed = 0;
      for (i = 0; i < newlines; i++)
	{
	  int close_list = 1;
	  /* Print out everything from next_line to the current line.  */
	  if (mle[i].line >= next_line)
	    {
	      if (next_line != 0)
		{
		  /* Just one line to print. */
		  if (next_line == mle[i].line)
		    {
		      ui_out_tuple_begin (uiout, "src_and_asm_line");
		      print_source_lines (symtab, next_line, mle[i].line + 1, 0);
		    }
		  else
		    {
		      /* Several source lines w/o asm instructions associated. */
		      for (; next_line < mle[i].line; next_line++)
			{
			  ui_out_tuple_begin (uiout, "src_and_asm_line");
			  print_source_lines (symtab, next_line, mle[i].line + 1, 0);
			  ui_out_list_begin (uiout, "line_asm_insn");
			  ui_out_list_end (uiout);
			  ui_out_tuple_end (uiout);
			}
		      /* Print the last line and leave list open for
		         asm instructions to be added. */
		      ui_out_tuple_begin (uiout, "src_and_asm_line");
		      print_source_lines (symtab, next_line, mle[i].line + 1, 0);
		    }
		}
	      else
		{
		  ui_out_tuple_begin (uiout, "src_and_asm_line");
		  print_source_lines (symtab, mle[i].line, mle[i].line + 1, 0);
		}

	      next_line = mle[i].line + 1;
	      ui_out_list_begin (uiout, "line_asm_insn");
	      if (i + 1 < newlines && mle[i + 1].line <= mle[i].line)
		close_list = 0;
	    }
	  for (pc = mle[i].start_pc; pc < mle[i].end_pc;)
	    {
	      QUIT;
	      if (how_many >= 0)
		{
		  if (num_displayed >= how_many)
		    break;
		  else
		    num_displayed++;
		}
	      ui_out_tuple_begin (uiout, NULL);
	      ui_out_field_core_addr (uiout, "address", pc);

	      if (!build_address_symbolic (pc, 0, &name, &offset, &filename, &line, &unmapped))
		{
		  /* We don't care now about line, filename and
		     unmapped, but we might in the future. */
		  ui_out_field_string (uiout, "func-name", name);
		  ui_out_field_int (uiout, "offset", offset);
		}
	      if (filename != NULL)
		xfree (filename);
	      if (name != NULL)
		xfree (name);

	      ui_file_rewind (stb->stream);
	      pc += (*tm_print_insn) (pc, &di);
	      ui_out_field_stream (uiout, "inst", stb);
	      ui_file_rewind (stb->stream);
	      ui_out_tuple_end (uiout);
	    }
	  if (close_list)
	    {
	      ui_out_list_end (uiout);
	      ui_out_tuple_end (uiout);
	      close_list = 0;
	    }
	  if (how_many >= 0)
	    if (num_displayed >= how_many)
	      break;
	}
      ui_out_list_end (uiout);
    }
  else
    {
    assembly_only:
      ui_out_list_begin (uiout, "asm_insns");
      num_displayed = 0;
      for (pc = low; pc < high;)
	{
	  QUIT;
	  if (how_many >= 0)
	    {
	      if (num_displayed >= how_many)
		break;
	      else
		num_displayed++;
	    }
	  ui_out_tuple_begin (uiout, NULL);
	  ui_out_field_core_addr (uiout, "address", pc);

	  if (!build_address_symbolic (pc, 0, &name, &offset, &filename, &line, &unmapped))
	    {
	      /* We don't care now about line, filename and
	         unmapped. But we might in the future. */
	      ui_out_field_string (uiout, "func-name", name);
	      ui_out_field_int (uiout, "offset", offset);
	    }
	  if (filename != NULL)
	    xfree (filename);
	  if (name != NULL)
	    xfree (name);

	  ui_file_rewind (stb->stream);
	  pc += (*tm_print_insn) (pc, &di);
	  ui_out_field_stream (uiout, "inst", stb);
	  ui_file_rewind (stb->stream);
	  ui_out_tuple_end (uiout);
	}
      ui_out_list_end (uiout);
    }
  gdb_flush (gdb_stdout);

  return MI_CMD_DONE;
}
