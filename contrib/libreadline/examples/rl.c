/*
 * rl - command-line interface to read a line from the standard input
 *      (or another fd) using readline.
 *
 * usage: rl [-p prompt] [-u unit] [-d default]
 */

/*
 * Remove the next line if you're compiling this against an installed
 * libreadline.a
 */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include "posixstat.h"
#include "readline.h"
#include "history.h"

extern int optind;
extern char *optarg;

extern char *strrchr();

static char *progname;
static char *deftext;

static int
set_deftext ()
{
  if (deftext)
    {
      rl_insert_text (deftext);
      deftext = (char *)NULL;
      rl_startup_hook = (Function *)NULL;
    }
}

usage()
{
  fprintf (stderr, "%s: usage: %s [-p prompt] [-u unit] [-d default]\n",
		progname, progname);
}

main (argc, argv)
     int argc;
     char **argv;
{
  char *temp, *prompt;
  struct stat sb;
  int done, opt, fd;
  FILE *ifp;

  progname = strrchr(argv[0], '/');
  if (progname == 0)
    progname = argv[0];
  else
    progname++;

  /* defaults */
  prompt = "readline$ ";
  fd = 0;
  deftext = (char *)0;

  while ((opt = getopt(argc, argv, "p:u:d:")) != EOF)
    {
      switch (opt)
	{
	case 'p':
	  prompt = optarg;
	  break;
	case 'u':
	  fd = atoi(optarg);
	  if (fd < 0)
	    {
	      fprintf (stderr, "%s: bad file descriptor `%s'\n", progname, optarg);
	      exit (2);
	    }
	  break;
	case 'd':
	  deftext = optarg;
	  break;
	default:
	  usage ();
	  exit (2);
	}
    }

  if (fd != 0)
    {
      if (fstat (fd, &sb) < 0)
	{
	  fprintf (stderr, "%s: %d: bad file descriptor\n", progname, fd);
	  exit (1);
	}
      ifp = fdopen (fd, "r");
      rl_instream = ifp;
    }

  if (deftext && *deftext)
    rl_startup_hook = set_deftext;

  temp = readline (prompt);

  /* Test for EOF. */
  if (temp == 0)
    exit (1);

  puts (temp);
  exit (0);
}
