/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: options.c,v 1.13 1995/10/18 05:01:59 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
ftpFlagCheck(Option opt)
{
    /* Verify that everything's sane */
    if ((int)opt.aux == OPT_FTP_ABORT && optionIsSet(OPT_FTP_RESELECT))
        OptFlags &= ~OPT_FTP_RESELECT;
    else if ((int)opt.aux == OPT_FTP_RESELECT && optionIsSet(OPT_FTP_ABORT))
        OptFlags &= ~OPT_FTP_ABORT;
    return NULL;
}

static char *
varCheck(Option opt)
{
    if (opt.aux)
	return variable_get((char *)opt.aux);
    return NULL;
}

/* Show our little logo */
static char *
resetLogo(char *str)
{
    return "[WHAP!]";
}

static Option Options[] = {
{ "NFS Secure",		"NFS server talks only on a secure port",
      OPT_IS_FLAG,	&OptFlags,	(void *)OPT_NFS_SECURE,		NULL		},
{ "NFS Slow",		"User is using a slow PC or ethernet card",
      OPT_IS_FLAG,	&OptFlags,	(void *)OPT_SLOW_ETHER,		NULL		},
{ "Debugging",		"Emit extra debugging output on VTY2 (ALT-F2)",
      OPT_IS_FLAG,	&OptFlags,	(void *)OPT_DEBUG,		NULL		},
{ "Yes to All",		"Assume \"Yes\" answers to all non-critical dialogs",
      OPT_IS_FLAG,	&OptFlags,	(void *)OPT_NO_CONFIRM,		NULL		},
{ "FTP Abort",		"On transfer failure, abort the installation of a distribution",
      OPT_IS_FLAG,	&OptFlags,	(void *)OPT_FTP_ABORT,		ftpFlagCheck	},
{ "FTP Reselect",	"On transfer failure, ask for another host and try to resume",
      OPT_IS_FLAG,	&OptFlags,	(void *)OPT_FTP_RESELECT,	ftpFlagCheck	},
{ "FTP username",	"Username and password to use instead of anonymous",
      OPT_IS_FUNC,	mediaSetFtpUserPass,	FTP_USER,		varCheck	},
{ "Tape Blocksize",	"Tape media block size in 512 byte blocks",
      OPT_IS_VAR,	"Please enter the tape block size in 512 byte blocks", TAPE_BLOCKSIZE,		varCheck },
{ "Extract Detail",	"How verbosely to display file name information during extractions",
      OPT_IS_FUNC,	mediaSetCPIOVerbosity,	CPIO_VERBOSITY_LEVEL,	varCheck	},
{ "Release Name",	"Which release to attempt to load from installation media",
      OPT_IS_VAR,	"Please specify the release you wish to load", RELNAME,		varCheck },
{ "Browser Pkg",	"This is the browser package that will be used for viewing HTML",
      OPT_IS_VAR,	"Please specify the name of the HTML browser package:",	BROWSER_PACKAGE,	varCheck },
{ "Browser Exec",	"This is the path to the main binary of the browser package",
      OPT_IS_VAR,	"Please specify a full pathname to the HTML browser binary:", BROWSER_BINARY,	varCheck },
{ "Config File",	"Name of default configuration file for Load command (top menu)",
      OPT_IS_VAR,	"Please specify the name of a configuration file", CONFIG_FILE,	varCheck },
{ "Use Defaults",	"Reset all values to startup defaults",
      OPT_IS_FUNC,	installVarDefaults,	0,			resetLogo	},
{ NULL },
};

#define OPT_START_ROW	4
#define OPT_END_ROW	20
#define OPT_NAME_COL	0
#define OPT_VALUE_COL	16
#define GROUP_OFFSET	40

Boolean
optionIsSet(int opt)
{
    return OptFlags & opt;
}

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

    case OPT_IS_FLAG:
	return (*(int *)opt.data) & (int)opt.aux ? "ON" : "OFF";

    case OPT_IS_FUNC:
    case OPT_IS_VAR:
	if (opt.check)
	    return opt.check(opt);
	else
	    return "<*>";
    }
    return "<unknown>";
}

static void
fire(Option opt)
{
    if (opt.type == OPT_IS_FLAG) {
	/* Toggle a flag */
	if (*((int *)opt.data) & (int)opt.aux)
	    *((int *)opt.data) &= ~(int)opt.aux;
	else
	    *((int *)opt.data) |= (int)opt.aux;
    }
    else if (opt.type == OPT_IS_FUNC) {
	int (*cp)(char *) = opt.data;

	cp(NULL);
    }
    else if (opt.type == OPT_IS_VAR) {
	dialog_clear();
	(void)variable_get_value(opt.aux, opt.data);
	dialog_clear();
    }
    if (opt.check)
	opt.check(opt);
}

int
optionsEditor(char *str)
{
    int i, optcol, optrow, key;
    static int currOpt = 0;

    dialog_clear();
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
	mvprintw(OPT_END_ROW + 0, 0, "Use SPACE to select/toggle an option, arrow keys to move,");
	mvprintw(OPT_END_ROW + 1, 0, "? or F1 for more help.  When you're done, type Q to Quit.");

	optrow = OPT_START_ROW;
	optcol = OPT_NAME_COL;
	for (i = 0; Options[i].name; i++) {
	    /* Names are painted somewhat gratuitously each time, but it's easier this way */
	    mvprintw(optrow, OPT_NAME_COL + optcol, Options[i].name);
	    if (currOpt == i) standout();
	    mvprintw(optrow++, OPT_VALUE_COL + optcol, value_of(Options[i]));
	    if (currOpt == i) standend();
	    if (optrow == OPT_END_ROW) {
		optrow = OPT_START_ROW;
		optcol += GROUP_OFFSET;
	    }
	    clrtoeol();
	}
	standout();
	mvaddstr(OPT_END_ROW + 3, 0, Options[currOpt].desc);
	standend();
	clrtoeol();
	move(0, 14);

	/* Start the edit loop */
	key = toupper(getch());
	switch (key) {
	case KEY_F(1):
	case '?':
	    systemDisplayHelp("options");
	    break;

	case KEY_UP:
	    if (currOpt)
		--currOpt;
	    else
		beep();
	    continue;

	case KEY_DOWN:
	    if (Options[currOpt + 1].name)
		++currOpt;
	    else
		beep();
	    continue;

	case KEY_HOME:
	    currOpt = 0;
	    continue;

	case KEY_END:
	    while (Options[currOpt + 1].name)
		++currOpt;
	    continue;

	case ' ':
	    fire(Options[currOpt]);
	    clear();
	    dialog_clear();
	    continue;

	case 'Q':
	    clear();
	    dialog_clear();
	    return RET_SUCCESS;

	default:
	    beep();
	}
    }
    /* NOTREACHED */
    return RET_SUCCESS;
}
