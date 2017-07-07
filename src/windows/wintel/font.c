/* font.c */

#include <windows.h>
#include <commdlg.h>
#include <assert.h>
#include "screen.h"
#include "ini.h"

void ProcessFontChange(
		       HWND hWnd)
{
  static DWORD dwFontColor;  /* Color of font if one has been selected */
  CHOOSEFONT cf;
  HDC hDC;
  SCREEN *pScr;
  TEXTMETRIC tm;
  char buf[16];
  char szStyle[LF_FACESIZE];

  pScr = (SCREEN *) GetWindowLong(hWnd, SCREEN_HANDLE);
  assert(pScr != NULL);

  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = hWnd;
  cf.lpLogFont = (LPLOGFONT) &(pScr->lf);
  cf.lpszStyle = szStyle;
  cf.Flags = CF_INITTOLOGFONTSTRUCT; /* | CF_USESTYLE; */
  cf.Flags |= CF_SCREENFONTS;
#if 0
  cf.Flags |= CF_ANSIONLY;
#endif
  cf.Flags |= CF_FORCEFONTEXIST;
  cf.Flags |= CF_FIXEDPITCHONLY;
  cf.Flags |= CF_NOSIMULATIONS;

  if (ChooseFont(&cf)) {
    if (pScr->hSelectedFont)
      DeleteObject(pScr->hSelectedFont);

    pScr->hSelectedFont = CreateFontIndirect(&(pScr->lf));
    pScr->lf.lfUnderline = TRUE;
    pScr->hSelectedULFont = CreateFontIndirect(&(pScr->lf));
    pScr->lf.lfUnderline = FALSE;
    hDC = GetDC(hWnd);
    SelectObject(hDC, pScr->hSelectedFont);
    GetTextMetrics(hDC, &tm);
    pScr->cxChar = tm.tmAveCharWidth;
    pScr->cyChar = tm.tmHeight + tm.tmExternalLeading;
    ReleaseDC(hWnd, hDC);
    SetWindowPos(hWnd, NULL, 0, 0, pScr->cxChar * pScr->width +
		 FRAME_WIDTH, pScr->cyChar * pScr->height +
		 FRAME_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);

    dwFontColor = RGB(255, 255, 255);
    InvalidateRect(hWnd, NULL, TRUE);
  }

  WritePrivateProfileString(INI_FONT, "FaceName", pScr->lf.lfFaceName, TELNET_INI);
  wsprintf(buf, "%d", (int) pScr->lf.lfHeight);
  WritePrivateProfileString(INI_FONT, "Height", buf, TELNET_INI);
  wsprintf(buf, "%d", (int) pScr->lf.lfWidth);
  WritePrivateProfileString(INI_FONT, "Width", buf, TELNET_INI);
  wsprintf(buf, "%d", (int) pScr->lf.lfEscapement);
  WritePrivateProfileString(INI_FONT, "Escapement", buf, TELNET_INI);
  wsprintf(buf, "%d", (int) pScr->lf.lfCharSet);
  WritePrivateProfileString(INI_FONT, "CharSet", buf, TELNET_INI);
  wsprintf(buf, "%d", (int) pScr->lf.lfPitchAndFamily);
  WritePrivateProfileString(INI_FONT, "PitchAndFamily", buf, TELNET_INI);

  return;

} /* ProcessFontChange */


void InitializeStruct(
			   WORD wCommDlgType,
			   LPSTR lpStruct,
			   HWND hWnd)
{
  LPCHOOSEFONT lpFontChunk;

  if (wCommDlgType == IDC_FONT) {
    lpFontChunk = (LPCHOOSEFONT) lpStruct;

    lpFontChunk->lStructSize = sizeof(CHOOSEFONT);
    lpFontChunk->hwndOwner = hWnd;
    lpFontChunk->Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY
      | CF_INITTOLOGFONTSTRUCT | CF_APPLY;
    lpFontChunk->rgbColors = RGB(0, 0, 255);
    lpFontChunk->lCustData = 0L;
    lpFontChunk->lpfnHook = NULL;
    lpFontChunk->lpTemplateName = NULL;
    lpFontChunk->hInstance = NULL;
    lpFontChunk->lpszStyle = NULL;
    lpFontChunk->nFontType = SCREEN_FONTTYPE;
    lpFontChunk->nSizeMin = 0;
    lpFontChunk->nSizeMax = 0;
  }

} /* InitialiseStruct */
