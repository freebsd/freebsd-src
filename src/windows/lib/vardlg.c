/*
 * Copyright (C) 1997 Cygnus Solutions.
 *
 * Author:  Michael Graff
 */
/*
 * Dialog box building for various numbers of (label, entry) fields.
 *
 * This code is somewhat hardcoded to build boxes for the krb5_get_init_creds()
 * function.
 */

#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "krb5.h"
#include "vardlg.h"

/*
 * a hack, I know...  No error checking below, either.
 */
static unsigned char dlg[DLG_BUF];

/*
 * Add a WORD (16-bit int) to the buffer.  Return the number of characters
 * added.
 */
static int
ADD_WORD(unsigned char *p, WORD w)
{
	*((WORD *)p) = w;

	return 2;
}

static int
ADD_DWORD(unsigned char *p, DWORD dw)
{
	*((DWORD *)p) = dw;

	return 4;
}

static size_t
ADD_UNICODE_STRING(unsigned char *p, const char *s)
{
	WORD *w;
	size_t i;
	size_t len;

	w = (WORD *)p;

	len = strlen(s) + 1; /* copy the null, too */

	for (i = 0 ; i < len ; i++)
		*w++ = *s++;

	return (len * 2);
}

#define DWORD_ALIGN(p) { while ((DWORD)p % 4) *p++ = 0x00; }

static size_t
ADD_DLGTEMPLATE(unsigned char *dlg, short x, short y, short cx, short cy,
		const char *caption, const char *fontname, WORD fontsize,
		WORD n)
{
	unsigned char    *p;
	DLGTEMPLATE       dlt;

	p = dlg;

	dlt.style = (DS_MODALFRAME | WS_POPUP);
	if (caption != NULL)
		dlt.style |= WS_CAPTION;
	if (fontname != NULL)
		dlt.style |= DS_SETFONT;
	dlt.dwExtendedStyle = 0;
	dlt.cdit = n;
	dlt.x = x;
	dlt.y = y;
	dlt.cx = cx;
	dlt.cy = cy;
	memcpy(p, &dlt, sizeof(dlt));
	p += sizeof(dlt);

	p += ADD_WORD(p, 0x0000);  /* menu == none */

	p += ADD_WORD(p, 0x0000);  /* class == default? */

	if (caption != NULL)
		p += ADD_UNICODE_STRING(p, caption);
	else
		p += ADD_WORD(p, 0x0000);

	if (fontname != NULL) {
		p += ADD_WORD(p, fontsize);
		p += ADD_UNICODE_STRING(p, fontname);
	}

	DWORD_ALIGN(p);

	return (p - dlg);
}

static size_t
ADD_DLGITEM(unsigned char *dlg, short x, short y, short cx, short cy,
	    const char *label, WORD id, WORD type, DWORD style)
{
	unsigned char    *p;
	DLGITEMTEMPLATE   dit;

	p = dlg;

	dit.style = style;
	dit.dwExtendedStyle = 0;
	dit.x = x;
	dit.y = y;
	dit.cx = cx;
	dit.cy = cy;
	dit.id = id;
	memcpy(p, &dit, sizeof(dit));
	p += sizeof(dit);

	p += ADD_WORD(p, 0xffff);
	p += ADD_WORD(p, type);

	p += ADD_UNICODE_STRING(p, label);

	/*
	 * creation data? For now, just make this empty, like the resource
	 * compiler does.
	 */
	p += ADD_WORD(p, 0x0000);

	DWORD_ALIGN(p);

	return (p - dlg);
}

#define ADD_DLGITEM_defpushbutton(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0080, 0x50010001);

#define ADD_DLGITEM_pushbutton(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0080, 0x50010000);

#define ADD_DLGITEM_left_static(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0082, 0x50020000);

#define ADD_DLGITEM_centered_static(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0082, 0x50020001);

#define ADD_DLGITEM_right_static(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0082, 0x50020002);

#define ADD_DLGITEM_entry(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0081, 0x50810080);

#define ADD_DLGITEM_hidden_entry(a, b, c, d, e, f, g) \
	ADD_DLGITEM((a), (b), (c), (d), (e), (f), (g), 0x0081, 0x508100a0);


/*
 * "build" the dialog box.  In this bit of code, we create the dialog box,
 * create the OK button, and a static label for the banner text.
 *
 * If there are items, we also create a Cancel button and one (label, entry)
 * fields for each item.
 */
