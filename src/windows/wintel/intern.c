/* intern.c */

#include <windows.h>
#include <string.h>
#include <assert.h>
#include "screen.h"

#define ScreenClearAttrib 0

SCREENLINE *
GetScreenLineFromY(SCREEN *pScr, int y)
{
  SCREENLINE *pScrLine;
  int idx;

  pScrLine = pScr->screen_top;
  for (idx = 0; idx < pScr->height; idx++) {
    if (idx == y)
      return(pScrLine);
    if (pScrLine == NULL)
      return(NULL);
    pScrLine = pScrLine->next;
  }

  return(NULL);
}


SCREENLINE *
ScreenClearLine(SCREEN *pScr, SCREENLINE *pScrLine)
{
  memset(pScrLine->attrib, ScreenClearAttrib, pScr->width);
  memset(pScrLine->text, ' ', pScr->width);
  return(pScrLine);
}


void
ScreenUnscroll(SCREEN *pScr)
{
  int idx;
  SCREENLINE *pScrLine;

  if (pScr->screen_bottom == pScr->buffer_bottom)
    return;

  pScr->screen_bottom = pScr->buffer_bottom;
  pScrLine = pScr->screen_bottom;
  for (idx = 1; idx < pScr->height; idx++) {
    if (pScrLine == NULL)
      return;
    pScrLine = pScrLine->prev;
  }
  pScr->screen_top = pScrLine;
}


void
ScreenCursorOn(SCREEN *pScr)
{
  int y;
  int nlines;

  if (pScr->screen_bottom != pScr->buffer_bottom)
    nlines = pScr->numlines - GetScrollPos(pScr->hWnd, SB_VERT);
  else
    nlines = 0;

  y = pScr->y + nlines;
  SetCaretPos(pScr->x * pScr->cxChar, (y+1) * pScr->cyChar);
  ShowCaret(pScr->hWnd);
}


void
ScreenCursorOff(SCREEN *pScr)
{
  HideCaret(pScr->hWnd);
}


void
ScreenELO(SCREEN *pScr, int s)
{
  SCREENLINE *pScrLine;
  RECT rc;

  if (s < 0)
    s = pScr->y;

  pScrLine = GetScreenLineFromY(pScr,s);
  memset(pScrLine->attrib, ScreenClearAttrib, pScr->width);
  memset(pScrLine->text, ' ', pScr->width);
  rc.left = 0;
  rc.right = pScr->width * pScr->cxChar;
  rc.top = pScr->cyChar * s;
  rc.bottom = pScr->cyChar * (s+1);
  InvalidateRect(pScr->hWnd, &rc, TRUE);
}

void
ScreenEraseScreen(SCREEN *pScr)
{
  int i;
  int x1 = 0;
  int y1 = 0;
  int x2 = pScr->width;
  int y2 = pScr->height;
  int n = -1;

  for(i = 0; i < pScr->height; i++)
    ScreenELO(pScr,i);

  InvalidateRect(pScr->hWnd, NULL, TRUE);
  UpdateWindow(pScr->hWnd);
}


void
ScreenTabClear(SCREEN *pScr)
{
  int x = 0;

  while(x <= pScr->width) {
    pScr->tabs[x] = ' ';
    x++;
  }
}


void
ScreenTabInit(SCREEN *pScr)
{
  int x = 0;

  ScreenTabClear(pScr);

  while(x <= pScr->width) {
    pScr->tabs[x] = 'x';
    x += 8;
  }
  pScr->tabs[pScr->width] = 'x';
}


void
ScreenReset(SCREEN *pScr)
{
  pScr->top = 0;
  pScr->bottom = pScr->height-1;
  pScr->parmptr = 0;
  pScr->escflg = 0;
  pScr->DECAWM = 1;
  pScr->bWrapPending = FALSE;
  pScr->DECCKM = 0;
  pScr->DECPAM = 0;
  /*  pScr->DECORG = 0;     */
  /*  pScr->Pattrib = -1;   */
  pScr->IRM = 0;
  pScr->attrib = 0;
  pScr->x = 0;
  pScr->y = 0;
  /*    pScr->charset = 0; */
  ScreenEraseScreen(pScr);
  ScreenTabInit(pScr);
#if 0
  /*
   * QAK - 7/27/90: added because resetting the virtual screen's
   * wrapping flag doesn't reset telnet window's wrapping
   */
  set_vtwrap(pScrn, pScr->DECAWM);
#endif
}


