#ifndef lint
static char *RCSid = "$Id: readline.c,v 1.1.1.1 1995/04/25 15:05:09 jkh Exp $";
#endif


/* GNUPLOT - readline.c */
/*
 * Copyright (C) 1986 - 1993   Thomas Williams, Colin Kelley
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the modified code.  Modifications are to be distributed
 * as patches to released version.
 *
 * This software is provided "as is" without express or implied warranty.
 *
 *
 * AUTHORS
 *
 *   Original Software:
 *     Tom Tkacik
 *
 *   Msdos port and some enhancements:
 *     Gershon Elber and many others.
 *
 * There is a mailing list for gnuplot users. Note, however, that the
 * newsgroup
 *	comp.graphics.gnuplot
 * is identical to the mailing list (they
 * both carry the same set of messages). We prefer that you read the
 * messages through that newsgroup, to subscribing to the mailing list.
 * (If you can read that newsgroup, and are already on the mailing list,
 * please send a message info-gnuplot-request@dartmouth.edu, asking to be
 * removed from the mailing list.)
 *
 * The address for mailing to list members is
 *	   info-gnuplot@dartmouth.edu
 * and for mailing administrative requests is
 *	   info-gnuplot-request@dartmouth.edu
 * The mailing list for bug reports is
 *	   bug-gnuplot@dartmouth.edu
 * The list of those interested in beta-test versions is
 *	   info-gnuplot-beta@dartmouth.edu
 */

#ifdef READLINE
#ifdef ATARI
#include "plot.h"
#endif
#ifdef _WINDOWS
#define _Windows
#endif

/* a small portable version of GNU's readline */
/* this is not the BASH or GNU EMACS version of READLINE due to Copyleft
	restrictions */
/* do not need any terminal capabilities except backspace, */
/* and space overwrites a character */

/* NANO-EMACS line editing facility */
/* printable characters print as themselves (insert not overwrite) */
/* ^A moves to the beginning of the line */
/* ^B moves back a single character */
/* ^E moves to the end of the line */
/* ^F moves forward a single character */
/* ^K kills from current position to the end of line */
/* ^P moves back through history */
/* ^N moves forward through history */
/* ^H and DEL delete the previous character */
/* ^D deletes the current character, or EOF if line is empty */
/* ^L/^R redraw line in case it gets trashed */
/* ^U kills the entire line */
/* ^W kills last word */
/* LF and CR return the entire line regardless of the cursor postition */
/* EOF with an empty line returns (char *)NULL */

/* all other characters are ignored */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#if !defined(MSDOS) && !defined(ATARI) && !defined(_Windows) && !defined(DOS386)

/*
 * Set up structures using the proper include file
 */
#if defined(_IBMR2) || defined(alliant)
#define SGTTY
#endif

/*  submitted by Francois.Dagorn@cicb.fr */
#ifdef SGTTY
#include <sgtty.h>
static struct sgttyb orig_termio, rl_termio;
/* define terminal control characters */
static struct tchars s_tchars;
#define VERASE    0
#define VEOF      1
#define VKILL     2
#ifdef TIOCGLTC		 /* available only with the 'new' line discipline */
static struct ltchars s_ltchars;
#define VWERASE   3
#define VREPRINT  4
#define VSUSP     5
#endif /* TIOCGLTC */
#define NCCS      6

#else /* SGTTY */

/* SIGTSTP defines job control */
/* if there is job control then we need termios.h instead of termio.h */
/* (Are there any systems with job control that use termio.h?  I hope not.) */
#ifdef SIGTSTP
#define TERMIOS
#include <termios.h>
/* Added by Robert Eckardt, RobertE@beta.TP2.Ruhr-Uni-Bochum.de */
#ifdef ISC22
#ifndef ONOCR			/* taken from sys/termio.h */
#define ONOCR 0000020	/* true at least for ISC 2.2 */
#endif
#ifndef IUCLC
#define IUCLC 0001000
#endif
#endif /* ISC22 */

