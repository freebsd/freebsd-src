/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: menus.c,v 1.8 1995/05/05 23:47:43 jkh Exp $
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

#include "sysinstall.h"

/* All the system menus go here.
 *
 * Hardcoded things like version number strings will disappear from
 * these menus just as soon as I add the code for doing inline variable
 * expansion.
 */

/* Forward decls for submenus */
extern DMenu MenuDocumentation;
extern DMenu MenuLanguage;
extern DMenu MenuMedia;
extern DMenu MenuInstallType;
extern DMenu MenuInstallOptions;
extern DMenu MenuDistributions;

/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "Welcome to FreeBSD 2.0.5!",	/* title */
    "This is the main menu of the FreeBSD installation system.  Please\n\
select one of the options below by using the arrow keys or typing the\n\
first character of the option name you're interested in.  Invoke an\n\
option by pressing enter.  If you'd like a shell, press ESC",	/* prompt */
    "Press F1 for usage instructions",	/* help line */
    "usage.hlp",			/* help file */
{ { "Usage", "Quick start - How to use this menu system.",	/* U */
	DMENU_DISPLAY_FILE, (void *)"usage.hlp", 0 },
  { "Doc", "More detailed documentation on FreeBSD.",		/* D */
	DMENU_SUBMENU, (void *)&MenuDocumentation, 0 },
  { "Lang", "Select natural language options.",			/* L */
	DMENU_SUBMENU, (void *)&MenuLanguage, 0 },
  { "Install", "Begin installation",				/* I */
	DMENU_CALL, (void *)installCustom, 0 },
  { NULL } },
};

/* The main documentation menu */
DMenu MenuDocumentation = {
DMENU_NORMAL_TYPE,
"Documentation for FreeBSD 2.0.5",	/* Title */
"If you are at all unsure about the configuration of your hardware\n\
or are looking to build a system specifically for FreeBSD, read the\n\
Hardware guide!  New users should also read the Install document for\n\
a step-by-step tutorial on installing FreeBSD.  For general information,\n\
consult the README file.  If you're having other problems, you may find\n\
answers in the FAQ.",
"Confused?  Press F1 for help.",
"usage.hlp",			/* help file */
{ { "README", "Read this for a general description of FreeBSD", /* R */
	DMENU_DISPLAY_FILE, (void *)"readme.hlp", 0 },
  { "Hardware", "The FreeBSD survival guide for PC hardware.",  /* H */
	DMENU_DISPLAY_FILE, (void *)"hardware.hlp", 0 },
  { "Install", "A step-by-step guide to installing FreeBSD.",   /* I */
	DMENU_DISPLAY_FILE, (void *)"install.hlp", 0 },
  { "FAQ", "Frequently Asked Questions about FreeBSD.",         /* F */
	DMENU_DISPLAY_FILE, (void *)"faq.hlp", 0 },
  { NULL } },
};

/* The language selection menu */
/*
 * Note:  The RADIO menus use a slightly different syntax.  If an item
 * name starts with `*', it's considered to be "ON" by default,
 * otherwise off.
 */
