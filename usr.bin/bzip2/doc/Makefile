# $FreeBSD: src/usr.bin/bzip2/doc/Makefile,v 1.1.2.1 2001/08/02 01:09:06 obrien Exp $

.PATH: ${.CURDIR}/../../../contrib/bzip2

INFO=	bzip2
INFOSECTION=	"Programming & development tools."
MAKEINFOFLAGS=	--no-validate

CLEANFILES+=	bzip2.texi

bzip2.texi:	manual.texi
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.info.mk>