static struct termios orig_termio, rl_termio;
#else
#include <termio.h>
static struct termio orig_termio, rl_termio;
/* termio defines NCC instead of NCCS */
#define NCCS    NCC
#endif /* SIGTSTP */
#endif /* SGTTY */

/* ULTRIX defines VRPRNT instead of VREPRINT */
#ifdef VRPRNT
#define VREPRINT VRPRNT
#endif

/* define characters to use with our input character handler */
static char term_chars[NCCS];

static int term_set = 0;	/* =1 if rl_termio set */

#define special_getc() ansi_getc()
static char ansi_getc();

#else /* !MSDOS && !ATARI && !_Windows */

#ifdef _Windows
#include <windows.h>
#include "win/wtext.h"
#include "win/wgnuplib.h"
extern TW textwin;
#define TEXTUSER 0xf1
#define TEXTGNUPLOT 0xf0
#define special_getc() msdos_getch()
static char msdos_getch();
#endif

#if defined(MSDOS) || defined(DOS386)
/* MSDOS specific stuff */
#ifdef DJGPP
#include <pc.h>
#endif
#ifdef __EMX__
#include <conio.h>
#endif
#define special_getc() msdos_getch()
static char msdos_getch();
#endif /* MSDOS */

#ifdef ATARI
#include <stdlib.h>
#ifdef __PUREC__
#include <tos.h>
#else
#include <osbind.h>
#endif
#define special_getc() tos_getch()
static char tos_getch();
#endif

#endif /* !MSDOS && !ATARI && !_Windows */

#if !defined(ATARI)
/* is it <string.h> or <strings.h>?   just declare what we need */
extern int      strlen();
extern char *strcpy();
#endif
#define alloc malloc
extern char *alloc();	/* we'll use the safe malloc from misc.c */

#define MAXBUF	1024
#define BACKSPACE 0x08	/* ^H */
#define SPACE	' '

struct hist {
	char *line;
	struct hist *prev;
	struct hist *next;
};

static struct hist *history = NULL;  /* no history yet */
static struct hist *cur_entry = NULL;

static char cur_line[MAXBUF];  /* current contents of the line */
static int cur_pos = 0;	/* current position of the cursor */
static int max_pos = 0;	/* maximum character position */


void add_history();
static void fix_line();
static void redraw_line();
static void clear_line();
static void clear_eoline();
static void copy_line();
static void set_termio();
void reset_termio();

/* user_putc and user_puts should be used in the place of
 * fputc(ch,stderr) and fputs(str,stderr) for all output
 * of user typed characters.  This allows MS-Windows to
 * display user input in a different color. */
int
user_putc(ch)
int ch;
{
	int rv;
#ifdef _Windows
	TextAttr(&textwin,TEXTUSER);
#endif
	rv = fputc(ch, stderr);
#ifdef _Windows
	TextAttr(&textwin,TEXTGNUPLOT);
#endif
	return rv;
}

int
user_puts(str)
char *str;
{
	int rv;
#ifdef _Windows
	TextAttr(&textwin,TEXTUSER);
#endif
	rv = fputs(str, stderr);
#ifdef _Windows
	TextAttr(&textwin,TEXTGNUPLOT);
#endif
	return rv;
}

/* This function provides a centralized non-destructive backspace capability */
/* M. Castro */

backspace()
{
	user_putc(BACKSPACE);
}

