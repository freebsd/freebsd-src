# $FreeBSD: src/lib/libsmutil/Makefile,v 1.1.2.4 2002/03/25 21:31:35 gshapiro Exp $

MAINTAINER=	gshapiro@FreeBSD.org

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/libsmutil

CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+=-DNEWDB -DNIS -DMAP_REGEX -DNOT_SENDMAIL

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}

LIB=	smutil

SRCS+=	sm_os.h
SRCS+=	debug.c err.c lockfile.c safefile.c snprintf.c cf.c
CLEANFILES+=sm_os.h

INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

sm_os.h:
	ln -sf ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h sm_os.h

.include <bsd.lib.mk>
