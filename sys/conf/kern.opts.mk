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

# These options are used by the kernel build process (kern.mk and kmod.mk)
# They have to be listed here so we can build modules outside of the
# src tree.

__DEFAULT_YES_OPTIONS = \
    FORMAT_EXTENSIONS \
    KERNEL_SYMBOLS

__DEFAULT_NO_OPTIONS = \

# Kludge to allow a less painful transition. If MAKESYSPATH isn't defined,
# assume we have a standard FreeBSD src tree layout and reach over and grab
# bsd.mkopt.mk from there. If it is defined, trust it to point someplace sane
# and include bsd.mkopt.mk from there. We need the !defined case to keep ports
# kernel modules working (though arguably they should define MAKESYSPATH). We
# need the latter case to keep the JIRA case working where they specifically
# use a non-standard layout, but do define MAKESYSPATH correctly.
.if !defined(MAKESYSPATH)
.include "../../share/mk/bsd.mkopt.mk"
.else
.include <bsd.mkopt.mk>
.endif
