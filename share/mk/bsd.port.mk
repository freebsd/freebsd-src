# -*- mode: Fundamental; tab-width: 4; -*-
#
#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# $Id: bsd.port.mk,v 1.109 1995/02/01 21:47:57 gpalmer Exp $
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
#				   ftp://ftp.freebsd.org/pub/FreeBSD/ports/distfiles)
#
# MASTER_SITE_OVERRIDE - If set, override the MASTER_SITES setting with this
#				  value.
# MASTER_SITE_FREEBSD - If set, only use the FreeBSD master repository for
#				  MASTER_SITES.
# PACKAGES		- A top level directory where all packages go (rather than
#				  going locally to each port). (default: ${PORTSDIR}/packages).
# GMAKE			- Set to path of GNU make if not in $PATH (default: gmake).
# XMKMF			- Set to path of `xmkmf' if not in $PATH (default: xmkmf).
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
# PKGNAME		- Name of the package file to create if the DISTNAME 
#				  isn't really relevant for the port/package
#				  (default: ${DISTNAME}).
# EXTRACT_ONLY	- If defined, a subset of ${DISTFILES} you want to
#			  	  actually extract.
# PATCHDIR 		- A directory containing any required patches
#				  (default: ${.CURDIR}/patches)
# SCRIPTDIR 	- A directory containing any auxiliary scripts
#				  (default: ${.CURDIR}/scripts)
# FILESDIR 		- A directory containing any miscellaneous additional files.
#				  (default: ${.CURDIR}/files)
# PKGDIR 		- A direction containing any package creation files.
#				  (default: ${.CURDIR}/pkg)
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
# NO_INSTALL_MANPAGES - For imake ports that don't like the install.man
#						target.
# HAS_CONFIGURE	- Says that the port has its own configure script.
# GNU_CONFIGURE	- Set if you are using GNU configure (optional).
# CONFIGURE_ARGS - Pass these args to configure, if ${HAS_CONFIGURE} set.
# IS_INTERACTIVE - Set this if your port needs to interact with the user
#				  during a build.  User can then decide to skip this port by
#				  setting ${BATCH}, or compiling only the interactive ports
#				  by setting ${INTERACTIVE}.
# EXEC_DEPENDS	- A list of "prog:dir" pairs of other ports this
#				  package depends on.  "prog" is the name of an
#				  executable.  make will search your $PATH for it and go
#				  into "dir" to do a "make all install" if it's not found.
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
# EXTRACT_ARGS	- Arguments to ${EXTRACT_CMD} (default: -C ${WRKDIR} -xzf).
#
# NCFTP			- Full path to ncftp command if not in $PATH (default: ncftp).
# NCFTPFLAGS    - Arguments to ${NCFTP} (default: -N).
#
#
# Default targets and their behaviors:
#
# fetch			- Retrieves ${DISTFILES} into ${DISTDIR} as necessary.
# extract		- Unpacks ${DISTFILES} into ${WRKDIR}.
# configure		- Applies patches, if any, and runs either GNU configure, one
#				  or more local configure scripts or nothing, depending on
#				  what's available.
# patch			- Apply any provided patches to the source.
# build			- Actually compile the sources.
# install		- Install the results of a build.
# reinstall		- Install the results of a build, ignoring "already installed"
#				  flag.
# package		- Create a package from an _installed_ port.
# describe		- Try to generate a one-line description for each port for
#				  use in INDEX files and the like.
# checksum		- Use files/md5 to ensure that your distfiles are valid
# makesum		- Generate files/md5 (only do this for your own ports!)
#
# Default sequence for "all" is:  fetch extract configure build

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
.if defined(USE_IMAKE)
PREFIX?=		${X11BASE}
.else
PREFIX?=		/usr/local
.endif
.if defined(USE_GMAKE)
EXEC_DEPENDS+=               gmake:${PORTSDIR}/devel/gmake
.endif

.if exists(${PORTSDIR}/../Makefile.inc)
.include "${PORTSDIR}/../Makefile.inc"
.endif


