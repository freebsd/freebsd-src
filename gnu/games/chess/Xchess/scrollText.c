/*
 * A Scrollable Text Output Window
 *
 * David Harrison 
 * University of California,  Berkeley
 * 1986
 *
 * The following is an implementation for a scrollable text output
 * system.  It handles exposure events only (other interactions are
 * under user control).  For scrolling,  a always present scroll bar
 * is implemented.  It detects size changes and compensates accordingly.
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/X10.h>
#include <sys/types.h>
#include "scrollText.h"

extern char *malloc();
extern char *realloc();
#define alloc(type)		(type *) malloc(sizeof(type))
#define numalloc(type, num)	(type *) malloc((unsigned) (num * sizeof(type)))
#define MAXINT		2147483647

extern XAssocTable *XCreateAssocTable();
extern caddr_t XLookUpAssoc();

static XAssocTable *textWindows = (XAssocTable *) 0;

#define NOOPTION	-1	/* Option hasn't been set yet                */
#define NORMSCROLL	0	/* Smooth scroll on LineToTop and TopToHere  */
#define JUMPSCROLL	1	/* Jump scrolling on LineToTop and TopToHere */

static int ScrollOption = NOOPTION;

typedef char *Generic;

#define DEFAULT_GC textInfo->fontGC[textInfo->curFont]

#define BARSIZE		15
#define BARBORDER	1
#define MAXFONTS	8
#define INITBUFSIZE	1024
#define INITLINES	50
#define INITEXPARY	50
#define XPADDING	2
#define YPADDING	2
#define INTERLINE	5
#define INTERSPACE	1
#define CURSORWIDTH	2
#define EXPANDPERCENT	40
#define BUFSIZE		1024
#define CUROFFSET	1
#define MAXFOREIGN	250
#define NOINDEX		-1

/* The wrap line indicator */
#define WRAPINDSIZE	7
#define STEMOFFSET	5
#define arrow_width 7
#define arrow_height 5
static char arrow_bits[] = {
   0x24, 0x26, 0x3f, 0x06, 0x04};

#define NEWLINE		'\n'
#define BACKSPACE	'\010'
#define NEWFONT		'\006'
#define LOWCHAR		'\040'
#define HIGHCHAR	'\176'

#define CHARMASK	0x00ff	/* Character mask */
#define FONTMASK	0x0700	/* Character font */
#define FONTSHIFT	8	/* Shift amount   */

#define WRAPFLAG	0x01	/* Line wrap flag */

/*
 * Lines are represented by a pointer into the overall array of
 * 16-bit characters.  The lower eight bits is used to indicate the character
 * (in ASCII),  and the next two bits are used to indicate the font
 * the character should be drawn in.
 */

typedef struct txtLine {
    int lineLength;		/* Current line length               */
    int lineHeight;		/* Full height of line in pixels     */
    int lineBaseLine;		/* Current baseline of the line      */
    int lineWidth;		/* Drawing position at end of line   */
    int lineText;		/* Offset into master buffer         */
    int lineFlags;		/* Line wrap flag is here            */
};


/*
 * For ExposeCopy events,  we queue up the redraw requests collapsing
 * them into line redraw requests until the CopyExpose event arrives.
 * The queue is represented as a dynamic array of the following
 * structure:
 */

typedef struct expEvent {
    int lineIndex;		/* Index of line to redraw  */
    int ypos;			/* Drawing position of line */
};


/*
 * The text buffer is represented using a dynamic counted array
 * of 16-bit quantities. This array expands as needed.
 * For the screen representation,  a dynamic counted array
 * of line structures is used.  This array points into the
 * text buffer to denote the start of each line and its parameters.
 * The windows are configured as one overall window which contains
 * the scroll bar as a sub-window along its right edge.  Thus,
 * the text drawing space is actually w-BARSIZE.
 */

#define NOTATBOTTOM	0x01	/* Need to scroll to bottom before appending */
#define FONTNUMWAIT	0x02	/* Waiting for font number                   */
#define COPYEXPOSE	0x04	/* Need to process a copy expose event       */
#define SCREENWRONG	0x08	/* TxtJamStr has invalidated screen contents */

typedef struct txtWin {
    /* Basic text buffer */
    int bufAlloc;		/* Allocated size of buffer           */
    int bufSpot;		/* Current writing position in buffer */
    short *mainBuffer;		/* Main buffer of text                */

    /* Line information */
    int numLines;		/* Number of display lines in buffer */
    int allocLines;		/* Number of lines allocated 	     */
    struct txtLine **txtBuffer;	/* Dynamic array of lines    	     */

    /* Current Window display information */
    Window mainWindow;		/* Text display window       */
    Window scrollBar;		/* Subwindow for scroll bar  */
    Pixmap arrowMap;		/* line wrap indicator       */
    int bgPix, fgPix;		/* Background and cursor     */
    GC CursorGC;		/* gc for the cursor         */
    GC bgGC;			/* gc for erasing things     */
    GC fontGC[MAXFONTS];	/* gc for doing fonts        */
    XFontStruct theFonts[MAXFONTS];/* Display fonts          */
    int  theColors[MAXFONTS];	/* foregrounds of the fonts  */
    int  curFont;		/* current font for tracking */
    int w, h;			/* Current size              */
    int startLine;		/* Top line in display       */
    int endLine;		/* Bottom line in display    */
    int bottomSpace;		/* Space at bottom of screen */
    int flagWord;		/* If non-zero,  not at end  */

    /* For handling ExposeCopy events */
    int exposeSize;		/* Current size of array      */
    int exposeAlloc;		/* Allocated size             */
    struct expEvent **exposeAry;/* Array of line indices      */

    /* Drawing position information */
    int curLine;		/* Current line in buffer    */
    int curX;			/* Current horizontal positi */
    int curY;			/* Current vertical drawing  */
};

/* Flags for the various basic character handling functions */

#define DODISP		0x01	/* Update the display  */
#define NONEWLINE	0x02	/* Dont append newline */



static int InitLine(newLine)
struct txtLine *newLine;	/* Newly created line structure */
/*
 * This routine initializes a newly created line structure.
 */
{
    newLine->lineLength = 0;
    newLine->lineHeight = 0;
    newLine->lineBaseLine = 0;
    newLine->lineWidth = XPADDING;
    newLine->lineText = NOINDEX;
    newLine->lineFlags = 0;
    return 1;
}




