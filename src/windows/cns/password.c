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

/*
 * Function: Changes the password.
 *
 * Parameters:
 *	hwnd - the current window from which command was invoked.
 *
 *	name - name of user to change password for
 *
 *	instance - instance of user to change password for
 *
 *	realm - realm in which to change password
 *
 *	oldpw - the old password
 *
 *	newpw - the new password to change to
 *
 * Returns: TRUE if change took place, FALSE otherwise.
 */
BOOL
change_password(HWND hwnd, char *name, char *instance, char *realm,
		char *oldpw, char *newpw)
{
#ifdef KRB4
  des_cblock new_key;
  char *ret_st;
  int krc;
  char *p;
  CREDENTIALS *c;
  int ncred;
  char pname[ANAME_SZ];
  char pinstance[INST_SZ];

  push_credentials(&c, pname, pinstance, &ncred);
  krc = krb_get_pw_in_tkt(name, instance, realm, PWSERV_NAME, KADM_SINST,
			  1, oldpw);

  if (krc != KSUCCESS) {
    if (krc == INTK_BADPW)
      p = "Old password is incorrect";
    else
      p = krb_get_err_text(krc);
    pop_credentials(c, pname, pinstance, ncred);
    MessageBox(hwnd, p, "", MB_OK | MB_ICONEXCLAMATION);

    return FALSE;
  }

  krc = kadm_init_link(PWSERV_NAME, KRB_MASTER, realm);

  if (krc != KSUCCESS) {
    pop_credentials(c, pname, pinstance, ncred);
    MessageBox(hwnd, kadm_get_err_text(krc), "", MB_OK | MB_ICONEXCLAMATION);

    return FALSE;
  }

  des_string_to_key(newpw, new_key);
  krc = kadm_change_pw2(new_key, newpw, &ret_st);
  pop_credentials(c, pname, pinstance, ncred);

  if (ret_st != NULL)
    free(ret_st);

  if (krc != KSUCCESS) {
    MessageBox(hwnd, kadm_get_err_text(krc), "", MB_OK | MB_ICONEXCLAMATION);

    return FALSE;
  }

  return TRUE;
#endif /* KRB4 */

#ifdef KRB5
  char *msg;                                  /* Message string */
  krb5_error_code code;                       /* Return value */
  code = k5_change_password(hwnd, k5_context, name, realm, oldpw, newpw, &msg);

  if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY)
      MessageBox(NULL, "Password incorrect", NULL, MB_ICONEXCLAMATION);
  else if (code == -1)
	  MessageBox(NULL, (msg ? msg : "Cannot change password"), NULL,
		 MB_ICONEXCLAMATION);
  else if (code != 0)
	  com_err(NULL, code, (msg ? msg : "while changing password."));
  else
	  MessageBox(NULL, (msg ? msg : "Password changed"), "Kerberos", MB_OK | MB_APPLMODAL);

  return (code == 0);

#endif /* KRB5 */
}
/*
 * Function: Process WM_COMMAND messages for the password dialog.
 */
void
password_command(HWND hwnd, int cid, HWND hwndCtl, UINT codeNotify)
{
  char name[ANAME_SZ];
  char instance[INST_SZ];
  char realm[REALM_SZ];
  char oldpw[MAX_KPW_LEN];
  char newpw1[MAX_KPW_LEN];
  char newpw2[MAX_KPW_LEN];
  HCURSOR hcursor;
  BOOL b;
  int id;

  if (codeNotify != BN_CLICKED) {
    GetDlgItemText(hwnd, IDD_PASSWORD_NAME, name, sizeof(name));
    trim(name);
    GetDlgItemText(hwnd, IDD_PASSWORD_REALM, realm, sizeof(realm));
    trim(realm);
    GetDlgItemText(hwnd, IDD_OLD_PASSWORD, oldpw, sizeof(oldpw));
    GetDlgItemText(hwnd, IDD_NEW_PASSWORD1, newpw1, sizeof(newpw1));
    GetDlgItemText(hwnd, IDD_NEW_PASSWORD2, newpw2, sizeof(newpw2));
    b = strlen(name) && strlen(realm) && strlen(oldpw) &&
      strlen(newpw1) && strlen(newpw2);
    EnableWindow(GetDlgItem(hwnd, IDOK), b);
    id = (b) ? IDOK : IDD_PASSWORD_CR;
    SendMessage(hwnd, DM_SETDEFID, id, 0);

    return; /* FALSE */
  }

  switch (cid) {
  case IDOK:
    if (isblocking)
      return; /* TRUE */

    GetDlgItemText(hwnd, IDD_PASSWORD_NAME, name, sizeof(name));
    trim(name);
    GetDlgItemText(hwnd, IDD_PASSWORD_INSTANCE, instance, sizeof(instance));
    trim(instance);
    GetDlgItemText(hwnd, IDD_PASSWORD_REALM, realm, sizeof(realm));
    trim(realm);
    GetDlgItemText(hwnd, IDD_OLD_PASSWORD, oldpw, sizeof(oldpw));
    GetDlgItemText(hwnd, IDD_NEW_PASSWORD1, newpw1, sizeof(newpw1));
    GetDlgItemText(hwnd, IDD_NEW_PASSWORD2, newpw2, sizeof(newpw2));

    if (strcmp(newpw1, newpw2) != 0) {
      MessageBox(hwnd, "The two passwords you entered don't match!", "",
		 MB_OK | MB_ICONEXCLAMATION);
      SetDlgItemText(hwnd, IDD_NEW_PASSWORD1, "");
      SetDlgItemText(hwnd, IDD_NEW_PASSWORD2, "");
      PostMessage(hwnd, WM_NEXTDLGCTL,
		  (WPARAM)GetDlgItem(hwnd, IDD_NEW_PASSWORD1), MAKELONG(1, 0));

      return; /* TRUE */
    }

    hcursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    start_blocking_hook(BLOCK_MAX_SEC);

    if (change_password(hwnd, name, instance, realm, oldpw, newpw1))
      EndDialog(hwnd, IDOK);
    else
      PostMessage(hwnd, WM_NEXTDLGCTL,
		  (WPARAM)GetDlgItem(hwnd, IDD_OLD_PASSWORD), MAKELONG(1, 0));

    end_blocking_hook();
    SetCursor(hcursor);

    return; /* TRUE */

  case IDCANCEL:
    if (isblocking)
      WSACancelBlockingCall();
    EndDialog(hwnd, IDCANCEL);

    return; /* TRUE */

  case IDD_PASSWORD_CR:
    id = GetDlgCtrlID(GetFocus());
    assert(id != 0);

    if (id == IDD_NEW_PASSWORD2)
      PostMessage(hwnd, WM_NEXTDLGCTL,
		  (WPARAM)GetDlgItem(hwnd, IDD_PASSWORD_NAME), MAKELONG(1, 0));
    else
      PostMessage(hwnd, WM_NEXTDLGCTL, 0, 0);

    return; /* TRUE */

  }

  return; /* FALSE */
}


