# $FreeBSD$

TMACOWN?=	${BINOWN}
TMACGRP?=	${BINGRP}
TMACMODE?=	${NOBINMODE}
TMACDIR?=	${SHAREDIR}/tmac

FILES=		0.MT 5.MT 4.MT ms.cov se_ms.cov
LOCALE=		locale se_locale

MAN=		groff_mm.7 groff_mmse.7 mmroff.7
MLINKS=		groff_mm.7 mm.7 groff_mmse.7 mmse.7

CLEANFILES+=	${MAN}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DIST_DIR}/mmroff.pl ${DESTDIR}${BINDIR}/mmroff
	${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		${DIST_DIR}/tmac.m ${DIST_DIR}/tmac.mse ${DESTDIR}${TMACDIR}
.for file in ${FILES}
	${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		${DIST_DIR}/mm/${file} ${DESTDIR}${TMACDIR}/mm
.endfor
.for locale in ${LOCALE}
.if !exists(${locale}) || empty(${locale})
	${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		/dev/null ${DESTDIR}${TMACDIR}/mm/${locale}
.endif
.endfor

.include "../Makefile.cfg"
.include <bsd.prog.mk>