int TxtGrab(display, txtWin, program, mainFont, bg, fg, cur)
Display *display;		/* display window is on  */
Window txtWin;			/* Window to take over as scrollable text    */
char *program;			/* Program name for Xdefaults                */
XFontStruct *mainFont;		/* Primary text font                         */
int bg, fg, cur;		/* Background, foreground, and cursor colors */
/*
 * This routine takes control of 'txtWin' and makes it into a scrollable
 * text output window.  It will create a sub-window for the scroll bar
 * with a background of 'bg' and an bar with color 'fg'.  Both fixed width
 * and variable width fonts are supported.  Additional fonts can be loaded
 * using 'TxtAddFont'.  Returns 0 if there were problems,  non-zero if
 * everything went ok.
 */
{
    struct txtWin *newWin;	/* Text package specific information */
    XWindowAttributes winInfo;	/* Window information                */
    int index;
    XGCValues gc_val;
    
    if (textWindows == (XAssocTable *) 0) {
	textWindows = XCreateAssocTable(32);
	if (textWindows == (XAssocTable *) 0) return(0);
    }
    if (XGetWindowAttributes(display, txtWin, &winInfo) == 0) return 0;

    if (ScrollOption == NOOPTION) {
	/* Read to see if the user wants jump scrolling or not */
	if (XGetDefault(display, program, "JumpScroll")) {
	    ScrollOption = JUMPSCROLL;
	} else {
	    ScrollOption = NORMSCROLL;
	}
    }

    /* Initialize local structure */
    newWin = alloc(struct txtWin);

    /* Initialize arrow pixmap */
    newWin->arrowMap = XCreatePixmapFromBitmapData(display, txtWin,
						   arrow_bits,
						   arrow_width, arrow_height,
						   cur, bg,
						   DisplayPlanes(display, 0));

    newWin->bufAlloc = INITBUFSIZE;
    newWin->bufSpot = 0;
    newWin->mainBuffer = numalloc(short, INITBUFSIZE);

    newWin->numLines = 1;
    newWin->allocLines = INITLINES;
    newWin->txtBuffer = numalloc(struct txtLine *, INITLINES);
    for (index = 0;  index < INITLINES;  index++) {
	newWin->txtBuffer[index] = alloc(struct txtLine);
	InitLine(newWin->txtBuffer[index]);
    }

    /* Window display information */
    newWin->mainWindow = txtWin;
    newWin->w = winInfo.width;
    newWin->h = winInfo.height;
    newWin->startLine = 0;
    newWin->endLine = 0;
    newWin->bottomSpace = winInfo.height
      - YPADDING - mainFont->ascent - mainFont->descent - INTERLINE;
    newWin->flagWord = 0;
    newWin->bgPix = bg;
    newWin->fgPix = fg;

    /* Scroll Bar Creation */
    newWin->scrollBar = XCreateSimpleWindow(display, txtWin,
				      winInfo.width - BARSIZE,
				      0, BARSIZE - (2*BARBORDER),
				      winInfo.height - (2*BARBORDER),
				      BARBORDER, 
				      fg, bg);
    XSelectInput(display, newWin->scrollBar, ExposureMask|ButtonReleaseMask);
    XMapRaised(display, newWin->scrollBar);

    /* Font and Color Initialization */
    newWin->theFonts[0] = *mainFont;
    newWin->theColors[0] = fg;
    gc_val.function = GXcopy;
    gc_val.plane_mask = AllPlanes;
    gc_val.foreground = fg;
    gc_val.background = bg;
    gc_val.graphics_exposures = 1;
    gc_val.font = mainFont->fid;
    gc_val.line_width = 1;
    gc_val.line_style = LineSolid;

    newWin->fontGC[0] = XCreateGC(display, txtWin,
				  GCFunction | GCPlaneMask |
				  GCForeground | GCBackground |
				  GCGraphicsExposures | GCFont,
				  &gc_val);

    gc_val.foreground = cur;
    newWin->CursorGC = XCreateGC(display, txtWin,
				 GCFunction | GCPlaneMask |
				  GCForeground | GCBackground |
				  GCLineStyle | GCLineWidth,
				  &gc_val);

    gc_val.foreground = bg;
    newWin->bgGC = XCreateGC(display, txtWin,
				  GCFunction | GCPlaneMask |
				  GCForeground | GCBackground |
				  GCGraphicsExposures | GCFont,
				  &gc_val);


    for (index = 1;  index < MAXFONTS;  index++) {
	newWin->theFonts[index].fid = 0;
	newWin->fontGC[index] = 0;
    }

    
    /* Initialize size of first line */
    newWin->txtBuffer[0]->lineHeight = newWin->theFonts[0].ascent +
	newWin->theFonts[0].descent;
    newWin->txtBuffer[0]->lineText = 0;

    /* ExposeCopy array initialization */
    newWin->exposeSize = 0;
    newWin->exposeAlloc = INITEXPARY;
    newWin->exposeAry = numalloc(struct expEvent *, INITEXPARY);
    for (index = 0;  index < newWin->exposeAlloc;  index++)
      newWin->exposeAry[index] = alloc(struct expEvent);
    /* Put plus infinity in last slot for sorting purposes */
    newWin->exposeAry[0]->lineIndex = MAXINT;

    /* Drawing Position Information */
    newWin->curLine = 0;
    newWin->curX = 0;
    newWin->curY = YPADDING + mainFont->ascent + mainFont->descent;

    /* Attach it to both windows */
    XMakeAssoc(display, textWindows, (XID) txtWin, (caddr_t) newWin);
    XMakeAssoc(display, textWindows, (XID) newWin->scrollBar, (caddr_t) newWin);
    return 1;
}


int TxtRelease(display, w)
Display *display;
Window w;			/* Window to release */
/*
 * This routine releases all resources associated with the
 * specified window which are consumed by the text
 * window package. This includes the entire text buffer,  line start
 * array,  and the scroll bar window.  However,  the window
 * itself is NOT destroyed.  The routine will return zero if
 * the window is not owned by the text window package.
 */
{
    struct txtWin *textInfo;
    int index;

    if ((textInfo = (struct txtWin *) XLookUpAssoc(display,
						 textWindows, (XID) w)) == 0)
      return 0;

    for (index = 0; index < MAXFONTS; index++)
	if (textInfo->fontGC[index] != 0)
	    XFreeGC(display, textInfo->fontGC[index]);

    free((Generic) textInfo->mainBuffer);
    for (index = 0;  index < textInfo->numLines;  index++) {
	free((Generic) textInfo->txtBuffer[index]);
    }
    free((Generic) textInfo->txtBuffer);
    XDestroyWindow(display, textInfo->scrollBar);
    for (index = 0;  index < textInfo->exposeSize;  index++) {
	free((Generic) textInfo->exposeAry[index]);
    }
    free((Generic) textInfo->exposeAry);
    XDeleteAssoc(display, textWindows, (XID) w);
    free((Generic) textInfo);
    return 1;
}



static int RecompBuffer(textInfo)
struct txtWin *textInfo;	/* Text window information */
/*
 * This routine recomputes all line breaks in a buffer after
 * a change in window size or font.  This is done by throwing
 * away the old line start array and recomputing it.  Although
 * a lot of this work is also done elsewhere,  it has been included
 * inline here for efficiency.
 */
{
    int startPos, endSize, linenum;
    register int index, chsize, curfont;
    register short *bufptr;
    register XFontStruct *fontptr;
    register struct txtLine *lineptr;
    char theChar;

    /* Record the old position so we can come back to it */
    for (startPos = textInfo->txtBuffer[textInfo->startLine]->lineText;
	 (startPos > 0) && (textInfo->mainBuffer[startPos] != '\n');
	 startPos--)
      /* null loop body */;
    
    /* Clear out the old line start array */
    for (index = 0;  index < textInfo->numLines;  index++) {
	InitLine(textInfo->txtBuffer[index]);
    }

    /* Initialize first line */
    textInfo->txtBuffer[0]->lineHeight =
	textInfo->theFonts[0].ascent + textInfo->theFonts[0].descent;
    textInfo->txtBuffer[0]->lineText = 0;

    /* Process the text back into lines */
    endSize = textInfo->w - BARSIZE - WRAPINDSIZE;
    bufptr = textInfo->mainBuffer;
    lineptr = textInfo->txtBuffer[0];
    linenum = 0;
    fontptr = &(textInfo->theFonts[0]);
    curfont = 0;
    for (index = 0;  index < textInfo->bufSpot;  index++) {
	theChar = bufptr[index] & CHARMASK;
	
	if ((bufptr[index] & FONTMASK) != curfont) {
	    int newFontNum, heightDiff;

	    /* Switch fonts */
	    newFontNum = (bufptr[index] & FONTMASK) >> FONTSHIFT;
	    if (textInfo->theFonts[newFontNum].fid != 0) {
		/* Valid font */
		curfont = bufptr[index] & FONTMASK;
		fontptr = &(textInfo->theFonts[newFontNum]);
		heightDiff = (fontptr->ascent + fontptr->descent) -
		    lineptr->lineHeight;
		if (heightDiff < 0) heightDiff = 0;
		lineptr->lineHeight += heightDiff;
	    }
	}
	if (theChar == '\n') {
	    /* Handle new line */
	    if (linenum >= textInfo->allocLines-1)
	      /* Expand number of lines */
	      ExpandLines(textInfo);
	    linenum++;
	    lineptr = textInfo->txtBuffer[linenum];
	    /* Initialize next line */
	    lineptr->lineHeight = fontptr->ascent + fontptr->descent;
	    lineptr->lineText = index+1;
	    /* Check to see if its the starting line */
	    if (index == startPos) textInfo->startLine = linenum;
	} else {
	    /* Handle normal character */
	    chsize = CharSize(textInfo, linenum, index);
	    if (lineptr->lineWidth + chsize > endSize) {
		/* Handle line wrap */
		lineptr->lineFlags |= WRAPFLAG;
		if (linenum >= textInfo->allocLines-1)
		  /* Expand number of lines */
		  ExpandLines(textInfo);
		linenum++;
		lineptr = textInfo->txtBuffer[linenum];
		/* Initialize next line */
		lineptr->lineHeight = fontptr->ascent + fontptr->descent;
		lineptr->lineText = index;
		lineptr->lineLength = 1;
		lineptr->lineWidth += chsize;
	    } else {
		/* Handle normal addition of character */
		lineptr->lineLength += 1;
		lineptr->lineWidth += chsize;
	    }
	}
    }
    /* We now have a valid line array.  Let's clean up some other fields. */
    textInfo->numLines = linenum+1;
    if (startPos == 0) {
	textInfo->startLine = 0;
    }
    textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
    textInfo->curLine = linenum;
    /* Check to see if we are at the bottom */
    if (textInfo->endLine >= textInfo->numLines-1) {
	textInfo->curY = textInfo->h - textInfo->bottomSpace -
	  lineptr->lineHeight;
	textInfo->flagWord &= (~NOTATBOTTOM);
    } else {
	textInfo->flagWord |= NOTATBOTTOM;
    }
    return 1;
}




