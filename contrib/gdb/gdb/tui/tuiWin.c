/*
** tuiWin.c
**    This module contains procedures for handling tui window functions
**    like resize, scrolling, scrolling, changing focus, etc.
**
** Author: Susan B. Macchia
*/


#include <string.h>
#include "defs.h"
#include "command.h"
#include "symtab.h"
#include "breakpoint.h"
#include "frame.h"

#include "tui.h"
#include "tuiData.h"
#include "tuiGeneralWin.h"
#include "tuiStack.h"
#include "tuiSourceWin.h"
#include "tuiDataWin.h"

/*******************************
** External Declarations
********************************/
extern void init_page_info ();

/*******************************
** Static Local Decls
********************************/
static void _makeVisibleWithNewHeight PARAMS ((TuiWinInfoPtr));
static void _makeInvisibleAndSetNewHeight PARAMS ((TuiWinInfoPtr, int));
static TuiStatus _tuiAdjustWinHeights PARAMS ((TuiWinInfoPtr, int));
static int _newHeightOk PARAMS ((TuiWinInfoPtr, int));
static void _tuiSetTabWidth_command PARAMS ((char *, int));
static void _tuiRefreshAll_command PARAMS ((char *, int));
static void _tuiSetWinHeight_command PARAMS ((char *, int));
static void _tuiXDBsetWinHeight_command PARAMS ((char *, int));
static void _tuiAllWindowsInfo PARAMS ((char *, int));
static void _tuiSetFocus_command PARAMS ((char *, int));
static void _tuiScrollForward_command PARAMS ((char *, int));
static void _tuiScrollBackward_command PARAMS ((char *, int));
static void _tuiScrollLeft_command PARAMS ((char *, int));
static void _tuiScrollRight_command PARAMS ((char *, int));
static void _parseScrollingArgs PARAMS ((char *, TuiWinInfoPtr *, int *));


/***************************************
** DEFINITIONS
***************************************/
#define WIN_HEIGHT_USAGE      "Usage: winheight <win_name> [+ | -] <#lines>\n"
#define XDBWIN_HEIGHT_USAGE   "Usage: w <#lines>\n"
#define FOCUS_USAGE           "Usage: focus {<win> | next | prev}\n"

/***************************************
** PUBLIC FUNCTIONS
***************************************/

/*
** _initialize_tuiWin().
**        Function to initialize gdb commands, for tui window manipulation.
*/
void
_initialize_tuiWin ()
{
  if (tui_version)
    {
      add_com ("refresh", class_tui, _tuiRefreshAll_command,
	       "Refresh the terminal display.\n");
      if (xdb_commands)
	add_com_alias ("U", "refresh", class_tui, 0);
      add_com ("tabset", class_tui, _tuiSetTabWidth_command,
	       "Set the width (in characters) of tab stops.\n\
Usage: tabset <n>\n");
      add_com ("winheight", class_tui, _tuiSetWinHeight_command,
	       "Set the height of a specified window.\n\
Usage: winheight <win_name> [+ | -] <#lines>\n\
Window names are:\n\
src  : the source window\n\
cmd  : the command window\n\
asm  : the disassembly window\n\
regs : the register display\n");
      add_com_alias ("wh", "winheight", class_tui, 0);
      add_info ("win", _tuiAllWindowsInfo,
		"List of all displayed windows.\n");
      add_com ("focus", class_tui, _tuiSetFocus_command,
	       "Set focus to named window or next/prev window.\n\
Usage: focus {<win> | next | prev}\n\
Valid Window names are:\n\
src  : the source window\n\
asm  : the disassembly window\n\
regs : the register display\n\
cmd  : the command window\n");
      add_com_alias ("fs", "focus", class_tui, 0);
      add_com ("+", class_tui, _tuiScrollForward_command,
	       "Scroll window forward.\nUsage: + [win] [n]\n");
      add_com ("-", class_tui, _tuiScrollBackward_command,
	       "Scroll window backward.\nUsage: - [win] [n]\n");
      add_com ("<", class_tui, _tuiScrollLeft_command,
	       "Scroll window forward.\nUsage: < [win] [n]\n");
      add_com (">", class_tui, _tuiScrollRight_command,
	       "Scroll window backward.\nUsage: > [win] [n]\n");
      if (xdb_commands)
	add_com ("w", class_xdb, _tuiXDBsetWinHeight_command,
		 "XDB compatibility command for setting the height of a command window.\n\
Usage: w <#lines>\n");
    }

  return;
}				/* _intialize_tuiWin */


/*
** tuiClearWinFocusFrom
**        Clear the logical focus from winInfo
*/
void
#ifdef __STDC__
tuiClearWinFocusFrom (
		       TuiWinInfoPtr winInfo)
#else
tuiClearWinFocusFrom (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (m_winPtrNotNull (winInfo))
    {
      if (winInfo->generic.type != CMD_WIN)
	unhighlightWin (winInfo);
      tuiSetWinWithFocus ((TuiWinInfoPtr) NULL);
    }

  return;
}				/* tuiClearWinFocusFrom */


/*
** tuiClearWinFocus().
**        Clear the window that has focus.
*/
void
#ifdef __STDC__
tuiClearWinFocus (void)
#else
tuiClearWinFocus ()
#endif
{
  tuiClearWinFocusFrom (tuiWinWithFocus ());

  return;
}				/* tuiClearWinFocus */


/*
** tuiSetWinFocusTo
**        Set the logical focus to winInfo
*/
void
#ifdef __STDC__
tuiSetWinFocusTo (
		   TuiWinInfoPtr winInfo)
#else
tuiSetWinFocusTo (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (m_winPtrNotNull (winInfo))
    {
      TuiWinInfoPtr winWithFocus = tuiWinWithFocus ();

      if (m_winPtrNotNull (winWithFocus) &&
	  winWithFocus->generic.type != CMD_WIN)
	unhighlightWin (winWithFocus);
      tuiSetWinWithFocus (winInfo);
      if (winInfo->generic.type != CMD_WIN)
	highlightWin (winInfo);
    }

  return;
}				/* tuiSetWinFocusTo */


