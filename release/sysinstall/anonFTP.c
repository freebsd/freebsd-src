/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: anonFTP.c,v 1.9 1996/03/23 07:21:28 jkh Exp $
 *
 * Copyright (c) 1995
 *	Coranth Gryphon.  All rights reserved.
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
 *	This product includes software developed by Coranth Gryphon
 *	for the FreeBSD Project.
 * 4. The name of Coranth Gryphon or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CORANTH GRYPHON ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CORANTH GRYPHON OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#include <dialog.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "ui_objects.h"
#include "dir.h"
#include "dialog.priv.h"
#include "colors.h"
#include "sysinstall.h"

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

/* The screen layout structure */
typedef struct _layout {
    int         y;              /* x & Y co-ordinates */
    int         x;
    int         len;            /* The size of the dialog on the screen */
    int         maxlen;         /* How much the user can type in ... */
    char        *prompt;        /* The string for the prompt */
    char        *help;          /* The display for the help line */
    void        *var;           /* The var to set when this changes */
    int         type;           /* The type of the dialog to create */
    void        *obj;           /* The obj pointer returned by libdialog */
} Layout;

static Layout layout[] = {
    { 2, 3, 8, ANONFTP_UID_LEN - 1,
      "UID:", "What user ID to assign to FTP Admin",
      tconf.uid, STRINGOBJ, NULL },
#define LAYOUT_UID	1
    
    { 2, 15, 15, ANONFTP_GROUP_LEN - 1,
      "Group:",  "Group name that ftp process belongs to",
      tconf.group, STRINGOBJ, NULL },
#define LAYOUT_GROUP	2
  
    { 2, 35, 24, ANONFTP_COMMENT_LEN - 1,
      "Comment:", "Password file comment for FTP Admin",
      tconf.comment, STRINGOBJ, NULL },
#define LAYOUT_COMMENT	3
  
    { 9, 10, 43, ANONFTP_HOMEDIR_LEN - 1,
      "FTP Root Directory:",
      "The top directory to chroot to when doing anonymous ftp",
      tconf.homedir, STRINGOBJ, NULL },
#define LAYOUT_HOMEDIR	4
  
    { 14, 20, 22, ANONFTP_UPLOAD_LEN - 1,
      "Upload Subdirectory:", "Designated sub-directory that holds uploads",
      tconf.upload, STRINGOBJ, NULL },
#define LAYOUT_UPLOAD	5
  
    { 19, 15, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_OKBUTTON		6
  
    { 19, 35, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	7
    { NULL },
};

int
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
	    strcpy(tconf.group, tptr);
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
	
	return (RET_SUCCESS); 	/* succeeds if already exists */
    }
    
    sprintf(pwline, "%s::%s:%d::0:0:%s:%s:/bin/date\n", FTP_NAME, tconf.uid, gid, tconf.comment, tconf.homedir);
    
    fptr = fopen(_PATH_MASTERPASSWD,"a");
    if (! fptr) {
	msgConfirm("Could not open master password file.");
	return (RET_FAIL);
    }
    fprintf(fptr, pwline);
    fclose(fptr);
    msgNotify("Remaking password file: %s", _PATH_MASTERPASSWD);
    vsystem("pwd_mkdb -p %s", _PATH_MASTERPASSWD);
    
    return (RET_SUCCESS);
}

