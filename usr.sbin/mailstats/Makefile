#	@(#)Makefile	8.2 (Berkeley) 9/21/96
# $FreeBSD$

MAINTAINER=	gshapiro@FreeBSD.org

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/mailstats

PROG=	mailstats
SRCS=	mailstats.c
MAN8=	mailstats.8
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -DNOT_SENDMAIL

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
