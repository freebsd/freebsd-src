/*
** This module contains functions for displaying the locator information in the locator window.
*/

#include "defs.h"
#include "symtab.h"
#include "breakpoint.h"
#include "frame.h"

#include "tui.h"
#include "tuiData.h"
#include "tuiStack.h"
#include "tuiSourceWin.h"


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/

static char *_getFuncNameFromFrame PARAMS ((struct frame_info *));
static void _tuiUpdateLocation_command PARAMS ((char *, int));



/*****************************************
** PUBLIC FUNCTION                        **
******************************************/

/*
** tuiClearLocatorDisplay()
*/
void
#ifdef __STDC__
tuiClearLocatorDisplay (void)
#else
tuiClearLocatorDisplay ()
#endif
{
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
  int i;

  if (locator->handle != (WINDOW *) NULL)
    {
      /* No need to werase, since writing a line of
         * blanks which we do below, is equivalent.
         */
      /* werase(locator->handle); */
      wmove (locator->handle, 0, 0);
      wstandout (locator->handle);
      for (i = 0; i < locator->width; i++)
	waddch (locator->handle, ' ');
      wstandend (locator->handle);
      tuiRefreshWin (locator);
      wmove (locator->handle, 0, 0);
      locator->contentInUse = FALSE;
    }

  return;
}				/* tuiClearLocatorDisplay */


/*
** tuiShowLocatorContent()
*/
void
#ifdef __STDC__
tuiShowLocatorContent (void)
#else
tuiShowLocatorContent ()
#endif
{
  char *string;
  TuiGenWinInfoPtr locator;

  locator = locatorWinInfoPtr ();

  if (m_genWinPtrNotNull (locator) && locator->handle != (WINDOW *) NULL)
    {
      string = displayableWinContentAt (locator, 0);
      if (string != (char *) NULL)
	{
	  wmove (locator->handle, 0, 0);
	  wstandout (locator->handle);
	  waddstr (locator->handle, string);
	  wstandend (locator->handle);
	  tuiRefreshWin (locator);
	  wmove (locator->handle, 0, 0);
	  if (string != nullStr ())
	    tuiFree (string);
	  locator->contentInUse = TRUE;
	}
    }

  return;
}				/* tuiShowLocatorContent */


/*
** tuiSetLocatorInfo().
**        Function to update the locator, with the provided arguments.
*/
void
#ifdef __STDC__
tuiSetLocatorInfo (
		    char *fname,
		    char *procname,
		    int lineNo,
		    Opaque addr,
		    TuiLocatorElementPtr element)
#else
tuiSetLocatorInfo (fname, procname, lineNo, addr, element)
     char *fname;
     char *procname;
     int lineNo;
     Opaque addr;
     TuiLocatorElementPtr element;
#endif
{
#ifdef COMMENT
  /* first free the old info */
  if (element->fileName)
    tuiFree (element->fileName);
  if (element->procName)
    tuiFree (element->procName);

  if (fname == (char *) NULL)
    element->fileName = fname;
  else
    element->fileName = tuiStrDup (fname);
  if (procname == (char *) NULL)
    element->procName = procname;
  else
    element->procName = tuiStrDup (procname);
#else
  element->fileName[0] = (char) 0;
  element->procName[0] = (char) 0;
  strcat_to_buf (element->fileName, MAX_LOCATOR_ELEMENT_LEN, fname);
  strcat_to_buf (element->procName, MAX_LOCATOR_ELEMENT_LEN, procname);
#endif
  element->lineNo = lineNo;
  element->addr = (Opaque) addr;

  return;
}				/* tuiSetLocatorInfo */


/*
** tuiUpdateLocatorFilename().
**        Update only the filename portion of the locator.
*/
void
#ifdef __STDC__
tuiUpdateLocatorFilename (
			   char *fileName)
#else
tuiUpdateLocatorFilename (fileName)
     char *fileName;
#endif
{
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();

  if (locator->content[0] == (Opaque) NULL)
    tuiSetLocatorContent ((struct frame_info *) NULL);
  ((TuiWinElementPtr) locator->content[0])->whichElement.locator.fileName[0] = (char) 0;
  strcat_to_buf (((TuiWinElementPtr) locator->content[0])->whichElement.locator.fileName,
		 MAX_LOCATOR_ELEMENT_LEN,
		 fileName);

  tuiShowLocatorContent ();

  return;
}				/* tuiUpdateLocatorFilename */


/*
** tui_vUpdateLocatorFilename().
**        Update only the filename portion of the locator with args in a va_list.
*/
void
#ifdef __STDC__
tui_vUpdateLocatorFilename (
			     va_list args)
#else
tui_vUpdateLocatorFilename (args)
     va_list args;
#endif
{
  char *fileName;

  fileName = va_arg (args, char *);
  tuiUpdateLocatorFilename (fileName);

  return;
}				/* tui_vUpdateLocatorFilename */


/*
** tuiSwitchFilename().
**   Update the filename portion of the locator. Clear the other info in locator.
** (elz)
*/
void
#ifdef __STDC__
tuiSwitchFilename (
		    char *fileName)
