/* execute.c - run a bc program. */

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

#include "bcdefs.h"
#include <signal.h>
#include "global.h"
#include "proto.h"


/* The SIGINT interrupt handling routine. */

int had_sigint;

void
stop_execution (sig)
     int sig;
{
  had_sigint = TRUE;
  printf ("\n");
  rt_error ("interrupted execution");
}


/* Get the current byte and advance the PC counter. */

unsigned char
byte (pc)
     program_counter *pc;
{
  int seg, offset;

  seg = pc->pc_addr >> BC_SEG_LOG;
  offset = pc->pc_addr++ % BC_SEG_SIZE;
  return (functions[pc->pc_func].f_body[seg][offset]);
}


/* The routine that actually runs the machine. */

void
execute ()
{
  int label_num, l_gp, l_off;
  bc_label_group *gp;

  char inst, ch;
  int  new_func;
  int  var_name;

  int const_base;

  bc_num temp_num;
  arg_list *auto_list;

  /* Initialize this run... */
  pc.pc_func = 0;
  pc.pc_addr = 0;
  runtime_error = FALSE;
  init_num (&temp_num);

  /* Set up the interrupt mechanism for an interactive session. */
  if (interactive)
    {
      signal (SIGINT, stop_execution);
      had_sigint = FALSE;
    }

  while (pc.pc_addr < functions[pc.pc_func].f_code_size && !runtime_error)
    {
      inst = byte(&pc);

#if DEBUG > 3
      { /* Print out address and the stack before each instruction.*/
	int depth; estack_rec *temp = ex_stack;

	printf ("func=%d addr=%d inst=%c\n",pc.pc_func, pc.pc_addr, inst);
	if (temp == NULL) printf ("empty stack.\n", inst);
	else
	  {
	    depth = 1;
	    while (temp != NULL)
	      {
		printf ("   %d = ", depth);
		out_num (temp->s_num, 10, out_char);
		depth++;
		temp = temp->s_next;
	      }
	  }
      }
#endif

    switch ( inst )
      {

      case 'A' : /* increment array variable (Add one). */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	incr_array (var_name);
	break;

      case 'B' : /* Branch to a label if TOS != 0. Remove value on TOS. */
      case 'Z' : /* Branch to a label if TOS == 0. Remove value on TOS. */
	c_code = !is_zero (ex_stack->s_num);
	pop ();
      case 'J' : /* Jump to a label. */
	label_num = byte(&pc);  /* Low order bits first. */
	label_num += byte(&pc) << 8;
	if (inst == 'J' || (inst == 'B' && c_code)
	    || (inst == 'Z' && !c_code)) {
	  gp = functions[pc.pc_func].f_label;
	  l_gp  = label_num >> BC_LABEL_LOG;
	  l_off = label_num % BC_LABEL_GROUP;
	  while (l_gp-- > 0) gp = gp->l_next;
	  pc.pc_addr = gp->l_adrs[l_off];
	}
	break;

      case 'C' : /* Call a function. */
	/* Get the function number. */
	new_func = byte(&pc);
	if ((new_func & 0x80) != 0)
	  new_func = ((new_func << 8) & 0x7f) + byte(&pc);

	/* Check to make sure it is defined. */
	if (!functions[new_func].f_defined)
	  {
	    rt_error ("Function %s not defined.", f_names[new_func]);
	    break;
	  }

	/* Check and push parameters. */
	process_params (&pc, new_func);

	/* Push auto variables. */
	for (auto_list = functions[new_func].f_autos;
	     auto_list != NULL;
	     auto_list = auto_list->next)
	  auto_var (auto_list->av_name);

	/* Push pc and ibase. */
	fpush (pc.pc_func);
	fpush (pc.pc_addr);
	fpush (i_base);

	/* Reset pc to start of function. */
	pc.pc_func = new_func;
	pc.pc_addr = 0;
	break;

      case 'D' : /* Duplicate top of stack */
	push_copy (ex_stack->s_num);
	break;

      case 'K' : /* Push a constant */
	/* Get the input base and convert it to a bc number. */
	if (pc.pc_func == 0)
	  const_base = i_base;
	else
	  const_base = fn_stack->s_val;
	if (const_base == 10)
	  push_b10_const (&pc);
	else
	  push_constant (prog_char, const_base);
	break;

      case 'L' : /* load array variable */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	load_array (var_name);
	break;

      case 'M' : /* decrement array variable (Minus!) */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	decr_array (var_name);
	break;

      case 'O' : /* Write a string to the output with processing. */
	while ((ch = byte(&pc)) != '"')
	  if (ch != '\\')
	    out_char (ch);
	  else
	    {
	      ch = byte(&pc);
	      if (ch == '"') break;
	      switch (ch)
		{
		case 'a':  out_char (007); break;
		case 'b':  out_char ('\b'); break;
		case 'f':  out_char ('\f'); break;
		case 'n':  out_char ('\n'); break;
		case 'q':  out_char ('"'); break;
		case 'r':  out_char ('\r'); break;
		case 't':  out_char ('\t'); break;
		case '\\': out_char ('\\'); break;
		default:  break;
		}
	    }
	if (interactive) fflush (stdout);
	break;

      case 'R' : /* Return from function */
	if (pc.pc_func != 0)
	  {
	    /* "Pop" autos and parameters. */
	    pop_vars(functions[pc.pc_func].f_autos);
	    pop_vars(functions[pc.pc_func].f_params);
	    /* reset the pc. */
	    fpop ();
	    pc.pc_addr = fpop ();
	    pc.pc_func = fpop ();
	  }
	else
	  rt_error ("Return from main program.");
	break;

      case 'S' : /* store array variable */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	store_array (var_name);
	break;

      case 'T' : /* Test tos for zero */
	c_code = is_zero (ex_stack->s_num);
	assign (c_code);
	break;

      case 'W' : /* Write the value on the top of the stack. */
      case 'P' : /* Write the value on the top of the stack.  No newline. */
	out_num (ex_stack->s_num, o_base, out_char);
	if (inst == 'W') out_char ('\n');
	store_var (3);  /* Special variable "last". */
	if (interactive) fflush (stdout);
	break;

      case 'c' : /* Call special function. */
	new_func = byte(&pc);

      switch (new_func)
	{
	case 'L':  /* Length function. */
	  /* For the number 0.xxxx,  0 is not significant. */
	  if (ex_stack->s_num->n_len == 1 &&
	      ex_stack->s_num->n_scale != 0 &&
	      ex_stack->s_num->n_value[0] == 0 )
	    int2num (&ex_stack->s_num, ex_stack->s_num->n_scale);
	  else
	    int2num (&ex_stack->s_num, ex_stack->s_num->n_len
		     + ex_stack->s_num->n_scale);
	  break;

	case 'S':  /* Scale function. */
	  int2num (&ex_stack->s_num, ex_stack->s_num->n_scale);
	  break;

	case 'R':  /* Square Root function. */
	  if (!bc_sqrt (&ex_stack->s_num, scale))
	    rt_error ("Square root of a negative number");
	  break;

	case 'I': /* Read function. */
	  push_constant (input_char, i_base);
	  break;
	}
	break;

      case 'd' : /* Decrement number */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	decr_var (var_name);
	break;

      case 'h' : /* Halt the machine. */
	exit (0);

      case 'i' : /* increment number */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	incr_var (var_name);
	break;

      case 'l' : /* load variable */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	load_var (var_name);
	break;

      case 'n' : /* Negate top of stack. */
	bc_sub (_zero_, ex_stack->s_num, &ex_stack->s_num);
	break;

      case 'p' : /* Pop the execution stack. */
	pop ();
	break;

      case 's' : /* store variable */
	var_name = byte(&pc);
	if ((var_name & 0x80) != 0)
	  var_name = ((var_name << 8) & 0x7f) + byte(&pc);
	store_var (var_name);
	break;

      case 'w' : /* Write a string to the output. */
	while ((ch = byte(&pc)) != '"') out_char (ch);
	if (interactive) fflush (stdout);
	break;

      case 'x' : /* Exchange Top of Stack with the one under the tos. */
	if (check_stack(2)) {
	  bc_num temp = ex_stack->s_num;
	  ex_stack->s_num = ex_stack->s_next->s_num;
	  ex_stack->s_next->s_num = temp;
	}
	break;

      case '0' : /* Load Constant 0. */
	push_copy (_zero_);
	break;

      case '1' : /* Load Constant 0. */
	push_copy (_one_);
	break;

      case '!' : /* Negate the boolean value on top of the stack. */
	c_code = is_zero (ex_stack->s_num);
	assign (c_code);
	break;

      case '&' : /* compare greater than */
	if (check_stack(2))
	  {
	    c_code = !is_zero (ex_stack->s_next->s_num)
	      && !is_zero (ex_stack->s_num);
	    pop ();
	    assign (c_code);
	  }
	break;

      case '|' : /* compare greater than */
	if (check_stack(2))
	  {
	    c_code = !is_zero (ex_stack->s_next->s_num)
	      || !is_zero (ex_stack->s_num);
	    pop ();
	    assign (c_code);
	  }
	break;

      case '+' : /* add */
	if (check_stack(2))
	  {
	    bc_add (ex_stack->s_next->s_num, ex_stack->s_num, &temp_num);
	    pop();
	    pop();
	    push_num (temp_num);
	    init_num (&temp_num);
	  }
	break;

      case '-' : /* subtract */
	if (check_stack(2))
	  {
	    bc_sub (ex_stack->s_next->s_num, ex_stack->s_num, &temp_num);
	    pop();
	    pop();
	    push_num (temp_num);
	    init_num (&temp_num);
	  }
	break;

      case '*' : /* multiply */
	if (check_stack(2))
	  {
	    bc_multiply (ex_stack->s_next->s_num, ex_stack->s_num,
			 &temp_num, scale);
	    pop();
	    pop();
	    push_num (temp_num);
	    init_num (&temp_num);
	  }
	break;

      case '/' : /* divide */
	if (check_stack(2))
	  {
	    if (bc_divide (ex_stack->s_next->s_num,
			   ex_stack->s_num, &temp_num, scale) == 0)
	      {
		pop();
		pop();
		push_num (temp_num);
		init_num (&temp_num);
	      }
	    else
	      rt_error ("Divide by zero");
	  }
	break;

      case '%' : /* remainder */
	if (check_stack(2))
	  {
	    if (is_zero (ex_stack->s_num))
	      rt_error ("Modulo by zero");
	    else
	      {
		bc_modulo (ex_stack->s_next->s_num,
			   ex_stack->s_num, &temp_num, scale);
		pop();
		pop();
		push_num (temp_num);
		init_num (&temp_num);
	      }
	  }
	break;

      case '^' : /* raise */
	if (check_stack(2))
	  {
	    bc_raise (ex_stack->s_next->s_num,
		      ex_stack->s_num, &temp_num, scale);
	    if (is_zero (ex_stack->s_next->s_num) && is_neg (ex_stack->s_num))
	      rt_error ("divide by zero");
	    pop();
	    pop();
	    push_num (temp_num);
	    init_num (&temp_num);
	  }
	break;

      case '=' : /* compare equal */
	if (check_stack(2))
	  {
	    c_code = bc_compare (ex_stack->s_next->s_num,
				 ex_stack->s_num) == 0;
	    pop ();
	    assign (c_code);
	  }
	break;

      case '#' : /* compare not equal */
	if (check_stack(2))
	  {
	    c_code = bc_compare (ex_stack->s_next->s_num,
				 ex_stack->s_num) != 0;
	    pop ();
	    assign (c_code);
	  }
	break;

      case '<' : /* compare less than */
	if (check_stack(2))
	  {
	    c_code = bc_compare (ex_stack->s_next->s_num,
				 ex_stack->s_num) == -1;
	    pop ();
	    assign (c_code);
	  }
	break;

      case '{' : /* compare less than or equal */
	if (check_stack(2))
	  {
	    c_code = bc_compare (ex_stack->s_next->s_num,
				 ex_stack->s_num) <= 0;
	    pop ();
	    assign (c_code);
	  }
	break;

      case '>' : /* compare greater than */
	if (check_stack(2))
	  {
	    c_code = bc_compare (ex_stack->s_next->s_num,
				 ex_stack->s_num) == 1;
	    pop ();
	    assign (c_code);
	  }
	break;

      case '}' : /* compare greater than or equal */
	if (check_stack(2))
	  {
	    c_code = bc_compare (ex_stack->s_next->s_num,
				 ex_stack->s_num) >= 0;
	    pop ();
	    assign (c_code);
	  }
	break;

	default  : /* error! */
	  rt_error ("bad instruction: inst=%c", inst);
      }
    }

  /* Clean up the function stack and pop all autos/parameters. */
  while (pc.pc_func != 0)
    {
      pop_vars(functions[pc.pc_func].f_autos);
      pop_vars(functions[pc.pc_func].f_params);
      fpop ();
      pc.pc_addr = fpop ();
      pc.pc_func = fpop ();
    }

  /* Clean up the execution stack. */
  while (ex_stack != NULL) pop();

  /* Clean up the interrupt stuff. */
  if (interactive)
    {
      signal (SIGINT, use_quit);
      if (had_sigint)
	printf ("Interruption completed.\n");
    }
}


