#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	termcap
CFLAGS+=-DCM_N -DCM_GT -DCM_B -DCM_D
SRCS=   termcap.c tgoto.c tputs.c tparm.c

MAN3=	termcap.3
MLINKS=	termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
	termcap.3 tgetstr.3 termcap.3 tgoto.3 termcap.3 tputs.3 \
	termcap.3 tparm.3
LINKS=	${LIBDIR}/libtermcap.a ${LIBDIR}/libtermlib.a
.if !defined(NOPIC)
LINKS+= ${SHLIBDIR}/libtermcap.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	${SHLIBDIR}/libtermlib.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
.endif
.if !defined(NOPROFILE)
LINKS+=	${LIBDIR}/libtermcap_p.a ${LIBDIR}/libtermlib_p.a
.endif

beforeinstall:
	-cd ${.CURDIR}; cmp -s termcap.h ${DESTDIR}/usr/include/termcap.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 termcap.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>
