/* vi.c */

/* Author:
 *	Steve Kirkendall
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


#include "config.h"
#include "ctype.h"
#include "vi.h"



/* This array describes what each key does */
#define NO_FUNC		(MARK (*)())0

#define NO_ARGS		0
#define CURSOR		1
#define CURSOR_CNT_KEY	2
#define CURSOR_MOVED	3
#define CURSOR_EOL	4
#define ZERO		5
#define DIGIT		6
#define CURSOR_TEXT	7
#define KEYWORD		8
#define ARGSMASK	0x0f
#define	C_C_K_REP1	(CURSOR_CNT_KEY | 0x10)
#define C_C_K_CUT	(CURSOR_CNT_KEY | 0x20)
#define C_C_K_MARK	(CURSOR_CNT_KEY | 0x30)
#define C_C_K_CHAR	(CURSOR_CNT_KEY | 0x40)
#ifndef NO_SHOWMODE
static int keymodes[] = {0, WHEN_REP1, WHEN_CUT, WHEN_MARK, WHEN_CHAR};
# define KEYMODE(args) (keymodes[(args) >> 4])
#else
# define KEYMODE(args) 0
#endif

static struct keystru
{
	MARK	(*func)();	/* the function to run */
	uchar	args;		/* description of the args needed */
#ifndef NO_VISIBLE
	short	flags;
#else
	uchar	flags;		/* other stuff */
#endif
}
	vikeys[] =
{
/* NUL not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#ifndef NO_EXTENSIONS
/* ^A  find cursor word */	{m_wsrch,	KEYWORD,	MVMT|NREL|VIZ},
#else
/* ^A  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/* ^B  page backward	*/	{m_scroll,	CURSOR,		FRNT|VIZ},
/* ^C  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^D  scroll dn 1/2page*/	{m_scroll,	CURSOR,		NCOL|VIZ},
/* ^E  scroll up	*/	{m_scroll,	CURSOR,		NCOL|VIZ},
/* ^F  page forward	*/	{m_scroll,	CURSOR,		FRNT|VIZ},
/* ^G  show file status	*/	{v_status,	NO_ARGS, 	NO_FLAGS},
/* ^H  move left, like h*/	{m_left,	CURSOR,		MVMT|VIZ},
/* ^I  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^J  move down	*/	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ|INCL},
/* ^K  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^L  redraw screen	*/	{v_redraw,	NO_ARGS,	NO_FLAGS|VIZ},
/* ^M  mv front next ln */	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ|INCL},
/* ^N  move down	*/	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ|INCL|NCOL},
/* ^O  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^P  move up		*/	{m_updnto,	CURSOR,		MVMT|LNMD|VIZ|INCL|NCOL},
/* ^Q  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^R  redraw screen	*/	{v_redraw,	NO_ARGS,	NO_FLAGS|VIZ},
/* ^S  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#ifndef NO_TAGSTACK
/* ^T  pop tagstack	*/	{v_pop,		CURSOR,		NO_FLAGS},
#else
/* ^T  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/* ^U  scroll up 1/2page*/	{m_scroll,	CURSOR,		NCOL|VIZ},
/* ^V  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^W  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^X  move to phys col	*/	{m_tocol,	CURSOR,		MVMT|NREL|VIZ},
/* ^Y  scroll down	*/	{m_scroll,	CURSOR,		NCOL|VIZ},
#ifdef SIGTSTP
/* ^Z  suspend elvis	*/	{v_suspend,	NO_ARGS,	NO_FLAGS},
#else
/* ^Z  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/* ESC not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^\  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* ^]  keyword is tag	*/	{v_tag,		KEYWORD,	NO_FLAGS},
/* ^^  previous file	*/	{v_switch,	CURSOR,		FRNT},
/* ^_  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/* SPC move right,like l*/	{m_right,	CURSOR,		MVMT|INCL|VIZ},
/*  !  run thru filter	*/	{v_filter,	CURSOR_MOVED,	FRNT|LNMD|INCL|VIZ},
/*  "  select cut buffer*/	{v_selcut,	C_C_K_CUT,	PTMV|VIZ},
#ifndef NO_EXTENSIONS
/*  #  increment number	*/	{v_increment,	KEYWORD,	SDOT},
#else
/*  #  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  $  move to rear	*/	{m_rear,	CURSOR,		MVMT|INCL|VIZ},
/*  %  move to match	*/	{m_match,	CURSOR,		MVMT|INCL|VIZ},
/*  &  repeat subst	*/	{v_again,	CURSOR_MOVED,	SDOT|NCOL|LNMD|INCL},
/*  '  move to a mark	*/	{m_tomark,	C_C_K_MARK,	MVMT|FRNT|NREL|LNMD|INCL|VIZ},
#ifndef NO_SENTENCE
/*  (  mv back sentence	*/	{m_sentence,	CURSOR,		MVMT|VIZ},
/*  )  mv fwd sentence	*/	{m_sentence,	CURSOR,		MVMT|VIZ},
#else
/*  (  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/*  )  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
#ifndef NO_ERRLIST
/*  *  errlist		*/	{v_errlist,	CURSOR,		FRNT|NREL},
#else
/*  *  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  +  mv front next ln */	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ|INCL},
#ifndef NO_CHARSEARCH
/*  ,  reverse [fFtT] cmd*/	{m__ch,		CURSOR,		MVMT|INCL|VIZ},
#else
/*  ,  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  -  mv front prev ln	*/	{m_updnto,	CURSOR,		MVMT|FRNT|LNMD|VIZ|INCL},
/*  .  special...	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/*  /  forward search	*/	{m_fsrch,	CURSOR_TEXT,	MVMT|NREL|VIZ},
/*  0  part of count?	*/	{NO_FUNC,	ZERO,		MVMT|PTMV|VIZ},
/*  1  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  2  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  3  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  4  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  5  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  6  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  7  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  8  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  9  part of count	*/	{NO_FUNC,	DIGIT,		PTMV|VIZ},
/*  :  run single EX cmd*/	{v_1ex,		CURSOR_TEXT,	NO_FLAGS},
#ifndef NO_CHARSEARCH
/*  ;  repeat [fFtT] cmd*/	{m__ch,		CURSOR,		MVMT|INCL|VIZ},
#else
/*  ;  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS|VIZ},
#endif
/*  <  shift text left	*/	{v_lshift,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/*  =  preset filter	*/	{v_reformat,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/*  >  shift text right	*/	{v_rshift,	CURSOR_MOVED,	SDOT|FRNT|LNMD|INCL|VIZ},
/*  ?  backward search	*/	{m_bsrch,	CURSOR_TEXT,	MVMT|NREL|VIZ},
#ifndef NO_AT
/*  @  execute a cutbuf */	{v_at,		C_C_K_CUT,	NO_FLAGS},
#else
/*  @  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  A  append at EOL	*/	{v_insert,	CURSOR,		SDOT},
/*  B  move back Word	*/	{m_bword,	CURSOR,		MVMT|VIZ},
/*  C  change to EOL	*/	{v_change,	CURSOR_EOL,	SDOT},
/*  D  delete to EOL	*/	{v_delete,	CURSOR_EOL,	SDOT},
/*  E  move end of Word	*/	{m_eword,	CURSOR,		MVMT|INCL|VIZ},
#ifndef NO_CHARSEARCH
/*  F  move bk to char	*/	{m_Fch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
#else
/*  F  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  G  move to line #	*/	{m_updnto,	CURSOR,		MVMT|NREL|LNMD|FRNT|INCL|VIZ},
/*  H  move to row	*/	{m_row,		CURSOR,		MVMT|LNMD|FRNT|VIZ|INCL},
/*  I  insert at front	*/	{v_insert,	CURSOR,		SDOT},
/*  J  join lines	*/	{v_join,	CURSOR,		SDOT},
#ifndef NO_EXTENSIONS
/*  K  look up keyword	*/	{v_keyword,	KEYWORD,	NO_FLAGS},
#else
/*  K  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  L  move to last row	*/	{m_row,		CURSOR,		MVMT|LNMD|FRNT|VIZ|INCL},
/*  M  move to mid row	*/	{m_row,		CURSOR,		MVMT|LNMD|FRNT|VIZ|INCL},
/*  N  reverse prev srch*/	{m_nsrch,	CURSOR,		MVMT|NREL|VIZ},
/*  O  insert above line*/	{v_insert,	CURSOR,		SDOT},
/*  P  paste before	*/	{v_paste,	CURSOR,		SDOT},
/*  Q  quit to EX mode	*/	{v_quit,	NO_ARGS,	NO_FLAGS},
/*  R  overtype		*/	{v_overtype,	CURSOR,		SDOT},
/*  S  change line	*/	{v_change,	CURSOR_MOVED,	SDOT},
#ifndef NO_CHARSEARCH
/*  T  move bk to char	*/	{m_Tch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
#else
/*  T  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  U  undo whole line	*/	{v_undoline,	CURSOR,		FRNT},
#ifndef NO_VISIBLE
/*  V  start visible	*/	{v_start,	CURSOR,		INCL|LNMD|VIZ},
#else
/*  V  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  W  move forward Word*/	{m_fword,	CURSOR,		MVMT|INCL|NWRP|VIZ},
/*  X  delete to left	*/	{v_xchar,	CURSOR,		SDOT},
/*  Y  yank text	*/	{v_yank,	CURSOR_MOVED,	NCOL},
/*  Z  save file & exit	*/	{v_xit,		CURSOR_CNT_KEY,	NO_FLAGS},
/*  [  move back section*/	{m_paragraph,	CURSOR,		MVMT|LNMD|NREL|VIZ},
#ifndef NO_POPUP
/*  \  pop-up menu	*/	{v_popup,	CURSOR_MOVED,	VIZ},
#else
/*  \  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  ]  move fwd section */	{m_paragraph,	CURSOR,		MVMT|LNMD|NREL|VIZ},
/*  ^  move to front	*/	{m_front,	CURSOR,		MVMT|VIZ},
/*  _  current line	*/	{m_updnto,	CURSOR,		MVMT|LNMD|FRNT|INCL},
/*  `  move to mark	*/	{m_tomark,	C_C_K_MARK,	MVMT|NREL|VIZ},
/*  a  append at cursor	*/	{v_insert,	CURSOR,		SDOT},
/*  b  move back word	*/	{m_bword,	CURSOR,		MVMT|VIZ},
/*  c  change text	*/	{v_change,	CURSOR_MOVED,	SDOT|VIZ},
/*  d  delete op	*/	{v_delete,	CURSOR_MOVED,	SDOT|VIZ},
/*  e  move end word	*/	{m_eword,	CURSOR,		MVMT|INCL|VIZ},
#ifndef NO_CHARSEARCH
/*  f  move fwd for char*/	{m_fch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
#else
/*  f  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  g  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/*  h  move left	*/	{m_left,	CURSOR,		MVMT|VIZ},
/*  i  insert at cursor	*/	{v_insert,	CURSOR,		SDOT},
/*  j  move down	*/	{m_updnto,	CURSOR,		MVMT|NCOL|LNMD|VIZ|INCL},
/*  k  move up		*/	{m_updnto,	CURSOR,		MVMT|NCOL|LNMD|VIZ|INCL},
/*  l  move right	*/	{m_right,	CURSOR,		MVMT|INCL|VIZ},
/*  m  define a mark	*/	{v_mark,	C_C_K_MARK,	NO_FLAGS},
/*  n  repeat prev srch	*/	{m_nsrch,	CURSOR, 	MVMT|NREL|VIZ},
/*  o  insert below line*/	{v_insert,	CURSOR,		SDOT},
/*  p  paste after	*/	{v_paste,	CURSOR,		SDOT},
/*  q  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
/*  r  replace chars	*/	{v_replace,	C_C_K_REP1,	SDOT},
/*  s  subst N chars	*/	{v_subst,	CURSOR,		SDOT},
#ifndef NO_CHARSEARCH
/*  t  move fwd to char	*/	{m_tch,		C_C_K_CHAR,	MVMT|INCL|VIZ},
#else
/*  t  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  u  undo		*/	{v_undo,	CURSOR,		NO_FLAGS},
#ifndef NO_VISIBLE
/*  v  start visible	*/	{v_start,	CURSOR,		INCL|VIZ},
#else
/*  v  not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS},
#endif
/*  w  move fwd word	*/	{m_fword,	CURSOR,		MVMT|INCL|NWRP|VIZ},
/*  x  delete character	*/	{v_xchar,	CURSOR,		SDOT},
/*  y  yank text	*/	{v_yank,	CURSOR_MOVED,	NCOL|VIZ},
/*  z  adjust scrn row	*/	{m_z, 		CURSOR_CNT_KEY,	NCOL|VIZ},
/*  {  back paragraph	*/	{m_paragraph,	CURSOR,		MVMT|VIZ},
/*  |  move to column	*/	{m_tocol,	CURSOR,		MVMT|NREL|VIZ},
/*  }  fwd paragraph	*/	{m_paragraph,	CURSOR,		MVMT|VIZ},
/*  ~  upper/lowercase	*/	{v_ulcase,	CURSOR,		SDOT},
/* DEL not defined	*/	{NO_FUNC,	NO_ARGS,	NO_FLAGS}
};



void vi()
{
	REG int			key;	/* keystroke from user */
	long			count;	/* numeric argument to some functions */
	REG struct keystru	*keyptr;/* pointer to vikeys[] element */
	MARK			tcurs;	/* temporary cursor */
	int			prevkey;/* previous key, if d/c/y/</>/! */
	MARK			range;	/* start of range for d/c/y/</>/! */
	char			text[132];
	int			dotkey;	/* last "key" of a change */
	int			dotpkey;/* last "prevkey" of a change */
	int			dotkey2;/* last extra "getkey()" of a change */
	int			dotcnt;	/* last "count" of a change */
	int			firstkey;
	REG int			i;

	/* tell the redraw() function to start from scratch */
	redraw(MARK_UNSET, FALSE);

#ifdef lint
	/* lint says that "range" might be used before it is set.  This
	 * can't really happen due to the way "range" and "prevkey" are used,
	 * but lint doesn't know that.  This line is here ONLY to keep lint
	 * happy.
	 */
	range = 0L;
#endif

	/* safeguard against '.' with no previous command */
	dotkey = dotpkey = dotkey2 = dotcnt = 0;

	/* go immediately into insert mode, if ":set inputmode" */
	firstkey = 0;
#ifndef NO_EXTENSIONS
	if (*o_inputmode)
	{
		firstkey = 'i';
	}
#endif

	/* Repeatedly handle VI commands */
	for (count = 0, prevkey = '\0'; mode == MODE_VI; )
	{
		/* if we've moved off the undoable line, then we can't undo it at all */
		if (markline(cursor) != U_line)
		{
			U_line = 0L;
		}

		/* report any changes from the previous command */
		if (rptlines >= *o_report)
		{
			redraw(cursor, FALSE);
			msg("%ld line%s %s", rptlines, (rptlines==1?"":"s"), rptlabel);
		}
		rptlines = 0L;

		/* get the next command key.  It must be ASCII */
		if (firstkey)
		{
			key = firstkey;
			firstkey = 0;
		}
		else
		{
			do
			{
				key = getkey(WHEN_VICMD);
			} while (key < 0 || key > 127);
		}
#ifdef DEBUG2
		debout("\nkey='%c'\n", key);
#endif

		/* Convert a doubled-up operator such as "dd" into "d_" */
		if (prevkey && key == prevkey)
		{
			key = '_';
		}

		/* look up the structure describing this command */
		keyptr = &vikeys[key];

		/* '&' and uppercase operators always act like doubled */
		if (!prevkey && keyptr->args == CURSOR_MOVED
			&& (key == '&' || isupper(key)))
		{
			range = cursor;
			prevkey = key;
			key = '_';
			keyptr = &vikeys[key];
		}

#ifndef NO_VISIBLE
		/* if we're in the middle of a v/V command, reject commands
		 * that aren't operators or movement commands
		 */
		if (V_from && !(keyptr->flags & VIZ))
		{
			beep();
			prevkey = 0;
			count = 0;
			continue;
		}
#endif

		/* if we're in the middle of a d/c/y/</>/! command, reject
		 * anything but movement.
		 */
		if (prevkey && !(keyptr->flags & (MVMT|PTMV)))
		{
			beep();
			prevkey = 0;
			count = 0;
			continue;
		}

		/* set the "dot" variables, if we're supposed to */
		if (((keyptr->flags & SDOT)
			|| (prevkey && vikeys[prevkey].flags & SDOT))
#ifndef NO_VISIBLE
		    && !V_from
#endif
		)
		{
			dotkey = key;
			dotpkey = prevkey;
			dotkey2 = '\0';
			dotcnt = count;

			/* remember the line before any changes are made */
			if (U_line != markline(cursor))
			{
				U_line = markline(cursor);
				strcpy(U_text, fetchline(U_line));
			}
		}

		/* if this is "." then set other vars from the "dot" vars */
		if (key == '.')
		{
			key = dotkey;
			keyptr = &vikeys[key];
			prevkey = dotpkey;
			if (prevkey)
			{
				range = cursor;
			}
			if (count == 0)
			{
				count = dotcnt;
			}
			doingdot = TRUE;

			/* remember the line before any changes are made */
			if (U_line != markline(cursor))
			{
				U_line = markline(cursor);
				strcpy(U_text, fetchline(U_line));
			}
		}
		else
		{
			doingdot = FALSE;
		}

		/* process the key as a command */
		tcurs = cursor;
		force_flags = NO_FLAGS;
		switch (keyptr->args & ARGSMASK)
		{
		  case ZERO:
			if (count == 0)
			{
				tcurs = cursor & ~(BLKSIZE - 1);
				break;
			}
			/* else fall through & treat like other digits... */

		  case DIGIT:
			count = count * 10 + key - '0';
			break;

		  case KEYWORD:
			/* if not on a keyword, fail */
			pfetch(markline(cursor));
			key = markidx(cursor);
			if (!isalnum(ptext[key]))
			{
				tcurs = MARK_UNSET;
				break;
			}

			/* find the start of the keyword */
			while (key > 0 && isalnum(ptext[key - 1]))
			{
				key--;
			}
			tcurs = (cursor & ~(BLKSIZE - 1)) + key;

			/* copy it into a buffer, and NUL-terminate it */
			i = 0;
			do
			{
				text[i++] = ptext[key++];
			} while (isalnum(ptext[key]));
			text[i] = '\0';

			/* call the function */
			tcurs = (*keyptr->func)(text, tcurs, count);
			count = 0L;
			break;

		  case NO_ARGS:
			if (keyptr->func)
			{
				(*keyptr->func)();
			}
			else
			{
				beep();
			}
			count = 0L;
			break;
	
		  case CURSOR:
			tcurs = (*keyptr->func)(cursor, count, key, prevkey);
			count = 0L;
			break;

		  case CURSOR_CNT_KEY:
			if (doingdot)
			{
				tcurs = (*keyptr->func)(cursor, count, dotkey2);
			}
			else
			{
				/* get a key */
				i = getkey(KEYMODE(keyptr->args));
				if (i == '\033') /* ESC */
				{
					count = 0;
					tcurs = MARK_UNSET;
					break; /* exit from "case CURSOR_CNT_KEY" */
				}
				else if (i == ctrl('V'))
				{
					i = getkey(0);
				}

				/* if part of an SDOT command, remember it */
				 if (keyptr->flags & SDOT
				 || (prevkey && vikeys[prevkey].flags & SDOT))
				{
					dotkey2 = i;
				}

				/* do it */
				tcurs = (*keyptr->func)(cursor, count, i);
			}
			if (keyptr->args != C_C_K_CUT)
				count = 0L;
			break;
	
		  case CURSOR_MOVED:
#ifndef NO_VISIBLE
			if (V_from)
			{
				range = cursor;
				tcurs = V_from;
				count = 0L;
				prevkey = key;
				key = (V_linemd ? 'V' : 'v');
				keyptr = &vikeys[key];
			}
			else
#endif
			{
				prevkey = key;
				range = cursor;
				force_flags = LNMD|INCL;
			}
			break;

		  case CURSOR_EOL:
			prevkey = key;
			/* a zero-length line needs special treatment */
			pfetch(markline(cursor));
			if (plen == 0)
			{
				/* act on a zero-length section of text */
				range = tcurs = cursor;
				key = '0';
			}
			else
			{
				/* act like CURSOR_MOVED with '$' movement */
				range = cursor;
				tcurs = m_rear(cursor, 1L);
				key = '$';
			}
			count = 0L;
			keyptr = &vikeys[key];
			break;

		  case CURSOR_TEXT:
		  	do
		  	{	
				text[0] = key;
				text[1] = '\0';
				if (doingdot || vgets(key, text + 1, sizeof text - 1) >= 0)
				{
					/* reassure user that <CR> was hit */
					qaddch('\r');
					refresh();

					/* call the function with the text */
					tcurs = (*keyptr->func)(cursor, text);
				}
				else
				{
					if (exwrote || mode == MODE_COLON)
					{
						redraw(MARK_UNSET, FALSE);
					}
					mode = MODE_VI;
				}
			} while (mode == MODE_COLON);
			count = 0L;
			break;
		}

		/* if that command took us out of vi mode, then exit the loop
		 * NOW, without tweaking the cursor or anything.  This is very
		 * important when mode == MODE_QUIT.
		 */
		if (mode != MODE_VI)
		{
			break;
		}

		/* now move the cursor, as appropriate */
		if (prevkey && ((keyptr->flags & MVMT)
#ifndef NO_VISIBLE
					       || V_from
#endif
				) && count == 0L)
		{
			/* movements used as targets are less strict */
			tcurs = adjmove(cursor, tcurs, (int)(keyptr->flags | force_flags));
		}
		else if (keyptr->args == CURSOR_MOVED)
		{
			/* the < and > keys have FRNT,
			 * but it shouldn't be applied yet
			 */
			tcurs = adjmove(cursor, tcurs, FINL);
		}
		else
		{
			tcurs = adjmove(cursor, tcurs, (int)(keyptr->flags | force_flags | FINL));
		}

		/* was that the end of a d/c/y/</>/! command? */
		if (prevkey && ((keyptr->flags & MVMT)
#ifndef NO_VISIBLE
					       || V_from
#endif
				) && count == 0L)
		{
#ifndef NO_VISIBLE
			/* turn off the hilight */
			V_from = 0L;
#endif

			/* if the movement command failed, cancel operation */
			if (tcurs == MARK_UNSET)
			{
				prevkey = 0;
				count = 0;
				continue;
			}

			/* make sure range=front and tcurs=rear.  Either way,
			 * leave cursor=range since that's where we started.
			 */
			cursor = range;
			if (tcurs < range)
			{
				range = tcurs;
				tcurs = cursor;
			}

			/* The 'w' and 'W' destinations should never take us
			 * to the front of a line.  Instead, they should take
			 * us only to the end of the preceding line.
			 */
			if ((keyptr->flags & NWRP) == NWRP
			  && markline(range) < markline(tcurs)
			  && (markline(tcurs) > nlines || tcurs == m_front(tcurs, 0L)))
			{
				tcurs = (tcurs & ~(BLKSIZE - 1)) - BLKSIZE;
				pfetch(markline(tcurs));
				tcurs += plen;
			}

			/* adjust for line mode & inclusion of last char/line */
			i = (keyptr->flags | vikeys[prevkey].flags);
			switch ((i | force_flags) & (INCL|LNMD))
			{
			  case INCL:
				tcurs++;
				break;

			  case INCL|LNMD:
				tcurs += BLKSIZE;
				/* fall through... */

			  case LNMD:
				range &= ~(BLKSIZE - 1);
				tcurs &= ~(BLKSIZE - 1);
				break;
			}

			/* run the function */
			tcurs = (*vikeys[prevkey].func)(range, tcurs);
			if (mode == MODE_VI)
			{
				(void)adjmove(cursor, cursor, FINL);
				cursor = adjmove(cursor, tcurs, (int)(vikeys[prevkey].flags | FINL));
			}

			/* cleanup */
			prevkey = 0;
		}
		else if (!prevkey)
		{
			if (tcurs != MARK_UNSET)
				cursor = tcurs;
		}
	}
}

/* This function adjusts the MARK value that they return; here we make sure
 * it isn't past the end of the line, and that the column hasn't been
 * *accidentally* changed.
 */
MARK adjmove(old, new, flags)
	MARK		old;	/* the cursor position before the command */
	REG MARK	new;	/* the cursor position after the command */
	int		flags;	/* various flags regarding cursor mvmt */
{
	static int	colno;	/* the column number that we want */
	REG char	*text;	/* used to scan through the line's text */
	REG int		i;

#ifdef DEBUG2
	debout("adjmove(%ld.%d, %ld.%d, 0x%x)\n", markline(old), markidx(old), markline(new), markidx(new), flags);
#endif
#ifdef DEBUG
	watch();
#endif

	/* if the command failed, bag it! */
	if (new == MARK_UNSET)
	{
		if (flags & FINL)
		{
			beep();
			return old;
		}
		return new;
	}

	/* if this is a non-relative movement, set the '' mark */
	if (flags & NREL)
	{
		mark[26] = old;
	}

	/* make sure it isn't past the end of the file */
	if (markline(new) < 1)
	{
		new = MARK_FIRST;
	}
	else if (markline(new) > nlines)
	{
		if (!(flags & FINL))
		{
			return MARK_EOF;
		}
		new = MARK_LAST;
	}

	/* fetch the new line */
	pfetch(markline(new));

	/* move to the front, if we're supposed to */
	if (flags & FRNT)
	{
		new = m_front(new, 1L);
	}

	/* change the column#, or change the mark to suit the column# */
	if (!(flags & NCOL))
	{
		/* change the column# */
		i = markidx(new);
		if (i == BLKSIZE - 1)
		{
			new &= ~(BLKSIZE - 1);
			if (plen > 0)
			{
				new += plen - 1;
			}
			colno = BLKSIZE * 8; /* one heck of a big colno */
		}
		else if (plen > 0)
		{
			if (i >= plen)
			{
				new = (new & ~(BLKSIZE - 1)) + plen - 1;
			}
			colno = idx2col(new, ptext, FALSE);
		}
		else
		{
			new &= ~(BLKSIZE - 1);
			colno = 0;
		}
	}
	else
	{
		/* adjust the mark to get as close as possible to column# */
		for (i = 0, text = ptext; i <= colno && *text; text++)
		{
			if (*text == '\t' && !*o_list)
			{
				i += *o_tabstop - (i % *o_tabstop);
			}
			else if (UCHAR(*text) < ' ' || *text == 127)
			{
				i += 2;
			}
#ifndef NO_CHARATTR
			else if (*o_charattr && text[0] == '\\' && text[1] == 'f' && text[2])
			{
				text += 2; /* plus one more in "for()" stmt */
			}
#endif
			else
			{
				i++;
			}
		}
		if (text > ptext)
		{
			text--;
		}
		new = (new & ~(BLKSIZE - 1)) + (int)(text - ptext);
	}

	return new;
}


#ifdef DEBUG
watch()
{
	static wasset;

	if (*origname)
	{
		wasset = TRUE;
	}
	else if (wasset)
	{
		mode = MODE_EX;
		msg("origname was clobbered");
		endwin();
		abort();
	}

	if (wasset && nlines == 0)
	{
		mode = MODE_EX;
		msg("nlines=0");
		endwin();
		abort();
	}
}
#endif
