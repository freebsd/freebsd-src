# $FreeBSD$
#
# The include file <bsd.own.mk> set common variables for owner,
# group, mode, and directories. Defaults are in brackets.
#
#
# +++ variables +++
#
# DESTDIR	Change the tree where the file gets installed. [not set]
#
# DISTDIR	Change the tree where the file for a distribution
# 		gets installed (see /usr/src/release/Makefile). [not set]
#
# COMPRESS_CMD	Program to compress documents.
#		Output is to stdout. [gzip -cn]
#
# COMPRESS_EXT	File name extension of ${COMPRESS_CMD} command. [.gz]
#
# BINOWN	Binary owner. [root]
#
# BINGRP	Binary group. [wheel]
#
# BINMODE	Binary mode. [555]
#
# NOBINMODE	Mode for non-executable files. [444]
#
# LIBDIR	Base path for libraries. [/usr/lib]
#
# LIBCOMPATDIR	Base path for compat libraries. [/usr/lib/compat]
#
# LIBDATADIR	Base path for misc. utility data files. [/usr/libdata]
#
# LIBEXECDIR	Base path for system daemons and utilities. [/usr/libexec]
#
# LINTLIBDIR	Base path for lint libraries. [/usr/libdata/lint]
#
# SHLIBDIR	Base path for shared libraries. [${LIBDIR}]
#
# LIBOWN	Library owner. [${BINOWN}]
#
# LIBGRP	Library group. [${BINGRP}]
#
# LIBMODE	Library mode. [${NOBINMODE}]
#
#
# DEBUGDIR	Base path for standalone debug files. [/usr/lib/debug]
#
# DEBUGMODE	Mode for debug files. [${NOBINMODE}]
#
#
# KMODDIR	Base path for loadable kernel modules
#		(see kld(4)). [/boot/kernel]
#
# KMODOWN	Kernel and KLD owner. [${BINOWN}]
#
# KMODGRP	Kernel and KLD group. [${BINGRP}]
#
# KMODMODE	KLD mode. [${BINMODE}]
#
#
# SHAREDIR	Base path for architecture-independent ascii
#		text files. [/usr/share]
#
# SHAREOWN	ASCII text file owner. [root]
#
# SHAREGRP	ASCII text file group. [wheel]
#
# SHAREMODE	ASCII text file mode. [${NOBINMODE}]
#
#
# CONFDIR	Base path for configuration files. [/etc]
#
# CONFOWN	Configuration file owner. [root]
#
# CONFGRP	Configuration file group. [wheel]
#
# CONFMODE	Configuration file mode. [644]
#
#
# DOCDIR	Base path for system documentation (e.g. PSD, USD,
#		handbook, FAQ etc.). [${SHAREDIR}/doc]
#
# DOCOWN	Documentation owner. [${SHAREOWN}]
#
# DOCGRP	Documentation group. [${SHAREGRP}]
#
# DOCMODE	Documentation mode. [${NOBINMODE}]
#
#
# INFODIR	Base path for GNU's hypertext system
#		called Info (see info(1)). [${SHAREDIR}/info]
#
# INFOOWN	Info owner. [${SHAREOWN}]
#
# INFOGRP	Info group. [${SHAREGRP}]
#
# INFOMODE	Info mode. [${NOBINMODE}]
#
#
# MANDIR	Base path for manual installation. [${SHAREDIR}/man/man]
#
# MANOWN	Manual owner. [${SHAREOWN}]
#
# MANGRP	Manual group. [${SHAREGRP}]
#
# MANMODE	Manual mode. [${NOBINMODE}]
#
#
# NLSDIR	Base path for National Language Support files
#		installation. [${SHAREDIR}/nls]
#
# NLSOWN	National Language Support files owner. [${SHAREOWN}]
#
# NLSGRP	National Language Support files group. [${SHAREGRP}]
#
# NLSMODE	National Language Support files mode. [${NOBINMODE}]
#
# INCLUDEDIR	Base path for standard C include files [/usr/include]

.if !target(__<bsd.own.mk>__)
__<bsd.own.mk>__:

.if !defined(_WITHOUT_SRCCONF)
SRCCONF?=	/etc/src.conf
.if exists(${SRCCONF}) || ${SRCCONF} != "/etc/src.conf"
.include "${SRCCONF}"
.endif
.endif

# Binaries
BINOWN?=	root
BINGRP?=	wheel
BINMODE?=	555
NOBINMODE?=	444

.if defined(MODULES_WITH_WORLD)
KMODDIR?=	/boot/modules
.else
KMODDIR?=	/boot/kernel
.endif
KMODOWN?=	${BINOWN}
KMODGRP?=	${BINGRP}
KMODMODE?=	${BINMODE}

