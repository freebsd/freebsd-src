/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: register.c,v 1.1.2.6 1997/05/26 04:57:42 jkh Exp $
 *
 * Copyright (c) 1997
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <ctype.h>

#define REGISTER_HELPFILE	"register"
#define REGISTRATION_FNAME	"/new-registration"
#define REGISTRATION_ADDRESS	"register@freebsd.org"
#define MAJORDOMO_ADDRESS	"majordomo@freebsd.org"

#define FIRSTNAME_FIELD_LEN	25
#define LASTNAME_FIELD_LEN	30
#define EMAIL_FIELD_LEN		61
#define ADDRESS_FIELD_LEN	160
#define CITY_FIELD_LEN		20
#define STATE_FIELD_LEN		15
#define ZIP_FIELD_LEN		15

static char firstname[FIRSTNAME_FIELD_LEN], lastname[LASTNAME_FIELD_LEN],
    email[EMAIL_FIELD_LEN], address[ADDRESS_FIELD_LEN],
    city[CITY_FIELD_LEN], state[STATE_FIELD_LEN], zip[ZIP_FIELD_LEN];

static int	okbutton, cancelbutton;

/* What the screen size is meant to be */
#define REGISTER_DIALOG_Y		0
#define REGISTER_DIALOG_X		2
#define REGISTER_DIALOG_WIDTH		COLS - 4
#define REGISTER_DIALOG_HEIGHT		LINES - 2