# Change these if you'd prefer to keep the cookies someplace else.
EXTRACT_COOKIE?=	${WRKDIR}/.extract_done
CONFIGURE_COOKIE?=	${WRKDIR}/.configure_done
INSTALL_COOKIE?=	${WRKDIR}/.install_done
BUILD_COOKIE?=		${WRKDIR}/.build_done
PATCH_COOKIE?=		${WRKDIR}/.patch_done

# How to do nothing.  Override if you, for some strange reason, would rather
# do something.
DO_NADA?=		echo -n

# Miscellaneous overridable commands:
GMAKE?=			gmake
XMKMF?=			xmkmf
MD5?=			md5
MD5_FILE?=		${FILESDIR}/md5
MAKE_FLAGS?=	-f
MAKEFILE?=		Makefile

NCFTP?=			ncftp
NCFTPFLAGS?=	-N

TOUCH?=			touch
TOUCH_FLAGS?=	-f

PATCH?=			patch
PATCH_STRIP?=	-p0
.if defined(PATCH_DEBUG)
PATCH_ARGS?=	-d ${WRKSRC} -E ${PATCH_STRIP}
.else
PATCH_ARGS?=	-d ${WRKSRC} --forward --quiet -E ${PATCH_STRIP}
.endif

EXTRACT_CMD?=	tar
EXTRACT_SUFX?=	.tar.gz
EXTRACT_ARGS?=	-C ${WRKDIR} -xzf

PKG_CMD?=		pkg_create
PKG_ARGS?=		-v -c ${PKGDIR}/COMMENT -d ${PKGDIR}/DESCR -f ${PKGDIR}/PLIST -p ${PREFIX}
PKG_SUFX?=		.tgz

ALL_TARGET?=		all
INSTALL_TARGET?=	install

.if defined(MASTER_SITE_FREEBSD)
MASTER_SITE_OVERRIDE=  ftp://freebsd.cdrom.com/pub/FreeBSD/FreeBSD-current/ports/distfiles/ 
.endif

# I guess we're in the master distribution business! :)  As we gain mirror
# sites for distfiles, add them to this list.
.if !defined(MASTER_SITE_OVERRIDE)
MASTER_SITES+=	ftp://freebsd.cdrom.com/pub/FreeBSD/FreeBSD-current/ports/distfiles/
.else
MASTER_SITES=	${MASTER_SITE_OVERRIDE}
.endif

# Derived names so that they're easily overridable.
DISTFILES?=		${DISTNAME}${EXTRACT_SUFX}
PKGNAME?=		${DISTNAME}
MAINTAINER?=	FreeBSD-Ports@FreeBSD.ORG

.if exists(${PACKAGES})
PKGFILE?=		${PACKAGES}/${PKGNAME}${PKG_SUFX}
.else
PKGFILE?=		${PKGNAME}${PKG_SUFX}
.endif

.if defined(GNU_CONFIGURE)
CONFIGURE_ARGS?=	--prefix=${PREFIX}
HAS_CONFIGURE=		yes
.endif

.MAIN: all

# If we're in BATCH mode and the port is interactive, or we're in
# interactive mode and the port is non-interactive, skip all the important
# targets.  The reason we have two modes is that one might want to leave
# a build in BATCH mode running overnight, then come back in the morning
# and do _only_ the interactive ones that required your intervention.
# This allows you to do both.
#
.if (defined(IS_INTERACTIVE) && defined(BATCH)) || (!defined(IS_INTERACTIVE) && defined(INTERACTIVE))
all:
	@${DO_NADA}
pre-build:
	@${DO_NADA}
build:
	@${DO_NADA}
pre-install:
	@${DO_NADA}
install:
	@${DO_NADA}
pre-fetch:
	@${DO_NADA}
fetch:
	@${DO_NADA}
pre-configure:
	@${DO_NADA}
configure:
	@${DO_NADA}
.endif

.if !target(all)
all: extract configure build
.endif

.if !target(is_depended)
is_depended:	all install
.endif

