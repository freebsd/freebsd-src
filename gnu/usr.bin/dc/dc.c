/* 
 * `dc' desk calculator utility.
 *
 * Copyright (C) 1984, 1993 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include "decimal.h"  /* definitions for our decimal arithmetic package */

FILE *open_file;	/* input file now open */
int file_count;		/* Number of input files not yet opened */
char **next_file;	/* Pointer to vector of names of input files left */

struct regstack
  {
    decimal value;	/* Saved value of register */
    struct regstack *rest;	/* Tail of list */
  };

typedef struct regstack *regstack;

regstack freeregstacks;	/* Chain of free regstack structures for fast realloc */

decimal regs[128];	/* "registers", with single-character names */
regstack regstacks[128]; /* For each register, a stack of previous values */

int stacktop;		/* index of last used element in stack */
int stacksize;		/* Current allocates size of stack */
decimal *stack;	/* Pointer to computation stack */

/* A decimal number can be regarded as a string by
 treating its contents as characters and ignoring the
 position of its decimal point.
 Decimal numbers are marked as strings by having an `after' field of -1
 One use of strings is to execute them as macros.
*/

#define STRING -1

int macrolevel;	/* Current macro nesting; 0 if taking keyboard input */
int macrostacksize;	/* Current allocated size of macrostack and macroindex */
decimal *macrostack;	/* Pointer to macro stack array */
int *macroindex;	/* Pointer to index-within-macro stack array */
	/* Note that an empty macro is popped from the stack
	   only when an trying to read a character from it
	   or trying to push another macro.  */

int ibase;	/* Radix for numeric input.  */
int obase;	/* Radix for numeric output.  */
int precision;	/* Number of digits to keep in multiply and divide.  */

char *buffer;	/* Address of buffer used for reading numbers */
int bufsize;	/* Current size of buffer (made bigger when nec) */

decimal dec_read ();
regstack get_regstack ();
int fetch ();
int fgetchar ();
char *concat ();
void pushsqrt ();
void condop ();
void setibase ();
void setobase ();
void setprecision ();
void pushmacro ();
decimal read_string ();
void pushlength ();
void pushscale ();
void unfetch ();
void popmacros ();
void popmacro ();
void popstack ();
void print_obj ();
void print_string ();
void free_regstack ();
void pushreg ();
void execute ();
void fputchar ();
void push ();
void incref ();
void decref ();
void binop ();

main (argc, argv, env)
     int argc;
     char **argv, **env;
{

  ibase = 10;
  obase = 10;
  precision = 0;

  freeregstacks = 0;

  bzero (regs, sizeof regs);
  bzero (regstacks, sizeof regstacks);

  bufsize = 40;
  buffer = (char *) xmalloc (40);

  stacksize = 40;
  stack = (decimal *) xmalloc (stacksize * sizeof (decimal));
  stacktop = -1;

  macrostacksize = 40;
  macrostack = (decimal *) xmalloc (macrostacksize * sizeof (decimal));
  macroindex = (int *) xmalloc (macrostacksize * sizeof (int));
  macrolevel = 0;
  /* Initialize for reading input files if any */

  open_file = 0;

  file_count = argc - 1;
  next_file = argv + 1;


  while (1)
    {
      execute ();
    }
}

/* Read and execute one command from the current source of input */

