/*
** tuiLayout.c
** This module contains procedures for handling the layout of the windows.
*/


#include "defs.h"
#include "command.h"
#include "symtab.h"
#include "frame.h"

#include "tui.h"
#include "tuiData.h"
#include "tuiGeneralWin.h"
#include "tuiStack.h"
#include "tuiRegs.h"
#include "tuiDisassem.h"

/*******************************
** Static Local Decls
********************************/

static void _initGenWinInfo PARAMS
  ((TuiGenWinInfoPtr, TuiWinType, int, int, int, int));
static void _initAndMakeWin PARAMS
  ((Opaque *, TuiWinType, int, int, int, int, int));
static void _showSourceOrDisassemAndCommand PARAMS
  ((TuiLayoutType));
static void _makeSourceOrDisassemWindow PARAMS
  ((TuiWinInfoPtr *, TuiWinType, int, int));
static void _makeCommandWindow PARAMS ((TuiWinInfoPtr *, int, int));
static void _makeSourceWindow PARAMS ((TuiWinInfoPtr *, int, int));
static void _makeDisassemWindow PARAMS
  ((TuiWinInfoPtr *, int, int));
static void _makeDataWindow PARAMS ((TuiWinInfoPtr *, int, int));
static void _showSourceCommand PARAMS ((void));
static void _showDisassemCommand PARAMS ((void));
static void _showSourceDisassemCommand PARAMS ((void));
static void _showData PARAMS ((TuiLayoutType));
static TuiLayoutType _nextLayout PARAMS ((void));
static TuiLayoutType _prevLayout PARAMS ((void));
static void _tuiLayout_command PARAMS ((char *, int));
static void _tuiToggleLayout_command PARAMS ((char *, int));
static void _tui_vToggleLayout_command PARAMS ((va_list));
static void _tuiToggleSplitLayout_command PARAMS ((char *, int));
static void _tui_vToggleSplitLayout_command PARAMS ((va_list));
static Opaque _extractDisplayStartAddr PARAMS ((void));
static void _tuiHandleXDBLayout PARAMS ((TuiLayoutDefPtr));
static TuiStatus _tuiSetLayoutTo PARAMS ((char *));


/***************************************
** DEFINITIONS
***************************************/

#define LAYOUT_USAGE     "Usage: layout prev | next | <layout_name> \n"

/***************************************
** Static Local Data
***************************************/
static TuiLayoutType lastLayout = UNDEFINED_LAYOUT;

/***************************************
** PUBLIC FUNCTIONS
***************************************/

/*
** showLayout().
**        Show the screen layout defined
*/
void
#ifdef __STDC__
showLayout (
	     TuiLayoutType layout)
#else
showLayout (layout)
     TuiLayoutType layout;
#endif
{
  TuiLayoutType curLayout = currentLayout ();

  if (layout != curLayout)
    {
      /*
        ** Since the new layout may cause changes in window size, we
        ** should free the content and reallocate on next display of
        ** source/asm
        */
      tuiClearAllSourceWinsContent (NO_EMPTY_SOURCE_PROMPT);
      freeAllSourceWinsContent ();
      clearSourceWindows ();
      if (layout == SRC_DATA_COMMAND || layout == DISASSEM_DATA_COMMAND)
	{
	  _showData (layout);
	  refreshAll (winList);
	}
      else
	{
	  /* First make the current layout be invisible */
	  m_allBeInvisible ();
	  m_beInvisible (locatorWinInfoPtr ());

	  switch (layout)
	    {
	      /* Now show the new layout */
	    case SRC_COMMAND:
	      _showSourceCommand ();
	      addToSourceWindows (srcWin);
	      break;
	    case DISASSEM_COMMAND:
	      _showDisassemCommand ();
	      addToSourceWindows (disassemWin);
	      break;
	    case SRC_DISASSEM_COMMAND:
	      _showSourceDisassemCommand ();
	      addToSourceWindows (srcWin);
	      addToSourceWindows (disassemWin);
	      break;
	    default:
	      break;
	    }
	}
    }

  return;
}				/* showLayout */


/*
** tuiSetLayout()
**    Function to set the layout to SRC_COMMAND, DISASSEM_COMMAND,
**    SRC_DISASSEM_COMMAND, SRC_DATA_COMMAND, or DISASSEM_DATA_COMMAND.
**    If the layout is SRC_DATA_COMMAND, DISASSEM_DATA_COMMAND, or
**    UNDEFINED_LAYOUT, then the data window is populated according
**    to regsDisplayType.
*/
TuiStatus
#ifdef __STDC__
tuiSetLayout (
	       TuiLayoutType layoutType,
	       TuiRegisterDisplayType regsDisplayType)
#else
tuiSetLayout (layoutType, regsDisplayType)
     TuiLayoutType layoutType;
     TuiRegisterDisplayType regsDisplayType;
