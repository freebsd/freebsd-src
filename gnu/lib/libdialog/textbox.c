/*
 *  textbox.c -- implements the text box
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


static void back_lines(int n);
static void print_page(WINDOW *win, int height, int width);
static void print_line(WINDOW *win, int row, int width);
static unsigned char *get_line(void);
static int get_search_term(WINDOW *win, unsigned char *search_term, int height, int width);
static void print_position(WINDOW *win, int height, int width);


static int hscroll = 0, fd, file_size, bytes_read, begin_reached = 1,
           end_reached = 0, page_length;
static unsigned char *buf, *page;


/*
 * Display text from a file in a dialog box.
 */
int dialog_textbox(unsigned char *title, unsigned char *file, int height, int width)
{
  int i, x, y, cur_x, cur_y, fpos, key = 0, dir, temp, temp1;
#ifdef HAVE_NCURSES
  int passed_end;
#endif
  unsigned char search_term[MAX_LEN+1], *tempptr, *found;
  WINDOW *dialog, *text;

  if (height < 0 || width < 0) {
    fprintf(stderr, "\nAutosizing is impossible in dialog_textbox().\n");
    return(-1);
  }

  search_term[0] = '\0';    /* no search term entered yet */

  /* Open input file for reading */
  if ((fd = open(file, O_RDONLY)) == -1) {
    fprintf(stderr, "\nCan't open input file <%s>in dialog_textbox().\n", file);
    return(-1);
  }
  /* Get file size. Actually, 'file_size' is the real file size - 1,
     since it's only the last byte offset from the beginning */
  if ((file_size = lseek(fd, 0, SEEK_END)) == -1) {
    fprintf(stderr, "\nError getting file size in dialog_textbox().\n");
    return(-1);
  }
  /* Restore file pointer to beginning of file after getting file size */
  if (lseek(fd, 0, SEEK_SET) == -1) {
    fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
    return(-1);
  }
  /* Allocate space for read buffer */
  if ((buf = malloc(BUF_SIZE+1)) == NULL) {
    endwin();
    fprintf(stderr, "\nCan't allocate memory in dialog_textbox().\n");
    exit(-1);
  }
  if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
    fprintf(stderr, "\nError reading file in dialog_textbox().\n");
    return(-1);
  }
  buf[bytes_read] = '\0';    /* mark end of valid data */
  page = buf;    /* page is pointer to start of page to be displayed */

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

  /* Create window for text region, used for scrolling text */
