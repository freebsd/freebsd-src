
/*
** This module contains functions to support i/o in the TUI
*/


#include <stdio.h>
#include "defs.h"
#include "terminal.h"
#include "tui.h"
#include "tuiData.h"
#include "tuiIO.h"
#include "tuiCommand.h"
#include "tuiWin.h"

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/* The Solaris header files seem to provide no declaration for this at
   all when __STDC__ is defined.  This shouldn't conflict with
   anything.  */
extern char *tgoto ();

int insert_mode = 0;

/********************************************
**       LOCAL STATIC FORWARD DECLS        **
********************************************/
static void _updateCommandInfo PARAMS ((int));
static unsigned int _tuiHandleResizeDuringIO PARAMS ((unsigned int));


/*********************************************************************************
**                              PUBLIC FUNCTIONS                                **
*********************************************************************************/

/*
** tuiPuts_unfiltered().
**        Function to put a string to the command window
**              When running in TUI mode, this is the "hook"
**              for fputs_unfiltered(). That is, all debugger
**              output eventually makes it's way to the bottom-level
**              routine fputs_unfiltered (main.c), which (in TUI
**              mode), calls tuiPuts_unfiltered().
*/
void
#ifdef __STDC__
tuiPuts_unfiltered (
		     const char *string,
		     GDB_FILE * stream)
#else
tuiPuts_unfiltered (string, stream)
     char *string;
     GDB_FILE *stream;
#endif
{
  int len = strlen (string);
  int i, linech;

  for (i = 0; i < len; i++)
    {
      if (string[i] == '\n' || string[i] == '\r')
	m_tuiStartNewLine;
      else
	{
	  if ((cmdWin->detail.commandInfo.curch + 1) > cmdWin->generic.width)
	    m_tuiStartNewLine;

	  if (insert_mode)
	    {
	      mvwinsch (cmdWin->generic.handle,
			cmdWin->detail.commandInfo.curLine,
			cmdWin->detail.commandInfo.curch++,
			string[i]);
	      wmove (cmdWin->generic.handle,
		     cmdWin->detail.commandInfo.curLine,
		     cmdWin->detail.commandInfo.curch);
	    }
	  else
	    mvwaddch (cmdWin->generic.handle,
		      cmdWin->detail.commandInfo.curLine,
		      cmdWin->detail.commandInfo.curch++,
		      string[i]);
	}
    }
  tuiRefreshWin (&cmdWin->generic);

  return;
}				/* tuiPuts_unfiltered */

/* A cover routine for tputs().
 * tputs() is called from the readline package to put
 * out strings representing cursor positioning.
 * In TUI mode (non-XDB-style), tui_tputs() is called instead.
 *
 * The reason we need to hook tputs() is:
 * Since the output is going to curses and not to
 * a raw terminal, we need to intercept these special
 * sequences, and handle them them here.
 *
 * This function seems to be correctly handling all sequences
 * aimed at hpterm's, but there is additional work to do
 * for xterm's and dtterm's. I abandoned further work on this
 * in favor of "XDB style". In "XDB style", the command region
 * looks like terminal, not a curses window, and this routine
 * is not called. - RT
 */
