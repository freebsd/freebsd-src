/*
** tuiSource.c
**         This module contains functions for displaying source in the source window
*/

#include "defs.h"
#include <ctype.h>
#include "symtab.h"
#include "frame.h"
#include "breakpoint.h"

#include "tui.h"
#include "tuiData.h"
#include "tuiStack.h"
#include "tuiSourceWin.h"
#include "tuiSource.h"


/*****************************************
** EXTERNAL FUNCTION DECLS                **
******************************************/

extern int open_source_file PARAMS ((struct symtab *));
extern void find_source_lines PARAMS ((struct symtab *, int));

/*****************************************
** EXTERNAL DATA DECLS                    **
******************************************/
extern int current_source_line;
extern struct symtab *current_source_symtab;


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/

static struct breakpoint *_hasBreak PARAMS ((char *, int));


/*****************************************
** STATIC LOCAL DATA                    **
******************************************/


/*****************************************
** PUBLIC FUNCTIONS                     **
******************************************/

/*********************************
** SOURCE/DISASSEM  FUNCTIONS    **
*********************************/

/*
** tuiSetSourceContent().
**    Function to display source in the source window.
*/
TuiStatus
#ifdef __STDC__
tuiSetSourceContent (
		      struct symtab *s,
		      int lineNo,
		      int noerror)
#else
tuiSetSourceContent (s, lineNo, noerror)
     struct symtab *s;
     int lineNo;
     int noerror;
#endif
{
  TuiStatus ret = TUI_FAILURE;

  if (s != (struct symtab *) NULL && s->filename != (char *) NULL)
    {
      register FILE *stream;
      register int i, desc, c, lineWidth, nlines;
      register char *srcLine;

      if ((ret = tuiAllocSourceBuffer (srcWin)) == TUI_SUCCESS)
	{
	  lineWidth = srcWin->generic.width - 1;
	  /*
            ** Take hilite (window border) into account, when calculating
            ** the number of lines
            */
	  nlines = (lineNo + (srcWin->generic.height - 2)) - lineNo;
	  desc = open_source_file (s);
	  if (desc < 0)
	    {
	      if (!noerror)
		{
		  char *name = alloca (strlen (s->filename) + 100);
		  sprintf (name, "%s:%d", s->filename, lineNo);
		  print_sys_errmsg (name, errno);
		}
	      ret = TUI_FAILURE;
	    }
	  else
	    {
	      if (s->line_charpos == 0)
		find_source_lines (s, desc);

	      if (lineNo < 1 || lineNo > s->nlines)
		{
		  close (desc);
		  printf_unfiltered (
			  "Line number %d out of range; %s has %d lines.\n",
				      lineNo, s->filename, s->nlines);
		}
	      else if (lseek (desc, s->line_charpos[lineNo - 1], 0) < 0)
		{
		  close (desc);
		  perror_with_name (s->filename);
		}
	      else
		{
		  register int offset, curLineNo, curLine, curLen, threshold;
		  TuiGenWinInfoPtr locator = locatorWinInfoPtr ();
		  /*
                    ** Determine the threshold for the length of the line
                    ** and the offset to start the display
                    */
		  offset = srcWin->detail.sourceInfo.horizontalOffset;
		  threshold = (lineWidth - 1) + offset;
		  stream = fdopen (desc, FOPEN_RT);
		  clearerr (stream);
		  curLine = 0;
		  curLineNo =
		    srcWin->detail.sourceInfo.startLineOrAddr.lineNo = lineNo;
		  if (offset > 0)
		    srcLine = (char *) xmalloc (
					   (threshold + 1) * sizeof (char));
		  while (curLine < nlines)
		    {
		      TuiWinElementPtr element = (TuiWinElementPtr)
		      srcWin->generic.content[curLine];
		      struct breakpoint *bp;

		      /* get the first character in the line */
		      c = fgetc (stream);

		      if (offset == 0)
			srcLine = ((TuiWinElementPtr)
				   srcWin->generic.content[
					curLine])->whichElement.source.line;
		      /* Init the line with the line number */
		      sprintf (srcLine, "%-6d", curLineNo);
		      curLen = strlen (srcLine);
		      i = curLen -
			((curLen / tuiDefaultTabLen ()) * tuiDefaultTabLen ());
		      while (i < tuiDefaultTabLen ())
			{
			  srcLine[curLen] = ' ';
			  i++;
			  curLen++;
			}
		      srcLine[curLen] = (char) 0;

		      /*
                        ** Set whether element is the execution point and
                        ** whether there is a break point on it.
                        */
		      element->whichElement.source.lineOrAddr.lineNo =
			curLineNo;
		      element->whichElement.source.isExecPoint =
			(strcmp (((TuiWinElementPtr)
			locator->content[0])->whichElement.locator.fileName,
				 s->filename) == 0
			 && curLineNo == ((TuiWinElementPtr)
			 locator->content[0])->whichElement.locator.lineNo);
		      bp = _hasBreak (s->filename, curLineNo);
		      element->whichElement.source.hasBreak =
			(bp != (struct breakpoint *) NULL &&
			 (!element->whichElement.source.isExecPoint ||
			  (bp->disposition != del || bp->hit_count <= 0)));
		      if (c != EOF)
			{
			  i = strlen (srcLine) - 1;
			  do
			    {
			      if ((c != '\n') &&
				  (c != '\r') && (++i < threshold))
				{
				  if (c < 040 && c != '\t')
				    {
				      srcLine[i++] = '^';
				      srcLine[i] = c + 0100;
				    }
				  else if (c == 0177)
				    {
				      srcLine[i++] = '^';
				      srcLine[i] = '?';
				    }
				  else
				    {	/*
                                        ** Store the charcter in the line
                                        ** buffer.  If it is a tab, then
                                        ** translate to the correct number of
                                        ** chars so we don't overwrite our
                                        ** buffer.
                                        */
				      if (c == '\t')
					{
					  int j, maxTabLen = tuiDefaultTabLen ();

					  for (j = i - (
					       (i / maxTabLen) * maxTabLen);
					       ((j < maxTabLen) &&
						i < threshold);
					       i++, j++)
					    srcLine[i] = ' ';
					  i--;
					}
				      else
					srcLine[i] = c;
				    }
				  srcLine[i + 1] = 0;
				}
			      else
				{	/*
                                    ** if we have not reached EOL, then eat
                                    ** chars until we do
                                    */
				  while (c != EOF && c != '\n' && c != '\r')
				    c = fgetc (stream);
				}
			    }
			  while (c != EOF && c != '\n' && c != '\r' &&
				 i < threshold && (c = fgetc (stream)));
			}
		      /* Now copy the line taking the offset into account */
		      if (strlen (srcLine) > offset)
			strcpy (((TuiWinElementPtr) srcWin->generic.content[
					curLine])->whichElement.source.line,
				&srcLine[offset]);
		      else
			((TuiWinElementPtr)
			 srcWin->generic.content[
			  curLine])->whichElement.source.line[0] = (char) 0;
		      curLine++;
		      curLineNo++;
		    }
		  if (offset > 0)
		    tuiFree (srcLine);
		  fclose (stream);
		  srcWin->generic.contentSize = nlines;
		  ret = TUI_SUCCESS;
		}
	    }
	}
    }
  return ret;
}				/* tuiSetSourceContent */


