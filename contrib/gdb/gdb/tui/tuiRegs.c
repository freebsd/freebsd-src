
/*
** tuiRegs.c
**         This module contains functions to support display of registers
**         in the data window.
*/


#include "defs.h"
#include "tui.h"
#include "tuiData.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "tuiLayout.h"
#include "tuiWin.h"


/*****************************************
** LOCAL DEFINITIONS                    **
******************************************/
#define DOUBLE_FLOAT_LABEL_WIDTH    6
#define DOUBLE_FLOAT_LABEL_FMT      "%6.6s: "
#define DOUBLE_FLOAT_VALUE_WIDTH    30	/*min of 16 but may be in sci notation */

#define SINGLE_FLOAT_LABEL_WIDTH    6
#define SINGLE_FLOAT_LABEL_FMT      "%6.6s: "
#define SINGLE_FLOAT_VALUE_WIDTH    25	/* min of 8 but may be in sci notation */

#define SINGLE_LABEL_WIDTH    10
#define SINGLE_LABEL_FMT      "%10.10s: "
#define SINGLE_VALUE_WIDTH    14/* minimum of 8 but may be in sci notation */

/* In the code HP gave Cygnus, this was actually a function call to a
   PA-specific function, which was supposed to determine whether the
   target was a 64-bit or 32-bit processor.  However, the 64-bit
   support wasn't complete, so we didn't merge that in, so we leave
   this here as a stub.  */
#define IS_64BIT 0

/*****************************************
** STATIC DATA                          **
******************************************/


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/
static TuiStatus _tuiSetRegsContent
  PARAMS ((int, int, struct frame_info *,
	   TuiRegisterDisplayType, int));
static char *_tuiRegisterName PARAMS ((int));
static TuiStatus _tuiGetRegisterRawValue
  PARAMS ((int, char *, struct frame_info *));
static void _tuiSetRegisterElement
  PARAMS ((int, struct frame_info *,
	   TuiDataElementPtr, int));
static void _tuiDisplayRegister
  PARAMS ((int, TuiGenWinInfoPtr, enum precision_type));
static void _tuiRegisterFormat
  PARAMS ((char *, int, int, TuiDataElementPtr,
	   enum precision_type));
static TuiStatus _tuiSetGeneralRegsContent PARAMS ((int));
static TuiStatus _tuiSetSpecialRegsContent PARAMS ((int));
static TuiStatus _tuiSetGeneralAndSpecialRegsContent PARAMS ((int));
static TuiStatus _tuiSetFloatRegsContent PARAMS ((TuiRegisterDisplayType, int));
static int _tuiRegValueHasChanged
  PARAMS ((TuiDataElementPtr, struct frame_info *,
	   char *));
static void _tuiShowFloat_command PARAMS ((char *, int));
static void _tuiShowGeneral_command PARAMS ((char *, int));
static void _tuiShowSpecial_command PARAMS ((char *, int));
static void _tui_vShowRegisters_commandSupport PARAMS ((va_list));
static void _tuiToggleFloatRegs_command PARAMS ((char *, int));
static void _tuiScrollRegsForward_command PARAMS ((char *, int));
static void _tuiScrollRegsBackward_command PARAMS ((char *, int));
static void _tui_vShowRegisters_commandSupport PARAMS ((va_list));



/*****************************************
** PUBLIC FUNCTIONS                     **
******************************************/

/*
** tuiLastRegsLineNo()
**        Answer the number of the last line in the regs display.
**        If there are no registers (-1) is returned.
*/
int
#ifdef __STDC__
tuiLastRegsLineNo (void)
#else
tuiLastRegsLineNo ()
#endif
{
  register int numLines = (-1);

  if (dataWin->detail.dataDisplayInfo.regsContentCount > 0)
    {
      numLines = (dataWin->detail.dataDisplayInfo.regsContentCount /
		  dataWin->detail.dataDisplayInfo.regsColumnCount);
      if (dataWin->detail.dataDisplayInfo.regsContentCount %
	  dataWin->detail.dataDisplayInfo.regsColumnCount)
	numLines++;
    }
  return numLines;
}				/* tuiLastRegsLineNo */


/*
** tuiLineFromRegElementNo()
**        Answer the line number that the register element at elementNo is
**        on.  If elementNo is greater than the number of register elements
**        there are, -1 is returned.
*/
int
#ifdef __STDC__
tuiLineFromRegElementNo (
			  int elementNo)
#else
tuiLineFromRegElementNo (elementNo)
     int elementNo;
#endif
{
  if (elementNo < dataWin->detail.dataDisplayInfo.regsContentCount)
    {
      int i, line = (-1);

      i = 1;
      while (line == (-1))
	{
	  if (elementNo <
	      (dataWin->detail.dataDisplayInfo.regsColumnCount * i))
	    line = i - 1;
	  else
	    i++;
	}

      return line;
    }
  else
    return (-1);
}				/* tuiLineFromRegElementNo */


