/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include "sysinstall.h"

/* Miscellaneous work routines for menus */
static int
setSrc(dialogMenuItem *self)
{
    Dists |= DIST_SRC;
    SrcDists = DIST_SRC_ALL;
    CRYPTODists |= (DIST_CRYPTO_SCRYPTO | DIST_CRYPTO_SSECURE |
	DIST_CRYPTO_SKERBEROS4 | DIST_CRYPTO_SKERBEROS5);
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
clearSrc(dialogMenuItem *self)
{
    Dists &= ~DIST_SRC;
    SrcDists = 0;
    CRYPTODists &= ~(DIST_CRYPTO_SCRYPTO | DIST_CRYPTO_SSECURE |
	DIST_CRYPTO_SKERBEROS4 | DIST_CRYPTO_SKERBEROS5);
    return DITEM_SUCCESS | DITEM_REDRAW;
}

#ifndef X_AS_PKG
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
#endif /* !X_AS_PKG */

#define _IS_SET(dist, set) (((dist) & (set)) == (set))

#define IS_DEVELOPER(dist, extra) (_IS_SET(dist, _DIST_DEVELOPER | extra) || \
	_IS_SET(dist, _DIST_DEVELOPER | extra))

#define IS_USER(dist, extra) (_IS_SET(dist, _DIST_USER | extra) || \
	_IS_SET(dist, _DIST_USER | extra))

static int
checkDistDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, 0) && _IS_SET(SrcDists, DIST_SRC_ALL);
}

static int
checkDistXDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, DIST_XF86) && _IS_SET(SrcDists, DIST_SRC_ALL);
}

static int
checkDistKernDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, 0) && _IS_SET(SrcDists, DIST_SRC_SYS);
}

static int
checkDistXKernDeveloper(dialogMenuItem *self)
{
    return IS_DEVELOPER(Dists, DIST_XF86) && _IS_SET(SrcDists, DIST_SRC_SYS);
}

static int
checkDistUser(dialogMenuItem *self)
{
    return IS_USER(Dists, 0);
}

static int
checkDistXUser(dialogMenuItem *self)
{
    return IS_USER(Dists, DIST_XF86);
}

static int
checkDistMinimum(dialogMenuItem *self)
{
    return Dists == (DIST_BASE | DIST_CRYPTO);
}

static int
checkDistEverything(dialogMenuItem *self)
{
    return Dists == DIST_ALL && CRYPTODists == DIST_CRYPTO_ALL &&
	_IS_SET(SrcDists, DIST_SRC_ALL) &&
#ifndef X_AS_PKG
	_IS_SET(XF86Dists, DIST_XF86_ALL) &&
	_IS_SET(XF86ServerDists, DIST_XF86_SERVER_ALL) &&
	_IS_SET(XF86FontDists, DIST_XF86_FONTS_ALL);
#else
	1;
#endif
}

static int
srcFlagCheck(dialogMenuItem *item)
{
    return SrcDists;
}

