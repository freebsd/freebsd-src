#-*- mode: Fundamental; tab-width: 4; -*-
# ex:ts=4
#
#	$Id: bsd.port.mk,v 1.259 1997/06/18 03:20:22 asami Exp $
#	$NetBSD: $
#
#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# Please view me with 4 column tabs!

# There are two different types of "maintainers" in the whole ports
# framework concept.  Maintainers of the bsd.port*.mk files
# are listed below in the ${OSNAME}_MAINTAINER entries (this file
# is used by multiple *BSD flavors).  You should consult them directly
# if you have any questions/suggestions regarding this file since only
# they are allowed to modify the master copies in the CVS repository!

# For each port, the MAINTAINER variable is what you should consult for
# contact information on the person(s) to contact if you have questions/
# suggestions about that specific port.  By default (if no MAINTAINER
# is listed), a port is maintained by the subscribers of the ports@freebsd.org
# mailing list, and any correspondece should be directed there.
#
FreeBSD_MAINTAINER=	asami@FreeBSD.ORG
OpenBSD_MAINTAINER=	imp@OpenBSD.ORG

# Supported Variables and their behaviors:
#
# Variables that typically apply to all ports:
# 
# OPSYS			- Portability clause.  This is the operating system the
#				  makefile is being used on.  Automatically set to
#				  "FreeBSD," "NetBSD," or "OpenBSD" as appropriate.
# PORTSDIR		- The root of the ports tree.  Defaults:
#					FreeBSD/OpenBSD: /usr/ports
#					NetBSD:          /usr/opt
# DISTDIR 		- Where to get gzip'd, tarballed copies of original sources
#				  (default: ${PORTSDIR}/distfiles).
# PREFIX		- Where to install things in general (default: /usr/local).
# MASTER_SITES	- Primary location(s) for distribution files if not found
#				  locally.
# PATCH_SITES	- Primary location(s) for distribution patch files
#				  (see PATCHFILES below) if not found locally.
#
# MASTER_SITE_BACKUP - Backup location(s) for distribution files and patch
#				  files if not found locally and ${MASTER_SITES}/${PATCH_SITES}
#				  (default:
#				  ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/${DIST_SUBDIR}/)
# MASTER_SITE_OVERRIDE - If set, override the MASTER_SITES setting with this
#				  value.
# MASTER_SITE_FREEBSD - If set, only use ${MASTER_SITE_BACKUP} for
#				  MASTER_SITES.
# PACKAGES		- A top level directory where all packages go (rather than
#				  going locally to each port). (default: ${PORTSDIR}/packages).
# GMAKE			- Set to path of GNU make if not in $PATH (default: gmake).
# XMKMF			- Set to path of `xmkmf' if not in $PATH (default: xmkmf -a ).
# MAINTAINER	- The e-mail address of the contact person for this port
#				  (default: ports@FreeBSD.ORG).
# CATEGORIES	- A list of descriptive categories into which this port falls.
#
# Variables that typically apply to an individual port.  Non-Boolean
# variables without defaults are *mandatory*.
#
# WRKDIR 		- A temporary working directory that gets *clobbered* on clean
#				  (default: ${.CURDIR}/work).
# WRKSRC		- A subdirectory of ${WRKDIR} where the distribution actually
#				  unpacks to.  (Default: ${WRKDIR}/${DISTNAME} unless
#				  NO_WRKSUBDIR is set, in which case simply ${WRKDIR}).
# DISTNAME		- Name of port or distribution.
# DISTFILES		- Name(s) of archive file(s) containing distribution
#				  (default: ${DISTNAME}${EXTRACT_SUFX}).
# PATCHFILES	- Name(s) of additional files that contain distribution
#				  patches (default: none).  make will look for them at
#				  PATCH_SITES (see above).  They will automatically be
#				  uncompressed before patching if the names end with
#				  ".gz" or ".Z".
# DIST_SUBDIR	- Suffix to ${DISTDIR}.  If set, all ${DISTFILES} 
#				  and ${PATCHFILES} will be put in this subdirectory of
#				  ${DISTDIR}.  Also they will be fetched in this subdirectory 
#				  from FreeBSD mirror sites.
# ALLFILES		- All of ${DISTFILES} and ${PATCHFILES}.
# IGNOREFILES	- If some of the ${ALLFILES} are not checksum-able, set
#				  this variable to their names.
# PKGNAME		- Name of the package file to create if the DISTNAME 
#				  isn't really relevant for the port/package
#				  (default: ${DISTNAME}).
# EXTRACT_ONLY	- If defined, a subset of ${DISTFILES} you want to
#			  	  actually extract.
# PATCHDIR 		- A directory containing any additional patches you made
#				  to port this software to FreeBSD (default:
#				  ${.CURDIR}/patches)
# SCRIPTDIR 	- A directory containing any auxiliary scripts
#				  (default: ${.CURDIR}/scripts)
# FILESDIR 		- A directory containing any miscellaneous additional files.
#				  (default: ${.CURDIR}/files)
# PKGDIR 		- A direction containing any package creation files.
#				  (default: ${.CURDIR}/pkg)
# PKG_DBDIR		- Where package installation is recorded (default: /var/db/pkg)
# FORCE_PKG_REGISTER - If set, it will overwrite any existing package
#				  registration information in ${PKG_DBDIR}/${PKGNAME}.
# NO_MTREE		- If set, will not invoke mtree from bsd.port.mk from
#				  the "install" target.
# MTREE_FILE	- The name of the mtree file (default: /etc/mtree/BSD.x11.dist
#				  if USE_IMAKE or USE_X11 is set, /etc/mtree/BSD.local.dist
#				  otherwise.)
#
# NO_BUILD		- Use a dummy (do-nothing) build target.
# NO_CONFIGURE	- Use a dummy (do-nothing) configure target.
# NO_CDROM		- Port may not go on CDROM.  Set this string to reason.
# NO_DESCRIBE	- Use a dummy (do-nothing) describe target.
# NO_EXTRACT	- Use a dummy (do-nothing) extract target.
# NO_INSTALL	- Use a dummy (do-nothing) install target.
# NO_PACKAGE	- Use a dummy (do-nothing) package target.
# NO_PKG_REGISTER - Don't register a port install as a package.
# NO_WRKSUBDIR	- Assume port unpacks directly into ${WRKDIR}.
# NO_WRKDIR		- There's no work directory at all; port does this someplace
#				  else.
# NO_DEPENDS	- Don't verify build of dependencies.
# BROKEN		- Port is broken.  Set this string to the reason why.
# RESTRICTED	- Port is restricted.  Set this string to the reason why.
# USE_GMAKE		- Says that the port uses gmake.
# USE_PERL5		- Says that the port uses perl5 for building and running.
# USE_IMAKE		- Says that the port uses imake.
# USE_X11		- Says that the port uses X11 (i.e., installs in ${X11BASE}).
# NO_INSTALL_MANPAGES - For imake ports that don't like the install.man
#						target.
# HAS_CONFIGURE	- Says that the port has its own configure script.
# GNU_CONFIGURE	- Set if you are using GNU configure (optional).
# CONFIGURE_SCRIPT - Name of configure script, defaults to 'configure'.
# CONFIGURE_ARGS - Pass these args to configure if ${HAS_CONFIGURE} is set.
# CONFIGURE_ENV - Pass these env (shell-like) to configure if
#				  ${HAS_CONFIGURE} is set.
# SCRIPTS_ENV	- Additional environment vars passed to scripts in
#                 ${SCRIPTDIR} executed by bsd.port.mk.
# MAKE_ENV		- Additional environment vars passed to sub-make in build
#				  stage.
# IS_INTERACTIVE - Set this if your port needs to interact with the user
#				  during a build.  User can then decide to skip this port by
#				  setting ${BATCH}, or compiling only the interactive ports
#				  by setting ${INTERACTIVE}.
# FETCH_DEPENDS - A list of "path:dir" pairs of other ports this
#				  package depends in the "fetch" stage.  "path" is the
#				  name of a file if it starts with a slash (/), an
#				  executable otherwise.  make will test for the
#				  existence (if it is a full pathname) or search for
#				  it in your $PATH (if it is an executable) and go
#				  into "dir" to do a "make all install" if it's not
#				  found.
# BUILD_DEPENDS - A list of "path:dir" pairs of other ports this
#				  package depends to build (between the "extract" and
#				  "build" stages, inclusive).  The test done to
#				  determine the existence of the dependency is the
#				  same as FETCH_DEPENDS.
# RUN_DEPENDS	- A list of "path:dir" pairs of other ports this
#				  package depends to run.  The test done to determine
#				  the existence of the dependency is the same as
#				  FETCH_DEPENDS.  This will be checked during the
#				  "install" stage and the name of the dependency will
#				  be put into the package as well.
# LIB_DEPENDS	- A list of "lib:dir" pairs of other ports this package
#				  depends on.  "lib" is the name of a shared library.
#				  make will use "ldconfig -r" to search for the
#				  library.  Note that lib can be any regular expression,
#				  and you need two backslashes in front of dots (.) to
#				  supress its special meaning (e.g., use
#				  "foo\\.2\\.:${PORTSDIR}/utils/foo" to match "libfoo.2.*").
# DEPENDS		- A list of other ports this package depends on being
#				  made first.  Use this for things that don't fall into
#				  the above two categories.
# EXTRACT_CMD	- Command for extracting archive (default: tar).
# EXTRACT_SUFX	- Suffix for archive names (default: .tar.gz).
# EXTRACT_BEFORE_ARGS -
#				  Arguments to ${EXTRACT_CMD} before filename
#				  (default: -C ${WRKDIR} -xzf).
# EXTRACT_AFTER_ARGS -
#				  Arguments to ${EXTRACT_CMD} following filename
#				  (default: none).
#
# FETCH_CMD		  - Full path to ftp/http fetch command if not in $PATH
#				  (default: /usr/bin/fetch).
# FETCH_BEFORE_ARGS -
#				  Arguments to ${FETCH_CMD} before filename (default: none).
# FETCH_AFTER_ARGS -
#				  Arguments to ${FETCH_CMD} following filename (default: none).
#
# Motif support:
#
# REQUIRES_MOTIF - Set this in your port if it requires Motif.  It will  be
#				  built only if HAVE_MOTIF is set.
# HAVE_MOTIF	- If set, means system has Motif.  Typically set in
#				  /etc/make.conf.
# MOTIF_STATIC	- If set, link libXm statically; otherwise, link it
#				  dynamically.  Typically set in /etc/make.conf.
# MOTIFLIB		- Set automatically to appropriate value depending on
#				  ${MOTIF_STATIC}.  Substitute references to -lXm with 
#				  patches to make your port conform to our standards.
# MOTIF_ONLY	- If set, build Motif ports only.  (Not much use except for
#				  building packages.)
#
# Variables to change if you want a special behavior:
#
# ECHO_MSG		- Used to print all the '===>' style prompts - override this
#				  to turn them off (default: /bin/echo).
# DEPENDS_TARGET - The target to execute when a port is calling a
#				  dependency (default: "install").
# PATCH_DEBUG	- If set, print out more information about the patches as
#				  it attempts to apply them.
#
# Variables that serve as convenient "aliases" for your *-install targets.
# Use these like: "${INSTALL_PROGRAM} ${WRKSRC}/prog ${PREFIX}/bin".
#
# INSTALL_PROGRAM - A command to install binary executables.
# INSTALL_SCRIPT - A command to install executable scripts.
# INSTALL_DATA	- A command to install sharable data.
# INSTALL_MAN	- A command to install manpages (doesn't compress).
#
# If your port doesn't automatically compress manpages, set the following.
# Depending on the setting of NOMANCOMPRESS, the make rules will compress
# the manpages for you.
#
# MAN<sect>		- A list of manpages, categorized by section.  For
#				  example, if your port has "man/man1/foo.1" and
#				  "man/mann/bar.n", set "MAN1=foo.1" and "MANN=bar.n".
#				  The available sections chars are "123456789LN".
# MANPREFIX		- The directory prefix for ${MAN<sect>} (default: ${PREFIX}).
#
# Default targets and their behaviors:
#
# fetch			- Retrieves ${DISTFILES} (and ${PATCHFILES} if defined)
#				  into ${DISTDIR} as necessary.
# fetch-list	- Show list of files that would be retrieved by fetch
# extract		- Unpacks ${DISTFILES} into ${WRKDIR}.
# patch			- Apply any provided patches to the source.
# configure		- Runs either GNU configure, one or more local configure
#				  scripts or nothing, depending on what's available.
# build			- Actually compile the sources.
# install		- Install the results of a build.
# reinstall		- Install the results of a build, ignoring "already installed"
#				  flag.
# package		- Create a package from an _installed_ port.
# describe		- Try to generate a one-line description for each port for
#				  use in INDEX files and the like.
# checkpatch	- Do a "patch -C" instead of a "patch".  Note that it may
#				  give incorrect results if multiple patches deal with
#				  the same file.
# checksum		- Use files/md5 to ensure that your distfiles are valid.
# makesum		- Generate files/md5 (only do this for your own ports!).
#
# Default sequence for "all" is:  fetch checksum extract patch configure build
#
# Please read the comments in the targets section below, you
# should be able to use the pre-* or post-* targets/scripts
# (which are available for every stage except checksum) or
# override the do-* targets to do pretty much anything you want.
#
# NEVER override the "regular" targets unless you want to open
# a major can of worms.

