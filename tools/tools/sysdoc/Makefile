# $FreeBSD: src/tools/tools/sysdoc/Makefile,v 1.1.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

MAINTAINER=	trhodes@FreeBSD.org

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
