/* storage.c:  Code and data storage manipulations.  This includes labels. */

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
#include "global.h"
#include "proto.h"


/* Initialize the storage at the beginning of the run. */

void
init_storage ()
{

  /* Functions: we start with none and ask for more. */
  f_count = 0;
  more_functions ();
  f_names[0] = "(main)";

  /* Variables. */
  v_count = 0;
  more_variables ();

  /* Arrays. */
  a_count = 0;
  more_arrays ();

  /* Other things... */
  ex_stack = NULL;
  fn_stack = NULL;
  i_base = 10;
  o_base = 10;
  scale  = 0;
  c_code = FALSE;
  init_numbers();
}

/* Three functions for increasing the number of functions, variables, or
   arrays that are needed.  This adds another 32 of the requested object. */

void
more_functions (VOID)
{
  int old_count;
  int indx1, indx2;
  bc_function *old_f;
  bc_function *f;
  char **old_names;

  /* Save old information. */
  old_count = f_count;
  old_f = functions;
  old_names = f_names;

  /* Add a fixed amount and allocate new space. */
  f_count += STORE_INCR;
  functions = (bc_function *) bc_malloc (f_count*sizeof (bc_function));
  f_names = (char **) bc_malloc (f_count*sizeof (char *));

  /* Copy old ones. */
  for (indx1 = 0; indx1 < old_count; indx1++)
    {
      functions[indx1] = old_f[indx1];
      f_names[indx1] = old_names[indx1];
    }

  /* Initialize the new ones. */
  for (; indx1 < f_count; indx1++)
    {
      f = &functions[indx1];
      f->f_defined = FALSE;
      for (indx2 = 0; indx2 < BC_MAX_SEGS; indx2++)
	f->f_body [indx2] = NULL;
      f->f_code_size = 0;
      f->f_label = NULL;
      f->f_autos = NULL;
      f->f_params = NULL;
    }

  /* Free the old elements. */
  if (old_count != 0)
    {
      free (old_f);
      free (old_names);
    }
}

void
more_variables ()
{
  int indx;
  int old_count;
  bc_var **old_var;
  char **old_names;

  /* Save the old values. */
  old_count = v_count;
  old_var = variables;
  old_names = v_names;

  /* Increment by a fixed amount and allocate. */
  v_count += STORE_INCR;
  variables = (bc_var **) bc_malloc (v_count*sizeof(bc_var *));
  v_names = (char **) bc_malloc (v_count*sizeof(char *));

  /* Copy the old variables. */
  for (indx = 3; indx < old_count; indx++)
    variables[indx] = old_var[indx];

  /* Initialize the new elements. */
  for (; indx < v_count; indx++)
    variables[indx] = NULL;

  /* Free the old elements. */
  if (old_count != 0)
    {
      free (old_var);
      free (old_names);
    }
}

void
more_arrays ()
{
  int indx;
  int old_count;
  bc_var_array **old_ary;
  char **old_names;

  /* Save the old values. */
  old_count = a_count;
  old_ary = arrays;
  old_names = a_names;

  /* Increment by a fixed amount and allocate. */
  a_count += STORE_INCR;
  arrays = (bc_var_array **) bc_malloc (a_count*sizeof(bc_var_array *));
  a_names = (char **) bc_malloc (a_count*sizeof(char *));

  /* Copy the old arrays. */
  for (indx = 1; indx < old_count; indx++)
    arrays[indx] = old_ary[indx];


  /* Initialize the new elements. */
  for (; indx < v_count; indx++)
    arrays[indx] = NULL;

  /* Free the old elements. */
  if (old_count != 0)
    {
      free (old_ary);
      free (old_names);
    }
}


/* clear_func clears out function FUNC and makes it ready to redefine. */

void
clear_func (func)
     char func;
{
  bc_function *f;
  int indx;
  bc_label_group *lg;

  /* Set the pointer to the function. */
  f = &functions[func];
  f->f_defined = FALSE;

  /* Clear the code segments. */
  for (indx = 0; indx < BC_MAX_SEGS; indx++)
    {
      if (f->f_body[indx] != NULL)
	{
	  free (f->f_body[indx]);
	  f->f_body[indx] = NULL;
	}
    }

  f->f_code_size = 0;
  if (f->f_autos != NULL)
    {
      free_args (f->f_autos);
      f->f_autos = NULL;
    }
  if (f->f_params != NULL)
    {
      free_args (f->f_params);
      f->f_params = NULL;
    }
  while (f->f_label != NULL)
    {
      lg = f->f_label->l_next;
      free (f->f_label);
      f->f_label = lg;
    }
}