# The following are used to create easy dummy targets for disabling some
# bit of default target behavior you don't want.  They still check to see
# if the target exists, and if so don't do anything, since you might want
# to set this globally for a group of ports in a Makefile.inc, but still
# be able to override from an individual Makefile (since you can't _undefine_
# a variable in make!).
.if defined(NO_EXTRACT) && !target(extract)
extract:
	@${TOUCH} ${TOUCH_FLAGS} ${EXTRACT_COOKIE}
checksum:
	@${DO_NADA}
makesum:
	@${DO_NADA}
.endif
.if defined(NO_CONFIGURE) && !target(configure)
configure:
	@${TOUCH} ${TOUCH_FLAGS} ${CONFIGURE_COOKIE}
.endif
.if defined(NO_BUILD) && !target(build)
build:
	@${TOUCH} ${TOUCH_FLAGS} ${BUILD_COOKIE}
.endif
.if defined(NO_PACKAGE) && !target(package)
package:
	@${DO_NADA}
.endif
.if defined(NO_INSTALL) && !target(install)
install:
	@${TOUCH} ${TOUCH_FLAGS} ${INSTALL_COOKIE}
.endif
.if defined(NO_PATCH) && !target(patch)
patch:
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
.endif

# More standard targets start here.

.if !target(describe)
describe:
.if defined(NO_PACKAGE)
	@echo "${.CURDIR}/${DISTNAME}:   ** Not packageable";
.else
	@if [ -f ${PKGDIR}/COMMENT ]; then \
		echo "${.CURDIR}/${DISTNAME}:	`cat ${PKGDIR}/COMMENT`"; \
	else \
		echo "${.CURDIR}/${DISTNAME}:	** No Description"; \
	fi
.endif
.endif

.if !target(reinstall)
reinstall: pre-reinstall install

pre-reinstall:
	@rm -f ${INSTALL_COOKIE}
.endif

.if !target(pre-install)
pre-install:
	@${DO_NADA}
.endif

.if !target(install)
install: build ${INSTALL_COOKIE}

${INSTALL_COOKIE}:
	@echo "===>  Installing for ${DISTNAME}"
	@${MAKE} ${.MAKEFLAGS} pre-install
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} PREFIX=${PREFIX} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
	@(cd ${WRKSRC}; ${GMAKE} ${MAKE_FLAGS} ${MAKEFILE} install.man)
.endif
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} PREFIX=${PREFIX} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
	@(cd ${WRKSRC}; ${MAKE} ${MAKE_FLAGS} ${MAKEFILE} install.man)
.endif
.endif
	@${TOUCH} ${TOUCH_FLAGS} ${INSTALL_COOKIE}
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

.if !target(depends)
depends: exec_depends lib_depends misc_depends

exec_depends:
.if defined(EXEC_DEPENDS)
.if defined(NO_DEPENDS)
# Just print out messages
	@for i in ${EXEC_DEPENDS}; do \
		prog=`echo $$i | sed -e 's/:.*//'`; \
		dir=`echo $$i | sed -e 's/.*://'`; \
		echo "===>  ${DISTNAME} depends on executable:  $$prog ($$dir)"; \
	done
.else
	@for i in ${EXEC_DEPENDS}; do \
		prog=`echo $$i | sed -e 's/:.*//'`; \
		dir=`echo $$i | sed -e 's/.*://'`; \
		if which -s "$$prog"; then \
			echo "===>  ${DISTNAME} depends on executable: $$prog - found"; \
		else \
			echo "===>  ${DISTNAME} depends on executable: $$prog - not found"; \
			echo "===>  Verifying build for $$prog in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				echo ">> No directory for $$prog.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} is_depended) ; \
				echo "===>  Returning to build of ${DISTNAME}"; \
			fi; \
		fi; \
	done
.endif
.else
	@${DO_NADA}
.endif

lib_depends:
.if defined(LIB_DEPENDS)
.if defined(NO_DEPENDS)
# Just print out messages
	@for i in ${LIB_DEPENDS}; do \
		lib=`echo $$i | sed -e 's/:.*//'`; \
		dir=`echo $$i | sed -e 's/.*://'`; \
		echo "===>  ${DISTNAME} depends on shared library:  $$lib ($$dir)"; \
	done
