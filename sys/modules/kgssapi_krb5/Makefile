# $FreeBSD: src/sys/modules/kgssapi_krb5/Makefile,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../kgssapi/krb5
KMOD=	kgssapi_krb5

SRCS=	krb5_mech.c \
	kcrypto.c \
	kcrypto_des.c \
	kcrypto_des3.c \
	kcrypto_aes.c \
	kcrypto_arcfour.c

SRCS+=		kgss_if.h gssd.h
MFILES=		kgssapi/kgss_if.m

S=		${.CURDIR}/../..

gssd.h:		$S/kgssapi/gssd.x
	rpcgen -hM $S/kgssapi/gssd.x | grep -v pthread.h > gssd.h

.include <bsd.kmod.mk>
