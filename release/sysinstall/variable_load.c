/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: variable_load.c,v 1.6 1997/06/05 09:48:03 jkh Exp $
 *
 * Copyright (c) 1997
 *	Paul Traina.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY PAUL TRAINA ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL TRAINA OR HIS KILLER RATS BE LIABLE
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
#include <sys/signal.h>
#include <sys/fcntl.h>

/* Add a string to a string list */
static char **
string_add(char **list, char *str, int *curr, int *max)
{

    if (*curr == *max) {
	*max += 20;
	list = (char **)realloc(list, sizeof(char *) * *max);
    }
    list[(*curr)++] = strdup(str);
    return list;
}

/* Toss the strings out */
static void
strings_free(char **list, int *curr, int *max)
{
    int i;

    for (i = 0; i < *curr; i++)
	free(list[i]);
    free(list);
    *curr = *max = 0;
}

int
variableLoad(dialogMenuItem *self)
{
    int             what = DITEM_RESTORE | DITEM_SUCCESS;
    char            buf[BUFSIZ];
    extern char    *distWanted;
    char           *cp;
    FILE           *fp;
    int		    i, curr, max;
    char	  **list;

    mediaClose();
    dialog_clear_norefresh();

    cp = variable_get_value(VAR_INSTALL_CFG,
			    "Specify the name of a configuration file\n"
			    "residing on a MSDOS or UFS floppy.");
    if (!cp || !*cp) {
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	return what;
    }

    distWanted = cp;
    /* Try to open the floppy drive */
    if (DITEM_STATUS(mediaSetFloppy(NULL)) == DITEM_FAILURE) {
	msgConfirm("Unable to set media device to floppy.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    if (!mediaDevice->init(mediaDevice)) {
	msgConfirm("Unable to mount floppy filesystem.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    fp = mediaDevice->get(mediaDevice, cp, TRUE);
    if (fp) {
	msgNotify("Loading %s pre-configuration file", cp);

	/* Hint to others that we're running from a script, should they care */
	variable_set2(VAR_NONINTERACTIVE, "YES");

	/* Now suck in the lot to execute later */
	curr = max = 0;
	list = NULL;
	while (fgets(buf, sizeof buf, fp)) {
	    if ((cp = strchr(buf, '\n')) != NULL)
		*cp = '\0';
	    if (*buf == '\0' || *buf == '#')
		continue;
	    list = string_add(list, buf, &curr, &max);
	}
	fclose(fp);
	mediaClose();

	for (i = 0; i < curr; i++) {
	    if (DITEM_STATUS(dispatchCommand(list[i])) != DITEM_SUCCESS) {
		msgConfirm("Command `%s' failed - rest of script aborted.\n", buf);
		what |= DITEM_FAILURE;
		break;
	    }
	}
	strings_free(list, &curr, &max);
    }
    else {
	msgConfirm("Configuration file '%s' not found.", cp);
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	mediaClose();
    }

    variable_unset(VAR_NONINTERACTIVE);
    return what;
}
