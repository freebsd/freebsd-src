/*
** tuiDataWin.c
**   This module contains functions to support the data/register window display.
*/


#include "defs.h"
#include "tui.h"
#include "tuiData.h"
#include "tuiRegs.h"


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/



/*****************************************
** PUBLIC FUNCTIONS                        **
******************************************/


/*
** tuiFirstDataItemDisplayed()
**    Answer the index first element displayed.
**    If none are displayed, then return (-1).
*/
int
#ifdef __STDC__
tuiFirstDataItemDisplayed (void)
#else
tuiFirstDataItemDisplayed ()
#endif
{
  int elementNo = (-1);
  int i;

  for (i = 0; (i < dataWin->generic.contentSize && elementNo < 0); i++)
    {
      TuiGenWinInfoPtr dataItemWin;

      dataItemWin = &((TuiWinContent)
		      dataWin->generic.content)[i]->whichElement.dataWindow;
      if (dataItemWin->handle != (WINDOW *) NULL && dataItemWin->isVisible)
	elementNo = i;
    }

  return elementNo;
}				/* tuiFirstDataItemDisplayed */


/*
** tuiFirstDataElementNoInLine()
**        Answer the index of the first element in lineNo.  If lineNo is
**        past the data area (-1) is returned.
*/
int
#ifdef __STDC__
tuiFirstDataElementNoInLine (
			      int lineNo)
#else
tuiFirstDataElementNoInLine (lineNo)
     int lineNo;
#endif
{
  int firstElementNo = (-1);

  /*
    ** First see if there is a register on lineNo, and if so, set the
    ** first element number
    */
  if ((firstElementNo = tuiFirstRegElementNoInLine (lineNo)) == -1)
    {				/*
      ** Looking at the general data, the 1st element on lineNo
      */
    }

  return firstElementNo;
}				/* tuiFirstDataElementNoInLine */


/*
** tuiDeleteDataContentWindows()
**        Function to delete all the item windows in the data window.
**        This is usually done when the data window is scrolled.
*/
void
#ifdef __STDC__
tuiDeleteDataContentWindows (void)
#else
tuiDeleteDataContentWindows ()
#endif
{
  int i;
  TuiGenWinInfoPtr dataItemWinPtr;

  for (i = 0; (i < dataWin->generic.contentSize); i++)
    {
      dataItemWinPtr = &((TuiWinContent)
		      dataWin->generic.content)[i]->whichElement.dataWindow;
      tuiDelwin (dataItemWinPtr->handle);
      dataItemWinPtr->handle = (WINDOW *) NULL;
      dataItemWinPtr->isVisible = FALSE;
    }

  return;
}				/* tuiDeleteDataContentWindows */


void
#ifdef __STDC__
tuiEraseDataContent (
		      char *prompt)
#else
tuiEraseDataContent (prompt)
     char *prompt;
#endif
{
  werase (dataWin->generic.handle);
  checkAndDisplayHighlightIfNeeded (dataWin);
  if (prompt != (char *) NULL)
    {
      int halfWidth = (dataWin->generic.width - 2) / 2;
      int xPos;

      if (strlen (prompt) >= halfWidth)
	xPos = 1;
      else
	xPos = halfWidth - strlen (prompt);
      mvwaddstr (dataWin->generic.handle,
		 (dataWin->generic.height / 2),
		 xPos,
		 prompt);
    }
  wrefresh (dataWin->generic.handle);

  return;
}				/* tuiEraseDataContent */


/*
** tuiDisplayAllData().
**        This function displays the data that is in the data window's
**        content.  It does not set the content.
*/
void
#ifdef __STDC__
tuiDisplayAllData (void)
#else
tuiDisplayAllData ()
#endif
{
  if (dataWin->generic.contentSize <= 0)
    tuiEraseDataContent (NO_DATA_STRING);
  else
    {
      tuiEraseDataContent ((char *) NULL);
      tuiDeleteDataContentWindows ();
      checkAndDisplayHighlightIfNeeded (dataWin);
      tuiDisplayRegistersFrom (0);
      /*
        ** Then display the other data
        */
      if (dataWin->detail.dataDisplayInfo.dataContent !=
	  (TuiWinContent) NULL &&
	  dataWin->detail.dataDisplayInfo.dataContentCount > 0)
	{
	}
    }
  return;
}				/* tuiDisplayAllData */


/*
** tuiDisplayDataFromLine()
**        Function to display the data starting at line, lineNo, in the
**        data window.
*/
void
#ifdef __STDC__
tuiDisplayDataFromLine (
			 int lineNo)
#else
tuiDisplayDataFromLine (lineNo)
     int lineNo;