DMenu MenuLanguage = {
DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
"Natural language selection",	/* title */
"Please specify the language you'd like to use by default.\n\n\
While almost all of the system's documentation is still written\n\
in english (and may never be translated), there are a few guides\n\
and types of system documentation that may be written in your\n\
preferred language.  When such are found, they will be used instead\n\
of the english versions.",		/* prompt */
"Press F1 for more information",	/* help line */
"language.hlp",			/* help file */
{ { "Danish", "Danish language and character set (ISO-8859-1)",   /* D */
	DMENU_CALL, (void *)lang_set_Danish, 0 },
  { "Dutch", "Dutch language and character set (ISO-8859-1)",	  /* D */
	DMENU_CALL, (void *)lang_set_Dutch, 0 },
  { "English", "English language (system default)",               /* E */
	DMENU_CALL, (void *)lang_set_English, 0 },
  { "French", "French language and character set (ISO-8859-1)",   /* F */
	DMENU_CALL, (void *)lang_set_French, 0 },
  { "German", "German language and character set (ISO-8859-1)",   /* G */
	DMENU_CALL, (void *)lang_set_German, 0 },
  { "Italian", "Italian language and character set (ISO-8859-1)", /* I */
	DMENU_CALL, (void *)lang_set_Italian, 0 },
  { "Japanese", "Japanese language and default character set (romaji)",/* J */
	DMENU_CALL, (void *)lang_set_Japanese, 0 },
  { "Norwegian", "Norwegian language and character set (ISO-8859-1)", /* N */
	DMENU_CALL, (void *)lang_set_Norwegian, 0 },
  { "Russian", "Russian language and character set (cp866-8x14)", /* R */
	DMENU_CALL, (void *)lang_set_Russian, 0 },
  { "Spanish", "Spanish language and character set (ISO-8859-1)", /* S */
	DMENU_CALL, (void *)lang_set_Spanish, 0 },
  { "Swedish", "Swedish language and character set (ISO-8859-1)", /* S */
	DMENU_CALL, (void *)lang_set_Swedish, 0 },
  { NULL } },
};

