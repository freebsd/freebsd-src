#	@(#)Makefile	8.1 (Berkeley) 7/2/95
# $FreeBSD$

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

.include <bsd.prog.mk>
