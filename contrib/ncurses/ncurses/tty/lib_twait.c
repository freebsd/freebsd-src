/****************************************************************************
 * Copyright (c) 1998,1999,2000 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_twait.c
**
**	The routine _nc_timed_wait().
**
**	(This file was originally written by Eric Raymond; however except for
**	comments, none of the original code remains - T.Dickey).
*/

#ifdef __BEOS__
#include <OS.h>
#endif

#include <curses.priv.h>

#if USE_FUNC_POLL
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
#elif HAVE_SELECT
# if HAVE_SYS_TIME_H && HAVE_SYS_TIME_SELECT
#  include <sys/time.h>
# endif
# if HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif

MODULE_ID("$Id: lib_twait.c,v 1.41 2000/12/10 03:04:30 tom Exp $")

static long
_nc_gettime(bool first)
{
    long res;

#if HAVE_GETTIMEOFDAY
# define PRECISE_GETTIME 1
    static struct timeval t0;
    struct timeval t1;
    gettimeofday(&t1, (struct timezone *) 0);
    if (first) {
	t0 = t1;
    }
    res = (t1.tv_sec - t0.tv_sec) * 1000
	+ (t1.tv_usec - t0.tv_usec) / 1000;
#else
# define PRECISE_GETTIME 0
    static time_t t0;
    time_t t1 = time((time_t *) 0);
    if (first) {
	t0 = t1;
    }
    res = (t1 - t0) * 1000;
#endif
    T(("%s time: %ld msec", first ? "get" : "elapsed", res));
    return res;
}

/*
 * Wait a specified number of milliseconds, returning nonzero if the timer
 * didn't expire before there is activity on the specified file descriptors.
 * The file-descriptors are specified by the mode:
 *	0 - none (absolute time)
 *	1 - ncurses' normal input-descriptor
 *	2 - mouse descriptor, if any
 *	3 - either input or mouse.
 * We return a mask that corresponds to the mode (e.g., 2 for mouse activity).
 *
 * If the milliseconds given are -1, the wait blocks until activity on the
 * descriptors.
 */
NCURSES_EXPORT(int)
_nc_timed_wait
(int mode, int milliseconds, int *timeleft)
{
    int fd;
    int count;

    int result;

#if USE_FUNC_POLL
    struct pollfd fds[2];
#elif defined(__BEOS__)
#elif HAVE_SELECT
    static fd_set set;
#endif

    long starttime, returntime;

    T(("start twait: %d milliseconds, mode: %d", milliseconds, mode));

#if PRECISE_GETTIME
  retry:
#endif
    starttime = _nc_gettime(TRUE);

    count = 0;

#if USE_FUNC_POLL
    memset(fds, 0, sizeof(fds));
    if (mode & 1) {
	fds[count].fd = SP->_ifd;
	fds[count].events = POLLIN;
	count++;
    }
    if ((mode & 2)
	&& (fd = SP->_mouse_fd) >= 0) {
	fds[count].fd = fd;
	fds[count].events = POLLIN;
	count++;
    }
    result = poll(fds, count, milliseconds);

#elif defined(__BEOS__)
    /*
     * BeOS's select() is declared in socket.h, so the configure script does
     * not see it.  That's just as well, since that function works only for
     * sockets.  This (using snooze and ioctl) was distilled from Be's patch
     * for ncurses which uses a separate thread to simulate select().
     *
     * FIXME: the return values from the ioctl aren't very clear if we get
     * interrupted.
     */
    result = 0;
    if (mode & 1) {
	bigtime_t d;
	bigtime_t useconds = milliseconds * 1000;
	int n, howmany;

	if (useconds == 0)	/* we're here to go _through_ the loop */
	    useconds = 1;

	for (d = 0; d < useconds; d += 5000) {
	    n = 0;
	    howmany = ioctl(0, 'ichr', &n);
	    if (howmany >= 0 && n > 0) {
		result = 1;
		break;
	    }
	    if (useconds > 1)
		snooze(5000);
	    milliseconds -= 5;
	}
    } else if (milliseconds > 0) {
	snooze(milliseconds * 1000);
	milliseconds = 0;
    }
#elif HAVE_SELECT
    /*
     * select() modifies the fd_set arguments; do this in the
     * loop.
     */
    FD_ZERO(&set);

    if (mode & 1) {
	FD_SET(SP->_ifd, &set);
	count = SP->_ifd + 1;
    }
    if ((mode & 2)
	&& (fd = SP->_mouse_fd) >= 0) {
	FD_SET(fd, &set);
	count = max(fd, count) + 1;
    }

    if (milliseconds >= 0) {
	struct timeval ntimeout;
	ntimeout.tv_sec = milliseconds / 1000;
	ntimeout.tv_usec = (milliseconds % 1000) * 1000;
	result = select(count, &set, NULL, NULL, &ntimeout);
    } else {
	result = select(count, &set, NULL, NULL, NULL);
    }
#endif

    returntime = _nc_gettime(FALSE);

    if (milliseconds >= 0)
	milliseconds -= (returntime - starttime);

#if PRECISE_GETTIME
    /*
     * If the timeout hasn't expired, and we've gotten no data,
     * this is probably a system where 'select()' needs to be left
     * alone so that it can complete.  Make this process sleep,
     * then come back for more.
     */
    if (result == 0 && milliseconds > 100) {
	napms(100);
	milliseconds -= 100;
	goto retry;
    }
#endif

    /* return approximate time left in milliseconds */
    if (timeleft)
	*timeleft = milliseconds;

    T(("end twait: returned %d (%d), remaining time %d msec",
       result, errno, milliseconds));

    /*
     * Both 'poll()' and 'select()' return the number of file descriptors
     * that are active.  Translate this back to the mask that denotes which
     * file-descriptors, so that we don't need all of this system-specific
     * code everywhere.
     */
    if (result != 0) {
	if (result > 0) {
	    result = 0;
#if USE_FUNC_POLL
	    for (count = 0; count < 2; count++) {
		if ((mode & (1 << count))
		    && (fds[count].revents & POLLIN)) {
		    result |= (1 << count);
		}
	    }
#elif defined(__BEOS__)
	    result = 1;		/* redundant, but simple */
#elif HAVE_SELECT
	    if ((mode & 2)
		&& (fd = SP->_mouse_fd) >= 0
		&& FD_ISSET(fd, &set))
		result |= 2;
	    if ((mode & 1)
		&& FD_ISSET(SP->_ifd, &set))
		result |= 1;
#endif
	} else
	    result = 0;
    }

    return (result);
}
