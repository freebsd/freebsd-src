/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: doc.c,v 1.8 1995/10/27 03:59:31 jkh Exp $
 *
 * Jordan Hubbard
 *
 * My contributions are in the public domain.
 *
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

    if (!strstr(variable_get(SYSTEM_STATE), "install")) {
	msgConfirm("This option may only be used after the system is installed, sorry!");
	return RET_FAIL;
    }

    /* Make sure we have media available */
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
    char tmp[512], target[512];
    char *where = NULL;
    char *browser = variable_get(VAR_BROWSER_BINARY);

    if (!file_executable(browser)) {
	dialog_clear();
	msgConfirm("Can't find the browser in %s!  Please ensure that it's\n"
		   "properly set in the Options editor.", browser);
	return RET_FAIL;
    }
    if (!strcmp(str, "Home"))
	where = "http://www.freebsd.org";
    else if (!strcmp(str, "Other"))
	where = msgGetInput("http://www.freebsd.org", "Please enter the URL of the location you wish to visit.");
    else if (!strcmp(str, "FAQ")) {
	strcpy(target, "/usr/share/doc/FAQ/freebsd-faq.html");
	if (!file_readable(target))
	    strcpy(target, "http://www.freebsd.org/FAQ");
	where = target;
    }
    else if (!strcmp(str, "Handbook")) {
	strcpy(target, "/usr/share/doc/handbook/handbook.html");
	if (!file_readable(target))
	    strcpy(target, "http://www.freebsd.org/handbook");
	where = target;
    }
    if (where) {
	sprintf(tmp, "%s %s", browser, where);
	systemExecute(tmp);
	return RET_SUCCESS;
    }
    else {
	msgConfirm("Hmmmmm!  I can't seem to access the documentation you selected!\n"
		   "Have you loaded the bin distribution?  Is your network connected?");
	return RET_FAIL;
    }
}