.else
	@for i in ${LIB_DEPENDS}; do \
		lib=`echo $$i | sed -e 's/:.*//'`; \
		dir=`echo $$i | sed -e 's/.*://'`; \
		if ldconfig -r | grep -q -e "-l$$lib"; then \
			echo "===>  ${DISTNAME} depends on shared library: $$lib - found"; \
		else \
			echo "===>  ${DISTNAME} depends on shared library: $$lib - not found"; \
			echo "===>  Verifying build for $$lib in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				echo ">> No directory for $$lib.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} is_depended) ; \
				echo "===>  Returning to build of ${DISTNAME}"; \
			fi; \
		fi; \
	done
.endif
.else
	@${DO_NADA}
.endif

misc_depends:
.if defined(DEPENDS)
	@echo "===>  ${DISTNAME} depends on:  ${DEPENDS}"
.if !defined(NO_DEPENDS)
	@for i in ${DEPENDS}; do \
		echo "===>  Verifying build for $$i"; \
		if [ ! -d $$i ]; then \
			echo ">> No directory for $$i.  Skipping.."; \
		else \
			(cd $$i; ${MAKE} ${.MAKEFLAGS} is_depended) ; \
		fi \
	done
	@echo "===>  Returning to build of ${DISTNAME}"
.endif
.else
	@${DO_NADA}
.endif

.endif

.if !target(pre-build)
pre-build:
	@${DO_NADA}
.endif

.if !target(build)
build: configure ${BUILD_COOKIE}

${BUILD_COOKIE}:
	@echo "===>  Building for ${DISTNAME}"
	@${MAKE} ${.MAKEFLAGS} pre-build
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${GMAKE} PREFIX=${PREFIX} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.else defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${MAKE} PREFIX=${PREFIX} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.endif
	@if [ -f ${SCRIPTDIR}/post-build ]; then \
		env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" \
		sh ${SCRIPTDIR}/post-build; \
	fi
	@${TOUCH} ${TOUCH_FLAGS} ${BUILD_COOKIE}
.endif

.if !target(pre-patch)
pre-patch:
	@${DO_NADA}
.endif

.if !target(patch)
patch: extract ${PATCH_COOKIE}

${PATCH_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} pre-patch
.if defined(PATCH_DEBUG)
	@if [ -d ${PATCHDIR} ]; then \
		echo "===>  Applying patches for ${DISTNAME}" ; \
		for i in ${PATCHDIR}/patch-*; do \
			echo "===>   Applying patch $$i" ; \
			${PATCH} ${PATCH_ARGS} < $$i; \
		done; \
	fi
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
.else
	@if [ -d ${PATCHDIR} ]; then \
		echo "===>  Applying patches for ${DISTNAME}" ; \
		for i in ${PATCHDIR}/patch-*; \
			do ${PATCH} ${PATCH_ARGS} < $$i; \
		done;\
	fi
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
.endif
.endif

.if !target(pre-configure)
pre-configure:
	@${DO_NADA}
.endif

.if !target(configure)
configure: depends patch ${CONFIGURE_COOKIE}

${CONFIGURE_COOKIE}:
	@echo "===>  Configuring for ${DISTNAME}"
	@${MAKE} ${.MAKEFLAGS} pre-configure
	@if [ -f ${SCRIPTDIR}/pre-configure ]; then \
		env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" \
		sh ${SCRIPTDIR}/pre-configure; \
	fi
	@if [ -f ${SCRIPTDIR}/configure ]; then \
		env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" \
		sh ${SCRIPTDIR}/configure; \
	fi
.if defined(HAS_CONFIGURE)
	@(cd ${WRKSRC}; CC="${CC}" ac_cv_path_CC="${CC}" CFLAGS="${CFLAGS}" \
	    INSTALL="/usr/bin/install -c -o ${BINOWN} -g ${BINGRP}" \
	    INSTALL_PROGRAM="/usr/bin/install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}" \
	    ./configure ${CONFIGURE_ARGS})
