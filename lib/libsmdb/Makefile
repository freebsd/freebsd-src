# $FreeBSD$

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/libsmdb

CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include
CFLAGS+=-DNEWDB -DNOT_SENDMAIL

LIB=	smdb

SRCS+=	smdb.c smdb1.c smdb2.c smndbm.c

INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
