
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	lib_kernel.c
 *
 *	Misc. low-level routines:
 *		wattron()
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
#include <nterm.h>

int wattron(WINDOW *win, chtype at)
{
	win->_attrs &= (unsigned long)0xffff00ff;
	win->_attrs |= at;
	return OK;
}

#ifndef MYTINFO
int reset_prog_mode()
{
	T(("reset_prog_mode() called"));

#ifdef TERMIOS
	tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb);
#else
	stty(cur_term->Filedes, &cur_term->Nttyb);
#endif

	return OK; 
}


int reset_shell_mode()
{
	T(("reset_shell_mode() called"));

#ifdef TERMIOS
	tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Ottyb);
#else
	stty(cur_term->Filedes, &cur_term->Ottyb);
#endif

	return OK; 
}
#endif

int curs_set(int vis)
{
	T(("curs_set(%d)", vis));

	if (vis < 0 || vis > 2)
		return ERR;

	switch(vis) {
	case 2:
		if (cursor_visible)
			tputs(cursor_visible, 1, _outc);
		break;
	case 1:
		if (cursor_normal)
			tputs(cursor_normal, 1, _outc);
		break;
	case 0:
		if (cursor_invisible)
			tputs(cursor_invisible, 1, _outc);
		break;
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

struct speed {
	speed_t s;
	int sp;
};

static struct speed speeds[] = {
	{B0, 0,},
	{B50, 50,},
	{B75, 75,},
	{B110, 110,},
	{B134, 134,},
	{B150, 150,},
	{B200, 200,},
	{B300, 300,},
	{B600, 600,},
	{B1200, 1200,},
	{B1800, 1800,},
	{B2400, 2400,},
	{B4800, 4800,},
	{B9600, 9600,},
#define MAX_BAUD	B9600
#ifdef B19200
#undef MAX_BAUD
#define MAX_BAUD	B19200
	{B19200, 19200,},
#endif
#ifdef B38400
#undef MAX_BAUD
#define MAX_BAUD	B38400
	{B38400, 38000,}
#endif
};

int
baudrate()
{
int i, ret;
#ifdef UNTRACE
	if (_tracing)
	    _tracef("baudrate() called");
#endif
#ifdef TERMIOS
	ret = cfgetospeed(&cur_term->Nttyb);
#else
	ret = cur_term->Nttyb.sg_ospeed;
#endif
	if(ret < 0 || ret > MAX_BAUD)
		return ERR;
	for (i = 0; i < (sizeof(speeds) / sizeof(struct speed)); i++)
		if (speeds[i].s == ret)
			return speeds[i].sp;
	return ERR;
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

