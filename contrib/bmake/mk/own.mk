# $Id: own.mk,v 1.51 2024/11/12 17:40:13 sjg Exp $

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.if !target(__init.mk__)
.include "init.mk"
.endif

.if !defined(NOMAKECONF) && !defined(NO_MAKECONF)
MAKECONF?=	/etc/mk.conf
.-include "${MAKECONF}"
.endif

.include <host-target.mk>

TARGET_OSNAME?= ${_HOST_OSNAME}
TARGET_OSREL?= ${_HOST_OSREL}
TARGET_OSTYPE?= ${HOST_OSTYPE}
TARGET_HOST?= ${HOST_TARGET}

# these may or may not exist
.-include <${TARGET_HOST}.mk>
.-include <config.mk>

RM?= rm
LN?= ln
INSTALL?= install

prefix?=	/usr
.if exists(${prefix}/lib)
libprefix?=	${prefix}
.else
libprefix?=	/usr
.endif

# FreeBSD at least does not set this
MACHINE_ARCH?=	${MACHINE}
# we need to make sure these are defined too in case sys.mk fails to.

# for suffix rules
IMPFLAGS?=	${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}}
.for s in .c .cc
COMPILE.$s += ${IMPFLAGS}
LINK.$s +=  ${IMPFLAGS}
.endfor

PRINT.VAR.MAKE = MAKESYSPATH=${MAKESYSPATH:U${.PARSEDIR}} ${.MAKE}
.if empty(.MAKEFLAGS:M-V*)
.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR)
PRINTOBJDIR=	${PRINT.VAR.MAKE} -r -V .OBJDIR -f /dev/null xxx
.else
PRINTOBJDIR=	${PRINT.VAR.MAKE} -V .OBJDIR
.endif
.else
PRINTOBJDIR=	echo # prevent infinite recursion
.endif

# we really like to have SRCTOP and OBJTOP defined...
.if !defined(SRCTOP) || !defined(OBJTOP)
.-include <srctop.mk>
.endif

.if !defined(SRCTOP) || !defined(OBJTOP)
# dpadd.mk is rather pointless without these
OPTIONS_DEFAULT_NO+= DPADD_MK
.endif

# process options
OPTIONS_DEFAULT_NO+= \
	DEBUG \
	INSTALL_AS_USER \
	GPROF \
	PROG_LDORDER_MK \
	LIBTOOL \
	LINT \

OPTIONS_DEFAULT_YES+= \
	ARCHIVE \
	AUTODEP \
	CRYPTO \
	DOC \
	DPADD_MK \
	GDB \
	KERBEROS \
	LINKLIB \
	MAN \
	NLS \
	OBJ \
	PIC \
	SHARE \
	SKEY \
	YP \

OPTIONS_DEFAULT_DEPENDENT+= \
	CATPAGES/MAN \
	DEBUG_RUST/DEBUG \
	LDORDER_MK/PROG_LDORDER_MK \
	OBJDIRS/OBJ \
	PICINSTALL/LINKLIB \
	PICLIB/PIC \
	PROFILE/LINKLIB \
	STAGING_MAN/STAGING \
	STAGING_PROG/STAGING \
	STAGING_RUST/STAGING_PROG \

.include <options.mk>

.if ${MK_INSTALL_AS_USER} == "yes"
# We ignore this if user is root.
_uid:= ${.MAKE.UID:U${id -u:L:sh}}
.if ${_uid} != 0
.if !defined(USERGRP)
USERGRP:=  ${.MAKE.GID:U${id -g:L:sh}}
.export USERGRP
.endif
.for x in BIN CONF DOC INC INFO FILES KMOD LIB MAN NLS PROG SHARE
$xOWN=  ${USER}
$xGRP=  ${USERGRP}
$x_INSTALL_OWN=
.endfor
.endif
.endif

# override this in sys.mk
ROOT_GROUP?=	wheel
BINGRP?=	${ROOT_GROUP}
BINOWN?=	root
BINMODE?=	555
NONBINMODE?=	444
DIRMODE?=	755