/*
** tuiFirstRegElementNoInLine()
**        Answer the index of the first element in lineNo.  If lineNo is
**        past the register area (-1) is returned.
*/
int
#ifdef __STDC__
tuiFirstRegElementNoInLine (
			     int lineNo)
#else
tuiFirstRegElementNoInLine (lineNo)
     int lineNo;
#endif
{
  if ((lineNo * dataWin->detail.dataDisplayInfo.regsColumnCount)
      <= dataWin->detail.dataDisplayInfo.regsContentCount)
    return ((lineNo + 1) *
	    dataWin->detail.dataDisplayInfo.regsColumnCount) -
      dataWin->detail.dataDisplayInfo.regsColumnCount;
  else
    return (-1);
}				/* tuiFirstRegElementNoInLine */


/*
** tuiLastRegElementNoInLine()
**        Answer the index of the last element in lineNo.  If lineNo is past
**        the register area (-1) is returned.
*/
int
#ifdef __STDC__
tuiLastRegElementNoInLine (
			    int lineNo)
#else
tuiLastRegElementNoInLine (lineNo)
     int lineNo;
#endif
{
  if ((lineNo * dataWin->detail.dataDisplayInfo.regsColumnCount) <=
      dataWin->detail.dataDisplayInfo.regsContentCount)
    return ((lineNo + 1) *
	    dataWin->detail.dataDisplayInfo.regsColumnCount) - 1;
  else
    return (-1);
}				/* tuiLastRegElementNoInLine */


/*
** tuiCalculateRegsColumnCount
**        Calculate the number of columns that should be used to display
**        the registers.
*/
int
#ifdef __STDC__
tuiCalculateRegsColumnCount (
			      TuiRegisterDisplayType dpyType)
#else
tuiCalculateRegsColumnCount (dpyType)
     TuiRegisterDisplayType dpyType;
#endif
{
  int colCount, colWidth;

  if (IS_64BIT || dpyType == TUI_DFLOAT_REGS)
    colWidth = DOUBLE_FLOAT_VALUE_WIDTH + DOUBLE_FLOAT_LABEL_WIDTH;
  else
    {
      if (dpyType == TUI_SFLOAT_REGS)
	colWidth = SINGLE_FLOAT_VALUE_WIDTH + SINGLE_FLOAT_LABEL_WIDTH;
      else
	colWidth = SINGLE_VALUE_WIDTH + SINGLE_LABEL_WIDTH;
    }
  colCount = (dataWin->generic.width - 2) / colWidth;

  return colCount;
}				/* tuiCalulateRegsColumnCount */


/*
** tuiShowRegisters().
**        Show the registers int the data window as indicated by dpyType.
**        If there is any other registers being displayed, then they are
**        cleared.  What registers are displayed is dependent upon dpyType.
*/
void
#ifdef __STDC__
tuiShowRegisters (
		   TuiRegisterDisplayType dpyType)
#else
tuiShowRegisters (dpyType)
     TuiRegisterDisplayType dpyType;
#endif
{
  TuiStatus ret = TUI_FAILURE;
  int refreshValuesOnly = FALSE;

  /* Say that registers should be displayed, even if there is a problem */
  dataWin->detail.dataDisplayInfo.displayRegs = TRUE;

  if (target_has_registers)
    {
      refreshValuesOnly =
	(dpyType == dataWin->detail.dataDisplayInfo.regsDisplayType);
      switch (dpyType)
	{
	case TUI_GENERAL_REGS:
	  ret = _tuiSetGeneralRegsContent (refreshValuesOnly);
	  break;
	case TUI_SFLOAT_REGS:
	case TUI_DFLOAT_REGS:
	  ret = _tuiSetFloatRegsContent (dpyType, refreshValuesOnly);
	  break;

/* could ifdef out */

	case TUI_SPECIAL_REGS:
	  ret = _tuiSetSpecialRegsContent (refreshValuesOnly);
	  break;
	case TUI_GENERAL_AND_SPECIAL_REGS:
	  ret = _tuiSetGeneralAndSpecialRegsContent (refreshValuesOnly);
	  break;

/* end of potential if def */

	default:
	  break;
	}
    }
  if (ret == TUI_FAILURE)
    {
      dataWin->detail.dataDisplayInfo.regsDisplayType = TUI_UNDEFINED_REGS;
      tuiEraseDataContent (NO_REGS_STRING);
    }
  else
    {
      int i;

      /* Clear all notation of changed values */
      for (i = 0; (i < dataWin->detail.dataDisplayInfo.regsContentCount); i++)
	{
	  TuiGenWinInfoPtr dataItemWin;

	  dataItemWin = &dataWin->detail.dataDisplayInfo.
	    regsContent[i]->whichElement.dataWindow;
	  (&((TuiWinElementPtr)
	     dataItemWin->content[0])->whichElement.data)->highlight = FALSE;
	}
      dataWin->detail.dataDisplayInfo.regsDisplayType = dpyType;
      tuiDisplayAllData ();
    }
  (tuiLayoutDef ())->regsDisplayType = dpyType;

  return;
}				/* tuiShowRegisters */


