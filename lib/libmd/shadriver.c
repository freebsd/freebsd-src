/* SHADRIVER.C - test driver for SHA-1 (and SHA-0)
 * $FreeBSD$
 */

/* Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
   rights reserved.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

/* The following makes SHA default to SHA-1 if it has not already been
     defined with C compiler flags.
 */
#ifndef SHA
#define SHA 1
#endif

#include <sys/types.h>

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "sha.h"
#if SHA == 1
#define SHA_Data SHA1_Data
#endif

/* Digests a string and prints the result.
 */
static void SHAString (string)
char *string;
{
  char buf[2*20+1];

  printf ("SHA-%d (\"%s\") = %s\n", 
	SHA, string, SHA_Data(string,strlen(string),buf));
}

/* Digests a reference suite of strings and prints the results.
 */
main()
{
  printf ("SHA-%d test suite:\n", SHA);

  SHAString ("");
  SHAString ("abc");
  SHAString ("message digest");
  SHAString ("abcdefghijklmnopqrstuvwxyz");
  SHAString
    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
  SHAString
    ("1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
  return 0;
}
