/* proto.h: Prototype function definitions for "external" functions. */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

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
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
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
#include <unistd.h>
#include <stdlib.h>

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
_PROTOTYPE(arg_list *nextarg, (arg_list *args, int val));
_PROTOTYPE(char *arg_str, (arg_list *args));
_PROTOTYPE(char *call_str, (arg_list *args));
_PROTOTYPE(void free_args, (arg_list *args));
_PROTOTYPE(void check_params, (arg_list *params, arg_list *autos));
_PROTOTYPE(void init_gen, (void));
_PROTOTYPE(void generate, (char *str));
_PROTOTYPE(void run_code, (void));
_PROTOTYPE(void out_char, (int ch));
_PROTOTYPE(id_rec *find_id, (id_rec *tree, char *id));
_PROTOTYPE(int insert_id_rec, (id_rec **root, id_rec *new_id));
_PROTOTYPE(void init_tree, (void));
_PROTOTYPE(int lookup, (char *name, int namekind));
_PROTOTYPE(char *bc_malloc, (int));
_PROTOTYPE(void out_of_memory, (void));
_PROTOTYPE(void welcome, (void));
_PROTOTYPE(void warranty, (char *));
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
_PROTOTYPE(int main, (int argc , char *argv []));
_PROTOTYPE(int open_new_file, (void));
_PROTOTYPE(void new_yy_file, (FILE *file));
_PROTOTYPE(void use_quit, (int));

/* From number.c */
_PROTOTYPE(void free_num, (bc_num *num));
_PROTOTYPE(bc_num new_num, (int length, int scale));
_PROTOTYPE(void init_numbers, (void));
_PROTOTYPE(bc_num copy_num, (bc_num num));
_PROTOTYPE(void init_num, (bc_num *num));
_PROTOTYPE(void str2num, (bc_num *num, char *str, int scale));
_PROTOTYPE(char *num2str, (bc_num num));
_PROTOTYPE(void int2num, (bc_num *num, int val));
_PROTOTYPE(long num2long, (bc_num num));
_PROTOTYPE(int bc_compare, (bc_num n1, bc_num n2));
_PROTOTYPE(char is_zero, (bc_num num));
_PROTOTYPE(char is_neg, (bc_num num));
_PROTOTYPE(void bc_add, (bc_num n1, bc_num n2, bc_num *result));
_PROTOTYPE(void bc_sub, (bc_num n1, bc_num n2, bc_num *result));
_PROTOTYPE(void bc_multiply, (bc_num n1, bc_num n2, bc_num *prod, int scale));
_PROTOTYPE(int bc_divide, (bc_num n1, bc_num n2, bc_num *quot, int scale));
_PROTOTYPE(int bc_modulo, (bc_num num1, bc_num num2, bc_num *result, int scale));
_PROTOTYPE(void bc_raise, (bc_num num1, bc_num num2, bc_num *result, int scale));
_PROTOTYPE(int bc_sqrt, (bc_num *num, int scale));
_PROTOTYPE(void out_long, (long val, int size, int space,
			   void (*out_char)(int)));
_PROTOTYPE(void out_num, (bc_num num, int o_base, void (* out_char)(int)));


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

/* Other things... */
_PROTOTYPE (int getopt, (int, char *[], CONST char *));

