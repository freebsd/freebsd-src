/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: installFinal.c,v 1.1 1995/10/19 16:15:39 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the authors listed above
 *	for the FreeBSD Project.
 * 4. The names of the authors or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

/* place-holder for now */
int
installApache(void)
{
    return RET_SUCCESS;
}

static DMenu MenuSamba = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
    "Samba Services Menu",
    "This allows you to configure various aspects of your Samba server.",
    NULL,
    NULL,
{ { "Homes",		"Make home directories available to users.",
	DMENU_SET_VARIABLE,	"SAMBA_homes=YES", 0, 0, dmenuVarCheck	},
  { "Printers",		"Allows for sharing of local printers.",
	DMENU_SET_VARIABLE,	"SAMBA_printers=YES", 0, 0, dmenuVarCheck},
  { "Export Paths",	"Specify local directories to make available.",
	DMENU_SET_VARIABLE,	"SAMBA_export=YES", 0, 0, dmenuVarCheck	},
  { NULL } },
};

/* These probably shouldn't be hard-coded, but making them options might prove to be even more confusing! */
#define FTP_UID 14
#define FTP_NAME  "ftp"
#define FTP_GROUP "operator"
#define FTP_COMMENT  "Anonymous FTP Admin"

#define SMB_CONF "./smb.conf"


/* Do any final optional hackery */
int
installFinal(void)
{
    int i, tval;
    char tbuf[256];
    char *tptr;
    FILE *fptr;

    i = RET_SUCCESS;

    /* Do we want to install and set up gated? */
    if (variable_get("gated")) {
	/* Load gated package and maybe even seek to configure or explain it a little */
    }

    /* Set up anonymous FTP access to this machine? */
    if (variable_get("anon_ftp")) {
	tptr = msgGetInput("/u", "What directory should the ftp home be in?");
	if (tptr && *tptr && (tptr[0] == '/')) {
	    int len = strlen(tbuf);

	    strcpy(tbuf, tptr);
	    if (tbuf[len - 1] == '/')
		tbuf[len - 1] = '\0';

	    if (vsystem("adduser -uid %d -home %s -shell date -dotdir no -batch %s %s \"%s\" ",
			FTP_UID, tbuf, FTP_NAME, FTP_GROUP, FTP_COMMENT)) {
		msgConfirm("Unable to create FTP user!  Anonymous FTP setup failed.");
		i = RET_FAIL;
	    }
	    else {    
		vsystem("mkdir %s/%s/pub", tbuf, FTP_NAME);
		vsystem("mkdir %s/%s/upload", tbuf, FTP_NAME);
		vsystem("chmod 0777 %s/%s/upload", tbuf, FTP_NAME);
	    }
	}
	else {
	    msgConfirm("Invalid Directory. Anonymous FTP will not be set up.");
	}
    }

    /* Set this machine up as a web server? */
    if (variable_get("apache_httpd")) {
	i = installApache();
    }

    /* Set this machine up as a Samba server? */
    if (variable_get("samba")) {
	if (!dmenuOpenSimple(&MenuSamba))
	    i = RET_FAIL;
	else {
	    fptr = fopen("/tmp/smb.conf","w");
	    if (fptr) {
		strcpy(tbuf,"FreeBSD - Samba %v");
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
		    for (tval = 0; ! tval; tval = msgYesNo("Another?")) {
			tptr = msgGetInput(NULL,"What directory to export?");
			if (tptr && *tptr && (tptr[0] == '/')) {
			    int len = strlen(tbuf);

			    strcpy(tbuf, tptr);
			    if (tbuf[len - 1] == '/')
				tbuf[len - 1] = '\0';
			    if (directoryExists(tbuf)) {
				tptr = msgGetInput(pathBaseName(tbuf), "What do you want to call this share?");
				if (tptr && *tptr) {
				    fprintf(fptr, "[%s]\npath = %s\n", tptr, tbuf);
				    tptr = msgGetInput(NULL, "Enter a short description of this share?");
				    if (tptr && *tptr)
					fprintf(fptr, "comment = %s\n", tptr);
				    if (msgYesNo("Do you want this share to be read only?"))
					fprintf(fptr, "read only = no\n\n");
				    else
					fprintf(fptr, "read only = yes\n\n");
				}
				else {
				    msgConfirm("Invalid Share Name.");
				}
			    }
			    else {
				msgConfirm("Directory does not exist.");
			    }
			}	/* end if (tptr)	 */
		    }	/* end for loop		 */
		}	/* end if (SAMBA_export) */
		
		fclose(fptr);
		vsystem("mv -f /tmp/smb.conf %s", SMB_CONF);
	    }
	    else {
		msgConfirm("Unable to open temporary smb.conf file.\nSamba must be configured by hand.");
	    }
	}
    }

    /* Set this machine up with a PC-NFS authentication server? */
    if (variable_get("pcnfsd")) {
	/* Load and configure pcnfsd */
    }

    /* If we're an NFS server, we need an exports file */
    if (variable_get("nfs_server") && !file_readable("/etc/exports")) {
	msgConfirm("You have chosen to be an NFS server but have not yet configured\n"
		   "the /etc/exports file.  You must configure this information before\n"
		   "other hosts will be able to mount file systems from your machine.\n"
		   "Press [ENTER] now to invoke an editor on /etc/exports");
	vsystem("echo '#The following example exports /usr to 3 machines named after ducks.' > /etc/exports");
	vsystem("echo '#/usr	huey louie dewie' >> /etc/exports");
	vsystem("echo >> /etc/exports");
	systemExecute("ee /etc/exports");
    }
    return i;
}

