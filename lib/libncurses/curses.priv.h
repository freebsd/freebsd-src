
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	curses.priv.h
 *
 *	Header file for curses library objects which are private to
 *	the library.
 *
 */

#include "version.h"

#ifndef __GNUC__
#define inline
#endif

#ifndef NOACTION
#include <unistd.h>
typedef struct sigaction sigaction_t;
#else
#include "SigAction.h"
#endif

#include "curses.h"

#define min(a,b)	((a) > (b)  ?  (b)  :  (a))
#define max(a,b)	((a) < (b)  ?  (b)  :  (a))

#define FG(n)	((n) & 0x0f)
#define BG(n)	(((n) & 0xf0) >> 4)

#define TextOf(c)    ((c) & (chtype)A_CHARTEXT)
#define AttrOf(c)    ((c) & (chtype)A_ATTRIBUTES)

#define BLANK        (' '|A_NORMAL)

#define CHANGED     -1

#define ALL_BUT_COLOR ((chtype)~(A_COLOR))

/* Macro to put together character and attribute info and return it.
   If colors are in the attribute, they have precedence. */
#define ch_or_attr(ch,at) \
    ((PAIR_NUMBER(at) > 0) ? \
     ((((chtype)ch) & ALL_BUT_COLOR) | (at)) : ((((chtype)ch) | (at))))

extern WINDOW	*newscr;

#ifdef TRACE
#define T(a)	if (_tracing & TRACE_ORDINARY) _tracef a
#define TR(n, a)	if (_tracing & (n)) _tracef a
extern int _tracing;
extern char *visbuf(const char *);
#else
#define T(a)
#define TR(n, a)
#endif

extern int _outch(int);
extern void init_acs(void);
extern void tstp(int);
extern WINDOW *makenew(int, int, int, int);
extern int timed_wait(int fd, int wait, int *timeleft);
extern chtype _nc_background(WINDOW *);
extern chtype _nc_render(WINDOW *, chtype);

struct try {
        struct try      *child;     /* ptr to child.  NULL if none          */
        struct try      *sibling;   /* ptr to sibling.  NULL if none        */
        unsigned char    ch;        /* character at this node               */
        unsigned short   value;     /* code of string so far.  0 if none.   */
};

/*
 * Structure for soft labels.
 */

typedef struct {
	char dirty;			/* all labels have changed */
	char hidden;			/* soft lables are hidden */
	WINDOW *win;
 	struct slk_ent {
 	    char text[9];		/* text for the label */
 	    char form_text[9];		/* formatted text (left/center/...) */
 	    int x;			/* x coordinate of this field */
 	    char dirty;			/* this label has changed */
 	    char visible;		/* field is visible */
	} ent[8];
} SLK;

#define FIFO_SIZE	32

struct screen {
   	FILE		*_ifp;	    	/* input file ptr for this terminal     */
   	FILE		*_ofp;	    	/* output file ptr for this terminal    */
   	int		_checkfd;
#ifdef MYTINFO
	struct _terminal *_term;
#else
	struct term	*_term;	    	/* used by terminfo stuff               */
#endif
	WINDOW		*_curscr;   	/* windows specific to a given terminal */
	WINDOW		*_newscr;
	WINDOW		*_stdscr;
	struct try  	*_keytry;   	/* "Try" for use with keypad mode       */
	unsigned int	_fifo[FIFO_SIZE]; 	/* Buffer for pushed back characters    */
	signed char	_fifohead,
			_fifotail,
			_fifopeek;
	bool		_endwin;
	chtype		_current_attr;
	bool		_coloron;
	int		_cursor;	/* visibility of the cursor		*/
	int         	_cursrow;   	/* Row and column of physical cursor    */
	int         	_curscol;
	bool		_nl;	    	/* True if NL -> CR/NL is on	    	*/
	bool		_raw;	    	/* True if in raw mode                  */
	int		_cbreak;    	/* 1 if in cbreak mode                  */
                       		    	/* > 1 if in halfdelay mode		*/
	bool		_echo;	    	/* True if echo on                      */
	bool		_nlmapping; 	/* True if terminal is really doing     */
				    	/* NL mapping (fn of raw and nl)    	*/
 	SLK		*_slk;	    	/* ptr to soft key struct / NULL    	*/
	int		_costs[9];  	/* costs of cursor movements for mvcur  */
	int		_costinit;  	/* flag wether costs[] is initialized   */
};

extern struct screen	*SP;

extern int _slk_format;			/* format specified in slk_init() */

#define MAXCOLUMNS    135
#define MAXLINES      66
#define UNINITIALISED ((struct try * ) -1)