/*
** tuiDisplayRegistersFrom().
**        Function to display the registers in the content from
**        'startElementNo' until the end of the register content or the
**        end of the display height.  No checking for displaying past
**        the end of the registers is done here.
*/
void
#ifdef __STDC__
tuiDisplayRegistersFrom (
			  int startElementNo)
#else
tuiDisplayRegistersFrom (startElementNo)
     int startElementNo;
#endif
{
  if (dataWin->detail.dataDisplayInfo.regsContent != (TuiWinContent) NULL &&
      dataWin->detail.dataDisplayInfo.regsContentCount > 0)
    {
      register int i = startElementNo;
      int j, valueCharsWide, charsWide, itemWinWidth, curY, labelWidth;
      enum precision_type precision;

      precision = (dataWin->detail.dataDisplayInfo.regsDisplayType
		   == TUI_DFLOAT_REGS) ?
	double_precision : unspecified_precision;
      if (IS_64BIT ||
	  dataWin->detail.dataDisplayInfo.regsDisplayType == TUI_DFLOAT_REGS)
	{
	  valueCharsWide = DOUBLE_FLOAT_VALUE_WIDTH;
	  labelWidth = DOUBLE_FLOAT_LABEL_WIDTH;
	}
      else
	{
	  if (dataWin->detail.dataDisplayInfo.regsDisplayType ==
	      TUI_SFLOAT_REGS)
	    {
	      valueCharsWide = SINGLE_FLOAT_VALUE_WIDTH;
	      labelWidth = SINGLE_FLOAT_LABEL_WIDTH;
	    }
	  else
	    {
	      valueCharsWide = SINGLE_VALUE_WIDTH;
	      labelWidth = SINGLE_LABEL_WIDTH;
	    }
	}
      itemWinWidth = valueCharsWide + labelWidth;
      /*
        ** Now create each data "sub" window, and write the display into it.
        */
      curY = 1;
      while (i < dataWin->detail.dataDisplayInfo.regsContentCount &&
	     curY <= dataWin->generic.viewportHeight)
	{
	  for (j = 0;
	       (j < dataWin->detail.dataDisplayInfo.regsColumnCount &&
		i < dataWin->detail.dataDisplayInfo.regsContentCount); j++)
	    {
	      TuiGenWinInfoPtr dataItemWin;
	      TuiDataElementPtr dataElementPtr;

	      /* create the window if necessary*/
	      dataItemWin = &dataWin->detail.dataDisplayInfo.
		regsContent[i]->whichElement.dataWindow;
	      dataElementPtr = &((TuiWinElementPtr)
				 dataItemWin->content[0])->whichElement.data;
	      if (dataItemWin->handle == (WINDOW *) NULL)
		{
		  dataItemWin->height = 1;
		  dataItemWin->width = (precision == double_precision) ?
		    itemWinWidth + 2 : itemWinWidth + 1;
		  dataItemWin->origin.x = (itemWinWidth * j) + 1;
		  dataItemWin->origin.y = curY;
		  makeWindow (dataItemWin, DONT_BOX_WINDOW);
		}
	      /*
                ** Get the printable representation of the register
                ** and display it
                */
	      _tuiDisplayRegister (
			    dataElementPtr->itemNo, dataItemWin, precision);
	      i++;		/* next register */
	    }
	  curY++;		/* next row; */
	}
    }

  return;
}				/* tuiDisplayRegistersFrom */


/*
** tuiDisplayRegElementAtLine().
**        Function to display the registers in the content from
**        'startElementNo' on 'startLineNo' until the end of the
**        register content or the end of the display height.
**        This function checks that we won't display off the end
**        of the register display.
*/
void
#ifdef __STDC__
tuiDisplayRegElementAtLine (
			     int startElementNo,
			     int startLineNo)
#else
tuiDisplayRegElementAtLine (startElementNo, startLineNo)
     int startElementNo;
     int startLineNo;
