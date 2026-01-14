/****************************************************************************
 * Copyright 2018-2020,2025 Thomas E. Dickey                                *
 * Copyright 2011-2014,2017 Free Software Foundation, Inc.                  *
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
 *  Author: Thomas E. Dickey                        2011                    *
 ****************************************************************************/

/* $Id: nc_termios.h,v 1.10 2025/10/18 17:53:13 tom Exp $ */

#ifndef NC_TERMIOS_included
#define NC_TERMIOS_included 1

#include <ncurses_cfg.h>

#if HAVE_TERMIOS_H && HAVE_TCGETATTR

#else /* !HAVE_TERMIOS_H */

#if HAVE_TERMIO_H

/* Add definitions to make termio look like termios.
 * But ifdef it, since there are some implementations
 * that try to do this for us in a fake <termio.h>.
 */
#ifndef TCSADRAIN
#define TCSADRAIN TCSETAW
#endif
#ifndef TCSAFLUSH
#define TCSAFLUSH TCSETAF
#endif
#ifndef tcsetattr
#define tcsetattr(fd, cmd, arg) ioctl(fd, cmd, arg)
#endif
#ifndef tcgetattr
#define tcgetattr(fd, arg) ioctl(fd, TCGETA, arg)
#endif
#ifndef cfgetospeed
#define cfgetospeed(t) ((t)->c_cflag & CBAUD)
#endif
#ifndef TCIFLUSH
#define TCIFLUSH 0
#endif
#ifndef tcflush
#define tcflush(fd, arg) ioctl(fd, TCFLSH, arg)
#endif

#if defined(_WIN32)
#undef TERMIOS
#endif

#else /* !HAVE_TERMIO_H */

#undef TERMIOS

#endif /* HAVE_TERMIO_H */

#endif /* HAVE_TERMIOS_H */

#endif /* NC_TERMIOS_included */