void
ScreenListMove(SCREENLINE *TD, SCREENLINE *BD, SCREENLINE *TI, SCREENLINE *BI)
{
  if (TD->prev != NULL)
    TD->prev->next = BD->next;    /* Maintain circularity */

  if (BD->next != NULL)
    BD->next->prev = TD->prev;

  TD->prev = TI;                    /* Place the node in its new home */
  BD->next = BI;

  if (TI != NULL)
    TI->next = TD;                /* Ditto prev->prev */

  if (BI != NULL)
    BI->prev = BD;
}


void
ScreenDelLines(SCREEN *pScr, int n, int s)
{
  SCREENLINE *BI;
  SCREENLINE *TI;
  SCREENLINE *TD;
  SCREENLINE *BD;
  SCREENLINE *pLine;
  int idx;
  RECT rc;
  HDC hDC;

  pScr->bWrapPending = FALSE;

  if (s < 0)
    s = pScr->y;

  if (s + n - 1 > pScr->bottom)
    n = pScr->bottom - s + 1;

  TD = GetScreenLineFromY(pScr, s);
  BD = GetScreenLineFromY(pScr, s + n - 1);
  TI = GetScreenLineFromY(pScr, pScr->bottom);
  BI = TI->next;

  /*
   * Adjust the top of the screen and buffer if they will move.
   */
  if (TD == pScr->screen_top) {
    if (pScr->screen_top == pScr->buffer_top)
      pScr->buffer_top = BD->next;
    pScr->screen_top = BD->next;
  }

  /*
   * Adjust the bottom of the screen and buffer if they will move.
   */
  if (TI == pScr->screen_bottom) {
    if (pScr->screen_bottom == pScr->buffer_bottom)
      pScr->buffer_bottom = BD;
    pScr->screen_bottom = BD;
  }

  if (TI != BD)
    ScreenListMove(TD, BD, TI, BI);

  /*
   * Clear the lines moved from the deleted area to the
   * bottom of the scrolling area.
   */
  pLine = TI;

  for (idx = 0; idx < n; idx++) {
    pLine = pLine->next;
    ScreenClearLine(pScr, pLine);
  }

  /*	CheckScreen(pScr); */

  /*
   * Scroll the affected area on the screen.
   */
  rc.left = 0;
  rc.right = pScr->width * pScr->cxChar;
  rc.top = s * pScr->cyChar;
  rc.bottom = (pScr->bottom + 1) * pScr->cyChar;

  hDC = GetDC(pScr->hWnd);

  ScrollDC(hDC, 0, -pScr->cyChar * n, &rc, &rc, NULL, NULL);

  PatBlt(hDC, 0, (pScr->bottom - n + 1) * pScr->cyChar,
	 pScr->width * pScr->cxChar, n * pScr->cyChar, WHITENESS);

  ReleaseDC(pScr->hWnd, hDC);
}


void
ScreenInsertLine(SCREEN *pScr, int s)
{
  ScreenInsLines(pScr, 1, s);
}


void
ScreenInsLines(SCREEN *pScr, int n, int s)
{
  SCREENLINE *TI;
  SCREENLINE *BI;
  SCREENLINE *TD;
  SCREENLINE *BD;
  SCREENLINE *pLine;
  int idx;
  RECT rc;
  HDC hDC;

  pScr->bWrapPending = FALSE;

  if (s < 0)
    s = pScr->y;

  if (s + n - 1 > pScr->bottom)
    n = pScr->bottom - s + 1;

  /*
   * Determine the top and bottom of the insert area.  Also determine
   * the top and bottom of the area to be deleted and moved to the
   * insert area.
   */
  BI = GetScreenLineFromY(pScr, s);
  TI = BI->prev;
  TD = GetScreenLineFromY(pScr, pScr->bottom - n + 1);
  BD = GetScreenLineFromY(pScr, pScr->bottom);

  /*
   * Adjust the top of the screen and buffer if they will move.
   */
  if (BI == pScr->screen_top) {
    if (pScr->screen_top == pScr->buffer_top)
      pScr->buffer_top = TD;
    pScr->screen_top = TD;
  }

  /*
   * Adjust the bottom of the screen and buffer if they will move.
   */
  if (BD == pScr->screen_bottom) {
    if (pScr->screen_bottom == pScr->buffer_bottom)
      pScr->buffer_bottom = TD->prev;
    pScr->screen_bottom = TD->prev;
  }

  /*
   * Move lines from the bottom of the scrolling region to the insert area.
   */
  if (TD != BI)
    ScreenListMove(TD,BD,TI,BI);

  /*
   * Clear the inserted lines
   */
  pLine = GetScreenLineFromY(pScr, s);

  for (idx = 0; idx < n; idx++) {
    ScreenClearLine(pScr, pLine);
    pLine = pLine->next;
  }

  /*	CheckScreen(pScr); */

  /*
   * Scroll the affected area on the screen.
   */
  rc.left = 0;
  rc.right = pScr->width * pScr->cxChar;
  rc.top = s * pScr->cyChar;
  rc.bottom = (pScr->bottom + 1) * pScr->cyChar;

  hDC = GetDC(pScr->hWnd);

  ScrollDC(hDC, 0, pScr->cyChar * n, &rc, &rc, NULL, NULL);

  PatBlt(hDC, 0, s * pScr->cyChar,
	 pScr->width * pScr->cxChar, n * pScr->cyChar, WHITENESS);

  ReleaseDC(pScr->hWnd, hDC);
}


