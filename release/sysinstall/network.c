/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: network.c,v 1.20 1996/12/09 06:45:03 jkh Exp $
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
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

static Boolean	networkInitialized;
static pid_t	startPPP(Device *devp);

static pid_t	pppPID;

Boolean
mediaInitNetwork(Device *dev)
{
    int i;
    char *rp;
    char *cp, ifconfig[255];

    if (!RunningAsInit || networkInitialized)
	return TRUE;

    msgDebug("Init routine called for network device %s.\n", dev->name);
    if (!file_readable("/etc/resolv.conf"))
	configResolv();

    /* Old PPP process lying around? */
    if (pppPID) {
	kill(pppPID, SIGTERM);
	pppPID = 0;
    }
    if (!strncmp("ppp", dev->name, 3)) {	/* PPP? */
	if (!(pppPID = startPPP(dev))) {
	    msgConfirm("Unable to start PPP!  This installation method cannot be used.");
	    return FALSE;
	}
	networkInitialized = TRUE;
	return TRUE;
    }
    else if (!strncmp("sl", dev->name, 2)) {	/* SLIP? */
	char *val;
	char attach[256];

	dialog_clear_norefresh();
	/* Cheesy slip attach */
	snprintf(attach, 256, "slattach -a -h -l -s 9600 %s", dev->devname);
	val = msgGetInput(attach,
			  "Warning:  SLIP is rather poorly supported in this revision\n"
			  "of the installation due to the lack of a dialing utility.\n"
			  "If you can use PPP for this instead then you're much better\n"
			  "off doing so, otherwise SLIP works fairly well for *hardwired*\n"
			  "links.  Please edit the following slattach command for\n"
			  "correctness (default here is: VJ compression, Hardware flow-\n"
			  "control, ignore carrier and 9600 baud data rate).  When you're\n"
			  "ready, press [ENTER] to execute it.");
	if (!val)
	    return FALSE;
	else
	    SAFE_STRCPY(attach, val);
	/*
	 * Doing this with vsystem() is actually bogus since we should be storing the pid of slattach
	 * for later killing.  It's just too convenient to call vsystem(), however, rather than
	 * constructing a proper argument for exec() so we punt on doing slip right for now.
	 */
	if (vsystem(attach)) {
	    msgConfirm("slattach returned a bad status!  Please verify that\n"
		       "the command is correct and try again.");
	    return FALSE;
	}
    }

    snprintf(ifconfig, 255, "%s%s", VAR_IFCONFIG, dev->name);
    cp = variable_get(ifconfig);
    if (!cp) {
	msgConfirm("The %s device is not configured.  You will need to do so\n"
		   "in the Networking configuration menu before proceeding.", dev->name);
	return FALSE;
    }
    msgNotify("ifconfig %s %s", dev->name, cp);
    i = vsystem("ifconfig %s %s", dev->name, cp);
    if (i) {
	msgConfirm("Unable to configure the %s interface!\n"
		   "This installation method cannot be used.", dev->name);
	return FALSE;
    }

    rp = variable_get(VAR_GATEWAY);
    if (!rp || *rp == '0') {
	msgConfirm("No gateway has been set. You may be unable to access hosts\n"
		   "not on your local network");
    }
    else {
	msgNotify("Adding default route to %s.", rp);
	vsystem("route add default %s", rp);
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

    if (isDebug())
	msgDebug("Shutdown called for network device %s\n", dev->name);
    /* Not a serial device? */
    if (strncmp("sl", dev->name, 2) && strncmp("ppp", dev->name, 3)) {
	int i;
	char ifconfig[255];

	snprintf(ifconfig, 255, "%s%s", VAR_IFCONFIG, dev->name);
	cp = variable_get(ifconfig);
	if (!cp)
	    return;
	msgNotify("ifconfig %s down", dev->name);
	i = vsystem("ifconfig %s down", dev->name);
	if (i)
	    msgConfirm("Warning: Unable to down the %s interface properly", dev->name);
	cp = variable_get(VAR_GATEWAY);
	if (cp) {
	    msgNotify("Deleting default route.");
	    vsystem("route delete default");
	}
    }
    else if (pppPID) {
	msgNotify("Killing previous PPP process %d.", pppPID);
	kill(pppPID, SIGTERM);
	pppPID = 0;
    }
    networkInitialized = FALSE;
}

/* Start PPP on the 3rd screen */
static pid_t
startPPP(Device *devp)
{
    int fd2;
    FILE *fp;
    char *val;
    pid_t pid = 0;
    char myaddr[16], provider[16], speed[16];

    /* These are needed to make ppp work */
    Mkdir("/var/log");
    Mkdir("/var/spool/lock");
    Mkdir("/etc/ppp");

    dialog_clear_norefresh();
    if (!variable_get(VAR_SERIAL_SPEED))
	variable_set2(VAR_SERIAL_SPEED, "115200");
    /* Get any important user values */
    val = variable_get_value(VAR_SERIAL_SPEED,
		      "Enter the baud rate for your modem - this can be higher than the actual\n"
		      "maximum data rate since most modems can talk at one speed to the\n"
		      "computer and at another speed to the remote end.\n\n"
		      "If you're not sure what to put here, just select the default.");
    SAFE_STRCPY(speed, (val && *val) ? val : "115200");

    val = variable_get(VAR_GATEWAY);
    SAFE_STRCPY(provider, (val && *val) ? val : "0");

    val = msgGetInput(provider, "Enter the IP address of your service provider or 0 if you\n"
		      "don't know it and would prefer to negotiate it dynamically.");
    SAFE_STRCPY(provider, (val && *val) ? val : "0");

    if (devp->private && ((DevInfo *)devp->private)->ipaddr[0])
	SAFE_STRCPY(myaddr, ((DevInfo *)devp->private)->ipaddr);
    else
	strcpy(myaddr, "0");

    if (!Fake)
	fp = fopen("/etc/ppp/ppp.linkup", "w");
    else
	fp = fopen("/dev/stderr", "w");
    if (fp != NULL) {
	fprintf(fp, "MYADDR:\n");
	fprintf(fp, " delete ALL\n");
        fprintf(fp, " add 0 0 HISADDR\n");
	fchmod(fileno(fp), 0755);
	fclose(fp);
    }
    if (!Fake)
	fd2 = open("/etc/ppp/ppp.secret", O_CREAT);
    else
	fd2 = -1;
    if (fd2 != -1) {
	fchmod(fd2, 0700);
	close(fd2);
    }
    if (!Fake)
	fp = fopen("/etc/ppp/ppp.conf", "w");
    else
	fp = fopen("/dev/stderr", "w");
    if (!fp) {
	msgConfirm("Couldn't open /etc/ppp/ppp.conf file!  This isn't going to work");
	return 0;
    }
    fprintf(fp, "default:\n");
    fprintf(fp, " set speed %s\n", speed);
    fprintf(fp, " set device %s\n", devp->devname);
    fprintf(fp, " set ifaddr %s %s\n", myaddr, provider);
    fclose(fp);

    if (!Fake && !file_readable("/dev/tun0") && mknod("/dev/tun0", 0600 | S_IFCHR, makedev(52, 0))) {
	msgConfirm("Warning:  No /dev/tun0 device.  PPP will not work!");
	return 0;
    }

    if (!Fake && !(pid = fork())) {
	int i, fd;
	struct termios foo;
	extern int login_tty(int);

	for (i = 0; i < 64; i++)
	    close(i);

	/* We're going over to VTY2 */
	DebugFD = fd = open("/dev/ttyv2", O_RDWR);
	ioctl(0, TIOCSCTTY, &fd);
	dup2(0, 1);
	dup2(0, 2);
	if (login_tty(fd) == -1)
	    msgDebug("ppp: Can't set the controlling terminal.\n");
	signal(SIGTTOU, SIG_IGN);
	if (tcgetattr(fd, &foo) != -1) {
	    foo.c_cc[VERASE] = '\010';
	    if (tcsetattr(fd, TCSANOW, &foo) == -1)
		msgDebug("ppp: Unable to set the erase character.\n");
	}
	else
	    msgDebug("ppp: Unable to get the terminal attributes!\n");
	execlp("ppp", "ppp", (char *)NULL);
	msgDebug("PPP process failed to exec!\n");
	exit(1);
    }
    else {
	msgConfirm("NOTICE: The PPP command is now started on VTY3 (type ALT-F3 to\n"
		   "interact with it, ALT-F1 to switch back here). The only command\n"
		   "you'll probably want or need to use is the \"term\" command\n"
		   "which starts a terminal emulator you can use to talk to your\n"
		   "modem and dial the service provider.  Once you're connected,\n"
		   "come back to this screen and press return.\n\n"
		   "DO NOT PRESS [ENTER] HERE UNTIL THE CONNECTION IS FULLY\n"
		   "ESTABLISHED!");
    }
    return pid;
}
