/*
** tui.c
**         General functions for the WDB TUI
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#include <curses.h>
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#include <signal.h>
#include <fcntl.h>
#include <termio.h>
#include <setjmp.h>
#include "defs.h"
#include "gdbcmd.h"
#include "tui.h"
#include "tuiData.h"
#include "tuiLayout.h"
#include "tuiIO.h"
#include "tuiRegs.h"
#include "tuiWin.h"

/* The Solaris header files seem to provide no declaration for this at
   all when __STDC__ is defined.  This shouldn't conflict with
   anything.  */
extern char *tgoto ();

/***********************
** Local Definitions
************************/
#define FILEDES         2
/* Solaris <sys/termios.h> defines CTRL. */
#ifndef CTRL
#define CTRL(x)         (x & ~0140)
#endif
#define CHK(val, dft)   (val<=0 ? dft : val)

#define TOGGLE_USAGE "Usage:toggle breakpoints"
#define TUI_TOGGLE_USAGE "Usage:\ttoggle $fregs\n\ttoggle breakpoints"

/*****************************
** Local static forward decls
******************************/
static void _tuiReset PARAMS ((void));
static void _toggle_command PARAMS ((char *, int));
static void _tui_vToggle_command PARAMS ((va_list));
static Opaque _tui_vDo PARAMS ((TuiOpaqueFuncPtr, va_list));



/***********************
** Public Functions
************************/

/*
** tuiInit().
*/
void
#ifdef __STDC__
tuiInit (char *argv0)
#else
tuiInit (argv0)
     char *argv0;
#endif
{
  extern void init_page_info ();
  extern void initialize_tui_files PARAMS ((void));

  initialize_tui_files ();
  initializeStaticData ();
  initscr ();
  refresh ();
  setTermHeightTo (LINES);
  setTermWidthTo (COLS);
  tuiInitWindows ();
  wrefresh (cmdWin->generic.handle);
  init_page_info ();
  /* Don't hook debugger output if doing command-window
     * the XDB way. However, one thing we do want to do in
     * XDB style is set up the scrolling region to be
     * the bottom of the screen (tuiTermUnsetup()).
     */
  fputs_unfiltered_hook = NULL;
  flush_hook = NULL;
  rl_initialize ();		/* need readline initialization to
		      * create termcap sequences
		      */
  tuiTermUnsetup (1, cmdWin->detail.commandInfo.curch);

  return;
}				/* tuiInit */


/*
** tuiInitWindows().
*/
void
#ifdef __STDC__
tuiInitWindows (void)
#else
tuiInitWindows ()
#endif
{
  TuiWinType type;

  tuiSetLocatorContent (0);
  showLayout (SRC_COMMAND);
  keypad (cmdWin->generic.handle, TRUE);
  echo ();
  crmode ();
  nl ();
  tuiSetWinFocusTo (srcWin);

  return;
}				/* tuiInitWindows */


/*
** tuiCleanUp().
**        Kill signal handler and cleanup termination method
*/
void
#ifdef __STDC__
tuiResetScreen (void)
#else
tuiResetScreen ()
#endif
{
  TuiWinType type = SRC_WIN;

  keypad (cmdWin->generic.handle, FALSE);
  for (; type < MAX_MAJOR_WINDOWS; type++)
    {
      if (m_winPtrNotNull (winList[type]) &&
	  winList[type]->generic.type != UNDEFINED_WIN &&
	  !winList[type]->generic.isVisible)
	tuiDelWindow (winList[type]);
    }
  endwin ();
  initscr ();
  refresh ();
  echo ();
  crmode ();
  nl ();

  return;
}				/* tuiResetScreen */


/*
** tuiCleanUp().
**        Kill signal handler and cleanup termination method
*/
void
#ifdef __STDC__
tuiCleanUp (void)
#else
tuiCleanUp ()
#endif
{
  char *buffer;
  extern char *term_cursor_move;

  signal (SIGINT, SIG_IGN);
  tuiTermSetup (0);		/* Restore scrolling region to whole screen */
  keypad (cmdWin->generic.handle, FALSE);
  freeAllWindows ();
  endwin ();
  buffer = tgoto (term_cursor_move, 0, termHeight ());
  tputs (buffer, 1, putchar);
  _tuiReset ();

  return;
}				/* tuiCleanUp */


/*
** tuiError().
*/
void
#ifdef __STDC__
tuiError (
	   char *string,
	   int exitGdb)