LIBDIR?=	/usr/lib
LIBCOMPATDIR?=	/usr/lib/compat
LIBDATADIR?=	/usr/libdata
LIBEXECDIR?=	/usr/libexec
LINTLIBDIR?=	/usr/libdata/lint
SHLIBDIR?=	${LIBDIR}
LIBOWN?=	${BINOWN}
LIBGRP?=	${BINGRP}
LIBMODE?=	${NOBINMODE}

DEBUGDIR?=	/usr/lib/debug
DEBUGMODE?=	${NOBINMODE}


# Share files
SHAREDIR?=	/usr/share
SHAREOWN?=	root
SHAREGRP?=	wheel
SHAREMODE?=	${NOBINMODE}

CONFDIR?=	/etc
CONFOWN?=	root
CONFGRP?=	wheel
CONFMODE?=	644

MANDIR?=	${SHAREDIR}/man/man
MANOWN?=	${SHAREOWN}
MANGRP?=	${SHAREGRP}
MANMODE?=	${NOBINMODE}

DOCDIR?=	${SHAREDIR}/doc
DOCOWN?=	${SHAREOWN}
DOCGRP?=	${SHAREGRP}
DOCMODE?=	${NOBINMODE}

INFODIR?=	${SHAREDIR}/info
INFOOWN?=	${SHAREOWN}
INFOGRP?=	${SHAREGRP}
INFOMODE?=	${NOBINMODE}

NLSDIR?=	${SHAREDIR}/nls
NLSOWN?=	${SHAREOWN}
NLSGRP?=	${SHAREGRP}
NLSMODE?=	${NOBINMODE}

INCLUDEDIR?=	/usr/include

#
# install(1) parameters.
#
HRDLINK?=	-l h
SYMLINK?=	-l s

INSTALL_LINK?=		${INSTALL} ${HRDLINK}
INSTALL_SYMLINK?=	${INSTALL} ${SYMLINK}

# Common variables
.if !defined(DEBUG_FLAGS)
STRIP?=		-s
.endif

COMPRESS_CMD?=	gzip -cn
COMPRESS_EXT?=	.gz

.if !defined(_WITHOUT_SRCCONF)
#
# Define MK_* variables (which are either "yes" or "no") for users
# to set via WITH_*/WITHOUT_* in /etc/src.conf and override in the
# make(1) environment.
# These should be tested with `== "no"' or `!= "no"' in makefiles.
# The NO_* variables should only be set by makefiles.
#

#
# Supported NO_* options (if defined, MK_* will be forced to "no",
# regardless of user's setting).
#
.for var in \
    CTF \
    DEBUG_FILES \
    INSTALLLIB \
    MAN \
    PROFILE
.if defined(NO_${var})
.if defined(WITH_${var})
.undef WITH_${var}
.endif
WITHOUT_${var}=
.endif
.endfor

#
# Older-style variables that enabled behaviour when set.
#
.if defined(YES_HESIOD)
WITH_HESIOD=
.endif

__DEFAULT_YES_OPTIONS = \
    ACCT \
    ACPI \
    AMD \
    APM \
    ARM_EABI \
    ASSERT_DEBUG \
    AT \
    ATF \
    ATM \
    AUDIT \
    AUTHPF \
    BIND \
    BIND_DNSSEC \
    BIND_ETC \
    BIND_LIBS_LWRES \
    BIND_MTREE \
    BIND_NAMED \
    BIND_UTILS \
    BINUTILS \
    BLUETOOTH \
    BMAKE \
    BOOT \
    BSD_CPIO \
    BSNMP \
    BZIP2 \
    CALENDAR \
    CAPSICUM \
    CDDL \
    CPP \
    CROSS_COMPILER \
    CRYPT \
    CTM \
    CXX \
    DICT \
    DYNAMICROOT \
    ED_CRYPTO \
    EXAMPLES \
    FLOPPY \
    FORMAT_EXTENSIONS \
    FORTH \
    FP_LIBC \
    FREEBSD_UPDATE \
    GAMES \
    GCC \
    GCOV \
    GDB \
    GNU \
    GPIB \
    GPIO \
    GROFF \
    HTML \
    INET \
    INET6 \
    INFO \
    INSTALLLIB \
    IPFILTER \
    IPFW \
    IPX \
    JAIL \
    KDUMP \
    KERBEROS \
    KERNEL_SYMBOLS \
    KVM \
    LDNS \
    LEGACY_CONSOLE \
    LIB32 \
    LIBPTHREAD \
    LIBTHR \
    LOCALES \
    LOCATE \
    LPR \
    LS_COLORS \
    MAIL \
    MAILWRAPPER \
    MAKE \
    MAN \
    NDIS \
    NETCAT \
    NETGRAPH \
    NIS \
    NLS \
    NLS_CATALOGS \
    NS_CACHING \
    NTP \
    OPENSSH \
    OPENSSL \
    PAM \
    PC_SYSINSTALL \
    PF \
    PKGBOOTSTRAP \
    PKGTOOLS \
    PMC \
    PORTSNAP \
    PPP \
    PROFILE \
    QUOTAS \
    RCMDS \
    RCS \
    RESCUE \
    ROUTED \
    SENDMAIL \
    SETUID_LOGIN \
    SHAREDOCS \
    SOURCELESS \
    SOURCELESS_HOST \
    SOURCELESS_UCODE \
    SSP \
    SVNLITE \
    SYMVER \
    SYSCONS \
    SYSINSTALL \
    TCSH \
    TELNET \
    TEXTPROC \
    TOOLCHAIN \
    USB \
    UTMPX \
    WIRELESS \
    WPA_SUPPLICANT_EAPOL \
    ZFS \
    ZONEINFO

