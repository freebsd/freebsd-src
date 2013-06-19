/* key-gen.c --- manufacturing sequential keys for some db tables
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <apr.h>
#include <apr_network_io.h>
#include "private/svn_fs_private.h"
#include "key-gen.h"

/* The Berkeley DB backend uses a key as a transaction name and the
   maximum key size must be less than the maximum transaction name
   length. */
#if MAX_KEY_SIZE > SVN_FS__TXN_MAX_LEN
#error The MAX_KEY_SIZE used for BDB txn names is greater than SVN_FS__TXN_MAX_LEN.
#endif


/*** Keys for reps and strings. ***/

void
svn_fs_fs__add_keys(const char *key1, const char *key2, char *result)
{
  apr_ssize_t i1 = strlen(key1) - 1;
  apr_ssize_t i2 = strlen(key2) - 1;
  int i3 = 0;
  int val;
  int carry = 0;
  char buf[MAX_KEY_SIZE + 2];

  while ((i1 >= 0) || (i2 >= 0) || (carry > 0))
    {
      val = carry;
      if (i1>=0)
        val += (key1[i1] <= '9') ? (key1[i1] - '0') : (key1[i1] - 'a' + 10);

      if (i2>=0)
        val += (key2[i2] <= '9') ? (key2[i2] - '0') : (key2[i2] - 'a' + 10);

      carry = val / 36;
      val = val % 36;

      buf[i3++] = (char)((val <= 9) ? (val + '0') : (val - 10 + 'a'));

      if (i1>=0)
        i1--;
      if (i2>=0)
        i2--;
    }

  /* Now reverse the resulting string and NULL terminate it. */
  for (i1 = 0; i1 < i3; i1++)
    result[i1] = buf[i3 - i1 - 1];

  result[i1] = '\0';
}


void
svn_fs_fs__next_key(const char *this, apr_size_t *len, char *next)
{
  apr_ssize_t i;
  apr_size_t olen = *len;     /* remember the first length */
  char c;                     /* current char */
  svn_boolean_t carry = TRUE; /* boolean: do we have a carry or not?
                                 We start with a carry, because we're
                                 incrementing the number, after all. */

  /* Leading zeros are not allowed, except for the string "0". */
  if ((*len > 1) && (this[0] == '0'))
    {
      *len = 0;
      return;
    }

  for (i = (olen - 1); i >= 0; i--)
    {
      c = this[i];

      /* Validate as we go. */
      if (! (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z'))))
        {
          *len = 0;
          return;
        }

      if (carry)
        {
          if (c == 'z')
            next[i] = '0';
          else
            {
              carry = FALSE;

              if (c == '9')
                next[i] = 'a';
              else
                next[i] = ++c;
            }
        }
      else
        next[i] = c;
    }

  /* The new length is OLEN, plus 1 if there's a carry out of the
     leftmost digit. */
  *len = olen + (carry ? 1 : 0);

  /* Ensure that we haven't overrun the (ludicrous) bound on key length.
     Note that MAX_KEY_SIZE is a bound on the size *including*
     the trailing null byte. */
  assert(*len < MAX_KEY_SIZE);

  /* Now we know it's safe to add the null terminator. */
  next[*len] = '\0';

  /* Handle any leftover carry. */
  if (carry)
    {
      memmove(next+1, next, olen);
      next[0] = '1';
    }
}


int
svn_fs_fs__key_compare(const char *a, const char *b)
{
  apr_size_t a_len = strlen(a);
  apr_size_t b_len = strlen(b);
  int cmp;

  if (a_len > b_len)
    return 1;
  if (b_len > a_len)
    return -1;
  cmp = strcmp(a, b);
  return (cmp ? (cmp / abs(cmp)) : 0);
}
