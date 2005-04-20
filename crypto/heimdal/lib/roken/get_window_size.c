/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: get_window_size.c,v 1.9 1999/12/02 16:58:46 joda Exp $");
#endif

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if 0 /* Where were those needed? /confused */
#ifdef HAVE_SYS_PROC_H
#include <sys/proc.h>
#endif

#ifdef HAVE_SYS_TTY_H
#include <sys/tty.h>
#endif
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include <roken.h>

int
get_window_size(int fd, struct winsize *wp)
{
    int ret = -1;
    
    memset(wp, 0, sizeof(*wp));

#if defined(TIOCGWINSZ)
    ret = ioctl(fd, TIOCGWINSZ, wp);
#elif defined(TIOCGSIZE)
    {
	struct ttysize ts;
	
	ret = ioctl(fd, TIOCGSIZE, &ts);
	if(ret == 0) {
	    wp->ws_row = ts.ts_lines;
	    wp->ws_col = ts.ts_cols;
	}
    }
#elif defined(HAVE__SCRSIZE)
    {
	int dst[2];
	
	_scrsize(dst);
	wp->ws_row = dst[1];
	wp->ws_col = dst[0];
	ret = 0;
    }
#endif
    if (ret != 0) {
        char *s;
        if((s = getenv("COLUMNS")))
	    wp->ws_col = atoi(s);
	if((s = getenv("LINES")))
	    wp->ws_row = atoi(s);
	if(wp->ws_col > 0 && wp->ws_row > 0)
	    ret = 0;
    }
    return ret;
}