char *
readline(prompt)
char *prompt;
{

	unsigned char cur_char;
	char *new_line;
	/* unsigned char *new_line; */

	/* set the termio so we can do our own input processing */
	set_termio();

	/* print the prompt */
	fputs(prompt, stderr);
	cur_line[0] = '\0';
	cur_pos = 0;
	max_pos = 0;
	cur_entry = NULL;

	/* get characters */
	for(;;) {
		cur_char = special_getc();
#ifdef OS2
 /* for emx: remap scan codes for cursor keys */
                if( cur_char == 0 ) {
                    cur_char = getc(stdin);
                    switch( cur_char){
                        case 75:  /* left, map to ^B */
                            cur_char=2;
                            break ;
                        case 77:  /* right, map to ^F */
                            cur_char=6;
                            break ;
                        case 115: /* ctrl left */
                        case 71:  /* home, map to ^A */
                            cur_char=1;
                            break ;
                        case 116: /* ctrl right */
                        case 79:  /* end, map to ^E */
                            cur_char=5;
                            break ;
                        case 72:  /* up, map to ^P */
                            cur_char=16;
                            break ;
                        case 80:  /* down, map to ^N */
                            cur_char=14;
                            break ;
                        case 83:  /* delete, map to ^D */
                            cur_char=4;
                            break ;
                        default:  /* ignore */
                            cur_char=0;
                            continue ;
                            }
                        }
#endif  /*OS2*/
		if((isprint(cur_char)
#if defined(ATARI) || defined(_Windows) || defined(MSDOS) || defined(DOS386)
   /* this should be used for all 8bit ASCII machines, I guess */
				    || ((unsigned char)cur_char > 0x7f)
#endif
		                       )&& max_pos<MAXBUF-1) {
			int i;
			for(i=max_pos; i>cur_pos; i--) {
				cur_line[i] = cur_line[i-1];
			}
			user_putc(cur_char);
			cur_line[cur_pos] = cur_char;
			cur_pos += 1;
			max_pos += 1;
			if (cur_pos < max_pos)
			    fix_line();
			cur_line[max_pos] = '\0';

		/* else interpret unix terminal driver characters */
#ifdef VERASE
		} else if(cur_char == term_chars[VERASE] ){  /* DEL? */
			if(cur_pos > 0) {
				int i;
				cur_pos -= 1;
				backspace();
				for(i=cur_pos; i<max_pos; i++)
					cur_line[i] = cur_line[i+1];
				max_pos -= 1;
				fix_line();
			}
#endif /* VERASE */
#ifdef VEOF
		} else if(cur_char == term_chars[VEOF] ){   /* ^D? */
			if(max_pos == 0) {
				reset_termio();
				return((char *)NULL);
			}
			if((cur_pos < max_pos)&&(cur_char == 004)) { /* ^D */
				int i;
				for(i=cur_pos; i<max_pos; i++)
					cur_line[i] = cur_line[i+1];
				max_pos -= 1;
				fix_line();
			}
#endif /* VEOF */
#ifdef VKILL
		} else if(cur_char == term_chars[VKILL] ){  /* ^U? */
			clear_line(prompt);
#endif /* VKILL */
#ifdef VWERASE
		} else if(cur_char == term_chars[VWERASE] ){  /* ^W? */
			while((cur_pos > 0) &&
			      (cur_line[cur_pos-1] == SPACE)) {
				cur_pos -= 1;
				backspace();
			}
			while((cur_pos > 0) &&
			      (cur_line[cur_pos-1] != SPACE)) {
				cur_pos -= 1;
				backspace();
			}
			clear_eoline();
			max_pos = cur_pos;
#endif /* VWERASE */
#ifdef VREPRINT
		} else if(cur_char == term_chars[VREPRINT] ){  /* ^R? */
			putc('\n',stderr); /* go to a fresh line */
			redraw_line(prompt);
#endif /* VREPRINT */
#ifdef VSUSP
		} else if(cur_char == term_chars[VSUSP]) {
			reset_termio();
			kill(0, SIGTSTP);

			/* process stops here */

			set_termio();
			/* print the prompt */
			redraw_line(prompt);
#endif /* VSUSP */
		} else {
			/* do normal editing commands */
			/* some of these are also done above */
			int i;
			switch(cur_char) {
			    case EOF:
				reset_termio();
				return((char *)NULL);
			    case 001: /* ^A */
				while(cur_pos > 0) {
					cur_pos -= 1;
					backspace();
				}
				break;
			    case 002: /* ^B */
				if(cur_pos > 0) {
					cur_pos -= 1;
					backspace();
				}
				break;
			    case 005: /* ^E */
				while(cur_pos < max_pos) {
					user_putc(cur_line[cur_pos]);
					cur_pos += 1;
				}
				break;
			    case 006: /* ^F */
				if(cur_pos < max_pos) {
					user_putc(cur_line[cur_pos]);
					cur_pos += 1;
				}
				break;
			    case 013: /* ^K */
				clear_eoline();
				max_pos = cur_pos;
				break;
			    case 020: /* ^P */
				if(history != NULL) {
					if(cur_entry == NULL) {
						cur_entry = history;
						clear_line(prompt);
						copy_line(cur_entry->line);
					} else if(cur_entry->prev != NULL) {
						cur_entry = cur_entry->prev;
						clear_line(prompt);
						copy_line(cur_entry->line);
					}
				}
				break;
			    case 016: /* ^N */
				if(cur_entry != NULL) {
					cur_entry = cur_entry->next;
					clear_line(prompt);
					if(cur_entry != NULL)
						copy_line(cur_entry->line);
					else
						cur_pos = max_pos = 0;
				}
				break;
			    case 014: /* ^L */
			    case 022: /* ^R */
				putc('\n',stderr); /* go to a fresh line */
				redraw_line(prompt);
				break;
			    case 0177: /* DEL */
			    case 010: /* ^H */
				if(cur_pos > 0) {
					cur_pos -= 1;
					backspace();
					for(i=cur_pos; i<max_pos; i++)
						cur_line[i] = cur_line[i+1];
					max_pos -= 1;
					fix_line();
				}
				break;
			    case 004: /* ^D */
				if(max_pos == 0) {
					reset_termio();
					return((char *)NULL);
				}
				if(cur_pos < max_pos) {
					for(i=cur_pos; i<max_pos; i++)
						cur_line[i] = cur_line[i+1];
					max_pos -= 1;
					fix_line();
				}
				break;
			    case 025:  /* ^U */
				clear_line(prompt);
				break;
			    case 027:  /* ^W */
				while((cur_pos > 0) &&
				      (cur_line[cur_pos-1] == SPACE)) {
					cur_pos -= 1;
					backspace();
				}
				while((cur_pos > 0) &&
				      (cur_line[cur_pos-1] != SPACE)) {
					cur_pos -= 1;
					backspace();
				}
				clear_eoline();
				max_pos = cur_pos;
				break;
			    case '\n': /* ^J */
			    case '\r': /* ^M */
				cur_line[max_pos+1] = '\0';
				putc('\n', stderr);
				new_line = (char *)alloc((unsigned long) (strlen(cur_line)+1), "history");
				strcpy(new_line,cur_line);
				reset_termio();
				return(new_line);
			    default:
				break;
			}
		}
	}
}

