/*
 * $Id$
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

static char		hostname[256], domainname[256],
			ipaddr[32], netmask[32], gateway[32],
			dns[32], extras[256];
static int		okbutton, cancelbutton;

#define TCP_DIALOG_Y		0
#define TCP_DIALOG_X		8
#define TCP_DIALOG_WIDTH	COLS - 16
#define TCP_DIALOG_HEIGHT	LINES - 2

/* The names of the available interfaces, for the list */
char *iface_names[MAX_INTERFACE];

typedef struct _interface {
    char *name;
    Device *dev;
} Interface;

typedef struct _layout {
    int     y;
    int     x;
    int     len;
    int     maxlen;
    char    *prompt;
    char    *help;
    void    *var;
    int type;
    void *obj;
} Layout;

static Layout layout[] = {
{ 2, 2, 25, 255,
      "Host name:", "The name of your machine on a network, e.g. foo.bar.com",
      hostname, STRINGOBJ, NULL },
{ 2, 35, 20, 255,
      "Domain name:",
      "The name of the domain that your machine is in, e.g. bar.com",
      domainname, STRINGOBJ, NULL },
{ 5, 2, 18, 15,
      "Gateway:",
      "IP address of host forwarding packets to non-local hosts or nets",
      gateway, STRINGOBJ, NULL },
{ 5, 35, 18, 15,
      "Name server:", "IP address of your local DNS server",
      dns, STRINGOBJ, NULL },
{ 8, 2, 10, 6,
      "Interface:", "One of potentially several network interfaces",
      ifaces, LISTOBJ, NULL },
{ 14, 2, 18, 15,
      "IP Address:",
      "The IP address to be used for your host - use 127.0.0.1 for loopback",
      ipaddr, STRINGOBJ, NULL },
{ 14, 35, 18, 15,
      "Netmask:",
      "The netmask for your network, e.g. 0xffffff00 for a class C network",
      netmask, STRINGOBJ, NULL },
{ 16, 2, 50, 255,
      "Extra options:",
      "Any options to ifconfig you'd like to specify manually",
      extras, STRINGOBJ, NULL },
{ 18, 2, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
{ 18, 15, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
{ NULL },
};

#define _validByte(b) ((b) > 0 && (b) < 255)

static void
feepout(char *msg)
{
    beep();
    msgConfirm(msg);
    dialog_clear();
    refresh();
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
    else if (dns[0] && !verifyIP(dns))
	feepout("Invalid name server IP address specified");
    else if (netmask[0] < '0' || netmask[0] > '9')
	feepout("Invalid or missing netmask");
    else
	return 1;
    return 0;
}

/* Call this to initialize the TCP dialog */
void
/* This is it - how to get TCP setup values */
void
tcpOpenDialog(void)
{
    WINDOW              *ds_win;
    ComposeObj          *obj = NULL;
    ComposeObj		*first, *last;
    int                 n=0, quit=FALSE, cancel=FALSE, ret,
    max;
    char                *tmp;

    ds_win = newwin(LINES, COLS, 0, 0);
    if (ds_win == 0) {
	msgFatal("Cannot open TCP/IP dialog window!!");
	exit(1);
    }
    draw_box(ds_win, TCP_DIALOG_Y, TCP_DIALOG_X,
	     TCP_DIALOG_HEIGHT, TCP_DIALOG_WIDTH,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, TCP_DIALOG_Y, TCP_DIALOG_X + 20,
	      " Network Configuration ");

    bzero(ipaddr, sizeof(ipaddr));
    bzero(netmask, sizeof(netmask));
    bzero(extras, sizeof(extras));

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
	strcpy(dns, tmp);
    else
	bzero(dns, sizeof(dns));

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
	    lt.obj = NewListObj(ds_win, lt.prompt, lt.var, "lo0",
				lt.y + TCP_DIALOG_Y, lt.x + TCP_DIALOG_X,
				4, 12, 1);
	default:
	    printf("Don't support this object yet!\n");
	    end_dialog();
	    exit(1);
	}
	AddObj(&obj, lt.type, (void *) lt.obj);
	n++;
    }
    max = n-1;
    
    last = obj;
    while (last->next)
	last = last->next;
    
    /* find the first object in the list */
    first = obj;
    while (first->prev)
	first = first->prev;
    
    n = 0;
    while (quit != TRUE) {
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
	    quit=TRUE;
	    cancel=TRUE;
	    break;

	case KEY_UP:
	    if (obj->prev !=NULL ) {
		obj = obj->prev;
		--n;
	    }
	    else {
		obj = last;
		n = max;
	    }
	    break;

	case KEY_DOWN:
	    if (obj->next != NULL) {
		obj = obj->next;
		++n;
	    }
	    else {
		obj = first;
		n=0;
	    }
	    break;

	case SEL_BUTTON:
	    if (verifySettings())
		quit=TRUE;
	    break;

	case SEL_CR:
	case SEL_TAB:
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

	default:
	    beep();
	}

	if (n == 1) {
	    if ((tmp = index(hostname, '.')) != NULL) {
		strncpy(domainname, tmp + 1, strlen(tmp + 1));
		RefreshStringObj(layout[1].obj);
	    }
	}
    }
    if (!cancel) {
	variable_set2("hostname", hostname);
	variable_set2("domainname", domainname);
	variable_set2("ip_addr", ipaddr);
	variable_set2("ip_gateway", gateway);
}
