/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Coranth Gryphon.  All rights reserved.
 * Copyright (c) 1996
 *	Jordan K. Hubbard.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND
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
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

/* This doesn't change until FTP itself changes */

#define FTP_NAME	"ftp"
#define MOTD_FILE	"ftpmotd"

/* These change if we want to use different defaults */

#define FTP_UID		14
#define FTP_GID		5
#define FTP_GROUP	"operator"
#define FTP_UPLOAD	"incoming"
#define FTP_COMMENT	"Anonymous FTP Admin"
#define FTP_HOMEDIR	"/var/ftp"

#define ANONFTP_HELPFILE "anonftp"

/* Set up the structure to hold configuration information */
/* Note that this is only what we could fit onto the one screen */

typedef struct
{
    char homedir[64];             /* Home Dir for Anon FTP */
    char group[32];               /* Group */
    char uid[8];                  /* UID */
    char comment[64];             /* PWD Comment */
    char upload[32];              /* Upload Dir */
} FTPConf;

static FTPConf tconf;

#define ANONFTP_HOMEDIR_LEN       64
#define ANONFTP_COMMENT_LEN       64
#define ANONFTP_UPLOAD_LEN        32
#define ANONFTP_GROUP_LEN         32
#define ANONFTP_UID_LEN           8

static int      okbutton, cancelbutton;

/* What the screen size is meant to be */
#define ANONFTP_DIALOG_Y         0
#define ANONFTP_DIALOG_X         8
#define ANONFTP_DIALOG_WIDTH     COLS - 16
#define ANONFTP_DIALOG_HEIGHT    LINES - 2

