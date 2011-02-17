/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $FreeBSD$
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
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/* These routines deal with getting things off of network media */

#include "sysinstall.h"
#include <signal.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

static Boolean	networkInitialized;

Boolean
mediaInitNetwork(Device *dev)
{
    int i;
    char *rp;
    char *cp, ifconfig[255];
    WINDOW *w;
    
    if (!RunningAsInit || networkInitialized)
	return TRUE;

    if (isDebug())
	msgDebug("Init routine called for network device %s.\n", dev->name);

    if (!file_readable("/etc/resolv.conf")) {
	if (DITEM_STATUS(configResolv(NULL)) == DITEM_FAILURE) {
	    msgConfirm("Can't seem to write out /etc/resolv.conf.  Net cannot be used.");
	    return FALSE;
	}
    }

    w = savescr();
    dialog_clear_norefresh();

    snprintf(ifconfig, 255, "%s%s", VAR_IFCONFIG, dev->name);
    cp = variable_get(ifconfig);
    if (cp) {
	/*
	 * If this interface isn't a DHCP one, bring it up.
	 * If it is, then it's already up.
	 */
	if (strstr(cp, "DHCP") == NULL) {
	    msgDebug("Not a DHCP interface.\n");
	    i = vsystem("ifconfig %s %s", dev->name, cp);
	    if (i) {
		msgConfirm("Unable to configure the %s interface!\n"
			   "This installation method cannot be used.",
			   dev->name);
		return FALSE;
	    }
	    rp = variable_get(VAR_GATEWAY);
	    if (!rp || *rp == '0') {
		msgConfirm("No gateway has been set. You will be unable to access hosts\n"
			   "not on your local network");
	    }
	    else {
		/* 
		 * Explicitly flush all routes to get back to a known sane
		 * state. We don't need to check this exit code because if
		 * anything fails it will show up in the route add below.
		 */
		system("route -n flush");
		msgDebug("Adding default route to %s.\n", rp);
		if (vsystem("route -n add default %s", rp) != 0) {
		    msgConfirm("Failed to add a default route; please check "
			       "your network configuration");
		    return FALSE;
		}
	    }
	} else {
	    msgDebug("A DHCP interface.  Should already be up.\n");
	}
    } else if ((cp = variable_get(VAR_IPV6ADDR)) == NULL || *cp == '\0') {
	msgConfirm("The %s device is not configured.  You will need to do so\n"
		   "in the Networking configuration menu before proceeding.", dev->name);
	return FALSE;
    }

    if (isDebug())
	msgDebug("Network initialized successfully.\n");
    networkInitialized = TRUE;
    return TRUE;
}

void
mediaShutdownNetwork(Device *dev)
{
    char *cp;

    if (!RunningAsInit || !networkInitialized)
	return;

	msgDebug("Shutdown called for network device %s\n", dev->name);
	int i;
	char ifconfig[255];

	snprintf(ifconfig, 255, "%s%s", VAR_IFCONFIG, dev->name);
	cp = variable_get(ifconfig);
	if (!cp)
	    return;
	msgDebug("ifconfig %s down\n", dev->name);
	i = vsystem("ifconfig %s down", dev->name);
	if (i)
	    msgConfirm("Warning: Unable to down the %s interface properly", dev->name);
	cp = variable_get(VAR_GATEWAY);
	if (cp) {
	    msgDebug("Deleting default route.\n");
	    vsystem("route -n delete default");
	}
}

