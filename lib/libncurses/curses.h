
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992, 1993, 1994                 *
*                          by Zeyd M. Ben-Halim                            *
*                          zmbenhal@netcom.com                             *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, not removed   *
*        from header files, and is reproduced in any documentation         *
*        accompanying it or the applications linked with it.               *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

#ifndef __NCURSES_H
#define __NCURSES_H
#define CURSES 1
#define CURSES_H 1

#include <stdio.h>
#include <stdarg.h>
#ifndef NOTERMIOS
#include <termios.h>
#ifndef TERMIOS
#define TERMIOS 1
#endif
#else
#include <sgtty.h>
#include <sys/ioctl.h>
#endif
#include <unctrl.h>

#define bool    char

typedef unsigned long  chtype;

/* attributes */
#define A_ATTRIBUTES	0xffffff00
#define A_NORMAL	0x00000000
#define A_STANDOUT	0x00010000
#define A_UNDERLINE	0x00020000
#define A_REVERSE	0x00040000
#define A_BLINK		0x00080000
#define A_DIM		0x00100000
#define A_BOLD		0x00200000
#define A_ALTCHARSET	0x00400000
#define A_INVIS		0x00800000
#define A_PROTECT	0x01000000
#define A_CHARTEXT	0x000000ff
#define A_COLOR		0x0000ff00
#define COLOR_PAIR(n)	(n << 8)
#define PAIR_NUMBER(a)	((a & A_COLOR) >> 8)

/* colors */
extern int COLORS;
extern int COLOR_PAIRS;
extern unsigned char *color_pairs;

#define COLOR_BLACK	0
#define COLOR_RED	1
#define COLOR_GREEN	2
#define COLOR_YELLOW	3
#define COLOR_BLUE	4
#define COLOR_MAGENTA	5
#define COLOR_CYAN	6
#define COLOR_WHITE	7

/* line graphics */

extern 	chtype acs_map[];


#define ACS_ULCORNER	(acs_map['l'])
#define ACS_LLCORNER	(acs_map['m'])
#define ACS_URCORNER	(acs_map['k'])
#define ACS_LRCORNER	(acs_map['j'])
#define ACS_RTEE	(acs_map['u'])
#define ACS_LTEE	(acs_map['t'])
#define ACS_BTEE	(acs_map['v'])
#define ACS_TTEE	(acs_map['w'])
#define ACS_HLINE	(acs_map['q'])
#define ACS_VLINE	(acs_map['x'])
#define ACS_PLUS	(acs_map['n'])
#define ACS_S1		(acs_map['o'])	/* scan line 1 */
#define ACS_S9		(acs_map['s'])	/* scan line 9 */
#define ACS_DIAMOND	(acs_map['`'])	/* diamond */
#define ACS_CKBOARD	(acs_map['a'])	/* checker board (stipple) */
#define ACS_DEGREE	(acs_map['f'])	/* degree symbol */
#define ACS_PLMINUS	(acs_map['g'])	/* plus/minus */
#define ACS_BULLET	(acs_map['~'])	/* bullet */
#define ACS_LARROW	(acs_map[','])	/* arrow pointing left */
#define ACS_RARROW	(acs_map['+'])	/* arrow pointing right */
#define ACS_DARROW	(acs_map['.'])	/* arrow pointing down */
#define ACS_UARROW	(acs_map['-'])	/* arrow pointing up */
#define ACS_BOARD	(acs_map['h'])	/* board of squares */
#define ACS_LANTERN	(acs_map['I'])	/* lantern symbol */
#define ACS_BLOCK	(acs_map['0'])	/* solid square block */

#ifndef TRUE
#  define TRUE    (1)
#  define FALSE   (0)
#endif

#define ERR     (-1)
#define OK      (0)

#define _SUBWIN         0x01
#define _ENDLINE        0x02
#define _FULLWIN        0x04
#define _SCROLLWIN      0x08
#define _ISPAD		0x10
#define _HASMOVED	0x20

#define _NOCHANGE       -1

