/****************************************************************************

  Program: telnet.c

  PURPOSE: Windows networking kernel - Telnet

  FUNCTIONS:

  WinMain() - calls initialization function, processes message loop
  InitApplication() - initializes window data and registers window
  InitInstance() - saves instance handle and creates main window
  MainWndProc() - processes messages
  About() - processes messages for "About" dialog box

  COMMENTS:

  Windows can have several copies of your application running at the
  same time.  The variable hInst keeps track of which instance this
  application is so that processing will be to the correct window.

  ****************************************************************************/

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "telnet.h"
#include "auth.h"

static HANDLE hInst;
static HWND hWnd;
static CONFIG *tmpConfig;
static CONNECTION *con = NULL;
static char hostdata[MAXGETHOSTSTRUCT];
static SCREEN *pScr;
static int debug = 1;

char strTmp[1024];			/* Scratch buffer */
BOOL bAutoConnection = FALSE;
short port_no = 23;
char szUserName[64];			/* Used in auth.c */
char szHostName[64];

#ifdef KRB4
#define WINDOW_CLASS   "K4_telnetWClass"
#endif

#ifdef KRB5
krb5_context k5_context;
#define WINDOW_CLASS   "K5_telnetWClass"
#endif

/*
 *
 * FUNCTION: WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
 *
 * PURPOSE: calls initialization function, processes message loop
 *
 * COMMENTS:
 *
 * Windows recognizes this function by name as the initial entry point
 * for the program.  This function calls the application initialization
 * routine, if no other instance of the program is running, and always
 * calls the instance initialization routine.  It then executes a message
 * retrieval and dispatch loop that is the top-level control structure
 * for the remainder of execution.  The loop is terminated when a WM_QUIT
 * message is received, at which time this function exits the application
 * instance by returning the value passed by PostQuitMessage().
 *
 * If this function must abort before entering the message loop, it
 * returns the conventional value NULL.
 */

int PASCAL
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  MSG msg;

  if (!hPrevInstance)
    if (!InitApplication(hInstance))
      return(FALSE);

  /*
   * Perform initializations that apply to a specific instance
   */
  bAutoConnection = parse_cmdline(lpCmdLine);

  if (!InitInstance(hInstance, nCmdShow))
    return(FALSE);

#ifdef _WIN32
  SetDebugErrorLevel(SLE_WARNING);
#endif

  /*
   * Acquire and dispatch messages until a WM_QUIT message is received.
   */
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    /* Process all non-network messages */
    while (PeekMessage(&msg, NULL, 0, WM_NETWORKEVENT-1, PM_REMOVE) ||
	   PeekMessage(&msg, NULL, WM_NETWORKEVENT+1, (UINT)-1, PM_REMOVE))
    {
	if (msg.message == WM_QUIT)	// Special case: WM_QUIT -- return
	    return msg.wParam;		//   the value from PostQuitMessage

	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }
  }

  return (msg.wParam);		/* Returns the value from PostQuitMessage */
}

/*
 * FUNCTION: InitApplication(HINSTANCE)
 *
 * PURPOSE: Initializes window data and registers window class
 *
 * COMMENTS:
 *
 * This function is called at initialization time only if no other
 * instances of the application are running.  This function performs
 * initialization tasks that can be done once for any number of running
 * instances.
 *
 * In this case, we initialize a window class by filling out a data
 * structure of type WNDCLASS and calling the Windows RegisterClass()
 * function.  Since all instances of this application use the same window
 * class, we only need to do this when the first instance is initialized.
 */

