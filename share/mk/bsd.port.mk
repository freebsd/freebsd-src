#-*- mode: Fundamental; tab-width: 4; -*-
#
#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# $Id: bsd.port.mk,v 1.188 1995/11/26 12:35:49 asami Exp $
#
# Please view me with 4 column tabs!


# Supported Variables and their behaviors:
#
# Variables that typically apply to all ports:
# 
# PORTSDIR		- The root of the ports tree (default: /usr/ports).
# DISTDIR 		- Where to get gzip'd, tarballed copies of original sources
#				  (default: ${PORTSDIR}/distfiles).
# PREFIX		- Where to install things in general (default: /usr/local).
# MASTER_SITES	- Primary location(s) for distribution files if not found
#				  locally (default:
#				   ftp://ftp.freebsd.org/pub/FreeBSD/distfiles)
# PATCH_SITES	- Primary location(s) for distributed patch files
#				  (see PATCHFILES below) if not found locally (default:
#				   ftp://ftp.freebsd.org/pub/FreeBSD/distfiles)
#
# MASTER_SITE_OVERRIDE - If set, override the MASTER_SITES setting with this
#				  value.
# MASTER_SITE_FREEBSD - If set, only use the FreeBSD master repository for
#				  MASTER_SITES.
# PACKAGES		- A top level directory where all packages go (rather than
#				  going locally to each port). (default: ${PORTSDIR}/packages).
# GMAKE			- Set to path of GNU make if not in $PATH (default: gmake).
# XMKMF			- Set to path of `xmkmf' if not in $PATH (default: xmkmf -a ).
# MAINTAINER	- The e-mail address of the contact person for this port
#				  (default: ports@FreeBSD.ORG).
# CATEGORIES	- A list of descriptive categories into which this port falls
#				  (default: orphans).
# KEYWORDS		- A list of descriptive keywords that might index well for this
#				  port (default: orphans).
#
# Variables that typically apply to an individual port.  Non-Boolean
# variables without defaults are *mandatory*.
# 
#
# WRKDIR 		- A temporary working directory that gets *clobbered* on clean
#				  (default: ${.CURDIR}/work).
# WRKSRC		- A subdirectory of ${WRKDIR} where the distribution actually
#				  unpacks to.  (Default: ${WRKDIR}/${DISTNAME} unless
#				  NO_WRKSUBDIR is set, in which case simply ${WRKDIR}).
# DISTNAME		- Name of port or distribution.
# DISTFILES		- Name(s) of archive file(s) containing distribution
#				  (default: ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX}).
# PATCHFILES	- Name(s) of additional files that contain distributed
#				  patches (default: none).  make will look for them at
#				  PATCH_SITES (see above).  They will automatically be
#				  uncompressed before patching if the names end with
#				  ".gz" or ".Z".
# PATCH_PRFX	- Filename prefix for distribution patches (default: none)
#				  typically ${DISTNAME}/ or foo-
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
#				  the "install" target.  This is the default if
#				  USE_IMAKE or USE_X11 is set.
#
# NO_EXTRACT	- Use a dummy (do-nothing) extract target.
# NO_CONFIGURE	- Use a dummy (do-nothing) configure target.
# NO_BUILD		- Use a dummy (do-nothing) build target.
# NO_PACKAGE	- Use a dummy (do-nothing) package target.
# NO_INSTALL	- Use a dummy (do-nothing) install target.
# NO_WRKSUBDIR	- Assume port unpacks directly into ${WRKDIR}.
# NO_WRKDIR		- There's no work directory at all; port does this someplace
#				  else.
# NO_DEPENDS	- Don't verify build of dependencies.
# USE_GMAKE		- Says that the port uses gmake.
# USE_IMAKE		- Says that the port uses imake.
# USE_X11		- Says that the port uses X11.
# NO_INSTALL_MANPAGES - For imake ports that don't like the install.man
#						target.
# HAS_CONFIGURE	- Says that the port has its own configure script.
# GNU_CONFIGURE	- Set if you are using GNU configure (optional).
# CONFIGURE_SCRIPT - Name of configure script, defaults to 'configure'.
# CONFIGURE_ARGS - Pass these args to configure, if ${HAS_CONFIGURE} set.
# CONFIGURE_ENV  - Pass these env (shell-like) to configure, if ${HAS_CONFIGURE} set.
# IS_INTERACTIVE - Set this if your port needs to interact with the user
#				  during a build.  User can then decide to skip this port by
#				  setting ${BATCH}, or compiling only the interactive ports
#				  by setting ${INTERACTIVE}.
# FETCH_DEPENDS - A list of "prog:dir" pairs of other ports this
#				  package depends in the "fetch" stage.  "prog" is the
#				  name of an executable.  make will search your $PATH
#				  for it and go into "dir" to do a "make all install"
#				  if it's not found.
# BUILD_DEPENDS - A list of "prog:dir" pairs of other ports this
#				  package depends to build (somewhere between the
#				  "extract" to "build" stage).  "prog" is the name
#				  of an executable.  make will search your $PATH for
#				  it and go into "dir" to do a "make all install" if
#				  it's not found.
# RUN_DEPENDS	- A list of "prog:dir" pairs of other ports this package
#				  depends to run.  "prog" is the name of an
#				  executable.  make will search your $PATH for it and
#				  go into "dir" to do a "make all install" if it's not
#				  found.  This will be build during the "install" stage
#				  and its name will be put into the package as well.
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
# NCFTP			- Full path to ncftp command if not in $PATH (default: ncftp).
# NCFTPFLAGS    - Arguments to ${NCFTP} (default: -N).
#
#
# Variables to change if you want a special behavior:
#
# ECHO_MSG		- Used to print all the '===>' style prompts - override this
#				  to turn them off (default: /bin/echo).
# IS_DEPENDED_TARGET -
#				  The target to execute when a port is called as a
#				  dependency (default: install).  E.g., "make fetch
#				  IS_DEPENDED_TARGET=fetch" will fetch all the distfiles,
#				  including those of dependencies, without actually building
#				  any of them).
#
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
# checksum		- Use files/md5 to ensure that your distfiles are valid
# makesum		- Generate files/md5 (only do this for your own ports!)
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

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# These need to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  They can, of course, be overridden
# by individual Makefiles.
PORTSDIR?=		${DESTDIR}/usr/ports
X11BASE?=		/usr/X11R6
DISTDIR?=		${PORTSDIR}/distfiles
PACKAGES?=		${PORTSDIR}/packages
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
PREFIX?=		/usr/local
.endif
# The following 4 lines should go away as soon as the ports are all updated
.if defined(EXEC_DEPENDS)
BUILD_DEPENDS+=	${EXEC_DEPENDS}
RUN_DEPENDS+=	${EXEC_DEPENDS}
.endif
.if defined(USE_GMAKE)
BUILD_DEPENDS+=               gmake:${PORTSDIR}/devel/gmake
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
DO_NADA?=		echo -n