# Get the operating system type
OPSYS!=	uname -s

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif


# These need to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  They can, of course, be overridden
# by individual Makefiles or local system make configuration.
.if (${OPSYS} == "NetBSD")
PORTSDIR?=		/usr/opt
.else
PORTSDIR?=		/usr/ports
.endif
LOCALBASE?=		${DESTDIR}/usr/local
X11BASE?=		${DESTDIR}/usr/X11R6
DISTDIR?=		${PORTSDIR}/distfiles
_DISTDIR?=		${DISTDIR}/${DIST_SUBDIR}
PACKAGES?=		${PORTSDIR}/packages
TEMPLATES?=		${PORTSDIR}/templates
.if !defined(NO_WRKDIR)
WRKDIR?=		${.CURDIR}/work
.else
WRKDIR?=		${.CURDIR}
.endif
.if defined(NO_WRKSUBDIR)
WRKSRC?=		${WRKDIR}
.else
WRKSRC?=		${WRKDIR}/${DISTNAME}
.endif
PATCHDIR?=		${.CURDIR}/patches
SCRIPTDIR?=		${.CURDIR}/scripts
FILESDIR?=		${.CURDIR}/files
PKGDIR?=		${.CURDIR}/pkg
.if defined(USE_IMAKE) || defined(USE_X11)
PREFIX?=		${X11BASE}
.else
PREFIX?=		${LOCALBASE}
.endif
# The following 4 lines should go away as soon as the ports are all updated
.if defined(EXEC_DEPENDS)
BUILD_DEPENDS+=	${EXEC_DEPENDS}
RUN_DEPENDS+=	${EXEC_DEPENDS}
.endif
.if defined(USE_GMAKE)
BUILD_DEPENDS+=		gmake:${PORTSDIR}/devel/gmake
.endif
.if defined(USE_PERL5)
BUILD_DEPENDS+=		perl5.004:${PORTSDIR}/lang/perl5
RUN_DEPENDS+=		perl5.004:${PORTSDIR}/lang/perl5
.endif

.if exists(${PORTSDIR}/../Makefile.inc)
.include "${PORTSDIR}/../Makefile.inc"
.endif

# Don't change these!!!  These names are built into the _TARGET_USE macro,
# there is no way to refer to them cleanly from within the macro AFAIK.
EXTRACT_COOKIE?=	${WRKDIR}/.extract_done
CONFIGURE_COOKIE?=	${WRKDIR}/.configure_done
INSTALL_COOKIE?=	${WRKDIR}/.install_done
BUILD_COOKIE?=		${WRKDIR}/.build_done
PATCH_COOKIE?=		${WRKDIR}/.patch_done
PACKAGE_COOKIE?=	${WRKDIR}/.package_done