/* Prog_char gets another byte from the program.  It is used for
   conversion of text constants in the code to numbers. */

char
prog_char ()
{
  return byte(&pc);
}


/* Read a character from the standard input.  This function is used
   by the "read" function. */

char
input_char ()
{
  char in_ch;

  /* Get a character from the standard input for the read function. */
  in_ch = getchar();

  /* Check for a \ quoted newline. */
  if (in_ch == '\\')
    {
      in_ch = getchar();
      if (in_ch == '\n')
	in_ch = getchar();
    }

  /* Classify and preprocess the input character. */
  if (isdigit(in_ch))
    return (in_ch - '0');
  if (in_ch >= 'A' && in_ch <= 'F')
    return (in_ch + 10 - 'A');
  if (in_ch >= 'a' && in_ch <= 'f')
    return (in_ch + 10 - 'a');
  if (in_ch == '.' || in_ch == '+' || in_ch == '-')
    return (in_ch);
  if (in_ch <= ' ')
    return (' ');

  return (':');
}


/* Push_constant converts a sequence of input characters as returned
   by IN_CHAR into a number.  The number is pushed onto the execution
   stack.  The number is converted as a number in base CONV_BASE. */

void
push_constant (in_char, conv_base)
   char (*in_char)(VOID);
   int conv_base;
{
  int digits;
  bc_num build, temp, result, mult, divisor;
  char  in_ch, first_ch;
  char  negative;

  /* Initialize all bc numbers */
  init_num (&temp);
  init_num (&result);
  init_num (&mult);
  build = copy_num (_zero_);
  negative = FALSE;

  /* The conversion base. */
  int2num (&mult, conv_base);

  /* Get things ready. */
  in_ch = in_char();
  while (in_ch == ' ')
    in_ch = in_char();

  if (in_ch == '+')
    in_ch = in_char();
  else
    if (in_ch == '-')
      {
	negative = TRUE;
	in_ch = in_char();
      }

  /* Check for the special case of a single digit. */
  if (in_ch < 16)
    {
      first_ch = in_ch;
      in_ch = in_char();
      if (in_ch < 16 && first_ch >= conv_base)
	first_ch = conv_base - 1;
      int2num (&build, (int) first_ch);
    }

  /* Convert the integer part. */
  while (in_ch < 16)
    {
      if (in_ch < 16 && in_ch >= conv_base) in_ch = conv_base-1;
      bc_multiply (build, mult, &result, 0);
      int2num (&temp, (int) in_ch);
      bc_add (result, temp, &build);
      in_ch = in_char();
    }
  if (in_ch == '.')
    {
      in_ch = in_char();
      if (in_ch >= conv_base) in_ch = conv_base-1;
      free_num (&result);
      free_num (&temp);
      divisor = copy_num (_one_);
      result = copy_num (_zero_);
      digits = 0;
      while (in_ch < 16)
	{
	  bc_multiply (result, mult, &result, 0);
	  int2num (&temp, (int) in_ch);
	  bc_add (result, temp, &result);
	  bc_multiply (divisor, mult, &divisor, 0);
	  digits++;
	  in_ch = in_char();
	  if (in_ch < 16 && in_ch >= conv_base) in_ch = conv_base-1;
	}
      bc_divide (result, divisor, &result, digits);
      bc_add (build, result, &build);
    }

  /* Final work.  */
  if (negative)
    bc_sub (_zero_, build, &build);

  push_num (build);
  free_num (&temp);
  free_num (&result);
  free_num (&mult);
}


