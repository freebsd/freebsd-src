/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Routines dealing with getting input from the keyboard (i.e. from the user).
 */

#include "less.h"
#if OS2
#include "cmd.h"
#include "pckeys.h"
#endif
#if MSDOS_COMPILER==WIN32C
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x400
#endif
#include <windows.h>
#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_EXTENDED_FLAGS 0x80
#define ENABLE_QUICK_EDIT_MODE 0x40
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif
public HANDLE tty;
public DWORD init_console_input_mode;
public DWORD curr_console_input_mode;
public DWORD base_console_input_mode;
public DWORD mouse_console_input_mode;
#else
public int tty;
#endif
extern int sigs;
#if LESSTEST
public char *ttyin_name = NULL;
public lbool is_lesstest(void)
{
	return ttyin_name != NULL;
}
#endif /*LESSTEST*/

#if !MSDOS_COMPILER
static int open_tty_device(constant char* dev)
{
#if OS2
	/* The __open() system call translates "/dev/tty" to "con". */
	return __open(dev, OPEN_READ);
#else
	return open(dev, OPEN_READ);
#endif
}

/*
 * Open the tty device.
 * Try ttyname(), then try /dev/tty, then use file descriptor 2.
 * In Unix, file descriptor 2 is usually attached to the screen,
 * but also usually lets you read from the keyboard.
 */
public int open_tty(void)
{
	int fd = -1;
#if LESSTEST
	if (is_lesstest())
	{
		fd = open_tty_device(ttyin_name);
		if (fd < 0)
			fd = 0; /* assume lesstest uses stdin */
	}
#endif /*LESSTEST*/
#if HAVE_TTYNAME
	if (fd < 0)
	{
		constant char *dev = ttyname(2);
		if (dev != NULL)
			fd = open_tty_device(dev);
	}
#endif
	if (fd < 0)
		fd = open_tty_device("/dev/tty");
	if (fd < 0)
		fd = 2;
#ifdef __MVS__
	struct f_cnvrt cvtreq = {SETCVTON, 0, 1047};
	fcntl(fd, F_CONTROL_CVT, &cvtreq);
#endif
	return fd;
}
#endif /* MSDOS_COMPILER */

/*
 * Open keyboard for input.
 */
public void open_getchr(void)
{
#if MSDOS_COMPILER==WIN32C
	/* Need this to let child processes inherit our console handle */
	SECURITY_ATTRIBUTES sa;
	memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	tty = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ, &sa, 
			OPEN_EXISTING, 0L, NULL);
	GetConsoleMode(tty, &init_console_input_mode);
	/* base mode: ensure we get ctrl-C events, and don't get VT input. */
	base_console_input_mode = (init_console_input_mode | ENABLE_PROCESSED_INPUT) & ~ENABLE_VIRTUAL_TERMINAL_INPUT;
	/* mouse mode: enable mouse and disable quick edit. */
	mouse_console_input_mode = (base_console_input_mode | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;
	/* Start with base mode. If --mouse is given, switch to mouse mode in init_mouse. */
	curr_console_input_mode = base_console_input_mode;
	SetConsoleMode(tty, curr_console_input_mode);
#else
#if MSDOS_COMPILER
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
	tty = open_tty();
#endif
#endif
}

/*
 * Close the keyboard.
 */
public void close_getchr(void)
{
#if MSDOS_COMPILER==WIN32C
	SetConsoleMode(tty, init_console_input_mode);
	CloseHandle(tty);
#endif
}

#if MSDOS_COMPILER==WIN32C
/*
 * Close the pipe, restoring the console mode (CMD resets it, losing the mouse).
 */
public int pclose(FILE *f)
{
	int result;

	result = _pclose(f);
	SetConsoleMode(tty, curr_console_input_mode);
	return result;
}
#endif

/*
 * Get the number of lines to scroll when mouse wheel is moved.
 */
public int default_wheel_lines(void)
{
	int lines = 1;
#if MSDOS_COMPILER==WIN32C
	if (SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0))
	{
		if (lines == WHEEL_PAGESCROLL)
			lines = 3;
	}
#endif
	return lines;
}

/*
 * Get a character from the keyboard.
 */
public int getchr(void)
{
	char c;
	ssize_t result;

	do
	{
		flush();
#if MSDOS_COMPILER && MSDOS_COMPILER != DJGPPC
		/*
		 * In raw read, we don't see ^C so look here for it.
		 */
#if MSDOS_COMPILER==WIN32C
#if 0
		if (ABORT_SIGS())
			return (READ_INTR);
#endif
		c = WIN32getch();
#else
		c = getch();
#endif
		result = 1;
		if (c == '\003')
			return (READ_INTR);
#else
		{
			unsigned char uc;
			result = iread(tty, &uc, sizeof(char));
			c = (char) uc;
		}
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
#if LESSTEST
		if (c == LESS_DUMP_CHAR)
		{
			dump_screen();
			result = 0;
			continue;
		}
#endif
#if 0 /* allow entering arbitrary hex chars for testing */
		/* ctrl-A followed by two hex chars makes a byte */
	{
		static int hex_in = 0;
		static int hex_value = 0;
		if (c == CONTROL('A'))
		{
			hex_in = 2;
			result = 0;
			continue;
		}
		if (hex_in > 0)
		{
			int v;
			if (c >= '0' && c <= '9')
				v = c - '0';
			else if (c >= 'a' && c <= 'f')
				v = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				v = c - 'A' + 10;
			else
				v = 0;
			hex_value = (hex_value << 4) | v;
			if (--hex_in > 0)
			{
				result = 0;
				continue;
			}
			c = hex_value;
		}
	}
#endif
		/*
		 * Various parts of the program cannot handle
		 * an input character of '\0'.
		 * If a '\0' was actually typed, convert it to '\340' here.
		 */
		if (c == '\0')
			c = '\340';
	} while (result != 1);

	return (c & 0xFF);
}
