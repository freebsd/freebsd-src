#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
double strtod ();
#endif

#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include "xstrtod.h"

int
xstrtod (str, ptr, result)
     const char *str;
     const char **ptr;
     double *result;
{
  double val;
  char *terminator;
  int fail;

  fail = 0;
  errno = 0;
  val = strtod (str, &terminator);

  /* Having a non-zero terminator is an error only when PTR is NULL. */
  if (terminator == str || (ptr == NULL && *terminator != '\0'))
    fail = 1;
  else
    {
      /* Allow underflow (in which case strtod returns zero),
	 but flag overflow as an error. */
      if (val != 0.0 && errno == ERANGE)
	fail = 1;
    }

  if (ptr != NULL)
    *ptr = terminator;

  *result = val;
  return fail;
}

