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
.if exists(${SRCCONF})
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


# Share files
SHAREDIR?=	/usr/share
SHAREOWN?=	root
SHAREGRP?=	wheel
SHAREMODE?=	${NOBINMODE}

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
    INSTALLLIB \
    MAN \
    PROFILE
.if defined(NO_${var})
WITHOUT_${var}=
.endif
.endfor

#
# Compat NO_* options (same as above, except their use is deprecated).
#
.if !defined(BURN_BRIDGES)
.for var in \
    ACPI \
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
    BLUETOOTH \
    BOOT \
    CALENDAR \
    CPP \
    CRYPT \
    CVS \
    CXX \
    DICT \
    DYNAMICROOT \
    EXAMPLES \
    FORTH \
    FP_LIBC \
    GAMES \
    GCOV \
    GDB \
    GNU \
    GPIB \
    GROFF \
    HTML \
    INET6 \
    INFO \
    IPFILTER \
    IPX \
    KERBEROS \
    LIB32 \
    LIBPTHREAD \
    LIBTHR \
    LOCALES \
    LPR \
    MAILWRAPPER \
    NETCAT \
    NIS \
    NLS \
    NLS_CATALOGS \
    NS_CACHING \
    OBJC \
    OPENSSH \
    OPENSSL \
    PAM \
    PF \
    RCMDS \
    RCS \
    RESCUE \
    SENDMAIL \
    SETUID_LOGIN \
    SHAREDOCS \
    SYSCONS \
    TCSH \
    TOOLCHAIN \
    USB \
    WPA_SUPPLICANT_EAPOL
.if defined(NO_${var})
#.warning NO_${var} is deprecated in favour of WITHOUT_${var}=
WITHOUT_${var}=
.endif
.endfor
.endif # !defined(BURN_BRIDGES)

#
# Older-style variables that enabled behaviour when set.
#
.if defined(YES_HESIOD)
WITH_HESIOD=
.endif
.if defined(MAKE_IDEA)
WITH_IDEA=
.endif

#
# Default behaviour of MK_CLANG depends on the architecture.
#
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "powerpc"
_clang_yes=CLANG
_clang_no=
.else
_clang_yes=
_clang_no=CLANG
.endif

#
# MK_* options which default to "yes".
#
.for var in \
    ACCT \
    ACPI \
    AMD \
    APM \
    ASSERT_DEBUG \
    AT \
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
    BOOT \
    BSD_CPIO \
    BSNMP \
    BZIP2 \
    CALENDAR \
    CDDL \
    ${_clang_yes} \
    CPP \
    CRYPT \
    CTM \
    CVS \
    CXX \
    DICT \
    DYNAMICROOT \
    EXAMPLES \
    FDT \
    FLOPPY \
    FORTH \
    FP_LIBC \
    FREEBSD_UPDATE \
    GAMES \
    GCC \
    GCOV \
    GDB \
    GNU \
    GPIB \
    GROFF \
    HTML \
    INET6 \
    INFO \
    INSTALLLIB \
    IPFILTER \
    IPFW \
    IPX \
    JAIL \
    KERBEROS \
    KVM \
    LEGACY_CONSOLE \
    LIB32 \
    LIBPTHREAD \
    LIBTHR \
    LOCALES \
    LOCATE \
    LPR \
    MAIL \
    MAILWRAPPER \
    MAKE \
    MAN \
    NCP \
    NDIS \
    NETCAT \
    NETGRAPH \
    NIS \
    NLS \
    NLS_CATALOGS \
    NS_CACHING \
    NTP \
    OBJC \
    OPENSSH \
    OPENSSL \
    PAM \
    PF \
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
    SSP \
    SYSINSTALL \
    SYMVER \
    SYSCONS \
    TCSH \
    TELNET \
    TEXTPROC \
    TOOLCHAIN \
    USB \
    WIRELESS \
    WPA_SUPPLICANT_EAPOL \
    ZFS \
    ZONEINFO
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

#
# MK_* options which default to "no".
#
.for var in \
    BIND_IDN \
    BIND_LARGE_FILE \
    BIND_LIBS \
    BIND_SIGCHASE \
    BIND_XML \
    BSD_GREP \
    ${_clang_no} \
    GPIO \
    HESIOD \
    IDEA
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

.if ${MK_BIND_MTREE} == "no"
MK_BIND_ETC:=	no
.endif

.if ${MK_CDDL} == "no"
MK_ZFS:=	no
.endif

.if ${MK_CRYPT} == "no"
MK_OPENSSL:=	no
MK_OPENSSH:=	no
MK_KERBEROS:=	no
.endif

.if ${MK_IPX} == "no"
MK_NCP:=	no
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
MK_CLANG:=	no
MK_GDB:=	no
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

.endif # !_WITHOUT_SRCCONF

.endif	# !target(__<bsd.own.mk>__)
