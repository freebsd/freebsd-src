#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# $Id: bsd.port.mk,v 1.10 1994/08/22 11:20:06 jkh Exp $

#
# Supported Variables and their behaviors:
#
# GMAKE		- Set to path of GNU make if not in $PATH.
# DISTDIR 	- Where to get gzip'd, tarballed copies of original sources.
# DISTNAME	- Name of package or distribution.
# WRKDIR 	- A temporary working directory that gets *clobbered* on clean.
# WRKSRC	- A subdirectory of ${WRKDIR} where the distribution actually
#		  unpacks to.  Defaults to ${WRKDIR}/${DISTNAME}.
# PATCHDIR 	- A directory containing required patches.
# SCRIPTDIR 	- A directory containing auxilliary scripts.
# FILESDIR 	- A directory containing any miscellaneous additional files.
# PKGDIR 	- Package creation files.
#
# USE_GMAKE	- Says that the package uses gmake (*).
# HAS_CONFIGURE	- Says that the package has its own configure script (*).
# CONFIGURE_ARGS - Pass these args to configure, if $HAS_CONFIGURE.
# HOME_LOCATION	- site/path name (or user's email address) describing
#		  where this package came from or can be obtained if the
#		  tarball is missing.
# DEPENDS	- A list of other packages this package depends on being
#		  made first.
# 
#
# Default targets and their behaviors:
#
# extract	- Unpacks ${DISTDIR}/${DISTNAME}.tar.gz into ${WRKDIR}.
# configure	- Applys patches, if any, and runs either GNU configure, a
#		  local configure or nothing, depending on settings.
# build		- Actually compile the sources.
# install	- Install the results of a build.
# package	- Create a package from a build.
# bundle	- From an unextracted source tree, re-create tarballs.


.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

GMAKE?=		gmake

# These need to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  It can, of course, be overridden
# by individual Makefiles.
PORTSDIR?=	/usr/ports
DISTDIR?=	${PORTSDIR}/distfiles

WRKDIR?=	${.CURDIR}/work
WRKSRC?=	${WRKDIR}/${DISTNAME}
PATCHDIR?=	${.CURDIR}/patches
SCRIPTDIR?=	${.CURDIR}/scripts
FILESDIR?=	${.CURDIR}/files
PKGDIR?=	${.CURDIR}/pkg

# Change these if you'd prefer to keep the cookies someplace else.
EXTRACT_COOKIE?=	${.CURDIR}/.extract_done
CONFIGURE_COOKIE?=	${.CURDIR}/.configure_done

# Miscellaneous overridable commands:
EXTRACT_CMD?=	tar
EXTRACT_SUFX?=	.tar.gz
EXTRACT_ARGS?=	-C ${WRKDIR} -xzf

BUNDLE_CMD?=	tar
BUNDLE_ARGS?=	-C ${WRKDIR} -czf

HOME_LOCATION?=	<original site unknown>

.MAIN: all
all: extract configure build

.if !target(install)
install:
	@echo "===>  Installing for ${DISTNAME}"
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} install)
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} install)
.endif
.endif

.if !target(package)
package:
# Makes some gross assumptions about a fairly simple package with no
# install, require or deinstall scripts.  Override this rule if your
# package is anything but run-of-the-mill (or show me a way to do this
# more generally).
	@if [ -d ${PKGDIR} ]; then
	   echo "===>  Building package for ${DISTNAME}"; \
	   pkg_create -c ${PKGDIR}/COMMENT -d ${PKGDIR}/DESCR \
	     -f ${PKGDIR}/PLIST ${DISTNAME}; \
	fi
.endif

.if !target(build)
build: configure
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
.endif

.if !target(configure)
# This is done with a .configure because configures are often expensive,
# and you don't want it done again gratuitously when you're trying to get
# a make of the whole tree to work.
configure: extract ${CONFIGURE_COOKIE}

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
	@if [ -f ${SCRIPTDIR}/post-configure ]; then \
	   sh ${SCRIPTDIR}/post-configure ${PORTSDIR} ${.CURDIR} ${WRKSRC}; \
	fi
	@touch -f ${CONFIGURE_COOKIE}
.endif

.if !target(bundle)
bundle:
	@echo "===>  Bundling for ${DISTNAME}"
	@if [ ! -f ${EXTRACT_COOKIE} ]; then \
	   echo ">> There doesn't appear to be a properly extracted"; \
	   echo ">> distribution for ${DISTNAME}. Skipping.."; \
	   exit 0; \
	fi
	@if [ -f ${CONFIGURE_COOKIE} ]; then \
	   echo ">> WARNING:  This source has been configured and may be"; \
	   echo ">> produce a tainted distfile!"; \
	fi
	${BUNDLE_CMD} ${BUNDLE_ARGS} ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX} \
		${DISTNAME}
.endif

.if !target(extract)
# We need to depend on .extract_done rather than the presence of ${WRKDIR}
# because if the user interrupts the extract in the middle (and it's often
# a long procedure), we get tricked into thinking that we've got a good dist
# in ${WRKDIR}.
extract: ${EXTRACT_COOKIE}

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

.if !target(clean)
clean:
	@echo "===>  Cleaning for ${DISTNAME}"
	@rm -f ${EXTRACT_COOKIE} ${CONFIGURE_COOKIE}
	@rm -rf ${WRKDIR}
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
