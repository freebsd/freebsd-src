#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# $Id$

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
# GNU_CONFIGURE	- Says that the package uses GNU configure (*).
# GNU_CONFIGURE_ARGS - If defined, override defaults with these args (*).
# HOME_LOCATION	- site/path name (or user's email address) describing
#		  where this package came from or can be obtained if the
#		  tarball is missing.
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


.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

GMAKE?=		gmake

# This needs to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  It can, of course, be overridden
# by individual Makefiles.
DISTDIR?=	/usr/ports/distfiles

WRKDIR?=	${.CURDIR}/work
WRKSRC?=	${WRKDIR}/${DISTNAME}
PATCHDIR?=	${.CURDIR}/patches
SCRIPTDIR?=	${.CURDIR}/scripts
FILESDIR?=	${.CURDIR}/files
PKGDIR?=	${.CURDIR}/pkg

# Miscellaneous overridable commands:
EXTRACT_CMD?=	tar
EXTRACT_SUFX?=	.tar.gz
EXTRACT_ARGS?=	-C ${WRKDIR} -xzf

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
	@[ -d ${PKGDIR} ] && \
		echo "===>  Building package for ${DISTNAME}" ; \
		pkg_create -c pkg/COMMENT -d pkg/DESCR -f pkg/PLIST ${DISTNAME}
.endif

.if !target(build)
build: configure
	@echo "===>  Building for ${DISTNAME}"
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} all)
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} all)
.endif
.endif

.if !target(configure)
configure: extract
	@echo "===>  Configuring for ${DISTNAME}"
	@if [ -d ${PATCHDIR} ]; then \
		echo "===>  Applying patches for ${DISTNAME}" ; \
		for i in ${PATCHDIR}/patch-*; do \
			patch -d ${WRKSRC} --quiet -E -p0 < $$i; \
		done; \
	fi
.if defined(GNU_CONFIGURE)
.if !defined(GNU_CONFIGURE_ARGS)
	@(cd ${WRKSRC}; ./configure i386--freebsd)
.else !defined(GNU_CONFIGURE_ARGS)
	@(cd ${WRKSRC}; ./configure ${GNU_CONFIGURE_ARGS})
.endif
.endif
# We have a small convention for our local configure scripts, which
# is that ${.CURDIR} and the package working directory get passed as
# command-line arguments since all other methods are a little
# problematic.
	@if [ -f ${SCRIPTDIR}/configure ]; then \
		sh ${SCRIPTDIR}/configure ${.CURDIR} ${WRKSRC}; \
	fi
.endif

.if !target(extract)
# We need to depend on .extract_done rather than the presence of ${WRKDIR}
# because if the user interrupts the extract in the middle (and it's often
# a long procedure), we get tricked into thinking that we've got a good dist
# in ${WRKDIR}.
extract: ${.CURDIR}/.extract_done

${.CURDIR}/.extract_done:
	@echo "===>  Extracting for ${DISTNAME}"
	@rm -rf ${WRKDIR}
	@mkdir -p ${WRKDIR}
	@if [ ! -f ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX} ]; then \
	  echo "Sorry, can't find ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX}."; \
	  echo "Please obtain this file from:"; \
	  echo "	$HOME_LOCATION"; \
	  echo "before proceeding."; \
	fi
	@${EXTRACT_CMD} ${EXTRACT_ARGS} ${DISTDIR}/${DISTNAME}${EXTRACT_SUFX}
	@touch -f ${.CURDIR}/.extract_done
.endif

.if !target(clean)
clean:
	@echo "===>  Cleaning for ${DISTNAME}"
	@rm -f ${.CURDIR}/.extract_done
	@rm -rf ${WRKDIR}
.endif

.if !target(cleandir)
cleandir: clean
.endif
