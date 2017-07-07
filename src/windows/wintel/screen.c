/* screen.c */

#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "telnet.h"
#include "ini.h"
#include "auth.h"

extern char *encrypt_output;  /* XXX hack... I wonder if this will work.  These are */
extern char *decrypt_input;   /* XXX really functions...  */

extern char *cInvertedArray;
extern int bMouseDown;
extern int bSelection;

static SCREEN *ScreenList;
static HINSTANCE hInst;
static char szScreenClass[] = "ScreenWClass";
static char szScreenMenu[] = "ScreenMenu";
static char cursor_key[8][4] = {		/* Send for cursor keys */
  "\x1B[D", "\x1B[A", "\x1B[C", "\x1B[B", /* Normal mode */
  "\x1BOD", "\x1BOA", "\x1BOC", "\x1BOB",	/* Numpad on mode */
};

void
ScreenInit(HINSTANCE hInstance)
{
  BOOL b;
  WNDCLASS wc;

  hInst = hInstance;

  ScreenList = NULL;

  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; /* Class style(s) */
  wc.lpfnWndProc = ScreenWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = sizeof(long);
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, "TERMINAL");
  wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
  wc.hbrBackground = GetStockObject(WHITE_BRUSH);
  wc.lpszMenuName =  szScreenMenu;
  wc.lpszClassName = szScreenClass;

  b = RegisterClass(&wc);
  assert(b);
}


void
SetScreenInstance(HINSTANCE hInstance)
{
  hInst = hInstance;
}

int
GetNewScreen(void)
{
  SCREEN *pScr;
  static int id = 0;

  pScr = (SCREEN *) calloc(sizeof(SCREEN), 1);
  if (pScr == NULL)
    return(-1);

  if (ScreenList == NULL) {
    pScr->next = NULL;
    pScr->prev = NULL;
  }
  else {
    if (ScreenList->next == NULL) {
      ScreenList->next = ScreenList;
      ScreenList->prev = ScreenList;
    }
    pScr->next = ScreenList;
    pScr->prev = ScreenList->prev;
    ScreenList->prev->next = pScr;
    ScreenList->prev = pScr;
  }

  ScreenList = pScr;
  return(id++);
}

SCREENLINE *
ScreenNewLine(void)
{
  SCREENLINE *pScrLine;

  pScrLine = calloc(sizeof(SCREENLINE) + 2*MAX_LINE_WIDTH, 1);
  if (pScrLine == NULL)
    return (NULL);
  pScrLine->text = &pScrLine->buffer[0];
  pScrLine->attrib = &pScrLine->buffer[MAX_LINE_WIDTH];
  return(pScrLine);
}

static void
MakeWindowTitle(char *host, int width, int height, char *title, int nchars)
{
  char buf[128];
  int hlen;

  hlen = strlen(host);

  title[0] = 0;

  if (hlen + 1 > nchars)
    return;

  strcpy(title, host);

  wsprintf(buf, " (%dh x %dw)", height, width);

  if ((int) strlen(buf) + hlen + 1 > nchars)
    return;

  strcat(title, buf);
}


