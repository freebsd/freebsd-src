/*
 * fillterm.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:54
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo fillterm.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

TERMINAL _term_buf;
TERMINAL *cur_term;

int
_fillterm(name, path, buf)
char *name, *buf;
struct term_path *path; {
	register int i, r;

	r = -1;

	for(i = NUM_OF_BOOLS; i;)
		_term_buf.bools[--i] = -1;
	for(i = NUM_OF_NUMS; i;)
		_term_buf.nums[--i] = -2;
	for(i = NUM_OF_STRS; i;)
		_term_buf.strs[--i] = (char *) -1;

	_term_buf.name_all = NULL;
	
	r = _findterm(name, path, buf);
	switch(r) {
	case 1:
		if (_gettcap(buf, &_term_buf, path) != 0)
			return -3;
		_tcapconv(); 
		_tcapdefault();
		break;
	case 2:
		if (_gettinfo(buf, &_term_buf, path) != 0)
			return -3;
		break;
	case 3:
		if (_gettbin(buf, &_term_buf) != 0)
			return -3;
		break;
	default:
		return r;
	}

	if ((_term_buf.name = _addstr(name)) == NULL)
		return -3;

	for(i = NUM_OF_BOOLS; i;)
		if (_term_buf.bools[--i] == -1)
			_term_buf.bools[i] = 0;
	for(i = NUM_OF_NUMS; i;)
		if (_term_buf.nums[--i] == -2)
			_term_buf.nums[i] = -1;
	for(i = NUM_OF_STRS; i;)
		if (_term_buf.strs[--i] == (char *) -1)
			_term_buf.strs[i] = NULL;

	_term_buf.fd = 1;
	_term_buf.pad = 1;
	_term_buf.baudrate = 0;
	_term_buf.strbuf = _endstr();

	cur_term = (TERMINAL *) malloc(sizeof(_term_buf));
	if (cur_term == NULL)
		return -3;
	memcpy((anyptr)cur_term, (anyptr)&_term_buf, sizeof(_term_buf));

	return r;
}
