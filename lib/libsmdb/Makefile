# $FreeBSD: src/lib/libsmdb/Makefile,v 1.5 2002/05/13 11:09:05 ru Exp $

MAINTAINER=	gshapiro@FreeBSD.org

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/libsmdb

CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+=-DNEWDB -DNOT_SENDMAIL

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}

LIB=	smdb

SRCS+=	sm_os.h
SRCS+=	smdb.c smdb1.c smdb2.c smndbm.c
CLEANFILES+=sm_os.h

INTERNALLIB=		true

sm_os.h:
	ln -sf ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h sm_os.h

.include <bsd.lib.mk>
