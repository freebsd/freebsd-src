# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $Id: Makefile,v 1.14 1995/06/11 19:32:12 rgrimes Exp $

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pw_copy.c pw_util.c
LDADD=	-lcrypt -lrpcsvc
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin

CFLAGS+=-DCRYPT -DYP -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass

BINOWN=	root
BINMODE=4555
MAN1=passwd.1
LINKS=${DESTDIR}${BINDIR}/passwd ${DESTDIR}${BINDIR}/yppasswd
MLINKS=passwd.1 yppasswd.1

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_EBONES))
SRCS+=	kpasswd.c
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/rlogin ${.CURDIR}/../../usr.bin/passwd \
	${.CURDIR}/../../eBones/passwd

CFLAGS+=	-DKERBEROS \
		-I${.CURDIR}/../../eBones/include \
		-I${.CURDIR}/../../eBones/libkadm
LDADD=	-lkadm -lkrb -ldes -lcrypt -lrpcsvc -lcom_err
DISTRIBUTION=	krb
.endif

beforeinstall:
	[ ! -e ${DESTDIR}${BINDIR}/passwd ] || \
		chflags noschg ${DESTDIR}${BINDIR}/passwd
afterinstall:
	chflags schg ${DESTDIR}${BINDIR}/passwd

.include <bsd.prog.mk>
