
# Options set in the build system which affect the building of kernel
# modules. These select which parts to compile in or out (eg INET) or which
# parts to omit (eg CDDL or SOURCELESS_HOST). Some of these will cause
# config.mk to define symbols in various opt_*.h files.

#
# Define MK_* variables (which are either "yes" or "no") for users
# to set via WITH_*/WITHOUT_* in /etc/src.conf and override in the
# make(1) environment.
# These should be tested with `== "no"' or `!= "no"' in makefiles.
# The NO_* variables should only be set by makefiles for variables
# that haven't been converted over.
#

# Note: bsd.own.mk must be included before the rest of kern.opts.mk to make
# building on 10.x and earlier work. This should be removed when that's no
# longer supported since it confounds the defaults (since it uses the host's
# notion of defaults rather than what's default in current when building
# within sys/modules).
.include <bsd.own.mk>

# These options are used by the kernel build process (kern.mk and kmod.mk)
# They have to be listed here so we can build modules outside of the
# src tree.

KLDXREF_CMD?=	kldxref

__DEFAULT_YES_OPTIONS = \
    AUTOFS \
    BHYVE \
    BLUETOOTH \
    CCD \
    CDDL \
    CRYPT \
    CUSE \
    DTRACE \
    EFI \
    FORMAT_EXTENSIONS \
    INET \
    INET6 \
    IPFILTER \
    IPSEC_SUPPORT \
    ISCSI \
    KERNEL_SYMBOLS \
    NETGRAPH \
    OFED \
    PF \
    SCTP_SUPPORT \
    SOURCELESS_HOST \
    SOURCELESS_UCODE \
    SPLIT_KERNEL_DEBUG \
    TESTS \
    USB_GADGET_EXAMPLES \
    VIMAGE \
    ZFS

__DEFAULT_NO_OPTIONS = \
    BHYVE_SNAPSHOT \
    KERNEL_BIN \
    KERNEL_RETPOLINE \
    RATELIMIT \
    REPRODUCIBLE_BUILD \
    VERIEXEC

# Some options are totally broken on some architectures. We disable them. If you
# need to enable them on an experimental basis, you must change this code.
# Note: These only apply to the list of modules we build by default and
# sometimes what is in the opt_*.h files by default.  Kernel config files are
# unaffected, though some targets can be affected by KERNEL_BIN, KERNEL_SYMBOLS,
# FORMAT_EXTENSIONS, CTF and SSP.

# Broken on 32-bit arm, kernel module compile errors
.if ${MACHINE_CPUARCH} == "arm"
BROKEN_OPTIONS+= OFED
.endif

# Things that don't work based on toolchain support.
.if ${MACHINE} != "i386" && ${MACHINE} != "amd64"
BROKEN_OPTIONS+= KERNEL_RETPOLINE
.endif

# EFI doesn't exist on powerpc or riscv and is broken on i386
.if ${MACHINE:Mpowerpc} || ${MACHINE:Mriscv} || ${MACHINE} == "i386"
BROKEN_OPTIONS+=EFI
.endif

# We only generate kernel.bin on arm and arm64, otherwise they break the build.
.if ${MACHINE} != "arm" && ${MACHINE} != "arm64"
BROKEN_OPTIONS+=KERNEL_BIN
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
__DEFAULT_NO_OPTIONS += FDT
.else
__DEFAULT_YES_OPTIONS += FDT
.endif

__SINGLE_OPTIONS = \
	INIT_ALL

__INIT_ALL_OPTIONS=	none pattern zero
__INIT_ALL_DEFAULT=	none
.if ${MACHINE} == "amd64"
# PR251083 conflict between INIT_ALL_ZERO and ifunc memset
BROKEN_SINGLE_OPTIONS+=	INIT_ALL zero none
.endif

# expanded inline from bsd.mkopt.mk to avoid share/mk dependency

