
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	raw.c
 *
 *	Routines:
 *		raw()
 *		echo()
 *		nl()
 *		cbreak()
 *		noraw()
 *		noecho()
 *		nonl()
 *		nocbreak()
 *
 */

#include "curses.priv.h"
#include "terminfo.h"

#ifdef TERMIOS
static tcflag_t iexten = 0;
#endif

int raw()
{
	T(("raw() called"));

	SP->_raw = TRUE;
	SP->_cbreak = TRUE;
	SP->_nlmapping = TRUE;

#ifdef TERMIOS
	if(iexten == 0)
		iexten = cur_term->Nttyb.c_lflag & IEXTEN;
	cur_term->Nttyb.c_lflag &= ~(ICANON|ISIG|iexten);
	cur_term->Nttyb.c_iflag &= ~(INPCK|ISTRIP|IXON);
	cur_term->Nttyb.c_oflag &= ~(OPOST);
	cur_term->Nttyb.c_cc[VMIN] = 1;
	cur_term->Nttyb.c_cc[VTIME] = 0;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= RAW;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int cbreak()
{
	T(("cbreak() called"));

	SP->_cbreak = 1;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag &= ~ICANON;
	cur_term->Nttyb.c_lflag |= ISIG;
	cur_term->Nttyb.c_cc[VMIN] = 1;
	cur_term->Nttyb.c_cc[VTIME] = 0;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= CBREAK;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int echo()
{
	T(("echo() called"));

	SP->_echo = TRUE;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= ECHO;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= ECHO;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}


int nl()
{
	T(("nl() called"));

	SP->_nl = TRUE;
	SP->_nlmapping = ! SP->_raw;

#ifdef TERMIOS
	cur_term->Nttyb.c_iflag |= IXON|ICRNL|IXOFF;
	cur_term->Nttyb.c_oflag |= OPOST|ONLCR;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags |= CRMOD;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}


int noraw()
{
	T(("noraw() called"));

	SP->_raw = FALSE;
	SP->_cbreak = FALSE;
	SP->_nlmapping = SP->_nl;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= ISIG|ICANON|iexten;
	cur_term->Nttyb.c_iflag |= IXON;
	cur_term->Nttyb.c_oflag |= OPOST;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~(RAW|CBREAK);
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif

}


int nocbreak()
{
	T(("nocbreak() called"));

	SP->_cbreak = 0;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag |= ICANON;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~CBREAK;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}

int noecho()
{
	T(("noecho() called"));

	SP->_echo = FALSE;

#ifdef TERMIOS
	cur_term->Nttyb.c_lflag &= ~(ECHO);
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~ECHO;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}


int nonl()
{
	T(("nonl() called"));

	SP->_nl = SP->_nlmapping = FALSE;

#ifdef TERMIOS
	cur_term->Nttyb.c_iflag &= ~ICRNL;
	cur_term->Nttyb.c_oflag &= ~ONLCR;
	if((tcsetattr(cur_term->Filedes, TCSANOW, &cur_term->Nttyb)) == -1)
		return ERR;
	else
		return OK;
#else
	cur_term->Nttyb.sg_flags &= ~CRMOD;
	stty(cur_term->Filedes, &cur_term->Nttyb);
	return OK;
#endif
}
