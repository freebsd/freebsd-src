#	@(#)Makefile	8.2 (Berkeley) 4/2/94
# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../usr.sbin/pwd_mkdb ${.CURDIR}/../../lib/libc/gen

PROG=	chpass
SRCS=	chpass.c edit.c field.c pw_scan.c table.c util.c
BINOWN=	root
BINMODE=4555
WARNS?=	5
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
#Some people need this, uncomment to activate
#CFLAGS+=-DRESTRICT_FULLNAME_CHANGE
CFLAGS+=-I${.CURDIR}/../../usr.sbin/pwd_mkdb -I${.CURDIR}/../../lib/libc/gen -I.

DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil
.if ${MK_NIS} != "no"
DPADD+=	${LIBYPCLNT}
LDADD+=	-lypclnt
.endif 

LINKS=	${BINDIR}/chpass ${BINDIR}/chfn
LINKS+=	${BINDIR}/chpass ${BINDIR}/chsh
.if ${MK_NIS} != "no"
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchpass
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchfn
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchsh
.endif

MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1
.if ${MK_NIS} != "no"
MLINKS+= chpass.1 ypchpass.1 chpass.1 ypchfn.1 chpass.1 ypchsh.1
.endif

beforeinstall:
.for i in chpass chfn chsh ypchpass ypchfn ypchsh
	-chflags noschg ${DESTDIR}${BINDIR}/$i
.endfor

.if !defined(NO_FSCHG)
afterinstall:
	-chflags schg ${DESTDIR}${BINDIR}/chpass
.endif

.include <bsd.prog.mk>
