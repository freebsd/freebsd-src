#	$Id: Makefile,v 1.6 1996/09/26 14:22:32 peter Exp $

LIB=		gnumalloc
SHLIBDIR=	${LIBDIR}/compat
INTERNALLIB=	yes		# Do not build or install lib*.a
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	cfree.c

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that programs that use autoconf will not "detect" -lgnumalloc
# and continue to propagate the bogosity.  The ldconfig path fix will enable
# you to run programs that were linked with -lgnumalloc (such as XFree86).
#
beforeinstall:
	rm -f ${DESTDIR}${LIBDIR}/libgnumalloc.a
	rm -f ${DESTDIR}${LIBDIR}/libgnumalloc.so.${SHLIB_MAJOR}.${SHLIB_MINOR}

.include <bsd.lib.mk>
