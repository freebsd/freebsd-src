/*
** tuiData.c
**    This module contains functions for manipulating the data
**    structures used by the TUI
*/

#include "defs.h"
#include "tui.h"
#include "tuiData.h"

/****************************
** GLOBAL DECLARATIONS
****************************/
TuiWinInfoPtr winList[MAX_MAJOR_WINDOWS];

/***************************
** Private Definitions
****************************/
#define FILE_WIDTH   30
#define PROC_WIDTH   40
#define LINE_WIDTH   4
#define PC_WIDTH     8

/***************************
** Private data
****************************/
static char *_tuiNullStr = TUI_NULL_STR;
static char *_tuiBlankStr = "   ";
static char *_tuiLocationStr = "  >";
static char *_tuiBreakStr = " * ";
static char *_tuiBreakLocationStr = " *>";
static TuiLayoutType _currentLayout = UNDEFINED_LAYOUT;
static int _termHeight, _termWidth;
static int _historyLimit = DEFAULT_HISTORY_COUNT;
static TuiGenWinInfo _locator;
static TuiGenWinInfo _execInfo[2];
static TuiWinInfoPtr _srcWinList[2];
static TuiList _sourceWindows =
{(OpaqueList) _srcWinList, 0};
static int _defaultTabLen = DEFAULT_TAB_LEN;
static TuiWinInfoPtr _winWithFocus = (TuiWinInfoPtr) NULL;
static TuiLayoutDef _layoutDef =
{SRC_WIN,			/* displayMode */
 FALSE,				/* split */
 TUI_UNDEFINED_REGS,		/* regsDisplayType */
 TUI_SFLOAT_REGS};		/* floatRegsDisplayType */
static int _winResized = FALSE;


/*********************************
** Static function forward decls
**********************************/
static void freeContent PARAMS ((TuiWinContent, int, TuiWinType));
static void freeContentElements PARAMS ((TuiWinContent, int, TuiWinType));



/*********************************
** PUBLIC FUNCTIONS
**********************************/

/******************************************
** ACCESSORS & MUTATORS FOR PRIVATE DATA
******************************************/

/*
** tuiWinResized().
**        Answer a whether the terminal window has been resized or not
*/
int
#ifdef __STDC__
tuiWinResized (void)
#else
tuiWinResized ()
#endif
{
  return _winResized;
}				/* tuiWinResized */


/*
** tuiSetWinResized().
**        Set a whether the terminal window has been resized or not
*/
void
#ifdef __STDC__
tuiSetWinResizedTo (
		     int resized)
#else
tuiSetWinResizedTo (resized)
     int resized;
#endif
{
  _winResized = resized;

  return;
}				/* tuiSetWinResizedTo */


/*
** tuiLayoutDef().
**        Answer a pointer to the current layout definition
*/
TuiLayoutDefPtr
#ifdef __STDC__
tuiLayoutDef (void)
#else
tuiLayoutDef ()
#endif
{
  return &_layoutDef;
}				/* tuiLayoutDef */


/*
** tuiWinWithFocus().
**        Answer the window with the logical focus
*/
TuiWinInfoPtr
#ifdef __STDC__
tuiWinWithFocus (void)
#else
tuiWinWithFocus ()
#endif
{
  return _winWithFocus;
}				/* tuiWinWithFocus */


/*
** tuiSetWinWithFocus().
**        Set the window that has the logical focus
*/
void
#ifdef __STDC__
tuiSetWinWithFocus (
		     TuiWinInfoPtr winInfo)
#else
tuiSetWinWithFocus (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  _winWithFocus = winInfo;

  return;
}				/* tuiSetWinWithFocus */


/*
** tuiDefaultTabLen().
**        Answer the length in chars, of tabs
*/
int
#ifdef __STDC__
tuiDefaultTabLen (void)
#else
tuiDefaultTabLen ()
#endif
{
  return _defaultTabLen;
}				/* tuiDefaultTabLen */


/*
** tuiSetDefaultTabLen().
**        Set the length in chars, of tabs
*/
void
#ifdef __STDC__
tuiSetDefaultTabLen (
		      int len)
#else
tuiSetDefaultTabLen (len)
     int len;
#endif
{
  _defaultTabLen = len;

  return;
}				/* tuiSetDefaultTabLen */


/*
** currentSourceWin()
**        Accessor for the current source window.  Usually there is only
**        one source window (either source or disassembly), but both can
**        be displayed at the same time.
*/
TuiListPtr
#ifdef __STDC__
sourceWindows (void)
#else
sourceWindows ()
#endif
{
  return &_sourceWindows;
}				/* currentSourceWindows */


/*
** clearSourceWindows()
**        Clear the list of source windows.  Usually there is only one
**        source window (either source or disassembly), but both can be
**        displayed at the same time.
*/
void
#ifdef __STDC__
clearSourceWindows (void)
#else
clearSourceWindows ()
#endif
{
  _sourceWindows.list[0] = (Opaque) NULL;
  _sourceWindows.list[1] = (Opaque) NULL;
  _sourceWindows.count = 0;

  return;
}				/* currentSourceWindows */


