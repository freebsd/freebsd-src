/*
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *      Gary J Palmer. All rights reserved.
 * Copyright (c) 1996
 *      Jordan K. Hubbard. All rights reserved.
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

/*
 * All kinds of hacking also performed by jkh on this code.  Don't
 * blame Gary for every bogosity you see here.. :-)
 *
 * -jkh
 */

#include "sysinstall.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* The help file for the TCP/IP setup screen */
#define TCP_HELPFILE		"tcp"

/* These are nasty, but they make the layout structure a lot easier ... */

static char	hostname[HOSTNAME_FIELD_LEN], domainname[HOSTNAME_FIELD_LEN],
		gateway[IPADDR_FIELD_LEN], nameserver[INET6_ADDRSTRLEN];
static int	okbutton, cancelbutton;
static char	ipaddr[IPADDR_FIELD_LEN], netmask[IPADDR_FIELD_LEN], extras[EXTRAS_FIELD_LEN];
static char	ipv6addr[INET6_ADDRSTRLEN];

/* What the screen size is meant to be */
#define TCP_DIALOG_Y		0
#define TCP_DIALOG_X		8
#define TCP_DIALOG_WIDTH	COLS - 16
#define TCP_DIALOG_HEIGHT	LINES - 2

static Layout layout[] = {
#define LAYOUT_HOSTNAME		0
    { 1, 2, 25, HOSTNAME_FIELD_LEN - 1,
      "Host:", "Your fully-qualified hostname, e.g. foo.bar.com",
      hostname, STRINGOBJ, NULL },
#define LAYOUT_DOMAINNAME	1
    { 1, 35, 20, HOSTNAME_FIELD_LEN - 1,
      "Domain:",
      "The name of the domain that your machine is in, e.g. bar.com",
      domainname, STRINGOBJ, NULL },
#define LAYOUT_GATEWAY		2
    { 5, 2, 18, IPADDR_FIELD_LEN - 1,
      "IPv4 Gateway:",
      "IPv4 address of host forwarding packets to non-local destinations",
      gateway, STRINGOBJ, NULL },
#define LAYOUT_NAMESERVER	3
    { 5, 35, 18, INET6_ADDRSTRLEN - 1,
      "Name server:", "IPv4 or IPv6 address of your local DNS server",
      nameserver, STRINGOBJ, NULL },
#define LAYOUT_IPADDR		4
    { 10, 10, 18, IPADDR_FIELD_LEN - 1,
      "IPv4 Address:",
      "The IPv4 address to be used for this interface",
      ipaddr, STRINGOBJ, NULL },
#define LAYOUT_NETMASK		5
    { 10, 35, 18, IPADDR_FIELD_LEN - 1,
      "Netmask:",
      "The netmask for this interface, e.g. 0xffffff00 for a class C network",
      netmask, STRINGOBJ, NULL },
#define LAYOUT_EXTRAS		6
    { 14, 10, 37, HOSTNAME_FIELD_LEN - 1,
      "Extra options to ifconfig:",
      "Any interface-specific options to ifconfig you would like to add",
      extras, STRINGOBJ, NULL },
#define LAYOUT_OKBUTTON		7
    { 19, 15, 0, 0,
      "OK", "Select this if you are happy with these settings",
      &okbutton, BUTTONOBJ, NULL },
#define LAYOUT_CANCELBUTTON	8
    { 19, 35, 0, 0,
      "CANCEL", "Select this if you wish to cancel this screen",
      &cancelbutton, BUTTONOBJ, NULL },
    { NULL },
};

#define _validByte(b) ((b) >= 0 && (b) <= 255)

/* whine */
static void
feepout(char *msg)
{
    beep();
    msgConfirm("%s", msg);
}