char *
#ifdef __STDC__
tuiStrDup (
	    char *str)
#else
tuiStrDup (str)
     char *str;
#endif
{
  char *newStr = (char *) NULL;

  if (str != (char *) NULL)
    {
      newStr = (char *) xmalloc (strlen (str) + 1);
      strcpy (newStr, str);
    }

  return newStr;
}				/* tuiStrDup */


/*
** tuiScrollForward().
*/
void
#ifdef __STDC__
tuiScrollForward (
		   TuiWinInfoPtr winToScroll,
		   int numToScroll)
#else
tuiScrollForward (winToScroll, numToScroll)
     TuiWinInfoPtr winToScroll;
     int numToScroll;
#endif
{
  if (winToScroll != cmdWin)
    {
      int _numToScroll = numToScroll;

      if (numToScroll == 0)
	_numToScroll = winToScroll->generic.height - 3;
      /*
        ** If we are scrolling the source or disassembly window, do a
        ** "psuedo" scroll since not all of the source is in memory,
        ** only what is in the viewport.  If winToScroll is the
        ** command window do nothing since the term should handle it.
        */
      if (winToScroll == srcWin)
	tuiVerticalSourceScroll (FORWARD_SCROLL, _numToScroll);
      else if (winToScroll == disassemWin)
	tuiVerticalDisassemScroll (FORWARD_SCROLL, _numToScroll);
      else if (winToScroll == dataWin)
	tuiVerticalDataScroll (FORWARD_SCROLL, _numToScroll);
    }

  return;
}				/* tuiScrollForward */


/*
** tuiScrollBackward().
*/
void
#ifdef __STDC__
tuiScrollBackward (
		    TuiWinInfoPtr winToScroll,
		    int numToScroll)
#else
tuiScrollBackward (winToScroll, numToScroll)
     TuiWinInfoPtr winToScroll;
     int numToScroll;
#endif
{
  if (winToScroll != cmdWin)
    {
      int _numToScroll = numToScroll;

      if (numToScroll == 0)
	_numToScroll = winToScroll->generic.height - 3;
      /*
        ** If we are scrolling the source or disassembly window, do a
        ** "psuedo" scroll since not all of the source is in memory,
        ** only what is in the viewport.  If winToScroll is the
        ** command window do nothing since the term should handle it.
        */
      if (winToScroll == srcWin)
	tuiVerticalSourceScroll (BACKWARD_SCROLL, _numToScroll);
      else if (winToScroll == disassemWin)
	tuiVerticalDisassemScroll (BACKWARD_SCROLL, _numToScroll);
      else if (winToScroll == dataWin)
	tuiVerticalDataScroll (BACKWARD_SCROLL, _numToScroll);
    }
  return;
}				/* tuiScrollBackward */


/*
** tuiScrollLeft().
*/
void
#ifdef __STDC__
tuiScrollLeft (
		TuiWinInfoPtr winToScroll,
		int numToScroll)
#else
tuiScrollLeft (winToScroll, numToScroll)
     TuiWinInfoPtr winToScroll;
     int numToScroll;
#endif
{
  if (winToScroll != cmdWin)
    {
      int _numToScroll = numToScroll;

      if (_numToScroll == 0)
	_numToScroll = 1;
      /*
        ** If we are scrolling the source or disassembly window, do a
        ** "psuedo" scroll since not all of the source is in memory,
        ** only what is in the viewport. If winToScroll is the
        ** command window do nothing since the term should handle it.
        */
      if (winToScroll == srcWin || winToScroll == disassemWin)
	tuiHorizontalSourceScroll (winToScroll, LEFT_SCROLL, _numToScroll);
    }
  return;
}				/* tuiScrollLeft */


/*
** tuiScrollRight().
*/
void
#ifdef __STDC__
tuiScrollRight (
		 TuiWinInfoPtr winToScroll,
		 int numToScroll)
#else
tuiScrollRight (winToScroll, numToScroll)
     TuiWinInfoPtr winToScroll;
     int numToScroll;
#endif
{
  if (winToScroll != cmdWin)
    {
      int _numToScroll = numToScroll;

      if (_numToScroll == 0)
	_numToScroll = 1;
      /*
        ** If we are scrolling the source or disassembly window, do a
        ** "psuedo" scroll since not all of the source is in memory,
        ** only what is in the viewport. If winToScroll is the
        ** command window do nothing since the term should handle it.
        */
      if (winToScroll == srcWin || winToScroll == disassemWin)
	tuiHorizontalSourceScroll (winToScroll, RIGHT_SCROLL, _numToScroll);
    }
  return;
}				/* tuiScrollRight */


/*
** tui_vScroll().
**    Scroll a window.  Arguments are passed through a va_list.
*/
void
#ifdef __STDC__
tui_vScroll (
	      va_list args)
#else
tui_vScroll (args)
     va_list args;
#endif
{
  TuiScrollDirection direction = va_arg (args, TuiScrollDirection);
  TuiWinInfoPtr winToScroll = va_arg (args, TuiWinInfoPtr);
  int numToScroll = va_arg (args, int);

  switch (direction)
    {
    case FORWARD_SCROLL:
      tuiScrollForward (winToScroll, numToScroll);
      break;
    case BACKWARD_SCROLL:
      tuiScrollBackward (winToScroll, numToScroll);
      break;
    case LEFT_SCROLL:
      tuiScrollLeft (winToScroll, numToScroll);
      break;
    case RIGHT_SCROLL:
      tuiScrollRight (winToScroll, numToScroll);
      break;
    default:
      break;
    }

  return;
}				/* tui_vScroll */