/*  text = newwin(height-4, width-2, y+1, x+1); */
  text = subwin(dialog, height-4, width-2, y+1, x+1);
  if (text == NULL) {
    endwin();
    fprintf(stderr, "\nsubwin(dialog,%d,%d,%d,%d) failed, maybe wrong dims\n", height-4,width-2,y+1,x+1);
    exit(1);
  }
  keypad(text, TRUE);

  draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);

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

  if (title != NULL) {
    wattrset(dialog, title_attr);
    wmove(dialog, 0, (width - strlen(title))/2 - 1);
    waddch(dialog, ' ');
    waddstr(dialog, title);
    waddch(dialog, ' ');
  }
  display_helpline(dialog, height-1, width);

  print_button(dialog, "  OK  ", height-2, width/2-6, TRUE);
  wnoutrefresh(dialog);
  getyx(dialog, cur_y, cur_x);    /* Save cursor position */

  /* Print first page of text */
  attr_clear(text, height-4, width-2, dialog_attr);
  print_page(text, height-4, width-2);
  print_position(dialog, height, width);
  wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
  wrefresh(dialog);

  while ((key != ESC) && (key != '\n') && (key != '\r')) {
    key = wgetch(dialog);
    switch (key) {
      case 'E':    /* Exit */
      case 'e':
        delwin(dialog);
        free(buf);
        close(fd);
        return 0;
      case 'g':    /* First page */
      case KEY_HOME:
        if (!begin_reached) {
          begin_reached = 1;
          /* First page not in buffer? */
          if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
            endwin();
            fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
            exit(-1);
          }
          if (fpos > bytes_read) {    /* Yes, we have to read it in */
            if (lseek(fd, 0, SEEK_SET) == -1) {
              endwin();
              fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
              exit(-1);
            }
            if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
              endwin();
              fprintf(stderr, "\nError reading file in dialog_textbox().\n");
              exit(-1);
            }
            buf[bytes_read] = '\0';
          }
          page = buf;
          print_page(text, height-4, width-2);
          print_position(dialog, height, width);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case 'G':    /* Last page */
#ifdef HAVE_NCURSES
      case KEY_END:
#endif
        end_reached = 1;
        /* Last page not in buffer? */
        if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
          endwin();
          fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
          exit(-1);
        }
        if (fpos < file_size) {    /* Yes, we have to read it in */
          if (lseek(fd, -BUF_SIZE, SEEK_END) == -1) {
            endwin();
            fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
            exit(-1);
          }
          if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
            endwin();
            fprintf(stderr, "\nError reading file in dialog_textbox().\n");
            exit(-1);
          }
          buf[bytes_read] = '\0';
        }
        page = buf + bytes_read;
        back_lines(height-4);
        print_page(text, height-4, width-2);
        print_position(dialog, height, width);
        wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
        wrefresh(dialog);
        break;
      case 'K':    /* Previous line */
      case 'k':
      case KEY_UP:
        if (!begin_reached) {
          back_lines(page_length+1);
#ifdef HAVE_NCURSES
          /* We don't call print_page() here but use scrolling to ensure
             faster screen update. However, 'end_reached' and 'page_length'
             should still be updated, and 'page' should point to start of
             next page. This is done by calling get_line() in the following
             'for' loop. */
          scrollok(text, TRUE);
          wscrl(text, -1);    /* Scroll text region down one line */
          scrollok(text, FALSE);
          page_length = 0;
          passed_end = 0;
          for (i = 0; i < height-4; i++) {
            if (!i) {
              print_line(text, 0, width-2);    /* print first line of page */
              wnoutrefresh(text);
            }
            else
              get_line();    /* Called to update 'end_reached' and 'page' */
            if (!passed_end)
              page_length++;
            if (end_reached && !passed_end)
              passed_end = 1;
          }
#else
          print_page(text, height-4, width-2);
#endif
          print_position(dialog, height, width);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case 'B':    /* Previous page */
      case 'b':
      case KEY_PPAGE:
        if (!begin_reached) {
          back_lines(page_length + height-4);
          print_page(text, height-4, width-2);
          print_position(dialog, height, width);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case 'J':    /* Next line */
      case 'j':
      case KEY_DOWN:
        if (!end_reached) {
          begin_reached = 0;
          scrollok(text, TRUE);
          scroll(text);    /* Scroll text region up one line */
          scrollok(text, FALSE);
          print_line(text, height-5, width-2);
#ifndef HAVE_NCURSES
          wmove(text, height-5, 0);
          waddch(text, ' ');
          wmove(text, height-5, width-3);
          waddch(text, ' ');
#endif
          wnoutrefresh(text);
          print_position(dialog, height, width);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case ' ':    /* Next page */
      case KEY_NPAGE:
        if (!end_reached) {
          begin_reached = 0;
          print_page(text, height-4, width-2);
          print_position(dialog, height, width);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case '0':    /* Beginning of line */
      case 'H':    /* Scroll left */
      case 'h':
      case KEY_LEFT:
        if (hscroll > 0) {
          if (key == '0')
            hscroll = 0;
          else
            hscroll--;
          /* Reprint current page to scroll horizontally */
          back_lines(page_length);
          print_page(text, height-4, width-2);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case 'L':    /* Scroll right */
      case 'l':
      case KEY_RIGHT:
        if (hscroll < MAX_LEN) {
          hscroll++;
          /* Reprint current page to scroll horizontally */
          back_lines(page_length);
          print_page(text, height-4, width-2);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        break;
      case '/':    /* Forward search */
      case 'n':    /* Repeat forward search */
      case '?':    /* Backward search */
      case 'N':    /* Repeat backward search */
        /* set search direction */
        dir = (key == '/' || key == 'n') ? 1 : 0;
        if (dir ? !end_reached : !begin_reached) {
          if (key == 'n' || key == 'N') {
            if (search_term[0] == '\0') {    /* No search term yet */
              fprintf(stderr, "\a");    /* beep */
              break;
            }
	  }
          else    /* Get search term from user */
            if (get_search_term(text, search_term, height-4, width-2) == -1) {
              /* ESC pressed in get_search_term(). Reprint page to clear box */
              wattrset(text, dialog_attr);
              back_lines(page_length);
              print_page(text, height-4, width-2);
              wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
              wrefresh(dialog);
              break;
            }
          /* Save variables for restoring in case search term can't be found */
          tempptr = page;
          temp = begin_reached;
          temp1 = end_reached;
          if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
            endwin();
            fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
            exit(-1);
          }
          fpos -= bytes_read;
          /* update 'page' to point to next (previous) line before
             forward (backward) searching */
          back_lines(dir ? page_length-1 : page_length+1);
          found = NULL;
          if (dir)    /* Forward search */
            while((found = strstr(get_line(), search_term)) == NULL) {
              if (end_reached)
                break;
	    }
          else    /* Backward search */
            while((found = strstr(get_line(), search_term)) == NULL) {
              if (begin_reached)
                break;
              back_lines(2);
            }
          if (found == NULL) {    /* not found */
            fprintf(stderr, "\a");    /* beep */
            /* Restore program state to that before searching */
            if (lseek(fd, fpos, SEEK_SET) == -1) {
              endwin();
              fprintf(stderr, "\nError moving file pointer in dialog_textbox().\n");
              exit(-1);
            }
            if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
              endwin();
              fprintf(stderr, "\nError reading file in dialog_textbox().\n");
              exit(-1);
            }
            buf[bytes_read] = '\0';
            page = tempptr;
            begin_reached = temp;
            end_reached = temp1;
            /* move 'page' to point to start of current page in order to
               re-print current page. Note that 'page' always points to
               start of next page, so this is necessary */
            back_lines(page_length);
          }
          else    /* Search term found */
            back_lines(1);
          /* Reprint page */
          wattrset(text, dialog_attr);
          print_page(text, height-4, width-2);
          if (found != NULL)
            print_position(dialog, height, width);
          wmove(dialog, cur_y, cur_x);    /* Restore cursor position */
          wrefresh(dialog);
        }
        else    /* no need to find */
          fprintf(stderr, "\a");    /* beep */
        break;
      case ESC:
        break;
    case KEY_F(1):
	display_helpfile();
	break;
    }
  }

  delwin(dialog);
  free(buf);
  close(fd);
  return -1;    /* ESC pressed */
}
/* End of dialog_textbox() */


/*
 * Go back 'n' lines in text file. Called by dialog_textbox().
 * 'page' will be updated to point to the desired line in 'buf'.
 */
static void back_lines(int n)
{
  int i, fpos;

  begin_reached = 0;
  /* We have to distinguish between end_reached and !end_reached since at end
     of file, the line is not ended by a '\n'. The code inside 'if' basically
     does a '--page' to move one character backward so as to skip '\n' of the
     previous line */
  if (!end_reached) {
    /* Either beginning of buffer or beginning of file reached? */
    if (page == buf) {
      if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
        endwin();
        fprintf(stderr, "\nError moving file pointer in back_lines().\n");
        exit(-1);
      }
      if (fpos > bytes_read) {    /* Not beginning of file yet */
        /* We've reached beginning of buffer, but not beginning of file yet,
           so read previous part of file into buffer. Note that we only
           move backward for BUF_SIZE/2 bytes, but not BUF_SIZE bytes to
           avoid re-reading again in print_page() later */
        /* Really possible to move backward BUF_SIZE/2 bytes? */
        if (fpos < BUF_SIZE/2 + bytes_read) {
          /* No, move less then */
          if (lseek(fd, 0, SEEK_SET) == -1) {
            endwin();
            fprintf(stderr, "\nError moving file pointer in back_lines().\n");
            exit(-1);
          }
          page = buf + fpos - bytes_read;
        }
        else {    /* Move backward BUF_SIZE/2 bytes */
          if (lseek(fd, -(BUF_SIZE/2 + bytes_read), SEEK_CUR) == -1) {
            endwin();
            fprintf(stderr, "\nError moving file pointer in back_lines().\n");
            exit(-1);
          }
          page = buf + BUF_SIZE/2;
        }
        if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
          endwin();
          fprintf(stderr, "\nError reading file in back_lines().\n");
          exit(-1);
        }
        buf[bytes_read] = '\0';
      }
      else {    /* Beginning of file reached */
        begin_reached = 1;
        return;
      }
    }
    if (*(--page) != '\n') {    /* '--page' here */
      /* Something's wrong... */
      endwin();
      fprintf(stderr, "\nInternal error in back_lines().\n");
      exit(-1);
    }
  }

  /* Go back 'n' lines */
  for (i = 0; i < n; i++)
    do {
      if (page == buf) {
        if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
          endwin();
          fprintf(stderr, "\nError moving file pointer in back_lines().\n");
          exit(-1);
        }
        if (fpos > bytes_read) {
          /* Really possible to move backward BUF_SIZE/2 bytes? */
          if (fpos < BUF_SIZE/2 + bytes_read) {
            /* No, move less then */
            if (lseek(fd, 0, SEEK_SET) == -1) {
              endwin();
              fprintf(stderr, "\nError moving file pointer in back_lines().\n");
              exit(-1);
            }
            page = buf + fpos - bytes_read;
          }
          else {    /* Move backward BUF_SIZE/2 bytes */
            if (lseek(fd, -(BUF_SIZE/2 + bytes_read), SEEK_CUR) == -1) {
              endwin();
              fprintf(stderr, "\nError moving file pointer in back_lines().\n");
              exit(-1);
            }
            page = buf + BUF_SIZE/2;
          }
          if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
            endwin();
            fprintf(stderr, "\nError reading file in back_lines().\n");
            exit(-1);
          }
          buf[bytes_read] = '\0';
        }
        else {    /* Beginning of file reached */
          begin_reached = 1;
          return;
        }
      }
    } while (*(--page) != '\n');
  page++;
}
/* End of back_lines() */