int TxtAddFont(display, textWin, fontNumber, newFont, newColor)
Display *display;
Window textWin;			/* Scrollable text window  */
int fontNumber;			/* Place to add font (0-7) */
XFontStruct *newFont;		/* Font to add             */
int newColor;			/* Color of font           */
/*
 * This routine loads a new font so that it can be used in a previously
 * created text window.  There are eight font slots numbered 0 through 7.
 * If there is already a font in the specified slot,  it will be replaced
 * and an automatic redraw of the window will take place.  See TxtWriteStr
 * for details on using alternate fonts.  The color specifies the foreground
 * color of the text.  The default foreground color is used if this
 * parameter is TXT_NO_COLOR.  Returns a non-zero value if
 * everything went well.
 */
{
    struct txtWin *textInfo;
    int redrawFlag;
    XGCValues gc_val;
    
    if ((fontNumber < 0) || (fontNumber >= MAXFONTS)) return 0;
    if ((textInfo = (struct txtWin *)
	 XLookUpAssoc(display, textWindows, (XID) textWin)) == 0)
      return 0;
    if (newColor == TXT_NO_COLOR) {
	newColor = textInfo->fgPix;
    }

    gc_val.font = newFont->fid;
    gc_val.foreground = newColor;
    gc_val.background = textInfo->bgPix;
    gc_val.plane_mask = AllPlanes;
    gc_val.graphics_exposures = 1;
    gc_val.function = GXcopy;
    
    if (textInfo->fontGC[fontNumber] != 0)
    {
	XChangeGC(display, textInfo->fontGC[fontNumber],
		  GCFont | GCForeground, &gc_val);
    }
    else
	textInfo->fontGC[fontNumber] = XCreateGC(display, textWin,
						 GCFont |
						 GCForeground |
						 GCBackground |
						 GCFunction |
						 GCPlaneMask |
						 GCGraphicsExposures,
						 &gc_val); 


    redrawFlag = (textInfo->theFonts[fontNumber].fid != 0) &&
      (((newFont) && (newFont->fid != textInfo->theFonts[fontNumber].fid)) ||
       (newColor != textInfo->theColors[fontNumber]));
    if (newFont) {
	textInfo->theFonts[fontNumber] = *newFont;
    }
    textInfo->theColors[fontNumber] = newColor;

    if (redrawFlag) {
	RecompBuffer(textInfo);
	XClearWindow(display, textWin);
	TxtRepaint(display, textWin);
    }
    return 1;
}



int TxtWinP(display, w)
Display *display;
Window w;
/*
 * Returns a non-zero value if the window has been previously grabbed
 * using TxtGrab and 0 if it has not.
 */
{
    if (XLookUpAssoc(display, textWindows, (XID) w))
      return(1);
    else return(0);
}



static int FindEndLine(textInfo, botSpace)
struct txtWin *textInfo;
int *botSpace;
/*
 * Given the starting line in 'textInfo->startLine',  this routine
 * determines the index of the last line that can be drawn given the
 * current size of the screen.  If there are not enough lines to
 * fill the screen,  the index of the last line will be returned.
 * The amount of empty bottom space is returned in 'botSpace'.
 */
{
    int index, height, lineHeight;

    height = YPADDING;
    index = textInfo->startLine;
    while (index < textInfo->numLines) {
	lineHeight = textInfo->txtBuffer[index]->lineHeight + INTERLINE;
	if (height + lineHeight > textInfo->h) break;
	height += lineHeight;
	index++;
    }
    if (botSpace) {
	*botSpace = textInfo->h - height;
    }
    return index - 1;
}



static int UpdateScroll(display, textInfo)
Display *display;
struct txtWin *textInfo;	/* Text window information */
/*
 * This routine computes the current extent of the scroll bar
 * indicator and repaints the bar with the correct information.
 */
{
    int top, bottom;

    if (textInfo->numLines > 1) {
	top = textInfo->startLine * (textInfo->h - 2*BARBORDER) /
	  (textInfo->numLines - 1);
	bottom = textInfo->endLine * (textInfo->h - 2*BARBORDER) /
	  (textInfo->numLines - 1);
    } else {
	top = 0;
	bottom = textInfo->h - (2*BARBORDER);
    }

    /* Draw it - make sure there is a little padding */
    if (top == 0) top++;
    if (bottom == textInfo->h-(2*BARBORDER)) bottom--;

    XFillRectangle(display, textInfo->scrollBar,
		   textInfo->bgGC, 
		   0, 0, BARSIZE, top-1);
    XFillRectangle(display, textInfo->scrollBar,
		   DEFAULT_GC, top, BARSIZE - (2*BARBORDER) - 2,
		   bottom - top);
    XFillRectangle(display, textInfo->scrollBar, DEFAULT_GC,
		   0, bottom+1, BARSIZE,
		   textInfo->h - (2 * BARBORDER) - bottom);

    return 1;
}




int TxtClear(display, w)
Display *display;
Window w;
/*
 * This routine clears a scrollable text window.  It resets the current
 * writing position to the upper left hand corner of the screen. 
 * NOTE:  THIS ALSO CLEARS THE CONTENTS OF THE TEXT WINDOW BUFFER AND
 * RESETS THE SCROLL BAR.  Returns 0 if the window is not a text window.
 * This should be used *instead* of XClear.
 */
{
    struct txtWin *textInfo;
    int index;

    if ((textInfo = (struct txtWin *) XLookUpAssoc(display, textWindows, (XID) w)) == 0)
      return 0;

    /* Zero out the arrays */
    textInfo->bufSpot = 0;
    for (index = 0;  index < textInfo->numLines;  index++) {
	InitLine(textInfo->txtBuffer[index]);
    }
    textInfo->txtBuffer[0]->lineHeight =
      textInfo->theFonts[textInfo->curFont].ascent +
	  textInfo->theFonts[textInfo->curFont].descent;

    textInfo->numLines = 1;
    textInfo->startLine = 0;
    textInfo->endLine = 0;
    textInfo->curLine = 0;
    textInfo->curX = 0;
    textInfo->curY = YPADDING + textInfo->theFonts[textInfo->curFont].ascent 
	+ textInfo->theFonts[textInfo->curFont].descent;

    textInfo->bottomSpace = textInfo->h - YPADDING -
      textInfo->theFonts[textInfo->curFont].ascent - INTERLINE -
	  textInfo->theFonts[textInfo->curFont].descent;
    /* Actually clear the window */
    XClearWindow(display, w);

    /* Draw the current cursor */
    XFillRectangle(display, w, textInfo->CursorGC,
		   XPADDING + CUROFFSET, textInfo->curY,
		   CURSORWIDTH,
		   textInfo->theFonts[textInfo->curFont].ascent +
		   textInfo->theFonts[textInfo->curFont].descent);

    /* Update the scroll bar */
    UpdateScroll(display, textInfo);
    return 1;
}