/*  Pop the function execution stack and return the top. */

int
fpop()
{
  fstack_rec *temp;
  int retval;

  if (fn_stack != NULL)
    {
      temp = fn_stack;
      fn_stack = temp->s_next;
      retval = temp->s_val;
      free (temp);
    }
  return (retval);
}


/* Push VAL on to the function stack. */

void
fpush (val)
     int val;
{
  fstack_rec *temp;

  temp = (fstack_rec *) bc_malloc (sizeof (fstack_rec));
  temp->s_next = fn_stack;
  temp->s_val = val;
  fn_stack = temp;
}


/* Pop and discard the top element of the regular execution stack. */

void
pop ()
{
  estack_rec *temp;

  if (ex_stack != NULL)
    {
      temp = ex_stack;
      ex_stack = temp->s_next;
      free_num (&temp->s_num);
      free (temp);
    }
}


/* Push a copy of NUM on to the regular execution stack. */

void
push_copy (num)
     bc_num num;
{
  estack_rec *temp;

  temp = (estack_rec *) bc_malloc (sizeof (estack_rec));
  temp->s_num = copy_num (num);
  temp->s_next = ex_stack;
  ex_stack = temp;
}


/* Push NUM on to the regular execution stack.  Do NOT push a copy. */

void
push_num (num)
     bc_num num;
{
  estack_rec *temp;

  temp = (estack_rec *) bc_malloc (sizeof (estack_rec));
  temp->s_num = num;
  temp->s_next = ex_stack;
  ex_stack = temp;
}


/* Make sure the ex_stack has at least DEPTH elements on it.
   Return TRUE if it has at least DEPTH elements, otherwise
   return FALSE. */

char
check_stack (depth)
     int depth;
{
  estack_rec *temp;

  temp = ex_stack;
  while ((temp != NULL) && (depth > 0))
    {
      temp = temp->s_next;
      depth--;
    }
  if (depth > 0)
    {
      rt_error ("Stack error.");
      return FALSE;
    }
  return TRUE;
}


/* The following routines manipulate simple variables and
   array variables. */

/* get_var returns a pointer to the variable VAR_NAME.  If one does not
   exist, one is created. */

bc_var *
get_var (var_name)
     int var_name;
{
  bc_var *var_ptr;

  var_ptr = variables[var_name];
  if (var_ptr == NULL)
    {
      var_ptr = variables[var_name] = (bc_var *) bc_malloc (sizeof (bc_var));
      init_num (&var_ptr->v_value);
    }
  return var_ptr;
}


/* get_array_num returns the address of the bc_num in the array
   structure.  If more structure is requried to get to the index,
   this routine does the work to create that structure. VAR_INDEX
   is a zero based index into the arrays storage array. INDEX is
   the index into the bc array. */

bc_num *
get_array_num (var_index, index)
     int var_index;
     long  index;
{
  bc_var_array *ary_ptr;
  bc_array *a_var;
  bc_array_node *temp;
  int log, ix, ix1;
  int sub [NODE_DEPTH];

  /* Get the array entry. */
  ary_ptr = arrays[var_index];
  if (ary_ptr == NULL)
    {
      ary_ptr = arrays[var_index] =
	(bc_var_array *) bc_malloc (sizeof (bc_var_array));
      ary_ptr->a_value = NULL;
      ary_ptr->a_next = NULL;
      ary_ptr->a_param = FALSE;
    }

  a_var = ary_ptr->a_value;
  if (a_var == NULL) {
    a_var = ary_ptr->a_value = (bc_array *) bc_malloc (sizeof (bc_array));
    a_var->a_tree = NULL;
    a_var->a_depth = 0;
  }

  /* Get the index variable. */
  sub[0] = index & NODE_MASK;
  ix = index >> NODE_SHIFT;
  log = 1;
  while (ix > 0 || log < a_var->a_depth)
    {
      sub[log] = ix & NODE_MASK;
      ix >>= NODE_SHIFT;
      log++;
    }

  /* Build any tree that is necessary. */
  while (log > a_var->a_depth)
    {
      temp = (bc_array_node *) bc_malloc (sizeof(bc_array_node));
      if (a_var->a_depth != 0)
	{
	  temp->n_items.n_down[0] = a_var->a_tree;
	  for (ix=1; ix < NODE_SIZE; ix++)
	    temp->n_items.n_down[ix] = NULL;
	}
      else
	{
	  for (ix=0; ix < NODE_SIZE; ix++)
	    temp->n_items.n_num[ix] = copy_num(_zero_);
	}
      a_var->a_tree = temp;
      a_var->a_depth++;
    }

  /* Find the indexed variable. */
  temp = a_var->a_tree;
  while ( log-- > 1)
    {
      ix1 = sub[log];
      if (temp->n_items.n_down[ix1] == NULL)
	{
	  temp->n_items.n_down[ix1] =
	    (bc_array_node *) bc_malloc (sizeof(bc_array_node));
	  temp = temp->n_items.n_down[ix1];
	  if (log > 1)
	    for (ix=0; ix < NODE_SIZE; ix++)
	      temp->n_items.n_down[ix] = NULL;
	  else
	    for (ix=0; ix < NODE_SIZE; ix++)
	      temp->n_items.n_num[ix] = copy_num(_zero_);
	}
      else
	temp = temp->n_items.n_down[ix1];
    }

  /* Return the address of the indexed variable. */
  return &(temp->n_items.n_num[sub[0]]);
}