#endif
{
  TuiStatus status = TUI_SUCCESS;

  if (layoutType != UNDEFINED_LAYOUT || regsDisplayType != TUI_UNDEFINED_REGS)
    {
      TuiLayoutType curLayout = currentLayout (), newLayout = UNDEFINED_LAYOUT;
      int regsPopulate = FALSE;
      Opaque addr = _extractDisplayStartAddr ();
      TuiWinInfoPtr newWinWithFocus = (TuiWinInfoPtr) NULL, winWithFocus = tuiWinWithFocus ();
      TuiLayoutDefPtr layoutDef = tuiLayoutDef ();


      if (layoutType == UNDEFINED_LAYOUT &&
	  regsDisplayType != TUI_UNDEFINED_REGS)
	{
	  if (curLayout == SRC_DISASSEM_COMMAND)
	    newLayout = DISASSEM_DATA_COMMAND;
	  else if (curLayout == SRC_COMMAND || curLayout == SRC_DATA_COMMAND)
	    newLayout = SRC_DATA_COMMAND;
	  else if (curLayout == DISASSEM_COMMAND ||
		   curLayout == DISASSEM_DATA_COMMAND)
	    newLayout = DISASSEM_DATA_COMMAND;
	}
      else
	newLayout = layoutType;

      regsPopulate = (newLayout == SRC_DATA_COMMAND ||
		      newLayout == DISASSEM_DATA_COMMAND ||
		      regsDisplayType != TUI_UNDEFINED_REGS);
      if (newLayout != curLayout || regsDisplayType != TUI_UNDEFINED_REGS)
	{
	  if (newLayout != curLayout)
	    {
	      if (winWithFocus != cmdWin)
		tuiClearWinFocus ();
	      showLayout (newLayout);
	      /*
                ** Now determine where focus should be
                */
	      if (winWithFocus != cmdWin)
		{
		  switch (newLayout)
		    {
		    case SRC_COMMAND:
		      tuiSetWinFocusTo (srcWin);
		      layoutDef->displayMode = SRC_WIN;
		      layoutDef->split = FALSE;
		      break;
		    case DISASSEM_COMMAND:
		      /* the previous layout was not showing
                            ** code. this can happen if there is no
                            ** source available:
                            ** 1. if the source file is in another dir OR
                            ** 2. if target was compiled without -g
                            ** We still want to show the assembly though!
                            */
		      addr = vcatch_errors ((OpaqueFuncPtr)
					    tuiGetBeginAsmAddress);
		      tuiSetWinFocusTo (disassemWin);
		      layoutDef->displayMode = DISASSEM_WIN;
		      layoutDef->split = FALSE;
		      break;
		    case SRC_DISASSEM_COMMAND:
		      /* the previous layout was not showing
                            ** code. this can happen if there is no
                            ** source available:
                            ** 1. if the source file is in another dir OR
                            ** 2. if target was compiled without -g
                            ** We still want to show the assembly though!
                            */
		      addr = vcatch_errors ((OpaqueFuncPtr)
					    tuiGetBeginAsmAddress);
		      if (winWithFocus == srcWin)
			tuiSetWinFocusTo (srcWin);
		      else
			tuiSetWinFocusTo (disassemWin);
		      layoutDef->split = TRUE;
		      break;
		    case SRC_DATA_COMMAND:
		      if (winWithFocus != dataWin)
			tuiSetWinFocusTo (srcWin);
		      else
			tuiSetWinFocusTo (dataWin);
		      layoutDef->displayMode = SRC_WIN;
		      layoutDef->split = FALSE;
		      break;
		    case DISASSEM_DATA_COMMAND:
		      /* the previous layout was not showing
                            ** code. this can happen if there is no
                            ** source available:
                            ** 1. if the source file is in another dir OR
                            ** 2. if target was compiled without -g
                            ** We still want to show the assembly though!
                            */
		      addr = vcatch_errors ((OpaqueFuncPtr)
					    tuiGetBeginAsmAddress);
		      if (winWithFocus != dataWin)
			tuiSetWinFocusTo (disassemWin);
		      else
			tuiSetWinFocusTo (dataWin);
		      layoutDef->displayMode = DISASSEM_WIN;
		      layoutDef->split = FALSE;
		      break;
		    default:
		      break;
		    }
		}
	      if (newWinWithFocus != (TuiWinInfoPtr) NULL)
		tuiSetWinFocusTo (newWinWithFocus);
	      /*
                ** Now update the window content
                */
	      if (!regsPopulate &&
		  (newLayout == SRC_DATA_COMMAND ||
		   newLayout == DISASSEM_DATA_COMMAND))
		tuiDisplayAllData ();

	      tuiUpdateSourceWindowsWithAddr (addr);
	    }
	  if (regsPopulate)
	    {
	      layoutDef->regsDisplayType =
		(regsDisplayType == TUI_UNDEFINED_REGS ?
		 TUI_GENERAL_REGS : regsDisplayType);
	      tuiShowRegisters (layoutDef->regsDisplayType);
	    }
	}
    }
  else
    status = TUI_FAILURE;

  return status;
}				/* tuiSetLayout */


/*
** tui_vSetLayoutTo()
**        Function to set the layout to SRC, ASM, SPLIT, NEXT, PREV, DATA,
**        REGS, $REGS, $GREGS, $FREGS, $SREGS with arguments in a va_list
*/
TuiStatus
#ifdef __STDC__
tui_vSetLayoutTo (
		   va_list args)
#else
tui_vSetLayoutTo (args)
     va_list args;
#endif
{
  char *layoutName;

  layoutName = va_arg (args, char *);

  return (_tuiSetLayoutTo (layoutName));
}				/* tui_vSetLayoutTo */