static int WarpToBottom(display, textInfo)
Display *display;
struct txtWin *textInfo;	/* Text Information */
/*
 * This routine causes the specified text window to display its
 * last screen of information.   It updates the scroll bar
 * to the appropriate spot.  The implementation scans backward
 * through the buffer to find an appropriate starting spot for
 * the window.
 */
{
    int index, height, lineHeight;

    index = textInfo->numLines-1;
    height = 0;
    while (index >= 0) {
	lineHeight = textInfo->txtBuffer[index]->lineHeight + INTERLINE;
	if (height + lineHeight > textInfo->h) break;
	height += lineHeight;
	index--;
    }
    textInfo->startLine = index + 1;
    textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
    textInfo->curY = textInfo->h - textInfo->bottomSpace -
      textInfo->txtBuffer[textInfo->endLine]->lineHeight;
    XClearWindow(display, textInfo->mainWindow);
    TxtRepaint(display, textInfo->mainWindow);
    return 1;
}



static int UpdateExposures(display, textInfo)
Display *display;
struct txtWin *textInfo;	/* Text window information */
/*
 * Before a new scrolling action occurs,  the text window package
 * must handle all COPYEXPOSE events generated by the last scrolling
 * action.  This routine is called to do this.  Foreign events (those
 * not handled by TxtFilter) are queued up and replaced on the queue
 * after the processing of the exposure events is complete.
 */
{
#if 0
    XEvent foreignQueue[MAXFOREIGN];
    int index, lastItem = 0;

    while (textInfo->flagWord & COPYEXPOSE) {
	XNextEvent(display, &(foreignQueue[lastItem]));
	if (!TxtFilter(display, &(foreignQueue[lastItem])))
	  lastItem++;
	if (lastItem >= MAXFOREIGN) {
	    printf("Too many foreign events to queue!\n");
	    textInfo->flagWord &= (~COPYEXPOSE);
	}
    }
    for (index = 0;  index < lastItem;  index++) {
	XPutBackEvent(display, &(foreignQueue[index]));
    }
#endif
    return 1;
}


static int ScrollDown(display,textInfo)
Display *display;
struct txtWin *textInfo;	/* Text window information */
/*
 * This routine scrolls the indicated text window down by one
 * line.  The line below the current line must exist.  The window
 * is scrolled so that the line below the last line is fully
 * displayed.  This may cause many lines to scroll off the top.
 * Scrolling is done using XCopyArea.  The exposure events should
 * be caught using ExposeCopy.
 */
{
    int lineSum, index, targetSpace, freeSpace, updateFlag;

    lineSum = 0;
    if (textInfo->endLine + 1 >= textInfo->numLines) return 0;
    targetSpace = textInfo->txtBuffer[textInfo->endLine+1]->lineHeight +
      INTERLINE;
    if (textInfo->bottomSpace < targetSpace) {
	index = textInfo->startLine;
	while (index < textInfo->endLine) {
	    lineSum += (textInfo->txtBuffer[index]->lineHeight + INTERLINE);
	    if (textInfo->bottomSpace + lineSum >= targetSpace) break;
	    index++;
	}

	/* Must move upward by 'lineSum' pixels */
	XCopyArea(display, textInfo->mainWindow, textInfo->mainWindow,
		  DEFAULT_GC, 0, lineSum,
		  textInfo->w - BARSIZE, textInfo->h,
		  0, 0);

	textInfo->flagWord |= COPYEXPOSE;
	/* Repair the damage to the structures */
	textInfo->startLine = index + 1;
	updateFlag = 1;
    } else {
	updateFlag = 0;
    }
    /* More lines might be able to fit.  Let's check. */
    freeSpace = textInfo->bottomSpace + lineSum - targetSpace;
    index = textInfo->endLine + 1;
    while (index < textInfo->numLines-1) {
	if (freeSpace - textInfo->txtBuffer[index+1]->lineHeight - INTERLINE < 0)
	  break;
	freeSpace -= (textInfo->txtBuffer[index+1]->lineHeight + INTERLINE);
	index++;
    }
    textInfo->endLine = index;
    textInfo->bottomSpace = freeSpace;
    if (updateFlag) {
	UpdateExposures(display, textInfo);
    }
    UpdateScroll(display, textInfo);
    return 1;
}




static int ExpandLines(textInfo)
struct txtWin *textInfo;	/* Text Information */
/*
 * This routine allocates and initializes additional space in
 * the line start array (txtBuffer).  The new space
 * is allocated using realloc.  The expansion factor is a percentage
 * given by EXPANDPERCENT.
 */
{
    int newSize, index;

    newSize = textInfo->allocLines;
    newSize += (newSize * EXPANDPERCENT) / 100;

    textInfo->txtBuffer = (struct txtLine **)
      realloc((char *) textInfo->txtBuffer,
	      (unsigned) (newSize * sizeof(struct txtLine *)));
    for (index = textInfo->allocLines;  index < newSize;  index++) {
	textInfo->txtBuffer[index] = alloc(struct txtLine);
	InitLine(textInfo->txtBuffer[index]);
    }
    textInfo->allocLines = newSize;
    return 1;
}

static int ExpandBuffer(textInfo)
struct txtWin *textInfo;	/* Text information */
/*
 * Expands the basic character buffer using realloc.  The expansion
 * factor is a percentage given by EXPANDPERCENT.
 */
{
    int newSize;

    newSize = textInfo->bufAlloc + (textInfo->bufAlloc * EXPANDPERCENT) / 100;
    textInfo->mainBuffer = (short *)
      realloc((char *) textInfo->mainBuffer, (unsigned) newSize * sizeof(short));
    textInfo->bufAlloc = newSize;
    return 1;
}



static int HandleNewLine(display, textInfo, flagWord)
Display *display;
struct txtWin *textInfo;	/* Text Information            */
int flagWord;			/* DODISP or NONEWLINE or both */
/*
 * This routine initializes the next line for drawing by setting
 * its height to the current font height,  scrolls the screen down
 * one line,  and updates the current drawing position to the
 * left edge of the newly cleared line.  If DODISP is specified,
 * the screen will be updated (otherwise not).  If NONEWLINE is
 * specified,  no newline character will be added to the text buffer
 * (this is for line wrap).
 */
{
    struct txtLine *curLine, *nextLine;

    /* Check to see if a new line must be allocated */
    if (textInfo->curLine >= textInfo->allocLines-1)
      /* Expand the number of lines */
      ExpandLines(textInfo);
    textInfo->numLines += 1;

    /* Then we initialize the next line */
    nextLine = textInfo->txtBuffer[textInfo->numLines-1];
    nextLine->lineHeight =
	textInfo->theFonts[textInfo->curFont].ascent +
	    textInfo->theFonts[textInfo->curFont].descent;

    curLine = textInfo->txtBuffer[textInfo->curLine];
    if (flagWord & DODISP) {
	/* Scroll down a line if required */
	if ((textInfo->curY + curLine->lineHeight +
	     nextLine->lineHeight + (INTERLINE * 2)) > textInfo->h)
	  {
	      ScrollDown(display, textInfo);
	  }
	else
	  {
	      /* Update the bottom space appropriately */
	      textInfo->bottomSpace -= (nextLine->lineHeight + INTERLINE);
	      textInfo->endLine += 1;
	  }
	/* Update drawing position */
	textInfo->curY = textInfo->h -
	  (textInfo->bottomSpace  + nextLine->lineHeight);
    }

    /* Move down a line */
    textInfo->curLine += 1;
    if (!(flagWord & NONEWLINE)) {
	/* Append end-of-line to text buffer */
	if (textInfo->bufSpot >= textInfo->bufAlloc) {
	    /* Allocate more space in main text buffer */
	    ExpandBuffer(textInfo);
	}
	textInfo->mainBuffer[(textInfo->bufSpot)++] =
	  (textInfo->curFont << FONTSHIFT) | '\n';
    }
    nextLine->lineText = textInfo->bufSpot;
    textInfo->curX = 0;
    return 1;
}



