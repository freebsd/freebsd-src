#	@(#)Makefile	5.6 (Berkeley) 6/24/90

PROG=	login
MAN5=	login.access.5
MAN1=	login.1
SRCS=	klogin.c login.c login_access.c login_skey.c 
DPADD=	${LIBUTIL} ${LIBSKEY}
LDADD=	-lutil -lskey
BINOWN=	root
BINMODE=4555
#CFLAGS+=-DLOGIN_ACCESS -DSKEY -DLOGALL -I${.CURDIR}/../../lib
CFLAGS+=-DLOGIN_ACCESS -DSKEY 

.if exists(${DESTDIR}/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif
.if exists(${DESTDIR}/usr/lib/libkrb.a)
DPADD+= ${LIBKRB} ${LIBDES}
LDADD+= -lkrb -ldes
CFLAGS+= -DKERBEROS
.endif

.include <bsd.prog.mk>
