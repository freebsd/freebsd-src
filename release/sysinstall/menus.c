/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: menus.c,v 1.16 1995/05/18 09:02:00 jkh Exp $
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

/* The initial installation menu */
DMenu MenuInitial = {
    DMENU_NORMAL_TYPE,
    "Welcome to FreeBSD 2.0.5!",	/* title */
    "This is the main menu of the FreeBSD installation system.  Please\n\
select one of the options below by using the arrow keys or typing the\n\
first character of the option name you're interested in.  Invoke an\n\
option by pressing enter.  If you'd like a shell, press ESC",	/* prompt */
    "Press F1 for usage instructions",				/* help line */
    "usage.hlp",						/* help file */
    { { "Usage", "Quick start - How to use this menu system.",	/* U */
	DMENU_DISPLAY_FILE, (void *)"usage.hlp", 0, 0	},
      { "Doc", "More detailed documentation on FreeBSD.",	/* D */
	DMENU_SUBMENU, (void *)&MenuDocumentation, 0, 0 },
      { "Language", "Set your preferred language.",		/* L */
	DMENU_SUBMENU, (void *)&MenuOptionsLanguage, 0, 0 },
      { "Options", "Select various options for this utility.",	/* O */
	DMENU_SUBMENU, (void *)&MenuOptions, 0, 0	},
      { "Proceed", "Go to the installation menu",		/* P */
	DMENU_SUBMENU, (void *)&MenuInstall, 0, 0		},
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
    { { "README", "Read this for a general description of FreeBSD",	/* R */
	DMENU_DISPLAY_FILE, (void *)"README", 0, 0		},
      { "Hardware", "The FreeBSD survival guide for PC hardware.",	/* H */
	DMENU_DISPLAY_FILE, (void *)"hardware.hlp", 0, 0	},
      { "Install", "A step-by-step guide to installing FreeBSD.",	/* I */
	DMENU_DISPLAY_FILE, (void *)"install.hlp", 0, 0		},
      { "Copyright", "The FreeBSD Copyright notices.",   		/* C */
	DMENU_DISPLAY_FILE, (void *)"COPYRIGHT", 0, 0		},
      { "Release", "The release notes for this version of FreeBSD.",	/* R */
	DMENU_DISPLAY_FILE, (void *)"COPYRIGHT", 0, 0		},
      { "FAQ", "Frequently Asked Questions about FreeBSD.",		/* F */
	DMENU_DISPLAY_FILE, (void *)"faq.hlp", 0, 0		},
      { NULL } },
};

/*
 * The language selection menu.
 *
 * Note:  The RADIO menus use a slightly different syntax.  If an item
 * name starts with `*', it's considered to be "ON" by default,
 * otherwise off.
 */
DMenu MenuOptionsLanguage = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Natural language selection",	/* title */
    "Please specify the language you'd like to use by default.\n\n\
While almost all of the system's documentation is still written\n\
in english (and may never be translated), there are a few guides\n\
and types of system documentation that may be written in your\n\
preferred language.  When such are found, they will be used instead\n\
of the english versions.  This feature is nonetheless considered\n\
to be in experimental status at this time.",		/* prompt */
    "Press F1 for more information",	/* help line */
    "language.hlp",			/* help file */
    { { "Danish", "Danish language and character set (ISO-8859-1)",	/* D */
	DMENU_CALL, (void *)lang_set_Danish, 0, 0	},
      { "Dutch", "Dutch language and character set (ISO-8859-1)",	/* D */
	DMENU_CALL, (void *)lang_set_Dutch, 0, 0	},
      { "English", "English language (system default)",			/* E */
	DMENU_CALL, (void *)lang_set_English, 0, 0	},
      { "French", "French language and character set (ISO-8859-1)",	/* F */
	DMENU_CALL, (void *)lang_set_French, 0, 0	},
      { "German", "German language and character set (ISO-8859-1)",	/* G */
	DMENU_CALL, (void *)lang_set_German, 0, 0	},
      { "Italian", "Italian language and character set (ISO-8859-1)",	/* I */
	DMENU_CALL, (void *)lang_set_Italian, 0, 0	},
      { "Japanese", "Japanese language and default character set (romaji)", /* J */
	DMENU_CALL, (void *)lang_set_Japanese, 0, 0	},
      { "Norwegian", "Norwegian language and character set (ISO-8859-1)", /* N */
	DMENU_CALL, (void *)lang_set_Norwegian, 0, 0	},
      { "Russian", "Russian language and character set (CP866)",	/* R */
	DMENU_CALL, (void *)lang_set_Russian, 0, 0	},
      { "Spanish", "Spanish language and character set (ISO-8859-1)",	/* S */
	DMENU_CALL, (void *)lang_set_Spanish, 0, 0	},
      { "Swedish", "Swedish language and character set (ISO-8859-1)",	/* S */
	DMENU_CALL, (void *)lang_set_Swedish, 0, 0	},
      { NULL } },
};