static int
x11FlagCheck(dialogMenuItem *item)
{
    return Dists & DIST_XF86;
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
    "this program (sysinstall).  Invoke an option by pressing [SPACE] or\n"
    "[ENTER].  To exit, use [TAB] to move to the Cancel button.",
    "Use PageUp or PageDown to move through this menu faster!",
    NULL,
    { { " Anon FTP",		"Configure anonymous FTP logins.",	dmenuVarCheck, configAnonFTP, NULL, "anon_ftp" },
      { " Commit",		"Commit any pending actions (dangerous!)", NULL, installCustomCommit },
#ifdef WITH_SYSCONS
      { " Console settings",	"Customize system console behavior.",	NULL, dmenuSubmenu, NULL, &MenuSyscons },
#endif
      { " Configure",		"The system configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { " Defaults, Load",	"Load default settings.",		NULL, dispatch_load_floppy },
#ifdef WITH_MICE
      { " Device, Mouse",	"The mouse configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuMouse },
#endif
      { " Disklabel",		"The disk Label editor",		NULL, diskLabelEditor },
      { " Dists, All",		"Root of the distribution tree.",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { " Dists, Basic",		"Basic FreeBSD distribution menu.",	NULL, dmenuSubmenu, NULL, &MenuSubDistributions },
      { " Dists, Developer",	"Select developer's distribution.",	checkDistDeveloper, distSetDeveloper },
      { " Dists, Src",		"Src distribution menu.",		NULL, dmenuSubmenu, NULL, &MenuSrcDistributions },
      { " Dists, X Developer",	"Select X developer's distribution.",	checkDistXDeveloper, distSetXDeveloper },
      { " Dists, Kern Developer", "Select kernel developer's distribution.", checkDistKernDeveloper, distSetKernDeveloper },
      { " Dists, User",		"Select average user distribution.",	checkDistUser, distSetUser },
      { " Dists, X User",	"Select average X user distribution.",	checkDistXUser, distSetXUser },
      { " Distributions, Adding", "Installing additional distribution sets", NULL, distExtractAll },
#ifndef X_AS_PKG
      { " Distributions, XFree86","XFree86 distribution menu.",		NULL, distSetXF86 },
#endif
      { " Documentation",	"Installation instructions, README, etc.", NULL, dmenuSubmenu, NULL, &MenuDocumentation },
      { " Doc, README",		"The distribution README file.",	NULL, dmenuDisplayFile, NULL, "README" },
      { " Doc, Early Adopter's",		"Early Adopter's Guide to FreeBSD 5.0.",	NULL, dmenuDisplayFile, NULL, "EARLY" },
      { " Doc, Errata",		"The distribution errata.",	NULL, dmenuDisplayFile, NULL, "ERRATA" },
      { " Doc, Hardware",	"The distribution hardware guide.",	NULL, dmenuDisplayFile,	NULL, "HARDWARE" },
      { " Doc, Install",		"The distribution installation guide.",	NULL, dmenuDisplayFile,	NULL, "INSTALL" },
      { " Doc, Copyright",	"The distribution copyright notices.",	NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { " Doc, Release",		"The distribution release notes.",	NULL, dmenuDisplayFile, NULL, "RELNOTES" },
      { " Doc, HTML",		"The HTML documentation menu.",		NULL, docBrowser },
      { " Dump Vars",		"(debugging) dump out internal variables.", NULL, dump_variables },
      { " Emergency shell",	"Start an Emergency Holographic shell.",	NULL, installFixitHoloShell },
#ifdef WITH_SLICES
      { " Fdisk",		"The disk Partition Editor",		NULL, diskPartitionEditor },
#endif
      { " Fixit",		"Repair mode with CDROM or fixit floppy.",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { " FTP sites",		"The FTP mirror site listing.",		NULL, dmenuSubmenu, NULL, &MenuMediaFTP },
      { " Gateway",		"Set flag to route packets between interfaces.", dmenuVarCheck, dmenuToggleVariable, NULL, "gateway=YES" },
      { " HTML Docs",		"The HTML documentation menu",		NULL, docBrowser },
      { " inetd Configuration",	"Configure inetd and simple internet services.",	dmenuVarCheck, configInetd, NULL, "inetd_enable=YES" },
      { " Install, Standard",	"A standard system installation.",	NULL, installStandard },
      { " Install, Express",	"An express system installation.",	NULL, installExpress },
      { " Install, Custom",	"The custom installation menu",		NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { " Label",		"The disk Label editor",		NULL, diskLabelEditor },
      { " Media",		"Top level media selection menu.",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { " Media, Tape",		"Select tape installation media.",	NULL, mediaSetTape },
      { " Media, NFS",		"Select NFS installation media.",	NULL, mediaSetNFS },
      { " Media, Floppy",	"Select floppy installation media.",	NULL, mediaSetFloppy },
      { " Media, CDROM/DVD",	"Select CDROM/DVD installation media.",	NULL, mediaSetCDROM },
      { " Media, DOS",		"Select DOS installation media.",	NULL, mediaSetDOS },
      { " Media, UFS",		"Select UFS installation media.",	NULL, mediaSetUFS },
      { " Media, FTP",		"Select FTP installation media.",	NULL, mediaSetFTP },
      { " Media, FTP Passive",	"Select passive FTP installation media.", NULL, mediaSetFTPPassive },
      { " Media, HTTP",		"Select FTP via HTTP proxy installation media.", NULL, mediaSetHTTP },
      { " Network Interfaces",	"Configure network interfaces",		NULL, tcpMenuSelect },
      { " Networking Services",	"The network services menu.",		NULL, dmenuSubmenu, NULL, &MenuNetworking },
      { " NFS, client",		"Set NFS client flag.",			dmenuVarCheck, dmenuToggleVariable, NULL, "nfs_client_enable=YES" },
      { " NFS, server",		"Set NFS server flag.",			dmenuVarCheck, configNFSServer, NULL, "nfs_server_enable=YES" },
      { " NTP Menu",		"The NTP configuration menu.",		NULL, dmenuSubmenu, NULL, &MenuNTP },
      { " Options",		"The options editor.",			NULL, optionsEditor },
      { " Packages",		"The packages collection",		NULL, configPackages },
#ifdef WITH_SLICES
      { " Partition",		"The disk Slice (PC-style partition) Editor",	NULL, diskPartitionEditor },
#endif
      { " PCNFSD",		"Run authentication server for PC-NFS.", dmenuVarCheck, configPCNFSD, NULL, "pcnfsd" },
      { " Root Password",	"Set the system manager's password.",   NULL, dmenuSystemCommand, NULL, "passwd root" },
      { " Router",		"Select routing daemon (default: routed)", NULL, configRouter, NULL, "router_enable" },
      { " Security",		"Configure system security options", NULL, dmenuSubmenu, NULL, &MenuSecurity },
#ifdef WITH_SYSCONS
      { " Syscons",		"The system console configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSyscons },
#ifndef PC98
      { " Syscons, Font",	"The console screen font.",	  NULL, dmenuSubmenu, NULL, &MenuSysconsFont },
#endif
      { " Syscons, Keymap",	"The console keymap configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { " Syscons, Keyrate",	"The console key rate configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { " Syscons, Saver",	"The console screen saver configuration menu.",	NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
#ifndef PC98
      { " Syscons, Screenmap",	"The console screenmap configuration menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsScrnmap },
      { " Syscons, Ttys",       "The console terminal type menu.", NULL, dmenuSubmenu, NULL, &MenuSysconsTtys },
#endif
#endif /* WITH_SYSCONS */
      { " Time Zone",		"Set the system's time zone.",		NULL, dmenuSystemCommand, NULL, "tzsetup" },
      { " TTYs",		"Configure system ttys.",		NULL, configEtcTtys, NULL, "ttys" },
      { " Upgrade",		"Upgrade an existing system.",		NULL, installUpgrade },
      { " Usage",		"Quick start - How to use this menu system.",	NULL, dmenuDisplayFile, NULL, "usage" },
      { " User Management",	"Add user and group information.",	NULL, dmenuSubmenu, NULL, &MenuUsermgmt },
#ifndef X_AS_PKG
      { " XFree86, Fonts",	"XFree86 Font selection menu.",		NULL, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
      { " XFree86, Server",	"XFree86 Server selection menu.",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectServer },
#if defined(__i386__) && defined(PC98)
      { " XFree86, PC98 Server",	"XFree86 PC98 Server selection menu.",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectPC98Server },
#endif
#endif
      { NULL } },
};

/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "sysinstall Main Menu",				/* title */
    "Welcome to the FreeBSD installation and configuration tool.  Please\n" /* prompt */
    "select one of the options below by using the arrow keys or typing the\n"
    "first character of the option name you're interested in.  Invoke an\n"
    "option with [SPACE] or [ENTER].  To exit, use [TAB] to move to Exit.", 
    "Press F1 for Installation Guide",			/* help line */
    "INSTALL",						/* help file */
    { { "Select" },
      { "X Exit Install",	NULL, NULL, dmenuExit },
      { " Usage",	"Quick start - How to use this menu system",	NULL, dmenuDisplayFile, NULL, "usage" },
      { "Standard",	"Begin a standard installation (recommended)",	NULL, installStandard },
      { "Express",	"Begin a quick installation (for experts)", NULL, installExpress },
      { " Custom",	"Begin a custom installation (for experts)",	NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
      { "Configure",	"Do post-install configuration of FreeBSD",	NULL, dmenuSubmenu, NULL, &MenuConfigure },
      { "Doc",	"Installation instructions, README, etc.",	NULL, dmenuSubmenu, NULL, &MenuDocumentation },
#ifdef WITH_SYSCONS
      { "Keymap",	"Select keyboard type",				NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
#endif
      { "Options",	"View/Set various installation options",	NULL, optionsEditor },
      { "Fixit",	"Repair mode with CDROM/DVD/floppy or start shell",	NULL, dmenuSubmenu, NULL, &MenuFixit },
      { "Upgrade",	"Upgrade an existing system",			NULL, installUpgrade },
      { "Load Config","Load default install configuration",		NULL, dispatch_load_floppy },
      { "Index",	"Glossary of functions",			NULL, dmenuSubmenu, NULL, &MenuIndex },
      { NULL } },
};

/* The main documentation menu */
DMenu MenuDocumentation = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Documentation Menu",
    "If you are at all unsure about the configuration of your hardware\n"
    "or are looking to build a system specifically for FreeBSD, read the\n"
    "Hardware guide!  New users should also read the Install document for\n"
    "a step-by-step tutorial on installing FreeBSD.  For general information,\n"
    "consult the README file.",
    "Confused?  Press F1 for help.",
    "usage",
    { { "X Exit",	"Exit this menu (returning to previous)",	NULL, dmenuExit },
      { "1 README",	"A general description of FreeBSD.  Read this!", NULL, dmenuDisplayFile, NULL, "README" },
      { "2 Early Adopter's",	"Early Adopter's Guide to FreeBSD 5.0.", NULL, dmenuDisplayFile, NULL, "EARLY" },
      { "3 Errata",	"Late-breaking, post-release news.", NULL, dmenuDisplayFile, NULL, "ERRATA" },
      { "4 Hardware",	"The FreeBSD survival guide for PC hardware.",	NULL, dmenuDisplayFile,	NULL, "HARDWARE" },
      { "5 Install",	"A step-by-step guide to installing FreeBSD.",	NULL, dmenuDisplayFile,	NULL, "INSTALL" },
      { "6 Copyright",	"The FreeBSD Copyright notices.",		NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
      { "7 Release"	,"The release notes for this version of FreeBSD.", NULL, dmenuDisplayFile, NULL, "RELNOTES" },
      { "8 Shortcuts",	"Creating shortcuts to sysinstall.",		NULL, dmenuDisplayFile, NULL, "shortcuts" },
      { "9 HTML Docs",	"Go to the HTML documentation menu (post-install).", NULL, docBrowser },
      { NULL } },
};

#ifdef WITH_MICE
DMenu MenuMouseType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
#ifdef PC98
    "Select a protocol type for your mouse",
    "If your mouse is attached to the bus mouse port, you should always choose\n"
    "\"Auto\", regardless of the model and the brand of the mouse.  All other\n"
    "protocol types are for serial mice and should not be used with the bus\n"
    "mouse.  If you have a serial mouse and are not sure about its protocol,\n"
    "you should also try \"Auto\".  It may not work for the serial mouse if the\n"
    "mouse does not support the PnP standard.  But, it won't hurt.  Many\n"
    "2-button serial mice are compatible with \"Microsoft\" or \"MouseMan\".\n"
    "3-button serial mice may be compatible with \"MouseSystems\" or \"MouseMan\".\n"
    "If the serial mouse has a wheel, it may be compatible with \"IntelliMouse\".",
    NULL,
    NULL,
    { { "1 Auto",	"Bus mouse or PnP serial mouse",	
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=auto" },
#else
    "Select a protocol type for your mouse",
    "If your mouse is attached to the PS/2 mouse port or the bus mouse port,\n"
    "you should always choose \"Auto\", regardless of the model and the brand\n"
    "of the mouse.  All other protocol types are for serial mice and should\n"
    "not be used with the PS/2 port mouse or the bus mouse.  If you have\n"
    "a serial mouse and are not sure about its protocol, you should also try\n"
    "\"Auto\".  It may not work for the serial mouse if the mouse does not\n"
    "support the PnP standard.  But, it won't hurt.  Many 2-button serial mice\n"
    "are compatible with \"Microsoft\" or \"MouseMan\".  3-button serial mice\n"
    "may be compatible with \"MouseSystems\" or \"MouseMan\".  If the serial\n"
    "mouse has a wheel, it may be compatible with \"IntelliMouse\".",
    NULL,
    NULL,
    { { "1 Auto",	"Bus mouse, PS/2 style mouse or PnP serial mouse",	
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=auto" },
#endif /* PC98 */
      { "2 GlidePoint",	"ALPS GlidePoint pad (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=glidepoint" },
      { "3 Hitachi","Hitachi tablet (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mmhittab" },
      { "4 IntelliMouse",	"Microsoft IntelliMouse (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=intellimouse" },
      { "5 Logitech",	"Logitech protocol (old models) (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=logitech" },
      { "6 Microsoft",	"Microsoft protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=microsoft" },
      { "7 MM Series","MM Series protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mmseries" },
      { "8 MouseMan",	"Logitech MouseMan/TrackMan models (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mouseman" },
      { "9 MouseSystems",	"MouseSystems protocol (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=mousesystems" },
      { "A ThinkingMouse","Kensington ThinkingMouse (serial)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_TYPE "=thinkingmouse" },
      { NULL } },
};

#ifdef PC98
DMenu MenuMousePort = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Select your mouse port from the following menu",
    "The built-in pointing device of laptop/notebook computers is usually\n"
    "a BusMouse style device.",
    NULL,
    NULL,
    {
      { "1 BusMouse",	"PC-98x1 bus mouse (/dev/mse0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/mse0" },
      { "2 COM1",	"Serial mouse on COM1 (/dev/cuaa0)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa0" },
      { "3 COM2",	"Serial mouse on COM2 (/dev/cuaa1)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa1" },
      { NULL } },
};
#else
DMenu MenuMousePort = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Select your mouse port from the following menu",
    "The built-in pointing device of laptop/notebook computers is usually\n"
    "a PS/2 style device.",
    NULL,
    NULL,
    { { "1 PS/2",	"PS/2 style mouse (/dev/psm0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/psm0" },
      { "2 COM1",	"Serial mouse on COM1 (/dev/cuaa0)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa0" },
      { "3 COM2",	"Serial mouse on COM2 (/dev/cuaa1)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa1" },
      { "4 COM3",	"Serial mouse on COM3 (/dev/cuaa2)",
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa2" },
      { "5 COM4",	"Serial mouse on COM4 (/dev/cuaa3)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/cuaa3" },
      { "6 BusMouse",	"Logitech, ATI or MS bus mouse (/dev/mse0)", 
	dmenuVarCheck, dmenuSetVariable, NULL, VAR_MOUSED_PORT "=/dev/mse0" },
      { NULL } },
};
#endif /* PC98 */

DMenu MenuMouse = {
    DMENU_NORMAL_TYPE,
    "Please configure your mouse",
    "You can cut and paste text in the text console by running the mouse\n"
    "daemon.  Specify a port and a protocol type of your mouse and enable\n"
    "the mouse daemon.  If you don't want this feature, select 6 to disable\n"
    "the daemon.\n"
    "Once you've enabled the mouse daemon, you can specify \"/dev/sysmouse\"\n"
    "as your mouse device and \"SysMouse\" or \"MouseSystems\" as mouse\n"
    "protocol when running the X configuration utility (see Configuration\n"
    "menu).",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)", NULL, dmenuExit },
      { "2 Enable",	"Test and run the mouse daemon", NULL, mousedTest, NULL, NULL },
      { "3 Type",	"Select mouse protocol type", NULL, dmenuSubmenu, NULL, &MenuMouseType },
      { "4 Port",	"Select mouse port", NULL, dmenuSubmenu, NULL, &MenuMousePort },
      { "5 Flags",      "Set additional flags", dmenuVarCheck, setMouseFlags,
	NULL, VAR_MOUSED_FLAGS "=" },
      { "6 Disable",	"Disable the mouse daemon", NULL, mousedDisable, NULL, NULL },
      { NULL } },
};
#endif /* WITH_MICE */

DMenu MenuMediaCDROM = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a CD/DVD type",
    "FreeBSD can be installed directly from a CD/DVD containing a valid\n"
    "FreeBSD distribution.  If you are seeing this menu it is because\n"
    "more than one CD/DVD drive was found on your system.  Please select one\n"
    "of the following CD/DVD drives as your installation drive.",
    "Press F1 to read the installation guide",
    "INSTALL",
    { { NULL } },
};

DMenu MenuMediaFloppy = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a Floppy drive",
    "You have more than one floppy drive.  Please choose which drive\n"
    "you would like to use.",
    NULL,
    NULL,
    { { NULL } },
};

DMenu MenuMediaDOS = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a DOS partition",
    "FreeBSD can be installed directly from a DOS partition\n"
    "assuming, of course, that you have copied the relevant\n"
    "distributions into your DOS partition before starting this\n"
    "installation.  If this is not the case then you should reboot\n"
    "DOS at this time and copy the distributions you wish to install\n"
    "into a \"FREEBSD\" subdirectory on one of your DOS partitions.\n"
    "Otherwise, please select the DOS partition containing the FreeBSD\n"
    "distribution files.",
    "Press F1 to read the installation guide",
    "INSTALL",
    { { NULL } },
};

DMenu MenuMediaFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select a FreeBSD FTP distribution site",
    "Please select the site closest to you or \"other\" if you'd like to\n"
    "specify a different choice.  Also note that not every site listed here\n"
    "carries more than the base distribution kits. Only the Primary site is\n"
    "guaranteed to carry the full range of possible distributions.",
    "Select a site that's close!",
    "INSTALL",
    { { "Primary Site",	"ftp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.freebsd.org" },
      { "URL", "Specify some other ftp site by URL", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=other" },
      { " 5.0 SNAP Server", "current.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://current.freebsd.org" },
      { " 4.x SNAP Server", "releng4.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://releng4.freebsd.org" },
      { " jp.FreeBSD.org SNAP Server", "snapshots.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://snapshots.jp.freebsd.org" },
      { " IPv6 Ready", "ftp2.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.jp.freebsd.org" },
      { " IPv6 Ready #2", "ftp7.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.jp.freebsd.org" },
      { "Argentina",	"ftp.ar.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ar.freebsd.org" },
      { "Australia",	"ftp.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.au.freebsd.org" },
      { " Australia #2","ftp2.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.au.freebsd.org" },
      { " Australia #3","ftp3.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.au.freebsd.org" },
      { " Australia #4","ftp4.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.au.freebsd.org" },
      { " Australia #5","ftp5.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.au.freebsd.org" },
      { " Australia #6","ftp6.au.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.au.freebsd.org" },
      { "Austria",	"ftp.at.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.at.freebsd.org" },
      { " Austria #2",	"ftp2.at.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.at.freebsd.org" },
      { "Brazil",	"ftp.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.br.freebsd.org" },
      { " Brazil #2",	"ftp2.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.br.freebsd.org" },
      { " Brazil #3",	"ftp3.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.br.freebsd.org" },
      { " Brazil #4",	"ftp4.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.br.freebsd.org" },
      { " Brazil #5",	"ftp5.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.br.freebsd.org" },
      { " Brazil #6",	"ftp6.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.br.freebsd.org" },
      { " Brazil #7",	"ftp7.br.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.br.freebsd.org" },
      /*
      { "Bulgaria",	"ftp.bg.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.bg.freebsd.org" },
      */
      { "Canada",	"ftp.ca.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ca.freebsd.org" },
      /*
      { " Canada #2",	"ftp2.ca.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ca.freebsd.org" },
      */
      { "China",	"ftp.cn.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.cn.freebsd.org" },
      { " China #2",	"ftp2.cn.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.cn.freebsd.org" },
      { " China #3",	"ftp3.cn.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.cn.freebsd.org" },
      { "Czech Republic", "ftp.cz.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH	"=ftp://ftp.cz.freebsd.org" },
      { "Denmark (Primary)",	"ftp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.dk.freebsd.org" },
      { " Denmark",	"ftp.dk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.dk.freebsd.org" },
      { " Denmark #2",	"ftp2.dk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.dk.freebsd.org" },
      { " Denmark #3",	"ftp3.dk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.dk.freebsd.org" },
      { "Estonia",	"ftp.ee.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ee.freebsd.org" },
      { "Finland",	"ftp.fi.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.fi.freebsd.org" },
      { " Finland #3",	"ftp3.fi.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.fi.freebsd.org" },
      { "France",	"ftp.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.fr.freebsd.org" },
      { " France #2",	"ftp2.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.fr.freebsd.org" },
      { " France #3",	"ftp3.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.fr.freebsd.org" },
      /*
      { " France #4",	"ftp4.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.fr.freebsd.org" },
      */
      { " France #5",	"ftp5.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.fr.freebsd.org" },
      { " France #6",	"ftp6.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.fr.freebsd.org" },
      /*
      { " France #7",	"ftp7.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.fr.freebsd.org" },
      */
      { " France #8",	"ftp8.fr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.fr.freebsd.org" },
      { "Germany",	"ftp.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.de.freebsd.org" },
      { " Germany #2",	"ftp2.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.de.freebsd.org" },
      { " Germany #3",	"ftp3.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.de.freebsd.org" },
      { " Germany #4",	"ftp4.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.de.freebsd.org" },
      { " Germany #5",	"ftp5.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.de.freebsd.org" },
      { " Germany #6",	"ftp6.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.de.freebsd.org" },
      { " Germany #7",	"ftp7.de.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.de.freebsd.org" },
      { "Greece",	"ftp.gr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.gr.freebsd.org" },
      { " Greece #2",	"ftp2.gr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.gr.freebsd.org" },
      { "Hong Kong",	"ftp.hk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.hk.freebsd.org" },
      { "Hungary",	"ftp.hu.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.hu.freebsd.org" },
      /*
      { " Hungary #2",	"ftp2.hu.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.hu.freebsd.org" },
      */
      { "Iceland",	"ftp.is.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.is.freebsd.org" },
      { "Ireland",	"ftp.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ie.freebsd.org" },
      { " Ireland #2",	"ftp2.ie.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ie.freebsd.org" },
      { "Israel",	"ftp.il.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.il.freebsd.org" },
      { " Israel #2",	"ftp2.il.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.il.freebsd.org" },
      { "Italy",	"ftp.it.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.it.freebsd.org" },
      { " Italy #2",	"ftp2.it.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.it.freebsd.org" },
      { "Japan",	"ftp.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.jp.freebsd.org" },
      { " Japan #2",	"ftp2.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.jp.freebsd.org" },
      { " Japan #3",	"ftp3.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.jp.freebsd.org" },
      { " Japan #4",	"ftp4.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.jp.freebsd.org" },
      { " Japan #5",	"ftp5.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.jp.freebsd.org" },
      { " Japan #6",	"ftp6.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.jp.freebsd.org" },
      { " Japan #7",	"ftp7.jp.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.jp.freebsd.org" },
      { "Korea",	"ftp.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.kr.freebsd.org" },
      { " Korea #2",	"ftp2.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.kr.freebsd.org" },
      { " Korea #3",	"ftp3.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.kr.freebsd.org" },
      { " Korea #4",	"ftp4.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.kr.freebsd.org" },
      { " Korea #5",	"ftp5.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.kr.freebsd.org" },
      { " Korea #6",	"ftp6.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.kr.freebsd.org" },
      { " Korea #7",	"ftp7.kr.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.kr.freebsd.org" },
      { "Lithuania",	"ftp.lt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.lt.freebsd.org" },
      { "Netherlands",	"ftp.nl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.nl.freebsd.org" },
      { " Netherlands #2",	"ftp2.nl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.nl.freebsd.org" },
      { "New Zealand",	"ftp.nz.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.nz.freebsd.org" },
      { "Norway",	"ftp.no.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.no.freebsd.org" },
      { " Norway #3",	"ftp3.no.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.no.freebsd.org" },
      { "Poland",	"ftp.pl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.pl.freebsd.org" },
      { " Poland #2",	"ftp2.pl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.pl.freebsd.org" },
      { " Poland #3",	"ftp3.pl.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.pl.freebsd.org" },
      { "Portugal (N/A)",	"ftp.pt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.pt.freebsd.org" },
      { " Portugal #2",	"ftp2.pt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.pt.freebsd.org" },
      { " Portugal #3",	"ftp3.pt.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.pt.freebsd.org" },
      { "Romania",	"ftp.ro.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ro.freebsd.org" },
      { "Russia",	"ftp.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ru.freebsd.org" },
      { " Russia #2",	"ftp2.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ru.freebsd.org" },
      { " Russia #3",	"ftp3.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.ru.freebsd.org" },
      { " Russia #4",    "ftp4.ru.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.ru.freebsd.org" },
      { "Saudi Arabia",	"ftp.isu.net.sa", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.isu.net.sa/pub/mirrors/ftp.freebsd.org/" },
      { "Singapore",	"ftp.sg.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.sg.freebsd.org" },
      { "Slovak Republic",	"ftp.sk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.sk.freebsd.org" },
      { " Slovak Republic #2",	"ftp2.sk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.sk.freebsd.org" },
      { "Slovenia (N/A)",	"ftp.si.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.si.freebsd.org" },
      { " Slovenia #2",	"ftp2.si.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.si.freebsd.org" },
      { "South Africa",	"ftp.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.za.freebsd.org" },
      { " South Africa #2", "ftp2.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.za.freebsd.org" },
      { " South Africa #3", "ftp3.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.za.freebsd.org" },
      { " South Africa #4", "ftp4.za.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.za.freebsd.org" },
      { "Spain",	"ftp.es.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.es.freebsd.org" },
      { " Spain #2",	"ftp2.es.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.es.freebsd.org" },
      { " Spain #3",	"ftp3.es.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.es.freebsd.org" },
      { "Sweden",	"ftp.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.se.freebsd.org" },
      { " Sweden #2",	"ftp2.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.se.freebsd.org" },
      { " Sweden #3",	"ftp3.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.se.freebsd.org" },
      /*
      { " Sweden #4",	"ftp4.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.se.freebsd.org" },
      */
      { " Sweden #5",	"ftp5.se.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.se.freebsd.org" },
      { "Switzerland",	"ftp.ch.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ch.freebsd.org" },
      { "Taiwan",	"ftp.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.tw.freebsd.org" },
      { " Taiwan #2",	"ftp2.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.tw.freebsd.org" },
      { " Taiwan #3",	"ftp3.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.tw.freebsd.org" },
      { " Taiwan #4",   "ftp4.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.tw.freebsd.org" },
      { " Taiwan #5",   "ftp5.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.tw.freebsd.org" },
      { " Taiwan #6",   "ftp6.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.tw.freebsd.org" },
      { " Taiwan #7",   "ftp7.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.tw.freebsd.org" },
      { " Taiwan #8",   "ftp8.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.tw.freebsd.org" },
      { " Taiwan #9",   "ftp9.tw.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp9.tw.freebsd.org" },
      { "Thailand",	"ftp.nectec.or.th", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.nectec.or.th/pub/mirrors/FreeBSD/" },
      { "UK",		"ftp.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.uk.freebsd.org" },
      { " UK #2",	"ftp2.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.uk.freebsd.org" },
      { " UK #3",	"ftp3.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.uk.freebsd.org" },
      { " UK #4",	"ftp4.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.uk.freebsd.org" },
      { " UK #5",	"ftp5.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.uk.freebsd.org" },
      { " UK #6",	"ftp6.uk.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.uk.freebsd.org" },
      { "Ukraine",	"ftp.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp.ua.freebsd.org" },
      { " Ukraine #2",	"ftp2.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.ua.freebsd.org" },
      { " Ukraine #3",	"ftp3.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.ua.freebsd.org" },
      /*
      { " Ukraine #4",	"ftp4.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.ua.freebsd.org" },
      */
      { " Ukraine #5",	"ftp5.ua.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.ua.freebsd.org" },
      { " USA #2",	"ftp2.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp2.freebsd.org" },
      { " USA #3",	"ftp3.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp3.freebsd.org" },
      { " USA #4",	"ftp4.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp4.freebsd.org" },
      { " USA #5",	"ftp5.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp5.freebsd.org" },
      { " USA #6",	"ftp6.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp6.freebsd.org" },
      { " USA #7",	"ftp7.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp7.freebsd.org" },
      { " USA #8",	"ftp8.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp8.freebsd.org" },
      { " USA #9",	"ftp9.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp9.freebsd.org" },
      { " USA #10",	"ftp10.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp10.freebsd.org" },
      { " USA #11",	"ftp11.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp11.freebsd.org" },
      { " USA #12",	"ftp12.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp12.freebsd.org" },
      { " USA #13",	"ftp13.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp13.freebsd.org" },
      { " USA #14",	"ftp14.freebsd.org", NULL, dmenuSetVariable, NULL,
	VAR_FTP_PATH "=ftp://ftp14.freebsd.org" },
      { NULL } }
};

DMenu MenuMediaTape = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a tape drive type",
    "FreeBSD can be installed from tape drive, though this installation\n"
    "method requires a certain amount of temporary storage in addition\n"
    "to the space required by the distribution itself (tape drives make\n"
    "poor random-access devices, so we extract _everything_ on the tape\n"
    "in one pass).  If you have sufficient space for this, then you should\n"
    "select one of the following tape devices detected on your system.",
    "Press F1 to read the installation guide",
    "INSTALL",
    { { NULL } },
};

DMenu MenuNetworkDevice = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Network interface information required",
    "If you are using PPP over a serial device, as opposed to a direct\n"
    "ethernet connection, then you may first need to dial your Internet\n"
    "Service Provider using the ppp utility we provide for that purpose.\n"
    "If you're using SLIP over a serial device then the expectation is\n"
    "that you have a HARDWIRED connection.\n\n"
    "You can also install over a parallel port using a special \"laplink\"\n"
    "cable to another machine running a fairly recent (2.0R or later)\n"
    "version of FreeBSD.",
    "Press F1 to read network configuration manual",
    "network_device",
    { { NULL } },
};

/* Prototype KLD load menu */
DMenu MenuKLD = {
    DMENU_NORMAL_TYPE,
    "KLD Menu",
    "Load a KLD from a floppy\n",
    NULL,
    NULL,
    { { NULL } },
};

/* The media selection menu */
DMenu MenuMedia = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n"
    "media, ranging from floppies to an Internet FTP server.  If you're\n"
    "installing FreeBSD from a supported CD/DVD drive then this is generally\n"
    "the best media to use if you have no overriding reason for using other\n"
    "media.",
    "Press F1 for more information on the various media types",
    "media",
    { { "1 CD/DVD",		"Install from a FreeBSD CD/DVD",	NULL, mediaSetCDROM },
      { "2 FTP",		"Install from an FTP server",		NULL, mediaSetFTPActive },
      { "3 FTP Passive",	"Install from an FTP server through a firewall", NULL, mediaSetFTPPassive },
      { "4 HTTP",		"Install from an FTP server through a http proxy", NULL, mediaSetHTTP },
      { "5 DOS",		"Install from a DOS partition",		NULL, mediaSetDOS },
      { "6 NFS",		"Install over NFS",			NULL, mediaSetNFS },
      { "7 File System",	"Install from an existing filesystem",	NULL, mediaSetUFS },
      { "8 Floppy",		"Install from a floppy disk set",	NULL, mediaSetFloppy },
      { "9 Tape",		"Install from SCSI or QIC tape",	NULL, mediaSetTape },
      { "X Options",		"Go to the Options screen",		NULL, optionsEditor },
      { NULL } },
};

/* The distributions menu */
DMenu MenuDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Distributions",
    "As a convenience, we provide several \"canned\" distribution sets.\n"
    "These select what we consider to be the most reasonable defaults for the\n"
    "type of system in question.  If you would prefer to pick and choose the\n"
    "list of distributions yourself, simply select \"Custom\".  You can also\n"
    "pick a canned distribution set and then fine-tune it with the Custom item.\n\n"
    "Choose an item by pressing [SPACE] or [ENTER].  When finished, choose the\n"
    "Exit item or move to the OK button with [TAB].",
    "Press F1 for more information on these options.",
    "distributions",
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",			"All system sources, binaries and X Window System)",
	checkDistEverything,	distSetEverything, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",		"Reset selected distribution list to nothing",
	NULL,			distReset, NULL, NULL, ' ', ' ', ' ' },
      { "4 Developer",		"Full sources, binaries and doc but no games", 
	checkDistDeveloper,	distSetDeveloper },
      { "5 X-Developer",	"Same as above + X Window System",
	checkDistXDeveloper,	distSetXDeveloper },
      { "6 Kern-Developer",	"Full binaries and doc, kernel sources only",
	checkDistKernDeveloper, distSetKernDeveloper },
      { "7 X-Kern-Developer",	"Same as above + X Window System",
	checkDistXKernDeveloper, distSetXKernDeveloper },
      { "8 User",		"Average user - binaries and doc only",
	checkDistUser,		distSetUser },
      { "9 X-User",		"Same as above + X Window System",
	checkDistXUser,		distSetXUser },
      { "A Minimal",		"The smallest configuration possible",
	checkDistMinimum,	distSetMinimum },
      { "B Custom",		"Specify your own distribution set",
	NULL,			dmenuSubmenu, NULL, &MenuSubDistributions, '>', '>', '>' },
      { NULL } },
};

DMenu MenuSubDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the distributions you wish to install.",
    "Please check off the distributions you wish to install.  At the\n"
    "very minimum, this should be \"base\".",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"All system sources, binaries and X Window System",
	NULL, distSetEverything, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all of the below",
	NULL, distReset, NULL, NULL, ' ', ' ', ' ' },
      { " base",	"Binary base distribution (required)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_BASE },
#ifdef __i386__
      { " compat1x",	"FreeBSD 1.x binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT1X },
      { " compat20",	"FreeBSD 2.0 binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT20 },
      { " compat21",	"FreeBSD 2.1 binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT21 },
      { " compat22",	"FreeBSD 2.2.x and 3.0 a.out binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT22 },
      { " compat3x",	"FreeBSD 3.x binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT3X },
#endif
#if __FreeBSD__ >= 4 && (defined(__i386__) || defined(__alpha__))
      { " compat4x",	"FreeBSD 4.x binary compatibility",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_COMPAT4X },
#endif
      { " crypto",	"Basic encryption services",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_CRYPTO, },
      { " krb4",	"KerberosIV authentication services",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_KERBEROS4 },
      { " krb5",	"Kerberos5 authentication services",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_KERBEROS5 },
      { " dict",	"Spelling checker dictionary files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DICT },
      { " doc",		"Miscellaneous FreeBSD online docs",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_DOC },
      { " games",	"Games (non-commercial)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_GAMES },
      { " info",	"GNU info files",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_INFO },
      { " man",		"System manual pages - recommended",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_MANPAGES },
      { " catman",	"Preformatted system manual pages",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_CATPAGES },
      { " proflibs",	"Profiled versions of the libraries",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PROFLIBS },
      { " src",		"Sources for everything",
	srcFlagCheck,	distSetSrc },
      { " ports",	"The FreeBSD Ports collection",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_PORTS },
      { " local",	"Local additions collection",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_LOCAL},
      { " XFree86",	"The XFree86 distribution",
#ifdef X_AS_PKG
	dmenuFlagCheck,	dmenuSetFlag, NULL, &Dists, '[', 'X', ']', DIST_XF86 },
#else
	x11FlagCheck,	distSetXF86 },
#endif
      { NULL } },
};

DMenu MenuSrcDistributions = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select the sub-components of src you wish to install.",
    "Please check off those portions of the FreeBSD source tree\n"
    "you wish to install.",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"Select all of the below",
	NULL,		setSrc, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all of the below",
	NULL,		clearSrc, NULL, NULL, ' ', ' ', ' ' },
      { " base",	"top-level files in /usr/src",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BASE },
      { " contrib",	"/usr/src/contrib (contributed software)",
	dmenuFlagCheck,	dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_CONTRIB },
      { " gnu",		"/usr/src/gnu (software from the GNU Project)",
	dmenuFlagCheck,	dmenuSetFlag,	NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GNU },
      { " etc",		"/usr/src/etc (miscellaneous system files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_ETC },
      { " games",	"/usr/src/games (the obvious!)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_GAMES },
      { " include",	"/usr/src/include (header files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_INCLUDE },
      { " lib",		"/usr/src/lib (system libraries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIB },
      { " libexec",	"/usr/src/libexec (system programs)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_LIBEXEC },
      { " release",	"/usr/src/release (release-generation tools)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_RELEASE },
      { " bin",		"/usr/src/bin (system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_BIN },
      { " sbin",	"/usr/src/sbin (system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SBIN },
      { " scrypto",	"/usr/src/crypto (contrib encryption sources)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_SCRYPTO },
      { " share",	"/usr/src/share (documents and shared files)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SHARE },
      { " skrb4",	"/usr/src/kerberosIV (sources for KerberosIV)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_SKERBEROS4 },
      { " skrb5",	"/usr/src/kerberos5 (sources for Kerberos5)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_SKERBEROS5 },
      { " ssecure",	"/usr/src/secure (BSD encryption sources)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &CRYPTODists, '[', 'X', ']', DIST_CRYPTO_SSECURE },
      { " sys",		"/usr/src/sys (FreeBSD kernel)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_SYS },
      { " tools",	"/usr/src/tools (miscellaneous tools)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_TOOLS },
      { " ubin",	"/usr/src/usr.bin (user binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_UBIN },
      { " usbin",	"/usr/src/usr.sbin (aux system binaries)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &SrcDists, '[', 'X', ']', DIST_SRC_USBIN },
      { NULL } },
};

DMenu MenuXF86Config = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select the XFree86 configuration tool you want to use.",
    "The first option, xf86cfg, is fully graphical.\n"
    "The second option provides a menu-based interface similar to\n"
    "what you are currently using. "
    "The third option, xf86config, is\n"
    "a more simplistic shell-script based tool and less friendly to\n"
    "new users, but it may work in situations where the other options\n"
    "do not.",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	NULL, dmenuExit },
      { "2 xf86cfg",	"Fully graphical XFree86 configuration tool.",
	NULL, dmenuSetVariable, NULL, VAR_XF86_CONFIG "=xf86cfg" },
      { "3 xf86cfg -textmode",	"ncurses-based XFree86 configuration tool.",
	NULL, dmenuSetVariable, NULL, VAR_XF86_CONFIG "=xf86cfg -textmode" },
      { "4 xf86config",	"Shell-script based XFree86 configuration tool.",
	NULL, dmenuSetVariable, NULL, VAR_XF86_CONFIG "=xf86config" },
      { "D XDesktop",	"X already set up, just do desktop configuration.",
	NULL, dmenuSubmenu, NULL, &MenuXDesktops },
      { NULL } },
};

DMenu MenuXDesktops = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select the default X desktop to use.",
    "By default, XFree86 comes with a fairly vanilla desktop which\n"
    "is based around the twm(1) window manager and does not offer\n"
    "much in the way of features.  It does have the advantage of\n"
    "being a standard part of X so you don't need to load anything\n"
    "extra in order to use it.  If, however, you have access to a\n"
    "reasonably full packages collection on your installation media,\n"
    "you can choose any one of the following desktops as alternatives.",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)",
	NULL, dmenuExit },
      { "2 KDE",		"The K Desktop Environment",
	NULL, dmenuSetVariable, NULL, VAR_DESKSTYLE "=kde" },
      { "3 GNOME 2",		"The GNOME 2 Desktop Environment",
	NULL, dmenuSetVariable, NULL, VAR_DESKSTYLE "=gnome2" },
      { "4 Afterstep",	"The Afterstep window manager",
	NULL, dmenuSetVariable, NULL, VAR_DESKSTYLE "=afterstep" },
      { "5 Windowmaker",	"The Windowmaker window manager",
	NULL, dmenuSetVariable, NULL, VAR_DESKSTYLE "=windowmaker" },
      { "6 fvwm",		"The fvwm window manager",
	NULL, dmenuSetVariable, NULL, VAR_DESKSTYLE "=fvwm2" },
      { NULL } },
};

#ifndef X_AS_PKG
DMenu MenuXF86Select = {
    DMENU_NORMAL_TYPE,
    "XFree86 Distribution",
    "Please select the components you need from the XFree86\n"
    "distribution sets.",
    NULL,
    NULL,
    { { "X Exit", "Exit this menu (returning to previous)", NULL, dmenuExit },
      { "Basic",	"Basic component menu (required)",	NULL, dmenuSubmenu, NULL, &MenuXF86SelectCore },
      { "Server",	"X server menu",			NULL, dmenuSubmenu, NULL, &MenuXF86SelectServer },
      { "Fonts",	"Font set menu",			NULL, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
      { NULL } },
};

DMenu MenuXF86SelectCore = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "XFree86 base distribution types",
    "Please check off the basic XFree86 components you wish to install.\n"
    "Bin, lib, and set are recommended for a minimum installaion.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"Select all below",
	NULL,		setX11Misc, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all below",
	NULL,		clearX11Misc, NULL, NULL, ' ', ' ', ' ' },
      { " bin",         "Client applications and shared libs",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_BIN },
      { " lib",         "Data files needed at runtime",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LIB },
      { " cfg",         "Configuration files",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_CFG },
      { " set",         "XFree86 Setup Utility",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_SET },
      { " man",         "Manual pages",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_MAN },
      { " doc",         "READMEs and release notes",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_DOC },
      { " html",        "HTML documentation files",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_HTML },
      { " lkit",        "Server link kit for all other machines",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LKIT },
      { " prog",        "Programmer's header and library files",
	dmenuFlagCheck, dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_PROG },
#if defined(__i386__) && defined(PC98)
      { " 9set",	"XFree86 Setup Utility for PC98 machines",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_9SET },
      { " lk98",	"Server link kit for PC98 machines",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86Dists, '[', 'X', ']', DIST_XF86_LKIT98 },
#endif
      { NULL } },
};

DMenu MenuXF86SelectFonts = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Font distribution selection.",
    "Please check off the individual font distributions you wish to\n\
install.  At the minimum, you should install the standard\n\
75 DPI and misc fonts if you're also installing a server\n\
(these are selected by default).",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"All fonts",
	NULL,		setX11Fonts, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset font selections",
	NULL,		clearX11Fonts, NULL, NULL, ' ', ' ', ' ' },
      { " fnts",	"Standard 75 DPI and miscellaneous fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_MISC },
      { " f100",	"100 DPI fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_100 },
      { " fcyr",	"Cyrillic Fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_CYR },
      { " fscl",	"Speedo and Type scalable fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_SCALE },
      { " non",		"Japanese, Chinese and other non-english fonts",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_NON },
      { " server",	"Font server",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86FontDists, '[', 'X', ']', DIST_XF86_FONTS_SERVER },
      { NULL } },
};

DMenu MenuXF86SelectServer = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "X Server selection.",
    "Please check off the types of X servers you wish to install.\n"
    "If you are unsure as to which server will work for your graphics card,\n"
    "it is recommended that try the SVGA or VGA16 servers or, for PC98\n"
    "machines, the 9EGC or 9840 servers.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { "All",		"Select all of the above",
	NULL,		setX11Servers, NULL, NULL, ' ', ' ', ' ' },
      { "Reset",	"Reset all of the above",
	NULL,		clearX11Servers, NULL, NULL, ' ', ' ', ' ' },
      { " SVGA",	"Standard VGA or Super VGA card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_SVGA },
      { " VGA16",	"Standard 16 color VGA card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_VGA16 },
      { " Mono",	"Standard Monochrome card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MONO },
      { " 3DL",		"8, 16 and 24 bit color 3D Labs boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_3DL },
      { " 8514",	"8-bit (256 color) IBM 8514 or compatible card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_8514 },
      { " AGX",		"8-bit AGX card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_AGX },
      { " I128",	"8, 16 and 24-bit #9 Imagine I128 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_I128 },
      { " Ma8",		"8-bit ATI Mach8 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH8 },
      { " Ma32",	"8 and 16-bit (65K color) ATI Mach32 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH32 },
      { " Ma64",	"8 and 16-bit (65K color) ATI Mach64 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_MACH64 },
      { " P9K",		"8, 16, and 24-bit color Weitek P9000 based boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_P9000 },
      { " S3",		"8, 16 and 24-bit color S3 based boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_S3 },
      { " S3V",		"8, 16 and 24-bit color S3 Virge based boards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_S3V },
      { " W32",		"8-bit ET4000/W32, /W32i and /W32p cards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_W32 },
#if defined(__i386__) && defined(PC98)
      { " PC98",	"Select an X server for a NEC PC98 [Submenu]",
	NULL,		dmenuSubmenu,  NULL, &MenuXF86SelectPC98Server, '>', ' ', '>', 0 },
#elif defined(__alpha__)
      { " TGA",		"TGA cards (alpha architecture only)",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_TGA },
#endif
      { NULL } },
};

#if defined(__i386__) && defined(PC98)
DMenu MenuXF86SelectPC98Server = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "PC98 X Server selection.",
    "Please check off the types of NEC PC98 X servers you wish to install.\n\
If you are unsure as to which server will work for your graphics card,\n\
it is recommended that try the SVGA or VGA16 servers (the VGA16 and\n\
Mono servers are particularly well-suited to most LCD displays).",
    NULL,
    NULL,
    { { " 9480",	"PC98 8-bit (256 color) PEGC-480 card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9480 },
      { " 9EGC",	"PC98 4-bit (16 color) EGC card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9EGC },
      { " 9GA9",	"PC98 GA-968V4/PCI (S3 968) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9GA9 },
      { " 9GAN",	"PC98 GANB-WAP (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9GAN },
      { " 9LPW",	"PC98 PowerWindowLB (S3) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9LPW },
      { " 9MGA",	"PC98 MGA (Matrox) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9MGA },
      { " 9NKV",	"PC98 NKV-NEC (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9NKV },
      { " 9NS3",	"PC98 NEC (S3) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9NS3 },
      { " 9SPW",	"PC98 SKB-PowerWindow (S3) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9SPW },
      { " 9SVG",	"PC98 generic SVGA card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9SVG },
      { " 9TGU",	"PC98 Cyber9320 and TGUI9680 cards",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9TGU },
      { " 9WEP",	"PC98 WAB-EP (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9WEP },
      { " 9WS",		"PC98 WABS (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9WS },
      { " 9WSN",	"PC98 WSN-A2F (cirrus) card",
	dmenuFlagCheck,	dmenuSetFlag, NULL, &XF86ServerDists, '[', 'X', ']', DIST_XF86_SERVER_9WSN },
      { NULL } }
};
#endif
#endif /* !X_AS_PKG */

DMenu MenuDiskDevices = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Select Drive(s)",
    "Please select the drive, or drives, on which you wish to perform\n"
    "this operation.  If you are attempting to install a boot partition\n"
    "on a drive other than the first one or have multiple operating\n"
    "systems on your machine, you will have the option to install a boot\n"
    "manager later.  To select a drive, use the arrow keys to move to it\n"
    "and press [SPACE] or [ENTER].  To de-select it, press it again.\n\n"
    "Use [TAB] to get to the buttons and leave this menu.",
    "Press F1 for important information regarding disk geometry!",
    "drives",
    { { NULL } },
};

DMenu MenuHTMLDoc = {
    DMENU_NORMAL_TYPE,
    "Select HTML Documentation pointer",
    "Please select the body of documentation you're interested in, the main\n"
    "ones right now being the FAQ and the Handbook.  You can also choose \"other\"\n"
    "to enter an arbitrary URL for browsing.",
    "Press F1 for more help on what you see here.",
    "html",
    { { "X Exit",	"Exit this menu (returning to previous)", NULL,	dmenuExit },
      { "2 Handbook",	"The FreeBSD Handbook.",				NULL, docShowDocument },
      { "3 FAQ",	"The Frequently Asked Questions guide.",		NULL, docShowDocument },
      { "4 Home",	"The Home Pages for the FreeBSD Project (requires net)", NULL, docShowDocument },
      { "5 Other",	"Enter a URL.",						NULL, docShowDocument },
      { NULL } },
};

/* The main installation menu */
DMenu MenuInstallCustom = {
    DMENU_NORMAL_TYPE,
    "Choose Custom Installation Options",
    "This is the custom installation menu. You may use this menu to specify\n"
    "details on the type of distribution you wish to have, where you wish\n"
    "to install it from and how you wish to allocate disk storage to FreeBSD.",
    "Press F1 to read the installation guide",
    "INSTALL",
    { { "X Exit",		"Exit this menu (returning to previous)", NULL,	dmenuExit },
      { "2 Options",		"View/Set various installation options", NULL, optionsEditor },
#ifndef WITH_SLICES
      { "3 Label",		"Label disk partitions",		NULL, diskLabelEditor },
      { "4 Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "5 Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "6 Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCustomCommit },
#else
      { "3 Partition",		"Allocate disk space for FreeBSD",	NULL, diskPartitionEditor },
      { "4 Label",		"Label allocated disk partitions",	NULL, diskLabelEditor },
      { "5 Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
      { "6 Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
      { "7 Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCustomCommit },
#endif
      { NULL } },
};

#ifdef PC98
/* IPL type menu */
DMenu MenuIPLType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "overwrite me",		/* will be disk specific label */
    "If you want a FreeBSD Boot Manager, select \"BootMgr\".  If you would\n"
    "prefer your Boot Manager to remain untouched then select \"None\".\n\n",
    "Press F1 to read about drive setup",
    "drives",
    { { "BootMgr",	"Install the FreeBSD Boot Manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr },
      { "None",		"Leave the IPL untouched",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { NULL } },
};
#else
/* MBR type menu */
DMenu MenuMBRType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
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
    "Press F1 to read about drive setup",
    "drives",
    { { "BootMgr",	"Install the FreeBSD Boot Manager",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr },
      { "Standard",	"Install a standard MBR (no boot manager)",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 1 },
      { "None",		"Leave the Master Boot Record untouched",
	dmenuRadioCheck, dmenuSetValue, NULL, &BootMgr, '(', '*', ')', 2 },
      { NULL } },
};
#endif /* PC98 */

/* Final configuration menu */
DMenu MenuConfigure = {
    DMENU_NORMAL_TYPE,
    "FreeBSD Configuration Menu",	/* title */
    "If you've already installed FreeBSD, you may use this menu to customize\n"
    "it somewhat to suit your particular configuration.  Most importantly,\n"
    "you can use the Packages utility to load extra \"3rd party\"\n"
    "software not provided in the base distributions.",
    "Press F1 for more information on these options",
    "configure",
    { { "X Exit",		"Exit this menu (returning to previous)",
	NULL,	dmenuExit },
      { " Distributions", "Install additional distribution sets",
	NULL, distExtractAll },
      { " Packages",	"Install pre-packaged software for FreeBSD",
	NULL, configPackages },
      { " Root Password", "Set the system manager's password",
	NULL,	dmenuSystemCommand, NULL, "passwd root" },
#ifdef WITH_SLICES
      { " Fdisk",	"The disk Slice (PC-style partition) Editor",
	NULL, diskPartitionEditor },
#endif
      { " Label",	"The disk Label editor",
	NULL, diskLabelEditor },
      { " User Management",	"Add user and group information",
	NULL, dmenuSubmenu, NULL, &MenuUsermgmt },
#ifdef WITH_SYSCONS
      { " Console",	"Customize system console behavior",
	NULL,	dmenuSubmenu, NULL, &MenuSyscons },
#endif
      { " Time Zone",	"Set which time zone you're in",
	NULL,	dmenuSystemCommand, NULL, "tzsetup" },
      { " Media",	"Change the installation media type",
	NULL,	dmenuSubmenu, NULL, &MenuMedia },
#ifdef WITH_MICE
      { " Mouse",	"Configure your mouse",
	NULL,	dmenuSubmenu, NULL, &MenuMouse, NULL },
#endif
      { " Networking",	"Configure additional network services",
	NULL,	dmenuSubmenu, NULL, &MenuNetworking },
      { " Security",	"Configure system security options",
	NULL,	dmenuSubmenu, NULL, &MenuSecurity },
      { " Startup",	"Configure system startup options",
	NULL,	dmenuSubmenu, NULL, &MenuStartup },
      { " TTYs",	"Configure system ttys.",
	NULL,	configEtcTtys, NULL, "ttys" },
      { " Options",	"View/Set various installation options",
	NULL, optionsEditor },
      { " XFree86",	"Configure XFree86 Server",
	NULL, configXSetup },
      { " Desktop",	"Configure XFree86 Desktop",
	NULL, configXDesktop },
      { " HTML Docs",	"Go to the HTML documentation menu (post-install)",
	NULL, docBrowser },
      { " Load KLD",	"Load a KLD from a floppy",
	NULL, kldBrowser },
      { NULL } },
};

DMenu MenuStartup = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Startup Services Menu",
    "This menu allows you to configure various aspects of your system's\n"
    "startup configuration.  Use [SPACE] or [ENTER] to select items, and\n"
    "[TAB] to move to the buttons.  Select Exit to leave this menu.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
#ifdef __i386__
      { " APM",		"Auto-power management services (typically laptops)",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "apm_enable=YES" },
#endif
#ifdef PCCARD_ARCH
      { " pccard",	"Enable PCCARD (AKA PCMCIA) services (also laptops)",
	dmenuVarCheck, dmenuToggleVariable, NULL, "pccard_enable=YES" },
      { " pccard mem",	"Set PCCARD memory address (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "pccard_mem" },
      { " pccard ifconfig",	"List of PCCARD ethernet devices to configure",
	dmenuVarCheck, dmenuISetVariable, NULL, "pccard_ifconfig" },
#endif
      { " usbd", "Enable USB daemon (detect USB attach / detach)",
        dmenuVarCheck, dmenuToggleVariable, NULL, "usbd_enable=YES" },
      { " usbd flags", "Set default flags to usbd (if enabled)", 
        dmenuVarCheck, dmenuISetVariable, NULL, "usbd_flags" },
      { " ",		" -- ", NULL,	NULL, NULL, NULL, ' ', ' ', ' ' },
      { " startup dirs",	"Set the list of dirs to look for startup scripts",
	dmenuVarCheck, dmenuISetVariable, NULL, "local_startup" },
      { " named",	"Run a local name server on this host",
	dmenuVarCheck, dmenuToggleVariable, NULL, "named_enable=YES" },
      { " named flags",	"Set default flags to named (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "named_flags" },
      { " nis client",	"This host wishes to be an NIS client.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "nis_client_enable=YES" },
      { " nis domainname",	"Set NIS domainname (if enabled)",
	dmenuVarCheck, dmenuISetVariable, NULL, "nisdomainname" },
      { " nis server",	"This host wishes to be an NIS server.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "nis_server_enable=YES" },
      { " ",		" -- ", NULL,	NULL, NULL, NULL, ' ', ' ', ' ' },
      { " accounting",	"This host wishes to run process accounting.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "accounting_enable=YES" },
      { " lpd",		"This host has a printer and wants to run lpd.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "lpd_enable=YES" },
#ifdef WITH_LINUX
      { " linux",	"This host wants to be able to run linux binaries.",
	dmenuVarCheck, configLinux, NULL, VAR_LINUX_ENABLE "=YES" },
#endif
#ifdef __i386__
      { " SCO",		"This host wants to be able to run IBCS2 binaries.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "ibcs2_enable=YES" },
      { " SVR4",	"This host wants to be able to run SVR4 binaries.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "svr4_enable=YES" },
#endif
#ifdef __alpha__
      { " OSF/1",	"This host wants to be able to run DEC OSF/1 binaries.",
	dmenuVarCheck, configOSF1, NULL, VAR_OSF1_ENABLE "=YES" },
#endif
      { " quotas",	"This host wishes to check quotas on startup.",
	dmenuVarCheck, dmenuToggleVariable, NULL, "check_quotas=YES" },
      { NULL } },
};

DMenu MenuNetworking = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Network Services Menu",
    "You may have already configured one network device (and the other\n"
    "various hostname/gateway/name server parameters) in the process\n"
    "of installing FreeBSD.  This menu allows you to configure other\n"
    "aspects of your system's network configuration.",
    NULL,
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { " Interfaces",	"Configure additional network interfaces",
	NULL, tcpMenuSelect },
      { " AMD",		"This machine wants to run the auto-mounter service",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "amd_enable=YES" },
      { " AMD Flags",	"Set flags to AMD service (if enabled)",
	dmenuVarCheck,	dmenuISetVariable, NULL, "amd_flags" },
      { " Anon FTP",	"This machine wishes to allow anonymous FTP.",
	dmenuVarCheck,	configAnonFTP, NULL, "anon_ftp" },
      { " Gateway",	"This machine will route packets between interfaces",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "gateway_enable=YES" },
      { " inetd",	"This machine wants to run the inet daemon",
	dmenuVarCheck,	configInetd, NULL, "inetd_enable=YES" },
      { " NFS client",	"This machine will be an NFS client",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "nfs_client_enable=YES" },
      { " NFS server",	"This machine will be an NFS server",
	dmenuVarCheck,	configNFSServer, NULL, "nfs_server_enable=YES" },
      { " Ntpdate",	"Select a clock-synchronization server",
	dmenuVarCheck,	dmenuSubmenu, NULL, &MenuNTP, '[', 'X', ']', "ntpdate_enable=YES" },
      { " PCNFSD",	"Run authentication server for clients with PC-NFS.",
	dmenuVarCheck,	configPCNFSD, NULL, "pcnfsd" },
      { " portmap",	"This machine wants to run the portmapper daemon",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "portmap_enable=YES" },
      { " Routed",	"Select routing daemon (default: routed)",
	dmenuVarCheck,	configRouter, NULL, "router_enable=YES" },
      { " Rwhod",	"This machine wants to run the rwho daemon",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "rwhod_enable=YES" },
      { " Sendmail",	"This machine wants to run the sendmail daemon",
	NULL,		dmenuSubmenu, NULL, &MenuSendmail },
      { " Sshd",	"This machine wants to run the ssh daemon",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "sshd_enable=YES" },
      { " TCP Extensions", "Allow RFC1323 and RFC1644 TCP extensions?",
	dmenuVarCheck,	dmenuToggleVariable, NULL, "tcp_extensions=YES" },
      { NULL } },
};

DMenu MenuSendmail = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Sendmail Invocation Selection",
    "There are three options for invoking sendmail at startup.\n"
    "Please select Yes if you want to use sendmail as your mail transfer\n"
    "agent.  Selecting No disables sendmail's network socket for incoming\n"
    "email, but still enables sendmail for local and outbound mail.\n"
    "None disables sendmail completely at startup and disables inbound,\n"
    "outbound, and local mail.  See /etc/mail/README for more\n"
    "information.\n",
    NULL,
    NULL,
    {
      { " Yes",		"Start sendmail",
	dmenuVarCheck, dmenuSetVariable, NULL, "sendmail_enable=YES" },
      { " No",		"Start sendmail, but don't listen from network",
	dmenuVarCheck, dmenuSetVariable, NULL, "sendmail_enable=NO" },
      { " None",	"Don't start any sendmail processes",
	dmenuVarCheck, dmenuSetVariable, NULL, "sendmail_enable=NONE" },
      { NULL } },
};

DMenu MenuNTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "NTPDATE Server Selection",
    "There are a number of time synchronization servers available\n"
    "for public use around the Internet.  Please select one reasonably\n"
    "close to you to have your system time synchronized accordingly.",
    "These are the primary open-access NTP servers",
    NULL,
    { { "None",		        "No NTP server",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=NO,ntpdate_flags=none" },
      { "Other",		"Select a site not on this list",
	dmenuVarsCheck, configNTP, NULL, NULL },
      { "Argentina",		"tick.nap.com.ar",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tick.nap.com.ar" },
      { "Argentina #2",		"time.sinectis.com.ar",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=time.sinectis.com.ar" },
      { "Argentina #3",		"tock.nap.com.ar",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tock.nap.com.ar" },
      { "Australia",		"augean.eleceng.adelaide.edu.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=augean.eleceng.adelaide.edu.au" },
      { "Australia #2",		"ntp.adelaide.edu.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.adelaide.edu.au" },
      { "Australia #3",		"ntp.saard.net",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.saard.net" },
      { "Australia #4",		"time.deakin.edu.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.deakin.edu.au" },
      { "Australia #5",		"time.esec.com.au",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.esec.com.au" },
      { "Belgium",		"ntp1.belbone.be",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.belbone.be" },
      { "Belgium #2",		"ntp2.belbone.be",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.belbone.be" },
      { "Brazil",		"ntp.cais.rnp.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.cais.rnp.br" },
      { "Brazil #2",		"ntp.pop-df.rnp.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.pop-df.rnp.br" },
      { "Brazil #3",		"ntp.ufes.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.ufes.br" },
      { "Brazil #4",		"ntp1.pucpr.br",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.pucpr.br" },
      { "Canada",		"ntp.cpsc.ucalgary.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.cpsc.ucalgary.ca" },
      { "Canada #2",		"ntp1.cmc.ec.gc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp1.cmc.ec.gc.ca" },
      { "Canada #3",		"ntp2.cmc.ec.gc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp2.cmc.ec.gc.ca" },
      { "Canada #4",		"tick.utoronto.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=tick.utoronto.ca" },
      { "Canada #5",		"time.chu.nrc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.chu.nrc.ca" },
      { "Canada #6",		"time.nrc.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.nrc.ca" },
      { "Canada #7",		"timelord.uregina.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=timelord.uregina.ca" },
      { "Canada #8",		"tock.utoronto.ca",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=tock.utoronto.ca" },
      { "Czech",		"ntp.karpo.cz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.karpo.cz" },
      { "Denmark",		"clock.netcetera.dk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock.netcetera.dk" },
      { "Denmark",		"clock2.netcetera.dk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock2.netcetera.dk" },
      { "Spain",		"slug.ctv.es",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=slug.ctv.es" },
      { "Finland",		"tick.keso.fi",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tick.keso.fi" },
      { "Finland #2",		"tock.keso.fi",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tock.keso.fi" },
      { "France",		"ntp.obspm.fr",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.obspm.fr" },
      { "France #2",		"ntp.univ-lyon1.fr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.univ-lyon1.fr" },
      { "France #3",		"ntp.via.ecp.fr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.via.ecp.fr" },
      { "Croatia",		"zg1.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=zg1.ntp.carnet.hr" },
      { "Croatia #2",		"zg2.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=zg2.ntp.carnet.hr" },
      { "Croatia #3",		"st.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=st.ntp.carnet.hr" },
      { "Croatia #4",		"ri.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ri.ntp.carnet.hr" },
      { "Croatia #5",		"os.ntp.carnet.hr",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=os.ntp.carnet.hr" },
      { "Hungary",		"time.kfki.hu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=time.kfki.hu" },
      { "Indonesia",		"ntp.incaf.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.incaf.net" },
      { "Ireland",		"ntp.maths.tcd.ie",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.maths.tcd.ie" },
      { "Italy",		"ntps.net4u.it",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=" },
      { "Japan",		"ntp.cyber-fleet.net",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.cyber-fleet.net" },
      { "Korea",		"time.nuri.net",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.nuri.net" },
      { "Mexico",		"ntp2a.audiotel.com.mx",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2a.audiotel.com.mx" },
      { "Mexico #2",		"ntp2b.audiotel.com.mx",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2b.audiotel.com.mx" },
      { "Mexico #3",		"ntp2c.audiotel.com.mx",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2c.audiotel.com.mx" },
      { "Nigeria",		"ntp.supernet300.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.supernet300.com" },
      { "Netherlands",		"ntp1.theinternetone.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.theinternetone.net" },
      { "Netherlands #2",	"ntp2.theinternetone.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.theinternetone.net" },
      { "Netherlands #3",	"ntp3.theinternetone.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp3.theinternetone.net" },
      { "Norway",		"fartein.ifi.uio.no",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=fartein.ifi.uio.no" },
      { "Norway #2",		"time.alcanet.no",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.alcanet.no" },
      { "New Zealand",		"ntp.massey.ac.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.massey.ac.nz" },
      { "New Zealand #2",	"ntp.public.otago.ac.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.public.otago.ac.nz" },
      { "New Zealand #3",	"tk1.ihug.co.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tk1.ihug.co.nz" },
      { "New Zealand #4",	"ntp.waikato.ac.nz",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.waikato.ac.nz" },
      { "Poland",		"info.cyf-kr.edu.pl",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=info.cyf-kr.edu.pl" },
      { "Portugal",		"bug.fe.up.pt",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=bug.fe.up.pt" },
      { "Romania",		"ntp.ip.ro",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.ip.ro" },
      { "Russia",		"ntp.psn.ru",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.psn.ru" },
      { "Russia #2",		"sign.chg.ru",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=sign.chg.ru" },
      { "Sweden",		"ntp.lth.se",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.lth.se" },
      { "Singapore",		"ntp.shim.org",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.shim.org" },
      { "Slovenia",		"calvus.rzs-hm.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=calvus.rzs-hm.si" },
      { "Slovenia #2",		"sizif.mf.uni-lj.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=sizif.mf.uni-lj.si" },
      { "Slovenia #3",		"ntp1.arnes.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.arnes.si" },
      { "Slovenia #4",		"ntp2.arnes.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.arnes.si" },
      { "Slovenia #5",		"time.ijs.si",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=time.ijs.si" },
      { "Scotland",		"ntp.cs.strath.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.cs.strath.ac.uk" },
      { "United Kingdom",	"ntp.exnet.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.exnet.com" },
      { "United Kingdom #2",	"ntp0.uk.uu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp0.uk.uu.net" },
      { "United Kingdom #3",	"ntp1.uk.uu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.uk.uu.net" },
      { "United Kingdom #4",	"ntp2.uk.uu.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.uk.uu.net" },
      { "United Kingdom #5",	"ntp2a.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2a.mcc.ac.uk" },
      { "United Kingdom #6",	"ntp2b.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2b.mcc.ac.uk" },
      { "United Kingdom #7",	"ntp2c.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2c.mcc.ac.uk" },
      { "United Kingdom #8",	"ntp2d.mcc.ac.uk",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2d.mcc.ac.uk" },
      { "United Kingdom #9",	"tick.tanac.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tick.tanac.net" },
      { "U.S. AR",	"sushi.compsci.lyon.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=sushi.compsci.lyon.edu" },
      { "U.S. AZ",	"ntp.drydog.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.drydog.com" },
      { "U.S. CA",	"ntp.ucsd.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.ucsd.edu" },
      { "U.S. CA #2",	"ntp1.mainecoon.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp1.mainecoon.com" },
      { "U.S. CA #3",	"ntp2.mainecoon.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp2.mainecoon.com" },
      { "U.S. CA #4",	"reloj.kjsl.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=reloj.kjsl.com" },
      { "U.S. CA #5",	"time.five-ten-sg.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=time.five-ten-sg.com" },
      { "U.S. DE",	"louie.udel.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=louie.udel.edu" },
      { "U.S. GA",		"ntp.shorty.com",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=ntp.shorty.com" },
      { "U.S. GA #2",		"rolex.usg.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=rolex.usg.edu" },
      { "U.S. GA #3",		"timex.usg.edu",
	dmenuVarsCheck,	dmenuSetVariables, NULL, 
	"ntpdate_enable=YES,ntpdate_flags=timex.usg.edu" },
      { "U.S. IL",	"ntp-0.cso.uiuc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-0.cso.uiuc.edu" },
      { "U.S. IL #2",	"ntp-1.cso.uiuc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-1.cso.uiuc.edu" },
      { "U.S. IL #3",	"ntp-1.mcs.anl.gov",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-1.mcs.anl.gov" },
      { "U.S. IL #4",	"ntp-2.cso.uiuc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-2.cso.uiuc.edu" },
      { "U.S. IL #5",	"ntp-2.mcs.anl.gov",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-2.mcs.anl.gov" },
      { "U.S. IN",	"gilbreth.ecn.purdue.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=gilbreth.ecn.purdue.edu" },
      { "U.S. IN #2",	"harbor.ecn.purdue.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=harbor.ecn.purdue.edu" },
      { "U.S. IN #3",	"molecule.ecn.purdue.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=molecule.ecn.purdue.edu" },
      { "U.S. KS",	"ntp1.kansas.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.kansas.net" },
      { "U.S. KS #2",	"ntp2.kansas.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.kansas.net" },
      { "U.S. MA",	"ntp.ourconcord.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.ourconcord.net" },
      { "U.S. MA #2",	"timeserver.cs.umb.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=timeserver.cs.umb.edu" },
      { "U.S. MN",	"ns.nts.umn.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ns.nts.umn.edu" },
      { "U.S. MN #2",	"nss.nts.umn.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=nss.nts.umn.edu" },
      { "U.S. MO",	"time-ext.missouri.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=time-ext.missouri.edu" },
      { "U.S. MT",	"chronos1.umt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=chronos1.umt.edu" },
      { "U.S. MT #2",	"chronos2.umt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=chronos2.umt.edu" },
      { "U.S. MT #3",	"chronos3.umt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=chronos3.umt.edu" },
      { "U.S. NC",	"clock1.unc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock1.unc.edu" },
      { "U.S. NV",	"cuckoo.nevada.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=cuckoo.nevada.edu" },
      { "U.S. NV #2",	"tick.cs.unlv.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tick.cs.unlv.edu" },
      { "U.S. NV #3",	"tock.cs.unlv.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tock.cs.unlv.edu" },
      { "U.S. NY",	"clock.linuxshell.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock.linuxshell.net" },
      { "U.S. NY #2",	"ntp.ctr.columbia.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.ctr.columbia.edu" },
      { "U.S. NY #3",	"ntp0.cornell.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp0.cornell.edu" },
      { "U.S. NY #4",	"ntp1.mpis.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.mpis.net" },
      { "U.S. NY #5",	"ntp2.mpis.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.mpis.net" },
      { "U.S. NY #6",	"sundial.columbia.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=sundial.columbia.edu" },
      { "U.S. NY #7",	"timex.cs.columbia.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=timex.cs.columbia.edu" },
      { "U.S. OK",	"constellation.ecn.uoknor.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=constellation.ecn.uoknor.edu" },
      { "U.S. PA",	"clock-1.cs.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock-1.cs.cmu.edu" },
      { "U.S. PA #2",	"clock-2.cs.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock-2.cs.cmu.edu" },
      { "U.S. PA #3",	"clock.psu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock.psu.edu" },
      { "U.S. PA #4",	"fuzz.psc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=fuzz.psc.edu" },
      { "U.S. PA #5",	"ntp-1.ece.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-1.ece.cmu.edu" },
      { "U.S. PA #6",	"ntp-2.ece.cmu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-2.ece.cmu.edu" },
      { "U.S. TX",	"ntp.cox.smu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.cox.smu.edu" },
      { "U.S. TX #2",	"ntp.fnbhs.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.fnbhs.com" },
      { "U.S. TX #3",	"ntp.tmc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.tmc.edu" },
      { "U.S. TX #4",	"ntp5.tamu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp5.tamu.edu" },
      { "U.S. TX #5",	"tick.greyware.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tick.greyware.com" },
      { "U.S. TX #6",	"tock.greyware.com",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=tock.greyware.com" },
      { "U.S. VA",	"ntp-1.vt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-1.vt.edu" },
      { "U.S. VA #2",	"ntp-2.vt.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp-2.vt.edu" },
      { "U.S. VA #3",	"ntp.cmr.gov",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.cmr.gov" },
      { "U.S. VT",	"ntp0.state.vt.us",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp0.state.vt.us" },
      { "U.S. VT #2",	"ntp1.state.vt.us",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.state.vt.us" },
      { "U.S. VT #3",	"ntp2.state.vt.us",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp2.state.vt.us" },
      { "U.S. WA",	"clock.tricity.wsu.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=clock.tricity.wsu.edu" },
      { "U.S. WA #2",	"ntp.tcp-udp.net",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.tcp-udp.net" },
      { "U.S. WI",	"ntp1.cs.wisc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp1.cs.wisc.edu" },
      { "U.S. WI #2",	"ntp3.cs.wisc.edu",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp3.cs.wisc.edu" },
      { "Venezuela",	"ntp.linux.org.ve",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.linux.org.ve" },
      { "South Africa",	"ntp.cs.unp.ac.za",
	dmenuVarsCheck, dmenuSetVariables, NULL,
	"ntpdate_enable=YES,ntpdate_flags=ntp.cs.unp.ac.za" },
      { NULL } },
};

#ifdef WITH_SYSCONS
DMenu MenuSyscons = {
    DMENU_NORMAL_TYPE,
    "System Console Configuration",
    "The default system console driver for FreeBSD (syscons) has a\n"
    "number of configuration options which may be set according to\n"
    "your preference.\n\n"
    "When you are done setting configuration options, select Cancel.",
    "Configure your system console settings",
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)", NULL, dmenuExit },
#ifdef PC98
      { "2 Keymap",	"Choose an alternate keyboard map",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "3 Repeat",	"Set the rate at which keys repeat",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "4 Saver",	"Configure the screen saver",		NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
#else
      { "2 Font",	"Choose an alternate screen font",	NULL, dmenuSubmenu, NULL, &MenuSysconsFont },
      { "3 Keymap",	"Choose an alternate keyboard map",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeymap },
      { "4 Repeat",	"Set the rate at which keys repeat",	NULL, dmenuSubmenu, NULL, &MenuSysconsKeyrate },
      { "5 Saver",	"Configure the screen saver",		NULL, dmenuSubmenu, NULL, &MenuSysconsSaver },
      { "6 Screenmap",	"Choose an alternate screenmap",	NULL, dmenuSubmenu, NULL, &MenuSysconsScrnmap },
      { "7 Ttys",       "Choose console terminal type",         NULL, dmenuSubmenu, NULL, &MenuSysconsTtys },
#endif
      { NULL } },
};

#ifdef PC98
DMenu MenuSysconsKeymap = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keymap",
    "The default system console driver for FreeBSD (syscons) defaults\n"
    "to a standard \"PC-98x1\" keyboard map.  Users may wish to choose\n"
    "one of the other keymaps below.\n"
    "Note that sysinstall itself only uses the part of the keyboard map\n"
    "which is required to generate the ANSI character subset, but your\n"
    "choice of keymap will also be saved for later (fuller) use.",
    "Choose a keyboard map",
    NULL,
    { { "Japanese PC-98x1",		"Japanese PC-98x1 keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.pc98" },
      { " Japanese PC-98x1 (ISO)",	"Japanese PC-98x1 (ISO) keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.pc98.iso" },
      { NULL } },
};
#else
DMenu MenuSysconsKeymap = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keymap",
    "The default system console driver for FreeBSD (syscons) defaults\n"
    "to a standard \"American\" keyboard map.  Users in other countries\n"
    "(or with different keyboard preferences) may wish to choose one of\n"
    "the other keymaps below.\n"
    "Note that sysinstall itself only uses the part of the keyboard map\n"
    "which is required to generate the ANSI character subset, but your\n"
    "choice of keymap will also be saved for later (fuller) use.",
    "Choose a keyboard map",
    NULL,
    { { "Belgian",	"Belgian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=be.iso" },
      { " Brazil CP850",	"Brazil CP850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.cp850" },
      { " Brazil ISO (accent)",	"Brazil ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.iso.acc" },
      { " Brazil ISO",	"Brazil ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=br275.iso" },
      { " Bulgarian BDS",	"Bulgarian BDS keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=bg.bds.ctrlcaps" },
      { " Bulgarian Phonetic",	"Bulgarian Phonetic keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=bg.phonetic.ctrlcaps" },
      { " Croatian ISO",	"Croatian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=hr.iso" },
      { " Czech ISO (accent)",	"Czech ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=cs.latin2.qwertz" },
      { "Danish CP865",	"Danish Code Page 865 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=danish.cp865" },
      { " Danish ISO",	"Danish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=danish.iso" },
      { "Estonian ISO", "Estonian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=estonian.iso" },
      { " Estonian ISO 15", "Estonian ISO 8859-15 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=estonian.iso15" },
      { " Estonian CP850", "Estonian Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=estonian.cp850" },
      { "Finnish CP850","Finnish Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=finnish.cp850" },
      { " Finnish ISO",  "Finnish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=finnish.iso" },
      { " French ISO (accent)", "French ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.iso.acc" },
      { " French ISO",	"French ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=fr.iso" },
      { "German CP850",	"German Code Page 850 keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=german.cp850"	},
      { " German ISO",	"German ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=german.iso" },
      { "Hungarian 101", "Hungarian ISO keymap (101 key)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=hu.iso2.101keys" },
      { " Hungarian 102", "Hungarian ISO keymap (102 key)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=hu.iso2.102keys" },
      { "Icelandic (accent)", "Icelandic ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=icelandic.iso.acc" },
      { " Icelandic",	"Icelandic ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=icelandic.iso" },
      { " Italian",	"Italian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=it.iso" },
      { "Japanese 106",	"Japanese 106 keymap",  dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=jp.106" },
      { "Latin American", "Latin American ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=lat-amer" },
      { "Norway ISO",	"Norwegian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=norwegian.iso" },
      { "Polish ISO",	"Polish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=pl_PL.ISO8859-2" },
      { " Portuguese (accent)",	"Portuguese ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=pt.iso.acc" },
      { " Portuguese",	"Portuguese ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=pt.iso" },
      { "Russia KOI8-R", "Russian KOI8-R keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ru.koi8-r" },
      { "Slovenian", "Slovenian ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=si.iso" },
      { " Spanish (accent)", "Spanish ISO keymap (accent keys)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=spanish.iso.acc" },
      { " Spanish",	"Spanish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=spanish.iso" },
      { " Swedish CP850", "Swedish Code Page 850 keymap", dmenuVarCheck,	dmenuSetKmapVariable, NULL, "keymap=swedish.cp850" },
      { " Swedish ISO",	"Swedish ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swedish.iso" },
      { " Swiss French ISO (accent)", "Swiss French ISO keymap (accent keys)", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissfrench.iso.acc" },
      { " Swiss French ISO", "Swiss French ISO keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissfrench.iso" },
      { " Swiss French CP850", "Swiss French Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissfrench.cp850" },
      { " Swiss German ISO (accent)", "Swiss German ISO keymap (accent keys)", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.iso.acc" },
      { " Swiss German ISO", "Swiss German ISO keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.iso" },
      { " Swiss German CP850", "Swiss German Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=swissgerman.cp850" },
      { "UK CP850",	"UK Code Page 850 keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=uk.cp850" },
      { " UK ISO",	"UK ISO keymap", dmenuVarCheck,	dmenuSetKmapVariable, NULL, "keymap=uk.iso" },
      { " Ukrainian KOI8-U",	"Ukrainian KOI8-U keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ua.koi8-u" },
      { " Ukrainian KOI8-U+KOI8-R",	"Ukrainian KOI8-U+KOI8-R keymap (alter)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=ua.koi8-u.shift.alt" },
      { " USA CapsLock->Ctrl",	"US standard (Caps as L-Control)",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.pc-ctrl" },
      { " USA Dvorak",	"US Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorak" },
      { " USA Dvorak (left)",	"US left handed Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorakl" },
      { " USA Dvorak (right)",	"US right handed Dvorak keymap", dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.dvorakr" },
      { " USA Emacs",	"US standard optimized for EMACS",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.emacs" },
      { " USA ISO",	"US ISO keymap",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.iso" },
      { " USA UNIX",	"US traditional UNIX-workstation",	dmenuVarCheck, dmenuSetKmapVariable, NULL, "keymap=us.unix" },
      { NULL } },
};
#endif /* PC98 */

DMenu MenuSysconsKeyrate = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Keyboard Repeat Rate",
    "This menu allows you to set the speed at which keys repeat\n"
    "when held down.",
    "Choose a keyboard repeat rate",
    NULL,
    { { "Slow",	"Slow keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=slow" },
      { "Normal", "\"Normal\" keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=normal" },
      { "Fast",	"Fast keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=fast" },
      { "Default", "Use default keyboard repeat rate",	dmenuVarCheck,	dmenuSetVariable, NULL, "keyrate=NO" },
      { NULL } },
};

DMenu MenuSysconsSaver = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screen Saver",
    "By default, the console driver will not attempt to do anything\n"
    "special with your screen when it's idle.  If you expect to leave your\n"
    "monitor switched on and idle for long periods of time then you should\n"
    "probably enable one of these screen savers to prevent phosphor burn-in.",
    "Choose a nifty-looking screen saver",
    NULL,
    { { "1 Blank",	"Simply blank the screen",
	dmenuVarCheck, configSaver, NULL, "saver=blank" },
      { "2 Daemon",	"\"BSD Daemon\" animated screen saver (text)",
	dmenuVarCheck, configSaver, NULL, "saver=daemon" },
      { "3 Fade",	"Fade out effect screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=fade" },
      { "4 Fire",	"Flames effect screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=fire" },
      { "5 Green",	"\"Green\" power saving mode (if supported by monitor)",
	dmenuVarCheck, configSaver, NULL, "saver=green" },
      { "6 Logo",	"\"BSD Daemon\" animated screen saver (graphics)",
	dmenuVarCheck, configSaver, NULL, "saver=logo" },
      { "7 Rain",	"Rain drops screen saver",
	dmenuVarCheck, configSaver, NULL, "saver=rain" },
      { "8 Snake",	"Draw a FreeBSD \"snake\" on your screen",
	dmenuVarCheck, configSaver, NULL, "saver=snake" },
      { "9 Star",	"A \"twinkling stars\" effect",
	dmenuVarCheck, configSaver, NULL, "saver=star" },
      { "Warp",	"A \"stars warping\" effect",
	dmenuVarCheck, configSaver, NULL, "saver=warp" },
      { "Timeout",	"Set the screen saver timeout interval",
	NULL, configSaverTimeout, NULL, NULL, ' ', ' ', ' ' },
      { NULL } },
};

#ifndef PC98
DMenu MenuSysconsScrnmap = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Screenmap",
    "Unless you load a specific font, most PC hardware defaults to\n"
    "displaying characters in the IBM 437 character set.  However,\n"
    "in the Unix world, this character set is very rarely used.  Most\n"
    "Western European countries, for example, prefer ISO 8859-1.\n"
    "American users won't notice the difference since the bottom half\n"
    "of all these character sets is ANSI anyway.\n"
    "If your hardware is capable of downloading a new display font,\n"
    "you should probably choose that option.  However, for hardware\n"
    "where this is not possible (e.g. monochrome adapters), a screen\n"
    "map will give you the best approximation that your hardware can\n"
    "display at all.",
    "Choose a screen map",
    NULL,
    { { "1 None",                 "No screenmap, don't touch font", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=NO" },
      { "2 ISO 8859-1 to IBM437", "W-Europe ISO 8859-1 to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=iso-8859-1_to_cp437" },
      { "3 ISO 8859-7 to IBM437", "Greek ISO 8859-7 to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=iso-8859-7_to_cp437" },
      { "4 US-ASCII to IBM437",   "US-ASCII to IBM 437 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=us-ascii_to_cp437" },
      { "5 KOI8-R to IBM866",     "Russian KOI8-R to IBM 866 screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=koi8-r2cp866" },
      { "6 KOI8-U to IBM866u",    "Ukrainian KOI8-U to IBM 866u screenmap", dmenuVarCheck, dmenuSetVariable, NULL, "scrnmap=koi8-u2cp866u" },
      { NULL } },
};

DMenu MenuSysconsTtys = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Terminal Type",
    "For various console encodings, a corresponding terminal type\n"
    "must be chosen in /etc/ttys.\n\n"
    "WARNING: For compatibility reasons, only entries starting with\n"
    "ttyv and terminal types starting with cons[0-9] can be changed\n"
    "via this menu.\n",
    "Choose a terminal type",
    NULL,
    { { "1 None",               "Don't touch anything",  dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=NO" },
      { "2 IBM437 (VGA default)", "cons25", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25" },
      { "3 ISO 8859-1",         "cons25l1", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25l1" },
      { "4 ISO 8859-2",         "cons25l2", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25l2" },
      { "5 ISO 8859-7",         "cons25l7", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25l7" },
      { "6 KOI8-R",             "cons25r", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25r" },
      { "7 KOI8-U",             "cons25u", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25u" },
      { "8 US-ASCII",           "cons25w", dmenuVarCheck, dmenuSetVariable, NULL, VAR_CONSTERM "=cons25w" },
      { NULL } },
};

DMenu MenuSysconsFont = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "System Console Font",
    "Most PC hardware defaults to displaying characters in the\n"
    "IBM 437 character set.  However, in the Unix world, this\n"
    "character set is very rarely used.  Most Western European\n"
    "countries, for example, prefer ISO 8859-1.\n"
    "American users won't notice the difference since the bottom half\n"
    "of all these charactersets is ANSI anyway.  However, they might\n"
    "want to load a font anyway to use the 30- or 50-line displays.\n"
    "If your hardware is capable of downloading a new display font,\n"
    "you can select the appropriate font below.",
    "Choose a font",
    NULL,
    { { "1 None", "Use hardware default font", dmenuVarCheck, dmenuSetVariables, NULL,
	"font8x8=NO,font8x14=NO,font8x16=NO" },
      { "2 IBM 437", "English and others, VGA default", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=cp437-8x8,font8x14=cp437-8x14,font8x16=cp437-8x16" },
      { "3 IBM 850", "Western Europe, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp850-8x8,font8x14=cp850-8x14,font8x16=cp850-8x16" },
      { "4 IBM 865", "Norwegian, IBM encoding",	dmenuVarCheck,	dmenuSetVariables, NULL,
	"font8x8=cp865-8x8,font8x14=cp865-8x14,font8x16=cp865-8x16" },
      { "5 IBM 866", "Russian, IBM encoding (use with KOI8-R screenmap)",   dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=cp866-8x8,font8x14=cp866-8x14,font8x16=cp866b-8x16,mousechar_start=3" },
      { "6 IBM 866u", "Ukrainian, IBM encoding (use with KOI8-U screenmap)",   dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=cp866u-8x8,font8x14=cp866u-8x14,font8x16=cp866u-8x16,mousechar_start=3" },
      { "7 IBM 1251", "Cyrillic, MS Windows encoding",  dmenuVarCheck, dmenuSetVariables, NULL,
	"font8x8=cp1251-8x8,font8x14=cp1251-8x14,font8x16=cp1251-8x16,mousechar_start=3" },
      { "8 ISO 8859-1", "Western Europe, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso-8x8,font8x14=iso-8x14,font8x16=iso-8x16" },
      { "9 ISO 8859-2", "Eastern Europe, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso02-8x8,font8x14=iso02-8x14,font8x16=iso02-8x16" },
      { "a ISO 8859-4", "Baltic, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso04-8x8,font8x14=iso04-8x14,font8x16=iso04-8x16" },
      { "b ISO 8859-7", "Greek, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso07-8x8,font8x14=iso07-8x14,font8x16=iso07-8x16" },
      { "c ISO 8859-8", "Hebrew, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso08-8x8,font8x14=iso08-8x14,font8x16=iso08-8x16" },
      { "d ISO 8859-15", "Europe, ISO encoding", dmenuVarCheck,  dmenuSetVariables, NULL,
	"font8x8=iso15-8x8,font8x14=iso15-8x14,font8x16=iso15-8x16" },
      { "e SWISS", "English, better resolution", dmenuVarCheck, dmenuSetVariables, NULL,
	"font8x8=swiss-8x8,font8x14=NO,font8x16=swiss-8x16" },
      { NULL } },
};
#endif /* PC98 */
#endif /* WITH_SYSCONS */

DMenu MenuUsermgmt = {
    DMENU_NORMAL_TYPE,
    "User and group management",
    "The submenus here allow to manipulate user groups and\n"
    "login accounts.\n",
    "Configure your user groups and users",
    NULL,
    { { "X Exit",	"Exit this menu (returning to previous)", NULL, dmenuExit },
      { "User",		"Add a new user to the system.",	NULL, userAddUser },
      { "Group",	"Add a new user group to the system.",	NULL, userAddGroup },
      { NULL } },
};

DMenu MenuSecurity = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "System Security Options Menu",
    "This menu allows you to configure aspects of the operating system security\n"
    "policy.  Please read the system documentation carefully before modifying\n"
    "these settings, as they may cause service disruption if used improperly.\n"
    "\n"
    "Most settings will take affect only following a system reboot.",
    "Configure system security options",
    NULL,
    { { "X Exit",      "Exit this menu (returning to previous)",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' },
      { " Security Profile",   "Select a security profile for the system",
	NULL, configSecurityProfile },
      { " LOMAC",         "Use Low Watermark Mandatory Access Control at boot",
	dmenuVarCheck,  dmenuToggleVariable, NULL, "lomac_enable=YES" },
      { " NFS port",	"Require that the NFS clients used reserved ports",
	dmenuVarCheck,  dmenuToggleVariable, NULL, "nfs_reserved_port_only=YES" },
      { NULL } },
};

DMenu MenuSecurityProfile = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Default system security profile",
    "Each item in this list will set what it considers to\n"
    "be \"appropriate\" values in that category for various\n"
    "security-related knobs in /etc/rc.conf.",
    "Select a canned security profile - F1 for help",
    "security",						/* help file */
    { { "X Exit",	"Exit this menu (returning to previous)", NULL, dmenuExit },
      { "Medium",	"Moderate security settings.", NULL, configSecurityModerate },
      { "Extreme",	"Very restrictive security settings.", NULL, configSecurityExtreme },
      { NULL } },
};

DMenu MenuFixit = {
    DMENU_NORMAL_TYPE,
    "Please choose a fixit option",
    "There are three ways of going into \"fixit\" mode:\n"
    "- you can use the live filesystem CDROM/DVD, in which case there will be\n"
    "  full access to the complete set of FreeBSD commands and utilities,\n"
    "- you can use the more limited (but perhaps customized) fixit floppy,\n"
    "- or you can start an Emergency Holographic Shell now, which is\n"
    "  limited to the subset of commands that is already available right now.",
    "Press F1 for more detailed repair instructions",
    "fixit",
{ { "X Exit",		"Exit this menu (returning to previous)",	NULL, dmenuExit },
  { "2 CDROM/DVD",	"Use the \"live\" filesystem CDROM/DVD",	NULL, installFixitCDROM },
  { "3 Floppy",		"Use a floppy generated from the fixit image",	NULL, installFixitFloppy },
  { "4 Shell",		"Start an Emergency Holographic Shell",		NULL, installFixitHoloShell },
  { NULL } },
};
