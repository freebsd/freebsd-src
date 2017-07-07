/* windows/cns/tktlist.c */
/*
 * Copyright 1994 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

/* Handle all actions of the Kerberos ticket list. */

#if !defined(KRB5) && !defined(KRB4)
#define KRB5 1
#endif

#include <windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <time.h>

#ifdef KRB4
#include "mit-copyright.h"
#include "kerberos.h"
#endif

#ifdef KRB5
#include "winsock.h"
#include "krb5.h"
#include "com_err.h"
#endif

#include "cns.h"
#include "tktlist.h"

/*
 * Ticket information for a list line
 */
typedef struct {
  BOOL ticket;		/* TRUE if this is a real ticket */
  time_t issue_time;	/* time_t of issue */
  long lifetime;		/* Lifetime for ticket in 5 minute intervals */
  char buf[0];		/* String to display */
} TICKETINFO, *LPTICKETINFO;

/*
 * Function: Returns a standard ctime date with day of week and year
 *	removed.
 *
 * Parameters:
 *	t - time_t date to convert
 *
 * Returns: A pointer to the adjusted time value.
 */
static char *
short_date (time_t t)
{
  static char buf[26 - 4];
  char *p;

  p = ctime(&t);
  assert(p != NULL);

  strcpy (buf, p + 4);
  buf[12] = '\0';

  return buf;
}


/*+
 * Function: Initializes and populates the ticket list with all existing
 * 	Kerberos tickets.
 *
 * Parameters:
 * 	hwnd - the window handle of the ticket window.
 *
 * Returns: Number of elements in the list or -1 on error
 */
int
ticket_init_list (HWND hwnd)
{
  int ncred;
  LRESULT rc;
  int l;
  LPTICKETINFO lpinfo;
  char buf[26+2 + 26+2 + ANAME_SZ+1 + INST_SZ+1 + REALM_SZ + 22];
#ifdef KRB4
  int i;
  time_t expiration;
  char service[ANAME_SZ];
  char instance[INST_SZ];
  char realm[REALM_SZ];
  CREDENTIALS c;
#endif
#ifdef KRB5
  krb5_cc_cursor cursor;
  krb5_error_code code;
  krb5_creds c;
  char *sname;                            /* Name of the service */
  char *flags_string(krb5_creds *cred);
#endif

  SetWindowRedraw(hwnd, FALSE);

  rc = ListBox_GetCount(hwnd);
  assert(rc != LB_ERR);

  if (rc > 0)
    ticket_destroy(hwnd);

  while (--rc >= 0)
    ListBox_DeleteString(hwnd, rc);

#ifdef KRB4
  ncred = krb_get_num_cred();
  for (i = 1; i <= ncred; i++) {
    krb_get_nth_cred(service, instance, realm, i);
    krb_get_cred(service, instance, realm, &c);
    strcpy(buf, " ");
    strncat(buf, short_date(c.issue_date - kwin_get_epoch()),
            sizeof(buf) - 1 - strlen(buf));
    expiration = c.issue_date - kwin_get_epoch() + (long) c.lifetime * 5L * 60L;
    strncat(buf, "      ", sizeof(buf) - 1 - strlen(buf));
    strncat(buf, short_date(expiration), sizeof(buf) - 1 - strlen(buf));
    strncat(buf, "      ", sizeof(buf) - 1 - strlen(buf));
    l = strlen(buf);
    sprintf(&buf[l], "%s%s%s%s%s (%d)",
	    c.service, (c.instance[0] ? "." : ""), c.instance,
	    (c.realm[0] ? "@" : ""), c.realm, c.kvno);
    l = strlen(buf);

    lpinfo = (LPTICKETINFO) malloc(sizeof(TICKETINFO) + l + 1);
    assert(lpinfo != NULL);

    if (lpinfo == NULL)
      return -1;

    lpinfo->ticket = TRUE;
    lpinfo->issue_time = c.issue_date - kwin_get_epoch(); /* back to system time */
    lpinfo->lifetime = (long) c.lifetime * 5L * 60L;
    strcpy(lpinfo->buf, buf);

    rc = ListBox_AddItemData(hwnd, lpinfo);
    assert(rc >= 0);

    if (rc < 0)
      return -1;
  }

#endif

#ifdef KRB5

  ncred = 0;
  if (code = krb5_cc_start_seq_get(k5_context, k5_ccache, &cursor)) {
    if (code != KRB5_FCC_NOFILE) {
      return -1;
    }
  } else {
    while (1) {
      code = krb5_cc_next_cred(k5_context, k5_ccache, &cursor, &c);
      if (code != 0)
	break;

      ncred++;
      strcpy (buf, "  ");
      strncat(buf, short_date (c.times.starttime - kwin_get_epoch()),
	      sizeof(buf) - 1 - strlen(buf));
      strncat(buf, "      ", sizeof(buf) - 1 - strlen(buf));
      strncat(buf, short_date (c.times.endtime - kwin_get_epoch()),
	      sizeof(buf) - 1 - strlen(buf));
      strncat(buf, "      ", sizeof(buf) - 1 - strlen(buf));

      /* Add ticket service name and realm */
      code = krb5_unparse_name (k5_context, c.server, &sname);
      if (code) {
	com_err (NULL, code, "while unparsing server name");
	break;
      }
      strncat (buf, sname, sizeof(buf) - 1 - strlen(buf));

      strncat (buf, flags_string (&c), sizeof(buf) - 1 - strlen(buf)); /* Add flag info */

      l = strlen(buf);
      lpinfo = (LPTICKETINFO) malloc(sizeof(TICKETINFO) + l + 1);
      assert(lpinfo != NULL);

      if (lpinfo == NULL)
	return -1;

      lpinfo->ticket = TRUE;
      lpinfo->issue_time = c.times.starttime - kwin_get_epoch();
      lpinfo->lifetime = c.times.endtime - c.times.starttime;
      strcpy(lpinfo->buf, buf);

      rc = ListBox_AddItemData(hwnd, lpinfo);
      assert(rc >= 0);

      if (rc < 0)
	return -1;
    }
    if (code == KRB5_CC_END) {               /* End of ccache */
      if (code = krb5_cc_end_seq_get(k5_context, k5_ccache, &cursor)) {
	return -1;
      }
    } else {
      return -1;
    }
  }
#endif

  if (ncred <= 0) {
    strcpy(buf, " No Tickets");
    lpinfo = (LPTICKETINFO) malloc(sizeof(TICKETINFO) + strlen(buf) + 1);
    assert(lpinfo != NULL);

    if (lpinfo == NULL)
      return -1;

    lpinfo->ticket = FALSE;
    strcpy (lpinfo->buf, buf);
    rc = ListBox_AddItemData(hwnd, lpinfo);
    assert(rc >= 0);
  }

  SetWindowRedraw(hwnd, TRUE);

  return ncred;
}