/*
 * Print a new page of text. Called by dialog_textbox().
 */
static void print_page(WINDOW *win, int height, int width)
{
  int i, passed_end = 0;

  page_length = 0;
  for (i = 0; i < height; i++) {
    print_line(win, i, width);
    if (!passed_end)
      page_length++;
    if (end_reached && !passed_end)
      passed_end = 1;
  }
  wnoutrefresh(win);
}
/* End of print_page() */


/*
 * Print a new line of text. Called by dialog_textbox() and print_page().
 */
static void print_line(WINDOW *win, int row, int width)
{
  int i, y, x;
  unsigned char *line;

  line = get_line();
  line += MIN(strlen(line),hscroll);    /* Scroll horizontally */
  wmove(win, row, 0);    /* move cursor to correct line */
  waddch(win,' ');
#ifdef HAVE_NCURSES
  waddnstr(win, line, MIN(strlen(line),width-2));
#else
  line[MIN(strlen(line),width-2)] = '\0';
  waddstr(win, line);
#endif

  getyx(win, y, x);
  /* Clear 'residue' of previous line */
  for (i = 0; i < width-x; i++)
    waddch(win, ' ');
}
/* End of print_line() */


/*
 * Return current line of text. Called by dialog_textbox() and print_line().
 * 'page' should point to start of current line before calling, and will be
 * updated to point to start of next line.
 */
