# $FreeBSD$

.include <bsd.own.mk>

STFSUITEDIR=${TESTSBASE}/sys/cddl/zfs

MAN=
FILESDIR=	${TESTSBASE}/sys/cddl/zfs/include
FILES+=		libremote.kshlib
FILES+=		libsas.kshlib
FILES+=		logapi.kshlib
FILES+=		libtest.kshlib
FILES+=		stf.shlib

FILES+=		commands.cfg
CLEANFILES+=	commands.cfg
commands.cfg:	translatecommands.awk commands.txt
	awk -v stfsuitedir=${STFSUITEDIR} -f ${.ALLSRC} > ${.TARGET}

FILES+=		default.cfg
CLEANFILES+=	default.cfg
default.cfg:	default.cfg.in
	sed "s:%%STFSUITEDIR%%:${STFSUITEDIR}:" ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