typedef struct screen  SCREEN;
typedef struct _win_st WINDOW;

struct _win_st {
	short   _cury, _curx;	/* current cursor position */
	short   _maxy, _maxx;	/* maximum values of x and y NOT the screen dimensions */
	short   _begy, _begx;
	short   _flags;
	chtype  _attrs;
	chtype  _bkgd;

	/* The following should be consolidated into one bitset */
	bool	_notimeout;
	bool	_use_idc;
	bool    _clear;
	bool    _leave;
	bool    _scroll;
	bool    _idlok;
	bool	_immed;
	bool	_sync;
	bool    _use_keypad;    /* 0=no, 1=yes */
	bool    _use_meta;      /* T=use the meta key */

	int	_delay;		/* 0 = nodelay
		   		  <0 = blocking
		   		  >0 = delay */
	chtype  **_line;
	short   *_firstchar;    /* First changed character in the line */
	short   *_lastchar;     /* Last changed character in the line */
	short	_regtop;	/* Top and bottom of scrolling region */
	short	_regbottom;
	int	_parx;
	int	_pary;
	WINDOW	*_parent;	/* parent if a sub-window */
};

extern WINDOW   *stdscr, *curscr, *newscr;

extern int	LINES, COLS;

#ifdef __cplusplus
extern "C" {
#endif

extern char ttytype[];		/* needed for backward compatibility */

extern int tigetflag(char *);
extern int tigetnum(char *);
extern char *tigetstr(char *);

/* Debugging : use with libdcurses.a */

extern void _init_trace(void);
extern void _tracef(char *, ...);
extern char *_traceattr(int mode);
extern char *_tracechar(const unsigned char mode);
extern void trace(const unsigned int tracelevel);

/* trace masks */
#define TRACE_DISABLE	0x00	/* turn off tracing */
#define TRACE_ORDINARY	0x01	/* ordinary trace mode */
#define TRACE_CHARPUT	0x02	/* also trace all character outputs */
#define TRACE_MAXIMUM	0x0f	/* maximum trace level */

/* function prototypes */

extern int baudrate(void);
extern int beep(void);
extern int cbreak(void);
extern int clearok(WINDOW *,int);
extern int copywin(WINDOW *,WINDOW *,int,int,int,int,int,int,int);
extern int curs_set(int);
extern int def_prog_mode(void);
extern int def_shell_mode(void);
extern int delwin(WINDOW *);
extern WINDOW *derwin(WINDOW *,int,int,int,int);
extern int doupdate(void);
extern WINDOW *dupwin(WINDOW *);
extern int echo(void);
extern int endwin(void);
extern char erasechar(void);
extern int flash(void);
extern int flushinp(void);
extern int halfdelay(int);
extern int idlok(WINDOW *,int);
extern int intrflush(WINDOW *,bool);
extern int is_linetouched(WINDOW *,int);
extern int is_wintouched(WINDOW *);
extern WINDOW *initscr(void);
extern int isendwin(void);
extern char *keyname(int);
extern int keypad(WINDOW *,int);
extern char killchar(void);
extern int leaveok(WINDOW *,int);
extern char *longname(void);
extern int meta(WINDOW *,int);
extern int mvcur(int,int,int,int);
extern int mvprintw(int,int,char *,...);
extern int mvscanw(int,int,char *,...);
extern int mvwin(WINDOW *,int,int);
extern int mvwprintw(WINDOW *,int,int,char *,...);
extern int mvwscanw(WINDOW *,int,int,char *,...);
extern WINDOW *newpad(int,int);
extern SCREEN *newterm(char *,FILE *,FILE *);
extern WINDOW *newwin(int,int,int,int);
extern int nl(void);
extern int nocbreak(void);
extern int nodelay(WINDOW *,int);
extern int noecho(void);
extern int nonl(void);
extern int noraw(void);
extern int notimeout(WINDOW *,bool);
extern int overlay(WINDOW *,WINDOW *);
extern int overwrite(WINDOW *,WINDOW *);
extern int pnoutrefresh(WINDOW *,int,int,int,int,int,int);
extern int prefresh(WINDOW *,int,int,int,int,int,int);
extern int printw(char *,...);
extern int putp(char *);
extern int raw(void);
extern int reset_prog_mode(void);
extern int reset_shell_mode(void);
extern int resetty(void);
extern int ripoffline(int line, int (*init)(WINDOW *, int));
extern int savetty(void);
extern int scanw(char *,...);
extern int scrollok(WINDOW *,int);
extern SCREEN *set_term(SCREEN *);
extern int setupterm(char *,int,int *);
extern WINDOW *subwin(WINDOW *,int,int,int,int);
extern int timeout(int);
extern char *tparm(const char *, ...);
extern int tputs(const char *,int,int (*)(int));
extern int typeahead(int);
extern int ungetch(int);
extern int vidattr(chtype);
extern int vidputs(chtype,int (*)(int));
extern int vwscanw(WINDOW *,char *,va_list);
extern int vwprintw(WINDOW *,char *,va_list);
extern int waddch(WINDOW *,chtype);
extern int waddchnstr(WINDOW *,chtype *,int);
extern int waddnstr(WINDOW *,char *,int);
extern int wattron(WINDOW *,chtype);
extern int wattroff(WINDOW *,chtype);
extern int wbkgd(WINDOW *,chtype);
extern int wborder(WINDOW *,chtype,chtype,chtype,chtype,chtype,chtype,chtype,chtype);
extern int wclear(WINDOW *);
extern int wclrtobot(WINDOW *);
extern int wclrtoeol(WINDOW *);
extern int wdelch(WINDOW *);
extern int wechochar(WINDOW *, chtype);
extern int werase(WINDOW *);
extern int wgetch(WINDOW *);
extern int wgetnstr(WINDOW *,char *,int maxlen);
extern int whline(WINDOW *,chtype,int);
extern int winsch(WINDOW *,chtype);
extern int winsdelln(WINDOW *,int);
extern int winsnstr(WINDOW *,char *,int);
extern int wmove(WINDOW *,int,int);
extern int wnoutrefresh(WINDOW *);
extern int wprintw(WINDOW *,char *,...);
extern int wredrawln(WINDOW *,int,int);
extern int wrefresh(WINDOW *);
extern int wscanw(WINDOW *,char *,...);
extern int wscrl(WINDOW *,int);
extern int wsetscrreg(WINDOW *,int,int);
extern int wtimeout(WINDOW *,int);
extern int wtouchln(WINDOW *,int,int,int);
extern int wvline(WINDOW *,chtype,int);

extern bool can_change_color(void);
extern int color_content(short,short *,short *, short *);
extern int has_colors(void);
extern int init_color(short,short,short,short);
extern int init_pair(short,short,short);
extern int pair_content(short,short*,short*);
extern int start_color(void);

extern int slk_init(int);
extern int slk_set(int,char *,int);
extern int slk_refresh(void);
extern int slk_noutrefresh(void);
extern char *slk_label(int);
extern int slk_clear(void);
extern int slk_restore(void);
extern int slk_touch(void);

#ifdef __cplusplus
}
#endif

