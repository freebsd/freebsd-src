# $Id: Makefile.inc,v 1.2 1998/06/09 22:59:16 jb Exp $
#
# Machine dependent definitions for the alpha architecture.
#
# If you want to build a libc for use with a NetBSD kernel, edit your
# /etc/make.conf to include the lines:
#
# NETBSD_SYSCALLS=1
# CFLAGS=-D__NETBSD_SYSCALLS
#
# The alpha libc now uses FreeBSD syscalls by default.

#
# Alpha is 64-bit, so it doesn't need quad functions:
#
NO_QUAD=1