#endif
{
  if (dataWin->detail.dataDisplayInfo.regsContent != (TuiWinContent) NULL &&
      dataWin->detail.dataDisplayInfo.regsContentCount > 0)
    {
      register int elementNo = startElementNo;

      if (startElementNo != 0 && startLineNo != 0)
	{
	  register int lastLineNo, firstLineOnLastPage;

	  lastLineNo = tuiLastRegsLineNo ();
	  firstLineOnLastPage = lastLineNo - (dataWin->generic.height - 2);
	  if (firstLineOnLastPage < 0)
	    firstLineOnLastPage = 0;
	  /*
            ** If there is no other data displayed except registers,
            ** and the elementNo causes us to scroll past the end of the
            ** registers, adjust what element to really start the display at.
            */
	  if (dataWin->detail.dataDisplayInfo.dataContentCount <= 0 &&
	      startLineNo > firstLineOnLastPage)
	    elementNo = tuiFirstRegElementNoInLine (firstLineOnLastPage);
	}
      tuiDisplayRegistersFrom (elementNo);
    }

  return;
}				/* tuiDisplayRegElementAtLine */



/*
** tuiDisplayRegistersFromLine().
**        Function to display the registers starting at line lineNo in
**        the data window.  Answers the line number that the display
**        actually started from.  If nothing is displayed (-1) is returned.
*/
int
#ifdef __STDC__
tuiDisplayRegistersFromLine (
			      int lineNo,
			      int forceDisplay)
#else
tuiDisplayRegistersFromLine (lineNo, forceDisplay)
     int lineNo;
     int forceDisplay;
#endif
{
  int elementNo;

  if (dataWin->detail.dataDisplayInfo.regsContentCount > 0)
    {
      int line, elementNo;

      if (lineNo < 0)
	line = 0;
      else if (forceDisplay)
	{			/*
            ** If we must display regs (forceDisplay is true), then make
            ** sure that we don't display off the end of the registers.
            */
	  if (lineNo >= tuiLastRegsLineNo ())
	    {
	      if ((line = tuiLineFromRegElementNo (
		 dataWin->detail.dataDisplayInfo.regsContentCount - 1)) < 0)
		line = 0;
	    }
	  else
	    line = lineNo;
	}
      else
	line = lineNo;

      elementNo = tuiFirstRegElementNoInLine (line);
      if (elementNo < dataWin->detail.dataDisplayInfo.regsContentCount)
	tuiDisplayRegElementAtLine (elementNo, line);
      else
	line = (-1);

      return line;
    }

  return (-1);			/* nothing was displayed */
}				/* tuiDisplayRegistersFromLine */


/*
** tuiCheckRegisterValues()
**        This function check all displayed registers for changes in
**        values, given a particular frame.  If the values have changed,
**        they are updated with the new value and highlighted.
*/
void
#ifdef __STDC__
tuiCheckRegisterValues (
			 struct frame_info *frame)
#else
tuiCheckRegisterValues (frame)
     struct frame_info *frame;
#endif
{
  if (m_winPtrNotNull (dataWin) && dataWin->generic.isVisible)
    {
      if (dataWin->detail.dataDisplayInfo.regsContentCount <= 0 &&
	  dataWin->detail.dataDisplayInfo.displayRegs)
	tuiShowRegisters ((tuiLayoutDef ())->regsDisplayType);
      else
	{
	  int i, j;
	  char rawBuf[MAX_REGISTER_RAW_SIZE];

	  for (i = 0;
	       (i < dataWin->detail.dataDisplayInfo.regsContentCount); i++)
	    {
	      TuiDataElementPtr dataElementPtr;
	      TuiGenWinInfoPtr dataItemWinPtr;
	      int wasHilighted;

	      dataItemWinPtr = &dataWin->detail.dataDisplayInfo.
		regsContent[i]->whichElement.dataWindow;
	      dataElementPtr = &((TuiWinElementPtr)
			     dataItemWinPtr->content[0])->whichElement.data;
	      wasHilighted = dataElementPtr->highlight;
	      dataElementPtr->highlight =
		_tuiRegValueHasChanged (dataElementPtr, frame, &rawBuf[0]);
	      if (dataElementPtr->highlight)
		{
		  for (j = 0; j < MAX_REGISTER_RAW_SIZE; j++)
		    ((char *) dataElementPtr->value)[j] = rawBuf[j];
		  _tuiDisplayRegister (
					dataElementPtr->itemNo,
					dataItemWinPtr,
			((dataWin->detail.dataDisplayInfo.regsDisplayType ==
			  TUI_DFLOAT_REGS) ?
			 double_precision : unspecified_precision));
		}
	      else if (wasHilighted)
		{
		  dataElementPtr->highlight = FALSE;
		  _tuiDisplayRegister (
					dataElementPtr->itemNo,
					dataItemWinPtr,
			((dataWin->detail.dataDisplayInfo.regsDisplayType ==
			  TUI_DFLOAT_REGS) ?
			 double_precision : unspecified_precision));
		}
	    }
	}
    }
  return;
}				/* tuiCheckRegisterValues */


