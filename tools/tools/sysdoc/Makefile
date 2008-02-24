# $FreeBSD: src/tools/tools/sysdoc/Makefile,v 1.1 2005/03/01 05:48:37 trhodes Exp $

MAINTAINER=	trhodes@FreeBSD.org

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