SCREEN *
InitNewScreen(CONFIG *Config)
{
  TEXTMETRIC tm;
  HMENU hMenu = NULL;
  SCREEN *scr = NULL;
  SCREENLINE *pScrLine;
  SCREENLINE *pScrLineLast;
  int id;
  int idx = 0;
  char title[128];
  HDC hDC;
  HFONT hFont;

  id = GetNewScreen();
  if (id == -1)
    return(0);

  scr = ScreenList;
  assert(scr != NULL);

  hMenu = LoadMenu(hInst, szScreenMenu);
  assert(hMenu != NULL);

  scr->title = Config->title;
  MakeWindowTitle(Config->title, Config->width, Config->height,
		  title, sizeof(title));

  scr->hwndTel = Config->hwndTel;  /* save HWND of calling window */

  if (Config->backspace) {
    CheckMenuItem(hMenu, IDM_BACKSPACE, MF_CHECKED);
    CheckMenuItem(hMenu, IDM_DELETE, MF_UNCHECKED);
  } else {
    CheckMenuItem(hMenu, IDM_BACKSPACE, MF_UNCHECKED);
    CheckMenuItem(hMenu, IDM_DELETE, MF_CHECKED);
  }

  hDC = GetDC(NULL);
  assert(hDC != NULL);

  scr->lf.lfPitchAndFamily = FIXED_PITCH;
  GetPrivateProfileString(INI_FONT, "FaceName", "Courier", scr->lf.
			  lfFaceName,	LF_FACESIZE, TELNET_INI);
  scr->lf.lfHeight = (int) GetPrivateProfileInt(INI_FONT, "Height", 0, TELNET_INI);
  scr->lf.lfWidth = (int) GetPrivateProfileInt(INI_FONT, "Width", 0, TELNET_INI);
  scr->lf.lfPitchAndFamily = (BYTE) GetPrivateProfileInt(INI_FONT, "PitchAndFamily", 0, TELNET_INI);
  scr->lf.lfCharSet = (BYTE) GetPrivateProfileInt(INI_FONT, "CharSet", 0, TELNET_INI);
  scr->lf.lfEscapement = (BYTE) GetPrivateProfileInt(INI_FONT, "Escapement", 0, TELNET_INI);
  scr->lf.lfQuality = PROOF_QUALITY;
  scr->hSelectedFont = CreateFontIndirect((LPLOGFONT) &(scr->lf));
  hFont = SelectObject(hDC, scr->hSelectedFont);
  GetTextMetrics(hDC, (LPTEXTMETRIC) &tm);
  SelectObject(hDC, hFont);
  scr->cxChar = tm.tmAveCharWidth;
  scr->cyChar = tm.tmHeight + tm.tmExternalLeading;

  ReleaseDC(NULL, hDC);

  scr->width = Config->width;
  scr->height = Config->height;
  scr->ID = id;
  scr->x = 0;
  scr->y = 0;
  scr->Oldx = 0;
  scr->Oldy = 0;
  scr->attrib = 0;
  scr->DECAWM = 1;
  scr->bWrapPending = FALSE;
  scr->top = 0;
  scr->bottom = scr->height-1;
  scr->parmptr = 0;
  scr->escflg = 0;
  scr->bAlert = FALSE;
  scr->numlines = 0;
  scr->maxlines = 150;

  cInvertedArray = calloc(scr->width * scr->height, 1);

  pScrLineLast = ScreenNewLine();
  if (pScrLineLast == NULL)
    return(NULL);
  scr->screen_top = scr->buffer_top = pScrLineLast;

  for (idx = 0; idx < scr->height - 1; idx++) {
    pScrLine = ScreenNewLine();
    if (pScrLine == NULL)
      return(NULL);
    pScrLine->prev = pScrLineLast;
    pScrLineLast->next = pScrLine;
    pScrLineLast = pScrLine;
  }

  scr->screen_bottom = scr->buffer_bottom = pScrLine;

  scr->hWnd = CreateWindow(szScreenClass, title, WS_OVERLAPPEDWINDOW | WS_VSCROLL,
			   CW_USEDEFAULT, CW_USEDEFAULT,
			   scr->cxChar * scr->width + FRAME_WIDTH,
			   scr->cyChar * scr->height + FRAME_HEIGHT,
			   NULL, hMenu, hInst, scr);
  assert(scr->hWnd != NULL);

  ShowWindow(scr->hWnd, SW_SHOW);

  CreateCaret(scr->hWnd, NULL, scr->cxChar, 2);
  SetCaretPos(scr->x*scr->cxChar, (scr->y+1) * scr->cyChar);
  ShowCaret(scr->hWnd);

  return(ScreenList);
}


void DeleteTopLine(
		   SCREEN *pScr)
{
  assert(pScr->buffer_top != NULL);

  pScr->buffer_top = pScr->buffer_top->next;
  assert(pScr->buffer_top != NULL);

  free(pScr->buffer_top->prev);
  pScr->buffer_top->prev = NULL;

  pScr->numlines--;

} /* DeleteTopLine */


static void SetScreenScrollBar(
			       SCREEN *pScr)
{
  if (pScr->numlines <= 0) {
    SetScrollRange(pScr->hWnd, SB_VERT, 0, 100, FALSE);
    SetScrollPos(pScr->hWnd, SB_VERT, 0, TRUE);
    EnableScrollBar(pScr->hWnd, SB_VERT, ESB_DISABLE_BOTH);
  }
  else {
    SetScrollRange(pScr->hWnd, SB_VERT, 0, pScr->numlines, FALSE);
    SetScrollPos(pScr->hWnd, SB_VERT, pScr->numlines, TRUE);
    EnableScrollBar(pScr->hWnd, SB_VERT, ESB_ENABLE_BOTH);
  }

} /* SetScreenScrollBar */


