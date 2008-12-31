# $FreeBSD: src/sys/modules/nmdm/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/nmdm

KMOD=	nmdm
SRCS=	nmdm.c
SRCS+=	opt_compat.h opt_tty.h vnode_if.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" >opt_compat.h

opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h
.endif

.include <bsd.kmod.mk>