# Miscellaneous overridable commands:
GMAKE?=			gmake
XMKMF?=			xmkmf -a
MD5?=			/sbin/md5
MD5_FILE?=		${FILESDIR}/md5
MAKE_FLAGS?=	-f
MAKEFILE?=		Makefile

NCFTP?=			/usr/bin/ncftp
NCFTPFLAGS?=	-N

TOUCH?=			/usr/bin/touch
TOUCH_FLAGS?=	-f

PATCH?=			/usr/bin/patch
PATCH_STRIP?=	-p0
PATCH_DIST_STRIP?=	-p0
.if defined(PATCH_DEBUG)
PATCH_ARGS?=	-d ${WRKSRC} -E ${PATCH_STRIP}
PATCH_DIST_ARGS?=	-d ${WRKSRC} -E ${PATCH_DIST_STRIP}
.else
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

EXTRACT_CMD?=	/usr/bin/tar
EXTRACT_SUFX?=	.tar.gz
# Backwards compatability.
.if defined(EXTRACT_ARGS)
EXTRACT_BEFORE_ARGS?=   ${EXTRACT_ARGS}
.else
EXTRACT_BEFORE_ARGS?=   -xzf
.endif

# Figure out where the local mtree file is
.if !defined(MTREE_LOCAL) && exists(/etc/mtree/BSD.local.dist)
MTREE_LOCAL=	/etc/mtree/BSD.local.dist
.endif
MTREE_CMD?=	/usr/sbin/mtree
MTREE_ARGS?=	-U -f ${MTREE_LOCAL} -d -e -p
.if defined(USE_X11) || defined(USE_IMAKE) || !defined(MTREE_LOCAL)
NO_MTREE=	yes
.endif

