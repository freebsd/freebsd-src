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
 * $FreeBSD$
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
 *
 *  09/28/98 - Patches by Anatoly A. Orehovsky - tolik@mpeks.tomsk.su
 *             Added ftree and tree options
 *
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <dialog.h>

void Usage(char *name);

int main(int argc, char *argv[])
{
  int offset = 0, clear_screen = 0, end_common_opts = 0, retval;
  unsigned char *title = NULL;
  unsigned char result[MAX_LEN];
  char *hline = NULL, *hfile = NULL;

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
    dialog_create_rc(argv[2]);
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
    else if (!strcmp(argv[offset+1], "--hline")) {
      if (argc-offset < 3 || hline != NULL) {    /* No two "--hline" please! */
        Usage(argv[0]);
        exit(-1);
      }
      else {
	hline = argv[offset+2];
	use_helpline(hline);
        offset += 2;
      }
    }
    else if (!strcmp(argv[offset+1], "--hfile")) {
      if (argc-offset < 3 || hfile != NULL) {    /* No two "--hfile" please! */
        Usage(argv[0]);
        exit(-1);
      }
      else {
	hfile = argv[offset+2];
	use_helpfile(hfile);
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
	dialog_update();    /* init_dialog() will clear the screen for us */
	end_dialog();
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

    dialog_update();
    if (clear_screen)    /* clear screen before exit */
      dialog_clear();
    end_dialog();
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

    dialog_update();
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--prgbox")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_prgbox(title, argv[offset+2], atoi(argv[offset+3]),
			   atoi(argv[offset+4]), TRUE, TRUE);

    dialog_update();
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
    return WEXITSTATUS(retval);
  }
  else if (!strcmp(argv[offset+1], "--infobox")) {
    if (argc-offset != 5) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_msgbox(title, argv[offset+2], atoi(argv[offset+3]),
                           atoi(argv[offset+4]), 0);

    dialog_update();
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
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

    dialog_update();
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
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
			 (argc-offset-6)/2, argv+offset + 6, result,
			 NULL, NULL);
    dialog_update();
    if (retval == 0)
	fputs(result, stderr);
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
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
			      (argc-offset-6)/3, argv+offset + 6, result);

    dialog_update();
    if (retval == 0) {
      unsigned char *s, *h; int first;

      h = result;
      first = 1;
      while ((s = strchr(h, '\n')) != NULL) {
	*s++ = '\0';
	if (!first)
	  fputc(' ', stderr);
	else
	  first = 0;
	fprintf(stderr, "\"%s\"", h);
	h = s;
      }
    }
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
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
			      (argc-offset-6)/3, argv+offset + 6, result);

    dialog_update();
    if (retval == 0)
	fputs(result, stderr);
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
    return retval;
  }
  else if (!strcmp(argv[offset+1], "--inputbox")) {
    if (argc-offset != 5 && argc-offset != 6) {
      Usage(argv[0]);
      exit(-1);
    }
    if (argc-offset == 6)
      strcpy(result, argv[offset+5]);
    else
      *result = '\0';
    init_dialog();
    retval = dialog_inputbox(title, argv[offset+2], atoi(argv[offset+3]),
			     atoi(argv[offset+4]), result);

    dialog_update();
    if (retval == 0)
	fputs(result, stderr);
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
    return retval;
  }
/* ftree and tree options */
  else if (!strcmp(argv[offset+1], "--ftree")) {
  	unsigned char *tresult;
    if (argc-offset != 8) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_ftree(argv[offset+2], *argv[offset+3],
    	title, argv[offset+4], atoi(argv[offset+5]), atoi(argv[offset+6]),
                            atoi(argv[offset+7]), &tresult);

    dialog_update();
    if (!retval)
    {
    	fputs(tresult, stderr);
    	free(tresult);
    }
    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
    return retval;
  }  
  else if (!strcmp(argv[offset+1], "--tree")) {
  	unsigned char *tresult;
    if (argc-offset < 8) {
      Usage(argv[0]);
      exit(-1);
    }
    init_dialog();
    retval = dialog_tree((unsigned char **)argv+offset+7, argc-offset-7,
	*argv[offset+2], title, argv[offset+3], atoi(argv[offset+4]),
	atoi(argv[offset+5]), atoi(argv[offset+6]), &tresult);

    dialog_update();
    if (!retval)
    	fputs(tresult, stderr);

    if (clear_screen)   /* clear screen before exit */
      dialog_clear();
    end_dialog();
    return retval;
  }    

  Usage(argv[0]);
  exit(-1);
}
/* End of main() */


/*
 * Print program usage
 */
void Usage(char *name)
{
  fprintf(stderr, "\
\ndialog version 0.3, by Savio Lam (lam836@cs.cuhk.hk).\
\n  patched to version %s by Stuart Herbert (S.Herbert@shef.ac.uk)\
\n  Changes Copyright (C) 1995 by Andrey A. Chernov, Moscow, Russia\
\n  patched by Anatoly A. Orehovsky (tolik@mpeks.tomsk.su)\
\n\
\n* Display dialog boxes from shell scripts *\
\n\
\nUsage: %s --clear\
\n       %s --create-rc <file>\
\n       %s [--title <title>] [--clear] [--hline <line>] [--hfile <file>]\\\
\n              <Box options>\
\n\
\nBox options:\
\n\
\n  --yesno     <text> <height> <width>\
\n  --msgbox    <text> <height> <width>\
\n  --prgbox    \"<command line>\" <height> <width>\
\n  --infobox   <text> <height> <width>\
\n  --inputbox  <text> <height> <width> [<init string>]\
\n  --textbox   <file> <height> <width>\
\n  --menu      <text> <height> <width> <menu height> <tag1> <item1>...\
\n  --checklist <text> <height> <width> <list height> <tag1> <item1> <status1>...\
\n  --radiolist <text> <height> <width> <list height> <tag1> <item1> <status1>...\
\n  --ftree     <file> <FS> <text> <height> <width> <menu height>\
\n  --tree      <FS> <text> <height> <width> <menu height> <item1>...\n", VERSION, name, name, name);
}
/* End of Usage() */