BOOL
InitApplication(HINSTANCE hInstance)
{
  WNDCLASS  wc;

  ScreenInit(hInstance);

  /*
   * Fill in window class structure with parameters that describe the
   * main window.
   */
  wc.style = CS_HREDRAW | CS_VREDRAW; /* Class style(s). */
  wc.lpfnWndProc = MainWndProc;       /* Function to retrieve messages for
				       * windows of this class.
				       */
  wc.cbClsExtra = 0;                  /* No per-class extra data. */
  wc.cbWndExtra = 0;                  /* No per-window extra data. */
  wc.hInstance = hInstance;           /* Application that owns the class. */
  wc.hIcon = NULL;		      /* LoadIcon(hInstance, "NCSA"); */
  wc.hCursor = NULL;		      /* Cursor(NULL, IDC_ARROW); */
  wc.hbrBackground = NULL;	      /* GetStockObject(WHITE_BRUSH); */
  wc.lpszMenuName =  NULL; 	      /* Name of menu resource in .RC file. */
  wc.lpszClassName = WINDOW_CLASS;    /* Name used in call to CreateWindow. */

  return(RegisterClass(&wc));
}


/*
 * FUNCTION:  InitInstance(HANDLE, int)
 *
 * PURPOSE:  Saves instance handle and creates main window
 *
 * COMMENTS:
 *
 * This function is called at initialization time for every instance of
 * this application.  This function performs initialization tasks that
 * cannot be shared by multiple instances.
 *
 * In this case, we save the instance handle in a static variable and
 * create and display the main program window.
 */
BOOL
InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  int xScreen = 0;
  int yScreen = 0;
  WSADATA wsaData;

  SetScreenInstance(hInstance);

  /*
   * Save the instance handle in static variable, which will be used in
   * many subsequence calls from this application to Windows.
   */
  hInst = hInstance;

  /*
   * Create a main window for this application instance.
   */
  hWnd = CreateWindow(
		      WINDOW_CLASS,	/* See RegisterClass() call. */
		      "TCPWin",		/* Text for window title bar. */
		      WS_SYSMENU,	/* Window style. */
		      xScreen / 3,	/* Default horizontal position. */
		      yScreen / 3,	/* Default vertical position. */
		      xScreen / 3,	/* Default width. */
		      yScreen / 3,	/* Default height. */
		      NULL,		/* Overlapped windows have no parent */
		      NULL,		/* Use the window class menu. */
		      hInstance,	/* This instance owns this window. */
		      NULL);		/* Pointer not needed. */

  if (!hWnd)
    return (FALSE);

  if (WSAStartup(0x0101, &wsaData) != 0) {   /* Initialize the network */
    MessageBox(NULL, "Couldn't initialize Winsock!", NULL,
	       MB_OK | MB_ICONEXCLAMATION);
    return(FALSE);
  }

  if (!OpenTelnetConnection()) {
    WSACleanup();
    return(FALSE);
  }

#ifdef KRB5
  krb5_init_context(&k5_context);
#endif

  return (TRUE);
}

char buf[2048];

/*
 * FUNCTION: MainWndProc(HWND, UINT, WPARAM, LPARAM)
 *
 * PURPOSE:  Processes messages
 *
 * MESSAGES:
 *
 * WM_COMMAND    - application menu (About dialog box)
 * WM_DESTROY    - destroy window
 */
