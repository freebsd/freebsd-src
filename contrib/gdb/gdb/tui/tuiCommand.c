/*
** tuiCommand.c
**     This module contains functions specific to command window processing.
*/


#include "defs.h"
#include "tui.h"
#include "tuiData.h"
#include "tuiWin.h"
#include "tuiIO.h"


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/



/*****************************************
** PUBLIC FUNCTIONS                        **
******************************************/

/*
** tuiDispatchCtrlChar().
**        Dispatch the correct tui function based upon the control character.
*/
unsigned int
#ifdef __STDC__
tuiDispatchCtrlChar (
		      unsigned int ch)
#else
tuiDispatchCtrlChar (ch)
     unsigned int ch;
#endif
{
  TuiWinInfoPtr winInfo = tuiWinWithFocus ();

  /*
    ** If the command window has the logical focus, or no-one does
    ** assume it is the command window; in this case, pass the
    ** character on through and do nothing here.
    */
  if (winInfo == (TuiWinInfoPtr) NULL || winInfo == cmdWin)
    return ch;
  else
    {
      unsigned int c = 0, chCopy = ch;
      register int i;
      char *term;

      /* If this is an xterm, page next/prev keys aren't returned
        ** by keypad as a single char, so we must handle them here.
        ** Seems like a bug in the curses library?
        */
      term = (char *) getenv ("TERM");
      for (i = 0; (term && term[i]); i++)
	term[i] = toupper (term[i]);
      if ((strcmp (term, "XTERM") == 0) && m_isStartSequence (ch))
	{
	  unsigned int pageCh = 0, tmpChar;

	  tmpChar = 0;
	  while (!m_isEndSequence (tmpChar))
	    {
	      tmpChar = (int) wgetch (cmdWin->generic.handle);
	      if (!tmpChar)
		break;
	      if (tmpChar == 53)
		pageCh = KEY_PPAGE;
	      else if (tmpChar == 54)
		pageCh = KEY_NPAGE;
	    }
	  chCopy = pageCh;
	}

      switch (chCopy)
	{
	case KEY_NPAGE:
	  tuiScrollForward (winInfo, 0);
	  break;
	case KEY_PPAGE:
	  tuiScrollBackward (winInfo, 0);
	  break;
	case KEY_DOWN:
	case KEY_SF:
	  tuiScrollForward (winInfo, 1);
	  break;
	case KEY_UP:
	case KEY_SR:
	  tuiScrollBackward (winInfo, 1);
	  break;
	case KEY_RIGHT:
	  tuiScrollLeft (winInfo, 1);
	  break;
	case KEY_LEFT:
	  tuiScrollRight (winInfo, 1);
	  break;
	case '\f':
	  tuiRefreshAll ();
	  break;
	default:
	  c = chCopy;
	  break;
	}
      return c;
    }
}				/* tuiDispatchCtrlChar */


/*
** tuiIncrCommandCharCountBy()
**     Increment the current character count in the command window,
**     checking for overflow.  Returns the new value of the char count.
*/
int
#ifdef __STDC__
tuiIncrCommandCharCountBy (
			    int count)
#else
tuiIncrCommandCharCountBy (count)
     int count;
#endif
{
  if (tui_version)
    {
      if ((count + cmdWin->detail.commandInfo.curch) >= cmdWin->generic.width)
	cmdWin->detail.commandInfo.curch =
	  (count + cmdWin->detail.commandInfo.curch) - cmdWin->generic.width;
      else
	cmdWin->detail.commandInfo.curch += count;
    }

  return cmdWin->detail.commandInfo.curch;
}				/* tuiIncrCommandCharCountBy */


/*
** tuiDecrCommandCharCountBy()
**     Decrement the current character count in the command window,
**     checking for overflow.  Returns the new value of the char count.
*/
int
#ifdef __STDC__
tuiDecrCommandCharCountBy (
			    int count)
#else
tuiDecrCommandCharCountBy (count)
     int count;
#endif
{
  if (tui_version)
    {
      if ((cmdWin->detail.commandInfo.curch - count) < 0)
	cmdWin->detail.commandInfo.curch =
	  cmdWin->generic.width + (cmdWin->detail.commandInfo.curch - count);
      else
	cmdWin->detail.commandInfo.curch -= count;
    }

  return cmdWin->detail.commandInfo.curch;
}				/* tuiDecrCommandCharCountBy */


/*
** tuiSetCommandCharCountTo()
**     Set the character count to count.
*/
int
#ifdef __STDC__
tuiSetCommandCharCountTo (
			   int count)
#else
tuiSetCommandCharCountTo (count)
     int count;
#endif
{
  if (tui_version)
    {
      if (count > cmdWin->generic.width - 1)
	{
	  cmdWin->detail.commandInfo.curch = 0;
	  tuiIncrCommandCharCountBy (count);
	}
      else
	cmdWin->detail.commandInfo.curch -= count;
    }

  return cmdWin->detail.commandInfo.curch;
}				/* tuiSetCommandCharCountTo */



/*
** tuiClearCommandCharCount()
**     Clear the character count to count.
*/
int
#ifdef __STDC__
tuiClearCommandCharCount (void)
#else
tuiClearCommandCharCount ()
#endif
{
  if (tui_version)
    cmdWin->detail.commandInfo.curch = 0;

  return cmdWin->detail.commandInfo.curch;
}				/* tuiClearCommandCharCount */



/*****************************************
** STATIC LOCAL FUNCTIONS                 **
******************************************/
