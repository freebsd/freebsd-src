/*
 * termcap.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/06/01 07:43:08
 *
 * termcap compatibility functions
 *
 */

#include "defs.h"
#include <term.h>
#ifdef __FreeBSD__
#include <unistd.h>
#endif

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo termcap.c 3.3 92/06/01 public domain, By Ross Ridge";
#endif

extern char _mytinfo_version[];
/* not static */
char *_force_pick1 = _mytinfo_version;

int
tgetent(buf, term)
char *term, *buf; {
	char *s;
	struct term_path *path;
	int r = -1;
	int fd;

	if (term == NULL) 
		term = getenv("TERM");
	if (term == NULL)
		return 0;

	path = _buildpath(
#ifdef USE_TERMINFO
			  "$MYTERMINFO", 2,
			  "$TERMINFO", 2,
#ifdef TERMINFODIR
			  TERMINFODIR, 0,
#endif
#ifdef TERMINFOSRC
			  TERMINFOSRC, 0,
#endif
#endif
#ifdef USE_TERMCAP
			  "$TERMCAP", 1,
#ifdef TERMCAPFILE
			  TERMCAPFILE, 0,
#endif
#endif
			  NULL, -1);

	if (path == NULL)
		return -1;

#if 1
	{
		char buf1[MAX_BUF];
		r = _fillterm(term, path, buf1);
	}
#else
	r = _fillterm(term, path, buf);
#endif

	_delpath(path);

	switch(r) {
	case -3:
	case -2:
	case -1:
		return -1;
	case 0:
		return 0;
	case 1:
	case 2:
	case 3:
		if (isatty(1))
			fd = 1;
		else if (isatty(2))
			fd = 2;
		else if (isatty(3))	/* V10 /dev/tty ?? */
			fd = 3;
		else if (isatty(0))
			fd = 0;
		else
			fd = 1;

		cur_term->fd = fd;
		_term_buf.fd = fd;

		if (_init_tty() == ERR)
			return 0;
		if ((s = getenv("LINES")) != NULL && atoi(s) > 0) 
			lines = atoi(s);
		if ((s = getenv("COLUMNS")) != NULL && atoi(s) > 0)
			columns = atoi(s);
		cur_term->termcap = 1;
		return 1;
	default:
		return -1;
	}
}

static char cap2[3];

int
tgetnum(cap)
char *cap; {
	int ind;

	cap2[0] = cap[0]; 
	cap2[1] = cap[1];
	cap2[2] = '\0';

	ind = _findnumcode(cap2);
	if (ind == -1)
		return -1;
	return cur_term->nums[ind];
}

int
tgetflag(cap)
char *cap; {
	int ind;

	cap2[0] = cap[0]; 
	cap2[1] = cap[1];
	cap2[2] = '\0';

	ind = _findboolcode(cap2);
	if (ind == -1)
		return 0;
	return cur_term->bools[ind];
}

char *
tgetstr(cap, area)
char *cap;
char **area; {
	register char *sp, *dp;
	int ind;

	cap2[0] = cap[0]; 
	cap2[1] = cap[1];
	cap2[2] = '\0';

	ind = _findstrcode(cap2);
	if (ind == -1)
		return NULL;
	sp = cur_term->strs[ind];
	if (area == NULL || sp == NULL)
		return sp;
	dp = *area;
	while (*sp != '\0')
		*dp++ = *sp++;
	*dp++ = '\0';
	sp = *area;
	*area = dp;
	return sp;
}