LRESULT CALLBACK
MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  HGLOBAL hBuffer;
  LPSTR lpBuffer;
  int iEvent, cnt, ret;
  char *tmpCommaLoc;
  struct sockaddr_in remote_addr;
  struct hostent *remote_host;

  switch (message) {
  case WM_MYSCREENCHANGEBKSP:
    if (!con)
      break;
    con->backspace = wParam;
    if (con->backspace == VK_BACK) {
      con->ctrl_backspace = 0x7f;
      WritePrivateProfileString(INI_TELNET, INI_BACKSPACE,
				INI_BACKSPACE_BS, TELNET_INI);
    }
    else {
      con->ctrl_backspace = VK_BACK;
      WritePrivateProfileString(INI_TELNET, INI_BACKSPACE,
				INI_BACKSPACE_DEL, TELNET_INI);
    }
    GetPrivateProfileString(INI_HOSTS, INI_HOST "0", "", buf, 128, TELNET_INI);
    tmpCommaLoc = strchr(buf, ',');
    if (tmpCommaLoc == NULL) {
      strcat (buf, ",");
      tmpCommaLoc = strchr(buf, ',');
    }
    if (tmpCommaLoc) {
      tmpCommaLoc++;
      if (con->backspace == VK_BACK)
	strcpy(tmpCommaLoc, INI_HOST_BS);
      else
	strcpy(tmpCommaLoc, INI_HOST_DEL);
    }
    WritePrivateProfileString(INI_HOSTS, INI_HOST "0", buf, TELNET_INI);
    break;

  case WM_MYSCREENCHAR:
    {
      unsigned char c;

      if (!con)
	break;
      if (wParam == VK_BACK)
	c = con->backspace;
      else if (wParam == 0x7f)
	c = con->ctrl_backspace;
      else if (wParam == VK_SPACE && GetKeyState(VK_CONTROL) < 0)
	c = 0;
      else
	c = wParam;
      TelnetSend(con->ks, &c, 1, 0);
    }
  break;

  case WM_MYCURSORKEY:
    /* Acts as a send through: buffer is lParam and length in wParam */
    if (!con)
      break;
    memcpy(buf, (char *)lParam, wParam);
    TelnetSend (con->ks, buf, wParam, 0);
    break;

  case WM_MYSCREENBLOCK:
    if (!con)
      break;
    hBuffer = (HGLOBAL) wParam;
    lpBuffer = GlobalLock(hBuffer);
    TelnetSend(con->ks, lpBuffer, lstrlen(lpBuffer), 0);
    GlobalUnlock(hBuffer);
    break;

  case WM_MYSCREENCLOSE:
#if 0
    if (con)
    {
	kstream_destroy(con->ks);
	con->ks = NULL;
    }
#endif
    DestroyWindow(hWnd);
    break;

  case WM_QUERYOPEN:
    return(0);
    break;

  case WM_DESTROY:          /* message: window being destroyed */
    if (con)
    {
	kstream_destroy(con->ks);
	free(con);
	WSACleanup();
    }
    PostQuitMessage(0);
    break;

  case WM_NETWORKEVENT:
    iEvent = WSAGETSELECTEVENT(lParam);

    switch (iEvent) {

    case FD_READ:
      if (con == NULL)
	break;
      cnt = kstream_read(con->ks, buf, 1500);
      buf[cnt] = 0;
      parse((CONNECTION *)con, (unsigned char *)buf, cnt);
      ScreenEm(buf, cnt, con->pScreen);
      break;

    case FD_CLOSE:
      kstream_destroy(con->ks);
      free(con);
      con = NULL;
      WSACleanup();
      PostQuitMessage(0);
      break;

    case FD_CONNECT:
      ret = WSAGETSELECTERROR(lParam);
      if (ret) {
	wsprintf(buf, "Error %d on Connect", ret);
	MessageBox(NULL, buf, NULL, MB_OK | MB_ICONEXCLAMATION);
	kstream_destroy(con->ks);
	free(con);
	WSACleanup();
	PostQuitMessage(0);
	break;
      }
      start_negotiation(con->ks);
      break;
    }

    break;

  case WM_HOSTNAMEFOUND:
    ret = WSAGETASYNCERROR(lParam);
    if (ret) {
      wsprintf(buf, "Error %d on GetHostbyName", ret);
      MessageBox(NULL, buf, NULL, MB_OK | MB_ICONEXCLAMATION);
      kstream_destroy(con->ks);
      free(con);
      WSACleanup();
      PostQuitMessage(0);
      break;
    }

    remote_host = (struct hostent *)hostdata;
    remote_addr.sin_family = AF_INET;
    memcpy(&(remote_addr.sin_addr), &(remote_host->h_addr[0]), 4);
    remote_addr.sin_port = htons(port_no);

    connect(con->socket, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr));
    break;

  case WM_MYSCREENSIZE:
    con->width = LOWORD(lParam);		/* width in characters */
    con->height = HIWORD(lParam);		/* height in characters */
    if (con->bResizeable && con->ks)
      send_naws(con);
    wsprintf(buf, "%d", con->height);
    WritePrivateProfileString(INI_TELNET, INI_HEIGHT, buf, TELNET_INI);
    wsprintf(buf, "%d", con->width);
    WritePrivateProfileString(INI_TELNET, INI_WIDTH, buf, TELNET_INI);
    break;

  default:              		/* Passes it on if unproccessed */
    return(DefWindowProc(hWnd, message, wParam, lParam));
  }
  return (0);
}