# How to do nothing.  Override if you, for some strange reason, would rather
# do something.
DO_NADA?=		/usr/bin/true

# Miscellaneous overridable commands:
GMAKE?=			gmake
XMKMF?=			xmkmf -a
.if exists(/sbin/md5)
MD5?=			/sbin/md5
.elif exists(/bin/md5)
MD5?=			/bin/md5
.elif exists(/usr/bin/md5)
MD5?=			/usr/bin/md5
.else
MD5?=			md5
.endif
MD5_FILE?=		${FILESDIR}/md5

MAKE_FLAGS?=	-f
MAKEFILE?=		Makefile
MAKE_ENV+=		PREFIX=${PREFIX} LOCALBASE=${LOCALBASE} X11BASE=${X11BASE} MOTIFLIB="${MOTIFLIB}" CFLAGS="${CFLAGS}"

.if exists(/usr/bin/fetch)
FETCH_CMD?=		/usr/bin/fetch
.else
FETCH_CMD?=		/usr/bin/ftp
.endif

TOUCH?=			/usr/bin/touch
TOUCH_FLAGS?=	-f

PATCH?=			/usr/bin/patch
PATCH_STRIP?=	-p0
PATCH_DIST_STRIP?=	-p0
.if defined(PATCH_DEBUG)
PATCH_DEBUG_TMP=	yes
PATCH_ARGS?=	-d ${WRKSRC} -E ${PATCH_STRIP}
PATCH_DIST_ARGS?=	-d ${WRKSRC} -E ${PATCH_DIST_STRIP}
.else
PATCH_DEBUG_TMP=	no
PATCH_ARGS?=	-d ${WRKSRC} --forward --quiet -E ${PATCH_STRIP}
PATCH_DIST_ARGS?=	-d ${WRKSRC} --forward --quiet -E ${PATCH_DIST_STRIP}
.endif
.if defined(BATCH)
PATCH_ARGS+=		--batch
PATCH_DIST_ARGS+=	--batch
.endif

.if defined(PATCH_CHECK_ONLY)
PATCH_ARGS+=	-C
PATCH_DIST_ARGS+=	-C
.endif

.if exists(/bin/tar)
EXTRACT_CMD?=	/bin/tar
.else
EXTRACT_CMD?=	/usr/bin/tar
.endif
EXTRACT_SUFX?=	.tar.gz
# Backwards compatability.
.if defined(EXTRACT_ARGS)
EXTRACT_BEFORE_ARGS?=   ${EXTRACT_ARGS}
.else
EXTRACT_BEFORE_ARGS?=   -xzf
.endif

# Figure out where the local mtree file is
.if !defined(MTREE_FILE)
.if defined(USE_IMAKE) || defined(USE_X11)
MTREE_FILE=	/etc/mtree/BSD.x11.dist
.else
MTREE_FILE=	/etc/mtree/BSD.local.dist
.endif
.endif
MTREE_CMD?=	/usr/sbin/mtree
MTREE_ARGS?=	-U -f ${MTREE_FILE} -d -e -p

# A few aliases for *-install targets
INSTALL_PROGRAM= \
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}
INSTALL_SCRIPT= \
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}
INSTALL_DATA= \
	${INSTALL} ${COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE}
INSTALL_MAN= \
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

INSTALL_MACROS=	BSD_INSTALL_PROGRAM="${INSTALL_PROGRAM}" \
			BSD_INSTALL_SCRIPT="${INSTALL_SCRIPT}" \
			BSD_INSTALL_DATA="${INSTALL_DATA}" \
			BSD_INSTALL_MAN="${INSTALL_MAN}"
MAKE_ENV+=	${INSTALL_MACROS}
SCRIPTS_ENV+=	${INSTALL_MACROS}

# The user can override the NO_PACKAGE by specifying this from
# the make command line
.if defined(FORCE_PACKAGE)
.undef NO_PACKAGE
.endif

COMMENT?=	${PKGDIR}/COMMENT
DESCR?=		${PKGDIR}/DESCR
PLIST?=		${PKGDIR}/PLIST

PKG_CMD?=		/usr/sbin/pkg_create
.if !defined(PKG_ARGS)
PKG_ARGS=		-v -c ${COMMENT} -d ${DESCR} -f ${PLIST} -p ${PREFIX} -P "`${MAKE} package-depends|sort -u`"
.if exists(${PKGDIR}/INSTALL)
PKG_ARGS+=		-i ${PKGDIR}/INSTALL
.endif
.if exists(${PKGDIR}/DEINSTALL)
PKG_ARGS+=		-k ${PKGDIR}/DEINSTALL
.endif
.if exists(${PKGDIR}/REQ)
PKG_ARGS+=		-r ${PKGDIR}/REQ
.endif
.if exists(${PKGDIR}/MESSAGE)
PKG_ARGS+=		-D ${PKGDIR}/MESSAGE
.endif
.if !defined(NO_MTREE)
PKG_ARGS+=		-m ${MTREE_FILE}
.endif
.endif
PKG_SUFX?=		.tgz
# where pkg_add records its dirty deeds.
PKG_DBDIR?=		/var/db/pkg

# shared/dynamic motif libs
.if defined(HAVE_MOTIF)
.if defined(MOTIF_STATIC)
MOTIFLIB?=	${X11BASE}/lib/libXm.a
.else
MOTIFLIB?=	-L${X11BASE}/lib -lXm
.endif
.endif

AWK?=		/usr/bin/awk
BASENAME?=	/usr/bin/basename
CAT?=		/bin/cat
CP?=		/bin/cp
ECHO?=		/bin/echo
FALSE?=		/usr/bin/false
GREP?=		/usr/bin/grep
GUNZIP_CMD?=	/usr/bin/gunzip -f
GZCAT?=		/usr/bin/gzcat
GZIP?=		-9
GZIP_CMD?=	/usr/bin/gzip -nf ${GZIP}
LDCONFIG?=	/sbin/ldconfig
MKDIR?=		/bin/mkdir -p
MV?=		/bin/mv
RM?=		/bin/rm
RMDIR?=		/bin/rmdir
SED?=		/usr/bin/sed
SETENV?=	/usr/bin/env
TR?=		/usr/bin/tr

# Used to print all the '===>' style prompts - override this to turn them off.
ECHO_MSG?=		${ECHO}

ALL_TARGET?=		all
INSTALL_TARGET?=	install

# Popular master sites
MASTER_SITE_XCONTRIB+=	\
	ftp://crl.dec.com/pub/X11/contrib/%SUBDIR%/ \
    ftp://ftp.eu.net/X11/contrib/%SUBDIR%/

MASTER_SITE_GNU+=	\
	ftp://prep.ai.mit.edu/pub/gnu/%SUBDIR%/ \
	ftp://wuarchive.wustl.edu/systems/gnu/%SUBDIR%/

MASTER_SITE_PERL_CPAN+=	\
	ftp://ftp.digital.com/pub/plan/perl/CPAN/modules/by-module/%SUBDIR%/ \
	ftp://ftp.cdrom.com/pub/perl/CPAN/modules/by-module/%SUBDIR%/

MASTER_SITE_TEX_CTAN+=  \
        ftp://ftp.cdrom.com/pub/tex/ctan/%SUBDIR%/  \
        ftp://wuarchive.wustl.edu/packages/TeX/%SUBDIR%/  \
        ftp://ftp.funet.fi/pub/TeX/CTAN/%SUBDIR%/  \
        ftp://ftp.tex.ac.uk/public/ctan/tex-archive/%SUBDIR%/  \
        ftp://ftp.dante.de/tex-archive/%SUBDIR%/

MASTER_SITE_SUNSITE+=	\
	ftp://sunsite.unc.edu/pub/Linux/%SUBDIR%/ \
	ftp://ftp.infomagic.com/pub/mirrors/linux/sunsite/%SUBDIR%/ \
	ftp://ftp.funet.fi/pub/mirrors/sunsite.unc.edu/pub/Linux/%SUBDIR%/

# Empty declaration to avoid "variable MASTER_SITES recursive" error
MASTER_SITES?=
PATCH_SITES?=

