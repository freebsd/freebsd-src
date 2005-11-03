# $FreeBSD: src/usr.bin/bzip2/doc/Makefile,v 1.3 2005/05/31 22:16:49 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/bzip2

INFO=	bzip2
INFOSECTION=	"Programming & development tools."
MAKEINFOFLAGS=	--no-validate

CLEANFILES+=	bzip2.texi

bzip2.texi:	manual.texi
	grep -v '^@bf{' ${.ALLSRC} > ${.TARGET}

.include <bsd.info.mk>
