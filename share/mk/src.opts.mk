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

# These options are used by the src builds. Those listed in
# __DEFAULT_YES_OPTIONS default to 'yes' and will build unless turned
# off.  __DEFAULT_NO_OPTIONS will default to 'no' and won't build
# unless turned on. Any options listed in 'BROKEN_OPTIONS' will be
# hard-wired to 'no'.  "Broken" here means not working or
# not-appropriate and/or not supported. It doesn't imply something is
# wrong with the code. There's not a single good word for this, so
# BROKEN was selected as the least imperfect one considered at the
# time. Options are added to BROKEN_OPTIONS list on a per-arch basis.
# At this time, there's no provision for mutually incompatible options.

__DEFAULT_YES_OPTIONS = \
    ACCT \
    ACPI \
    APM \
    AT \
    ATM \
    AUDIT \
    AUTHPF \
    AUTOFS \
    BHYVE \
    BLACKLIST \
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
    CAROOT \
    CASPER \
    CCD \
    CDDL \
    CLANG \
    CLANG_BOOTSTRAP \
    CLANG_IS_CC \
    CLEAN \
    CPP \
    CROSS_COMPILER \
    CRYPT \
    CUSE \
    CXX \
    CXGBETOOL \
    DIALOG \
    DICT \
    DMAGENT \
    DYNAMICROOT \
    EE \
    EFI \
    ELFTOOLCHAIN_BOOTSTRAP \
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
    GH_BC \
    GNU_DIFF \
    GOOGLETEST \
    GPIO \
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
    LIBCPLUSPLUS \
    LLD \
    LLD_BOOTSTRAP \
    LLD_IS_LD \
    LLVM_COV \
    LLVM_CXXFILT \
    LLVM_TARGET_ALL \
    LOADER_GELI \
    LOADER_LUA \
    LOADER_OFW \
    LOADER_UBOOT \
    LOCALES \
    LOCATE \
    LPR \
    LS_COLORS \
    MAIL \
    MAILWRAPPER \
    MAKE \
    MALLOC_PRODUCTION \
    MLX5TOOL \
    NDIS \
    NETCAT \
    NETGRAPH \
    NLS_CATALOGS \
    NS_CACHING \
    NTP \
    NVME \
    OFED \
    OPENSSL \
    PAM \
    PF \
    PKGBOOTSTRAP \
    PMC \
    PORTSNAP \
    PPP \
    QUOTAS \
    RADIUS_SUPPORT \
    RBOOTD \
    REPRODUCIBLE_BUILD \
    RESCUE \
    ROUTED \
    SENDMAIL \
    SERVICESDB \
    SETUID_LOGIN \
    SHARED_TOOLCHAIN \
    SHAREDOCS \
    SOURCELESS \
    SOURCELESS_HOST \
    SOURCELESS_UCODE \
    STATS \
    SYSCONS \
    SYSTEM_COMPILER \
    SYSTEM_LINKER \
    TALK \
    TCP_WRAPPERS \
    TCSH \
    TELNET \
    TEXTPROC \
    TFTP \
    UNBOUND \
    USB \
    UTMPX \
    VI \
    VT \
    WIRELESS \
    WPA_SUPPLICANT_EAPOL \
    ZFS \
    LOADER_ZFS \
    ZONEINFO

__DEFAULT_NO_OPTIONS = \
    BEARSSL \
    BHYVE_SNAPSHOT \
    CLANG_EXTRAS \
    CLANG_FORMAT \
    DTRACE_TESTS \
    EXPERIMENTAL \
    HESIOD \
    LIBSOFT \
    LLVM_ASSERTIONS \
    LLVM_BINUTILS \
    LOADER_FIREWIRE \
    LOADER_VERBOSE \
    LOADER_VERIEXEC_PASS_MANIFEST \
    OFED_EXTRA \
    OPENLDAP \
    RPCBIND_WARMSTART_SUPPORT \
    SORT_THREADS \
    SVN \
    SVNLITE \
    ZONEINFO_LEAPSECONDS_SUPPORT \

# LEFT/RIGHT. Left options which default to "yes" unless their corresponding
# RIGHT option is disabled.
__DEFAULT_DEPENDENT_OPTIONS= \
	CLANG_FULL/CLANG \
	LOADER_VERIEXEC/BEARSSL \
	LOADER_EFI_SECUREBOOT/LOADER_VERIEXEC \
	LOADER_VERIEXEC_VECTX/LOADER_VERIEXEC \
	VERIEXEC/BEARSSL \