static Layout layout[] = {
#define LAYOUT_UID		0
    { 2, 3, 8, ANONFTP_UID_LEN - 1,
      "UID:", "What user ID to assign to FTP Admin",
      tconf.uid, STRINGOBJ, NULL },
#define LAYOUT_GROUP		1
    { 2, 15, 15, ANONFTP_GROUP_LEN - 1,
      "Group:",  "Group name that ftp process belongs to",
      tconf.group, STRINGOBJ, NULL },
#define LAYOUT_COMMENT		2
    { 2, 35, 24, ANONFTP_COMMENT_LEN - 1,
      "Comment:", "Password file comment for FTP Admin",
      tconf.comment, STRINGOBJ, NULL },
#define LAYOUT_HOMEDIR		3
    { 9, 10, 43, ANONFTP_HOMEDIR_LEN - 1,
      "FTP Root Directory:",
      "The top directory to chroot to when doing anonymous ftp",
      tconf.homedir, STRINGOBJ, NULL },
#define LAYOUT_UPLOAD		4
    { 14, 20, 22, ANONFTP_UPLOAD_LEN - 1,
      "Upload Subdirectory:", "Designated sub-directory that holds uploads",
      tconf.upload, STRINGOBJ, NULL },
#define LAYOUT_OKBUTTON		5
    { 19, 15, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	6
    { 19, 35, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
    { 0 },
};

static int
createFtpUser(void)
{
    struct passwd *tpw;
    struct group  *tgrp;
    char pwline[256];
    char *tptr;
    int gid;
    FILE *fptr;
    
    if ((gid = atoi(tconf.group)) <= 0) {
	if (!(tgrp = getgrnam(tconf.group))) {
	    /* group does not exist, create it by name */
	    
	    tptr = msgGetInput("14", "What group ID to use for group %s ?", tconf.group);
	    if (tptr && *tptr && ((gid = atoi(tptr)) > 0)) {
		if ((fptr = fopen(_PATH_GROUP,"a"))) {
		    fprintf(fptr,"%s:*:%d:%s\n",tconf.group,gid,FTP_NAME);
		    fclose(fptr);
		}
	    }
	    else
		gid = FTP_GID;
	}
	else
	    gid = tgrp->gr_gid;
    }
    else if (!getgrgid(gid)) {
	/* group does not exist, create it by number */
	
	tptr = msgGetInput("14", "What group name to use for gid %d ?", gid);
	if (tptr && *tptr) {
	    SAFE_STRCPY(tconf.group, tptr);
	    if ((tgrp = getgrnam(tconf.group))) {
		gid = tgrp->gr_gid;
	    }
	    else if ((fptr = fopen(_PATH_GROUP,"a"))) {
		fprintf(fptr,"%s:*:%d:%s\n",tconf.group,gid,FTP_NAME);
		fclose(fptr);
	    }
	}
    }
    
    if ((tpw = getpwnam(FTP_NAME))) {
	if (tpw->pw_uid != FTP_UID)
	    msgConfirm("FTP user already exists with a different uid.");
	
	return DITEM_SUCCESS; 	/* succeeds if already exists */
    }
    
    sprintf(pwline, "%s:*:%s:%d::0:0:%s:%s:/nonexistent\n", FTP_NAME, tconf.uid, gid, tconf.comment, tconf.homedir);
    
    fptr = fopen(_PATH_MASTERPASSWD,"a");
    if (! fptr) {
	msgConfirm("Could not open master password file.");
	return DITEM_FAILURE;
    }
    fprintf(fptr, "%s", pwline);
    fclose(fptr);
    msgNotify("Remaking password file: %s", _PATH_MASTERPASSWD);
    vsystem("pwd_mkdb -p %s", _PATH_MASTERPASSWD);
    return DITEM_SUCCESS | DITEM_RESTORE;
}

/* This is it - how to get the setup values */
static int
anonftpOpenDialog(void)
{
    WINDOW              *ds_win;
    ComposeObj		*obj = NULL;
    int                 n = 0, cancel = FALSE;
    int                 max;
    char                title[80];
    WINDOW		*w = savescr();

    /* We need a curses window */
    if (!(ds_win = openLayoutDialog(ANONFTP_HELPFILE, " Anonymous FTP Configuration ",
			      ANONFTP_DIALOG_X, ANONFTP_DIALOG_Y, ANONFTP_DIALOG_WIDTH, ANONFTP_DIALOG_HEIGHT))) {
	beep();
	msgConfirm("Cannot open anonymous ftp dialog window!!");
	restorescr(w);
	return DITEM_FAILURE;
    }
    
    /* Draw a sub-box for the path configuration */
    draw_box(ds_win, ANONFTP_DIALOG_Y + 7, ANONFTP_DIALOG_X + 8,
	     ANONFTP_DIALOG_HEIGHT - 11, ANONFTP_DIALOG_WIDTH - 17,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    sprintf(title, " Path Configuration ");
    mvwaddstr(ds_win, ANONFTP_DIALOG_Y + 7, ANONFTP_DIALOG_X + 22, title);
    
    /** Initialize the config Data Structure **/
    bzero(&tconf, sizeof(tconf));
    
    SAFE_STRCPY(tconf.group, FTP_GROUP);
    SAFE_STRCPY(tconf.upload, FTP_UPLOAD);
    SAFE_STRCPY(tconf.comment, FTP_COMMENT);
    SAFE_STRCPY(tconf.homedir, FTP_HOMEDIR);
    sprintf(tconf.uid, "%d", FTP_UID);
    
    /* Some more initialisation before we go into the main input loop */
    obj = initLayoutDialog(ds_win, layout, ANONFTP_DIALOG_X, ANONFTP_DIALOG_Y, &max);

    cancelbutton = okbutton = 0;
    while (layoutDialogLoop(ds_win, layout, &obj, &n, max, &cancelbutton, &cancel));

    /* Clear this crap off the screen */
    delwin(ds_win);
    use_helpfile(NULL);
    restorescr(w);
    if (cancel)
	return DITEM_FAILURE;
    return DITEM_SUCCESS;
}

int
configAnonFTP(dialogMenuItem *self __unused)
{
    int i;


    if (msgYesNo("Anonymous FTP permits un-authenticated users to connect to the system\n"
		 "FTP server, if FTP service is enabled.  Anonymous users are\n"
		 "restricted to a specific subset of the file system, and the default\n"
		 "configuration provides a drop-box incoming directory to which uploads\n"
		 "are permitted.  You must separately enable both inetd(8), and enable\n"
		 "ftpd(8) in inetd.conf(5) for FTP services to be available.  If you\n"
		 "did not do so earlier, you will have the opportunity to enable inetd(8)\n"
		 "again later.\n\n"
		 "Do you wish to continue configuring anonymous FTP?")) {
	return DITEM_FAILURE;
    }
    
    /* Be optimistic */
    i = DITEM_SUCCESS;
    
    i = anonftpOpenDialog();
    if (DITEM_STATUS(i) != DITEM_SUCCESS) {
	msgConfirm("Configuration of Anonymous FTP cancelled per user request.");
	return i;
    }
    
    /*** Use defaults for any invalid values ***/
    if (atoi(tconf.uid) <= 0)
	sprintf(tconf.uid, "%d", FTP_UID);
    
    if (!tconf.group[0])
	SAFE_STRCPY(tconf.group, FTP_GROUP);
    
    if (!tconf.upload[0])
	SAFE_STRCPY(tconf.upload, FTP_UPLOAD);
    
    /*** If the user did not specify a directory, use default ***/
    
    if (tconf.homedir[strlen(tconf.homedir) - 1] == '/')
	tconf.homedir[strlen(tconf.homedir) - 1] = '\0';
    
    if (!tconf.homedir[0])
	SAFE_STRCPY(tconf.homedir, FTP_HOMEDIR);
    
    /*** If HomeDir does not exist, create it ***/
    
    if (!directory_exists(tconf.homedir))
	vsystem("mkdir -p %s", tconf.homedir);
    
    if (directory_exists(tconf.homedir)) {
	msgNotify("Configuring %s for use by anon FTP.", tconf.homedir);
	vsystem("chmod 555 %s && chown root.%s %s", tconf.homedir, tconf.group, tconf.homedir);
	vsystem("mkdir %s/bin && chmod 555 %s/bin", tconf.homedir, tconf.homedir);
	vsystem("cp /bin/ls %s/bin && chmod 111 %s/bin/ls", tconf.homedir, tconf.homedir);
	vsystem("cp /bin/date %s/bin && chmod 111 %s/bin/date", tconf.homedir, tconf.homedir);
	vsystem("mkdir %s/etc && chmod 555 %s/etc", tconf.homedir, tconf.homedir);
	vsystem("mkdir -p %s/pub", tconf.homedir);
	vsystem("mkdir -p %s/%s", tconf.homedir, tconf.upload);
	vsystem("chmod 1777 %s/%s", tconf.homedir, tconf.upload);
	
	if (DITEM_STATUS(createFtpUser()) == DITEM_SUCCESS) {
	    msgNotify("Copying password information for anon FTP.");
	    vsystem("awk -F: '{if ($3 < 10 || $1 == \"ftp\") print $0}' /etc/passwd > %s/etc/passwd && chmod 444 %s/etc/passwd", tconf.homedir, tconf.homedir);
	    vsystem("awk -F: '{if ($3 < 100) print $0}' /etc/group > %s/etc/group && chmod 444 %s/etc/group", tconf.homedir, tconf.homedir);
	    vsystem("chown -R root.%s %s/pub", tconf.group, tconf.homedir);
	}
	else {
	    msgConfirm("Unable to create FTP user!  Anonymous FTP setup failed.");
	    i = DITEM_FAILURE;
	}
	
	if (!msgYesNo("Create a welcome message file for anonymous FTP users?")) {
	    char cmd[256];
	    vsystem("echo Your welcome message here. > %s/etc/%s", tconf.homedir, MOTD_FILE);
	    sprintf(cmd, "%s %s/etc/%s", variable_get(VAR_EDITOR), tconf.homedir, MOTD_FILE);
	    if (!systemExecute(cmd))
		i = DITEM_SUCCESS;
	    else
		i = DITEM_FAILURE;
	}
    }
    else {
	msgConfirm("Invalid Directory: %s\n"
		   "Anonymous FTP will not be set up.", tconf.homedir);
	i = DITEM_FAILURE;
    }
    if (DITEM_STATUS(i) == DITEM_SUCCESS)
	variable_set2("anon_ftp", "YES", 0);
    return i | DITEM_RESTORE;
}