# The user can override the NO_PACKAGE by specifying this from
# the make command line
.if defined(FORCE_PACKAGE)
.undef NO_PACKAGE
.endif

PKG_CMD?=		/usr/sbin/pkg_create
.if !defined(PKG_ARGS)
PKG_ARGS=		-v -c ${PKGDIR}/COMMENT -d ${PKGDIR}/DESCR -f ${PKGDIR}/PLIST -p ${PREFIX} -P "`${MAKE} package-depends|sort|uniq`"
.if exists(${PKGDIR}/INSTALL)
PKG_ARGS+=		-i ${PKGDIR}/INSTALL
.endif
.if exists(${PKGDIR}/DEINSTALL)
PKG_ARGS+=		-k ${PKGDIR}/DEINSTALL
.endif
.if exists(${PKGDIR}/REQ)
PKG_ARGS+=		-r ${PKGDIR}/REQ
.endif
.if !defined(NO_MTREE) && defined(MTREE_LOCAL)
PKG_ARGS+=		-m ${MTREE_LOCAL}
.endif
.endif
PKG_SUFX?=		.tgz
# where pkg_add records its dirty deeds.
PKG_DBDIR?=		/var/db/pkg

# Used to print all the '===>' style prompts - override this to turn them off.
ECHO_MSG?=		/bin/echo

ALL_TARGET?=		all
INSTALL_TARGET?=	install

# If the user has this set, go to the FreeBSD respository for everything.
.if defined(MASTER_SITE_FREEBSD)
MASTER_SITE_OVERRIDE=  ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/ 
.endif

# I guess we're in the master distribution business! :)  As we gain mirror
# sites for distfiles, add them to this list.
.if !defined(MASTER_SITE_OVERRIDE)
MASTER_SITES+=	ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/
PATCH_SITES+=	ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/${PATCH_PRFX}
.else
MASTER_SITES:=	${MASTER_SITE_OVERRIDE} ${MASTER_SITES}
PATCH_SITES:=	${MASTER_SITE_OVERRIDE}${PATCH_PRFX} ${PATCH_SITES}
.endif

.if defined(PATCH_PRFX)
PATCHDIST!=	/bin/echo ${PATCH_PRFX} | sed 's|^\(.*\)/$$|/\1|'
PATCHDIST:=	${DISTDIR}${PATCHDIST}
.else
PATCHDIST:=	${DISTDIR}
.endif

# Derived names so that they're easily overridable.
DISTFILES?=		${DISTNAME}${EXTRACT_SUFX}
PKGNAME?=		${DISTNAME}

# This is what is actually going to be extracted, and is overridable
#  by user.
EXTRACT_ONLY?=	${DISTFILES}

# Documentation
MAINTAINER?=	ports@FreeBSD.ORG
CATEGORIES?=	orphans
KEYWORDS+=		${CATEGORIES}

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
CONFIGURE_ARGS?=	--prefix=${PREFIX}
HAS_CONFIGURE=		yes
.endif

.MAIN: all

################################################################
# If we're in BATCH mode and the port is interactive, or we're
# in interactive mode and the port is non-interactive, skip all
# the important targets.  The reason we have two modes is that
# one might want to leave a build in BATCH mode running
# overnight, then come back in the morning and do _only_ the
# interactive ones that required your intervention.
#
# This allows you to do both.
################################################################

.if (defined(IS_INTERACTIVE) && defined(BATCH)) || (!defined(IS_INTERACTIVE) && defined(INTERACTIVE))
all:
	@${DO_NADA}
build:
	@${DO_NADA}
install:
	@${DO_NADA}
fetch:
	@${DO_NADA}
configure:
	@${DO_NADA}
package:
	@${DO_NADA}
.endif

.if defined(ALL_HOOK)
all:
	@/usr/bin/env CURDIR=${.CURDIR} DISTNAME=${DISTNAME} \
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

.if !defined(IS_DEPENDED_TARGET)
IS_DEPENDED_TARGET=	install
.endif

.if !target(is_depended)
is_depended:	${IS_DEPENDED_TARGET}
.endif

################################################################
# The following are used to create easy dummy targets for
# disabling some bit of default target behavior you don't want.
# They still check to see if the target exists, and if so don't
# do anything, since you might want to set this globally for a
# group of ports in a Makefile.inc, but still be able to
# override from an individual Makefile.
################################################################