void
tui_tputs (str, affcnt, putfunc)
     char *str;
     int affcnt;
     int (*putfunc) PARAMS ((int));
{
  extern char *rl_prompt;	/* the prompt string */

  /* This set of globals are defined and initialized
   * by the readline package.
   *
   * Note we're assuming tui_tputs() is being called
   * by the readline package. That's because we're recognizing
   * that a given string is being passed by
   * matching the string address against readline's
   * term_<whatever> global. To make this more general,
   * we'd have to actually recognize the termcap sequence
   * inside the string (more work than I want to do). - RT
   *
   * We don't see or need to handle every one of these here;
   * this is just the full list defined in readline/readline.c
   */
  extern char *term_backspace;
  extern char *term_clreol;
  extern char *term_clrpag;
  extern char *term_cr;
  extern char *term_dc;
  extern char *term_ei;
  extern char *term_goto;
  extern char *term_ic;
  extern char *term_im;
  extern char *term_mm;
  extern char *term_mo;
  extern char *term_up;
  extern char *term_scroll_region;
  extern char *term_memory_lock;
  extern char *term_memory_unlock;
  extern char *term_cursor_move;
  extern char *visible_bell;

  /* Sanity check - if not TUI, just call tputs() */
  if (!tui_version)
    tputs (str, affcnt, putfunc);

  /* The strings we special-case are handled first */

  if (str == term_backspace)
    {
      /* Backspace. */

      /* We see this on an emacs control-B.
     * I.e., it's like the left-arrow key (not like the backspace key).
     * The effect that readline wants when it transmits this
     * character to us is simply to back up one character
     * (but not to write a space over the old character).
     */

      _updateCommandInfo (-1);
      wmove (cmdWin->generic.handle,
	     cmdWin->detail.commandInfo.curLine,
	     cmdWin->detail.commandInfo.curch);
      wrefresh (cmdWin->generic.handle);

    }
  else if (str == term_clreol)
    {

      /* Clear to end of line. */
      wclrtoeol (cmdWin->generic.handle);
      wrefresh (cmdWin->generic.handle);

    }
  else if (str == term_cr)
    {

      /* Carriage return */
      _updateCommandInfo (-cmdWin->detail.commandInfo.curch);
      wmove (cmdWin->generic.handle,
	     cmdWin->detail.commandInfo.curLine,
	     0 /* readline will rewrite the prompt from 0 */ );
      wrefresh (cmdWin->generic.handle);

    }
  else if (str == term_goto)
    {

      /* This is actually a tgoto() specifying a character position,
     * followed by either a term_IC/term_DC which [I think] means
     * insert/delete one character at that position.
     * There are complications with this one - need to either
     * extract the position from the string, or have a backdoor
     * means of communicating it from ../readline/display.c.
     * So this one is not yet implemented.
     * Not doing it seems to have no ill effects on command-line-editing
     * that I've noticed so far. - RT
     */

    }
  else if (str == term_dc)
    {

      /* Delete character at current cursor position */
      wdelch (cmdWin->generic.handle);
      wrefresh (cmdWin->generic.handle);

    }
  else if (str == term_im)
    {

      /* Turn on insert mode. */
      insert_mode = 1;

    }
  else if (str == term_ei)
    {

      /* Turn off insert mode. */
      insert_mode = 0;

      /* Strings we know about but don't handle
   * specially here are just passed along to tputs().
   *
   * These are not handled because (as far as I can tell)
   * they are not actually emitted by the readline package
   * in the course of doing command-line editing. Some of them
   * theoretically could be used in the future, in which case we'd
   * need to handle them.
   */
    }
  else if (str == term_ic ||	/* insert character */
	   str == term_cursor_move ||	/* cursor move */
	   str == term_clrpag ||/* clear page */
	   str == term_mm ||	/* turn on meta key */
	   str == term_mo ||	/* turn off meta key */
	   str == term_up ||	/* up one line (not expected) */
	   str == term_scroll_region ||	/* set scroll region */
	   str == term_memory_lock ||	/* lock screen above cursor */
	   str == term_memory_unlock ||	/* unlock screen above cursor */
	   str == visible_bell)
    {				/* flash screen */
      tputs (str, affcnt, putfunc);
    }
  else
    {				/* something else */
      tputs (str, affcnt, putfunc);
    }
}				/* tui_tputs */


/*
** tui_vwgetch()
**        Wrapper around wgetch with the window in a va_list
*/
unsigned int
#ifdef __STDC__
tui_vwgetch (va_list args)
#else
tui_vwgetch (args)
     va_list args;
#endif
{
  unsigned int ch;
  WINDOW *window;

  window = va_arg (args, WINDOW *);

  return ((unsigned int) wgetch (window));
}				/* tui_vwgetch */


/*
** tui_vread()
**   Wrapper around read() with paramets in a va_list
*/
unsigned int
#ifdef __STDC__
tui_vread (va_list args)
#else
tui_vread (args)
     va_list args;
