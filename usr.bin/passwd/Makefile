#	@(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	passwd
SRCS=	des_rw.c krb_passwd.c local_passwd.c passwd.c pw_copy.c pw_util.c
DPADD=	${LIBKRB} ${LIBDES}
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin
CFLAGS+=-DKERBEROS -DCRYPT -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass
LDADD=	-lkrb -ldes
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
