/*
 * tty.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:49
 *
 * Do some tty related things
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tty.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif
#ifndef EXTA_IS
#define EXTA_IS 19200
#endif

#ifndef EXTB_IS
#define EXTB_IS 38400
#endif

#ifdef lint
#define ioctl _ioctl
/* shutup lint */
/* ARGSUSED */
/* VARARGS1 */
static int ioctl(a, b, p) int a; long b; anyptr *p;  { return 0; }
#endif

#if defined(USE_TERMIO) || defined(USE_TERMIOS)

#ifdef USE_TERMIOS
speed_t _baud_tbl[] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800,
		    2400, 4800, 9600, EXTA_IS, EXTB_IS
#ifdef B57600
		    ,57600
#endif
#ifdef B115200
		    ,115200
#endif
		    };
#else
#ifdef USE_SMALLMEM
unsigned short _baud_tbl[] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
			      1800, 2400, 4800, 9600, EXTA_IS, EXTB_IS};
#else
long _baud_tbl[] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800,
		    2400, 4800, 9600, EXTA_IS, EXTB_IS};
#endif
#endif

#ifdef USE_TERMIO
static struct termio old;
#else
static struct termios old;
#endif

int
_lit_output() {
#ifdef USE_TERMIO
	struct termio tmp;
	if (ioctl(cur_term->fd, TCGETA, &old) == -1)
		return 0;
#else
	struct termios tmp;
	if (tcgetattr(cur_term->fd, &old) == -1)
		return 0;
#endif
	if (!(old.c_oflag & OPOST) || !(old.c_oflag & ONLCR))
		return 0;
	memcpy((anyptr)&tmp, (anyptr)&old, sizeof(old));
	tmp.c_oflag &= ~OPOST;
#ifdef USE_TERMIO
	ioctl(cur_term->fd, TCSETA, &tmp);
#else
	tcsetattr(cur_term->fd, TCSADRAIN, &tmp);
#endif
	return 1;
}

void
_norm_output() {
#ifdef USE_TERMIO
	ioctl(cur_term->fd, TCSETA, &old);
#else
	tcsetattr(cur_term->fd, TCSADRAIN, &old);
#endif
}

int
_check_tty() {
	if ((cur_term->prog_mode.c_iflag & IXON) && cur_term->xon)
		cur_term->pad = 0;
	else
		cur_term->pad = 1;
#ifdef USE_TERMIO
	cur_term->baudrate = _baud_tbl[cur_term->prog_mode.c_cflag & CBAUD];
#else
	cur_term->baudrate = cfgetospeed(&cur_term->prog_mode);
#endif
	return OK;
}

int
def_prog_mode() {
	if (cur_term == NULL)
		return ERR;
#ifdef USE_WINSZ
#ifdef TIOCGWINSZ
	if (ioctl(cur_term->fd, TIOCGWINSZ, &cur_term->prog_winsz) < 0)
		return ERR;
#endif
#endif
#ifdef USE_TERMIO
	if (ioctl(cur_term->fd, TCGETA, &cur_term->prog_mode) == 0
#else
	if (tcgetattr(cur_term->fd, &cur_term->prog_mode) == 0
#endif
            && _check_tty() == OK)
		return OK;
	return ERR;
}

int
def_shell_mode() {
	if (cur_term == NULL)
		return ERR;
	cur_term->termcap = 0;
#ifdef USE_WINSZ
#ifdef TIOCGWINSZ
	if (ioctl(cur_term->fd, TIOCGWINSZ, &cur_term->shell_winsz) < 0)
		return ERR;
#endif
#endif
#ifdef USE_TERMIO
	return ioctl(cur_term->fd, TCGETA, &cur_term->shell_mode)==0 ? OK : ERR;
#else
	return tcgetattr(cur_term->fd, &cur_term->shell_mode)==0 ? OK : ERR;
#endif
}


#pragma weak reset_prog_mode
int
reset_prog_mode() {
	if (cur_term == NULL)
		return ERR;
#ifdef USE_TERMIO
	return ioctl(cur_term->fd, TCSETA, &cur_term->prog_mode)==0 ? OK : ERR;
#else
	return tcsetattr(cur_term->fd, TCSADRAIN, &cur_term->prog_mode)==0 ? OK : ERR;
#endif
}

#pragma weak reset_shell_mode
int
reset_shell_mode() {
	if (cur_term == NULL)
		return ERR;
#ifdef USE_TERMIO
	return ioctl(cur_term->fd, TCSETA, &cur_term->shell_mode)==0 ? OK : ERR;
#else
	return tcsetattr(cur_term->fd, TCSADRAIN, &cur_term->shell_mode)==0 ? OK : ERR;
#endif
}

int
_init_tty() {
	cur_term->true_lines = lines;
	cur_term->true_columns = columns;
	if (pad_char == NULL)
		cur_term->padch = '\000';
	else
		cur_term->padch = pad_char[0];
	if (def_shell_mode() == ERR || def_prog_mode() == ERR) {
		cur_term->pad = 0;
		cur_term->baudrate = 1;
		cur_term->xon = 0;
		return OK;
	}
	cur_term->xon = (xoff_character == NULL || xoff_character[0] == '\021')
			&& (xon_character == NULL || xon_character[0] == '\023')
			&& xon_xoff;
#ifdef USE_WINSZ
#ifdef TIOCGWINSZ
	if (cur_term->prog_winsz.ws_row != 0
	    && cur_term->prog_winsz.ws_col != 0) {
		lines = cur_term->prog_winsz.ws_row;
		columns = cur_term->prog_winsz.ws_col;
	}
#endif
#endif
	return OK;
}

#else

#ifdef USE_SGTTY

#ifdef USE_SMALLMEM
unsigned short _baud_tbl[] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
			      1800, 2400, 4800, 9600, EXTA_IS, EXTB_IS};
