# $Id: Makefile,v 1.2 1995/02/01 02:13:06 wpaul Exp $
# @(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	yppasswdd
MAN8=	yppasswdd.8

SRCS=	yppasswdd.c yppasswd_xdr.c update.c pw_copy.c pw_util.c

LDADD=	-lcrypt
CFLAGS+=-DCRYPT -I${.CURDIR} -I${.CURDIR}/../../../usr.sbin/vipw \
	-I${.CURDIR}/../../../usr.bin/chpass
CFLAGS+=-DVERSION=\"0.5\" -DYPLIBDIR=\"/usr/libexec\" -D_GNU_SOURCE

afterinstall: /usr/libexec/yppwupdate

/usr/libexec/yppwupdate: ${.CURDIR}/yppwupdate
	install -c -o bin -g bin -m 555 ${.CURDIR}/yppwupdate /usr/libexec/yppwupdate

.include <bsd.prog.mk>
