/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: installPreconfig.c,v 1.10 1995/10/22 17:39:15 jkh Exp $
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
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#define MSDOSFS
#include <sys/mount.h>
#undef MSDOSFS
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

static struct _word {
    char *name;
    int (*handler)(char *str);
} resWords[] = {
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
    { "installApache",		installApache		},
    { "installCommit",		installCommit		},
    { "installExpress",		installExpress		},
    { "installUpgrade",		installUpgrade		},
    { "installPreconfig",	installPreconfig	},
    { "installFixup",		installFixup		},
    { "installFinal",		installFinal		},
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
    { "mediaSetFtpUserPass",	mediaSetFtpUserPass	},
    { "mediaSetCPIOVerbosity",	mediaSetCPIOVerbosity	},
    { "mediaGetType",		mediaGetType		},
    { "msgConfirm",		msgSimpleConfirm	},
    { "msgNotify",		msgSimpleNotify		},
    { "packageAdd",		package_add		},
    { "system",			(int (*)(char *))system	},
    { "systemInteractive",	systemExecute		},
    { "tcpInstallDevice",	tcpInstallDevice	},
    { NULL, NULL },
};

static int
call_possible_resword(char *name, char *value, int *status)
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

/* From the top menu - try to mount the floppy and read a configuration file from it */
int
installPreconfig(char *str)
{
    struct ufs_args u_args;
    struct msdosfs_args	m_args;
    int fd, i;
    char buf[128];
    char *cfg_file;

    memset(&u_args, 0, sizeof(u_args));
    u_args.fspec = "/dev/fd0";
    Mkdir("/mnt2", NULL);

    memset(&m_args, 0, sizeof(m_args));
    m_args.fspec = "/dev/fd0";
    m_args.uid = m_args.gid = 0;
    m_args.mask = 0777;

    i = RET_FAIL;
    while (1) {
	if (!(cfg_file = variable_get_value(VAR_CONFIG_FILE,
					    "Please insert the floppy containing this configuration file\n"
					    "into drive A now and press [ENTER].")))
	    break;

	if (mount(MOUNT_UFS, "/mnt2", MNT_RDONLY, (caddr_t)&u_args) == -1) {
	    if (mount(MOUNT_MSDOS, "/mnt2", MNT_RDONLY, (caddr_t)&m_args) == -1) {
		dialog_clear();
		if (msgYesNo("Unable to mount the configuration floppy - do you want to try again?"))
		    break;
		else
		    continue;
	}
    }
    fnord:
	if (!cfg_file)
	    break;
	sprintf(buf, "/mnt2/%s", cfg_file);
	msgDebug("Attempting to open configuration file: %s\n", buf);
	fd = open(buf, O_RDONLY);
	if (fd == -1) {
	    dialog_clear();
	    if (msgYesNo("Unable to find the configuration file: %s\n"
			 "Do you want to try again?", buf)) {
		unmount("/mnt2", 0);
		break;
	    }
	    else
		goto fnord;
	}
	else {
	    Attribs *cattr = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
	    int i, j;

	    if (attr_parse(cattr, fd) == RET_FAIL) {
		dialog_clear();
		msgConfirm("Cannot parse configuration file %s!  Please verify your media.", cfg_file);
	    }
	    else {
		i = RET_SUCCESS;
		for (j = 0; cattr[j].name[0]; j++) {
		    int status;

		    if (call_possible_resword(cattr[j].name, cattr[j].value, &status)) {
			if (status != RET_SUCCESS) {
			    msgDebug("macro call to %s(%s) returns %d status!\n", cattr[j].name, cattr[j].value,
				     status);
			    i = status;
			}
		    }
		    else
			variable_set2(cattr[j].name, cattr[j].value);
		}
		if (i == RET_SUCCESS) {
		    dialog_clear();
		    msgConfirm("Configuration file %s loaded successfully!\n"
			       "Some parameters may now have new default values.", buf);
		}
		else if (i == RET_FAIL) {
		    dialog_clear();
		    msgConfirm("Configuration file %s loaded with some errors.\n", buf);
		}
	    }
	    close(fd);
	    safe_free(cattr);
	    unmount("/mnt2", 0);
	    break;
	}
    }
    return i;
}