#endif
{
  int result = 0;
  int filedes = va_arg (args, int);
  char *buf = va_arg (args, char *);
  int nbytes = va_arg (args, int);

  result = read (filedes, buf, nbytes);

  return result;
}				/* tui_vread() */

/*
** tuiRead()
**    Function to perform a read() catching resize events
*/
int
#ifdef __STDC__
tuiRead (
	  int filedes,
	  char *buf,
	  int nbytes)
#else
tuiRead (filedes, buf, nbytes)
     int filedes;
     char *buf;
     int nbytes;
#endif
{
  int result = 0;

  result = (int) vcatch_errors ((OpaqueFuncPtr) tui_vread, filedes, buf, nbytes);
  *buf = _tuiHandleResizeDuringIO (*buf);

  return result;
}				/* tuiRead */


/*
** tuiGetc().
**        Get a character from the command window.
**		This is called from the readline package,
**              that is, we have:
**                tuiGetc() [here], called from
**                readline code [in ../readline/], called from
**                command_line_input() in top.c
*/
unsigned int
#ifdef __STDC__
tuiGetc (void)
#else
tuiGetc ()
#endif
{
  unsigned int ch;
  extern char *rl_prompt;
  extern char *rl_line_buffer;
  extern int rl_point;

  /* Call the curses routine that reads one character */
#ifndef COMMENT
  ch = (unsigned int) vcatch_errors ((OpaqueFuncPtr) tui_vwgetch,
				     cmdWin->generic.handle);
#else
  ch = wgetch (cmdWin->generic.handle);
#endif
  ch = _tuiHandleResizeDuringIO (ch);

  if (m_isCommandChar (ch))
    {				/* Handle prev/next/up/down here */
      tuiTermSetup (0);
      ch = tuiDispatchCtrlChar (ch);
      cmdWin->detail.commandInfo.curch = strlen (rl_prompt) + rl_point;
      tuiTermUnsetup (0, cmdWin->detail.commandInfo.curch);
    }
  if (ch == '\n' || ch == '\r' || ch == '\f')
    cmdWin->detail.commandInfo.curch = 0;
  else
    tuiIncrCommandCharCountBy (1);

  return ch;
}				/* tuiGetc */


/*
** tuiBufferGetc().
*/
/*elz: this function reads a line of input from the user and
puts it in a static buffer. Subsequent calls to this same function
obtain one char at the time, providing the caller with a behavior
similar to fgetc. When the input is buffered, the backspaces have
the needed effect, i.e. ignore the last char active in the buffer*/
/* so far this function is called only from the query function in
utils.c*/

unsigned int
#ifdef __STDC__
tuiBufferGetc (void)
#else
tuiBufferGetc ()
#endif
{
  unsigned int ch;
  static unsigned char _ibuffer[512];
  static int index_read = -1;
  static int length_of_answer = -1;
  int pos = 0;

  if (length_of_answer == -1)
    {
      /* this is the first time through, need to read the answer*/
      do
	{
	  /* Call the curses routine that reads one character */
	  ch = (unsigned int) wgetch (cmdWin->generic.handle);
	  if (ch != '\b')
	    {
	      _ibuffer[pos] = ch;
	      pos++;
	    }
	  else
	    pos--;
	}
      while (ch != '\r' && ch != '\n');

      length_of_answer = pos;
      index_read = 0;
    }

  ch = _ibuffer[index_read];
  index_read++;

  if (index_read == length_of_answer)
    {
      /*this is the last time through, reset for next query*/
      index_read = -1;
      length_of_answer = -1;
    }

  wrefresh (cmdWin->generic.handle);

  return (ch);
}				/* tuiBufferGetc */


/*
** tuiStartNewLines().
*/
void
#ifdef __STDC__
tuiStartNewLines (
		   int numLines)
#else
tuiStartNewLines (numLines)
     int numLines;
