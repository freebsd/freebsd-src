/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: apache.c,v 1.3 1995/10/22 23:20:44 jkh Exp $
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
#include "ui_objects.h"
#include "dir.h"
#include "dialog.priv.h"
#include "colors.h"
#include "sysinstall.h"

#define APACHE_BASE     "/usr/local/www"
#define APACHE_HELPFILE "apache.hlp"
#define APACHE_PACKAGE  "apache-0.8.14"

typedef struct
{
    char docroot[128];             /* DocumentRoot */
    char userdir[128];             /* UserDir */
    char readme[32];               /* ReadmeName */
    char email[64];                /* ServerAdmin */
    char hostname[64];             /* ServerName */
    char user[32];                 /* User */
    char group[32];                /* Group */
    char maxcon[8];                /* Max Connections */
} ApacheConf;

static ApacheConf tconf;

#define APACHE_DOCROOT_LEN      128
#define APACHE_USERDIR_LEN      128
#define APACHE_README_LEN        32
#define APACHE_HEADER_LEN        32
#define APACHE_EMAIL_LEN         64
#define APACHE_HOSTNAME_LEN      64
#define APACHE_USER_LEN          32
#define APACHE_GROUP_LEN         32
#define APACHE_MAXCON_LEN        8

static int      okbutton, cancelbutton;

/* What the screen size is meant to be */
#define APACHE_DIALOG_Y         0
#define APACHE_DIALOG_X         8
#define APACHE_DIALOG_WIDTH     COLS - 16
#define APACHE_DIALOG_HEIGHT    LINES - 2

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
{ 1, 2, 24, HOSTNAME_FIELD_LEN - 1,
      "Host Name:",
      "What name to report this host as to client browsers",
      tconf.hostname, STRINGOBJ, NULL },
#define LAYOUT_HOSTNAME 0

{ 1, 30, 30, APACHE_EMAIL_LEN - 1,
      "Email Address:",
      "The email address of the site maintainer, e.g. webmaster@bar.com",
      tconf.email, STRINGOBJ, NULL },
#define LAYOUT_EMAIL    1

{ 5, 3, 15, APACHE_USER_LEN - 1,
      "Default User:", "Default username for access to web pages",
      tconf.user, STRINGOBJ, NULL },
#define LAYOUT_USER             2

{ 5, 22, 15, APACHE_GROUP_LEN - 1,
      "Default Group:",  "Default group name for access to web pages",
      tconf.group, STRINGOBJ, NULL },
#define LAYOUT_GROUP    3

{ 5, 46, 13, APACHE_MAXCON_LEN - 1,
      "Max Connect:",  "Maximum number of concurrent http connections",
      tconf.maxcon, STRINGOBJ, NULL },
#define LAYOUT_MAXCON   4

{ 10, 10, 43, APACHE_DOCROOT_LEN - 1,
      "Root Document Path:",
      "The top directory that holds the system web pages",
      tconf.docroot, STRINGOBJ, NULL },
#define LAYOUT_DOCROOT          5

{ 14, 10, 18, APACHE_USERDIR_LEN - 1,
      "User Directory:",
      "Personal sub-directory that holds users' web pages (eg. ~/Web)",
      tconf.userdir, STRINGOBJ, NULL },
#define LAYOUT_USERDIR          6

{ 14, 35, 18, APACHE_README_LEN - 1,
      "Readme File:",
      "The name of the README file found in each directory",
      tconf.readme, STRINGOBJ, NULL },
#define LAYOUT_README           7

