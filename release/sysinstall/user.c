/*
 * $Id: user.c,v 1.1.2.3 1996/12/14 22:23:30 jkh Exp $
 *
 * Copyright (c) 1996
 *      Jörg Wunsch. All rights reserved.
 *
 * The basic structure has been taken from tcpip.c, which is:
 *
 * Copyright (c) 1995
 *      Gary J Palmer. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/param.h>
#include <string.h>
#include <sysexits.h>
#include <dialog.h>
#include "ui_objects.h"
#include "dir.h"
#include "dialog.priv.h"
#include "colors.h"
#include "sysinstall.h"

/* The help file for the user mgmt screen */
#define USER_HELPFILE		"usermgmt"

/* XXX should they be moved out to sysinstall.h? */
#define GNAME_FIELD_LEN 32
#define GID_FIELD_LEN 10
#define GMEMB_FIELD_LEN 64

#define UID_FIELD_LEN 10
#define UGROUP_FIELD_LEN GNAME_FIELD_LEN
#define GECOS_FIELD_LEN 64
#define UMEMB_FIELD_LEN GMEMB_FIELD_LEN
#define HOMEDIR_FIELD_LEN 48
#define SHELL_FIELD_LEN 48
#define PASSWD_FIELD_LEN 32

/* These are nasty, but they make the layout structure a lot easier ... */

static char gname[GNAME_FIELD_LEN],
	gid[GID_FIELD_LEN],
	gmemb[GMEMB_FIELD_LEN],
	uname[UT_NAMESIZE + 1],
        passwd[PASSWD_FIELD_LEN],
	uid[UID_FIELD_LEN],
	ugroup[UGROUP_FIELD_LEN],
	gecos[GECOS_FIELD_LEN],
	umemb[UMEMB_FIELD_LEN],
	homedir[HOMEDIR_FIELD_LEN],
	shell[SHELL_FIELD_LEN];
#define CLEAR(v)	memset(v, 0, sizeof v)

static int	okbutton, cancelbutton;


/* What the screen size is meant to be */
#define USER_DIALOG_Y		0
#define USER_DIALOG_X		8
#define USER_DIALOG_WIDTH	COLS - 16
#define USER_DIALOG_HEIGHT	LINES - 2

/* The screen layout structure */
typedef struct _layout {
    int		y;		/* x & Y co-ordinates */
    int		x;
    int		len;		/* The size of the dialog on the screen */
    int		maxlen;		/* How much the user can type in ... */
    char	*prompt;	/* The string for the prompt */
    char	*help;		/* The display for the help line */
    void	*var;		/* The var to set when this changes */
    int		type;		/* The type of the dialog to create */
    void	*obj;		/* The obj pointer returned by libdialog */
} Layout;

