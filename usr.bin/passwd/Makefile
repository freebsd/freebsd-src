# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $Id: Makefile,v 1.12 1995/02/14 21:08:45 rgrimes Exp $

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pw_copy.c pw_util.c
LDADD=	-lcrypt -lrpcsvc
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin

CFLAGS+=-DCRYPT -DYP -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
MAN1=passwd.1 yppasswd.1
MLINKS=yppasswd.1 ypchsh.1 yppasswd.1 ypchpass.1 yppasswd.1 ypchfn.1

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_EBONES))
SRCS+=	kpasswd.c
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/rlogin ${.CURDIR}/../../usr.bin/passwd \
	${.CURDIR}/../../eBones/passwd

CFLAGS+=	-DKERBEROS \
		-I${.CURDIR}/../../eBones/include \
		-I${.CURDIR}/../../eBones/libkadm
LDADD=	-lkadm -lkrb -ldes -lcrypt -lrpcsvc -lcom_err
.endif

afterinstall:
	ln -sf passwd ${DESTDIR}${BINDIR}/yppasswd
	ln -sf passwd ${DESTDIR}${BINDIR}/ypchsh
	ln -sf passwd ${DESTDIR}${BINDIR}/ypchfn
	ln -sf passwd ${DESTDIR}${BINDIR}/ypchpass

.include <bsd.prog.mk>
