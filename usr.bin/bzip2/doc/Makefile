# $FreeBSD: src/usr.bin/bzip2/doc/Makefile,v 1.2 2002/02/01 16:33:40 sobomax Exp $

.PATH: ${.CURDIR}/../../../contrib/bzip2

INFO=	bzip2
INFOSECTION=	"Programming & development tools."
MAKEINFOFLAGS=	--no-validate

CLEANFILES+=	bzip2.texi bzip2.texi.orig

bzip2.texi:	manual.texi manual.texi.diff
	patch -s -b .orig -o ${.TARGET} < ${.ALLSRC:M*.diff} ${.ALLSRC:M*.texi}

.include <bsd.info.mk>
