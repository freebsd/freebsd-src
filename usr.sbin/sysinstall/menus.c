/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: menus.c,v 1.77 1996/07/02 01:03:47 jkh Exp $
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

#include "sysinstall.h"

/* Miscellaneous work routines for menus */
static int
setSrc(dialogMenuItem *self)
{
    Dists |= DIST_SRC;
    SrcDists = DIST_SRC_ALL | DIST_SRC_SMAILCF;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearSrc(dialogMenuItem *self)
{
    Dists &= ~DIST_SRC;
    SrcDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11All(dialogMenuItem *self)
{
    XF86Dists = DIST_XF86_ALL;
    XF86ServerDists = DIST_XF86_SERVER_ALL;
    XF86FontDists = DIST_XF86_FONTS_ALL;
    Dists |= DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11All(dialogMenuItem *self)
{
    XF86Dists = 0;
    XF86ServerDists = 0;
    XF86FontDists = 0;
    Dists &= ~DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11Misc(dialogMenuItem *self)
{
    XF86Dists |= DIST_XF86_MISC_ALL;
    Dists |= DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11Misc(dialogMenuItem *self)
{
    XF86Dists &= ~DIST_XF86_MISC_ALL;
    if (!XF86ServerDists && !XF86FontDists)
	Dists &= ~DIST_XF86;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11Servers(dialogMenuItem *self)
{
    XF86Dists |= DIST_XF86_SERVER;
    XF86ServerDists = DIST_XF86_SERVER_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11Servers(dialogMenuItem *self)
{
    XF86Dists &= ~DIST_XF86_SERVER;
    XF86ServerDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
setX11Fonts(dialogMenuItem *self)
{
    XF86Dists |= DIST_XF86_FONTS;
    XF86FontDists = DIST_XF86_FONTS_ALL;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearX11Fonts(dialogMenuItem *self)
{
    XF86Dists &= ~DIST_XF86_FONTS;
    XF86FontDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

#define IS_DEVELOPER(dist, extra) (((dist) == (_DIST_DEVELOPER | (extra))) || ((dist) == (_DIST_DEVELOPER | DIST_DES | (extra))))
#define IS_USER(dist, extra) (((dist) == (_DIST_USER | (extra))) || ((dist) == (_DIST_USER | DIST_DES | (extra))))

static int
checkDistDeveloper(dialogMenuItem *self)
{
    return (IS_DEVELOPER(Dists, 0) && SrcDists == DIST_SRC_ALL);
}

static int
checkDistXDeveloper(dialogMenuItem *self)
{
    return (IS_DEVELOPER(Dists, DIST_XF86) && SrcDists == DIST_SRC_ALL);
}

static int
checkDistKernDeveloper(dialogMenuItem *self)
{
    return (IS_DEVELOPER(Dists, 0) && SrcDists == DIST_SRC_SYS);
}

static int
checkDistUser(dialogMenuItem *self)
{
    return (IS_USER(Dists, 0));
}

static int
checkDistXUser(dialogMenuItem *self)
{
    return (IS_USER(Dists, DIST_XF86));
}

static int
checkDistMinimum(dialogMenuItem *self)
{
    return (Dists == DIST_BIN);
}

static int
checkDistEverything(dialogMenuItem *self)
{
    return (Dists == DIST_ALL && SrcDists == DIST_SRC_ALL && XF86Dists == DIST_XF86_ALL &&
	    XF86ServerDists == DIST_XF86_SERVER_ALL && XF86FontDists == DIST_XF86_FONTS_ALL);
}

static int
DESFlagCheck(dialogMenuItem *item)
{
    return DESDists;
}

static int
srcFlagCheck(dialogMenuItem *item)
{
    return SrcDists;
}

static int
x11FlagCheck(dialogMenuItem *item)
{
    return XF86Dists;
}

static int
checkTrue(dialogMenuItem *item)
{
    return TRUE;
}

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

DMenu MenuIndex = {
    DMENU_NORMAL_TYPE,
    "Glossary of functions",
    "This menu contains an alphabetized index of the top level functions in\n"
    "this program (sysinstall).  Invoke an option by pressing [ENTER].\n"
    "Leave the index page by selecting Cancel [TAB-ENTER].",
    "Use PageUp or PageDown to move through this menu faster!",
    NULL,
    { { "Add User",	"Add users to the system.", NULL, dmenuSystemCommand, NULL, "adduser -config_create ; adduser -s" },
      { "Anon FTP",	"Configure anonymous FTP logins.",	dmenuVarCheck, configAnonFTP, NULL, "anon_ftp" },
      { "Commit",	"Commit any pending actions (dangerous!)", NULL, installCustomCommit },
      { "Console settings",	"Customize system console behavior.", NULL, dmenuSubmenu, NULL, &MenuSyscons },
      { "Configure",		"The system configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { "Device, Mouse",	"The mouse configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuMouse },
      { "Dists, All",	"Root of the distribution tree.",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "Dists, Basic",	"Basic FreeBSD distribution menu.",	NULL, dmenuSubmenu, NULL, &MenuSubDistributions },
      { "Dists, DES",	"DES distribution menu.",		NULL, dmenuSubmenu, NULL, &MenuDESDistributions },
      { "Dists, Developer", "Select developer's distribution.", checkDistDeveloper, distSetDeveloper },
      { "Dists, Src",	"Src distribution menu.",		NULL, dmenuSubmenu, NULL, &MenuSrcDistributions },
      { "Dists, X Developer", "Select X developer's distribution.",	checkDistXDeveloper, distSetXDeveloper },
      { "Dists, Kern Developer", "Select kernel developer's distribution.", checkDistKernDeveloper, distSetKernDeveloper },
      { "Dists, User",		"Select average user distribution.", checkDistUser, distSetUser },
      { "Dists, X User",	"Select average X user distribution.", checkDistXUser, distSetXUser },
      { "Distributions, XFree86","XFree86 distribution menu.",		NULL, distSetXF86 },
      { "Documentation",	"Installation instructions, README, etc.", NULL, dmenuSubmenu, NULL, &MenuDocumentation },
      { "Doc, README",		"The distribution README file.",	NULL, dmenuDisplayFile, NULL, "readme" },
      { "Doc, Hardware",	"The distribution hardware guide.",	NULL, dmenuDisplayFile,	NULL, "hardware" },
      { "Doc, Install",		"The distribution installation guide.",	NULL, dmenuDisplayFile,	NULL, "install" },
      { "Doc, Copyright",	"The distribution copyright notices.",	NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { "Doc, Release",		"The distribution release notes.",	NULL, dmenuDisplayFile, NULL, "relnotes" },
      { "Doc, HTML",		"The HTML documentation menu.",		NULL, docBrowser },
      { "Extract",		"Extract selected distributions from media.",		NULL, distExtractAll },
      { "Fixit",		"Repair mode with CDROM or floppy.",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { "FTP sites",		"The FTP mirror site listing.",		NULL, dmenuSubmenu, NULL, &MenuMediaFTP },
      { "Gated",		"Load and configure gated instead of routed.",  dmenuVarCheck, configGated, NULL, "gated" },
      { "Gateway",		"Set flag to route packets between interfaces.", dmenuVarCheck, dmenuToggleVariable, NULL, "gateway=YES" },
      { "HTML Docs",		"The HTML documentation menu",		NULL, docBrowser },
      { "Install, Novice",	"A novice system installation.",	NULL, installNovice },
      { "Install, Express",	"An express system installation.",	NULL, installExpress },
      { "Install, Custom",	"The custom installation menu",		NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { "Label",		"The disk Label editor",		NULL, diskLabelEditor },
      { "Media",		"Top level media selection menu.",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "Media, Tape",		"Select tape installation media.",	NULL, mediaSetTape },
      { "Media, NFS",		"Select NFS installation media.",	NULL, mediaSetNFS },
      { "Media, Floppy",	"Select floppy installation media.",	NULL, mediaSetFloppy },
      { "Media, CDROM",		"Select CDROM installation media.",	NULL, mediaSetCDROM },
      { "Media, DOS",		"Select DOS installation media.",	NULL, mediaSetDOS },
      { "Media, UFS",		"Select UFS installation media.",	NULL, mediaSetUFS },
      { "Media, FTP",		"Select FTP installation media.",	NULL, mediaSetFTP },
      { "Media, FTP Passive",	"Select passive FTP installation media.", NULL, mediaSetFTPPassive },
      { "Network Interfaces",	"Configure network interfaces",		NULL, tcpMenuSelect },
      { "Networking Services",	"The network services menu.",		NULL, dmenuSubmenu, NULL, &MenuNetworking },
      { "NFS, client",		"Set NFS client flag.",			dmenuVarCheck, dmenuToggleVariable, NULL, "nfs_client=YES" },
      { "NFS, server",		"Set NFS server flag.",			dmenuVarCheck, configNFSServer, NULL, "nfs_server" },
      { "NTP Menu",		"The NTP configuration menu.",		NULL, dmenuSubmenu, NULL, &MenuNTP },
      { "Options",		"The options editor.",			NULL, optionsEditor },
      { "Packages",		"The packages collection",		NULL, configPackages },
      { "Partition",		"The disk Partition Editor",		NULL, diskPartitionEditor },
      { "PCNFSD",		"Run authentication server for PC-NFS.",	dmenuVarCheck, configPCNFSD, NULL, "pcnfsd" },
      { "Ports",		"Link to FreeBSD ports collection.",	NULL, configPorts },
      { "Root Password",	"Set the system manager's password.",   NULL, dmenuSystemCommand, NULL, "passwd root" },
      { "Routed",		"Set flags for routed (default: -q)",	dmenuVarCheck, configRoutedFlags, NULL, "routed" },
      { "Samba",		"Configure Samba for LanManager access.", dmenuVarCheck, configSamba, NULL, "samba" },
      { "Syscons",		"The system console configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSyscons },
      { "Syscons, Keymap",	"The console keymap configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "Syscons, Keyrate",	"The console key rate configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "Syscons, Saver",	"The console screen saver configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
      { "Time Zone",		"Set the system's time zone.",		NULL, dmenuSystemCommand, NULL, "rm -f /etc/wall_cmos_clock /etc/localtime; tzsetup" },
      { "Upgrade",		"Upgrade an existing system.",		NULL, installUpgrade },
      { "Usage",		"Quick start - How to use this menu system.",	NULL, dmenuDisplayFile, NULL, "usage" },
      { "WEB Server",		"Configure host as a WWW server.",	dmenuVarCheck, configApache, NULL, "apache_httpd" },
      { "XFree86, Fonts",	"XFree86 Font selection menu.",		NULL, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
      { "XFree86, Server",	"XFree86 Server selection menu.",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectServer },
      { NULL } },
};
      
/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "Welcome to FreeBSD!",				/* title */
    "This is the main menu of the FreeBSD installation system.  Please\n" /* prompt */
    "select one of the options below by using the arrow keys or typing the\n"
    "first character of the option name you're interested in.  Invoke an\n"
    "option by pressing [ENTER] or [TAB-ENTER] to exit the installation.", 
    "Press F1 for Installation Guide",			/* help line */
    "install",						/* help file */
    { { "Select" },
      { "Exit Install",	NULL, NULL, dmenuExit },
      { "1 Usage",	"Quick start - How to use this menu system",		NULL, dmenuDisplayFile, NULL, "usage" },
      { "2 Doc",	"Installation instructions, README, etc.",		NULL, dmenuSubmenu, NULL, &MenuDocumentation },
      { "3 Options",	"Go to the options editor",				NULL, optionsEditor },
      { "4 Novice",	"Begin a novice installation (for beginners)",		NULL, installNovice },
      { "5 Express",	"Begin a quick installation (for the impatient)",	NULL, installExpress },
      { "6 Custom",	"Begin a custom installation (for experts)",		NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { "7 Fixit",	"Go into repair mode with CDROM or floppy",		NULL, dmenuSubmenu, NULL, &MenuFixit },
      { "8 Upgrade",	"Upgrade an existing 2.0.5 system",			NULL, installUpgrade },
      { "9 Configure",	"Do post-install configuration of FreeBSD",		NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { "0 Index",	"Glossary of functions.",		NULL, dmenuSubmenu, NULL, &MenuIndex },
      { NULL } },
};

DMenu MenuFixit = {
    DMENU_NORMAL_TYPE,
    "Please choose a fixit option",
"There are two ways of going into \"fixit\" mode - you may either elect\n\
to use the 2nd FreeBSD CDROM, in which case there will be full access\n\
access to the complete set of FreeBSD commands and utilities, or you\n\
can use the more limited fixit floppy if you don't have a CDROM or are\n\
somehow faced with a situation where a CDROM is impractical.  The fixit\n\
floppy has only a minimal subset of commands which we deemed most useful\n\
for fixing a system in trouble.",
    "Press F1 for more detailed repair instructions",
    "fixit",
{ { "1 CDROM",	"Use the 2nd \"live\" CDROM from the distribution",	NULL, installFixitCDROM },
  { "2 Floppy",	"Use a floppy generated from the fixit image",		NULL, installFixitFloppy },
  { NULL } },
};


/* The main documentation menu */
DMenu MenuDocumentation = {
    DMENU_NORMAL_TYPE,
    "Documentation for FreeBSD " RELEASE_NAME,
    "If you are at all unsure about the configuration of your hardware\n\
or are looking to build a system specifically for FreeBSD, read the\n\
Hardware guide!  New users should also read the Install document for\n\
a step-by-step tutorial on installing FreeBSD.  For general information,\n\
consult the README file.",
    "Confused?  Press F1 for help.",
    "usage",
{ { "1 README",	"A general description of FreeBSD.  Read this!",	NULL, dmenuDisplayFile, NULL, "readme" },
  { "2 Hardware","The FreeBSD survival guide for PC hardware.",		NULL, dmenuDisplayFile,	NULL, "hardware" },
  { "3 Install","A step-by-step guide to installing FreeBSD.",		NULL, dmenuDisplayFile,	NULL, "install" },
  { "4 Copyright","The FreeBSD Copyright notices.",			NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
  { "5 Release","The release notes for this version of FreeBSD.",	NULL, dmenuDisplayFile, NULL, "relnotes" },
  { "6 Shortcuts", "Creating shortcuts to sysinstall.",	NULL, dmenuDisplayFile, NULL, "shortcuts" },
  { "7 HTML Docs","Go to the HTML documentation menu (post-install).",	NULL, docBrowser },
  { "0 Exit",	"Exit this menu (returning to previous)",		NULL, dmenuExit },
  { NULL } },
};

static int
whichMouse(dialogMenuItem *self)
{
    char buf[BUFSIZ];

    if (!file_readable("/dev/mouse"))
	return FALSE;
    if (readlink("/dev/mouse", buf, BUFSIZ) == -1)
	return FALSE;
    if (!strcmp(self->prompt, "COM1"))
	return !strcmp(buf, "/dev/cuaa0");
    else if (!strcmp(self->prompt, "COM2"))
	return !strcmp(buf, "/dev/cuaa1");
    if (!strcmp(self->prompt, "COM3"))
	return !strcmp(buf, "/dev/cuaa2");
    if (!strcmp(self->prompt, "COM4"))
	return !strcmp(buf, "/dev/cuaa3");
    if (!strcmp(self->prompt, "BusMouse"))
	return !strcmp(buf, "/dev/msg0");
    if (!strcmp(self->prompt, "PS/2"))
	return !strcmp(buf, "/dev/psm0");
    return FALSE;
}

DMenu MenuMouse = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "Please select your mouse type from the following menu",
    "There are many different types of mice currently on the market,\n\
but this configuration menu should at least narrow down the choices\n\
somewhat.  Once you've selected one of the below, you can specify\n\
/dev/mouse as your mouse device when running the XFree86 configuration\n\
utility (see Configuration menu).  Please note that for PS/2 mice,\n\
a kernel recompile is also required!  See the handbook for more details\n\
on building a kernel.",
    "For more information, visit the Documentation menu",
    NULL,
{ { "COM1",	"Serial mouse on COM1",	whichMouse, dmenuSystemCommand, NULL,
    "ln -fs /dev/cuaa0 /dev/mouse", '(', '*', ')', 1 },
  { "COM2",	"Serial mouse on COM2", whichMouse, dmenuSystemCommand, NULL,
    "ln -fs /dev/cuaa1 /dev/mouse", '(', '*', ')', 1 },
  { "COM3",	"Serial mouse on COM3", whichMouse, dmenuSystemCommand, NULL,
    "ln -fs /dev/cuaa2 /dev/mouse", '(', '*', ')', 1 },
  { "COM4",	"Serial mouse on COM4", whichMouse, dmenuSystemCommand, NULL,
    "ln -fs /dev/cuaa3 /dev/mouse", '(', '*', ')', 1 },
  { "BusMouse",	"Logitech or ATI bus mouse", whichMouse, dmenuSystemCommand, NULL,
    "ln -fs /dev/mse0 /dev/mouse", '(', '*', ')', 1 },
  { "PS/2",	"PS/2 style mouse (requires kernel rebuild)", whichMouse, dmenuSystemCommand, NULL,
    "ln -fs /dev/psm0 /dev/mouse", '(', '*', ')', 1 },
  { NULL } },
};

DMenu MenuMediaCDROM = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a CDROM type",
    "FreeBSD can be installed directly from a CDROM containing a valid\n\
FreeBSD distribution.  If you are seeing this menu it is because\n\
more than one CDROM drive was found on your system.  Please select one\n\
of the following CDROM drives as your installation drive.",
    "Press F1 to read the installation guide",
    "install",
    { { NULL } },
};

DMenu MenuMediaFloppy = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a Floppy drive",
    "You have more than one floppy drive.  Please chose the drive\n\
you would like to use for this operation",
    NULL,
    NULL,
    { { NULL } },
};

DMenu MenuMediaDOS = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a DOS partition",
    "FreeBSD can be installed directly from a DOS partition\n\
assuming, of course, that you have copied the relevant\n\
distributions into your DOS partition before starting this\n\
installation.  If this is not the case then you should reboot\n\
DOS at this time and copy the distributions you wish to install\n\
into a \"FREEBSD\" subdirectory on one of your DOS partitions.\n\
Otherwise, please select the DOS partition containing the FreeBSD\n\
distribution files.",
    "Press F1 to read the installation guide",
    "install",
    { { NULL } },
};

DMenu MenuMediaFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select a FreeBSD FTP distribution site",
    "Please select the site closest to you or \"other\" if you'd like to\n\
specify a different choice.  Also note that not every site listed here\n\
carries more than the base distribution kits. Only the Primary site is\n\
guaranteed to carry the full range of possible distributions.",
    "Select a site that's close!",
    "install",
{ { "Primary Site",	"ftp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.freebsd.org/pub/FreeBSD/" },
  { "Other",		"Specify some other ftp site by URL", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=other" },
  { "Australia",	"ftp.au.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.au.freebsd.org/pub/FreeBSD/" },
  { "Australia #2",	"ftp2.au.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.au.freebsd.org/pub/FreeBSD/" },
  { "Australia #3",	"ftp3.au.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.au.freebsd.org/pub/FreeBSD/" },
  { "Australia #4",	"ftp4.au.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp4.au.freebsd.org/pub/FreeBSD/" },
  { "Brazil",		"ftp.br.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.br.freebsd.org/pub/FreeBSD/" },
  { "Brazil #2",	"ftp2.br.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.br.freebsd.org/pub/FreeBSD/" },
  { "Brazil #3",	"ftp3.br.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.br.freebsd.org/pub/FreeBSD/" },
  { "Brazil #4",	"ftp4.br.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp4.br.freebsd.org/pub/FreeBSD/" },
  { "Brazil #5",	"ftp5.br.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp5.br.freebsd.org/pub/FreeBSD/" },
  { "Canada",		"ftp.ca.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.ca.freebsd.org/pub/FreeBSD/" },
  { "Czech Republic",	"sunsite.mff.cuni.cz", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://sunsite.mff.cuni.cz/OS/FreeBSD/" },
  { "Estonia",		"ftp.ee.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.ee.freebsd.org/pub/FreeBSD/" },
  { "Finland",		"nic.funet.fi", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://nic.funet.fi/pub/unix/FreeBSD/" },
  { "France",		"ftp.ibp.fr", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.ibp.fr/pub/FreeBSD/" },
  { "Germany",		"ftp.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.de.freebsd.org/pub/FreeBSD/" },
  { "Germany #2",	"ftp2.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.de.freebsd.org/pub/FreeBSD/" },
  { "Germany #3",	"ftp3.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.de.freebsd.org/pub/FreeBSD/" },
  { "Germany #4",	"ftp4.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp4.de.freebsd.org/pub/FreeBSD/" },
  { "Germany #5",	"ftp5.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp5.de.freebsd.org/pub/FreeBSD/" },
  { "Germany #6",	"ftp6.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp6.de.freebsd.org/pub/FreeBSD/" },
  { "Germany #7",	"ftp7.de.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp7.de.freebsd.org/pub/FreeBSD/" },
  { "Holland",	 	"ftp.nl.freebsd.ort", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.nl.freebsd.org/pub/os/FreeBSD/cdrom/" },
  { "Hong Kong",	"ftp.hk.super.net", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.hk.super.net/pub/FreeBSD/" },
  { "Ireland",		"ftp.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.ie.freebsd.org/pub/FreeBSD/" },
  { "Israel",		"orgchem.weizmann.ac.il", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://orgchem.weizmann.ac.il/pub/FreeBSD/" },
  { "Israel #2",	"xray4.weizmann.ac.il", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://xray4.weizmann.ac.il/pub/FreeBSD/" },
  { "Japan",		"ftp.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.jp.freebsd.org/pub/FreeBSD/" },
  { "Japan #2",		"ftp2.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.jp.freebsd.org/pub/FreeBSD/" },
  { "Japan #3",		"ftp3.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.jp.freebsd.org/pub/FreeBSD/" },
  { "Japan #4",		"ftp4.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp4.jp.freebsd.org/pub/FreeBSD/" },
  { "Japan #5",		"ftp5.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp5.jp.freebsd.org/pub/FreeBSD/" },
  { "Japan #6",		"ftp6.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp6.jp.freebsd.org/pub/FreeBSD/" },
  { "Korea",		"ftp.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.kr.freebsd.org/pub/FreeBSD/" },
  { "Korea #2",		"ftp2.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.kr.freebsd.org/pub/FreeBSD/" },
  { "Netherlands",	"ftp.nl.net", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.nl.net/pub/os/FreeBSD/" },
  { "Poland",		"SunSITE.icm.edu.pl", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://SunSITE.icm.edu.pl/pub/FreeBSD/" },
  { "Portugal",		"ftp.ua.pt", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.ua.pt/pub/misc/FreeBSD/" },
  { "Russia",		"ftp.kiae.su", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.kiae.su/FreeBSD/" },
  { "South Africa",	"ftp.za.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.za.freebsd.org/pub/FreeBSD/" },
  { "South Africa #2",	"ftp2.za.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.za.freebsd.org/pub/FreeBSD/" },
  { "South Africa #3",	"ftp3.za.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.za.freebsd.org/pub/FreeBSD/" },
  { "Sweden",		"ftp.luth.se", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.luth.se/pub/FreeBSD/" },
  { "Taiwan",		"ftp.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.tw.freebsd.org/pub/FreeBSD" },
  { "Taiwan #2",	"ftp2.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.tw.freebsd.org/pub/FreeBSD" },
  { "Taiwan #3",	"ftp3.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.tw.freebsd.org/pub/FreeBSD/" },
  { "Thailand",		"ftp.nectec.or.th", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.nectec.or.th/pub/mirrors/FreeBSD/" },
  { "UK",		"ftp.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.uk.freebsd.org/pub/BSD/FreeBSD/" },
  { "UK #2",		"ftp2.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.uk.freebsd.org/pub/BSD/FreeBSD/" },
  { "UK #3",		"ftp3.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.uk.freebsd.org/pub/BSD/FreeBSD/" },
  { "USA",		"ftp.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp.freebsd.org/pub/FreeBSD/" },
  { "USA #2",		"ftp2.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp2.freebsd.org/pub/FreeBSD/" },
  { "USA #3",		"ftp3.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp3.freebsd.org/pub/FreeBSD/" },
  { "USA #4",		"ftp4.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp4.freebsd.org/pub/FreeBSD/" },
  { "USA #5",		"ftp5.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp5.freebsd.org/pub/FreeBSD/" },
  { "USA #6",		"ftp6.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp6.freebsd.org/pub/FreeBSD/" },
  { "USA #7",		"ftp7.freebsd.org", NULL, dmenuSetVariable, NULL,
    VAR_FTP_PATH "=ftp://ftp7.freebsd.org/pub/FreeBSD/" },
  { NULL } }
};

DMenu MenuMediaTape = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a tape drive type",
    "FreeBSD can be installed from tape drive, though this installation\n\
method requires a certain amount of temporary storage in addition\n\
to the space required by the distribution itself (tape drives make\n\
poor random-access devices, so we extract _everything_ on the tape\n\
in one pass).  If you have sufficient space for this, then you should\n\
select one of the following tape devices detected on your system.",
    "Press F1 to read the installation guide",
    "install",
    { { NULL } },
};

DMenu MenuNetworkDevice = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Network interface information required",
    "If you are using PPP over a serial device as opposed\n"
    "to a direct ethernet connection, then you may first need to dial your\n"
    "service provider using the ppp utility we provide for that purpose.\n"
    "If you're using SLIP over a serial device then it's expected that you\n"
    "have a hardwired connection.\n\n"
    "You can also install over a parallel port using a special \"laplink\"\n"
    "cable to another machine running a fairly recent (2.0R or later) version\n"
    "of FreeBSD.",
    "Press F1 to read network configuration manual",
    "network_device",
    { { NULL } },
};

/* The media selection menu */
DMenu MenuMedia = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n\
media, ranging from floppies to an Internet FTP server.  If you're\n\
installing FreeBSD from a supported CDROM drive then this is generally\n\
the best media to use if you have no overriding reason for using other\n\
media.",
    "Press F1 for more information on the various media types",
    "media",
{ { "1 CDROM",		"Install from a FreeBSD CDROM",
    NULL, mediaSetCDROM },
  { "2 DOS",		"Install from a DOS partition",
    NULL, mediaSetDOS },
  { "3 File System",	"Install from an existing filesystem",
    NULL, mediaSetUFS },
  { "4 Floppy",		"Install from a floppy disk set",
    NULL, mediaSetFloppy },
  { "5 FTP",		"Install from an FTP server",
    NULL, mediaSetFTPActive },
  { "6 FTP Passive",	"Install from an FTP server through a firewall",
    NULL, mediaSetFTPPassive },
  { "7 NFS",		"Install over NFS",
    NULL, mediaSetNFS },
  { "8 Tape",		"Install from SCSI or QIC tape",
    NULL, mediaSetTape },
  { NULL } },
};

/* The distributions menu */
DMenu MenuDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Distributions",
    "As a convenience, we provide several \"canned\" distribution sets.\n\
These select what we consider to be the most reasonable defaults for the\n\
type of system in question.  If you would prefer to pick and choose the\n\
list of distributions yourself, simply select \"Custom\".  You can also\n\
pick a canned distribution set and then fine-tune it with the Custom item.\n\n\
When you are finished chose the Exit item or Cancel to abort.",
    "Press F1 for more information on these options.",
    "distributions",
{ { "1 Developer",	"Full sources, binaries and doc but no games [180MB]",
    checkDistDeveloper, distSetDeveloper },
  { "2 X-Developer",	"Same as above, but includes XFree86 [201MB]",
    checkDistXDeveloper, distSetXDeveloper },
  { "3 Kern-Developer",	"Full binaries and doc, kernel sources only [70MB]",
    checkDistKernDeveloper, distSetKernDeveloper },
  { "4 User",		"Average user - binaries and doc only [52MB]",
    checkDistUser, distSetUser },
  { "5 X-User",		"Same as above, but includes XFree86 [52MB]",
    checkDistXUser, distSetXUser },
  { "6 Minimal",	"The smallest configuration possible [44MB]",
    checkDistMinimum, distSetMinimum },
  { "7 All",		"All sources, binaries and XFree86 binaries [700MB]",
    checkDistEverything, distSetEverything },
  { "8 Custom",		"Specify your own distribution set [?]",
    NULL, dmenuSubmenu, NULL, &MenuSubDistributions, ' ', ' ', ' ' },
  { "9 Clear",		"Reset selected distribution list to nothing [0MB]",
    NULL, distReset, NULL, NULL, ' ', ' ', ' ' },
  { "0 Exit",		"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuSubDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the distributions you wish to install.",
    "Please check off the distributions you wish to install.  At the\n\
very minimum, this should be \"bin\".  WARNING:  Do not export the\n\
DES distribution out of the U.S.!  It is for U.S. customers only.",
    NULL,
    NULL,
{ { "bin",		"Binary base distribution (required) [36MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_BIN },
  { "commerce",		"Commercial and shareware demos [10MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMMERCIAL },
  { "compat1x",		"FreeBSD 1.x binary compatibility [2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT1X },
  { "compat20",		"FreeBSD 2.0 binary compatibility [2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT20 },
  { "compat21",		"FreeBSD 2.1 binary compatibility [2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT21 },
  { "DES",		"DES encryption code - NOT FOR EXPORT! [.3MB]",
    DESFlagCheck, distSetDES },
  { "dict",		"Spelling checker dictionary files [4.2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DICT },
  { "doc",		"FreeBSD Handbook and other online docs [10MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DOC },
  { "games",		"Games (non-commercial) [6.4MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_GAMES },
  { "info",		"GNU info files [4.1MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_INFO },
  { "man",		"System manual pages - recommended [3.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_MANPAGES },
  { "proflibs",		"Profiled versions of the libraries [3.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PROFLIBS },
  { "src",		"Sources for everything but DES [120MB]",
    srcFlagCheck, distSetSrc },
  { "XFree86",		"The XFree86 3.1.2-S distribution",
    x11FlagCheck, distSetXF86 },
  { "xperimnt",		"Experimental work in progress!",
    dmenuFlagCheck, dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_EXPERIMENTAL },
  { "All",		"All sources, binaries and XFree86 binaries [700MB]",
    NULL, distSetEverything, NULL, NULL, ' ', ' ', ' ' },
  { "Clear",		"Reset all of the above [0MB]",
    NULL, distReset, NULL, NULL, ' ', ' ', ' ' },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuDESDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the encryption facilities you wish to install.",
    "Please check off any special DES-based encryption distributions\n\
you would like to install.  Please note that these services are NOT FOR\n\
EXPORT from the United States, nor are they available on CDROM (for the\n\
same reason).  For information on non-U.S. FTP distributions of this\n\
software, please consult the release notes.",
    NULL,
    NULL,
{ { "des",	"Basic DES encryption services [1MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_DES, },
  { "krb",	"Kerberos encryption services [2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_KERBEROS },
  { "sebones",	"Sources for eBones (Kerberos) [1MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_SEBONES },
  { "ssecure",	"Sources for DES [1MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &DESDists, '[', 'X', ']', DIST_DES_SSECURE },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuSrcDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS ,
    "Select the sub-components of src you wish to install.",
    "Please check off those portions of the FreeBSD source tree\n\
you wish to install.",
    NULL,
    NULL,
{ { "base",	"top-level files in /usr/src [300K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BASE },
  { "gnu",	"/usr/src/gnu (software from the GNU Project) [42MB]",
    dmenuFlagCheck, dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GNU },
  { "etc",	"/usr/src/etc (miscellaneous system files) [460K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_ETC },
  { "games",	"/usr/src/games (the obvious!) [7.8MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GAMES },
  { "include",	"/usr/src/include (header files) [467K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_INCLUDE },
  { "lib",	"/usr/src/lib (system libraries) [9.2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIB },
  { "libexec",	"/usr/src/libexec (system programs) [1.2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIBEXEC },
  { "lkm",	"/usr/src/lkm (Loadable Kernel Modules) [193K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LKM	},
  { "release",	"/usr/src/release (release-generation tools) [533K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_RELEASE },
  { "bin",	"/usr/src/bin (system binaries) [2.5MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BIN },
  { "sbin",	"/usr/src/sbin (system binaries) [1.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SBIN },
  { "share",	"/usr/src/share (documents and shared files) [10MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SHARE },
  { "sys",	"/usr/src/sys (FreeBSD kernel) [13MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SYS },
  { "ubin",	"/usr/src/usr.bin (user binaries) [13MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_UBIN },
  { "usbin",	"/usr/src/usr.sbin (aux system binaries) [14MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_USBIN },
  { "smailcf",	"/usr/src/usr.sbin (sendmail config macros) [341K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SMAILCF },
  { "All",	"Select all of the above [120MB]",
    NULL, setSrc, NULL, NULL, ' ', ' ', ' ' },
  { "Clear",	"Reset all of the above [0MB]",
    NULL, clearSrc, NULL, NULL, ' ', ' ', ' ' },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuXF86Select = {
    DMENU_NORMAL_TYPE,
    "XFree86 3.1.2-S Distribution",
    "Please select the components you need from the XFree86 3.1.2-S\n\
distribution.  We recommend that you select what you need from the basic\n\
component set and at least one entry from the Server and Font set menus.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
{ { "Basic",	"Basic component menu (required)",
    NULL, dmenuSubmenu, NULL, &MenuXF86SelectCore },
  { "Server",	"X server menu",
    NULL, dmenuSubmenu, NULL, &MenuXF86SelectServer },
  { "Fonts",	"Font set menu",
    NULL, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
  { "All",	"Select the entire XFree86 distribution",
    NULL, setX11All },
  { "Clear",	"Reset XFree86 distribution list",
    NULL, clearX11All },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuXF86SelectCore = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "XFree86 3.1.2-S base distribution types",
    "Please check off the basic XFree86 components you wish to install.\n\
Bin, lib, xicf, and xdcf are recommended for a minimum installaion.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
{ { "bin",	"Client applications and shared libs [4.1MB].",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_BIN },
  { "lib",	"Data files needed at runtime [750K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LIB },
  { "xicf",	"Customizable xinit runtime configuration file [10K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_XINIT },
  { "xdcf",	"Customizable xdm runtime configuration file [20K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_XDMCF },
  { "etc",	"Clock setting and diagnostic source codes [70K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_ETC },
  { "doc",	"READMEs and release notes [600K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_DOC },
  { "man",	"Manual pages [1.7MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_MAN },
  { "ctrb",	"Various contributed binaries (ico, xman, etc) [550K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_CTRB },
  { "prog",	"Programmer's header and library files [4.1MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_PROG },
  { "link",	"Kit to reconfigure/rebuild X Servers [8.8MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LINK },
  { "ubin",	"rstart daemon [2K]",
    dmenuFlagCheck, dmenuSetFlag,	NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_UBIN },
  { "pex",	"PEX fonts and libs needed by PEX apps [290K]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_PEX },
  { "sources",	"XFree86 3.1.2-S standard + contrib sources [200MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_SRC },
  { "All",	"Select all of the above [20MB]",
    NULL, setX11Misc, NULL, NULL, ' ', ' ', ' ' },
  { "Clear",	"Reset all of the above [0MB]",
    NULL, clearX11Misc, NULL, NULL, ' ', ' ', ' ' },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuXF86SelectFonts = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS ,
    "Font distribution selection.",
    "Please check off the individual font distributions you wish to\n\
install.  At the minimum, you should install the standard\n\
75 DPI and misc fonts if you're also installing a server\n\
(these are selected by default).",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
{ { "fnts",	"Standard 75 DPI and miscellaneous fonts [3.6MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_MISC },
  { "f100",	"100 DPI fonts [1.8MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_100 },
  { "fcyr",	"Cyrillic Fonts [1.8MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_CYR },
  { "fscl",	"Speedo and Type scalable fonts [1.6MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_SCALE },
  { "non",	"Japanese, Chinese and other non-english fonts [3.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_NON },
  { "server",	"Font server [0.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_SERVER },
  { "All",	"All fonts [10MB]",
    NULL, setX11Fonts, NULL, NULL, ' ', ' ', ' ' },
  { "Clear",	"Reset font selections [0MB]",
    NULL, clearX11Fonts, NULL, NULL, ' ', ' ', ' ' },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuXF86SelectServer = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "X Server selection.",
    "Please check off the types of X servers you wish to install.\n\
If you are unsure as to which server will work for your graphics card,\n\
it is recommended that try the SVGA or VGA16 servers (the VGA16 and\n\
Mono servers are particularly well-suited to most LCD displays).",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
{ { "SVGA",	"Standard VGA or Super VGA display [2.8MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_SVGA },
  { "VGA16",	"Standard 16 color VGA display [1.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_VGA16 },
  { "Mono",	"Standard Monochrome display [1.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MONO },
  { "8514",	"8-bit (256 color) IBM 8514 or compatible card [2.2MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_8514 },
  { "AGX",	"8-bit AGX card [2.4MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_AGX },
  { "Ma8",	"8-bit ATI Mach8 card [2.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH8 },
  { "Ma32",	"8 and 16-bit (65K color) for ATI Mach32 card [2.4MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH32 },
  { "Ma64",	"8 and 16-bit (65K color) for ATI Mach64 card [2.5MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH64 },
  { "P9K",	"8, 16, and 24-bit color for Weitek P9000 based boards [2.5MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_P9000 },
  { "S3",	"8, 16 and 24-bit color for S3 based boards [2.7MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_S3 },
  { "W32",	"8-bit Color for ET4000/W32, /W32i and /W32p cards [2.3MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_W32 },
  { "nest",	"A nested server for testing purposes [1.8MB]",
    dmenuFlagCheck, dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_NEST },
  { "All",	"Select all of the above [25MB]",
    NULL, setX11Servers, NULL, NULL, ' ', ' ', ' ' },
  { "Clear",	"Reset all of the above [0MB]",
    NULL, clearX11Servers, NULL, NULL, ' ', ' ', ' ' },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuDiskDevices = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select Drive(s)",
    "Please select the drive, or drives, on which you wish to perform\n\
this operation.  If you are attempting to install a boot partition\n\
on a drive other than the first one or have multiple operating\n\
systems on your machine, you will have the option to install a boot\n\
manager later.  To select a drive, use the arrow keys to move to it\n\
and press [SPACE].  To de-select it, press [SPACE] again.\n\n\
Select OK or Cancel to leave this menu.",
    "Press F1 for important information regarding disk geometry!",
    "drives",
    { { NULL } },
};

DMenu MenuHTMLDoc = {
    DMENU_NORMAL_TYPE,
    "Select HTML Documentation pointer",
    "Please select the body of documentation you're interested in, the main\n\
ones right now being the FAQ and the Handbook.  You can also chose \"other\"\n\
to enter an arbitrary URL for browsing.",
    "Press F1 for more help on what you see here.",
    "html",
{ { "Handbook",	"The FreeBSD Handbook.",				NULL, docShowDocument },
  { "FAQ",	"The Frequently Asked Questions guide.",		NULL, docShowDocument },
  { "Home",	"The Home Pages for the FreeBSD Project (requires net)", NULL, docShowDocument },
  { "Other",	"Enter a URL.",						NULL, docShowDocument },
  { NULL } },
};

/* The main installation menu */
DMenu MenuInstallCustom = {
    DMENU_NORMAL_TYPE,
    "Choose Custom Installation Options",
    "This is the custom installation menu. You may use this menu to specify\n\
details on the type of distribution you wish to have, where you wish\n\
to install it from and how you wish to allocate disk storage to FreeBSD.",
    "Press F1 to read the installation guide",
    "install",
{ { "1 Options",	"Go to Options editor",			NULL, optionsEditor },
  { "2 Partition",	"Allocate disk space for FreeBSD",	NULL, diskPartitionEditor },
  { "3 Label",		"Label allocated disk partitions",	NULL, diskLabelEditor },
  { "4 Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
  { "5 Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
  { "6 Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCustomCommit },
  { "7 Extract",	"Just do distribution extract step",	NULL, distExtractAll },
  { "0 Exit",		"Exit this menu (returning to previous)", NULL, dmenuExit },
  { NULL } },
};

/* MBR type menu */
DMenu MenuMBRType = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "FreeBSD comes with a boot selector that allows you to easily\n"
    "select between FreeBSD and any other operating systems on your machine\n"
"at boot time.  If you have more than one drive and want to boot\n"
"from the second one, the boot selector will also make it possible\n"
"to do so (limitations in the PC BIOS usually prevent this otherwise).\n"
"If you do not want a boot selector, or wish to replace an existing\n"
"one, select \"standard\".  If you would prefer your Master Boot\n"
"Record to remain untouched then select \"None\".\n\n"
"  NOTE:  PC-DOS users will almost certainly require \"None\"!",
    "Press F1 to read the installation guide",
    "install",
{ { "BootMgr",	"Install the FreeBSD Boot Manager (\"Booteasy\")",
    dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr },
  { "Standard",	"Install a standard MBR (no boot manager)",
    dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
  { "None",	"Leave the Master Boot Record untouched",
    dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 2 },
  { NULL } },
};

/* Final configuration menu */
DMenu MenuConfigure = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Configuration Menu",	/* title */
    "If you've already installed FreeBSD, you may use this menu to customize\n\
it somewhat to suit your particular configuration.  Most importantly,\n\
you can use the Packages utility to load extra \"3rd party\"\n\
software not provided in the base distributions.",
    "Press F1 for more information on these options",
    "configure",
{ { "1 Add User",	"Add users to the system",
    NULL, dmenuSystemCommand, NULL, "adduser -config_create ; adduser -s" },
  { "2 Console",	"Customize system console behavior",
    NULL, dmenuSubmenu, NULL, &MenuSyscons },
  { "3 Time Zone",	"Set which time zone you're in",
    NULL, dmenuSystemCommand, NULL, "rm -f /etc/wall_cmos_clock /etc/localtime; tzsetup" },
  { "4 Media",		"Change the installation media type",
    NULL, dmenuSubmenu, NULL, &MenuMedia	},
  { "5 Mouse",		"Select the type of mouse you have",
    NULL, dmenuSubmenu, NULL, &MenuMouse, NULL },
  { "6 Networking",	"Configure additional network services",
    NULL, dmenuSubmenu, NULL, &MenuNetworking },
  { "7 Options",	"Go to options editor",
    NULL, optionsEditor },
  { "8 Packages",	"Install pre-packaged software for FreeBSD",
    NULL, configPackages },
  { "9 Ports",		"Link to FreeBSD Ports Collection on CD/NFS",
    NULL, configPorts },
  { "A Root Password",	"Set the system manager's password",
    NULL, dmenuSystemCommand, NULL, "passwd root" },
  { "B HTML Docs",	"Go to the HTML documentation menu (post-install)",
    NULL, docBrowser },
  { "C XFree86",	"Configure XFree86",
    NULL, configXFree86 },
  { "0 Exit",		"Exit this menu (returning to previous)",
    NULL, dmenuExit },
  { NULL } },
};

DMenu MenuNetworking = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Network Services Menu",
    "You may have already configured one network device (and the other\n\
various hostname/gateway/name server parameters) in the process\n\
of installing FreeBSD.  This menu allows you to configure other\n\
aspects of your system's network configuration.",
    NULL,
    NULL,
{ { "Interfaces",	"Configure additional network interfaces",
    NULL, tcpMenuSelect },
  { "NFS client",	"This machine will be an NFS client",
    dmenuVarCheck, dmenuToggleVariable, NULL, "nfs_client=YES" },
  { "NFS server",	"This machine will be an NFS server",
    dmenuVarCheck, configNFSServer, NULL, "nfs_server" },
  { "Gateway",		"This machine will route packets between interfaces",
    dmenuVarCheck, dmenuToggleVariable, NULL, "gateway=YES" },
  { "Gated",		"This machine wants to run gated instead of routed",
    dmenuVarCheck, configGated, NULL, "gated" },
  { "Novell",		"Install the Novell client/server demo package",
    dmenuVarCheck, configNovell, NULL, "novell" },
  { "Ntpdate",		"Select a clock-syncronization server",
    dmenuVarCheck, dmenuSubmenu, NULL, &MenuNTP, '[', 'X', ']', (int)"ntpdate" },
  { "Routed",		"Set flags for routed (default: -q)",
    dmenuVarCheck, configRoutedFlags, NULL, "routed" },
  { "Rwhod",		"This machine wants to run the rwho daemon",
    dmenuVarCheck, dmenuToggleVariable, NULL, "rwhod=YES" },
  { "Anon FTP",		"This machine wishes to allow anonymous FTP.",
    dmenuVarCheck, configAnonFTP, NULL, "anon_ftp" },
  { "WEB Server",	"This machine wishes to be a WWW server.",
    dmenuVarCheck, configApache, NULL, "apache_httpd" },
  { "Samba",		"Install Samba for LanManager (NETBUI) access.",
    dmenuVarCheck, configSamba, NULL, "samba" },
  { "PCNFSD",		"Run authentication server for clients with PC-NFS.",
    dmenuVarCheck, configPCNFSD, NULL, "pcnfsd" },
  { "Exit",	"Exit this menu (returning to previous)",
    checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
  { NULL } },
};

DMenu MenuNTP = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "NTPDATE Server Selection",
    "There are a number of time syncronization servers available\n\
for public use around the Internet.  Please select one reasonably\n\
close to you to have your system time syncronized accordingly.",
    "These are the primary open-access NTP servers",
    NULL,
{ { "Other",			"Select a site not on this list",
    NULL, configNTP },
  { "Australia",		"ntp.syd.dms.csiro.au (HP 5061 Cesium Beam)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=ntp.syd.dms.csiro.au" },
  { "Canada",			"tick.usask.ca (GOES clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=tick.usask.ca" },
  { "France",			"canon.inria.fr (TDF clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=canon.inria.fr" },
  { "Germany",			"ntps1-{0,1,2}.uni-erlangen.de (GPS)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=ntps1-0.uni-erlangen.de" },
  { "Germany #2",		"ntps1-0.cs.tu-berlin.de (GPS)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=ntps1-0.cs.tu-berlin.de" },
  { "Japan",			"clock.nc.fukuoka-u.ac.jp (GPS clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=clock.nc.fukuoka-u.ac.jp" },
  { "Japan #2",			"clock.tl.fukuoka-u.ac.jp (GPS clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=clock.tl.fukuoka-u.ac.jp" },
  { "Netherlands",		"ntp0.nl.net (GPS clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=ntp0.nl.net" },
  { "Norway",			"timer.unik.no (NTP clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=timer.unik.no" },
  { "Sweden",			"Time1.Stupi.SE (Cesium/GPS)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=Time1.Stupi.SE" },
  { "Switzerland",		"swisstime.ethz.ch (DCF77 clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=swisstime.ethz.ch" },
  { "U.S. East Coast",		"bitsy.mit.edu (WWV clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=bitsy.mit.edu" },
  { "U.S. East Coast #2",	"otc1.psu.edu (WWV clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=otc1.psu.edu" },
  { "U.S. West Coast",		"apple.com (WWV clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=apple.com" },
  { "U.S. West Coast #2",	"clepsydra.dec.com (GOES clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=clepsydra.dec.com" },
  { "U.S. West Coast #3",	"clock.llnl.gov (WWVB clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=clock.llnl.gov" },
  { "U.S. Midwest",		"ncar.ucar.edu (WWVB clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=ncar.ucar.edu" },
  { "U.S. Pacific",		"chantry.hawaii.net (WWV/H clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=chantry.hawaii.net" },
  { "U.S. Southwest",		"shorty.chpc.utexas.edu (WWV clock)",
    dmenuVarCheck, dmenuSetVariable, NULL, "ntpdate=shorty.chpc.utexas.edu" },
  { NULL } },
};

DMenu MenuSyscons = {
    DMENU_NORMAL_TYPE,
    "System Console Configuration",
    "The default system console driver for FreeBSD (syscons) has a\n\
number of configuration options which may be set according to\n\
your preference.\n\n\
When you are done setting configuration options, select Cancel.",
    "Configure your system console settings",
    NULL,
{ { "Keymap",	"Choose an alternate keyboard map",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
  { "Repeat",	"Set the rate at which keys repeat",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
  { "Saver",	"Configure the screen saver",		NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
  { "Exit",	"Exit this menu (returning to previous)", NULL, dmenuExit },
  { NULL } },
};

DMenu MenuSysconsKeymap = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keymap",
    "The default system console driver for FreeBSD (syscons) defaults\n\
to a standard \"American\" keyboard map.  Users in other countries\n\
(or with different keyboard preferences) may wish to choose one of\n\
the other keymaps below.",
    "Choose a keyboard map",
    NULL,
{ { "Danish CP865", "Danish Code Page 865 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=danish.cp865" },
  { "Danish ISO", "Danish ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=danish.iso" },
  { "French ISO", "French ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=fr.iso" },
  { "German CP850", "German Code Page 850 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=german.cp850"	},
  { "German ISO", "German ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=german.iso" },
  { "Italian", "Italian ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=it.iso" },
  { "Japanese 106", "Japanese 106 keymap",  dmenuVarCheck, dmenuSetVariable, NULL, "keymap=jp.106" },
  { "Russian CP866", "Russian Code Page 866 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=ru.cp866" },
  { "Russian KOI8", "Russian koi8 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=ru.koi8-r" },
  { "Russian s-KOI8", "Russian shifted koi8 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=ru.koi8-r.shift" },
  { "Swedish CP850", "Swedish Code Page 850 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=swedish.cp850" },
  { "Swedish ISO", "Swedish ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=swedish.iso" },
  { "U.K. CP850", "United Kingdom Code Page 850 keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=uk.cp850" },
  { "U.K. ISO", "United Kingdom ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=uk.iso" },
  { "U.S. ISO", "United States ISO keymap", dmenuVarCheck, dmenuSetVariable, NULL, "keymap=us.iso" },
  { NULL } },
};

DMenu MenuSysconsKeyrate = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keyboard Repeat Rate",
    "This menu allows you to set the speed at which keys repeat\n\
when held down.",
    "Choose a keyboard repeat rate",
    NULL,
{ { "Slow", "Slow keyboard repeat rate", dmenuVarCheck, dmenuSetVariable, NULL, "keyrate=slow" },
  { "Normal", "\"Normal\" keyboard repeat rate", dmenuVarCheck, dmenuSetVariable, NULL, "keyrate=normal" },
  { "Fast", "Fast keyboard repeat rate", dmenuVarCheck, dmenuSetVariable, NULL, "keyrate=fast" },
  { "Default", "Use default keyboard repeat rate", dmenuVarCheck, dmenuSetVariable, NULL, "keyrate=NO" },
  { NULL } },
};

DMenu MenuSysconsSaver = {
    DMENU_RADIO_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screen Saver",
    "By default, the console driver will not attempt to do anything\n\
special with your screen when it's idle.  If you expect to leave your\n\
monitor switched on and idle for long periods of time then you should\n\
probably enable one of these screen savers to prevent phosphor burn-in.",
    "Choose a nifty-looking screen saver",
    NULL,
{ { "blank",	"Simply blank the screen",
    dmenuVarCheck, dmenuSetVariable, NULL, "saver=blank" },
  { "Green",	"\"Green\" power saving mode (if supported by monitor)",
    dmenuVarCheck, dmenuSetVariable, NULL, "saver=green" },
  { "Snake",	"Draw a FreeBSD \"snake\" on your screen",
    dmenuVarCheck, dmenuSetVariable, NULL, "saver=snake" },
  { "Star",	"A \"twinkling stars\" effect",
    dmenuVarCheck, dmenuSetVariable, NULL, "saver=star" },
  { "Timeout",	"Set the screen saver timeout interval",
    dmenuVarCheck, configSaverTimeout, NULL, "blanktime", ' ', ' ', ' ' },
  { NULL } },
};
