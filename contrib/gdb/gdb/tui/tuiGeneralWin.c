/*
** TuiGeneralWin.c
** This module supports general window behavior
*/

#include <curses.h>
#include "defs.h"
#include "tui.h"
#include "tuiData.h"
#include "tuiGeneralWin.h"


/*
** local support functions
*/
static void _winResize PARAMS ((void));


/***********************
** PUBLIC FUNCTIONS
***********************/
/*
** tuiRefreshWin()
**        Refresh the window
*/
void
#ifdef __STDC__
tuiRefreshWin (
		TuiGenWinInfoPtr winInfo)
#else
tuiRefreshWin (winInfo)
     TuiGenWinInfoPtr winInfo;
#endif
{
  if (winInfo->type == DATA_WIN && winInfo->contentSize > 0)
    {
      int i;

      for (i = 0; (i < winInfo->contentSize); i++)
	{
	  TuiGenWinInfoPtr dataItemWinPtr;

	  dataItemWinPtr = &((TuiWinContent)
			     winInfo->content)[i]->whichElement.dataWindow;
	  if (m_genWinPtrNotNull (dataItemWinPtr) &&
	      dataItemWinPtr->handle != (WINDOW *) NULL)
	    wrefresh (dataItemWinPtr->handle);
	}
    }
  else if (winInfo->type == CMD_WIN)
    {
      /* Do nothing */
    }
  else
    {
      if (winInfo->handle != (WINDOW *) NULL)
	wrefresh (winInfo->handle);
    }

  return;
}				/* tuiRefreshWin */


/*
** tuiDelwin()
**        Function to delete the curses window, checking for null
*/
void
#ifdef __STDC__
tuiDelwin (
	    WINDOW * window)
#else
tuiDelwin (window)
     WINDOW *window;
#endif
{
  if (window != (WINDOW *) NULL)
    delwin (window);

  return;
}				/* tuiDelwin */


/*
** boxWin().
*/
void
#ifdef __STDC__
boxWin (
	 TuiGenWinInfoPtr winInfo,
	 int highlightFlag)
#else
boxWin (winInfo, highlightFlag)
     TuiGenWinInfoPtr winInfo;
     int highlightFlag;
#endif
{
  if (m_genWinPtrNotNull (winInfo) && winInfo->handle != (WINDOW *) NULL)
    {
      if (highlightFlag == HILITE)
	box (winInfo->handle, '|', '-');
      else
	{
/*            wattron(winInfo->handle, A_DIM);*/
	  box (winInfo->handle, ':', '.');
/*            wattroff(winInfo->handle, A_DIM);*/
	}
    }

  return;
}				/* boxWin */


/*
** unhighlightWin().
*/
void
#ifdef __STDC__
unhighlightWin (
		 TuiWinInfoPtr winInfo)
#else
unhighlightWin (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (m_winPtrNotNull (winInfo) && winInfo->generic.handle != (WINDOW *) NULL)
    {
      boxWin ((TuiGenWinInfoPtr) winInfo, NO_HILITE);
      wrefresh (winInfo->generic.handle);
      m_setWinHighlightOff (winInfo);
    }
}				/* unhighlightWin */


/*
** highlightWin().
*/
void
#ifdef __STDC__
highlightWin (
	       TuiWinInfoPtr winInfo)
#else
highlightWin (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (m_winPtrNotNull (winInfo) &&
      winInfo->canHighlight && winInfo->generic.handle != (WINDOW *) NULL)
    {
      boxWin ((TuiGenWinInfoPtr) winInfo, HILITE);
      wrefresh (winInfo->generic.handle);
      m_setWinHighlightOn (winInfo);
    }
}				/* highlightWin */


/*
** checkAndDisplayHighlightIfNecessay
*/
void
#ifdef __STDC__
checkAndDisplayHighlightIfNeeded (
				   TuiWinInfoPtr winInfo)
#else
checkAndDisplayHighlightIfNeeded (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (m_winPtrNotNull (winInfo) && winInfo->generic.type != CMD_WIN)
    {
      if (winInfo->isHighlighted)
	highlightWin (winInfo);
      else
	unhighlightWin (winInfo);

    }
  return;
}				/* checkAndDisplayHighlightIfNeeded */


