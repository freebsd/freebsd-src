/*
 * cns.h
 *
 * Public Domain -- written by Cygnus Support.
 */

/* Only one time, please */
#ifndef	KWIN_DEFS
#define KWIN_DEFS

#if !defined(KRB4) && !defined(KRB5)
#define KRB5
#endif

#ifndef RC_INVOKED

#ifdef KRB4
#include "mit-copyright.h"
#include "krb.h"
#include "kadm.h"
#include "org.h"
#endif

#ifdef KRB5
#include "winsock.h"
#include "krb5.h"
#include "krbini.h"
#include "com_err.h"

#define DEFAULT_TKT_LIFE    120             /* In 5 minute units */
#define ANAME_SZ	        40
#define	REALM_SZ	        40
#define	SNAME_SZ	        40
#define	INST_SZ		        40
#define MAX_KPW_LEN	        128
/* include space for '.' and '@' */
#define	MAX_K_NAME_SZ	    (ANAME_SZ + INST_SZ + REALM_SZ + 2)
#ifdef CYGNUS
#define ORGANIZATION        "Cygnus Solutions\n(800)CYGNUS-1\nhttp://www.cygnus.com\ninfo@cygnus.com"
#endif
#define CREDENTIALS         char
#endif

/*
 * Constants
 */
#define BLOCK_MAX_SEC 30	       /* Blocking timeout duration */
#define KWIN_UPDATE_PERIOD 30000       /* Every 30 seconds update the screen */
#define TIME_BUFFER	300	       /* Pop-up time buffer in seconds */
#define WM_KWIN_SETNAME (WM_USER+100)  /* Sets the name fields in the dialog */

#endif /* RC_INVOKED */

/*
 * Menu items
 */
#define FILE_MENU_ITEMS 3
#define FILE_MENU_MAX_LOGINS 5
#define IDM_KWIN 1000
#define   IDM_OPTIONS 1001
#define   IDM_EXIT 1002
#define   IDM_FIRST_LOGIN 1003

#define   IDM_HELP_INDEX 1020
#define   IDM_ABOUT 1021

/*
 * Accelerator
 */
#define IDA_KWIN 2000

/*
 * Dialog and dialog item ids
 */
#define KWIN_DIALOG_CLASS "KERBEROS"	/* class for kerberos dialog */
#define KWIN_DIALOG_NAME "Krb5"		/* name for kerberos dialog */

#define ID_KWIN 100			/* the main kerberos dialog */
#define IDD_KWIN_FIRST 101
#define   IDD_TICKET_LIST_TITLE 101
#define   IDD_TICKET_LIST 102

#ifdef KRB4

#define IDD_MIN_TITLE 103
#define   IDD_LOGIN_NAME_TITLE 103
#define   IDD_LOGIN_INSTANCE_TITLE 104
#define   IDD_LOGIN_REALM_TITLE 105
#define   IDD_LOGIN_PASSWORD_TITLE 106
#define IDD_MAX_TITLE 106

#define IDD_MIN_EDIT 107
#define   IDD_LOGIN_NAME 107
#define   IDD_LOGIN_INSTANCE 108
#define   IDD_LOGIN_REALM 109
#define   IDD_LOGIN_PASSWORD 110
#define IDD_MAX_EDIT 110

#endif

#ifdef KRB5

#define IDD_MIN_TITLE 103
#define   IDD_LOGIN_NAME_TITLE 103
#define   IDD_LOGIN_PASSWORD_TITLE 104
#define   IDD_LOGIN_REALM_TITLE 105
#define IDD_MAX_TITLE 105

#define IDD_MIN_EDIT 107
#define   IDD_LOGIN_NAME 107
#define   IDD_LOGIN_PASSWORD 108
#define   IDD_LOGIN_REALM 109
#define IDD_MAX_EDIT 109

#endif

#define IDD_MIN_BUTTON 111
#define   IDD_CHANGE_PASSWORD 111
#define   IDD_TICKET_DELETE 112
#define   IDD_LOGIN 113
#define   IDD_MAX_BUTTON 113
#define IDD_PASSWORD_CR2 114            /* For better cr handling */