#else
tuiSwitchFilename (fileName)
     char *fileName;
#endif
{
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();

  if (locator->content[0] == (Opaque) NULL)
    tuiSetLocatorContent ((struct frame_info *) NULL);
  ((TuiWinElementPtr) locator->content[0])->whichElement.locator.fileName[0] = (char) 0;

  tuiSetLocatorInfo (fileName,
		     (char *) NULL,
		     0,
		     (Opaque) NULL,
	   &((TuiWinElementPtr) locator->content[0])->whichElement.locator);

  tuiShowLocatorContent ();

  return;
}				/* tuiSwitchFilename */


/*
** tuiGetLocatorFilename().
**   Get the filename portion of the locator.
** (elz)
*/
void
#ifdef __STDC__
tuiGetLocatorFilename (
			TuiGenWinInfoPtr locator,
			char **filename)
#else
tuiGetLocatorFilename (locator, filename)
     TuiGenWinInfoPtr locator;
     char **filename;
#endif
{

  /* the current filename could be non known, in which case the xmalloc would
     allocate no memory, because the length would be 0 */
  if (((TuiWinElementPtr) locator->content[0])->whichElement.locator.fileName)
    {
      int name_length =
      strlen (((TuiWinElementPtr) locator->content[0])->whichElement.locator.fileName);

      (*filename) = (char *) xmalloc (name_length + 1);
      strcpy ((*filename),
	      ((TuiWinElementPtr) locator->content[0])->whichElement.locator.fileName);
    }

  return;
}				/* tuiGetLocatorFilename */


/*
** tuiUpdateLocatorInfoFromFrame().
**        Function to update the locator, with the information extracted from frameInfo
*/
void
#ifdef __STDC__
tuiUpdateLocatorInfoFromFrame (
				struct frame_info *frameInfo,
				TuiLocatorElementPtr element)
#else
tuiUpdateLocatorInfoFromFrame (frameInfo, element)
     struct frame_info *frameInfo;
     TuiLocatorElementPtr element;
#endif
{
  struct symtab_and_line symtabAndLine;

  /* now get the new info */
  symtabAndLine = find_pc_line (frameInfo->pc,
			   (frameInfo->next != (struct frame_info *) NULL &&
			    !frameInfo->next->signal_handler_caller &&
			    !frame_in_dummy (frameInfo->next)));
  if (symtabAndLine.symtab && symtabAndLine.symtab->filename)
    tuiSetLocatorInfo (symtabAndLine.symtab->filename,
		       _getFuncNameFromFrame (frameInfo),
		       symtabAndLine.line,
		       (Opaque) frameInfo->pc,
		       element);
  else
    tuiSetLocatorInfo ((char *) NULL,
		       _getFuncNameFromFrame (frameInfo),
		       0,
		       (Opaque) frameInfo->pc,
		       element);

  return;
}				/* tuiUpdateLocatorInfoFromFrame */


/*
** tuiSetLocatorContent().
**        Function to set the content of the locator
*/
void
#ifdef __STDC__
tuiSetLocatorContent (
		       struct frame_info *frameInfo)
#else
tuiSetLocatorContent (frameInfo)
     struct frame_info *frameInfo;
#endif
{
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
  TuiWinElementPtr element;
  struct symtab_and_line symtabAndLine;

  /* Allocate the element if necessary */
  if (locator->contentSize <= 0)
    {
      TuiWinContent contentPtr;

      if ((locator->content = (OpaquePtr) allocContent (1, locator->type)) == (OpaquePtr) NULL)
	error ("Unable to Allocate Memory to Display Location.");
      locator->contentSize = 1;
    }

  if (frameInfo != (struct frame_info *) NULL)
    tuiUpdateLocatorInfoFromFrame (frameInfo,
	   &((TuiWinElementPtr) locator->content[0])->whichElement.locator);
  else
    tuiSetLocatorInfo ((char *) NULL,
		       (char *) NULL,
		       0,
		       (Opaque) NULL,
	   &((TuiWinElementPtr) locator->content[0])->whichElement.locator);
  return;
}				/* tuiSetLocatorContent */


/*
** tuiUpdateLocatorDisplay().
**        Function to update the locator display
*/
void
#ifdef __STDC__
tuiUpdateLocatorDisplay (
			  struct frame_info *frameInfo)
#else
tuiUpdateLocatorDisplay (frameInfo)
     struct frame_info *frameInfo;
#endif
{
  tuiClearLocatorDisplay ();
  tuiSetLocatorContent (frameInfo);
  tuiShowLocatorContent ();

  return;
}				/* tuiUpdateLocatorDisplay */


/*
** tuiShowFrameInfo().
**        Function to print the frame inforrmation for the TUI.
*/
void
#ifdef __STDC__
tuiShowFrameInfo (
		   struct frame_info *fi)
#else
tuiShowFrameInfo (fi)
     struct frame_info *fi;