# Substitute subdirectory names
MASTER_SITES:=	${MASTER_SITES:S/%SUBDIR%/${MASTER_SITE_SUBDIR}/}
PATCH_SITES:=	${PATCH_SITES:S/%SUBDIR%/${PATCH_SITE_SUBDIR}/}

# The primary backup site.
MASTER_SITE_BACKUP?=	\
	ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/${DIST_SUBDIR}/

# If the user has this set, go to the FreeBSD repository for everything.
.if defined(MASTER_SITE_FREEBSD)
MASTER_SITE_OVERRIDE=  ${MASTER_SITE_BACKUP}
.endif

# Where to put distfiles that don't have any other master site
MASTER_SITE_LOCAL?= \
	ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/LOCAL_PORTS/

# I guess we're in the master distribution business! :)  As we gain mirror
# sites for distfiles, add them to this list.
.if !defined(MASTER_SITE_OVERRIDE)
MASTER_SITES+=	${MASTER_SITE_BACKUP}
PATCH_SITES+=	${MASTER_SITE_BACKUP}
.else
MASTER_SITES:=	${MASTER_SITE_OVERRIDE} ${MASTER_SITES}
PATCH_SITES:=	${MASTER_SITE_OVERRIDE} ${PATCH_SITES}
.endif

# Search CDROM first if mounted, symlink instead of copy if
# FETCH_SYMLINK_DISTFILES is set
.if exists(/cdrom/ports/distfiles)
MASTER_SITES:=	file:/cdrom/ports/distfiles/${DIST_SUBDIR}/ ${MASTER_SITES}
PATCH_SITES:=	file:/cdrom/ports/distfiles/${DIST_SUBDIR}/ ${PATCH_SITES}
.if defined(FETCH_SYMLINK_DISTFILES)
FETCH_BEFORE_ARGS+=	-l
.endif
.endif

# Derived names so that they're easily overridable.
DISTFILES?=		${DISTNAME}${EXTRACT_SUFX}
PKGNAME?=		${DISTNAME}

ALLFILES?=	${DISTFILES} ${PATCHFILES}

.if defined(IGNOREFILES)
CKSUMFILES!=	\
	for file in ${ALLFILES}; do \
		ignore=0; \
		for tmp in ${IGNOREFILES}; do \
			if [ "$$file" = "$$tmp" ]; then \
				ignore=1; \
			fi; \
		done; \
		if [ "$$ignore" = 0 ]; then \
			echo "$$file"; \
		fi; \
	done
.else
CKSUMFILES=		${ALLFILES}
.endif

# List of all files, with ${DIST_SUBDIR} in front.  Used for checksum.
.if defined(DIST_SUBDIR)
_CKSUMFILES?=	${CKSUMFILES:S/^/${DIST_SUBDIR}\//}
_IGNOREFILES?=	${IGNOREFILES:S/^/${DIST_SUBDIR}\//}
.else
_CKSUMFILES?=	${CKSUMFILES}
_IGNOREFILES?=	${IGNOREFILES}
.endif

# This is what is actually going to be extracted, and is overridable
#  by user.
EXTRACT_ONLY?=	${DISTFILES}

# Documentation
MAINTAINER?=	ports@FreeBSD.ORG

.if !defined(CATEGORIES)
.BEGIN:
	@${ECHO_MSG} "CATEGORIES is mandatory."
	@${FALSE}
.endif

# Note this has to start with a capital letter (or more accurately, it
#  shouldn't match "[a-z]*"), see the target "delete-package-links" below.
PKGREPOSITORYSUBDIR?=	All
PKGREPOSITORY?=		${PACKAGES}/${PKGREPOSITORYSUBDIR}
.if exists(${PACKAGES})
PKGFILE?=		${PKGREPOSITORY}/${PKGNAME}${PKG_SUFX}
.else
PKGFILE?=		${PKGNAME}${PKG_SUFX}
.endif

CONFIGURE_SCRIPT?=	configure

.if defined(GNU_CONFIGURE)
CONFIGURE_ARGS+=	--prefix=${PREFIX}
HAS_CONFIGURE=		yes
.endif

# Passed to most of script invocations
SCRIPTS_ENV+=	CURDIR=${.CURDIR} DISTDIR=${DISTDIR} \
		  WRKDIR=${WRKDIR} WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} \
		  SCRIPTDIR=${SCRIPTDIR} FILESDIR=${FILESDIR} \
		  PORTSDIR=${PORTSDIR} DEPENDS="${DEPENDS}" \
		  PREFIX=${PREFIX} LOCALBASE=${LOCALBASE} X11BASE=${X11BASE}

.if defined(BATCH)
SCRIPTS_ENV+=	BATCH=yes
.endif

MANPREFIX?=	${PREFIX}

.for sect in 1 2 3 4 5 6 7 8 9
MAN${sect}PREFIX?=	${MANPREFIX}
.endfor
MANLPREFIX?=	${MANPREFIX}
MANNPREFIX?=	${MANPREFIX}

MANLANG?=	""	# english only by default

.for lang in ${MANLANG}

.for sect in 1 2 3 4 5 6 7 8 9
.if defined(MAN${sect})
_MANPAGES+=	${MAN${sect}:S%^%${MAN${sect}PREFIX}/man/${lang}/man${sect}/%}
.endif
.endfor

.if defined(MANL)
_MANPAGES+=	${MANL:S%^%${MANLPREFIX}/man/${lang}/manl/%}
.endif

.if defined(MANN)
_MANPAGES+=	${MANN:S%^%${MANNPREFIX}/man/${lang}/mann/%}
.endif

.endfor

.if defined(_MANPAGES) && defined(MANCOMPRESSED)
_MANPAGES:=	${_MANPAGES:S/$/.gz/}
.endif

.MAIN: all

################################################################
# Many ways to disable a port.
#
# If we're in BATCH mode and the port is interactive, or we're
# in interactive mode and the port is non-interactive, skip all
# the important targets.  The reason we have two modes is that
# one might want to leave a build in BATCH mode running
# overnight, then come back in the morning and do _only_ the
# interactive ones that required your intervention.
#
# Don't attempt to build ports that require Motif if you don't
# have Motif.
#
# Ignore ports that can't be resold if building for a CDROM.
#
# Don't build a port if it's restricted and we don't want to get
# into that.
#
# Don't build a port if it's broken.
################################################################

.if !defined(NO_IGNORE)
.if (defined(IS_INTERACTIVE) && defined(BATCH))
IGNORE=	"is an interactive port"
.elif (!defined(IS_INTERACTIVE) && defined(INTERACTIVE))
IGNORE=	"is not an interactive port"
.elif (defined(REQUIRES_MOTIF) && !defined(HAVE_MOTIF))
IGNORE=	"requires Motif"
.elif (defined(MOTIF_ONLY) && !defined(REQUIRES_MOTIF))
IGNORE=	"does not require Motif"
.elif (defined(NO_CDROM) && defined(FOR_CDROM))
IGNORE=	"may not be placed on a CDROM: ${NO_CDROM}"
.elif (defined(RESTRICTED) && defined(NO_RESTRICTED))
IGNORE=	"is restricted: ${RESTRICTED}"
.elif ((defined(USE_IMAKE) || defined(USE_X11)) && !exists(${X11BASE}))
IGNORE=	"uses X11, but ${X11BASE} not found"
.elif defined(BROKEN)
IGNORE=	"is marked as broken: ${BROKEN}"
.endif

.if defined(IGNORE)
.if defined(IGNORE_SILENT)
IGNORECMD=	${DO_NADA}
.else
IGNORECMD=	${ECHO_MSG} "===>  ${PKGNAME} ${IGNORE}."
.endif
fetch:
	@${IGNORECMD}
checksum:
	@${IGNORECMD}
extract:
	@${IGNORECMD}
patch:
	@${IGNORECMD}
configure:
	@${IGNORECMD}
all:
	@${IGNORECMD}
build:
	@${IGNORECMD}
install:
	@${IGNORECMD}
package:
	@${IGNORECMD}
.endif
.endif

.if defined(ALL_HOOK)
all:
	@cd ${.CURDIR} && ${SETENV} CURDIR=${.CURDIR} DISTNAME=${DISTNAME} \
	  DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} WRKSRC=${WRKSRC} \
	  PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
	  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
	  DEPENDS="${DEPENDS}" BUILD_DEPENDS="${BUILD_DEPENDS}" \
	  RUN_DEPENDS="${RUN_DEPENDS}" X11BASE=${X11BASE} \
	${ALL_HOOK}
