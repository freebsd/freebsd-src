/*
 *  msgbox.c -- implements the message box and info box
 *
 *  AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <dialog.h>
#include "dialog.priv.h"


/* local prototypes */
static int 	getnlines(unsigned char *buf);
static void	print_page(WINDOW *win, int height, int width, unsigned char *buf, int startline, int hscroll);
static void 	print_perc(WINDOW *win, int y, int x, float p);


/*
 * Display a message box. Program will pause and display an "OK" button
 * if the parameter 'pause' is non-zero.
 */
int dialog_msgbox(unsigned char *title, unsigned char *prompt, int height, int width, int pause)
{
  int i, j, x, y, key = 0;
  WINDOW *dialog;

  if (height < 0)
	height = strheight(prompt)+2+2*(!!pause);
  if (width < 0) {
	i = strwidth(prompt);
	j = ((title != NULL) ? strwidth(title) : 0);
	width = MAX(i,j)+4;
  }
  if (pause)
	width = MAX(width,10);

  if (width > COLS)
	width = COLS;
  if (height > LINES)
	height = LINES;
  /* center dialog box on screen */
  x = DialogX ? DialogX : (COLS - width)/2;
  y = DialogY ? DialogY : (LINES - height)/2;

#ifdef HAVE_NCURSES
  if (use_shadow)
    draw_shadow(stdscr, y, x, height, width);
#endif
  dialog = newwin(height, width, y, x);
  if (dialog == NULL) {
    endwin();
    fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n", height,width,y,x);
    exit(1);
  }
  keypad(dialog, TRUE);

  draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);

  if (title != NULL) {
    wattrset(dialog, title_attr);
    wmove(dialog, 0, (width - strlen(title))/2 - 1);
    waddch(dialog, ' ');
    waddstr(dialog, title);
    waddch(dialog, ' ');
  }
  wattrset(dialog, dialog_attr);
  wmove(dialog, 1, 2);
  print_autowrap(dialog, prompt, height-1, width-2, width, 1, 2, TRUE, FALSE);

  if (pause) {
    wattrset(dialog, border_attr);
    wmove(dialog, height-3, 0);
    waddch(dialog, ACS_LTEE);
    for (i = 0; i < width-2; i++)
      waddch(dialog, ACS_HLINE);
    wattrset(dialog, dialog_attr);
    waddch(dialog, ACS_RTEE);
    wmove(dialog, height-2, 1);
    for (i = 0; i < width-2; i++)
    waddch(dialog, ' ');
    display_helpline(dialog, height-1, width);
    print_button(dialog, "  OK  ", height-2, width/2-6, TRUE);
    wrefresh(dialog);
    while (key != ESC && key != '\n' && key != ' ' && key != '\r')
      key = wgetch(dialog);
    if (key == '\r')
      key = '\n';
  }
  else {
    key = '\n';
    wrefresh(dialog);
  }

  delwin(dialog);
  return (key == ESC ? -1 : 0);
}
/* End of dialog_msgbox() */

