# $FreeBSD$

LIB=		y
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	main.c yyerror.c

.if ${OBJFORMAT} != aout
NOPIC=	true
.endif

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that `ld' will not continue to generate binaries linked
# shared against liby, so that in a future release we can move this
# off to a compat dist (like compat22).
#
beforeinstall:
	rm -f ${DESTDIR}/usr/lib/compat/lib${LIB}.so \
	 ${DESTDIR}${ORIG_SHLIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	 ${DESTDIR}/usr/lib/compat/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	 ${DESTDIR}/usr/lib/compat/lib${LIB}.so.${SHLIB_MAJOR}

.include <bsd.lib.mk>

# This must follow the .include in case SHLIBDIR is defined there.
ORIG_SHLIBDIR:=	${SHLIBDIR}

.if ${OBJFORMAT} == aout
# The ldconfig line in/etc/rc doesn't depend on ${LIBDIR} or ${SHLIBDIR},
# so neither does this.
SHLIBDIR=	/usr/lib/compat/aout
.endif
