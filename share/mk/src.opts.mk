# $FreeBSD$
#
# Option file for FreeBSD /usr/src builds.
#
# Users define WITH_FOO and WITHOUT_FOO on the command line or in /etc/src.conf
# and /etc/make.conf files. These translate in the build system to MK_FOO={yes,no}
# with sensible (usually) defaults.
#
# Makefiles must include bsd.opts.mk after defining specific MK_FOO options that
# are applicable for that Makefile (typically there are none, but sometimes there
# are exceptions). Recursive makes usually add MK_FOO=no for options that they wish
# to omit from that make.
#
# Makefiles must include bsd.mkopt.mk before they test the value of any MK_FOO
# variable.
#
# Makefiles may also assume that this file is included by src.opts.mk should it
# need variables defined there prior to the end of the Makefile where
# bsd.{subdir,lib.bin}.mk is traditionally included.
#
# The old-style YES_FOO and NO_FOO are being phased out. No new instances of them
# should be added. Old instances should be removed since they were just to
# bridge the gap between FreeBSD 4 and FreeBSD 5.
#
# Makefiles should never test WITH_FOO or WITHOUT_FOO directly (although an
# exception is made for _WITHOUT_SRCONF which turns off this mechanism
# completely inside bsd.*.mk files).
#

.if !target(__<src.opts.mk>__)
__<src.opts.mk>__:

.include <bsd.own.mk>

#
# Define MK_* variables (which are either "yes" or "no") for users
# to set via WITH_*/WITHOUT_* in /etc/src.conf and override in the
# make(1) environment.
# These should be tested with `== "no"' or `!= "no"' in makefiles.
# The NO_* variables should only be set by makefiles for variables
# that haven't been converted over.
#

# These options are used by src the builds

__DEFAULT_YES_OPTIONS = \
    ACCT \
    ACPI \
    AMD \
    APM \
    AT \
    ATM \
    AUDIT \
    AUTHPF \
    AUTOFS \
    BHYVE \
    BINUTILS \
    BINUTILS_BOOTSTRAP \
    BLUETOOTH \
    BOOT \
    BOOTPARAMD \
    BOOTPD \
    BSD_CPIO \
    BSDINSTALL \
    BSNMP \
    BZIP2 \
    CALENDAR \
    CAPSICUM \
    CASPER \
    CCD \
    CDDL \
    CPP \
    CROSS_COMPILER \
    CRYPT \
    CTM \
    CUSE \
    CXX \
    DICT \
    DMAGENT \
    DYNAMICROOT \
    ED_CRYPTO \
    EE \
    ELFTOOLCHAIN_TOOLS \
    EXAMPLES \
    FDT \
    FILE \
    FINGER \
    FLOPPY \
    FMTREE \
    FORTH \
    FP_LIBC \
    FREEBSD_UPDATE \
    FTP \
    GAMES \
    GCOV \
    GDB \
    GNU \
    GNU_GREP_COMPAT \
    GPIO \
    GPL_DTC \
    GROFF \
    HAST \
    HTML \
    HYPERV \
    ICONV \
    INET \
    INET6 \
    INETD \
    IPFILTER \
    IPFW \
    ISCSI \
    JAIL \
    KDUMP \
    KVM \
    LDNS \
    LDNS_UTILS \
    LEGACY_CONSOLE \
    LIB32 \
    LIBPTHREAD \
    LIBTHR \
    LOCALES \
    LOCATE \
    LPR \
    LS_COLORS \
    LZMA_SUPPORT \
    MAIL \
    MAILWRAPPER \
    MAKE \
    MANDOCDB \
    NDIS \
    NETCAT \
    NETGRAPH \
    NLS_CATALOGS \
    NS_CACHING \
    NTP \
    OPENSSL \
    PAM \
    PC_SYSINSTALL \
    PF \
    PKGBOOTSTRAP \
    PMC \
    PORTSNAP \
    PPP \
    QUOTAS \
    RADIUS_SUPPORT \
    RCMDS \
    RBOOTD \
    RCS \
    RESCUE \
    ROUTED \
    SENDMAIL \
    SETUID_LOGIN \
    SHAREDOCS \
    SOURCELESS \
    SOURCELESS_HOST \
    SOURCELESS_UCODE \
    SVNLITE \
    SYSCONS \
    SYSINSTALL \
    TALK \
    TCP_WRAPPERS \
    TCSH \
    TELNET \
    TESTS \
    TEXTPROC \
    TFTP \
    TIMED \
    UNBOUND \
    USB \
    UTMPX \
    VI \
    VT \
    WIRELESS \
    WPA_SUPPLICANT_EAPOL \
    ZFS \
    ZONEINFO

__DEFAULT_NO_OPTIONS = \
    BSD_GREP \
    CLANG_EXTRAS \
    DTRACE_TESTS \
    EISA \
    HESIOD \
    LLDB \
    NAND \
    OFED \
    OPENLDAP \
    OPENSSH_NONE_CIPHER \
    SHARED_TOOLCHAIN \
    SORT_THREADS \
    SVN