/*
 * pseudo functions
 */
#define wgetstr(w, s)		wgetnstr(w, s, -1)

#define napms(x)		usleep(1000*x)
#define setterm(term)		setupterm(term, 1, (int *)0)

#define fixterm()		reset_prog_mode()
#define resetterm()		reset_shell_mode()
#define saveterm()		def_prog_mode()
#define crmode()		cbreak()
#define nocrmode()		nocbreak()
#define gettmode()

#define getyx(win,y,x)   	(y = (win)->_cury, x = (win)->_curx)
#define getbegyx(win,y,x)	(y = (win)->_begy, x = (win)->_begx)
#define getmaxyx(win,y,x)	(y = (win)->_maxy + 1, x = (win)->_maxx + 1)
#define getsyx(y,x)		getyx(stdscr, y, x)
#define setsyx(y,x)		(stdscr->_cury = y, stdscr->_curx = x)

#define wbkgdset(win,ch)        ((win)->_bkgd = ch)
#define getbkgd(win)            ((win)->_bkgd)

/* It seems older SYSV curses define these */
#define getattrs(win)		(win->_attrs)
#define getmaxx(win)		((win)->_maxx + 1)
#define getmaxy(win)		((win)->_maxy + 1)

#define winch(win)       	((win)->_line[(win)->_cury][(win)->_curx])
#define wstandout(win)      	(wattrset(win,A_STANDOUT))
#define wstandend(win)      	(wattrset(win,A_NORMAL))
#define wattrset(win,at)    	((win)->_attrs = (at))