void
execute ()
{
  int c = fetch ();

  if (c < 0) exit (0);

    {
      switch (c)
	{
	case '+':		/* Arithmetic operators... */
	  binop (decimal_add);
	  break;

	case '-':
	  binop (decimal_sub);
	  break;

	case '*':
	  binop (decimal_mul_dc);  /* Like decimal_mul but hairy
				      way of deciding precision to keep */
	  break;

	case '/':
	  binop (decimal_div);
	  break;

	case '%':
	  binop (decimal_rem);
	  break;

	case '^':
	  binop (decimal_expt);
	  break;

	case '_':		/* Begin a negative decimal constant */
	  {
	    decimal tem = dec_read (stdin);
	    tem->sign = !tem->sign;
	    push (tem);
	  }
	  break;

	case '.':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':		/* All these begin decimal constants */
	  unfetch (c);
	  push (dec_read (stdin));
	  break;

	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	  unfetch (c);
	  push (dec_read (stdin));
	  break;

	case 'c':		/* Clear the stack */
	  while (stacktop >= 0)
	    decref (stack[stacktop--]);
	  break;

	case 'd':		/* Duplicate top of stack */
	  if (stacktop < 0)
	    error ("stack empty", 0);
	  else push (stack[stacktop]);
	  break;

	case 'f':		/* Describe all registers and stack contents */
	  {
	    int regno;
	    int somereg = 0;	/* set to 1 if we print any registers */
	    for (regno = 0; regno < 128; regno++)
	      {
		if (regs[regno])
		  {
		    printf ("register %c: ", regno);
		    print_obj (regs[regno]);
		    somereg = 1;
		    printf ("\n");
		  }
	      }
	    if (somereg)
	      printf ("\n");
	    if (stacktop < 0)
	      printf ("stack empty\n");
	    else
	      {
		int i;
		printf ("stack:\n");
		for (i = 0; i <= stacktop; i++)
		  {
		    print_obj (stack[stacktop - i]);
		    printf ("\n");
		  }
	      }
	  }
	  break;

	case 'i':		/* ibase <- top of stack */
	  popstack (setibase);
	  break;

	case 'I':		/* Push current ibase */
	  push (decimal_from_int (ibase));
	  break;

	case 'k':		/* like i, I but for precision instead of ibase */
	  popstack (setprecision);
	  break;

	case 'K':
	  push (decimal_from_int (precision));
	  break;

	case 'l':		/* l<x> load register <x> onto stack */
	  {
	    char c1 = fetch ();
	    if (c1 < 0) exit (0);
	    if (!regs[c1])
	      error ("register %c empty", c1);
	    else
	      push (regs[c1]);
	  }
	  break;

	case 'L':		/* L<x> load register <x> to stack, pop <x>'s own stack */
	  {
	    char c1 = fetch ();
	    if (c1 < 0) exit (0);
	    if (!regstacks[c1])
	      error ("nothing pushed on register %c", c1);
	    else
	      {
		regstack r = regstacks[c1];
		if (!regs[c1])
		  error ("register %c empty after pop", c1);
		else
		  push (regs[c1]);
		regs[c1] = r->value;
		regstacks[c1] = r->rest;
		free_regstack (r);
	      }
	  }
	  break;

	case 'o':		/* o, O like i, I but for obase instead of ibase */
	  popstack (setobase);
	  break;

	case 'O':
	  push (decimal_from_int (obase));
	  break;

	case 'p':		/* Print tos, don't pop, do print newline afterward */
	  if (stacktop < 0)
	    error ("stack empty", 0);
	  else
	    {
	      print_obj (stack[stacktop]);
	      printf ("\n");
	    }
	  break;

	case 'P':		/* Print tos, do pop, no newline afterward */
	  popstack (print_obj);
	  break;

	case 'q':		/* Exit */
	  if (macrolevel)
	    { popmacro (); popmacro (); }  /* decrease recursion level by 2 */
	  else
	    exit (0);		/* If not in a macro, exit the program.  */

	  break;

	case 'Q':		/* Tos says how many levels to exit */
	  popstack (popmacros);
	  break;

	case 's':		/* s<x> -- Pop stack and set register <x> */
	  if (stacktop < 0)
	    empty ();
	  else
	    {
	      int c1 = fetch ();
	      if (c1 < 0) exit (0);
	      if (regs[c1]) decref (regs[c1]);
	      regs[c1] = stack[stacktop--];
	    }
	  break;

	case 'S':		/* S<x> -- pop stack and push as new value of register <x> */
	  if (stacktop < 0)
	    empty ();
	  else
	    {
	      int c1 = fetch ();
	      if (c1 < 0) exit (0);
	      pushreg (c1);
	      regs[c1] = stack[stacktop--];
	    }
	  break;

	case 'v':		/* tos gets square root of tos */
	  popstack (pushsqrt);
	  break;

	case 'x':		/* pop stack , call as macro */
	  popstack (pushmacro);
	  break;

	case 'X':		/* Pop stack, get # fraction digits, push that */
	  popstack (pushscale);
	  break;

	case 'z':		/* Compute depth of stack, push that */
	  push (decimal_from_int (stacktop + 1));
	  break;

	case 'Z':		/* Pop stack, get # digits, push that */
	  popstack (pushlength);
	  break;

	case '<':		/* Conditional: pop two numbers, compare, maybe execute register */
	  /* Note: for no obvious reason, the standard Unix `dc'
	     considers < to be true if the top of stack is less
	     than the next-to-top of stack,
	     and vice versa for >.
	     This seems backwards to me, but I am preserving compatibility.  */
	  condop (1);
	  break;

	case '>':
	  condop (-1);
	  break;

	case '=':
	  condop (0);
	  break;

	case '?':		/* Read expression from terminal and execute it */
	  /* First ignore any leading newlines */
	  {
	    int c1;
	    while ((c1 = getchar ()) == '\n');
	    ungetc (c1, stdin);
	  }
	  /* Read a line from the terminal and execute it.  */
	  pushmacro (read_string ('\n', fgetchar, 0));
	  break;

	case '[':		/* Begin string constant */
	  push (read_string (']', fetch, '['));
	  break;

        case ' ':
	case '\n':
	  break;

	default:
	  error ("undefined command %c", c);
	}
    }
}