/*
** tuiRefreshAll().
*/
void
#ifdef __STDC__
tuiRefreshAll (void)
#else
tuiRefreshAll ()
#endif
{
  TuiWinType type;

  refreshAll (winList);
  for (type = SRC_WIN; type < MAX_MAJOR_WINDOWS; type++)
    {
      if (winList[type]->generic.isVisible)
	{
	  switch (type)
	    {
	    case SRC_WIN:
	    case DISASSEM_WIN:
	      tuiClearWin (&winList[type]->generic);
	      if (winList[type]->detail.sourceInfo.hasLocator)
		tuiClearLocatorDisplay ();
	      tuiShowSourceContent (winList[type]);
	      checkAndDisplayHighlightIfNeeded (winList[type]);
	      tuiEraseExecInfoContent (winList[type]);
	      tuiUpdateExecInfo (winList[type]);
	      break;
	    case DATA_WIN:
	      tuiRefreshDataWin ();
	      break;
	    default:
	      break;
	    }
	}
    }
  tuiClearLocatorDisplay ();
  tuiShowLocatorContent ();

  return;
}				/* tuiRefreshAll */


/*
** tuiResizeAll().
**      Resize all the windows based on the the terminal size.  This
**      function gets called from within the readline sinwinch handler.
*/
void
#ifdef __STDC__
tuiResizeAll (void)
#else
tuiResizeAll ()
#endif
{
  int heightDiff, widthDiff;
  extern int screenheight, screenwidth;	/* in readline */

  widthDiff = screenwidth - termWidth ();
  heightDiff = screenheight - termHeight ();
  if (heightDiff || widthDiff)
    {
      TuiLayoutType curLayout = currentLayout ();
      TuiWinInfoPtr winWithFocus = tuiWinWithFocus ();
      TuiWinInfoPtr firstWin, secondWin;
      TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
      TuiWinType winType;
      int i, newHeight, splitDiff, cmdSplitDiff, numWinsDisplayed = 2;

      /* turn keypad off while we resize */
      if (winWithFocus != cmdWin)
	keypad (cmdWin->generic.handle, FALSE);
      init_page_info ();
      setTermHeightTo (screenheight);
      setTermWidthTo (screenwidth);
      if (curLayout == SRC_DISASSEM_COMMAND ||
	curLayout == SRC_DATA_COMMAND || curLayout == DISASSEM_DATA_COMMAND)
	numWinsDisplayed++;
      splitDiff = heightDiff / numWinsDisplayed;
      cmdSplitDiff = splitDiff;
      if (heightDiff % numWinsDisplayed)
	{
	  if (heightDiff < 0)
	    cmdSplitDiff--;
	  else
	    cmdSplitDiff++;
	}
      /* now adjust each window */
      clear ();
      refresh ();
      switch (curLayout)
	{
	case SRC_COMMAND:
	case DISASSEM_COMMAND:
	  firstWin = (TuiWinInfoPtr) (sourceWindows ())->list[0];
	  firstWin->generic.width += widthDiff;
	  locator->width += widthDiff;
	  /* check for invalid heights */
	  if (heightDiff == 0)
	    newHeight = firstWin->generic.height;
	  else if ((firstWin->generic.height + splitDiff) >=
		   (screenheight - MIN_CMD_WIN_HEIGHT - 1))
	    newHeight = screenheight - MIN_CMD_WIN_HEIGHT - 1;
	  else if ((firstWin->generic.height + splitDiff) <= 0)
	    newHeight = MIN_WIN_HEIGHT;
	  else
	    newHeight = firstWin->generic.height + splitDiff;

	  _makeInvisibleAndSetNewHeight (firstWin, newHeight);
	  cmdWin->generic.origin.y = locator->origin.y + 1;
	  cmdWin->generic.width += widthDiff;
	  newHeight = screenheight - cmdWin->generic.origin.y;
	  _makeInvisibleAndSetNewHeight (cmdWin, newHeight);
	  _makeVisibleWithNewHeight (firstWin);
	  _makeVisibleWithNewHeight (cmdWin);
	  if (firstWin->generic.contentSize <= 0)
	    tuiEraseSourceContent (firstWin, EMPTY_SOURCE_PROMPT);
	  break;
	default:
	  if (curLayout == SRC_DISASSEM_COMMAND)
	    {
	      firstWin = srcWin;
	      firstWin->generic.width += widthDiff;
	      secondWin = disassemWin;
	      secondWin->generic.width += widthDiff;
	    }
	  else
	    {
	      firstWin = dataWin;
	      firstWin->generic.width += widthDiff;
	      secondWin = (TuiWinInfoPtr) (sourceWindows ())->list[0];
	      secondWin->generic.width += widthDiff;
	    }
	  /* Change the first window's height/width */
	  /* check for invalid heights */
	  if (heightDiff == 0)
	    newHeight = firstWin->generic.height;
	  else if ((firstWin->generic.height +
		    secondWin->generic.height + (splitDiff * 2)) >=
		   (screenheight - MIN_CMD_WIN_HEIGHT - 1))
	    newHeight = (screenheight - MIN_CMD_WIN_HEIGHT - 1) / 2;
	  else if ((firstWin->generic.height + splitDiff) <= 0)
	    newHeight = MIN_WIN_HEIGHT;
	  else
	    newHeight = firstWin->generic.height + splitDiff;
	  _makeInvisibleAndSetNewHeight (firstWin, newHeight);

	  if (firstWin == dataWin && widthDiff != 0)
	    firstWin->detail.dataDisplayInfo.regsColumnCount =
	      tuiCalculateRegsColumnCount (
			  firstWin->detail.dataDisplayInfo.regsDisplayType);
	  locator->width += widthDiff;

	  /* Change the second window's height/width */
	  /* check for invalid heights */
	  if (heightDiff == 0)
	    newHeight = secondWin->generic.height;
	  else if ((firstWin->generic.height +
		    secondWin->generic.height + (splitDiff * 2)) >=
		   (screenheight - MIN_CMD_WIN_HEIGHT - 1))
	    {
	      newHeight = screenheight - MIN_CMD_WIN_HEIGHT - 1;
	      if (newHeight % 2)
		newHeight = (newHeight / 2) + 1;
	      else
		newHeight /= 2;
	    }
	  else if ((secondWin->generic.height + splitDiff) <= 0)
	    newHeight = MIN_WIN_HEIGHT;
	  else
	    newHeight = secondWin->generic.height + splitDiff;
	  secondWin->generic.origin.y = firstWin->generic.height - 1;
	  _makeInvisibleAndSetNewHeight (secondWin, newHeight);

	  /* Change the command window's height/width */
	  cmdWin->generic.origin.y = locator->origin.y + 1;
	  _makeInvisibleAndSetNewHeight (
			     cmdWin, cmdWin->generic.height + cmdSplitDiff);
	  _makeVisibleWithNewHeight (firstWin);
	  _makeVisibleWithNewHeight (secondWin);
	  _makeVisibleWithNewHeight (cmdWin);
	  if (firstWin->generic.contentSize <= 0)
	    tuiEraseSourceContent (firstWin, EMPTY_SOURCE_PROMPT);
	  if (secondWin->generic.contentSize <= 0)
	    tuiEraseSourceContent (secondWin, EMPTY_SOURCE_PROMPT);
	  break;
	}
      /*
        ** Now remove all invisible windows, and their content so that they get
        ** created again when called for with the new size
        */
      for (winType = SRC_WIN; (winType < MAX_MAJOR_WINDOWS); winType++)
	{
	  if (winType != CMD_WIN && m_winPtrNotNull (winList[winType]) &&
	      !winList[winType]->generic.isVisible)
	    {
	      freeWindow (winList[winType]);
	      winList[winType] = (TuiWinInfoPtr) NULL;
	    }
	}
      tuiSetWinResizedTo (TRUE);
      /* turn keypad back on, unless focus is in the command window */
      if (winWithFocus != cmdWin)
	keypad (cmdWin->generic.handle, TRUE);
    }
  return;
}				/* tuiResizeAll */


