# $Id: Makefile.inc,v 1.1.1.1 1998/03/09 06:34:41 jb Exp $
#
# Machine dependent definitions for the alpha architecture.
#
# The alpha uses a NetBSD kernel:
#
NETBSD_SYSCALLS=1

# This is needed in more places than just libc, so it is now set in sys.mk
#CFLAGS+=-D__NETBSD_SYSCALLS

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=1