#
# Default behaviour of some options depends on the architecture.  Unfortunately
# this means that we have to test TARGET_ARCH (the buildworld case) as well
# as MACHINE_ARCH (the non-buildworld case).  Normally TARGET_ARCH is not
# used at all in bsd.*.mk, but we have to make an exception here if we want
# to allow defaults for some things like clang to vary by target architecture.
# Additional, per-target behavior should be rarely added only after much
# gnashing of teeth and grinding of gears.
#
.if defined(TARGET_ARCH)
__T=${TARGET_ARCH}
.else
__T=${MACHINE_ARCH}
.endif
.if defined(TARGET)
__TT=${TARGET}
.else
__TT=${MACHINE}
.endif

.include <bsd.compiler.mk>
.if !${COMPILER_FEATURES:Mc++11}
# If the compiler is not C++11 capable, disable clang and use gcc instead.
__DEFAULT_YES_OPTIONS+=GCC GCC_BOOTSTRAP GNUCXX
__DEFAULT_NO_OPTIONS+=CLANG CLANG_BOOTSTRAP CLANG_FULL CLANG_IS_CC
.elif ${__T} == "aarch64" || ${__T} == "amd64" || ${__TT} == "arm" || \
    ${__T} == "i386"
# On x86 and arm, clang is enabled, and will be installed as the default cc.
__DEFAULT_YES_OPTIONS+=CLANG CLANG_BOOTSTRAP CLANG_FULL CLANG_IS_CC
__DEFAULT_NO_OPTIONS+=GCC GCC_BOOTSTRAP GNUCXX
.elif ${__T:Mpowerpc*}
# On powerpc, clang is enabled, but gcc is installed as the default cc.
__DEFAULT_YES_OPTIONS+=CLANG CLANG_FULL GCC GCC_BOOTSTRAP GNUCXX
__DEFAULT_NO_OPTIONS+=CLANG_BOOTSTRAP CLANG_IS_CC
.else
# Everything else disables clang, and uses gcc instead.
__DEFAULT_YES_OPTIONS+=GCC GCC_BOOTSTRAP GNUCXX
__DEFAULT_NO_OPTIONS+=CLANG CLANG_BOOTSTRAP CLANG_FULL CLANG_IS_CC
.endif
.if ${__T} == "aarch64"
BROKEN_OPTIONS+=BINUTILS BINUTILS_BOOTSTRAP GCC GCC_BOOTSTRAP GDB
__DEFAULT_YES_OPTIONS+=ELFCOPY_AS_OBJCOPY
.else
__DEFAULT_NO_OPTIONS+=ELFCOPY_AS_OBJCOPY
.endif
# LLVM lacks support for FreeBSD 64-bit atomic operations for ARMv4/ARMv5
.if ${__T} == "arm" || ${__T} == "armeb"
BROKEN_OPTIONS+=LLDB
.endif

.include <bsd.mkopt.mk>

#
# MK_* options that default to "yes" if the compiler is a C++11 compiler.
#
.for var in \
    LIBCPLUSPLUS
.if !defined(MK_${var})
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
.endif
.endfor

#
# Force some options off if their dependencies are off.
# Order is somewhat important.
#
.if ${MK_LIBPTHREAD} == "no"
MK_LIBTHR:=	no
.endif

.if ${MK_LDNS} == "no"
MK_LDNS_UTILS:=	no
MK_UNBOUND:= no
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
MK_GNUCXX:=	no
.endif

.if ${MK_MAIL} == "no"
MK_MAILWRAPPER:= no
MK_SENDMAIL:=	no
MK_DMAGENT:=	no
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

.if ${MK_TESTS} == "no"
MK_DTRACE_TESTS:= no
.endif

.if ${MK_TEXTPROC} == "no"
MK_GROFF:=	no
.endif

.if ${MK_CROSS_COMPILER} == "no"
MK_BINUTILS_BOOTSTRAP:= no
MK_CLANG_BOOTSTRAP:= no
MK_GCC_BOOTSTRAP:= no
.endif

.if ${MK_TOOLCHAIN} == "no"
MK_BINUTILS:=	no
MK_CLANG:=	no
MK_GCC:=	no
MK_GDB:=	no
MK_INCLUDES:=	no
.endif

.if ${MK_CLANG} == "no"
MK_CLANG_EXTRAS:= no
MK_CLANG_FULL:= no
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
    KERBEROS \
    KVM \
    NETGRAPH \
    PAM \
    TESTS \
    WIRELESS
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
.if defined(WITH_${vv:H})
MK_${vv:H}:=	yes
.elif defined(WITHOUT_${vv:H})
MK_${vv:H}:=	no
.else
MK_${vv:H}:=	${MK_${vv:T}}
.endif
.endfor

.if !${COMPILER_FEATURES:Mc++11}
MK_LLDB:=	no
.endif

# gcc 4.8 and newer supports libc++, so suppress gnuc++ in that case.
# while in theory we could build it with that, we don't want to do
# that since it creates too much confusion for too little gain.
.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 40800
MK_GNUCXX:=no
MK_GCC:=no
.endif

.endif #  !target(__<src.opts.mk>__)