/*
** clearSourceWindowsDetail()
**        Clear the pertinant detail in the source windows.
*/
void
#ifdef __STDC__
clearSourceWindowsDetail (void)
#else
clearSourceWindowsDetail ()
#endif
{
  int i;

  for (i = 0; i < (sourceWindows ())->count; i++)
    clearWinDetail ((TuiWinInfoPtr) (sourceWindows ())->list[i]);

  return;
}				/* currentSourceWindows */


/*
** addSourceWindowToList().
**       Add a window to the list of source windows.  Usually there is
**       only one source window (either source or disassembly), but
**       both can be displayed at the same time.
*/
void
#ifdef __STDC__
addToSourceWindows (
		     TuiWinInfoPtr winInfo)
#else
addToSourceWindows (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (_sourceWindows.count < 2)
    _sourceWindows.list[_sourceWindows.count++] = (Opaque) winInfo;

  return;
}				/* addToSourceWindows */


/*
** clearWinDetail()
**        Clear the pertinant detail in the windows.
*/
void
#ifdef __STDC__
clearWinDetail (
		 TuiWinInfoPtr winInfo)
#else
clearWinDetail (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  if (m_winPtrNotNull (winInfo))
    {
      switch (winInfo->generic.type)
	{
	case SRC_WIN:
	case DISASSEM_WIN:
	  winInfo->detail.sourceInfo.startLineOrAddr.addr = (Opaque) NULL;
	  winInfo->detail.sourceInfo.horizontalOffset = 0;
	  break;
	case CMD_WIN:
	  winInfo->detail.commandInfo.curLine =
	    winInfo->detail.commandInfo.curch = 0;
	  break;
	case DATA_WIN:
	  winInfo->detail.dataDisplayInfo.dataContent =
	    (TuiWinContent) NULL;
	  winInfo->detail.dataDisplayInfo.dataContentCount = 0;
	  winInfo->detail.dataDisplayInfo.regsContent =
	    (TuiWinContent) NULL;
	  winInfo->detail.dataDisplayInfo.regsContentCount = 0;
	  winInfo->detail.dataDisplayInfo.regsDisplayType =
	    TUI_UNDEFINED_REGS;
	  winInfo->detail.dataDisplayInfo.regsColumnCount = 1;
	  winInfo->detail.dataDisplayInfo.displayRegs = FALSE;
	  break;
	default:
	  break;
	}
    }

  return;
}				/* clearWinDetail */


/*
** blankStr()
**        Accessor for the blank string.
*/
char *
#ifdef __STDC__
blankStr (void)
#else
blankStr ()
#endif
{
  return _tuiBlankStr;
}				/* blankStr */


/*
** locationStr()
**        Accessor for the location string.
*/
char *
#ifdef __STDC__
locationStr (void)
#else
locationStr ()
#endif
{
  return _tuiLocationStr;
}				/* locationStr */


/*
** breakStr()
**        Accessor for the break string.
*/
char *
#ifdef __STDC__
breakStr (void)
#else
breakStr ()
#endif
{
  return _tuiBreakStr;
}				/* breakStr */


/*
** breakLocationStr()
**        Accessor for the breakLocation string.
*/
char *
#ifdef __STDC__
breakLocationStr (void)
#else
breakLocationStr ()
#endif
{
  return _tuiBreakLocationStr;
}				/* breakLocationStr */


/*
** nullStr()
**        Accessor for the null string.
*/
char *
#ifdef __STDC__
nullStr (void)
#else
nullStr ()
#endif
{
  return _tuiNullStr;
}				/* nullStr */


/*
** sourceExecInfoPtr().
**        Accessor for the source execution info ptr.
*/
TuiGenWinInfoPtr
#ifdef __STDC__
sourceExecInfoWinPtr (void)
#else
sourceExecInfoWinPtr ()
#endif
{
  return &_execInfo[0];
}				/* sourceExecInfoWinPtr */


/*
** disassemExecInfoPtr().
**        Accessor for the disassem execution info ptr.
*/
TuiGenWinInfoPtr
#ifdef __STDC__
disassemExecInfoWinPtr (void)
#else
disassemExecInfoWinPtr ()
#endif
{
  return &_execInfo[1];
}				/* disassemExecInfoWinPtr */


/*
** locatorWinInfoPtr().
**        Accessor for the locator win info.  Answers a pointer to the
**        static locator win info struct.
*/
TuiGenWinInfoPtr
#ifdef __STDC__
locatorWinInfoPtr (void)
#else
locatorWinInfoPtr ()
#endif
{
  return &_locator;
}				/* locatorWinInfoPtr */


/*
** historyLimit().
**        Accessor for the history limit
*/
int
#ifdef __STDC__
historyLimit (void)
#else
historyLimit ()
#endif
{
  return _historyLimit;
}				/* historyLimit */


/*
** setHistoryLimitTo().
**        Mutator for the history limit
*/
void
#ifdef __STDC__
setHistoryLimitTo (
		    int h)
#else
setHistoryLimitTo (h)
     int h;
