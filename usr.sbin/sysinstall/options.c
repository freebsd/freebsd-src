/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: options.c,v 1.2 1995/10/04 10:34:04 jkh Exp $
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
userPassCheck(Option opt)
{
    char *cp = variable_get(FTP_USER);

    return cp ? cp : "ftp";
}

static char *
ftpFlagCheck(Option opt)
{
    /* Verify that everything's sane */
    if (opt.aux == OPT_FTP_ABORT && optionIsSet(OPT_FTP_RESELECT))
        OptFlags &= ~OPT_FTP_RESELECT;
    else if (opt.aux == OPT_FTP_RESELECT && optionIsSet(OPT_FTP_ABORT))
        OptFlags &= ~OPT_FTP_ABORT;
    return NULL;
}

static char *
blockSizeCheck(Option opt)
{
    char *cp = variable_get(TAPE_BLOCKSIZE);

    return cp ? cp : DEFAULT_TAPE_BLOCKSIZE;
}

static char *
releaseCheck(Option opt)
{
    /* This one is always defined */
    return variable_get(RELNAME);
}

static char *
checkCpioVerbosity(Option opt)
{
    /* This one is always defined */
    return getenv(CPIO_VERBOSITY_LEVEL);
}

/* Nuke all the flags */
static int
resetFlags(char *str)
{
    OptFlags = OPT_DEFAULT_FLAGS;
    return 0;
}

/* Show our little logo */
static char *
resetLogo(char *str)
{
    return "[whap!]";
}

static Option Options[] = {
{ "NFS Secure",		"NFS server talks only on a secure port",
      OPT_IS_FLAG,	&OptFlags,	OPT_NFS_SECURE,		NULL		},
{ "NFS Slow",		"User is using a slow PC or ethernet card",
      OPT_IS_FLAG,	&OptFlags,	OPT_SLOW_ETHER,		NULL		},
{ "Debugging",		"Emit extra debugging output on VTY1 (ALT-F2)",
      OPT_IS_FLAG,	&OptFlags,	OPT_DEBUG,		NULL		},
{ "Yes to All",		"Assume \"Yes\" answers to all non-critical dialogs",
      OPT_IS_FLAG,	&OptFlags,	OPT_NO_CONFIRM,		NULL		},
{ "FTP Abort",		"On transfer failure, abort the installation of a distribution",
      OPT_IS_FLAG,	&OptFlags,	OPT_FTP_ABORT,		ftpFlagCheck	},
{ "FTP Reselect",	"On transfer failure, ask for another host and try to resume",
      OPT_IS_FLAG,	&OptFlags,	OPT_FTP_RESELECT,	ftpFlagCheck	},
{ "FTP username",	"Username and password to use instead of anonymous",
      OPT_IS_FUNC,	mediaSetFtpUserPass,	0,		userPassCheck	},
{ "Tape Blocksize",	"Tape media block size in 512 byte blocks",
      OPT_IS_FUNC,	mediaSetTapeBlocksize,	0,		blockSizeCheck	},
{ "Detail Level",	"How to display filenames on debug screen as CPIO extracts them",
      OPT_IS_FUNC,	mediaSetCPIOVerbosity,	0,		checkCpioVerbosity },
{ "Release Name",	"Which release to attempt to load from installation media",
      OPT_IS_FUNC,	installSelectRelease,	0,		releaseCheck	},
{ "Reset Flags",	"Reset all flag values to defaults",
      OPT_IS_FUNC,	resetFlags,	0,			resetLogo	},
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
	return (*(int *)opt.data) & opt.aux ? "ON" : "OFF";

    case OPT_IS_FUNC:
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
	if (*((int *)opt.data) & opt.aux)
	    *((int *)opt.data) &= ~opt.aux;
	else
	    *((int *)opt.data) |= opt.aux;
    }
    else if (opt.type == OPT_IS_FUNC) {
	int (*cp)(char *) = opt.data;

	cp(NULL);
    }
    if (opt.check)
	opt.check(opt);
}

int
optionsEditor(char *str)
{
    int i, optcol, optrow, key;
    static int currOpt = 0;

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
	mvaddstr(OPT_END_ROW + 3, 0, Options[currOpt].desc);
	clrtoeol();
	move(0, 14);
	/* Start the edit loop */
	key = toupper(getch());
	switch (key) {
	case KEY_F(1):
	case '?':
	    systemDisplayFile("options");
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
	case '\n':
	    fire(Options[currOpt]);
	    continue;

	case 'Q':
	    clear();
	    dialog_clear();
	    return 0;

	default:
	    beep();
	}
    }
    /* NOTREACHED */
    return 0;
}