int
dialog_mesgbox(unsigned char *title, unsigned char *prompt, int height, int width)
/*
 * Desc: basically the same as dialog_msgbox, but ... can use PGUP, PGDN and
 *	 arrowkeys to move around the text and pause is always enabled
 */
{
    int 	i, j, x, y, key=0;
    int		theight, startline, hscroll, max_lines;
    WINDOW 	*dialog;

    if (height < 0)
	height = strheight(prompt)+2+2;
    if (width < 0) {
	i = strwidth(prompt);
	j = ((title != NULL) ? strwidth(title) : 0);
	width = MAX(i,j)+4;
    }
    width = MAX(width,10);

    if (width > COLS)
	width = COLS;
    if (height > LINES)
	height = LINES;
    /* center dialog box on screen */
    x = (COLS - width)/2;
    y = (LINES - height)/2;

#ifdef HAVE_NCURSES
    if (use_shadow)
	draw_shadow(stdscr, y, x, height, width);
#endif
    dialog = newwin(height, width, y, x);
    if (dialog == NULL) {
	endwin();
	fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n", height,width,y,x);
	exit(1);
    }
    keypad(dialog, TRUE);

    draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);

    if (title != NULL) {
	wattrset(dialog, title_attr);
	wmove(dialog, 0, (width - strlen(title))/2 - 1);
	waddch(dialog, ' ');
	waddstr(dialog, title);
	waddch(dialog, ' ');
    }

    wattrset(dialog, border_attr);
    wmove(dialog, height-3, 0);
    waddch(dialog, ACS_LTEE);
    for (i = 0; i < width-2; i++)
      waddch(dialog, ACS_HLINE);
    wattrset(dialog, dialog_attr);
    waddch(dialog, ACS_RTEE);
    wmove(dialog, height-2, 1);
    for (i = 0; i < width-2; i++)
    waddch(dialog, ' ');
    display_helpline(dialog, height-1, width);
    print_button(dialog, "  OK  ", height-2, width/2-6, TRUE);
    wattrset(dialog, dialog_attr);

    theight = height - 4;
    startline = 0;
    hscroll = 0;
    max_lines = getnlines(prompt);
    print_page(dialog, theight, width, prompt, startline, hscroll);
    print_perc(dialog, height-3, width-9, (float) (startline+theight)/max_lines);
    wmove(dialog, height-2, width/2-6);
    wrefresh(dialog);
    while ((key != ESC) && (key != '\n') && (key != '\r')) {
	key = wgetch(dialog);
	switch(key) {
	case KEY_HOME:
	    startline=0;
	    hscroll=0;
	    break;
	case KEY_END:
	    startline = max_lines - theight;
	    if (startline < 0) startline = 0;
	    break;
	case '\020':	/* ^P */
	case KEY_UP:
	    if (startline > 0) startline--;
	    break;
	case '\016':	/* ^N */
	case KEY_DOWN:
	    if (startline < max_lines - theight) startline++;
	    break;
	case KEY_RIGHT:
	    hscroll+=5;
	    break;
	case KEY_LEFT:
	    if (hscroll > 0) hscroll-=5;
	    if (hscroll < 0) hscroll =0;
	    break;
	case KEY_PPAGE:
	    if (startline - height > 0) {
		startline -= theight;
	    } else {
		startline = 0;
	    }
	    break;
	case KEY_NPAGE:
	    if (startline + theight < max_lines - theight) {
		startline += theight;
	    } else {
		startline = max_lines - theight;
		if (startline < 0) startline = 0;
	    }
	    break;
	case KEY_F(1):
	case '?':
	    display_helpfile();
	    break;
	}
	print_page(dialog, theight, width, prompt, startline, hscroll);
	print_perc(dialog, height-3, width-9, (float) (startline+theight)/max_lines);
	wmove(dialog, height-2, width/2-2);
	wrefresh(dialog);
    }

    delwin(dialog);
    return (key == ESC ? -1 : 0);

} /* dialog_mesgbox() */

static void
print_perc(WINDOW *win, int y, int x, float p)
/*
 * Desc: print p as a percentage at the coordinates (y,x)
 */
{
    char	ps[10];

    if (p>1.0) p=1.0;
    sprintf(ps, "(%3d%%)", (int) (p*100));
    wmove(win, y, x);
    waddstr(win, ps);

    return;
} /* print_perc() */

static int
getnlines(unsigned char *buf)
/*
 * Desc: count the # of lines in <buf>
 */
{
    int i = 0;

    if (*buf)
	i++;
    while (*buf) {
	if (*buf == '\n' || *buf == '\r')
	    i++;
	buf++;
    }
    return(i);
} /* getlines() */


unsigned char *
getline(unsigned char *buf, int n)
/*
 * Desc: return a pointer to the n'th line in <buf> or NULL if its
 *	 not there
 */
{
    int i;

    if (n<0) {
	return(NULL);
    }

    i=0;
    while (*buf && i<n) {
	if (*buf == '\n' || *buf == '\r') {
	    i++;
	}
	buf++;
    }
    if (i<n) {
	return(NULL);
    } else {
	return(buf);
    }
} /* getline() */

static void
print_page(WINDOW *win, int height, int width, unsigned char *buf, int startline, int hscroll)
/*
 * Desc: Print a page of text in the current window, starting at line <startline>
 *	 with a <horizontal> scroll of hscroll from buffer <buf>
 */
{
    int i, j;
    unsigned char *b;

    b = getline(buf, startline);
    for (i=0; i<height; i++) {
	/* clear line */
	wmove(win, 1+i, 1);
	for (j=0; j<width-2; j++) waddnstr(win, " ", 1);
	wmove(win, 1+i, 1);
	j = 0;
	/* scroll to the right */
	while (*b && (*b != '\n') && (*b != '\r') && (j<hscroll)) {
	    b++;
	    j++;
	}
	/* print new line */
	j = 0;
	while (*b && (*b != '\n') && (*b != '\r') && (j<width-2)) {
	    waddnstr(win, b, 1);
	    if (*b != '\t') {	/* check for tabs */
		j++;
	    } else {
		j = ((int) (j+1)/8 + 1) * 8 - 1;
	    }
	    b++;
	}
	while (*b && (*b != '\n') && (*b != '\r')) b++;
	if (*b) b++;	/* skip over '\n', if it exists */
    }
} /* print_page() */




