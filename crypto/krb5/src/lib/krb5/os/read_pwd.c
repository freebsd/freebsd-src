/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/read_pwd.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"

#if !defined(_WIN32)
#define DEFINED_KRB5_READ_PASSWORD
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#ifndef ECHO_PASSWORD
#include <termios.h>
#endif /* ECHO_PASSWORD */

krb5_error_code
krb5_read_password(krb5_context context,
                   const char *prompt, const char *prompt2,
                   char *return_pwd, unsigned int *size_return)
{
    krb5_data reply_data, verify_data = empty_data();
    krb5_prompt k5prompt, vprompt;
    krb5_error_code retval;

    /* *size_return is the space available in the return buffer on input. */
    reply_data = make_data(return_pwd, *size_return);
    k5prompt.prompt = (char *)prompt;
    k5prompt.hidden = 1;
    k5prompt.reply = &reply_data;
    retval = krb5_prompter_posix(NULL, NULL, NULL, NULL, 1, &k5prompt);
    if (retval || prompt2 == NULL)
        goto done;

    retval = alloc_data(&verify_data, *size_return);
    if (retval)
        goto done;
    vprompt.prompt = (char *)prompt2;
    vprompt.hidden = 1;
    vprompt.reply = &verify_data;
    retval = krb5_prompter_posix(NULL, NULL, NULL, NULL, 1, &vprompt);
    if (retval)
        goto done;
    if (strncmp(return_pwd, verify_data.data, *size_return) != 0)
        retval = KRB5_LIBOS_BADPWDMATCH;

done:
    zapfree(verify_data.data, verify_data.length);
    if (!retval)
        *size_return = k5prompt.reply->length;
    else
        zap(return_pwd, *size_return);
    return retval;
}
#endif

#if defined(_WIN32)
#define DEFINED_KRB5_READ_PASSWORD

#include <io.h>

typedef struct {
    char *pwd_prompt;
    char *pwd_prompt2;
    char *pwd_return_pwd;
    int  *pwd_size_return;
} pwd_params;

void
center_dialog(HWND hwnd)
{
    int scrwidth, scrheight;
    int dlgwidth, dlgheight;
    RECT r;
    HDC hdc;

    if (hwnd == NULL)
        return;

    GetWindowRect(hwnd, &r);
    dlgwidth = r.right  - r.left;
    dlgheight = r.bottom - r.top ;
    hdc = GetDC(NULL);
    scrwidth = GetDeviceCaps(hdc, HORZRES);
    scrheight = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(NULL, hdc);
    r.left = (scrwidth - dlgwidth) / 2;
    r.top  = (scrheight - dlgheight) / 2;
    MoveWindow(hwnd, r.left, r.top, dlgwidth, dlgheight, TRUE);
}

#ifdef _WIN32
static krb5_error_code
read_console_password(krb5_context        context,
                      const char          * prompt,
                      const char          * prompt2,
                      char                * password,
                      int                 * pwsize)
{
    HANDLE              handle;
    DWORD               old_mode, new_mode;
    char                *tmpstr = 0;
    char                *ptr;
    int                 scratchchar;
    krb5_error_code     errcode = 0;

    handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        return ENOTTY;
    if (!GetConsoleMode(handle, &old_mode))
        return ENOTTY;

    new_mode = old_mode;
    new_mode |=  ( ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT );
    new_mode &= ~( ENABLE_ECHO_INPUT );

    if (!SetConsoleMode(handle, new_mode))
        return ENOTTY;

    (void) fputs(prompt, stdout);
    (void) fflush(stdout);
    (void) memset(password, 0, *pwsize);

    if (fgets(password, *pwsize, stdin) == NULL) {
        (void) putchar('\n');
        errcode = KRB5_LIBOS_CANTREADPWD;
        goto cleanup;
    }
    (void) putchar('\n');

    if ((ptr = strchr(password, '\n')))
        *ptr = '\0';
    else /* need to flush */
        do {
            scratchchar = getchar();
        } while (scratchchar != EOF && scratchchar != '\n');

    if (prompt2) {
        if (! (tmpstr = (char *)malloc(*pwsize))) {
            errcode = ENOMEM;
            goto cleanup;
        }
        (void) fputs(prompt2, stdout);
        (void) fflush(stdout);
        if (fgets(tmpstr, *pwsize, stdin) == NULL) {
            (void) putchar('\n');
            errcode = KRB5_LIBOS_CANTREADPWD;
            goto cleanup;
        }
        (void) putchar('\n');

        if ((ptr = strchr(tmpstr, '\n')))
            *ptr = '\0';
        else /* need to flush */
            do {
                scratchchar = getchar();
            } while (scratchchar != EOF && scratchchar != '\n');

        if (strncmp(password, tmpstr, *pwsize)) {
            errcode = KRB5_LIBOS_BADPWDMATCH;
            goto cleanup;
        }
    }

cleanup:
    (void) SetConsoleMode(handle, old_mode);
    if (tmpstr) {
        (void) memset(tmpstr, 0, *pwsize);
        (void) free(tmpstr);
    }
    if (errcode)
        (void) memset(password, 0, *pwsize);
    else
        *pwsize = strlen(password);
    return errcode;
}
#endif

