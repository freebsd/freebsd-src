/* bsd-interf.c: test BSD interface.  */

#ifndef _POSIX_SOURCE /* whole file */

#include "test.h"

void
test_berk_search (pattern, string)
    const char *pattern;
    char *string;
{
  const char *return_value = re_comp (pattern);

  if (return_value != 0)
    {
      printf ("This didn't compile: `%s'.\n", pattern);
      printf ("  The error message was: `%s'.\n", return_value);
    }
  else
    if (test_should_match && re_exec (string) != strlen (string))
      {
        printf ("Should have matched but didn't:\n");
        printf ("  The pattern was: %s.\n", pattern);
        if (string)
          printf ("  The string was: `%s'.'n", string);
        else
          printf ("  The string was empty.\n");
      }
}


void
test_bsd_interface ()
{
  test_berk_search ("a", "ab");
}

#endif /* _POSIX_SOURCE */