static int CharSize(textInfo, lineNum, charNum)
struct txtWin *textInfo;	/* Current Text Information */
int lineNum;			/* Line in buffer           */
int charNum;			/* Character in line        */
/*
 * This routine determines the size of the specified character.
 * It takes in account the font of the character and whether its
 * fixed or variable.  The size includes INTERSPACE spacing between
 * the characters.
 */
{
    register XFontStruct *charFont;
    register short *theLine;
    register short theChar;

    theLine = &(textInfo->mainBuffer[textInfo->txtBuffer[lineNum]->lineText]);
    theChar = theLine[charNum] & CHARMASK;
    charFont = &(textInfo->theFonts[(theChar & FONTMASK) >> FONTSHIFT]);
    if (theChar <= charFont->min_char_or_byte2 ||
	theChar >= charFont->max_char_or_byte2 ||
	charFont->per_char == 0)
	return  charFont->max_bounds.width + 1;
    else
	return charFont->per_char[theChar].width + 1;
}





static int HandleBackspace(display, textInfo, flagWord)
Display *display;
struct txtWin *textInfo;	/* Text Information  */
int flagWord;			/* DODISP or nothing */
/*
 * This routine handles a backspace found in the input stream.  The
 * character before the current writing position will be erased and
 * the drawing position will move back one character.  If the writing
 * position is at the left margin,  the drawing position will move
 * up to the previous line.  If it is a line that has been wrapped,
 * the character at the end of the previous line will be erased.
 */
{
    struct txtLine *thisLine, *prevLine;
    int chSize;

    thisLine = textInfo->txtBuffer[textInfo->curLine];
    /* First,  determine whether we need to go back a line */
    if (thisLine->lineLength == 0) {
	/* Bleep if at top of buffer */
	if (textInfo->curLine == 0) {
	    XBell(display, 50);
	    return 0;
	}

	/* See if we have to scroll in the other direction */
	if ((flagWord & DODISP) && (textInfo->curY <= YPADDING)) {
	    /* This will display the last lines of the buffer */
	    WarpToBottom(display, textInfo);
	}

	/* Set drawing position at end of previous line */
	textInfo->curLine -= 1;
	prevLine = textInfo->txtBuffer[textInfo->curLine];
	textInfo->numLines -= 1;
	if (flagWord & DODISP) {
	    textInfo->curY -= (prevLine->lineHeight + INTERLINE);
	    textInfo->bottomSpace += (thisLine->lineHeight + INTERLINE);
	    textInfo->endLine -= 1;
	}

	/* We are unlinewrapping if the previous line has flag set */
	if (prevLine->lineFlags & WRAPFLAG) {
	    /* Get rid of line wrap indicator */
	    if (flagWord & DODISP) {
		XFillRectangle(display, textInfo->mainWindow,
			       textInfo->bgGC,
			       textInfo->w - BARSIZE - WRAPINDSIZE,
			       textInfo->curY,  WRAPINDSIZE,
			       prevLine->lineHeight);
	    }
	    prevLine->lineFlags &= (~WRAPFLAG);
	    /* Call recursively to wipe out the ending character */
	    HandleBackspace(display, textInfo, flagWord);
	} else {
	    /* Delete the end-of-line in the primary buffer */
	    textInfo->bufSpot -= 1;
	}
    } else {
	/* Normal deletion of character */
	chSize =
	  CharSize(textInfo, textInfo->curLine,
		   textInfo->txtBuffer[textInfo->curLine]->lineLength - 1);
	/* Move back appropriate amount and wipe it out */
	thisLine->lineWidth -= chSize;
	if (flagWord & DODISP) {
	    XFillRectangle(display, textInfo->mainWindow,
			   textInfo->bgGC,
			   thisLine->lineWidth, textInfo->curY,
			   chSize, thisLine->lineHeight);
	}
	/* Delete from buffer */
	textInfo->txtBuffer[textInfo->curLine]->lineLength -= 1;
	textInfo->bufSpot -= 1;
    }
    return 1;
}



static int DrawLineWrap(display, win, x, y, h, col)
Display *display;
Window win;			/* What window to draw it in     */
int x, y;			/* Position of upper left corner */
int h;				/* Height of indicator           */
int col;			/* Color of indicator            */
/*
 * This routine draws a line wrap indicator at the end of a line.
 * Visually,  it is an arrow of the specified height directly against
 * the scroll bar border.  The bitmap used for the arrow is stored
 * in 'arrowMap' with size 'arrow_width' and 'arrow_height'.
 */
{
    struct txtWin *textInfo;

    textInfo = (struct txtWin *)XLookUpAssoc(display, textWindows,
					     (XID) win);

    /* First,  draw the arrow */
    XCopyArea(display, textInfo->arrowMap, textInfo->mainWindow,
	       textInfo->CursorGC,
	       0, 0, arrow_width, arrow_height,
	       x, y + h - arrow_height, 1);

    /* Then draw the stem */
    XDrawLine(display, textInfo->mainWindow, textInfo->CursorGC,
	      x + STEMOFFSET, y,
	      x + STEMOFFSET, y + h - arrow_height);
    return 1;
}




static int DrawLine(display, textInfo, lineIndex, ypos)
Display *display;
struct txtWin *textInfo;	/* Text window information   */
int lineIndex;			/* Index of line to draw     */
int ypos;			/* Y position for line       */
/*
 * This routine destructively draws the indicated line in the
 * indicated window at the indicated position.  It does not
 * clear to end of line however.  It draws a line wrap indicator
 * if needed but does not draw a cursor.
 */
{
    int index, startPos, curFont, theColor, curX, saveX, fontIndex;
    struct txtLine *someLine;
    char lineBuffer[BUFSIZE], *glyph;
    short *linePointer;
    XFontStruct *theFont;
    XGCValues gc;

    /* First,  we draw the text */
    index = 0;
    curX = XPADDING;
    someLine = textInfo->txtBuffer[lineIndex];
    linePointer = &(textInfo->mainBuffer[someLine->lineText]);
    while (index < someLine->lineLength) {
	startPos = index;
	saveX = curX;
	curFont = linePointer[index] & FONTMASK;
	fontIndex = curFont >> FONTSHIFT;
	theFont = &(textInfo->theFonts[fontIndex]);
	theColor = textInfo->theColors[fontIndex];
	glyph = &(lineBuffer[0]);
	while ((index < someLine->lineLength) &&
	       ((linePointer[index] & FONTMASK) == curFont))
	{
	    *glyph = linePointer[index] & CHARMASK;
	    index++;
	    curX += CharSize(textInfo, lineIndex, index);
	    glyph++;
	}
	
	/* Flush out the glyphs */
	XFillRectangle(display, textInfo->mainWindow,
		       textInfo->bgGC,
		       saveX, ypos,
		   textInfo->w - BARSIZE,
		   someLine->lineHeight + YPADDING + INTERLINE);

	XDrawString(display, textInfo->mainWindow,
		    textInfo->fontGC[fontIndex],
		    saveX, ypos,
		    lineBuffer, someLine->lineLength);
    }
    /* Then the line wrap indicator (if needed) */
    if (someLine->lineFlags & WRAPFLAG) {
	DrawLineWrap(display, textInfo->mainWindow,
		     textInfo->w - BARSIZE - WRAPINDSIZE,
		     ypos, someLine->lineHeight,
		     textInfo->fgPix);
    }
    return 1;
}