void
ScreenIndex(SCREEN * pScr)
{
  if (pScr->y >= pScr->bottom)
    ScreenScroll(pScr);
  else
    pScr->y++;

  pScr->bWrapPending = FALSE;
}


void
ScreenWrapNow(SCREEN *pScr, int *xp, int *yp)
{
  if (pScr->bWrapPending && pScr->x >= pScr->width - 1) {
    pScr->x = 0;
    ScreenIndex(pScr);
  }

  pScr->bWrapPending = FALSE;

  *xp = pScr->x;
  *yp = pScr->y;
}


void
ScreenEraseToEOL(SCREEN *pScr)
{
  int x1 = pScr->x;
  int y1 = pScr->y;
  int x2 = pScr->width;
  int y2 = pScr->y;
  int n = -1;
  SCREENLINE *pScrLine;
  RECT rc;

  ScreenWrapNow(pScr, &x1, &y1);

  y2 = y1;
#if 0
  wsprintf(strTmp,"[EraseEOL:%d]",y2);
  OutputDebugString(strTmp);
#endif
  pScrLine = GetScreenLineFromY(pScr,y2);
  memset(&pScrLine->attrib[x1], ScreenClearAttrib, pScr->width-x1+1);
  memset(&pScrLine->text[x1], ' ', pScr->width - x1 + 1);
  rc.left = x1 * pScr->cxChar;
  rc.right = pScr->width * pScr->cxChar;
  rc.top = pScr->cyChar * y1;
  rc.bottom = pScr->cyChar * (y1 + 1);
  InvalidateRect(pScr->hWnd, &rc, TRUE);
  UpdateWindow(pScr->hWnd);
}


void
ScreenDelChars(SCREEN *pScr, int n)
{
  int x = pScr->x;
  int y = pScr->y;
  int width;
  SCREENLINE *pScrLine;
  RECT rc;

  pScr->bWrapPending = FALSE;

  pScrLine = GetScreenLineFromY(pScr, y);

  width = pScr->width - x - n;

  if (width > 0) {
    memmove(&pScrLine->attrib[x], &pScrLine->attrib[x + n], width);
    memmove(&pScrLine->text[x], &pScrLine->text[x + n], width);
  }

  memset(&pScrLine->attrib[pScr->width - n], ScreenClearAttrib, n);
  memset(&pScrLine->text[pScr->width - n], ' ', n);

  rc.left = x * pScr->cxChar;
  rc.right = pScr->width * pScr->cxChar;
  rc.top = pScr->cyChar * y;
  rc.bottom = pScr->cyChar * (y + 1);

  InvalidateRect(pScr->hWnd, &rc, TRUE);

  UpdateWindow(pScr->hWnd);
}


void
ScreenRevIndex(SCREEN *pScr)
{
  SCREENLINE *pScrLine;
  SCREENLINE *pTopLine;

  pScr->bWrapPending = FALSE;
  pScrLine = GetScreenLineFromY(pScr, pScr->y);
  pTopLine = GetScreenLineFromY(pScr, pScr->top);

  if(pScrLine == pTopLine)
    ScreenInsertLine(pScr, pScr->y);
  else
    pScr->y--;
}


void
ScreenEraseToBOL(SCREEN *pScr)
{
  int x1 = 0;
  int y1 = pScr->y;
  int x2 = pScr->x;
  int y2 = pScr->y;
  int n = -1;
  SCREENLINE *pScrLine;

  pScrLine = GetScreenLineFromY(pScr, pScr->y);

  ScreenWrapNow(pScr, &x2, &y1);
  y2 = y1;
  memset(pScrLine->attrib, ScreenClearAttrib, x2);
  memset(pScrLine->text, ' ', x2);
}