.endif
.if defined(USE_IMAKE)
.if defined(USE_GMAKE)
	@(cd ${WRKSRC}; ${XMKMF} && ${GMAKE} Makefiles)
.else
	@(cd ${WRKSRC}; ${XMKMF} && ${MAKE} Makefiles)
.endif
.endif
	@if [ -f ${SCRIPTDIR}/post-configure ]; then \
		env CURDIR=${.CURDIR} DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} \
		  WRKSRC=${WRKSRC} PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
		  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
		  DEPENDS="${DEPENDS}" \
		sh ${SCRIPTDIR}/post-configure; \
	fi
	@${TOUCH} ${TOUCH_FLAGS} ${CONFIGURE_COOKIE}
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
		if [ ! -f $$file -a ! -f `basename $$file` ]; then \
			echo ">> $$file doesn't seem to exist on this system."; \
			echo ">> Attempting to fetch it from a master site."; \
			for site in ${MASTER_SITES}; do \
				if ${NCFTP} ${NCFTPFLAGS} $${site}$${file}; then \
					break; \
				fi \
			done; \
			if [ ! -f $$file -a ! -f `basename $$file` ]; then \
				echo ">> Couldn't fetch it - please try to retreive this";\
				echo ">> port manually into ${DISTDIR} and try again."; \
				exit 1; \
			fi; \
	    fi \
	 done)
.endif

.if !target(makesum)
makesum: fetch
	@if [ ! -d ${FILESDIR} ]; then mkdir -p ${FILESDIR}; fi
	@if [ -f ${MD5_FILE} ]; then rm -f ${MD5_FILE}; fi
	@(cd ${DISTDIR}; \
	for file in ${DISTFILES}; do \
		${MD5} $$file >> ${MD5_FILE}; \
	done)
.endif

.if !target(checksum)
checksum: fetch
	@if [ ! -f ${MD5_FILE} ]; then \
		echo ">> No MD5 checksum file."; \
	else \
		(cd ${DISTDIR}; OK=""; \
		for file in ${DISTFILES}; do \
			CKSUM=`${MD5} $$file | awk '{print $$4}'`; \
			CKSUM2=`grep "($$file)" ${MD5_FILE} | awk '{print $$4}'`; \
			if [ "$$CKSUM2" = "" ]; then \
				echo ">> No checksum recorded for $$file"; \
				OK="false"; \
			elif [ "$$CKSUM" != "$$CKSUM2" ]; then \
				echo ">> Checksum mismatch for $$file"; \
				exit 1; \
			fi; \
			done; \
			if [ "$$OK" = "" ]; then \
				echo "Checksums OK."; \
			else \
				echo "Checksums OK for files that have them."; \
   	     fi) ; \
	fi
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
extract: fetch ${EXTRACT_COOKIE}

${EXTRACT_COOKIE}:
	@${MAKE} ${.MAKEFLAGS} checksum pre-extract
	@echo "===>  Extracting for ${DISTNAME}"
	@rm -rf ${WRKDIR}
	@mkdir -p ${WRKDIR}
.if defined(EXTRACT_ONLY)
	@for file in ${EXTRACT_ONLY}; do \
		if ! ${EXTRACT_CMD} ${EXTRACT_ARGS} ${DISTDIR}/$$file; then \
			exit 1; \
		fi \
	done
.else
	@for file in ${DISTFILES}; do \
		if ! ${EXTRACT_CMD} ${EXTRACT_ARGS} ${DISTDIR}/$$file; then \
			exit 1; \
		fi \
	done
.endif
	@${TOUCH} ${TOUCH_FLAGS} ${EXTRACT_COOKIE}
.endif

.if !target(pre-clean)
pre-clean:
	@${DO_NADA}
.endif

.if !target(clean)
clean: pre-clean
	@echo "===>  Cleaning for ${DISTNAME}"
	@rm -f ${EXTRACT_COOKIE} ${CONFIGURE_COOKIE} ${INSTALL_COOKIE} \
		${BUILD_COOKIE} ${PATCH_COOKIE}
.if !defined(NO_WRKDIR)
	@rm -rf ${WRKDIR}
.endif
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