/* Store the top of the execution stack into VAR_NAME.
   This includes the special variables ibase, obase, and scale. */

void
store_var (var_name)
     int var_name;
{
  bc_var *var_ptr;
  long temp;
  char toobig;

  if (var_name > 2)
    {
      /* It is a simple variable. */
      var_ptr = get_var (var_name);
      if (var_ptr != NULL)
	{
	  free_num(&var_ptr->v_value);
	  var_ptr->v_value = copy_num (ex_stack->s_num);
	}
    }
  else
    {
      /* It is a special variable... */
      toobig = FALSE;
      if (is_neg (ex_stack->s_num))
	{
	  switch (var_name)
	    {
	    case 0:
	      rt_warn ("negative ibase, set to 2");
	      temp = 2;
	      break;
	    case 1:
	      rt_warn ("negative obase, set to 2");
	      temp = 2;
	      break;
	    case 2:
	      rt_warn ("negative scale, set to 0");
	      temp = 0;
	      break;
	    }
	}
      else
	{
	  temp = num2long (ex_stack->s_num);
	  if (!is_zero (ex_stack->s_num) && temp == 0)
	    toobig = TRUE;
	}
      switch (var_name)
	{
	case 0:
	  if (temp < 2 && !toobig)
	    {
	      i_base = 2;
	      rt_warn ("ibase too small, set to 2");
	    }
	  else
	    if (temp > 16 || toobig)
	      {
		i_base = 16;
		rt_warn ("ibase too large, set to 16");
	      }
	    else
	      i_base = (int) temp;
	  break;

	case 1:
	  if (temp < 2 && !toobig)
	    {
	      o_base = 2;
	      rt_warn ("obase too small, set to 2");
	    }
	  else
	    if (temp > BC_BASE_MAX || toobig)
	      {
		o_base = BC_BASE_MAX;
		rt_warn ("obase too large, set to %d", BC_BASE_MAX);
	      }
	    else
	      o_base = (int) temp;
	  break;

	case 2:
	  /*  WARNING:  The following if statement may generate a compiler
	      warning if INT_MAX == LONG_MAX.  This is NOT a problem. */
	  if (temp > BC_SCALE_MAX || toobig )
	    {
	      scale = BC_SCALE_MAX;
	      rt_warn ("scale too large, set to %d", BC_SCALE_MAX);
	    }
	  else
	    scale = (int) temp;
	}
    }
}


/* Store the top of the execution stack into array VAR_NAME.
   VAR_NAME is the name of an array, and the next to the top
   of stack for the index into the array. */

void
store_array (var_name)
     int var_name;
{
  bc_num *num_ptr;
  long index;

  if (!check_stack(2)) return;
  index = num2long (ex_stack->s_next->s_num);
  if (index < 0 || index > BC_DIM_MAX ||
      (index == 0 && !is_zero(ex_stack->s_next->s_num)))
    rt_error ("Array %s subscript out of bounds.", a_names[var_name]);
  else
    {
      num_ptr = get_array_num (var_name, index);
      if (num_ptr != NULL)
	{
	  free_num (num_ptr);
	  *num_ptr = copy_num (ex_stack->s_num);
	  free_num (&ex_stack->s_next->s_num);
	  ex_stack->s_next->s_num = ex_stack->s_num;
	  init_num (&ex_stack->s_num);
	  pop();
	}
    }
}


/*  Load a copy of VAR_NAME on to the execution stack.  This includes
    the special variables ibase, obase and scale.  */

