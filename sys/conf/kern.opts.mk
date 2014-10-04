# $FreeBSD$

# Options set in the build system that affect the kernel somehow.

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

__DEFAULT_YES_OPTIONS = \
    BLUETOOTH \
    CDDL \
    CRYPT \
    FORMAT_EXTENSIONS \
    INET \
    INET6 \
    IPFILTER \
    KERNEL_SYMBOLS \
    NETGRAPH \
    PF \
    SOURCELESS_HOST \
    SOURCELESS_UCODE \
    USB_GADGET_EXAMPLES \
    ZFS

__DEFAULT_NO_OPTIONS = \
    EISA \
    NAND \
    OFED

# expanded inline from bsd.mkopt.mk to avoid share/mk dependency

# Those that default to yes
.for var in ${__DEFAULT_YES_OPTIONS}
.if !defined(MK_${var})
.if defined(WITHOUT_${var})			# WITHOUT always wins
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.endif
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
.endif
.endfor
.undef __DEFAULT_NO_OPTIONS

#
# MK_*_SUPPORT options which default to "yes" unless their corresponding
# MK_* variable is set to "no".
#
.for var in \
    INET \
    INET6
.if defined(WITHOUT_${var}_SUPPORT) || ${MK_${var}} == "no"
MK_${var}_SUPPORT:= no
.else
MK_${var}_SUPPORT:= yes
.endif
.endfor