/*
** tuiToggleFloatRegs().
*/
void
#ifdef __STDC__
tuiToggleFloatRegs (void)
#else
tuiToggleFloatRegs ()
#endif
{
  TuiLayoutDefPtr layoutDef = tuiLayoutDef ();

  if (layoutDef->floatRegsDisplayType == TUI_SFLOAT_REGS)
    layoutDef->floatRegsDisplayType = TUI_DFLOAT_REGS;
  else
    layoutDef->floatRegsDisplayType = TUI_SFLOAT_REGS;

  if (m_winPtrNotNull (dataWin) && dataWin->generic.isVisible &&
      (dataWin->detail.dataDisplayInfo.regsDisplayType == TUI_SFLOAT_REGS ||
       dataWin->detail.dataDisplayInfo.regsDisplayType == TUI_DFLOAT_REGS))
    tuiShowRegisters (layoutDef->floatRegsDisplayType);

  return;
}				/* tuiToggleFloatRegs */


void
_initialize_tuiRegs ()
{
  if (tui_version && xdb_commands)
    {
      add_com ("fr", class_tui, _tuiShowFloat_command,
	       "Display only floating point registers\n");
      add_com ("gr", class_tui, _tuiShowGeneral_command,
	       "Display only general registers\n");
      add_com ("sr", class_tui, _tuiShowSpecial_command,
	       "Display only special registers\n");
      add_com ("+r", class_tui, _tuiScrollRegsForward_command,
	       "Scroll the registers window forward\n");
      add_com ("-r", class_tui, _tuiScrollRegsBackward_command,
	       "Scroll the register window backward\n");
      add_com ("tf", class_tui, _tuiToggleFloatRegs_command,
	       "Toggle between single and double precision floating point registers.\n");
      add_cmd (TUI_FLOAT_REGS_NAME_LOWER,
	       class_tui,
	       _tuiToggleFloatRegs_command,
	       "Toggle between single and double precision floating point \
registers.\n",
	       &togglelist);
    }

  return;
}				/* _initialize_tuiRegs */


/*****************************************
** STATIC LOCAL FUNCTIONS                 **
******************************************/


/*
** _tuiRegisterName().
**        Return the register name.
*/
static char *
#ifdef __STDC__
_tuiRegisterName (
		   int regNum)
#else
_tuiRegisterName (regNum)
     int regNum;
#endif
{
  if (reg_names[regNum] != (char *) NULL && *(reg_names[regNum]) != (char) 0)
    return reg_names[regNum];
  else
    return ((char *) NULL);
}				/* tuiGetRegisterName */


/*
** _tuiRegisterFormat
**        Function to format the register name and value into a buffer,
**        suitable for printing or display
*/
static void
#ifdef __STDC__
_tuiRegisterFormat (
		     char *buf,
		     int bufLen,
		     int regNum,
		     TuiDataElementPtr dataElement,
		     enum precision_type precision)
#else
_tuiRegisterFormat (buf, bufLen, regNum, dataElement, precision)
     char *buf;
     int bufLen;
     int regNum;
     TuiDataElementPtr dataElement;
     enum precision_type precision;
#endif
{
  char tmpBuf[15];
  char *fmt;
  GDB_FILE *stream;

  stream = gdb_file_init_astring(bufLen);
  pa_do_strcat_registers_info (regNum, 0, stream, precision);
  strcpy (buf, gdb_file_get_strbuf(stream));
  gdb_file_deallocate(&stream);

  return;
}				/* _tuiRegisterFormat */


#define NUM_GENERAL_REGS    32
/*
** _tuiSetGeneralRegsContent().
**      Set the content of the data window to consist of the general registers.
*/
static TuiStatus
#ifdef __STDC__
_tuiSetGeneralRegsContent (
			    int refreshValuesOnly)
#else
_tuiSetGeneralRegsContent (refreshValuesOnly)
     int refreshValuesOnly;
#endif
{
  return (_tuiSetRegsContent (0,
			      NUM_GENERAL_REGS - 1,
			      selected_frame,
			      TUI_GENERAL_REGS,
			      refreshValuesOnly));

}				/* _tuiSetGeneralRegsContent */


#define START_SPECIAL_REGS    PCOQ_HEAD_REGNUM
/*
** _tuiSetSpecialRegsContent().
**      Set the content of the data window to consist of the special registers.
*/
static TuiStatus
#ifdef __STDC__
_tuiSetSpecialRegsContent (
			    int refreshValuesOnly)
#else
_tuiSetSpecialRegsContent (refreshValuesOnly)
     int refreshValuesOnly;
#endif
{
  TuiStatus ret = TUI_FAILURE;
  int i, endRegNum;

  endRegNum = FP0_REGNUM - 1;
#if 0
  endRegNum = (-1);
  for (i = START_SPECIAL_REGS; (i < ARCH_NUM_REGS && endRegNum < 0); i++)
    if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT)
      endRegNum = i - 1;
#endif
  ret = _tuiSetRegsContent (START_SPECIAL_REGS,
			    endRegNum,
			    selected_frame,
			    TUI_SPECIAL_REGS,
			    refreshValuesOnly);

  return ret;
}				/* _tuiSetSpecialRegsContent */


