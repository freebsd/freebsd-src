/* edit.c */

#include <windows.h>
#include <commdlg.h>
#include <ctype.h>
#include <assert.h>
#include "screen.h"

char *cInvertedArray;
int bMouseDown = FALSE;
int bSelection;

static int iLocStart;
static int iLocEnd;

void Edit_LbuttonDown(
		      HWND hWnd,
		      LPARAM lParam)
{
  SCREEN *pScr;
  HMENU hMenu;
  int iTmp;
  int iXlocStart;
  int iYlocStart;
  HDC hDC;

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  hDC = GetDC(hWnd);
  for (iTmp = 0; iTmp < pScr->width * pScr->height; iTmp++) {
    if (cInvertedArray[iTmp]) {
      PatBlt(hDC, iTmp % pScr->width * pScr->cxChar,
	     (int) (iTmp / pScr->width) * pScr->cyChar,
	     pScr->cxChar, pScr->cyChar, DSTINVERT);
      cInvertedArray[iTmp] = 0;
    }
  }
  bSelection = FALSE;
  hMenu = GetMenu(hWnd);
  EnableMenuItem(hMenu, IDM_COPY, MF_GRAYED);
  ReleaseDC(hWnd, hDC);
  iXlocStart = (int) LOWORD(lParam) / pScr->cxChar;
  if (iXlocStart >= pScr->width)
    iXlocStart = pScr->width - 1;
  iYlocStart = (int) HIWORD(lParam) / pScr->cyChar;
  if (iYlocStart >= pScr->height)
    iYlocStart = pScr->height - 1;
  iLocStart = iXlocStart + iYlocStart * pScr->width;
  bMouseDown = TRUE;

} /* Edit_LbuttonDown */


void Edit_LbuttonUp(
		    HWND hWnd,
		    LPARAM lParam)
{
  SCREEN *pScr;
  int iTmp;
  int iTmp2;
  HMENU hMenu;

  bMouseDown = FALSE;
  if (bSelection)
    return;
  bSelection = TRUE;

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  iTmp = (int) LOWORD(lParam) / pScr->cxChar;
  if (iTmp >= pScr->width)
    iTmp = pScr->width - 1;
  iTmp2 = (int) HIWORD(lParam) / pScr->cyChar;
  if (iTmp2 >= pScr->height)
    iTmp2 = pScr->height - 1;
  iLocEnd = iTmp + iTmp2 * pScr->width;
  if (iLocEnd == iLocStart) {
    bSelection = FALSE;
  }
  else {
    hMenu = GetMenu(hWnd);
    EnableMenuItem(hMenu, IDM_COPY, MF_ENABLED);
  }

} /* Edit_LbuttonUp */


