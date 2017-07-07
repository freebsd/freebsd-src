/*
 * Copyright (C) 1997 Cygnus Solutions.
 *
 * Author:  Michael Graff
 */

#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "krb5.h"

#include "vardlg.h"
#include "gic.h"

/*
 * Steps performed:
 *
 *   1)  Create the dialog with all the windows we will need
 *	 later.  This is done by calling vardlg_build() from
 *	 gic_prompter().
 *
 *   2)  Run the dialog from within gic_prompter().  If the return
 *	 value of the dialog is -1 or IDCANCEL, return an error.
 *	 Otherwise, return success.
 *
 *   3)  From within the dialog initialization code, call
 *	 vardlg_config(), which will:
 *
 *	a)  Set all the label strings in all the entry labels and
 *	    the banner.
 *
 *	b)  Set the maximum input lengths on the entry fields.
 *
 *	c)  Calculate the size of the text used within the banner.
 *
 *	d)  Calculate the longest string of text used as a label.
 *
 *	e)  Resize each label and each entry within the dialog
 *	    to "look nice."
 *
 *	f)  Place the OK and perhaps the Cancel buttons at the bottom
 *	    of the dialog.
 *
 *   4)  When the OK button is clicked, copy all the values from the
 *	 input fields and store them in the pointers we are given.
 *	 Also, set the actual lengths to what we collected from the
 *	 entries.  Finally, call EndDialog(IDOK) to end the dialog.
 */

/*
 * Yes, a global.  It is a PITA to not use them in windows.
 */
gic_data *gd;


/*
 * initialize the dialog
 */
static BOOL
gic_dialog_init(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
	vardlg_config(hwnd, gd->width, gd->banner, gd->num_prompts,
		gd->prompts, (WORD)(gd->id));

	return FALSE;
}

/*
 * process dialog "commands"
 */
static void
gic_dialog_command(HWND hwnd, int cid, HWND hwndCtl, UINT codeNotify)
{

	int n;
	WORD id;

	/*
	 * We are only interested in button clicks, and then only of
	 * type IDOK or IDCANCEL.
	 */
	if (codeNotify != BN_CLICKED)
		return;
	if (cid != IDOK && cid != IDCANCEL)
		return;

	/*
	 * If we are canceled, wipe all the fields and return IDCANCEL.
	 */
	if (cid == IDCANCEL) {
		EndDialog(hwnd, IDCANCEL);
		return;
	}

	/*
	 * must be IDOK...
	 */
	id = (gd->id + 2);
	for (n = 0 ; n < gd->num_prompts ; n++) {
		Edit_GetText(GetDlgItem(hwnd, id), gd->prompts[n].reply->data,
			gd->prompts[n].reply->length);
		gd->prompts[n].reply->length = (unsigned)strlen(gd->prompts[n].reply->data);
		id += 2;
	}

	EndDialog(hwnd, IDOK);
}

/*
 * The dialog callback.
 */
static INT_PTR CALLBACK
gic_dialog(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		HANDLE_MSG(hwnd, WM_INITDIALOG, gic_dialog_init);

		HANDLE_MSG(hwnd, WM_COMMAND, gic_dialog_command);
	}

	return FALSE;
}


/*
 * All the disgusting code to use the get_init_creds() functions in a
 * broken environment
 */
krb5_error_code KRB5_CALLCONV
gic_prompter(krb5_context ctx, void *data, const char *name,
	     const char *banner, int num_prompts, krb5_prompt prompts[])
{
	int       rc;
	void     *dlg;

	gd = data;

	gd->banner = banner;
	gd->num_prompts = num_prompts;
	gd->prompts = prompts;
	if (gd->width == 0)
		gd->width = 450;

	dlg = vardlg_build((WORD)(gd->width), name, gd->banner,
			   (WORD)num_prompts, prompts, (WORD)(gd->id));

	rc = DialogBoxIndirect(gd->hinstance, (LPDLGTEMPLATE)dlg, gd->hwnd, gic_dialog);

	if (rc != IDOK)
		return 1;

	return 0;
}
