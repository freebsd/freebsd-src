/*
** tuiDisassem.c
**         This module contains functions for handling disassembly display.
*/


#include "defs.h"
#include "symtab.h"
#include "breakpoint.h"
#include "frame.h"

#include "tui.h"
#include "tuiData.h"
#include "tuiLayout.h"
#include "tuiSourceWin.h"
#include "tuiStack.h"


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/

static struct breakpoint *_hasBreak PARAMS ((CORE_ADDR));


/*****************************************
** PUBLIC FUNCTIONS                        **
******************************************/

/*
** tuiSetDisassemContent().
**        Function to set the disassembly window's content.
*/
TuiStatus
#ifdef __STDC__
tuiSetDisassemContent (
		       struct symtab *s,
		       Opaque startAddr)
#else
     tuiSetDisassemContent (s, startAddr)
     struct symtab *s;
     Opaque startAddr;
#endif
{
  TuiStatus ret = TUI_FAILURE;
  GDB_FILE *gdb_dis_out;

  if (startAddr != (Opaque) NULL)
    {
      register int i, desc;

      if ((ret = tuiAllocSourceBuffer (disassemWin)) == TUI_SUCCESS)
	{
	  register int offset = disassemWin->detail.sourceInfo.horizontalOffset;
	  register int threshold, curLine = 0, lineWidth, maxLines;
	  CORE_ADDR newpc, pc;
	  disassemble_info asmInfo;
	  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
	  extern void strcat_address PARAMS ((CORE_ADDR, char *, int));
	  extern void strcat_address_numeric PARAMS ((CORE_ADDR, int, char *, int));
	  int curLen = 0;
	  int tab_len = tuiDefaultTabLen ();

	  maxLines = disassemWin->generic.height - 2;	/* account for hilite */
	  lineWidth = disassemWin->generic.width - 1;
	  threshold = (lineWidth - 1) + offset;

	  /* now init the gdb_file structure */
          gdb_dis_out = gdb_file_init_astring (threshold);

	  INIT_DISASSEMBLE_INFO_NO_ARCH (asmInfo, gdb_dis_out, (fprintf_ftype) fprintf_filtered);
	  asmInfo.read_memory_func = dis_asm_read_memory;
	  asmInfo.memory_error_func = dis_asm_memory_error;

	  disassemWin->detail.sourceInfo.startLineOrAddr.addr = startAddr;

	  /* Now construct each line */
	  for (curLine = 0, pc = (CORE_ADDR) startAddr; (curLine < maxLines);)
	    {
	      TuiWinElementPtr element = (TuiWinElementPtr)disassemWin->generic.content[curLine];
	      struct breakpoint *bp;

	      print_address (pc, gdb_dis_out);

	      curLen = strlen (gdb_file_get_strbuf (gdb_dis_out));
	      i = curLen - ((curLen / tab_len) * tab_len);

              /* adjust buffer length if necessary */
	      gdb_file_adjust_strbuf ((tab_len - i > 0) ? (tab_len - i ) : 0, gdb_dis_out);
		
	      /* Add spaces to make the instructions start onthe same column */
	      while (i < tab_len)
		{
		  gdb_file_get_strbuf (gdb_dis_out)[curLen] = ' ';
		  i++;
		  curLen++;
		}
	      gdb_file_get_strbuf (gdb_dis_out)[curLen] = '\0';

	      newpc = pc + ((*tm_print_insn) (pc, &asmInfo));

	      /* Now copy the line taking the offset into account */
	      if (strlen (gdb_file_get_strbuf (gdb_dis_out)) > offset)
		strcpy (element->whichElement.source.line,
			&(gdb_file_get_strbuf (gdb_dis_out)[offset]));
	      else
		element->whichElement.source.line[0] = '\0';
	      element->whichElement.source.lineOrAddr.addr = (Opaque) pc;
	      element->whichElement.source.isExecPoint =
		(pc == (CORE_ADDR) ((TuiWinElementPtr)locator->content[0])->whichElement.locator.addr);
	      bp = _hasBreak (pc);
	      element->whichElement.source.hasBreak =
		(bp != (struct breakpoint *) NULL &&
		 (!element->whichElement.source.isExecPoint ||
		  (bp->disposition != del || bp->hit_count <= 0)));
	      curLine++;
	      pc = newpc;
              /* reset the buffer to empty */
	      gdb_file_get_strbuf (gdb_dis_out)[0] = '\0';
	    }
	  gdb_file_deallocate (&gdb_dis_out);
	  disassemWin->generic.contentSize = curLine;
	  ret = TUI_SUCCESS;
	}
    }

  return ret;
}				/* tuiSetDisassemContent */


/*
** tuiShowDisassem().
**        Function to display the disassembly window with disassembled code.
*/
void
#ifdef __STDC__
tuiShowDisassem (
		  Opaque startAddr)
#else
tuiShowDisassem (startAddr)
     Opaque startAddr;
#endif
{
  struct symtab *s = find_pc_symtab ((CORE_ADDR) startAddr);
  TuiWinInfoPtr winWithFocus = tuiWinWithFocus ();

  tuiAddWinToLayout (DISASSEM_WIN);
  tuiUpdateSourceWindow (disassemWin, s, startAddr, FALSE);
  /*
    ** if the focus was in the src win, put it in the asm win, if the
    ** source view isn't split
    */
  if (currentLayout () != SRC_DISASSEM_COMMAND && winWithFocus == srcWin)
    tuiSetWinFocusTo (disassemWin);

  return;
}				/* tuiShowDisassem */