static Layout layout[] = {
#define LAYOUT_LASTNAME		0
    { 1, 2, LASTNAME_FIELD_LEN - 1, LASTNAME_FIELD_LEN - 1,
      "Last Name:", "Your surname (family name) or company name should go here.",
      lastname, STRINGOBJ, NULL },
#define LAYOUT_FIRSTNAME	1
    { 1, 36, FIRSTNAME_FIELD_LEN - 1, FIRSTNAME_FIELD_LEN - 1,
      "First Name:", "Your given name or a contact name if registering for a company.",
      firstname, STRINGOBJ, NULL },
#define LAYOUT_EMAIL		2
    { 6, 2, EMAIL_FIELD_LEN - 1, EMAIL_FIELD_LEN - 1,
      "EMail Address:",
      "Where you'd like any announcement email sent, e.g. bsdmail@someplace.com",
      email, STRINGOBJ, NULL },
#define LAYOUT_ADDRESS		3
    { 10, 2, 60, ADDRESS_FIELD_LEN - 1,
      "Street address:", "Your street address, all in one line (optional).",
      address, STRINGOBJ, NULL },
#define LAYOUT_CITY		4
    { 14, 2, CITY_FIELD_LEN - 1, CITY_FIELD_LEN - 1,
      "City:", "Your city name (optional)",
      city, STRINGOBJ, NULL },
#define LAYOUT_STATE		5
    { 14, 26, STATE_FIELD_LEN - 1, STATE_FIELD_LEN - 1,
      "State / Province:",
      "Your local state or province.",
      state, STRINGOBJ, NULL },
#define LAYOUT_ZIP		5
    { 14, 50, ZIP_FIELD_LEN - 1, ZIP_FIELD_LEN - 1,
      "Zip / Country Code:",
      "Your U.S. Zip code or International country code (optional).",
      zip, STRINGOBJ, NULL },
#define LAYOUT_OKBUTTON		7
    { 18, 20, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	8
    { 18, 40, 0, 0,
      "CANCEL", "Select this if you wish to cancel this registration",
      &cancelbutton, BUTTONOBJ, NULL },
    { NULL },
};

/* Submenu selections */
#define COMMERCE_MAIL	0
#define COMMERCE_EMAIL	1
#define ANNOUNCE_LIST	2
#define NEWSLETTER	3

static struct { int y, x, sel; char *desc, *allowed; } hotspots[] = {
    { 5, 35, 0, "Do you wish to receive FreeBSD [ONLY!] related commercial mail?",         "Y" },
    { 5, 57, 0, "Do you wish to receive FreeBSD [ONLY!] related commercial email?",        "Y" },
    { 6, 35, 0, "Sign up (with majordomo@FreeBSD.org) for important announcements?",       "Y" },
    { 10, 35, 0, "Sign up for the FreeBSD Newsletter?  P = Postal (paper) copy, E = Email", "PE" },
};

/* Check the accuracy of user's choices before letting them move on */
static int
verifySettings(void)
{
    if (!lastname[0]) {
	msgConfirm("Missing last name / company name field.");
	return 0;
    }
    else if (email[0] && !index(email, '@'))
	return !msgYesNo("Hmmmm, this email address has no `@' in it.  Are you\n"
			 "sure that %s is a valid address?");
    else if (address[0] && !city[0]) {
	msgConfirm("Missing City name.");
	return 0;
    }
    else if (!email[0] && (hotspots[COMMERCE_EMAIL].sel || hotspots[NEWSLETTER].sel == 2)) {
	msgConfirm("You've signed up to receive commercial email or the newsletter by\n"
		   "email but have no email address specified!");
	return 0;
    }
    else if (!address[0] && (hotspots[COMMERCE_MAIL].sel || hotspots[NEWSLETTER].sel == 1)) {
	msgConfirm("You've signed up to receive commercial mail or the newsletter by\n"
		   "post but have no postal address specified!");
	return 0;
    }
    return 1;
}

/* Do the actual work of mailing out the registration once all is filled in */
static void
handle_registration(void)
{
    FILE *fp;
    WINDOW *save = savescr();

    dialog_clear_norefresh();
    (void)unlink(REGISTRATION_FNAME);
    fp = fopen(REGISTRATION_FNAME, "w");
    if (!fp) {
	msgConfirm("Unable to open %s for the new registration.\n"
		   "That's pretty bad!  Please fix whatever's wrong\n"
		   "and try this registration again.");
	restorescr(save);
	return;
    }
    fprintf(fp, "<entry>\n");
    fprintf(fp, "<first>%s</first>\n", firstname);
    fprintf(fp, "<last>%s</last>\n", lastname);
    fprintf(fp, "<email>%s</email>\n", email);
    fprintf(fp, "<address>%s</address>\n", address);
    fprintf(fp, "<city>%s</city>\n", city);
    fprintf(fp, "<state>%s</state>\n", state);
    fprintf(fp, "<zip>%s</zip>\n", zip);
    fprintf(fp, "<options commerce_email=\"%s\" commerce_mail=\"%s\" announce=\"%s\" newsletter=\"%s\"></options>\n",
	    hotspots[COMMERCE_EMAIL].sel ? "yes" : "no", hotspots[COMMERCE_MAIL].sel ? "yes" : "no",
	    hotspots[ANNOUNCE_LIST].sel ? "yes" : "no",
	    hotspots[NEWSLETTER].sel == 0 ? "no" : hotspots[NEWSLETTER].sel == 1 ? "postal" : "email"); 
    fprintf(fp, "<version>%s</version>\n", RELEASE_NAME);
    fprintf(fp, "</entry>\n");
    fclose(fp);
    dialog_clear_norefresh();
    if (!msgYesNo("Do you have a working network connection and outgoing email\n"
		  "enabled at this time?  I need to be able to reach freebsd.org\n"
		  "in order to submit your registration.")) {
	dialog_clear_norefresh();
	if (!vsystem("mail %s < %s", REGISTRATION_ADDRESS, REGISTRATION_FNAME)) {
	    msgConfirm("Thank you!  Your registration has been sent in successfully.\n");
	    (void)unlink(REGISTRATION_FNAME);
	}
	else {
	    msgConfirm("Argh!  The mail program returned a bad status - there\n"
		       "must be something still not quite configured correctly.\n"
		       "leaving the registration in: %s\n"
		       "When you're connected to the net and ready to send it,\n"
		       "simply type:  mail %s < %s\n", REGISTRATION_ADDRESS, REGISTRATION_FNAME,
		       REGISTRATION_FNAME);
	}
	if (hotspots[ANNOUNCE_LIST].sel) {
	    char *cp;
	    
	    dialog_clear_norefresh();
	    cp = msgGetInput(email, "What email address would you like to subscribe under?\n"
			     "This is a fairly low-traffic mailing list and only generates\n"
			     "around 5 messages a month, so it's also safe to receive at your\n"
			     "standard email address.");
	    if (!cp)
		msgConfirm("OK, I won't subscribe to announce at this time.  To do it manually\n"
			   "yourself, simply send mail to %s.", MAJORDOMO_ADDRESS);
	    else {
		dialog_clear_norefresh();
		if (!vsystem("echo subscribe freebsd-announce %s | mail %s", email, MAJORDOMO_ADDRESS))
		    msgConfirm("Your request to join the announce mailing list has been sent.\n"
			      "you should receive notification back in 24 hours or less, otherwise\n"
			      "something has gone wrong and you should try this again by sending\n"
			      "a message to %s which contains the line:\n\n"
			      "subscribe freebsd-announce %s\n", MAJORDOMO_ADDRESS, email);
		else
		    msgConfirm("Argh!  The mail program returned a bad status - there\n"
			       "must be something still not quite configured correctly.\n"
			       "Please fix this then try again by sending a message to\n"
			       "to %s which contains the line:\n\n"
			       "subscribe freebsd-announce %s\n", MAJORDOMO_ADDRESS, email);
	    }
	}
    }
    else {
	dialog_clear_norefresh();
	msgConfirm("OK, your registration has been left in the file %s\n"
		   "When you're connected to the net and ready to send it,\n"
		   "simply type:  mail %s < %s\n", REGISTRATION_FNAME,
		   REGISTRATION_ADDRESS, REGISTRATION_FNAME);
    }
    restorescr(save);
}

/* Put up a subdialog for the registration options */
static void
subdialog(WINDOW *win)
{
    int i, j, attrs;
    char help_line[80];

    attrs = getattrs(win);
    mvwaddstr(win, hotspots[COMMERCE_MAIL].y, hotspots[COMMERCE_MAIL].x - 1, "[ ] Postal Adverts");
    mvwaddstr(win, hotspots[COMMERCE_EMAIL].y, hotspots[COMMERCE_EMAIL].x - 1, "[ ] Email Adverts");
    mvwaddstr(win, hotspots[ANNOUNCE_LIST].y, hotspots[ANNOUNCE_LIST].x - 1,
	      "[ ] The announce@FreeBSD.ORG mailing list.");
    mvwaddstr(win, hotspots[NEWSLETTER].y, hotspots[NEWSLETTER].x - 1, "[ ] The FreeBSD Newsletter.");
    /* Tack up the initial values */
    for (i = 0; i < sizeof(hotspots) / sizeof(hotspots[0]); i++) {
	wattrset(win, attrs | A_BOLD);
	mvwaddch(win, hotspots[i].y, hotspots[i].x, hotspots[i].sel ? hotspots[i].allowed[hotspots[i].sel - 1] : 'N');
    }
    wattrset(win, attrs);
    wrefresh(win);

    for (i = 0; i < sizeof(hotspots) / sizeof(hotspots[0]);) {
	int ch, len = strlen(hotspots[i].desc);
	char *cp;

	/* Display the help line at the bottom of the screen */
	for (j = 0; j < 79; j++)
	    help_line[j] = (j < len) ? hotspots[i].desc[j] : ' ';
	help_line[j] = '\0';
	use_helpline(help_line);
	display_helpline(win, LINES - 1, COLS - 1);
	wmove(win, hotspots[i].y, hotspots[i].x);
	wrefresh(win);
	switch(ch = toupper(getch())) {
	case KEY_UP:
		if (i)
		    i--;
		continue;

	case KEY_DOWN:
	case '\011':	/* TAB */
	case '\012':	/* ^J */
	case '\014':	/* ^M */
	    /* Treat as a no-change op */
	    ++i;
	    break;

	case 'N':	/* No is generic to all */
	    hotspots[i].sel = 0;
	    wattrset(win, attrs | A_BOLD);
	    mvwaddch(win, hotspots[i].y, hotspots[i].x, 'N');
	    wattrset(win, attrs);
	    wrefresh(win);
	    ++i;
	    break;

	default:
	    cp = index(hotspots[i].allowed, ch);
	    if (cp) {
		hotspots[i].sel = (cp - hotspots[i].allowed) + 1;
		wattrset(win, attrs | A_BOLD);
		mvwaddch(win, hotspots[i].y, hotspots[i].x, *cp);
		wattrset(win, attrs);
		wrefresh(win);
		++i;
	    }
	    else
		beep();
	    break;
	}
    }
}

/* Register a user */
int
registerOpenDialog(void)
{
    WINDOW              *ds_win, *save = savescr();
    ComposeObj          *obj = NULL;
    int                 n = 0, cancel = FALSE;
    int			max, ret = DITEM_SUCCESS;

    dialog_clear_norefresh();
    /* We need a curses window */
    if (!(ds_win = openLayoutDialog(REGISTER_HELPFILE, " FreeBSD Registration Form:  Press F1 for Help / General Info ",
				    REGISTER_DIALOG_X, REGISTER_DIALOG_Y,
				    REGISTER_DIALOG_WIDTH, REGISTER_DIALOG_HEIGHT))) {
	beep();
	msgConfirm("Cannot open registration dialog window!!");
	restorescr(save);
	return DITEM_FAILURE;
    }

    /* Some more initialisation before we go into the main input loop */
    obj = initLayoutDialog(ds_win, layout, REGISTER_DIALOG_X, REGISTER_DIALOG_Y, &max);

reenter:
    cancelbutton = okbutton = 0;
    while (layoutDialogLoop(ds_win, layout, &obj, &n, max, &cancelbutton, &cancel)) {
	if (n == LAYOUT_ADDRESS)
	    subdialog(ds_win);
    }
    
    if (!cancel && !verifySettings())
	goto reenter;

    /* OK, we've got a valid registration, now push it out */
    if (!cancel)
	handle_registration();

    /* Clear this crap off the screen */
    delwin(ds_win);
    dialog_clear_norefresh();
    use_helpfile(NULL);

    restorescr(save);
    return ret;
}