#define IDD_KWIN_LAST 114


#define ID_PASSWORD 200
#define   IDD_PASSWORD_NAME 204
#define   IDD_PASSWORD_INSTANCE 205
#define   IDD_PASSWORD_REALM 206
#define   IDD_OLD_PASSWORD 207
#define   IDD_NEW_PASSWORD1 208
#define   IDD_NEW_PASSWORD2 209
#define   IDD_PASSWORD_CR 210


#define ID_OPTS 300
#define   IDD_CONF 301
#define   IDD_REALMS 302
#define   IDD_LIFETIME 303
#define   IDD_CCACHE 304
#define   IDD_ACTIONS 310
#define     IDD_BEEP 311
#define     IDD_ALERT 312
#define   IDD_TKOPT 320
#define   IDD_FORWARDABLE 321
#define   IDD_NOADDRESSES 322

/*
 * the entire range (400 through 499) is reserved for the blasted variable
 * dialog box thingie.
 */
#define ID_VARDLG    400

/*
 * Dialog dimensions
 */
#define KWIN_MIN_WIDTH 180
#define KWIN_MIN_HEIGHT 110

/*
 * Icons
 */
#define IDI_KWIN 1		/* The program icon */

#define ICON_WIDTH 30	/* Width used with icons */
#define ICON_HEIGHT 20	/* Height used with icons */

#define IDI_FIRST_CLOCK 2
#define IDI_0_MIN 2		/* < 5 minutes left */
#define IDI_5_MIN 3
#define IDI_10_MIN 4
#define IDI_15_MIN 5
#define IDI_20_MIN 6
#define IDI_25_MIN 7
#define IDI_30_MIN 8
#define IDI_35_MIN 9
#define IDI_40_MIN 10
#define IDI_45_MIN 11
#define IDI_50_MIN 12
#define IDI_55_MIN 13
#define IDI_60_MIN 14
#define IDI_EXPIRED 15
#define IDI_TICKET 16
#define IDI_LAST_CLOCK 16
#define MAX_ICONS (IDI_LAST_CLOCK - IDI_FIRST_CLOCK + 1)

#ifndef RC_INVOKED

extern BOOL isblocking;
extern HFONT hfontdialog;
extern HINSTANCE hinstance;
extern BOOL alert;
extern BOOL beep;

extern char confname[FILENAME_MAX];

#ifdef KRB5
extern krb5_context k5_context;
extern krb5_ccache k5_ccache;
extern char ccname[FILENAME_MAX];
extern BOOL forwardable;
extern BOOL noaddresses;
#endif

/*
 * Prototypes
 */

/* in cns.c */

void kwin_init_name(HWND, char *);
void kwin_set_default_focus(HWND);
time_t kwin_get_epoch(void);

/* in options.c */
BOOL opts_initdialog(HWND, HWND, LPARAM);
void opts_command(HWND, int, HWND, UINT);
BOOL CALLBACK opts_dlg_proc(HWND, UINT, WPARAM, LPARAM);
BOOL opts_dialog(HWND);

/* in password.c */
BOOL change_password(HWND, char *, char *, char *, char *, char *);
void password_command(HWND, int, HWND, UINT);
BOOL password_initdialog(HWND, HWND, LPARAM);
BOOL CALLBACK password_dlg_proc(HWND, UINT, WPARAM, LPARAM);
BOOL password_dialog(HWND);

#ifdef KRB5
krb5_error_code k5_dest_tkt(void);
int k5_get_num_cred(int);
int k5_kname_parse(char *, char *, char *);
krb5_error_code k5_init_ccache(krb5_ccache *);
int k5_name_from_ccache(krb5_ccache);
krb5_error_code k5_change_password(HWND, krb5_context, char *, char *, char *,
				   char *, char **);

#endif /* KRB5 */

HICON kwin_get_icon(time_t);
void trim(char *);
void start_blocking_hook(int);
void end_blocking_hook(void);
void center_dialog(HWND);
void set_dialog_font(HWND, HFONT);

#endif /* RC_INVOKED */

#endif