/*
 *
 * FUNCTION: SaveHostName(hostname, port)
 *
 * PURPOSE: Saves the currently selected host name and port number
 * in the KERBEROS.INI file and returns the preferred backspace
 * setting if one exists for that host.
 *
 * RETURNS: VK_BACK or 0x7f depending on the desired backspace setting.
 */
int
SaveHostName(char *host, int port)
{
  char buf[128];		/* Scratch buffer */
  char fullhost[128];		/* Host & port combination */
  char hostName[10][128];	/* Entries from INI files */
  char *comma;			/* For parsing del/bs info */
  int len;			/* Length of fullhost */
  int n;			/* Number of items written */
  int i;			/* Index */
  int bs;			/* What we return */

  if (port == 23)				/* Default telnet port */
    strcpy(fullhost, host);			/* ...then don't add it on */
  else
    wsprintf(fullhost, "%s %d", host, port);
  len = strlen(fullhost);

  comma = NULL;
  for (i = 0; i < 10; i++) {
    wsprintf(buf, INI_HOST "%d", i); /* INI item to fetch */
    GetPrivateProfileString(INI_HOSTS, buf, "", hostName[i],
			    128, TELNET_INI);

    if (!hostName[i][0])
      break;

    if (strncmp (hostName[i], fullhost, len))	/* A match?? */
      continue;					/* Nope, keep going */
    comma = strchr (hostName[i], ',');
  }

  if (comma) {
    ++comma;					/* Past the comma */
    while (*comma == ' ')			/* Past leading white space */
      ++comma;
    bs = VK_BACK;				/* Default for unknown entry */
    if (_stricmp(comma, INI_HOST_DEL) == 0)
      bs = 0x7f;
  }
  else {					/* No matching entry */
    GetPrivateProfileString(INI_TELNET, INI_BACKSPACE, INI_BACKSPACE_BS,
			    buf, sizeof(buf), TELNET_INI);
    bs = VK_BACK;				/* Default value */
    if (_stricmp(buf, INI_BACKSPACE_DEL) == 0)
      bs = 0x7f;
  }

  /*
   * Build up default host name
   */
  strcpy(buf, fullhost);
  strcat(buf, ", ");
  strcat(buf, (bs == VK_BACK) ? INI_BACKSPACE_BS : INI_BACKSPACE_DEL);
  WritePrivateProfileString(INI_HOSTS, INI_HOST "0", buf, TELNET_INI);

  n = 0;
  for (i = 0; i < 10; i++) {
    if (!hostName[i][0])			/* End of the list? */
      break;
    if (strncmp(hostName[i], fullhost, len) != 0) {
      wsprintf(buf, INI_HOST "%d", ++n);
      WritePrivateProfileString(INI_HOSTS, buf, hostName[i], TELNET_INI);
    }
  }
  return(bs);
}


