#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/mount_nfs/Makefile,v 1.8.2.1 2001/04/25 10:58:37 ru Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c mounttab.c
MAN=	mount_nfs.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}
.PATH: ${MOUNT} ${UMNTALL}

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS
DPADD=	${LIBKRB} ${LIBCRYPTO}
LDADD=	-lkrb -lcrypto
DISTRIBUTION=	krb
.endif

.include <bsd.prog.mk>
