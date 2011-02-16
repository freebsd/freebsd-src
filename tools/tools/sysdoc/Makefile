# $FreeBSD: src/tools/tools/sysdoc/Makefile,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

MAINTAINER=	trhodes@FreeBSD.org

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