#else
tuiError (string, exitGdb)
     char *string;
     int exitGdb;
#endif
{
  puts_unfiltered (string);
  if (exitGdb)
    {
      tuiCleanUp ();
      exit (-1);
    }

  return;
}				/* tuiError */


/*
** tui_vError()
**        tuiError with args in a va_list.
*/
void
#ifdef __STDC__
tui_vError (
	     va_list args)
#else
tui_vError (args)
     va_list args;
#endif
{
  char *string;
  int exitGdb;

  string = va_arg (args, char *);
  exitGdb = va_arg (args, int);

  tuiError (string, exitGdb);

  return;
}				/* tui_vError */


/*
** tuiFree()
**    Wrapper on top of free() to ensure that input address is greater than 0x0
*/
void
#ifdef __STDC__
tuiFree (
	  char *ptr)
#else
tuiFree (ptr)
     char *ptr;
#endif
{
  if (ptr != (char *) NULL)
    {
      free (ptr);
    }

  return;
}				/* tuiFree */


/* tuiGetLowDisassemblyAddress().
**        Determine what the low address will be to display in the TUI's
**        disassembly window.  This may or may not be the same as the
**        low address input.
*/
Opaque
#ifdef __STDC__
tuiGetLowDisassemblyAddress (
			      Opaque low,
			      Opaque pc)
#else
tuiGetLowDisassemblyAddress (low, pc)
     Opaque low;
     Opaque pc;
#endif
{
  int line;
  Opaque newLow;

  /*
    ** Determine where to start the disassembly so that the pc is about in the
    ** middle of the viewport.
    */
  for (line = 0, newLow = pc;
       (newLow > low &&
	line < (tuiDefaultWinViewportHeight (DISASSEM_WIN,
					     DISASSEM_COMMAND) / 2));)
    {
      bfd_byte buffer[4];

      newLow -= sizeof (bfd_getb32 (buffer));
      line++;
    }

  return newLow;
}				/* tuiGetLowDisassemblyAddress */


/* tui_vGetLowDisassemblyAddress().
**        Determine what the low address will be to display in the TUI's
**        disassembly window with args in a va_list.
*/
Opaque
#ifdef __STDC__
tui_vGetLowDisassemblyAddress (
				va_list args)
#else
tui_vGetLowDisassemblyAddress (args)
     va_list args;
#endif
{
  int line;
  Opaque newLow;
  Opaque low;
  Opaque pc;

  low = va_arg (args, Opaque);
  pc = va_arg (args, Opaque);

  return (tuiGetLowDisassemblyAddress (low, pc));

}				/* tui_vGetLowDisassemblyAddress */


/*
** tuiDo().
**        General purpose function to execute a tui function.  Transitions
**        between curses and the are handled here.  This function is called
**        by non-tui gdb functions.
**
**        Errors are caught here.
**        If there is no error, the value returned by 'func' is returned.
**        If there is an error, then zero is returned.
**
**       Must not be called with immediate_quit in effect (bad things might
**       happen, say we got a signal in the middle of a memcpy to quit_return).
**       This is an OK restriction; with very few exceptions immediate_quit can
**       be replaced by judicious use of QUIT.
*/
Opaque
#ifdef __STDC__
tuiDo (
	TuiOpaqueFuncPtr func,...)
#else
tuiDo (func, va_alist)
     TuiOpaqueFuncPtr func;
     va_dcl
#endif
{
  extern int terminal_is_ours;

  Opaque ret = (Opaque) NULL;

  /* It is an error to be tuiDo'ing if we
     * don't own the terminal.
     */
  if (!terminal_is_ours)
    return ret;

  if (tui_version)
    {
      va_list args;

#ifdef __STDC__
      va_start (args, func);
#else
      va_start (args);
#endif
      ret = _tui_vDo (func, args);
      va_end (args);
    }

  return ret;
}				/* tuiDo */


/*
** tuiDoAndReturnToTop().
**        General purpose function to execute a tui function.  Transitions
**        between curses and the are handled here.  This function is called
**        by non-tui gdb functions who wish to reset gdb to the top level.
**        After the tuiDo is performed, a return to the top level occurs.
**
**        Errors are caught here.
**        If there is no error, the value returned by 'func' is returned.
**        If there is an error, then zero is returned.
**
**       Must not be called with immediate_quit in effect (bad things might
**       happen, say we got a signal in the middle of a memcpy to quit_return).
**       This is an OK restriction; with very few exceptions immediate_quit can
**       be replaced by judicious use of QUIT.
**
*/
Opaque
#ifdef __STDC__
tuiDoAndReturnToTop (
		      TuiOpaqueFuncPtr func,...)