#endif
{
  _historyLimit = h;

  return;
}				/* setHistoryLimitTo */

/*
** termHeight().
**        Accessor for the termHeight
*/
int
#ifdef __STDC__
termHeight (void)
#else
termHeight ()
#endif
{
  return _termHeight;
}				/* termHeight */


/*
** setTermHeightTo().
**        Mutator for the term height
*/
void
#ifdef __STDC__
setTermHeightTo (
		  int h)
#else
setTermHeightTo (h)
     int h;
#endif
{
  _termHeight = h;

  return;
}				/* setTermHeightTo */


/*
** termWidth().
**        Accessor for the termWidth
*/
int
#ifdef __STDC__
termWidth (void)
#else
termWidth ()
#endif
{
  return _termWidth;
}				/* termWidth */


/*
** setTermWidth().
**        Mutator for the termWidth
*/
void
#ifdef __STDC__
setTermWidthTo (
		 int w)
#else
setTermWidthTo (w)
     int w;
#endif
{
  _termWidth = w;

  return;
}				/* setTermWidthTo */


/*
** currentLayout().
**        Accessor for the current layout
*/
TuiLayoutType
#ifdef __STDC__
currentLayout (void)
#else
currentLayout ()
#endif
{
  return _currentLayout;
}				/* currentLayout */


/*
** setCurrentLayoutTo().
**        Mutator for the current layout
*/
void
#ifdef __STDC__
setCurrentLayoutTo (
		     TuiLayoutType newLayout)
#else
setCurrentLayoutTo (newLayout)
     TuiLayoutType newLayout;
#endif
{
  _currentLayout = newLayout;

  return;
}				/* setCurrentLayoutTo */


/*
** setGenWinOrigin().
**        Set the origin of the window
*/
void
#ifdef __STDC__
setGenWinOrigin (
		  TuiGenWinInfoPtr winInfo,
		  int x,
		  int y)
#else
setGenWinOrigin (winInfo, x, y)
     TuiGenWinInfoPtr winInfo;
     int x;
     int y;
#endif
{
  winInfo->origin.x = x;
  winInfo->origin.y = y;

  return;
}				/* setGenWinOrigin */


/*****************************
** OTHER PUBLIC FUNCTIONS
*****************************/


/*
** tuiNextWin().
**        Answer the next window in the list, cycling back to the top
**        if necessary
*/
TuiWinInfoPtr
#ifdef __STDC__
tuiNextWin (
	     TuiWinInfoPtr curWin)
#else
tuiNextWin (curWin)
     TuiWinInfoPtr curWin;
#endif
{
  TuiWinType type = curWin->generic.type;
  TuiWinInfoPtr nextWin = (TuiWinInfoPtr) NULL;

  if (curWin->generic.type == CMD_WIN)
    type = SRC_WIN;
  else
    type = curWin->generic.type + 1;
  while (type != curWin->generic.type && m_winPtrIsNull (nextWin))
    {
      if (winList[type]->generic.isVisible)
	nextWin = winList[type];
      else
	{
	  if (type == CMD_WIN)
	    type = SRC_WIN;
	  else
	    type++;
	}
    }

  return nextWin;
}				/* tuiNextWin */


/*
** tuiPrevWin().
**        Answer the prev window in the list, cycling back to the bottom
**        if necessary
*/
TuiWinInfoPtr
#ifdef __STDC__
tuiPrevWin (
	     TuiWinInfoPtr curWin)
#else
tuiPrevWin (curWin)
     TuiWinInfoPtr curWin;
#endif
{
  TuiWinType type = curWin->generic.type;
  TuiWinInfoPtr prev = (TuiWinInfoPtr) NULL;

  if (curWin->generic.type == SRC_WIN)
    type = CMD_WIN;
  else
    type = curWin->generic.type - 1;
  while (type != curWin->generic.type && m_winPtrIsNull (prev))
    {
      if (winList[type]->generic.isVisible)
	prev = winList[type];
      else
	{
	  if (type == SRC_WIN)
	    type = CMD_WIN;
	  else
	    type--;
	}
    }

  return prev;
}				/* tuiPrevWin */


/*
** displayableWinContentOf().
**        Answer a the content at the location indicated by index.  Note
**        that if this is a locator window, the string returned should be
**        freed after use.
*/
char *
#ifdef __STDC__
displayableWinContentOf (
			  TuiGenWinInfoPtr winInfo,
			  TuiWinElementPtr elementPtr)
#else
displayableWinContentOf (winInfo, elementPtr)
     TuiGenWinInfoPtr winInfo;
     TuiWinElementPtr elementPtr;