/*
** _tuiSetGeneralAndSpecialRegsContent().
**      Set the content of the data window to consist of the special registers.
*/
static TuiStatus
#ifdef __STDC__
_tuiSetGeneralAndSpecialRegsContent (
				      int refreshValuesOnly)
#else
_tuiSetGeneralAndSpecialRegsContent (refreshValuesOnly)
     int refreshValuesOnly;
#endif
{
  TuiStatus ret = TUI_FAILURE;
  int i, endRegNum = (-1);

  endRegNum = FP0_REGNUM - 1;
#if 0
  endRegNum = (-1);
  for (i = 0; (i < ARCH_NUM_REGS && endRegNum < 0); i++)
    if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT)
      endRegNum = i - 1;
#endif
  ret = _tuiSetRegsContent (
	 0, endRegNum, selected_frame, TUI_SPECIAL_REGS, refreshValuesOnly);

  return ret;
}				/* _tuiSetGeneralAndSpecialRegsContent */

/*
** _tuiSetFloatRegsContent().
**        Set the content of the data window to consist of the float registers.
*/
static TuiStatus
#ifdef __STDC__
_tuiSetFloatRegsContent (
			  TuiRegisterDisplayType dpyType,
			  int refreshValuesOnly)
#else
_tuiSetFloatRegsContent (dpyType, refreshValuesOnly)
     TuiRegisterDisplayType dpyType;
     int refreshValuesOnly;
#endif
{
  TuiStatus ret = TUI_FAILURE;
  int i, startRegNum;

  startRegNum = FP0_REGNUM;
#if 0
  startRegNum = (-1);
  for (i = ARCH_NUM_REGS - 1; (i >= 0 && startRegNum < 0); i--)
    if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) != TYPE_CODE_FLT)
      startRegNum = i + 1;
#endif
  ret = _tuiSetRegsContent (startRegNum,
			    ARCH_NUM_REGS - 1,
			    selected_frame,
			    dpyType,
			    refreshValuesOnly);

  return ret;
}				/* _tuiSetFloatRegsContent */


/*
** _tuiRegValueHasChanged().
**        Answer TRUE if the register's value has changed, FALSE otherwise.
**        If TRUE, newValue is filled in with the new value.
*/
static int
#ifdef __STDC__
_tuiRegValueHasChanged (
			 TuiDataElementPtr dataElement,
			 struct frame_info *frame,
			 char *newValue)
#else
_tuiRegValueHasChanged (dataElement, frame, newValue)
     TuiDataElementPtr dataElement;
     struct frame_info *frame;
     char *newValue;
#endif
{
  int hasChanged = FALSE;

  if (dataElement->itemNo != UNDEFINED_ITEM &&
      _tuiRegisterName (dataElement->itemNo) != (char *) NULL)
    {
      char rawBuf[MAX_REGISTER_RAW_SIZE];
      int i;

      if (_tuiGetRegisterRawValue (
			 dataElement->itemNo, rawBuf, frame) == TUI_SUCCESS)
	{
	  for (i = 0; (i < MAX_REGISTER_RAW_SIZE && !hasChanged); i++)
	    hasChanged = (((char *) dataElement->value)[i] != rawBuf[i]);
	  if (hasChanged && newValue != (char *) NULL)
	    {
	      for (i = 0; (i < MAX_REGISTER_RAW_SIZE); i++)
		newValue[i] = rawBuf[i];
	    }
	}
    }
  return hasChanged;
}				/* _tuiRegValueHasChanged */



/*
** _tuiGetRegisterRawValue().
**        Get the register raw value.  The raw value is returned in regValue.
*/
static TuiStatus
#ifdef __STDC__
_tuiGetRegisterRawValue (
			  int regNum,
			  char *regValue,
			  struct frame_info *frame)
#else
_tuiGetRegisterRawValue (regNum, regValue, frame)
     int regNum;
     char *regValue;
     struct frame_info *frame;
#endif
{
  TuiStatus ret = TUI_FAILURE;

  if (target_has_registers)
    {
      read_relative_register_raw_bytes_for_frame (regNum, regValue, frame);
      ret = TUI_SUCCESS;
    }

  return ret;
}				/* _tuiGetRegisterRawValue */



/*
** _tuiSetRegisterElement().
**       Function to initialize a data element with the input and
**       the register value.
*/
static void
#ifdef __STDC__
_tuiSetRegisterElement (
			 int regNum,
			 struct frame_info *frame,
			 TuiDataElementPtr dataElement,
			 int refreshValueOnly)
#else
_tuiSetRegisterElement (regNum, frame, dataElement, refreshValueOnly)
     int regNum;
     struct frame_info *frame;
     TuiDataElementPtr dataElement;
     int refreshValueOnly;
