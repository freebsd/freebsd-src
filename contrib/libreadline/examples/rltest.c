/* **************************************************************** */
/*								    */
/*			Testing Readline			    */
/*								    */
/* **************************************************************** */

#include <stdio.h>
#include <sys/types.h>
#include "../readline.h"
#include "../history.h"

main ()
{
  HIST_ENTRY **history_list ();
  char *temp = (char *)NULL;
  char *prompt = "readline$ ";
  int done = 0;

  while (!done)
    {
      temp = readline (prompt);

      /* Test for EOF. */
      if (!temp)
	exit (1);

      /* If there is anything on the line, print it and remember it. */
      if (*temp)
	{
	  fprintf (stderr, "%s\r\n", temp);
	  add_history (temp);
	}

      /* Check for `command' that we handle. */
      if (strcmp (temp, "quit") == 0)
	done = 1;

      if (strcmp (temp, "list") == 0)
	{
	  HIST_ENTRY **list = history_list ();
	  register int i;
	  if (list)
	    {
	      for (i = 0; list[i]; i++)
		{
		  fprintf (stderr, "%d: %s\r\n", i, list[i]->line);
		  free (list[i]->line);
		}
	      free (list);
	    }
	}
      free (temp);
    }
}