DMenu MenuMediaCDROM = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a CDROM type",
    "FreeBSD can be installed directly from a CDROM containing a valid\n\
FreeBSD 2.0.5 distribution.  If you are seeing this menu, it's either\n\
because you haven't booted directly from the CDROM in DOS/Windows or\n\
your CDROM was not detected.  If you feel that you are seeing this dialog\n\
in error, you may wish to reboot FreeBSD with the -c boot flag (see the\n\
hardware guide in the Documentation menu for more info) and check that your\n\
CDROM controller and the kernel agree on reasonable values.  Please also\n\
note that FreeBSD does NOT currently support IDE CDROM drives!",
    "Press F1 for more information on CDROM support",
    "media_cdrom.hlp",
    { { "Matsushita", "Panasonic \"Sound Blaster\" CDROM.",		/* M */
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/matcd0a", 0, 0	},
      { "Mitsumi", "Mitsumi FX-001 series drive (not IDE)",		/* M */
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/mcd0a", 0, 0	},
      { "SCSI", "SCSI CDROM drive attached to supported SCSI controller", /* S */
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/cd0a", 0, 0	},
      { "Sony", "Sony CDU31/33A or compatible CDROM drive",		/* S */
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/scd0a", 0, 0	},
      { NULL } },
};

DMenu MenuMediaFloppy = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose a Floppy drive",
    "FreeBSD can also be installed from floppy disk media,\n\
though not without some pain.  You should have prepared your\n\
floppy distribution media using the DOS floppy install-set\n\
construction procedure or have otherwise prepared a set of\n\
diskettes for each distribution that properly contains all the\n\
components of the distribution plus the extraction and checksumming\n\
scripts.",
    "Please select the floppy drive you want to use",
    NULL,
    { { "A", "Floppy drive A",		/* M */
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/fd0a", 0, 0	},
      { "B", "Floppy drive B",		/* M */
	DMENU_SET_VARIABLE, (void *)"mediaDevice=/dev/fd1a", 0, 0	},
      { NULL } },
};

DMenu MenuMediaFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please specify an FTP site",
    "FreeBSD is distributed from a number of sites on the Internet. Please\n\
select the site closest to you or \"other\" if you'd like to specify another\n\
choice.  Also note that not all sites carry every possible distribution!\n\
Distributions other than the basic user set are only guaranteed to be\n\
available from the Primary site.\n\n\
If the first site selected doesn't respond, try one of the alternates.\n\
You may also wish to investigate the Ftp options menu in case of trouble.",
    "Select a site that's close!",
    "media_ftp.hlp",
    { { "Primary",  "ftp.freebsd.org",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.freebsd.org/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Secondary", "freefall.cdrom.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://freefall.cdrom.com/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Australia", "ftp.physics.usyd.edu.au",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.physics.usyd.edu.au/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Finland", "nic.funet.fi",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://nic.funet.fi/pub/unix/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "France", "ftp.ibp.fr", 
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.ibp.fr/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Germany", "ftp.uni-duisburg.de",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.uni-duisburg.de/pub/unix/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Israel", "orgchem.weizmann.ac.il",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://orgchem.weizmann.ac.il/pub/FreeBSD-2.0.5-ALPHA", 0, 0 },
      { "Japan", "ftp.sra.co.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.sra.co.jp/pub/os/FreeBSD/distribution/2.0.5-ALPHA", 0, 0 },
      { "Japan-2", "ftp.mei.co.jp", 
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.mei.co.jp/free/PC-UNIX/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Japan-3", "ftp.waseda.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.waseda.ac.jp/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Japan-4", "ftp.pu-toyama.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.pu-toyama.ac.jp/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Japan-5", "ftpsv1.u-aizu.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftpsv1.u-aizu.ac.jp/pub/os/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Japan-6", "tutserver.tutcc.tut.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://tutserver.tutcc.tut.ac.jp/FreeBSD/FreeBSD-2.0.5-ALPHA", 0, 0 },
      { "Japan-7", "ftp.ee.uec.ac.jp",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.ee.uec.ac.jp/pub/os/FreeBSD.other/FreeBSD-2.0.5-ALPHA", 0, 0 },
      { "Korea", "ftp.cau.ac.kr",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.cau.ac.kr/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Netherlands", "ftp.nl.net",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.nl.net/pub/os/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Russia", "ftp.kiae.su",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.kiae.su/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Sweden", "ftp.luth.se",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.luth.se/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Taiwan", "netbsd.csie.nctu.edu.tw",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://netbsd.csie.nctu.edu.tw/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "Thailand", "ftp.nectec.or.th",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.nectec.or.th/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "UK", "ftp.demon.co.uk",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.demon.co.uk/pub/BSD/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "UK-2", "src.doc.ic.ac.uk",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://src.doc.ic.ac.uk/packages/unix/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "UK-3", "unix.hensa.ac.uk",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://unix.hensa.ac.uk/pub/walnut.creek/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "USA", "ref.tfs.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ref.tfs.com/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "USA-2", "ftp.dataplex.net",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.dataplex.net/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "USA-3", "kryten.atinc.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://kryten.atinc.com/pub/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { "USA-4", "ftp.neosoft.com",
	DMENU_SET_VARIABLE, (void *)"mediaDevice=ftp://ftp.neosoft.com/systems/FreeBSD/2.0.5-ALPHA", 0, 0 },
      { NULL } }
};

/* The media selection menu */
DMenu MenuMedia = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Media",
    "FreeBSD can be installed from a variety of different installation\n\
media, ranging from floppies to the Internet.  If you're installing\n\
FreeBSD from a supported CDROM drive then this is generally the best\n\
method to use unless you have some overriding reason for using another\n\
method.",
    "Press F1 for more information on the various media types",
    "media.hlp",
    { { "CDROM", "Install from a FreeBSD CDROM",
	DMENU_CALL, (void *)mediaSetCDROM, 0, 0		},
      { "DOS", "Install from a DOS partition",
	DMENU_CALL, (void *)mediaSetDOS, 0, 0		},
      { "File System", "Install from a UFS or NFS mounted distribution",
	DMENU_CALL, (void *)mediaSetFS, 0, 0		},
      { "Floppy", "Install from a floppy disk set",
	DMENU_CALL, (void *)mediaSetFloppy, 0, 0	},
      { "FTP", "Install from an Internet FTP server",
	DMENU_CALL, (void *)mediaSetFTP, 0, 0		},
      { "Tape", "Install from SCSI or QIC tape",
	DMENU_CALL, (void *)mediaSetTape, 0, 0		},
      { NULL } },
};

/* The installation type menu */
DMenu MenuInstallType = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose Installation Type",
    "As a convenience, we provide several \"canned\" installation types.\n\
These select what we consider to be the most reasonable defaults for the\n\
type of system in question.  If you would prefer to pick and choose\n\
the list of distributions yourself, simply select \"custom\".",
    "Press F1 for more information on the various distributions",
    "dist_types.hlp",
    { { "Developer", "Full sources, binaries and doc but no games [48MB]",
	DMENU_CALL, (void *)distSetDeveloper, 0, 0	},
      { "X-Developer", "Same as above, but includes XFree86 [73MB]",
	DMENU_CALL, (void *)distSetXDeveloper, 0, 0	},
      { "User", "Average user - binaries and doc but no sources [19MB]",
	DMENU_CALL, (void *)distSetUser, 0, 0		},
      { "X-User", "Same as above, but includes XFree86 [45MB]",
	DMENU_CALL, (void *)distSetXUser, 0, 0		},
      { "Minimal", "The smallest configuration possible [15MB]",
	DMENU_CALL, (void *)distSetMinimum, 0, 0	},
      { "Everything", "All sources, binaries and XFree86 binaries [80MB]",
	DMENU_CALL, (void *)distSetEverything, 0, 0	},
      { "Custom", "Specify your own distribution set [?]",
	DMENU_SUBMENU, (void *)&MenuDistributions, 0, 0	},
      { NULL } },
};

DMenu MenuDistributions = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
    "Select the distributions you wish to install.",
    "Please check off the distributions you wish to install.  Some\n
of the most generally useful distributions are already checked, and\n\
selecting OK at this stage will chose them as defaults.",
    "Press F1 for a more complete description of these distributions.",
    "distribution_types.hlp",
    { { "*bin", "Binary base distribution (required) [15MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_BIN, 0		},
      { "commercial", "Commercial demos and shareware [10MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_COMMERCIAL, 0	},
      { "compat1x", "FreeBSD 1.x binary compatability package [2MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_COMPAT1X, 0	},
      { "compat20", "FreeBSD 2.0 binary compatability package [2MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_COMPAT20, 0	},
      { "DES", "DES encryption code and sources [.3MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_DES, 0		},
      { "dict", "Spelling checker disctionary files [1.3MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_DICT, 0		},
      { "games", "Games and other amusements (non-commercial) [2.8MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_GAMES, 0		},
      { "info", "GNU info files [1.2MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_INFO, 0		},
      { "*man", "System manual pages - strongly recommended [2.8MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_MANPAGES, 0	},
      { "proflibs", "Profiled versions of the libraries [.9MB]",
	DMENU_SET_FLAG, (void *)&Dists, DIST_PROFLIBS, 0	},
      { "src", "Sources for everything but DES [25MB]",
	DMENU_CALL, (void *)distSetSrc, 0			},
      { "XFree86", "The XFree86 3.1.1L distribution [?]",
	DMENU_SUBMENU, (void *)&MenuXF86, 0			},
      { NULL } },
};

DMenu MenuSrcDistributions = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
    "Select the sub-components of src you wish to install.",
    "Please check off those portions of the FreeBSD source tree\n\
you wish to install.",
    "Press F1 for a more complete description of distributions.",
    "distribution_types.hlp",
    { { "base", "top-level files in /usr/src [300K]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_BASE, 0	},
      { "gnu", "/usr/src/gnu (software from the GNU Project) [42MB]]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_GNU, 0	},
      { "etc", "/usr/src/etc (miscellaneous system files) [460K]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_ETC, 0	},
      { "games", "/usr/src/games (diversions) [7.8MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_GAMES, 0	},
      { "include", "/usr/src/include (header files) [467K]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_INCLUDE, 0	},
      { "lib", "/usr/src/lib (system libraries) [9.2MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_LIB, 0	},
      { "libexec", "/usr/src/libexec (system programs) [1.2MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_LIBEXEC, 0	},
      { "lkm", "/usr/src/lkm (Loadable Kernel Modules) [193K]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_LKM, 0	},
      { "release", "/usr/src/release (release-generation tools) [533K]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_RELEASE, 0	},
      { "sbin", "/usr/src/sbin (system binaries) [1.3MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_SBIN, 0	},
      { "share", "/usr/src/share (documents and shared files) [10MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_SHARE, 0	},
      { "sys", "/usr/src/sys (FreeBSD kernel) [13MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_SYS, 0	},
      { "ubin", "/usr/src/usr.bin (user binaries) [13MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_UBIN, 0	},
      { "usbin", "/usr/src/usr.sbin (aux system binaries) [14MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_USBIN, 0	},
      { "XFree86", "XFree86 3.1.1L source + contrib distribution [200MB]",
	DMENU_SET_FLAG, (void *)&SrcDists, DIST_SRC_XF86, 0	},
      { NULL } },
};

DMenu MenuXF86 = {
    DMENU_NORMAL_TYPE,
    "XFree86 3.1.1u1 Distribution",
    "Welcome to the XFree86 3.1.1u1 distribution from The XFree86\n\
Project, Inc.  Our recommended sequence is to Select the desired\n\
release components, Configure XFree86 and then (optionally)\n\
Start it up!",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XFree86.hlp",
    { { "Select", "Select and load components of the XFree86 distribution",
	DMENU_SUBMENU, &MenuXF86Select, 0, 0 },
      { "Configure", "Configure an installed XFree86 distribution",
	DMENU_SYSTEM_COMMAND, "PATH=/usr/bin:/bin:/usr/X11R6/bin xf86config",
	0, 0 },
      { "Start", "Try to start the server up",
	DMENU_SYSTEM_COMMAND, "PATH=/usr/bin:/bin:/usr/X11R6/bin startx",
	0, 0 },
      { NULL } }
};

DMenu MenuXF86Select = {
    DMENU_NORMAL_TYPE,
    "XFree86 3.1.1u1 Distribution",
    "Please select the components you need from the XFree86 3.1.1u1\n\
distribution.  We recommend that you select what you need from the basic\n\
components set and at least one entry from the Server and Font set menus.\n\n\
When you're finished, select Cancel.",
    "Press F1 to read the XFree86 release notes for FreeBSD",
    "XF86S.hlp",
    { { "Basic", "Basic component menu (required)",
	DMENU_SUBMENU, &MenuXF86SelectCore, 0, 0	},
      { "Server", "X server menu", 
	DMENU_SUBMENU, &MenuXF86SelectServer, 0, 0	},
      { "Fonts", "Font set menu",
	DMENU_SUBMENU, &MenuXF86SelectFonts, 0, 0	},
      { NULL } },
};

DMenu MenuXF86SelectCore = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
    "XFree86 3.1.1 base distribution types",
    "Please check off the basic XFree86 components you wish to install.\n\
Those deemed most generally useful are already checked off for you.",
    NULL,
    NULL,
    { { "*bin", "X client applications and shared libs [4MB].",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_BIN, 0	},
      { "*lib", "Data files needed at runtime [600K]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_LIB, 0	},
      { "xicf", "Customizable xinit runtime configuration file [100K]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_XINIT, 0	},
      { "xdcf", "Customizable xdm runtime configuration file [100K]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_XDMCF, 0	},
      { "doc", "READMEs and XFree86 specific man pages [500K]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_DOC, 0	},
      { "*man", "Man pages (except XFree86 specific ones) [1.2MB]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_MAN, 0	},
      { "prog", "Programmer's header and library files [4MB]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_PROG, 0	},
      { "link", "X Server reconfiguration kit [7.8MB]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_LINK, 0	},
      { "pex", "PEX fonts and libs needed by PEX apps [500K]",
	DMENU_SET_FLAG, (void *)&XF86Dists, DIST_XF86_PEX, 0	},
      { NULL } },
};

DMenu MenuXF86SelectFonts = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
    "Font distribution selection.",
    "Please check off the individual font distributions you wish to\n\
install.  At the minimum, you should certainly install the standard\n\
75 DPI and misc fonts if you're also installing a server.",
    NULL,
    NULL,
    { { "*fnts", "Standard 75 DPI and miscellaneous fonts [3.6MB]",
	DMENU_SET_FLAG, (void *)&XF86FontDists, DIST_XF86_FONTS_MISC, 0	},
      { "f100", "100 DPI fonts [1.8MB]",
	DMENU_SET_FLAG, (void *)&XF86FontDists, DIST_XF86_FONTS_100, 0	},
      { "fscl", "Speedo and Type scalable fonts [1.6MB]",
	DMENU_SET_FLAG, (void *)&XF86FontDists, DIST_XF86_FONTS_SCALE, 0 },
      { "non", "Japanese, Chinese and other non-english fonts [3.3MB]",
	DMENU_SET_FLAG, (void *)&XF86FontDists, DIST_XF86_FONTS_NON, 0	},
      { "server", "Font server [0.3MB]",
	DMENU_SET_FLAG, (void *)&XF86FontDists, DIST_XF86_FONTS_SERVER, 0 },
      { NULL } },
};

DMenu MenuXF86SelectServer = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
    "X Server selection.",
    "Please check off the types of X servers you wish to install.\n\
If you are unsure as which server will work for your graphics card,\n\
it is recommended that try the SVGA or VGA16 servers (the VGA16 and\n\
Mono servers are also particularly well-suited to most LCD displays).",
    "xservers.hlp",
    "Press F1 for more information on the various X server types",
    { { "*SVGA", "Standard VGA or Super VGA display [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_SVGA, 0 },
      { "VGA16", "Standard 16 color VGA display [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_VGA16, 0 },
      { "Mono", "Standard Monochrome display [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_MONO, 0 },
      { "8514", "8-bit (256 color) IBM 8514 or compatible card [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_8514, 0 },
      { "AGX", "8-bit AGX card [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_AGX, 0 },
      { "Mch3", "8 and 16-bit (65K color) for ATI Mach32 card [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_MACH32, 0 },
      { "Mch8", "8-bit ATI Mach8 card [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_MACH8, 0 },
      { "P9K", "8, 16, and 24-bit color for Weitek P9000 based boards [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_P9000, 0 },
      { "S3", "8, 16 and 24-bit color for S3 based boards [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_S3, 0 },
      { "W32", "8-bit Color for ET4000/W32, /W32i and /W32p cards [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_W32, 0 },
      { "nest", "A nested server for testing purposes [1MB]",
	DMENU_SET_FLAG, (void *)&XF86ServerDists, DIST_XF86_SERVER_NEST, 0 },
      { NULL } },
};

DMenu MenuDiskDevices = {
    DMENU_MULTIPLE_TYPE | DMENU_SELECTION_RETURNS,
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
DMenu MenuOptions = {
    DMENU_NORMAL_TYPE,
    "Choose Installation Options",
    "This menu controls how the FreeBSD installation will deal with various\n\
error conditions (should they arise), the default language used for\n\
documentation, etc.\n\n\
When you're done setting options, please select Cancel.",
    NULL,
    NULL,
    { { "Ftp Options", "Ftp options menu",
	DMENU_SUBMENU, (void *)&MenuOptionsFTP, 0, 0		},
      { "NFS Secure", "NFS server talks only on a secure port",
	DMENU_SET_VARIABLE, (void *)"nfsServerSecure=yes", 0, 0	},
      { "NFS Slow", "User is using a slow PC or ethernet card",
	DMENU_SET_VARIABLE, (void *)"nfsSlowPC=yes", 0, 0	},
      { "Extra Debugging", "Toggle the extra debugging flag",
	DMENU_SET_VARIABLE, (void *)"debug=yes", 0, 0		},
      { "No Debugging", "Turn the extra debugging flag off",
	DMENU_SET_VARIABLE, (void *)"debug=no", 0, 0		},
      { NULL } },
};

DMenu MenuOptionsFTP = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Choose FTP Options",
    "In case of ftp failure, how would you like this installation\n\
to deal with it?  You have several choices:",
    NULL,
    NULL,
    { { "FTP Retry", "On transfer failure, retry same host",
	DMENU_SET_VARIABLE, (void *)"ftpRetryType=loop", 0, 0 },
      { "FTP Reselect", "On transfer failure, ask for another host",
	DMENU_SET_VARIABLE, (void *)"ftpRetryType=reselect", 0, 0 },
      { "FTP Abort", "On transfer failure, abort installation",
	DMENU_SET_VARIABLE, (void *)"ftpRetryType=abort", 0, 0 },
      { "FTP passive", "Use \"passive mode\" for firewalled FTP",
	DMENU_SET_VARIABLE, (void *)"ftpPassive=yes", 0, 0 },
      { NULL } },
};

/* The main installation menu */
DMenu MenuInstall = {
    DMENU_NORMAL_TYPE,
    "Choose Installation Options",		/* title */
    "Before installation can continue, you need to specify a few\n\
details on the type of distribution you wish to have, where you wish\n\
to install it from, and how you wish to allocate disk storage to FreeBSD\n\
None of the items in this menu will actually modify the contents of\n\
your disk until you select the \"GO!\" menu item (and even then, only\n\
after a final confirmation).  If you do not wish to install FreeBSD\n\
at this time then select Cancel to leave this menu.",
    "Press F1 to read the installation guide",
    "install.hlp",
    { { "Distributions", "Choose the type of installation you want", /* T */
	DMENU_SUBMENU, (void *)&MenuInstallType, 0, 0	},
      { "Media", "Choose the installation media type",		/* M */
	DMENU_SUBMENU, (void *)&MenuMedia, 0, 0		},
      { "Partition", "Allocate disk space for FreeBSD",		/* P */
	DMENU_CALL, (void *)diskPartitionEditor, 0, 0	},
      { "Label", "Label allocated disk partitions",		/* L */
	DMENU_CALL, (void *)diskLabelEditor, 0, 0	},
      { "Networking", "Configure any network interfaces",	/* N */
	DMENU_CALL, (void *)tcpOpenDialog, 0, 0		},
      { "GO!", "Start the whole show and go out for coffee!",	/* P */
	DMENU_CALL, (void *)installCommit, 0, 0		},
      { NULL } },
};
