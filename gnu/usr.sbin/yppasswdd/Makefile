# $Id: Makefile,v 1.3 1995/02/03 03:44:41 wpaul Exp $
# @(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	yppasswdd
MAN8=	yppasswdd.8

SRCS=	yppasswdd.c yppasswd_xdr.c update.c pw_copy.c pw_util.c

LDADD=	-lcrypt
CFLAGS+=-DCRYPT -I${.CURDIR} -I${.CURDIR}/../../../usr.sbin/vipw \
	-I${.CURDIR}/../../../usr.bin/chpass
CFLAGS+=-DVERSION=\"0.5\" -DYPLIBDIR=\"/usr/libexec\" -D_GNU_SOURCE

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/yppwupdate \
		${DESTDIR}/usr/libexec/yppwupdate

.include <bsd.prog.mk>