#endif
{
  TuiWinInfoPtr winInfo;
  register int i;

  if (fi)
    {
      register int startLine, i;
      register struct symtab *s;
      CORE_ADDR low;
      TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
      int sourceAlreadyDisplayed;


      s = find_pc_symtab (fi->pc);
      sourceAlreadyDisplayed = tuiSourceIsDisplayed (s->filename);
      tuiUpdateLocatorDisplay (fi);
      for (i = 0; i < (sourceWindows ())->count; i++)
	{
	  winInfo = (TuiWinInfoPtr) (sourceWindows ())->list[i];
	  if (winInfo == srcWin)
	    {
	      startLine =
		(((TuiWinElementPtr) locator->content[0])->whichElement.locator.lineNo -
		 (winInfo->generic.viewportHeight / 2)) + 1;
	      if (startLine <= 0)
		startLine = 1;
	    }
	  else
	    {
	      if (find_pc_partial_function (fi->pc, (char **) NULL, &low, (CORE_ADDR) NULL) == 0)
		error ("No function contains program counter for selected frame.\n");
	      else
		low = (CORE_ADDR) tuiGetLowDisassemblyAddress ((Opaque) low, (Opaque) fi->pc);
	    }

	  if (winInfo == srcWin)
	    {
	      if (!(sourceAlreadyDisplayed && m_tuiLineDisplayedWithinThreshold (
								    winInfo,
										  ((TuiWinElementPtr) locator->content[0])->whichElement.locator.lineNo)))
		tuiUpdateSourceWindow (winInfo, s, (Opaque) startLine, TRUE);
	      else
		tuiSetIsExecPointAt ((Opaque)
				     ((TuiWinElementPtr) locator->content[0])->whichElement.locator.lineNo,
				     winInfo);
	    }
	  else
	    {
	      if (winInfo == disassemWin)
		{
		  if (!m_tuiLineDisplayedWithinThreshold (winInfo,
							  ((TuiWinElementPtr) locator->content[0])->whichElement.locator.addr))
		    tuiUpdateSourceWindow (winInfo, s, (Opaque) low, TRUE);
		  else
		    tuiSetIsExecPointAt ((Opaque)
					 ((TuiWinElementPtr) locator->content[0])->whichElement.locator.addr,
					 winInfo);
		}
	    }
	  tuiUpdateExecInfo (winInfo);
	}
    }
  else
    {
      tuiUpdateLocatorDisplay (fi);
      for (i = 0; i < (sourceWindows ())->count; i++)
	{
	  winInfo = (TuiWinInfoPtr) (sourceWindows ())->list[i];
	  tuiClearSourceContent (winInfo, EMPTY_SOURCE_PROMPT);
	  tuiUpdateExecInfo (winInfo);
	}
    }

  return;
}				/* tuiShowFrameInfo */


/*
** tui_vShowFrameInfo().
**        Function to print the frame inforrmation for the TUI with args in a va_list.
*/
void
#ifdef __STDC__
tui_vShowFrameInfo (
		     va_list args)
#else
tui_vShowFrameInfo (args)
     va_list args;
#endif
{
  struct frame_info *fi;

  fi = va_arg (args, struct frame_info *);
  tuiShowFrameInfo (fi);

  return;
}				/* tui_vShowFrameInfo */


/*
** _initialize_tuiStack().
**      Function to initialize gdb commands, for tui window stack manipulation.
*/
void
_initialize_tuiStack ()
{
  if (tui_version)
    {
      add_com ("update", class_tui, _tuiUpdateLocation_command,
	       "Update the source window and locator to display the current execution point.\n");
    }

  return;
}				/* _initialize_tuiStack */


/*****************************************
** STATIC LOCAL FUNCTIONS                 **
******************************************/

/*
**    _getFuncNameFromFrame().
*/
static char *
#ifdef __STDC__
_getFuncNameFromFrame (
			struct frame_info *frameInfo)
#else
_getFuncNameFromFrame (frameInfo)
     struct frame_info *frameInfo;
#endif
{
  char *funcName = (char *) NULL;

  find_pc_partial_function (frameInfo->pc,
			    &funcName,
			    (CORE_ADDR *) NULL,
			    (CORE_ADDR *) NULL);
  return funcName;
}				/* _getFuncNameFromFrame */


/*
** _tuiUpdateLocation_command().
**        Command to update the display with the current execution point
*/
static void
#ifdef __STDC__
_tuiUpdateLocation_command (
			     char *arg,
			     int fromTTY)
#else
_tuiUpdateLocation_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
#ifndef TRY
  extern void frame_command PARAMS ((char *, int));
  frame_command ("0", FALSE);
#else
  struct frame_info *curFrame;

  /* Obtain the current execution point */
  if ((curFrame = get_current_frame ()) != (struct frame_info *) NULL)
    {
      struct frame_info *frame;
      int curLevel = 0;

      for (frame = get_prev_frame (curLevel);
	   (frame != (struct frame_info *) NULL && (frame != curFrame));
	   frame = get_prev_frame (frame))
	curLevel++;

      if (curFrame != (struct frame_info *) NULL)
	print_frame_info (frame, curLevel, 0, 1);
    }
#endif

  return;
}				/* _tuiUpdateLocation_command */