/*
 * Function: Destroy the ticket list.  Make sure to delete all
 *	ticket entries created during ticket initialization.
 *
 * Parameters:
 *	hwnd - the window handle of the ticket window.
 */
void
ticket_destroy (
		HWND hwnd)
{
  int i;
  int n;
  LRESULT rc;

  n = ListBox_GetCount(hwnd);

  for (i = 0; i < n; i++) {
    rc = ListBox_GetItemData(hwnd, i);
    assert(rc != LB_ERR);

    if (rc != LB_ERR)
      free ((void *) rc);
  }
}


/*
 * Function: Respond to the WM_MEASUREITEM message for the ticket list
 * 	by setting each list item up at 1/4 inch hight.
 */
void
ticket_measureitem(HWND hwnd, MEASUREITEMSTRUCT *lpmi)
{
  int logpixelsy;
  HDC hdc;

  if (lpmi->CtlID != IDD_TICKET_LIST)
    return;

  hdc = GetDC(HWND_DESKTOP);
  logpixelsy = GetDeviceCaps(hdc, LOGPIXELSY);
  ReleaseDC(HWND_DESKTOP, hdc);
  lpmi->itemHeight = logpixelsy / 4;	/* 1/4 inch */
}


/*
 * Function: Respond to the WM_DRAWITEM message for the ticket list
 * 	by displaying a single list item.
 */