/*
** tuiAddWinToLayout().
**        Add the specified window to the layout in a logical way.
**        This means setting up the most logical layout given the
**        window to be added.
*/
void
#ifdef __STDC__
tuiAddWinToLayout (
		    TuiWinType type)
#else
tuiAddWinToLayout (type)
     TuiWinType type;
#endif
{
  TuiLayoutType curLayout = currentLayout ();

  switch (type)
    {
    case SRC_WIN:
      if (curLayout != SRC_COMMAND &&
	  curLayout != SRC_DISASSEM_COMMAND &&
	  curLayout != SRC_DATA_COMMAND)
	{
	  clearSourceWindowsDetail ();
	  if (curLayout == DISASSEM_DATA_COMMAND)
	    showLayout (SRC_DATA_COMMAND);
	  else
	    showLayout (SRC_COMMAND);
	}
      break;
    case DISASSEM_WIN:
      if (curLayout != DISASSEM_COMMAND &&
	  curLayout != SRC_DISASSEM_COMMAND &&
	  curLayout != DISASSEM_DATA_COMMAND)
	{
	  clearSourceWindowsDetail ();
	  if (curLayout == SRC_DATA_COMMAND)
	    showLayout (DISASSEM_DATA_COMMAND);
	  else
	    showLayout (DISASSEM_COMMAND);
	}
      break;
    case DATA_WIN:
      if (curLayout != SRC_DATA_COMMAND &&
	  curLayout != DISASSEM_DATA_COMMAND)
	{
	  if (curLayout == DISASSEM_COMMAND)
	    showLayout (DISASSEM_DATA_COMMAND);
	  else
	    showLayout (SRC_DATA_COMMAND);
	}
      break;
    default:
      break;
    }

  return;
}				/* tuiAddWinToLayout */


/*
** tui_vAddWinToLayout().
**        Add the specified window to the layout in a logical way,
**        with arguments in a va_list.
*/
void
#ifdef __STDC__
tui_vAddWinToLayout (
		      va_list args)
#else
tui_vAddWinToLayout (args)
     va_list args;
#endif
{
  TuiWinType type = va_arg (args, TuiWinType);

  tuiAddWinToLayout (type);

  return;
}				/* tui_vAddWinToLayout */


/*
** tuiDefaultWinHeight().
**        Answer the height of a window.  If it hasn't been created yet,
**        answer what the height of a window would be based upon its
**        type and the layout.
*/
int
#ifdef __STDC__
tuiDefaultWinHeight (
		      TuiWinType type,
		      TuiLayoutType layout)
#else
tuiDefaultWinHeight (type, layout)
     TuiWinType type;
     TuiLayoutType layout;
#endif
{
  int h;

  if (winList[type] != (TuiWinInfoPtr) NULL)
    h = winList[type]->generic.height;
  else
    {
      switch (layout)
	{
	case SRC_COMMAND:
	case DISASSEM_COMMAND:
	  if (m_winPtrIsNull (cmdWin))
	    h = termHeight () / 2;
	  else
	    h = termHeight () - cmdWin->generic.height;
	  break;
	case SRC_DISASSEM_COMMAND:
	case SRC_DATA_COMMAND:
	case DISASSEM_DATA_COMMAND:
	  if (m_winPtrIsNull (cmdWin))
	    h = termHeight () / 3;
	  else
	    h = (termHeight () - cmdWin->generic.height) / 2;
	  break;
	default:
	  h = 0;
	  break;
	}
    }

  return h;
}				/* tuiDefaultWinHeight */


/*
** tuiDefaultWinViewportHeight().
**        Answer the height of a window.  If it hasn't been created yet,
**        answer what the height of a window would be based upon its
**        type and the layout.
*/
int
#ifdef __STDC__
tuiDefaultWinViewportHeight (
			      TuiWinType type,
			      TuiLayoutType layout)
#else
tuiDefaultWinViewportHeight (type, layout)
     TuiWinType type;
     TuiLayoutType layout;
#endif
{
  int h;

  h = tuiDefaultWinHeight (type, layout);

  if (winList[type] == cmdWin)
    h -= 1;
  else
    h -= 2;

  return h;
}				/* tuiDefaultWinViewportHeight */


/*
** _initialize_tuiLayout().
**        Function to initialize gdb commands, for tui window layout
**        manipulation.
*/
void
_initialize_tuiLayout ()
{
  if (tui_version)
    {
      add_com ("layout", class_tui, _tuiLayout_command,
	       "Change the layout of windows.\n\
Usage: layout prev | next | <layout_name> \n\
Layout names are:\n\
   src   : Displays source and command windows.\n\
   asm   : Displays disassembly and command windows.\n\
   split : Displays source, disassembly and command windows.\n\
   regs  : Displays register window. If existing layout\n\
           is source/command or assembly/command, the \n\
           register window is displayed. If the\n\
           source/assembly/command (split) is displayed, \n\
           the register window is displayed with \n\
           the window that has current logical focus.\n");
      if (xdb_commands)
	{
	  add_com ("td", class_tui, _tuiToggleLayout_command,
		   "Toggle between Source/Command and Disassembly/Command layouts.\n");
	  add_com ("ts", class_tui, _tuiToggleSplitLayout_command,
		   "Toggle between Source/Command or Disassembly/Command and \n\
Source/Disassembly/Command layouts.\n");
	}
    }

  return;
}				/* _intialize_tuiLayout */