__DEFAULT_NO_OPTIONS = \
    BIND_IDN \
    BIND_LARGE_FILE \
    BIND_LIBS \
    BIND_SIGCHASE \
    BIND_XML \
    BSDCONFIG \
    BSD_GREP \
    CLANG_EXTRAS \
    CTF \
    DEBUG_FILES \
    GNU_PATCH \
    GPL_DTC \
    HESIOD \
    ICONV \
    INSTALL_AS_USER \
    LDNS_UTILS \
    NMTREE \
    NAND \
    OFED \
    OPENSSH_NONE_CIPHER \
    SHARED_TOOLCHAIN \
    SVN \
    USB_GADGET_EXAMPLES

#
# Default behaviour of some options depends on the architecture.  Unfortunately
# this means that we have to test TARGET_ARCH (the buildworld case) as well
# as MACHINE_ARCH (the non-buildworld case).  Normally TARGET_ARCH is not
# used at all in bsd.*.mk, but we have to make an exception here if we want
# to allow defaults for some things like clang and fdt to vary by target
# architecture.
#
.if defined(TARGET_ARCH)
__T=${TARGET_ARCH}
.else
__T=${MACHINE_ARCH}
.endif
# Clang is only for x86, powerpc and little-endian arm right now, by default.
.if ${__T} == "amd64" || ${__T} == "i386" || ${__T:Mpowerpc*}
__DEFAULT_YES_OPTIONS+=CLANG CLANG_FULL
.elif ${__T} == "arm" || ${__T} == "armv6"
__DEFAULT_YES_OPTIONS+=CLANG
# GCC is unable to build the full clang on arm, disable it by default.
__DEFAULT_NO_OPTIONS+=CLANG_FULL
.else
__DEFAULT_NO_OPTIONS+=CLANG CLANG_FULL
.endif
# Clang the default system compiler only on little-endian arm and x86.
.if ${__T} == "amd64" || ${__T} == "arm" || ${__T} == "armv6" || \
    ${__T} == "i386"
__DEFAULT_YES_OPTIONS+=CLANG_IS_CC
.else
__DEFAULT_NO_OPTIONS+=CLANG_IS_CC
.endif
# FDT is needed only for arm, mips and powerpc
.if ${__T:Marm*} || ${__T:Mpowerpc*} || ${__T:Mmips*}
__DEFAULT_YES_OPTIONS+=FDT
.else
__DEFAULT_NO_OPTIONS+=FDT
.endif
.undef __T

#
# MK_* options which default to "yes".
#
.for var in ${__DEFAULT_YES_OPTIONS}
.if defined(WITH_${var}) && defined(WITHOUT_${var})
.error WITH_${var} and WITHOUT_${var} can't both be set.
.endif
.if defined(MK_${var})
.error MK_${var} can't be set by a user.
.endif
.if defined(WITHOUT_${var})
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.endfor
.undef __DEFAULT_YES_OPTIONS

#
# MK_* options which default to "no".
#
.for var in ${__DEFAULT_NO_OPTIONS}
.if defined(WITH_${var}) && defined(WITHOUT_${var})
.error WITH_${var} and WITHOUT_${var} can't both be set.
.endif
.if defined(MK_${var})
.error MK_${var} can't be set by a user.
.endif
.if defined(WITH_${var})
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.endfor
.undef __DEFAULT_NO_OPTIONS

#
# Force some options off if their dependencies are off.
# Order is somewhat important.
#
.if ${MK_LIBPTHREAD} == "no"
MK_LIBTHR:=	no
.endif

.if ${MK_LIBTHR} == "no"
MK_BIND:=	no
.endif

.if ${MK_BIND} == "no"
MK_BIND_DNSSEC:= no
MK_BIND_ETC:=	no
MK_BIND_LIBS:=	no
MK_BIND_LIBS_LWRES:= no
MK_BIND_MTREE:=	no
MK_BIND_NAMED:=	no
MK_BIND_UTILS:=	no
.endif