#endif
{
  int _lineNo = lineNo;

  if (lineNo < 0)
    _lineNo = 0;

  checkAndDisplayHighlightIfNeeded (dataWin);

  /* there is no general data, force regs to display (if there are any) */
  if (dataWin->detail.dataDisplayInfo.dataContentCount <= 0)
    tuiDisplayRegistersFromLine (_lineNo, TRUE);
  else
    {
      int elementNo, startLineNo;
      int regsLastLine = tuiLastRegsLineNo ();


      /* display regs if we can */
      if (tuiDisplayRegistersFromLine (_lineNo, FALSE) < 0)
	{			/*
            ** _lineNo is past the regs display, so calc where the
            ** start data element is
            */
	  if (regsLastLine < _lineNo)
	    {			/* figure out how many lines each element is to obtain
                    the start elementNo */
	    }
	}
      else
	{			/*
           ** calculate the starting element of the data display, given
           ** regsLastLine and how many lines each element is, up to
           ** _lineNo
           */
	}
      /* Now display the data , starting at elementNo */
    }

  return;
}				/* tuiDisplayDataFromLine */


/*
** tuiDisplayDataFrom()
**        Display data starting at element elementNo
*/
void
#ifdef __STDC__
tuiDisplayDataFrom (
		     int elementNo,
		     int reuseWindows)
#else
tuiDisplayDataFrom (elementNo, reuseWindows)
     int elementNo;
     int reuseWindows;
#endif
{
  int firstLine = (-1);

  if (elementNo < dataWin->detail.dataDisplayInfo.regsContentCount)
    firstLine = tuiLineFromRegElementNo (elementNo);
  else
    {				/* calculate the firstLine from the element number */
    }

  if (firstLine >= 0)
    {
      tuiEraseDataContent ((char *) NULL);
      if (!reuseWindows)
	tuiDeleteDataContentWindows ();
      tuiDisplayDataFromLine (firstLine);
    }

  return;
}				/* tuiDisplayDataFrom */


/*
** tuiRefreshDataWin()
**        Function to redisplay the contents of the data window.
*/
void
#ifdef __STDC__
tuiRefreshDataWin (void)
#else
tuiRefreshDataWin ()
#endif
{
  tuiEraseDataContent ((char *) NULL);
  if (dataWin->generic.contentSize > 0)
    {
      int firstElement = tuiFirstDataItemDisplayed ();

      if (firstElement >= 0)	/* re-use existing windows */
	tuiDisplayDataFrom (firstElement, TRUE);
    }

  return;
}				/* tuiRefreshDataWin */


/*
** tuiCheckDataValues().
**        Function to check the data values and hilite any that have changed
*/
void
#ifdef __STDC__
tuiCheckDataValues (
		     struct frame_info *frame)
#else
tuiCheckDataValues (frame)
     struct frame_info *frame;
#endif
{
  tuiCheckRegisterValues (frame);

  /* Now check any other data values that there are */
  if (m_winPtrNotNull (dataWin) && dataWin->generic.isVisible)
    {
      int i;

      for (i = 0; dataWin->detail.dataDisplayInfo.dataContentCount; i++)
	{
#ifdef LATER
	  TuiDataElementPtr dataElementPtr;
	  TuiGenWinInfoPtr dataItemWinPtr;
	  Opaque newValue;

	  dataItemPtr = &dataWin->detail.dataDisplayInfo.
	    dataContent[i]->whichElement.dataWindow;
	  dataElementPtr = &((TuiWinContent)
			     dataItemWinPtr->content)[0]->whichElement.data;
	  if value
	    has changed (dataElementPtr, frame, &newValue)
	    {
	      dataElementPtr->value = newValue;
	      update the display with the new value, hiliting it.
	    }
#endif
	}
    }
}				/* tuiCheckDataValues */


/*
** tui_vCheckDataValues().
**        Function to check the data values and hilite any that have
**        changed with args in a va_list
*/
void
#ifdef __STDC__
tui_vCheckDataValues (
		       va_list args)
#else
tui_vCheckDataValues (args)
     va_list args;
#endif
{
  struct frame_info *frame = va_arg (args, struct frame_info *);

  tuiCheckDataValues (frame);

  return;
}				/* tui_vCheckDataValues */


/*
** tuiVerticalDataScroll()
**        Scroll the data window vertically forward or backward.
*/
void
#ifdef __STDC__
tuiVerticalDataScroll (
			TuiScrollDirection scrollDirection,
			int numToScroll)
#else
tuiVerticalDataScroll (scrollDirection, numToScroll)
     TuiScrollDirection scrollDirection;
     int numToScroll;
#endif
{
  int firstElementNo;
  int firstLine = (-1);

  firstElementNo = tuiFirstDataItemDisplayed ();
  if (firstElementNo < dataWin->detail.dataDisplayInfo.regsContentCount)
    firstLine = tuiLineFromRegElementNo (firstElementNo);
  else
    {				/* calculate the first line from the element number which is in
        ** the general data content
        */
    }

  if (firstLine >= 0)
    {
      int lastElementNo, lastLine;

      if (scrollDirection == FORWARD_SCROLL)
	firstLine += numToScroll;
      else
	firstLine -= numToScroll;
      tuiEraseDataContent ((char *) NULL);
      tuiDeleteDataContentWindows ();
      tuiDisplayDataFromLine (firstLine);
    }

  return;
}				/* tuiVerticalDataScroll */


/*****************************************
** STATIC LOCAL FUNCTIONS               **
******************************************/