.if defined(NO_EXTRACT) && !target(extract)
extract: checksum
	@${TOUCH} ${TOUCH_FLAGS} ${EXTRACT_COOKIE}
checksum: fetch
	@${DO_NADA}
makesum:
	@${DO_NADA}
.endif
.if defined(NO_CONFIGURE) && !target(configure)
configure: patch
	@${TOUCH} ${TOUCH_FLAGS} ${CONFIGURE_COOKIE}
.endif
.if defined(NO_BUILD) && !target(build)
build: configure
	@${TOUCH} ${TOUCH_FLAGS} ${BUILD_COOKIE}
.endif
.if defined(NO_PACKAGE) && !target(package)
package:
	@${DO_NADA}
.endif
.if defined(NO_PACKAGE) && !target(repackage)
repackage:
	@${DO_NADA}
.endif
.if defined(NO_INSTALL) && !target(install)
install: build
	@${TOUCH} ${TOUCH_FLAGS} ${INSTALL_COOKIE}
.endif
.if defined(NO_PATCH) && !target(patch)
patch: extract
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
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
	@if [ ! -d ${DISTDIR} ]; then /bin/mkdir -p ${DISTDIR}; fi
	@(cd ${DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file -a ! -f `/usr/bin/basename $$file` ]; then \
			${ECHO_MSG} ">> $$file doesn't seem to exist on this system."; \
			for site in ${MASTER_SITES}; do \
			    ${ECHO_MSG} ">> Attempting to fetch from $${site}."; \
				(${NCFTP} ${NCFTPFLAGS} $${site}$${file} || true); \
				if [ -f $$file -o -f `/usr/bin/basename $$file` ]; then \
					continue 2; \
				fi \
			done; \
			${ECHO_MSG} ">> Couldn't fetch it - please try to retreive this";\
			${ECHO_MSG} ">> port manually into ${DISTDIR} and try again."; \
			exit 1; \
	    fi \
	 done)
.if defined(PATCHFILES)
	@if [ ! -d ${PATCHDIST} ]; then /bin/mkdir -p ${PATCHDIST}; fi
	@(cd ${PATCHDIST}; \
	 for file in ${PATCHFILES}; do \
		if [ ! -f $$file -a ! -f `/usr/bin/basename $$file` ]; then \
			${ECHO_MSG} ">> $$file doesn't seem to exist on this system."; \
			for site in ${PATCH_SITES}; do \
			    ${ECHO_MSG} ">> Attempting to fetch from $${site}."; \
				(${NCFTP} ${NCFTPFLAGS} $${site}$${file} || true); \
				if [ -f $$file -o -f `/usr/bin/basename $$file` ]; then \
					continue 2; \
				fi \
			done; \
			${ECHO_MSG} ">> Couldn't fetch it - please try to retreive this";\
			${ECHO_MSG} ">> port manually into ${PATCHDIST} and try again."; \
			exit 1; \
	    fi \
	 done)
.endif
.endif

# Extract

.if !target(do-extract)
do-extract:
	@/bin/rm -rf ${WRKDIR}
	@/bin/mkdir -p ${WRKDIR}
	@for file in ${EXTRACT_ONLY}; do \
		if ! (cd ${WRKDIR};${EXTRACT_CMD} ${EXTRACT_BEFORE_ARGS} ${DISTDIR}/$$file ${EXTRACT_AFTER_ARGS});\
		then \
			exit 1; \
		fi \
	done
.endif

# Patch

.if !target(do-patch)
do-patch:
.if defined(PATCHFILES)
	@${ECHO_MSG} "===>  Applying distributed patches for ${PKGNAME}"
.if defined(PATCH_DEBUG)
	@(cd ${PATCHDIST}; \
	  for i in ${PATCHFILES}; do \
		${ECHO_MSG} "===>   Applying distributed patch $$i" ; \
		case $$i in \
			*.Z|*.gz) \
				/usr/bin/gzcat $$i | ${PATCH} ${PATCH_DIST_ARGS}; \
				;; \
			*) \
				${PATCH} ${PATCH_DIST_ARGS} < $$i; \
				;; \
		esac; \
	  done)
