# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $Id: Makefile,v 1.18 1995/09/14 21:02:16 gibbs Exp $

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pw_copy.c pw_util.c pw_yp.c
DPADD=	${LIBCRYPT} ${LIBRPCSVC}
LDADD=	-lcrypt -lrpcsvc
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin

CFLAGS+=-DCRYPT -DYP -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass

BINOWN=	root
BINMODE=4555
MAN1=passwd.1
LINKS=${BINDIR}/passwd ${BINDIR}/yppasswd
MLINKS=passwd.1 yppasswd.1

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_EBONES))
SRCS+=	kpasswd.c
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/rlogin ${.CURDIR}/../../usr.bin/passwd \
	${.CURDIR}/../../eBones/usr.bin/passwd

CFLAGS+=	-DKERBEROS \
		-I${.CURDIR}/../../eBones/include \
		-I${.CURDIR}/../../eBones/lib/libkadm
# XXX not defined: ${LIBKADM}, ${LIBCOM_ERR}
DPADD=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCRYPT} ${LIBRPCSVC} ${LIBCOM_ERR}
LDADD=	-lkadm -lkrb -ldes -lcrypt -lrpcsvc -lcom_err
DISTRIBUTION=	krb
.endif

beforeinstall:
	[ ! -e ${DESTDIR}${BINDIR}/passwd ] || \
		chflags noschg ${DESTDIR}${BINDIR}/passwd

afterinstall:
	chflags schg ${DESTDIR}${BINDIR}/passwd

.include <bsd.prog.mk>
