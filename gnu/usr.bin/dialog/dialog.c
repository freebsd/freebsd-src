/*
 *  dialog - Display simple dialog boxes from shell scripts
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
 *
 *  HISTORY:
 *
 *  17/12/93 - Version 0.1 released.
 *
 *  19/12/93 - menu will now scroll if there are more items than can fit
 *             on the screen.
 *           - added 'checklist', a dialog box with a list of options that
 *             can be turned on or off. A list of options that are on is 
 *             returned on exit.
 *
 *  20/12/93 - Version 0.15 released.
 *
 *  29/12/93 - Incorporated patch from Patrick J. Volkerding
 *             (volkerdi@mhd1.moorhead.msus.edu) that made these changes:
 *             - increased MAX_LEN to 2048
 *             - added 'infobox', equivalent to a message box without pausing
 *             - added option '--clear' that will clear the screen
 *             - Explicit line breaking when printing prompt text can be
 *               invoked by real newline '\n' besides the string "\n"
 *           - an optional parameter '--title <string>' can be used to
 *             specify a title string for the dialog box
 *
 *  03/01/94 - added 'textbox', a dialog box for displaying text from a file.
 *           - Version 0.2 released.
 *
 *  04/01/94 - some fixes and improvements for 'textbox':
 *             - fixed a bug that will cause a segmentation violation when a
 *               line is longer than MAX_LEN characters. Lines will now be
 *               truncated if they are longer than MAX_LEN characters.
 *             - removed wrefresh() from print_line(). This will increase
 *               efficiency of print_page() which calls print_line().
 *             - display current position in the form of percentage into file.
 *           - Version 0.21 released.
 *
 *  05/01/94 - some changes for faster screen update.
 *
 *  07/01/94 - much more flexible color settings. Can use all 16 colors
 *             (8 normal, 8 highlight) of the Linux console.
 *
 *  08/01/94 - added run-time configuration using configuration file.
 *
 *  09/01/94 - some minor bug fixes and cleanups for menubox, checklist and
 *             textbox.
 *
 *  11/01/94 - added a man page.
 *
 *  13/01/94 - some changes for easier porting to other Unix systems (tested
 *             on Ultrix, SunOS and HPUX)
 *           - Version 0.3 released.
 * 
 *  08/06/94 - Patches by Stuart Herbert - S.Herbert@shef.ac.uk
 * 	       Fixed attr_clear and the textbox stuff to work with ncurses 1.8.5
 * 	       Fixed the wordwrap routine - it'll actually wrap properly now
 *	       Added a more 3D look to everything - having your own rc file could
 *	         prove 'interesting' to say the least :-)
 *             Added radiolist option
 *	     - Version 0.4 released.
 */


#define __DIALOG_MAIN__


#include "dialog.h"
#ifdef HAVE_NCURSES
#include "colors.h"
#endif


int main(int argc, unsigned char *argv[])
{
  int offset = 0, clear_screen = 0, end_common_opts = 0, retval;
  unsigned char *title = NULL;

#if defined(LOCALE)
  (void) setlocale(LC_ALL, "");
#endif

  if (argc < 2) {
    Usage(argv[0]);
    exit(-1);
  }
  else if (!strcmp(argv[1], "--create-rc")) {
#ifdef HAVE_NCURSES
    if (argc != 3) {
      Usage(argv[0]);
      exit(-1);
    }
    create_rc(argv[2]);
    return 0;
#else
    fprintf(stderr, "\nThis option is currently unsupported on your system.\n");
    return -1;
#endif
  }

  while (offset < argc-1 && !end_common_opts) {    /* Common options */
    if (!strcmp(argv[offset+1], "--title")) {
      if (argc-offset < 3 || title != NULL) {    /* No two "--title" please! */
        Usage(argv[0]);
        exit(-1);
      }
      else {
        title = argv[offset+2];
        offset += 2;
      }
    }
    else if (!strcmp(argv[offset+1], "--clear")) {
      if (clear_screen) {    /* Hey, "--clear" can't appear twice! */
        Usage(argv[0]);
        exit(-1);
      }
      else if (argc == 2) {    /* we only want to clear the screen */
        init_dialog();
        refresh();    /* init_dialog() will clear the screen for us */
        endwin();
        return 0;
      }
      else {
        clear_screen = 1;
        offset++;
      }
    }
    else    /* no more common options */
      end_common_opts = 1;
  }

  if (argc-1 == offset) {    /* no more options */
    Usage(argv[0]);
    exit(-1);
  }

  /* Box options */

  if (!strcmp(argv[offset+1], "--yesno")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_yesno(title, argv[offset+2], atoi(argv[offset+3]),
                          atoi(argv[offset+4]));

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--msgbox")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_msgbox(title, argv[offset+2], atoi(argv[offset+3]),
                           atoi(argv[offset+4]), 1);

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--infobox")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_msgbox(title, argv[offset+2], atoi(argv[offset+3]),
                           atoi(argv[offset+4]), 0);

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--textbox")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_textbox(title, argv[offset+2], atoi(argv[offset+3]),
                            atoi(argv[offset+4]));

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--menu")) {
    if (argc-offset < 8 || ((argc-offset) % 2)) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_menu(title, argv[offset+2], atoi(argv[offset+3]),
                         atoi(argv[offset+4]), atoi(argv[offset+5]),
                         (argc-offset-6)/2, argv+offset + 6);

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--checklist")) {
    if (argc-offset < 9 || ((argc-offset-6) % 3)) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_checklist(title, argv[offset+2], atoi(argv[offset+3]),
                              atoi(argv[offset+4]), atoi(argv[offset+5]),
                              (argc-offset-6)/3, argv+offset + 6);

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--radiolist")) {
    if (argc-offset < 9 || ((argc-offset-6) % 3)) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_radiolist(title, argv[offset+2], atoi(argv[offset+3]),
                              atoi(argv[offset+4]), atoi(argv[offset+5]),
                              (argc-offset-6)/3, argv+offset + 6);

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--inputbox")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_inputbox(title, argv[offset+2], atoi(argv[offset+3]),
                             atoi(argv[offset+4]));

    if (clear_screen) {    /* clear screen before exit */
      attr_clear(stdscr, LINES, COLS, screen_attr);
      refresh();
    }
    endwin();
    return retval;
  }

  Usage(argv[0]);
  exit(-1);
}
/* End of main() */