/*
 * Function: Process WM_INITDIALOG messages for the password dialog.
 * 	Set up all initial dialog values from the parent dialog.
 *
 * Returns: TRUE if we didn't set the focus here,
 * 	FALSE if we did.
 */
BOOL
password_initdialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
  char name[ANAME_SZ];
  char realm[REALM_SZ];
  HWND hwndparent;
  int id;
#ifdef KRB4
  char instance[INST_SZ];
#endif

  center_dialog(hwnd);
  set_dialog_font(hwnd, hfontdialog);

  hwndparent = GetParent(hwnd);
  assert(hwndparent != NULL);

  GetDlgItemText(hwndparent, IDD_LOGIN_NAME, name, sizeof(name));
  trim(name);
  SetDlgItemText(hwnd, IDD_PASSWORD_NAME, name);

#ifdef KRB4
  GetDlgItemText(hwndparent, IDD_LOGIN_INSTANCE, instance, sizeof(instance));
  trim(instance);
  SetDlgItemText(hwnd, IDD_PASSWORD_INSTANCE, instance);
#endif

  GetDlgItemText(hwndparent, IDD_LOGIN_REALM, realm, sizeof(realm));
  trim(realm);
  SetDlgItemText(hwnd, IDD_PASSWORD_REALM, realm);

  if (strlen(name) == 0)
    id = IDD_PASSWORD_NAME;
  else if (strlen(realm) == 0)
    id = IDD_PASSWORD_REALM;
  else
    id = IDD_OLD_PASSWORD;

  SetFocus(GetDlgItem(hwnd, id));

  return FALSE;
}


/*
 * Function: Process dialog specific messages for the password dialog.
 */
BOOL CALLBACK
password_dlg_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {

    HANDLE_MSG(hwnd, WM_INITDIALOG, password_initdialog);

    HANDLE_MSG(hwnd, WM_COMMAND, password_command);

  case WM_SETCURSOR:
    if (isblocking) {
      SetCursor(LoadCursor(NULL, IDC_WAIT));
      SetWindowLongPtr(hwnd, DWLP_MSGRESULT, TRUE);

      return TRUE;
    }
    break;
  }

  return FALSE;
}


/*
 * Function: Display and process the password dialog.
 *
 * Parameters:
 *	hwnd - the parent window for the dialog
 *
 * Returns: TRUE if the dialog completed successfully, FALSE otherwise.
 */
BOOL
password_dialog(HWND hwnd)
{
  DLGPROC dlgproc;
  int rc;

#ifdef _WIN32
  dlgproc = password_dlg_proc;
#else
  dlgproc = (FARPROC)MakeProcInstance(password_dlg_proc, hinstance);
  assert(dlgproc != NULL);

  if (dlgproc == NULL)
    return FALSE;
#endif

  rc = DialogBox(hinstance, MAKEINTRESOURCE(ID_PASSWORD), hwnd, dlgproc);
  assert(rc != -1);

#ifndef _WIN32
  FreeProcInstance((FARPROC)dlgproc);
#endif

  return rc == IDOK;
}