/* elz: this function sets the contents of the source window to empty
   except for a line in the middle with a warning message about the
   source not being available. This function is called by
   tuiEraseSourceContents, which in turn is invoked when the source files
   cannot be accessed*/

void
#ifdef __STDC__
tuiSetSourceContentNil (
			 TuiWinInfoPtr winInfo,
			 char *warning_string)
#else
tuiSetSourceContentNil (winInfo, warning_string)
     TuiWinInfoPtr winInfo;
     char *warning_string;
#endif
{
  int lineWidth;
  int nLines;
  int curr_line = 0;

  lineWidth = winInfo->generic.width - 1;
  nLines = winInfo->generic.height - 2;

  /* set to empty each line in the window, except for the one
    which contains the message*/
  while (curr_line < winInfo->generic.contentSize)
    {
      /* set the information related to each displayed line
     to null: i.e. the line number is 0, there is no bp,
     it is not where the program is stopped */

      TuiWinElementPtr element =
      (TuiWinElementPtr) winInfo->generic.content[curr_line];
      element->whichElement.source.lineOrAddr.lineNo = 0;
      element->whichElement.source.isExecPoint = FALSE;
      element->whichElement.source.hasBreak = FALSE;

      /* set the contents of the line to blank*/
      element->whichElement.source.line[0] = (char) 0;

      /* if the current line is in the middle of the screen, then we want to
     display the 'no source available' message in it.
     Note: the 'weird' arithmetic with the line width and height comes from
     the function tuiEraseSourceContent. We need to keep the screen and the
     window's actual contents in synch */

      if (curr_line == (nLines / 2 + 1))
	{
	  int i;
	  int xpos;
	  int warning_length = strlen (warning_string);
	  char *srcLine;

	  srcLine = element->whichElement.source.line;

	  if (warning_length >= ((lineWidth - 1) / 2))
	    xpos = 1;
	  else
	    xpos = (lineWidth - 1) / 2 - warning_length;

	  for (i = 0; i < xpos; i++)
	    srcLine[i] = ' ';

	  sprintf (srcLine + i, "%s", warning_string);

	  for (i = xpos + warning_length; i < lineWidth; i++)
	    srcLine[i] = ' ';

	  srcLine[i] = '\n';

	}			/* end if */

      curr_line++;

    }				/* end while*/

}				/*tuiSetSourceContentNil*/




