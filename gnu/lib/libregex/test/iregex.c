/* Main program for interactive testing.  For maximum output, compile
   this and regex.c with -DDEBUG.  */

#include <stdio.h>
#include <sys/types.h>
#include "regex.h"

/* Don't bother to guess about <string.h> vs <strings.h>, etc.  */
extern int strlen ();

#define BYTEWIDTH 8

extern void printchar ();
extern char upcase[];

static void scanstring ();
static void print_regs ();

int
main (argc, argv)
     int argc;
     char **argv;
{
  int i;
  struct re_pattern_buffer buf;
  char fastmap[(1 << BYTEWIDTH)];

  /* Allow a command argument to specify the style of syntax.  You can
     use the `syntax' program to decode integer syntax values.  */
  if (argc > 1)
    re_set_syntax (atoi (argv[1]));

  buf.allocated = 0;
  buf.buffer = NULL;
  buf.fastmap = fastmap;
  buf.translate = upcase;

  for (;;)
    {
      char pat[500], str[500];
      struct re_registers regs;

      /* Some C compilers don't like `char pat[500] = ""'.  */
      pat[0] = 0;

      printf ("Pattern (%s) = ", pat);
      gets (pat);
      scanstring (pat);

      if (feof (stdin))
        {
          putchar ('\n');
          exit (0);
	}

      if (*pat)
	{
          re_compile_pattern (pat, strlen (pat), &buf);
	  re_compile_fastmap (&buf);
#ifdef DEBUG
	  print_compiled_pattern (&buf);
#endif
	}

      printf ("String = ");
      gets (str);	/* Now read the string to match against */
      scanstring (str);

      i = re_match (&buf, str, strlen (str), 0, &regs);
      printf ("Match value  %d.\t", i);
      if (i >= 0)
        print_regs (regs);
      putchar ('\n');

      i = re_search (&buf, str, strlen (str), 0, strlen (str), &regs);
      printf ("Search value %d.\t", i);
      if (i >= 0)
        print_regs (regs);
      putchar ('\n');
    }

  /* We never get here, but what the heck.  */
  return 0;
}

void
scanstring (s)
     char *s;
{
  char *write = s;

  while (*s != '\0')
    {
      if (*s == '\\')
	{
	  s++;

	  switch (*s)
	    {
	    case '\0':
	      break;

	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	      *write = *s++ - '0';

	      if ('0' <= *s && *s <= '9')
		{
		  *write = (*write << 3) + (*s++ - '0');
		  if ('0' <= *s && *s <= '9')
		    *write = (*write << 3) + (*s++ - '0');
		}
	      write++;
	      break;

	    case 'n':
	      *write++ = '\n';
	      s++;
	      break;

	    case 't':
	      *write++ = '\t';
	      s++;
	      break;

	    default:
	      *write++ = *s++;
	      break;
	    }
	}
      else
	*write++ = *s++;
    }

  *write++ = '\0';
}

/* Print REGS in human-readable form.  */

void
print_regs (regs)
     struct re_registers regs;
{
  int i, end;

  printf ("Registers: ");

  if (regs.num_regs == 0 || regs.start[0] == -1)
    {
      printf ("(none)");
    }
  else
    {
      /* Find the last register pair that matched.  */
      for (end = regs.num_regs - 1; end >= 0; end--)
        if (regs.start[end] != -1)
          break;

      printf ("[%d ", regs.start[0]);
      for (i = 1; i <= end; i++)
        printf ("(%d %d) ", regs.start[i], regs.end[i]);
      printf ("%d]", regs.end[0]);
    }
}