#define subpad(p,l,c,y,x)	derwin(p,l,c,y,x)
#define scroll(win)		wscrl(win,1)

#define touchwin(win)		wtouchln((win), 0, (win)->_maxy + 1, 1)
#define touchline(win, s, c)	wtouchln((win), s, c, 1)
#define untouchwin(win)		wtouchln((win), 0, (win)->_maxy + 1, 0)

#define box(win, v, h)		wborder(win, v, v, h, h, 0, 0, 0, 0)
#define border(ls, rs, ts, bs, tl, tr, bl, br)	wborder(stdscr, ls, rs, ts, bs, tl, tr, bl, br)
#define hline(ch, n)		whline(stdscr, ch, n)
#define vline(ch, n)		wvline(stdscr, ch, n)

#define winsstr(w, s)		winsnstr(w, s, 0)

#define redrawwin(w)		wredrawln(w, 0, w->_maxy+1)
#define waddstr(win,str)	waddnstr(win,str,-1)
#define waddchstr(win,str)	waddchnstr(win,str,-1)

/*
 * pseudo functions for standard screen
 */

#define bkgdset(ch)		wbkgdset(stdscr,ch)
#define bkgd(ch)		wbkgd(stdscr,ch)
#define inch()       		winch(stdscr)
#define standout()     		wstandout(stdscr)
#define standend()     		wstandend(stdscr)
#define attron(at)     		wattron(stdscr,at)
#define attroff(at)    		wattroff(stdscr,at)
#define attrset(at)    		wattrset(stdscr,at)
#define addch(ch)      		waddch(stdscr,ch)
#define echochar(c)		wechochar(stdscr, c)
#define getch()        		wgetch(stdscr)
#define addstr(str)    		waddnstr(stdscr,str,-1)
#define getstr(str)    		wgetstr(stdscr,str)
#define move(y, x)     		wmove(stdscr,y,x)
#define clear()        		wclear(stdscr)
#define erase()        		werase(stdscr)
#define clrtobot()     		wclrtobot(stdscr)
#define clrtoeol()     		wclrtoeol(stdscr)
#define insertln()     		winsdelln(stdscr, 1)
#define winsertln(w)     	winsdelln(w, 1)
#define deleteln()     		winsdelln(stdscr, -1)
#define wdeleteln(w)     	winsdelln(w, -1)
#define refresh()      		wrefresh(stdscr)
#define insch(c)       		winsch(stdscr,c)
#define delch()        		wdelch(stdscr)
#define setscrreg(t,b) 		wsetscrreg(stdscr,t,b)
#define scrl(n)			wscrl(stdscr,n)
#define timeout(delay)		wtimeout(stdscr, delay)
#define addnstr(str,n)		waddnstr(stdscr,str,n)
#define addchstr(str)		waddchstr(stdscr,str)
#define addchnstr(str,n)	waddchnstr(stdscr,str, n)
#define insdelln(n)		winsdelln(stdscr, n)
#define insstr(s)		winsstr(stdscr, s)
#define insnstr(s,n)		winsnstr(stdscr, s, n)

/*
 * mv functions
*/