/*
** makeWindow().
*/
void
#ifdef __STDC__
makeWindow (
	     TuiGenWinInfoPtr winInfo,
	     int boxIt)
#else
makeWindow (winInfo, boxIt)
     TuiGenWinInfoPtr winInfo;
     int boxIt;
#endif
{
  WINDOW *handle;

  handle = newwin (winInfo->height,
		   winInfo->width,
		   winInfo->origin.y,
		   winInfo->origin.x);
  winInfo->handle = handle;
  if (handle != (WINDOW *) NULL)
    {
      if (boxIt == BOX_WINDOW)
	boxWin (winInfo, NO_HILITE);
      winInfo->isVisible = TRUE;
      scrollok (handle, TRUE);
      tuiRefreshWin (winInfo);

#ifndef FOR_TEST
      if (			/*!m_WinIsAuxillary(winInfo->type) && */
	   (winInfo->type != CMD_WIN) &&
	   (winInfo->content == (OpaquePtr) NULL))
	{
	  mvwaddstr (handle, 1, 1, winName (winInfo));
	  tuiRefreshWin (winInfo);
	}
#endif /*FOR_TEST*/
    }

  return;
}				/* makeWindow */


/*
** tuiClearWin().
**        Clear the window of all contents without calling wclear.
*/
void
#ifdef __STDC__
tuiClearWin (
	      TuiGenWinInfoPtr winInfo)
#else
tuiClearWin (winInfo)
     TuiGenWinInfoPtr winInfo;
#endif
{
  if (m_genWinPtrNotNull (winInfo) && winInfo->handle != (WINDOW *) NULL)
    {
      int curRow, curCol;

      for (curRow = 0; (curRow < winInfo->height); curRow++)
	for (curCol = 0; (curCol < winInfo->width); curCol++)
	  mvwaddch (winInfo->handle, curRow, curCol, ' ');

      tuiRefreshWin (winInfo);
    }

  return;
}				/* tuiClearWin */


/*
** makeVisible().
**        We can't really make windows visible, or invisible.  So we
**        have to delete the entire window when making it visible,
**        and create it again when making it visible.
*/
void
#ifdef __STDC__
makeVisible (
	      TuiGenWinInfoPtr winInfo,
	      int visible)
#else
makeVisible (winInfo, visible)
     TuiGenWinInfoPtr winInfo;
     int visible;
#endif
{
  /* Don't tear down/recreate command window */
  if (winInfo->type == CMD_WIN)
    return;

  if (visible)
    {
      if (!winInfo->isVisible)
	{
	  makeWindow (
		       winInfo,
	   (winInfo->type != CMD_WIN && !m_winIsAuxillary (winInfo->type)));
	  winInfo->isVisible = TRUE;
	}
      tuiRefreshWin (winInfo);
    }
  else if (!visible &&
	   winInfo->isVisible && winInfo->handle != (WINDOW *) NULL)
    {
      winInfo->isVisible = FALSE;
      tuiClearWin (winInfo);
      tuiDelwin (winInfo->handle);
      winInfo->handle = (WINDOW *) NULL;
    }

  return;
}				/* makeVisible */


/*
** makeAllVisible().
**        Makes all windows invisible (except the command and locator windows)
*/
void
#ifdef __STDC__
makeAllVisible (
		 int visible)
#else
makeAllVisible (visible)
     int visible;
#endif
{
  int i;

  for (i = 0; i < MAX_MAJOR_WINDOWS; i++)
    {
      if (m_winPtrNotNull (winList[i]) &&
	  ((winList[i])->generic.type) != CMD_WIN)
	{
	  if (m_winIsSourceType ((winList[i])->generic.type))
	    makeVisible ((winList[i])->detail.sourceInfo.executionInfo,
			 visible);
	  makeVisible ((TuiGenWinInfoPtr) winList[i], visible);
	}
    }

  return;
}				/* makeAllVisible */