void Edit_MouseMove(HWND hWnd, LPARAM lParam){
  SCREEN *pScr;
  int iTmp;
  int iTmp2;
  int iXlocCurr;
  int iYlocCurr;
  int iLocCurr;
  int iX;
  int iX2;
  int iY;
  int iY2;
  SCREENLINE *pScrLine;
  HDC hDC;

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  hDC = GetDC(hWnd);
  iXlocCurr = (int) LOWORD(lParam) / pScr->cxChar;
  if (iXlocCurr >= pScr->width)
    iXlocCurr = pScr->width - 1;
  iYlocCurr = (int) HIWORD(lParam) / pScr->cyChar;
  if (iYlocCurr >= pScr->height)
    iYlocCurr = pScr->height - 1;
  iLocCurr = iXlocCurr + (iYlocCurr * pScr->width);
  if (iLocCurr > iLocStart) {
    for (iTmp=0; iTmp < iLocStart; iTmp++) {
      if (cInvertedArray[iTmp]) {
	PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar,
	       (int) (iTmp / pScr->width) * pScr->cyChar,
	       pScr->cxChar, pScr->cyChar, DSTINVERT);
	cInvertedArray[iTmp] = 0;
      }
    }
    iX = iLocStart % pScr->width;
    iY = (int) (iLocStart / pScr->width);
    iX2 = iLocCurr % pScr->width;
    iY2 = (int) (iLocCurr / pScr->width);
    if (iY == iY2) {
      pScrLine = GetScreenLineFromY(pScr, iY);
      for (iTmp2 = iX; iTmp2 < iX2; iTmp2++) {
	if ((!cInvertedArray[iTmp2 + (pScr->width * iY)]) && pScrLine->text[iTmp2]) {
	  PatBlt(hDC, iTmp2 * pScr->cxChar, iY * pScr->cyChar,
		 pScr->cxChar, pScr->cyChar, DSTINVERT);
	  cInvertedArray[iTmp2 + (pScr->width * iY)] = pScrLine->text[iTmp2];
	}
      }
    }
    else {
      pScrLine = GetScreenLineFromY(pScr, iY);

      for (iTmp2 = iX; iTmp2 < pScr->width; iTmp2++) {
	if ((!cInvertedArray[iTmp2 + (pScr->width * iY)]) && pScrLine->text[iTmp2]) {
	  PatBlt(hDC, iTmp2 * pScr->cxChar, iY * pScr->cyChar,
		 pScr->cxChar, pScr->cyChar, DSTINVERT);
	  cInvertedArray[iTmp2 + (pScr->width * iY)] = pScrLine->text[iTmp2];
	}
      }

      for (iTmp = iY + 1; iTmp < iY2; iTmp++) {
	pScrLine = GetScreenLineFromY(pScr, iTmp);
	for (iTmp2 = 0; iTmp2 < pScr->width; iTmp2++) {
	  if ((!cInvertedArray[iTmp2 + (pScr->width * iTmp)]) && pScrLine->text[iTmp2]) {
	    PatBlt(hDC, iTmp2 * pScr->cxChar, iTmp * pScr->cyChar,
		   pScr->cxChar, pScr->cyChar, DSTINVERT);
	    cInvertedArray[iTmp2 + (pScr->width * iTmp)] = pScrLine->text[iTmp2];
	  }
	}
      }

      if (iY2 != iY) {
	pScrLine = GetScreenLineFromY(pScr, iY2);
	for (iTmp2 = 0; iTmp2 < iX2; iTmp2++) {
	  if ((!cInvertedArray[iTmp2 + (pScr->width * iY2)]) && pScrLine->text[iTmp2]) {
	    PatBlt(hDC, iTmp2 * pScr->cxChar, iY2 * pScr->cyChar,
		   pScr->cxChar, pScr->cyChar, DSTINVERT);
	    cInvertedArray[iTmp2 + (pScr->width * iY2)] = pScrLine->text[iTmp2];
	  }
	}
      }
    }

    for (iTmp = iLocCurr; iTmp < pScr->width * pScr->height; iTmp++) {
      if (cInvertedArray[iTmp]) {
	PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar, (int) (iTmp / pScr->width) * pScr->cyChar,
	       pScr->cxChar, pScr->cyChar, DSTINVERT);
	cInvertedArray[iTmp] = 0;
      }
    }
  }
  else { /* going backwards */
    for (iTmp = 0; iTmp < iLocCurr; iTmp++) {
      if (cInvertedArray[iTmp]) {
	PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar, (int) (iTmp / pScr->width) * pScr->cyChar,
	       pScr->cxChar, pScr->cyChar, DSTINVERT);
	cInvertedArray[iTmp] = 0;
      }
    }
    iX = iLocCurr % pScr->width;
    iY = (int) (iLocCurr / pScr->width);
    iX2 = (iLocStart % pScr->width);
    iY2 = (int) (iLocStart / pScr->width);
    if (iY == iY2) {
      pScrLine = GetScreenLineFromY(pScr, iY);
      for (iTmp2= iX; iTmp2 < iX2; iTmp2++) {
	if ((!cInvertedArray[iTmp2 + (pScr->width * iY)]) && pScrLine->text[iTmp2]) {
	  PatBlt(hDC, iTmp2 * pScr->cxChar, iY * pScr->cyChar,
		 pScr->cxChar, pScr->cyChar, DSTINVERT);
	  cInvertedArray[iTmp2 + (pScr->width * iY)] = pScrLine->text[iTmp2];
	}
      }
    }
    else {
      pScrLine = GetScreenLineFromY(pScr, iY);
      for (iTmp2 = iX; iTmp2 < pScr->width; iTmp2++) {
	if ((!cInvertedArray[iTmp2 + (pScr->width * iY)]) && pScrLine->text[iTmp2]) {
	  PatBlt(hDC, iTmp2 * pScr->cxChar, iY * pScr->cyChar,
		 pScr->cxChar, pScr->cyChar, DSTINVERT);
	  cInvertedArray[iTmp2 + (pScr->width * iY)] = pScrLine->text[iTmp2];
	}
      }
      for (iTmp = iY + 1; iTmp < iY2; iTmp++) {
	pScrLine = GetScreenLineFromY(pScr, iTmp);
	for (iTmp2 = 0; iTmp2 < pScr->width; iTmp2++) {
	  if ((!cInvertedArray[iTmp2 + (pScr->width * iTmp)]) && pScrLine->text[iTmp2]) {
	    PatBlt(hDC, iTmp2 * pScr->cxChar, iTmp * pScr->cyChar,
		   pScr->cxChar, pScr->cyChar, DSTINVERT);
	    cInvertedArray[iTmp2 + (pScr->width * iTmp)] = pScrLine->text[iTmp2];
	  }
	}
      }
      if (iY2 != iY) {
	pScrLine = GetScreenLineFromY(pScr, iY2);
	for (iTmp2 = 0; iTmp2 < iX2; iTmp2++) {
	  if ((!cInvertedArray[iTmp2 + (pScr->width * iY2)]) && pScrLine->text[iTmp2]) {
	    PatBlt(hDC, iTmp2 * pScr->cxChar, iY2 * pScr->cyChar,
		   pScr->cxChar, pScr->cyChar, DSTINVERT);
	    cInvertedArray[iTmp2 + (pScr->width * iY2)] = pScrLine->text[iTmp2];
	  }
	}
      }
    }
    for (iTmp = iLocStart; iTmp < pScr->width * pScr->height; iTmp++) {
      if (cInvertedArray[iTmp]) {
	PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar, (int) (iTmp / pScr->width) * pScr->cyChar,
	       pScr->cxChar, pScr->cyChar, DSTINVERT);
	cInvertedArray[iTmp] = 0;
      }
    }
  }
  ReleaseDC(hWnd, hDC);
} /* Edit_MouseMove */