/* Verify IP address integrity */
static int
verifyIP(char *ip, unsigned long *mask, unsigned long *out)
{
    long a, b, c, d;
    char *endptr;

    unsigned long parsedip;
    unsigned long max_addr = (255 << 24) | (255 << 16) | (255 << 8) | 255;

    if (ip == NULL)
	return 0;
    a = strtol(ip, &endptr, 10);
    if (*endptr++ != '.')
	return 0;
    b = strtol(endptr, &endptr, 10);
    if (*endptr++ != '.')
	return 0;
    c = strtol(endptr, &endptr, 10);
    if (*endptr++ != '.')
	return 0;
    d = strtol(endptr, &endptr, 10);
    if (*endptr != '\0')
	return 0;
    if (!_validByte(a) || !_validByte(b) || !_validByte(c) || !_validByte(d))
	return 0;
    parsedip = (a << 24) | (b << 16) | (c << 8) | d;
    if (out) 
	*out = parsedip;
    /*
     * The ip address must not be network or broadcast address.
     */
    if (mask && ((parsedip == (parsedip & *mask)) || 
	(parsedip == ((parsedip & *mask) + max_addr - *mask))))
	return 0;
    return 1;
}

static int
verifyIP6(char *ip)
{
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
    if (getaddrinfo(ip, NULL, &hints, &res) == 0) {
	freeaddrinfo(res);
	return 1;
    }
    return 0;
}

/* Verify IPv4 netmask as being well-formed as
   a 0x or AAA.BBB.CCC.DDD mask */
static int
verifyNetmask(const char *netmask, unsigned long *out)
{
    unsigned long mask;
    unsigned long tmp;
    char *endptr;

    if (netmask[0] == '0' && (netmask[1] == 'x' || netmask[1] == 'X')) {
        /* Parse out hex mask */
        mask = strtoul(netmask, &endptr, 0);
        if (*endptr != '\0')
            return 0;
    } else {
        /* Parse out quad decimal mask */
        mask = strtoul(netmask, &endptr, 10);
        if (!_validByte(mask) || *endptr++ != '.')
            return 0;
        tmp = strtoul(endptr, &endptr, 10);
        if (!_validByte(tmp) || *endptr++ != '.')
            return 0;
	mask = (mask << 8) + tmp;
        tmp = strtoul(endptr, &endptr, 10);
        if (!_validByte(tmp) || *endptr++ != '.')
            return 0;
	mask = (mask << 8) + tmp;
        tmp = strtoul(endptr, &endptr, 10);
        if (!_validByte(tmp) || *endptr++ != '\0')
            return 0;
	mask = (mask << 8) + tmp;
    }
    /* Verify that we have a continous netmask */
    if ((((-mask & mask) - 1) | mask) != 0xffffffff)
        return 0;
    if (out)
        *out = mask;
    return 1;
}

static int
verifyGW(char *gw, unsigned long *ip, unsigned long *mask)
{
    unsigned long parsedgw;

    if (!verifyIP(gw, mask, &parsedgw))
	return 0;
    /* Gateway needs to be within the set of IPs reachable through the
       interface */
    if (ip && mask && ((parsedgw & *mask) != (*ip & *mask)))
	return 0;
    return 1;
}

/* Check for the settings on the screen - the per-interface stuff is
   moved to the main handling code now to do it on the fly - sigh */
static int
verifySettings(void)
{
    unsigned long parsedip;
    unsigned long parsednetmask;

    if (!hostname[0])
	feepout("Must specify a host name of some sort!");
    else if (netmask[0] && !verifyNetmask(netmask, &parsednetmask))
	feepout("Invalid netmask value");
    else if (nameserver[0] && !verifyIP(nameserver, NULL, NULL) &&
		    !verifyIP6(nameserver))
	feepout("Invalid name server IP address specified");
    else if (ipaddr[0] && !verifyIP(ipaddr, &parsednetmask, &parsedip))
	feepout("Invalid IPv4 address");
    else if (gateway[0] && strcmp(gateway, "NO") &&
	     !verifyGW(gateway, ipaddr[0] ? &parsedip : NULL,
		     netmask[0] ? &parsednetmask : NULL))
	feepout("Invalid gateway IPv4 address specified");
    else
	return 1;
    return 0;
}

