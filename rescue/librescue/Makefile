#
# $FreeBSD$
#

# Certain library entries have hard-coded references to
# /bin, /sbin, etc, that require those entries to be
# recompiled for use in /rescue.  This Makefile
# accomplishes that.  Note that this is pure build hackery.
# This library should never be installed, and isn't even linked
# with in the normal way. (See ../rescue/Makefile for details.)

LIB=		rescue
NOPROFILE=	yes	# Don't generate profile version
INTERNALLIB=	yes	# Don't install this library

CFLAGS+=	-DRESCUE
# Flags copied from src/lib/libc and src/lib/libutil
CFLAGS+=	-I${.CURDIR}/../../lib/libc/include
CFLAGS+=	-D__DBINTERFACE_PRIVATE
CFLAGS+=	-DINET6
CFLAGS+=	-DPOSIX_MISTAKE
CFLAGS+=	-I${.CURDIR}/../../lib/libc/locale
CFLAGS+=	-DBROKEN_DES
CFLAGS+=	-DPORTMAP
CFLAGS+=	-DDES_BUILTIN
CFLAGS+=	-DYP
CFLAGS+=	-DHESIOD

.PATH: ${.CURDIR}/../../lib/libc/gen ${.CURDIR}/../../lib/libutil ${.CURDIR}/../../lib/libc/net ${.CURDIR}/../../lib/libc/stdlib

SRCS = exec.c getusershell.c login_class.c popen.c rcmdsh.c sysctl.c system.c

.include <bsd.lib.mk>
