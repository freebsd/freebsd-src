/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: menus.c,v 1.49 1996/04/07 03:52:33 jkh Exp $
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

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "Welcome to FreeBSD!",				/* title */
    "This is the main menu of the FreeBSD installation system.  Please\n\
select one of the options below by using the arrow keys or typing the\n\
first character of the option name you're interested in.  Invoke an\n\
option by pressing [ENTER].",				/* prompt */
    "Press F1 for usage instructions",			/* help line */
    "usage",						/* help file */
{ { "Usage",	"Quick start - How to use this menu system",		NULL, dmenuDisplayFile, NULL, "usage" },
  { "Doc",	"More detailed documentation on FreeBSD",		NULL, dmenuSubmenu, NULL, &MenuDocumentation },
  { "Options",	"Go to options editor",					NULL, optionsEditor },
  { "Novice",	"Begin a novice installation (for beginners)",		NULL, installNovice },
  { "Express",	"Begin a quick installation (for the impatient)",	NULL, installExpress },
  { "Custom",	"Begin a custom installation (for experts)",		NULL, dmenuSubmenu, NULL, &MenuInstallCustom },
  { "Fixit",	"Go into repair mode with CDROM or floppy",		NULL, dmenuSubmenu, NULL, &MenuFixit },
  { "Upgrade",	"Upgrade an existing 2.0.5 system",			NULL, installUpgrade },
  { "Configure","Do post-install configuration of FreeBSD",		NULL, dmenuSubmenu, NULL, &MenuConfigure },
  { "Quit",	"Exit this menu (and the installation)",		NULL },
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
{ { "CDROM",	"Use the 2nd \"live\" CDROM from the distribution",	NULL, installFixitCDROM },
  { "Floppy",	"Use a floppy generated from the fixit image",		NULL, installFixitFloppy },
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
{ { "README",	"A general description of FreeBSD.  Read this!",	NULL, dmenuDisplayFile, NULL, "readme" },
  { "Hardware",	"The FreeBSD survival guide for PC hardware.",		NULL, dmenuDisplayFile,	NULL, "hardware" },
  { "Install",	"A step-by-step guide to installing FreeBSD.",		NULL, dmenuDisplayFile,	NULL, "install" },
  { "Copyright","The FreeBSD Copyright notices.",			NULL, dmenuDisplayFile,	NULL, "COPYRIGHT" },
  { "Release",	"The release notes for this version of FreeBSD.",	NULL, dmenuDisplayFile, NULL, "relnotes" },
  { "HTML Docs","Go to the HTML documentation menu (post-install).",	NULL, docBrowser },
  { "Exit",	"Exit this menu (returning to previous)",		NULL, dmenuCancel },
  { NULL } },
};

DMenu MenuMouse = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
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
{ { "COM1",	"Serial mouse on COM1",	NULL, dmenuSystemCommand, NULL,	"ln -fs /dev/cuaa0 /dev/mouse" },
  { "COM2",	"Serial mouse on COM2", NULL, dmenuSystemCommand, NULL,	"ln -fs /dev/cuaa1 /dev/mouse" },
  { "COM3",	"Serial mouse on COM3", NULL, dmenuSystemCommand, NULL,	"ln -fs /dev/cuaa2 /dev/mouse" },
  { "COM4",	"Serial mouse on COM4", NULL, dmenuSystemCommand, NULL,	"ln -fs /dev/cuaa3 /dev/mouse" },
  { "BusMouse",	"Logitech or ATI bus mouse", NULL, dmenuSystemCommand, NULL, "ln -fs /dev/mse0 /dev/mouse" },
  { "PS/2",	"PS/2 style mouse (requires kernel rebuild)", NULL, dmenuSystemCommand, NULL, "ln -fs /dev/psm0 /dev/mouse" },
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
    "If you are using PPP over a serial device (cuaa0 or cuaa1) as opposed\n\
to a direct ethernet connection, then you may first need to dial your\n\
service provider using the ppp utility we provide for that purpose.\n\
You can also install over a parallel port using a special \"laplink\"\n\
cable, though this only works if you have another FreeBSD machine running\n\
a fairly recent (2.0R or later) release to talk to.\n\n\
To use PPP, select one of the serial devices, otherwise select lp0 for\n\
the parallel port or one of the ethernet controllers (if you have one)\n\
for an ethernet installation.",
    "Press F1 to read network configuration manual",
    "network_device",
    { { NULL } },
};

/* The media selection menu */
DMenu MenuMedia = {
    DMENU_RADIO_TYPE,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n\
media, ranging from floppies to an Internet FTP server.  If you're\n\
installing FreeBSD from a supported CDROM drive then this is generally\n\
the best media to use if you have no overriding reason for using other\n\
media.",
    "Press F1 for more information on the various media types",
    "media",
{ { "CDROM",	"Install from a FreeBSD CDROM",			NULL, mediaSetCDROM },
  { "DOS",	"Install from a DOS partition",			NULL, mediaSetDOS },
  { "File System", "Install from an existing filesystem",	NULL, mediaSetUFS },
  { "Floppy",	"Install from a floppy disk set",		NULL, mediaSetFloppy },
  { "FTP",	"Install from an FTP server",			NULL, mediaSetFTPActive },
  { "FTP Passive", "Install from an FTP server through a firewall", NULL, mediaSetFTPPassive },
  { "NFS",	"Install over NFS",				NULL, mediaSetNFS },
  { "Tape",	"Install from SCSI or QIC tape",		NULL, mediaSetTape },
  { NULL } },
};

/* The distributions menu */
DMenu MenuDistributions = {
    DMENU_NORMAL_TYPE,
    "Choose Distributions",
    "As a convenience, we provide several \"canned\" distribution sets.\n\
These select what we consider to be the most reasonable defaults for the\n\
type of system in question.  If you would prefer to pick and choose the\n\
list of distributions yourself, simply select \"Custom\".  You can also\n\
add distribution sets together by picking more than one, fine-tuning the\n\
final results with the Custom item.  When you are finished, select Cancel",
    "Press F1 for more information on these options.",
    "distributions",
{ { "Developer",	"Full sources, binaries and doc but no games [180MB]",
    NULL, distSetDeveloper },
  { "X-Developer",	"Same as above, but includes XFree86 [201MB]",
    NULL, distSetXDeveloper },
  { "Kern-Developer",	"Full binaries and doc, kernel sources only [70MB]",
    NULL, distSetKernDeveloper },
  { "User",		"Average user - binaries and doc but no sources [52MB]",
    NULL, distSetUser },
  { "X-User",		"Same as above, but includes XFree86 [52MB]",
    NULL, distSetXUser },
  { "Minimal",		"The smallest configuration possible [44MB]",
    NULL, distSetMinimum },
  { "Everything",	"All sources, binaries and XFree86 binaries [700MB]",
    NULL, distSetEverything	},
  { "Custom",		"Specify your own distribution set [?]",
    NULL, dmenuSubmenu, NULL, &MenuSubDistributions },
  { "Clear",		"Reset selected distribution list to None",
    NULL, distReset },
  { NULL } },
};

static int
DESFlagCheck(dialogMenuItem *item)
{
    return Dists & DIST_DES;
}

static int
srcFlagCheck(dialogMenuItem *item)
{
    return Dists & DIST_SRC;
}

static int
x11FlagCheck(dialogMenuItem *item)
{
    return Dists & DIST_XF86;
}

DMenu MenuSubDistributions = {
    DMENU_CHECKLIST_TYPE,
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
  { NULL } },
};