/*
** tuiShowSource().
**        Function to display source in the source window.  This function
**        initializes the horizontal scroll to 0.
*/
void
#ifdef __STDC__
tuiShowSource (
		struct symtab *s,
		Opaque line,
		int noerror)
#else
tuiShowSource (s, line, noerror)
     struct symtab *s;
     Opaque line;
     int noerror;
#endif
{
  srcWin->detail.sourceInfo.horizontalOffset = 0;
  m_tuiShowSourceAsIs (s, line, noerror);

  return;
}				/* tuiShowSource */


/*
** tuiSourceIsDisplayed().
**        Answer whether the source is currently displayed in the source window.
*/
int
#ifdef __STDC__
tuiSourceIsDisplayed (
		       char *fname)
#else
tuiSourceIsDisplayed (fname)
     char *fname;
#endif
{
  return (srcWin->generic.contentInUse &&
	  (strcmp (((TuiWinElementPtr) (locatorWinInfoPtr ())->
		  content[0])->whichElement.locator.fileName, fname) == 0));
}				/* tuiSourceIsDisplayed */


/*
** tuiVerticalSourceScroll().
**      Scroll the source forward or backward vertically
*/
void
#ifdef __STDC__
tuiVerticalSourceScroll (
			  TuiScrollDirection scrollDirection,
			  int numToScroll)
#else
tuiVerticalSourceScroll (scrollDirection, numToScroll)
     TuiScrollDirection scrollDirection;
     int numToScroll;
#endif
{
  if (srcWin->generic.content != (OpaquePtr) NULL)
    {
      int line;
      Opaque addr;
      struct symtab *s;
      TuiWinContent content = (TuiWinContent) srcWin->generic.content;

      if (current_source_symtab == (struct symtab *) NULL)
	s = find_pc_symtab (selected_frame->pc);
      else
	s = current_source_symtab;

      if (scrollDirection == FORWARD_SCROLL)
	{
	  line = content[0]->whichElement.source.lineOrAddr.lineNo +
	    numToScroll;
	  if (line > s->nlines)
	    /*line = s->nlines - winInfo->generic.contentSize + 1;*/
	    /*elz: fix for dts 23398*/
	    line = content[0]->whichElement.source.lineOrAddr.lineNo;
	}
      else
	{
	  line = content[0]->whichElement.source.lineOrAddr.lineNo -
	    numToScroll;
	  if (line <= 0)
	    line = 1;
	}
      tuiUpdateSourceWindowAsIs (srcWin, s, (Opaque) line, FALSE);
    }

  return;
}				/* tuiVerticalSourceScroll */


/*****************************************
** STATIC LOCAL FUNCTIONS                 **
******************************************/

/*
** _hasBreak().
**        Answer whether there is a break point at the input line in
**        the source file indicated
*/
static struct breakpoint *
#ifdef __STDC__
_hasBreak (
	    char *sourceFileName,
	    int lineNo)
#else
_hasBreak (sourceFileName, lineNo)
     char *sourceFileName;
     int lineNo;
#endif
{
  struct breakpoint *bpWithBreak = (struct breakpoint *) NULL;
  struct breakpoint *bp;
  extern struct breakpoint *breakpoint_chain;


  for (bp = breakpoint_chain;
       (bp != (struct breakpoint *) NULL &&
	bpWithBreak == (struct breakpoint *) NULL);
       bp = bp->next)
    if ((strcmp (sourceFileName, bp->source_file) == 0) &&
	(lineNo == bp->line_number))
      bpWithBreak = bp;

  return bpWithBreak;
}				/* _hasBreak */