int ScreenScroll(
		 SCREEN *pScr)
{
  SCREENLINE *pScrLine;
  SCREENLINE *pPrev;
  SCREENLINE *pNext;
  SCREENLINE *pScrollTop;
  SCREENLINE *pScrollBottom;
  BOOL bFullScreen = TRUE;
  HDC hDC;
  RECT rc;

  Edit_ClearSelection(pScr);

  pScrollTop = GetScreenLineFromY(pScr, pScr->top);

  pScrollBottom = GetScreenLineFromY(pScr, pScr->bottom);

  if (pScrollTop != pScr->screen_top) {
    bFullScreen = FALSE;
    rc.left = 0;
    rc.right = pScr->cxChar * pScr->width;
    rc.top = pScr->cyChar * (pScr->top);
    rc.bottom = pScr->cyChar * (pScr->bottom+1);

    pNext = pScrollTop->next;
    pPrev = pScrollTop->prev;

    pPrev->next = pNext;
    pNext->prev = pPrev;

    pScrLine = pScrollTop;
    ScreenClearLine(pScr, pScrLine);
  }
  else {
    pScr->numlines++;
    pScrLine = ScreenNewLine();
    if (pScrLine == NULL)
      return(0);
    pScr->screen_top = pScrollTop->next;
  }

  if (pScrLine == NULL)
    return(0);

  pNext = pScrollBottom->next;
  pScrollBottom->next = pScrLine;
  pScrLine->next = pNext;
  pScrLine->prev = pScrollBottom;
  if (pNext != NULL)
    pNext->prev = pScrLine;

  if (pScrollBottom != pScr->screen_bottom) {
    bFullScreen = FALSE;
    rc.left = 0;
    rc.right = pScr->cxChar * pScr->width;
    rc.top = pScr->cyChar * pScr->top;
    rc.bottom = pScr->cyChar * (pScr->bottom+1);
  }
  else {
    if (pScr->screen_bottom == pScr->buffer_bottom)
      pScr->buffer_bottom = pScrLine;
    pScr->screen_bottom = pScrLine;
  }

#if 0
  CheckScreen(fpScr);
#endif

  pScr->y++;

  if (pScr->y > pScr->bottom)
    pScr->y = pScr->bottom;

  hDC = GetDC(pScr->hWnd);
  assert(hDC != NULL);

  if (bFullScreen)
    ScrollDC(hDC, 0, -pScr->cyChar, NULL, NULL, NULL, NULL);
  else
    ScrollDC(hDC, 0, -pScr->cyChar, &rc, &rc, NULL, NULL);

  PatBlt(hDC, 0, pScr->bottom * pScr->cyChar,
	 pScr->width * pScr->cxChar, pScr->cyChar, WHITENESS);

  ReleaseDC(pScr->hWnd, hDC);

  if (pScr->numlines == pScr->maxlines)
    DeleteTopLine(pScr);
  else
    SetScreenScrollBar(pScr);

  return(1);

} /* ScreenScroll */


int DrawTextScreen(
		   RECT rcInvalid,
		   SCREEN *pScr,
		   HDC hDC)
{
  SCREENLINE *pScrLineTmp;
  SCREENLINE *pScrLine;
  int x = 0;
  int y = 0;
  int left = 0;
  int right = 0;
  int i;
  int len;
  char attrib;
#define YPOS (y*pScr->cyChar)

  pScrLine = pScr->screen_top;

  for (y = 0; y < pScr->height; y++) {
    if (!pScrLine)
      continue;

    if (YPOS >= rcInvalid.top - pScr->cyChar &&
	YPOS <= rcInvalid.bottom + pScr->cyChar) {

      if (y < 0)
	y = 0;

      if (y >= pScr->height)
	y = pScr->height - 1;

      left = (rcInvalid.left / pScr->cxChar) - 1;

      right = (rcInvalid.right / pScr->cxChar) + 1;

      if (left < 0)
	left = 0;

      if (right > pScr->width - 1)
	right = pScr->width - 1;

      x = left;

      while (x <= right) {
	if (!pScrLine->text[x]) {
	  x++;
	  continue;
	}

	if (SCR_isrev(pScrLine->attrib[x])) {
	  SelectObject(hDC, pScr->hSelectedFont);
	  SetTextColor(hDC, RGB(255, 255, 255));
	  SetBkColor(hDC, RGB(0, 0, 0));
	}
	else if (SCR_isblnk(pScrLine->attrib[x])) {
	  SelectObject(hDC, pScr->hSelectedFont);
	  SetTextColor(hDC, RGB(255, 0, 0));
	  SetBkColor(hDC, RGB(255, 255, 255));
	}
	else if (SCR_isundl(pScrLine->attrib[x])) {
	  SetTextColor(hDC, RGB(255, 0, 0));
	  SetBkColor(hDC, RGB(255, 255, 255));
	  SelectObject(hDC, pScr->hSelectedULFont);
	}
	else {
	  SelectObject(hDC,pScr->hSelectedFont);
	  SetTextColor(hDC, RGB(0, 0, 0));
	  SetBkColor(hDC, RGB(255, 255, 255));
	}

	len = 1;
	attrib = pScrLine->attrib[x];
	for (i = x + 1; i <= right; i++) {
	  if (pScrLine->attrib[i] != attrib || !pScrLine->text[i])
	    break;
	  len++;
	}

	TextOut(hDC, x*pScr->cxChar, y*pScr->cyChar, &pScrLine->text[x], len);
	x += len;
      }
    }
    pScrLineTmp = pScrLine->next;
    pScrLine = pScrLineTmp;
  }

  return(0);

} /* DrawTextScreen */