/* Functionals for performing arithmetic, etc */

/* Call the function `op', with the top of stack value as argument,
 and then pop the stack.
 If the stack is empty, print a message and do not call `op'.  */

void
popstack (op)
     void (*op) ();
{
  if (stacktop < 0)
    empty ();
  else
    {
      decimal value = stack[stacktop--];
      op (value);
      decref (value);
    }
}

/* Call the function `op' with two arguments taken from the stack top,
 then pop those arguments and push the value returned by `op'.
 `op' is assumed to return a decimal number.
 If there are not two values on the stack, print a message
 and do not call `op'.  */

void
binop (op)
     decimal (*op) ();
{
  if (stacktop < 1)
    error ("stack empty", 0);
  else if (stack[stacktop]->after == STRING || stack[stacktop - 1]->after == STRING)
    error ("operands not both numeric");
  else
    {
      decimal arg2 = stack [stacktop--];
      decimal arg1 = stack [stacktop--];

      push (op (arg1, arg2, precision));

      decref (arg1);
      decref (arg2);
    }
}

void
condop (cond)
     int cond;
{
  int regno = fetch ();
  if (!regs[regno])
    error ("register %c is empty", regno);
  else if (stacktop < 1)
    empty ();
  else
    {
      decimal arg2 = stack[stacktop--];
      decimal arg1 = stack[stacktop--];
      int relation = decimal_compare (arg1, arg2);
      decref (arg1);
      decref (arg2);
      if (cond == relation
	  || (cond < 0 && relation < 0)
	  || (cond > 0 && relation > 0))
	pushmacro (regs[regno]);
    }
}

/* Handle the command input source */

/* Fetch the next command character from a macro or from the terminal */

int
fetch()
{
  int c = -1;

  while (macrolevel &&
		LENGTH (macrostack[macrolevel-1]) == macroindex[macrolevel-1])
    popmacro();
  if (macrolevel)
    return macrostack[macrolevel - 1]->contents[macroindex[macrolevel-1]++];
  while (1)
    {
      if (open_file)
	{
	  c = getc (open_file);
	  if (c >= 0) break;
	  fclose (open_file);
	  open_file = 0;
	}
      else if (file_count)
	{
	  open_file = fopen (*next_file++, "r");
	  file_count--;
	  if (!open_file)
	    perror_with_name (*(next_file - 1));
	}
      else break;
    }
  if (c >= 0) return c;
  return getc (stdin);
}

