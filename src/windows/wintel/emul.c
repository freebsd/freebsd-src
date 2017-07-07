/* emul.c */

#include "windows.h"
#include "screen.h"


static int
ScreenEmChars(SCREEN *pScr, char *c, int len)
{
  /*
   * Function: Send a string of characters to the screen.  Placement
   *   continues as long as the stream of characters does not contain any
   *   control chracters or cause wrapping to another line.  When a control
   *   character is encountered or wrapping occurs, display stops and a
   *   count of the number of characters is returned.
   *
   * Parameters:
   *   pScr - the screen to place the characters on.
   *   c - the string of characters to place on the screen.
   *   len - the number of characters contained in the string
   *
   * Returns: The number of characters actually placed on the screen.
   */

  int insert;
  int ocount;
  int attrib;
  int extra;
  int nchars;
  char *acurrent;	/* place to put attributes */
  char *current; 	/* place to put characters */
  char *start;
  SCREENLINE *pScrLine;

  if (len <= 0)
    return(0);

  if (pScr->x != pScr->width - 1)
    pScr->bWrapPending = FALSE;
  else {
    if (pScr->bWrapPending) {
      pScr->x = 0;
      pScr->bWrapPending = FALSE;
      ScreenIndex(pScr);
    }
  }

  pScrLine = GetScreenLineFromY(pScr, pScr->y);
  if (pScrLine == NULL)
    return(0);

  current = &pScrLine->text[pScr->x];
  acurrent = &pScrLine->attrib[pScr->x];
  start = current;
  ocount = pScr->x;
  extra = 0;

  attrib = pScr->attrib;
  insert = pScr->IRM;

  for (nchars = 0; nchars < len && *c >= 32; nchars++) {
    if (insert)
      ScreenInsChar(pScr, 1);

    *current = *c;
    *acurrent = (char) attrib;
    c++;
    if (pScr->x < pScr->width - 1) {
      acurrent++;
      current++;
      pScr->x++;
    }
    else {
      extra = 1;
      if (pScr->DECAWM) {
	pScr->bWrapPending = TRUE;
	nchars++;
	break;
      }
    }
  }

  ScreenDraw(pScr, ocount, pScr->y, pScr->attrib,
	     pScr->x - ocount + extra, start);

  return(nchars);
}


