/* Test that routines allow reusing a source variable as destination.  */

#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "urandom.h"

#ifndef SIZE
#define SIZE 16
#endif

#ifndef EXPO
#define EXPO 32
#endif

#if __STDC__
typedef void (*dss_func) (mpf_ptr, mpf_srcptr, mpf_srcptr);
#else
typedef void (*dss_func) ();
#endif

dss_func dss_funcs[] =
{
  mpf_div, mpf_add, mpf_mul, mpf_sub,
};

char *dss_func_names[] =
{
  "mpf_div", "mpf_add", "mpf_mul", "mpf_sub",
};

#if __STDC__
typedef void (*dsi_func) (mpf_ptr, mpf_srcptr, unsigned long int);
#else
typedef void (*dsi_func) ();
#endif

dsi_func dsi_funcs[] =
{
  mpf_div_ui, mpf_add_ui, mpf_mul_ui, mpf_sub_ui,
};

char *dsi_func_names[] =
{
  "mpf_div_ui", "mpf_add_ui", "mpf_mul_ui", "mpf_sub_ui",
};

#if __STDC__
typedef void (*dis_func) (mpf_ptr, unsigned long int, mpf_srcptr);
#else
typedef void (*dis_func) ();
#endif

dis_func dis_funcs[] =
{
  mpf_ui_div, mpf_ui_sub,
};

char *dis_func_names[] =
{
  "mpf_ui_div", "mpf_ui_sub",
};

main (argc, argv)
     int argc;
     char **argv;
{
  int i;
  int pass, reps = 100000;
  mpf_t in1, in2, out1;
  unsigned long int in1i, in2i;
  mpf_t res1, res2, res3;
  mp_size_t bprec = 100;

  if (argc > 1)
    {
      reps = strtol (argv[1], 0, 0);
      if (argc > 2)
	bprec = strtol (argv[2], 0, 0);
    }

  mpf_set_default_prec (bprec);

  mpf_init (in1);
  mpf_init (in2);
  mpf_init (out1);
  mpf_init (res1);
  mpf_init (res2);
  mpf_init (res3);

  for (pass = 1; pass <= reps; pass++)
    {
      mpf_random2 (in1, urandom () % SIZE - SIZE/2, urandom () % EXPO);
      mpf_random2 (in2, urandom () % SIZE - SIZE/2, urandom () % EXPO);

      for (i = 0; i < sizeof (dss_funcs) / sizeof (dss_func); i++)
	{
	  /* Don't divide by 0.  */
	  if (i == 0 && mpf_cmp_ui (in2, 0) == 0)
	    continue;

	  (dss_funcs[i]) (res1, in1, in2);

	  mpf_set (out1, in1);
	  (dss_funcs[i]) (out1, out1, in2);
	  mpf_set (res2, out1);

	  mpf_set (out1, in2);
	  (dss_funcs[i]) (out1, in1, out1);
	  mpf_set (res3, out1);

	  if (mpf_cmp (res1, res2) != 0)
	    dump_abort (dss_func_names[i], res1, res2);
	  if (mpf_cmp (res1, res3) != 0)
	    dump_abort (dss_func_names[i], res1, res3);
	}

      in2i = urandom ();
      for (i = 0; i < sizeof (dsi_funcs) / sizeof (dsi_func); i++)
	{
	  /* Don't divide by 0.  */
	  if (i == 0 && in2i == 0)
	    continue;

	  (dsi_funcs[i]) (res1, in1, in2i);

	  mpf_set (out1, in1);
	  (dsi_funcs[i]) (out1, out1, in2i);
	  mpf_set (res2, out1);

	  if (mpf_cmp (res1, res2) != 0)
	    dump_abort (dsi_func_names[i], res1, res2);
	}

      in1i = urandom ();
      for (i = 0; i < sizeof (dis_funcs) / sizeof (dis_func); i++)
	{
	  /* Don't divide by 0.  */
	  if (i == 0 && mpf_cmp_ui (in2, 0) == 0)
	    continue;

	  (dis_funcs[i]) (res1, in1i, in2);

	  mpf_set (out1, in2);
	  (dis_funcs[i]) (out1, in1i, in2);
	  mpf_set (res2, out1);

	  if (mpf_cmp (res1, res2) != 0)
	    dump_abort (dis_func_names[i], res1, res2);
	}

    }

  exit (0);
}

dump_abort (name, res1, res2)
     char *name;
     mpf_t res1, res2;
{
  printf ("failure in %s:", name);
  oo (res1);
  oo (res2);
  abort ();
}

oo (x)
     mpf_t x;
{
  mp_size_t i;
  printf (" exp = %ld\n", x->_mp_exp);
  printf ("size = %d\n", x->_mp_size);
  for (i = ABS (x->_mp_size) - 1; i >= 0; i--)
    printf ("%08lX ", x->_mp_d[i]);
  printf ("\n");
  mpf_dump (x);
}

#if 0
void mpf_div_2exp	_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));
void mpf_mul_2exp	_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

void mpf_abs		_PROTO ((mpf_ptr, mpf_srcptr));
void mpf_sqrt		_PROTO ((mpf_ptr, mpf_srcptr));
void mpf_neg		_PROTO ((mpf_ptr, mpf_srcptr));
#endif