# MK_*_SUPPORT options which default to "yes" unless their corresponding
# MK_* variable is set to "no".
#
.for var in \
    BLACKLIST \
    BZIP2 \
    INET \
    INET6 \
    KERBEROS \
    KVM \
    NETGRAPH \
    PAM \
    TESTS \
    WIRELESS
__DEFAULT_DEPENDENT_OPTIONS+= ${var}_SUPPORT/${var}
.endfor

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

# All supported backends for LLVM_TARGET_XXX
__LLVM_TARGETS= \
		aarch64 \
		arm \
		mips \
		powerpc \
		riscv \
		x86
__LLVM_TARGET_FILT=	C/(amd64|i386)/x86/:C/powerpc.*/powerpc/:C/armv[67]/arm/:C/riscv.*/riscv/:C/mips.*/mips/
.for __llt in ${__LLVM_TARGETS}
# Default enable the given TARGET's LLVM_TARGET support
.if ${__T:${__LLVM_TARGET_FILT}} == ${__llt}
__DEFAULT_YES_OPTIONS+=	LLVM_TARGET_${__llt:${__LLVM_TARGET_FILT}:tu}
# aarch64 needs arm for -m32 support.
.elif ${__T} == "aarch64" && ${__llt:Marm*} != ""
__DEFAULT_DEPENDENT_OPTIONS+=	LLVM_TARGET_ARM/LLVM_TARGET_AARCH64
# Default the rest of the LLVM_TARGETs to the value of MK_LLVM_TARGET_ALL.
.else
__DEFAULT_DEPENDENT_OPTIONS+=	LLVM_TARGET_${__llt:${__LLVM_TARGET_FILT}:tu}/LLVM_TARGET_ALL
.endif
.endfor

__DEFAULT_NO_OPTIONS+=LLVM_TARGET_BPF

.include <bsd.compiler.mk>

.if ${__T} == "aarch64" || ${__T} == "amd64" || ${__T} == "i386" || ${__T:Mpowerpc*}
__DEFAULT_YES_OPTIONS+=LLDB
.else
__DEFAULT_NO_OPTIONS+=LLDB
.endif
# LIB32 is supported on amd64, mips64, and powerpc64
.if (${__T} == "amd64" || ${__T:Mmips64*} || ${__T} == "powerpc64")
__DEFAULT_YES_OPTIONS+=LIB32
.else
BROKEN_OPTIONS+=LIB32
.endif
# Only doing soft float API stuff on armv6 and armv7
.if ${__T} != "armv6" && ${__T} != "armv7"
BROKEN_OPTIONS+=LIBSOFT
.endif
.if ${__T:Mmips*}
# GOOGLETEST cannot currently be compiled on mips due to external circumstances.
# Notably, the freebsd-gcc port isn't linking in libgcc so we end up trying ot
# link to a hidden symbol. LLVM would successfully link this in, but some of
# the mips variants are broken under LLVM until LLVM 10. GOOGLETEST should be
# marked no longer broken with the switch to LLVM.
BROKEN_OPTIONS+=GOOGLETEST SSP
.endif
# EFI doesn't exist on mips or powerpc.
.if ${__T:Mmips*} || ${__T:Mpowerpc*}
BROKEN_OPTIONS+=EFI
.endif
# OFW is only for powerpc, exclude others
.if ${__T:Mpowerpc*} == ""
BROKEN_OPTIONS+=LOADER_OFW
.endif
# UBOOT is only for arm, mips and powerpc, exclude others
.if ${__T:Marm*} == "" && ${__T:Mmips*} == "" && ${__T:Mpowerpc*} == ""
BROKEN_OPTIONS+=LOADER_UBOOT
.endif
# GELI and Lua in loader currently cause boot failures on powerpc.
# Further debugging is required -- probably they are just broken on big
# endian systems generically (they jump to null pointers or try to read
# crazy high addresses, which is typical of endianness problems).
.if ${__T:Mpowerpc*}
BROKEN_OPTIONS+=LOADER_GELI LOADER_LUA
.endif

# Kernel TLS is enabled by default on amd64 and aarch64
.if ${__T} == "aarch64" || ${__T} == "amd64"
__DEFAULT_YES_OPTIONS+=OPENSSL_KTLS
.else
__DEFAULT_NO_OPTIONS+=OPENSSL_KTLS
.endif

.if ${__T:Mmips64*}
# profiling won't work on MIPS64 because there is only assembly for o32
BROKEN_OPTIONS+=PROFILE
.endif
.if ${__T} != "aarch64" && ${__T} != "amd64" && ${__T} != "i386" && \
    ${__T} != "powerpc64"
