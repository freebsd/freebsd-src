#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# $Id: bsd.port.mk,v 1.21 1994/08/28 14:41:34 jkh Exp $

#
# Supported Variables and their behaviors:
#
# Variables that typically apply to all ports:
# 
# PORTSDIR	- The root of the ports tree (default: /usr/ports).
# DISTDIR 	- Where to get gzip'd, tarballed copies of original sources
#		- (default: ${PORTSDIR}/distfiles.
# PACKAGES	- A top level directory where all packages go (rather than
#		- going someplace locally). (default: ${PORTSDIR}/packages).
# GMAKE		- Set to path of GNU make if not in $PATH (default: gmake).
# XMKMF		- Set to path of `xmkmf' if not in $PATH (default: xmkmf).
#
# Variables that typically apply to an individual port:
#
# WRKDIR 	- A temporary working directory that gets *clobbered* on clean.
# WRKSRC	- A subdirectory of ${WRKDIR} where the distribution actually
#		  unpacks to.  Defaults to ${WRKDIR}/${DISTNAME}.
# DISTNAME	- Name of port or distribution.
# PATCHDIR 	- A directory containing required patches.
# SCRIPTDIR 	- A directory containing auxilliary scripts.
# FILESDIR 	- A directory containing any miscellaneous additional files.
# PKGDIR 	- Package creation files.
#
# NO_EXTRACT	- Use a dummy (do-nothing) extract target.
# NO_CONFIGURE	- Use a dummy (do-nothing) configure target.
# NO_BUILD	- Use a dummy (do-nothing) build target.
# NO_PACKAGE	- Use a dummy (do-nothing) package target.
# NO_INSTALL	- Use a dummy (do-nothing) install target.
# USE_GMAKE	- Says that the port uses gmake.
# USE_IMAKE	- Says that the port uses imake.
# HAS_CONFIGURE	- Says that the port has its own configure script.
# CONFIGURE_ARGS - Pass these args to configure, if $HAS_CONFIGURE.
# HOME_LOCATION	- site/path name (or user's email address) describing
#		  where this port came from or can be obtained if the
#		  tarball is missing.
# DEPENDS	- A list of other ports this package depends on being
#		  made first, relative to ${PORTSDIR} (e.g. x11/tk, lang/tcl,
#		  etc).
# 
#
# Default targets and their behaviors:
#
# extract	- Unpacks ${DISTDIR}/${DISTNAME}.tar.gz into ${WRKDIR}.
# configure	- Applys patches, if any, and runs either GNU configure, one
#		  or more local configure scripts or nothing, depending on
#		  what's available.
# build		- Actually compile the sources.
# install	- Install the results of a build.
# package	- Create a package from a build.
# bundle	- From an unextracted source tree, re-create tarballs.


.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

GMAKE?=		gmake

# These need to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  They can, of course, be overridden
# by individual Makefiles.
PORTSDIR?=	/usr/ports
DISTDIR?=	${PORTSDIR}/distfiles
PACKAGES?=	${PORTSDIR}/packages

WRKDIR?=	${.CURDIR}/work
WRKSRC?=	${WRKDIR}/${DISTNAME}
PATCHDIR?=	${.CURDIR}/patches
SCRIPTDIR?=	${.CURDIR}/scripts
FILESDIR?=	${.CURDIR}/files
PKGDIR?=	${.CURDIR}/pkg

# Change these if you'd prefer to keep the cookies someplace else.
EXTRACT_COOKIE?=	${.CURDIR}/.extract_done
CONFIGURE_COOKIE?=	${.CURDIR}/.configure_done

# How to do nothing.  Override if you, for some strange reason, would rather
# do something.
DO_NADA?=		echo -n

# Miscellaneous overridable commands:
EXTRACT_CMD?=	tar
EXTRACT_SUFX?=	.tar.gz
EXTRACT_ARGS?=	-C ${WRKDIR} -xzf

BUNDLE_CMD?=	tar
BUNDLE_ARGS?=	-C ${WRKDIR} -czf

PKG_CMD?=	pkg_create
PKG_ARGS?=	-v -c ${PKGDIR}/COMMENT -d ${PKGDIR}/DESCR -f ${PKGDIR}/PLIST
PKG_SUFX?=	.tgz

HOME_LOCATION?=	<original site unknown>

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
	@(cd ${WRKSRC}; ${GMAKE} install)
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} install)
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
	   if [ -d ${PACKAGES} ]; then \
	   	echo "===>  Building package for ${DISTNAME} in ${PACKAGES}"; \
		${PKG_CMD} ${PKG_ARGS} ${PACKAGES}/${DISTNAME}${PKG_SUFX}; \
	   else \
	   	echo "===>  Building package for ${DISTNAME} in ${.CURDIR}"; \
	   	${PKG_CMD} ${PKG_ARGS} ${DISTNAME}${PKG_SUFX}; \
	   fi; \
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
	@for i in $(DEPENDS); do \
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
	@(cd ${WRKSRC}; ${GMAKE} all)
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} all)
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
	@(cd ${WRKSRC}; ${XMKMF} && make Makefiles)
.endif
	@if [ -f ${SCRIPTDIR}/post-configure ]; then \
	   sh ${SCRIPTDIR}/post-configure ${PORTSDIR} ${.CURDIR} ${WRKSRC}; \
	fi
	@touch -f ${CONFIGURE_COOKIE}
.endif

.if !target(pre-bundle)
pre-bundle:
	@${DO_NADA}
.endif

.if !target(bundle)
bundle: pre-bundle
	@echo "===>  Bundling for ${DISTNAME}"
	@if [ ! -f ${EXTRACT_COOKIE} ]; then \
	   echo ">> There doesn't appear to be a properly extracted"; \
	   echo ">> distribution for ${DISTNAME}. Skipping.."; \
	   exit 0; \
	fi
	@if [ -f ${CONFIGURE_COOKIE} ]; then \
	   echo ">> WARNING:  This source has been configured and may"; \
	   echo ">> produce a tainted distfile!"; \
	fi
	${BUNDLE_CMD} ${BUNDLE_ARGS} ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX} \
		${DISTNAME}
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
extract: pre-extract ${EXTRACT_COOKIE}

${EXTRACT_COOKIE}:
	@echo "===>  Extracting for ${DISTNAME}"
	@rm -rf ${WRKDIR}
	@mkdir -p ${WRKDIR}
	@if [ ! -f ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX} ]; then \
	   echo ">> Sorry, can't find: ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX}"; \
	   echo ">> Please obtain this file from:"; \
	   echo ">>	${HOME_LOCATION}"; \
	   echo ">>before proceeding."; \
	   exit 1; \
	fi
	@${EXTRACT_CMD} ${EXTRACT_ARGS} ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX}
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