/* Unread character c on command input stream, whatever it is */

void
unfetch (c)
     char c;
{
  if (macrolevel)
    macroindex[macrolevel-1]--;
  else if (open_file)
    ungetc (c, open_file);
  else
    ungetc (c, stdin);
}

/* Begin execution of macro m.  */

void
pushmacro (m)
     decimal m;
{
  while (macrolevel &&
		LENGTH (macrostack[macrolevel-1]) == macroindex[macrolevel-1])
    popmacro();
  if (m->after == STRING)
    {
      if (macrolevel == macrostacksize)
	{
	  macrostacksize *= 2;
	  macrostack = (decimal *) xrealloc (macrostack, macrostacksize * sizeof (decimal));
	  macroindex = (int *) xrealloc (macroindex, macrostacksize * sizeof (int));
	}
      macroindex[macrolevel] = 0;
      macrostack[macrolevel++] = m;
      incref (m);
    }
  else
    {   /* Number supplied as a macro!  */
      push (m);   /* Its effect wouyld be to push the number.  */
    }
}

/* Pop a specified number of levels of macro execution.
 The number of levels is specified by a decimal number d.  */

void
popmacros (d)
     decimal d;
{
  int num_pops = decimal_to_int (d);
  int i;
  for (i = 0; i < num_pops; i++)
    popmacro ();
}
/* Exit one level of macro execution.  */

void
popmacro ()
{
  if (!macrolevel)
    exit (0);
  else
    {
      decref (macrostack[--macrolevel]);
    }
}

void
push (d)
     decimal d;
{
  if (stacktop == stacksize - 1)
    stack = (decimal *) xrealloc (stack, (stacksize *= 2) * sizeof (decimal));

    incref (d);

    stack[++stacktop] = d;
}

/* Reference counting and storage freeing */

void
decref (d)
     decimal d;
{
  if (!--d->refcnt)
    free (d);
}

void
incref (d)
     decimal d;
{
  d->refcnt++;
}

empty ()
{
  error ("stack empty", 0);
}

regstack
get_regstack ()
{
  if (freeregstacks)
    {
      regstack r = freeregstacks;
      freeregstacks = r ->rest;
      return r;
    }
  else
    return (regstack) xmalloc (sizeof (struct regstack));
}

void
free_regstack (r)
     regstack r;
{
  r->rest = freeregstacks;
  freeregstacks = r;
}

void
pushreg (c)
     char c;
{
  regstack r = get_regstack ();

  r->rest = regstacks[c];
  r->value = regs[c];
  regstacks[c] = r;
  regs[c] = 0;
}

/* Input of numbers and strings */

/* Return a character read from the terminal.  */

fgetchar ()
{
  return getchar ();
}

void
fputchar (c)
     char (c);
{
  putchar (c);
}

/* Read text from command input source up to a close-bracket,
   make a string out of it, and return it.
   If STARTC is nonzero, then it and STOPC must balance when nested.  */

decimal
read_string (stopc, inputfn, startc)
     char stopc;
     int (*inputfn) ();
     int startc;
{
  int c;
  decimal result;
  int i = 0;
  int count = 0;

  while (1)
    {
      c = inputfn ();
      if (c < 0 || (c == stopc && count == 0))
	{
	  if (count != 0)
	    error ("Unmatched `%c'", startc);
	  break;
	}
      if (c == stopc)
	count--;
      if (c == startc)
	count++;
      if (i + 1 >= bufsize)
	buffer = (char *) xrealloc (buffer, bufsize *= 2);
      buffer[i++] = c;
    }
  result = make_decimal (i, 0);
  result->after = -1;	/* Mark it as a string */
  result->before++;	/* but keep the length unchanged */
  bcopy (buffer, result->contents, i);
  return result;
}