void *
vardlg_build(WORD cx, const char *name, const char *banner,
	     WORD n, krb5_prompt prompts[], WORD id)
{
	unsigned char *p;
	WORD i;

	p = dlg;  /* global */

	if (cx < MIN_WIDTH)
		cx = MIN_WIDTH;
	if (cx > MAX_WIDTH)
		cx = MAX_WIDTH;

	/*
	 * Store the dialog template
	 */
	p += ADD_DLGTEMPLATE(p, 0, 0, cx, 0, name ?
			     strlen(name) < 30 ? name : "Kerberos V5" :
			     "Kerberos V5",
			     "MS Sans Serif", 8,
			     (WORD)(n * 2 + 3));

	/*
	 * Create a label for the banner.  This will be ID (id).
	 */
	p += ADD_DLGITEM_left_static(p, 0, 0, 0, 0, "", id++);

	/*
	 * Each label field is ID (id + 1) + (item * 2), and each entry field
	 * is (id + 2) + (item * 2)
	 */
	for (i = 0 ; i < n ; i++) {
		p += ADD_DLGITEM_right_static(p, 0, 0, 0, 0, "", id++);
		if (prompts[i].hidden) {
			p += ADD_DLGITEM_hidden_entry(p, 0, 0, 0, 0, "", id++);
		} else {
			p += ADD_DLGITEM_entry(p, 0, 0, 0, 0, "", id++);
		}
	}

	/*
	 * Create the OK and Cancel buttons.
	 */
	p += ADD_DLGITEM_defpushbutton(p, 0, 0, 0, 0,
				       "OK", IDOK);
	if (n != 0)
		p += ADD_DLGITEM_pushbutton(p, 0, 0, 0, 0,
					    "Cancel", IDCANCEL);

	return dlg;
}

#define SPACE_Y     4  /* logical units */
#define SPACE_X     4  /* logical units */
#define ENTRY_PX  120  /* pixels */
#define BUTTON_PX  70  /* pixels */
#define BUTTON_PY  30  /* pixels */

