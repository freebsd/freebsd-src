/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
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

static char *
varCheck(Option opt)
{
    char *cp = NULL;

    if (opt.aux)
	cp = variable_get((char *)opt.aux);
    if (!cp)
	return "NO";
    return cp;
}

/* Show our little logo */
static char *
resetLogo(char *str)
{
    return "[RESET!]";
}

static char *
mediaCheck(Option opt)
{
    if (mediaDevice) {
	switch(mediaDevice->type) {
	case DEVICE_TYPE_UFS:
	case DEVICE_TYPE_DISK:
	    return "File system";

	case DEVICE_TYPE_FLOPPY:
	    return "Floppy";

	case DEVICE_TYPE_FTP:
	    return "FTP";

	case DEVICE_TYPE_CDROM:
	    return "CDROM";

	case DEVICE_TYPE_TAPE:
	    return "Tape";

	case DEVICE_TYPE_DOS:
	    return "DOS";

	case DEVICE_TYPE_NFS:
	    return "NFS";

	case DEVICE_TYPE_NONE:
	case DEVICE_TYPE_NETWORK:
	case DEVICE_TYPE_ANY:
	default:
	    return "<unknown>";
	}
    }
    return "<not yet set>";
}

#define TAPE_PROMPT	"Please enter the tape block size in 512 byte blocks:"
#define RELNAME_PROMPT	"Please specify the release you wish to load or\n\"none\" for a generic release install:"
#define BPKG_PROMPT	"Please specify the name of the HTML browser package:"
#define BBIN_PROMPT	"Please specify a full pathname to the HTML browser binary:"
#define EDITOR_PROMPT	"Please specify the name of the text editor you wish to use:"
#define PKG_PROMPT	"Please specify a temporary directory with lots of free space:"
#define INSTROOT_PROMPT	"Please specify a root directory if installing somewhere other than /"
#define TIMEOUT_PROMPT	"Please specify the number of seconds to wait for slow media:"
#define APACHE_PKG_PROMPT "Please specify the package name for the Apache WEB server:"
#define SAMBA_PKG_PROMPT "Please specify the package name for the Samba server:"
#define GATED_PKG_PROMPT "Please specify the package name for the gated software:"
#define PCNFSD_PKG_PROMPT "Please specify the package name for the PCNFSD server:"

static Option Options[] = {
{ "NFS Secure",		"NFS server talks only on a secure port",
      OPT_IS_VAR,	NULL,			VAR_NFS_SECURE,		varCheck	},
{ "NFS Slow",		"User is using a slow PC or ethernet card",
      OPT_IS_VAR,	NULL,			VAR_SLOW_ETHER,		varCheck	},
{ "Debugging",		"Emit extra debugging output on VTY2 (ALT-F2)",
      OPT_IS_VAR,	NULL,			VAR_DEBUG,		varCheck	},
{ "Yes to All",		"Assume \"Yes\" answers to all non-critical dialogs",
      OPT_IS_VAR,	NULL,			VAR_NO_CONFIRM,		varCheck	},
{ "FTP username",	"Username and password to use instead of anonymous",
      OPT_IS_FUNC,	mediaSetFTPUserPass,	VAR_FTP_USER,		varCheck	},
{ "Editor",		"Which text editor to use during installation",
      OPT_IS_VAR,	EDITOR_PROMPT,		VAR_EDITOR,		varCheck	},
{ "Tape Blocksize",	"Tape media block size in 512 byte blocks",
      OPT_IS_VAR,	TAPE_PROMPT,		VAR_TAPE_BLOCKSIZE,	varCheck	},
{ "Extract Detail",	"How verbosely to display file name information during extractions",
      OPT_IS_FUNC,	mediaSetCPIOVerbosity,	VAR_CPIO_VERBOSITY,	varCheck	},
{ "Release Name",	"Which release to attempt to load from installation media",
      OPT_IS_VAR,	RELNAME_PROMPT,		VAR_RELNAME,		varCheck	},
{ "Install Root",	"Which directory to unpack distributions or packages relative to",
      OPT_IS_VAR,	INSTROOT_PROMPT,	VAR_INSTALL_ROOT,	varCheck	},
{ "Browser Pkg",	"This is the browser package that will be used for viewing HTML docs",
      OPT_IS_VAR,	BPKG_PROMPT,		VAR_BROWSER_PACKAGE,	varCheck	},
{ "Browser Exec",	"This is the path to the main binary of the browser package",
      OPT_IS_VAR,	BBIN_PROMPT,		VAR_BROWSER_BINARY,	varCheck	},
{ "Media Type",		"The current installation media type.",
      OPT_IS_FUNC,	mediaGetType,		VAR_MEDIA_TYPE,		mediaCheck	},
{ "Media Timeout",	"Timeout value in seconds for slow media.",
      OPT_IS_VAR,	TIMEOUT_PROMPT,		VAR_MEDIA_TIMEOUT,	varCheck	},
{ "Package Temp",	"The directory where package temporary files should go",
      OPT_IS_VAR,	PKG_PROMPT,		VAR_PKG_TMPDIR,		varCheck	},
{ "Apache package",	"The name of the Apache package to install if requested",
      OPT_IS_VAR,	APACHE_PKG_PROMPT,	VAR_APACHE_PKG,		varCheck	},
{ "Samba package",	"The name of the Samba package to install if requested",
      OPT_IS_VAR,	SAMBA_PKG_PROMPT,	VAR_SAMBA_PKG,		varCheck	},
{ "Gated package",	"The name of the gated package to install if requested",
      OPT_IS_VAR,	GATED_PKG_PROMPT,	VAR_GATED_PKG,		varCheck	},
{ "PCNFSD package",	"The name of the PCNFSD package to install if requested",
      OPT_IS_VAR,	PCNFSD_PKG_PROMPT,	VAR_PCNFSD_PKG,		varCheck	},
{ "Use Defaults",	"Reset all values to startup defaults",
      OPT_IS_FUNC,	installVarDefaults,	0,			resetLogo	},
{ NULL },
};