BROKEN_OPTIONS+=CXGBETOOL
BROKEN_OPTIONS+=MLX5TOOL
.endif

# HyperV is currently x86-only
.if ${__T} != "amd64" && ${__T} != "i386"
BROKEN_OPTIONS+=HYPERV
.endif

# NVME is only aarch64, x86 and powerpc64*
.if ${__T} != "aarch64" && ${__T} != "amd64" && ${__T} != "i386" && \
    ${__T:Mpowerpc64*} == ""
BROKEN_OPTIONS+=NVME
.endif

.if ${__T} == "aarch64" || ${__T} == "amd64" || ${__T} == "i386" || \
    ${__T:Mpowerpc64*} != "" || ${__T:Mriscv64*} != ""
__DEFAULT_YES_OPTIONS+=OPENMP
.else
__DEFAULT_NO_OPTIONS+=OPENMP
.endif

.include <bsd.mkopt.mk>

#
# Force some options off if their dependencies are off.
# Order is somewhat important.
#
.if ${MK_CAPSICUM} == "no"
MK_CASPER:=	no
.endif

.if ${MK_SOURCELESS} == "no"
MK_SOURCELESS_HOST:=	no
MK_SOURCELESS_UCODE:= no
.endif

.if ${MK_CDDL} == "no"
MK_ZFS:=	no
MK_LOADER_ZFS:=	no
MK_CTF:=	no
.endif

.if ${MK_CRYPT} == "no"
MK_OPENSSL:=	no
MK_OPENSSH:=	no
MK_KERBEROS:=	no
MK_KERBEROS_SUPPORT:=	no
.endif

.if ${MK_CXX} == "no"
MK_CLANG:=	no
MK_GOOGLETEST:=	no
MK_OFED:=	no
MK_OPENMP:=	no
MK_PMC:=	no
MK_TESTS:=	no
.endif

.if ${MK_DIALOG} == "no"
MK_BSDINSTALL:=	no
.endif

.if ${MK_FILE} == "no"
MK_SVNLITE:=	no
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

.if ${MK_NLS} == "no"
MK_NLS_CATALOGS:= no
.endif

.if ${MK_OPENSSL} == "no"
MK_DMAGENT:=	no
MK_OPENSSH:=	no
MK_OPENSSL_KTLS:=	no
MK_KERBEROS:=	no
MK_KERBEROS_SUPPORT:=	no
MK_LDNS:=	no
MK_PKGBOOTSTRAP:=	no
MK_SVN:=		no
MK_SVNLITE:=		no
MK_ZFS:=	no
.endif

.if ${MK_LDNS} == "no"
MK_LDNS_UTILS:=	no
MK_UNBOUND:= no
.endif

.if ${MK_PF} == "no"
MK_AUTHPF:=	no
.endif

.if ${MK_OFED} == "no"
MK_OFED_EXTRA:=	no
.endif

.if ${MK_TESTS} == "no"
MK_DTRACE_TESTS:= no
.endif

.if ${MK_TESTS_SUPPORT} == "no"
MK_GOOGLETEST:=	no
.endif

.if ${MK_ZONEINFO} == "no"
MK_ZONEINFO_LEAPSECONDS_SUPPORT:= no
.endif

.if ${MK_CROSS_COMPILER} == "no"
MK_CLANG_BOOTSTRAP:= no
MK_ELFTOOLCHAIN_BOOTSTRAP:= no
MK_LLD_BOOTSTRAP:= no
.endif

.if ${MK_TOOLCHAIN} == "no"
MK_CLANG:=	no
MK_INCLUDES:=	no
MK_LLD:=	no
MK_LLDB:=	no
MK_LLVM_BINUTILS:=	no
.endif

.if ${MK_CLANG} == "no"
MK_CLANG_EXTRAS:= no
MK_CLANG_FORMAT:= no
MK_CLANG_FULL:= no
MK_LLVM_COV:= no
.endif

.if ${MK_LLVM_BINUTILS} == "yes"
# MK_LLVM_CXXFILT is a subset of MK_LLVM_BINUTILS and should therefore be
# enabled if MK_LLVM_BINUTILS is set.
MK_LLVM_CXXFILT:=	yes
.endif

.if ${MK_LOADER_VERIEXEC} == "no"
MK_LOADER_VERIEXEC_PASS_MANIFEST := no
.endif

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

#
# Set defaults for the MK_*_SUPPORT variables.
#

.endif #  !target(__<src.opts.mk>__)