#define mvwaddch(win,y,x,ch)    	(wmove(win,y,x) == ERR ? ERR : waddch(win,ch))
#define mvwgetch(win,y,x)       	(wmove(win,y,x) == ERR ? ERR : wgetch(win))
#define mvwaddchnstr(win,y,x,str,n)	(wmove(win,y,x) == ERR ? ERR : waddchnstr(win,str,n))
#define mvwaddchstr(win,y,x,str)  	(wmove(win,y,x) == ERR ? ERR : waddchnstr(win,str,-1))
#define mvwaddnstr(win,y,x,str,n)	(wmove(win,y,x) == ERR ? ERR : waddnstr(win,str,n))
#define mvwaddstr(win,y,x,str)  	(wmove(win,y,x) == ERR ? ERR : waddnstr(win,str,-1))
#define mvwgetstr(win,y,x,str)      	(wmove(win,y,x) == ERR ? ERR : wgetstr(win,str))
#define mvwinch(win,y,x)        	(wmove(win,y,x) == ERR ? ERR : winch(win))
#define mvwdelch(win,y,x)       	(wmove(win,y,x) == ERR ? ERR : wdelch(win))
#define mvwinsch(win,y,x,c)     	(wmove(win,y,x) == ERR ? ERR : winsch(win,c))
#define mvaddch(y,x,ch)         	mvwaddch(stdscr,y,x,ch)
#define mvgetch(y,x)            	mvwgetch(stdscr,y,x)
#define mvaddnstr(y,x,str,n)		mvwaddnstr(stdscr,y,x,str,n)
#define mvaddstr(y,x,str)       	mvwaddstr(stdscr,y,x,str)
#define mvgetstr(y,x,str)           	mvwgetstr(stdscr,y,x,str)
#define mvinch(y,x)             	mvwinch(stdscr,y,x)
#define mvdelch(y,x)            	mvwdelch(stdscr,y,x)
#define mvinsch(y,x,c)          	mvwinsch(stdscr,y,x,c)
#define mvwinsstr(w, y, x, s)		(wmove(w,y,x) == ERR ? ERR : winsstr(w,s))
#define mvwinsnstr(w, y, x, s, n)	(wmove(w,y,x) == ERR ? ERR : winsnstr(w,s,n))
#define mvinsstr(y,x,s)			mvwinsstr(stdscr,y,x,s)
#define mvinsnstr(y,x,s,n)		mvwinsnstr(stdscr,y,x,s,n)

/* Funny "characters" enabled for various special function keys for input */
/* Whether such a key exists depend if its definition is in the terminfo entry */

#define KEY_MIN		0401		/* Minimum curses key */
#define KEY_BREAK       0401            /* break key (unreliable) */
#define KEY_DOWN        0402            /* The four arrow keys ... */
#define KEY_UP          0403
#define KEY_LEFT        0404
#define KEY_RIGHT       0405            /* ... */
#define KEY_HOME        0406            /* Home key (upward+left arrow) */
#define KEY_BACKSPACE   0407            /* backspace (unreliable) */
#define KEY_F0          0410            /* Function keys.  Space for 64 */
#define KEY_F(n)        (KEY_F0+(n))    /* keys is reserved. */
#define KEY_DL          0510            /* Delete line */
#define KEY_IL          0511            /* Insert line */
#define KEY_DC          0512            /* Delete character */
#define KEY_IC          0513            /* Insert char or enter insert mode */
#define KEY_EIC         0514            /* Exit insert char mode */
#define KEY_CLEAR       0515            /* Clear screen */
#define KEY_EOS         0516            /* Clear to end of screen */
#define KEY_EOL         0517            /* Clear to end of line */
#define KEY_SF          0520            /* Scroll 1 line forward */
#define KEY_SR          0521            /* Scroll 1 line backwards (reverse) */
#define KEY_NPAGE       0522            /* Next page */
#define KEY_PPAGE       0523            /* Previous page */
#define KEY_STAB        0524            /* Set tab */
#define KEY_CTAB        0525            /* Clear tab */
#define KEY_CATAB       0526            /* Clear all tabs */
#define KEY_ENTER       0527            /* Enter or send (unreliable) */
#define KEY_SRESET      0530            /* soft (partial) reset (unreliable) */
#define KEY_RESET       0531            /* reset or hard reset (unreliable) */
#define KEY_PRINT       0532            /* print or copy */
#define KEY_LL          0533            /* home down or bottom (lower left) */