# Those that default to yes
.for var in ${__DEFAULT_YES_OPTIONS}
.if !defined(MK_${var})
.if defined(WITHOUT_${var})			# WITHOUT always wins
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.else
.if ${MK_${var}} != "yes" && ${MK_${var}} != "no"
.error "Illegal value for MK_${var}: ${MK_${var}}"
.endif
.endif # !defined(MK_${var})
.endfor
.undef __DEFAULT_YES_OPTIONS

# Those that default to no
.for var in ${__DEFAULT_NO_OPTIONS}
.if !defined(MK_${var})
.if defined(WITH_${var}) && !defined(WITHOUT_${var}) # WITHOUT always wins
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.else
.if ${MK_${var}} != "yes" && ${MK_${var}} != "no"
.error "Illegal value for MK_${var}: ${MK_${var}}"
.endif
.endif # !defined(MK_${var})
.endfor
.undef __DEFAULT_NO_OPTIONS

#
# MK_* options which are always no, usually because they are
# unsupported/badly broken on this architecture.
#
.for var in ${BROKEN_OPTIONS}
MK_${var}:=	no
.endfor
.undef BROKEN_OPTIONS

#
# Group options set an OPT_FOO variable for each option.
#
.for opt in ${__SINGLE_OPTIONS}
.if !defined(__${opt}_OPTIONS) || empty(__${opt}_OPTIONS)
.error __${opt}_OPTIONS not defined or empty
.endif
.if !defined(__${opt}_DEFAULT) || empty(__${opt}_DEFAULT)
.error __${opt}_DEFAULT undefined or empty
.endif
.if defined(${opt})
OPT_${opt}:=	${${opt}}
.else
OPT_${opt}:=	${__${opt}_DEFAULT}
.endif
.if empty(OPT_${opt}) || ${__${opt}_OPTIONS:M${OPT_${opt}}} != ${OPT_${opt}}
.error Invalid option OPT_${opt} (${OPT_${opt}}), must be one of: ${__${opt}_OPTIONS}
.endif
.endfor
.undef __SINGLE_OPTIONS

.for opt val rep in ${BROKEN_SINGLE_OPTIONS}
.if ${OPT_${opt}} == ${val}
OPT_${opt}:=	${rep}
.endif
.endfor
#end of bsd.mkopt.mk expanded inline.

#
# MK_*_SUPPORT options which default to "yes" unless their corresponding
# MK_* variable is set to "no".
#
.for var in \
    INET \
    INET6 \
    VIMAGE
.if defined(WITHOUT_${var}_SUPPORT) || ${MK_${var}} == "no"
MK_${var}_SUPPORT:= no
.else
.if defined(KERNBUILDDIR)	# See if there's an opt_foo.h
.if !defined(OPT_${var})
OPT_${var}!= cat ${KERNBUILDDIR}/opt_${var:tl}.h; echo
.export OPT_${var}
.endif
.if ${OPT_${var}} == ""		# nothing -> no
MK_${var}_SUPPORT:= no
.else
MK_${var}_SUPPORT:= yes
.endif
.else				# otherwise, yes
MK_${var}_SUPPORT:= yes
.endif
.endif
.endfor

.if ${MK_SPLIT_KERNEL_DEBUG} == "no"
MK_KERNEL_SYMBOLS:=	no
.endif

.if ${MK_CDDL} == "no"
MK_DTRACE:=	no
.endif

# Some modules only compile successfully if option FDT is set, due to #ifdef FDT
# wrapped around declarations.  Module makefiles can optionally compile such
# things using .if !empty(OPT_FDT)
.if !defined(OPT_FDT) && defined(KERNBUILDDIR)
OPT_FDT!= sed -n '/FDT/p' ${KERNBUILDDIR}/opt_platform.h
.export OPT_FDT
.if empty(OPT_FDT)
MK_FDT:=no
.else
MK_FDT:=yes
.endif
.endif
