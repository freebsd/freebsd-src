#include <stdio.h>

#ifdef READLINE_LIBRARY
#  include "history.h"
#else
#  include <readline/history.h>
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  char line[1024], *t;
  int len, done = 0;

  line[0] = 0;

  using_history ();
  while (!done)
    {
      printf ("history$ ");
      fflush (stdout);
      t = fgets (line, sizeof (line) - 1, stdin);
      if (t && *t)
        {
          len = strlen (t);
          if (t[len - 1] == '\n')
            t[len - 1] = '\0';
        }

      if (!t)
        strcpy (line, "quit");

      if (line[0])
        {
          char *expansion;
          int result;

          using_history ();

          result = history_expand (line, &expansion);
          if (result)
            fprintf (stderr, "%s\n", expansion);

          if (result < 0 || result == 2)
            {
              free (expansion);
              continue;
            }

          add_history (expansion);
          strncpy (line, expansion, sizeof (line) - 1);
          free (expansion);
        }

      if (strcmp (line, "quit") == 0)
        done = 1;
      else if (strcmp (line, "save") == 0)
        write_history ("history_file");
      else if (strcmp (line, "read") == 0)
        read_history ("history_file");
      else if (strcmp (line, "list") == 0)
        {
          register HIST_ENTRY **the_list;
          register int i;

          the_list = history_list ();
          if (the_list)
            for (i = 0; the_list[i]; i++)
              printf ("%d: %s\n", i + history_base, the_list[i]->line);
        }
      else if (strncmp (line, "delete", 6) == 0)
        {
          int which;
          if ((sscanf (line + 6, "%d", &which)) == 1)
            {
              HIST_ENTRY *entry = remove_history (which);
              if (!entry)
                fprintf (stderr, "No such entry %d\n", which);
              else
                {
                  free (entry->line);
                  free (entry);
                }
            }
          else
            {
              fprintf (stderr, "non-numeric arg given to `delete'\n");
            }
        }
    }
}