/* The keypad is arranged like this: */
/* a1    up    a3   */
/* left   b2  right  */
/* c1   down   c3   */

#define KEY_A1		0534	/* Upper left of keypad */
#define KEY_A3		0535	/* Upper right of keypad */
#define KEY_B2		0536	/* Center of keypad */
#define KEY_C1		0537	/* Lower left of keypad */
#define KEY_C3		0540	/* Lower right of keypad */
#define KEY_BTAB	0541	/* Back tab key */
#define KEY_BEG		0542	/* beg(inning) key */
#define KEY_CANCEL	0543	/* cancel key */
#define KEY_CLOSE	0544	/* close key */
#define KEY_COMMAND	0545	/* cmd (command) key */
#define KEY_COPY	0546	/* copy key */
#define KEY_CREATE	0547	/* create key */
#define KEY_END		0550	/* end key */
#define KEY_EXIT	0551	/* exit key */
#define KEY_FIND	0552	/* find key */
#define KEY_HELP	0553	/* help key */
#define KEY_MARK	0554	/* mark key */
#define KEY_MESSAGE	0555	/* message key */
#define KEY_MOVE	0556	/* move key */
#define KEY_NEXT	0557	/* next object key */
#define KEY_OPEN	0560	/* open key */
#define KEY_OPTIONS	0561	/* options key */
#define KEY_PREVIOUS	0562	/* previous object key */
#define KEY_REDO	0563	/* redo key */
#define KEY_REFERENCE	0564	/* ref(erence) key */
#define KEY_REFRESH	0565	/* refresh key */
#define KEY_REPLACE	0566	/* replace key */
#define KEY_RESTART	0567	/* restart key */
#define KEY_RESUME	0570	/* resume key */
#define KEY_SAVE	0571	/* save key */
#define KEY_SBEG	0572	/* shifted beginning key */
#define KEY_SCANCEL	0573	/* shifted cancel key */
#define KEY_SCOMMAND	0574	/* shifted command key */
#define KEY_SCOPY	0575	/* shifted copy key */
#define KEY_SCREATE	0576	/* shifted create key */
#define KEY_SDC		0577	/* shifted delete char key */
#define KEY_SDL		0600	/* shifted delete line key */
#define KEY_SELECT	0601	/* select key */
#define KEY_SEND	0602	/* shifted end key */
#define KEY_SEOL	0603	/* shifted clear line key */
#define KEY_SEXIT	0604	/* shifted exit key */
#define KEY_SFIND	0605	/* shifted find key */
#define KEY_SHELP	0606	/* shifted help key */
#define KEY_SHOME	0607	/* shifted home key */
#define KEY_SIC		0610	/* shifted input key */
#define KEY_SLEFT	0611	/* shifted left arrow key */
#define KEY_SMESSAGE	0612	/* shifted message key */
#define KEY_SMOVE	0613	/* shifted move key */
#define KEY_SNEXT	0614	/* shifted next key */
#define KEY_SOPTIONS	0615	/* shifted options key */
#define KEY_SPREVIOUS	0616	/* shifted prev key */
#define KEY_SPRINT	0617	/* shifted print key */
#define KEY_SREDO	0620	/* shifted redo key */
#define KEY_SREPLACE	0621	/* shifted replace key */
#define KEY_SRIGHT	0622	/* shifted right arrow */
#define KEY_SRSUME	0623	/* shifted resume key */
#define KEY_SSAVE	0624	/* shifted save key */
#define KEY_SSUSPEND	0625	/* shifted suspend key */
#define KEY_SUNDO	0626	/* shifted undo key */
#define KEY_SUSPEND	0627	/* suspend key */
#define KEY_UNDO	0630	/* undo key */
#define KEY_MAX		0777	/* Maximum curses key */

#endif
