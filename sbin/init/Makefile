#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	init
MAN=	init.8
MLINKS=	init.8 securelevel.8
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=crypto
DPADD=	${LIBUTIL} ${DESCRYPTOBJDIR}/libdescrypt.a
LDADD=	-lutil -L${DESCRYPTOBJDIR} -ldescrypt
.else
DPADD=	${LIBUTIL} ${SCRYPTOBJDIR}/libscrypt.a
LDADD=	-lutil -L${SCRYPTOBJDIR} -lscrypt
.endif

.if exists(${.OBJDIR}/../../lib/libcrypt)
SCRYPTOBJDIR=	${.OBJDIR}/../../lib/libcrypt
.else
SCRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if exists (${.CURDIR}/../../secure)

.if exists(${.OBJDIR}/../../secure/lib/libcrypt)
DESCRYPTOBJDIR=	${.OBJDIR}/../../secure/lib/libcrypt
.else
DESCRYPTOBJDIR=	${.CURDIR}/../../secure/lib/libcrypt
.endif

.endif

beforeinstall:
.if exists(${DESTDIR}${BINDIR}/${PROG})
	-chflags noschg ${DESTDIR}${BINDIR}/${PROG}
	mv ${DESTDIR}${BINDIR}/${PROG} ${DESTDIR}${BINDIR}/${PROG}.bak
.endif

.include <bsd.prog.mk>