static BOOL SetInternalScreenSize(
				  SCREEN *pScr,
				  int width,
				  int height)
{
  RECT rc;
  char *p;
  int idx;
  int n;
  int newlines;
  SCREENLINE *pNewLine;
  SCREENLINE *pTopLine;
  SCREENLINE *pBottomLine;
#if 0
  int col;
  int row;
  int dydestbottom;
#endif

  GetClientRect(pScr->hWnd, &rc);

  width = (rc.right - rc.left) / pScr->cxChar;
  height = (rc.bottom - rc.top) / pScr->cyChar;

  if (pScr->height == height && pScr->width == width)
    return(FALSE);

  pScr->Oldx = 0;
  pScr->Oldy = 0;
  pScr->attrib = 0;

  /*
    Reallocate the inverted array of bytes and copy the values
    from the old screen to the new screen.
    */
  p = calloc(width * height, 1);

  ScreenCursorOff(pScr);

#if 0	/* Copy inversion array to desitination */
  for (col = 0; col < width; col++) {
    for (row = 0; row < height; row++) {
      dydestbottom = height - 1 - row;
      if (col < pScr->width && dydestbottom < pScr->height - 1)
	p[row * width + col] =
	  cInvertedArray[(pScr->height - 1 - dydestbottom) * pScr->width + col];
    }
  }
#endif

  free(cInvertedArray);
  cInvertedArray = p;

  /*
    Append any new lines which need to be added to accomodate the new
    screen size.
    */
  pBottomLine = pScr->buffer_bottom;
  newlines = height - (pScr->height + pScr->numlines);

  if (newlines > 0) {
    pScr->y += pScr->numlines;
    pScr->numlines = 0;

    for (idx = 0; idx < newlines; idx++) {
      pNewLine = ScreenNewLine();
      if (pNewLine == NULL)
	return(FALSE);
      pNewLine->prev = pBottomLine;
      if (pBottomLine == NULL)
	return(FALSE);
      pBottomLine->next = pNewLine;
      pBottomLine = pNewLine;
    }
  }

  /*
    If we already have plenty of lines, then we need to get rid of the
    scrollback lines, if too many exist.  The cursor should end up
    the same distance from the bottom of the screen as is started out
    in this instance.
    */
  if (newlines < 0) {
    pScr->y = (height - 1) - (pScr->bottom - pScr->y);
    if (pScr->y < 0)
      pScr->y = 0;
    pScr->numlines = -newlines;
    n = pScr->numlines - pScr->maxlines;
    for (idx = 0; idx < n; idx++)
      DeleteTopLine(pScr);
  }

  /*
    Calculate the position of the buffer relative to the screen.
    */
  pScr->screen_bottom = pBottomLine;
  pScr->buffer_bottom = pBottomLine;

  pTopLine = pBottomLine;

  for (idx = 1; idx < height; idx++) {
    pTopLine = pTopLine->prev;
  }

  pScr->screen_top = pTopLine;
  pScr->width = width;
  pScr->height = height;
  pScr->top = 0;
  pScr->bottom = height - 1;

  if (pScr->x >= width)
    pScr->x = width - 1;

  if (pScr->y >= height)
    pScr->y = height - 1;

  SetScreenScrollBar(pScr);
  ScreenCursorOn(pScr);
  return(TRUE);

} /* SetInternalScreenSize */


static int ScreenAdjustUp(
			  SCREEN *pScr,
			  int n)
{
  int idx;
  SCREENLINE *pLine1;
  SCREENLINE *pLine2;

  for (idx = 0; idx < n; idx++) {
    if (pScr->screen_top == pScr->buffer_top)
      return(-idx);
    pLine1 = pScr->screen_top->prev;
    if (pLine1 == NULL)
      return(-idx);
    pLine2 = pScr->screen_bottom->prev;
    if (pLine2 == NULL)
      return(-idx);
    pScr->screen_top = pLine1;
    pScr->screen_bottom = pLine2;
  }

  return(idx);

} /* ScreenAdjustUp */


static int ScreenAdjustDown(
			    SCREEN *pScr,
			    int n)
{
  int idx;
  SCREENLINE *pLine1;
  SCREENLINE *pLine2;

  for (idx = 0; idx < n; idx++) {
    if (pScr->screen_bottom == pScr->buffer_bottom)
      return(-idx);
    pLine1 = pScr->screen_top->next;
    if (pLine1 == NULL)
      return(-idx);
    pLine2 = pScr->screen_bottom->next;
    if (pLine2 == NULL)
      return(-idx);
    pScr->screen_top = pLine1;
    pScr->screen_bottom = pLine2;
  }

  return(idx);

} /* ScreenAdjustDown */