/*
** tuiSigwinchHandler()
**    SIGWINCH signal handler for the tui.  This signal handler is
**    always called, even when the readline package clears signals
**    because it is set as the old_sigwinch() (TUI only)
*/
void
#ifdef __STDC__
tuiSigwinchHandler (
		     int signal)
#else
tuiSigwinchHandler (signal)
     int signal;
#endif
{
  /*
    ** Say that a resize was done so that the readline can do it
    ** later when appropriate.
    */
  tuiSetWinResizedTo (TRUE);

  return;
}				/* tuiSigwinchHandler */



/*************************
** STATIC LOCAL FUNCTIONS
**************************/


/*
** _tuiScrollForward_command().
*/
static void
#ifdef __STDC__
_tuiScrollForward_command (
			    char *arg,
			    int fromTTY)
#else
_tuiScrollForward_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  int numToScroll = 1;
  TuiWinInfoPtr winToScroll;

  if (arg == (char *) NULL)
    _parseScrollingArgs (arg, &winToScroll, (int *) NULL);
  else
    _parseScrollingArgs (arg, &winToScroll, &numToScroll);
  tuiDo ((TuiOpaqueFuncPtr) tui_vScroll,
	 FORWARD_SCROLL,
	 winToScroll,
	 numToScroll);

  return;
}				/* _tuiScrollForward_command */


/*
** _tuiScrollBackward_command().
*/
static void
#ifdef __STDC__
_tuiScrollBackward_command (
			     char *arg,
			     int fromTTY)
#else
_tuiScrollBackward_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  int numToScroll = 1;
  TuiWinInfoPtr winToScroll;

  if (arg == (char *) NULL)
    _parseScrollingArgs (arg, &winToScroll, (int *) NULL);
  else
    _parseScrollingArgs (arg, &winToScroll, &numToScroll);
  tuiDo ((TuiOpaqueFuncPtr) tui_vScroll,
	 BACKWARD_SCROLL,
	 winToScroll,
	 numToScroll);

  return;
}				/* _tuiScrollBackward_command */


/*
** _tuiScrollLeft_command().
*/
static void
#ifdef __STDC__
_tuiScrollLeft_command (
			 char *arg,
			 int fromTTY)
#else
_tuiScrollLeft_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  int numToScroll;
  TuiWinInfoPtr winToScroll;

  _parseScrollingArgs (arg, &winToScroll, &numToScroll);
  tuiDo ((TuiOpaqueFuncPtr) tui_vScroll,
	 LEFT_SCROLL,
	 winToScroll,
	 numToScroll);

  return;
}				/* _tuiScrollLeft_command */


/*
** _tuiScrollRight_command().
*/
static void
#ifdef __STDC__
_tuiScrollRight_command (
			  char *arg,
			  int fromTTY)
#else
_tuiScrollRight_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  int numToScroll;
  TuiWinInfoPtr winToScroll;

  _parseScrollingArgs (arg, &winToScroll, &numToScroll);
  tuiDo ((TuiOpaqueFuncPtr) tui_vScroll,
	 RIGHT_SCROLL,
	 winToScroll,
	 numToScroll);

  return;
}				/* _tuiScrollRight_command */


/*
** _tuiSetFocus().
**     Set focus to the window named by 'arg'
*/
static void
#ifdef __STDC__
_tuiSetFocus (
	       char *arg,
	       int fromTTY)
#else
_tuiSetFocus (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if (arg != (char *) NULL)
    {
      char *bufPtr = (char *) tuiStrDup (arg);
      int i;
      TuiWinInfoPtr winInfo = (TuiWinInfoPtr) NULL;

      for (i = 0; (i < strlen (bufPtr)); i++)
	bufPtr[i] = toupper (arg[i]);

      if (subsetCompare (bufPtr, "NEXT"))
	winInfo = tuiNextWin (tuiWinWithFocus ());
      else if (subsetCompare (bufPtr, "PREV"))
	winInfo = tuiPrevWin (tuiWinWithFocus ());
      else
	winInfo = partialWinByName (bufPtr);

      if (winInfo == (TuiWinInfoPtr) NULL || !winInfo->generic.isVisible)
	warning ("Invalid window specified. \n\
The window name specified must be valid and visible.\n");
      else
	{
	  tuiSetWinFocusTo (winInfo);
	  keypad (cmdWin->generic.handle, (winInfo != cmdWin));
	}

      if (dataWin->generic.isVisible)
	tuiRefreshDataWin ();
      tuiFree (bufPtr);
      printf_filtered ("Focus set to %s window.\n",
		       winName ((TuiGenWinInfoPtr) tuiWinWithFocus ()));
    }
  else
    warning ("Incorrect Number of Arguments.\n%s", FOCUS_USAGE);

  return;
}				/* _tuiSetFocus */


