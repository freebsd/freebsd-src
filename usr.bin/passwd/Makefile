# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $Id: Makefile,v 1.6 1995/01/19 21:03:48 wollman Exp $

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c yppasswd_xdr.c passwd.c pw_copy.c pw_util.c
LDADD=	-lcrypt
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../gnu/usr.sbin/yppasswdd

CFLAGS+=-DCRYPT -DYP -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass \
	-I${.CURDIR}/../../gnu/usr.sbin/yppasswdd

BINOWN=	root
BINMODE=4555
BINDIR= /usr/bin
INSTALLFLAGS=-fschg

afterinstall: symlinks

symlinks: passwd
	ln -sf ${BINDIR}/passwd ${BINDIR}/yppasswd
	ln -sf ${BINDIR}/passwd ${BINDIR}/ypchsh
	ln -sf ${BINDIR}/passwd ${BINDIR}/ypshfn
	ln -sf ${BINDIR}/passwd ${BINDIR}/ypchpass

.include <bsd.prog.mk>
