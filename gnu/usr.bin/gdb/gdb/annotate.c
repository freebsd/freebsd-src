/* Annotation routines for GDB.
   Copyright 1986, 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "annotate.h"
#include "value.h"
#include "target.h"
#include "gdbtypes.h"

static void print_value_flags PARAMS ((struct type *));

static void
print_value_flags (t)
     struct type *t;
{
  if (can_dereference (t))
    printf_filtered ("*");
  else
    printf_filtered ("-");
}

void
breakpoints_changed ()
{
  if (annotation_level > 1)
    {
      target_terminal_ours ();
      printf_unfiltered ("\n\032\032breakpoints-invalid\n");
    }
}

void
annotate_breakpoint (num)
     int num;
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032breakpoint %d\n", num);
}

void
annotate_watchpoint (num)
     int num;
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032watchpoint %d\n", num);
}

void
annotate_starting ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032starting\n");
}

void
annotate_stopped ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032stopped\n");
}

void
annotate_exited (exitstatus)
     int exitstatus;
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032exited %d\n", exitstatus);
}

void
annotate_signalled ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signalled\n");
}

void
annotate_signal_name ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signal-name\n");
}

void
annotate_signal_name_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signal-name-end\n");
}

void
annotate_signal_string ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signal-string\n");
}

void
annotate_signal_string_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signal-string-end\n");
}

void
annotate_signal ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signal\n");
}

void
annotate_breakpoints_headers ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032breakpoints-headers\n");
}

void
annotate_field (num)
     int num;
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032field %d\n", num);
}

void
annotate_breakpoints_table ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032breakpoints-table\n");
}

void
annotate_record ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032record\n");
}

void
annotate_breakpoints_table_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032breakpoints-table-end\n");
}

void
annotate_frames_invalid ()
{
  if (annotation_level > 1)
    {
      target_terminal_ours ();
      printf_unfiltered ("\n\032\032frames-invalid\n");
    }
}

void
annotate_field_begin (type)
     struct type *type;
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032field-begin ");
      print_value_flags (type);
      printf_filtered ("\n");
    }
}

void
annotate_field_name_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032field-name-end\n");
}

void
annotate_field_value ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032field-value\n");
}

void
annotate_field_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032field-end\n");
}

void
annotate_quit ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032quit\n");
}

void
annotate_error ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032error\n");
}

void
annotate_error_begin ()
{
  if (annotation_level > 1)
    fprintf_filtered (gdb_stderr, "\n\032\032error-begin\n");
}

void
annotate_value_history_begin (histindex, type)
     int histindex;
     struct type *type;
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032value-history-begin %d ", histindex);
      print_value_flags (type);
      printf_filtered ("\n");
    }
}

void
annotate_value_begin (type)
     struct type *type;
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032value-begin ");
      print_value_flags (type);
      printf_filtered ("\n");
    }
}

void
annotate_value_history_value ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032value-history-value\n");
}

void
annotate_value_history_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032value-history-end\n");
}

void
annotate_value_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032value-end\n");
}

void
annotate_display_begin ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-begin\n");
}

void
annotate_display_number_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-number-end\n");
}

void
annotate_display_format ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-format\n");
}

void
annotate_display_expression ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-expression\n");
}

void
annotate_display_expression_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-expression-end\n");
}

void
annotate_display_value ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-value\n");
}

void
annotate_display_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032display-end\n");
}

void
annotate_arg_begin ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032arg-begin\n");
}

void
annotate_arg_name_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032arg-name-end\n");
}

void
annotate_arg_value (type)
     struct type *type;
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032arg-value ");
      print_value_flags (type);
      printf_filtered ("\n");
    }
}

void
annotate_arg_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032arg-end\n");
}

void
annotate_source (filename, line, character, mid, pc)
     char *filename;
     int line;
     int character;
     int mid;
     CORE_ADDR pc;
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032source ");
  else
    printf_filtered ("\032\032");

  printf_filtered ("%s:%d:%d:%s:", filename,
		   line, character,
		   mid ? "middle" : "beg");
  print_address_numeric (pc, 0, gdb_stdout);
  printf_filtered ("\n");
}

void
annotate_frame_begin (level, pc)
     int level;
     CORE_ADDR pc;
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032frame-begin %d ", level);
      print_address_numeric (pc, 0, gdb_stdout);
      printf_filtered ("\n");
    }
}

void
annotate_function_call ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032function-call\n");
}

void
annotate_signal_handler_caller ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032signal-handler-caller\n");
}

void
annotate_frame_address ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-address\n");
}

void
annotate_frame_address_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-address-end\n");
}

void
annotate_frame_function_name ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-function-name\n");
}

void
annotate_frame_args ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-args\n");
}

void
annotate_frame_source_begin ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-source-begin\n");
}

void
annotate_frame_source_file ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-source-file\n");
}

void
annotate_frame_source_file_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-source-file-end\n");
}

void
annotate_frame_source_line ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-source-line\n");
}

void
annotate_frame_source_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-source-end\n");
}

void
annotate_frame_where ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-where\n");
}

void
annotate_frame_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032frame-end\n");
}

void
annotate_array_section_begin (index, elttype)
     int index;
     struct type *elttype;
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032array-section-begin %d ", index);
      print_value_flags (elttype);
      printf_filtered ("\n");
    }
}

void
annotate_elt_rep (repcount)
     unsigned int repcount;
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032elt-rep %u\n", repcount);
}

void
annotate_elt_rep_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032elt-rep-end\n");
}

void
annotate_elt ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032elt\n");
}

void
annotate_array_section_end ()
{
  if (annotation_level > 1)
    printf_filtered ("\n\032\032array-section-end\n");
}

