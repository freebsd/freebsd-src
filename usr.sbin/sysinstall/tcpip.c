/*
 * $Id: tcpip.c,v 1.7 1995/05/18 13:18:35 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gary J Palmer
 *      for the FreeBSD Project.
 * 4. The name of Gary J Palmer or the FreeBSD Project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GARY J PALMER ``AS IS'' AND ANY EXPRESS 
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL GARY J PALMER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include "rc.h"
#include "sysinstall.h"

/* These are nasty, but they make the layout structure a lot easier ... */

static char		hostname[256], domainname[256],
			gateway[32], nameserver[32], iface[8];
static int		okbutton, cancelbutton;
static char		ipaddr[32], netmask[32], extras[256];

/* What the screen size is meant to be */
#define TCP_DIALOG_Y		0
#define TCP_DIALOG_X		8
#define TCP_DIALOG_WIDTH	COLS - 16
#define TCP_DIALOG_HEIGHT	LINES - 2

/* The per interface set of records */
typedef struct _interface {
    char	ipaddr[32];
    char	netmask[32];
    char	extras[256]; /* Extra stuff for ifconfig (link0, etc) */
    int		valid;
    Device	*dptr;
} Interface;

/* The names and details of the available interfaces, for the list */
Interface 	if_list[INTERFACE_MAX];
char		*iface_names[INTERFACE_MAX];

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