/*
** _tui_vSetFocus()
*/
static void
#ifdef __STDC__
_tui_vSetFocus (
		 va_list args)
#else
_tui_vSetFocus (args)
     va_list args;
#endif
{
  char *arg = va_arg (args, char *);
  int fromTTY = va_arg (args, int);

  _tuiSetFocus (arg, fromTTY);

  return;
}				/* tui_vSetFocus */


/*
** _tuiSetFocus_command()
*/
static void
#ifdef __STDC__
_tuiSetFocus_command (
		       char *arg,
		       int fromTTY)
#else
_tuiSetFocus_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vSetFocus, arg, fromTTY);

  return;
}				/* tui_SetFocus */


/*
** _tuiAllWindowsInfo().
*/
static void
#ifdef __STDC__
_tuiAllWindowsInfo (
		     char *arg,
		     int fromTTY)
#else
_tuiAllWindowsInfo (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  TuiWinType type;
  TuiWinInfoPtr winWithFocus = tuiWinWithFocus ();

  for (type = SRC_WIN; (type < MAX_MAJOR_WINDOWS); type++)
    if (winList[type]->generic.isVisible)
      {
	if (winWithFocus == winList[type])
	  printf_filtered ("        %s\t(%d lines)  <has focus>\n",
			   winName (&winList[type]->generic),
			   winList[type]->generic.height);
	else
	  printf_filtered ("        %s\t(%d lines)\n",
			   winName (&winList[type]->generic),
			   winList[type]->generic.height);
      }

  return;
}				/* _tuiAllWindowsInfo */


/*
** _tuiRefreshAll_command().
*/
static void
#ifdef __STDC__
_tuiRefreshAll_command (
			 char *arg,
			 int fromTTY)
#else
_tuiRefreshAll_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) tuiRefreshAll);
}


/*
** _tuiSetWinTabWidth_command().
**        Set the height of the specified window.
*/
static void
#ifdef __STDC__
_tuiSetTabWidth_command (
			  char *arg,
			  int fromTTY)
#else
_tuiSetTabWidth_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if (arg != (char *) NULL)
    {
      int ts;

      ts = atoi (arg);
      if (ts > 0)
	tuiSetDefaultTabLen (ts);
      else
	warning ("Tab widths greater than 0 must be specified.\n");
    }

  return;
}				/* _tuiSetTabWidth_command */


/*
** _tuiSetWinHeight().
**        Set the height of the specified window.
*/
static void
#ifdef __STDC__
_tuiSetWinHeight (
		   char *arg,
		   int fromTTY)
#else
_tuiSetWinHeight (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if (arg != (char *) NULL)
    {
      char *buf = tuiStrDup (arg);
      char *bufPtr = buf;
      char *wname = (char *) NULL;
      int newHeight, i;
      TuiWinInfoPtr winInfo;

      wname = bufPtr;
      bufPtr = strchr (bufPtr, ' ');
      if (bufPtr != (char *) NULL)
	{
	  *bufPtr = (char) 0;

	  /*
            ** Validate the window name
            */
	  for (i = 0; i < strlen (wname); i++)
	    wname[i] = toupper (wname[i]);
	  winInfo = partialWinByName (wname);

	  if (winInfo == (TuiWinInfoPtr) NULL || !winInfo->generic.isVisible)
	    warning ("Invalid window specified. \n\
The window name specified must be valid and visible.\n");
	  else
	    {
	      /* Process the size */
	      while (*(++bufPtr) == ' ')
		;

	      if (*bufPtr != (char) 0)
		{
		  int negate = FALSE;
		  int fixedSize = TRUE;
		  int inputNo;;

		  if (*bufPtr == '+' || *bufPtr == '-')
		    {
		      if (*bufPtr == '-')
			negate = TRUE;
		      fixedSize = FALSE;
		      bufPtr++;
		    }
		  inputNo = atoi (bufPtr);
		  if (inputNo > 0)
		    {
		      if (negate)
			inputNo *= (-1);
		      if (fixedSize)
			newHeight = inputNo;
		      else
			newHeight = winInfo->generic.height + inputNo;
		      /*
                        ** Now change the window's height, and adjust all
                        ** other windows around it
                        */
		      if (_tuiAdjustWinHeights (winInfo,
						newHeight) == TUI_FAILURE)
			warning ("Invalid window height specified.\n%s",
				 WIN_HEIGHT_USAGE);
		      else
			init_page_info ();
		    }
		  else
		    warning ("Invalid window height specified.\n%s",
			     WIN_HEIGHT_USAGE);
		}
	    }
	}
      else
	printf_filtered (WIN_HEIGHT_USAGE);

      if (buf != (char *) NULL)
	tuiFree (buf);
    }
  else
    printf_filtered (WIN_HEIGHT_USAGE);

  return;
}				/* _tuiSetWinHeight */


/*
** _tui_vSetWinHeight().
**        Set the height of the specified window, with va_list.
*/
static void
#ifdef __STDC__
_tui_vSetWinHeight (
		     va_list args)
#else
_tui_vSetWinHeight (args)
     va_list args;
#endif
{
  char *arg = va_arg (args, char *);
  int fromTTY = va_arg (args, int);

  _tuiSetWinHeight (arg, fromTTY);

  return;
}				/* _tui_vSetWinHeight */


/*
** _tuiSetWinHeight_command().
**        Set the height of the specified window, with va_list.
*/
static void
#ifdef __STDC__
_tuiSetWinHeight_command (
			   char *arg,
			   int fromTTY)
#else
_tuiSetWinHeight_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vSetWinHeight, arg, fromTTY);

  return;
}				/* _tuiSetWinHeight_command */


/*
** _tuiXDBsetWinHeight().
**        XDB Compatibility command for setting the window height.  This will
**        increase or decrease the command window by the specified amount.
*/
static void
#ifdef __STDC__
_tuiXDBsetWinHeight (
		      char *arg,
		      int fromTTY)