/* fix up the line from cur_pos to max_pos */
/* do not need any terminal capabilities except backspace, */
/* and space overwrites a character */
static void
fix_line()
{
	int i;

	/* write tail of string */
	for(i=cur_pos; i<max_pos; i++)
		user_putc(cur_line[i]);

	/* write a space at the end of the line in case we deleted one */
	user_putc(SPACE);

	/* backup to original position */
	for(i=max_pos+1; i>cur_pos; i--)
		backspace();

}

/* redraw the entire line, putting the cursor where it belongs */
static void
redraw_line(prompt)
char *prompt;
{
	int i;

	fputs(prompt, stderr);
	user_puts(cur_line);

	/* put the cursor where it belongs */
	for(i=max_pos; i>cur_pos; i--)
		backspace();
}

/* clear cur_line and the screen line */
static void
clear_line(prompt)
char *prompt;
{
	int i;
	for(i=0; i<max_pos; i++)
		cur_line[i] = '\0';

	for(i=cur_pos; i>0; i--)
		backspace();

	for(i=0; i<max_pos; i++)
		putc(SPACE, stderr);

	putc('\r', stderr);
	fputs(prompt, stderr);

	cur_pos = 0;
	max_pos = 0;
}

/* clear to end of line and the screen end of line */
static void
clear_eoline(prompt)
char *prompt;
{
	int i;
	for(i=cur_pos; i<max_pos; i++)
		cur_line[i] = '\0';

	for(i=cur_pos; i<max_pos; i++)
		putc(SPACE, stderr);
	for(i=cur_pos; i<max_pos; i++)
		backspace();
}