/*************************
** STATIC LOCAL FUNCTIONS
**************************/


/*
** _tuiSetLayoutTo()
**    Function to set the layout to SRC, ASM, SPLIT, NEXT, PREV, DATA, REGS,
**        $REGS, $GREGS, $FREGS, $SREGS.
*/
static TuiStatus
#ifdef __STDC__
_tuiSetLayoutTo (
		  char *layoutName)
#else
_tuiSetLayoutTo (layoutName)
     char *layoutName;
#endif
{
  TuiStatus status = TUI_SUCCESS;

  if (layoutName != (char *) NULL)
    {
      register int i;
      register char *bufPtr;
      TuiLayoutType newLayout = UNDEFINED_LAYOUT;
      TuiRegisterDisplayType dpyType = TUI_UNDEFINED_REGS;
      TuiLayoutType curLayout = currentLayout ();

      bufPtr = (char *) tuiStrDup (layoutName);
      for (i = 0; (i < strlen (layoutName)); i++)
	bufPtr[i] = toupper (bufPtr[i]);

      /* First check for ambiguous input */
      if (strlen (bufPtr) <= 1 && (*bufPtr == 'S' || *bufPtr == '$'))
	{
	  warning ("Ambiguous command input.\n");
	  status = TUI_FAILURE;
	}
      else
	{
	  if (subsetCompare (bufPtr, "SRC"))
	    newLayout = SRC_COMMAND;
	  else if (subsetCompare (bufPtr, "ASM"))
	    newLayout = DISASSEM_COMMAND;
	  else if (subsetCompare (bufPtr, "SPLIT"))
	    newLayout = SRC_DISASSEM_COMMAND;
	  else if (subsetCompare (bufPtr, "REGS") ||
		   subsetCompare (bufPtr, TUI_GENERAL_SPECIAL_REGS_NAME) ||
		   subsetCompare (bufPtr, TUI_GENERAL_REGS_NAME) ||
		   subsetCompare (bufPtr, TUI_FLOAT_REGS_NAME) ||
		   subsetCompare (bufPtr, TUI_SPECIAL_REGS_NAME))
	    {
	      if (curLayout == SRC_COMMAND || curLayout == SRC_DATA_COMMAND)
		newLayout = SRC_DATA_COMMAND;
	      else
		newLayout = DISASSEM_DATA_COMMAND;

/* could ifdef out the following code. when compile with -z, there are null 
   pointer references that cause a core dump if 'layout regs' is the first 
   layout command issued by the user. HP has asked us to hook up this code 
   - edie epstein
 */
	      if (subsetCompare (bufPtr, TUI_FLOAT_REGS_NAME))
		{
		  if (dataWin->detail.dataDisplayInfo.regsDisplayType !=
		      TUI_SFLOAT_REGS &&
		      dataWin->detail.dataDisplayInfo.regsDisplayType !=
		      TUI_DFLOAT_REGS)
		    dpyType = TUI_SFLOAT_REGS;
		  else
		    dpyType =
		      dataWin->detail.dataDisplayInfo.regsDisplayType;
		}
	      else if (subsetCompare (bufPtr,
				      TUI_GENERAL_SPECIAL_REGS_NAME))
		dpyType = TUI_GENERAL_AND_SPECIAL_REGS;
	      else if (subsetCompare (bufPtr, TUI_GENERAL_REGS_NAME))
		dpyType = TUI_GENERAL_REGS;
	      else if (subsetCompare (bufPtr, TUI_SPECIAL_REGS_NAME))
		dpyType = TUI_SPECIAL_REGS;
	      else
		{
		  if (dataWin->detail.dataDisplayInfo.regsDisplayType !=
		      TUI_UNDEFINED_REGS)
		    dpyType =
		      dataWin->detail.dataDisplayInfo.regsDisplayType;
		  else
		    dpyType = TUI_GENERAL_REGS;
		}

/* end of potential ifdef 
 */

/* if ifdefed out code above, then assume that the user wishes to display the 
   general purpose registers 
 */

/*              dpyType = TUI_GENERAL_REGS; 
 */
	    }
	  else if (subsetCompare (bufPtr, "NEXT"))
	    newLayout = _nextLayout ();
	  else if (subsetCompare (bufPtr, "PREV"))
	    newLayout = _prevLayout ();
	  else
	    status = TUI_FAILURE;
	  free (bufPtr);

	  tuiSetLayout (newLayout, dpyType);
	}
    }
  else
    status = TUI_FAILURE;

  return status;
}				/* _tuiSetLayoutTo */


static Opaque
#ifdef __STDC__
_extractDisplayStartAddr (void)
#else
_extractDisplayStartAddr ()
#endif
{
  TuiLayoutType curLayout = currentLayout ();
  Opaque addr;

  switch (curLayout)
    {
    case SRC_COMMAND:
    case SRC_DATA_COMMAND:
      addr = (Opaque) find_line_pc (
				     current_source_symtab,
			  srcWin->detail.sourceInfo.startLineOrAddr.lineNo);
      break;
    case DISASSEM_COMMAND:
    case SRC_DISASSEM_COMMAND:
    case DISASSEM_DATA_COMMAND:
      addr = disassemWin->detail.sourceInfo.startLineOrAddr.addr;
      break;
    default:
      addr = (Opaque) NULL;
      break;
    }

  return addr;
}				/* _extractDisplayStartAddr */