/*
** tuiShowDisassemAndUpdateSource().
**        Function to display the disassembly window.
*/
void
#ifdef __STDC__
tuiShowDisassemAndUpdateSource (
				 Opaque startAddr)
#else
tuiShowDisassemAndUpdateSource (startAddr)
     Opaque startAddr;
#endif
{
  struct symtab_and_line sal;

  tuiShowDisassem (startAddr);
  if (currentLayout () == SRC_DISASSEM_COMMAND)
    {
      TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
      /*
        ** Update what is in the source window if it is displayed too,
        ** note that it follows what is in the disassembly window and visa-versa
        */
      sal = find_pc_line ((CORE_ADDR) startAddr, 0);
      current_source_symtab = sal.symtab;
      tuiUpdateSourceWindow (srcWin, sal.symtab, (Opaque) sal.line, TRUE);
      tuiUpdateLocatorFilename (sal.symtab->filename);
    }

  return;
}				/* tuiShowDisassemAndUpdateSource */


/*
** tuiShowDisassemAsIs().
**        Function to display the disassembly window.  This function shows
**        the disassembly as specified by the horizontal offset.
*/
void
#ifdef __STDC__
tuiShowDisassemAsIs (
		      Opaque addr)
#else
tuiShowDisassemAsIs (addr)
     Opaque addr;
#endif
{
  tuiAddWinToLayout (DISASSEM_WIN);
  tuiUpdateSourceWindowAsIs (disassemWin, (struct symtab *) NULL, addr, FALSE);
  /*
    ** Update what is in the source window if it is displayed too, not that it
    ** follows what is in the disassembly window and visa-versa
    */
  if (currentLayout () == SRC_DISASSEM_COMMAND)
    tuiShowSourceContent (srcWin);	/*????  Need to do more? */

  return;
}				/* tuiShowDisassem */


/*
** tuiGetBeginAsmAddress().
*/
Opaque
#ifdef __STDC__
tuiGetBeginAsmAddress (void)
#else
tuiGetBeginAsmAddress ()
#endif
{
  TuiGenWinInfoPtr locator;
  TuiLocatorElementPtr element;
  Opaque addr;

  locator = locatorWinInfoPtr ();
  element = &((TuiWinElementPtr) locator->content[0])->whichElement.locator;

  if (element->addr == (Opaque) 0)
    {
      /*the target is not executing, because the pc is 0*/

      addr = (Opaque) parse_and_eval_address ("main");

      if (addr == (Opaque) 0)
	addr = (Opaque) parse_and_eval_address ("MAIN");

    }
  else				/* the target is executing */
    addr = element->addr;

  return addr;
}				/* tuiGetBeginAsmAddress */


/*
** tuiVerticalDisassemScroll().
**      Scroll the disassembly forward or backward vertically
*/
void
#ifdef __STDC__
tuiVerticalDisassemScroll (
			    TuiScrollDirection scrollDirection,
			    int numToScroll)
#else
tuiVerticalDisassemScroll (scrollDirection, numToScroll)
     TuiScrollDirection scrollDirection;
     int numToScroll;
#endif
{
  if (disassemWin->generic.content != (OpaquePtr) NULL)
    {
      Opaque pc, lowAddr;
      TuiWinContent content;
      struct symtab *s;

      content = (TuiWinContent) disassemWin->generic.content;
      if (current_source_symtab == (struct symtab *) NULL)
	s = find_pc_symtab (selected_frame->pc);
      else
	s = current_source_symtab;

      pc = content[0]->whichElement.source.lineOrAddr.addr;
      if (find_pc_partial_function ((CORE_ADDR) pc,
				    (char **) NULL,
				    (CORE_ADDR *) & lowAddr,
				    (CORE_ADDR) NULL) == 0)
	error ("No function contains prgram counter for selected frame.\n");
      else
	{
	  register int line = 0;
	  register Opaque newLow;
	  bfd_byte buffer[4];

	  newLow = pc;
	  if (scrollDirection == FORWARD_SCROLL)
	    {
	      for (; line < numToScroll; line++)
		newLow += sizeof (bfd_getb32 (buffer));
	    }
	  else
	    {
	      for (; newLow >= (Opaque) 0 && line < numToScroll; line++)
		newLow -= sizeof (bfd_getb32 (buffer));
	    }
	  tuiUpdateSourceWindowAsIs (disassemWin, s, newLow, FALSE);
	}
    }

  return;
}				/* tuiVerticalDisassemScroll */



/*****************************************
** STATIC LOCAL FUNCTIONS                 **
******************************************/
/*
** _hasBreak().
**      Answer whether there is a break point at the input line in the
**      source file indicated
*/
static struct breakpoint *
#ifdef __STDC__
_hasBreak (
	    CORE_ADDR addr)
#else
_hasBreak (addr)
     CORE_ADDR addr;
#endif
{
  struct breakpoint *bpWithBreak = (struct breakpoint *) NULL;
  struct breakpoint *bp;
  extern struct breakpoint *breakpoint_chain;


  for (bp = breakpoint_chain;
       (bp != (struct breakpoint *) NULL &&
	bpWithBreak == (struct breakpoint *) NULL);
       bp = bp->next)
    if (addr == bp->address)
      bpWithBreak = bp;

  return bpWithBreak;
}				/* _hasBreak */