static unsigned char *get_line(void)
{
  int i = 0, fpos;
  static unsigned char line[MAX_LEN+1];

  end_reached = 0;
  while (*page != '\n') {
    if (*page == '\0') {    /* Either end of file or end of buffer reached */
      if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
        endwin();
        fprintf(stderr, "\nError moving file pointer in get_line().\n");
        exit(-1);
      }
      if (fpos < file_size) {    /* Not end of file yet */
        /* We've reached end of buffer, but not end of file yet, so read next
           part of file into buffer */
        if ((bytes_read = read(fd, buf, BUF_SIZE)) == -1) {
          endwin();
          fprintf(stderr, "\nError reading file in get_line().\n");
          exit(-1);
        }
        buf[bytes_read] = '\0';
        page = buf;
      }
      else {
        if (!end_reached)
          end_reached = 1;
        break;
      }
    }
    else
      if (i < MAX_LEN)
        line[i++] = *(page++);
      else {
        if (i == MAX_LEN)  /* Truncate lines longer than MAX_LEN characters */
          line[i++] = '\0';
        page++;
      }
  }
  if (i <= MAX_LEN)
    line[i] = '\0';
  if (!end_reached)
    page++;    /* move pass '\n' */

  return line;
}
/* End of get_line() */


/*
 * Display a dialog box and get the search term from user
 */
static int get_search_term(WINDOW *win, unsigned char *search_term, int height, int width)
{
  int x, y, key = 0, first,
      box_height = 3, box_width = 30;

  x = (width - box_width)/2;
  y = (height - box_height)/2;
#ifdef HAVE_NCURSES
  if (use_shadow)
    draw_shadow(win, y, x, box_height, box_width);
#endif
  draw_box(win, y, x, box_height, box_width, dialog_attr, searchbox_border_attr);
  wattrset(win, searchbox_title_attr);
  wmove(win, y, x+box_width/2-4);
  waddstr(win, " Search ");
  wattrset(win, dialog_attr);

  search_term[0] = '\0';

  first = 1;
  while (key != ESC) {
    key = line_edit(win, y+1, x+1, -1, box_width-2, searchbox_attr, first, search_term, 0);
    first = 0;
    switch (key) {
      case '\n':
        if (search_term[0] != '\0')
          return 0;
        break;
      case ESC:
	break;
    }
  }

  return -1;    /* ESC pressed */
}
/* End of get_search_term() */


/*
 * Print current position
 */
static void print_position(WINDOW *win, int height, int width)
{
  int fpos, percent;

  if ((fpos = lseek(fd, 0, SEEK_CUR)) == -1) {
    endwin();
    fprintf(stderr, "\nError moving file pointer in print_position().\n");
    exit(-1);
  }
  wattrset(win, position_indicator_attr);
  percent = !file_size ? 100 : ((fpos-bytes_read+page-buf)*100)/file_size;
  wmove(win, height-3, width-9);
  wprintw(win, "(%3d%%)", percent);
}
/* End of print_position() */
