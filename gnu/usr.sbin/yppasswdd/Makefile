# $Id: Makefile,v 1.4 1995/02/15 04:35:55 wpaul Exp $
# @(#)Makefile	8.3 (Berkeley) 4/2/94

PROG=	yppasswdd
MAN8=	yppasswdd.8

SRCS=	yppasswdd.c update.c pw_copy.c pw_util.c

LDADD=	-lcrypt -lrpcsvc
CFLAGS+=-DCRYPT -I${.CURDIR} -I${.CURDIR}/../../../usr.sbin/vipw \
	-I${.CURDIR}/../../../usr.bin/chpass
CFLAGS+=-DVERSION=\"0.5\" -DYPLIBDIR=\"/usr/libexec\" -D_GNU_SOURCE

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/yppwupdate \
		${DESTDIR}/usr/libexec/yppwupdate

.include <bsd.prog.mk>
