/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <stdarg.h>

/* Whack up an informational message on the status line */
void
msgInfo(char *fmt, ...)
{
    va_list args;
    char *errstr;

    errstr = (char *)malloc(FILENAME_MAX);
    errstr[0] = '\0';
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    move(25, 0);
    addstr(errstr);
    free(errstr);
}

/* Whack up a warning on the status line */
void
msgWarn(char *fmt, ...)
{
    va_list args;
    char *errstr;

    errstr = (char *)malloc(FILENAME_MAX);
    strcpy(errstr, "Warning: ");
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    move(25, 0);
    beep();
    standout();
    addstr(errstr);
    standend();
    free(errstr);
}

/* Whack up an error on the status line */
void
msgError(char *fmt, ...)
{
    va_list args;
    char *errstr;

    errstr = (char *)malloc(FILENAME_MAX);
    strcpy(errstr, "Error: ");
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    move(25, 0);
    beep();
    standout();
    addstr(errstr);
    standend();
    free(errstr);
}

/* Whack up a fatal error on the status line */
void
msgFatal(char *fmt, ...)
{
    va_list args;
    char *errstr;

    errstr = (char *)malloc(FILENAME_MAX);
    strcpy(errstr, "Fatal Error: ");
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    move(25, 0);
    beep();
    standout();
    addstr(errstr);
    addstr(" - ");
    addstr("PRESS ANY KEY TO ");
    if (getpid() == 1)
	addstr("REBOOT");
    else
	addstr("QUIT");
    standend();
    free(errstr);
    getch();
    systemShutdown();
}