#endif
{

  char *string = nullStr ();

  if (elementPtr != (TuiWinElementPtr) NULL || winInfo->type == LOCATOR_WIN)
    {
      /*
        ** Now convert the line to a displayable string
        */
      switch (winInfo->type)
	{
	case SRC_WIN:
	case DISASSEM_WIN:
	  string = elementPtr->whichElement.source.line;
	  break;
	case CMD_WIN:
	  string = elementPtr->whichElement.command.line;
	  break;
	case LOCATOR_WIN:
	  if ((string = (char *) xmalloc (
		      (termWidth () + 1) * sizeof (char))) == (char *) NULL)
	      string = nullStr ();
	  else
	    {
	      char lineNo[50], pc[50], buf[50], *fname, *pname;
	      register int strSize = termWidth (), i, procWidth, fileWidth;

	      /*
                    ** First determine the amount of file/proc name width
                    ** we have available
                    */
	      i = strSize - (PC_WIDTH + LINE_WIDTH
			     + 25	/* pc and line labels */
			     + strlen (FILE_PREFIX) + 1	/* file label */
			     + 15 /* procedure label */ );
	      if (i >= FILE_WIDTH + PROC_WIDTH)
		{
		  fileWidth = FILE_WIDTH;
		  procWidth = PROC_WIDTH;
		}
	      else
		{
		  fileWidth = i / 2;
		  procWidth = i - fileWidth;
		}

	      /* Now convert elements to string form */
	      if (elementPtr != (TuiWinElementPtr) NULL &&
		  *elementPtr->whichElement.locator.fileName != (char) 0 &&
		  srcWin->generic.isVisible)
		fname = elementPtr->whichElement.locator.fileName;
	      else
		fname = "??";
	      if (elementPtr != (TuiWinElementPtr) NULL &&
		  *elementPtr->whichElement.locator.procName != (char) 0)
		pname = elementPtr->whichElement.locator.procName;
	      else
		pname = "??";
	      if (elementPtr != (TuiWinElementPtr) NULL &&
		  elementPtr->whichElement.locator.lineNo > 0)
		sprintf (lineNo, "%d",
			 elementPtr->whichElement.locator.lineNo);
	      else
		strcpy (lineNo, "??");
	      if (elementPtr != (TuiWinElementPtr) NULL &&
		  elementPtr->whichElement.locator.addr > (Opaque) 0)
		sprintf (pc, "0x%x",
			 elementPtr->whichElement.locator.addr);
	      else
		strcpy (pc, "??");
	      /*
                    ** Now create the locator line from the string version
                    ** of the elements.  We could use sprintf() here but
                    ** that wouldn't ensure that we don't overrun the size
                    ** of the allocated buffer.  strcat_to_buf() will.
                    */
	      *string = (char) 0;
	      /* Filename */
	      strcat_to_buf (string, strSize, " ");
	      strcat_to_buf (string, strSize, FILE_PREFIX);
	      if (strlen (fname) > fileWidth)
		{
		  strncpy (buf, fname, fileWidth - 1);
		  buf[fileWidth - 1] = '*';
		  buf[fileWidth] = (char) 0;
		}
	      else
		strcpy (buf, fname);
	      strcat_to_buf (string, strSize, buf);
	      /* procedure/class name */
	      sprintf (buf, "%15s", PROC_PREFIX);
	      strcat_to_buf (string, strSize, buf);
	      if (strlen (pname) > procWidth)
		{
		  strncpy (buf, pname, procWidth - 1);
		  buf[procWidth - 1] = '*';
		  buf[procWidth] = (char) 0;
		}
	      else
		strcpy (buf, pname);
	      strcat_to_buf (string, strSize, buf);
	      sprintf (buf, "%10s", LINE_PREFIX);
	      strcat_to_buf (string, strSize, buf);
	      strcat_to_buf (string, strSize, lineNo);
	      sprintf (buf, "%10s", PC_PREFIX);
	      strcat_to_buf (string, strSize, buf);
	      strcat_to_buf (string, strSize, pc);
	      for (i = strlen (string); i < strSize; i++)
		string[i] = ' ';
	      string[strSize] = (char) 0;
	    }
	  break;
	case EXEC_INFO_WIN:
	  string = elementPtr->whichElement.simpleString;
	  break;
	default:
	  break;
	}
    }
  return string;
}				/* displayableWinContentOf */


/*
**    winContentAt().
**        Answer a the content at the location indicated by index
*/
char *
#ifdef __STDC__
displayableWinContentAt (
			  TuiGenWinInfoPtr winInfo,
			  int index)
#else
displayableWinContentAt (winInfo, index)
     TuiGenWinInfoPtr winInfo;
     int index;
#endif
{
  return (displayableWinContentOf (winInfo, (TuiWinElementPtr) winInfo->content[index]));
}				/* winContentAt */


/*
** winElementHeight().
**        Answer the height of the element in lines
*/
int
#ifdef __STDC__
winElementHeight (
		   TuiGenWinInfoPtr winInfo,
		   TuiWinElementPtr element)
#else
winElementHeight (winInfo, element)
     TuiGenWinInfoPtr winInfo;
     TuiWinElementPtr element;
#endif
{
  int h;

  if (winInfo->type == DATA_WIN)
/* FOR NOW SAY IT IS ONLY ONE LINE HIGH */
    h = 1;
  else
    h = 1;

  return h;
}				/* winElementHeight */


/*
**  winByName().
**      Answer the window represented by name
*/
TuiWinInfoPtr
#ifdef __STDC__
winByName (
	    char *name)