long PASCAL ScreenWndProc(
			      HWND hWnd,
			      UINT message,
			      WPARAM wParam,
			      LPARAM lParam)
{
  MINMAXINFO *lpmmi;
  SCREEN *pScr;
  HMENU hMenu;
  PAINTSTRUCT ps;
  int x = 0;
  int y = 0;
  int ScrollPos;
  int tmpScroll = 0;
  int idx;
  HDC hDC;
  RECT rc;
  char title[128];
  static int bDoubleClick = FALSE;

  switch (message) {

  case WM_COMMAND:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);

    switch (wParam) {

    case IDM_EXIT:
      if (MessageBox(hWnd, "Terminate this connection?", "Telnet", MB_OKCANCEL) == IDOK) {
	pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
	assert (pScr != NULL);
	SendMessage(pScr->hwndTel, WM_MYSCREENCLOSE, 0, (LPARAM) pScr);
      }
      break;

    case IDM_BACKSPACE:
      hMenu = GetMenu(hWnd);
      CheckMenuItem(hMenu, IDM_BACKSPACE, MF_CHECKED);
      CheckMenuItem(hMenu, IDM_DELETE, MF_UNCHECKED);
      SendMessage(pScr->hwndTel, WM_MYSCREENCHANGEBKSP, VK_BACK, (LPARAM) pScr);
      break;

    case IDM_DELETE:
      hMenu = GetMenu(hWnd);
      CheckMenuItem(hMenu, IDM_BACKSPACE, MF_UNCHECKED);
      CheckMenuItem(hMenu, IDM_DELETE, MF_CHECKED);
      SendMessage(pScr->hwndTel, WM_MYSCREENCHANGEBKSP, 0x7f, (LPARAM) pScr);
      break;

    case IDM_FONT:
      ScreenCursorOff(pScr);
      ProcessFontChange(hWnd);
      ScreenCursorOn(pScr);
      break;

    case IDM_COPY:
      Edit_Copy(hWnd);
      hMenu=GetMenu(hWnd);
      Edit_ClearSelection(pScr);
      break;

    case IDM_PASTE:
      Edit_Paste(hWnd);
      break;

    case IDM_HELP_INDEX:
      WinHelp(hWnd, HELP_FILE, HELP_INDEX, 0);
      break;

    case IDM_ABOUT:
#ifdef CYGNUS
#ifdef KRB4
      strcpy(strTmp, "          Kerberos 4 for Windows\n");
#endif
#ifdef KRB5
      strcpy(strTmp, "          KerbNet for Windows\n");
#endif
      strcat(strTmp, "\n                   Version 1.00\n\n");
      strcat(strTmp, "             For support, contact:\n");
      strcat(strTmp, "   Cygnus Support - (415) 903-1400\n");
#else /* CYGNUS */
      strcpy(strTmp, "   Kerberos 5 Telnet for Windows\n");
      strcat(strTmp, "               ALPHA SNAPSHOT 2\n\n");
#endif /* CYGNUS */
      if (encrypt_flag) {
	strcat(strTmp, "\n[Encryption of output requested.  State: ");
	strcat(strTmp, (encrypt_output ? "encrypting]" : "INACTIVE]"));
	strcat(strTmp, "\n[Decryption of input requested.  State: ");
	strcat(strTmp, (decrypt_input ? "decrypting]\n" : "INACTIVE]\n"));
      }
      MessageBox(NULL, strTmp, "Kerberos", MB_OK);
      break;

#if defined(DEBUG)
    case IDM_DEBUG:
      CheckScreen(pScr);
      break;
#endif
    }

    break;

  case WM_NCCREATE:
    pScr = (SCREEN *) ((LPCREATESTRUCT) lParam)->lpCreateParams;
    pScr->hWnd = hWnd;
    SetWindowLong(hWnd, SCREEN_HANDLE, (LONG) pScr);
    SetScrollRange(hWnd, SB_VERT, 0, 100, FALSE);
    SetScrollPos(hWnd, SB_VERT, 0, TRUE);
    EnableScrollBar(hWnd, SB_VERT, ESB_DISABLE_BOTH);
    return(TRUE);

  case WM_VSCROLL:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);

    ScreenCursorOff(pScr);

    switch(wParam) {

    case SB_LINEDOWN:
      if (ScreenAdjustDown(pScr, 1) <= 0)
	break;
      hDC = GetDC(hWnd);
      assert(hDC != NULL);
      rc.left = 0;
      rc.right = pScr->cxChar * pScr->width;
      rc.top = 0;
      rc.bottom = pScr->cyChar * (pScr->bottom + 1);
      ScrollDC(hDC, 0, -pScr->cyChar, &rc, &rc, NULL, NULL);
      ReleaseDC(hWnd, hDC);
      rc.top = pScr->cyChar * pScr->bottom;
      InvalidateRect(hWnd, &rc, TRUE);
      ScrollPos = GetScrollPos(hWnd, SB_VERT);
      SetScrollPos(hWnd, SB_VERT, ScrollPos + 1, TRUE);
      UpdateWindow(hWnd);
      break;

    case SB_LINEUP:
      if (ScreenAdjustUp(pScr, 1) <= 0)
	break;
      hDC = GetDC(hWnd);
      assert(hDC != NULL);
      rc.left = 0;
      rc.right = pScr->cxChar * pScr->width;
      rc.top = 0;
      rc.bottom = pScr->cyChar * (pScr->bottom + 1);
      ScrollDC(hDC, 0, pScr->cyChar, &rc, &rc, NULL, NULL);
      ReleaseDC(hWnd, hDC);
      rc.bottom = pScr->cyChar;
      InvalidateRect(hWnd, &rc, TRUE);
      ScrollPos = GetScrollPos(pScr->hWnd, SB_VERT);
      SetScrollPos(hWnd,SB_VERT, ScrollPos - 1, TRUE);
      UpdateWindow(hWnd);
      break;

    case SB_PAGEDOWN:
      idx = abs(ScreenAdjustDown(pScr, pScr->height));
      hDC = GetDC(hWnd);
      assert(hDC != NULL);
      rc.left = 0;
      rc.right = pScr->cxChar * pScr->width;
      rc.top = 0;
      rc.bottom = pScr->cyChar * (pScr->bottom+1);
      ScrollDC(hDC, 0, -idx * pScr->cyChar, &rc, &rc, NULL, NULL);
      ReleaseDC(hWnd, hDC);
      rc.top = pScr->cyChar * (pScr->bottom - idx + 1);
      InvalidateRect(hWnd, &rc, TRUE);
      ScrollPos=GetScrollPos(hWnd, SB_VERT);
      SetScrollPos(hWnd, SB_VERT, ScrollPos + idx, TRUE);
      break;

    case SB_PAGEUP:
      idx = abs(ScreenAdjustUp(pScr, pScr->height));
      hDC = GetDC(hWnd);
      assert(hDC != NULL);
      rc.left = 0;
      rc.right = pScr->cxChar * pScr->width;
      rc.top = 0;
      rc.bottom = pScr->cyChar * (pScr->bottom + 1);
      ScrollDC(hDC, 0, idx * pScr->cyChar, &rc, &rc, NULL, NULL);
      ReleaseDC(hWnd, hDC);
      rc.bottom = idx * pScr->cyChar;
      InvalidateRect(hWnd, &rc, TRUE);
      ScrollPos=GetScrollPos(hWnd, SB_VERT);
      SetScrollPos(hWnd, SB_VERT, ScrollPos - idx, TRUE);
      break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
      ScrollPos = GetScrollPos(hWnd, SB_VERT);
      tmpScroll = ScrollPos - LOWORD(lParam);
      if (tmpScroll == 0)
	break;
      if (tmpScroll > 0)
	ScreenAdjustUp(pScr, tmpScroll);
      else
	ScreenAdjustDown(pScr, -tmpScroll);
      if (abs(tmpScroll) < pScr->height) {
	hDC = GetDC(hWnd);
	assert(hDC != NULL);
	rc.left = 0;
	rc.right = pScr->cxChar * pScr->width;
	rc.top = 0;
	rc.bottom = pScr->cyChar * (pScr->bottom + 1);
	ScrollDC(hDC, 0, tmpScroll * pScr->cyChar, &rc, &rc, NULL, NULL);
	ReleaseDC(hWnd, hDC);
	if (tmpScroll > 0) {
	  rc.bottom = tmpScroll * pScr->cyChar;
	  InvalidateRect(hWnd, &rc, TRUE);
	}
	else {
	  rc.top = (pScr->bottom + tmpScroll + 1) * pScr->cyChar;
	  InvalidateRect(hWnd, &rc, TRUE);
	}
      }
      else
	InvalidateRect(hWnd, NULL, TRUE);

      SetScrollPos(hWnd, SB_VERT, LOWORD(lParam), TRUE);
      UpdateWindow(hWnd);
      break;
    }

    ScreenCursorOn(pScr);
    break;

  case WM_KEYDOWN:
    if (wParam == VK_INSERT) {
      if (GetKeyState(VK_SHIFT) < 0)
	PostMessage(hWnd, WM_COMMAND, IDM_PASTE, 0);
      else if (GetKeyState(VK_CONTROL) < 0)
	PostMessage(hWnd, WM_COMMAND, IDM_COPY, 0);
      break;
    }
    /*
    ** Check for cursor keys. With control pressed, we treat as
    ** keyboard equivalents to scrolling. Otherwise, we send
    ** a WM_MYCURSORKEY message with the appropriate string
    ** to be sent. Sending the actual string allows the upper
    ** level to be ignorant of keyboard modes, etc.
    */
    if (wParam < VK_PRIOR || wParam > VK_DOWN) /* Is it a cursor key? */
      break;

    if (GetKeyState (VK_CONTROL) >= 0) {	/* No control key */
      if (wParam >= VK_LEFT && wParam <= VK_DOWN) {
	pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
	assert (pScr != NULL);
	wParam = wParam - VK_LEFT + (pScr->DECCKM ? 4 : 0);
	SendMessage (pScr->hwndTel, WM_MYCURSORKEY,
		     strlen(cursor_key[wParam]),
		     (LPARAM) (char *) cursor_key[wParam]);
      }
    } else {								/* Control is down */
      switch (wParam) {
      case VK_PRIOR:						/* Page up   */
	SendMessage(hWnd, WM_VSCROLL, SB_PAGEUP, 0);
	break;
      case VK_NEXT:						/* Page down */
	SendMessage(hWnd, WM_VSCROLL, SB_PAGEDOWN, 0);
	break;
      case VK_UP:							/* Line up   */
	SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);
	break;
      case VK_DOWN:						/* Line down */
	SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
	break;
      }
    }
    UpdateWindow(hWnd);
    break;

  case WM_CHAR:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);
    SendMessage(pScr->hwndTel, WM_MYSCREENCHAR, wParam, (LPARAM) pScr);
    break;

  case WM_INITMENU:
    if (IsClipboardFormatAvailable(CF_TEXT))
      EnableMenuItem((HMENU) wParam, IDM_PASTE, MF_ENABLED);
    else
      EnableMenuItem((HMENU) wParam, IDM_PASTE, MF_GRAYED);
    if (bSelection)
      EnableMenuItem((HMENU) wParam, IDM_COPY, MF_ENABLED);
    else
      EnableMenuItem((HMENU) wParam, IDM_COPY, MF_GRAYED);
    break;

  case WM_GETMINMAXINFO:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    if (pScr == NULL) /* Used on creation when window word not set */
      pScr = ScreenList;
    lpmmi = (MINMAXINFO *) lParam;
    if (FRAME_WIDTH + MAX_LINE_WIDTH * pScr->cxChar < lpmmi->ptMaxSize.x)
      lpmmi->ptMaxSize.x = FRAME_WIDTH + MAX_LINE_WIDTH * pScr->cxChar;
    lpmmi->ptMaxTrackSize.x = lpmmi->ptMaxSize.x;
    lpmmi->ptMinTrackSize.x = FRAME_WIDTH + 20 * pScr->cxChar;
    lpmmi->ptMinTrackSize.y = FRAME_HEIGHT + 4 * pScr->cyChar;
    break;

  case WM_LBUTTONDOWN:
    if (bDoubleClick)
      Edit_TripleClick(hWnd, lParam);
    else
      Edit_LbuttonDown(hWnd, lParam);
    break;

  case WM_LBUTTONUP:
    Edit_LbuttonUp(hWnd, lParam);
    break;

  case WM_LBUTTONDBLCLK:
    bDoubleClick = TRUE;
    SetTimer(hWnd, TIMER_TRIPLECLICK, GetDoubleClickTime(), NULL);
    Edit_LbuttonDblclk(hWnd, lParam);
    break;

  case WM_TIMER:
    if (wParam == TIMER_TRIPLECLICK)
      bDoubleClick = FALSE;
    break;

  case WM_RBUTTONUP:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);
    Edit_Copy(hWnd);
    Edit_ClearSelection(pScr);
    Edit_Paste(hWnd);
    break;

  case WM_MOUSEMOVE:
    if (bMouseDown)
      Edit_MouseMove(hWnd, lParam);
    break;

  case WM_RBUTTONDOWN:
