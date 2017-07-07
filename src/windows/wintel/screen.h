extern long PASCAL ScreenWndProc(HWND,UINT,WPARAM,LPARAM);

/*
*          Definition of attribute bits in the Virtual Screen
*
*          0   -   Bold
*          1   -
*          2   -
*          3   -   Underline
*          4   -   Blink
*          5   -
*          6   -   Reverse
*          7   -   Graphics character set
*
*/
#define SCR_isbold(x)   (x & 0x01)
#define SCR_isundl(x)   (x & 0x08)
#define SCR_isblnk(x)   (x & 0x10)
#define SCR_isrev(x)    (x & 0x40)
#define SCR_setrev(x)   (x ^= 0x40)
#define SCR_isgrph(x)   (x & 0x80)
#define SCR_inattr(x)   (x & 0xd9)
#define SCR_graph(x)    (x | 0x80)
#define SCR_notgraph(x) (x & 0x7F)

#define SCREEN_HANDLE            0    /* offset in extra window info */

#define WM_MYSCREENCHAR			(WM_USER+1)
#define WM_MYSCREENBLOCK		(WM_USER+2)
#define WM_MYSYSCHAR 			(WM_USER+3)
#define WM_MYSCREENCLOSE		(WM_USER+4)
#define WM_MYSCREENCHANGEBKSP	(WM_USER+5)
#define WM_MYSCREENSIZE			(WM_USER+6)
#define WM_NETWORKEVENT			(WM_USER+7)
#define WM_HOSTNAMEFOUND		(WM_USER+8)
#define WM_MYCURSORKEY			(WM_USER+9)

#define FRAME_HEIGHT ((2* GetSystemMetrics(SM_CYFRAME))+GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYMENU)+3)
#define FRAME_WIDTH  (2*GetSystemMetrics(SM_CXFRAME)+GetSystemMetrics(SM_CXVSCROLL))
#define TAB_SPACES 8
#define SPACE 32
#define ALERT 0x21
#define MAX_LINE_WIDTH 512 /* not restricted to 1 byte */

typedef struct SCREENLINE {
	struct SCREENLINE *next;
	struct SCREENLINE *prev;
	int width;
	char *text;
	char *attrib;
	char buffer[0];
} SCREENLINE;

typedef struct SCREEN {
	LPSTR title;
	HWND hWnd;
	HWND hwndTel;
	SCREENLINE *screen_top;
	SCREENLINE *screen_bottom;
	SCREENLINE *buffer_top;
	SCREENLINE *buffer_bottom;
	int ID;
	int type;
	int width;
	int height;
	int maxlines;       /* Maximum number of scrollback lines */
	int numlines;       /* Current number of scrollback lines */
	int savelines;      /* Save lines off top? */
	int ESscroll;       /* Scroll screen when ES received */
	int attrib;         /* current attribute */
	int x;              /* current cursor position */
	int y;              /* current cursor position */
	int Oldx;           /* internally used to redraw cursor */
	int Oldy;
	int Px;             /* saved cursor pos and attribute */
	int Py;
	int Pattrib;
	int VSIDC;          /* Insert/Delete character mode 0=draw line */
	int DECAWM;         /* AutoWrap mode 0=off */
	BOOL bWrapPending;  /* AutoWrap mode is on - wrap on next character */
	int DECCKM;         /* Cursor key mode */
	int DECPAM;         /* keyPad Application mode */
	int IRM;            /* Insert/Replace mode */
	int escflg;         /* Current Escape level */
	int top;            /* Vertical bounds of screen */
	int bottom;
	int parmptr;
	int cxChar;         /* Width of the current font */
	int cyChar;         /* Height of the current font */
	BOOL bAlert;
	int parms[6];       /* Ansi Params */
	LOGFONT lf;
	HFONT hSelectedFont;
	HFONT hSelectedULFont;
	char tabs[MAX_LINE_WIDTH];
	struct SCREEN *next;
	struct SCREEN *prev;
} SCREEN;

typedef struct CONFIG {
	LPSTR title;
	HWND hwndTel;
	int ID;
	int type;
	int height;
	int width;
	int maxlines;       /* Maximum number of scrollback lines */
	int backspace;
	int ESscroll;       /* Scroll screen when ES received */
	int VSIDC;          /* Insert/Delete character mode 0=draw line */
	int DECAWM;         /* AutoWrap mode 0=off */
	int IRM;            /* Insert/Replace mode */
} CONFIG;