void
ScreenEraseLine(SCREEN *pScr, int s)
{
  int x1 = 0;
  int y1 = s;
  int x2 = pScr->width;
  int y2 = s;
  int n = -1;
  SCREENLINE *pScrLine;
  RECT rc;

  if (s < 0) {
    ScreenWrapNow(pScr, &x1, &y1);
    s = y2 = y1;
    x1 = 0;
  }

  pScrLine = GetScreenLineFromY(pScr,y1);
  memset(pScrLine->attrib, ScreenClearAttrib, pScr->width);
  memset(pScrLine->text, ' ', pScr->width);
  rc.left = 0;
  rc.right = pScr->width * pScr->cxChar;
  rc.top = pScr->cyChar * y1;
  rc.bottom = pScr->cyChar * (y1+1);
  InvalidateRect(pScr->hWnd, &rc, TRUE);
  SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
}


void
ScreenEraseToEndOfScreen(SCREEN *pScr)
{
  int i;
  int x1 = 0;
  int y1 = pScr->y+1;
  int x2 = pScr->width;
  int y2 = pScr->height;
  int n = -1;

  ScreenWrapNow(pScr, &x1, &y1);
  y1++;
  x1 = 0;
  i = y1;
  ScreenEraseToEOL(pScr);
  while (i < pScr->height) {
    ScreenELO(pScr, i);
    ScreenEraseLine(pScr, i);
    i++;
  }
}


void
ScreenRange(SCREEN *pScr)
{
  if (pScr->x < 0)
    pScr->x = 0;

  if (pScr->x >= pScr->width)
    pScr->x = pScr->width - 1;

  if (pScr->y < 0)
    pScr->y = 0;

  if (pScr->y >= pScr->height)
    pScr->y = pScr->height - 1;
}


void
ScreenAlign(SCREEN *pScr)  /* vt100 alignment, fill screen with 'E's */
{
  char *tt;
  int i;
  int j;
  SCREENLINE *pScrLine;

  pScrLine = GetScreenLineFromY(pScr, pScr->top);
  ScreenEraseScreen(pScr);

  for(j = 0; j < pScr->height; j++) {
    tt = &pScrLine->text[0];
    for(i = 0; i <= pScr->width; i++)
      *tt++ = 'E';
    pScrLine = pScrLine->next;
  }
}


void
ScreenApClear(SCREEN *pScr)
{
  /*
   * reset all the ANSI parameters back to the default state
   */
  for(pScr->parmptr=5; pScr->parmptr>=0; pScr->parmptr--)
    pScr->parms[pScr->parmptr] = -1;

  pScr->parmptr = 0;
}


void
ScreenSetOption(SCREEN *pScr, int toggle)
{
  if (pScr->parms[0] == -2 && pScr->parms[1] == 1)
    pScr->DECCKM = toggle;

#if 0
  switch(pScr->parms[0]) {

  case -2:	/* Set on the '?' char */
    switch(pScr->parms[1]) {

    case 1: /* set/reset cursor key mode */
      pScr->DECCKM = toggle;
      break;

#ifdef NOT_SUPPORTED
    case 2: /* set/reset ANSI/vt52 mode */
      break;
#endif

    case 3: /* set/reset column mode */
      pScr->x = pScr->y = 0;  /* Clear the screen, mama! */
      ScreenEraseScreen(pScr);
#if 0	/* removed for variable screen size */
      if (toggle)  /* 132 column mode */
	pScr->width = pScr->allwidth;
      else
	pScr->width = 79;
#endif
      break;

#ifdef NOT_SUPPORTED
    case 4: /* set/reset scrolling mode */
    case 5: /* set/reset screen mode */
    case 6: /* set/rest origin mode */
      pScr->DECORG = toggle;
      break;
#endif

    case 7:	/* set/reset wrap mode */
      pScr->DECAWM = toggle;
#if 0
      /*
       * QAK - 7/27/90: added because resetting the virtual screen's
       * wrapping flag doesn't reset telnet window's wrapping
       */
      set_vtwrap(pScrn, fpScr->DECAWM);
#endif
      break;

#ifdef NOT_SUPPORTED
    case 8: /* set/reset autorepeat mode */
    case 9: /* set/reset interlace mode */
      break;
#endif

    default:
      break;
    } /* end switch */
    break;

  case 4:
    pScr->IRM=toggle;
    break;

  default:
    break;

  } /* end switch */
#endif
}


#ifdef NOT_SUPPORTED
void
ScreenTab(SCREEN *pScr)
{
  if (pScr->x> = pScr->width)
    pScr->x = pScr->width;
  pScr->x++;
  while (pScr->tabs[fpScr->x] != 'x' && pScr->x < pScr->width)
    pScr->x++;
}
#endif


