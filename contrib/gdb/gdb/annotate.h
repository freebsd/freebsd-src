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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

extern void breakpoints_changed PARAMS ((void));

extern void annotate_breakpoint PARAMS ((int));
extern void annotate_watchpoint PARAMS ((int));
extern void annotate_starting PARAMS ((void));
extern void annotate_stopped PARAMS ((void));
extern void annotate_exited PARAMS ((int));
extern void annotate_signalled PARAMS ((void));
extern void annotate_signal_name PARAMS ((void));
extern void annotate_signal_name_end PARAMS ((void));
extern void annotate_signal_string PARAMS ((void));
extern void annotate_signal_string_end PARAMS ((void));
extern void annotate_signal PARAMS ((void));

extern void annotate_breakpoints_headers PARAMS ((void));
extern void annotate_field PARAMS ((int));
extern void annotate_breakpoints_table PARAMS ((void));
extern void annotate_record PARAMS ((void));
extern void annotate_breakpoints_table_end PARAMS ((void));

extern void annotate_frames_invalid PARAMS ((void));

#ifdef __STDC__
struct type;
#endif

extern void annotate_field_begin PARAMS ((struct type *));
extern void annotate_field_name_end PARAMS ((void));
extern void annotate_field_value PARAMS ((void));
extern void annotate_field_end PARAMS ((void));

extern void annotate_quit PARAMS ((void));
extern void annotate_error PARAMS ((void));
extern void annotate_error_begin PARAMS ((void));

extern void annotate_value_history_begin PARAMS ((int, struct type *));
extern void annotate_value_begin PARAMS ((struct type *));
extern void annotate_value_history_value PARAMS ((void));
extern void annotate_value_history_end PARAMS ((void));
extern void annotate_value_end PARAMS ((void));

extern void annotate_display_begin PARAMS ((void));
extern void annotate_display_number_end PARAMS ((void));
extern void annotate_display_format PARAMS ((void));
extern void annotate_display_expression PARAMS ((void));
extern void annotate_display_expression_end PARAMS ((void));
extern void annotate_display_value PARAMS ((void));
extern void annotate_display_end PARAMS ((void));

extern void annotate_arg_begin PARAMS ((void));
extern void annotate_arg_name_end PARAMS ((void));
extern void annotate_arg_value PARAMS ((struct type *));
extern void annotate_arg_end PARAMS ((void));

extern void annotate_source PARAMS ((char *, int, int, int, CORE_ADDR));

extern void annotate_frame_begin PARAMS ((int, CORE_ADDR));
extern void annotate_function_call PARAMS ((void));
extern void annotate_signal_handler_caller PARAMS ((void));
extern void annotate_frame_address PARAMS ((void));
extern void annotate_frame_address_end PARAMS ((void));
extern void annotate_frame_function_name PARAMS ((void));
extern void annotate_frame_args PARAMS ((void));
extern void annotate_frame_source_begin PARAMS ((void));
extern void annotate_frame_source_file PARAMS ((void));
extern void annotate_frame_source_file_end PARAMS ((void));
extern void annotate_frame_source_line PARAMS ((void));
extern void annotate_frame_source_end PARAMS ((void));
extern void annotate_frame_where PARAMS ((void));
extern void annotate_frame_end PARAMS ((void));

extern void annotate_array_section_begin PARAMS ((int, struct type *));
extern void annotate_elt_rep PARAMS ((unsigned int));
extern void annotate_elt_rep_end PARAMS ((void));
extern void annotate_elt PARAMS ((void));
extern void annotate_array_section_end PARAMS ((void));