#endif
{
  if (numLines > 0)
    {
      if (cmdWin->generic.viewportHeight > 1 &&
	cmdWin->detail.commandInfo.curLine < cmdWin->generic.viewportHeight)
	cmdWin->detail.commandInfo.curLine += numLines;
      else
	scroll (cmdWin->generic.handle);
      cmdWin->detail.commandInfo.curch = 0;
      wmove (cmdWin->generic.handle,
	     cmdWin->detail.commandInfo.curLine,
	     cmdWin->detail.commandInfo.curch);
      tuiRefreshWin (&cmdWin->generic);
    }

  return;
}				/* tuiStartNewLines */


/*
** tui_vStartNewLines().
**        With numLines in a va_list
*/
void
#ifdef __STDC__
tui_vStartNewLines (
		     va_list args)
#else
tui_vStartNewLines (args)
     va_list args;
#endif
{
  int numLines = va_arg (args, int);

  tuiStartNewLines (numLines);

  return;
}				/* tui_vStartNewLines */


/****************************************************************************
**                   LOCAL STATIC FUNCTIONS                                **
*****************************************************************************/


/*
** _tuiHandleResizeDuringIO
**    This function manages the cleanup when a resize has occured
**    From within a call to getch() or read.  Returns the character
**    to return from getc or read.
*/
static unsigned int
#ifdef __STDC__
_tuiHandleResizeDuringIO (
			   unsigned int originalCh)	/* the char just read */
#else
_tuiHandleResizeDuringIO (originalCh)
     unsigned int originalCh;
#endif
{
  if (tuiWinResized ())
    {
      tuiDo ((TuiOpaqueFuncPtr) tuiRefreshAll);
      dont_repeat ();
      tuiSetWinResizedTo (FALSE);
      rl_reset ();
      return '\n';
    }
  else
    return originalCh;
}				/* _tuiHandleResizeDuringIO */


/*
** _updateCommandInfo().
**        Function to update the command window information.
*/
static void
#ifdef __STDC__
_updateCommandInfo (
		     int sizeOfString)
#else
_updateCommandInfo (sizeOfString)
     int sizeOfString;
#endif
{

  if ((sizeOfString +
       cmdWin->detail.commandInfo.curch) > cmdWin->generic.width)
    {
      int newCurch = sizeOfString + cmdWin->detail.commandInfo.curch;

      tuiStartNewLines (1);
      cmdWin->detail.commandInfo.curch = newCurch - cmdWin->generic.width;
    }
  else
    cmdWin->detail.commandInfo.curch += sizeOfString;

  return;
}				/* _updateCommandInfo */


/* Looked at in main.c, fputs_unfiltered(), to decide
 * if it's safe to do standard output to the command window.
 */
int tui_owns_terminal = 0;

/* Called to set up the terminal for TUI (curses) I/O.
 * We do this either on our way "in" to GDB after target
 * program execution, or else within tuiDo just before
 * going off to TUI routines.
 */

void
#ifdef __STDC__
tuiTermSetup (
	       int turn_off_echo)
#else
tuiTermSetup (turn_off_echo)
     int turn_off_echo;