static void
#ifdef __STDC__
_tuiHandleXDBLayout (
		      TuiLayoutDefPtr layoutDef)
#else
_tuiHandleXDBLayout (layoutDef)
     TuiLayoutDefPtr layoutDef;
#endif
{
  if (layoutDef->split)
    {
      tuiSetLayout (SRC_DISASSEM_COMMAND, TUI_UNDEFINED_REGS);
      tuiSetWinFocusTo (winList[layoutDef->displayMode]);
    }
  else
    {
      if (layoutDef->displayMode == SRC_WIN)
	tuiSetLayout (SRC_COMMAND, TUI_UNDEFINED_REGS);
      else
	tuiSetLayout (DISASSEM_DATA_COMMAND, layoutDef->regsDisplayType);
    }


  return;
}				/* _tuiHandleXDBLayout */


static void
#ifdef __STDC__
_tuiToggleLayout_command (
			   char *arg,
			   int fromTTY)
#else
_tuiToggleLayout_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vToggleLayout_command, arg, fromTTY);
}

static void
#ifdef __STDC__
_tui_vToggleLayout_command (
			     va_list args)
#else
_tui_vToggleLayout_command (args)
     va_list args;
#endif
{
  TuiLayoutDefPtr layoutDef = tuiLayoutDef ();

  if (layoutDef->displayMode == SRC_WIN)
    layoutDef->displayMode = DISASSEM_WIN;
  else
    layoutDef->displayMode = SRC_WIN;

  if (!layoutDef->split)
    _tuiHandleXDBLayout (layoutDef);

  return;
}				/* _tuiToggleLayout_command */


static void
#ifdef __STDC__
_tuiToggleSplitLayout_command (
				char *arg,
				int fromTTY)
#else
_tuiToggleSplitLayout_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vToggleSplitLayout_command, arg, fromTTY);
}

static void
#ifdef __STDC__
_tui_vToggleSplitLayout_command (
				  va_list args)
#else
_tui_vToggleSplitLayout_command (args)
     va_list args;
#endif
{
  TuiLayoutDefPtr layoutDef = tuiLayoutDef ();

  layoutDef->split = (!layoutDef->split);
  _tuiHandleXDBLayout (layoutDef);

  return;
}				/* _tui_vToggleSplitLayout_command */


static void
#ifdef __STDC__
_tuiLayout_command (
		     char *arg,
		     int fromTTY)
#else
_tuiLayout_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if ((TuiStatus) tuiDo (
		   (TuiOpaqueFuncPtr) tui_vSetLayoutTo, arg) != TUI_SUCCESS)
    warning ("Invalid layout specified.\n%s" LAYOUT_USAGE);

  return;
}				/* _tuiLayout_command */

/*
** _nextLayout().
**        Answer the previous layout to cycle to.
*/
static TuiLayoutType
#ifdef __STDC__
_nextLayout (void)
#else
_nextLayout ()
#endif
{
  TuiLayoutType newLayout;

  newLayout = currentLayout ();
  if (newLayout == UNDEFINED_LAYOUT)
    newLayout = SRC_COMMAND;
  else
    {
      newLayout++;
      if (newLayout == UNDEFINED_LAYOUT)
	newLayout = SRC_COMMAND;
    }

  return newLayout;
}				/* _nextLayout */


/*
** _prevLayout().
**        Answer the next layout to cycle to.
*/
static TuiLayoutType
#ifdef __STDC__
_prevLayout (void)
#else
_prevLayout ()
#endif
{
  TuiLayoutType newLayout;

  newLayout = currentLayout ();
  if (newLayout == SRC_COMMAND)
    newLayout = DISASSEM_DATA_COMMAND;
  else
    {
      newLayout--;
      if (newLayout == UNDEFINED_LAYOUT)
	newLayout = DISASSEM_DATA_COMMAND;
    }

  return newLayout;
}				/* _prevLayout */



/*
** _makeCommandWindow().
*/
static void
#ifdef __STDC__
_makeCommandWindow (
		     TuiWinInfoPtr * winInfoPtr,
		     int height,
		     int originY)
#else
_makeCommandWindow (winInfoPtr, height, originY)
     TuiWinInfoPtr *winInfoPtr;
     int height;
     int originY;
#endif
{
  _initAndMakeWin ((Opaque *) winInfoPtr,
		   CMD_WIN,
		   height,
		   termWidth (),
		   0,
		   originY,
		   DONT_BOX_WINDOW);

  (*winInfoPtr)->canHighlight = FALSE;

  return;
}				/* _makeCommandWindow */


/*
** _makeSourceWindow().
*/
static void
#ifdef __STDC__
_makeSourceWindow (
		    TuiWinInfoPtr * winInfoPtr,
		    int height,
		    int originY)
#else
_makeSourceWindow (winInfoPtr, height, originY)
     TuiWinInfoPtr *winInfoPtr;
     int height;
     int originY;
#endif
{
  _makeSourceOrDisassemWindow (winInfoPtr, SRC_WIN, height, originY);

  return;
}				/* _makeSourceWindow */