#else
tuiDoAndReturnToTop (func, va_alist)
     TuiOpaqueFuncPtr func;
     va_dcl
#endif
{
  extern int terminal_is_ours;

  Opaque ret = (Opaque) NULL;

  /* It is an error to be tuiDo'ing if we
     * don't own the terminal.
     */
  if (!terminal_is_ours)
    return ret;

  if (tui_version)
    {
      va_list args;

#ifdef __STDC__
      va_start (args, func);
#else
      va_start (args);
#endif
      ret = _tui_vDo (func, args);

      /* force a return to the top level */
      return_to_top_level (RETURN_ERROR);
    }

  return ret;
}				/* tuiDoAndReturnToTop */


void
#ifdef __STDC__
tui_vSelectSourceSymtab (
			  va_list args)
#else
tui_vSelectSourceSymtab (args)
     va_list args;
#endif
{
  struct symtab *s = va_arg (args, struct symtab *);

  select_source_symtab (s);
  return;
}				/* tui_vSelectSourceSymtab */


/*
** _initialize_tui().
**      Function to initialize gdb commands, for tui window manipulation.
*/
void
_initialize_tui ()
{
#if 0
  if (tui_version)
    {
      add_com ("toggle", class_tui, _toggle_command,
	       "Toggle Terminal UI Features\n\
Usage: Toggle $fregs\n\
\tToggles between single and double precision floating point registers.\n");
    }
#endif
  char *helpStr;

  if (tui_version)
    helpStr = "Toggle Specified Features\n\
Usage:\ttoggle $fregs\n\ttoggle breakpoints";
  else
    helpStr = "Toggle Specified Features\nUsage:toggle breakpoints";
  add_abbrev_prefix_cmd ("toggle",
			 class_tui,
			 _toggle_command,
			 helpStr,
			 &togglelist,
			 "toggle ",
			 1,
			 &cmdlist);
}				/* _initialize_tui*/


/*
** va_catch_errors().
**       General purpose function to execute a function, catching errors.
**       If there is no error, the value returned by 'func' is returned.
**       If there is error, then zero is returned.
**       Note that 'func' must take a variable argument list as well.
**
**       Must not be called with immediate_quit in effect (bad things might
**       happen, say we got a signal in the middle of a memcpy to quit_return).
**       This is an OK restriction; with very few exceptions immediate_quit can
**       be replaced by judicious use of QUIT.
*/
Opaque
#ifdef __STDC__
va_catch_errors (
		  TuiOpaqueFuncPtr func,
		  va_list args)
#else
va_catch_errors (func, args)
     TuiOpaqueFuncPtr func;
     va_list args;
#endif
{
  Opaque ret = (Opaque) NULL;

  /*
  ** We could have used catch_errors(), but it doesn't handle variable args.
  ** Also, for the tui, we always want to catch all errors, so we don't
  ** need to pass a mask, or an error string.
  */
  jmp_buf saved_error;
  jmp_buf saved_quit;
  jmp_buf tmp_jmp;
  struct cleanup *saved_cleanup_chain;
  char *saved_error_pre_print;
  char *saved_quit_pre_print;
  extern jmp_buf error_return;
  extern jmp_buf quit_return;

  saved_cleanup_chain = save_cleanups ();
  saved_error_pre_print = error_pre_print;
  saved_quit_pre_print = quit_pre_print;

  memcpy ((char *) saved_error, (char *) error_return, sizeof (jmp_buf));
  error_pre_print = "";
  memcpy (saved_quit, quit_return, sizeof (jmp_buf));
  quit_pre_print = "";

  if (setjmp (tmp_jmp) == 0)
    {
      va_list argList = args;
      memcpy (error_return, tmp_jmp, sizeof (jmp_buf));
      memcpy (quit_return, tmp_jmp, sizeof (jmp_buf));
      ret = func (argList);
    }
  restore_cleanups (saved_cleanup_chain);
  memcpy (error_return, saved_error, sizeof (jmp_buf));
  error_pre_print = saved_error_pre_print;
  memcpy (quit_return, saved_quit, sizeof (jmp_buf));
  quit_pre_print = saved_quit_pre_print;

  return ret;
}