int
OpenTelnetConnection(void)
{
  int nReturn, ret;
  struct sockaddr_in sockaddr;
  char *p;
  static struct kstream_crypt_ctl_block ctl;
  char buf[128];

  tmpConfig = calloc(sizeof(CONFIG), 1);

  if (bAutoConnection) {
    tmpConfig->title = calloc(lstrlen(szHostName), 1);
    lstrcpy(tmpConfig->title, (char *) szHostName);
  } else {
    nReturn = DoDialog("OPENTELNETDLG", OpenTelnetDlg);
    if (nReturn == FALSE)
      return(FALSE);
  }

  con = (CONNECTION *) GetNewConnection();
  if (con == NULL)
    return(0);

  tmpConfig->width =
    GetPrivateProfileInt(INI_TELNET, INI_WIDTH, DEF_WIDTH, TELNET_INI);

  tmpConfig->height =
    GetPrivateProfileInt(INI_TELNET, INI_HEIGHT, DEF_HEIGHT, TELNET_INI);
  con->width = tmpConfig->width;
  con->height = tmpConfig->height;

  con->backspace = SaveHostName(tmpConfig->title, port_no);

  if (con->backspace == VK_BACK) {
    tmpConfig->backspace = TRUE;
    con->ctrl_backspace = 0x7f;
  } else {
    tmpConfig->backspace = FALSE;
    con->ctrl_backspace = 0x08;
  }

  tmpConfig->hwndTel = hWnd;
  con->pScreen = InitNewScreen(tmpConfig);
  if (!con->pScreen) {
    assert(FALSE);
    free(con->pScreen);
    free(con);
    free(tmpConfig);
    return(-1);
  }

  ret = (SOCKET) socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (ret == SOCKET_ERROR) {
    wsprintf(buf, "Socket error on socket = %d!", WSAGetLastError());
    MessageBox(NULL, buf, NULL, MB_OK | MB_ICONEXCLAMATION);
    if (con->pScreen != NULL)
      DestroyWindow(con->pScreen->hWnd);
    free(con);
    free(tmpConfig);
    return(-1);
  }

  con->socket = ret;

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sockaddr.sin_port = htons(0);

  ret = bind(con->socket, (struct sockaddr *) &sockaddr,
	     (int) sizeof(struct sockaddr_in));

  if (ret == SOCKET_ERROR) {
    wsprintf(buf, "Socket error on bind!");
    MessageBox(NULL, buf, NULL, MB_OK | MB_ICONEXCLAMATION);
    if (con->pScreen != NULL)
      DestroyWindow(con->pScreen->hWnd);
    free(con);
    free(tmpConfig);
    return(-1);
  }

  WSAAsyncSelect(con->socket, hWnd, WM_NETWORKEVENT,
		 FD_READ | FD_CLOSE | FD_CONNECT);

  lstrcpy(szHostName, tmpConfig->title);
  p = strchr(szHostName, '@');
  if (p != NULL) {
    *p = 0;
    strcpy (szUserName, szHostName);
    strcpy(szHostName, ++p);
  }

  WSAAsyncGetHostByName(hWnd, WM_HOSTNAMEFOUND, szHostName, hostdata,
			MAXGETHOSTSTRUCT);

  ctl.encrypt = auth_encrypt;
  ctl.decrypt = auth_decrypt;
  ctl.init = auth_init;
  ctl.destroy = auth_destroy;

  con->ks = kstream_create_from_fd(con->socket, &ctl, NULL);

  if (con->ks == NULL)
    return(-1);

  kstream_set_buffer_mode(con->ks, 0);

  return(1);
}


CONNECTION *
GetNewConnection(void)
{
  CONNECTION *pCon;

  pCon = calloc(sizeof(CONNECTION), 1);
  if (pCon == NULL)
    return NULL;
  pCon->backspace = TRUE;
  pCon->bResizeable = TRUE;
  return(pCon);
}


int
DoDialog(char *szDialog, DLGPROC lpfnDlgProc)
{
  int nReturn;

  nReturn = DialogBox(hInst, szDialog, hWnd, lpfnDlgProc);
  return (nReturn);
}


/*
 * FUNCTION: OpenTelnetDlg(HWND, unsigned, WORD, LONG)
 *
 * PURPOSE:  Processes messages for "Open New Telnet Connection" dialog box
 *
 * MESSAGES:
 *
 * WM_INITDIALOG - initialize dialog box
 * WM_COMMAND    - Input received
 */