DMenu MenuDESDistributions = {
    DMENU_CHECKLIST_TYPE,
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
  { NULL } },
};

DMenu MenuSrcDistributions = {
    DMENU_CHECKLIST_TYPE,
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
  { NULL } },
};

static int
clearx11(dialogMenuItem *self)
{
    XF86Dists = 0;
    XF86ServerDists = 0;
    XF86FontDists = 0;
    Dists &= ~DIST_XF86;
    return DITEM_REDRAW;
}

static int
checkx11Basic(dialogMenuItem *self)
{
    return XF86Dists;
}

static int
checkx11Servers(dialogMenuItem *self)
{
    return XF86ServerDists;
}

static int
checkx11Fonts(dialogMenuItem *self)
{
    return XF86FontDists;
}


DMenu MenuXF86Select = {
    DMENU_CHECKLIST_TYPE,
    "XFree86 3.1.2-S Distribution",
    "Please select the components you need from the XFree86 3.1.2-S\n\
distribution.  We recommend that you select what you need from the basic\n\
component set and at least one entry from the Server and Font set menus.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86",
{ { "Basic",	"Basic component menu (required)",
    checkx11Basic, dmenuSubmenu, NULL, &MenuXF86SelectCore },
  { "Server",	"X server menu",
    checkx11Servers, dmenuSubmenu, NULL, &MenuXF86SelectServer },
  { "Fonts",	"Font set menu",
    checkx11Fonts, dmenuSubmenu, NULL, &MenuXF86SelectFonts },
  { "Clear",	"Reset XFree86 distribution list",
    NULL, clearx11 },
  { NULL } },
};

DMenu MenuXF86SelectCore = {
    DMENU_CHECKLIST_TYPE,
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
  { NULL } },
};

DMenu MenuXF86SelectFonts = {
    DMENU_CHECKLIST_TYPE,
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
  { NULL } },
};

DMenu MenuXF86SelectServer = {
    DMENU_CHECKLIST_TYPE,
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
  { NULL } },
};