INCLUDEDIR?=	${prefix}/include
INCDIR?=	${INCLUDEDIR}

# Define MANZ to have the man pages compressed (gzip)
#MANZ=		1

MANDIR?=	${prefix}/share/man
MANGRP?=	${BINGRP}
MANOWN?=	${BINOWN}
MANMODE?=	${NONBINMODE}

INCLUDEDIR?=	${libprefix}/include
LIBDIR?=	${libprefix}/lib
SHLIBDIR?=	${libprefix}/lib
.if ${USE_SHLIBDIR:Uno} == "yes"
_LIBSODIR?=	${SHLIBDIR}
.else
_LIBSODIR?=	${LIBDIR}
.endif
# this is where ld.*so lives
SHLINKDIR?=	/usr/libexec
LINTLIBDIR?=	${libprefix}/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=        ${prefix}/share/doc
DOCGRP?=	${BINGRP}
DOCOWN?=	${BINOWN}
DOCMODE?=       ${NONBINMODE}

NLSDIR?=	${prefix}/share/nls
NLSGRP?=	${BINGRP}
NLSOWN?=	${BINOWN}
NLSMODE?=	${NONBINMODE}

KMODDIR?=	${prefix}/lkm
KMODGRP?=	${BINGRP}
KMODOWN?=	${BINOWN}
KMODMODE?=	${NONBINMODE}

SHAREGRP?=	${BINGRP}
SHAREOWN?=	${BINOWN}
SHAREMODE?=	${NONBINMODE}

COPY?=		-c
STRIP_FLAG?=	-s

.if ${TARGET_OSNAME} == "NetBSD"
.if exists(/usr/libexec/ld.elf_so)
OBJECT_FMT=ELF
.endif
OBJECT_FMT?=a.out
.endif
# sys.mk should set something appropriate if need be.
OBJECT_FMT?=ELF

.if (${_HOST_OSNAME} == "FreeBSD")
CFLAGS+= ${CPPFLAGS}
.endif

# allow for per target flags
# apply the :T:R first, so the more specific :T can override if needed
CPPFLAGS += ${CPPFLAGS_${.TARGET:T:R}} ${CPPFLAGS_${.TARGET:T}}
CFLAGS += ${CFLAGS_${.TARGET:T:R}} ${CFLAGS_${.TARGET:T}}

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# don't try to generate PIC versions of libraries on machines
# which don't support PIC.
.if  (${MACHINE_ARCH} == "vax") || \
    ((${MACHINE_ARCH} == "mips") && defined(STATIC_TOOLCHAIN)) || \
    ((${MACHINE_ARCH} == "alpha") && defined(ECOFF_TOOLCHAIN))
MK_PIC=no
.endif

# No lint, for now.
NOLINT=


.if ${MK_LINKLIB} == "no"
MK_PICINSTALL=	no
MK_PROFILE=	no
.endif

.if ${MK_MAN} == "no"
MK_CATPAGES=	no
.endif

.if ${MK_OBJ} == "no"
MK_OBJDIRS=	no
MK_AUTO_OBJ=	no
.endif

.if ${MK_SHARE} == "no"
MK_CATPAGES=	no
MK_DOC=		no
MK_INFO=	no
MK_MAN=		no
MK_NLS=		no
.endif

# :U incase not using our sys.mk
.if ${MK_META_MODE:Uno} == "yes"
# should all be set by sys.mk if not default
TARGET_SPEC_VARS ?= MACHINE
.if ${MAKE_VERSION} >= 20120325
.if ${TARGET_SPEC_VARS:[#]} > 1
TARGET_SPEC_VARS_REV := ${TARGET_SPEC_VARS:[-1..1]}
.else
TARGET_SPEC_VARS_REV = ${TARGET_SPEC_VARS}
.endif
.endif
.if ${MK_STAGING} == "yes"
STAGE_ROOT?= ${OBJROOT}/stage
STAGE_OBJTOP?= ${STAGE_ROOT}/${TARGET_SPEC_VARS_REV:@v@${$v}@:ts/}
.endif
.endif

.endif
