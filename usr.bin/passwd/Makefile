# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $Id$

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c
LDADD=	-lcrypt
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin
CFLAGS+=-DCRYPT -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass


.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))
SRCS+=	krb_passwd.c
CFLAGS+=-DKERBEROS
LDADD+=-lkrb -ldes
DPADD=	${LIBKRB} ${LIBDES}
.endif


BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