static int HandleNewFont(display, fontNum, textInfo, flagWord)
Display *display;
int fontNum;			/* Font number       */
struct txtWin *textInfo;	/* Text information  */
int flagWord;			/* DODISP or nothing */
/*
 * This routine handles a new font request.  These requests take
 * the form "^F<digit>".  The parsing is done in TxtWriteStr.
 * This routine is called only if the form is valid.  It may return
 * a failure (0 status) if the requested font is not loaded.
 * If the new font is larger than any of the current
 * fonts on the line,  it will change the line height and redisplay
 * the line.
 */
{
    struct txtLine *thisLine;
    int heightDiff, baseDiff, redrawFlag;

    if (textInfo->theFonts[fontNum].fid == 0) {
	return 0;
    } else {
	thisLine = textInfo->txtBuffer[textInfo->curLine];
	textInfo->curFont = fontNum;
	redrawFlag = 0;
	heightDiff = textInfo->theFonts[fontNum].ascent +
	    textInfo->theFonts[fontNum].descent -
		thisLine->lineHeight;

	if (heightDiff > 0) {
	    redrawFlag = 1;
	} else {
	    heightDiff = 0;
	}

	if (redrawFlag) {
	    if (flagWord & DODISP) {
		/* Clear current line */
		XFillRectangle(display, textInfo->mainWindow,
			       textInfo->bgGC,
			       0, textInfo->curY, textInfo->w,
			       thisLine->lineHeight);

		/* Check to see if it requires scrolling */
		if ((textInfo->curY + thisLine->lineHeight + heightDiff +
		     INTERLINE) > textInfo->h)
		  {
		      /* 
		       * General approach:  "unscroll" the last line up
		       * and then call ScrollDown to do the right thing.
		       */
		      textInfo->endLine -= 1;
		      textInfo->bottomSpace += thisLine->lineHeight +
			  INTERLINE;

		      XFillRectangle(display, textInfo->mainWindow,
				     textInfo->bgGC,
				     0, textInfo->h - textInfo->bottomSpace,
				     textInfo->w, textInfo->bottomSpace);

		      thisLine->lineHeight += heightDiff;
		      ScrollDown(display, textInfo);
		      textInfo->curY = textInfo->h -
			(textInfo->bottomSpace + INTERLINE +
			 thisLine->lineHeight);
		  }
		else 
		  {
		      /* Just update bottom space */
		      textInfo->bottomSpace -= heightDiff;
		      thisLine->lineHeight += heightDiff;
		  }
		/* Redraw the current line */
		DrawLine(display, textInfo, textInfo->curLine, textInfo->curY);
	    } else {
		/* Just update line height */
		thisLine->lineHeight += heightDiff;
	    }
	}
	return 1;
    }
}



int TxtWriteStr(display, w, str)
Display *display;
Window w;			/* Text window            */
register char *str;		/* 0 terminated string */
/*
 * This routine writes a string to the specified text window.
 * The following notes apply:
 *   - Text is always appended to the end of the text buffer.
 *   - If the scroll bar is positioned such that the end of the
 *     text is not visible,  an automatic scroll to the bottom
 *     will be done before the appending of text.
 *   - Non-printable ASCII characters are not displayed.
 *   - The '\n' character causes the current text position to
 *     advance one line and start at the left.
 *   - Tabs are not supported.
 *   - Lines too long for the screen will be wrapped and a line wrap
 *     indication will be drawn.
 *   - Backspace clears the previous character.  It will do the right
 *     thing if asked to backspace past a wrapped line.
 *   - A new font can be chosen using the sequence '^F<digit>' where
 *     <digit> is 0-7.  The directive will be ignored if
 *     there is no font in the specified slot.
 * Returns 0 if something went wrong.  
 */
{
    register int fontIndex;
    register struct txtWin *textInfo;
    register struct txtLine *thisLine;

    if ((textInfo = (struct txtWin *) XLookUpAssoc(display, textWindows, (XID) w)) == 0)
      return 0;
    
    /* See if screen needs to be updated */
    if (textInfo->flagWord & SCREENWRONG) {
	TxtRepaint(display, textInfo->mainWindow);
    }

    /* See if we have to scroll down to the bottom */
    if (textInfo->flagWord & NOTATBOTTOM) {
	WarpToBottom(display, textInfo);
	textInfo->flagWord &= (~NOTATBOTTOM);
    }

    /* Undraw the current cursor */
    thisLine = textInfo->txtBuffer[textInfo->curLine];

    XFillRectangle(display, w, textInfo->bgGC,
	    thisLine->lineWidth + CUROFFSET,
	    textInfo->curY,
	    CURSORWIDTH,
	    thisLine->lineHeight);

    for ( /* str is ok */ ; (*str != 0) ; str++) {
	/* Check to see if we are waiting on a font */
	if (textInfo->flagWord & FONTNUMWAIT) {
	    textInfo->flagWord &= (~FONTNUMWAIT);
	    fontIndex = *str - '0';
	    if ((fontIndex >= 0) && (fontIndex < MAXFONTS)) {
		/* Handle font -- go get next character */
		if (HandleNewFont(display, fontIndex, textInfo, DODISP))
		    continue;
	    }
	}
	
	/* Inline code for handling normal character case */
	if ((*str >= LOWCHAR) && (*str <= HIGHCHAR)) {
	    register XFontStruct *thisFont;
	    register struct txtLine *thisLine;
	    register int charWidth;
	    int thisColor;

	    /* Determine size of character */
	    thisFont = &(textInfo->theFonts[textInfo->curFont]);
	    thisColor = textInfo->theColors[textInfo->curFont];
	    if (*str <= thisFont->min_char_or_byte2 ||
		*str >= thisFont->max_char_or_byte2 ||
		thisFont->per_char == 0)
		charWidth = thisFont->max_bounds.width + 1;
	    else
		charWidth = thisFont->per_char[*str].width + 1;

	    /* Check to see if line wrap is required */
	    thisLine = textInfo->txtBuffer[textInfo->curLine];
	    if (thisLine->lineWidth + charWidth >
		(textInfo->w-BARSIZE-WRAPINDSIZE))
	      {
		  DrawLineWrap(display, textInfo->mainWindow,
			       textInfo->w-BARSIZE-WRAPINDSIZE,
			       textInfo->curY, thisLine->lineHeight,
			       textInfo->fgPix);
		  thisLine->lineFlags |= WRAPFLAG;
		  /* Handle the spacing problem the same way as a newline */
		  HandleNewLine(display, textInfo, DODISP | NONEWLINE);
		  thisLine = textInfo->txtBuffer[textInfo->curLine];
	      }
	    
	    /* Ready to draw character */
	    XDrawString(display, textInfo->mainWindow,
			DEFAULT_GC, 
			textInfo->curX += charWidth,
			textInfo->curY + thisLine->lineHeight, 
			str, 1);
	    
	    /* Append character onto main buffer */
	    if (textInfo->bufSpot >= textInfo->bufAlloc)
	      /* Make room for more characters */
	      ExpandBuffer(textInfo);
	    textInfo->mainBuffer[(textInfo->bufSpot)++] =
	      (textInfo->curFont << FONTSHIFT) | (*str);
	    
	    /* Update the line start array */
	    thisLine->lineLength += 1;
	    thisLine->lineWidth += charWidth;
	} else if (*str == NEWLINE) {
	    HandleNewLine(display, textInfo, DODISP);
	} else if (*str == NEWFONT) {
	    /* Go into waiting for font number mode */
	    textInfo->flagWord |= FONTNUMWAIT;
	} else if (*str == BACKSPACE) {
	    HandleBackspace(display, textInfo, DODISP);
	} else {
	    /* Ignore all others */
	}
    }
    /* Draw the cursor in its new position */
    thisLine = textInfo->txtBuffer[textInfo->curLine];

    XFillRectangle(display, w, textInfo->CursorGC,
	    thisLine->lineWidth + CUROFFSET,
	    textInfo->curY /* + thisLine->lineHeight */,
	    CURSORWIDTH, thisLine->lineHeight);

    return 1;
}



