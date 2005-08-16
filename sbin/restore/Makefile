#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

PROG=	restore
LINKS=	${BINDIR}/restore ${BINDIR}/rrestore
CFLAGS+=-DRRESTORE
CFLAGS+=-I${.CURDIR}/../../libexec/rlogind
SRCS=	main.c interactive.c restore.c dirs.c symtab.c tape.c utilities.c \
	dumprmt.c
MAN=	restore.8
MLINKS+=restore.8 rrestore.8
.PATH:	${.CURDIR}/../dump

.if defined(MAKE_KERBEROS4) && !defined(NO_OPENSSL) && !defined(NOCRYPT)
.PATH:	${.CURDIR}/../../crypto/kerberosIV/appl/bsd
SRCS+=	krcmd.c kcmd.c rcmd_util.c
DPADD+=	${LIBKRB} ${LIBCRYPTO}
LDADD+=	-lkrb -lcrypto
CFLAGS+=-DKERBEROS -DHAVE_CONFIG_H \
	-I${.CURDIR}/../../kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/include \
	-I${.CURDIR}/../../crypto/kerberosIV/appl/bsd
DISTRIBUTION=	krb4
.endif

.include <bsd.prog.mk>