.endif

.if !target(all)
all: build
.endif

.if !defined(DEPENDS_TARGET)
DEPENDS_TARGET=	install
.endif

################################################################
# The following are used to create easy dummy targets for
# disabling some bit of default target behavior you don't want.
# They still check to see if the target exists, and if so don't
# do anything, since you might want to set this globally for a
# group of ports in a Makefile.inc, but still be able to
# override from an individual Makefile.
################################################################

# Disable checksum
.if defined(NO_CHECKSUM) && !target(checksum)
checksum: fetch
	@${DO_NADA}
.endif

# Disable extract
.if defined(NO_EXTRACT) && !target(extract)
extract: checksum
	@${TOUCH} ${TOUCH_FLAGS} ${EXTRACT_COOKIE}
checksum: fetch
	@${DO_NADA}
makesum:
	@${DO_NADA}
.endif

# Disable patch
.if defined(NO_PATCH) && !target(patch)
patch: extract
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
.endif

# Disable configure
.if defined(NO_CONFIGURE) && !target(configure)
configure: patch
	@${TOUCH} ${TOUCH_FLAGS} ${CONFIGURE_COOKIE}
.endif

# Disable build
.if defined(NO_BUILD) && !target(build)
build: configure
	@${TOUCH} ${TOUCH_FLAGS} ${BUILD_COOKIE}
.endif

# Disable install
.if defined(NO_INSTALL) && !target(install)
install: build
	@${TOUCH} ${TOUCH_FLAGS} ${INSTALL_COOKIE}
.endif

# Disable package
.if defined(NO_PACKAGE) && !target(package)
package:
.if defined(IGNORE_SILENT)
	@${DO_NADA}
.else
	@${ECHO_MSG} "===>  ${PKGNAME} may not be packaged: ${NO_PACKAGE}."
.endif
.endif

# Disable describe
.if defined(NO_DESCRIBE) && !target(describe)
describe:
	@${DO_NADA}
.endif

################################################################
# More standard targets start here.
#
# These are the body of the build/install framework.  If you are
# not happy with the default actions, and you can't solve it by
# adding pre-* or post-* targets/scripts, override these.
################################################################

# Fetch

.if !target(do-fetch)
do-fetch:
	@${MKDIR} ${_DISTDIR}
	@(cd ${_DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			if [ -h $$file -o -h `${BASENAME} $$file` ]; then \
				${ECHO_MSG} ">> ${_DISTDIR}/$$file is a broken symlink."; \
				${ECHO_MSG} ">> Perhaps a filesystem (most likely a CD) isn't mounted?"; \
				${ECHO_MSG} ">> Please correct this problem and try again."; \
				exit 1; \
			fi ; \
			${ECHO_MSG} ">> $$file doesn't seem to exist on this system."; \
			for site in ${MASTER_SITES}; do \
			    ${ECHO_MSG} ">> Attempting to fetch from $${site}."; \
				if ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} ${FETCH_AFTER_ARGS}; then \
					continue 2; \
				fi \
			done; \
			${ECHO_MSG} ">> Couldn't fetch it - please try to retrieve this";\
			${ECHO_MSG} ">> port manually into ${_DISTDIR} and try again."; \
			exit 1; \
	    fi \
	 done)
.if defined(PATCHFILES)
	@(cd ${_DISTDIR}; \
	 for file in ${PATCHFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			if [ -h $$file -o -h `${BASENAME} $$file` ]; then \
				${ECHO_MSG} ">> ${_DISTDIR}/$$file is a broken symlink."; \
				${ECHO_MSG} ">> Perhaps a filesystem (most likely a CD) isn't mounted?"; \
				${ECHO_MSG} ">> Please correct this problem and try again."; \
				exit 1; \
			fi ; \
			${ECHO_MSG} ">> $$file doesn't seem to exist on this system."; \
			for site in ${PATCH_SITES}; do \
			    ${ECHO_MSG} ">> Attempting to fetch from $${site}."; \
				if ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} ${FETCH_AFTER_ARGS}; then \
					continue 2; \
				fi \
			done; \
			${ECHO_MSG} ">> Couldn't fetch it - please try to retrieve this";\
			${ECHO_MSG} ">> port manually into ${_DISTDIR} and try again."; \
			exit 1; \
	    fi \
	 done)
.endif
.endif

# Extract

.if !target(do-extract)
do-extract:
.if !defined(NO_WRKDIR)
	@${RM} -rf ${WRKDIR}
	@${MKDIR} ${WRKDIR}
.endif
	@for file in ${EXTRACT_ONLY}; do \
		if ! (cd ${WRKDIR} && ${EXTRACT_CMD} ${EXTRACT_BEFORE_ARGS} ${_DISTDIR}/$$file ${EXTRACT_AFTER_ARGS});\
		then \
			exit 1; \
		fi \
	done
.endif

# Patch

.if !target(do-patch)
do-patch:
.if defined(PATCHFILES)
	@${ECHO_MSG} "===>  Applying distribution patches for ${PKGNAME}"
	@(cd ${_DISTDIR}; \
	  for i in ${PATCHFILES}; do \
		if [ ${PATCH_DEBUG_TMP} = yes ]; then \
			${ECHO_MSG} "===>   Applying distribution patch $$i" ; \
		fi; \
		case $$i in \
			*.Z|*.gz) \
				${GZCAT} $$i | ${PATCH} ${PATCH_DIST_ARGS}; \
				;; \
			*) \
				${PATCH} ${PATCH_DIST_ARGS} < $$i; \
				;; \
		esac; \
	  done)
.endif
	@if [ -d ${PATCHDIR} ]; then \
		if [ "`echo ${PATCHDIR}/patch-*`" = "${PATCHDIR}/patch-*" ]; then \
			${ECHO_MSG} "===>   Ignoring empty patch directory"; \
			if [ -d ${PATCHDIR}/CVS ]; then \
				${ECHO_MSG} "===>   Perhaps you forgot the -P flag to cvs co or update?"; \
			fi; \
		else \
			${ECHO_MSG} "===>  Applying ${OPSYS} patches for ${PKGNAME}" ; \
			for i in ${PATCHDIR}/patch-*; do \
				case $$i in \
					*.orig|*.rej|*~) \
						${ECHO_MSG} "===>   Ignoring patchfile $$i" ; \
						;; \
					*) \
						if [ ${PATCH_DEBUG_TMP} = yes ]; then \
							${ECHO_MSG} "===>   Applying ${OPSYS} patch $$i" ; \
						fi; \
						${PATCH} ${PATCH_ARGS} < $$i; \
						;; \
				esac; \
			done; \
		fi; \
	fi
.endif

# Configure

.if !target(do-configure)
do-configure:
	@if [ -f ${SCRIPTDIR}/configure ]; then \
		cd ${.CURDIR} && ${SETENV} ${SCRIPTS_ENV} /bin/sh \
		  ${SCRIPTDIR}/configure; \
	fi
.if defined(HAS_CONFIGURE)
	@(cd ${WRKSRC} && CC="${CC}" ac_cv_path_CC="${CC}" CFLAGS="${CFLAGS}" \
	    INSTALL="/usr/bin/install -c -o ${BINOWN} -g ${BINGRP}" \
	    INSTALL_PROGRAM="${INSTALL_PROGRAM}" \
	    ${CONFIGURE_ENV} ./${CONFIGURE_SCRIPT} ${CONFIGURE_ARGS})
.endif
.if defined(USE_IMAKE)
	@(cd ${WRKSRC} && ${XMKMF})
.endif
.endif

# Build

.if !target(do-build)
do-build:
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${SETENV} ${MAKE_ENV} ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${SETENV} ${MAKE_ENV} ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.endif
.endif

# Install

.if !target(do-install)
do-install:
.if defined(USE_GMAKE)
	@(cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
	@(cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} install.man)
