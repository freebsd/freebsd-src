# $FreeBSD: src/tools/tools/sysdoc/Makefile,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

MAINTAINER=	trhodes@FreeBSD.org

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
