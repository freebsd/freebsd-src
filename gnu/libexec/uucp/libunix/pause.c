/* pause.c
   Pause for half a second.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

/* Pick a timing routine to use.  I somewhat arbitrarily picked usleep
   above nap above napms above poll above select.  */
#if HAVE_USLEEP || HAVE_NAP || HAVE_NAPMS || HAVE_POLL
#undef HAVE_SELECT
#define HAVE_SELECT 0
#endif

#if HAVE_USLEEP || HAVE_NAP || HAVE_NAPMS
#undef HAVE_POLL
#define HAVE_POLL 0
#endif

#if HAVE_USLEEP || HAVE_NAP
#undef HAVE_NAPMS
#define HAVE_NAPMS 0
#endif

#if HAVE_USLEEP
#undef HAVE_NAP
#define HAVE_NAP 0
#endif

#if HAVE_SELECT
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#if HAVE_POLL
#if HAVE_STROPTS_H
#include <stropts.h>
#endif
#if HAVE_POLL_H
#include <poll.h>
#endif
#if ! HAVE_STROPTS_H && ! HAVE_POLL_H
/* We need a definition for struct pollfd, although it doesn't matter
   what it contains.  */
struct pollfd
{
  int idummy;
};
#endif /* ! HAVE_STROPTS_H && ! HAVE_POLL_H */
#endif /* HAVE_POLL */

#if HAVE_TIME_H
#if ! HAVE_SYS_TIME_H || ! HAVE_SELECT || TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

void
usysdep_pause ()
{
#if HAVE_NAPMS
  napms (500);
#endif /* HAVE_NAPMS */
#if HAVE_NAP
#if HAVE_HUNDREDTHS_NAP
  nap (50L);
#else
  nap (500L);
#endif /* ! HAVE_HUNDREDTHS_NAP */
#endif /* HAVE_NAP */
#if HAVE_USLEEP
  usleep (500 * (long) 1000);
#endif /* HAVE_USLEEP */
#if HAVE_POLL
  struct pollfd sdummy;

  /* We need to pass an unused pollfd structure because poll checks
     the address before checking the number of elements.  */
  poll (&sdummy, 0, 500);
#endif /* HAVE_POLL */
#if HAVE_SELECT
  struct timeval s;

  s.tv_sec = 0;
  s.tv_usec = 500 * (long) 1000;
  select (0, (pointer) NULL, (pointer) NULL, (pointer) NULL, &s);
#endif /* HAVE_SELECT */
#if ! HAVE_NAPMS && ! HAVE_NAP && ! HAVE_USLEEP
#if ! HAVE_SELECT && ! HAVE_POLL
  sleep (1);
#endif /* ! HAVE_SELECT && ! HAVE_POLL */
#endif /* ! HAVE_NAPMS && ! HAVE_NAP && ! HAVE_USLEEP */
}