#if 0
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);
    wsprintf(strTmp,"fp->x=%d fp->y=%d text=%s \r\n",
	     pScr->screen_top->x, pScr->screen_top->y, pScr->screen_top->text);
    OutputDebugString(strTmp);
#endif
    break;

  case WM_PAINT:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);
    BeginPaint (hWnd, &ps);
    SelectObject(ps.hdc, pScr->hSelectedFont);
    if (pScr->screen_bottom != NULL)
      DrawTextScreen(ps.rcPaint, pScr, ps.hdc);
    else
      OutputDebugString("screen_bottom is NULL.\r\n");
    EndPaint(hWnd, &ps);
    break;

  case WM_CLOSE:
    if (MessageBox(hWnd, "Terminate this connection?", "Telnet", MB_OKCANCEL) == IDOK) {
      pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
      assert (pScr != NULL);
      SendMessage(pScr->hwndTel, WM_MYSCREENCLOSE, 0, (LPARAM) pScr);
      return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    break;

  case WM_DESTROY:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    if (pScr != NULL)
      DeleteObject(pScr->hSelectedFont);
    return (DefWindowProc(hWnd, message, wParam, lParam));

  case WM_ACTIVATE:
    if (wParam != WA_INACTIVE) {
      pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
      assert (pScr != NULL);
      if (pScr->bAlert) {
	char strTitle[128];
	int idx;

	GetWindowText(hWnd, strTitle, sizeof(strTitle));
	if (strTitle[0] == ALERT) {
	  idx = lstrlen(strTitle);
	  strTitle[idx - 2] = 0;
	  SetWindowText(hWnd, &strTitle[2]);
	  pScr->bAlert = FALSE;
	}
      }
    }
    return (DefWindowProc(hWnd, message, wParam, lParam));

  case WM_SIZE:
    if (wParam == SIZE_MINIMIZED)
      break;

    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);

    if (SetInternalScreenSize(pScr, LOWORD(lParam), HIWORD(lParam))) {
      SendMessage(pScr->hwndTel, WM_MYSCREENSIZE, 0,
		  MAKELONG(pScr->width, pScr->height));
    }
    MakeWindowTitle(pScr->title, pScr->width, pScr->height,
		    title, sizeof(title));
    SetWindowText(hWnd, title);
    break;

  case WM_SETFOCUS:
    pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
    assert (pScr != NULL);
    CreateCaret(hWnd, NULL, pScr->cxChar, 2);
    ScreenCursorOn(pScr);
    break;

  case WM_KILLFOCUS:
    DestroyCaret();
    break;

  default:
    return(DefWindowProc(hWnd, message, wParam, lParam));
  }

  return(0);

} /* ScreenWndProc */