.else
	@(cd ${PATCHDIST}; \
	  for i in ${PATCHFILES}; do \
		case $$i in \
			*.Z|*.gz) \
				/usr/bin/gzcat $$i | ${PATCH} ${PATCH_DIST_ARGS}; \
				;; \
			*) \
				${PATCH} ${PATCH_DIST_ARGS} < $$i; \
				;; \
		esac; \
	  done)
.endif
.endif
.if defined(PATCH_DEBUG)
	@if [ -d ${PATCHDIR} ]; then \
		${ECHO_MSG} "===>  Applying FreeBSD patches for ${PKGNAME}" ; \
		for i in ${PATCHDIR}/patch-*; do \
			case $$i in \
				*.orig|*~) \
					${ECHO_MSG} "===>   Ignoring patchfile $$i" ; \
					;; \
				*) \
					${ECHO_MSG} "===>   Applying FreeBSD patch $$i" ; \
					${PATCH} ${PATCH_ARGS} < $$i; \
					;; \
			esac; \
		done; \
	fi
.else
	@if [ -d ${PATCHDIR} ]; then \
		${ECHO_MSG} "===>  Applying FreeBSD patches for ${PKGNAME}" ; \
		for i in ${PATCHDIR}/patch-*; do \
			case $$i in \
				*.orig|*~) \
					${ECHO_MSG} "===>   Ignoring patchfile $$i" ; \
					;; \
				*) \
					${PATCH} ${PATCH_ARGS} < $$i; \
					;; \
			esac; \
		done;\
	fi
.endif
.endif

# Configure

.if !target(do-configure)
do-configure:
	@if [ -f ${SCRIPTDIR}/configure ]; then \
		/usr/bin/env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" X11BASE=${X11BASE} \
		/bin/sh ${SCRIPTDIR}/configure; \
	fi
.if defined(HAS_CONFIGURE)
	@(cd ${WRKSRC}; CC="${CC}" ac_cv_path_CC="${CC}" CFLAGS="${CFLAGS}" \
	    INSTALL="/usr/bin/install -c -o ${BINOWN} -g ${BINGRP}" \
	    INSTALL_PROGRAM="/usr/bin/install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}" \
	    ${CONFIGURE_ENV} ./${CONFIGURE_SCRIPT} ${CONFIGURE_ARGS})
.endif
.if defined(USE_IMAKE)
	@(cd ${WRKSRC}; ${XMKMF})
.endif
.endif

# Build

.if !target(do-build)
do-build:
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} PREFIX=${PREFIX} X11BASE=${X11BASE} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} PREFIX=${PREFIX} X11BASE=${X11BASE} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.endif
.endif

# Install

.if !target(do-install)
do-install:
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} PREFIX=${PREFIX} X11BASE=${X11BASE} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
	@(cd ${WRKSRC}; ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} install.man)
.endif
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} PREFIX=${PREFIX} X11BASE=${X11BASE} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
	@(cd ${WRKSRC}; ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} install.man)
.endif
.endif
.endif

# Package

.if !target(do-package)
do-package:
	@if [ -e ${PKGDIR}/PLIST ]; then \
		${ECHO_MSG} "===>  Building package for ${PKGNAME}"; \
		if [ -d ${PACKAGES} ]; then \
			if [ ! -d ${PKGREPOSITORY} ]; then \
				if ! /bin/mkdir -p ${PKGREPOSITORY}; then \
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
			if ! /bin/mkdir -p ${PACKAGES}/$$cat; then \
				${ECHO_MSG} ">> Can't create directory ${PACKAGES}/$$cat."; \
				exit 1; \
			fi; \
		fi; \
		ln -s ../${PKGREPOSITORYSUBDIR}/${PKGNAME}${PKG_SUFX} ${PACKAGES}/$$cat; \
	done;
.endif

.if !target(delete-package-links)
delete-package-links:
	@/bin/rm -f ${PACKAGES}/[a-z]*/${PKGNAME}${PKG_SUFX};
.endif

.if !target(delete-package)
delete-package:
	@${MAKE} ${.MAKEFLAGS} delete-package-links
	@/bin/rm -f ${PKGFILE}
.endif

################################################################
# This is the "generic" port target, actually a macro used from the
# six main targets.  See below for more.
################################################################

_PORT_USE: .USE
.if make(real-fetch)
	@${MAKE} ${.MAKEFLAGS} fetch-depends
.endif
.if make(real-extract)
	@${MAKE} ${.MAKEFLAGS} build-depends lib-depends misc-depends
