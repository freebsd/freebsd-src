#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	termcap
SHLIB_MAJOR= 2
SHLIB_MINOR= 1
CFLAGS+=-DCM_N -DCM_GT -DCM_B -DCM_D -I${.CURDIR}
SRCS=   termcap.c tgoto.c tputs.c tparm.c tospeed.c

MAN3=	termcap.3
MLINKS=	termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
	termcap.3 tgetstr.3 termcap.3 tgoto.3 termcap.3 tputs.3 \
	termcap.3 tparm.3 termcap.3 __set_ospeed.3

LINKS=	${LIBDIR}/libtermcap.a ${LIBDIR}/libtermlib.a
.if !defined(NOPIC)
.if ${BINFORMAT} == aout
LINKS+= ${SHLIBDIR}/libtermcap.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	${SHLIBDIR}/libtermlib.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.else
# no need for major at all, it's an ld-time redirection only
LINKS+=	${SHLIBDIR}/libtermcap.so \
	${SHLIBDIR}/libtermlib.so
.endif
.endif
.if !defined(NOPROFILE)
LINKS+=	${LIBDIR}/libtermcap_p.a ${LIBDIR}/libtermlib_p.a
.endif

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/termcap.h \
		${DESTDIR}/usr/include

.if ${BINFORMAT} == aout
# This is a hack.. Work around a major number bump that should have been
# done differently.  Back out this delta when it's time to go to 3.0 for real.
.if !defined(NOPIC)
afterinstall:
.for lib in libtermcap.so libtermlib.so
	rm -f ${DESTDIR}/usr/lib/compat/${lib}.3.0
	ln -s ${SHLIBDIR}/${lib}.${SHLIB_MAJOR}.${SHLIB_MINOR} \
		${DESTDIR}/usr/lib/compat/${lib}.3.0
	rm -f ${DESTDIR}${SHLIBDIR}/${lib}.3.0
.endfor
.endif
.endif

.include <bsd.lib.mk>