/*
** scrollWinForward
*/
void
#ifdef __STDC__
scrollWinForward (
		   TuiGenWinInfoPtr winInfo,
		   int numLines)
#else
scrollWinForward (winInfo, numLines)
     TuiGenWinInfoPtr winInfo;
     int numLines;
#endif
{
  if (winInfo->content != (OpaquePtr) NULL &&
      winInfo->lastVisibleLine < winInfo->contentSize - 1)
    {
      int i, firstLine, newLastLine;

      firstLine = winInfo->lastVisibleLine - winInfo->viewportHeight + 1;
      if (winInfo->lastVisibleLine + numLines > winInfo->contentSize)
	newLastLine = winInfo->contentSize - 1;
      else
	newLastLine = winInfo->lastVisibleLine + numLines - 1;

      for (i = (newLastLine - winInfo->viewportHeight);
	   (i <= newLastLine); i++)
	{
	  TuiWinElementPtr line;
	  int lineHeight;

	  line = (TuiWinElementPtr) winInfo->content[i];
	  if (line->highlight)
	    wstandout (winInfo->handle);
	  mvwaddstr (winInfo->handle,
		     i - (newLastLine - winInfo->viewportHeight),
		     1,
		     displayableWinContentOf (winInfo, line));
	  if (line->highlight)
	    wstandend (winInfo->handle);
	  lineHeight = winElementHeight (winInfo, line);
	  newLastLine += (lineHeight - 1);
	}
      winInfo->lastVisibleLine = newLastLine;
    }

  return;
}				/* scrollWinForward */


/*
** scrollWinBackward
*/
void
#ifdef __STDC__
scrollWinBackward (
		    TuiGenWinInfoPtr winInfo,
		    int numLines)
#else
scrollWinBackward (winInfo, numLines)
     TuiGenWinInfoPtr winInfo;
     int numLines;
#endif
{
  if (winInfo->content != (OpaquePtr) NULL &&
      (winInfo->lastVisibleLine - winInfo->viewportHeight) > 0)
    {
      int i, newLastLine, firstLine;

      firstLine = winInfo->lastVisibleLine - winInfo->viewportHeight + 1;
      if ((firstLine - numLines) < 0)
	newLastLine = winInfo->viewportHeight - 1;
      else
	newLastLine = winInfo->lastVisibleLine - numLines + 1;

      for (i = newLastLine - winInfo->viewportHeight; (i <= newLastLine); i++)
	{
	  TuiWinElementPtr line;
	  int lineHeight;

	  line = (TuiWinElementPtr) winInfo->content[i];
	  if (line->highlight)
	    wstandout (winInfo->handle);
	  mvwaddstr (winInfo->handle,
		     i - (newLastLine - winInfo->viewportHeight),
		     1,
		     displayableWinContentOf (winInfo, line));
	  if (line->highlight)
	    wstandend (winInfo->handle);
	  lineHeight = winElementHeight (winInfo, line);
	  newLastLine += (lineHeight - 1);
	}
      winInfo->lastVisibleLine = newLastLine;
    }

  return;
}				/* scrollWinBackward */


/*
** refreshAll().
**        Function to refresh all the windows currently displayed
*/
void
#ifdef __STDC__
refreshAll (
	     TuiWinInfoPtr * list)
#else
refreshAll (list)
     TuiWinInfoPtr *list;
#endif
{
  TuiWinType type;
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();

  for (type = SRC_WIN; (type < MAX_MAJOR_WINDOWS); type++)
    {
      if (list[type]->generic.isVisible)
	{
	  if (type == SRC_WIN || type == DISASSEM_WIN)
	    {
	      touchwin (list[type]->detail.sourceInfo.executionInfo->handle);
	      tuiRefreshWin (list[type]->detail.sourceInfo.executionInfo);
	    }
	  touchwin (list[type]->generic.handle);
	  tuiRefreshWin (&list[type]->generic);
	}
    }
  if (locator->isVisible)
    {
      touchwin (locator->handle);
      tuiRefreshWin (locator);
    }

  return;
}				/* refreshAll */


/*********************************
** Local Static Functions
*********************************/
