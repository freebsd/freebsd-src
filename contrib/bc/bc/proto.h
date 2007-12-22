/* proto.h: Prototype function definitions for "external" functions. */

/*  This file is part of GNU bc.
    Copyright (C) 1991-1994, 1997, 2000 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
      The Free Software Foundation, Inc.
      59 Temple Place, Suite 330
      Boston, MA 02111 USA

    You may contact the author by:
       e-mail:  philnelson@acm.org
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

/* For the pc version using k&r ACK. (minix1.5 and earlier.) */
#ifdef SHORTNAMES
#define init_numbers i_numbers
#define push_constant push__constant
#define load_const in_load_const
#define yy_get_next_buffer yyget_next_buffer
#define yy_init_buffer yyinit_buffer
#define yy_last_accepting_state yylast_accepting_state
#define arglist1 arg1list
#endif

/* Include the standard library header files. */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* Define the _PROTOTYPE macro if it is needed. */

#ifndef _PROTOTYPE
#ifdef __STDC__
#define _PROTOTYPE(func, args) func args
#else
#define _PROTOTYPE(func, args) func()
#endif
#endif

/* From execute.c */
_PROTOTYPE(void stop_execution, (int));
_PROTOTYPE(unsigned char byte, (program_counter *pc));
_PROTOTYPE(void execute, (void));
_PROTOTYPE(char prog_char, (void));
_PROTOTYPE(char input_char, (void));
_PROTOTYPE(void push_constant, (char (*in_char)(void), int conv_base));
_PROTOTYPE(void push_b10_const, (program_counter *pc));
_PROTOTYPE(void assign, (int c_code));

/* From util.c */
_PROTOTYPE(char *strcopyof, (char *str));
_PROTOTYPE(arg_list *nextarg, (arg_list *args, int val, int is_var));
_PROTOTYPE(char *arg_str, (arg_list *args));
_PROTOTYPE(char *call_str, (arg_list *args));
_PROTOTYPE(void free_args, (arg_list *args));
_PROTOTYPE(void check_params, (arg_list *params, arg_list *autos));
_PROTOTYPE(void init_gen, (void));
_PROTOTYPE(void generate, (char *str));
_PROTOTYPE(void run_code, (void));
_PROTOTYPE(void out_char, (int ch));
_PROTOTYPE(void out_schar, (int ch));
_PROTOTYPE(id_rec *find_id, (id_rec *tree, char *id));
_PROTOTYPE(int insert_id_rec, (id_rec **root, id_rec *new_id));
_PROTOTYPE(void init_tree, (void));
_PROTOTYPE(int lookup, (char *name, int namekind));
_PROTOTYPE(char *bc_malloc, (int));
_PROTOTYPE(void out_of_memory, (void));
_PROTOTYPE(void welcome, (void));
_PROTOTYPE(void warranty, (char *));
_PROTOTYPE(void show_bc_version, (void));
_PROTOTYPE(void limits, (void));
_PROTOTYPE(void yyerror, (char *str ,...));
_PROTOTYPE(void warn, (char *mesg ,...));
_PROTOTYPE(void rt_error, (char *mesg ,...));
_PROTOTYPE(void rt_warn, (char *mesg ,...));

/* From load.c */
_PROTOTYPE(void init_load, (void));
_PROTOTYPE(void addbyte, (int byte));
_PROTOTYPE(void def_label, (long lab));
_PROTOTYPE(long long_val, (char **str));
_PROTOTYPE(void load_code, (char *code));

/* From main.c */
_PROTOTYPE(int open_new_file, (void));
_PROTOTYPE(void new_yy_file, (FILE *file));
_PROTOTYPE(void use_quit, (int));

/* From storage.c */
_PROTOTYPE(void init_storage, (void));
_PROTOTYPE(void more_functions, (void));
_PROTOTYPE(void more_variables, (void));
_PROTOTYPE(void more_arrays, (void));
_PROTOTYPE(void clear_func, (int func ));
_PROTOTYPE(int fpop, (void));
_PROTOTYPE(void fpush, (int val ));
_PROTOTYPE(void pop, (void));
_PROTOTYPE(void push_copy, (bc_num num ));
_PROTOTYPE(void push_num, (bc_num num ));
_PROTOTYPE(char check_stack, (int depth ));
_PROTOTYPE(bc_var *get_var, (int var_name ));
_PROTOTYPE(bc_num *get_array_num, (int var_index, long index ));
_PROTOTYPE(void store_var, (int var_name ));
_PROTOTYPE(void store_array, (int var_name ));
_PROTOTYPE(void load_var, (int var_name ));
_PROTOTYPE(void load_array, (int var_name ));
_PROTOTYPE(void decr_var, (int var_name ));
_PROTOTYPE(void decr_array, (int var_name ));
_PROTOTYPE(void incr_var, (int var_name ));
_PROTOTYPE(void incr_array, (int var_name ));
_PROTOTYPE(void auto_var, (int name ));
_PROTOTYPE(void free_a_tree, (bc_array_node *root, int depth ));
_PROTOTYPE(void pop_vars, (arg_list *list ));
_PROTOTYPE(void process_params, (program_counter *pc, int func ));

/* For the scanner and parser.... */
_PROTOTYPE(int yyparse, (void));
_PROTOTYPE(int yylex, (void)); 

#if defined(LIBEDIT)
/* The *?*&^ prompt function */
_PROTOTYPE(char *null_prompt, (EditLine *));
#endif

/* Other things... */
#ifndef HAVE_UNISTD_H
_PROTOTYPE (int getopt, (int, char *[], CONST char *));
#endif