.if ${MK_LDNS} == "no"
MK_LDNS_UTILS:=	no
.endif

.if ${MK_LDNS_UTILS} != "no"
MK_BIND_UTILS:=	no
.endif

.if ${MK_BIND_MTREE} == "no"
MK_BIND_ETC:=	no
.endif

.if ${MK_SOURCELESS} == "no"
MK_SOURCELESS_HOST:=	no
MK_SOURCELESS_UCODE:= no
.endif

.if ${MK_CDDL} == "no"
MK_ZFS:=	no
MK_CTF:=	no
.endif

.if ${MK_CRYPT} == "no"
MK_OPENSSL:=	no
MK_OPENSSH:=	no
MK_KERBEROS:=	no
.endif

.if ${MK_CXX} == "no"
MK_CLANG:=	no
MK_GROFF:=	no
.endif

.if ${MK_MAIL} == "no"
MK_MAILWRAPPER:= no
MK_SENDMAIL:=	no
.endif

.if ${MK_NETGRAPH} == "no"
MK_ATM:=	no
MK_BLUETOOTH:=	no
.endif

.if ${MK_OPENSSL} == "no"
MK_OPENSSH:=	no
MK_KERBEROS:=	no
.endif

.if ${MK_PF} == "no"
MK_AUTHPF:=	no
.endif

.if ${MK_TEXTPROC} == "no"
MK_GROFF:=	no
.endif

.if ${MK_TOOLCHAIN} == "no"
MK_BINUTILS:=	no
MK_CLANG:=	no
MK_GCC:=	no
MK_GDB:=	no
.endif

.if ${MK_CLANG} == "no"
MK_CLANG_EXTRAS:= no
MK_CLANG_FULL:= no
MK_CLANG_IS_CC:= no
.endif

#
# Set defaults for the MK_*_SUPPORT variables.
#

#
# MK_*_SUPPORT options which default to "yes" unless their corresponding
# MK_* variable is set to "no".
#
.for var in \
    BZIP2 \
    GNU \
    INET \
    INET6 \
    IPX \
    KERBEROS \
    KVM \
    NETGRAPH \
    PAM \
    WIRELESS
.if defined(WITH_${var}_SUPPORT) && defined(WITHOUT_${var}_SUPPORT)
.error WITH_${var}_SUPPORT and WITHOUT_${var}_SUPPORT can't both be set.
.endif
.if defined(MK_${var}_SUPPORT)
.error MK_${var}_SUPPORT can't be set by a user.
.endif
.if defined(WITHOUT_${var}_SUPPORT) || ${MK_${var}} == "no"
MK_${var}_SUPPORT:= no
.else
MK_${var}_SUPPORT:= yes
.endif
.endfor

#
# MK_* options whose default value depends on another option.
#
.for vv in \
    GSSAPI/KERBEROS \
    MAN_UTILS/MAN
.if defined(WITH_${vv:H}) && defined(WITHOUT_${vv:H})
.error WITH_${vv:H} and WITHOUT_${vv:H} can't both be set.
.endif
.if defined(MK_${vv:H})
.error MK_${vv:H} can't be set by a user.
.endif
.if defined(WITH_${vv:H})
MK_${vv:H}:=	yes
.elif defined(WITHOUT_${vv:H})
MK_${vv:H}:=	no
.else
MK_${vv:H}:=	${MK_${vv:T}}
.endif
.endfor

#
# MK_* options that default to "yes" if the compiler is a C++11 compiler.
#
.include <bsd.compiler.mk>
.for var in \
    LIBCPLUSPLUS
.if defined(WITH_${var}) && defined(WITHOUT_${var})
.error WITH_${var} and WITHOUT_${var} can't both be set.
.endif
.if defined(MK_${var})
.error MK_${var} can't be set by a user.
.endif
.if ${COMPILER_FEATURES:Mc++11}
.if defined(WITHOUT_${var})
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.else
.if defined(WITH_${var})
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.endif
.endfor

.if ${MK_CTF} != "no"
CTFCONVERT_CMD=	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.elif defined(.PARSEDIR) || (defined(MAKE_VERSION) && ${MAKE_VERSION} >= 5201111300)
CTFCONVERT_CMD=
.else
CTFCONVERT_CMD=	@:
.endif 

.if ${MK_INSTALL_AS_USER} != "no"
_uid!=	id -u
.if ${_uid} != 0
.if !defined(USER)
USER!=	id -un
.endif
_gid!=	id -gn
.for x in BIN CONF DOC INFO KMOD LIB MAN NLS SHARE
$xOWN=	${USER}
$xGRP=	${_gid}
.endfor
.endif
.endif

.endif # !_WITHOUT_SRCCONF

.endif	# !target(__<bsd.own.mk>__)
