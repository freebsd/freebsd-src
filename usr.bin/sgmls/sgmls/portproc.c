/* portproc.c -

   Semi-portable implementation of run_process().

     Written by James Clark (jjc@jclark.com).
*/

#include "config.h"

#ifdef SUPPORT_SUBDOC

#include "std.h"
#include "entity.h"
#include "appl.h"

/* This code shows how you might use system() to implement run_process().
ANSI C says very little about the behaviour of system(), and so this
is necessarily system dependent. */

/* Characters that are significant to the shell and so need quoting. */
#define SHELL_MAGIC "$\\\"';&()|<>^ \t\n"
/* Character with which to quote shell arguments. */
#define SHELL_QUOTE_CHAR '\''
/* String that can be used to get SHELL_QUOTE_CHAR into a quoted argument. */
#define SHELL_ESCAPE_QUOTE "'\\''"
/* Character that can be used to separate arguments to the shell. */
#define SHELL_ARG_SEP ' '

static UNS shell_quote P((char *, char *));

int run_process(argv)
char **argv;
{
      char **p;
      char *s, *command;
      int ret;
      UNS len = 0;

      for (p = argv; *p; p++)
	   len += shell_quote(*p, (char *)0);
      len += p - argv;
      s = command = xmalloc(len);
      for (p = argv; *p; ++p) {
	   if (s > command)
		*s++ = SHELL_ARG_SEP;
	   s += shell_quote(*p, s);
      }
      *s++ = '\0';
      errno = 0;
      ret = system(command);
      if (ret < 0)
	   appl_error(E_EXEC, argv[0], strerror(errno));
      free(command);
      return ret;
}

/* Quote a string so that it appears as a single argument to the
shell (as used for system()).  Store the quoted argument in result, if
result is not NULL.  Return the length. */

static
UNS shell_quote(s, result)
char *s, *result;
{
     UNS len = 0;
     int quoted = 0;

     if (strpbrk(s, SHELL_MAGIC)) {
	  quoted = 1;
	  len++;
	  if (result)
	       result[0] = SHELL_QUOTE_CHAR;
     }
     for (; *s; s++) {
	  if (*s == SHELL_QUOTE_CHAR) {
	       if (result)
		    strcpy(result + len, SHELL_ESCAPE_QUOTE);
	       len += strlen(SHELL_ESCAPE_QUOTE);
	  }
	  else {
	       if (result)
		    result[len] = *s;
	       len++;
	  }
     }
     if (quoted) {
	  if (result)
	       result[len] = SHELL_QUOTE_CHAR;
	  len++;
     }
     return len;
}

#endif /* SUPPORT_SUBDOC */

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
