# $FreeBSD$

LIB=		gnumalloc
INTERNALLIB=	yes		# Do not build or install ${LIB}*.a
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	cfree.c

.if ${OBJFORMAT} != aout
NOPIC=		true
.endif

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that programs that use autoconf will not "detect" -lgnumalloc
# and continue to propagate the bogosity.  The ldconfig path fix will enable
# you to run programs that were linked with -lgnumalloc (such as XFree86).
#
beforeinstall:
	-rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.a \
	 ${DESTDIR}${LIBDIR}/lib${LIB}_p.a \
	 ${DESTDIR}${ORIG_SHLIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	 ${DESTDIR}/usr/lib/compat/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	 ${DESTDIR}/usr/lib/compat/lib${LIB}.so.${SHLIB_MAJOR} \
	 ${DESTDIR}/usr/lib/compat/lib${LIB}.so \

.include <bsd.lib.mk>

# This must follow the .include in case SHLIBDIR is defined there.
ORIG_SHLIBDIR:=	${SHLIBDIR}

# The ldconfig line in/etc/rc doesn't depend on ${LIBDIR} or ${SHLIBDIR},
# so neither does this.
SHLIBDIR=	/usr/lib/compat/aout
