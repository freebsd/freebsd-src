/*********************************************************************
 * [program]   overwrite() - Play with overwrite() function to       *
 *             attempt pop-up windows.                               *
 * ----------------------------------------------------------------- *
 * [written]   1-Feb-1993 by Neal Ensor (ensor@cs.utk.edu)           *
 * ----------------------------------------------------------------- *
 * [notes]     Originally written on SVR4 UNIX, then recompiled on   *
 *             Linux (Slackware 1.1.1, ncurses 1.8.1)                *
 * ----------------------------------------------------------------- *
 * [problem]   On ncurses, the overwrite() function doesn't seem to  *
 *             overwrite.  Maybe I'm missing something, but this     *
 *             program in SVR4 displays three windows, waits for a   *
 *             keypress, then removes the top window.  With ncurses, *
 *             nothing changes on the display.                       *
 *********************************************************************/

#  include <ncurses.h>         /* ncurses include lives here */

main()
{
    /****************************************************************
     * Declare three little window pointers...                      *
     ****************************************************************/
    WINDOW *win, *win1, *win2;

    /****************************************************************
     * Set up the screen...                                         *
     ****************************************************************/
    initscr();
    /* traceon(); */
    noecho();
    nonl();
    cbreak();
    refresh();

    /****************************************************************
     * Draw three overlapping windows.                              *
     ****************************************************************/
    win=newwin(6,45, 6,6);
    win1=newwin(10,20,5,5);
    win2=newwin(10,30,7,7);

    /****************************************************************
     * Box them, and print a hidden message...                      *
     ****************************************************************/
    box(win,  0, 0);
    box(win1, 0, 0);
    box(win2, 0, 0);
    mvwprintw(win1, 6,6, "Hey!");
    mvwaddch(win, 1, 1, '0');
    mvwaddch(win1, 1, 1, '1');
    mvwaddch(win2, 1, 1, '2');
    wnoutrefresh(win);
    wnoutrefresh(win1);
    wnoutrefresh(win2);
    doupdate();

    /****************************************************************
     * Await a keypress to show what we've done so far.             *
     ****************************************************************/
    getch();

    /****************************************************************
     * Now, overwrite win2 with contents of all lower windows IN    *
     * ORDER from the stdscr up...                                  *
     ****************************************************************/
    if (overwrite(stdscr, win2) == ERR)
      fprintf(stderr, "overwrite(stdscr, win2) failed!\n");

    touchwin(stdscr); wnoutrefresh(stdscr);
    touchwin(win); wnoutrefresh(win);
    touchwin(win1); wnoutrefresh(win1);
    touchwin(win2); wnoutrefresh(win2);
    doupdate();

    getch();
    if (overwrite(win, win2) == ERR)
      fprintf(stderr, "overwrite(win, win2) failed!\n");

    touchwin(stdscr); wnoutrefresh(stdscr);
    touchwin(win); wnoutrefresh(win);
    touchwin(win1); wnoutrefresh(win1);
    touchwin(win2); wnoutrefresh(win2);
    doupdate();

    getch();
    if (overwrite(win1, win2) == ERR)
      fprintf(stderr, "overwrite(win1, win2) failed!\n");

    /****************************************************************
     * Perform touches, and hidden refreshes on each window.        *
     * ------------------------------------------------------------ *
     * NOTE:  If you replace the wnoutrefresh() call with wrefresh()*
     * you can see all windows being redrawn untouched.             *
     ****************************************************************/
    touchwin(stdscr); wnoutrefresh(stdscr);
    touchwin(win); wnoutrefresh(win);
    touchwin(win1); wnoutrefresh(win1);
    touchwin(win2); wnoutrefresh(win2);
    doupdate();

    /****************************************************************
     * At this point, win2 should be "destroyed"; having all other  *
     * window contents overwritten onto it.  The doupdate() should  *
     * effectively remove it from the screen, leaving the others    *
     * untouched.  On SVR4, this happens, but not with ncurses.     *
     * I'd suspect something in overwrite() causes this, as nothing *
     * appears to be overwritten after the calls, with no errors    *
     * being reported.  This was compiled on my Linux from Slackware*
     * 1.1.1, with ncurses1.8.1 recompiled on it, using the console *
     * entry from the "new" terminfo from ncurses1.8.1.             *
     ****************************************************************/
    getch();

    /****************************************************************
     * Clean up our act and exit.                                   *
     ****************************************************************/
    endwin();
}

