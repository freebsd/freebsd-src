#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=		resolv
SHLIBDIR=	${LIBDIR}/compat
INTERNALLIB=	yes		# Do not build or install libresolv*.a
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	fakelib.c

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that programs that use autoconf will not "detect" -lresolv
# and continue to propagate the bogosity.  The ldconfig path fix will
# enable you to run programs that were linked with -lresolv.
#
# libresolv on FreeBSD-2.x **never did anything**!!
#
beforeinstall:
	rm -f ${DESTDIR}${LIBDIR}/libresolv.a
	rm -f ${DESTDIR}${LIBDIR}/libresolv.so.${SHLIB_MAJOR}.${SHLIB_MINOR}

.include <bsd.lib.mk>
