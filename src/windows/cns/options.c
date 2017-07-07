/*
 * Copyright 1994 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

/*
 * functions to tweak the options dialog
 */

#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <time.h>

#include "cns.h"
#include "tktlist.h"
#include "cns_reg.h"

/*
 * Function: Process WM_INITDIALOG messages for the options dialog.
 * 	Set up all initial dialog values from the KERBEROS_INI file.
 *
 * Returns: TRUE if we didn't set the focus here,
 * 	    FALSE if we did.
 */
BOOL
opts_initdialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
  center_dialog(hwnd);
  set_dialog_font(hwnd, hfontdialog);

  /* krb.conf file */
  strcpy(confname, cns_res.confname);
#ifndef _WIN32
  _strupr(confname);
#endif
  SetDlgItemText(hwnd, IDD_CONF, confname);

  if (cns_res.conf_override == 0)
      EnableWindow(GetDlgItem(hwnd, IDD_CONF), 0);
  else
      EnableWindow(GetDlgItem(hwnd, IDD_CONF), 1);

  /* Credential cache file */
  strcpy(ccname, cns_res.ccname);
#ifndef _WIN32
  _strupr(ccname);
#endif
  SetDlgItemText(hwnd, IDD_CCACHE, ccname);

  if (cns_res.cc_override == 0)
      EnableWindow(GetDlgItem(hwnd, IDD_CCACHE), 0);
  else
      EnableWindow(GetDlgItem(hwnd, IDD_CCACHE), 1);

  /* Ticket duration */
  SetDlgItemInt(hwnd, IDD_LIFETIME, cns_res.lifetime, FALSE);

  /* Expiration action */
  alert = cns_res.alert;
  SendDlgItemMessage(hwnd, IDD_ALERT, BM_SETCHECK, alert, 0);

  beep = cns_res.beep;
  SendDlgItemMessage(hwnd, IDD_BEEP, BM_SETCHECK, beep, 0);

  forwardable = cns_res.forwardable;
  SendDlgItemMessage(hwnd, IDD_FORWARDABLE, BM_SETCHECK, forwardable, 0);

  noaddresses = cns_res.noaddresses;
  SendDlgItemMessage(hwnd, IDD_NOADDRESSES, BM_SETCHECK, noaddresses, 0);

  return TRUE;
}


/*
 * Function: Process WM_COMMAND messages for the options dialog.
 */
void
opts_command(HWND hwnd, int cid, HWND hwndCtl, UINT codeNotify)
{
  char newname[FILENAME_MAX];
  BOOL b;
  int lifetime;

  switch (cid) {
  case IDOK:

    /* Ticket duration */
    lifetime = GetDlgItemInt(hwnd, IDD_LIFETIME, &b, FALSE);

    if (!b) {
      MessageBox(hwnd, "Lifetime must be a number!", "",
		 MB_OK | MB_ICONEXCLAMATION);
      return; /* TRUE */
    }

    cns_res.lifetime = lifetime;

    if (cns_res.conf_override) {
	    /* krb.conf file */
	    GetDlgItemText(hwnd, IDD_CONF, newname, sizeof(newname));
	    trim(newname);
	    if (newname[0] == '\0')
		    strcpy(newname, cns_res.def_confname);
	    if (_stricmp(newname, confname)) {  /* file name changed */
	      MessageBox(NULL,
			 "Change to configuration file location requires a restart"
			 "of KerbNet.\n"
			 "Please exit this application and restart it for the change to take"
			 "effect",
			 "", MB_OK | MB_ICONEXCLAMATION);
	    }
	    strcpy(confname, newname);
    }

    /* Credential cache file */
    GetDlgItemText(hwnd, IDD_CCACHE, newname, sizeof(newname));
    trim(newname);

    if (newname[0] == '\0')
	    strcpy(newname, cns_res.def_ccname);

    if (_stricmp(ccname, newname)) {     /* Did we change ccache file? */
      krb5_error_code code;
      krb5_ccache cctemp;

      code = k5_init_ccache(&cctemp);
      if (code) {                     /* Problem opening new one? */
	com_err(NULL, code,
		"while changing ccache.\r\nRestoring old ccache.");
      } else {
        strcpy(ccname, newname);
        strcpy(cns_res.ccname, newname);

	code = krb5_cc_close(k5_context, k5_ccache);
	k5_ccache = cctemp;         /* Copy new into old */
	if (k5_name_from_ccache(k5_ccache)) {
	  kwin_init_name(GetParent(hwnd), "");
	  kwin_set_default_focus(GetParent(hwnd));
	}
	ticket_init_list(GetDlgItem (GetParent(hwnd),
				     IDD_TICKET_LIST));
      }
    }

    /*
     * get values for the clickboxes
     */
    alert = SendDlgItemMessage(hwnd, IDD_ALERT, BM_GETCHECK, 0, 0);
    cns_res.alert = alert;

    beep = SendDlgItemMessage(hwnd, IDD_BEEP, BM_GETCHECK, 0, 0);
    cns_res.beep = beep;

    forwardable = SendDlgItemMessage(hwnd, IDD_FORWARDABLE, BM_GETCHECK, 0, 0);
    cns_res.forwardable = forwardable;

    noaddresses = SendDlgItemMessage(hwnd, IDD_NOADDRESSES, BM_GETCHECK, 0, 0);
    cns_res.noaddresses = noaddresses;

    EndDialog(hwnd, IDOK);

    return; /* TRUE */

  case IDCANCEL:
    EndDialog(hwnd, IDCANCEL);

    return; /* TRUE */
  }

  return; /* FALSE */
}


/*
 * Function: Process dialog specific messages for the opts dialog.
 */
BOOL CALLBACK
opts_dlg_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    HANDLE_MSG(hwnd, WM_INITDIALOG, opts_initdialog);

    HANDLE_MSG(hwnd, WM_COMMAND, opts_command);
  }

  return FALSE;
}


/*
 * Function: Display and process the options dialog.
 *
 * Parameters:
 *	hwnd - the parent window for the dialog
 *
 * Returns: TRUE if the dialog completed successfully, FALSE otherwise.
 */
BOOL
opts_dialog(HWND hwnd)
{
  DLGPROC dlgproc;
  int rc;

#ifdef _WIN32
  dlgproc = opts_dlg_proc;
#else
  dlgproc = (FARPROC)MakeProcInstance(opts_dlg_proc, hinstance);
  assert(dlgproc != NULL);

  if (dlgproc == NULL)
    return FALSE;
#endif

  rc = DialogBox(hinstance, MAKEINTRESOURCE(ID_OPTS), hwnd, dlgproc);
  assert(rc != -1);

#ifndef _WIN32
  FreeProcInstance((FARPROC)dlgproc);
#endif

  return rc == IDOK;
}
