# $FreeBSD$

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/vacation

PROG=	vacation
SRCS=	vacation.c
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+=-DNEWDB -DNOT_SENDMAIL
CFLAGS+=-D_FFR_LISTDB -D_FFR_DEBUG

WARNS?=	2

LIBSMDIR=	${.OBJDIR}/../../lib/libsm
LIBSM=		${LIBSMDIR}/libsm.a

LIBSMDBDIR=	${.OBJDIR}/../../lib/libsmdb
LIBSMDB=	${LIBSMDBDIR}/libsmdb.a

LIBSMUTILDIR=	${.OBJDIR}/../../lib/libsmutil
LIBSMUTIL=	${LIBSMUTILDIR}/libsmutil.a

DPADD=	${LIBSMDB} ${LIBSMUTIL} ${LIBSM}
LDADD=	${LIBSMDB} ${LIBSMUTIL} ${LIBSM}

SRCS+=	sm_os.h
CLEANFILES+=sm_os.h

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
DPADD+=${SENDMAIL_DPADD}
LDADD+=${SENDMAIL_LDADD}
LDFLAGS+=${SENDMAIL_LDFLAGS}

sm_os.h:
	ln -sf ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h sm_os.h

.include <bsd.prog.mk>