#define OPT_START_ROW	4
#define OPT_END_ROW	19
#define OPT_NAME_COL	0
#define OPT_VALUE_COL	16
#define GROUP_OFFSET	40

static char *
value_of(Option opt)
{
    static char ival[40];

    switch (opt.type) {
    case OPT_IS_STRING:
	return (char *)opt.data;

    case OPT_IS_INT:
	sprintf(ival, "%d", (int)opt.data);
	return ival;

    case OPT_IS_FUNC:
    case OPT_IS_VAR:
	if (opt.check)
	    return opt.check(opt);
	else
	    return "<*>";
    }
    return "<unknown>";
}

static int
fire(Option opt)
{
    int status = 0;

    if (opt.type == OPT_IS_FUNC) {
	int (*cp)(char *) = opt.data, rcode;

	rcode = cp(NULL);
	if (rcode & (DITEM_RECREATE | DITEM_RESTORE))
	    status = 1;
    }
    else if (opt.type == OPT_IS_VAR) {
	if (opt.data) {
	    (void)variable_get_value(opt.aux, opt.data);
	    status = 1;
	}
	else if (variable_get(opt.aux))
	    variable_unset(opt.aux);
	else
	    variable_set2(opt.aux, "YES");
    }
    if (opt.check)
	opt.check(opt);
    refresh();
    return status;
}

int
optionsEditor(dialogMenuItem *self)
{
    int i, optcol, optrow, key;
    static int currOpt = 0;

    dialog_clear_norefresh();
    clear();

    while (1) {
	/* Whap up the header */
	attrset(A_REVERSE); mvaddstr(0, 0, "Options Editor"); attrset(A_NORMAL);
	for (i = 0; i < 2; i++) {
	    mvaddstr(OPT_START_ROW - 2, OPT_NAME_COL + (i * GROUP_OFFSET), "Name");
	    mvaddstr(OPT_START_ROW - 1, OPT_NAME_COL + (i * GROUP_OFFSET), "----");

	    mvaddstr(OPT_START_ROW - 2, OPT_VALUE_COL + (i * GROUP_OFFSET), "Value");
	    mvaddstr(OPT_START_ROW - 1, OPT_VALUE_COL + (i * GROUP_OFFSET), "-----");
	}
	/* And the footer */
	mvprintw(OPT_END_ROW + 1, 0, "Use SPACE to select/toggle an option, arrow keys to move,");
	mvprintw(OPT_END_ROW + 2, 0, "? or F1 for more help.  When you're done, type Q to Quit.");

	optrow = OPT_START_ROW;
	optcol = OPT_NAME_COL;
	for (i = 0; Options[i].name; i++) {
	    /* Names are painted somewhat gratuitously each time, but it's easier this way */
	    mvprintw(optrow, OPT_NAME_COL + optcol, Options[i].name);
	    if (currOpt == i)
		attrset(ATTR_SELECTED);
	    mvprintw(optrow++, OPT_VALUE_COL + optcol, value_of(Options[i]));
	    if (currOpt == i)
		attrset(A_NORMAL);
	    if (optrow == OPT_END_ROW) {
		optrow = OPT_START_ROW;
		optcol += GROUP_OFFSET;
	    }
	    clrtoeol();
	}
	attrset(ATTR_TITLE);
	mvaddstr(OPT_END_ROW + 4, 0, Options[currOpt].desc);
	attrset(A_NORMAL);
	clrtoeol();
	move(0, 14);
        refresh();

	/* Start the edit loop */
	key = toupper(getch());
	switch (key) {
	case KEY_F(1):
	case '?':
	    systemDisplayHelp("options");
	    clear();
	    break;

	case '\020':	/* ^P */
	case KEY_UP:
	    if (currOpt)
		--currOpt;
	    else
		for (currOpt = 0; Options[currOpt + 1].name; currOpt++);
	    continue;

	case '\016':	/* ^N */
	case KEY_DOWN:
	    if (Options[currOpt + 1].name)
		++currOpt;
	    else
		currOpt = 0;
	    continue;

	case KEY_HOME:
	    currOpt = 0;
	    continue;

	case KEY_END:
	    while (Options[currOpt + 1].name)
		++currOpt;
	    continue;

	case ' ':
	    if (fire(Options[currOpt]))
	    	clear();
	    continue;

	case '\033':	/* ESC */
	case 'Q':
	    clear();
	    dialog_clear();
	    return DITEM_SUCCESS | DITEM_RESTORE;

	default:
	    beep();
	}
    }
    /* NOTREACHED */
    return DITEM_SUCCESS | DITEM_RESTORE;
}