/* The group configuration menu. */
static Layout groupLayout[] = {
#define LAYOUT_GNAME		0
{ 4, 10, 20, GNAME_FIELD_LEN - 1,
	"Group name:", "The alphanumeric name of the new group (mandatory)",
	gname, STRINGOBJ, NULL },
#define LAYOUT_GID		1
{ 4, 38, 10, GID_FIELD_LEN - 1,
	"GID:", "The numerical ID for this group (leave blank for automatic choice)",
	gid, STRINGOBJ, NULL },
#define LAYOUT_GMEMB		2
{ 11, 10, 40, GMEMB_FIELD_LEN - 1,
	"Group members:", "Who belongs to this group (i.e., gets access rights for it)",
	gmemb, STRINGOBJ, NULL },
#define LAYOUT_OKBUTTON		3
{ 18, 15, 0, 0,
	"OK", "Select this if you are happy with these settings",
	&okbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	4
{ 18, 35, 0, 0,
	"CANCEL", "Select this if you wish to cancel this screen",
	&cancelbutton, BUTTONOBJ, NULL },
{ NULL },
};

/* The user configuration menu. */
static Layout userLayout[] = {
#define LAYOUT_UNAME		0
{ 3, 6, UT_NAMESIZE, UT_NAMESIZE + 4,
	"Login ID:", "The login name of the new user (mandatory)",
	uname, STRINGOBJ, NULL },
#define LAYOUT_UID		1
{ 3, 23, 8, UID_FIELD_LEN - 1,
	"UID:", "The numerical ID for this user (leave blank for automatic choice)",
	uid, STRINGOBJ, NULL },
#define LAYOUT_UGROUP		2
{ 3, 33, 8, UGROUP_FIELD_LEN - 1,
	"Group:", "The login group name for this user (leave blank for automatic choice)",
	ugroup, STRINGOBJ, NULL },
#define LAYOUT_PASSWD		3
{ 3, 43, 15, PASSWD_FIELD_LEN - 1,
	"Password:", "The password for this user (enter this field with care!)",
	passwd, STRINGOBJ, NULL },
#define LAYOUT_GECOS		4
{ 8, 6, 33, GECOS_FIELD_LEN - 1,
	"Full name:", "The user's full name (comment)",
	gecos, STRINGOBJ, NULL },
#define LAYOUT_UMEMB		5
{ 8, 43, 15, UMEMB_FIELD_LEN - 1,
	"Member groups:", "The groups this user belongs to (i.e. gets access rights for)",
	umemb, STRINGOBJ, NULL },
#define LAYOUT_HOMEDIR		6
{ 13, 6, 20, HOMEDIR_FIELD_LEN - 1,
	"Home directory:", "The user's home directory (leave blank for default)",
	homedir, STRINGOBJ, NULL },
#define LAYOUT_SHELL		7
{ 13, 29, 29, SHELL_FIELD_LEN - 1,
	"Login shell:", "The user's login shell (leave blank for default)",
	shell, STRINGOBJ, NULL },
#define LAYOUT_U_OKBUTTON	8
{ 18, 15, 0, 0,
	"OK", "Select this if you are happy with these settings",
	&okbutton, BUTTONOBJ, NULL },
#define LAYOUT_U_CANCELBUTTON	9
{ 18, 35, 0, 0,
	"CANCEL", "Select this if you wish to cancel this screen",
	&cancelbutton, BUTTONOBJ, NULL },
{ NULL },
};

/* whine */
static void
feepout(char *msg)
{
    beep();
    dialog_notify(msg);
}

/* Check for the settings on the screen. */

static int
verifyGroupSettings(void)
{
    char tmp[256], *cp;
    long lgid;

    if (strlen(gname) == 0) {
	feepout("The group name field must not be empty!");
	return 0;
    }
    snprintf(tmp, 256, "pw group show -q -n %s > /dev/null", gname);
    if (vsystem(tmp) == 0) {
	feepout("This group name is already in use.");
	return 0;
    }
    if (strlen(gid) > 0) {
	lgid = strtol(gid, &cp, 10);
	if (lgid < 0 || lgid >= 65536 || (*cp != '\0' && !isspace(*cp))) {
	    feepout("The GID must be a number between 1 and 65535.");
	    return 0;
	}
    }
    if (strlen(gmemb) > 0) {
	if (strpbrk(gmemb, " \t") != NULL) {
	    feepout("The group member list must not contain any whitespace;\n"
		    "use commas to separate the names.");
	    return 0;
	}
#ifndef notyet  /* XXX */
	feepout("Sorry, the group member list feature\n"
		"is currently not yet implemented.");
	return 0;
#endif
    }

    return 1;
}

/*
 * Ask pw(8) to fill in the blanks for us.
 * Works solely on the global variables.
 */

static void
completeGroup(void)
{
    int pfd[2], i;
    char tmp[256], *cp;
    ssize_t l;
    size_t amnt;
    pid_t pid;
    char *vec[4] =
    {
	"pw", "group", "next", 0
    };

    pipe (pfd);
    if ((pid = fork()) == 0)
    {
	/* The kiddy. */
	dup2(pfd[1], 1);
	dup2(pfd[1], 2);
	for (i = getdtablesize(); i > 2; i--)
	    close(i);

	chroot(variable_get(VAR_INSTALL_ROOT));
	execv("/usr/sbin/pw", vec);
	msgDebug("Cannot execv() /usr/sbin/pw.\n");
	_exit(99);
    }
    else
    {
	/* The oldie. */
	close(pfd[1]);
	amnt = sizeof tmp;
	i = 0;
	while((l = read(pfd[0], &tmp[i], amnt)) > 0)
	{
	    amnt -= l;
	    i += l;
	    if (amnt == 0)
	    {
		close(pfd[0]);
		break;
	    }
	}
	close(pfd[0]);
	tmp[i] = '\0';
	waitpid(pid, &i, 0);
	if (WIFSIGNALED(i) || WEXITSTATUS(i) != 0)
	    /* ignore by now */
	    return;
	if ((cp = strchr(tmp, '\n')) != NULL)
	    *cp = '\0';
	strncpy(gid, tmp, sizeof gid);
    }
}

static void
addGroup(WINDOW *ds_win)
{
    char tmp[256];
    int pfd[2], i;
    ssize_t l;
    size_t amnt;
    pid_t pid;
    char *vec[8] =
    {
	"pw", "group", "add", "-n", 0, "-g", 0, 0
    };
#define VEC_GNAME 4
#define VEC_GID 6

    msgNotify("Adding group \"%s\"...", gname);

    pipe (pfd);
    if ((pid = fork()) == 0)
    {
	/* The kiddy. */
	dup2(pfd[1], 1);
	dup2(pfd[1], 2);
	for (i = getdtablesize(); i > 2; i--)
	    close(i);

	vec[VEC_GNAME] = gname;

	if (strlen(gid) > 0)
	    vec[VEC_GID] = gid;
	else
	    vec[VEC_GID - 1] = 0;

	chroot(variable_get(VAR_INSTALL_ROOT));
	execv("/usr/sbin/pw", vec);
	msgDebug("Cannot execv() /usr/sbin/pw.\n");
	_exit(99);
    }
    else
    {
	/* The oldie. */
	close(pfd[1]);
	amnt = sizeof tmp;
	i = 0;
	while((l = read(pfd[0], &tmp[i], amnt)) > 0)
	{
	    amnt -= l;
	    i += l;
	    if (amnt == 0)
	    {
		close(pfd[0]);
		break;
	    }
	}
	close(pfd[0]);
	tmp[i] = '\0';
	waitpid(pid, &i, 0);
	if (WIFSIGNALED(i))
	    msgDebug("pw(8) exited with signal %d.\n", WTERMSIG(i));
	else if(WEXITSTATUS(i))
	{
	    i = 0;
	    if(strncmp(tmp, "pw: ", 4) == 0)
		i = 4;
	    tmp[sizeof tmp - 1] = '\0';	/* sanity */
	    msgConfirm("The `pw' command exited with an error status.\n"
		       "Its error message was:\n\n%s",
		       &tmp[i]);
	}
    }
#undef VEC_GNAME
#undef VEC_GID
}

int
userAddGroup(dialogMenuItem *self)
{
    WINDOW              *ds_win, *save;
    ComposeObj          *obj = NULL;
    ComposeObj		*first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret;
    int			max, firsttime=TRUE;
    char		help[FILENAME_MAX];

    if (RunningAsInit && !strstr(variable_get(SYSTEM_STATE), "install")) {
        msgConfirm("This option may only be used after the system is installed, sorry!");
        return DITEM_FAILURE;
    }

    save = savescr();
    dialog_clear_norefresh();
    /* We need a curses window */
    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0)
	msgFatal("Cannot open AddGroup dialog window!!");

    /* Say where our help comes from */
    use_helpfile(systemHelpFile(USER_HELPFILE, help));

    /* Setup a nice screen for us to splat stuff onto */
    draw_box(ds_win, USER_DIALOG_Y, USER_DIALOG_X, USER_DIALOG_HEIGHT, USER_DIALOG_WIDTH,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, USER_DIALOG_Y, USER_DIALOG_X + 18, " Users and Group Management ");
    draw_box(ds_win, USER_DIALOG_Y + 2, USER_DIALOG_X + 8, USER_DIALOG_HEIGHT - 6,
	     USER_DIALOG_WIDTH - 17, dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, USER_DIALOG_Y + 2, USER_DIALOG_X + 22, " Add a new group ");

    CLEAR(gname);
    CLEAR(gid);
    CLEAR(gmemb);

    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    n = 0;
#define lt groupLayout[n]
    while (lt.help != NULL) {
	switch (lt.type) {
	case STRINGOBJ:
	    lt.obj = NewStringObj(ds_win, lt.prompt, lt.var,
				  lt.y + USER_DIALOG_Y, lt.x + USER_DIALOG_X,
				  lt.len, lt.maxlen);
	    break;

	case BUTTONOBJ:
	    lt.obj = NewButtonObj(ds_win, lt.prompt, lt.var,
				  lt.y + USER_DIALOG_Y, lt.x + USER_DIALOG_X);
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
    for (first = obj; first->prev; first = first->prev);

    /* Some more initialisation before we go into the main input loop */
    n = 0;
    cancelbutton = okbutton = 0;

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

	case KEY_DOWN:
	case SEL_TAB:
	case SEL_CR:
	    if (firsttime && n == LAYOUT_GNAME)
	    {
		/* fill in the blanks, well, just the GID */
		completeGroup();
		RefreshStringObj(groupLayout[LAYOUT_GID].obj);
		firsttime = FALSE;
	    }
	    if (n < max)
		++n;
	    else
		n = 0;
	    break;

	    /* The user has pressed enter over a button object */
	case SEL_BUTTON:
 	    if (cancelbutton)
		cancel = TRUE, quit = TRUE;
	    else {
		if (verifyGroupSettings())
		    quit = TRUE;
	    }
	    break;

	case KEY_UP:
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
#undef lt

    /* Clear this crap off the screen */
    dialog_clear_norefresh();
    use_helpfile(NULL);

    if (!cancel) {
	addGroup(ds_win);
	restorescr(save);
	return DITEM_SUCCESS;
    }
    restorescr(save);
    return DITEM_FAILURE;
}

/* Check for the settings on the screen. */

static int
verifyUserSettings(WINDOW *ds_win)
{
    char tmp[256], *cp;
    long luid;
    WINDOW *save;
    int rv;

    if (strlen(uname) == 0) {
	feepout("The user name field must not be empty!");
	return 0;
    }
    snprintf(tmp, 256, "pw user show -q -n %s > /dev/null", uname);
    if (vsystem(tmp) == 0) {
	feepout("This user name is already in use.");
	return 0;
    }
    if (strlen(uid) > 0) {
	luid = strtol(uid, &cp, 10);
	if (luid < 0 || luid >= 65536 || (*cp != '\0' && !isspace(*cp))) {
	    feepout("The UID must be a number between 1 and 65535.");
	    return 0;
	}
    }
    if (strlen(shell) > 0) {
	while((cp = getusershell()) != NULL)
	    if (strcmp(cp, shell) == 0)
		break;
	endusershell();
	if (cp == NULL) {
	    save = savescr();
	    rv = msgYesNo("Warning:\n\n"
			  "The requested shell \"%s\" is not\n"
			  "a valid user shell.\n\n"
			  "Use it anyway?\n", shell);
	    restorescr(save);
	    wrefresh(ds_win);
	    if (rv != DITEM_SUCCESS)
		return 0;
	}
	
    }

    if (strlen(umemb) > 0) {
	if (strpbrk(umemb, " \t") != NULL) {
	    feepout("The member groups list must not contain any whitespace;\n"
		    "use commas to separate the names.");
	    return 0;
	}
    }

    return 1;
}

/*
 * Ask pw(8) to fill in the blanks for us.
 * Works solely on the global variables.
 */

static void
completeUser(void)
{
    int pfd[2], i;
    char tmp[256], *cp, *cp2;
    ssize_t l;
    size_t amnt;
    pid_t pid;
    char *vec[7] =
    {
	"pw", "user", "add", "-N", "-n", 0, 0
    };
#define VEC_UNAME 5

    pipe (pfd);
    if ((pid = fork()) == 0)
    {
	/* The kiddy. */
	dup2(pfd[1], 1);
	dup2(pfd[1], 2);
	for (i = getdtablesize(); i > 2; i--)
	    close(i);

	vec[VEC_UNAME] = uname;

	chroot(variable_get(VAR_INSTALL_ROOT));
	execv("/usr/sbin/pw", vec);
	msgDebug("Cannot execv() /usr/sbin/pw.\n");
	_exit(99);
    }
    else
    {
	/* The oldie. */
	close(pfd[1]);
	amnt = sizeof tmp;
	i = 0;
	while((l = read(pfd[0], &tmp[i], amnt)) > 0)
	{
	    amnt -= l;
	    i += l;
	    if (amnt == 0)
	    {
		close(pfd[0]);
		break;
	    }
	}
	close(pfd[0]);
	tmp[i] = '\0';
	waitpid(pid, &i, 0);
	if (WIFSIGNALED(i) || WEXITSTATUS(i) != 0)
	    /* ignore by now */
	    return;
	if ((cp = strchr(tmp, '\n')) != NULL)
	    *cp = '\0';
	if ((cp = strchr(tmp, ':')) == NULL || (cp = strchr(++cp, ':')) == NULL)
	    return;
	cp++;
	if ((cp2 = strchr(cp, ':')) == NULL)
	    return;
	*cp2++ = '\0';
	strncpy(uid, cp, sizeof uid);
	cp = cp2;
	if ((cp2 = strchr(cp, ':')) == NULL)
	    return;
	*cp2++ = '\0';
#ifdef notyet /* XXX pw user add -g doesn't accept a numerical GID */
	strncpy(ugroup, cp, sizeof ugroup);
#endif
	cp = cp2;
	if ((cp2 = strchr(cp, ':')) == NULL || (cp2 = strchr(++cp2, ':')) == NULL ||
	    (cp = cp2 = strchr(++cp2, ':')) == NULL || (cp2 = strchr(++cp2, ':')) == NULL)
	    return;
	*cp2++ = '\0';
	cp++;
	strncpy(gecos, cp, sizeof gecos);
	cp = cp2;
	if ((cp2 = strchr(cp, ':')) == NULL)
	    return;
	*cp2++ = '\0';
	strncpy(homedir, cp, sizeof homedir);
	if (*cp2)
	    strncpy(shell, cp2, sizeof shell);
    }
#undef VEC_UNAME
}

static void
addUser(WINDOW *ds_win)
{
    char tmp[256], *msg;
    int pfd[2], ipfd[2], i, j;
    ssize_t l;
    size_t amnt;
    pid_t pid;
    /*
     * Maximal list:
     * pw user add -m -n uname -g grp -u uid -c comment -d homedir -s shell -G grplist -h 0
     */
    char *vec[21] =
    {
	"pw", "user", "add", "-m", "-n", /* ... */
    };
#define VEC_UNAME 5

    msgNotify("Adding user \"%s\"...", uname);

    pipe (pfd);
    pipe (ipfd);
    if ((pid = fork()) == 0)
    {
	/* The kiddy. */
	dup2(ipfd[0], 0);
	dup2(pfd[1], 1);
	dup2(pfd[1], 2);
	for (i = getdtablesize(); i > 2; i--)
	    close(i);

	vec[i = VEC_UNAME] = uname;
	i++;
#define ADDVEC(var, option) do { if (strlen(var) > 0) { vec[i++] = option; vec[i++] = var; } } while (0)
	ADDVEC(ugroup, "-g");
	ADDVEC(uid, "-u");
	ADDVEC(gecos, "-c");
	ADDVEC(homedir, "-d");
	ADDVEC(shell, "-s");
	ADDVEC(umemb, "-G");
	if (passwd[0]) {
	    vec[i++] = "-h";
	    vec[i++] = "0";
	}
	vec[i] = 0;

	chroot(variable_get(VAR_INSTALL_ROOT));
	execv("/usr/sbin/pw", vec);
	msgDebug("Cannot execv() /usr/sbin/pw.\n");
	_exit(99);
    }
    else
    {
	/* The oldie. */
	close(pfd[1]);
	close(ipfd[0]);

	if (passwd[0])
	    write(ipfd[1], passwd, strlen(passwd));
	close(ipfd[1]);
	amnt = sizeof tmp;
	i = 0;
	while((l = read(pfd[0], &tmp[i], amnt)) > 0)
	{
	    amnt -= l;
	    i += l;
	    if (amnt == 0)
	    {
		close(pfd[0]);
		break;
	    }
	}
	close(pfd[0]);
	tmp[i] = '\0';
	waitpid(pid, &i, 0);
	if (WIFSIGNALED(i))
	{
	    j = WTERMSIG(i);
	    msg = "The `pw' command exited with signal %d.\n";
	    goto sysfail;
	}
	else if((j = WEXITSTATUS(i)))
	{
	    i = 0;
	    if(strncmp(tmp, "pw: ", 4) == 0)
		i = 4;
	    tmp[sizeof tmp - 1] = '\0';	/* sanity */
	    if (j == EX_DATAERR || j == EX_NOUSER || j == EX_SOFTWARE)
		msgConfirm("The `pw' command exited with an error status.\n"
			   "Its error message was:\n\n%s",
			   &tmp[i]);
	    else
	    {
		msg = "The `pw' command exited with unexpected status %d.\n";
	sysfail:
		msgDebug(msg, j);
		msgDebug("Command stdout and stderr was:\n\n%s", tmp);
		msgConfirm(msg, j);
	    }
	}
	else if (!passwd[0])
	    msgConfirm("You will need to enter a password for this user\n"
		       "later, using the passwd(1) command from the shell.\n\n"
		       "The account for `%s' is currently still disabled.",
		       uname);
    }
#undef VEC_UNAME
#undef ADDVEC
}

int
userAddUser(dialogMenuItem *self)
{
    WINDOW              *ds_win, *save;
    ComposeObj          *obj = NULL;
    ComposeObj		*first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret;
    int			max, firsttime=TRUE;
    char		help[FILENAME_MAX];

    if (RunningAsInit && !strstr(variable_get(SYSTEM_STATE), "install")) {
        msgConfirm("This option may only be used after the system is installed, sorry!");
        return DITEM_FAILURE;
    }

    save = savescr();
    dialog_clear_norefresh();
    /* We need a curses window */
    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0)
	msgFatal("Cannot open AddUser dialog window!!");

    /* Say where our help comes from */
    use_helpfile(systemHelpFile(USER_HELPFILE, help));

    /* Setup a nice screen for us to splat stuff onto */
    draw_box(ds_win, USER_DIALOG_Y, USER_DIALOG_X, USER_DIALOG_HEIGHT, USER_DIALOG_WIDTH,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, USER_DIALOG_Y, USER_DIALOG_X + 18, " Users and Group Management ");
    draw_box(ds_win, USER_DIALOG_Y + 1, USER_DIALOG_X + 3, USER_DIALOG_HEIGHT - 5,
	     USER_DIALOG_WIDTH - 6, dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, USER_DIALOG_Y + 1, USER_DIALOG_X + 22, " Add a new user ");

    CLEAR(uname);
    CLEAR(uid);
    CLEAR(ugroup);
    CLEAR(gecos);
    CLEAR(passwd);
    CLEAR(umemb);
    CLEAR(homedir);
    CLEAR(shell);
    
    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    n = 0;
#define lt userLayout[n]
    while (lt.help != NULL) {
	if (n == LAYOUT_PASSWD)
	    DialogInputAttrs = DITEM_NO_ECHO;	/* This will affect the new string object if set */
	switch (lt.type) {
	case STRINGOBJ:
	    lt.obj = NewStringObj(ds_win, lt.prompt, lt.var,
				  lt.y + USER_DIALOG_Y, lt.x + USER_DIALOG_X,
				  lt.len, lt.maxlen);
	    break;

	case BUTTONOBJ:
	    lt.obj = NewButtonObj(ds_win, lt.prompt, lt.var,
				  lt.y + USER_DIALOG_Y, lt.x + USER_DIALOG_X);
	    break;

	default:
	    msgFatal("Don't support this object yet!");
	}
	AddObj(&obj, lt.type, (void *) lt.obj);
	DialogInputAttrs = 0;
	n++;
    }
    max = n - 1;

    /* Find the last object we can traverse to */
    last = obj;
    while (last->next)
	last = last->next;

    /* Find the first object in the list */
    first = obj;
    for (first = obj; first->prev; first = first->prev);

    /* Some more initialisation before we go into the main input loop */
    n = 0;
    cancelbutton = okbutton = 0;

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

	case KEY_DOWN:
	case SEL_TAB:
	case SEL_CR:
	    if (firsttime && n == LAYOUT_UNAME)
	    {
		/* fill in the blanks, well, just the GID */
		completeUser();
		RefreshStringObj(userLayout[LAYOUT_UID].obj);
		RefreshStringObj(userLayout[LAYOUT_UGROUP].obj);
		RefreshStringObj(userLayout[LAYOUT_GECOS].obj);
		RefreshStringObj(userLayout[LAYOUT_UMEMB].obj);
		RefreshStringObj(userLayout[LAYOUT_HOMEDIR].obj);
		RefreshStringObj(userLayout[LAYOUT_SHELL].obj);
		firsttime = FALSE;
	    }
	    if (n < max)
		++n;
	    else
		n = 0;
	    break;

	    /* The user has pressed enter over a button object */
	case SEL_BUTTON:
 	    if (cancelbutton)
		cancel = TRUE, quit = TRUE;
	    else {
		if (verifyUserSettings(ds_win))
		    quit = TRUE;
	    }
	    break;

	case KEY_UP:
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
#undef lt

    /* Clear this crap off the screen */
    dialog_clear_norefresh();
    use_helpfile(NULL);

    if (!cancel) {
	addUser(ds_win);
	restorescr(save);
	return DITEM_SUCCESS;
    }
    restorescr(save);
    return DITEM_FAILURE;
}

