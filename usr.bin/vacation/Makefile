# $FreeBSD$

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/vacation

PROG=	vacation
SRCS=	vacation.c
MAN8=	vacation.1
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include
CFLAGS+=-DNEWDB -DNOT_SENDMAIL
CFLAGS+=-D_FFR_LISTDB -D_FFR_DEBUG

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
