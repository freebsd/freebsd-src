#	@(#)Makefile	8.1 (Berkeley) 7/19/93
#	$Id: Makefile,v 1.15 1998/01/20 10:39:56 bde Exp $

PROG=	init
MAN8=	init.8
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP

# Uncomment the following line to activate System V compatibility mode
#CFLAGS+=-DCOMPAT_SYSV_INIT

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
DISTRIBUTION=des
DPADD=	${LIBUTIL} ${DESCRYPTOBJDIR}/libdescrypt.a
LDADD=	-lutil -L${DESCRYPTOBJDIR} -ldescrypt
.else
DPADD=	${LIBUTIL} ${SCRYPTOBJDIR}/libscrypt.a
LDADD=	-lutil -L${SCRYPTOBJDIR} -lscrypt
.endif

.include <bsd.prog.mk>

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