#else
_tuiXDBsetWinHeight (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if (arg != (char *) NULL)
    {
      int inputNo = atoi (arg);

      if (inputNo > 0)
	{			/* Add 1 for the locator */
	  int newHeight = termHeight () - (inputNo + 1);

	  if (!_newHeightOk (winList[CMD_WIN], newHeight) ||
	      _tuiAdjustWinHeights (winList[CMD_WIN],
				    newHeight) == TUI_FAILURE)
	    warning ("Invalid window height specified.\n%s",
		     XDBWIN_HEIGHT_USAGE);
	}
      else
	warning ("Invalid window height specified.\n%s",
		 XDBWIN_HEIGHT_USAGE);
    }
  else
    warning ("Invalid window height specified.\n%s", XDBWIN_HEIGHT_USAGE);

  return;
}				/* _tuiXDBsetWinHeight */


/*
** _tui_vXDBsetWinHeight().
**        Set the height of the specified window, with va_list.
*/
static void
#ifdef __STDC__
_tui_vXDBsetWinHeight (
			va_list args)
#else
_tui_vXDBsetWinHeight (args)
     va_list args;
#endif
{
  char *arg = va_arg (args, char *);
  int fromTTY = va_arg (args, int);

  _tuiXDBsetWinHeight (arg, fromTTY);

  return;
}				/* _tui_vXDBsetWinHeight */


/*
** _tuiSetWinHeight_command().
**        Set the height of the specified window, with va_list.
*/
static void
#ifdef __STDC__
_tuiXDBsetWinHeight_command (
			      char *arg,
			      int fromTTY)
#else
_tuiXDBsetWinHeight_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vXDBsetWinHeight, arg, fromTTY);

  return;
}				/* _tuiXDBsetWinHeight_command */


/*
** _tuiAdjustWinHeights().
**        Function to adjust all window heights around the primary
*/
static TuiStatus
#ifdef __STDC__
_tuiAdjustWinHeights (
		       TuiWinInfoPtr primaryWinInfo,
		       int newHeight)
#else
_tuiAdjustWinHeights (primaryWinInfo, newHeight)
     TuiWinInfoPtr primaryWinInfo;
     int newHeight;
#endif
{
  TuiStatus status = TUI_FAILURE;

  if (_newHeightOk (primaryWinInfo, newHeight))
    {
      status = TUI_SUCCESS;
      if (newHeight != primaryWinInfo->generic.height)
	{
	  int i, diff;
	  TuiWinInfoPtr winInfo;
	  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
	  TuiLayoutType curLayout = currentLayout ();

	  diff = (newHeight - primaryWinInfo->generic.height) * (-1);
	  if (curLayout == SRC_COMMAND || curLayout == DISASSEM_COMMAND)
	    {
	      TuiWinInfoPtr srcWinInfo;

	      _makeInvisibleAndSetNewHeight (primaryWinInfo, newHeight);
	      if (primaryWinInfo->generic.type == CMD_WIN)
		{
		  winInfo = (TuiWinInfoPtr) (sourceWindows ())->list[0];
		  srcWinInfo = winInfo;
		}
	      else
		{
		  winInfo = winList[CMD_WIN];
		  srcWinInfo = primaryWinInfo;
		}
	      _makeInvisibleAndSetNewHeight (winInfo,
					     winInfo->generic.height + diff);
	      cmdWin->generic.origin.y = locator->origin.y + 1;
	      _makeVisibleWithNewHeight (winInfo);
	      _makeVisibleWithNewHeight (primaryWinInfo);
	      if (srcWinInfo->generic.contentSize <= 0)
		tuiEraseSourceContent (srcWinInfo, EMPTY_SOURCE_PROMPT);
	    }
	  else
	    {
	      TuiWinInfoPtr firstWin, secondWin;

	      if (curLayout == SRC_DISASSEM_COMMAND)
		{
		  firstWin = srcWin;
		  secondWin = disassemWin;
		}
	      else
		{
		  firstWin = dataWin;
		  secondWin = (TuiWinInfoPtr) (sourceWindows ())->list[0];
		}
	      if (primaryWinInfo == cmdWin)
		{		/*
                    ** Split the change in height accross the 1st & 2nd windows
                    ** adjusting them as well.
                    */
		  int firstSplitDiff = diff / 2;	/* subtract the locator */
		  int secondSplitDiff = firstSplitDiff;

		  if (diff % 2)
		    {
		      if (firstWin->generic.height >
			  secondWin->generic.height)
			if (diff < 0)
			  firstSplitDiff--;
			else
			  firstSplitDiff++;
		      else
			{
			  if (diff < 0)
			    secondSplitDiff--;
			  else
			    secondSplitDiff++;
			}
		    }
		  /* make sure that the minimum hieghts are honored */
		  while ((firstWin->generic.height + firstSplitDiff) < 3)
		    {
		      firstSplitDiff++;
		      secondSplitDiff--;
		    }
		  while ((secondWin->generic.height + secondSplitDiff) < 3)
		    {
		      secondSplitDiff++;
		      firstSplitDiff--;
		    }
		  _makeInvisibleAndSetNewHeight (
						  firstWin,
				 firstWin->generic.height + firstSplitDiff);
		  secondWin->generic.origin.y = firstWin->generic.height - 1;
		  _makeInvisibleAndSetNewHeight (
		    secondWin, secondWin->generic.height + secondSplitDiff);
		  cmdWin->generic.origin.y = locator->origin.y + 1;
		  _makeInvisibleAndSetNewHeight (cmdWin, newHeight);
		}
	      else
		{
		  if ((cmdWin->generic.height + diff) < 1)
		    {		/*
                        ** If there is no way to increase the command window
                        ** take real estate from the 1st or 2nd window.
                        */
		      if ((cmdWin->generic.height + diff) < 1)
			{
			  int i;
			  for (i = cmdWin->generic.height + diff;
			       (i < 1); i++)
			    if (primaryWinInfo == firstWin)
			      secondWin->generic.height--;
			    else
			      firstWin->generic.height--;
			}
		    }
		  if (primaryWinInfo == firstWin)
		    _makeInvisibleAndSetNewHeight (firstWin, newHeight);
		  else
		    _makeInvisibleAndSetNewHeight (
						    firstWin,
						  firstWin->generic.height);
		  secondWin->generic.origin.y = firstWin->generic.height - 1;
		  if (primaryWinInfo == secondWin)
		    _makeInvisibleAndSetNewHeight (secondWin, newHeight);
		  else
		    _makeInvisibleAndSetNewHeight (
				      secondWin, secondWin->generic.height);
		  cmdWin->generic.origin.y = locator->origin.y + 1;
		  if ((cmdWin->generic.height + diff) < 1)
		    _makeInvisibleAndSetNewHeight (cmdWin, 1);
		  else
		    _makeInvisibleAndSetNewHeight (
				     cmdWin, cmdWin->generic.height + diff);
		}
	      _makeVisibleWithNewHeight (cmdWin);
	      _makeVisibleWithNewHeight (secondWin);
	      _makeVisibleWithNewHeight (firstWin);
	      if (firstWin->generic.contentSize <= 0)
		tuiEraseSourceContent (firstWin, EMPTY_SOURCE_PROMPT);
	      if (secondWin->generic.contentSize <= 0)
		tuiEraseSourceContent (secondWin, EMPTY_SOURCE_PROMPT);
	    }
	}
    }

  return status;
}				/* _tuiAdjustWinHeights */


