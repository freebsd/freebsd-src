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
FILES+=		stf.kshlib

FILES+=		commands.cfg
commands.cfg:	translatecommands.awk commands.txt
	awk -v stfsuitedir=${STFSUITEDIR} -f ${.ALLSRC} > ${.TARGET}
CLEANFILES+=	commands.cfg

FILES+=		default.cfg
default.cfg:	default.cfg.in
	sed "s:%%STFSUITEDIR%%:${STFSUITEDIR}:" ${.ALLSRC} > ${.TARGET}
CLEANFILES+=	default.cfg

.include <bsd.prog.mk>