int TxtJamStr(display, w, str)
Display *display;
Window w;			/* Text window            */
register char *str;		/* NULL terminated string */
/*
 * This is the same as TxtWriteStr except the screen is NOT updated.
 * After a call to this routine,  TxtRepaint should be called to
 * update the screen.  This routine is meant to be used to load
 * a text buffer with information and then allow the user to
 * scroll through it at will.
 */
{
    register int fontIndex;
    register struct txtWin *textInfo;

    if ((textInfo = (struct txtWin *) XLookUpAssoc(display, textWindows, (XID) w)
	 ) == 0)
      return 0;
    
    for ( /* str is ok */ ; (*str != 0) ; str++) {
	/* Check to see if we are waiting on a font */
	if (textInfo->flagWord & FONTNUMWAIT) {
	    textInfo->flagWord &= (~FONTNUMWAIT);
	    fontIndex = *str - '0';
	    if ((fontIndex >= 0) && (fontIndex < MAXFONTS)) {
		if (HandleNewFont(display, fontIndex, textInfo, 0)) {
		    /* Handled font -- go get next character */
		    continue;
		}
	    }
	}
	/* Inline code for handling normal character case */
	if ((*str >= LOWCHAR) && (*str <= HIGHCHAR)) {
	    register XFontStruct *thisFont;
	    register struct txtLine *thisLine;
	    register int charWidth;
	    
	    /* Determine size of character */
	    thisFont = &(textInfo->theFonts[textInfo->curFont]);

	    if (*str <= thisFont->min_char_or_byte2 ||
		*str >= thisFont->max_char_or_byte2 ||
		thisFont->per_char == 0)
		charWidth = thisFont->max_bounds.width + 1;
	    else
		charWidth = thisFont->per_char[*str].width + 1;

	    /* Check to see if line wrap is required */
	    thisLine = textInfo->txtBuffer[textInfo->curLine];
	    if (thisLine->lineWidth + charWidth >
		(textInfo->w-BARSIZE-WRAPINDSIZE))
	      {
		  thisLine->lineFlags |= WRAPFLAG;
		  /* Handle the spacing problem the same way as a newline */
		  HandleNewLine(display, textInfo, NONEWLINE);
		  thisLine = textInfo->txtBuffer[textInfo->curLine];
	      }
	    /* Append character onto main buffer */
	    if (textInfo->bufSpot >= textInfo->bufAlloc)
	      /* Make room for more characters */
	      ExpandBuffer(textInfo);
	    textInfo->mainBuffer[(textInfo->bufSpot)++] =
	      (textInfo->curFont << FONTSHIFT) | (*str);
	    
	    /* Update the line start array */
	    thisLine->lineLength += 1;
	    thisLine->lineWidth += charWidth;
	} else if (*str == NEWLINE) {
	    HandleNewLine(display, textInfo, 0);
	} else if (*str == NEWFONT) {
	    /* Go into waiting for font number mode */
	    textInfo->flagWord |= FONTNUMWAIT;
	} else if (*str == BACKSPACE) {
	    HandleBackspace(display, textInfo, 0);
	} else {
	    /* Ignore all others */
	}
    }
    textInfo->flagWord |= SCREENWRONG;
    return 1;
}



int TxtRepaint(display,w)
Display *display;
Window w;
/*
 * Repaints the given scrollable text window.  The routine repaints
 * the entire window.  For handling exposure events,  the TxtFilter 
 * routine should be used.
 */
{
    struct txtWin *textInfo;
    int index, ypos;

    if ((textInfo = (struct txtWin *) XLookUpAssoc(display, textWindows, (XID) w)
	 ) == 0)
      return 0;

    /* Check to see if the screen is up to date */
    if (textInfo->flagWord & SCREENWRONG) {
	textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
	textInfo->flagWord &= (~SCREENWRONG);
    }

    ypos = YPADDING;
    index = textInfo->startLine;
    for (;;) {
	DrawLine(display, textInfo, index, ypos);
	if (index >= textInfo->endLine) break;
	ypos += (textInfo->txtBuffer[index]->lineHeight + INTERLINE);
	index++;
    }
    /* Draw the cursor (if on screen) */
    if (textInfo->endLine == textInfo->curLine) {
	XFillRectangle(display, w, textInfo->CursorGC,
		       textInfo->txtBuffer[index]->lineWidth + CUROFFSET,
		       ypos /* + textInfo->txtBuffer[index]->lineHeight */,
		       CURSORWIDTH, textInfo->txtBuffer[index]->lineHeight);

    }
    /* Update the scroll bar */
    UpdateScroll(display, textInfo);
    return 1;
}



static int InsertIndex(textInfo, thisIndex, ypos)
struct txtWin *textInfo;	/* Text Window Information    */
int thisIndex;			/* Line index of exposed line */
int ypos;			/* Drawing position of line   */
/*
 * This routine inserts the supplied line index into the copy
 * exposure array for 'textInfo'.  The array is kept sorted
 * from lowest to highest using insertion sort.  The array
 * is dynamically expanded if needed.
 */
{
    struct expEvent *newItem;
    int newSize, index, downIndex;

    /* Check to see if we need to expand it */
    if ((textInfo->exposeSize + 3) >= textInfo->exposeAlloc) {
	newSize = textInfo->exposeAlloc +
	  (textInfo->exposeAlloc * EXPANDPERCENT / 100);
	textInfo->exposeAry = (struct expEvent **)
	  realloc((char *) textInfo->exposeAry,
		  (unsigned) (newSize * sizeof(struct expEvent *)));
	for (index = textInfo->exposeAlloc;  index < newSize;  index++)
	  textInfo->exposeAry[index] = alloc(struct expEvent);
	textInfo->exposeAlloc = newSize;
    }
    /* Find spot for insertion.  NOTE: last spot has big number */
    for (index = 0;  index <= textInfo->exposeSize;  index++) {
	if (textInfo->exposeAry[index]->lineIndex >= thisIndex) {
	    if (textInfo->exposeAry[index]->lineIndex > thisIndex) {
		/* Insert before this entry */
		newItem = textInfo->exposeAry[textInfo->exposeSize+1];
		for (downIndex = textInfo->exposeSize;
		     downIndex >= index;
		     downIndex--)
		  {
		      textInfo->exposeAry[downIndex+1] =
			textInfo->exposeAry[downIndex];
		  }
		/* Put a free structure at this spot */
		textInfo->exposeAry[index] = newItem;
		/* Fill it in */
		textInfo->exposeAry[index]->lineIndex = thisIndex;
		textInfo->exposeAry[index]->ypos = ypos;
		/* Break out of loop */
		textInfo->exposeSize += 1;
	    }
	    break;
	}
    }
    return 1;
}



static int ScrollUp(display, textInfo)
Display *display;
struct txtWin *textInfo;	/* Text window information   */
/*
 * This routine scrolls the indicated text window up by one
 * line.  The line above the current line must exist.  The
 * window is scrolled so that the line above the start line
 * is displayed at the top of the screen.  This may cause
 * many lines to scroll off the bottom.  The scrolling is
 * done using XCopyArea.  The exposure events should be caught
 * by ExposeCopy.
 */
{
    int targetSpace;

    /* Make sure all exposures have been handled by now */
    if (textInfo->startLine == 0) return 0;
    targetSpace = textInfo->txtBuffer[textInfo->startLine-1]->lineHeight +
      INTERLINE;
    /* Move the area downward by the target amount */
    XCopyArea(display, textInfo->mainWindow, textInfo->mainWindow,
	      DEFAULT_GC,
	      0, YPADDING, textInfo->w - BARSIZE,
	      textInfo->h, 0, targetSpace);

    textInfo->flagWord |= COPYEXPOSE;
    /* Update the text window parameters */
    textInfo->startLine -= 1;
    textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));

    /* Clear out bottom space region */
    XClearArea(display, textInfo->mainWindow,
	       0, textInfo->h - textInfo->bottomSpace,
	       textInfo->w, textInfo->bottomSpace);
    
    UpdateExposures(display, textInfo);
    UpdateScroll(display, textInfo);

    return 1;
}


static int ScrollToSpot(display, textInfo, ySpot)
Display *display;
struct txtWin *textInfo;	/* Text window information          */
int ySpot;			/* Button position in scroll window */
/*
 * This routine scrolls the specified text window relative to the
 * position of the mouse in the scroll bar.  The center of the screen
 * will be positioned to correspond to the mouse position.
 */
{
    int targetLine, aboveLines;

    targetLine = textInfo->numLines * ySpot / textInfo->h;
    textInfo->startLine = targetLine;
    textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
    aboveLines = 0;
    /* Make the target line the *center* of the window */
    while ((textInfo->startLine > 0) &&
	   (aboveLines < textInfo->endLine - targetLine))
      {
	  textInfo->startLine -= 1;
	  textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
	  aboveLines++;
      }
    if (textInfo->endLine == textInfo->numLines-1) {
	WarpToBottom(display, textInfo);
    } else {
	XClearWindow(display, textInfo->mainWindow);
	TxtRepaint(display, textInfo->mainWindow);
    }
    return 1;
}



