#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#
# We do still make this one conditional on kerberos, in case 
# the libraries do not exist

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_KERBEROS) \
	|| defined(MAKE_EBONES))

PROG=	kpasswdd
SRCS=	kpasswdd.c
CFLAGS+=-DCRYPT -DKERBEROS -I${.CURDIR}/../../usr.bin/passwd
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
.PATH:	${.CURDIR}/../../usr.bin/rlogin
MAN8=	kpasswdd.8

.endif

.include <bsd.prog.mk>