void
load_var (var_name)
     int var_name;
{
  bc_var *var_ptr;

  switch (var_name)
    {

    case 0:
      /* Special variable ibase. */
      push_copy (_zero_);
      int2num (&ex_stack->s_num, i_base);
      break;

    case 1:
      /* Special variable obase. */
      push_copy (_zero_);
      int2num (&ex_stack->s_num, o_base);
      break;

    case 2:
      /* Special variable scale. */
      push_copy (_zero_);
      int2num (&ex_stack->s_num, scale);
      break;

    default:
      /* It is a simple variable. */
      var_ptr = variables[var_name];
      if (var_ptr != NULL)
	push_copy (var_ptr->v_value);
      else
	push_copy (_zero_);
    }
}


/*  Load a copy of VAR_NAME on to the execution stack.  This includes
    the special variables ibase, obase and scale.  */

void
load_array (var_name)
     int var_name;
{
  bc_num *num_ptr;
  long   index;

  if (!check_stack(1)) return;
  index = num2long (ex_stack->s_num);
  if (index < 0 || index > BC_DIM_MAX ||
     (index == 0 && !is_zero(ex_stack->s_num)))
    rt_error ("Array %s subscript out of bounds.", a_names[var_name]);
  else
    {
      num_ptr = get_array_num (var_name, index);
      if (num_ptr != NULL)
	{
	  pop();
	  push_copy (*num_ptr);
	}
    }
}


/* Decrement VAR_NAME by one.  This includes the special variables
   ibase, obase, and scale. */

void
decr_var (var_name)
     int var_name;
{
  bc_var *var_ptr;

  switch (var_name)
    {

    case 0: /* ibase */
      if (i_base > 2)
	i_base--;
      else
	rt_warn ("ibase too small in --");
      break;

    case 1: /* obase */
      if (o_base > 2)
	o_base--;
      else
	rt_warn ("obase too small in --");
      break;

    case 2: /* scale */
      if (scale > 0)
	scale--;
      else
	rt_warn ("scale can not be negative in -- ");
      break;

    default: /* It is a simple variable. */
      var_ptr = get_var (var_name);
      if (var_ptr != NULL)
	bc_sub (var_ptr->v_value,_one_,&var_ptr->v_value);
    }
}


/* Decrement VAR_NAME by one.  VAR_NAME is an array, and the top of
   the execution stack is the index and it is popped off the stack. */

void
decr_array (var_name)
     char var_name;
{
  bc_num *num_ptr;
  long   index;

  /* It is an array variable. */
  if (!check_stack (1)) return;
  index = num2long (ex_stack->s_num);
  if (index < 0 || index > BC_DIM_MAX ||
     (index == 0 && !is_zero (ex_stack->s_num)))
    rt_error ("Array %s subscript out of bounds.", a_names[var_name]);
  else
    {
      num_ptr = get_array_num (var_name, index);
      if (num_ptr != NULL)
	{
	  pop ();
	  bc_sub (*num_ptr, _one_, num_ptr);
	}
    }
}


/* Increment VAR_NAME by one.  This includes the special variables
   ibase, obase, and scale. */

void
incr_var (var_name)
     int var_name;
{
  bc_var *var_ptr;

  switch (var_name)
    {

    case 0: /* ibase */
      if (i_base < 16)
	i_base++;
      else
	rt_warn ("ibase too big in ++");
      break;

    case 1: /* obase */
      if (o_base < BC_BASE_MAX)
	o_base++;
      else
	rt_warn ("obase too big in ++");
      break;

    case 2:
      if (scale < BC_SCALE_MAX)
	scale++;
      else
	rt_warn ("Scale too big in ++");
      break;

    default:  /* It is a simple variable. */
      var_ptr = get_var (var_name);
      if (var_ptr != NULL)
	bc_add (var_ptr->v_value, _one_, &var_ptr->v_value);

    }
}


/* Increment VAR_NAME by one.  VAR_NAME is an array and top of
   execution stack is the index and is popped off the stack. */

void
incr_array (var_name)
     int var_name;
{
  bc_num *num_ptr;
  long   index;

  if (!check_stack (1)) return;
  index = num2long (ex_stack->s_num);
  if (index < 0 || index > BC_DIM_MAX ||
      (index == 0 && !is_zero (ex_stack->s_num)))
    rt_error ("Array %s subscript out of bounds.", a_names[var_name]);
  else
    {
      num_ptr = get_array_num (var_name, index);
      if (num_ptr != NULL)
	{
	  pop ();
	  bc_add (*num_ptr, _one_, num_ptr);
	}
    }
}


/* Routines for processing autos variables and parameters. */

/* NAME is an auto variable that needs to be pushed on its stack. */

