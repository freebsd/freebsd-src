# $FreeBSD$

MAINTAINER=	gshapiro@FreeBSD.org

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/libsmdb

CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include
CFLAGS+=-DNEWDB -DNOT_SENDMAIL

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}

LIB=	smdb

SRCS+=	smdb.c smdb1.c smdb2.c smndbm.c

INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