#endif
{
  if (dataElement != (TuiDataElementPtr) NULL)
    {
      if (!refreshValueOnly)
	{
	  dataElement->itemNo = regNum;
	  dataElement->name = _tuiRegisterName (regNum);
	  dataElement->highlight = FALSE;
	}
      if (dataElement->value == (Opaque) NULL)
	dataElement->value = (Opaque) xmalloc (MAX_REGISTER_RAW_SIZE);
      if (dataElement->value != (Opaque) NULL)
	_tuiGetRegisterRawValue (regNum, dataElement->value, frame);
    }

  return;
}				/* _tuiSetRegisterElement */


/*
** _tuiSetRegsContent().
**        Set the content of the data window to consist of the registers
**        numbered from startRegNum to endRegNum.  Note that if
**        refreshValuesOnly is TRUE, startRegNum and endRegNum are ignored.
*/
static TuiStatus
#ifdef __STDC__
_tuiSetRegsContent (
		     int startRegNum,
		     int endRegNum,
		     struct frame_info *frame,
		     TuiRegisterDisplayType dpyType,
		     int refreshValuesOnly)
#else
_tuiSetRegsContent (startRegNum, endRegNum, frame, dpyType, refreshValuesOnly)
     int startRegNum;
     int endRegNum;
     struct frame_info *frame;
     TuiRegisterDisplayType dpyType;
     int refreshValuesOnly;
#endif
{
  TuiStatus ret = TUI_FAILURE;
  int numRegs = endRegNum - startRegNum + 1;
  int allocatedHere = FALSE;

  if (dataWin->detail.dataDisplayInfo.regsContentCount > 0 &&
      !refreshValuesOnly)
    {
      freeDataContent (dataWin->detail.dataDisplayInfo.regsContent,
		       dataWin->detail.dataDisplayInfo.regsContentCount);
      dataWin->detail.dataDisplayInfo.regsContentCount = 0;
    }
  if (dataWin->detail.dataDisplayInfo.regsContentCount <= 0)
    {
      dataWin->detail.dataDisplayInfo.regsContent =
	allocContent (numRegs, DATA_WIN);
      allocatedHere = TRUE;
    }

  if (dataWin->detail.dataDisplayInfo.regsContent != (TuiWinContent) NULL)
    {
      int i;

      if (!refreshValuesOnly || allocatedHere)
	{
	  dataWin->generic.content = (OpaquePtr) NULL;
	  dataWin->generic.contentSize = 0;
	  addContentElements (&dataWin->generic, numRegs);
	  dataWin->detail.dataDisplayInfo.regsContent =
	    (TuiWinContent) dataWin->generic.content;
	  dataWin->detail.dataDisplayInfo.regsContentCount = numRegs;
	}
      /*
        ** Now set the register names and values
        */
      for (i = startRegNum; (i <= endRegNum); i++)
	{
	  TuiGenWinInfoPtr dataItemWin;

	  dataItemWin = &dataWin->detail.dataDisplayInfo.
	    regsContent[i - startRegNum]->whichElement.dataWindow;
	  _tuiSetRegisterElement (
				   i,
				   frame,
	   &((TuiWinElementPtr) dataItemWin->content[0])->whichElement.data,
				   !allocatedHere && refreshValuesOnly);
	}
      dataWin->detail.dataDisplayInfo.regsColumnCount =
	tuiCalculateRegsColumnCount (dpyType);
#ifdef LATER
      if (dataWin->detail.dataDisplayInfo.dataContentCount > 0)
	{
	  /* delete all the windows? */
	  /* realloc content equal to dataContentCount + regsContentCount */
	  /* append dataWin->detail.dataDisplayInfo.dataContent to content */
	}
#endif
      dataWin->generic.contentSize =
	dataWin->detail.dataDisplayInfo.regsContentCount +
	dataWin->detail.dataDisplayInfo.dataContentCount;
      ret = TUI_SUCCESS;
    }

  return ret;
}				/* _tuiSetRegsContent */


/*
** _tuiDisplayRegister().
**        Function to display a register in a window.  If hilite is TRUE,
**        than the value will be displayed in reverse video
*/
static void
#ifdef __STDC__
_tuiDisplayRegister (
		      int regNum,
		      TuiGenWinInfoPtr winInfo,	/* the data item window */
		      enum precision_type precision)
#else
_tuiDisplayRegister (regNum, winInfo, precision)
     int regNum;
     TuiGenWinInfoPtr winInfo;	/* the data item window */
     enum precision_type precision;