/*
** _makeDisassemWindow().
*/
static void
#ifdef __STDC__
_makeDisassemWindow (
		      TuiWinInfoPtr * winInfoPtr,
		      int height,
		      int originY)
#else
_makeDisassemWindow (winInfoPtr, height, originY)
     TuiWinInfoPtr *winInfoPtr;
     int height;
     int originY;
#endif
{
  _makeSourceOrDisassemWindow (winInfoPtr, DISASSEM_WIN, height, originY);

  return;
}				/* _makeDisassemWindow */


/*
** _makeDataWindow().
*/
static void
#ifdef __STDC__
_makeDataWindow (
		  TuiWinInfoPtr * winInfoPtr,
		  int height,
		  int originY)
#else
_makeDataWindow (winInfoPtr, height, originY)
     TuiWinInfoPtr *winInfoPtr;
     int height;
     int originY;
#endif
{
  _initAndMakeWin ((Opaque *) winInfoPtr,
		   DATA_WIN,
		   height,
		   termWidth (),
		   0,
		   originY,
		   BOX_WINDOW);

  return;
}				/* _makeDataWindow */



/*
**    _showSourceCommand().
**        Show the Source/Command layout
*/
static void
#ifdef __STDC__
_showSourceCommand (void)
#else
_showSourceCommand ()
#endif
{
  _showSourceOrDisassemAndCommand (SRC_COMMAND);

  return;
}				/* _showSourceCommand */


/*
**    _showDisassemCommand().
**        Show the Dissassem/Command layout
*/
static void
#ifdef __STDC__
_showDisassemCommand (void)
#else
_showDisassemCommand ()
#endif
{
  _showSourceOrDisassemAndCommand (DISASSEM_COMMAND);

  return;
}				/* _showDisassemCommand */


/*
**    _showSourceDisassemCommand().
**        Show the Source/Disassem/Command layout
*/
static void
#ifdef __STDC__
_showSourceDisassemCommand (void)
#else
_showSourceDisassemCommand ()
#endif
{
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();

  if (currentLayout () != SRC_DISASSEM_COMMAND)
    {
      int cmdHeight, srcHeight, asmHeight;

      if (m_winPtrNotNull (cmdWin))
	cmdHeight = cmdWin->generic.height;
      else
	cmdHeight = termHeight () / 3;

      srcHeight = (termHeight () - cmdHeight) / 2;
      asmHeight = termHeight () - (srcHeight + cmdHeight);

      if (m_winPtrIsNull (srcWin))
	_makeSourceWindow (&srcWin, srcHeight, 0);
      else
	{
	  _initGenWinInfo (&srcWin->generic,
			   srcWin->generic.type,
			   srcHeight,
			   srcWin->generic.width,
			   srcWin->detail.sourceInfo.executionInfo->width,
			   0);
	  srcWin->canHighlight = TRUE;
	  _initGenWinInfo (srcWin->detail.sourceInfo.executionInfo,
			   EXEC_INFO_WIN,
			   srcHeight,
			   3,
			   0,
			   0);
	  m_beVisible (srcWin);
	  m_beVisible (srcWin->detail.sourceInfo.executionInfo);
	  srcWin->detail.sourceInfo.hasLocator = FALSE;;
	}
      if (m_winPtrNotNull (srcWin))
	{
	  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();

	  tuiShowSourceContent (srcWin);
	  if (m_winPtrIsNull (disassemWin))
	    {
	      _makeDisassemWindow (&disassemWin, asmHeight, srcHeight - 1);
	      _initAndMakeWin ((Opaque *) & locator,
			       LOCATOR_WIN,
			       2 /* 1 */ ,
			       termWidth (),
			       0,
			       (srcHeight + asmHeight) - 1,
			       DONT_BOX_WINDOW);
	    }
	  else
	    {
	      _initGenWinInfo (locator,
			       LOCATOR_WIN,
			       2 /* 1 */ ,
			       termWidth (),
			       0,
			       (srcHeight + asmHeight) - 1);
	      disassemWin->detail.sourceInfo.hasLocator = TRUE;
	      _initGenWinInfo (
				&disassemWin->generic,
				disassemWin->generic.type,
				asmHeight,
				disassemWin->generic.width,
			disassemWin->detail.sourceInfo.executionInfo->width,
				srcHeight - 1);
	      _initGenWinInfo (disassemWin->detail.sourceInfo.executionInfo,
			       EXEC_INFO_WIN,
			       asmHeight,
			       3,
			       0,
			       srcHeight - 1);
	      disassemWin->canHighlight = TRUE;
	      m_beVisible (disassemWin);
	      m_beVisible (disassemWin->detail.sourceInfo.executionInfo);
	    }
	  if (m_winPtrNotNull (disassemWin))
	    {
	      srcWin->detail.sourceInfo.hasLocator = FALSE;
	      disassemWin->detail.sourceInfo.hasLocator = TRUE;
	      m_beVisible (locator);
	      tuiShowLocatorContent ();
	      tuiShowSourceContent (disassemWin);

	      if (m_winPtrIsNull (cmdWin))
		_makeCommandWindow (&cmdWin,
				    cmdHeight,
				    termHeight () - cmdHeight);
	      else
		{
		  _initGenWinInfo (&cmdWin->generic,
				   cmdWin->generic.type,
				   cmdWin->generic.height,
				   cmdWin->generic.width,
				   0,
				   cmdWin->generic.origin.y);
		  cmdWin->canHighlight = FALSE;
		  m_beVisible (cmdWin);
		}
	      if (m_winPtrNotNull (cmdWin))
		tuiRefreshWin (&cmdWin->generic);
	    }
	}
      setCurrentLayoutTo (SRC_DISASSEM_COMMAND);
    }

  return;
}				/* _showSourceDisassemCommand */