/* copy line to cur_line, draw it and set cur_pos and max_pos */
static void
copy_line(line)
char *line;
{
	strcpy(cur_line, line);
	user_puts(cur_line);
	cur_pos = max_pos = strlen(cur_line);
}

/* add line to the history */
void
add_history(line)
char *line;
{
	struct hist *entry;
	entry = (struct hist *)alloc((unsigned long)sizeof(struct hist),"history");
	entry->line = alloc((unsigned long)(strlen(line)+1),"history");
	strcpy(entry->line, line);

	entry->prev = history;
	entry->next = NULL;
	if(history != NULL) {
		history->next = entry;
	}
	history = entry;
}


/* Convert ANSI arrow keys to control characters */
static char
ansi_getc()
{
  char c = getc(stdin);
  if (c == 033) {
    c = getc(stdin); /* check for CSI */
    if (c == '[') {
      c = getc(stdin); /* get command character */
      switch (c) {
      case 'D': /* left arrow key */
	c = 002;
	break;
      case 'C': /* right arrow key */
	c = 006;
	break;
      case 'A': /* up arrow key */
	c = 020;
	break;
      case 'B': /* down arrow key */
	c = 016;
	break;
      }
    }
  }
  return c;
}

#if defined(MSDOS) || defined(_Windows) || defined(DOS386)

/* Convert Arrow keystrokes to Control characters: */
static  char
msdos_getch()
{
#ifdef DJGPP
	char c;
	int ch = getkey();
	c = (ch & 0xff00) ? 0 : ch & 0xff;
#else
    char c = getch();
#endif

    if (c == 0) {
#ifdef DJGPP
	c = ch & 0xff;
#else
	c = getch(); /* Get the extended code. */
#endif
	switch (c) {
	    case 75: /* Left Arrow. */
		c = 002;
		break;
	    case 77: /* Right Arrow. */
		c = 006;
		break;
	    case 72: /* Up Arrow. */
		c = 020;
		break;
	    case 80: /* Down Arrow. */
		c = 016;
		break;
	    case 115: /* Ctl Left Arrow. */
	    case 71: /* Home */
		c = 001;
		break;
	    case 116: /* Ctl Right Arrow. */
	    case 79: /* End */
		c = 005;
		break;
	    case 83: /* Delete */
		c = 004;
		break;
	    default:
		c = 0;
		break;
	}
    }
    else if (c == 033) { /* ESC */
	c = 025;
    }


    return c;
}

#endif /* MSDOS */

#ifdef ATARI

/* Convert Arrow keystrokes to Control characters: TOS version */

/* the volatile could be necessary to keep gcc from reordering
   the two Super calls
*/
#define CONTERM ((/*volatile*/ char *)0x484L)

static void
remove_conterm()
{
  void *ssp=(void*)Super(0L);
  *CONTERM &= ~0x8;
  Super(ssp);
}

