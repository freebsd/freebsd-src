
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_getch.c
**
**	The routine getch().
**
*/

#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#if defined(BRAINDEAD)
extern int errno;
#endif
#include "curses.priv.h"

#define head	SP->_fifohead
#define tail	SP->_fifotail
#define peek	SP->_fifopeek

#define h_inc() { head == FIFO_SIZE-1 ? head = 0 : head++; if (head == tail) head = -1, tail = 0;}
#define h_dec() { head == 0 ?  head = FIFO_SIZE-1 : head--; if (head == tail) tail = -1;}
#define t_inc() { tail == FIFO_SIZE-1 ? tail = 0 : tail++; if (tail == head) tail = -1;}
#define p_inc() { peek == FIFO_SIZE-1 ? peek = 0 : peek++;}

static int fifo_peek()
{
	T(("peeking at %d", peek+1));
	return SP->_fifo[++peek];
}

static inline void fifo_dump()
{
int i;
	T(("head = %d, tail = %d, peek = %d", head, tail, peek));
	for (i = 0; i < 10; i++)
		T(("char %d = %d (%c)", i, SP->_fifo[i], (unsigned char)SP->_fifo[i]));
}

static inline int fifo_pull()
{
int ch;
 	ch = SP->_fifo[head];
	T(("pulling %d from %d", ch, head));

	h_inc();
	fifo_dump();
	return ch;
}

int ungetch(int ch)
{
	if (tail == -1)
		return ERR;
	if (head == -1) {
		head = 0;
		t_inc()
	} else
		h_dec();
	
	SP->_fifo[head] = ch;
	T(("ungetch ok"));
	fifo_dump();
	return OK;
}

static inline int fifo_push()
{
int n;
unsigned char ch;

	if (tail == -1) return ERR;
again:    
	n = read(fileno(SP->_ifp), &ch, 1);
	if (n == -1 && errno == EINTR)
		goto again;
	T(("read %d characters", n));
	SP->_fifo[tail] = ch;
	if (head == -1) head = tail;
	t_inc();
	T(("pushed %d at %d", ch, tail));
	fifo_dump();
	return ch;
}

static inline void fifo_clear()
{
int i;
	for (i = 0; i < FIFO_SIZE; i++)
		SP->_fifo[i] = 0;
	head = -1; tail = peek = 0;
}

static int kgetch(WINDOW *);

int
wgetch(WINDOW *win)
{
bool	setHere = FALSE;	/* cbreak mode was set here */
int	ch; 

	T(("wgetch(%x) called", win));

	/* this should be eliminated */
	if (! win->_scroll  &&  (SP->_echo) &&  (win->_flags & _FULLWIN)
	   &&  win->_curx == win->_maxx &&  win->_cury == win->_maxy)
		return(ERR);

	if ((is_wintouched(win) || (win->_flags & _HASMOVED)) &&
	    !(win->_flags & _ISPAD))
		wrefresh(win);

	if (SP->_echo  &&  ! (SP->_raw  ||  SP->_cbreak)) {
		cbreak();
		setHere = TRUE;
	}

	if (win->_delay >= 0 || SP->_cbreak > 1) {
	int delay;

		T(("timed delay in wgetch()"));
		if (SP->_cbreak > 1)
		    delay = (SP->_cbreak-1) * 100;
		else
		    delay = win->_delay;

		T(("delay is %d microseconds", delay));

		if (head == -1)	/* fifo is empty */
			if (timed_wait(fileno(SP->_ifp), delay, NULL) == 0)
				return ERR;
		/* else go on to read data available */
	}

	if (win->_use_keypad)
		ch = kgetch(win);
	else {
		if (head == -1)
			fifo_push();
		ch = fifo_pull();
	}

	/* This should be eliminated */
	/* handle 8-bit input */
	if (ch & 0x80)
		if (!win->_use_meta)
			ch &= 0x7f;

	/* there must be a simpler way of doing this */
	if (!(win->_flags & _ISPAD) &&
	     SP->_echo  &&  ch < 0400) {   /* ch < 0400 => not a keypad key */
		mvwaddch(curscr, win->_begy + win->_cury,
                         win->_begx + win->_curx, ch | win->_attrs);
		waddch(win, ch | win->_attrs);
	}
	if (setHere)
	    nocbreak();

	T(("wgetch returning : '%c', '0x%x'", ch, ch));

	return(ch);
}


/*
**      int
**      kgetch()
**
**      Get an input character, but take care of keypad sequences, returning
**      an appropriate code when one matches the input.  After each character
**      is received, set a one-second alarm call.  If no more of the sequence
**      is received by the time the alarm goes off, pass through the sequence
**      gotten so far.
**
*/

static int
kgetch(WINDOW *win)
{
struct try  *ptr;
int ch = 0;
int timeleft = 1000;

    T(("kgetch(%x) called", win));

    ptr = SP->_keytry;

	if (head == -1)  {
		ch = fifo_push();
		peek = 0;
    	while (ptr != NULL) {
			T(("ch = %d", ch));
			while ((ptr != NULL) && (ptr->ch != (unsigned char)ch)) 
			    ptr = ptr->sibling;
	
			if (ptr != NULL)
		    if (ptr->value != 0) {	/* sequence terminated */
		    	T(("end of sequence"));
		    	fifo_clear();
				return(ptr->value);
		    } else {			/* go back for another character */
				ptr = ptr->child;
				T(("going back for more"));
		    } else
				break;

		    T(("waiting for rest of sequence"));
   			if (timed_wait(fileno(SP->_ifp), timeleft, &timeleft) < 1) {
				T(("ran out of time"));
				return(fifo_pull());
   			} else {
   				T(("got more!"));
   				fifo_push();
   				ch = fifo_peek();
   			}
		}
    } 
    return(fifo_pull());
}
