# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../usr.bin/lastcomm

PROG=	sa
MAN=	sa.8
SRCS=	main.c db.c pdb.c usrdb.c readrec.c

PACKAGE=acct

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
