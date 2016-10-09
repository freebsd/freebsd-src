.include "${.CURDIR}/../devps/fontfiles.mk"
FILESDIR=	${FNTDIR}/devpslow
FILES=	DESC
.for f in ${FONTFILES}
SYMLINKS+=	../devps/$f ${FNTDIR}/devpslow/$f
.endfor

MAN=
.include <bsd.prog.mk>
.PATH:	${DOCTOOLSDIR}/troff/troff.d/font/devpslow
