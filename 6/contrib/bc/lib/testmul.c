/* compute the crossover for recursive and simple multiplication */

#include <stdio.h>
#include <time.h>
#include "number.h"
#ifndef VARARGS
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/* from number.c ... */
extern int mul_base_digits;
/* extern int mul_small_digits; */
extern bc_num _one_;

/* global variables */
int test_n = 1000;
int test_time = 30 * CLOCKS_PER_SEC;  /* 30 seconds */ 

/* Other things for number.c. */
int std_only;

void
out_of_memory()
{
  fprintf (stderr, "Fatal error: Out of memory for malloc.\n");
  exit (1);
}

/* Runtime error will  print a message and stop the machine. */

#ifndef VARARGS
#ifdef __STDC__
void
rt_error (char *mesg, ...)
#else
void
rt_error (mesg)
     char *mesg;
#endif
#else
void
rt_error (mesg, va_alist)
     char *mesg;
#endif
{
  va_list args;
  char error_mesg [255];

#ifndef VARARGS   
  va_start (args, mesg);
#else
  va_start (args);
#endif
  vsprintf (error_mesg, mesg, args);
  va_end (args);
  
  fprintf (stderr, "Runtime error: %s\n", error_mesg);
}

/* A runtime warning tells of some action taken by the processor that
   may change the program execution but was not enough of a problem
   to stop the execution. */

#ifndef VARARGS
#ifdef __STDC__
void
rt_warn (char *mesg, ...)
#else
void
rt_warn (mesg)
     char *mesg;
#endif
#else
void
rt_warn (mesg, va_alist)
     char *mesg;
#endif
{
  va_list args;
  char error_mesg [255];

#ifndef VARARGS   
  va_start (args, mesg);
#else
  va_start (args);
#endif
  vsprintf (error_mesg, mesg, args);
  va_end (args);

  fprintf (stderr, "Runtime warning: %s\n", error_mesg);
}

void
out_char (int ch)
{
  putchar (ch);
}

/* Time stuff !!! */

int
timeit ( bc_num a, bc_num b, int *n)
{
  clock_t first;
  int i, res;
  bc_num c;

  bc_init_num (&c);
  first = clock();
  *n = 0;
  do {
    for (i=0; i<test_n; i++)
      bc_multiply(a,b,&c,0);
    *n += test_n;
     res = (int) (clock() - first);
  } while (res < test_time);
  return res;
}

int debug = 0;  /* Print debugging messages? */

int main (int argc, char **argv)
{
  bc_num ten, num, expo, big;

  int min, max, mid;

#if 0
  int smallsize;
#endif

  int n1, n2;
  clock_t t1, t2;
  float permul1, permul2;

  /* args? */
  if (argc > 1)
    if (strcmp (argv[1], "-d") == 0)
      debug = 1;

  bc_init_numbers();
  bc_init_num (&ten);
  bc_init_num (&num);
  bc_init_num (&expo);
  bc_init_num (&big);
  bc_int2num (&ten, 10);

  if (debug)
    fprintf (stderr, "Timings are for %d multiplies\n"
	             "Minimum time is %d seconds\n", test_n,
	     test_time/CLOCKS_PER_SEC);

  /* Two of the same size */
  min = 10;
  max = 500;

  if (debug)
    fprintf (stderr, "Testing numbers of the same length.\n");

  while (min < max) {
    mid = (min+max)/2;
    if (debug) fprintf (stderr,"Checking %d...\n", mid);

    bc_int2num (&expo, mid);
    bc_raise (ten, expo, &num, 0);
    bc_sub (num, _one_, &num, 0);

    mul_base_digits = 2*mid+1;
    t1 = timeit (num, num, &n1);
    permul1 = (float)t1/(float)n1;

    mul_base_digits = 2*mid-1;
    t2 = timeit (num, num, &n2);
    permul2 = (float)t2/(float)n2;

    if (permul1 < permul2)
      min = mid+1;
    else
      max = mid-1;

    if (debug) {
      fprintf (stderr, "n1 = %d :: n2 = %d\n", n1, n2);
      fprintf (stderr, "p1 = %f :: p2 = %f\n", permul1, permul2);
    }
  }  

  if (debug)
    fprintf (stderr, "Base digits crossover at %d digits\n", min);
  printf ("#define MUL_BASE_DIGITS %d\n", 2*min);


#if 0
  mul_base_digits = min;

  /* Small one times a big one. */

  smallsize = min/2;
  bc_int2num (&expo, smallsize);
  bc_raise (ten, expo, &big, 0);
  bc_sub (num, _one_, &big, 0);

  min = min / 2;
  max = 500;

  if (debug)
    fprintf (stderr, "Testing numbers of the different length.\n");

  while (min < max) {
    mid = (min+max)/2;
    if (debug) fprintf (stderr, "Checking %d...\n", mid);

    bc_int2num (&expo, mid-smallsize);
    bc_raise (ten, expo, &num, 0);
    bc_sub (num, _one_, &num, 0);

    mul_small_digits = mid+1;
    t1 = timeit (big, num, &n1);
    permul1 = (float)t1/(float)n1;

    mul_small_digits = mid-1;
    t2 = timeit (big, num, &n2);
    permul2 = (float)t2/(float)n2;

    if (permul1 < permul2)
      min = mid+1;
    else
      max = mid-1;

    if (debug) {
      fprintf (stderr, "n1 = %d :: n2 = %d\n", n1, n2);
      fprintf (stderr, "p1 = %f :: p2 = %f\n", permul1, permul2);
    }
  }  
  
  if (debug)
    fprintf (stderr, "Non equal digits crossover at %d total digits\n", min);
  printf ("#define MUL_SMALL_DIGITS = %d\n", min);

#endif

  return 0;
}
