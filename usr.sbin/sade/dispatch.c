/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: dispatch.c,v 1.12 1997/03/10 21:11:52 jkh Exp $
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

static int _shutdown(dialogMenuItem *unused);
    
static struct _word {
    char *name;
    int (*handler)(dialogMenuItem *self);
} resWords[] = {
    { "configAnonFTP",		configAnonFTP		},
    { "configRouter",		configRouter		},
    { "configNFSServer",	configNFSServer		},
    { "configSamba",		configSamba		},
    { "configRegister",		configRegister		},
    { "configPackages",		configPackages		},
    { "diskPartitionEditor",	diskPartitionEditor	},
    { "diskPartitionWrite",	diskPartitionWrite	},
    { "diskLabelEditor",	diskLabelEditor		},
    { "diskLabelCommit",	diskLabelCommit		},
    { "distReset",		distReset		},
    { "distSetDeveloper",	distSetDeveloper	},
    { "distSetXDeveloper",	distSetXDeveloper	},
    { "distSetKernDeveloper",	distSetKernDeveloper	},
    { "distSetUser",		distSetUser		},
    { "distSetXUser",		distSetXUser		},
    { "distSetMinimum",		distSetMinimum		},
    { "distSetEverything",	distSetEverything	},
    { "distSetDES",		distSetDES		},
    { "distSetSrc",		distSetSrc		},
    { "distSetXF86",		distSetXF86		},
    { "distExtractAll",		distExtractAll		},
    { "docBrowser",		docBrowser		},
    { "docShowDocument",	docShowDocument		},
    { "installCommit",		installCommit		},
    { "installExpress",		installExpress		},
    { "installUpgrade",		installUpgrade		},
    { "installFixup",		installFixup		},
    { "installFilesystems",	installFilesystems	},
    { "mediaSetCDROM",		mediaSetCDROM		},
    { "mediaSetFloppy",		mediaSetFloppy		},
    { "mediaSetDOS",		mediaSetDOS		},
    { "mediaSetTape",		mediaSetTape		},
    { "mediaSetFTP",		mediaSetFTP		},
    { "mediaSetFTPActive",	mediaSetFTPActive	},
    { "mediaSetFTPPassive",	mediaSetFTPPassive	},
    { "mediaSetUFS",		mediaSetUFS		},
    { "mediaSetNFS",		mediaSetNFS		},
    { "mediaSetFTPUserPass",	mediaSetFTPUserPass	},
    { "mediaSetCPIOVerbosity",	mediaSetCPIOVerbosity	},
    { "mediaGetType",		mediaGetType		},
    { "optionsEditor",		optionsEditor		},
    { "register",		configRegister		},	/* Alias */
    { "addGroup",		userAddGroup		},
    { "addUser",		userAddUser		},
    { "shutdown",		_shutdown 		},
    { NULL, NULL },
};

static int
call_possible_resword(char *name, dialogMenuItem *value, int *status)
{
    int i, rval;

    rval = 0;
    for (i = 0; resWords[i].name; i++) {
	if (!strcmp(name, resWords[i].name)) {
	    *status = resWords[i].handler(value);
	    rval = 1;
	    break;
	}
    }
    return rval;
}

/* Just convenience */
static int _shutdown(dialogMenuItem *unused)
{
    systemShutdown(0);
    return DITEM_FAILURE;
}

/* For a given string, call it or spit out an undefined command diagnostic */
int
dispatchCommand(char *str)
{
    int i;
    char *cp;

    if (!str || !*str) {
	msgConfirm("Null or zero-length string passed to dispatchCommand");
	return DITEM_FAILURE;
    }
    /* If it's got a newline, trim it */
    if ((cp = index(str, '\n')) != NULL)
	*cp = '\0';

    /* A command might be a pathname if it's encoded in argv[0], as we also support */
    if (index(str, '=')) {
	variable_set(str);
	i = DITEM_SUCCESS;
    }
    else {
	if ((cp = index(str, '/')) != NULL)
	    str = cp + 1;
	if (!call_possible_resword(str, NULL, &i)) {
	    msgConfirm("No such command: %s", str);
	    i = DITEM_FAILURE;
	}
    }
    return i;
}
