# $Id$
#
# Machine dependent definitions for the alpha architecture.
#
# The alpha uses a NetBSD kernel:
#
NETBSD_SYSCALLS=1

CFLAGS+=-D__NETBSD_SYSCALLS

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=1
