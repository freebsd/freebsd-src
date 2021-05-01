/*
 * filesize.c -- Utilities for displaying file sizes
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */


/*** Includes. ***/

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <apr_strings.h>

#include "cl.h"


/*** Code. ***/

/* The structure that describes the units and their magnitudes. */
typedef struct filesize_order_t
{
  svn_filesize_t mask;
  const char *suffix;
  const char *short_suffix;
} filesize_order_t;


/* Get the index of the order of magnitude of the given SIZE.
   The returned index will be within [0 .. order_size - 1]. */
static apr_size_t
get_order_index(svn_filesize_t abs_size,
                const filesize_order_t *order,
                apr_size_t order_size)
{
  /* It would be sexy to do a binary search here, but with only 7 elements
     in the arrays ... we should ### FIXME: do the binary search anyway. */
  apr_size_t index = order_size;
  while (index > 0)
    {
      --index;
      if (abs_size > order[index].mask)
        break;
    }
  return index;
}


/* Format the adjusted size with the given units. */
static const char *
format_size(double human_readable_size,
            svn_boolean_t long_units,
            const filesize_order_t *order,
            apr_size_t index,
            apr_pool_t *result_pool)
{
  /* NOTE: We want to display a locale-specific decimal sepratator, but
           APR's formatter completely ignores the locale. So we use the
           good, old, standard, *dangerous* sprintf() to format the size.

           But, on the bright side, we require that the number has no more
           than 3 non-fractional digits. So the call to sprintf() here
           should be safe. */
  const double absolute_human_readable_size = fabs(human_readable_size);
  const char *const suffix = (long_units ? order[index].suffix
                              : order[index].short_suffix);

  /*   3 digits (or 2 digits and 1 decimal separator)
     + 1 negative sign (which should not appear under normal circumstances)
     + 1 nul terminator
     ---
     = 5 characters of space needed in the buffer. */
    char buffer[64];

    assert(absolute_human_readable_size < 1000);

    /* When the adjusted size has only one significant digit left of
       the decimal point, show tenths of a unit, too. Except when
       the absolute size is actually a single-digit number, because
       files can't have fractional byte sizes. */
    if (absolute_human_readable_size >= 10)
      sprintf(buffer, "%.0f", human_readable_size);
    else
      {
        double integral;
        const double frac = modf(absolute_human_readable_size, &integral);
        const int decimals = (index > 0 && (integral < 9 || frac <= .949999999));
        sprintf(buffer, "%.*f", decimals, human_readable_size);
      }

    return apr_pstrcat(result_pool, buffer, suffix, SVN_VA_NULL);
}


static const char *
get_base2_unit_file_size(svn_filesize_t size,
                         svn_boolean_t long_units,
                         apr_pool_t *result_pool)
{
  static const filesize_order_t order[] =
    {
      {APR_INT64_C(0x0000000000000000), " B",   "B"}, /* byte */
      {APR_INT64_C(0x00000000000003FF), " KiB", "K"}, /* kibi */
      {APR_INT64_C(0x00000000000FFFFF), " MiB", "M"}, /* mibi */
      {APR_INT64_C(0x000000003FFFFFFF), " GiB", "G"}, /* gibi */
      {APR_INT64_C(0x000000FFFFFFFFFF), " TiB", "T"}, /* tibi */
      {APR_INT64_C(0x0003FFFFFFFFFFFF), " EiB", "E"}, /* exbi */
      {APR_INT64_C(0x0FFFFFFFFFFFFFFF), " PiB", "P"}  /* pibi */
    };
  static const apr_size_t order_size = sizeof(order) / sizeof(order[0]);

  const svn_filesize_t abs_size = ((size < 0) ? -size : size);
  apr_size_t index = get_order_index(abs_size, order, order_size);
  double human_readable_size;

  /* Adjust the size to the given order of magnitude.

     This is division by (order[index].mask + 1), which is the base-2^10
     magnitude of the size; and that is the same as an arithmetic right
     shift by (index * 10) bits. But we split it into an integer and a
     floating-point division, so that we don't overflow the mantissa at
     very large file sizes. */
  if ((abs_size >> 10 * index) > 999)
    {
      /* This assertion should never fail, because we only have 4 binary
         digits in the petabyte (all right, "pibibyte") range and so the
         number of petabytes can't be large enough to cause the program
         flow to enter this conditional block. */
      assert(index < order_size - 1);
      ++index;
    }

  human_readable_size = (index == 0 ? (double)size
                         : (size >> (10 * index - 10)) / 1024.0);

  return format_size(human_readable_size,
                     long_units, order, index, result_pool);
}


static const char *
get_base10_unit_file_size(svn_filesize_t size,
                          svn_boolean_t long_units,
                          apr_pool_t *result_pool)
{
  static const filesize_order_t order[] =
    {
      {APR_INT64_C(                 0), " B",  "B"}, /* byte */
      {APR_INT64_C(               999), " kB", "k"}, /* kilo */
      {APR_INT64_C(            999999), " MB", "M"}, /* mega */
      {APR_INT64_C(         999999999), " GB", "G"}, /* giga */
      {APR_INT64_C(      999999999999), " TB", "T"}, /* tera */
      {APR_INT64_C(   999999999999999), " EB", "E"}, /* exa  */
      {APR_INT64_C(999999999999999999), " PB", "P"}  /* peta */
      /*          9223372036854775807 is the maximum value.  */
    };
  static const apr_size_t order_size = sizeof(order) / sizeof(order[0]);

  const svn_filesize_t abs_size = ((size < 0) ? -size : size);
  apr_size_t index = get_order_index(abs_size, order, order_size);
  double human_readable_size;

  /* Adjust the size to the given order of magnitude.

     This is division by (order[index].mask + 1), which is the
     base-1000 magnitude of the size. We split the operation into an
     integer and a floating-point division, so that we don't
     overflow the mantissa. */
  if (index == 0)
    human_readable_size = (double)size;
  else
    {
      const svn_filesize_t divisor = (order[index - 1].mask + 1);
      /*      [Keep integer arithmetic here!] */
      human_readable_size = (size / divisor) / 1000.0;
    }

  /* Adjust index and number for rounding. */
  if (human_readable_size >= 999.5)
    {
      /* This assertion should never fail, because we only have one
         decimal digit in the petabyte range and so the number of
         petabytes can't be large enough to cause the program flow
         to enter this conditional block. */
      assert(index < order_size - 1);
      human_readable_size /= 1000.0;
      ++index;
    }

  return format_size(human_readable_size,
                     long_units, order, index, result_pool);
}


svn_error_t *
svn_cl__format_file_size(const char **result,
                         svn_filesize_t size,
                         svn_cl__size_unit_t base,
                         svn_boolean_t long_units,
                         apr_pool_t *result_pool)
{
  switch (base)
    {
    case SVN_CL__SIZE_UNIT_NONE:
    case SVN_CL__SIZE_UNIT_XML:
      *result = apr_psprintf(result_pool, "%" SVN_FILESIZE_T_FMT, size);
      break;

    case SVN_CL__SIZE_UNIT_BASE_2:
      *result = get_base2_unit_file_size(size, long_units, result_pool);
      break;

    case SVN_CL__SIZE_UNIT_BASE_10:
      *result = get_base10_unit_file_size(size, long_units, result_pool);
      break;

    default:
      SVN_ERR_MALFUNCTION();
    }

  return SVN_NO_ERROR;
}
