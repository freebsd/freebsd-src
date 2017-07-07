/*
 * Copyright (C) 1997 Cygnus Solutions
 *
 * Author:  Michael Graff
 */

#ifndef _WINDOWS_LIB_VARDLG_H
#define _WINDOWS_LIB_VARDLG_H

#include <windows.h>
#include <windowsx.h>

#define DLG_BUF 4096

/*
 * The minimum and maximum dialog box widths we will allow.
 */
#define MIN_WIDTH 350
#define MAX_WIDTH 600

/*
 * "build" the dialog box.  In this bit of code, we create the dialog box,
 * create the OK button, and a static label for the banner text.
 *
 * If there are items, we also create a Cancel button and one (label, entry)
 * fields for each item.
 */
void *vardlg_build(WORD, const char *, const char *, WORD, krb5_prompt *, WORD);

void  vardlg_config(HWND, WORD, const char *, WORD, krb5_prompt *, WORD);

#endif /* _WINDOWS_LIB_VARDLG_H */