/*
 * Print program usage
 */
void Usage(unsigned char *name)
{
  fprintf(stderr, "\
\ndialog version 0.3, by Savio Lam (lam836@cs.cuhk.hk).\
\n  patched to version %s by Stuart Herbert (S.Herbert@shef.ac.uk)\
\n\
\n* Display dialog boxes from shell scripts *\
\n\
\nUsage: %s --clear\
\n       %s --create-rc <file>\
\n       %s [--title <title>] [--clear] <Box options>\
\n\
\nBox options:\
\n\
\n  --yesno     <text> <height> <width>\
\n  --msgbox    <text> <height> <width>\
\n  --infobox   <text> <height> <width>\
\n  --inputbox  <text> <height> <width>\
\n  --textbox   <file> <height> <width>\
\n  --menu      <text> <height> <width> <menu height> <tag1> <item1>...\
\n  --checklist <text> <height> <width> <list height> <tag1> <item1> <status1>...\
\n  --radiolist <text> <height> <width> <list height> <tag1> <item1> <status1>...\n", VERSION, name, name, name);
}
/* End of Usage() */


/*
 * Do some initialization for dialog
 */
void init_dialog(void)
{
#ifdef HAVE_NCURSES
  if (parse_rc() == -1)    /* Read the configuration file */
    exit(-1);
#endif

  initscr();     /* Init curses */
  keypad(stdscr, TRUE);
  cbreak();
  noecho();

#ifdef HAVE_NCURSES
  if (use_colors || use_shadow)    /* Set up colors */
    color_setup();
#endif

  /* Set screen to screen attribute */
  attr_clear(stdscr, LINES, COLS, screen_attr);
  wnoutrefresh(stdscr);
}
/* End of init_dialog() */


#ifdef HAVE_NCURSES
/*
 * Setup for color display
 */
void color_setup(void)
{
  int i;

  if (has_colors()) {    /* Terminal supports color? */
    start_color();

    /* Initialize color pairs */
    for (i = 0; i < ATTRIBUTE_COUNT; i++)
      init_pair(i+1, color_table[i][0], color_table[i][1]);

    /* Setup color attributes */
    for (i = 0; i < ATTRIBUTE_COUNT; i++)
      attributes[i] = C_ATTR(color_table[i][2], i+1);
  }
}
/* End of color_setup() */
#endif


/*
 * Set window to attribute 'attr'
 */
void attr_clear(WINDOW *win, int height, int width, chtype attr)
{
  int i, j;

  wattrset(win, attr);    /* Set window to attribute 'attr' */
  for (i = 0; i < height; i++) {
    wmove(win, i, 0);
    for (j = 0; j < width; j++)
      waddch(win, ' ');
  }
}
/* End of attr_clear() */


/*
 * Print a string of text in a window, automatically wrap around to the
 * next line if the string is too long to fit on one line. Note that the
 * string may contain "\n" to represent a newline character or the real
 * newline '\n', but in that case, auto wrap around will be disabled.
 */