/*
** _makeInvisibleAndSetNewHeight().
**        Function make the target window (and auxillary windows associated
**        with the targer) invisible, and set the new height and location.
*/
static void
#ifdef __STDC__
_makeInvisibleAndSetNewHeight (
				TuiWinInfoPtr winInfo,
				int height)
#else
_makeInvisibleAndSetNewHeight (winInfo, height)
     TuiWinInfoPtr winInfo;
     int height;
#endif
{
  int i;
  struct symtab *s;
  TuiGenWinInfoPtr genWinInfo;


  m_beInvisible (&winInfo->generic);
  winInfo->generic.height = height;
  if (height > 1)
    winInfo->generic.viewportHeight = height - 1;
  else
    winInfo->generic.viewportHeight = height;
  if (winInfo != cmdWin)
    winInfo->generic.viewportHeight--;

  /* Now deal with the auxillary windows associated with winInfo */
  switch (winInfo->generic.type)
    {
    case SRC_WIN:
    case DISASSEM_WIN:
      genWinInfo = winInfo->detail.sourceInfo.executionInfo;
      m_beInvisible (genWinInfo);
      genWinInfo->height = height;
      genWinInfo->origin.y = winInfo->generic.origin.y;
      if (height > 1)
	genWinInfo->viewportHeight = height - 1;
      else
	genWinInfo->viewportHeight = height;
      if (winInfo != cmdWin)
	genWinInfo->viewportHeight--;

      if (m_hasLocator (winInfo))
	{
	  genWinInfo = locatorWinInfoPtr ();
	  m_beInvisible (genWinInfo);
	  genWinInfo->origin.y = winInfo->generic.origin.y + height;
	}
      break;
    case DATA_WIN:
      /* delete all data item windows */
      for (i = 0; i < winInfo->generic.contentSize; i++)
	{
	  genWinInfo = (TuiGenWinInfoPtr) & ((TuiWinElementPtr)
		      winInfo->generic.content[i])->whichElement.dataWindow;
	  tuiDelwin (genWinInfo->handle);
	  genWinInfo->handle = (WINDOW *) NULL;
	}
      break;
    default:
      break;
    }

  return;
}				/* _makeInvisibleAndSetNewHeight */


/*
** _makeVisibleWithNewHeight().
**        Function to make the windows with new heights visible.
**        This means re-creating the windows' content since the window
**        had to be destroyed to be made invisible.
*/
static void
#ifdef __STDC__
_makeVisibleWithNewHeight (
			    TuiWinInfoPtr winInfo)
#else
_makeVisibleWithNewHeight (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  int i;
  struct symtab *s;

  m_beVisible (&winInfo->generic);
  checkAndDisplayHighlightIfNeeded (winInfo);
  switch (winInfo->generic.type)
    {
    case SRC_WIN:
    case DISASSEM_WIN:
      freeWinContent (winInfo->detail.sourceInfo.executionInfo);
      m_beVisible (winInfo->detail.sourceInfo.executionInfo);
      if (winInfo->generic.content != (OpaquePtr) NULL)
	{
	  TuiLineOrAddress lineOrAddr;

	  if (winInfo->generic.type == SRC_WIN)
	    lineOrAddr.lineNo =
	      winInfo->detail.sourceInfo.startLineOrAddr.lineNo;
	  else
	    lineOrAddr.addr =
	      winInfo->detail.sourceInfo.startLineOrAddr.addr;
	  freeWinContent (&winInfo->generic);
	  tuiUpdateSourceWindow (winInfo,
				 current_source_symtab,
				 ((winInfo->generic.type == SRC_WIN) ?
				  (Opaque) lineOrAddr.lineNo :
				  lineOrAddr.addr),
				 TRUE);
	}
      else if (selected_frame != (struct frame_info *) NULL)
	{
	  Opaque line = 0;
	  extern int current_source_line;

	  s = find_pc_symtab (selected_frame->pc);
	  if (winInfo->generic.type == SRC_WIN)
	    line = (Opaque) current_source_line;
	  else
	    line = (Opaque) find_line_pc (s, current_source_line);
	  tuiUpdateSourceWindow (winInfo, s, line, TRUE);
	}
      if (m_hasLocator (winInfo))
	{
	  m_beVisible (locatorWinInfoPtr ());
	  tuiClearLocatorDisplay ();
	  tuiShowLocatorContent ();
	}
      break;
    case DATA_WIN:
      tuiDisplayAllData ();
      break;
    case CMD_WIN:
      winInfo->detail.commandInfo.curLine = 0;
      winInfo->detail.commandInfo.curch = 0;
      wmove (winInfo->generic.handle,
	     winInfo->detail.commandInfo.curLine,
	     winInfo->detail.commandInfo.curch);
      break;
    default:
      break;
    }

  return;
}				/* _makeVisibleWithNewHeight */


