#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.17 1999/07/01 13:33:56 ru Exp $

PROG=	init
MAN8=	init.8
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
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
