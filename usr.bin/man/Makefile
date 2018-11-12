# $FreeBSD$

.include <src.opts.mk>

SCRIPTS= man.sh
LINKS=	${BINDIR}/man ${BINDIR}/manpath

MAN=	man.1 manpath.1 man.conf.5

.if ${MK_MANDOCDB} == no
LINKS+=	${BINDIR}/man ${BINDIR}/apropos \
	${BINDIR}/man ${BINDIR}/whatis
MAN+=	apropos.1
MLINKS=	apropos.1 whatis.1
.endif

.include <bsd.prog.mk>
