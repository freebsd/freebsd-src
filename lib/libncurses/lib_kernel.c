
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	lib_kernel.c
 *
 *	Misc. low-level routines:
 *		reset_prog_mode()
 *		reset_shell_mode()
 *		baudrate()
 *		erasechar()
 *		killchar()
 *		flushinp()
 *		savetty()
 *		resetty()
 *
 *
 */

#include "curses.priv.h"
#include "terminfo.h"

int wattron(WINDOW *win, chtype at)
{
	T(("wattron(%x,%s) current = %s", win, _traceattr(at), _traceattr(win->_attrs)));
	if (PAIR_NUMBER(at) > 0x00) {
		win->_attrs = (win->_attrs & ~A_COLOR) | at ;
		T(("new attribute is %s", _traceattr(win->_attrs)));
	} else {
  		win->_attrs |= at;
		T(("new attribute is %s", _traceattr(win->_attrs)));
	}
	return OK;
}

int wattroff(WINDOW *win, chtype at)
{
#define IGNORE_COLOR_OFF FALSE

	T(("wattroff(%x,%s) current = %s", win, _traceattr(at), _traceattr(win->_attrs)));
	if (IGNORE_COLOR_OFF == TRUE) {
		if (PAIR_NUMBER(at) == 0xff) /* turn off color */
			win->_attrs &= ~at;
		else /* leave color alone */
			win->_attrs &= ~(at & ~A_COLOR);
	} else {
		if (PAIR_NUMBER(at) > 0x00) /* turn off color */
			win->_attrs &= ~at;
		else /* leave color alone */
			win->_attrs &= ~(at & ~A_COLOR);
	}
	T(("new attribute is %s", _traceattr(win->_attrs)));
  	return OK;
}

int reset_prog_mode()
{
	int ret = ERR;

	T(("reset_prog_mode() called"));

	if (cur_term != 0) {
		if (tcsetattr(cur_term->fd, TCSADRAIN, &cur_term->prog_mode)==0)
			ret = OK;
		if (SP && stdscr && stdscr->_use_keypad)
			_nc_keypad(TRUE);
	}
	return ret;
}


int reset_shell_mode()
{
	int ret = ERR;

	T(("reset_shell_mode() called"));

	if (cur_term != 0) {
		if (SP)
		{
			fflush(SP->_ofp);
			_nc_keypad(FALSE);
		}
		if (tcsetattr(cur_term->fd, TCSADRAIN, &cur_term->shell_mode)==0)
			ret = OK;
	}

	return ret;
}

int curs_set(int vis)
{
int cursor = SP->_cursor;

	T(("curs_set(%d)", vis));

	if (vis < 0 || vis > 2)
		return ERR;

	switch(vis) {
	case 2:
		if (cursor_visible)
			putp(cursor_visible);
		break;
	case 1:
		if (cursor_normal)
			putp(cursor_normal);
		break;
	case 0:
		if (cursor_invisible)
			putp(cursor_invisible);
		break;
	}
	SP->_cursor = vis;
	return cursor;
}

int delay_output(int ms)
{
int speed = 0;

	T(("delay_output(%d) called", ms));

	if (!no_pad_char && (speed = baudrate()) == ERR)
		return(ERR);
	else {
		register int    nullcount;

		if (!no_pad_char)
			for (nullcount = ms * 1000 / speed; nullcount > 0; nullcount--)
				putc(*pad_char, SP->_ofp);
		(void) fflush(SP->_ofp);
		if (no_pad_char)
			napms(ms);
	}

      	return OK;
}

/*
 *	erasechar()
 *
 *	Return erase character as given in cur_term->Ottyb.
 *
 */

char
erasechar()
{
	T(("erasechar() called"));

#ifdef TERMIOS
    return(cur_term->Ottyb.c_cc[VERASE]);
#else
    return(cur_term->Ottyb.sg_erase);
#endif

}



/*
 *	killchar()
 *
 *	Return kill character as given in cur_term->Ottyb.
 *
 */

char
killchar()
{
	T(("killchar() called"));

#ifdef TERMIOS
    return(cur_term->Ottyb.c_cc[VKILL]);
#else
    return(cur_term->Ottyb.sg_kill);
#endif
}



/*
 *	flushinp()
 *
 *	Flush any input on cur_term->Filedes
 *
 */

int flushinp()
{
	T(("flushinp() called"));

#ifdef TERMIOS
	tcflush(cur_term->Filedes, TCIFLUSH);
#else
    ioctl(cur_term->Filedes, TIOCFLUSH, 0);
#endif
    if (SP) {
	  	SP->_fifohead = -1;
	  	SP->_fifotail = 0;
	  	SP->_fifopeek = 0;
	}
	return OK;

}



/*
 *	int
 *	baudrate()
 *
 *	Returns the current terminal's baud rate.
 *
 */

#ifndef TERMIOS
struct speed {
	speed_t s;
	int sp;
};

static struct speed speeds[] = {
	{B0, 0},
	{B50, 50},
	{B75, 75},
	{B110, 110},
	{B134, 134},
	{B150, 150},
	{B200, 200},
	{B300, 300},
	{B600, 600},
	{B1200, 1200},
	{B1800, 1800},
	{B2400, 2400},
	{B4800, 4800},
	{B9600, 9600}
#define MAX_BAUD	B9600
#ifdef B19200
#undef MAX_BAUD
#define MAX_BAUD	B19200
	,{B19200, 19200}
#endif
#ifdef B38400
#undef MAX_BAUD
#define MAX_BAUD	B38400
	,{B38400, 38400}
#endif
#ifdef B57600
#undef MAX_BAUD
#define MAX_BAUD        B57600
	,{B57600, 57600}
#endif
#ifdef B115200
#undef MAX_BAUD
#define MAX_BAUD        B115200
	,{B115200, 115200}
#endif
};
#endif

int
baudrate()
{
#ifndef TERMIOS
int i, ret;
#endif

	T(("baudrate() called"));

#ifdef TERMIOS
	return cfgetospeed(&cur_term->Nttyb);
#else
	ret = cur_term->Nttyb.sg_ospeed;
	if(ret < 0 || ret > MAX_BAUD)
		return ERR;
	for (i = 0; i < (sizeof(speeds) / sizeof(struct speed)); i++)
		if (speeds[i].s == ret)
			return speeds[i].sp;
	return ERR;
#endif
}


/*
**	savetty()  and  resetty()
**
*/

static TTY   buf;

int savetty()
{
	T(("savetty() called"));

#ifdef TERMIOS
	tcgetattr(cur_term->Filedes, &buf);
#else
	gtty(cur_term->Filedes, &buf);
#endif
	return OK;
}

int resetty()
{
	T(("resetty() called"));

#ifdef TERMIOS
	tcsetattr(cur_term->Filedes, TCSANOW, &buf);
#else
        stty(cur_term->Filedes, &buf);
#endif
	return OK;
}


int
resizeterm(int ToLines, int ToCols)
{
	return OK;
}
