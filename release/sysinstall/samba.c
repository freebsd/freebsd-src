/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: samba.c,v 1.9.2.3 1997/01/19 09:59:37 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard & Coranth Gryphon.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR THEIR PETS BE LIABLE
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

static DMenu MenuSamba = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Samba Services Menu",
    "This allows you to configure various aspects of your Samba server.",
    NULL,
    NULL,
{ { "Homes",		"Make home directories available to users.",
    dmenuVarCheck, dmenuSetVariable, NULL, "SAMBA_homes=YES" },
  { "Printers",		"Allows for sharing of local printers.",
    dmenuVarCheck, dmenuSetVariable, NULL, "SAMBA_printers=YES" },
  { "Export Paths",	"Specify local directories to make available.",
    dmenuVarCheck, dmenuSetVariable, NULL, "SAMBA_export=YES" },
  { NULL } },
};

/* These probably shouldn't be hard-coded, but making them options might prove to be even more confusing! */
#define SMB_CONF_DIR "/usr/local/etc"
#define SMB_CONF "/usr/local/etc/smb.conf"

int
configSamba(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;
    WINDOW *w = savescr();

    if (!dmenuOpenSimple(&MenuSamba, FALSE))
	i = DITEM_FAILURE;
    else if (DITEM_STATUS(package_add(variable_get(VAR_SAMBA_PKG))) != DITEM_SUCCESS)
	i = DITEM_FAILURE;
    else {
	FILE *fptr;
	char tbuf[256], *tptr;
	int tval;

	fptr = fopen("/tmp/smb.conf","w");
	if (fptr) {
	    strcpy(tbuf, "FreeBSD - Samba %v");
	    if (variable_get("SAMBA_string")) {
		tptr = msgGetInput("FreeBSD - Samba %%v", "What should this server list as its description?\n"
				   "Note that the \"%%v\" refers to the samba version number.");
		if (tptr && *tptr)
		    strcpy(tbuf, tptr);
	    }
		
	    fprintf(fptr, "[global]\n");
	    fprintf(fptr, "comment = %s\n", tbuf);
	    fprintf(fptr, "log file = /var/log/samba.log\n");
	    fprintf(fptr, "dont descend = /dev,/proc,/root,/stand\n\n");
	    
	    fprintf(fptr, "printing = bsd\n");
	    fprintf(fptr, "map archive = no\n");
	    fprintf(fptr, "status = yes\n");
	    fprintf(fptr, "public = yes\n");
	    fprintf(fptr, "read only = no\n");
	    fprintf(fptr, "preserve case = yes\n");
	    fprintf(fptr, "strip dot = yes\n");
	    fprintf(fptr, "security = share\n");
	    fprintf(fptr, "guest ok = yes\n\n");
	    
	    if (variable_get("SAMBA_homes")) {
		fprintf(fptr, "[homes]\n");
		fprintf(fptr, "browseable = no\n");
		fprintf(fptr, "comment = User Home Directory\n");
		fprintf(fptr, "create mode = 0775\n");
		fprintf(fptr, "public = no\n\n");
	    }
	    
	    if (variable_get("SAMBA_printers")) {
		fprintf(fptr, "[printers]\n");
		fprintf(fptr, "path = /var/spool\n");
		fprintf(fptr, "comment = Printers\n");
		fprintf(fptr, "create mode = 0700\n");
		fprintf(fptr, "browseable = no\n");
		fprintf(fptr, "printable = yes\n");
		fprintf(fptr, "read only = yes\n");
		fprintf(fptr, "public = no\n\n");
	    }

	    if (variable_get("SAMBA_export")) {
		dialog_clear_norefresh();
		for (tval = 0; ! tval; tval = msgYesNo("Another?")) {
		    tptr = msgGetInput(NULL,"What directory to export?");
		    if (tptr && *tptr && (tptr[0] == '/')) {
			int len = strlen(tbuf);
			
			strcpy(tbuf, tptr);
			if (tbuf[len - 1] == '/')
			    tbuf[len - 1] = '\0';
			if (directory_exists(tbuf)) {
			    tptr = msgGetInput(pathBaseName(tbuf), "What do you want to call this share?");
			    if (tptr && *tptr) {
				fprintf(fptr, "[%s]\npath = %s\n", tptr, tbuf);
				tptr = msgGetInput(NULL, "Enter a short description of this share?");
				if (tptr && *tptr)
				    fprintf(fptr, "comment = %s\n", tptr);
				if (msgYesNo("Do you want this share to be read only?") != 0)
				    fprintf(fptr, "read only = no\n\n");
				else
				    fprintf(fptr, "read only = yes\n\n");
			    }
			    else
				msgConfirm("Invalid Share Name.");
			}
			else
			    msgConfirm("Directory does not exist.");
		    }	/* end if (tptr)	 */
		}	/* end for loop		 */
	    }	/* end if (SAMBA_export) */
	    fclose(fptr);
	    Mkdir(SMB_CONF_DIR);
	    vsystem("mv -f /tmp/smb.conf %s", SMB_CONF);
	}
	else
	    msgConfirm("Unable to open temporary smb.conf file.\n"
		       "Samba will have to be configured by hand.");
    }
    if (DITEM_STATUS(i) == DITEM_SUCCESS)
	variable_set2("samba", "YES");
    restorescr(w);
    return i | DITEM_RESTORE;
}
