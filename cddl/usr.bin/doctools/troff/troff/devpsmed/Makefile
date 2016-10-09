.include "${.CURDIR}/../devps/fontfiles.mk"
FILESDIR=	${FNTDIR}/devpsmed
FILES=	DESC
.for f in ${FONTFILES}
SYMLINKS+=	../devps/$f ${FNTDIR}/devpsmed/$f
.endfor

MAN=
.include <bsd.prog.mk>
.PATH:	${DOCTOOLSDIR}/troff/troff.d/font/devpsmed