void ScreenBell(
		SCREEN *pScr)
{
  char strTitle[128];
  int idx;

  MessageBeep(MB_ICONEXCLAMATION);
  if (pScr->hWnd != GetActiveWindow()) {
    FlashWindow(pScr->hWnd, TRUE);
    if (!pScr->bAlert) {
      strTitle[0] = ALERT;
      strTitle[1] = SPACE;
      GetWindowText(pScr->hWnd, &strTitle[2], sizeof(strTitle) - 2);
      idx = lstrlen(strTitle);
      strTitle[idx] = SPACE;
      strTitle[idx+1] = ALERT;
      strTitle[idx+2] = 0;
      SetWindowText(pScr->hWnd, strTitle);
    }
    FlashWindow(pScr->hWnd, FALSE);
    pScr->bAlert = TRUE;
  }

} /* ScreenBell */


void ScreenBackspace(SCREEN *pScr)
{
  RECT rc;

  pScr->bWrapPending = FALSE;
  rc.left = pScr->x * pScr->cxChar;
  rc.right = (pScr->x + 1) * pScr->cxChar;
  rc.top = pScr->cyChar * pScr->y;
  rc.bottom = pScr->cyChar * (pScr->y + 1);
  InvalidateRect(pScr->hWnd, &rc, TRUE);
  pScr->x--;
  if (pScr->x < 0)
    pScr->x = 0;
  UpdateWindow(pScr->hWnd);

} /* ScreenBackspace */