static Layout layout[] = {
{ 1, 2, 25, 255,
      "Host name:", "The name of your machine on a network, e.g. foo.bar.com",
      hostname, STRINGOBJ, NULL },
#define LAYOUT_HOSTNAME		0
{ 1, 35, 20, 255,
      "Domain name:",
      "The name of the domain that your machine is in, e.g. bar.com",
      domainname, STRINGOBJ, NULL },
#define LAYOUT_DOMAINNAME	1
{ 5, 2, 18, 15,
      "Gateway:",
      "IP address of host forwarding packets to non-local hosts or nets",
      gateway, STRINGOBJ, NULL },
#define LAYOUT_GATEWAY		2
{ 5, 35, 18, 15,
      "Name server:", "IP address of your local DNS server",
      nameserver, STRINGOBJ, NULL },
#define LAYOUT_NAMESERVER	3
{ 9, 2, 9, 6,
      "Interface:",
      "Network devices found on boot (use <TAB> to exit from here)",
      iface, LISTOBJ, NULL },
#define LAYOUT_IFACE		4
{ 10, 18, 18, 15,
      "IP Address:",
      "The IP address to be used for this interface - use 127.0.0.1 for lo0",
      ipaddr, STRINGOBJ, NULL },
#define LAYOUT_IPADDR		5
{ 10, 37, 18, 15,
      "Netmask:",
      "The netmask for this interfaace, e.g. 0xffffff00 for a class C network",
      netmask, STRINGOBJ, NULL },
#define LAYOUT_NETMASK		6
{ 14, 18, 37, 255,
      "Extra options to ifconfig:",
      "Any interface-specific options to ifconfig you'd like to use",
      extras, STRINGOBJ, NULL },
#define LAYOUT_EXTRAS		7
{ 19, 10, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_OKBUTTON		8
{ 19, 30, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	9
{ NULL },
};

#define _validByte(b) ((b) > 0 && (b) < 255)

/* A JKH special - inform the user of an unusal error in a controlled
   fashion */
static void
feepout(char *msg)
{
    beep();
    dialog_notify(msg);
}

/* Very basic IP address integrity check - could be drastically improved */
static int
verifyIP(char *ip)
{
    int a, b, c, d;

    if (ip && sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
	_validByte(a) && _validByte(b) && _validByte(c) &&
	_validByte(d))
	return 1;
    else
	return 0;
}

/* Check for the settings on the screen - the per interface stuff is
   moved to the main handling code now to do it on the fly - sigh */

static int
verifySettings(void)
{
    if (!hostname[0])
	feepout("Must specify a host name of some sort!");
#if 0
    else if (!verifyIP(ipaddr))
	feepout("Invalid or missing value for IP address");
#endif
    else if (gateway[0] && !verifyIP(gateway))
	feepout("Invalid gateway IP address specified");
    else if (nameserver[0] && !verifyIP(nameserver))
	feepout("Invalid name server IP address specified");
#if 0
    else if (netmask[0] < '0' || netmask[0] > '9')
	feepout("Invalid or missing netmask");
#endif
    else
	return 1;
    return 0;
}

/* This is it - how to get TCP setup values */
int
tcpOpenDialog(char *str)
{
    WINDOW              *ds_win;
    ComposeObj          *obj = NULL;
    ComposeObj		*first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret,
    			max, n_iface;
    char                *tmp;
    Device		**devs;
    char		old_iface[8];
    char		help[FILENAME_MAX];

    /* We need a curses window */
    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0)
	msgFatal("Cannot open TCP/IP dialog window!!");

    /* Look for net.devices for us to configure */
    devs = deviceFind(NULL, DEVICE_TYPE_NETWORK);
    if (!devs) {
	msgConfirm("Couldn't find any potential network devices!");
	return 0;
    }

    while (devs[n] != NULL) {
	iface_names[n] = (devs[n])->name;
	if_list[n].dptr = devs[n];
	++n;
    }
    n_iface = n;

    /* Setup a nice screen for us to splat stuff onto */
    systemHelpFile(TCP_HELPFILE, help);
    use_helpfile(help);
    draw_box(ds_win, TCP_DIALOG_Y, TCP_DIALOG_X,
	     TCP_DIALOG_HEIGHT, TCP_DIALOG_WIDTH,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, TCP_DIALOG_Y, TCP_DIALOG_X + 20,
	      " Network Configuration ");
    draw_box(ds_win, TCP_DIALOG_Y + 9, TCP_DIALOG_X + 16,
	     TCP_DIALOG_HEIGHT - 13, TCP_DIALOG_WIDTH - 21,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, TCP_DIALOG_Y + 9, TCP_DIALOG_X + 24,
	      " Per Interface Configuration ");

    /* Initialise vars so that dialog has something to chew on */
    strcpy(ipaddr, if_list[0].ipaddr);
    strcpy(netmask, if_list[0].netmask);
    strcpy(extras, if_list[0].extras);

    /* Look up values already recorded with the system, or blank the
       string variables ready to accept some new data */
    tmp = getenv(VAR_HOSTNAME);
    if (tmp)
	strcpy(hostname, tmp);
    else
	bzero(hostname, sizeof(hostname));
    tmp = getenv(VAR_DOMAINNAME);
    if (tmp)
	strcpy(domainname, tmp);
    else
	bzero(domainname, sizeof(domainname));
    tmp = getenv(VAR_GATEWAY);
    if (tmp)
	strcpy(gateway, tmp);
    else
	bzero(gateway, sizeof(gateway));
    tmp = getenv(VAR_NAMESERVER);
    if (tmp)
	strcpy(nameserver, tmp);
    else
	bzero(nameserver, sizeof(nameserver));

    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    n = 0;
#define lt layout[n]
    while (lt.help != NULL) {
	switch (lt.type) {
	case STRINGOBJ:
	    lt.obj = NewStringObj(ds_win, lt.prompt, lt.var,
				  lt.y + TCP_DIALOG_Y, lt.x + TCP_DIALOG_X,
				  lt.len, lt.maxlen);
	    break;

	case BUTTONOBJ:
	    lt.obj = NewButtonObj(ds_win, lt.prompt, lt.var,
				  lt.y + TCP_DIALOG_Y, lt.x + TCP_DIALOG_X);
	    break;

	case LISTOBJ:
	    lt.obj = NewListObj(ds_win, lt.prompt, (char **) iface_names,
				lt.var, lt.y + TCP_DIALOG_Y,
				lt.x + TCP_DIALOG_X, lt.len, 12, n_iface);
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
    cancelbutton = okbutton = 0;
    strcpy(iface, iface_names[0]);
    if_list[0].valid = FALSE;
    strcpy(old_iface, iface);

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

	/* We are in the Hostname field - calculate the domainname */
	if (n == 0) {
	    if ((tmp = index(hostname, '.')) != NULL) {
		strncpy(domainname, tmp + 1, strlen(tmp + 1));
		domainname[strlen(tmp+1)] = '\0';
		RefreshStringObj(layout[1].obj);
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

	    /* More special case handling - if we are at the interface
               list, move to the OK button - the user hasn't selected
               one of the entries in the list by pressing CR, so (s)he
               must be wanting to skip to <OK> & <CANCEL> */
	case KEY_DOWN:
	    if (n == LAYOUT_EXTRAS) {
		n = LAYOUT_IFACE;
		obj = (((first->next)->next)->next)->next;
	    } else if (obj->next != NULL) {
		obj = obj->next;
		++n;
	    } else {
		obj = first;
		n = 0;
	    }
	    break;

	    /* Same as KEY_DOWN, but dialog has already move us to the
               next object on the list, which makes this slightly
               different. */
	case SEL_TAB:
	    if (n == LAYOUT_EXTRAS) {
		n = LAYOUT_IFACE;
		obj = (((first->next)->next)->next)->next;
            } else if (n < max) {
		++n;
	    } else {
		n = 0;
	    }

	    /* This looks double dutch, but we have already MOVED onto
               the next field, so we special case around getting to
               that field, rather than moving off the previous
               one. Hence we are really testing for
	       (n == LAYOUT_IFACE) */

	    if (n == LAYOUT_IPADDR) {
		n = LAYOUT_OKBUTTON;
		obj = ((obj->next)->next)->next;
	    }
	    break;

	    /* The user has pressed enter over a button object */
	case SEL_BUTTON:
 	    if (cancelbutton) {
		cancel = TRUE, quit = TRUE;
	    } else {
		if (verifySettings())
		    quit = TRUE;
	    }
	    break;

	    /* Generic CR handler */
	case SEL_CR:
	    /* Has the user selected a new interface? */
	    if (strcmp(old_iface, iface)) {
		/* Moved to a different condition for better handling */
#if 0
		/* First, find the old value */
		n_iface = 0;
		while (strcmp(old_iface, iface_names[n_iface]) &&
		       (iface_names[n_iface] != NULL))
		    ++n_iface;
		
		if (iface_names[n_iface] == NULL)
		    msgFatal("Erk - run off the end of the list of interfaces!");

		/* Sanity check what the user supplied - this could probably
		   be better :-( */
		
		if (!verifyIP(ipaddr)) {
		    feepout("Invalid or missing IP address!");
		    strcpy(iface, old_iface);
		    n = LAYOUT_IFACE;
		    obj = (((first->next)->next)->next)->next;
		    RefreshListObj(layout[LAYOUT_IFACE].obj);
		}

		if (netmask[0] < '0' || netmask[0] > '9') {
		    feepout("Invalid or missing netmask!");
		    strcpy(iface, old_iface);
		    n = LAYOUT_IFACE;
		    obj = (((first->next)->next)->next)->next;
		    RefreshListObj(layout[LAYOUT_IFACE].obj);
		}
		
		strcpy(if_list[n_iface].ipaddr, ipaddr);
		strcpy(if_list[n_iface].netmask, netmask);
		strcpy(if_list[n_iface].extras, extras);
		if_list[n_iface].valid = TRUE;
#endif /* if 0 */
		/* Now go find the new location */
		n_iface = 0;
		while (strcmp(iface, iface_names[n_iface]) &&
		       (iface_names[n_iface] != NULL))
		    ++n_iface;
		if (iface_names[n_iface] == NULL)
		    msgFatal("Erk - run off the end of the list of interfaces!");
		strcpy(ipaddr, if_list[n_iface].ipaddr);
		strcpy(netmask, if_list[n_iface].netmask);
		strcpy(extras, if_list[n_iface].extras);
		if_list[n_iface].valid = FALSE;
		
		RefreshStringObj(layout[LAYOUT_IPADDR].obj);
		RefreshStringObj(layout[LAYOUT_NETMASK].obj);
		RefreshStringObj(layout[LAYOUT_EXTRAS].obj);
		
		strcpy(old_iface, iface);
	    }

	    /* Loop back to the interface list from the extras box -
               now we handle the case of saving out the data the user
               typed in (and also do basic verification of it's
               sanity) */
	    if (n == LAYOUT_EXTRAS) {
		n = LAYOUT_IFACE;
		obj = (((first->next)->next)->next)->next;
		/* First, find the old value */
		n_iface = 0;
		while (strcmp(old_iface, iface_names[n_iface]) &&
		       (iface_names[n_iface] != NULL))
		    ++n_iface;
		
		if (iface_names[n_iface] == NULL)
		    msgFatal("Erk - run off the end of the list of interfaces!");

		/* Sanity check what the user supplied - this could probably
		   be better :-( */
		
		if (!verifyIP(ipaddr)) {
		    feepout("Invalid or missing IP address!");
		    strcpy(iface, old_iface);
		    n = LAYOUT_IFACE;
		    obj = (((first->next)->next)->next)->next;
		    RefreshListObj(layout[LAYOUT_IFACE].obj);
		}

		if (netmask[0] < '0' || netmask[0] > '9') {
		    feepout("Invalid or missing netmask!");
		    strcpy(iface, old_iface);
		    n = LAYOUT_IFACE;
		    obj = (((first->next)->next)->next)->next;
		    RefreshListObj(layout[LAYOUT_IFACE].obj);
		}
		
		strcpy(if_list[n_iface].ipaddr, ipaddr);
		strcpy(if_list[n_iface].netmask, netmask);
		strcpy(if_list[n_iface].extras, extras);
		if_list[n_iface].valid = TRUE;
		if_list[n_iface].dptr->enabled = TRUE;
	    } else if (n < max)
		++n;
	    else
		n = 0;
	    break;

	    /* This doesn't seem to work anymore - dunno why. Foo */

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
	
	/* BODGE ALERT! */
	if ((tmp = index(hostname, '.')) != NULL) {
	    strncpy(domainname, tmp + 1, strlen(tmp + 1));
	    domainname[strlen(tmp+1)] = '\0';
	    RefreshStringObj(layout[1].obj);
	}
    }

    /* Clear this crap off the screen */
    dialog_clear();
    refresh();
    use_helpfile(NULL);
    
    /* We actually need to inform the rest of sysinstall about this
       data now - if the user hasn't selected cancel, save the stuff
       out to the environment via the variable_set layers */

    if (!cancel) {
	int foo;
	variable_set2(VAR_HOSTNAME, hostname);
	variable_set2(VAR_DOMAINNAME, domainname);
	if (gateway[0])
	    variable_set2(VAR_GATEWAY, gateway);
	if (nameserver[0])
	    variable_set2(VAR_NAMESERVER, nameserver);

	/* Loop over the per-interface data saving data which has been
           validated ... */
	for (foo = 0 ; foo < INTERFACE_MAX ; foo++) {
	    if (if_list[foo].valid == TRUE) {
		char temp[512], ifn[64];
		sprintf(temp, "inet %s %s netmask %s",
			if_list[foo].ipaddr, if_list[foo].extras,
			if_list[foo].netmask);
		sprintf(ifn, "%s%s", VAR_IFCONFIG, iface_names[foo]);
		variable_set2(ifn, temp);
	    }
	}
    }
    return 0;
}