/*
** vcatch_errors().
**        Catch errors occurring in tui or non tui function, handling
**        variable param lists. Note that 'func' must take a variable
**        argument list as well.
*/
Opaque
#ifdef __STDC__
vcatch_errors (
		OpaqueFuncPtr func,...)
#else
vcatch_errors (va_alist)
     va_dcl
/*
vcatch_errors(func, va_alist)
    OpaqueFuncPtr    func;
    va_dcl
*/
#endif
{
  Opaque ret = (Opaque) NULL;
  va_list args;
#ifdef __STDC__
  va_start (args, func);
/*
    va_arg(args, OpaqueFuncPtr);
*/
#else
  OpaqueFuncPtr func;

  va_start (args);
  func = va_arg (args, OpaqueFuncPtr);
#endif
  ret = va_catch_errors (func, args);
  va_end (args);

  return ret;
}


void
#ifdef __STDC__
strcat_to_buf (
		char *buf,
		int buflen,
		char *itemToAdd)
#else
strcat_to_buf (buf, buflen, itemToAdd)
     char *buf;
     int buflen;
     char *itemToAdd;
#endif
{
  if (itemToAdd != (char *) NULL && buf != (char *) NULL)
    {
      if ((strlen (buf) + strlen (itemToAdd)) <= buflen)
	strcat (buf, itemToAdd);
      else
	strncat (buf, itemToAdd, (buflen - strlen (buf)));
    }

  return;
}				/* strcat_to_buf */

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
strcat_to_buf_with_fmt (
			 char *buf,
			 int bufLen,
			 char *format,...)
#else
strcat_to_buf_with_fmt (va_alist)
     va_dcl
#endif
{
  char *linebuffer;
  struct cleanup *old_cleanups;
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  char *buf;
  int bufLen;
  char *format;

  va_start (args);
  buf = va_arg (args, char *);
  bufLen = va_arg (args, int);
  format = va_arg (args, char *);
#endif
  vasprintf (&linebuffer, format, args);
  old_cleanups = make_cleanup (free, linebuffer);
  strcat_to_buf (buf, bufLen, linebuffer);
  do_cleanups (old_cleanups);
  va_end (args);
}





/***********************
** Static Functions
************************/


/*
** _tui_vDo().
**        General purpose function to execute a tui function.  Transitions
**        between curses and the are handled here.  This function is called
**        by non-tui gdb functions.
**
**        Errors are caught here.
**        If there is no error, the value returned by 'func' is returned.
**        If there is an error, then zero is returned.
**
**       Must not be called with immediate_quit in effect (bad things might
**       happen, say we got a signal in the middle of a memcpy to quit_return).
**       This is an OK restriction; with very few exceptions immediate_quit can
**       be replaced by judicious use of QUIT.
*/
static Opaque
#ifdef __STDC__
_tui_vDo (
	   TuiOpaqueFuncPtr func,
	   va_list args)
#else
_tui_vDo (func, args)
     TuiOpaqueFuncPtr func;
     va_list args;
#endif
{
  extern int terminal_is_ours;

  Opaque ret = (Opaque) NULL;

  /* It is an error to be tuiDo'ing if we
     * don't own the terminal.
     */
  if (!terminal_is_ours)
    return ret;

  if (tui_version)
    {
      /* If doing command window the "XDB way" (command window
         * is unmanaged by curses...
         */
      /* Set up terminal for TUI */
      tuiTermSetup (1);

      ret = va_catch_errors (func, args);

      /* Set up terminal for command window */
      tuiTermUnsetup (1, cmdWin->detail.commandInfo.curch);
    }

  return ret;
}				/* _tui_vDo */


static void
#ifdef __STDC__
_toggle_command (
		  char *arg,
		  int fromTTY)
#else
_toggle_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  printf_filtered ("Specify feature to toggle.\n%s\n",
		   (tui_version) ? TUI_TOGGLE_USAGE : TOGGLE_USAGE);
/*
  tuiDo((TuiOpaqueFuncPtr)_Toggle_command, arg, fromTTY);
*/
}

/*
** _tui_vToggle_command().
*/
static void
#ifdef __STDC__
_tui_vToggle_command (
		       va_list args)
#else
_tui_vToggle_command (args)
     va_list args;
