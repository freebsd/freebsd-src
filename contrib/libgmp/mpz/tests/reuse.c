/* Test that routines allow reusing a source variable as destination.  */

#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "urandom.h"

#ifndef SIZE
#define SIZE 16
#endif

#if __STDC__
typedef void (*dss_func) (mpz_ptr, mpz_srcptr, mpz_srcptr);
#else
typedef void (*dss_func) ();
#endif

dss_func dss_funcs[] =
{
  mpz_add, mpz_and, mpz_cdiv_q, mpz_cdiv_r, mpz_fdiv_q, mpz_fdiv_r,
  mpz_gcd, mpz_ior, mpz_mul, mpz_sub, mpz_tdiv_q, mpz_tdiv_r
};

char *dss_func_names[] =
{
  "mpz_add", "mpz_and", "mpz_cdiv_q", "mpz_cdiv_r", "mpz_fdiv_q", "mpz_fdiv_r",
  "mpz_gcd", "mpz_ior", "mpz_mul", "mpz_sub", "mpz_tdiv_q", "mpz_tdiv_r"
};

char dss_func_division[] = {0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1};

#if 0
mpz_divexact /* requires special operands */
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  int i;
  int pass, reps = 10000;
  mpz_t in1, in2, out1;
  mpz_t res1, res2, res3;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (in1);
  mpz_init (in2);
  mpz_init (out1);
  mpz_init (res1);
  mpz_init (res2);
  mpz_init (res3);

  for (pass = 1; pass <= reps; pass++)
    {
      mpz_random (in1, urandom () % SIZE - SIZE/2);
      mpz_random (in2, urandom () % SIZE - SIZE/2);

      for (i = 0; i < sizeof (dss_funcs) / sizeof (dss_func); i++)
	{
	  if (dss_func_division[i] && mpz_cmp_ui (in2, 0) == 0)
	    continue;

	  (dss_funcs[i]) (res1, in1, in2);

	  mpz_set (out1, in1);
	  (dss_funcs[i]) (out1, out1, in2);
	  mpz_set (res2, out1);

	  mpz_set (out1, in2);
	  (dss_funcs[i]) (out1, in1, out1);
	  mpz_set (res3, out1);

	  if (mpz_cmp (res1, res2) != 0)
	    dump_abort (dss_func_names[i], in1, in2);
	  if (mpz_cmp (res1, res3) != 0)
	    dump_abort (dss_func_names[i], in1, in2);
	}
    }

  exit (0);
}

dump_abort (name, in1, in2)
     char *name;
     mpz_t in1, in2;
{
  printf ("failure in %s (", name);
  mpz_out_str (stdout, -16, in1);
  printf (" ");
  mpz_out_str (stdout, -16, in2);
  printf (")\n");
  abort ();
}

#if 0
void mpz_add_ui		_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_div_2exp	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_mod_2exp	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_mul_2exp	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_mul_ui		_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_pow_ui		_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_sub_ui		_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_tdiv_q_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
void mpz_tdiv_r_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

void mpz_abs		_PROTO ((mpz_ptr, mpz_srcptr));
void mpz_com		_PROTO ((mpz_ptr, mpz_srcptr));
void mpz_sqrt		_PROTO ((mpz_ptr, mpz_srcptr));
void mpz_neg		_PROTO ((mpz_ptr, mpz_srcptr));

void mpz_tdiv_qr_ui	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int));

void mpz_powm_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int, mpz_srcptr));

void mpz_gcdext		_PROTO ((mpz_ptr, mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));

void mpz_cdiv_qr	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));
void mpz_fdiv_qr	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));
void mpz_tdiv_qr	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));

void mpz_powm		_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr, mpz_srcptr));

void mpz_sqrtrem	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr));

unsigned long int mpz_cdiv_qr_ui	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int));
unsigned long int mpz_fdiv_qr_ui	_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int));

unsigned long int mpz_cdiv_q_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
unsigned long int mpz_cdiv_r_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
unsigned long int mpz_fdiv_q_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
unsigned long int mpz_fdiv_r_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
unsigned long int mpz_gcd_ui	_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));
#endif