#else
long _baud_tbl[] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
		    1800, 2400, 4800, 9600, EXTA_IS, EXTB_IS};
#endif


#ifdef TIOCLGET

static int old;

int
_lit_output() {
	struct sgttyb buf;
	int tmp;

	ioctl(cur_term->fd, TIOCGETP, &buf);
	if (buf.sg_flags & RAW)
		return 0;
	ioctl(cur_term->fd, TIOCLGET, &old);
	if (old & LLITOUT)
		return 0;
	tmp = old | LLITOUT;
	ioctl(cur_term->fd, TIOCLSET, &tmp);
	return 1;
}

void
_norm_output() {
	ioctl(cur_term->fd, TIOCLSET, &old);
}

#else

static struct sgttyb old;

int
_lit_output() {
	struct sgttyb tmp;
	ioctl(cur_term->fd, TIOCGETP, &old);
	if (old.sg_flags & RAW)
		return 0;
	memcpy((anyptr)&tmp, (anyptr)&old, sizeof(old));
	tmp.sg_flags |= RAW;
	ioctl(cur_term->fd, TIOCSETP, &tmp);
	return 1;
}

void
_norm_output() {
	ioctl(cur_term->fd, TIOCSETP, &old);
}

#endif

int
_check_tty() {
	if (!(cur_term->prog_mode.v6.sg_flags & RAW) && cur_term->xon)
		cur_term->pad = 0;
	else
		cur_term->pad = 1;
	cur_term->baudrate = _baud_tbl[cur_term->prog_mode.v6.sg_ospeed & 0xf];
	return OK;
}

int
def_shell_mode() {
	if (ioctl(cur_term->fd, TIOCGETP, &cur_term->shell_mode.v6) < 0)
		return ERR;
#ifdef TIOCGETC
	if (ioctl(cur_term->fd, TIOCGETC, &cur_term->shell_mode.v7) < 0)
		return ERR;
#endif
#ifdef TIOCLGET
	if (ioctl(cur_term->fd, TIOCLGET, &cur_term->shell_mode.bsd) < 0)
		return ERR;
#endif
#ifdef TIOCGLTC
	if (ioctl(cur_term->fd, TIOCGLTC, &cur_term->shell_mode.bsd_new) < 0)
		return ERR;
#endif
#ifdef USE_WINSZ
#ifdef TIOCGWINSZ
	if (ioctl(cur_term->fd, TIOCGWINSZ, &cur_term->shell_winsz)<0)
		return ERR;
#endif
#endif
	cur_term->termcap = 0;
	return OK;
}