static int
#ifdef __STDC__
_newHeightOk (
	       TuiWinInfoPtr primaryWinInfo,
	       int newHeight)
#else
_newHeightOk (primaryWinInfo, newHeight)
     TuiWinInfoPtr primaryWinInfo;
     int newHeight;
#endif
{
  int ok = (newHeight < termHeight ());

  if (ok)
    {
      int diff, curHeight;
      TuiLayoutType curLayout = currentLayout ();

      diff = (newHeight - primaryWinInfo->generic.height) * (-1);
      if (curLayout == SRC_COMMAND || curLayout == DISASSEM_COMMAND)
	{
	  ok = ((primaryWinInfo->generic.type == CMD_WIN &&
		 newHeight <= (termHeight () - 4) &&
		 newHeight >= MIN_CMD_WIN_HEIGHT) ||
		(primaryWinInfo->generic.type != CMD_WIN &&
		 newHeight <= (termHeight () - 2) &&
		 newHeight >= MIN_WIN_HEIGHT));
	  if (ok)
	    {			/* check the total height */
	      TuiWinInfoPtr winInfo;

	      if (primaryWinInfo == cmdWin)
		winInfo = (TuiWinInfoPtr) (sourceWindows ())->list[0];
	      else
		winInfo = cmdWin;
	      ok = ((newHeight +
		     (winInfo->generic.height + diff)) <= termHeight ());
	    }
	}
      else
	{
	  int curTotalHeight, totalHeight, minHeight;
	  TuiWinInfoPtr firstWin, secondWin;

	  if (curLayout == SRC_DISASSEM_COMMAND)
	    {
	      firstWin = srcWin;
	      secondWin = disassemWin;
	    }
	  else
	    {
	      firstWin = dataWin;
	      secondWin = (TuiWinInfoPtr) (sourceWindows ())->list[0];
	    }
	  /*
            ** We could simply add all the heights to obtain the same result
            ** but below is more explicit since we subtract 1 for the
            ** line that the first and second windows share, and add one
            ** for the locator.
            */
	  curTotalHeight =
	    (firstWin->generic.height + secondWin->generic.height - 1)
	    + cmdWin->generic.height + 1 /*locator*/ ;
	  if (primaryWinInfo == cmdWin)
	    {
	      /* locator included since first & second win share a line */
	      ok = ((firstWin->generic.height +
		     secondWin->generic.height + diff) >=
		    (MIN_WIN_HEIGHT * 2) &&
		    newHeight >= MIN_CMD_WIN_HEIGHT);
	      if (ok)
		{
		  totalHeight = newHeight + (firstWin->generic.height +
					  secondWin->generic.height + diff);
		  minHeight = MIN_CMD_WIN_HEIGHT;
		}
	    }
	  else
	    {
	      minHeight = MIN_WIN_HEIGHT;
	      /*
                ** First see if we can increase/decrease the command
                ** window.  And make sure that the command window is
                ** at least 1 line
                */
	      ok = ((cmdWin->generic.height + diff) > 0);
	      if (!ok)
		{		/*
                     ** Looks like we have to increase/decrease one of
                     ** the other windows
                     */
		  if (primaryWinInfo == firstWin)
		    ok = (secondWin->generic.height + diff) >= minHeight;
		  else
		    ok = (firstWin->generic.height + diff) >= minHeight;
		}
	      if (ok)
		{
		  if (primaryWinInfo == firstWin)
		    totalHeight = newHeight +
		      secondWin->generic.height +
		      cmdWin->generic.height + diff;
		  else
		    totalHeight = newHeight +
		      firstWin->generic.height +
		      cmdWin->generic.height + diff;
		}
	    }
	  /*
            ** Now make sure that the proposed total height doesn't exceed
            ** the old total height.
            */
	  if (ok)
	    ok = (newHeight >= minHeight && totalHeight <= curTotalHeight);
	}
    }

  return ok;
}				/* _newHeightOk */


/*
** _parseScrollingArgs().
*/
static void
#ifdef __STDC__
_parseScrollingArgs (
		      char *arg,
		      TuiWinInfoPtr * winToScroll,
		      int *numToScroll)
#else
_parseScrollingArgs (arg, winToScroll, numToScroll)
     char *arg;
     TuiWinInfoPtr *winToScroll;
     int *numToScroll;
#endif
{
  if (numToScroll)
    *numToScroll = 0;
  *winToScroll = tuiWinWithFocus ();

  /*
    ** First set up the default window to scroll, in case there is no
    ** window name arg
    */
  if (arg != (char *) NULL)
    {
      char *buf, *bufPtr;

      /* process the number of lines to scroll */
      buf = bufPtr = tuiStrDup (arg);
      if (isdigit (*bufPtr))
	{
	  char *numStr;

	  numStr = bufPtr;
	  bufPtr = strchr (bufPtr, ' ');
	  if (bufPtr != (char *) NULL)
	    {
	      *bufPtr = (char) 0;
	      if (numToScroll)
		*numToScroll = atoi (numStr);
	      bufPtr++;
	    }
	  else if (numToScroll)
	    *numToScroll = atoi (numStr);
	}

      /* process the window name if one is specified */
      if (bufPtr != (char *) NULL)
	{
	  char *wname;
	  int i;

	  if (*bufPtr == ' ')
	    while (*(++bufPtr) == ' ')
	      ;

	  if (*bufPtr != (char) 0)
	    wname = bufPtr;

	  /* Validate the window name */
	  for (i = 0; i < strlen (wname); i++)
	    wname[i] = toupper (wname[i]);
	  *winToScroll = partialWinByName (wname);

	  if (*winToScroll == (TuiWinInfoPtr) NULL ||
	      !(*winToScroll)->generic.isVisible)
	    warning ("Invalid window specified. \n\
The window name specified must be valid and visible.\n");
	  else if (*winToScroll == cmdWin)
	    *winToScroll = (TuiWinInfoPtr) (sourceWindows ())->list[0];
	}
      tuiFree (buf);
    }

  return;
}				/* _parseScrollingArgs */
