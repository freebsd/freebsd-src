#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

MAINTAINER=	gshapiro@FreeBSD.org

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/mail.local

PROG=	mail.local
SRCS=	mail.local.c
MAN=	mail.local.8
CFLAGS+=-I${SENDMAIL_DIR}/include
WFORMAT=0

.if exists(${.OBJDIR}/../../lib/libsmutil)
LIBSMUTILDIR:=	${.OBJDIR}/../../lib/libsmutil
.else
LIBSMUTILDIR!=	cd ${.CURDIR}/../../lib/libsmutil; make -V .OBJDIR
.endif
LIBSMUTIL:=	${LIBSMUTILDIR}/libsmutil.a

DPADD=	${LIBSMUTIL}
LDADD=	${LIBSMUTIL}

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
DPADD+=${SENDMAIL_DPADD}
LDADD+=${SENDMAIL_LDADD}
LDFLAGS+=${SENDMAIL_LDFLAGS}

.include <bsd.prog.mk>