#else
winByName (name)
     char *name;
#endif
{
  TuiWinInfoPtr winInfo = (TuiWinInfoPtr) NULL;
  int i = 0;

  while (i < MAX_MAJOR_WINDOWS && m_winPtrIsNull (winInfo))
    {
      if (strcmp (name, winName (&(winList[i]->generic))) == 0)
	winInfo = winList[i];
      i++;
    }

  return winInfo;
}				/* winByName */


/*
**  partialWinByName().
**      Answer the window represented by name
*/
TuiWinInfoPtr
#ifdef __STDC__
partialWinByName (
		   char *name)
#else
partialWinByName (name)
     char *name;
#endif
{
  TuiWinInfoPtr winInfo = (TuiWinInfoPtr) NULL;

  if (name != (char *) NULL)
    {
      int i = 0;

      while (i < MAX_MAJOR_WINDOWS && m_winPtrIsNull (winInfo))
	{
	  char *curName = winName (&winList[i]->generic);
	  if (strlen (name) <= strlen (curName) &&
	      strncmp (name, curName, strlen (name)) == 0)
	    winInfo = winList[i];
	  i++;
	}
    }

  return winInfo;
}				/* partialWinByName */


/*
** winName().
**      Answer the name of the window
*/
char *
#ifdef __STDC__
winName (
	  TuiGenWinInfoPtr winInfo)
#else
winName (winInfo)
     TuiGenWinInfoPtr winInfo;
#endif
{
  char *name = (char *) NULL;

  switch (winInfo->type)
    {
    case SRC_WIN:
      name = SRC_NAME;
      break;
    case CMD_WIN:
      name = CMD_NAME;
      break;
    case DISASSEM_WIN:
      name = DISASSEM_NAME;
      break;
    case DATA_WIN:
      name = DATA_NAME;
      break;
    default:
      name = "";
      break;
    }

  return name;
}				/* winName */


/*
** initializeStaticData
*/
void
#ifdef __STDC__
initializeStaticData (void)
#else
initializeStaticData ()
#endif
{
  initGenericPart (sourceExecInfoWinPtr ());
  initGenericPart (disassemExecInfoWinPtr ());
  initGenericPart (locatorWinInfoPtr ());

  return;
}				/* initializeStaticData */


/*
** allocGenericWinInfo().
*/
TuiGenWinInfoPtr
#ifdef __STDC__
allocGenericWinInfo (void)
#else
allocGenericWinInfo ()
#endif
{
  TuiGenWinInfoPtr win;

  if ((win = (TuiGenWinInfoPtr) xmalloc (
		     sizeof (TuiGenWinInfoPtr))) != (TuiGenWinInfoPtr) NULL)
    initGenericPart (win);

  return win;
}				/* allocGenericWinInfo */


/*
** initGenericPart().
*/
void
#ifdef __STDC__
initGenericPart (
		  TuiGenWinInfoPtr win)
#else
initGenericPart (win)
     TuiGenWinInfoPtr win;
#endif
{
  win->width =
    win->height =
    win->origin.x =
    win->origin.y =
    win->viewportHeight =
    win->contentSize =
    win->lastVisibleLine = 0;
  win->handle = (WINDOW *) NULL;
  win->content = (OpaquePtr) NULL;
  win->contentInUse =
    win->isVisible = FALSE;

  return;
}				/* initGenericPart */


/*
** initContentElement().
*/
void
#ifdef __STDC__
initContentElement (
		     TuiWinElementPtr element,
		     TuiWinType type)
#else
initContentElement (element, type)
     TuiWinElementPtr element;
     TuiWinType type;
#endif
{
  element->highlight = FALSE;
  switch (type)
    {
    case SRC_WIN:
    case DISASSEM_WIN:
      element->whichElement.source.line = (char *) NULL;
      element->whichElement.source.lineOrAddr.lineNo = 0;
      element->whichElement.source.isExecPoint = FALSE;
      element->whichElement.source.hasBreak = FALSE;
      break;
    case DATA_WIN:
      initGenericPart (&element->whichElement.dataWindow);
      element->whichElement.dataWindow.type = DATA_ITEM_WIN;
      ((TuiGenWinInfoPtr) & element->whichElement.dataWindow)->content =
	(OpaquePtr) allocContent (1, DATA_ITEM_WIN);
      ((TuiGenWinInfoPtr)
       & element->whichElement.dataWindow)->contentSize = 1;
      break;
    case CMD_WIN:
      element->whichElement.command.line = (char *) NULL;
      break;
    case DATA_ITEM_WIN:
      element->whichElement.data.name = (char *) NULL;
      element->whichElement.data.type = TUI_REGISTER;
      element->whichElement.data.itemNo = UNDEFINED_ITEM;
      element->whichElement.data.value = (Opaque) NULL;
      element->whichElement.data.highlight = FALSE;
      break;
    case LOCATOR_WIN:
      element->whichElement.locator.fileName[0] =
	element->whichElement.locator.procName[0] = (char) 0;
      element->whichElement.locator.lineNo = 0;
      element->whichElement.locator.addr = 0;
      break;
    case EXEC_INFO_WIN:
      element->whichElement.simpleString = blankStr ();
      break;
    default:
      break;
    }
  return;
}				/* initContentElement */

