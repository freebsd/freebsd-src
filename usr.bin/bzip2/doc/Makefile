# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/bzip2

INFO=	bzip2
INFOSECTION=	"Programming & development tools."
MAKEINFOFLAGS=	--no-validate

CLEANFILES+=	bzip2.texi

bzip2.texi:	manual.texi
	grep -v '^@bf{' ${.ALLSRC} > ${.TARGET}

.include <bsd.info.mk>