/* Read a number from the current input source */

decimal
dec_read ()
{
  int c;
  int i = 0;

  while (1)
    {
      c = fetch ();
      if (! ((c >= '0' && c <= '9')
	     || (c >= 'A' && c <= 'F')
	     || c == '.'))
        break;
      if (i + 1 >= bufsize)
	buffer = (char *) xrealloc (buffer, bufsize *= 2);
      buffer[i++] = c;
    }
  buffer[i++] = 0;
  unfetch (c);

  return decimal_parse (buffer, ibase);
}

/* Output of numbers and strings */

/* Print the contents of obj, either numerically or as a string,
 according to what obj says it is.  */

void
print_obj (obj)
     decimal obj;
{
  if (obj->after == STRING)
    print_string (obj);
  else
    decimal_print (obj, fputchar, obase);
}

/* Print the contents of the decimal number `string', treated as a string.  */

void
print_string (string)
     decimal string;
{
  char *p = string->contents;
  int len = LENGTH (string);
  int i;

  for (i = 0; i < len; i++)
    {
      putchar (*p++);
    }
}

/* Set the input radix from the value of the decimal number d, if valid.  */

void
setibase (d)
     decimal d;
{
  int value = decimal_to_int (d);
  if (value < 2 || value > 36)
    error ("input radix must be from 2 to 36", 0);
  else
    ibase = value;
}

/* Set the output radix from the value of the decimal number d, if valid.  */

void
setobase (d)
     decimal d;
{
  int value = decimal_to_int (d);
  if (value < 2 || value > 36)
    error ("output radix must be from 2 to 36", 0);
  else
    obase = value;
}

/* Set the precision for mul and div from the value of the decimal number d, if valid.  */

void
setprecision (d)
     decimal d;
{
  int value = decimal_to_int (d);
  if (value < 0 || value > 30000)
    error ("precision must be nonnegative and < 30000", 0);
  else
    precision = value;
}

/* Push the number of digits in decimal number d, as a decimal number.  */

void
pushlength (d)
     decimal d;
{
  push (decimal_from_int (LENGTH (d)));
}

/* Push the number of fraction digits in d.  */

void
pushscale (d)
     decimal d;
{
  push (decimal_from_int (d->after));
}

/* Push the square root of decimal number d.  */

void
pushsqrt (d)
     decimal d;
{
  push (decimal_sqrt (d, precision));
}

/* Print error message and exit.  */

fatal (s1, s2)
     char *s1, *s2;
{
  error (s1, s2);
  exit (1);
}

/* Print error message.  `s1' is printf control string, `s2' is arg for it. */

error (s1, s2)
     char *s1, *s2;
{
  printf ("dc: ");
  printf (s1, s2);
  printf ("\n");
}

decimal_error (s1, s2)
     char *s1, *s2;
{
  error (s1, s2);
}

perror_with_name (name)
     char *name;
{
  extern int errno, sys_nerr;
  extern char *sys_errlist[];
  char *s;

  if (errno < sys_nerr)
    s = concat ("", sys_errlist[errno], " for %s");
  else
    s = "cannot open %s";
  error (s, name);
}

/* Return a newly-allocated string whose contents concatenate those of s1, s2, s3.  */

char *
concat (s1, s2, s3)
     char *s1, *s2, *s3;
{
  int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
  char *result = (char *) xmalloc (len1 + len2 + len3 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  strcpy (result + len1 + len2, s3);
  *(result + len1 + len2 + len3) = 0;

  return result;
}

/* Like malloc but get fatal error if memory is exhausted.  */

int
xmalloc (size)
     int size;
{
  int result = malloc (size);
  if (!result)
    fatal ("virtual memory exhausted", 0);
  return result;
}

int
xrealloc (ptr, size)
     char *ptr;
     int size;
{
  int result = realloc (ptr, size);
  if (!result)
    fatal ("virtual memory exhausted");
  return result;
}