INT_PTR CALLBACK
OpenTelnetDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  char szConnectName[256];
  HDC hDC;
  int xExt, yExt;
  DWORD Ext;
  HWND hEdit;
  int n;
  int iHostNum = 0;
  char tmpName[128];
  char tmpBuf[80];
  char *tmpCommaLoc;

  switch (message) {
  case WM_INITDIALOG:
    hDC = GetDC(hDlg);
    Ext = GetDialogBaseUnits();
    xExt = (190 *LOWORD(Ext)) /4 ;
    yExt = (72 * HIWORD(Ext)) /8 ;
    GetPrivateProfileString(INI_HOSTS, INI_HOST "0", "", tmpName,
			    128, TELNET_INI);
    if (tmpName[0]) {
      tmpCommaLoc = strchr(tmpName, ',');
      if (tmpCommaLoc)
	*tmpCommaLoc = '\0';
      SetDlgItemText(hDlg, TEL_CONNECT_NAME, tmpName);
    }
    hEdit = GetWindow(GetDlgItem(hDlg, TEL_CONNECT_NAME), GW_CHILD);
    while (TRUE) {
      wsprintf(tmpBuf, INI_HOST "%d", iHostNum++);
      GetPrivateProfileString(INI_HOSTS, tmpBuf, "", tmpName,
			      128, TELNET_INI);
      tmpCommaLoc = strchr(tmpName, ',');
      if (tmpCommaLoc)
	*tmpCommaLoc = '\0';
      if (tmpName[0])
	SendDlgItemMessage(hDlg, TEL_CONNECT_NAME, CB_ADDSTRING, 0,
			   (LPARAM) ((LPSTR) tmpName));
      else
	break;
    }
#ifdef FORWARD
    EnableWindow(GetDlgItem(hDlg, IDC_FORWARD), 1);
    SendDlgItemMessage(hDlg, IDC_FORWARD, BM_SETCHECK, forward_flag, 0);
    if (forward_flag)
      EnableWindow(GetDlgItem(hDlg, IDC_FORWARDFORWARD), 1);
    else
      EnableWindow(GetDlgItem(hDlg, IDC_FORWARDFORWARD), 0);
    SendDlgItemMessage(hDlg, IDC_FORWARDFORWARD, BM_SETCHECK,
		       forwardable_flag, 0);
#endif

#ifdef ENCRYPTION
    EnableWindow(GetDlgItem(hDlg, IDC_ENCRYPT), 1);
    SendDlgItemMessage(hDlg, IDC_ENCRYPT,
		       BM_SETCHECK, encrypt_flag, 0);
#endif

    EnableWindow(GetDlgItem(hDlg, TEL_CONNECT_USERID), 1);

    SetWindowPos(hDlg, NULL,
		 (GetSystemMetrics(SM_CXSCREEN)/2)-(xExt/2),
		 (GetSystemMetrics(SM_CYSCREEN)/2)-(yExt/2),
		 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    ReleaseDC(hDlg, hDC);
    SendMessage(hEdit, WM_USER + 1, 0, 0);
    SendMessage(hDlg, WM_SETFOCUS, 0, 0);
    return (TRUE);

  case WM_COMMAND:
    switch (wParam) {
    case TEL_CANCEL:
    case IDCANCEL:			/* From the menu */
      EndDialog(hDlg, FALSE);
      break;

#ifdef FORWARD
    case IDC_FORWARD:
      forward_flag = (BOOL)SendDlgItemMessage(hDlg, IDC_FORWARD,
					      BM_GETCHECK, 0, 0);
      if (forward_flag)
	EnableWindow(GetDlgItem(hDlg, IDC_FORWARDFORWARD), 1);
      else
	EnableWindow(GetDlgItem(hDlg, IDC_FORWARDFORWARD), 0);
      break;

    case IDC_FORWARDFORWARD:
      forwardable_flag = (BOOL)SendDlgItemMessage(hDlg, IDC_FORWARDFORWARD,
						  BM_GETCHECK, 0, 0);
      break;
#endif

#if ENCRYPTION
    case IDC_ENCRYPT:
      encrypt_flag = (BOOL)SendDlgItemMessage(hDlg, IDC_ENCRYPT,
					      BM_GETCHECK, 0, 0);
      break;
#endif
    case TEL_CONNECT_USERID:
      GetDlgItemText(hDlg, TEL_CONNECT_USERID, szUserName, sizeof(szUserName));
      break;

    case TEL_OK:
      GetDlgItemText(hDlg, TEL_CONNECT_NAME, szConnectName, 256);

      n = parse_cmdline (szConnectName);
      if (! n) {
	MessageBox(hDlg, "You must enter a session name!",
		   NULL, MB_OK);
	break;
      }
      tmpConfig->title = calloc(lstrlen(szHostName) + 1, 1);
      lstrcpy(tmpConfig->title, szConnectName);
      EndDialog(hDlg, TRUE);
      break;
    }
    return (FALSE);
  }
  return(FALSE);
}