DMenu MenuMediaCDROM = {
DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
"Choose a CDROM type",
"FreeBSD can be installed directly from a CDROM containing a valid\n\
FreeBSD 2.0.5 distribution.  If you're seeing this menu, it's because\n\
your CDROM drive was not properly auto-detected, or you did not launch\n\
this installation from the CD under DOS or Windows.  If you think you're\n\
seeing this dialog in error, you may wish to reboot FreeBSD with the\n\
-c boot flag (.. boot: /kernel -c) and check that your hardware and\n
the kernel agree on reasonable values.", 
"Press F1 for more information on CDROM support",
"media_cdrom.hlp",
{ { "Matsushita", "Panasonic \"Sound Blaster\" CDROM.",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/matcd0a", 0 },
  { "Mitsumi", "Mitsumi FX-001 series drive (not IDE)",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/mcd0a", 0 },
  { "SCSI", "SCSI CDROM drive attached to supported SCSI controller",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/cd0a", 0 },
  { "Sony", "Sony CDU31/33A or compatible CDROM drive",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/scd0a", 0 },
  { NULL } },
};

DMenu MenuMediaFTP = {
DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
"Please specify an FTP site",
"FreeBSD is distributed from a number of sites on the Internet.\n\
Please select the site closest to you or \"other\" if you'd like\n\
to specify another choice.  Also note that not all sites carry\n\
every possible distribution!  Distributions other than the basic\n\
binary set are only guaranteed to be available from the Primary site.\n\
If the first site selected doesn't respond, try one of the alternates.",
"Select a site that's close!",
"media_ftp.hlp",
{ { "Primary",  "ftp.freebsd.org",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.freebsd.org/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Secondary", "freefall.cdrom.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://freefall.cdrom.com/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Australia", "ftp.physics.usyd.edu.au",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.physics.usyd.edu.au/FreeBSD/2.0.5-ALPHA", 0 },
  { "Finland", "nic.funet.fi",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://nic.funet.fi/pub/unix/FreeBSD/2.0.5-ALPHA", 0 },
  { "France", "ftp.ibp.fr", 
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.ibp.fr/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Germany", "ftp.uni-duisburg.de",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.uni-duisburg.de/pub/unix/FreeBSD/2.0.5-ALPHA", 0 },
  { "Israel", "orgchem.weizmann.ac.il",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://orgchem.weizmann.ac.il/pub/FreeBSD-2.0.5-ALPHA", 0 },
  { "Japan", "ftp.sra.co.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.sra.co.jp/pub/os/FreeBSD/distribution/2.0.5-ALPHA", 0 },
  { "Japan-2", "ftp.mei.co.jp", 
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.mei.co.jp/free/PC-UNIX/FreeBSD/2.0.5-ALPHA", 0 },
  { "Japan-3", "ftp.waseda.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.waseda.ac.jp/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Japan-4", "ftp.pu-toyama.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.pu-toyama.ac.jp/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Japan-5", "ftpsv1.u-aizu.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftpsv1.u-aizu.ac.jp/pub/os/FreeBSD/2.0.5-ALPHA", 0 },
  { "Japan-6", "tutserver.tutcc.tut.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://tutserver.tutcc.tut.ac.jp/FreeBSD/FreeBSD-2.0.5-ALPHA", 0 },
  { "Japan-7", "ftp.ee.uec.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.ee.uec.ac.jp/pub/os/FreeBSD.other/FreeBSD-2.0.5-ALPHA", 0 },
  { "Korea", "ftp.cau.ac.kr",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.cau.ac.kr/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Netherlands", "ftp.nl.net",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.nl.net/pub/os/FreeBSD/2.0.5-ALPHA", 0 },
  { "Russia", "ftp.kiae.su",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.kiae.su/FreeBSD/2.0.5-ALPHA", 0 },
  { "Sweden", "ftp.luth.se",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.luth.se/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Taiwan", "netbsd.csie.nctu.edu.tw",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://netbsd.csie.nctu.edu.tw/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "Thailand", "ftp.nectec.or.th",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.nectec.or.th/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "UK", "ftp.demon.co.uk",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.demon.co.uk/pub/BSD/FreeBSD/2.0.5-ALPHA", 0 },
  { "UK-2", "src.doc.ic.ac.uk",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://src.doc.ic.ac.uk/packages/unix/FreeBSD/2.0.5-ALPHA", 0 },
  { "UK-3", "unix.hensa.ac.uk",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://unix.hensa.ac.uk/pub/walnut.creek/FreeBSD/2.0.5-ALPHA", 0 },
  { "USA", "ref.tfs.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ref.tfs.com/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "USA-2", "ftp.dataplex.net",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.dataplex.net/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "USA-3", "kryten.atinc.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://kryten.atinc.com/pub/FreeBSD/2.0.5-ALPHA", 0 },
  { "USA-4", "ftp.neosoft.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.neosoft.com/systems/FreeBSD/2.0.5-ALPHA", 0 },
  { NULL } }
};

/* The media selection menu */
DMenu MenuMedia = {
DMENU_NORMAL_TYPE,
"Choose Installation Media",
"FreeBSD can be installed from a variety of different installation\n\
media, ranging from floppies to the Internet.  If you're installing\n\
FreeBSD from a supported CDROM drive then this is generally the best\n\
method to use unless you have some overriding reason for using another\n\
method. Please also note that the DES distribution is NOT available on \n\
CDROM due to U.S. export restrictions.",
"Press F1 for more information on the various media types",
"media.hlp",
{ { "CDROM", "Install from a FreeBSD CDROM",
	DMENU_CALL, (void *)mediaSetCDROM, 0 },
  { "FLOPPY", "Install from a floppy disk set",
	DMENU_CALL, (void *)mediaSetFloppy, 0 },
  { "DOS", "Install from a DOS partition",
	DMENU_CALL, (void *)mediaSetDOS, 0 },
  { "TAPE", "Install from SCSI or QIC tape",
	DMENU_CALL, (void *)mediaSetTape, 0 },
  { "FTP", "Install from an Internet FTP server",
	DMENU_CALL, (void *)mediaSetFTP, 0 },
  { "FILESYSTEM", "Install from a UFS or NFS mounted distribution",
	DMENU_CALL, (void *)mediaSetFS, 0 },
  { NULL } },
};

/* The installation type menu */
DMenu MenuInstallType = {
DMENU_NORMAL_TYPE,
"Choose Installation Type",
"As a convenience, we provide several `canned' installation types. \
These pick what we consider to be the most reasonable defaults for the \
type of system in question.  If you would prefer to pick and choose \
the list of distributions yourself, simply select `custom'.",
"Press F1 for more information on the various distributions",
"dist_types.hlp",
{ { "Developer", "Includes full sources, binaries and doc but no games.",
	DMENU_CALL, (void *)distSetDeveloper, 0 },
  { "X-Developer", "Same as above, but includes XFree86.",
	DMENU_CALL, (void *)distSetXDeveloper, 0 },
  { "User", "General user.  Binaries and doc but no sources.",
	DMENU_CALL, (void *)distSetUser, 0 },
  { "X-User", "Same as above, but includes XFree86.",
	DMENU_CALL, (void *)distSetXUser, 0 },
  { "Minimal", "The smallest configuration possible.",
	DMENU_CALL, (void *)distSetMinimum, 0 },
  { "Everything", "The entire source and binary distribution.",
	DMENU_CALL, (void *)distSetEverything, 0 },
  { "Custom", "Specify your own distribution set",
	DMENU_SUBMENU, (void *)&MenuDistributions, 0 },
  { NULL } },
};

DMenu MenuDistributions = {
DMENU_MULTIPLE_TYPE,
"Select the distributions you wish to install.",
"Please check off the distributions you wish to install.",
"Press F1 for a more complete description of these distributions.",
"distribution_types.hlp",
{ { "*bin", "Binary base distribution (required)",
	DMENU_NOP, NULL, 0 },
  { "commercial", "Commercial demos and shareware",
	DMENU_NOP, NULL, 0 },
  { "compat1x", "FreeBSD 1.x binary compatability package",
	DMENU_NOP, NULL, 0 },
  { "DES", "DES encryption code and sources",
	DMENU_NOP, NULL, 0 },
  { "dict", "Spelling checker disctionary files",
	DMENU_NOP, NULL, 0 },
  { "games", "Games and other amusements (non-commercial)",
	DMENU_NOP, NULL, 0 },
  { "info", "GNU info files",
	DMENU_NOP, NULL, 0 },
  { "man", "System manual pages - strongly recommended",
	DMENU_NOP, NULL, 0 },
  { "proflibs", "Profiled versions of the libraries",
	DMENU_NOP, NULL, 0 },
  { "src", "Sources for everything but DES",
	DMENU_NOP, NULL, 0 },
  { "XFree86", "The XFree86 3.1.1 distribution",
	DMENU_NOP, NULL, 0 },
  { NULL } },
};

DMenu MenuDiskDevices = {
DMENU_MULTIPLE_TYPE,
"Select Drive(s)",
"Please select the drive, or drives, on which you wish to install\n\
FreeBSD.  You need to select at least one drive containing some free\n\
space, though FreeBSD can be installed across several drives if you do\n\
not have the required space on a single drive.  If you wish to boot\n\
off a drive that's not a `zero drive', or have multiple operating\n\
systems on your machine, you will have the option to install a boot\n\
manager later.",
"Press F1 for more information on what you see here.",
"drives.hlp",
{ { NULL } },
};

/* The installation options menu */
DMenu MenuInstallOptions = {
DMENU_NORMAL_TYPE,
"Choose Installation Options",
"blah blah",
NULL,
NULL,
{ { NULL } },
};

/* The main installation menu */
DMenu MenuInstall = {
DMENU_NORMAL_TYPE,
"Choose Installation Options",		/* title */
"Before installation can continue, you need to specify a few items\n\
of information regarding the location of the distribution and the kind\n\
of installation you want to have (and where).  There are also a number\n\
of options you can specify in the Options menu.  If you do not wish to\n\
install FreeBSD at this time, you may select Cancel to leave this menu",
"You may also wish to read the install guide - press F1 to do so",
"install.hlp",
{ { "Media", "Choose Installation media type",		/* M */
	DMENU_SUBMENU, (void *)&MenuMedia, 0 },
  { "Type", "Choose the type of installation you want",	/* T */
	DMENU_SUBMENU, (void *)&MenuInstallType, 0 },
  { "Options", "Specify installation options",		/* O */
	DMENU_SUBMENU, (void *)&MenuInstallOptions, 0 },
  { "Proceed", "Proceed with installation",			/* P */
	DMENU_CANCEL, (void *)NULL, 0 },
  { NULL } },
};

