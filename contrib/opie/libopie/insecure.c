/* insecure.c: The opieinsecure() library function.

%%% portions-copyright-cmetz-96
Portions of this software are Copyright 1996-1998 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

Portions of this software are Copyright 1995 by Randall Atkinson and Dan
McDonald, All Rights Reserved. All Rights under this copyright are assigned
to the U.S. Naval Research Laboratory (NRL). The NRL Copyright Notice and
License Agreement applies to this software.

        History:

	Modified by cmetz for OPIE 2.31. Fixed a logic bug. Call endut[x]ent().
	Modified by cmetz for OPIE 2.3. Added result caching. Use
	     __opiegetutmpentry(). Ifdef around ut_host check. Eliminate
	     unused variable.
	Modified by cmetz for OPIE 2.2. Use FUNCTION declaration et al.
             Allow IP loopback. DISPLAY and ut_host must match exactly,
             not just the part before the colon. Added work-around for 
             Sun CDE dtterm bug. Leave the environment as it was
             found. Use uname().
        Created at NRL for OPIE 2.2 from opiesubr.c. Fixed pointer
             assignment that should have been a comparison.
*/
#include "opie_cfg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>	/* ANSI C standard library */
#include <sys/param.h>
#include <unistd.h>

#include <utmp.h>
#if DOUTMPX
#include <utmpx.h>
#define utmp utmpx
#define endutent endutxent
#endif	/* DOUTMPX */

#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif /* HAVE_SYS_UTSNAME_H */

#include "opie.h"

char *remote_terms[] = { "xterm", "xterms", "kterm", NULL };

int opieinsecure FUNCTION_NOARGS
{
#ifndef NO_INSECURE_CHECK
  char *display_name;
  char *s;
  char *term_name;
  int  insecure = 0;
#if HAVE_UT_HOST
  struct utmp utmp;
#endif /* HAVE_UT_HOST */
  static int result = -1;

  if (result != -1)
    return result;

  display_name = (char *) getenv("DISPLAY");
  term_name = (char *) getenv("TERM");

  if (display_name) {
    insecure = 1;
    if (s = strchr(display_name, ':')) {
      int n = s - display_name;
      if (!n)
	insecure = 0;
      else {
	if (!strncmp("unix", display_name, n))
	  insecure = 0;
        else if (!strncmp("localhost", display_name, n))
	    insecure = 0;
        else if (!strncmp("loopback", display_name, n))
	    insecure = 0;
        else if (!strncmp("127.0.0.1", display_name, n))
	    insecure = 0;
	else {
          struct utsname utsname;

	  if (!uname(&utsname)) {
	    if (!strncmp(utsname.nodename, display_name, n))
	      insecure = 0;
	    else {
	      if (s = strchr(display_name, '.')) {
		int n2 = s - display_name;
                if (n < n2)
                  n2 = n;
		if (!strncmp(utsname.nodename, display_name, n2))
		  insecure = 0;
	      } /* endif display_name is '.' */
	    } /* endif hostname != display_name */
	  } /* endif was able to get hostname */
	} /* endif display_name == UNIX */
      }
    }
    } /* endif display_name == ":" */ 
    if (insecure)
      return (result = 1);

  /* If no DISPLAY variable exists and TERM=xterm, 
     then we probably have an xterm executing on a remote system 
     with an rlogin or telnet to our system.  If it were a local
     xterm, then the DISPLAY environment variable would
     have to exist. rja */
  if (!display_name && term_name) {
    int i;
    for (i = 0; remote_terms[i]; i++)
      if (!strcmp(term_name, remote_terms[i]))
        return (result = 1);
  };

#if HAVE_UT_HOST
  if (isatty(0)) {
    memset(&utmp, 0, sizeof(struct utmp));
    {
      int i = __opiegetutmpentry(ttyname(0), &utmp);
      endutent();
      if (!i && utmp.ut_host[0]) {
	insecure = 1;

	if (s = strchr(utmp.ut_host, ':')) {
	  int n = s - utmp.ut_host;
	  if (!n)
	    insecure = 0;
	  else
	    if (display_name) {
	      if (!strncmp(utmp.ut_host, display_name, n))
		insecure = 0;
#ifdef SOLARIS
	      else
		if (s = strchr(utmp.ut_host, ' ')) {
		  *s = ':';
		  if (s = strchr(s + 1, ' '))
		    *s = '.';
		  if (!strncmp(utmp.ut_host, display_name, n))
		    insecure = 0; 
		}
#endif /* SOLARIS */
	    }
	}
      }
    };
  };
#endif /* HAVE_UT_HOST */
  if (insecure)
    return (result = 1);

  return (result = 0);
#else /* NO_INSECURE_CHECK */
  return 0;
#endif /* NO_INSECURE_CHECK */
}
