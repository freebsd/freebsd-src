/*
   Tests the generated perfect hash function.
   The -v option prints diagnostics as to whether a word is in 
   the set or not.  Without -v the program is useful for timing.
*/ 
  
#include <stdio.h>

#define MAX_LEN 80

int 
main (argc, argv) 
     int   argc;
     char *argv[];
{
  int  verbose = argc > 1 ? 1 : 0;
  char buf[MAX_LEN];

  while (gets (buf)) 
    if (in_word_set (buf, strlen (buf)) && verbose) 
      printf ("in word set %s\n", buf);
    else if (verbose) 
      printf ("NOT in word set %s\n", buf);

  return 0;
}