/*
**    _showData().
**        Show the Source/Data/Command or the Dissassembly/Data/Command layout
*/
static void
#ifdef __STDC__
_showData (
	    TuiLayoutType newLayout)
#else
_showData (newLayout)
     TuiLayoutType newLayout;
#endif
{
  int totalHeight = (termHeight () - cmdWin->generic.height);
  int srcHeight, dataHeight;
  TuiWinType winType;
  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();


  dataHeight = totalHeight / 2;
  srcHeight = totalHeight - dataHeight;
  m_allBeInvisible ();
  m_beInvisible (locator);
  _makeDataWindow (&dataWin, dataHeight, 0);
  dataWin->canHighlight = TRUE;
  if (newLayout == SRC_DATA_COMMAND)
    winType = SRC_WIN;
  else
    winType = DISASSEM_WIN;
  if (m_winPtrIsNull (winList[winType]))
    {
      if (winType == SRC_WIN)
	_makeSourceWindow (&winList[winType], srcHeight, dataHeight - 1);
      else
	_makeDisassemWindow (&winList[winType], srcHeight, dataHeight - 1);
      _initAndMakeWin ((Opaque *) & locator,
		       LOCATOR_WIN,
		       2 /* 1 */ ,
		       termWidth (),
		       0,
		       totalHeight - 1,
		       DONT_BOX_WINDOW);
    }
  else
    {
      _initGenWinInfo (&winList[winType]->generic,
		       winList[winType]->generic.type,
		       srcHeight,
		       winList[winType]->generic.width,
		   winList[winType]->detail.sourceInfo.executionInfo->width,
		       dataHeight - 1);
      _initGenWinInfo (winList[winType]->detail.sourceInfo.executionInfo,
		       EXEC_INFO_WIN,
		       srcHeight,
		       3,
		       0,
		       dataHeight - 1);
      m_beVisible (winList[winType]);
      m_beVisible (winList[winType]->detail.sourceInfo.executionInfo);
      _initGenWinInfo (locator,
		       LOCATOR_WIN,
		       2 /* 1 */ ,
		       termWidth (),
		       0,
		       totalHeight - 1);
    }
  winList[winType]->detail.sourceInfo.hasLocator = TRUE;
  m_beVisible (locator);
  tuiShowLocatorContent ();
  addToSourceWindows (winList[winType]);
  setCurrentLayoutTo (newLayout);

  return;
}				/* _showData */

/*
** _initGenWinInfo().
*/
static void
#ifdef __STDC__
_initGenWinInfo (
		  TuiGenWinInfoPtr winInfo,
		  TuiWinType type,
		  int height,
		  int width,
		  int originX,
		  int originY)
#else
_initGenWinInfo (winInfo, type, height, width, originX, originY)
     TuiGenWinInfoPtr winInfo;
     TuiWinType type;
     int height;
     int width;
     int originX;
     int originY;
#endif
{
  int h = height;

  winInfo->type = type;
  winInfo->width = width;
  winInfo->height = h;
  if (h > 1)
    {
      winInfo->viewportHeight = h - 1;
      if (winInfo->type != CMD_WIN)
	winInfo->viewportHeight--;
    }
  else
    winInfo->viewportHeight = 1;
  winInfo->origin.x = originX;
  winInfo->origin.y = originY;

  return;
}				/* _initGenWinInfo */

/*
** _initAndMakeWin().
*/
static void
#ifdef __STDC__
_initAndMakeWin (
		  Opaque * winInfoPtr,
		  TuiWinType winType,
		  int height,
		  int width,
		  int originX,
		  int originY,
		  int boxIt)
#else
_initAndMakeWin (winInfoPtr, winType, height, width, originX, originY, boxIt)
     Opaque *winInfoPtr;
     TuiWinType winType;
     int height;
     int width;
     int originX;
     int originY;
     int boxIt;
#endif
{
  Opaque opaqueWinInfo = *winInfoPtr;
  TuiGenWinInfoPtr generic;

  if (opaqueWinInfo == (Opaque) NULL)
    {
      if (m_winIsAuxillary (winType))
	opaqueWinInfo = (Opaque) allocGenericWinInfo ();
      else
	opaqueWinInfo = (Opaque) allocWinInfo (winType);
    }
  if (m_winIsAuxillary (winType))
    generic = (TuiGenWinInfoPtr) opaqueWinInfo;
  else
    generic = &((TuiWinInfoPtr) opaqueWinInfo)->generic;

  if (opaqueWinInfo != (Opaque) NULL)
    {
      _initGenWinInfo (generic, winType, height, width, originX, originY);
      if (!m_winIsAuxillary (winType))
	{
	  if (generic->type == CMD_WIN)
	    ((TuiWinInfoPtr) opaqueWinInfo)->canHighlight = FALSE;
	  else
	    ((TuiWinInfoPtr) opaqueWinInfo)->canHighlight = TRUE;
	}
      makeWindow (generic, boxIt);
      if (winType == LOCATOR_WIN)
	tuiClearLocatorDisplay ();
      echo ();
    }
  *winInfoPtr = opaqueWinInfo;

  return;
}				/* _initAndMakeWin */


