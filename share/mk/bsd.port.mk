# -*- mode: Fundamental; tab-width: 4; -*-
#
#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# $Id: bsd.port.mk,v 1.33 1994/09/11 12:55:54 jkh Exp $
#
# Please view me with 4 column tabs!


# Supported Variables and their behaviors:
#
# Variables that typically apply to all ports:
# 
# PORTSDIR		- The root of the ports tree (default: /usr/ports).
# DISTDIR 		- Where to get gzip'd, tarballed copies of original sources
#				  (default: ${PORTSDIR}/distfiles).
# MASTER_SITE	- Primary location for distribution files if not found
#				  locally.
# PACKAGES		- A top level directory where all packages go (rather than
#				  going locally to each port). (default: ${PORTSDIR}/packages).
# GMAKE			- Set to path of GNU make if not in $PATH (default: gmake).
# XMKMF			- Set to path of `xmkmf' if not in $PATH (default: xmkmf).
#
# Variables that typically apply to an individual port:
#
# WRKDIR 		- A temporary working directory that gets *clobbered* on clean.
# WRKSRC		- A subdirectory of ${WRKDIR} where the distribution actually
#				  unpacks to.  (Default: ${WRKDIR}/${DISTNAME} unless
#				  NO_WRKSUBDIR is set, in which case simply ${WRKDIR}).
# DISTNAME		- Name of port or distribution.
# DISTFILES		- Name(s) of archive file(s) containing distribution
#				  (default: ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX}).
# EXTRACT_ONLY		- If defined, a subset of ${DISTFILES} you want to
#			  actually extract.
# PATCHDIR 		- A directory containing any required patches.
# SCRIPTDIR 	- A directory containing any auxilliary scripts.
# FILESDIR 		- A directory containing any miscellaneous additional files.
# PKGDIR 		- A direction containing any package creation files.
#
# NO_EXTRACT	- Use a dummy (do-nothing) extract target.
# NO_CONFIGURE	- Use a dummy (do-nothing) configure target.
# NO_BUILD		- Use a dummy (do-nothing) build target.
# NO_PACKAGE	- Use a dummy (do-nothing) package target.
# NO_INSTALL	- Use a dummy (do-nothing) install target.
# NO_WRKSUBDIR	- Assume port unpacks directly into ${WRKDIR}.
# USE_GMAKE		- Says that the port uses gmake.
# USE_IMAKE		- Says that the port uses imake.
# HAS_CONFIGURE	- Says that the port has its own configure script.
# CONFIGURE_ARGS - Pass these args to configure, if $HAS_CONFIGURE.
# HOME_LOCATION	- site/path name (or user's email address) describing
#				  where this port came from or can be obtained if the
#				  tarball is missing and there is no MASTER_SITE.
# DEPENDS		- A list of other ports this package depends on being
#				  made first, relative to ${PORTSDIR} (e.g. x11/tk, lang/tcl,
#				  etc).
# EXTRACT_CMD	- Command for extracting archive (default: tar).
# EXTRACT_SUFX	- Suffix for archive names (default: .tar.gz).
# EXTRACT_ARGS	- Arguments to ${EXTRACT_CMD} (default: -C ${WRKDIR} -xzf).
#
# NCFTP			- Full path to ncftp command if not in $PATH (default: ncftp).
# NCFTP_ARGS	- Arguments to ${NCFTP} (default: -N).
#
#
# Default targets and their behaviors:
#
# fetch			- Retrieves ${DISTFILES} into ${DISTDIR} as necessary.
# extract		- Unpacks ${DISTFILES} into ${WRKDIR}.
# configure		- Applies patches, if any, and runs either GNU configure, one
#				  or more local configure scripts or nothing, depending on
#				  what's available.
# build			- Actually compile the sources.
# install		- Install the results of a build.
# package		- Create a package from a build.
#
# Default sequence for "all" is:  fetch extract configure build

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# These need to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  They can, of course, be overridden
# by individual Makefiles.
PORTSDIR?=		/usr/ports
DISTDIR?=		${PORTSDIR}/distfiles
PACKAGES?=		${PORTSDIR}/packages
WRKDIR?=		${.CURDIR}/work
.if defined(NO_WRKSUBDIR)
WRKSRC?=		${WRKDIR}
.else
WRKSRC?=		${WRKDIR}/${DISTNAME}
.endif
PATCHDIR?=		${.CURDIR}/patches
SCRIPTDIR?=		${.CURDIR}/scripts
FILESDIR?=		${.CURDIR}/files
PKGDIR?=		${.CURDIR}/pkg