/* When converting base 10 constants from the program, we use this
   more efficient way to convert them to numbers.  PC tells where
   the constant starts and is expected to be advanced to after
   the constant. */

void
push_b10_const (pc)
     program_counter *pc;
{
  bc_num build;
  program_counter look_pc;
  int kdigits, kscale;
  char inchar;
  char *ptr;

  /* Count the digits and get things ready. */
  look_pc = *pc;
  kdigits = 0;
  kscale  = 0;
  inchar = byte (&look_pc);
  while (inchar != '.' && inchar != ':')
    {
      kdigits++;
      inchar = byte(&look_pc);
    }
  if (inchar == '.' )
    {
      inchar = byte(&look_pc);
      while (inchar != ':')
	{
	  kscale++;
	  inchar = byte(&look_pc);
	}
    }

  /* Get the first character again and move the pc. */
  inchar = byte(pc);

  /* Secial cases of 0, 1, and A-F single inputs. */
  if (kdigits == 1 && kscale == 0)
    {
      if (inchar == 0)
	{
	  push_copy (_zero_);
	  inchar = byte(pc);
	  return;
	}
      if (inchar == 1) {
      push_copy (_one_);
      inchar = byte(pc);
      return;
    }
    if (inchar > 9)
      {
	init_num (&build);
	int2num (&build, inchar);
	push_num (build);
	inchar = byte(pc);
	return;
      }
    }

  /* Build the new number. */
  if (kdigits == 0)
    {
      build = new_num (1,kscale);
      ptr = build->n_value;
      *ptr++ = 0;
    }
  else
    {
      build = new_num (kdigits,kscale);
      ptr = build->n_value;
    }

  while (inchar != ':')
    {
      if (inchar != '.')
	if (inchar > 9)
	  *ptr++ = 9;
	else
	  *ptr++ = inchar;
      inchar = byte(pc);
    }
  push_num (build);
}


/* Put the correct value on the stack for C_CODE.  Frees TOS num. */

void
assign (c_code)
     char c_code;
{
  free_num (&ex_stack->s_num);
  if (c_code)
    ex_stack->s_num = copy_num (_one_);
  else
    ex_stack->s_num = copy_num (_zero_);
}