static int LineToTop(display, textInfo, pos)
Display *display;
struct txtWin *textInfo;	/* Text window information */
int pos;			/* Y position of mouse     */
/*
 * This routine scrolls the screen down until the line at the
 * mouse position is at the top of the screen.  It stops
 * if it can't scroll the buffer down that far.  If the
 * global 'ScrollOption' is NORMSCROLL,  a smooth scroll
 * is used.  Otherwise,  it jumps to the right position
 * and repaints the screen.
 */
{
    int index, sum;

    /* First,  we find the current line */
    sum = 0;
    for (index = textInfo->startLine;  index <= textInfo->endLine;  index++) {
	if (sum + textInfo->txtBuffer[index]->lineHeight + INTERLINE> pos) break;
	sum += textInfo->txtBuffer[index]->lineHeight + INTERLINE;
    }
    /* We always want to scroll down at least one line */
    if (index == textInfo->startLine) index++;
    if (ScrollOption == NORMSCROLL) {
	/* Scroll down until 'index' is the starting line */
	while ((textInfo->startLine < index) && ScrollDown(display, textInfo))
	{
	    /* Empty Loop Body */
	}
    } else {
	/* Immediately jump to correct spot */
	textInfo->startLine = index;
	textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
	if (textInfo->endLine == textInfo->numLines-1) {
	    WarpToBottom(display, textInfo);
	} else {
	    XClearWindow(display, textInfo->mainWindow);
	    TxtRepaint(display, textInfo->mainWindow);
	}
    }
    /* Check to see if at end of buffer */
    if (textInfo->endLine >= textInfo->numLines-1) {
	textInfo->flagWord &= (~NOTATBOTTOM);
    }
    return 1;
}



static int TopToHere(display, textInfo, pos)
Display *display;
struct txtWin *textInfo;	/* Text window information */
int pos;			/* Y position of mouse     */
/*
 * This routine scrolls the screen up until the top line of
 * the screen is at the current Y position of the mouse.  Again,
 * it will stop if it can't scroll that far.  If the global
 * 'ScrollOption' is NORMSCROLL,  a smooth scroll is used.
 * If it's not,  it will simply redraw the screen at the
 * correct spot.
 */
{
    int sum, target, linesup, index;

    target = pos - textInfo->txtBuffer[textInfo->startLine]->lineHeight;
    /* We always want to scroll up at least one line */
    if (target <= 0) target = 1;
    sum = 0;
    linesup = 0;
    /* Check to see if we are at the top anyway */
    if (textInfo->startLine == 0) return 0;
    if (ScrollOption == NORMSCROLL) {
	/* Scroll up until sum of new top lines greater than target */
	while ((sum < target) && ScrollUp(display, textInfo)) {
	    sum += textInfo->txtBuffer[textInfo->startLine]->lineHeight;
	    linesup++;
	}
    } else {
	/* Search backward to find index */
	index = textInfo->startLine - 1;
	while ((index > 0) && (sum < target)) {
	    sum += textInfo->txtBuffer[index]->lineHeight;
	    linesup++;
	    index--;
	}
	/* Go directly to the index */
	textInfo->startLine = index;
	textInfo->endLine = FindEndLine(textInfo, &(textInfo->bottomSpace));
	XClearWindow(display, textInfo->mainWindow);
	TxtRepaint(display, textInfo->mainWindow);
    }
    /* If we scrolled,  assert we are not at bottom of buffer */
    if (linesup > 0) {
	textInfo->flagWord |= NOTATBOTTOM;
    }
    return 1;
}



int TxtFilter(display, evt)
Display *display;
XEvent *evt;
/*
 * This routine handles events associated with scrollable text windows.
 * It will handle all exposure events and any button released events
 * in the scroll bar of a text window.  It does NOT handle any other
 * events.  If it cannot handle the event,  it will return 0.
 */
{
    XExposeEvent *expose = &evt->xexpose;
    XButtonEvent *btEvt = &evt->xbutton;
    XGraphicsExposeEvent *gexpose = &evt->xgraphicsexpose;
    XNoExposeEvent *noexpose = &evt->xnoexpose;
    struct txtWin *textInfo;
    int index, ypos;
    Window w, sw;

    if (textWindows == (XAssocTable *) 0) {
	textWindows = XCreateAssocTable(32);
	if (textWindows == (XAssocTable *) 0) return(0);
    }
    if (evt->type == Expose) {
	w = expose->window;
	sw = 0;
    }
    else if (evt->type == GraphicsExpose) {
	w = gexpose->drawable;
	sw = 0;
    }
    else if (evt->type == NoExpose) {
	w = noexpose->drawable;
	sw = 0;
    }
    else if (evt->type == ButtonRelease) {
	w = btEvt->window;
	sw = btEvt->subwindow;
    }
    else
	return 0;

    if ((textInfo = (struct txtWin *)
	 XLookUpAssoc(display, textWindows, (XID) w)) == 0)	
	return 0;

    /* Determine whether it's main window or not */
    if ((w == textInfo->mainWindow) && (sw == 0)) {
	/* Main Window - handle exposures */
	switch (evt->type) {
	case Expose:
	    ypos = 0 /*YPADDING*/;
	    for (index = textInfo->startLine;
		 index <= textInfo->endLine;
		 index++)
	      {
		  int lh = textInfo->txtBuffer[index]->lineHeight;

		  if (((ypos + lh) >= expose->y) &&
		      (ypos <= (expose->y + expose->height)))
		    {
			/* Intersection region */
			/* Draw line immediately */
			DrawLine(display, textInfo, index, ypos);
			/* And possibly draw cursor */
			if (textInfo->curLine == index) {
			    XFillRectangle(display, w, textInfo->CursorGC,
				       textInfo->txtBuffer[index]->lineWidth +
					   CUROFFSET,
					   ypos,
					   CURSORWIDTH,
					   lh);
			}
		    }
		  ypos += lh + INTERLINE;
	      }
	    break;
	case GraphicsExpose:
	    ypos = 0 /*YPADDING*/;
	    for (index = textInfo->startLine;
		 index <= textInfo->endLine;
		 index++)
	      {
		  int lh = textInfo->txtBuffer[index]->lineHeight;

		  if (((ypos + lh) >= gexpose->y) &&
		      (ypos <= (gexpose->y + gexpose->height)))
		    {
			/* Intersection region */
			/* Draw line immediately */
			DrawLine(display, textInfo, index, ypos);
			/* And possibly draw cursor */
			if (textInfo->curLine == index) {
			    XFillRectangle(display, w, textInfo->CursorGC,
				    textInfo->txtBuffer[index]->lineWidth +
				    CUROFFSET,
				    ypos,
				    CURSORWIDTH,
				    lh);
			}
		    }
		  ypos += lh + INTERLINE;
	      }
	    break;
	case NoExpose:
	    break;
	default:
	    /* Not one of our events */
	    return 0;
	}
    } else {
	switch (evt->type) {
	case Expose:
	    UpdateScroll(display, textInfo);
	    break;
	case ButtonRelease:
	    /* Find out which button */
	    switch (btEvt->button) {
	    case Button1:
		/* Scroll up until top line is at mouse position */
		TopToHere(display, textInfo, btEvt->y);
		break;
	    case Button2:
		/* Scroll to spot relative to position */
		ScrollToSpot(display, textInfo, btEvt->y);
		if (textInfo->endLine >= textInfo->numLines-1) {
		    textInfo->flagWord &= (~NOTATBOTTOM);
		} else {
		    textInfo->flagWord |= NOTATBOTTOM;
		}
		break;
	    case Button3:
		/* Scroll down until pointed line is at top */
		LineToTop(display, textInfo, btEvt->y);
		break;
	    }
	    break;
	default:
	    /* Not one of our events */
	    return 0;
	}
    }
    return 1;
}