.endif
.if make(real-install)
	@${MAKE} ${.MAKEFLAGS} run-depends
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
	@${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/pre-/}
	@if [ -f ${SCRIPTDIR}/${.TARGET:S/^real-/pre-/} ]; then \
		/usr/bin/env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" X11BASE=${X11BASE} \
			/bin/sh ${SCRIPTDIR}/${.TARGET:S/^real-/pre-/}; \
	fi
	@${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/do-/}
	@${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/post-/}
	@if [ -f ${SCRIPTDIR}/${.TARGET:S/^real-/post-/} ]; then \
		/usr/bin/env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" X11BASE=${X11BASE} \
			/bin/sh ${SCRIPTDIR}/${.TARGET:S/^real-/post-/}; \
	fi
.if make(real-install)
	@${MAKE} ${.MAKEFLAGS} fake-pkg
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
	@${MAKE} ${.MAKEFLAGS} real-fetch
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
	@${MAKE} ${.MAKEFLAGS} real-extract
${PATCH_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} real-patch
${CONFIGURE_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} real-configure
${BUILD_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} real-build
${INSTALL_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} real-install
${PACKAGE_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} real-package

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
	@${MAKE} PATCH_CHECK_ONLY=yes ${.MAKEFLAGS} patch
.endif

# Reinstall
#
# Special target to re-run install

.if !target(reinstall)
reinstall: pre-reinstall install

pre-reinstall:
	@/bin/rm -f ${INSTALL_COOKIE}
	@/bin/rm -f ${PACKAGE_COOKIE}
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
	@${ECHO_MSG} "===>  Cleaning for ${PKGNAME}"
	@/bin/rm -f ${EXTRACT_COOKIE} ${CONFIGURE_COOKIE} ${INSTALL_COOKIE} \
		${BUILD_COOKIE} ${PATCH_COOKIE}
.if defined(NO_WRKDIR)
	@/bin/rm -f ${WRKDIR}/.*_done
.else
	@/bin/rm -rf ${WRKDIR}
.endif
.endif

# Prints out a list of files to fetch (useful to do a batch fetch)

.if !target(fetch-list)
fetch-list:
	@if [ ! -d ${DISTDIR} ]; then /bin/mkdir -p ${DISTDIR}; fi
	@(cd ${DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file -a ! -f `/usr/bin/basename $$file` ]; then \
			for site in ${MASTER_SITES}; do \
				/bin/echo -n ${NCFTP} ${NCFTPFLAGS} $${site}$${file} '||' ; \
					break; \
			done; \
			/bin/echo "echo $${file} not fetched" ; \
		fi \
	done)
.if defined(PATCHFILES)
	@(cd ${DISTDIR}; \
	 for file in ${PATCHFILES}; do \
		if [ ! -f $$file -a ! -f `/usr/bin/basename $$file` ]; then \
			for site in ${PATCH_SITES}; do \
				/bin/echo -n ${NCFTP} ${NCFTPFLAGS} $${site}$${file} ${PATCH_PRFX}$${file} '||' ; \
					break; \
			done; \
			/bin/echo "echo $${file} not fetched" ; \
		fi \
	 done)
.endif
.endif

# Checksumming utilities

.if !target(makesum)
makesum: fetch
	@if [ ! -d ${FILESDIR} ]; then /bin/mkdir -p ${FILESDIR}; fi
	@if [ -f ${MD5_FILE} ]; then /bin/rm -f ${MD5_FILE}; fi
	@(cd ${DISTDIR}; \
	 for file in ${DISTFILES} ${PATCHFILES:S|^|${PATCH_PRFX}|}; do \
		${MD5} $$file >> ${MD5_FILE}; \
	 done)
.endif