.if exists(${PORTSDIR}/../Makefile.inc)
.include "${PORTSDIR}/../Makefile.inc"
.endif


# Change these if you'd prefer to keep the cookies someplace else.
EXTRACT_COOKIE?=	${.CURDIR}/.extract_done
CONFIGURE_COOKIE?=	${.CURDIR}/.configure_done

# How to do nothing.  Override if you, for some strange reason, would rather
# do something.
DO_NADA?=		echo -n

# Miscellaneous overridable commands:
GMAKE?=			gmake
XMKMF?=			xmkmf
MAKE_FLAGS?=	-f
MAKEFILE?=		Makefile

NCFTP?=			ncftp
NCFTPFLAGS?=	-N

EXTRACT_CMD?=	tar
EXTRACT_SUFX?=	.tar.gz
EXTRACT_ARGS?=	-C ${WRKDIR} -xzf

PKG_CMD?=		pkg_create
PKG_ARGS?=		-v -c ${PKGDIR}/COMMENT -d ${PKGDIR}/DESCR -f ${PKGDIR}/PLIST
PKG_SUFX?=		.tgz

# I guess we're in the master distribution business! :)  As we gain mirror
# sites for distfiles, add them to this list.
MASTER_SITES+=	ftp://freebsd.cdrom.com/pub/FreeBSD/FreeBSD-current/ports/distfiles

# Derived names so that they're easily overridable.
DISTFILES?=		${DISTNAME}${EXTRACT_SUFX}

.if exists(${PACKAGES})
PKGFILE?=		${PACKAGES}/${DISTNAME}${PKG_SUFX}
.else
PKGFILE?=		${DISTNAME}${PKG_SUFX}
.endif

.MAIN: all
all: extract configure build

# The following are used to create easy dummy targets for disabling some
# bit of default target behavior you don't want.  They still check to see
# if the target exists, and if so don't do anything, since you might want
# to set this globally for a group of ports in a Makefile.inc, but still
# be able to override from an individual Makefile (since you can't _undefine_
# a variable in make!).
.if defined(NO_EXTRACT) && !target(extract)
extract:
	@touch -f ${EXTRACT_COOKIE}
.endif
.if defined(NO_CONFIGURE) && !target(configure)
configure:
	@touch -f ${CONFIGURE_COOKIE}
.endif
.if defined(NO_BUILD) && !target(build)
build:
	@${DO_NADA}
.endif
.if defined(NO_PACKAGE) && !target(package)
package:
	@${DO_NADA}
.endif
.if defined(NO_INSTALL) && !target(install)
install:
	@${DO_NADA}
.endif

# More standard targets start here.

.if !target(pre-install)
pre-install:
	@${DO_NADA}
.endif

.if !target(install)
install: pre-install
	@echo "===>  Installing for ${DISTNAME}"
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} install)
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} install)
.endif
.endif

.if !target(pre-package)
pre-package:
	@${DO_NADA}
.endif

.if !target(package)
package: pre-package
# Makes some gross assumptions about a fairly simple package with no
# install, require or deinstall scripts.  Override the arguments with
# PKG_ARGS if your package is anything but run-of-the-mill.
	@if [ -d ${PKGDIR} ]; then \
		echo "===>  Building package for ${DISTNAME}"; \
		${PKG_CMD} ${PKG_ARGS} ${PKGFILE}; \
	fi
.endif

.if !target(pre-build)
pre-build:
	@${DO_NADA}
.endif

.if !target(build)
build: configure pre-build
	@echo "===>  Building for ${DISTNAME}"
.if defined(DEPENDS)
	@echo "===>  ${DISTNAME} depends on:  ${DEPENDS}"
	@for i in ${DEPENDS}; do \
		echo "===>  Verifying build for $$i"; \
		if [ ! -d ${PORTSDIR}/$$i ]; then \
			echo ">> No directory for ${PORTSDIR}/$$i.  Skipping.."; \
		else \
			(cd ${PORTSDIR}/$$i; ${MAKE}) ; \
		fi \
	done
	@echo "===>  Returning to build of ${DISTNAME}"