void
ticket_drawitem(HWND hwnd, const DRAWITEMSTRUCT *lpdi)
{
  BOOL rc;
  COLORREF bkcolor;
  HBRUSH hbrush;
  UINT textheight;
  UINT alignment;
  int left, top;
  BOOL b;
  LPTICKETINFO lpinfo;
  HICON hicon;
#if 0
  COLORREF textcolor;
  COLORREF orgbkcolor;
  COLORREF orgtextcolor;
#endif
  SIZE Size;

  if (lpdi->CtlID != IDD_TICKET_LIST)
    return;

  lpinfo = (LPTICKETINFO) lpdi->itemData;

  if (lpdi->itemAction == ODA_FOCUS)
    return;

#if 0
  if (lpdi->itemState & ODS_SELECTED) {
    textcolor = GetSysColor(COLOR_HIGHLIGHTTEXT);
    bkcolor = GetSysColor(COLOR_HIGHLIGHT);

    orgtextcolor = SetTextColor(lpdi->hDC, textcolor);
    assert(textcolor != 0x80000000);

    orgbkcolor = SetBkColor(lpdi->hDC, bkcolor);
    assert(bkcolor != 0x80000000);
  }
  else
#endif

    bkcolor = GetBkColor(lpdi->hDC);
  hbrush = CreateSolidBrush(bkcolor);
  assert(hbrush != NULL);

  FillRect(lpdi->hDC, &(lpdi->rcItem), hbrush);
  DeleteObject(hbrush);

  /*
   * Display the appropriate icon
   */
  if (lpinfo->ticket) {
    hicon = kwin_get_icon(lpinfo->issue_time + lpinfo->lifetime);
    left = lpdi->rcItem.left - (32 - ICON_WIDTH) / 2;
    top = lpdi->rcItem.top;
    top += (lpdi->rcItem.bottom - lpdi->rcItem.top - 32) / 2;

    b = DrawIcon(lpdi->hDC, left, top, hicon);
    assert(b);
  }

  /*
   * Display centered string
   */
#ifdef _WIN32
  GetTextExtentPoint32(lpdi->hDC, "X", 1, &Size);
#else
  GetTextExtentPoint(lpdi->hDC, "X", 1, &Size);
#endif

  textheight = Size.cy;

  alignment = SetTextAlign(lpdi->hDC, TA_TOP | TA_LEFT);

  if (lpinfo->ticket)
    left = lpdi->rcItem.left + ICON_WIDTH;
  else
    left = lpdi->rcItem.left;

  top = lpdi->rcItem.top;
  top += (lpdi->rcItem.bottom - lpdi->rcItem.top - textheight) / 2;
  rc = TextOut(lpdi->hDC, left, top, (LPSTR) lpinfo->buf,
	       strlen((LPSTR) lpinfo->buf));
  assert(rc);

  alignment = SetTextAlign(lpdi->hDC, alignment);

#if 0
  if (lpdi->itemState & ODS_SELECTED) {
    textcolor = SetTextColor(lpdi->hDC, orgtextcolor);
    assert(textcolor != 0x80000000);

    bkcolor = SetBkColor(lpdi->hDC, orgbkcolor);
    assert(bkcolor != 0x80000000);
  }

#endif
}


#ifdef KRB5

/*
 *
 * Flags_string
 *
 * Return buffer with the current flags for the credential
 *
 */
char *
flags_string(krb5_creds *cred) {
  static char buf[32];
  int i = 0;

  buf[i++] = ' ';
  buf[i++] = '(';
  if (cred->ticket_flags & TKT_FLG_FORWARDABLE)
    buf[i++] = 'F';
  if (cred->ticket_flags & TKT_FLG_FORWARDED)
    buf[i++] = 'f';
  if (cred->ticket_flags & TKT_FLG_PROXIABLE)
    buf[i++] = 'P';
  if (cred->ticket_flags & TKT_FLG_PROXY)
    buf[i++] = 'p';
  if (cred->ticket_flags & TKT_FLG_MAY_POSTDATE)
    buf[i++] = 'D';
  if (cred->ticket_flags & TKT_FLG_POSTDATED)
    buf[i++] = 'd';
  if (cred->ticket_flags & TKT_FLG_INVALID)
    buf[i++] = 'i';
  if (cred->ticket_flags & TKT_FLG_RENEWABLE)
    buf[i++] = 'R';
  if (cred->ticket_flags & TKT_FLG_INITIAL)
    buf[i++] = 'I';
  if (cred->ticket_flags & TKT_FLG_HW_AUTH)
    buf[i++] = 'H';
  if (cred->ticket_flags & TKT_FLG_PRE_AUTH)
    buf[i++] = 'A';

  buf[i++] = ')';
  buf[i] = '\0';
  if (i <= 3)
    buf[0] = '\0';
  return(buf);
}

#endif /* KRB5 */
