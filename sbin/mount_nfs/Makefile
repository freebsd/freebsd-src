#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN8=	mount_nfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNFS -I${MOUNT}
.PATH:	${MOUNT}

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
CFLAGS+=-DKERBEROS
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
DISTRIBUTION=	krb
.endif

.include <bsd.prog.mk>