static	char
tos_getch()
{
    long rawkey;
    char c;
    int scan_code;
    void *ssp;
    static  int init = 1;
    static  int in_help = 0;

    if (in_help) {
	switch(in_help) {
	    case 1:
	    case 5: in_help++; return 'e';
	    case 2:
	    case 6: in_help++; return 'l';
	    case 3:
	    case 7: in_help++; return 'p';
	    case 4: in_help = 0; return 0x0d;
	    case 8: in_help = 0; return ' ';
	}
    }

    if (init) {
	ssp = (void*)Super(0L);
	if( !(*CONTERM & 0x8) ) {
	    *CONTERM |= 0x8;
	} else {
	    init=0;
	}
	(void)Super(ssp);
	if( init ) {
	    atexit(remove_conterm);
	    init = 0;
	}
    }

   (void)Cursconf(1, 0); /* cursor on */
    rawkey = Cnecin();
    c = (char)rawkey;
    scan_code= ((int)(rawkey>>16)) & 0xff;	/* get the scancode */
    if( rawkey&0x07000000 ) scan_code |= 0x80; 	/* shift or control */

    switch (scan_code) {
	case 0x62:				/* HELP		*/
	    if (max_pos==0) {
		in_help = 1;
		return 'h';
	    } else {
		return 0;
	    }
	case 0xe2:				/* shift HELP	*/
	    if (max_pos==0) {
		in_help = 5;
		return 'h';
	    } else {
		return 0;
	    }
	case 0x48: /* Up Arrow */
	    return 0x10; /* ^P */
	case 0x50: /* Down Arrow */
	    return 0x0e; /* ^N */
	case 0x4b: /* Left Arrow */
	    return 0x02; /* ^B */
	case 0x4d: /* Right Arrow */
	    return 0x06; /* ^F */
	case 0xcb: /* Shift Left Arrow */
	case 0xf3: /* Ctrl Left Arrow (TOS-bug ?) */
	case 0x47: /* Home */
	    return 0x01; /* ^A */
	case 0xcd: /* Shift Right Arrow */
	case 0xf4: /* Ctrl Right Arrow (TOS-bug ?) */
	case 0xc7: /* Shift Home */
	case 0xf7: /* Crtl Home */
	    return 0x05; /* ^E */
	case 0x61: /* Undo - redraw line */
	    return 0x0c; /* ^L */
	default:
	    if (c == 0x1b) return 0x15; /* ESC becomes ^U */
	    if (c == 0x7f) return 0x04; /* Del becomes ^D */
	    break;
    }

    return c;
}

#endif /* ATARI */

  /* set termio so we can do our own input processing */