.endif
.else defined(USE_GMAKE)
	@(cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
	@(cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} install.man)
.endif
.endif
.endif

# Package

.if !target(do-package)
do-package:
	@if [ -e ${PLIST} ]; then \
		${ECHO_MSG} "===>  Building package for ${PKGNAME}"; \
		if [ -d ${PACKAGES} ]; then \
			if [ ! -d ${PKGREPOSITORY} ]; then \
				if ! ${MKDIR} ${PKGREPOSITORY}; then \
					${ECHO_MSG} ">> Can't create directory ${PKGREPOSITORY}."; \
					exit 1; \
				fi; \
			fi; \
		fi; \
		if ${PKG_CMD} ${PKG_ARGS} ${PKGFILE}; then \
			if [ -d ${PACKAGES} ]; then \
				${MAKE} ${.MAKEFLAGS} package-links; \
			fi; \
		else \
			${MAKE} ${.MAKEFLAGS} delete-package; \
			exit 1; \
		fi; \
	fi
.endif

# Some support rules for do-package

.if !target(package-links)
package-links:
	@${MAKE} ${.MAKEFLAGS} delete-package-links
	@for cat in ${CATEGORIES}; do \
		if [ ! -d ${PACKAGES}/$$cat ]; then \
			if ! ${MKDIR} ${PACKAGES}/$$cat; then \
				${ECHO_MSG} ">> Can't create directory ${PACKAGES}/$$cat."; \
				exit 1; \
			fi; \
		fi; \
		ln -s ../${PKGREPOSITORYSUBDIR}/${PKGNAME}${PKG_SUFX} ${PACKAGES}/$$cat; \
	done;
.endif

.if !target(delete-package-links)
delete-package-links:
	@${RM} -f ${PACKAGES}/[a-z]*/${PKGNAME}${PKG_SUFX};
.endif

.if !target(delete-package)
delete-package:
	@${MAKE} ${.MAKEFLAGS} delete-package-links
	@${RM} -f ${PKGFILE}
.endif

################################################################
# This is the "generic" port target, actually a macro used from the
# six main targets.  See below for more.
################################################################

_PORT_USE: .USE
.if make(real-fetch)
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} fetch-depends
.endif
.if make(real-extract)
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} build-depends lib-depends misc-depends
.endif
.if make(real-install)
.if !defined(NO_PKG_REGISTER) && !defined(FORCE_PKG_REGISTER)
	@if [ -d ${PKG_DBDIR}/${PKGNAME} ]; then \
		${ECHO_MSG} "===>  ${PKGNAME} is already installed - perhaps an older version?"; \
		${ECHO_MSG} "      If so, you may wish to \`\`pkg_delete ${PKGNAME}'' and install"; \
		${ECHO_MSG} "      this port again by \`\`make reinstall'' to upgrade it properly."; \
		${ECHO_MSG} "      If you really wish to overwrite the old port of ${PKGNAME}"; \
		${ECHO_MSG} "      without deleting it first, set the variable \"FORCE_PKG_REGISTER\""; \
		${ECHO_MSG} "      in your environment or the \"make install\" command line."; \
		exit 1; \
	fi
