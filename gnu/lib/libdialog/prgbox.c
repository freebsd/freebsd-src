/*
 *  prgbox.c -- implements the message box and info box
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
 *
 * $FreeBSD$
 */


#include <sys/types.h>

#include <dialog.h>
#include <errno.h>
#include <sys/wait.h>
#include "dialog.priv.h"

/*
 * Display a message box. Program will pause and display an "OK" button
 * if the parameter 'pause' is non-zero.
 */
int dialog_prgbox(unsigned char *title, const unsigned char *line, int height, int width, int pause, int use_shell)
{
  int i, x, y, key = 0;
  WINDOW *dialog;
  FILE *f;
  const unsigned char *name;
  unsigned char *s, buf[MAX_LEN];
  int status;

  if (height < 0 || width < 0) {
    endwin();
    fprintf(stderr, "\nAutosizing is impossible in dialog_prgbox().\n");
    exit(-1);
  }
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

  if (!use_shell) {
    char cmdline[MAX_LEN];
    char *av[51], **ap = av, *val, *p;

    strcpy(cmdline, line);
    p = cmdline;
    while ((val = strsep(&p," \t")) != NULL) {
      if (*val != '\0')
	*ap++ = val;
    }
    *ap = NULL;
    f = raw_popen(name = av[0], av, "r");
  } else
    f = raw_popen(name = line, NULL, "r");

  status = -1;
  if (f == NULL) {
  err:
      sprintf(buf, "%s: %s\n", name, strerror(errno));
  prr:
      print_autowrap(dialog, buf, height-(pause?3:1), width-2, width, 1, 2, FALSE, TRUE);
      wrefresh(dialog);
  } else {
    while (fgets(buf, sizeof(buf), f) != NULL) {
      i = strlen(buf);
      if (buf[i-1] == '\n')
	buf[i-1] = '\0';
      s = buf;
      while ((s = strchr(s, '\t')) != NULL)
	*s++ = ' ';
      print_autowrap(dialog, buf, height-(pause?3:1), width-2, width, 1, 2, FALSE, TRUE);
      print_autowrap(dialog, "\n", height-(pause?3:1), width-2, width, 1, 2, FALSE, FALSE);
      wrefresh(dialog);
    }
    if ((status = raw_pclose(f)) == -1)
      goto err;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
      sprintf(buf, "%s: program not found\n", name);
      goto prr;
    }
  }

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
  return (status);
}
/* End of dialog_msgbox() */
