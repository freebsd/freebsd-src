/* wt-proto.h */
BOOL
InitApplication(
    HINSTANCE
    );

BOOL
InitInstance(
    HINSTANCE,
    int
    );

LRESULT
CALLBACK
MainWndProc(
    HWND,
    UINT,
    WPARAM,
    LPARAM
    );

INT_PTR
CALLBACK
OpenTelnetDlg(
    HWND,
    UINT,
    WPARAM,
    LPARAM
    );

int
TelnetSend(
    kstream,
    char *,
    int,
    int
    );

int
OpenTelnetConnection(
    void
    );

int
DoDialog(
    char *szDialog,
    DLGPROC lpfnDlgProc
    );

BOOL
parse_cmdline(
    char *cmdline
    );

CONNECTION *
GetNewConnection(
    void
    );

void
start_negotiation(
    kstream ks
    );