/*
** _makeSourceOrDisassemWindow().
*/
static void
#ifdef __STDC__
_makeSourceOrDisassemWindow (
			      TuiWinInfoPtr * winInfoPtr,
			      TuiWinType type,
			      int height,
			      int originY)
#else
_makeSourceOrDisassemWindow (winInfoPtr, type, height, originY)
     TuiWinInfoPtr *winInfoPtr;
     TuiWinType type;
     int height;
     int originY;
#endif
{
  TuiGenWinInfoPtr executionInfo = (TuiGenWinInfoPtr) NULL;

  /*
    ** Create the exeuction info window.
    */
  if (type == SRC_WIN)
    executionInfo = sourceExecInfoWinPtr ();
  else
    executionInfo = disassemExecInfoWinPtr ();
  _initAndMakeWin ((Opaque *) & executionInfo,
		   EXEC_INFO_WIN,
		   height,
		   3,
		   0,
		   originY,
		   DONT_BOX_WINDOW);
  /*
    ** Now create the source window.
    */
  _initAndMakeWin ((Opaque *) winInfoPtr,
		   type,
		   height,
		   termWidth () - executionInfo->width,
		   executionInfo->width,
		   originY,
		   BOX_WINDOW);

  (*winInfoPtr)->detail.sourceInfo.executionInfo = executionInfo;

  return;
}				/* _makeSourceOrDisassemWindow */


/*
**    _showSourceOrDisassemAndCommand().
**        Show the Source/Command or the Disassem layout
*/
static void
#ifdef __STDC__
_showSourceOrDisassemAndCommand (
				  TuiLayoutType layoutType)
#else
_showSourceOrDisassemAndCommand (layoutType)
     TuiLayoutType layoutType;
#endif
{
  if (currentLayout () != layoutType)
    {
      TuiWinInfoPtr *winInfoPtr;
      int areaLeft;
      int srcHeight, cmdHeight;
      TuiGenWinInfoPtr locator = locatorWinInfoPtr ();

      if (m_winPtrNotNull (cmdWin))
	cmdHeight = cmdWin->generic.height;
      else
	cmdHeight = termHeight () / 3;
      srcHeight = termHeight () - cmdHeight;


      if (layoutType == SRC_COMMAND)
	winInfoPtr = &srcWin;
      else
	winInfoPtr = &disassemWin;

      if (m_winPtrIsNull (*winInfoPtr))
	{
	  if (layoutType == SRC_COMMAND)
	    _makeSourceWindow (winInfoPtr, srcHeight - 1, 0);
	  else
	    _makeDisassemWindow (winInfoPtr, srcHeight - 1, 0);
	  _initAndMakeWin ((Opaque *) & locator,
			   LOCATOR_WIN,
			   2 /* 1 */ ,
			   termWidth (),
			   0,
			   srcHeight - 1,
			   DONT_BOX_WINDOW);
	}
      else
	{
	  _initGenWinInfo (locator,
			   LOCATOR_WIN,
			   2 /* 1 */ ,
			   termWidth (),
			   0,
			   srcHeight - 1);
	  (*winInfoPtr)->detail.sourceInfo.hasLocator = TRUE;
	  _initGenWinInfo (
			    &(*winInfoPtr)->generic,
			    (*winInfoPtr)->generic.type,
			    srcHeight - 1,
			    (*winInfoPtr)->generic.width,
		      (*winInfoPtr)->detail.sourceInfo.executionInfo->width,
			    0);
	  _initGenWinInfo ((*winInfoPtr)->detail.sourceInfo.executionInfo,
			   EXEC_INFO_WIN,
			   srcHeight - 1,
			   3,
			   0,
			   0);
	  (*winInfoPtr)->canHighlight = TRUE;
	  m_beVisible (*winInfoPtr);
	  m_beVisible ((*winInfoPtr)->detail.sourceInfo.executionInfo);
	}
      if (m_winPtrNotNull (*winInfoPtr))
	{
	  (*winInfoPtr)->detail.sourceInfo.hasLocator = TRUE;
	  m_beVisible (locator);
	  tuiShowLocatorContent ();
	  tuiShowSourceContent (*winInfoPtr);

	  if (m_winPtrIsNull (cmdWin))
	    {
	      _makeCommandWindow (&cmdWin, cmdHeight, srcHeight);
	      tuiRefreshWin (&cmdWin->generic);
	    }
	  else
	    {
	      _initGenWinInfo (&cmdWin->generic,
			       cmdWin->generic.type,
			       cmdWin->generic.height,
			       cmdWin->generic.width,
			       cmdWin->generic.origin.x,
			       cmdWin->generic.origin.y);
	      cmdWin->canHighlight = FALSE;
	      m_beVisible (cmdWin);
	    }
	}
      setCurrentLayoutTo (layoutType);
    }

  return;
}				/* _showSourceOrDisassemAndCommand */