/*
** initWinInfo().
*/
void
#ifdef __STDC__
initWinInfo (
	      TuiWinInfoPtr winInfo)
#else
initWinInfo (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  initGenericPart (&winInfo->generic);
  winInfo->canHighlight =
    winInfo->isHighlighted = FALSE;
  switch (winInfo->generic.type)
    {
    case SRC_WIN:
    case DISASSEM_WIN:
      winInfo->detail.sourceInfo.executionInfo = (TuiGenWinInfoPtr) NULL;
      winInfo->detail.sourceInfo.hasLocator = FALSE;
      winInfo->detail.sourceInfo.horizontalOffset = 0;
      winInfo->detail.sourceInfo.startLineOrAddr.addr = (Opaque) NULL;
      break;
    case DATA_WIN:
      winInfo->detail.dataDisplayInfo.dataContent = (TuiWinContent) NULL;
      winInfo->detail.dataDisplayInfo.dataContentCount = 0;
      winInfo->detail.dataDisplayInfo.regsContent = (TuiWinContent) NULL;
      winInfo->detail.dataDisplayInfo.regsContentCount = 0;
      winInfo->detail.dataDisplayInfo.regsDisplayType =
	TUI_UNDEFINED_REGS;
      winInfo->detail.dataDisplayInfo.regsColumnCount = 1;
      winInfo->detail.dataDisplayInfo.displayRegs = FALSE;
      break;
    case CMD_WIN:
      winInfo->detail.commandInfo.curLine = 0;
      winInfo->detail.commandInfo.curch = 0;
      break;
    default:
      winInfo->detail.opaque = (Opaque) NULL;
      break;
    }

  return;
}				/* initWinInfo */


/*
** allocWinInfo().
*/
TuiWinInfoPtr
#ifdef __STDC__
allocWinInfo (
	       TuiWinType type)
#else
allocWinInfo (type)
     TuiWinType type;
#endif
{
  TuiWinInfoPtr winInfo = (TuiWinInfoPtr) NULL;

  winInfo = (TuiWinInfoPtr) xmalloc (sizeof (TuiWinInfo));
  if (m_winPtrNotNull (winInfo))
    {
      winInfo->generic.type = type;
      initWinInfo (winInfo);
    }

  return winInfo;
}				/* allocWinInfo */


/*
** allocContent().
**        Allocates the content and elements in a block.
*/
TuiWinContent
#ifdef __STDC__
allocContent (
	       int numElements,
	       TuiWinType type)
#else
allocContent (numElements, type)
     int numElements;
     TuiWinType type;
#endif
{
  TuiWinContent content = (TuiWinContent) NULL;
  char *elementBlockPtr = (char *) NULL;
  int i;

  if ((content = (TuiWinContent)
  xmalloc (sizeof (TuiWinElementPtr) * numElements)) != (TuiWinContent) NULL)
    {				/*
        ** All windows, except the data window, can allocate the elements
        ** in a chunk.  The data window cannot because items can be
        ** added/removed from the data display by the user at any time.
        */
      if (type != DATA_WIN)
	{
	  if ((elementBlockPtr = (char *)
	   xmalloc (sizeof (TuiWinElement) * numElements)) != (char *) NULL)
	    {
	      for (i = 0; i < numElements; i++)
		{
		  content[i] = (TuiWinElementPtr) elementBlockPtr;
		  initContentElement (content[i], type);
		  elementBlockPtr += sizeof (TuiWinElement);
		}
	    }
	  else
	    {
	      tuiFree ((char *) content);
	      content = (TuiWinContent) NULL;
	    }
	}
    }

  return content;
}				/* allocContent */


/*
** addContentElements().
**        Adds the input number of elements to the windows's content.  If
**        no content has been allocated yet, allocContent() is called to
**        do this.  The index of the first element added is returned,
**        unless there is a memory allocation error, in which case, (-1)
**        is returned.
*/
int
#ifdef __STDC__
addContentElements (
		     TuiGenWinInfoPtr winInfo,
		     int numElements)
#else
addContentElements (winInfo, numElements)
     TuiGenWinInfoPtr winInfo;
     int numElements;
#endif
{
  TuiWinElementPtr elementPtr;
  int i, indexStart;

  if (winInfo->content == (OpaquePtr) NULL)
    {
      winInfo->content = (OpaquePtr) allocContent (numElements, winInfo->type);
      indexStart = 0;
    }
  else
    indexStart = winInfo->contentSize;
  if (winInfo->content != (OpaquePtr) NULL)
    {
      for (i = indexStart; (i < numElements + indexStart); i++)
	{
	  if ((elementPtr = (TuiWinElementPtr)
	       xmalloc (sizeof (TuiWinElement))) != (TuiWinElementPtr) NULL)
	    {
	      winInfo->content[i] = (Opaque) elementPtr;
	      initContentElement (elementPtr, winInfo->type);
	      winInfo->contentSize++;
	    }
	  else			/* things must be really hosed now! We ran out of memory!?*/
	    return (-1);
	}
    }

  return indexStart;
}				/* addContentElements */


