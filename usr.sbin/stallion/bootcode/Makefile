# $FreeBSD$

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
