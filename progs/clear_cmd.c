/****************************************************************************
 * Copyright 2018,2020,2025 Thomas E. Dickey                                *
 * Copyright 2016,2017 Free Software Foundation, Inc.                       *
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
 *  Author: Thomas E. Dickey                                                *
 ****************************************************************************/

/*
 * clear.c --  clears the terminal's screen
 */

#define USE_LIBTINFO
#include <clear_cmd.h>

MODULE_ID("$Id: clear_cmd.c,v 1.8 2025/12/06 21:00:26 tom Exp $")

#ifdef TERMIOS
static int
putch(int c)
{
    return putchar(c);
}
#endif

int
clear_cmd(bool legacy)
{
    int retval;
#ifdef TERMIOS
    retval = tputs(clear_screen, lines > 0 ? lines : 1, putch);
    if (!legacy) {
	/* Clear the scrollback buffer if possible. */
	char *E3 = tigetstr(UserCap(E3));
	if (VALID_STRING(E3))
	    (void) tputs(E3, lines > 0 ? lines : 1, putch);
    }
#elif defined(_NC_WINDOWS)
    /*
     * https://learn.microsoft.com/en-us/windows/console/clearing-the-screen
     */
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen =
    {0, 0};
#if defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    DWORD mode = 0;

    retval = ERR;

    if (GetConsoleMode(hConsole, &mode)) {
	const DWORD originalMode = mode;
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

	if (SetConsoleMode(hConsole, mode)) {
	    DWORD written = 0;
	    PCWSTR sequence = legacy ? L"\x1b[2J" : L"\x1b[2J\x1b[3J";
	    if (WriteConsoleW(hConsole, sequence,
			      (DWORD) wcslen(sequence),
			      &written, NULL)) {
		SetConsoleCursorPosition(hConsole, coordScreen);
		retval = OK;
	    }
	    SetConsoleMode(hConsole, originalMode);
	}
    }
#else
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    (void) legacy;
    retval = ERR;

    /* Get the number of character cells in the current buffer,
     * to fill the entire screen with blanks */
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)
	&& (dwConSize = csbi.dwSize.X * csbi.dwSize.Y) > 0
	&& FillConsoleOutputCharacter(hConsole,
				      (TCHAR) ' ',
				      dwConSize,
				      coordScreen,
				      &cCharsWritten)
	&& FillConsoleOutputAttribute(hConsole,
				      csbi.wAttributes,
				      dwConSize,
				      coordScreen,
				      &cCharsWritten)) {
	SetConsoleCursorPosition(hConsole, coordScreen);
	retval = OK;
    }
#endif
#endif
    return retval;
}