/*
**  tuiDelWindow().
**     Delete all curses windows associated with winInfo, leaving everything
**     else in tact.
*/
void
#ifdef __STDC__
tuiDelWindow (
	       TuiWinInfoPtr winInfo)
#else
tuiDelWindow (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  Opaque detail;
  int i;
  TuiGenWinInfoPtr genericWin;


  switch (winInfo->generic.type)
    {
    case SRC_WIN:
    case DISASSEM_WIN:
      genericWin = locatorWinInfoPtr ();
      if (genericWin != (TuiGenWinInfoPtr) NULL)
	{
	  tuiDelwin (genericWin->handle);
	  genericWin->handle = (WINDOW *) NULL;
	  genericWin->isVisible = FALSE;
	}
      genericWin = winInfo->detail.sourceInfo.executionInfo;
      if (genericWin != (TuiGenWinInfoPtr) NULL)
	{
	  tuiDelwin (genericWin->handle);
	  genericWin->handle = (WINDOW *) NULL;
	  genericWin->isVisible = FALSE;
	}
      break;
    case DATA_WIN:
      if (winInfo->generic.content != (OpaquePtr) NULL)
	{
	  int i;

	  tuiDelDataWindows (
			      winInfo->detail.dataDisplayInfo.regsContent,
			  winInfo->detail.dataDisplayInfo.regsContentCount);
	  tuiDelDataWindows (
			      winInfo->detail.dataDisplayInfo.dataContent,
			  winInfo->detail.dataDisplayInfo.dataContentCount);
	}
      break;
    default:
      break;
    }
  if (winInfo->generic.handle != (WINDOW *) NULL)
    {
      tuiDelwin (winInfo->generic.handle);
      winInfo->generic.handle = (WINDOW *) NULL;
      winInfo->generic.isVisible = FALSE;
    }

  return;
}				/* tuiDelWindow */


/*
**  freeWindow().
*/
void
#ifdef __STDC__
freeWindow (
	     TuiWinInfoPtr winInfo)
#else
freeWindow (winInfo)
     TuiWinInfoPtr winInfo;
#endif
{
  Opaque detail;
  int i;
  TuiGenWinInfoPtr genericWin;


  switch (winInfo->generic.type)
    {
    case SRC_WIN:
    case DISASSEM_WIN:
      genericWin = locatorWinInfoPtr ();
      if (genericWin != (TuiGenWinInfoPtr) NULL)
	{
	  tuiDelwin (genericWin->handle);
	  genericWin->handle = (WINDOW *) NULL;
	}
      freeWinContent (genericWin);
      genericWin = winInfo->detail.sourceInfo.executionInfo;
      if (genericWin != (TuiGenWinInfoPtr) NULL)
	{
	  tuiDelwin (genericWin->handle);
	  genericWin->handle = (WINDOW *) NULL;
	  freeWinContent (genericWin);
	}
      break;
    case DATA_WIN:
      if (winInfo->generic.content != (OpaquePtr) NULL)
	{
	  freeDataContent (
			    winInfo->detail.dataDisplayInfo.regsContent,
			  winInfo->detail.dataDisplayInfo.regsContentCount);
	  winInfo->detail.dataDisplayInfo.regsContent =
	    (TuiWinContent) NULL;
	  winInfo->detail.dataDisplayInfo.regsContentCount = 0;
	  freeDataContent (
			    winInfo->detail.dataDisplayInfo.dataContent,
			  winInfo->detail.dataDisplayInfo.dataContentCount);
	  winInfo->detail.dataDisplayInfo.dataContent =
	    (TuiWinContent) NULL;
	  winInfo->detail.dataDisplayInfo.dataContentCount = 0;
	  winInfo->detail.dataDisplayInfo.regsDisplayType =
	    TUI_UNDEFINED_REGS;
	  winInfo->detail.dataDisplayInfo.regsColumnCount = 1;
	  winInfo->detail.dataDisplayInfo.displayRegs = FALSE;
	  winInfo->generic.content = (OpaquePtr) NULL;
	  winInfo->generic.contentSize = 0;
	}
      break;
    default:
      break;
    }
  if (winInfo->generic.handle != (WINDOW *) NULL)
    {
      tuiDelwin (winInfo->generic.handle);
      winInfo->generic.handle = (WINDOW *) NULL;
      freeWinContent (&winInfo->generic);
    }
  free (winInfo);

  return;
}				/* freeWindow */


/*
** freeAllSourceWinsContent().
*/
void
#ifdef __STDC__
freeAllSourceWinsContent (void)
#else
freeAllSourceWinsContent ()
#endif
{
  int i;

  for (i = 0; i < (sourceWindows ())->count; i++)
    {
      TuiWinInfoPtr winInfo = (TuiWinInfoPtr) (sourceWindows ())->list[i];

      if (m_winPtrNotNull (winInfo))
	{
	  freeWinContent (&(winInfo->generic));
	  freeWinContent (winInfo->detail.sourceInfo.executionInfo);
	}
    }

  return;
}				/* freeAllSourceWinsContent */


