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


#define APACHE_HELPFILE "apache"
#define FREEBSD_GIF	"http://www.freebsd.org/gifs/powerlogo.gif"

/* These change if the package uses different defaults */

#define DEFAULT_USER	"bin"
#define DEFAULT_GROUP	"bin"
#define WELCOME_FILE	"index.html"
#define USER_HOMEDIR	"public_html"

#define APACHE_BASE	"/usr/local/www"
#define DATA_SUBDIR	"data"
#define CONFIG_SUBDIR	"server/conf"

#define LOGS_SUBDIR	"/var/log"
#define ACCESS_LOGNAME	"httpd.access"
#define ERROR_LOGNAME	"httpd.error"

/* Set up the structure to hold configuration information */
/* Note that this is only what we could fit onto the one screen */

typedef struct {
    char docroot[128];             /* DocumentRoot */
    char userdir[128];             /* UserDir */
    char welcome[32];              /* Welcome Doc */
    char email[64];                /* ServerAdmin */
    char hostname[64];             /* ServerName */
    char logdir[64];		   /* Where to put Logs */
    char accesslog[32];	   	   /* access_log */
    char errorlog[32];		   /* error_log */
    char defuser[16];		   /* default user id */
    char defgroup[16];		   /* default group id */
} ApacheConf;

static ApacheConf tconf;

#define APACHE_DOCROOT_LEN      128
#define APACHE_USERDIR_LEN      128
#define APACHE_WELCOME_LEN       32
#define APACHE_EMAIL_LEN         64
#define APACHE_HOSTNAME_LEN      64
#define APACHE_LOGDIR_LEN        64
#define APACHE_ACCESSLOG_LEN     32
#define APACHE_ERRORLOG_LEN      32
#define APACHE_DEFUSER_LEN       16
#define APACHE_DEFGROUP_LEN      16

static int      okbutton, cancelbutton;

/* What the screen size is meant to be */
#define APACHE_DIALOG_Y         0
#define APACHE_DIALOG_X         0
#define APACHE_DIALOG_WIDTH     COLS
#define APACHE_DIALOG_HEIGHT    LINES - 2

