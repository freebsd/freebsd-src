# $FreeBSD: src/usr.sbin/stallion/bootcode/Makefile,v 1.9.2.3 2002/02/18 15:03:37 ru Exp $

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
