/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: doc.c,v 1.3 1995/10/22 01:32:42 jkh Exp $
 *
 * Jordan Hubbard
 *
 * My contributions are in the public domain.
 *
 * Parts of this file are also blatently stolen from Poul-Henning Kamp's
 * previous version of sysinstall, and as such fall under his "BEERWARE license"
 * so buy him a beer if you like it!  Buy him a beer for me, too!
 * Heck, get him completely drunk and send me pictures! :-)
 */

#include "sysinstall.h"

/*
 * This is called from the main menu.  Try to find a copy of Lynx from somewhere
 * and fire it up on the first copy of the handbook we can find.
 */
int
docBrowser(char *junk)
{
    char *browser = variable_get(VAR_BROWSER_PACKAGE);
 
    /* Make sure we were started at a reasonable time */
    if (!strcmp(variable_get(SYSTEM_STATE), "init")) {
	dialog_clear();
	msgConfirm("Sorry, it's not possible to invoke the browser until the system\n"
		   "is installed completely enough to support a copy of %s.", browser);
	return RET_FAIL;
    }

    if (!mediaVerify())
	return RET_FAIL;

    /* First, make sure we have whatever browser we've chosen is here */
    if (package_add(browser) != RET_SUCCESS) {
	dialog_clear();
	msgConfirm("Unable to install the %s HTML browser package.  You may\n"
		   "wish to verify that your media is configured correctly and\n"
		   "try again.", browser);
	return RET_FAIL;
    }
    if (!file_executable(variable_get(VAR_BROWSER_BINARY))) {
	dialog_clear();
	if (!msgYesNo("Hmmm.  The %s package claims to have installed, but I can't\n"
		      "find its binary in %s!  You may wish to try a different\n"
		      "location to load the package from (go to Media menu) and see if that\n"
		      "makes a difference.\n\n"
		      "I suggest that we remove the version that was extracted since it does\n"
		      "not appear to be correct.   Would you like me to do that now?"))
	    vsystem("pkg_delete %s %s", !strcmp(variable_get(VAR_CPIO_VERBOSITY), "high") ? "-v" : "", browser);
	return RET_FAIL;
    }

    /* Run browser on the appropriate doc */
    dmenuOpenSimple(&MenuHTMLDoc);
    return RET_SUCCESS;
}

/* Try to show one of the documents requested from the HTML doc menu */
int
docShowDocument(char *str)
{
    char *browser = variable_get(VAR_BROWSER_BINARY);

    if (!file_executable(browser)) {
	dialog_clear();
	msgConfirm("Can't find the browser in %s!  Please ensure that it's\n"
		   "properly set in the Options editor.", browser);
	return RET_FAIL;
    }
    if (!strcmp(str, "Home"))
	vsystem("%s http://www.freebsd.org", browser);
    else if (!strcmp(str, "Other")) {
    }
    else {
	char target[512];

	sprintf(target, "/usr/share/doc/%s/%s.html", str, str);
	if (file_readable(target))
	    vsystem("%s file:%s", browser, target);
	else
	    vsystem("%s http://www.freebsd.org/%s");
    }
    return RET_SUCCESS;
}