static void
set_termio()
{
#if !defined(MSDOS) && !defined(ATARI) && !defined(_Windows) && !defined(DOS386)
/* set termio so we can do our own input processing */
/* and save the old terminal modes so we can reset them later */
	if(term_set == 0) {
		/*
		 * Get terminal modes.
		 */
#ifdef SGTTY
		ioctl(0, TIOCGETP, &orig_termio);
#else  /* SGTTY */
#ifdef TERMIOS
#ifdef TCGETS
		ioctl(0, TCGETS, &orig_termio);
#else
		tcgetattr(0, &orig_termio);
#endif /* TCGETS */
#else
		ioctl(0, TCGETA, &orig_termio);
#endif /* TERMIOS */
#endif /* SGTTY */

		/*
		 * Save terminal modes
		 */
		rl_termio = orig_termio;

		/*
		 * Set the modes to the way we want them
		 *  and save our input special characters
		 */
#ifdef SGTTY
		rl_termio.sg_flags |= CBREAK;
		rl_termio.sg_flags &= ~(ECHO|XTABS);
		ioctl(0, TIOCSETN, &rl_termio);

		ioctl(0, TIOCGETC, &s_tchars);
		term_chars[VERASE]   = orig_termio.sg_erase;
		term_chars[VEOF]     = s_tchars.t_eofc;
		term_chars[VKILL]    = orig_termio.sg_kill;
#ifdef TIOCGLTC
		ioctl(0, TIOCGLTC, &s_ltchars);
		term_chars[VWERASE]  = s_ltchars.t_werasc;
		term_chars[VREPRINT] = s_ltchars.t_rprntc;
		term_chars[VSUSP]    = s_ltchars.t_suspc;

		/* disable suspending process on ^Z */
		s_ltchars.t_suspc = 0;
		ioctl(0, TIOCSLTC, &s_ltchars);
#endif /* TIOCGLTC */
#else  /* SGTTY */
#ifdef IUCLC
		rl_termio.c_iflag &= ~(BRKINT|PARMRK|INPCK|IUCLC|IXON|IXOFF);
#else
		rl_termio.c_iflag &= ~(BRKINT|PARMRK|INPCK|IXON|IXOFF);
#endif
		rl_termio.c_iflag |=  (IGNBRK|IGNPAR);

		/* rl_termio.c_oflag &= ~(ONOCR); Costas Sphocleous Irvine,CA */

		rl_termio.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL|NOFLSH);
#ifdef OS2
 /* for emx: remove default terminal processing */
                rl_termio.c_lflag &= ~(IDEFAULT);
#endif /* OS2 */
		rl_termio.c_lflag |=  (ISIG);
		rl_termio.c_cc[VMIN] = 1;
		rl_termio.c_cc[VTIME] = 0;

#ifndef VWERASE
#define VWERASE 3
#endif
		term_chars[VERASE]   = orig_termio.c_cc[VERASE];
		term_chars[VEOF]     = orig_termio.c_cc[VEOF];
		term_chars[VKILL]    = orig_termio.c_cc[VKILL];
#ifdef TERMIOS
		term_chars[VWERASE]  = orig_termio.c_cc[VWERASE];
#ifdef VREPRINT
		term_chars[VREPRINT] = orig_termio.c_cc[VREPRINT];
#else
#ifdef VRPRNT
		term_chars[VRPRNT] = orig_termio.c_cc[VRPRNT];
#endif
#endif
		term_chars[VSUSP]    = orig_termio.c_cc[VSUSP];

		/* disable suspending process on ^Z */
		rl_termio.c_cc[VSUSP] = 0;
#endif /* TERMIOS */
#endif /* SGTTY */

		/*
		 * Set the new terminal modes.
		 */
#ifdef SGTTY
		ioctl(0, TIOCSLTC, &s_ltchars);
#else
#ifdef TERMIOS
#ifdef TCSETSW
		ioctl(0, TCSETSW, &rl_termio);
#else
		tcsetattr(0, TCSADRAIN, &rl_termio);
#endif /* TCSETSW */
#else
		ioctl(0, TCSETAW, &rl_termio);
#endif /* TERMIOS */
#endif /* SGTTY */
		term_set = 1;
	}
#endif /* !MSDOS && !ATARI && !defined(_Windows) */
}

void
reset_termio()
{
#if !defined(MSDOS) && !defined(ATARI) && !defined(_Windows) && !defined(DOS386)
/* reset saved terminal modes */
	if(term_set == 1) {
#ifdef SGTTY
		ioctl(0, TIOCSETN, &orig_termio);
#ifdef TIOCGLTC
		/* enable suspending process on ^Z */
		s_ltchars.t_suspc = term_chars[VSUSP];
		ioctl(0, TIOCSLTC, &s_ltchars);
#endif /* TIOCGLTC */
#else  /* SGTTY */
#ifdef TERMIOS
#ifdef TCSETSW
		ioctl(0, TCSETSW, &orig_termio);
#else
		tcsetattr(0, TCSADRAIN, &orig_termio);
#endif /* TCSETSW */
#else
		ioctl(0, TCSETAW, &orig_termio);
#endif /* TERMIOS */
#endif /* SGTTY */
		term_set = 0;
	}
#endif /* !MSDOS && !ATARI && !_Windows */
}
#endif /* READLINE */
