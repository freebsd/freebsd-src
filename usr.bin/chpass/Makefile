#	@(#)Makefile	8.2 (Berkeley) 4/2/94

PROG=	chpass
SRCS=	chpass.c edit.c field.c pw_copy.c pw_scan.c pw_util.c table.c util.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../usr.sbin/pwd_mkdb ${.CURDIR}/../../usr.sbin/vipw
CFLAGS+=-I${.CURDIR}/../../usr.sbin/pwd_mkdb -I${.CURDIR}/../../usr.sbin/vipw
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn
LINKS+=	${BINDIR}/chpass ${BINDIR}/chsh
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchpass
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchfn
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1
COPTS+=	-DYP
SRCS+=	pw_yp.c
DPADD=	${LIBRPCSVC} ${LIBCRYPT}
LDADD+=	-lrpcsvc -lcrypt

beforeinstall:
	[ ! -e ${DESTDIR}${BINDIR}/chpass ] || \
		chflags noschg ${DESTDIR}${BINDIR}/chpass

afterinstall:
	chflags schg ${DESTDIR}${BINDIR}/chpass

.include <bsd.prog.mk>
