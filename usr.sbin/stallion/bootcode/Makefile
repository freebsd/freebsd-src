# $FreeBSD: src/usr.sbin/stallion/bootcode/Makefile,v 1.12 2001/12/17 13:59:33 ru Exp $

.SUFFIXES: .uu
.uu:
	rm -f ${.TARGET}
	uudecode ${.IMPSRC}

FILES=	2681.sys cdk.sys
FILESDIR=	${BOOTDIR}
CLEANFILES= 	${FILES}

MAN=	stl.4
MLINKS=	stl.4 stli.4
MANSUBDIR=/i386

.include <bsd.prog.mk>
