/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated for what's essentially a complete rewrite.
 *
 * $Id: variable_load.c,v 1.1.4.1 1997/03/29 06:44:54 jkh Exp $
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

int
variableLoad(dialogMenuItem * self)
{
    int             what = DITEM_RESTORE;
    char            buf[BUFSIZ];
    extern char    *distWanted;
    char           *cp;
    FILE           *fp;

    mediaClose();
    dialog_clear_norefresh();

    cp = variable_get_value(VAR_INSTALL_CFG,
			    "Specify the name of a configuration file\n"
			    "residing on a MSDOS or UFS floppy.");
    if (!cp || !*cp) {
	variable_unset(VAR_INSTALL_CFG);
	return DITEM_FAILURE | what;
    }

    distWanted = cp = variable_get(VAR_INSTALL_CFG);

    /* Try to open the floppy drive if we can do that first */
    if (DITEM_STATUS(mediaSetFloppy(NULL)) == DITEM_FAILURE) {
	msgConfirm("Unable to set media device to floppy.");
	what |= DITEM_FAILURE;
	goto terminate_device;
    }

    if (!mediaDevice->init(mediaDevice)) {
	msgConfirm("Unable to mount floppy filesystem.");
	what |= DITEM_FAILURE;
	goto terminate_device;
    }

    fp = mediaDevice->get(mediaDevice, cp, TRUE);
    if (!fp) {
	msgConfirm("Configuration file '%s' not found.");
	variable_unset(VAR_INSTALL_CFG);
	what |= DITEM_FAILURE;
	goto terminate_device;
    }

    msgNotify("Loading %s pre-configuration file", cp);

    while (fgets(buf, sizeof buf, fp)) {
	if (DITEM_STATUS(dispatchCommand(buf)) != DITEM_SUCCESS) {
	    msgConfirm("Command `%s' failed - rest of script aborted.\n", buf);
	    what |= DITEM_FAILURE;
	    goto terminate_file;
	}
    }
    what |= DITEM_SUCCESS;

terminate_file:
    fclose(fp);

terminate_device:
    mediaClose();

    return what;
}
