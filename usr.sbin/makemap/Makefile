#	@(#)Makefile	8.4 (Berkeley) 6/10/97
# $FreeBSD: src/usr.sbin/makemap/Makefile,v 1.2.6.2 2000/09/19 23:37:12 gshapiro Exp $

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/makemap

PROG=	makemap
SRCS=	makemap.c
MAN8=	makemap.8
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