static void
dhcpGetInfo(Device *devp)
{
    /* If it fails, do it the old-fashioned way */
    if (dhcpParseLeases("/var/db/dhclient.leases", hostname, domainname,
			 nameserver, ipaddr, gateway, netmask) == -1) {
	FILE *ifp;
	char *cp, cmd[256], data[2048];
	int i, j;

	/* Bah, now we have to kludge getting the information from ifconfig */
	snprintf(cmd, sizeof cmd, "ifconfig %s", devp->name);
	ifp = popen(cmd, "r");
	if (ifp) {
	    j = fread(data, 1, sizeof(data), ifp);
	    fclose(ifp);
	    if (j < 0)	/* paranoia */
		j = 0;
	    data[j] = '\0';
	    if (isDebug())
		msgDebug("DHCP configured interface returns %s\n", data);
	    /* XXX This is gross as it assumes a certain ordering to
	       ifconfig's output! XXX */
	    if ((cp = strstr(data, "inet ")) != NULL) {
		i = 0;
		cp += 5;	/* move over keyword */
		while (*cp != ' ')
		    ipaddr[i++] = *(cp++);
		ipaddr[i] = '\0';
		if (!strncmp(++cp, "netmask", 7)) {
		    i = 0;
		    cp += 8;
		    while (*cp != ' ')
			netmask[i++] = *(cp++);
		    netmask[i] = '\0';
		}
	    }
	}
    }

    /* If we didn't get a name server value, hunt for it in resolv.conf */
    if (!nameserver[0] && file_readable("/etc/resolv.conf"))
	configEnvironmentResolv("/etc/resolv.conf");
    if (hostname[0])
	variable_set2(VAR_HOSTNAME, hostname, 0);
}

static void
rtsolGetInfo(Device *devp)
{
    FILE *ifp;
    char *cp, cmd[256], data[2048];
    int i;

    snprintf(cmd, sizeof cmd, "ifconfig %s", devp->name);
    if ((ifp = popen(cmd, "r")) == NULL)
	return;
    while (fgets(data, sizeof(data), ifp) != NULL) {
	if (isDebug())
	    msgDebug("RTSOL configured interface returns %s", data);
	if ((cp = strstr(data, "inet6 ")) != NULL) {
	    cp += 6;	/* move over keyword */
	    if (strncmp(cp, "fe80:", 5)) {
		i = 0;
		while (*cp != ' ')
		    ipv6addr[i++] = *(cp++);
		ipv6addr[i] = '\0';
	    }
	}
    }
    fclose(ifp);
}

