#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/mail.local

PROG=	mail.local
SRCS=	mail.local.c
MAN8=	mail.local.8
BINMODE=4555
INSTALLFLAGS=-fschg
CFLAGS+=-I${SENDMAIL_DIR}/include

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