static int CALLBACK
read_pwd_proc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    pwd_params *dp;

    switch(msg) {
    case WM_INITDIALOG:
        dp = (pwd_params *) lParam;
        SetWindowLongPtr(hdlg, DWLP_USER, lParam);
        SetDlgItemText(hdlg, ID_READ_PWD_PROMPT, dp->pwd_prompt);
        SetDlgItemText(hdlg, ID_READ_PWD_PROMPT2, dp->pwd_prompt2);
        SetDlgItemText(hdlg, ID_READ_PWD_PWD, "");
        center_dialog(hdlg);
        return TRUE;

    case WM_COMMAND:
        dp = (pwd_params *) GetWindowLongPtr(hdlg, DWLP_USER);
        switch (wParam) {
        case IDOK:
            *(dp->pwd_size_return) =
                GetDlgItemText(hdlg, ID_READ_PWD_PWD,
                               dp->pwd_return_pwd, *(dp->pwd_size_return));
            EndDialog(hdlg, TRUE);
            break;

        case IDCANCEL:
            memset(dp->pwd_return_pwd, 0 , *(dp->pwd_size_return));
            *(dp->pwd_size_return) = 0;
            EndDialog(hdlg, FALSE);
            break;
        }
        return TRUE;

    default:
        return FALSE;
    }
}

krb5_error_code KRB5_CALLCONV
krb5_read_password(context, prompt, prompt2, return_pwd, size_return)
    krb5_context context;
    const char *prompt;
    const char *prompt2;
    char *return_pwd;
    int *size_return;
{
    DLGPROC dlgproc;
    HINSTANCE hinst;
    pwd_params dps;
    int rc;

#ifdef _WIN32
    if (_isatty(_fileno(stdin)))
        return(read_console_password
               (context, prompt, prompt2, return_pwd, size_return));
#endif

    dps.pwd_prompt = prompt;
    dps.pwd_prompt2 = prompt2;
    dps.pwd_return_pwd = return_pwd;
    dps.pwd_size_return = size_return;

    hinst = get_lib_instance();
#ifdef _WIN32
    dlgproc = read_pwd_proc;
#else
    dlgproc = (FARPROC) MakeProcInstance(read_pwd_proc, hinst);
#endif
    rc = DialogBoxParam(hinst, MAKEINTRESOURCE(ID_READ_PWD_DIALOG), 0,
                        dlgproc, (LPARAM) &dps);
#ifndef _WIN32
    FreeProcInstance ((FARPROC) dlgproc);
#endif
    return 0;
}
#endif

#ifndef DEFINED_KRB5_READ_PASSWORD
#define DEFINED_KRB5_READ_PASSWORD
/*
 * Don't expect to be called, just define it for sanity and the linker.
 */
krb5_error_code KRB5_CALLCONV
krb5_read_password(context, prompt, prompt2, return_pwd, size_return)
    krb5_context context;
    const char *prompt;
    const char *prompt2;
    char *return_pwd;
    int *size_return;
{
    *size_return = 0;
    return KRB5_LIBOS_CANTREADPWD;
}
#endif