/* This is it - how to get TCP setup values */
int
tcpOpenDialog(Device *devp)
{
    WINDOW              *ds_win, *save = NULL;
    ComposeObj          *obj = NULL;
    int                 n = 0, filled = 0, cancel = FALSE;
    int			max, ret = DITEM_SUCCESS;
    int			use_dhcp = FALSE;
    int			use_rtsol = FALSE;
    char                *tmp;
    char		title[80];

    save = savescr();
    /* Initialise vars from previous device values */
    if (devp->private) {
	DevInfo *di = (DevInfo *)devp->private;

	SAFE_STRCPY(ipaddr, di->ipaddr);
	SAFE_STRCPY(netmask, di->netmask);
	SAFE_STRCPY(extras, di->extras);
	use_dhcp = di->use_dhcp;
	use_rtsol = di->use_rtsol;
    }
    else { /* See if there are any defaults */
	char *cp;
	char *old_interactive = NULL;

	/*
	 * This is a hack so that the dialogs below are interactive in a
	 * script if we have requested interactive behavior.
	 */
	if (variable_get(VAR_NONINTERACTIVE) &&
	  variable_get(VAR_NETINTERACTIVE)) {
	    old_interactive = strdup(VAR_NONINTERACTIVE);
	    variable_unset(VAR_NONINTERACTIVE);
	}


	/*
	 * Try a RTSOL scan if such behavior is desired.
	 * If the variable was configured and is YES, do it.
	 * If it was configured to anything else, treat it as NO.
	 * Otherwise, ask the question interactively.
	 */
	if (!variable_cmp(VAR_TRY_RTSOL, "YES") || 
	    (variable_get(VAR_TRY_RTSOL)==0 && !msgNoYes("Do you want to try IPv6 configuration of the interface?"))) {
	    int i;
	    size_t len;

	    i = 0;
	    sysctlbyname("net.inet6.ip6.forwarding", NULL, 0, &i, sizeof(i));
	    i = 1;
	    sysctlbyname("net.inet6.ip6.accept_rtadv", NULL, 0, &i, sizeof(i));
	    vsystem("ifconfig %s up", devp->name);
	    len = sizeof(i);
	    sysctlbyname("net.inet6.ip6.dad_count", &i, &len, NULL, 0);
	    sleep(i + 1);
	    Mkdir("/var/run");
	    msgNotify("Scanning for RA servers...");
	    if (0 == vsystem("rtsol %s", devp->name)) {
		len = sizeof(i);
		sysctlbyname("net.inet6.ip6.dad_count", &i, &len, NULL, 0);
		sleep(i + 1);
		rtsolGetInfo(devp);
		use_rtsol = TRUE;
	    } else
		use_rtsol = FALSE;
	}


	/*
	 * First try a DHCP scan if such behavior is desired.
	 * If the variable was configured and is YES, do it.
	 * If it was configured to anything else, treat it as NO.
	 * Otherwise, ask the question interactively.
	 */
	if (!variable_cmp(VAR_TRY_DHCP, "YES") || 
	    (variable_get(VAR_TRY_DHCP)==0 && !msgNoYes("Do you want to try DHCP configuration of the interface?"))) {
	    Mkdir("/var/db");
	    Mkdir("/var/run");
	    Mkdir("/tmp");
	    msgNotify("Scanning for DHCP servers...");
	    if (0 == vsystem("dhclient -1 %s", devp->name)) {
		dhcpGetInfo(devp);
		use_dhcp = TRUE;
	    }
	    else
		use_dhcp = FALSE;
	}

	/* Restore old VAR_NONINTERACTIVE if needed. */
	if (old_interactive != NULL) {
	    variable_set2(VAR_NONINTERACTIVE, old_interactive, 0);
	    free(old_interactive);
	}

	/* Special hack so it doesn't show up oddly in the tcpip setup menu */
	if (!strcmp(gateway, "NO"))
	    gateway[0] = '\0';

	/* Get old IP address from variable space, if available */
	if (!ipaddr[0]) {
	    if ((cp = variable_get(VAR_IPADDR)) != NULL)
		SAFE_STRCPY(ipaddr, cp);
	    else if ((cp = variable_get(string_concat3(devp->name, "_", VAR_IPADDR))) != NULL)
		SAFE_STRCPY(ipaddr, cp);
	}

	/* Get old netmask from variable space, if available */
	if (!netmask[0]) {
	    if ((cp = variable_get(VAR_NETMASK)) != NULL)
		SAFE_STRCPY(netmask, cp);
	    else if ((cp = variable_get(string_concat3(devp->name, "_", VAR_NETMASK))) != NULL)
		SAFE_STRCPY(netmask, cp);
	}

	/* Get old extras string from variable space, if available */
	if (!extras[0]) {
	    if ((cp = variable_get(VAR_EXTRAS)) != NULL)
		SAFE_STRCPY(extras, cp);
	    else if ((cp = variable_get(string_concat3(devp->name, "_", VAR_EXTRAS))) != NULL)
		SAFE_STRCPY(extras, cp);
	}
    }

    /* Look up values already recorded with the system, or blank the string variables ready to accept some new data */
    if (!hostname[0]) {
	tmp = variable_get(VAR_HOSTNAME);
	if (tmp)
	    SAFE_STRCPY(hostname, tmp);
    }
    if (!domainname[0]) {
	tmp = variable_get(VAR_DOMAINNAME);
	if (tmp)
	    SAFE_STRCPY(domainname, tmp);
    }
    if (!gateway[0]) {
	tmp = variable_get(VAR_GATEWAY);
	if (tmp && strcmp(tmp, "NO"))
	    SAFE_STRCPY(gateway, tmp);
    }
    if (!nameserver[0]) {
	tmp = variable_get(VAR_NAMESERVER);
	if (tmp)
	    SAFE_STRCPY(nameserver, tmp);
    }

    /* If non-interactive, jump straight over the dialog crap and into config section */
    if (variable_get(VAR_NONINTERACTIVE) &&
	!variable_get(VAR_NETINTERACTIVE)) {
	if (!hostname[0])
	    msgConfirm("WARNING: hostname variable not set and is a non-optional\n"
		       "parameter.  Please add this to your installation script\n"
		       "or set the netInteractive variable (see sysinstall man page)");
	else
	    goto netconfig;
    }

    /* Now do all the screen I/O */
    dialog_clear_norefresh();

    /* Modify the help line for PLIP config */
    if (!strncmp(devp->name, "lp", 2))
	layout[LAYOUT_EXTRAS].help = 
         "For PLIP configuration, you must enter the peer's IP address here.";

    /* We need a curses window */
    tmp = " Network Configuration ";
    if (ipv6addr[0])
	tmp = string_concat(tmp, "(IPv6 ready) ");
    if (!(ds_win = openLayoutDialog(TCP_HELPFILE, tmp,
				    TCP_DIALOG_X, TCP_DIALOG_Y, TCP_DIALOG_WIDTH, TCP_DIALOG_HEIGHT))) {
	beep();
	msgConfirm("Cannot open TCP/IP dialog window!!");
	restorescr(save);
	return DITEM_FAILURE;
    }

    /* Draw interface configuration box */
    draw_box(ds_win, TCP_DIALOG_Y + 9, TCP_DIALOG_X + 8, TCP_DIALOG_HEIGHT - 13, TCP_DIALOG_WIDTH - 17,
	     dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    sprintf(title, " Configuration for Interface %s ", devp->name);
    mvwaddstr(ds_win, TCP_DIALOG_Y + 9, TCP_DIALOG_X + 14, title);

    /* Some more initialisation before we go into the main input loop */
    obj = initLayoutDialog(ds_win, layout, TCP_DIALOG_X, TCP_DIALOG_Y, &max);

reenter:
    cancelbutton = okbutton = 0;
    while (layoutDialogLoop(ds_win, layout, &obj, &n, max, &cancelbutton, &cancel)) {
	/* Prevent this from being irritating if user really means NO */
	if (filled < 3) {
	    /* Insert a default value for the netmask, 0xffffff00 is
	     * the most appropriate one (entire class C, or subnetted
	     * class A/B network).
	     */
	    if (!netmask[0]) {
		strcpy(netmask, "255.255.255.0");
		RefreshStringObj(layout[LAYOUT_NETMASK].obj);
		++filled;
	    }
	    if (!index(hostname, '.') && domainname[0]) {
		strcat(hostname, ".");
		strcat(hostname, domainname);
		RefreshStringObj(layout[LAYOUT_HOSTNAME].obj);
		++filled;
	    }
	    else if (((tmp = index(hostname, '.')) != NULL) && !domainname[0]) {
		SAFE_STRCPY(domainname, tmp + 1);
		RefreshStringObj(layout[LAYOUT_DOMAINNAME].obj);
		++filled;
	    }
	}
    }
    if (!cancel && !verifySettings())
	goto reenter;

    /* Clear this crap off the screen */
    delwin(ds_win);
    dialog_clear_norefresh();
    use_helpfile(NULL);

    /* We actually need to inform the rest of sysinstall about this
       data now if the user hasn't selected cancel.  Save the stuff
       out to the environment via the variable_set() mechanism */

netconfig:
    if (!cancel) {
	DevInfo *di;
	char temp[512], ifn[255];
	char *pccard;
	int ipv4_enable = FALSE;

	if (hostname[0]) {
	    variable_set2(VAR_HOSTNAME, hostname, 1);
	    sethostname(hostname, strlen(hostname));
	}
	if (domainname[0])
	    variable_set2(VAR_DOMAINNAME, domainname, 0);
	if (gateway[0])
	    variable_set2(VAR_GATEWAY, gateway, use_dhcp ? 0 : 1);
	if (nameserver[0])
	    variable_set2(VAR_NAMESERVER, nameserver, 0);
	if (ipaddr[0])
	    variable_set2(VAR_IPADDR, ipaddr, 0);
	if (ipv6addr[0])
	    variable_set2(VAR_IPV6ADDR, ipv6addr, 0);

	if (!devp->private)
	    devp->private = (DevInfo *)safe_malloc(sizeof(DevInfo));
	di = devp->private;
	SAFE_STRCPY(di->ipaddr, ipaddr);
	SAFE_STRCPY(di->netmask, netmask);
	SAFE_STRCPY(di->extras, extras);
	di->use_dhcp = use_dhcp;
	di->use_rtsol = use_rtsol;

	if (use_dhcp || ipaddr[0])
	    ipv4_enable = TRUE;
	if (ipv4_enable) {
	    sprintf(ifn, "%s%s", VAR_IFCONFIG, devp->name);
	    if (use_dhcp)
		sprintf(temp, "DHCP");
	    else
		sprintf(temp, "inet %s %s netmask %s",
			ipaddr, extras, netmask);
	    variable_set2(ifn, temp, 1);
	}
#ifdef PCCARD_ARCH
	pccard = variable_get("_pccard_install");
	if (pccard && strcmp(pccard, "YES") == 0 && ipv4_enable) {
	    variable_set2("pccard_ifconfig", temp, 1);
	}
#endif
	if (use_rtsol)
	    variable_set2(VAR_IPV6_ENABLE, "YES", 1);
	if (!use_dhcp)
	    configResolv(NULL);	/* XXX this will do it on the MFS copy XXX */
	ret = DITEM_SUCCESS;
    }
    else
	ret = DITEM_FAILURE;
    restorescr(save);
    return ret;
}

static Device *NetDev;

static int
netHook(dialogMenuItem *self)
{
    Device **devs;

    devs = deviceFindDescr(self->prompt, self->title, DEVICE_TYPE_NETWORK);
    if (devs) {
	if (DITEM_STATUS(tcpOpenDialog(devs[0])) != DITEM_FAILURE)
	    NetDev = devs[0];
	else
	    NetDev = NULL;
    }
    return devs ? DITEM_LEAVE_MENU : DITEM_FAILURE;
}

/* Get a network device */
Device *
tcpDeviceSelect(void)
{
    DMenu *menu;
    Device **devs, *rval;
    int cnt;

    devs = deviceFind(variable_get(VAR_NETWORK_DEVICE), DEVICE_TYPE_NETWORK);
    cnt = deviceCount(devs);
    rval = NULL;

    if (!cnt) {
	msgConfirm("No network devices available!");
	return NULL;
    }
    else if ((!RunningAsInit) && (variable_check("NETWORK_CONFIGURED=NO") != TRUE)) {
	if (!msgYesNo("Running multi-user, assume that the network is already configured?"))
	    return devs[0];
    }
    if (cnt == 1) {
	if (DITEM_STATUS(tcpOpenDialog(devs[0]) == DITEM_SUCCESS))
	    rval = devs[0];
    }
    else if (variable_get(VAR_NONINTERACTIVE) && variable_get(VAR_NETWORK_DEVICE)) {
	devs = deviceFind(variable_get(VAR_NETWORK_DEVICE), DEVICE_TYPE_NETWORK);
	cnt = deviceCount(devs);
	if (cnt) {
	    if (DITEM_STATUS(tcpOpenDialog(devs[0]) == DITEM_SUCCESS))
		rval = devs[0];
	}
    }
    else {
	int status;

	menu = deviceCreateMenu(&MenuNetworkDevice, DEVICE_TYPE_NETWORK, netHook, NULL);
	if (!menu)
	    msgFatal("Unable to create network device menu!  Argh!");
	status = dmenuOpenSimple(menu, FALSE);
	free(menu);
	if (status)
	    rval = NetDev;
    }
    return rval;
}

/* Do it from a menu that doesn't care about status */
int
tcpMenuSelect(dialogMenuItem *self)
{
    Device *tmp;
    WINDOW *save;

    variable_set("NETWORK_CONFIGURED=NO",0);
    tmp = tcpDeviceSelect();
    variable_unset("NETWORK_CONFIGURED");
    save = savescr();
    if (tmp && tmp->private && !((DevInfo *)tmp->private)->use_dhcp && !msgYesNo("Would you like to bring the %s interface up right now?", tmp->name))
	if (!DEVICE_INIT(tmp))
	    msgConfirm("Initialization of %s device failed.", tmp->name);
    restorescr(save);
    return DITEM_SUCCESS;
}