static Layout layout[] = {
#define LAYOUT_HOSTNAME 0
    { 1, 2, 30, HOSTNAME_FIELD_LEN - 1,
      "Host Name:",
      "What name to report this host as to client browsers",
      tconf.hostname, STRINGOBJ, NULL },
#define LAYOUT_EMAIL    1
    { 1, 40, 32, APACHE_EMAIL_LEN - 1,
      "Email Address:",
      "The email address of the site maintainer, e.g. webmaster@bar.com",
      tconf.email, STRINGOBJ, NULL },
#define LAYOUT_WELCOME           2
    { 5, 5, 20, APACHE_WELCOME_LEN - 1,
      "Default Document:",
      "The name of the default document found in each directory",
      tconf.welcome, STRINGOBJ, NULL },
#define LAYOUT_DEFUSER          3
    { 5, 40, 14, APACHE_DEFUSER_LEN - 1,
      "Default UserID:", "Default UID for access to web pages",
      tconf.defuser, STRINGOBJ, NULL },
#define LAYOUT_DEFGROUP          4
    { 5, 60, 14, APACHE_DEFGROUP_LEN - 1,
      "Default Group ID:", "Default GID for access to web pages",
      tconf.defgroup, STRINGOBJ, NULL },
#define LAYOUT_DOCROOT          5
    { 10, 4, 36, APACHE_DOCROOT_LEN - 1,
      "Root Document Path:",
      "The top directory that holds the system web pages",
      tconf.docroot, STRINGOBJ, NULL },
#define LAYOUT_USERDIR          6
    { 10, 50, 14, APACHE_USERDIR_LEN - 1,
      "User Directory:",
      "Personal sub-directory that holds users' web pages (eg. ~/Web)",
      tconf.userdir, STRINGOBJ, NULL },
#define LAYOUT_LOGDIR           7
    { 14, 4, 28, APACHE_LOGDIR_LEN - 1,
      "Log Dir:", "Directory to put httpd log files",
      tconf.logdir, STRINGOBJ, NULL },
#define LAYOUT_ACCESSLOG    8
    { 14, 38, 16, APACHE_ACCESSLOG_LEN - 1,
      "Access Log:",  "Name of log file to report access",
      tconf.accesslog, STRINGOBJ, NULL },
#define LAYOUT_ERRORLOG   9
    { 14, 60, 16, APACHE_ERRORLOG_LEN - 1,
      "Error Log:",  "Name of log file to report errors",
      tconf.errorlog, STRINGOBJ, NULL },
#define LAYOUT_OKBUTTON         10
    { 19, 15, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON     11
    { 19, 45, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
    { NULL },
};

/* This is it - how to get Apache setup values */
static int
apacheOpenDialog(void)
{
    WINDOW              *ds_win;
    ComposeObj		*obj = NULL;
    int                 n = 0, cancel = FALSE;
    int                 max;
    char                *tmp;
    char                title[80];
    
    /* We need a curses window */
    if (!(ds_win = openLayoutDialog(APACHE_HELPFILE, " Apache HTTPD Configuration ",
				    APACHE_DIALOG_X, APACHE_DIALOG_Y, APACHE_DIALOG_WIDTH, APACHE_DIALOG_HEIGHT))) {
        beep();
        msgConfirm("Cannot open apache dialog window!!");
        return DITEM_SUCCESS;
    }
    
    /* Draw a sub-box for the path configuration */
    draw_box(ds_win, APACHE_DIALOG_Y + 9, APACHE_DIALOG_X + 1, APACHE_DIALOG_HEIGHT - 13, APACHE_DIALOG_WIDTH - 2,
             dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    sprintf(title, " Path Configuration ");
    mvwaddstr(ds_win, APACHE_DIALOG_Y + 9, APACHE_DIALOG_X + 22, title);
    
    /** Initialize the config Data Structure **/
    bzero(&tconf, sizeof(tconf));
    
    tmp = variable_get(VAR_DOMAINNAME);
    if (tmp) {
        sprintf(tconf.email, "webmaster@%s", tmp);
        sprintf(tconf.hostname, "www.%s", tmp);
    }
    
    SAFE_STRCPY(tconf.defuser, DEFAULT_USER);
    SAFE_STRCPY(tconf.defgroup, DEFAULT_GROUP);
    
    SAFE_STRCPY(tconf.userdir, USER_HOMEDIR);
    SAFE_STRCPY(tconf.welcome, WELCOME_FILE);
    
    SAFE_STRCPY(tconf.logdir, LOGS_SUBDIR);
    SAFE_STRCPY(tconf.accesslog, ACCESS_LOGNAME);
    SAFE_STRCPY(tconf.errorlog, ERROR_LOGNAME);
    
    sprintf(tconf.docroot, "%s/%s", APACHE_BASE,DATA_SUBDIR);
    
    /* Some more initialisation before we go into the main input loop */
    obj = initLayoutDialog(ds_win, layout, APACHE_DIALOG_X, APACHE_DIALOG_Y, &max);

    cancelbutton = okbutton = 0;
    while (layoutDialogLoop(ds_win, layout, &obj, &n, max, &cancelbutton, &cancel)) {
	if (n == LAYOUT_HOSTNAME) {
	    if ((tmp = index(tconf.hostname, '.')) != NULL) {
		sprintf(tconf.email, "webmaster@%s", tmp + 1);
		RefreshStringObj(layout[LAYOUT_EMAIL].obj);
	    }
	}
    }

    /* Clear this crap off the screen */
    dialog_clear_norefresh();
    use_helpfile(NULL);
    
    if (cancel)
	return DITEM_FAILURE | DITEM_RESTORE;
    return DITEM_SUCCESS | DITEM_RESTORE;
}

int
configApache(dialogMenuItem *self)
{
    int i;
    char company[64], file[128];
    char *tptr;
    FILE *fptr;
    
    /* Be optimistic */
    i = DITEM_SUCCESS;

    dialog_clear_norefresh();
    msgConfirm("Since you elected to install the WEB server, we'll now add the\n"
	       "Apache HTTPD package and set up a few configuration files.");
    i = package_add(PACKAGE_APACHE);
    if (DITEM_STATUS(i) != DITEM_SUCCESS) {
	msgConfirm("Hmmmmm.  Looks like we weren't able to fetch the Apache WEB server\n"
		   "package.  You may wish to fetch and configure it by hand by looking\n"
		   "in /usr/ports/net/apache (in the ports collection) or looking for the\n"
		   "precompiled apache package in packages/networking/%s.", PACKAGE_APACHE);
        return i | DITEM_RESTORE;
    }

    dialog_clear_norefresh();
    i = apacheOpenDialog();
    if (DITEM_STATUS(i) != DITEM_SUCCESS) {
	msgConfirm("Configuration of the Apache WEB server was cancelled per\n"
		   "user request.");
	return i;
    }
    /*** Fix defaults for invalid value ***/
    if (!tconf.logdir[0])
	SAFE_STRCPY(tconf.logdir, LOGS_SUBDIR);
    if (!tconf.accesslog[0])
	SAFE_STRCPY(tconf.accesslog, ACCESS_LOGNAME);
    if (!tconf.errorlog[0])
	SAFE_STRCPY(tconf.errorlog, ERROR_LOGNAME);
    
    if (!tconf.welcome[0])
	SAFE_STRCPY(tconf.welcome, WELCOME_FILE);
    if (!tconf.userdir[0])
	SAFE_STRCPY(tconf.userdir, USER_HOMEDIR);
    
    if (!tconf.defuser[0])
	SAFE_STRCPY(tconf.defuser, DEFAULT_USER);
    if (!tconf.defgroup[0])
	SAFE_STRCPY(tconf.defgroup, DEFAULT_GROUP);
    
    /*** If the user did not specify a directory, use default ***/
    
    if (tconf.docroot[strlen(tconf.docroot) - 1] == '/')
	tconf.docroot[strlen(tconf.docroot) - 1] = '\0';
    
    if (!tconf.docroot[0])
	sprintf(tconf.docroot, "%s/%s", APACHE_BASE, DATA_SUBDIR);
    
    /*** If DocRoot does not exist, create it ***/
    
    if (!directory_exists(tconf.docroot))
	vsystem("mkdir -p %s", tconf.docroot);
    
    if (directory_exists(tconf.docroot)) {
	sprintf(file, "%s/%s", tconf.docroot, tconf.welcome);
	if (!file_readable(file)) {
	    tptr = msgGetInput(NULL, "What is your company name?");
	    if (tptr && tptr[0])
		SAFE_STRCPY(company, tptr);
	    else
		SAFE_STRCPY(company, "our Web Page");
	    
	    msgNotify("Creating sample web page...");
	    fptr = fopen(file,"w");
	    if (fptr) {
		fprintf(fptr, "<CENTER>\n<TITLE>Welcome Page</TITLE>\n");
		fprintf(fptr, "<H1>Welcome to %s </H1>\n</CENTER>\n",company);
		fprintf(fptr, "<P><HR SIZE=4>\n<CENTER>\n");
		fprintf(fptr, "<A HREF=\"http://www.FreeBSD.org/What\">\n");
		fprintf(fptr, "<IMG SRC=\"%s\" ALIGN=CENTER BORDER=0 ", FREEBSD_GIF);
		fprintf(fptr, " ALT=\"Powered by FreeBSD\"></A>\n");
		if (!tconf.email[0]) {
		    if ((tptr = variable_get(VAR_DOMAINNAME)))
			sprintf(tconf.email, "root@%s", tptr);
		}
		if (tconf.email[0]) {
		    fprintf(fptr, "<ADDRESS><H4>\n");
		    fprintf(fptr, "    For questions or comments, please send mail to:\n");
		    fprintf(fptr, "        <A HREF=\"mailto:%s\">%s</A>\n",
			    tconf.email, tconf.email);
		    fprintf(fptr, "</H4></ADDRESS>\n");
		}
		fprintf(fptr, "</CENTER>\n\n");
		fclose(fptr);
	    }
	    else {
		msgConfirm("Unable to create sample Web Page.");
		i = DITEM_FAILURE;
	    }
	}
    }
    else {
	msgConfirm("Unable to create Document Root Directory.");
	i = DITEM_FAILURE;
    }
    
    msgNotify("Writing configuration files....");
    
    (void)vsystem("mkdir -p %s/%s", APACHE_BASE, CONFIG_SUBDIR);
    sprintf(file, "%s/%s/access.conf", APACHE_BASE, CONFIG_SUBDIR);
    if (file_readable(file))
	vsystem("mv -f %s %s.ORIG", file, file);
    
    fptr = fopen(file,"w");
    if (fptr) {
	fprintf(fptr, "<Directory %s/cgi-bin>\n", APACHE_BASE);
	fprintf(fptr, "Options Indexes FollowSymLinks\n");
	fprintf(fptr, "</Directory>\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "<Directory %s>\n", tconf.docroot);
	fprintf(fptr, "Options Indexes FollowSymLinks\n");
	fprintf(fptr, "AllowOverride All\n");
	fprintf(fptr, "</Directory>\n");
	fprintf(fptr, "\n");
	fclose(fptr);
    }
    else {
	msgConfirm("Could not create %s",file);
	i = DITEM_FAILURE;
    }
    
    sprintf(file, "%s/%s/httpd.conf", APACHE_BASE, CONFIG_SUBDIR);
    if (file_readable(file))
	vsystem("mv -f %s %s.ORIG", file, file);
    
    fptr = fopen(file,"w");
    if (fptr) {
	fprintf(fptr, "ServerType standalone\n");
	fprintf(fptr, "Port 80\n");
	fprintf(fptr, "TimeOut 400\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "ErrorLog %s/%s\n", LOGS_SUBDIR, ERROR_LOGNAME);
	fprintf(fptr, "TransferLog %s/%s\n", LOGS_SUBDIR, ACCESS_LOGNAME);
	fprintf(fptr, "PidFile /var/run/httpd.pid\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "StartServers 5\n");
	fprintf(fptr, "MinSpareServers 5\n");
	fprintf(fptr, "MaxSpareServers 10\n");
	fprintf(fptr, "MaxRequestsPerChild 30\n");
	fprintf(fptr, "MaxClients 150\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "User %s\n",tconf.defuser);
	fprintf(fptr, "Group %s\n",tconf.defgroup);
	fprintf(fptr, "\n");
	
	if (tconf.email[0])
	    fprintf(fptr, "ServerAdmin %s\n", tconf.email);
	if (tconf.hostname[0])
	    fprintf(fptr, "ServerName %s\n", tconf.hostname);
	
	fclose(fptr);
    }
    else {
	msgConfirm("Could not create %s",file);
	i = DITEM_FAILURE;
    }
    
    sprintf(file, "%s/%s/srm.conf", APACHE_BASE, CONFIG_SUBDIR);
    if (file_readable(file))
	vsystem("mv -f %s %s.ORIG", file, file);
    fptr = fopen(file,"w");
    if (fptr) {
	fprintf(fptr, "FancyIndexing on\n");
	fprintf(fptr, "DefaultType text/plain\n");
	fprintf(fptr, "IndexIgnore */.??* *~ *# */HEADER* */README* */RCS\n");
	fprintf(fptr, "HeaderName HEADER\n");
	fprintf(fptr, "ReadmeName README\n");
	fprintf(fptr, "AccessFileName .htaccess\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "AddEncoding x-compress Z\n");
	fprintf(fptr, "AddEncoding x-gzip gz\n");
	fprintf(fptr, "DefaultIcon /icons/unknown.gif\n");
	fprintf(fptr, "\n");
	fprintf(fptr, "AddIconByEncoding (CMP,/icons/compressed.gif) x-compress x-gzip\n");
	fprintf(fptr, "AddIconByType (TXT,/icons/text.gif) text/*\n");
	fprintf(fptr, "AddIconByType (IMG,/icons/image2.gif) image/*\n");
	fprintf(fptr, "AddIconByType (SND,/icons/sound2.gif) audio/*\n");
	fprintf(fptr, "AddIconByType (VID,/icons/movie.gif) video/*\n");
	fprintf(fptr, "\n");
	
	fprintf(fptr, "AddIcon /icons/text.gif .ps .shtml\n");
	fprintf(fptr, "AddIcon /icons/movie.gif .mpg .qt\n");
	fprintf(fptr, "AddIcon /icons/binary.gif .bin\n");
	fprintf(fptr, "AddIcon /icons/burst.gif .wrl\n");
	fprintf(fptr, "AddIcon /icons/binhex.gif .hqx .sit\n");
	fprintf(fptr, "AddIcon /icons/uu.gif .uu\n");
	fprintf(fptr, "AddIcon /icons/tar.gif .tar\n");
	fprintf(fptr, "AddIcon /icons/back.gif ..\n");
	fprintf(fptr, "AddIcon /icons/dir.gif ^^DIRECTORY^^\n");
	fprintf(fptr, "AddIcon /icons/blank.gif ^^BLANKICON^^\n");
	fprintf(fptr, "\n");
	
	fprintf(fptr, "ScriptAlias /cgi_bin/ %s/cgi_bin/\n",APACHE_BASE);
	fprintf(fptr, "Alias /icons/ %s/icons/\n",APACHE_BASE);
	fprintf(fptr, "DocumentRoot %s\n",tconf.docroot);
	fprintf(fptr, "UserDir %s\n", tconf.userdir);
	fprintf(fptr, "DirectoryIndex %s\n", tconf.welcome);
	fprintf(fptr, "\n");
	
	fclose(fptr);
    }
    else {
	msgConfirm("Could not create %s",file);
	i = DITEM_FAILURE;
    }
    if (DITEM_STATUS(i) == DITEM_SUCCESS)
	variable_set2("apache_httpd", "YES");
    return i | DITEM_RESTORE;
}
