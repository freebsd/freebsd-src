/* Sleep a given number of milliseconds.
   Copyright (C) 1992, 1993, 1994, 1997 Free Software Foundation, Inc.
   François Pinard <pinard@iro.umontreal.ca>, 1992.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* This code is heavily borrowed from Taylor UUCP 1.03.	 Ian picks one of
   usleep, nap, napms, poll, select and sleep, in decreasing order of
   preference.	The sleep function is always available.	 */

/* In many cases, we will sleep if the wanted number of milliseconds
   is higher than this value.  */
#define THRESHOLD_FOR_SLEEP 30000

/* Include some header files.  */

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_POLL
# if HAVE_STROPTS_H
#  include <stropts.h>
# endif
# if HAVE_POLL_H
#  include <sys/types.h>
#  include <poll.h>
# endif
# if !HAVE_STROPTS_H && !HAVE_POLL_H
/* We need a definition for struct pollfd, although it doesn't matter
   what it contains.  */
struct pollfd
{
  int idummy;
};
# endif
#else
# if HAVE_SELECT
#  include <sys/time.h>
# endif
#endif

/*---------------------------------------.
| Sleep a given number of milliseconds.	 |
`---------------------------------------*/

void
msleep (milliseconds)
     int milliseconds;
{
#if HAVE_USLEEP

  if (milliseconds > 0)
    usleep (milliseconds * (long) 1000);

#else
# if HAVE_NAP

  if (milliseconds > 0)
    nap ((long) milliseconds);

# else
#  if HAVE_NAPMS

  if (milliseconds >= THRESHOLD_FOR_SLEEP)
    {
      sleep (milliseconds / 1000);
      milliseconds %= 1000;
    }
  if (milliseconds > 0)
    napms (milliseconds);

#  else
#   if HAVE_POLL

  struct pollfd sdummy;		/* poll(2) checks this address */

  if (milliseconds >= THRESHOLD_FOR_SLEEP)
    {
      sleep (milliseconds / 1000);
      milliseconds %= 1000;
    }
  if (milliseconds > 0)
    poll (&sdummy, 0, milliseconds);

#   else
#    if HAVE_SELECT

  struct timeval s;

  if (milliseconds >= THRESHOLD_FOR_SLEEP)
    {
      sleep (milliseconds / 1000);
      milliseconds %= 1000;
    }
  if (milliseconds > 0)
    {
      s.tv_sec = milliseconds / 1000;
      s.tv_usec = (milliseconds % 1000) * (long) 1000;
      select (0, NULL, NULL, NULL, &s);
    }

#    else

  /* Round the time up to the next full second.  */

  if (milliseconds > 0)
    sleep ((milliseconds + 999) / 1000);

#    endif
#   endif
#  endif
# endif
#endif
}