void
vardlg_config(HWND hwnd, WORD width, const char *banner, WORD num_prompts,
	      krb5_prompt *prompts, WORD id)
{
	int         n;
	WORD        cid;
	HDC         hdc;
	SIZE        csize;
	SIZE        maxsize;
	LONG        cx, cy;
	LONG        ccx, ccy;
	LONG        space_x, space_y;
	LONG        max_x, max_y;
	LONG        banner_y;
	RECT        rect;
	int         done;
	const char *p;

	/*
	 * First, set the banner's text.
	 */
	Static_SetText(GetDlgItem(hwnd, id), banner);

	/*
	 * Next, run through the items and set their static text.
	 * Also, set the corresponding edit string and set the
	 * maximum input length.
	 */
	cid = (id + 1);

	for (n = 0 ; n < num_prompts ; n++) {
		Static_SetText(GetDlgItem(hwnd, cid), prompts[n].prompt);
		cid++;
		Edit_SetText(GetDlgItem(hwnd, cid), "");
		Edit_LimitText(GetDlgItem(hwnd, cid), prompts[n].reply->length);
		cid++;
	}

	/*
	 * Now run through the entry fields and find the longest string.
	 */
	maxsize.cx = maxsize.cy = 0;
	cid = (id + 1);
	hdc = GetDC(GetDlgItem(hwnd, cid)); /* assume one label is the same as all the others */

	for (n = 0 ; n < num_prompts ; n++) {
		GetTextExtentPoint32(hdc, prompts[n].prompt, (int)strlen(prompts[n].prompt), &csize);
		if (csize.cx > maxsize.cx)
			maxsize.cx = csize.cx;
		if (csize.cy > maxsize.cy)
			maxsize.cy = csize.cy;
	}

#if 0
	/*
	 * convert the maximum values into pixels.  Ugh.
	 */
	rect.left = 0;
	rect.top = 0;
	rect.right = maxsize.cx;
	rect.bottom = maxsize.cy;
	MapDialogRect(hwnd, &rect);

	max_x = rect.right;
	max_y = rect.bottom;
#else
	max_x = maxsize.cx;
	max_y = (long)(((double)maxsize.cy) * 1.5);
#endif

	/*
	 * convert the spacing values, too.  Ugh.  Ugh.
	 */
	rect.left = 0;
	rect.top = 0;
	rect.right = SPACE_X;
	rect.bottom = SPACE_Y;
	MapDialogRect(hwnd, &rect);

	space_x = rect.right;
	space_y = rect.bottom;

	/*
	 * Now we know the maximum length of the string for the entry labels.  Guestimate
	 * that the entry fields should be ENTRY_PX pixels long and resize the dialog
	 * window to fit the longest string plus the entry fields (plus a little for the
	 * spacing between the edges of the windows and the static and edit fields, and
	 * between the static and edit fields themselves.)
	 */
	cx = max_x + ENTRY_PX + (space_x * 3);
	cy = (max_y + space_y) * num_prompts;

	/*
	 * resize the dialog box itself (take 1)
	 */
	SetWindowPos(hwnd, HWND_TOPMOST,
		0, 0,
		cx + 10, cy + 30,
		SWP_NOMOVE);

	/*
	 * position the dialog items.  First, the banner. (take 1)
	 */
	SetWindowPos(GetDlgItem(hwnd, id), HWND_BOTTOM,
		space_x, space_y,
		(cx - space_x * 2), max_y,
		0);

	/*
	 * Now that the window for the banner is in place, convert the width into logical units
	 * and find out how many lines we need to reserve room for.
	 */
	done = 0;
	p = banner;
	banner_y = 0;

	do {
		int nFit;
		int pDx[128];

		hdc = GetDC(GetDlgItem(hwnd, id));

		GetTextExtentExPoint(hdc, p, (int)strlen(p), cx, &nFit,
			pDx, &csize);

		banner_y += csize.cy;

		p += nFit;

	} while (*p != 0);

	banner_y += space_y;

	/*
	 * position the banner (take 2)
	 */
	SetWindowPos(GetDlgItem(hwnd, id), HWND_BOTTOM,
		space_x, space_y,
		(cx - space_x * 2), banner_y,
		0);

	/*
	 * Don't forget to include the banner estimate and the buttons, too.  Once again,
	 * assume the buttons are BUTTON_PY pixels high.  The extra three space_y's are
	 * for between the top of the dialog and the banner, between the banner and the
	 * first label, and between the buttons and the bottom of the screen.
	 */
	cy += banner_y + BUTTON_PY + (space_y * 3);

	/*
	 * resize the dialog box itself (Again...  ugh!)
	 */
	SetWindowPos(hwnd, HWND_TOPMOST,
		0, 0,
		cx + 10, cy + 30,
		SWP_NOMOVE);

	cid = (id + 1);
	ccy = banner_y + (space_y * 2);
	ccx = max_x + (space_x * 2);  /* where the edit fields start */

	for (n = 0 ; n < num_prompts ; n++) {
		SetWindowPos(GetDlgItem(hwnd, cid), HWND_BOTTOM,
			space_x, ccy,
			max_x, max_y, 0);
		cid++;
		SetWindowPos(GetDlgItem(hwnd, cid), HWND_BOTTOM,
			ccx, ccy,
			ENTRY_PX, max_y - 3, 0);
		cid++;
		ccy += (max_y + space_y);
	}

	/*
	 * Now the buttons.  If there are any entries we will have both an OK and a
	 * Cancel button.  If we don't have any entries, we will have only an OK.
	 */
	if (num_prompts == 0) {
		SetWindowPos(GetDlgItem(hwnd, IDOK), HWND_BOTTOM,
			(cx / 2), cy - space_y - BUTTON_PY,
			BUTTON_PX, BUTTON_PY, 0);
	} else {
		SetWindowPos(GetDlgItem(hwnd, IDOK), HWND_BOTTOM,
			space_x, cy - space_y - BUTTON_PY,
			BUTTON_PX, BUTTON_PY, 0);
		SetWindowPos(GetDlgItem(hwnd, IDCANCEL), HWND_BOTTOM,
			cx - space_x - BUTTON_PX, cy - space_y - BUTTON_PY,
			BUTTON_PX, BUTTON_PY, 0);
	}

	return;
}

/*
 * To use these functions, first create the dialog box and entries.
 * You will always get an OK button.  If there are at least one item,
 * you will also get a cancel button.  The OK button is IDOK, and the cancel
 * button is IDCANCEL, as usual.
 *
 * After calling bld_dlg, the banner will have ID "id", and the labels
 * will be "1 + id + i * 2" (i is the entry number, starting with zero) and
 * the entries will be "2 + id + i * 2".
 *
 *	unsigned char *dlg = vardlg_build(minwidth, banner, num_prompts,
 *					  krb5_prompt[], id);
 *
 * Then, "run" the dialog using:
 *
 *	rc = DialogBoxIndirect(hinstance, (LPDLGTEMPLATE)dlg,
 *			       HWND_DESKTOP, myDialogProc);
 *
 * Note that the vardlg_build function uses a static data area and so cannot
 * be used more than once before the DialogBoxIndirect() procedure is called.
 * I assume windows won't need that area after that call is complete.
 *
 * In the dialog's _initialization_ procedure, call
 *
 *	vardlg_config(hwnd, banner, num_prompts, krb5_prompt[], id);
 *
 * This function will resize the various elements of the dialog and fill in the
 * labels.
 */