.if !target(checksum)
checksum: fetch
	@if [ ! -f ${MD5_FILE} ]; then \
		${ECHO_MSG} ">> No MD5 checksum file."; \
	else \
		(cd ${DISTDIR}; OK=""; \
		  for file in ${DISTFILES} ${PATCHFILES:S|^|${PATCH_PRFX}|}; do \
			CKSUM=`${MD5} $$file | awk '{print $$4}'`; \
			CKSUM2=`grep "($$file)" ${MD5_FILE} | awk '{print $$4}'`; \
			if [ "$$CKSUM2" = "" ]; then \
				${ECHO_MSG} ">> No checksum recorded for $$file"; \
				OK="false"; \
			elif [ "$$CKSUM" != "$$CKSUM2" ]; then \
				${ECHO_MSG} ">> Checksum mismatch for $$file"; \
				exit 1; \
			fi; \
		  done; \
		  if [ "$$OK" = "" ]; then \
			${ECHO_MSG} "Checksums OK."; \
		  else \
			${ECHO_MSG} "Checksums OK for files that have them."; \
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
.if !defined(NO_PACKAGE)
	@/bin/echo ${PKGNAME}
.endif
.endif

# Show (recursively) all the packages this package depends on.

.if !target(package-depends)
package-depends:
	@for i in ${RUN_DEPENDS} ${LIB_DEPENDS} ${DEPENDS}; do \
		dir=`/bin/echo $$i | /usr/bin/sed -e 's/.*://'`; \
		(cd $$dir ; ${MAKE} package-name package-depends); \
	done
.endif

# Build a package but don't check the package cookie

.if !target(repackage)
repackage: pre-repackage package

pre-repackage:
	@/bin/rm -f ${PACKAGE_COOKIE}
.endif

# Build a package but don't check the cookie for installation, also don't
# install package cookie

.if !target(package-noinstall)
package-noinstall:
	@${MAKE} ${.MAKEFLAGS} PACKAGE_NOINSTALL=yes real-package
.endif

################################################################
# Dependency checking
################################################################

.if !target(depends)
depends: lib-depends misc-depends
	@${MAKE} ${.MAKEFLAGS} fetch-depends
	@${MAKE} ${.MAKEFLAGS} build-depends
	@${MAKE} ${.MAKEFLAGS} run-depends

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
.if defined(NO_DEPENDS)
# Just print out messages
	@for i in ${DEPENDS_TMP}; do \
		prog=`/bin/echo $$i | /usr/bin/sed -e 's/:.*//'`; \
		dir=`/bin/echo $$i | /usr/bin/sed -e 's/.*://'`; \
		if expr "$$prog" : \\/ >/dev/null; then \
			${ECHO_MSG} "===>  ${PKGNAME} depends on file:  $$prog ($$dir)"; \
		else \
			${ECHO_MSG} "===>  ${PKGNAME} depends on executable:  $$prog ($$dir)"; \
		fi; \
	done
.else
	@for i in ${DEPENDS_TMP}; do \
		prog=`/bin/echo $$i | /usr/bin/sed -e 's/:.*//'`; \
		dir=`/bin/echo $$i | /usr/bin/sed -e 's/.*://'`; \
		if expr "$$prog" : \\/ >/dev/null; then \
			if [ -e "$$prog" ]; then \
				${ECHO_MSG} "===>  ${PKGNAME} depends on file: $$prog - found"; \
				notfound=0; \
			else \
				${ECHO_MSG} "===>  ${PKGNAME} depends on file: $$prog - not found"; \
				notfound=1; \
			fi; \
		else \
			if which -s "$$prog"; then \
				${ECHO_MSG} "===>  ${PKGNAME} depends on executable: $$prog - found"; \
				notfound=0; \
			else \
				${ECHO_MSG} "===>  ${PKGNAME} depends on executable: $$prog - not found"; \
				notfound=1; \
			fi; \
		fi; \
		if [ $$notfound != 0 ]; then \
			${ECHO_MSG} "===>  Verifying build for $$prog in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$prog.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} is_depended) ; \
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
.if defined(NO_DEPENDS)
# Just print out messages
	@for i in ${LIB_DEPENDS}; do \
		lib=`/bin/echo $$i | /usr/bin/sed -e 's/:.*//'`; \
		dir=`/bin/echo $$i | /usr/bin/sed -e 's/.*://'`; \
		${ECHO_MSG} "===>  ${PKGNAME} depends on shared library:  $$lib ($$dir)"; \
	done
.else
	@for i in ${LIB_DEPENDS}; do \
		lib=`/bin/echo $$i | /usr/bin/sed -e 's/:.*//'`; \
		dir=`/bin/echo $$i | /usr/bin/sed -e 's/.*://'`; \
		if /sbin/ldconfig -r | grep -q -e "-l$$lib"; then \
			${ECHO_MSG} "===>  ${PKGNAME} depends on shared library: $$lib - found"; \
		else \
			${ECHO_MSG} "===>  ${PKGNAME} depends on shared library: $$lib - not found"; \
			${ECHO_MSG} "===>  Verifying build for $$lib in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$lib.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} is_depended) ; \
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
	@${ECHO_MSG} "===>  ${PKGNAME} depends on:  ${DEPENDS}"
.if !defined(NO_DEPENDS)
	@for i in ${DEPENDS}; do \
		${ECHO_MSG} "===>  Verifying build for $$i"; \
		if [ ! -d $$i ]; then \
			${ECHO_MSG} ">> No directory for $$i.  Skipping.."; \
		else \
			(cd $$i; ${MAKE} ${.MAKEFLAGS} is_depended) ; \
		fi \
	done
	@${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"
.endif
.else
	@${DO_NADA}
.endif

.endif

################################################################
# Everything after here are internal targets and really
# shouldn't be touched by anybody but the release engineers.
################################################################

# This target generates an index entry suitable for aggregation into
# a large index.  Format is:
#
# distribution-name|port-path|installation-prefix|comment| \
#  description-file|maintainer|categories|keywords
#
.if !target(describe)
describe:
	@/bin/echo -n "${PKGNAME}|${.CURDIR}/${PKGNAME}|"
	@/bin/echo -n "${PREFIX}|"
	@if [ -f ${PKGDIR}/COMMENT ]; then \
		/bin/echo -n "`/bin/cat ${PKGDIR}/COMMENT`"; \
	else \
		/bin/echo -n "** No Description"; \
	fi
	@if [ -f ${PKGDIR}/DESCR ]; then \
		/bin/echo -n "|${PKGDIR}/DESCR"; \
	else \
		/bin/echo -n "|/dev/null"; \
	fi
	@/bin/echo -n "|${MAINTAINER}|${CATEGORIES}|${KEYWORDS}"
	@/bin/echo ""
.endif

# Fake installation of package so that user can pkg_delete it later.
# Also, make sure that an installed port is recognized correctly in
# accordance to the @pkgdep directive in the packing lists

.if !target(fake-pkg)
fake-pkg:
	@if [ ! -f ${PKGDIR}/PLIST -o ! -f ${PKGDIR}/COMMENT -o ! -f ${PKGDIR}/DESCR ]; then /bin/echo "** Missing package files for ${PKGNAME} - installation not recorded."; exit 1; fi
	@if [ ! -d ${PKG_DBDIR} ]; then /bin/rm -f ${PKG_DBDIR}; /bin/mkdir -p ${PKG_DBDIR}; fi
.if defined(FORCE_PKG_REGISTER)
	@/bin/rm -rf ${PKG_DBDIR}/${PKGNAME}
.endif
	@if [ ! -d ${PKG_DBDIR}/${PKGNAME} ]; then \
		${ECHO_MSG} "===>  Registering installation for ${PKGNAME}"; \
		/bin/mkdir -p ${PKG_DBDIR}/${PKGNAME}; \
		${PKG_CMD} ${PKG_ARGS} -O ${PKGFILE} > ${PKG_DBDIR}/${PKGNAME}/+CONTENTS; \
		/bin/cp ${PKGDIR}/DESCR ${PKG_DBDIR}/${PKGNAME}/+DESC; \
		/bin/cp ${PKGDIR}/COMMENT ${PKG_DBDIR}/${PKGNAME}/+COMMENT; \
		if [ -f ${PKGDIR}/INSTALL ]; then \
			/bin/cp ${PKGDIR}/INSTALL ${PKG_DBDIR}/${PKGNAME}/+INSTALL; \
		fi; \
		if [ -f ${PKGDIR}/DEINSTALL ]; then \
			/bin/cp ${PKGDIR}/DEINSTALL ${PKG_DBDIR}/${PKGNAME}/+DEINSTALL; \
		fi; \
		if [ -f ${PKGDIR}/REQ ]; then \
			/bin/cp ${PKGDIR}/REQ ${PKG_DBDIR}/${PKGNAME}/+REQ; \
		fi; \
	else \
		${ECHO_MSG} "===> ${PKGNAME} is already installed - perhaps an older version?"; \
		${ECHO_MSG} "     If so, you may wish to \`\`pkg_delete ${PKGNAME}'' and install"; \
		${ECHO_MSG} "     this port again to upgrade it properly."; \
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