void Edit_ClearSelection(
			 SCREEN *pScr)
{
  int iTmp;
  HDC hDC;
  HMENU hMenu;

  hDC = GetDC(pScr->hWnd);
  for (iTmp = 0; iTmp < pScr->width * pScr->height; iTmp++) {
    if (cInvertedArray[iTmp]) {
      PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar,
	     (int) (iTmp / pScr->width) * pScr->cyChar,
	     pScr->cxChar, pScr->cyChar, DSTINVERT);
      cInvertedArray[iTmp] = 0;
    }
  }
  bSelection = FALSE;
  hMenu=GetMenu(pScr->hWnd);
  EnableMenuItem(hMenu, IDM_COPY, MF_GRAYED);
  ReleaseDC(pScr->hWnd, hDC);
} /* Edit_ClearSelection */


void Edit_Copy(
	       HWND hWnd)
{
  int iTmp,iIdx;
  HGLOBAL hCutBuffer;
  LPSTR lpCutBuffer;
  SCREEN *pScr;

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  hCutBuffer= GlobalAlloc(GHND, (DWORD) (pScr->width * pScr->height + 1));
  lpCutBuffer= GlobalLock(hCutBuffer);

  if (iLocStart > iLocEnd) { /* swap variables */
    iTmp = iLocStart;
    iLocStart = iLocEnd;
    iLocEnd = iLocStart;
  }
  iTmp = iLocStart;
  iIdx = 0;
  while (iTmp < iLocEnd) {
    if (!cInvertedArray[iTmp]) {
      lpCutBuffer[iIdx++] = '\r';
      lpCutBuffer[iIdx++] = '\n';
      iTmp = (((int) (iTmp / pScr->width)) + 1) * pScr->width;
      continue;
    }
    lpCutBuffer[iIdx++] = cInvertedArray[iTmp++];
  }
  lpCutBuffer[iIdx] = 0;
  GlobalUnlock(hCutBuffer);
  OpenClipboard(hWnd);
  EmptyClipboard();
  SetClipboardData(CF_TEXT, hCutBuffer);
  CloseClipboard();

} /* Edit_Copy */


void Edit_Paste(
		HWND hWnd)
{
  HGLOBAL hClipMemory;
  static HGLOBAL hMyClipBuffer;
  LPSTR lpClipMemory;
  LPSTR lpMyClipBuffer;
  SCREEN *pScr;

  if (hMyClipBuffer)
    GlobalFree(hMyClipBuffer);
  OpenClipboard(hWnd);
  hClipMemory = GetClipboardData(CF_TEXT);
  hMyClipBuffer = GlobalAlloc(GHND, GlobalSize(hClipMemory));
  lpMyClipBuffer = GlobalLock(hMyClipBuffer);
  lpClipMemory= GlobalLock(hClipMemory);

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  lstrcpy(lpMyClipBuffer, lpClipMemory);
#if 0
  OutputDebugString(lpMyClipBuffer);
#endif
  PostMessage(pScr->hwndTel, WM_MYSCREENBLOCK, (WPARAM) hMyClipBuffer, (LPARAM) pScr);
  CloseClipboard();
  GlobalUnlock(hClipMemory);
  GlobalUnlock(hMyClipBuffer);

} /* Edit_Paste */