void print_autowrap(WINDOW *win, unsigned char *prompt, int width, int y, int x)
{
  int first = 1, cur_x, cur_y;
  unsigned char tempstr[MAX_LEN+1], *word, *tempptr, *tempptr1;

  strcpy(tempstr, prompt);
  if ((strstr(tempstr, "\\n") != NULL) ||
      (strchr(tempstr, '\n') != NULL)) {    /* Prompt contains "\n" or '\n' */
    word = tempstr;
    cur_y = y;
    wmove(win, cur_y, x);
    while (1) {
      tempptr = strstr(word, "\\n");
      tempptr1 = strchr(word, '\n');
      if (tempptr == NULL && tempptr1 == NULL)
        break;
      else if (tempptr == NULL) {    /* No more "\n" */
        tempptr = tempptr1;
        tempptr[0] = '\0';
      }
      else if (tempptr1 == NULL) {    /* No more '\n' */
        tempptr[0] = '\0';
        tempptr++;
      }
      else {    /* Prompt contains both "\n" and '\n' */
        if (strlen(tempptr)-2 < strlen(tempptr1)-1) {
          tempptr = tempptr1;
          tempptr[0] = '\0';
        }
        else {
          tempptr[0] = '\0';
          tempptr++;
        }
      }

      waddstr(win, word);
      word = tempptr + 1;
      wmove(win, ++cur_y, x);
    }
    waddstr(win, word);
  }
  else if (strlen(tempstr) <= width-x*2) {    /* If prompt is short */
    wmove(win, y, (width - strlen(tempstr)) / 2);
    waddstr(win, tempstr);
  }
  else {
    cur_x = x;
    cur_y = y;
    /* Print prompt word by word, wrap around if necessary */
    while ((word = strtok(first ? tempstr : NULL, " ")) != NULL) {
      if (first)    /* First iteration */
        first = 0;
      if (cur_x+strlen(word) >= width) {    /* wrap around to next line */
        cur_y++;
        cur_x = x;
      }
      wmove(win, cur_y, cur_x);
      waddstr(win, word);
      getyx(win, cur_y, cur_x);
      cur_x++;
    }
  }
}
/* End of print_autowrap() */


/*
 * Print a button
 */
void print_button(WINDOW *win, unsigned char *label, int y, int x, int selected)
{
  int i, temp;

  wmove(win, y, x);
  wattrset(win, selected ? button_active_attr : button_inactive_attr);
  waddstr(win, "<");
  temp = strspn(label, " ");
  label += temp;
  wattrset(win, selected ? button_label_active_attr : button_label_inactive_attr);
  for (i = 0; i < temp; i++)
    waddch(win, ' ');
  wattrset(win, selected ? button_key_active_attr : button_key_inactive_attr);
  waddch(win, label[0]);
  wattrset(win, selected ? button_label_active_attr : button_label_inactive_attr);
  waddstr(win, label+1);
  wattrset(win, selected ? button_active_attr : button_inactive_attr);
  waddstr(win, ">");
  wmove(win, y, x+temp+1);
}
/* End of print_button() */


/*
 * Draw a rectangular box with line drawing characters
 */
void draw_box(WINDOW *win, int y, int x, int height, int width, chtype box, chtype border)
{
  int i, j;

  wattrset(win, 0);
  for (i = 0; i < height; i++) {
    wmove(win, y + i, x);
    for (j = 0; j < width; j++)
      if (!i && !j)
        waddch(win, border | ACS_ULCORNER);
      else if (i == height-1 && !j)
        waddch(win, border | ACS_LLCORNER);
      else if (!i && j == width-1)
        waddch(win, box | ACS_URCORNER);
      else if (i == height-1 && j == width-1)
        waddch(win, box | ACS_LRCORNER);
      else if (!i)
        waddch(win, border | ACS_HLINE);
      else if (i == height-1)
        waddch(win, box | ACS_HLINE);
      else if (!j)
        waddch(win, border | ACS_VLINE);
      else if (j == width-1)
        waddch(win, box | ACS_VLINE);
      else
        waddch(win, box | ' ');
  }
}
/* End of draw_box() */


#ifdef HAVE_NCURSES
/*
 * Draw shadows along the right and bottom edge to give a more 3D look
 * to the boxes
 */
void draw_shadow(WINDOW *win, int y, int x, int height, int width)
{
  int i;

  if (has_colors()) {    /* Whether terminal supports color? */
    wattrset(win, shadow_attr);
    wmove(win, y + height, x + 2);
    for (i = 0; i < width; i++)
      waddch(win, winch(win) & A_CHARTEXT);
    for (i = y + 1; i < y + height + 1; i++) {
      wmove(win, i, x + width);
      waddch(win, winch(win) & A_CHARTEXT);
      waddch(win, winch(win) & A_CHARTEXT);
    }
    wnoutrefresh(win);
  }
}
/* End of draw_shadow() */
#endif