void
ScreenEm(LPSTR c, int len, SCREEN *pScr)
{
  int escflg;		/* vt100 escape level */
  RECT rc;
  unsigned int ic;
  char stat[20];
  int i;
  int nchars;

  if (pScr->screen_bottom != pScr->buffer_bottom) {
    ScreenUnscroll(pScr);
    InvalidateRect(pScr->hWnd, NULL, TRUE);
    SetScrollPos(pScr->hWnd, SB_VERT, pScr->numlines, TRUE);
  }

  ScreenCursorOff(pScr);
  escflg = pScr->escflg;

#ifdef UM
  if (pScr->localprint && len > 0) {	/* see if printer needs anything */
    pcount = send_localprint(c, len);
    len -= pcount;
    c += pcount;
  }
#endif

  while (len > 0) {
    /*
     * look at first character in the vt100 string, if it is a
     * non-printable ascii code
     */
    while((*c < 32) && (escflg == 0) && (len > 0)) {
      switch(*c) {

      case 0x1b:		/* ESC found (begin vt100 control sequence) */
	escflg++;
	break;

      case -1:			/* IAC from telnet session */
	escflg = 6;
	break;

#ifdef CISB
      case 0x05:		/* CTRL-E found (answerback) */
	bp_ENQ();
	break;
#endif

      case 0x07:		/* CTRL-G found (bell) */
	ScreenBell(pScr);
	break;

      case 0x08:		/* CTRL-H found (backspace) */
	ScreenBackspace(pScr);
	break;

      case 0x09:		/* CTRL-I found (tab) */
	ScreenTab(pScr);	/* Later change for versatile tabbing */
	break;

      case 0x0a:		/* CTRL-J found (line feed) */
      case 0x0b:		/* CTRL-K found (treat as line feed) */
      case 0x0c:		/* CTRL-L found (treat as line feed) */
	ScreenIndex(pScr);
	break;

      case 0x0d:		/* CTRL-M found (carriage feed) */
	ScreenCarriageFeed(pScr);
	break;

#if 0
      case 0x0e:      	/* CTRL-N found (invoke Graphics (G1) character set) */
	if (pScr->G1)
	  pScr->attrib = VSgraph(pScr->attrib);
	else
	  pScr->attrib = VSnotgraph(pScr->attrib);
	pScr->charset = 1;
	break;

      case 0x0f:	/* CTRL-O found (invoke 'normal' (G0) character set) */
	if(pScr->G0)
	  pScr->attrib = VSgraph(pScr->attrib);
	else
	  pScr->attrib = VSnotgraph(pScr->attrib);
	pScr->charset = 0;
	break;
#endif

#ifdef CISB
      case 0x10:      		/* CTRL-P found (undocumented in vt100) */
	bp_DLE(c, len);
	len = 0;
	break;
#endif

#if 0
      case 0x11:      		/* CTRL-Q found (XON) (unused presently) */
      case 0x13:      		/* CTRL-S found (XOFF) (unused presently) */
      case 0x18:      		/* CTRL-X found (CAN) (unused presently) */
      case 0x1a:      		/* CTRL-Z found (SUB) (unused presently) */
	break;
#endif
      }

      c++;		/* advance to the next character in the string */
      len--;		/* decrement the counter */
    }

    if (escflg == 0) {	/* check for normal character to print */
      nchars = ScreenEmChars(pScr, c, len);
      c += nchars;
      len -= nchars;
    }

    while ((len > 0) && (escflg == 1)) {	/* ESC character was found */
      switch(*c) {

      case 0x08:      			/* CTRL-H found (backspace) */
	ScreenBackspace(pScr);
	break;

	/*
	 * mostly cursor movement options, and DEC private stuff following
	 */
      case '[':
	ScreenApClear(pScr);
	escflg = 2;
	break;

      case '#':               	/* various screen adjustments */
	escflg = 3;
	break;

      case '(':               	/* G0 character set options */
	escflg = 4;
	break;

      case ')':               	/* G1 character set options */
	escflg = 5;
	break;

      case '>':               	/* keypad numeric mode (DECKPAM) */
	pScr->DECPAM = 0;
	escflg = 0;
	break;

      case '=':               	/* keypad application mode (DECKPAM) */
	pScr->DECPAM = 1;
	escflg = 0;
	break;

      case '7':               	/* save cursor (DECSC) */
	ScreenSaveCursor(pScr);
	escflg = 0;
	break;

      case '8':               	/* restore cursor (DECRC) */
	ScreenRestoreCursor(pScr);
	escflg = 0;
	break;

#if 0
      case 'c':				/* reset to initial state (RIS) */
	ScreenReset(pScr);
	escflg = 0;
	break;
#endif

      case 'D':				/* index (move down one line) (IND) */
	ScreenIndex(pScr);
	escflg = 0;
	break;

      case 'E':	/* next line (move down one line and to first column) (NEL) */
	pScr->x = 0;
	ScreenIndex(pScr);
	escflg = 0;
	break;

      case 'H':				/* horizontal tab set (HTS) */
	pScr->tabs[pScr->x] = 'x';
	escflg = 0;
	break;

#ifdef CISB
      case 'I':				/* undoumented in vt100 */
	bp_ESC_I();
	break;
#endif

      case 'M':					/* reverse index (move up one line) (RI) */
	ScreenRevIndex(pScr);
	escflg = 0;
	break;

      case 'Z':					/* identify terminal (DECID) */
	escflg = 0;
	break;

      default:
	/* put the ESC character into the Screen */
	ScreenEmChars(pScr, "\033", 1);
	/* put the next character into the Screen */
	ScreenEmChars(pScr, c, 1);
	escflg = 0;
	break;

      } /* end switch */

      c++;
      len--;
    }

    while((escflg == 2) && (len > 0)) {     /* '[' handling */
      switch(*c) {

      case 0x08:			/* backspace */
	ScreenBackspace(pScr);
	break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':					/* numeric parameters */
	if (pScr->parms[pScr->parmptr] < 0)
	  pScr->parms[pScr->parmptr] = 0;
	pScr->parms[pScr->parmptr] *= 10;
	pScr->parms[pScr->parmptr] += *c - '0';
	break;

      case '?':					/* vt100 mode change */
	pScr->parms[pScr->parmptr++] = -2;
	break;

      case ';':					/* parameter divider */
	pScr->parmptr++;
	break;

      case 'A':					/* cursor up (CUU) */
	pScr->bWrapPending = FALSE;
	rc.left = pScr->x * pScr->cxChar;
	rc.right = (pScr->x + 1) * pScr->cxChar;
	rc.top = pScr->cyChar * pScr->y;
	rc.bottom = pScr->cyChar * (pScr->y + 1);
	InvalidateRect(pScr->hWnd, &rc, TRUE);
	if (pScr->parms[0] < 1)
	  pScr->y--;
	else
	  pScr->y -= pScr->parms[0];
	if(pScr->y < pScr->top)
	  pScr->y = pScr->top;
	ScreenRange(pScr);
	escflg = 0;
	SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
	break;

      case 'B':					/* cursor down (CUD) */
	pScr->bWrapPending = FALSE;
	rc.left = pScr->x * pScr->cxChar;
	rc.right = (pScr->x + 1) * pScr->cxChar;
	rc.top = pScr->cyChar * pScr->y;
	rc.bottom = pScr->cyChar * (pScr->y + 1);
	InvalidateRect(pScr->hWnd, &rc, TRUE);
	if (pScr->parms[0] < 1)
	  pScr->y++;
	else
	  pScr->y += pScr->parms[0];
	if (pScr->y > pScr->bottom)
	  pScr->y = pScr->bottom;
	ScreenRange(pScr);
	escflg = 0;
	SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
	break;

      case 'C':		/* cursor forward (right) (CUF) */
	pScr->bWrapPending = FALSE;
	rc.left = pScr->x * pScr->cxChar;
	rc.right = (pScr->x + 1) * pScr->cxChar;
	rc.top = pScr->cyChar * pScr->y;
	rc.bottom = pScr->cyChar * (pScr->y +1);
	InvalidateRect(pScr->hWnd, &rc, TRUE);
	if(pScr->parms[0] < 1)
	  pScr->x++;
	else
	  pScr->x += pScr->parms[0];
	ScreenRange(pScr);
	if (pScr->x > pScr->width)
	  pScr->x = pScr->width;
	escflg = 0;
	SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
	break;

      case 'D':		/* cursor backward (left) (CUB) */
	pScr->bWrapPending = FALSE;
	rc.left = pScr->x * pScr->cxChar;
	rc.right = (pScr->x + 1) * pScr->cxChar;
	rc.top = pScr->cyChar * pScr->y;
	rc.bottom = pScr->cyChar * (pScr->y + 1);
	InvalidateRect(pScr->hWnd, &rc, TRUE);
	if(pScr->parms[0] < 1)
	  pScr->x--;
	else
	  pScr->x -= pScr->parms[0];
	ScreenRange(pScr);
	escflg = 0;
	SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
	break;

      case 'f':			/* horizontal & vertical position (HVP) */
      case 'H':			/* cursor position (CUP) */
	pScr->bWrapPending = FALSE;
	rc.left = pScr->x * pScr->cxChar;
	rc.right = (pScr->x + 1) * pScr->cxChar;
	rc.top = pScr->cyChar * pScr->y;
	rc.bottom = pScr->cyChar * (pScr->y + 1);
	InvalidateRect(pScr->hWnd, &rc, TRUE);
	pScr->x = pScr->parms[1] - 1;
	pScr->y = pScr->parms[0] - 1;
	ScreenRange(pScr);	/* make certain the cursor position is valid */
	escflg = 0;
	SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
	break;

      case 'J':					/* erase in display (ED) */
	switch(pScr->parms[0]) {

	case -1:
	case 0:		/* erase from active position to end of screen */
	  ScreenEraseToEndOfScreen(pScr);
	  break;
	case 1:		/* erase from start of screen to active position */
#if 0
	  ScreenEraseToPosition(pScr);
#endif
	  break;

	case 2:					/* erase whole screen */
	  ScreenEraseScreen(pScr);
	  break;

	default:
	  break;
	}

	escflg = 0;
	break;

      case 'K':					/* erase in line (EL) */
	switch(pScr->parms[0]) {
	case -1:
	case 0:					/* erase to end of line */
	  ScreenEraseToEOL(pScr);
	  break;

	case 1:				/* erase to beginning of line */
	  ScreenEraseToBOL(pScr);
	  break;

	case 2:					/* erase whole line */
	  ScreenEraseLine(pScr, -1);
	  break;

	default:
	  break;
	}

	escflg = 0;
	break;

      case 'L':		/* insert n lines preceding current line (IL) */
	if (pScr->parms[0] < 1)
	  pScr->parms[0] = 1;
	ScreenInsLines(pScr, pScr->parms[0], -1);
	escflg = 0;
	break;

      case 'M':	/* delete n lines from current position downward (DL) */
	if (pScr->parms[0] < 1)
	  pScr->parms[0] = 1;
	ScreenDelLines(pScr, pScr->parms[0], -1);
	escflg = 0;
	break;

      case 'P':		/* delete n chars from cursor to the left (DCH) */
	if (pScr->parms[0] < 1)
	  pScr->parms[0] = 1;
	ScreenDelChars(pScr, pScr->parms[0]);
	escflg = 0;
	break;

#if 0
      case 'R':		/* receive cursor position status from host */
	break;
#endif

#if 0
      case 'c':				/* device attributes (DA) */
	ScreenSendIdent();
	escflg = 0;
	break;
#endif

      case 'g':               	/* tabulation clear (TBC) */
	if (pScr->parms[0] == 3)/* clear all tabs */
	  ScreenTabClear(pScr);
	else
	  if (pScr->parms[0] <= 0)	/* clear tab stop at active position */
	    pScr->tabs[pScr->x] = ' ';
	escflg = 0;
	break;

      case 'h':               	/* set mode (SM) */
	ScreenSetOption(pScr,1);
	escflg = 0;
	break;

      case 'i':               	/* toggle printer */
#if 0
	if(pScr->parms[pScr->parmptr] == 5)
	  pScr->localprint = 1;
	else if (pScr->parms[pScr->parmptr] == 4)
	  pScr->localprint = 0;
#endif
	escflg = 0;
	break;

      case 'l':					/* reset mode (RM) */
	ScreenSetOption(pScr,0);
	escflg = 0;
	break;

      case 'm':				/* select graphics rendition (SGR) */
	{
	  int temp = 0;

	  while (temp <= pScr->parmptr) {
	    if (pScr->parms[temp] < 1)
	      pScr->attrib &= 128;
	    else
	      pScr->attrib |= 1 << (pScr->parms[temp] - 1);
	    temp++;
	  }
	}
      escflg = 0;
      break;

      case 'n':               	/* device status report (DSR) */
	switch (pScr->parms[0]) {
#if 0
	case 0: 	/* response from vt100; ready, no malfunctions */
	case 3: 	/* response from vt100; malfunction, retry */
#endif
	case 5: 	/* send status */
	case 6: 				/* send active position */
	  wsprintf(stat, "\033[%d;%dR", pScr->y + 1, pScr->x + 1);
	  for (i = 0; stat[i]; i++)
	    SendMessage(pScr->hwndTel, WM_MYSCREENCHAR,
			stat[i], (LPARAM) pScr);
	  break;
	} /* end switch */
	escflg = 0;
	break;

      case 'q':			/* load LEDs (unsupported) (DECLL) */
	escflg = 0;
	break;

      case 'r':			/* set top & bottom margins (DECSTBM) */
	if (pScr->parms[0] < 0)
	  pScr->top = 0;
	else
	  pScr->top = pScr->parms[0] - 1;
	if (pScr->parms[1] < 0)
	  pScr->bottom = pScr->height - 1;
	else
	  pScr->bottom = pScr->parms[1] - 1;
	if (pScr->top < 0)
	  pScr->top = 0;
	if (pScr->top > pScr->height-1)
	  pScr->top = pScr->height-1;
	if (pScr->bottom < 1)
	  pScr->bottom = pScr->height;
	if (pScr->bottom >= pScr->height)
	  pScr->bottom = pScr->height - 1;
	if (pScr->top >= pScr->bottom) {/* check for valid scrolling region */
	  if (pScr->bottom >= 1)     	/*
					 * assume the bottom value has
					 * precedence, unless it is as the
					 * top of the screen
					 */
	    pScr->top = pScr->bottom - 1;
	  else                /* totally psychotic case, bottom of screen set to the very top line, move the bottom to below the top */
	    pScr->bottom = pScr->top + 1;
	}
	pScr->x = 0;
	pScr->y = 0;
#if 0
	if (pScr->DECORG)
	  pScr->y = pScr->top;	/* origin mode relative */
#endif
	escflg = 0;
	break;

#if 0
      case 'x':	/* request/report terminal parameters
		   (DECREQTPARM/DECREPTPARM) */
      case 'y':				/* invoke confidence test (DECTST) */
	break;
#endif

      default:
	escflg = 0;
	break;

      }

      c++;
      len--;

#if 0
      if (pScr->localprint && (len > 0)) {  /* see if printer needs anything */
	pcount = send_localprint(c, len);
	len -= pcount;
	c += pcount;
      }
#endif
    }

    while ((escflg == 3) && (len > 0)) { /* #  Handling */
      switch (*c) {
      case 0x08:      			/* backspace */
	ScreenBackspace(pScr);
	break;

#if 0
      case '3':			/* top half of double line (DECDHL) */
      case '4':			/* bottom half of double line (DECDHL) */
      case '5':			/* single width line (DECSWL) */
      case '6':			/* double width line (DECDWL) */
	break;
#endif

      case '8':               	/* screen alignment display (DECALN) */
	ScreenAlign(pScr);
	escflg = 0;
	break;

      default:
	escflg = 0;
	break;

      }

      c++;
      len--;
    }

    while ((escflg == 4) && (len > 0)) { /* ( Handling (GO character set) */
      switch (*c) {

      case 0x08:      			/* backspace */
	ScreenBackspace(pScr);
	break;

#if 0
      case 'A':               /* united kingdom character set (unsupported) */
      case 'B':               /* ASCII character set */
      case '1':               /* choose standard graphics (same as ASCII) */
	pScr->G0 = 0;
	if (!pScr->charset)
	  pScr->attrib = ScreenNotGraph(pScr->attrib);
	escflg = 0;
	break;

      case '0':               /* choose special graphics set */
      case '2':               /* alternate character set (special graphics) */
	pScr->G0 = 1;
	if(!pScr->charset)
	  pScr->attrib = ScreenGraph(pScr->attrib);
	escflg = 0;
	break;
#endif

      default:
	escflg = 0;
	break;
      }

      c++;
      len--;

    } /* end while */

    while((escflg == 5) && (len > 0)) { /* ) Handling (G1 handling) */
      switch (*c) {

      case 0x08:					/* backspace */
	ScreenBackspace(pScr);
	break;

#if 0
      case 'A':               /* united kingdom character set (unsupported) */
      case 'B':               /* ASCII character set */
      case '1':               /* choose standard graphics (same as ASCII) */
	pScr->G1 = 0;
	if (pScr->charset)
	  pScr->attrib = ScreenNotGraph(pScr->attrib);
	escflg = 0;
	break;

      case '0':               /* choose special graphics set */
      case '2':               /* alternate character set (special graphics) */
	pScr->G1 = 1;
	if(pScr->charset)
	  pScr->attrib = ScreenGraph(pScr->attrib);
	escflg = 0;
	break;
#endif

      default:
	escflg = 0;
	break;
      } /* end switch */

      c++;
      len--;
    } /* end while */

    while ((escflg >= 6) && (escflg <= 10) && (len > 0)) { /* Handling IAC */
      ic = (unsigned char) *c;
      switch (escflg) {

      case 6:				/* Handling IAC xx */
	if (ic == 255) 			/* if IAC */
	  escflg = 0;
	else if (ic == 250)		/* if SB */
	  escflg = 7;
	else
	  escflg = 9;
	break;

      case 7:				/* Handling IAC SB xx */
	if (ic == 255)			/* if IAC */
	  escflg = 8;
	break;

      case 8:				/* Handling IAC SB IAC xx */
	if (ic == 255)			/* if IAC IAC */
	  escflg = 7;
	else if (ic == 240)		/* if IAC SE */
	  escflg = 0;
	break;

      case 9:						/* IAC xx xx */
	escflg = 0;
	break;
      }
      c++;		/* advance to the next character in the string */
      len--;		/* decrement the counter */
    }

    if (escflg > 2 && escflg < 6 && len > 0) {
      escflg = 0;
      c++;
      len--;
    }
  }
  pScr->escflg = escflg;
  ScreenCursorOn(pScr);
}