void Edit_LbuttonDblclk(
			HWND hWnd,
			LPARAM lParam)
{
  HDC hDC;
  SCREEN *pScr;
  int iTmp;
  int iTmp2;
  int iXlocStart;
  int iYloc;
  SCREENLINE *pScrLine;

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  hDC = GetDC(hWnd);
  for (iTmp = 0; iTmp < pScr->width * pScr->height; iTmp++) {
    if (cInvertedArray[iTmp]) {
      PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar,
	     (int) (iTmp / pScr->width) * pScr->cyChar,
	     pScr->cxChar, pScr->cyChar, DSTINVERT);
      cInvertedArray[iTmp] = 0;
    }
  }
  bSelection = FALSE;
  iXlocStart = (int) LOWORD(lParam) / pScr->cxChar;
  if (iXlocStart >= pScr->width)
    iXlocStart = pScr->width - 1;
  iYloc = (int) HIWORD(lParam) / pScr->cyChar;
  if (iYloc >= pScr->height)
    iYloc = pScr->height - 1;
  iLocStart = iXlocStart + (iYloc * pScr->width);

  pScrLine = GetScreenLineFromY(pScr, iYloc);

  iTmp = iXlocStart;
  while (isalnum((int) pScrLine->text[iTmp])) {
    PatBlt(hDC, iTmp * pScr->cxChar, iYloc * pScr->cyChar,
	   pScr->cxChar, pScr->cyChar, DSTINVERT);
    cInvertedArray[iTmp + (iYloc * pScr->width)] = pScrLine->text[iTmp];
    iTmp++;
  }
  iTmp2 = iXlocStart - 1;
  while (isalnum((int) pScrLine->text[iTmp2])) {
    PatBlt(hDC, iTmp2 * pScr->cxChar, iYloc * pScr->cyChar,
	   pScr->cxChar, pScr->cyChar, DSTINVERT);
    cInvertedArray[iTmp2 + (iYloc * pScr->width)] = pScrLine->text[iTmp2];
    iTmp2--;
  }
  iLocStart = (iTmp2 + 1) + (iYloc * pScr->width);
  iLocEnd = iTmp + (iYloc * pScr->width);

  bSelection = TRUE;
  ReleaseDC(hWnd, hDC);

} /* Edit_LbuttonDblclk */


void Edit_TripleClick(
		      HWND hWnd,
		      LPARAM lParam)
{
  HDC hDC;
  SCREEN *pScr;
  int iTmp;
  int iYloc;
  SCREENLINE *pScrLine;

#if 0
  OutputDebugString("Triple Click \r\n");
#endif
  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  hDC = GetDC(hWnd);
  for (iTmp = 0; iTmp < pScr->width * pScr->height; iTmp++) {
    if (cInvertedArray[iTmp]) {
      PatBlt(hDC, (iTmp % pScr->width) * pScr->cxChar,
	     (int) (iTmp / pScr->width) * pScr->cyChar,
	     pScr->cxChar, pScr->cyChar, DSTINVERT);
      cInvertedArray[iTmp] = 0;
    }
  }
  bSelection = FALSE;
  iYloc = (int) HIWORD(lParam) / pScr->cyChar;
  if (iYloc >= pScr->height)
    iYloc = pScr->height - 1;
  iLocStart = iYloc * pScr->width;

  pScrLine = GetScreenLineFromY(pScr, iYloc);

  for (iTmp = 0; iTmp < pScr->width; iTmp++) {
    if (pScrLine->text[iTmp]) {
      PatBlt(hDC, iTmp * pScr->cxChar, iYloc * pScr->cyChar,
	     pScr->cxChar, pScr->cyChar, DSTINVERT);
      cInvertedArray[iTmp + (iYloc * pScr->width)] = pScrLine->text[iTmp];
    }
    else
      break;
  }
  iLocEnd = iTmp + (iYloc * pScr->width);

  bSelection = TRUE;
  ReleaseDC(hWnd, hDC);

} /* Edit_TripleClick */