/* This is it - how to get the setup values */
int
anonftpOpenDialog(void)
{
    WINDOW              *ds_win;
    ComposeObj          *obj = NULL;
    ComposeObj          *first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret;
    int                 max;
    char                help[FILENAME_MAX];
    char                title[80];
    
    /* We need a curses window */
    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0) {
	beep();
	msgConfirm("Cannot open anonymous ftp dialog window!!");
	return(RET_FAIL);
    }
    
    /* Say where our help comes from */
    systemHelpFile(ANONFTP_HELPFILE, help);
    use_helpfile(help);
    
    /* Setup a nice screen for us to splat stuff onto */
    draw_box(ds_win, ANONFTP_DIALOG_Y, ANONFTP_DIALOG_X, ANONFTP_DIALOG_HEIGHT, ANONFTP_DIALOG_WIDTH, dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, ANONFTP_DIALOG_Y, ANONFTP_DIALOG_X + 20, " Anonymous FTP Configuration ");
    
    draw_box(ds_win, ANONFTP_DIALOG_Y + 7, ANONFTP_DIALOG_X + 8, ANONFTP_DIALOG_HEIGHT - 11, ANONFTP_DIALOG_WIDTH - 17,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    sprintf(title, " Path Configuration ");
    mvwaddstr(ds_win, ANONFTP_DIALOG_Y + 7, ANONFTP_DIALOG_X + 22, title);
    
    /** Initialize the config Data Structure **/
    
    bzero(&tconf, sizeof(tconf));
    
    strcpy(tconf.group, FTP_GROUP);
    strcpy(tconf.upload, FTP_UPLOAD);
    strcpy(tconf.comment, FTP_COMMENT);
    strcpy(tconf.homedir, FTP_HOMEDIR);
    sprintf(tconf.uid, "%d", FTP_UID);
    
    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    
    n = 0;
#define lt layout[n]
    
    while (lt.help != NULL) {
	switch (lt.type) {
	case STRINGOBJ:
	  lt.obj = NewStringObj(ds_win, lt.prompt, lt.var,
				lt.y + ANONFTP_DIALOG_Y, lt.x + ANONFTP_DIALOG_X,
				lt.len, lt.maxlen);
	  break;
	  
	case BUTTONOBJ:
	  lt.obj = NewButtonObj(ds_win, lt.prompt, lt.var,
				lt.y + ANONFTP_DIALOG_Y, lt.x + ANONFTP_DIALOG_X);
	  break;
	  
	default:
	  msgFatal("Don't support this object yet!");
	}
	AddObj(&obj, lt.type, (void *) lt.obj);
	n++;
    }
    max = n - 1;
    
    /* Find the last object we can traverse to */
    last = obj;
    while (last->next)
	last = last->next;
    
    /* Find the first object in the list */
    first = obj;
    while (first->prev)
	first = first->prev;
    
    /* Some more initialisation before we go into the main input loop */
    n = 0;
    cancelbutton = 0;
    cancel = FALSE;
    okbutton = 0;
    
    /* Incoming user data - DUCK! */
    while (!quit) {
	char help_line[80];
	int i, len = strlen(lt.help);
	
	/* Display the help line at the bottom of the screen */
	for (i = 0; i < 79; i++)
	    help_line[i] = (i < len) ? lt.help[i] : ' ';
	    help_line[i] = '\0';
	    use_helpline(help_line);
	    display_helpline(ds_win, LINES - 1, COLS - 1);
	    
	    /* Ask for libdialog to do its stuff */
	    ret = PollObj(&obj);
	    
	    /* Handle special case stuff that libdialog misses. Sigh */
	    switch (ret) {
		/* Bail out */
	    case SEL_ESC:
		quit = TRUE, cancel=TRUE;
		break;
	      
		/* This doesn't work for list dialogs. Oh well. Perhaps
		   should special case the move from the OK button ``up''
		   to make it go to the interface list, but then it gets
		   awkward for the user to go back and correct screw up's
		   in the per-interface section */
	      
	    case KEY_UP:
		if (obj->prev !=NULL ) {
		    obj = obj->prev;
		    --n;
		} else {
		    obj = last;
		    n = max;
		}
		break;
	      
	    case KEY_DOWN:
		if (obj->next != NULL) {
		    obj = obj->next;
		    ++n;
		} else {
		    obj = first;
		    n = 0;
		}
		break;
		
	    case SEL_TAB:
		if (n < max)
		    ++n;
		else
		    n = 0;
		break;
	      
		/* The user has pressed enter over a button object */
	    case SEL_BUTTON:
		quit = TRUE;
		if (cancelbutton)
		    cancel = TRUE;
		break;
	      
		/* Generic CR handler */
	    case SEL_CR:
		if (n < max)
		    ++n;
		else
		    n = 0;
		break;
	      
	    case SEL_BACKTAB:
		if (n)
		    --n;
		else
		    n = max;
		break;
	      
	    case KEY_F(1):
		display_helpfile();
	    
	    /* They tried some key combination we don't support - tell them! */
	    default:
		beep();
	    }
    }
    
    /* Clear this crap off the screen */
    dialog_clear();
    refresh();
    use_helpfile(NULL);
    
    if (cancel)
	return RET_FAIL;
    return RET_SUCCESS;
}

int
configAnonFTP(dialogMenuItem *self)
{
    int i;
    
    /* Be optimistic */
    i = RET_SUCCESS;
    
    dialog_clear();
    i = anonftpOpenDialog();
    if (i != RET_SUCCESS) {
	dialog_clear();
	msgConfirm("Configuration of Anonymous FTP cancelled per user request.");
	return i;
    }
    
    /*** Use defaults for any invalid values ***/
    if (atoi(tconf.uid) <= 0)
	sprintf(tconf.uid, "%d", FTP_UID);
    
    if (!tconf.group[0])
	strcpy(tconf.group, FTP_GROUP);
    
    if (!tconf.upload[0])
	strcpy(tconf.upload, FTP_UPLOAD);
    
    /*** If the user did not specify a directory, use default ***/
    
    if (tconf.homedir[strlen(tconf.homedir)-1] == '/')
	tconf.homedir[strlen(tconf.homedir)-1] = '\0';
    
    if (!tconf.homedir[0])
	strcpy(tconf.homedir, FTP_HOMEDIR);
    
    /*** If HomeDir does not exist, create it ***/
    
    if (!directory_exists(tconf.homedir)) {
	vsystem("mkdir -p %s" ,tconf.homedir);
    }
    
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
	
	if (createFtpUser() == RET_SUCCESS) {
	    msgNotify("Copying password information for anon FTP.");
	    vsystem("cp /etc/pwd.db %s/etc && chmod 444 %s/etc/pwd.db", tconf.homedir, tconf.homedir);
	    vsystem("cp /etc/passwd %s/etc && chmod 444 %s/etc/passwd",tconf.homedir, tconf.homedir);
	    vsystem("cp /etc/group %s/etc && chmod 444 %s/etc/group", tconf.homedir, tconf.homedir);
	    vsystem("chown -R %s.%s %s/pub", FTP_NAME, tconf.group, tconf.homedir);
	}
	else {
	    dialog_clear();
	    msgConfirm("Unable to create FTP user!  Anonymous FTP setup failed.");
	    i = RET_FAIL;
	}
	
	dialog_clear();
	if (!msgYesNo("Create a welcome message file for anonymous FTP users?")) {
	    char cmd[256];
	    
	    dialog_clear();
	    msgNotify("Uncompressing the editor - please wait..");
	    vsystem("echo Your welcome message here. > %s/etc/%s", tconf.homedir, MOTD_FILE);
	    sprintf(cmd, "%s %s/etc/%s", variable_get(VAR_EDITOR), tconf.homedir, MOTD_FILE);
	    systemExecute(cmd);
	}
    }
    else {
	dialog_clear();
	msgConfirm("Invalid Directory: %s\n"
		   "Anonymous FTP will not be set up.", tconf.homedir);
	i = RET_FAIL;
    }
    return i;
}