int
def_prog_mode() {
	if (ioctl(cur_term->fd, TIOCGETP, &cur_term->prog_mode.v6) < 0)
		return ERR;
#ifdef TIOCGETC
	if (ioctl(cur_term->fd, TIOCGETC, &cur_term->prog_mode.v7) < 0)
		return ERR;
#endif
#ifdef TIOCLGET
	if (ioctl(cur_term->fd, TIOCLGET, &cur_term->prog_mode.bsd) < 0)
		return ERR;
#endif
#ifdef TIOCGLTC
	if (ioctl(cur_term->fd, TIOCGLTC, &cur_term->prog_mode.bsd_new) < 0)
		return ERR;
#endif
#ifdef USE_WINSZ
#ifdef TIOCGWINSZ
	if (ioctl(cur_term->fd, TIOCGWINSZ, &cur_term->prog_winsz)<0)
		return ERR;
#endif
#endif
	return _check_tty();
}

int
reset_shell_mode() {
	if (ioctl(cur_term->fd, TIOCSETP, &cur_term->shell_mode.v6) < 0)
		return ERR;
#ifdef TIOCGETC
	if (ioctl(cur_term->fd, TIOCSETC, &cur_term->shell_mode.v7) < 0)
		return ERR;
#endif
#ifdef TIOCLGET
	if (ioctl(cur_term->fd, TIOCLSET, &cur_term->shell_mode.bsd) < 0)
		return ERR;
#endif
#ifdef TIOCGLTC
	if (ioctl(cur_term->fd, TIOCSLTC, &cur_term->shell_mode.bsd_new) < 0)
		return ERR;
#endif
	return OK;
}

int
reset_prog_mode() {
	if (ioctl(cur_term->fd, TIOCSETP, &cur_term->prog_mode.v6) < 0)
		return ERR;
#ifdef TIOCGETC
	if (ioctl(cur_term->fd, TIOCSETC, &cur_term->prog_mode.v7) < 0)
		return ERR;
#endif
#ifdef TIOCLGET
	if (ioctl(cur_term->fd, TIOCLSET, &cur_term->prog_mode.bsd) < 0)
		return ERR;
#endif
#ifdef TIOCGLTC
	if (ioctl(cur_term->fd, TIOCSLTC, &cur_term->prog_mode.bsd_new) < 0)
		return ERR;
#endif
	return OK;
}

int
_init_tty() {
	cur_term->true_lines = lines;
	cur_term->true_columns = columns;
	if (pad_char == NULL)
		cur_term->padch = '\000';
	else
		cur_term->padch = pad_char[0];
	if (def_shell_mode() == ERR || def_prog_mode() == ERR) {
		cur_term->pad = 0;
		cur_term->baudrate = 1;
		cur_term->xon = 0;
		return OK;
	}
#ifndef TIOCGETC
	cur_term->xon = (xoff_character == NULL || xoff_character[0] == '\021')
			&& (xon_character == NULL || xon_character[0] == '\023')
		        && xon_xoff;
#else
	if (xon_xoff) {
		if (xon_character != NULL) {
			cur_term->prog_mode.v7.t_startc = xon_character[0];
			if (ioctl(cur_term->fd, TIOCSETC,
			    &cur_term->prog_mode.v7) < 0)
				return ERR;
		}
		if (xoff_character != NULL) {
			cur_term->prog_mode.v7.t_stopc = xoff_character[0];
			if (ioctl(cur_term->fd, TIOCSETC,
			          &cur_term->prog_mode.v7) < 0)
				return ERR;
		}
	}
	cur_term->xon = xon_xoff;
#endif
#ifdef USE_WINSZ
#ifdef TIOCGWINSZ
	if (cur_term->prog_winsz.ws_row != 0
	    && cur_term->prog_winsz.ws_col != 0) {
		lines = cur_term->prog_winsz.ws_row;
		columns = cur_term->prog_winsz.ws_col;
	}
#endif
#endif
	return OK;
}

#else

int
_lit_output() {
	return 0;
}

void
_norm_output() {
	return;
}

int
_check_tty() {
	return OK;
}

int
def_prog_mode() {
	return OK;
}

int
reset_prog_mode() {
	return OK;
}

int
def_shell_mode() {
	return OK;
}

int
reset_shell_mode() {
	return OK;
}

int
_init_tty() {
	cur_term->pad = 1;
	cur_term->padch = 0;
	cur_term->baudrate = 1200;
	cur_term->xon = 0;
	cur_term->termcap = 0;
	cur_term->true_lines = lines;
	cur_term->true_columns = columns;
}

#endif

#endif