.endif
	@if [ `/bin/sh -c umask` != 0022 ]; then \
		${ECHO_MSG} "===>  Warning: your umask is \"`/bin/sh -c umask`"\".; \
		${ECHO_MSG} "      If this is not desired, set it to an appropriate value"; \
		${ECHO_MSG} "      and install this port again by \`\`make reinstall''."; \
	fi
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} run-depends lib-depends
.endif
.if make(real-install)
.if !defined(NO_MTREE)
	@if [ `id -u` = 0 ]; then \
		${MTREE_CMD} ${MTREE_ARGS} ${PREFIX}/; \
	else \
		${ECHO_MSG} "Warning: not superuser, can't run mtree."; \
		${ECHO_MSG} "Become root and try again to ensure correct permissions."; \
	fi
.endif
.endif
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/pre-/}
	@if [ -f ${SCRIPTDIR}/${.TARGET:S/^real-/pre-/} ]; then \
		cd ${.CURDIR} && ${SETENV} ${SCRIPTS_ENV} /bin/sh \
			${SCRIPTDIR}/${.TARGET:S/^real-/pre-/}; \
	fi
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/do-/}
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/post-/}
	@if [ -f ${SCRIPTDIR}/${.TARGET:S/^real-/post-/} ]; then \
		cd ${.CURDIR} && ${SETENV} ${SCRIPTS_ENV} /bin/sh \
			${SCRIPTDIR}/${.TARGET:S/^real-/post-/}; \
	fi
.if make(real-install) && defined(_MANPAGES)
.if defined(MANCOMPRESSED) && defined(NOMANCOMPRESS)
	@${ECHO_MSG} "===>   Uncompressing manual pages for ${PKGNAME}"
.for manpage in ${_MANPAGES}
	@${GUNZIP_CMD} ${manpage}
.endfor
.elif !defined(MANCOMPRESSED) && !defined(NOMANCOMPRESS)
	@${ECHO_MSG} "===>   Compressing manual pages for ${PKGNAME}"
.for manpage in ${_MANPAGES}
	@${GZIP_CMD} ${manpage}
.endfor
.endif
.endif
.if make(real-install) && !defined(NO_PKG_REGISTER)
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} fake-pkg
.endif
.if !make(real-fetch) \
	&& (!make(real-patch) || !defined(PATCH_CHECK_ONLY)) \
	&& (!make(real-package) || !defined(PACKAGE_NOINSTALL))
	@${TOUCH} ${TOUCH_FLAGS} ${WRKDIR}/.${.TARGET:S/^real-//}_done
.endif

################################################################
# Skeleton targets start here
# 
# You shouldn't have to change these.  Either add the pre-* or
# post-* targets/scripts or redefine the do-* targets.  These
# targets don't do anything other than checking for cookies and
# call the necessary targets/scripts.
################################################################

.if !target(fetch)
fetch:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-fetch
.endif

.if !target(extract)
extract: checksum ${EXTRACT_COOKIE}
.endif

.if !target(patch)
patch: extract ${PATCH_COOKIE}
.endif

.if !target(configure)
configure: patch ${CONFIGURE_COOKIE}
.endif

.if !target(build)
build: configure ${BUILD_COOKIE}
.endif

.if !target(install)
install: build ${INSTALL_COOKIE}
.endif

.if !target(package)
package: install ${PACKAGE_COOKIE}
.endif

${EXTRACT_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-extract
${PATCH_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-patch
${CONFIGURE_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-configure
${BUILD_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-build
${INSTALL_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-install
${PACKAGE_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-package

# And call the macros

real-fetch: _PORT_USE
real-extract: _PORT_USE
	@${ECHO_MSG} "===>  Extracting for ${PKGNAME}"
real-patch: _PORT_USE
	@${ECHO_MSG} "===>  Patching for ${PKGNAME}"
real-configure: _PORT_USE
	@${ECHO_MSG} "===>  Configuring for ${PKGNAME}"
real-build: _PORT_USE
	@${ECHO_MSG} "===>  Building for ${PKGNAME}"
real-install: _PORT_USE
	@${ECHO_MSG} "===>  Installing for ${PKGNAME}"
real-package: _PORT_USE

# Empty pre-* and post-* targets, note we can't use .if !target()
# in the _PORT_USE macro

.for name in fetch extract patch configure build install package

.if !target(pre-${name})
pre-${name}:
	@${DO_NADA}
.endif

.if !target(post-${name})
post-${name}:
	@${DO_NADA}
.endif

.endfor

# Checkpatch
#
# Special target to verify patches

.if !target(checkpatch)
checkpatch:
	@cd ${.CURDIR} && ${MAKE} PATCH_CHECK_ONLY=yes ${.MAKEFLAGS} patch
.endif

# Reinstall
#
# Special target to re-run install

.if !target(reinstall)
reinstall:
	@${RM} -f ${INSTALL_COOKIE} ${PACKAGE_COOKIE}
	@${MAKE} install
.endif

################################################################
# Some more targets supplied for users' convenience
################################################################

# Cleaning up

.if !target(pre-clean)
pre-clean:
	@${DO_NADA}
.endif

.if !target(clean)
clean: pre-clean
.if !defined(NOCLEANDEPENDS)
	@${MAKE} clean-depends
.endif
	@${ECHO_MSG} "===>  Cleaning for ${PKGNAME}"
.if !defined(NO_WRKDIR)
	@if [ -d ${WRKDIR} ]; then \
		if [ -w ${WRKDIR} ]; then \
			${RM} -rf ${WRKDIR}; \
		else \
			${ECHO_MSG} "===>   ${WRKDIR} not writable, skipping"; \
		fi; \
	fi
.else
	@${RM} -f ${WRKDIR}/.*_done
.endif
.endif

.if !target(pre-distclean)
pre-distclean:
	@${DO_NADA}
.endif

.if !target(distclean)
distclean: pre-distclean clean
	@${ECHO_MSG} "===>  Dist cleaning for ${PKGNAME}"
	@(if [ -d ${_DISTDIR} ]; then \
		cd ${_DISTDIR}; \
		${RM} -f ${DISTFILES} ${PATCHFILES}; \
	fi)
.if defined(DIST_SUBDIR)
	-@${RMDIR} ${_DISTDIR}  
.endif
.endif

# Prints out a list of files to fetch (useful to do a batch fetch)

.if !target(fetch-list)
fetch-list:
	@${MKDIR} ${_DISTDIR}
	@(cd ${_DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			for site in ${MASTER_SITES}; do \
				${ECHO} -n ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} "${FETCH_AFTER_ARGS}" '||' ; \
					break; \
			done; \
			${ECHO} "echo $${file} not fetched" ; \
		fi \
	done)
.if defined(PATCHFILES)
	@(cd ${_DISTDIR}; \
	 for file in ${PATCHFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			for site in ${PATCH_SITES}; do \
				${ECHO} -n ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} "${FETCH_AFTER_ARGS}" '||' ; \
					break; \
			done; \
			${ECHO} "echo $${file} not fetched" ; \
		fi \
	 done)
.endif
.endif

# Checksumming utilities

.if !target(makesum)
makesum: fetch
	@${MKDIR} ${FILESDIR}
	@if [ -f ${MD5_FILE} ]; then ${RM} -f ${MD5_FILE}; fi
	@(cd ${DISTDIR}; \
	 for file in ${_CKSUMFILES}; do \
		${MD5} $$file >> ${MD5_FILE}; \
	 done)
	@for file in ${_IGNOREFILES}; do \
		${ECHO} "MD5 ($$file) = IGNORE" >> ${MD5_FILE}; \
	done
.endif

.if !target(checksum)
checksum: fetch
	@if [ ! -f ${MD5_FILE} ]; then \
		${ECHO_MSG} ">> No MD5 checksum file."; \
	else \
		(cd ${DISTDIR}; OK="true"; \
		  for file in ${_CKSUMFILES}; do \
			CKSUM=`${MD5} < $$file`; \
			CKSUM2=`${GREP} "^MD5 ($$file)" ${MD5_FILE} | ${AWK} '{print $$4}'`; \
			if [ "$$CKSUM2" = "" ]; then \
				${ECHO_MSG} ">> No checksum recorded for $$file."; \
				OK="false"; \
			elif [ "$$CKSUM2" = "IGNORE" ]; then \
				${ECHO_MSG} ">> Checksum for $$file is set to IGNORE in md5 file even though"; \
				${ECHO_MSG} "   the file is not in the "'$$'"{IGNOREFILES} list."; \
				OK="false"; \
			elif [ "$$CKSUM" = "$$CKSUM2" ]; then \
				${ECHO_MSG} ">> Checksum OK for $$file."; \
			else \
				${ECHO_MSG} ">> Checksum mismatch for $$file."; \
				OK="false"; \
			fi; \
		  done; \
		  for file in ${_IGNOREFILES}; do \
			CKSUM2=`${GREP} "($$file)" ${MD5_FILE} | ${AWK} '{print $$4}'`; \
			if [ "$$CKSUM2" = "" ]; then \
				${ECHO_MSG} ">> No checksum recorded for $$file, file is in "'$$'"{IGNOREFILES} list."; \
				OK="false"; \
			elif [ "$$CKSUM2" != "IGNORE" ]; then \
				${ECHO_MSG} ">> Checksum for $$file is not set to IGNORE in md5 file even though"; \
				${ECHO_MSG} "   the file is in the "'$$'"{IGNOREFILES} list."; \
				OK="false"; \
			fi; \
		  done; \
		  if [ "$$OK" != "true" ]; then \
			${ECHO_MSG} "Make sure the Makefile and md5 file (${MD5_FILE})"; \
			${ECHO_MSG} "are up to date.  If you want to override this check, type"; \
			${ECHO_MSG} "\"make NO_CHECKSUM=yes [other args]\"."; \
			exit 1; \
		  fi) ; \
	fi
.endif

################################################################
# The special package-building targets
# You probably won't need to touch these
################################################################

# Nobody should want to override this unless PKGNAME is simply bogus.

.if !target(package-name)
package-name:
	@${ECHO} ${PKGNAME}
.endif

# Show (recursively) all the packages this package depends on.

.if !target(package-depends)
package-depends:
	@for dir in `${ECHO} ${LIB_DEPENDS} ${RUN_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u` `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		if [ -d $$dir ]; then \
			(cd $$dir ; ${MAKE} package-name package-depends); \
		else \
			${ECHO_MSG} "Warning: \"$$dir\" non-existent -- @pkgdep registration incomplete" >&2; \
		fi; \
	done
.endif

# Build a package but don't check the package cookie

.if !target(repackage)
repackage: pre-repackage package

pre-repackage:
	@${RM} -f ${PACKAGE_COOKIE}
.endif

# Build a package but don't check the cookie for installation, also don't
# install package cookie

.if !target(package-noinstall)
package-noinstall:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} PACKAGE_NOINSTALL=yes real-package
.endif

################################################################
# Dependency checking
################################################################

.if !target(depends)
depends: lib-depends misc-depends
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} fetch-depends
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} build-depends
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} run-depends

.if make(fetch-depends)
DEPENDS_TMP+=	${FETCH_DEPENDS}
.endif

.if make(build-depends)
DEPENDS_TMP+=	${BUILD_DEPENDS}
.endif

.if make(run-depends)
DEPENDS_TMP+=	${RUN_DEPENDS}
.endif