/*
** freeWinContent().
*/
void
#ifdef __STDC__
freeWinContent (
		 TuiGenWinInfoPtr winInfo)
#else
freeWinContent (winInfo)
     TuiGenWinInfoPtr winInfo;
#endif
{
  if (winInfo->content != (OpaquePtr) NULL)
    {
      freeContent ((TuiWinContent) winInfo->content,
		   winInfo->contentSize,
		   winInfo->type);
      winInfo->content = (OpaquePtr) NULL;
    }
  winInfo->contentSize = 0;

  return;
}				/* freeWinContent */


/*
** freeAllWindows().
*/
void
#ifdef __STDC__
freeAllWindows (void)
#else
freeAllWindows ()
#endif
{
  TuiWinType type = SRC_WIN;

  for (; type < MAX_MAJOR_WINDOWS; type++)
    if (m_winPtrNotNull (winList[type]) &&
	winList[type]->generic.type != UNDEFINED_WIN)
      freeWindow (winList[type]);
  return;
}				/* freeAllWindows */


void
#ifdef __STDC__
tuiDelDataWindows (
		    TuiWinContent content,
		    int contentSize)
#else
tuiDelDataWindows (content, contentSize)
     TuiWinContent content;
     int contentSize;
#endif
{
  int i;

  /*
    ** Remember that data window content elements are of type TuiGenWinInfoPtr,
    ** each of which whose single element is a data element.
    */
  for (i = 0; i < contentSize; i++)
    {
      TuiGenWinInfoPtr genericWin = &content[i]->whichElement.dataWindow;

      if (genericWin != (TuiGenWinInfoPtr) NULL)
	{
	  tuiDelwin (genericWin->handle);
	  genericWin->handle = (WINDOW *) NULL;
	  genericWin->isVisible = FALSE;
	}
    }

  return;
}				/* tuiDelDataWindows */


void
#ifdef __STDC__
freeDataContent (
		  TuiWinContent content,
		  int contentSize)
#else
freeDataContent (content, contentSize)
     TuiWinContent content;
     int contentSize;
#endif
{
  int i;

  /*
    ** Remember that data window content elements are of type TuiGenWinInfoPtr,
    ** each of which whose single element is a data element.
    */
  for (i = 0; i < contentSize; i++)
    {
      TuiGenWinInfoPtr genericWin = &content[i]->whichElement.dataWindow;

      if (genericWin != (TuiGenWinInfoPtr) NULL)
	{
	  tuiDelwin (genericWin->handle);
	  genericWin->handle = (WINDOW *) NULL;
	  freeWinContent (genericWin);
	}
    }
  freeContent (content,
	       contentSize,
	       DATA_WIN);

  return;
}				/* freeDataContent */


/**********************************
** LOCAL STATIC FUNCTIONS        **
**********************************/


/*
** freeContent().
*/
static void
#ifdef __STDC__
freeContent (
	      TuiWinContent content,
	      int contentSize,
	      TuiWinType winType)
#else
freeContent (content, contentSize, winType)
     TuiWinContent content;
     int contentSize;
     TuiWinType winType;
#endif
{
  if (content != (TuiWinContent) NULL)
    {
      freeContentElements (content, contentSize, winType);
      tuiFree ((char *) content);
    }

  return;
}				/* freeContent */


/*
** freeContentElements().
*/
static void
#ifdef __STDC__
freeContentElements (
		      TuiWinContent content,
		      int contentSize,
		      TuiWinType type)
#else
freeContentElements (content, contentSize, type)
     TuiWinContent content;
     int contentSize;
     TuiWinType type;
#endif
{
  if (content != (TuiWinContent) NULL)
    {
      int i;

      if (type == SRC_WIN || type == DISASSEM_WIN)
	{
	  /* free whole source block */
	  if (content[0]->whichElement.source.line != (char *) NULL)
	    tuiFree (content[0]->whichElement.source.line);
	}
      else
	{
	  for (i = 0; i < contentSize; i++)
	    {
	      TuiWinElementPtr element;

	      element = content[i];
	      if (element != (TuiWinElementPtr) NULL)
		{
		  switch (type)
		    {
		    case DATA_WIN:
		      tuiFree ((char *) element);
		      break;
		    case DATA_ITEM_WIN:
		      /*
                            ** Note that data elements are not allocated
                            ** in a single block, but individually, as needed.
                            */
		      if (element->whichElement.data.type != TUI_REGISTER)
			tuiFree ((char *)
				 element->whichElement.data.name);
		      tuiFree ((char *) element->whichElement.data.value);
		      tuiFree ((char *) element);
		      break;
		    case CMD_WIN:
		      tuiFree ((char *) element->whichElement.command.line);
		      break;
		    default:
		      break;
		    }
		}
	    }
	}
      if (type != DATA_WIN && type != DATA_ITEM_WIN)
	tuiFree ((char *) content[0]);	/* free the element block */
    }

  return;
}				/* freeContentElements */
