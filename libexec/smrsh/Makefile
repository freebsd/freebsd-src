#	@(#)Makefile	8.1 (Berkeley) 7/2/95
# $FreeBSD: src/libexec/smrsh/Makefile,v 1.1.6.2 2000/09/19 23:37:12 gshapiro Exp $

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/smrsh

PROG=	smrsh
SRCS=	smrsh.c
MAN8=	smrsh.8
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include

.if exists(${.OBJDIR}/../../lib/libsmutil)
LIBSMUTILDIR:=	${.OBJDIR}/../../lib/libsmutil
.else
LIBSMUTILDIR!=	cd ${.CURDIR}/../../lib/libsmutil; make -V .OBJDIR
.endif
LIBSMUTIL:=	${LIBSMUTILDIR}/libsmutil.a

DPADD+=	${LIBSMUTIL}
LDADD+=	${LIBSMUTIL}

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
LDFLAGS+=${SENDMAIL_LDFLAGS}
LDADD+=${SENDMAIL_LDADD}
DPADD+=${SENDMAIL_DPADD}

.include <bsd.prog.mk>