_DEPENDS_USE:	.USE
.if defined(DEPENDS_TMP)
.if !defined(NO_DEPENDS)
	@for i in ${DEPENDS_TMP}; do \
		prog=`${ECHO} $$i | ${SED} -e 's/:.*//'`; \
		dir=`${ECHO} $$i | ${SED} -e 's/[^:]*://'`; \
		if expr "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		if expr "$$prog" : \\/ >/dev/null; then \
			if [ -e "$$prog" ]; then \
				${ECHO_MSG} "===>  ${PKGNAME} depends on file: $$prog - found"; \
				notfound=0; \
			else \
				${ECHO_MSG} "===>  ${PKGNAME} depends on file: $$prog - not found"; \
				notfound=1; \
			fi; \
		else \
			if which "$$prog" > /dev/null 2>&1 ; then \
				${ECHO_MSG} "===>  ${PKGNAME} depends on executable: $$prog - found"; \
				notfound=0; \
			else \
				${ECHO_MSG} "===>  ${PKGNAME} depends on executable: $$prog - not found"; \
				notfound=1; \
			fi; \
		fi; \
		if [ $$notfound != 0 ]; then \
			${ECHO_MSG} "===>  Verifying $$target for $$prog in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$prog.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
				${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"; \
			fi; \
		fi; \
	done
.endif
.else
	@${DO_NADA}
.endif

fetch-depends:	_DEPENDS_USE
build-depends:	_DEPENDS_USE
run-depends:	_DEPENDS_USE

lib-depends:
.if defined(LIB_DEPENDS)
.if !defined(NO_DEPENDS)
	@for i in ${LIB_DEPENDS}; do \
		lib=`${ECHO} $$i | ${SED} -e 's/:.*//'`; \
		dir=`${ECHO} $$i | ${SED} -e 's/[^:]*://'`; \
		if expr "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		if /sbin/ldconfig -r | ${GREP} -q -e "-l$$lib"; then \
			${ECHO_MSG} "===>  ${PKGNAME} depends on shared library: $$lib - found"; \
		else \
			${ECHO_MSG} "===>  ${PKGNAME} depends on shared library: $$lib - not found"; \
			${ECHO_MSG} "===>  Verifying $$target for $$lib in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$lib.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
				${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"; \
			fi; \
		fi; \
	done
.endif
.else
	@${DO_NADA}
.endif

misc-depends:
.if defined(DEPENDS)
.if !defined(NO_DEPENDS)
	@for dir in ${DEPENDS}; do \
		if expr "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		${ECHO_MSG} "===>  ${PKGNAME} depends on: $$dir"; \
		${ECHO_MSG} "===>  Verifying $$target for $$dir"; \
		if [ ! -d $$dir ]; then \
			${ECHO_MSG} ">> No directory for $$dir.  Skipping.."; \
		else \
			(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
		fi \
	done
	@${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"
.endif
.else
	@${DO_NADA}
.endif

.endif

.if !target(clean-depends)
clean-depends:
.if defined(FETCH_DEPENDS) || defined(BUILD_DEPENDS) || defined(LIB_DEPENDS) \
	|| defined(RUN_DEPENDS)
	@for dir in `${ECHO} ${FETCH_DEPENDS} ${BUILD_DEPENDS} ${LIB_DEPENDS} ${RUN_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u`; do \
		if [ -d $$dir ] ; then \
			(cd $$dir; ${MAKE} NOCLEANDEPENDS=yes clean clean-depends); \
		fi \
	done
.endif
.if defined(DEPENDS)
	@for dir in `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		if [ -d $$dir ] ; then \
			(cd $$dir; ${MAKE} NOCLEANDEPENDS=yes clean clean-depends); \
		fi \
	done
.endif
.endif

.if !target(depends-list)
depends-list:
	@for dir in `${ECHO} ${FETCH_DEPENDS} ${BUILD_DEPENDS} ${LIB_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u` `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		(cd $$dir; ${MAKE} package-name depends-list); \
	done
.endif

################################################################
# Everything after here are internal targets and really
# shouldn't be touched by anybody but the release engineers.
################################################################

# This target generates an index entry suitable for aggregation into
# a large index.  Format is:
#
# distribution-name|port-path|installation-prefix|comment| \
#  description-file|maintainer|categories|build deps|run deps
#
.if !target(describe)
describe:
	@${ECHO} -n "${PKGNAME}|${.CURDIR}|"; \
	${ECHO} -n "${PREFIX}|"; \
	if [ -f ${COMMENT} ]; then \
		${ECHO} -n "`${CAT} ${COMMENT}`"; \
	else \
		${ECHO} -n "** No Description"; \
	fi; \
	if [ -f ${DESCR} ]; then \
		${ECHO} -n "|${DESCR}"; \
	else \
		${ECHO} -n "|/dev/null"; \
	fi; \
	${ECHO} -n "|${MAINTAINER}|${CATEGORIES}|"; \
	case "A${FETCH_DEPENDS}B${BUILD_DEPENDS}C${LIB_DEPENDS}D${DEPENDS}E" in \
		ABCDE) ;; \
		*) cd ${.CURDIR} && ${ECHO} -n `make depends-list|sort -u`;; \
	esac; \
	${ECHO} -n "|"; \
	case "A${RUN_DEPENDS}B${LIB_DEPENDS}C${DEPENDS}D" in \
		ABCD) ;; \
		*) cd ${.CURDIR} && ${ECHO} -n `make package-depends|sort -u`;; \
	esac; \
	${ECHO} ""
.endif

.if !target(readmes)
readmes:	readme
.endif

.if !target(readme)
readme:
	@rm -f README.html
	@cd ${.CURDIR} && make README.html
.endif

README.html:
	@${ECHO_MSG} "===>  Creating README.html for ${PKGNAME}"
	@${CAT} ${TEMPLATES}/README.port | \
		${SED} -e 's%%PORT%%'`${ECHO} ${.CURDIR} | ${SED} -e 's.*/\([^/]*/[^/]*\)$$\1'`'g' \
			-e 's%%PKG%%${PKGNAME}g' \
			-e '/%%COMMENT%%/r${PKGDIR}/COMMENT' \
			-e '/%%COMMENT%%/d' \
			-e 's%%BUILD_DEPENDS%%'"`${MAKE} print-depends-list`"'' \
			-e 's%%RUN_DEPENDS%%'"`${MAKE} print-package-depends`"'' \
		>> $@

.if !target(print-depends-list)
print-depends-list:
.if defined(FETCH_DEPENDS) || defined(BUILD_DEPENDS) || \
	defined(LIB_DEPENDS) || defined(DEPENDS)
	@${ECHO} -n 'This port requires package(s) "'
	@${ECHO} -n `make depends-list | sort -u`
	@${ECHO} '" to build.'
.endif
.endif

.if !target(print-package-depends)
print-package-depends:
.if defined(RUN_DEPENDS) || defined(LIB_DEPENDS) || defined(DEPENDS)
	@${ECHO} -n 'This port requires package(s) "'
	@${ECHO} -n `make package-depends | sort -u`
	@${ECHO} '" to run.'
.endif
.endif

# Fake installation of package so that user can pkg_delete it later.
# Also, make sure that an installed port is recognized correctly in
# accordance to the @pkgdep directive in the packing lists

.if !target(fake-pkg)
fake-pkg:
	@if [ ! -f ${PLIST} -o ! -f ${COMMENT} -o ! -f ${DESCR} ]; then ${ECHO} "** Missing package files for ${PKGNAME} - installation not recorded."; exit 1; fi
	@if [ ! -d ${PKG_DBDIR} ]; then ${RM} -f ${PKG_DBDIR}; ${MKDIR} ${PKG_DBDIR}; fi
.if defined(FORCE_PKG_REGISTER)
	@${RM} -rf ${PKG_DBDIR}/${PKGNAME}
.endif
	@if [ ! -d ${PKG_DBDIR}/${PKGNAME} ]; then \
		${ECHO_MSG} "===>  Registering installation for ${PKGNAME}"; \
		${MKDIR} ${PKG_DBDIR}/${PKGNAME}; \
		${PKG_CMD} ${PKG_ARGS} -O ${PKGFILE} > ${PKG_DBDIR}/${PKGNAME}/+CONTENTS; \
		${CP} ${DESCR} ${PKG_DBDIR}/${PKGNAME}/+DESC; \
		${CP} ${COMMENT} ${PKG_DBDIR}/${PKGNAME}/+COMMENT; \
		if [ -f ${PKGDIR}/INSTALL ]; then \
			${CP} ${PKGDIR}/INSTALL ${PKG_DBDIR}/${PKGNAME}/+INSTALL; \
		fi; \
		if [ -f ${PKGDIR}/DEINSTALL ]; then \
			${CP} ${PKGDIR}/DEINSTALL ${PKG_DBDIR}/${PKGNAME}/+DEINSTALL; \
		fi; \
		if [ -f ${PKGDIR}/REQ ]; then \
			${CP} ${PKGDIR}/REQ ${PKG_DBDIR}/${PKGNAME}/+REQ; \
		fi; \
		for dep in `make package-depends ECHO_MSG=/usr/bin/true | sort -u`; do \
			if [ -d ${PKG_DBDIR}/$$dep ]; then \
				if ! ${GREP} ^${PKGNAME}$$ ${PKG_DBDIR}/$$dep/+REQUIRED_BY \
					>/dev/null 2>&1; then \
					${ECHO} ${PKGNAME} >> ${PKG_DBDIR}/$$dep/+REQUIRED_BY; \
				fi; \
			fi; \
		done; \
	fi
.endif

# Depend is generally meaningless for arbitrary ports, but if someone wants
# one they can override this.  This is just to catch people who've gotten into
# the habit of typing `make depend all install' as a matter of course.
#
.if !target(depend)
depend:
.endif

# Same goes for tags
.if !target(tags)
tags:
.endif