#endif
{
  char *buffer;
  int start;
  int end;
  int endcol;
  extern char *term_scroll_region;
  extern char *term_cursor_move;
  extern char *term_memory_lock;
  extern char *term_memory_unlock;

  /* Turn off echoing, since the TUI does not
     * expect echoing. Below I only put in the TERMIOS
     * case, since that is what applies on HP-UX. turn_off_echo
     * is 1 except for the case where we're being called
     * on a "quit", in which case we want to leave echo on.
     */
  if (turn_off_echo)
    {
#ifdef HAVE_TERMIOS
      struct termios tio;
      tcgetattr (0, &tio);
      tio.c_lflag &= ~(ECHO);
      tcsetattr (0, TCSANOW, &tio);
#endif
    }

  /* Compute the start and end lines of the command
     * region. (Actually we only use end here)
     */
  start = winList[CMD_WIN]->generic.origin.y;
  end = start + winList[CMD_WIN]->generic.height - 1;
  endcol = winList[CMD_WIN]->generic.width - 1;

  if (term_memory_unlock)
    {

      /* Un-do the effect of the memory lock in terminal_inferior() */
      tputs (term_memory_unlock, 1, (int (*)PARAMS ((int))) putchar);
      fflush (stdout);

    }
  else if (term_scroll_region)
    {

      /* Un-do the effect of setting scroll region in terminal_inferior() */
      /* I'm actually not sure how to do this (we don't know for
       * sure what the scroll region was *before* we changed it),
       * but I'll guess that setting it to the whole screen is
       * the right thing. So, ...
       */

      /* Set scroll region to be 0..end */
      buffer = (char *) tgoto (term_scroll_region, end, 0);
      tputs (buffer, 1, (int (*)PARAMS ((int))) putchar);

    }				/* else we're out of luck */

  /* This is an attempt to keep the logical & physical
       * cursor in synch, going into curses. Without this,
       * curses seems to be confused by the fact that
       * GDB has physically moved the curser on it. One
       * visible effect of removing this code is that the
       * locator window fails to get updated and the line
       * of text that *should* go into the locator window
       * often goes to the wrong place.
       */
  /* What's done here is to  tell curses to write a ' '
       * at the bottom right corner of the screen.
       * The idea is to wind up with the cursor in a known
       * place.
       * Note I'm relying on refresh()
       * only writing what changed (the space),
       * not the whole screen.
       */
  standend ();
  move (end, endcol - 1);
  addch (' ');
  refresh ();

  tui_owns_terminal = 1;
}				/* tuiTermSetup */


/* Called to set up the terminal for target program I/O, meaning I/O
 * is confined to the command-window area.  We also call this on our
 * way out of tuiDo, thus setting up the terminal this way for
 * debugger command I/O.  */
void
#ifdef __STDC__
tuiTermUnsetup (
		 int turn_on_echo,
		 int to_column)
#else
tuiTermUnsetup (turn_on_echo, to_column)
     int turn_on_echo;
     int to_column;
#endif
{
  int start;
  int end;
  int curline;
  char *buffer;
  /* The next bunch of things are from readline */
  extern char *term_scroll_region;
  extern char *term_cursor_move;
  extern char *term_memory_lock;
  extern char *term_memory_unlock;
  extern char *term_se;

  /* We need to turn on echoing, since the TUI turns it off */
  /* Below I only put in the TERMIOS case, since that
     * is what applies on HP-UX.
     */
  if (turn_on_echo)
    {
#ifdef HAVE_TERMIOS
      struct termios tio;
      tcgetattr (0, &tio);
      tio.c_lflag |= (ECHO);
      tcsetattr (0, TCSANOW, &tio);
#endif
    }

  /* Compute the start and end lines of the command
     * region, as well as the last "real" line of
     * the region (normally same as end, except when
     * we're first populating the region)
     */
  start = winList[CMD_WIN]->generic.origin.y;
  end = start + winList[CMD_WIN]->generic.height - 1;
  curline = start + winList[CMD_WIN]->detail.commandInfo.curLine;

  /* We want to confine target I/O to the command region.
     * In order to do so, we must either have "memory lock"
     * (hpterm's) or "scroll regions" (xterm's).
     */
  if (term_cursor_move && term_memory_lock)
    {

      /* Memory lock means lock region above cursor.
       * So first position the cursor, then call memory lock.
       */
      buffer = tgoto (term_cursor_move, 0, start);
      tputs (buffer, 1, (int (*)PARAMS ((int))) putchar);
      tputs (term_memory_lock, 1, (int (*)PARAMS ((int))) putchar);

    }
  else if (term_scroll_region)
    {

      /* Set the scroll region to the command window */
      buffer = tgoto (term_scroll_region, end, start);
      tputs (buffer, 1, (int (*)PARAMS ((int))) putchar);

    }				/* else we can't do anything about target I/O */

  /* Also turn off standout mode, in case it is on */
  if (term_se != NULL)
    tputs (term_se, 1, (int (*)PARAMS ((int))) putchar);

  /* Now go to the appropriate spot on the end line */
  buffer = tgoto (term_cursor_move, to_column, end);
  tputs (buffer, 1, (int (*)PARAMS ((int))) putchar);
  fflush (stdout);

  tui_owns_terminal = 0;
}				/* tuiTermUnsetup */