{ 19, 15, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_OKBUTTON         8

{ 19, 35, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON     9
{ NULL },
};

DMenu MenuApacheLanguages = {
DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
"Apache Languages Menu",
"This allows you to specify which languages are known by your httpd server.",
NULL,
NULL,
{
{ "English",          "English",
      DMENU_SET_VARIABLE, "APACHE_English=en", 0, 0 },
{ "French",           "French",
      DMENU_SET_VARIABLE, "APACHE_French=fr", 0, 0 },
{ "German",           "German",
      DMENU_SET_VARIABLE, "APACHE_German=de", 0, 0 },
{ "Italian",          "Italian",
      DMENU_SET_VARIABLE, "APACHE_Italian=it", 0, 0 },
{ "Japanese",         "Japanese",
      DMENU_SET_VARIABLE, "APACHE_Japanese=jp", 0, 0 },
{ NULL } },
};

/* This is it - how to get Apache setup values */
int
apacheOpenDialog()
{
    WINDOW              *ds_win;
    ComposeObj          *obj = NULL;
    ComposeObj          *first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret;
    int                 max;
    char                *tmp;
    char                help[FILENAME_MAX];
    char                title[80];
    
    /* We need a curses window */
    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0)
    {
        beep();
        msgConfirm("Cannot open apache dialog window!!");
        return(RET_SUCCESS);
    }
    
    /* Say where our help comes from */
    systemHelpFile(APACHE_HELPFILE, help);
    use_helpfile(help);
    
    /* Setup a nice screen for us to splat stuff onto */
    draw_box(ds_win, APACHE_DIALOG_Y, APACHE_DIALOG_X, APACHE_DIALOG_HEIGHT, APACHE_DIALOG_WIDTH, dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, APACHE_DIALOG_Y, APACHE_DIALOG_X + 20, " Apache HTTPD Configuration ");
    
    draw_box(ds_win, APACHE_DIALOG_Y + 9, APACHE_DIALOG_X + 8, APACHE_DIALOG_HEIGHT - 13, APACHE_DIALOG_WIDTH - 17,
             dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    sprintf(title, " Path Configuration ");
    mvwaddstr(ds_win, APACHE_DIALOG_Y + 9, APACHE_DIALOG_X + 22, title);
    
    /** Initialize the config Data Structure **/
    
    bzero(&tconf, sizeof(tconf));
    
    tmp = variable_get(VAR_DOMAINNAME);
    if (tmp)
    {
        sprintf(tconf.email, "webmaster@%s", tmp);
        sprintf(tconf.hostname, "www.%s", tmp);
    }
    
    strcpy(tconf.user, "guest");
    strcpy(tconf.group, "guest");
    strcpy(tconf.userdir, "public_html");
    strcpy(tconf.readme, "README");
    strcpy(tconf.maxcon, "150");
    sprintf(tconf.docroot, "%s/data", APACHE_BASE);
    
    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    
    n = 0;
#define lt layout[n]
    
    while (lt.help != NULL) {
        switch (lt.type) {
        case STRINGOBJ:
            lt.obj = NewStringObj(ds_win, lt.prompt, lt.var,
				  lt.y + APACHE_DIALOG_Y, lt.x + APACHE_DIALOG_X,
				  lt.len, lt.maxlen);
            break;
	    
        case BUTTONOBJ:
            lt.obj = NewButtonObj(ds_win, lt.prompt, lt.var,
				  lt.y + APACHE_DIALOG_Y, lt.x + APACHE_DIALOG_X);
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
	
	
        /* We are in the Hostname field - calculate the e-mail addr */
	
        if (n == LAYOUT_HOSTNAME) {
            if ((tmp = index(tconf.hostname, '.')) != NULL) {
                sprintf(tconf.email,"webmaster@%s",tmp+1);
                RefreshStringObj(layout[LAYOUT_EMAIL].obj);
            }
        }
	
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
installApache(char *unused)
{
    int i,maxcon;
    char tbuf[128], company[64], file[128], cmd[256];
    char *tptr;
    FILE *fptr;

    msgConfirm("Since you elected to install the WEB server, we'll now add the\n"
	       "Apache HTTPD package and set up a few configuration files.");
    i = package_add(APACHE_PACKAGE);
    if (i == RET_SUCCESS)
	i = apacheOpenDialog();

    if (i != RET_SUCCESS) {
	msgConfirm("Hmmmmm.  Looks like we weren't able to fetch the Apache WEB server\n"
		   "package.  You may wish to fetch and configure it by hand by looking\n"
		   "in /usr/ports/net/apache (in the ports collection) or looking for the\n"
		   "precompiled apache package in packages/networking/%s.", APACHE_PACKAGE);
        return (i);
    }
    
    /*** Fix defaults for invalid value ***/
    maxcon = atoi(tconf.maxcon);
    if (maxcon <= 0)
	maxcon = 150;
    
    if (! tconf.group[0])
	strcpy(tconf.group, "guest");
    if (! tconf.user[0])
	strcpy(tconf.user, "nobody");
    
    if (! tconf.readme[0])
	strcpy(tconf.readme, "README");
    if (! tconf.userdir[0])
	strcpy(tconf.userdir, "public_html");
    
    /*** If the user did not specify a directory, use default ***/
    
    if (tconf.docroot[strlen(tconf.docroot)-1] == '/')
	tconf.docroot[strlen(tconf.docroot)-1] = '\0';
    
    if (! tconf.docroot[0])
	sprintf(tconf.docroot,"%s/data",APACHE_BASE);
    
    /*** If DocRoot does not exist, create it ***/
    
    if (! directoryExists(tconf.docroot))
    {
	sprintf(cmd,"mkdir -p %s",tconf.docroot);
	system(cmd);
    }
    
    if (directoryExists(tconf.docroot))
    {
	sprintf(file,"%s/welcome.html",tconf.docroot);
	if (! file_readable(file))
	{
	    msgNotify("Creating sample web page...");
	    sleep(1);
	    
	    tptr = msgGetInput(NULL,
			       "What is your company name?");
	    if (tptr && strlen(tptr))
		strcpy(company,tptr);
	    else
		strcpy(company,"our Web Page");
	    
	    fptr = fopen(file,"w");
	    if (fptr)
	    {
		fprintf(fptr,"<CENTER>\n<TITLE>Welcome Page</TITLE>\n");
		fprintf(fptr,"<H1>Welcome to %s </H1>\n</CENTER>\n",company);
		fprintf(fptr,"<P><HR SIZE=4>\n<CENTER>\n");
		fprintf(fptr,"<A HREF=\"http://www.FreeBSD.org/What\">\n");
		fprintf(fptr,"<IMG SRC=\"./power.gif\" ALIGN=CENTER BORDER=0 ");
		fprintf(fptr," ALT=\"Powered by FreeBSD\"></A>\n");
		if (! tconf.email[0])
		{
		    if ((tptr = variable_get(VAR_DOMAINNAME)))
			sprintf(tconf.email,"root@%s",tptr);
		}
		if (tbuf[0])
		{
		    fprintf(fptr,"<ADDRESS><H4>\n");
		    fprintf(fptr,"    For questions or comments, send mail to:\n");
		    fprintf(fptr,"        <A HREF=\"mailto:%s\">%s</A>\n",
			    tconf.email,tconf.email);
		    fprintf(fptr,"</H4></ADDRESS>\n");
		}
		fprintf(fptr,"</CENTER>\n\n");
		fclose(fptr);
	    }
	    else
		msgConfirm("Unable to create sample Web Page.");
	}
    }
    else
	msgConfirm("Unable to create Document Root Directory.");
    
    
    msgNotify("Writing configuration files....");
    sleep(1);
    
    sprintf(file, "%s/config/access.conf", APACHE_BASE);
    if (file_readable(file))
    {
	sprintf(cmd,"mv -f %s %s.ORIG", file, file);
	system(cmd);
    }
    fptr = fopen(file,"w");
    if (fptr)
    {
	fprintf(fptr,"<Directory %s/cgi-bin>\n", APACHE_BASE);
	fprintf(fptr,"Options Indexes FollowSymLinks\n</Directory>\n\n");
	fprintf(fptr,"<Directory %s>\n", tconf.docroot);
	fprintf(fptr,"Options Indexes FollowSymLinks\nAllowOverride All\n");
	fprintf(fptr,"</Directory>\n\n");
	fclose(fptr);
    }
    else
	msgConfirm("Could not create %s",file);
    
    sprintf(file, "%s/config/httpd.conf", APACHE_BASE);
    if (file_readable(file))
    {
	sprintf(cmd,"mv -f %s %s.ORIG", file, file);
	system(cmd);
    }
    fptr = fopen(file,"w");
    if (fptr)
    {
	fprintf(fptr,"ServerType standalone\nPort 80\nTimeOut 400\n");
	fprintf(fptr,"ErrorLog logs/error_log\nTransferLog logs/access_log\n");
	fprintf(fptr,"PidFile /var/run/httpd.pid\n\nStartServers 5\n");
	fprintf(fptr,"MinSpareServers 5\nMaxSpareServers 10\n");
	fprintf(fptr,"MaxRequestsPerChild 30\nMaxClients %d\n\n",maxcon);
	fprintf(fptr,"User %s\nGroup %s\n\n",tconf.user,tconf.group);
	
	if (tconf.email[0])
	    fprintf(fptr,"ServerAdmin %s\n",tconf.email);
	if (tconf.hostname[0])
	    fprintf(fptr,"ServerName %s\n",tconf.hostname);
	
	fclose(fptr);
    }
    else
	msgConfirm("Could not create %s",file);
    
    sprintf(file, "%s/config/srm.conf", APACHE_BASE);
    if (file_readable(file))
    {
	sprintf(cmd,"mv -f %s %s.ORIG", file, file);
	system(cmd);
    }
    fptr = fopen(file,"w");
    if (fptr)
    {
	fprintf(fptr,"FancyIndexing on\nDirectoryIndex index.html\n");
	fprintf(fptr,"IndexIgnore */.??* *~ *# */HEADER* */%s* */RCS\n",
                tconf.readme);
	fprintf(fptr,"HeaderName HEADER\nDefaultType text/plain\n");
	fprintf(fptr,"AccessFileName .htaccess\n\n");
	fprintf(fptr,"AddEncoding x-compress Z\nAddEncoding x-gzip gz\n");
	fprintf(fptr,"DefaultIcon /icons/unknown.gif\n\n");
	fprintf(fptr,
		"AddIconByEncoding (CMP,/icons/compressed.gif) x-compress x-gzip\n");
	fprintf(fptr,"AddIconByType (TXT,/icons/text.gif) text/*\n");
	fprintf(fptr,"AddIconByType (IMG,/icons/image2.gif) image/*\n");
	fprintf(fptr,"AddIconByType (SND,/icons/sound2.gif) audio/*\n");
	fprintf(fptr,"AddIconByType (VID,/icons/movie.gif) video/*\n\n");
	
	fprintf(fptr,"AddIcon /icons/text.gif .ps .shtml\n");
	fprintf(fptr,"AddIcon /icons/movie.gif .mpg .qt\n");
	fprintf(fptr,"AddIcon /icons/binary.gif .bin\n");
	fprintf(fptr,"AddIcon /icons/burst.gif .wrl\n");
	fprintf(fptr,"AddIcon /icons/binhex.gif .hqx .sit\n");
	fprintf(fptr,"AddIcon /icons/uu.gif .uu\n");
	fprintf(fptr,"AddIcon /icons/tar.gif .tar\n");
	fprintf(fptr,"AddIcon /icons/back.gif ..\n");
	fprintf(fptr,"AddIcon /icons/dir.gif ^^DIRECTORY^^\n");
	fprintf(fptr,"AddIcon /icons/blank.gif ^^BLANKICON^^\n\n");
	
	fprintf(fptr,"ScriptAlias /cgi_bin/ %s/cgi_bin/\n",APACHE_BASE);
	fprintf(fptr,"Alias /icons/ %s/icons/\n",APACHE_BASE);
	fprintf(fptr,"DocumentRoot %s\n",tconf.docroot);
	fprintf(fptr,"UserDir %s\nReadmeName %s\n\n",tconf.userdir,tconf.readme);
	
	fclose(fptr);
    }
    else
	msgConfirm("Could not create %s",file);
    
    return i;
}