DMenu MenuDiskDevices = {
    DMENU_CHECKLIST_TYPE,
    "Select Drive(s)",
    "Please select the drive, or drives, on which you wish to perform\n\
this operation.  If you are attempting to install a boot partition\n\
on a drive other than the first one or have multiple operating\n\
systems on your machine, you will have the option to install a boot\n\
manager later.  To select a drive, use the arrow keys to move to it\n\
and press [SPACE].  When you're finished, select Cancel to go on to\n\
the next step.",
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
{ { "Options",		"Go to Options editor",			NULL, optionsEditor },
  { "Partition",	"Allocate disk space for FreeBSD",	NULL, diskPartitionEditor },
  { "Label",		"Label allocated disk partitions",	NULL, diskLabelEditor },
  { "Distributions",	"Select distribution(s) to extract",	NULL, dmenuSubmenu, NULL, &MenuDistributions },
  { "Media",		"Choose the installation media type",	NULL, dmenuSubmenu, NULL, &MenuMedia },
  { "Commit",		"Perform any pending Partition/Label/Extract actions", NULL, installCommit },
  { "Extract",		"Just do distribution extract step",	NULL, distExtractAll },
  { "Exit",		"Exit this menu (returning to previous)", NULL, dmenuCancel },
  { NULL } },
};

/* MBR type menu */
DMenu MenuMBRType = {
    DMENU_RADIO_TYPE,
    "overwrite me",		/* will be disk specific label */
    "FreeBSD comes with a boot selector that allows you to easily\n"
    "select between FreeBSD and any other operating systems on your machine\n"
"at boot time.  If you have more than one drive and want to boot\n"
"from the second one, the boot selector will also make it possible\n"
"to do so (limitations in the PC BIOS usually prevent this otherwise).\n"
"If you do not want a boot selector, or wish to replace an existing\n"
"one, select \"standard\".  If you would prefer your Master Boot\n"
"Record to remain untouched then select \"none\".  NOTE:  PC-DOS users\n"
"will almost certainly NOT want to select one!",
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
{ { "Add User",		"Add users to the system",
    NULL, dmenuSystemCommand, NULL, "adduser -config_create ; adduser -s" },
  { "Console",		"Customize system console behavior",
    NULL, dmenuSubmenu, NULL, &MenuSyscons },
  { "Time Zone",	"Set which time zone you're in",
    NULL, dmenuSystemCommand, NULL, "rm -f /etc/wall_cmos_clock /etc/localtime; tzsetup" },
  { "Media",		"Change the installation media type",
    NULL, dmenuSubmenu, NULL, &MenuMedia	},
  { "Mouse",		"Select the type of mouse you have",
    NULL, dmenuSubmenu, NULL, &MenuMouse, NULL },
  { "Networking",	"Configure additional network services",
    NULL, dmenuSubmenu, NULL, &MenuNetworking },
  { "Options",		"Go to options editor",
    NULL, optionsEditor },
  { "Packages",		"Install pre-packaged software for FreeBSD",
    NULL, configPackages },
  { "Ports",		"Link to FreeBSD Ports Collection on CD/NFS",
    NULL, configPorts },
  { "Root Password",	"Set the system manager's password",
    NULL, dmenuSystemCommand, NULL, "passwd root" },
  { "HTML Docs",	"Go to the HTML documentation menu (post-install)",
    NULL, docBrowser },
  { "XFree86",		"Configure XFree86",
    NULL, configXFree86 },
  { "Exit",		"Exit this menu (returning to previous)",
    NULL, dmenuCancel },
  { NULL } },
};

DMenu MenuNetworking = {
    DMENU_CHECKLIST_TYPE,
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
    dmenuVarCheck, dmenuSetVariable, NULL, "nfs_client=YES" },
  { "NFS server",	"This machine will be an NFS server",
    NULL, configNFSServer },
  { "Gateway",		"This machine will route packets between interfaces",
    dmenuVarCheck, dmenuSetVariable, NULL, "gateway=YES" },
  { "Gated",		"This machine wants to run gated instead of routed",
    NULL, configGated },
  { "Ntpdate",		"Select a clock-syncronization server",
    dmenuVarCheck, dmenuSubmenu, NULL, &MenuNTP, '[', 'X', ']', (int)"ntpdate" },
  { "Routed",		"Set flags for routed (default: -q)",
    dmenuVarCheck, configRoutedFlags, NULL, "routed" },
  { "Rwhod",		"This machine wants to run the rwho daemon",
    dmenuVarCheck, dmenuSetVariable, NULL, "rwhod=YES" },
  { "Anon FTP",		"This machine wishes to allow anonymous FTP.",
    NULL, configAnonFTP },
  { "WEB Server",	"This machine wishes to be a WWW server.",
    NULL, configApache },
  { "Samba",		"Install Samba for LanManager (NETBUI) access.",
    NULL, configSamba },
  { "PCNFSD",		"Run authentication server for clients with PC-NFS.",
    NULL, configPCNFSD },
  { NULL } },
};

DMenu MenuNTP = {
    DMENU_RADIO_TYPE,
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
  { "Exit",	"Exit this menu (returning to previous)", NULL, dmenuCancel },
  { NULL } },
};

DMenu MenuSysconsKeymap = {
    DMENU_RADIO_TYPE,
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
    DMENU_RADIO_TYPE,
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
    DMENU_RADIO_TYPE,
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