#endif
{
  char *arg;
  int fromTTY;

  arg = va_arg (args, char *);

  if (arg == (char *) NULL)
    printf_filtered (TOGGLE_USAGE);
  else
    {
      char *ptr = (char *) tuiStrDup (arg);
      int i;

      for (i = 0; (ptr[i]); i++)
	ptr[i] = toupper (arg[i]);

      if (subsetCompare (ptr, TUI_FLOAT_REGS_NAME))
	tuiToggleFloatRegs ();
/*        else if (subsetCompare(ptr, "ANOTHER TOGGLE OPTION"))
            ...
*/
      else
	printf_filtered (TOGGLE_USAGE);
      tuiFree (ptr);
    }

  return;
}				/* _tuiToggle_command */


static void
#ifdef __STDC__
_tuiReset (void)
#else
_tuiReset ()
#endif
{
  struct termio mode;

  /*
    ** reset the teletype mode bits to a sensible state.
    ** Copied tset.c
    */
#if ! defined (USG) && defined (TIOCGETC)
  struct tchars tbuf;
#endif /* !USG && TIOCGETC */
#ifdef UCB_NTTY
  struct ltchars ltc;

  if (ldisc == NTTYDISC)
    {
      ioctl (FILEDES, TIOCGLTC, &ltc);
      ltc.t_suspc = CHK (ltc.t_suspc, CTRL ('Z'));
      ltc.t_dsuspc = CHK (ltc.t_dsuspc, CTRL ('Y'));
      ltc.t_rprntc = CHK (ltc.t_rprntc, CTRL ('R'));
      ltc.t_flushc = CHK (ltc.t_flushc, CTRL ('O'));
      ltc.t_werasc = CHK (ltc.t_werasc, CTRL ('W'));
      ltc.t_lnextc = CHK (ltc.t_lnextc, CTRL ('V'));
      ioctl (FILEDES, TIOCSLTC, &ltc);
    }
#endif /* UCB_NTTY */
#ifndef USG
#ifdef TIOCGETC
  ioctl (FILEDES, TIOCGETC, &tbuf);
  tbuf.t_intrc = CHK (tbuf.t_intrc, CTRL ('?'));
  tbuf.t_quitc = CHK (tbuf.t_quitc, CTRL ('\\'));
  tbuf.t_startc = CHK (tbuf.t_startc, CTRL ('Q'));
  tbuf.t_stopc = CHK (tbuf.t_stopc, CTRL ('S'));
  tbuf.t_eofc = CHK (tbuf.t_eofc, CTRL ('D'));
  /* brkc is left alone */
  ioctl (FILEDES, TIOCSETC, &tbuf);
#endif /* TIOCGETC */
  mode.sg_flags &= ~(RAW
#ifdef CBREAK
		     | CBREAK
#endif /* CBREAK */
		     | VTDELAY | ALLDELAY);
  mode.sg_flags |= XTABS | ECHO | CRMOD | ANYP;
#else /*USG*/
  ioctl (FILEDES, TCGETA, &mode);
  mode.c_cc[VINTR] = CHK (mode.c_cc[VINTR], CTRL ('?'));
  mode.c_cc[VQUIT] = CHK (mode.c_cc[VQUIT], CTRL ('\\'));
  mode.c_cc[VEOF] = CHK (mode.c_cc[VEOF], CTRL ('D'));

  mode.c_iflag &= ~(IGNBRK | PARMRK | INPCK | INLCR | IGNCR | IUCLC | IXOFF);
  mode.c_iflag |= (BRKINT | ISTRIP | ICRNL | IXON);
  mode.c_oflag &= ~(OLCUC | OCRNL | ONOCR | ONLRET | OFILL | OFDEL |
		    NLDLY | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);
  mode.c_oflag |= (OPOST | ONLCR);
  mode.c_cflag &= ~(CSIZE | PARODD | CLOCAL);
#ifndef hp9000s800
  mode.c_cflag |= (CS8 | CREAD);
#else /*hp9000s800*/
  mode.c_cflag |= (CS8 | CSTOPB | CREAD);
#endif /* hp9000s800 */
  mode.c_lflag &= ~(XCASE | ECHONL | NOFLSH);
  mode.c_lflag |= (ISIG | ICANON | ECHO | ECHOK);
  ioctl (FILEDES, TCSETAW, &mode);
#endif /* USG */

  return;
}				/* _tuiReset */