.endif
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} all)
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} all)
.endif
	@if [ -f ${SCRIPTDIR}/post-build ]; then \
		sh ${SCRIPTDIR}/post-build ${PORTSDIR} ${.CURDIR} ${WRKSRC}; \
	fi
.endif

.if !target(pre-configure)
pre-configure:
	@${DO_NADA}
.endif

.if !target(configure)
# This is done with a .configure because configures are often expensive,
# and you don't want it done again gratuitously when you're trying to get
# a make of the whole tree to work.
configure: pre-configure extract ${CONFIGURE_COOKIE}

${CONFIGURE_COOKIE}:
	@echo "===>  Configuring for ${DISTNAME}"
	@if [ -d ${PATCHDIR} ]; then \
		echo "===>  Applying patches for ${DISTNAME}" ; \
		for i in ${PATCHDIR}/patch-*; do \
			patch -d ${WRKSRC} --quiet -E -p0 < $$i; \
	done; \
	fi
# We have a small convention for our local configure scripts, which
# is that ${PORTSDIR}, ${.CURDIR} and ${WRKSRC} get passed as
# command-line arguments since all other methods are a little
# problematic.
	@if [ -f ${SCRIPTDIR}/pre-configure ]; then \
		sh ${SCRIPTDIR}/pre-configure ${PORTSDIR} ${.CURDIR} ${WRKSRC}; \
	fi
	@if [ -f ${SCRIPTDIR}/configure ]; then \
		sh ${SCRIPTDIR}/configure ${PORTSDIR} ${.CURDIR} ${WRKSRC}; \
	fi
.if defined(HAS_CONFIGURE)
	@(cd ${WRKSRC}; ./configure ${CONFIGURE_ARGS})
.endif
.if defined(USE_IMAKE)
	@(cd ${WRKSRC}; ${XMKMF} && ${MAKE} Makefiles)
.endif
	@if [ -f ${SCRIPTDIR}/post-configure ]; then \
		sh ${SCRIPTDIR}/post-configure ${PORTSDIR} ${.CURDIR} ${WRKSRC}; \
	fi
	@touch -f ${CONFIGURE_COOKIE}
.endif

.if !target(pre-fetch)
pre-fetch:
	@${DO_NADA}
.endif

.if !target(fetch)
fetch: pre-fetch
	@if [ ! -d ${DISTDIR} ]; then mkdir -p ${DISTDIR}; fi
	@(cd ${DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file ]; then \
			echo ">> $$file doesn't seem to exist on this system."; \
			echo ">> Attempting to fetch it from a master site."; \
			for site in ${MASTER_SITES}; do \
				if ${NCFTP} ${NCFTPFLAGS} $$site/$$file; then \
					echo ">> $$file Fetched!" ; \
					break; \
				fi \
			done \
			if [ ! -f $$file ]; then \
				echo ">> Couldn't fetch it - please try to retreive this";\
				echo ">> port manually into ${DISTDIR} and try again."; \
				exit 1; \
			fi; \
	    fi \
	 done)
.endif

.if !target(pre-extract)
pre-extract:
	@${DO_NADA}
.endif

.if !target(extract)
# We need to depend on .extract_done rather than the presence of ${WRKDIR}
# because if the user interrupts the extract in the middle (and it's often
# a long procedure), we get tricked into thinking that we've got a good dist
# in ${WRKDIR}.
extract: fetch pre-extract ${EXTRACT_COOKIE}

${EXTRACT_COOKIE}:
	@echo "===>  Extracting for ${DISTNAME}"
	@rm -rf ${WRKDIR}
	@mkdir -p ${WRKDIR}
.if defined(EXTRACT_ONLY)
	@for file in ${EXTRACT_ONLY}; do \
		${EXTRACT_CMD} ${EXTRACT_ARGS} ${DISTDIR}/$$file ; \
	done
.else
	@for file in ${DISTFILES}; do \
		${EXTRACT_CMD} ${EXTRACT_ARGS} ${DISTDIR}/$$file ; \
	done
.endif
	@touch -f ${EXTRACT_COOKIE}
.endif

.if !target(pre-clean)
pre-clean:
	@${DO_NADA}
.endif

.if !target(clean)
clean: pre-clean
	@echo "===>  Cleaning for ${DISTNAME}"
	@rm -f ${EXTRACT_COOKIE} ${CONFIGURE_COOKIE}
	@rm -rf ${WRKDIR}
.endif

# No pre-targets for depend or tags.  It would be silly.

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
