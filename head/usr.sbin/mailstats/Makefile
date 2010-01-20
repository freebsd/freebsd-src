#	@(#)Makefile	8.2 (Berkeley) 9/21/96
# $FreeBSD$

SENDMAIL_DIR= ${.CURDIR}/../../contrib/sendmail
.PATH: ${SENDMAIL_DIR}/mailstats

PROG=	mailstats
SRCS=	mailstats.c
MAN=	mailstats.8

CFLAGS+= -I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+= -DNOT_SENDMAIL

WARNS?=	2

LIBSMDIR=	${.OBJDIR}/../../lib/libsm
LIBSM=		${LIBSMDIR}/libsm.a

LIBSMUTILDIR=	${.OBJDIR}/../../lib/libsmutil
LIBSMUTIL=	${LIBSMUTILDIR}/libsmutil.a

DPADD=	${LIBSMUTIL} ${LIBSM}
LDADD=	${LIBSMUTIL} ${LIBSM}

SRCS+=	sm_os.h
CLEANFILES+=sm_os.h

# User customizations to the sendmail build environment
CFLAGS+= ${SENDMAIL_CFLAGS}
DPADD+=	${SENDMAIL_DPADD}
LDADD+=	${SENDMAIL_LDADD}
LDFLAGS+= ${SENDMAIL_LDFLAGS}

sm_os.h:
	ln -sf ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h sm_os.h

.include <bsd.prog.mk>
