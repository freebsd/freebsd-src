/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media_strategy.c,v 1.28 1995/05/26 20:30:59 jkh Exp $
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

/*
 * These routines deal with getting things off of floppy media, though
 * with one exception:  genericGetDist() is actually used from lots of places
 * since it can think of the world as just "one big floppy" too if that's
 * appropriate.
 */

#include "sysinstall.h"

static Boolean networkInitialized;

Boolean
mediaInitNetwork(Device *dev)
{
    int i;
    char *rp;

    if (networkInitialized)
	return TRUE;

    configResolv();
    if (!strncmp("cuaa", dev->name, 4)) {
	if (!tcpStartPPP(dev)) {
	    msgConfirm("Unable to start PPP!  This installation method\ncannot be used.");
	    return FALSE;
	}
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

    if (!strncmp("cuaa", dev->name, 4)) {
	msgConfirm("You may now go to the 3rd screen (ALT-F3) and shut down\nyour PPP connection.  It shouldn't be needed any longer\n(unless you wish to create a shell by typing ESC and\nexperiment with it further, in which case go right ahead!)");
	return;
    }
    else {
	int i;
	char ifconfig[64];

	snprintf(ifconfig, 64, "%s%s", VAR_IFCONFIG, dev->name);
	cp = getenv(ifconfig);
	if (!cp)
	    return;
	i = vsystem("ifconfig %s down", dev->name);
	if (i)
	    msgConfirm("Warning: Unable to down the %s interface properly", dev->name);
    }

    cp = getenv(VAR_GATEWAY);
    if (cp)
	vsystem("route delete default");
    networkInitialized = FALSE;
}