void ScreenTab(
	       SCREEN *pScr)
{
  int num_spaces;
  int idx;
  SCREENLINE *pScrLine;
  int iTest = 0;
  HDC hDC;

  num_spaces = TAB_SPACES - (pScr->x % TAB_SPACES);
  if (pScr->x + num_spaces >= pScr->width)
    num_spaces = pScr->width - pScr->x;
  pScrLine = GetScreenLineFromY(pScr, pScr->y);
  if (pScrLine == NULL)
    return;
  for (idx = 0; idx < num_spaces; idx++, pScr->x++) {
    if (!pScrLine->text[pScr->x])
      iTest=1;
    if (iTest)
      pScrLine->text[pScr->x] = SPACE;
  }
  hDC = GetDC(pScr->hWnd);
  assert(hDC != NULL);
  SelectObject(hDC, pScr->hSelectedFont);
  TextOut(hDC, (pScr->x - num_spaces) * pScr->cxChar, pScr->y * pScr->cyChar,
	  pScrLine->text + pScr->x - num_spaces, num_spaces);
  ReleaseDC(pScr->hWnd, hDC);
  if (pScr->x >= pScr->width)
    pScr->x = pScr->width - 1;
  pScr->bWrapPending = FALSE;

} /* ScreenTab */


void ScreenCarriageFeed(
			SCREEN *pScr)
{
  pScr->bWrapPending = FALSE;
  pScr->x = 0;

} /* ScreenCarriageFeed */
