#include <sys/types.h>
#include <stdio.h>
#include "regex.h"

#define BYTEWIDTH 8

/* Sorry, but this is just a test program.  */
#define LINE_MAX 500

int
main (argc, argv)
    int argc;
    char *argv[];
{
  FILE *f;
  char *filename;
  char pat[500]; /* Sorry for that maximum size, too.  */
  char line[LINE_MAX];
  struct re_pattern_buffer buf;
  char fastmap[(1 << BYTEWIDTH)];
  const char *compile_ret;
  unsigned lineno = 1;
  unsigned nfound = 0;
  
  /* Actually, it might be useful to allow the data file to be standard
     input, and to specify the pattern on the command line.  */
  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s <filename>.\n", argv[0]);
      exit (1);
    }
 
  filename = argv[1];
  f = fopen (filename, "r");
  if (f == NULL)
    perror (filename);

  buf.allocated = 0;
  buf.buffer = NULL;
  buf.fastmap = fastmap;

  printf ("Pattern = ", pat);
  gets (pat);
  
  if (feof (stdin))
    {
      putchar ('\n');
      exit (0);
    }

  compile_ret = re_compile_pattern (pat, strlen (pat), &buf);
  if (compile_ret != NULL)
    {
      fprintf (stderr, "%s: %s\n", pat, compile_ret);
      exit (1);
    }

  while (fgets (line, LINE_MAX, f) != NULL)
    {
      size_t len = strlen (line);
      struct re_registers regs;
      int search_ret
        = re_search_2 (&buf, NULL, 0, line, len, 0, len, &regs, len);
      
      if (search_ret == -2)
        {
          fprintf (stderr, "%s:%d: re_search failed.\n", filename, lineno);
          exit (1);
        }
      
      nfound += search_ret != -1;
      lineno++;
    }

  printf ("Matches found: %u (out of %u lines).\n", nfound, lineno - 1);
  return 0;
}
