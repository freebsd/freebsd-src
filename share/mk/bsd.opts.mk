#
# Option file for bmake builds. These options are available to all users of
# bmake (including the source tree userland and kernel builds). They generally
# control how binaries are made, shared vs dynamic, etc and some general options
# relevant for all build environments.
#
# Users define WITH_FOO and WITHOUT_FOO on the command line or in /etc/src.conf
# and /etc/make.conf files. These translate in the build system to MK_FOO={yes,no}
# with (usually) sensible defaults.
#
# Makefiles must include bsd.opts.mk after defining specific MK_FOO options that
# are applicable for that Makefile (typically there are none, but sometimes there
# are exceptions). Recursive makes usually add MK_FOO=no for options that they wish
# to omit from that make.
#
# Makefiles must include bsd.mkopt.mk before they test the value of any MK_FOO
# variable.
#
# Makefiles may also assume that this file is included by bsd.own.mk should it
# need variables defined there prior to the end of the Makefile where
# bsd.{subdir,lib.bin}.mk is traditionally included.
#
# The old-style YES_FOO and NO_FOO are being phased out. No new instances of them
# should be added. Old instances should be removed since they were just to
# bridge the gap between FreeBSD 4 and FreeBSD 5.
#
# Makefiles should never test WITH_FOO or WITHOUT_FOO directly (although an
# exception is made for _WITHOUT_SRCONF which turns off this mechanism
# completely).
#

.if !target(__<bsd.opts.mk>__)
__<bsd.opts.mk>__:

.if !defined(_WITHOUT_SRCCONF)
#
# Define MK_* variables (which are either "yes" or "no") for users
# to set via WITH_*/WITHOUT_* in /etc/src.conf and override in the
# make(1) environment.
# These should be tested with `== "no"' or `!= "no"' in makefiles.
# The NO_* variables should only be set by makefiles for variables
# that haven't been converted over.
#

# Only these options are used by bsd.*.mk. KERBEROS and OPENSSH are
# unfortunately needed to support statically linking the entire
# tree. su(1) wouldn't link since it depends on PAM which depends on
# ssh libraries when building with OPENSSH, and likewise for KERBEROS.

# All other variables used to build /usr/src live in src.opts.mk
# and variables from both files are documented in src.conf(5).

__DEFAULT_YES_OPTIONS = \
    ASSERT_DEBUG \
    DEBUG_FILES \
    DOCCOMPRESS \
    INCLUDES \
    INSTALLLIB \
    KERBEROS \
    MAKE_CHECK_USE_SANDBOX \
    MAN \
    MANCOMPRESS \
    MANSPLITPKG \
    NIS \
    NLS \
    OPENSSH \
    RELRO \
    SSP \
    TESTS \
    TOOLCHAIN \
    UNDEFINED_VERSION \
    WARNS \
    WERROR

__DEFAULT_NO_OPTIONS = \
    ASAN \
    BIND_NOW \
    CCACHE_BUILD \
    CTF \
    INSTALL_AS_USER \
    PROFILE \
    RETPOLINE \
    STALE_STAGED \
    UBSAN

__DEFAULT_DEPENDENT_OPTIONS = \
    MAKE_CHECK_USE_SANDBOX/TESTS \
    STAGING_MAN/STAGING \
    STAGING_PROG/STAGING \
    STALE_STAGED/STAGING \

#
# Default to disabling PIE on 32-bit architectures. The small address space
# means that ASLR is of limited effectiveness, and it may cause issues with
# some memory-hungry workloads.
#
.if ${MACHINE_ARCH} == "armv6" || ${MACHINE_ARCH} == "armv7" \
    || ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" \
    || ${MACHINE_ARCH} == "powerpcspe"
__DEFAULT_NO_OPTIONS+= PIE
.else
__DEFAULT_YES_OPTIONS+=PIE
.endif

__SINGLE_OPTIONS = \
   INIT_ALL

__INIT_ALL_OPTIONS=	none pattern zero
__INIT_ALL_DEFAULT=	none

.-include <local.opts.mk>

.include <bsd.mkopt.mk>

.include <bsd.cpu.mk>

.endif # !_WITHOUT_SRCCONF

.endif
