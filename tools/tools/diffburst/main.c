#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

/* Split a recursive diff into sub-patches to satisfy the requirement
  that each patch only patch a single file */

int
main(int ac, char *av[])
{
  int	ln;
  char	line[8192];
  char	suf[2] = { 'a', 'a' };
  FILE	*fp = NULL;

  memset(line, 0, sizeof(line));
  for (ln = 1; fgets(line, sizeof(line) - 1, stdin); ln++)
  {
    if (line[strlen(line) - 1] != '\n')
      errx(1, "line %d is too long", ln);
    if (!strncmp(line, "diff", 4) && fp != NULL)
    {
      fclose(fp);
      fp = NULL;
    }
    if (fp == NULL)
    {
      char	name[200];

      snprintf(name, sizeof(name), "patch-%c%c", suf[0], suf[1]);
      if (suf[1]++ == 'z')
      {
	suf[1] = 'a';
	suf[0]++;
      }
      if ((fp = fopen(name, "w")) == NULL)
	err(1, "%s", name);
    }
    fprintf(fp, "%s", line);
  }
  return(0);
}

