/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Routines dealing with getting input from the keyboard (i.e. from the user).
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#include "windows.h"
extern char WIN32getch();
static DWORD console_mode;
#endif

static int tty;
extern int sigs;

/*
 * Open keyboard for input.
 */
	public void
open_getchr()
{
#if MSDOS_COMPILER==WIN32C
	/* Need this to let child processes inherit our console handle */
	SECURITY_ATTRIBUTES sa;
	memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	tty = (int) CreateFile("CONIN$", GENERIC_READ,
			FILE_SHARE_READ, &sa, 
			OPEN_EXISTING, 0L, NULL);
	GetConsoleMode((HANDLE)tty, &console_mode);
	/* Make sure we get Ctrl+C events. */
	SetConsoleMode((HANDLE)tty, ENABLE_PROCESSED_INPUT);
#else
#if MSDOS_COMPILER || OS2
	extern int fd0;
	/*
	 * Open a new handle to CON: in binary mode 
	 * for unbuffered keyboard read.
	 */
	 fd0 = dup(0);
	 close(0);
	 tty = open("CON", OPEN_READ);
#if MSDOS_COMPILER==DJGPPC
	/*
	 * Setting stdin to binary causes Ctrl-C to not
	 * raise SIGINT.  We must undo that side-effect.
	 */
	(void) __djgpp_set_ctrl_c(1);
#endif
#else
	/*
	 * Try /dev/tty.
	 * If that doesn't work, use file descriptor 2,
	 * which in Unix is usually attached to the screen,
	 * but also usually lets you read from the keyboard.
	 */
	tty = open("/dev/tty", OPEN_READ);
	if (tty < 0)
		tty = 2;
#endif
#endif
}

/*
 * Close the keyboard.
 */
	public void
close_getchr()
{
#if MSDOS_COMPILER==WIN32C
	SetConsoleMode((HANDLE)tty, console_mode);
	CloseHandle((HANDLE)tty);
#endif
}

/*
 * Get a character from the keyboard.
 */
	public int
getchr()
{
	char c;
	int result;

	do
	{
#if MSDOS_COMPILER && MSDOS_COMPILER != DJGPPC
		/*
		 * In raw read, we don't see ^C so look here for it.
		 */
		flush();
#if MSDOS_COMPILER==WIN32C
		if (ABORT_SIGS())
			return (READ_INTR);
		c = WIN32getch(tty);
#else
		c = getch();
#endif
		result = 1;
		if (c == '\003')
			return (READ_INTR);
#else
#if OS2
	{
		static int scan = -1;
		flush();
		if (scan >= 0)
		{
			c = scan;
			scan = -1;
		} else
		{
			if ((c = _read_kbd(0, 1, 0)) == -1)
				return (READ_INTR);
			if (c == '\0')
			{
				/*
				 * Zero is usually followed by another byte,
				 * since certain keys send two bytes.
				 */
				scan = _read_kbd(0, 0, 0);
			}
		}
		result = 1;
	}
#else
		result = iread(tty, &c, sizeof(char));
		if (result == READ_INTR)
			return (READ_INTR);
		if (result < 0)
		{
			/*
			 * Don't call error() here,
			 * because error calls getchr!
			 */
			quit(QUIT_ERROR);
		}
#endif
#endif
		/*
		 * Various parts of the program cannot handle
		 * an input character of '\0'.
		 * If a '\0' was actually typed, convert it to '\340' here.
		 */
		if (c == '\0')
			c = '\340';
	} while (result != 1);

	return (c & 0377);
}