BOOL
ScreenInsChar(SCREEN *pScr, int x)
{
  int i;
  SCREENLINE *pScrLine;
  RECT rc;

  pScrLine = GetScreenLineFromY(pScr, pScr->y);
  if (pScrLine == NULL)
    return(FALSE);

  for(i = pScr->width - x; i >= pScr->x; i--) {
    pScrLine->text[x+i] = pScrLine->text[i];
    pScrLine->attrib[x+i] = pScrLine->attrib[i];
  }

  memset(&pScrLine->attrib[pScr->x], ScreenClearAttrib, x);
  memset(&pScrLine->text[pScr->x], ' ', x);
  rc.left = pScr->cxChar * x;
  rc.right = pScr->cxChar * (x + pScr->x);
  rc.top = pScr->cyChar * (pScr->y - 1);
  rc.bottom = pScr->cyChar * pScr->y;
  InvalidateRect(pScr->hWnd, &rc, TRUE);
  SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
  return(TRUE);
}


void
ScreenSaveCursor(SCREEN *pScr)
{
  pScr->Px = pScr->x;
  pScr->Py = pScr->y;
  pScr->Pattrib = pScr->attrib;
}


void
ScreenRestoreCursor(SCREEN *pScr)
{
  pScr->x = pScr->Px;
  pScr->y = pScr->Py;
  ScreenRange(pScr);
}


void
ScreenDraw(SCREEN *pScr, int x, int y, int a, int len, char *c)
{
  int idx;
  SCREENLINE *pScrLine;
  RECT rc;

  pScrLine = GetScreenLineFromY(pScr, y);
  assert(pScrLine != NULL);

  for(idx = x; idx < x + len; idx++) {
    pScrLine->text[idx] = c[idx - x];
    pScrLine->attrib[idx - x] = a;
  }

  rc.left = pScr->cxChar * x;
  rc.right = pScr->cxChar * (x + len);
  rc.top = pScr->cyChar * pScr->y;
  rc.bottom = pScr->cyChar * (pScr->y + 1);
  InvalidateRect(pScr->hWnd, &rc, TRUE);
  SendMessage(pScr->hWnd, WM_PAINT, 0, 0);
}


#if ! defined(NDEBUG)

BOOL
CheckScreen(SCREEN *pScr)
{
  SCREENLINE *pLinePrev;
  SCREENLINE *pLine;
  int nscreen = 0;
  int nbuffer = 0;
  int topline = 0;
  char buf[512];
  BOOL bBottom;
  BOOL bOK;

  pLine = pScr->buffer_top;

  if (pLine == NULL) {
    OutputDebugString("CheckScreen: buffer_top invalid");
    MessageBox(NULL, "buffer_top invalid", "CheckScreen", MB_OK);
    return(FALSE);
  }

  bBottom = FALSE;
  while (TRUE) {
    pLinePrev = pLine;
    if (nscreen > 0 || pLine == pScr->screen_top)
      if (!bBottom)
	nscreen++;
    nbuffer++;
    if (pLine == pScr->screen_top)
      topline = nbuffer - 1;
    if (pLine == pScr->screen_bottom)
      bBottom = TRUE;
    pLine = pLine->next;
    if (pLine == NULL)
      break;
    if (pLine->prev != pLinePrev) {
      wsprintf(buf,
	       "Previous ptr of line %d does not match next ptr of line %d",
	       nbuffer, nbuffer - 1);
      OutputDebugString(buf);
      MessageBox(NULL, buf, "CheckScreen", MB_OK);
    }
  }

  if (pLinePrev == pScr->buffer_bottom && nscreen == pScr->height)
    bOK = TRUE;
  else {
    OutputDebugString("CheckScreen: Invalid number of lines on screen");
    bOK = FALSE;
  }

  wsprintf(buf, "screen.width = %d\nscreen.height = %d\nscreen.maxlines = %d\nscreen.numlines = %d\nscreen.x = %d\nscreen.y = %d\nscreen.top = %d\nscreen.bottom = %d\nActual top line = %d\nActual buffer lines = %d\nActual screen lines = %d\nBottom of buffer is %s",
	   pScr->width, pScr->height, pScr->maxlines, pScr->numlines,
	   pScr->x, pScr->y, pScr->top, pScr->bottom,
	   topline, nbuffer, nscreen,
	   (pLinePrev == pScr->buffer_bottom) ? "valid" : "invalid");

  MessageBox(NULL, buf, "CheckScreen", MB_OK);

  return(bOK);
}

#endif
