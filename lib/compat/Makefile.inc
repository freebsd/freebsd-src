# $FreeBSD: src/lib/compat/Makefile.inc,v 1.6.2.2 2002/07/17 19:08:24 ru Exp $

# Suck in ${SHLIBDIR} definition.
.include <bsd.own.mk>

.if defined(LIBS) && !empty(LIBS)
beforeinstall: __remove-stale-libs
__remove-stale-libs: .PHONY
.for lib in ${LIBS}
.if exists(${DESTDIR}${SHLIBDIR}/${lib})
	-chflags noschg ${DESTDIR}${SHLIBDIR}/${lib}
	rm -f ${DESTDIR}${SHLIBDIR}/${lib}
.endif
.endfor
.endif