/*
 *
 * FUNCTION: TelnetSend(kstream ks, char *buf, int len, int flags)
 *
 * PURPOSE:	This is a replacement for the WinSock send() function, to
 *		send a buffer of characters to an output socket.  It differs
 *		by retrying endlessly if sending the bytes would cause
 *		the send() to block.  <gnu@cygnus.com> observed EWOULDBLOCK
 *		errors when running using TCP Software's PC/TCP 3.0 stack,
 *		even when writing as little as 109 bytes into a socket
 *		that had no more than 9 bytes queued for output.  Note also
 *		that a kstream is used during output rather than a socket
 *		to facilitate encryption.
 *
 *		Eventually, for cleanliness and responsiveness, this
 *		routine should not loop; instead, if the send doesn't
 *		send all the bytes, it should put them into a buffer
 *		and return.  Message handling code would send out the
 *		buffer whenever it gets an FD_WRITE message.
 */
int
TelnetSend(kstream ks, char *buf, int len, int flags)
{
  int writelen;
  int origlen = len;

  while (TRUE) {
    writelen = kstream_write(ks, buf, len);

    if (writelen == len)	   /* Success, first or Nth time */
      return (origlen);

    if (writelen == SOCKET_ERROR) {
      if (WSAGetLastError() != WSAEWOULDBLOCK)
	return (SOCKET_ERROR);	/* Some error */
      /* For WOULDBLOCK, immediately repeat the send. */
    }
    else {
      /* Partial write; update the pointers and retry. */
      len -= writelen;
      buf += writelen;
    }
  }
}


/*
 * Function: Trim leading and trailing white space from a string.
 *
 * Parameters:
 *	s - the string to trim.
 */
void
trim(char *s)
{
  int l;
  int i;

  for (i = 0; s[i]; i++)
    if (s[i] != ' ' && s[i] != '\t')
      break;

  l = strlen(&s[i]);
  memmove(s, &s[i], l + 1);

  for (l--; l >= 0; l--) {
    if (s[l] != ' ' && s[l] != '\t')
      break;
  }
  s[l + 1] = 0;
}


/*
 *
 * Parse_cmdline
 *
 * Reads hostname and port number off the command line.
 *
 * Formats: telnet
 *          telnet <host>
 *          telnet <host> <port no>
 *          telnet -p <port no>
 *
 * Returns: TRUE if we have a hostname
 */
BOOL
parse_cmdline(char *cmdline)
{
  char *ptr;

  *szHostName = '\0';                 /* Nothing yet */
  if (*cmdline == '\0')               /* Empty command line? */
    return(FALSE);

  trim (cmdline);                     /* Remove excess spaces */
  ptr = strchr (cmdline, ' ');        /* Find 2nd token */

  if (ptr != NULL) {                  /* Port number given */
    *ptr++ = '\0';                  /* Separate into 2 words */
    port_no = atoi (ptr);
  }

  if (*cmdline != '-' && *cmdline != '/') {   /* Host name given */
    lstrcpy (szHostName, cmdline);
    return(TRUE);
  }

  return(FALSE);
}

#ifdef DEBUG
void
hexdump(char *msg, unsigned char *st, int cnt)
{
  int i;
  char strTmp[128];

  OutputDebugString("\r\n");
  if (msg != NULL) {
    OutputDebugString(msg);
    OutputDebugString("\r\n");
  }
  for(i = 0 ; i < cnt ; i++) {
    int j;

    for(j = 0 ; (j < 16) && ((i + j) < cnt) ; j++) {
      wsprintf(strTmp,"%02x  ", st[i + j]);
      if (j == 8)
	OutputDebugString("| ");
      OutputDebugString(strTmp);
    }
    i += j - 1;
    OutputDebugString("\r\n");
  } /* end for */
}
#endif
