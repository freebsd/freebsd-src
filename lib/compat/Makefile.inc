# $FreeBSD: src/lib/compat/Makefile.inc,v 1.6.2.1 2001/11/27 02:49:59 peter Exp $

LIBCOMPATDIR?=	${LIBDIR}/compat/aout

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
