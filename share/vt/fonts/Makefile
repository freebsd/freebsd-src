# $FreeBSD$

PACKAGE=	vt-data

.PATH:	${SRCTOP}/contrib/spleen

FONTS=	gallant.fnt \
	spleen-5x8.fnt \
	spleen-6x12.fnt \
	spleen-8x16.fnt \
	spleen-12x24.fnt \
	spleen-16x32.fnt \
	spleen-32x64.fnt \
	terminus-b32.fnt \
	tom-thumb.fnt \
	vgarom-8x8.fnt \
	vgarom-8x14.fnt \
	vgarom-8x16.fnt \
	vgarom-16x32.fnt \
	vgarom-thin-8x8.fnt \
	vgarom-thin-8x16.fnt
FILES=	${FONTS} INDEX.fonts

CLEANFILES+=	${FONTS}

.SUFFIXES: .bdf .fnt .fnt.uu .hex
.bdf.fnt .hex.fnt:
	vtfontcvt -o ${.TARGET} ${.IMPSRC}

FILESDIR=	${SHAREDIR}/vt/fonts

.include <bsd.prog.mk>
