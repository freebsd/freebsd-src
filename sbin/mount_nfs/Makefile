#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD$

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