void
auto_var (name)
     int name;
{
  bc_var *v_temp;
  bc_var_array *a_temp;
  int ix;

  if (name > 0)
    {
      /* A simple variable. */
      ix = name;
      v_temp = (bc_var *) bc_malloc (sizeof (bc_var));
      v_temp->v_next = variables[ix];
      init_num (&v_temp->v_value);
      variables[ix] = v_temp;
    }
  else
    {
      /* An array variable. */
      ix = -name;
      a_temp = (bc_var_array *) bc_malloc (sizeof (bc_var_array));
      a_temp->a_next = arrays[ix];
      a_temp->a_value = NULL;
      a_temp->a_param = FALSE;
      arrays[ix] = a_temp;
    }
}


/* Free_a_tree frees everything associated with an array variable tree.
   This is used when popping an array variable off its auto stack.  */

void
free_a_tree ( root, depth )
     bc_array_node *root;
     int depth;
{
  int ix;

  if (root != NULL)
    {
      if (depth > 1)
	for (ix = 0; ix < NODE_SIZE; ix++)
	  free_a_tree (root->n_items.n_down[ix], depth-1);
      else
	for (ix = 0; ix < NODE_SIZE; ix++)
	  free_num ( &(root->n_items.n_num[ix]));
      free (root);
    }
}


/* LIST is an NULL terminated list of varible names that need to be
   popped off their auto stacks. */

void
pop_vars (list)
     arg_list *list;
{
  bc_var *v_temp;
  bc_var_array *a_temp;
  int    ix;

  while (list != NULL)
    {
      ix = list->av_name;
      if (ix > 0)
	{
	  /* A simple variable. */
	  v_temp = variables[ix];
	  if (v_temp != NULL)
	    {
	      variables[ix] = v_temp->v_next;
	      free_num (&v_temp->v_value);
	      free (v_temp);
	    }
	}
      else
	{
	  /* An array variable. */
	  ix = -ix;
	  a_temp = arrays[ix];
	  if (a_temp != NULL)
	    {
	      arrays[ix] = a_temp->a_next;
	      if (!a_temp->a_param && a_temp->a_value != NULL)
		{
		  free_a_tree (a_temp->a_value->a_tree,
			       a_temp->a_value->a_depth);
		  free (a_temp->a_value);
		}
	      free (a_temp);
	    }
	}
      list = list->next;
    }
}


/* A call is being made to FUNC.  The call types are at PC.  Process
   the parameters by doing an auto on the parameter variable and then
   store the value at the new variable or put a pointer the the array
   variable. */

void
process_params (pc, func)
     program_counter *pc;
     int func;
{
  char ch;
  arg_list *params;
  int ix, ix1;
  bc_var *v_temp;
  bc_var_array *a_src, *a_dest;
  bc_num *n_temp;

  /* Get the parameter names from the function. */
  params = functions[func].f_params;

  while ((ch = byte(pc)) != ':')
    {
      if (params != NULL)
	{
	  if ((ch == '0') && params->av_name > 0)
	    {
	      /* A simple variable. */
	      ix = params->av_name;
	      v_temp = (bc_var *) bc_malloc (sizeof(bc_var));
	      v_temp->v_next = variables[ix];
	      v_temp->v_value = ex_stack->s_num;
	      init_num (&ex_stack->s_num);
	      variables[ix] = v_temp;
	    }
	  else
	    if ((ch == '1') && (params->av_name < 0))
	      {
		/* The variables is an array variable. */

		/* Compute source index and make sure some structure exists. */
		ix = (int) num2long (ex_stack->s_num);
		n_temp = get_array_num (ix, 0);

		/* Push a new array and Compute Destination index */
		auto_var (params->av_name);
		ix1 = -params->av_name;

		/* Set up the correct pointers in the structure. */
		if (ix == ix1)
		  a_src = arrays[ix]->a_next;
		else
		  a_src = arrays[ix];
		a_dest = arrays[ix1];
		a_dest->a_param = TRUE;
		a_dest->a_value = a_src->a_value;
	      }
	    else
	      {
		if (params->av_name < 0)
		  rt_error ("Parameter type mismatch parameter %s.",
			    a_names[-params->av_name]);
		else
		  rt_error ("Parameter type mismatch, parameter %s.",
			    v_names[params->av_name]);
		params++;
	      }
	  pop ();
	}
      else
	{
	    rt_error ("Parameter number mismatch");
	    return;
	}
      params = params->next;
    }
  if (params != NULL)
    rt_error ("Parameter number mismatch");
}
