#	@(#)Makefile	5.5 (Berkeley) 2/19/91

PROG=	chpass
SRCS=   chpass.c edit.c field.c pw_copy.c pw_scan.c pw_util.c table.c util.c \
	pw_fastmkdb.c
BINOWN=	root
BINMODE=4555
.PATH:  ${.CURDIR}/../../usr.sbin/pwd_mkdb ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/passwd
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1

.if defined (PW_COMPACT)
CFLAGS+=-DPW_COMPACT
.endif

.include <bsd.prog.mk>
