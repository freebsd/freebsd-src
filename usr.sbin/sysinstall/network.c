/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: network.c,v 1.2 1995/05/28 03:05:00 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 * Copyright (c) 1995
 * 	Gary J Palmer. All rights reserved.
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

/* These routines deal with getting things off of network media */

#include "sysinstall.h"
#include <sys/fcntl.h>

static Boolean networkInitialized;
static Boolean startPPP(Device *devp);

Boolean
mediaInitNetwork(Device *dev)
{
    int i;
    char *rp;

    if (networkInitialized)
	return TRUE;

    configResolv();
    if (!strncmp("cuaa", dev->name, 4)) {
	if (!msgYesNo("You have selected a serial-line network interface.\nDo you want to use PPP with it?")) {
	    if (!startPPP(dev)) {
		msgConfirm("Unable to start PPP!  This installation method\ncannot be used.");
		return FALSE;
	    }
	}
	else
	    msgConfirm("Warning:  SLIP is rather poorly supported in this revision\nof the installation due to the lack of a dialing utility.\nIf you can use PPP for this instead then you're much better\noff doing so, otherwise SLIP works fairly well for *hardwired*\nlinks.  Use the shell on the 4TH screen (ALT-F4) to run slattach\nand otherwise set the link up, then hit return here to continue.");
    }
    else {
	char *cp, ifconfig[64];

	snprintf(ifconfig, 64, "%s%s", VAR_IFCONFIG, dev->name);
	cp = getenv(ifconfig);
	if (!cp) {
	    msgConfirm("The %s device is not configured.  You will need to do so\nin the Networking configuration menu before proceeding.");
	    return FALSE;
	}
	i = vsystem("ifconfig %s %s", dev->name, cp);
	if (i) {
	    msgConfirm("Unable to configure the %s interface!\nThis installation method cannot be used.", dev->name);
	    return FALSE;
	}
    }

    rp = getenv(VAR_GATEWAY);
    if (!rp)
	msgConfirm("No gateway has been set. You will not be able to access hosts\n
not on the local network\n");
    else
	vsystem("route add default %s", rp);
    networkInitialized = TRUE;
    return TRUE;
}

void
mediaShutdownNetwork(Device *dev)
{
    char *cp;

    if (!networkInitialized)
	return;

    /* If we're running PPP or SLIP, it's too much trouble to shut down so forget it */
    if (strncmp("cuaa", dev->name, 4)) {
	int i;
	char ifconfig[64];

	snprintf(ifconfig, 64, "%s%s", VAR_IFCONFIG, dev->name);
	cp = getenv(ifconfig);
	if (!cp)
	    return;
	i = vsystem("ifconfig %s down", dev->name);
	if (i)
	    msgConfirm("Warning: Unable to down the %s interface properly", dev->name);
	cp = getenv(VAR_GATEWAY);
	if (cp)
	    vsystem("route delete default");
	networkInitialized = FALSE;
    }
}

int
configRoutedFlags(char *str)
{
    char *val;

    val = msgGetInput("-q", "Specify the flags for routed; -q is the default, -s is\na good choice for gateway machines.");
    if (val)
	variable_set2("routedflags", val);
    return 0;
}

/* Start PPP on the 3rd screen */
static Boolean
startPPP(Device *devp)
{
    int fd;
    FILE *fp;
    char *val;
    char myaddr[16], provider[16];

    fd = open("/dev/ttyv2", O_RDWR);
    if (fd == -1)
	return FALSE;
    Mkdir("/var/log", NULL);
    Mkdir("/var/spool/lock", NULL);
    Mkdir("/etc/ppp", NULL);
    vsystem("touch /etc/ppp/ppp.linkup; chmod +x /etc/ppp/ppp.linkup");
    vsystem("touch /etc/ppp/ppp.secret; chmod +x /etc/ppp/ppp.secret");
    fp = fopen("/etc/ppp/ppp.conf", "w");
    if (!fp) {
	msgConfirm("Couldn't open /etc/ppp/ppp.conf file!  This isn't going to work");
	return FALSE;
    }
    fprintf(fp, "default:\n");
    fprintf(fp, " set device %s\n", devp->devname);
    val = msgGetInput("115200",
"Enter the baud rate for your modem - this can be higher than the actual\nmaximum data rate since most modems can talk at one speed to the\ncomputer and at another speed to the remote end.\n\nIf you're not sure what to put here, just select the default.");
    if (!val)
	val = "115200";
    fprintf(fp, " set speed %s\n", val);
    if (getenv(VAR_GATEWAY))
	strcpy(provider, getenv(VAR_GATEWAY));
    else
	strcpy(provider, "0");
    val = msgGetInput(provider, "Enter the IP address of your service provider or 0 if you\ndon't know it and would prefer to negotiate it dynamically.");
    if (!val)
	val = "0";
    if (devp->private && ((DevInfo *)devp->private)->ipaddr[0])
	strcpy(myaddr, ((DevInfo *)devp->private)->ipaddr);
    else
	strcpy(myaddr, "0");
    fprintf(fp, " set ifaddr %s %s\n", myaddr, val);
    fclose(fp);
    if (!fork()) {
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	execl("/stand/ppp", "/stand/ppp", (char *)NULL);
	exit(1);
    }
    msgConfirm("The PPP command is now started on screen 3 (type ALT-F3 to\ninteract with it, ALT-F1 to switch back here). The only command\nyou'll probably want or need to use is the \"term\" command\nwhich starts a terminal emulator you can use to talk to your\nmodem and dial the service provider.  Once you're connected,\ncome back to this screen and press return.  DO NOT PRESS RETURN\nHERE UNTIL THE CONNECTION IS FULLY ESTABLISHED!");
    return TRUE;
}