#define TELNET_SCREEN   0
#define CONSOLE_SCREEN  1

#define IDM_FONT        100
#define IDM_BACKSPACE   101
#define IDM_DELETE      102
#define IDM_ABOUT       103
#define IDM_HELP_INDEX  104
#define IDM_EXIT        105

#define HELP_FILE "ktelnet.hlp"

#define IDM_COPY        200
#define IDM_PASTE       201
#define IDM_DEBUG       202

#define TIMER_TRIPLECLICK 1000

#define IDC_ALLOCFAIL           1
#define IDC_LOCKFAIL            2
#define IDC_LOADSTRINGFAIL      3
#define IDC_FONT                6

#define DESIREDPOINTSIZE 12

/*
Prototypes
*/
	void NEAR InitializeStruct(
		WORD wCommDlgType,
		LPSTR lpStruct,
		HWND hWnd);

	void ScreenInit(
		HINSTANCE hInstance);

	void SetScreenInstance(
		HINSTANCE hInstance);

	SCREENLINE *ScreenNewLine();

	void ScreenBell(
		SCREEN *pScr);

	void ScreenBackspace(
		SCREEN *pScr);

	void ScreenTab(
		SCREEN *pScr);

	void ScreenCarriageFeed(
		SCREEN *pScr);

	int ScreenScroll(
		SCREEN *pScr);

	void DeleteTopLine(
		SCREEN *pScr);

/*
emul.c
*/
	void ScreenEm(
		LPSTR c,
		int len,
		SCREEN *pScr);

/*
intern.c
*/
	SCREENLINE *GetScreenLineFromY(
		SCREEN *pScr,
		int y);

	SCREENLINE *ScreenClearLine(
		SCREEN *pScr,
		SCREENLINE *pScrLine);

	void ScreenUnscroll(
		SCREEN *pScr);

	void ScreenELO(
		SCREEN *pScr,
		int s);

	void ScreenEraseScreen(
		SCREEN *pScr);

	void ScreenTabClear(
		SCREEN *pScr);

	void ScreenTabInit(
		SCREEN *pScr);

	void ScreenReset(
		SCREEN *pScr);

	void ScreenIndex(
		SCREEN *pScr);

	void ScreenWrapNow(
		SCREEN *pScr,
		int *xp,
		int *yp);

	void ScreenEraseToEOL(
		SCREEN *pScr);

	void ScreenEraseToBOL(
		SCREEN *pScr);

	void ScreenEraseLine(
		SCREEN *pScr,
		int s);

	void ScreenEraseToEndOfScreen(
		SCREEN *pScr);

	void ScreenRange(
		SCREEN *pScr);

	void ScreenAlign(
		SCREEN *pScr);

	void ScreenApClear(
		SCREEN *pScr);

	void ScreenSetOption(
		SCREEN *pScr,
		int toggle);

	BOOL ScreenInsChar(
		SCREEN *pScr,
		int x);

	void ScreenSaveCursor(
		SCREEN *pScr);

	void ScreenRestoreCursor(
		SCREEN *pScr);

	void ScreenDraw(
		SCREEN *pScr,
		int x,
		int y,
		int a,
		int len,
		char *c);

	void ScreenCursorOff(
		SCREEN *pScr);

	void ScreenCursorOn(
		SCREEN *pScr);

	void ScreenDelChars(
		SCREEN *pScr,
		int n);

	void ScreenRevIndex(
		SCREEN *pScr);

	void ScreenDelLines(
		SCREEN *pScr,
		int n,
		int s);

	void ScreenInsLines(
		SCREEN *pScr,
		int n,
		int s);

	#if ! defined(NDEBUG)
		BOOL CheckScreen(
			SCREEN *pScr);
	#endif

	void ProcessFontChange(
		HWND hWnd);

	void Edit_LbuttonDown(
		HWND hWnd,
		LPARAM lParam);

	void Edit_LbuttonDblclk(
		HWND hWnd,
		LPARAM lParam);

	void Edit_LbuttonUp(
		HWND hWnd,
		LPARAM lParam);

	void Edit_TripleClick(
		HWND hWnd,
		LPARAM lParam);

	void Edit_MouseMove(
		HWND hWnd,
		LPARAM lParam);

	void Edit_ClearSelection(
		SCREEN *pScr);

	void Edit_Copy(
		HWND hWnd);

	void Edit_Paste(
		HWND hWnd);

	SCREEN *InitNewScreen(
		CONFIG *Config);
