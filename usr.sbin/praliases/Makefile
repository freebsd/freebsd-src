#	@(#)Makefile	8.2 (Berkeley) 9/21/96
# $FreeBSD: src/usr.sbin/praliases/Makefile,v 1.1.6.2 2000/09/19 23:37:13 gshapiro Exp $

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/praliases

PROG=	praliases
SRCS=	praliases.c
MAN8=	praliases.8
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include
CFLAGS+=-DNEWDB -DNOT_SENDMAIL

.if exists(${.OBJDIR}/../../lib/libsmdb)
LIBSMDBDIR:=	${.OBJDIR}/../../lib/libsmdb
.else
LIBSMDBDIR!=	cd ${.CURDIR}/../../lib/libsmdb; make -V .OBJDIR
.endif
LIBSMDB:=	${LIBSMDBDIR}/libsmdb.a

.if exists(${.OBJDIR}/../../lib/libsmutil)
LIBSMUTILDIR:=	${.OBJDIR}/../../lib/libsmutil
.else
LIBSMUTILDIR!=	cd ${.CURDIR}/../../lib/libsmutil; make -V .OBJDIR
.endif
LIBSMUTIL:=	${LIBSMUTILDIR}/libsmutil.a

DPADD+=	${LIBSMDB} ${LIBSMUTIL}
LDADD+=	${LIBSMDB} ${LIBSMUTIL}

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
LDFLAGS+=${SENDMAIL_LDFLAGS}
LDADD+=${SENDMAIL_LDADD}
DPADD+=${SENDMAIL_DPADD}

.include <bsd.prog.mk>
