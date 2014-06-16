# $FreeBSD$

FILES=	gallant.fnt

CLEANFILES+=	${FILES}

.SUFFIXES: .uu
.uu:
	uudecode < ${.IMPSRC}

FILESDIR=	${SHAREDIR}/vt/fonts

.include <bsd.prog.mk>