#endif
{
  if (winInfo->handle != (WINDOW *) NULL)
    {
      char buf[100];
      int valueCharsWide, labelWidth;
      TuiDataElementPtr dataElementPtr = &((TuiWinContent)
				    winInfo->content)[0]->whichElement.data;

      if (IS_64BIT ||
	  dataWin->detail.dataDisplayInfo.regsDisplayType == TUI_DFLOAT_REGS)
	{
	  valueCharsWide = DOUBLE_FLOAT_VALUE_WIDTH;
	  labelWidth = DOUBLE_FLOAT_LABEL_WIDTH;
	}
      else
	{
	  if (dataWin->detail.dataDisplayInfo.regsDisplayType ==
	      TUI_SFLOAT_REGS)
	    {
	      valueCharsWide = SINGLE_FLOAT_VALUE_WIDTH;
	      labelWidth = SINGLE_FLOAT_LABEL_WIDTH;
	    }
	  else
	    {
	      valueCharsWide = SINGLE_VALUE_WIDTH;
	      labelWidth = SINGLE_LABEL_WIDTH;
	    }
	}

      buf[0] = (char) 0;
      _tuiRegisterFormat (buf,
			  valueCharsWide + labelWidth,
			  regNum,
			  dataElementPtr,
			  precision);
      if (dataElementPtr->highlight)
	wstandout (winInfo->handle);

      werase (winInfo->handle);
      wmove (winInfo->handle, 0, 0);
      waddstr (winInfo->handle, buf);

      if (dataElementPtr->highlight)
	wstandend (winInfo->handle);
      tuiRefreshWin (winInfo);
    }
  return;
}				/* _tuiDisplayRegister */


static void
#ifdef __STDC__
_tui_vShowRegisters_commandSupport (
				     va_list args)
#else
_tui_vShowRegisters_commandSupport (args)
     va_list args;
#endif
{
  TuiRegisterDisplayType dpyType = va_arg (args, TuiRegisterDisplayType);

  if (m_winPtrNotNull (dataWin) && dataWin->generic.isVisible)
    {				/* Data window already displayed, show the registers */
      if (dataWin->detail.dataDisplayInfo.regsDisplayType != dpyType)
	tuiShowRegisters (dpyType);
    }
  else
    (tuiLayoutDef ())->regsDisplayType = dpyType;

  return;
}				/* _tui_vShowRegisters_commandSupport */


static void
#ifdef __STDC__
_tuiShowFloat_command (
			char *arg,
			int fromTTY)
#else
_tuiShowFloat_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if (m_winPtrIsNull (dataWin) || !dataWin->generic.isVisible ||
      (dataWin->detail.dataDisplayInfo.regsDisplayType != TUI_SFLOAT_REGS &&
       dataWin->detail.dataDisplayInfo.regsDisplayType != TUI_DFLOAT_REGS))
    tuiDo ((TuiOpaqueFuncPtr) _tui_vShowRegisters_commandSupport,
	   (tuiLayoutDef ())->floatRegsDisplayType);

  return;
}				/* _tuiShowFloat_command */


static void
#ifdef __STDC__
_tuiShowGeneral_command (
			  char *arg,
			  int fromTTY)
#else
_tuiShowGeneral_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vShowRegisters_commandSupport,
	 TUI_GENERAL_REGS);

  return;
}				/* _tuiShowGeneral_command */


static void
#ifdef __STDC__
_tuiShowSpecial_command (
			  char *arg,
			  int fromTTY)
#else
_tuiShowSpecial_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) _tui_vShowRegisters_commandSupport,
	 TUI_SPECIAL_REGS);

  return;
}				/* _tuiShowSpecial_command */


static void
#ifdef __STDC__
_tuiToggleFloatRegs_command (
			      char *arg,
			      int fromTTY)
#else
_tuiToggleFloatRegs_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  if (m_winPtrNotNull (dataWin) && dataWin->generic.isVisible)
    tuiDo ((TuiOpaqueFuncPtr) tuiToggleFloatRegs);
  else
    {
      TuiLayoutDefPtr layoutDef = tuiLayoutDef ();

      if (layoutDef->floatRegsDisplayType == TUI_SFLOAT_REGS)
	layoutDef->floatRegsDisplayType = TUI_DFLOAT_REGS;
      else
	layoutDef->floatRegsDisplayType = TUI_SFLOAT_REGS;
    }


  return;
}				/* _tuiToggleFloatRegs_command */


static void
#ifdef __STDC__
_tuiScrollRegsForward_command (
				char *arg,
				int fromTTY)
#else
_tuiScrollRegsForward_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) tui_vScroll, FORWARD_SCROLL, dataWin, 1);

  return;
}				/* _tuiScrollRegsForward_command */


static void
#ifdef __STDC__
_tuiScrollRegsBackward_command (
				 char *arg,
				 int fromTTY)
#else
_tuiScrollRegsBackward_command (arg, fromTTY)
     char *arg;
     int fromTTY;
#endif
{
  tuiDo ((TuiOpaqueFuncPtr) tui_vScroll, BACKWARD_SCROLL, dataWin, 1);

  return;
}				/* _tuiScrollRegsBackward_command */
