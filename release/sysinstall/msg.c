/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: msg.c,v 1.4 1995/05/04 19:48:16 jkh Exp $
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

/* Whack up an informational message on the status line, in stand-out */
void
msgYap(char *fmt, ...)
{
    va_list args;
    char *errstr;
    int attrs;

    errstr = (char *)malloc(FILENAME_MAX);
    errstr[0] = '\0';
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    attrs = getattrs(stdscr);
    attrset(A_BOLD);
    mvaddstr(23, 0, errstr);
    attrset(attrs);
    refresh();
    free(errstr);
}

/* Whack up an informational message on the status line */
void
msgInfo(char *fmt, ...)
{
    va_list args;
    char *errstr;
    int attrs;

    errstr = (char *)malloc(FILENAME_MAX);
    errstr[0] = '\0';
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    attrs = getattrs(stdscr);
    attrset(A_NORMAL);
    mvaddstr(23, 0, errstr);
    attrset(attrs);
    refresh();
    free(errstr);
}

/* Whack up a warning on the status line */
void
msgWarn(char *fmt, ...)
{
    va_list args;
    char *errstr;
    int attrs;

    errstr = (char *)malloc(FILENAME_MAX);
    strcpy(errstr, "Warning: ");
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    attrs = getattrs(stdscr);
    beep();
    attrset(A_BOLD);
    mvaddstr(23, 0, errstr);
    attrset(attrs);
    refresh();
    free(errstr);
}

/* Whack up an error on the status line */
void
msgError(char *fmt, ...)
{
    va_list args;
    char *errstr;
    int attrs;

    errstr = (char *)malloc(FILENAME_MAX);
    strcpy(errstr, "Error: ");
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    beep();
    attrs = getattrs(stdscr);
    attrset(A_BOLD);
    mvaddstr(23, 0, errstr);
    attrset(attrs);
    refresh();
    free(errstr);
}

/* Whack up a fatal error on the status line */
void
msgFatal(char *fmt, ...)
{
    va_list args;
    char *errstr;
    int attrs;

    errstr = (char *)malloc(FILENAME_MAX);
    strcpy(errstr, "Fatal Error: ");
    va_start(args, fmt);
    vsnprintf((char *)(errstr + strlen(errstr)), FILENAME_MAX, fmt, args);
    va_end(args);
    beep();
    attrs = getattrs(stdscr);
    attrset(A_BOLD);
    mvaddstr(23, 0, errstr);
    addstr(" - ");
    addstr("PRESS ANY KEY TO ");
    if (getpid() == 1)
	addstr("REBOOT");
    else
	addstr("QUIT");
    attrset(attrs);
    refresh();
    free(errstr);
    getch();
    systemShutdown();
}

/* Put up a message in a popup confirmation box */
void
msgConfirm(char *fmt, ...)
{
    va_list args;
    char *errstr;

    errstr = (char *)malloc(FILENAME_MAX);
    va_start(args, fmt);
    vsnprintf(errstr, FILENAME_MAX, fmt, args);
    va_end(args);
    use_helpline(NULL);
    use_helpfile(NULL);
    dialog_mesgbox("User Attention Requested", errstr, -1, -1);
    free(errstr);
}

/* Put up a message in a popup yes/no box and return 1 for YES, 0 for NO */
int
msgYesNo(char *fmt, ...)
{
    va_list args;
    char *errstr;
    int ret;

    errstr = (char *)malloc(FILENAME_MAX);
    va_start(args, fmt);
    vsnprintf(errstr, FILENAME_MAX, fmt, args);
    va_end(args);
    use_helpline(NULL);
    use_helpfile(NULL);
    ret = dialog_yesno("User Confirmation Requested", errstr, -1, -1);
    free(errstr);
    return ret;
}

/* Put up a message in an input box and return the value */
char *
msgGetInput(char *buf, char *fmt, ...)
{
    va_list args;
    char *errstr;
    static char input_buffer[256];
    int rval;

    errstr = (char *)malloc(FILENAME_MAX);
    va_start(args, fmt);
    vsnprintf(errstr, FILENAME_MAX, fmt, args);
    va_end(args);
    use_helpline(NULL);
    use_helpfile(NULL);
    strcpy(input_buffer, buf);
    rval = dialog_inputbox("Value Required", errstr, -1, -1, input_buffer);
    free(errstr);
    if (!rval)
	return input_buffer;
    else
	return NULL;
}

