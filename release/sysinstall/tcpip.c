/*
 * $Id: tcpip.c,v 1.1 1995/05/16 02:53:28 jkh Exp $
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

static char		hostname[256], domainname[256],
			gateway[32], nameserver[32], iface[8];
static int		okbutton, cancelbutton;
static char		ipaddr[32], netmask[32], extras[256];

#define TCP_DIALOG_Y		0
#define TCP_DIALOG_X		8
#define TCP_DIALOG_WIDTH	COLS - 16
#define TCP_DIALOG_HEIGHT	LINES - 2

typedef struct _interface {
    char ipaddr[32];
    char netmask[32];
    char extras[256];
    Device *dev;
} Interface;

/* The names and details of the available interfaces, for the list */
Interface 	if_list[INTERFACE_MAX];
char		*iface_names[INTERFACE_MAX];

typedef struct _layout {
    int		y;
    int		x;
    int		len;
    int		maxlen;
    char	*prompt;
    char	*help;
    void	*var;
    int		type;
    void	*obj;
} Layout;

static Layout layout[] = {
{ 1, 2, 25, 255,
      "Host name:", "The name of your machine on a network, e.g. foo.bar.com",
      hostname, STRINGOBJ, NULL },
{ 1, 35, 20, 255,
      "Domain name:",
      "The name of the domain that your machine is in, e.g. bar.com",
      domainname, STRINGOBJ, NULL },
{ 5, 2, 18, 15,
      "Gateway:",
      "IP address of host forwarding packets to non-local hosts or nets",
      gateway, STRINGOBJ, NULL },
{ 5, 35, 18, 15,
      "Name server:", "IP address of your local DNS server",
      nameserver, STRINGOBJ, NULL },
{ 9, 2, 9, 6,
      "Interface:", "One of potentially several network interfaces",
      iface, LISTOBJ, NULL },
{ 10, 18, 18, 15,
      "IP Address:",
      "The IP address to be used for your host - use 127.0.0.1 for loopback",
      ipaddr, STRINGOBJ, NULL },
{ 10, 37, 18, 15,
      "Netmask:",
      "The netmask for your network, e.g. 0xffffff00 for a class C network",
      netmask, STRINGOBJ, NULL },
{ 14, 18, 37, 255,
      "Extra options to ifconfig:",
      "Any options to ifconfig you'd like to specify manually",
      extras, STRINGOBJ, NULL },
{ 19, 10, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
{ 19, 30, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
{ NULL },
};

#define _validByte(b) ((b) > 0 && (b) < 255)

static void
feepout(char *msg)
{
    beep();
    dialog_notify(msg);
}

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

static int
verifySettings(void)
{
    if (!hostname[0])
	feepout("Must specify a host name of some sort!");
    else if (!verifyIP(ipaddr))
	feepout("Invalid or missing value for IP address");
    else if (gateway[0] && !verifyIP(gateway))
	feepout("Invalid gateway IP address specified");
    else if (nameserver[0] && !verifyIP(nameserver))
	feepout("Invalid name server IP address specified");
    else if (netmask[0] < '0' || netmask[0] > '9')
	feepout("Invalid or missing netmask");
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

    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0)
	msgFatal("Cannot open TCP/IP dialog window!!");

    devs = deviceFind(NULL, DEVICE_TYPE_NETWORK);
    if (!devs) {
	msgConfirm("Couldn't find any potential network devices!");
	return 0;
    }

    while (devs[n] != NULL) {
	iface_names[n] = (devs[n])->name;
	++n;
    }
    n_iface = --n;

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


    strcpy(ipaddr, if_list[0].ipaddr);
    strcpy(netmask, if_list[0].netmask);
    strcpy(extras, if_list[0].extras);

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
    
    last = obj;
    while (last->next)
	last = last->next;
    
    /* find the first object in the list */
    first = obj;
    while (first->prev)
	first = first->prev;

    n = 0;
    cancelbutton = okbutton = 0;
    strcpy(iface, iface_names[0]);
    strcpy(old_iface, iface);

    while (!quit) {
	char help_line[80];
	int i, len = strlen(lt.help);

	for (i = 0; i < 79; i++)
	    help_line[i] = (i < len) ? lt.help[i] : ' ';
	help_line[i] = '\0';
	use_helpline(help_line);
	display_helpline(ds_win, LINES - 1, COLS - 1);

	ret = PollObj(&obj);

	switch (ret) {
	case SEL_ESC:
	    quit = TRUE, cancel=TRUE;
	    break;

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
	    if (n == 7) {
		n = 4;
		obj = (((first->next)->next)->next)->next;
	    } else if (obj->next != NULL) {
		obj = obj->next;
		++n;
	    } else {
		obj = first;
		n = 0;
	    }
	    break;

	case SEL_TAB:
	    if (n == 7) {
		n = 4;
		obj = (((first->next)->next)->next)->next;
	    } else if (obj->next != NULL) {
		++n;
	    } else {
		n = 0;
	    }
	    if (n == 5) {
		n = 8;
		obj = ((obj->next)->next)->next;
	    }
	    break;

	case SEL_BUTTON:
 	    if (cancelbutton) {
		cancel = TRUE, quit = TRUE;
	    } else {
		if (verifySettings())
		    quit = TRUE;
	    }
	    break;

	case SEL_CR:
	    if (strcmp(old_iface, iface)) {
		/* First, find the old value */
		n_iface = 0;
		while (strcmp(old_iface, iface_names[n_iface]) &&
		       (iface_names[n_iface] != NULL))
		    ++n_iface;
		
		if (iface_names[n_iface] == NULL)
		    msgFatal("Erk - run off the end of the list of interfaces!");
		strcpy(if_list[n_iface].ipaddr, ipaddr);
		strcpy(if_list[n_iface].netmask, netmask);
		strcpy(if_list[n_iface].extras, extras);
		
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
		
		RefreshStringObj(layout[5].obj);
		RefreshStringObj(layout[6].obj);
		RefreshStringObj(layout[7].obj);
		
		strcpy(old_iface, iface);
	    }

	    if (n == 7) {
		n = 4;
		obj = (((first->next)->next)->next)->next;
	    } else if (n < max)
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

	default:
	    beep();
	}

	if (n == 1) {
	    if ((tmp = index(hostname, '.')) != NULL) {
		strncpy(domainname, tmp + 1, strlen(tmp + 1));
		domainname[strlen(tmp+1)] = '\0';
		RefreshStringObj(layout[1].obj);
	    }
	}
    }

    dialog_clear();
    refresh();

    if (!cancel) {
	variable_set2(VAR_HOSTNAME, hostname);
	variable_set2(VAR_DOMAINNAME, domainname);
	if (gateway[0])
	    variable_set2(VAR_GATEWAY, gateway);
	if (nameserver[0])
	    variable_set2(VAR_NAMESERVER, nameserver);
	return 1;
    }

    return 0;
}
