# $FreeBSD$

FILES=	gallant.fnt \
	vgarom-8x8.fnt \
	vgarom-8x14.fnt \
	vgarom-8x16.fnt \
	vgarom-thin-8x8.fnt \
	vgarom-thin-8x16.fnt

CLEANFILES+=	${FILES}

.SUFFIXES: .fnt .fnt.uu .hex
.hex.fnt:
	vtfontcvt ${.IMPSRC} ${.TARGET}

.fnt.uu.fnt:
	uudecode -p < ${.IMPSRC} > ${.TARGET}

FILESDIR=	${SHAREDIR}/vt/fonts

.include <bsd.prog.mk>
