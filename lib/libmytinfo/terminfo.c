/*
 * terminfo.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:30
 *
 * terminfo compatible libary functions
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo terminfo.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

extern char _mytinfo_version[];
/* not static */
char *_force_pick2 = _mytinfo_version;

#ifdef USE_FAKE_STDIO

#ifdef __GNUC__
__inline__
#endif
static int
printerr(msg)
char *msg; {
	return write(2, msg, strlen(msg));
}

#define RETERR(e, msg)  { (err == NULL ? (printerr(msg), exit(1)) \
				       : (*err = e)); return ERR; }

#else

#define RETERR(e, msg)  { (err == NULL ? (fprintf(stderr, "setupterm(\"%s\",%d,NULL): %s", term, fd, msg), exit(1)) \
				       : (*err = e)); return ERR; }

#endif

int
setupterm(term, fd, err)
char *term;
int fd;
int *err; {
	struct term_path *path;
	char *s;
	int r = -1;
	char buf[MAX_BUF];


	if (term == NULL) 
		term = getenv("TERM");
	if (term == NULL)
		RETERR(0, "TERM not set\n")

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
		RETERR(0, "malloc error\n");

	r = _fillterm(term, path, buf);

	_delpath(path);

	switch(r) {
	case -3:
		RETERR(0, "malloc error\n");
	case -2:
		RETERR(-1, "bad format\n");
	case -1:
		RETERR(-1, "database not found\n");
	case 0:
		RETERR(0, "terminal not found\n");
	case 1:
	case 2:
	case 3:
		cur_term->fd = fd;
		_term_buf.fd = fd;
		if (_init_tty() == ERR)
			RETERR(0, "problem initializing tty\n");
		if ((s = getenv("LINES")) != NULL && atoi(s) > 0) 
			lines = atoi(s);
		if ((s = getenv("COLUMNS")) != NULL && atoi(s) > 0)
			columns = atoi(s);
		if (err != NULL)
			*err = 1;
		return OK;
	default:
		RETERR(0, "oops...\n");
	}
}

int
set_curterm(p)
TERMINAL *p; {
	cur_term = p;
	if (_init_tty() == ERR)
		return ERR;
	if (_check_tty() == ERR)
		return ERR;
	return OK;
}

int
del_curterm(p)
TERMINAL *p; {
	_del_strs(p);
	free((anyptr) p);

	return OK;
}
