#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/hack/Makefile,v 1.20 1999/12/10 21:13:37 billf Exp $

PROG=	hack
SRCS=	alloc.c hack.Decl.c hack.apply.c hack.bones.c hack.c hack.cmd.c \
	hack.do.c hack.do_name.c hack.do_wear.c hack.dog.c hack.eat.c \
	hack.end.c hack.engrave.c hack.fight.c hack.invent.c hack.ioctl.c \
	hack.lev.c hack.main.c hack.makemon.c hack.mhitu.c hack.mklev.c \
	hack.mkmaze.c hack.mkobj.c hack.mkshop.c hack.mon.c hack.monst.c \
	hack.o_init.c hack.objnam.c hack.options.c hack.pager.c hack.potion.c \
	hack.pri.c hack.read.c hack.rip.c hack.rumors.c hack.save.c \
	hack.search.c hack.shk.c hack.shknam.c hack.steal.c hack.termcap.c \
	hack.timeout.c hack.topl.c hack.track.c hack.trap.c hack.tty.c \
	hack.u_init.c hack.unix.c hack.vault.c hack.version.c hack.wield.c \
	hack.wizard.c hack.worm.c hack.worn.c hack.zap.c rnd.c \
	hack.onames.h
MAN6=	hack.6
DPADD=	${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-ltermcap -lcompat
CFLAGS+= -fwritable-strings -I${.CURDIR} -I.
HIDEGAME=hidegame
CLEANFILES=hack.onames.h makedefs makedefs.o

build-tools: makedefs

hack.onames.h: makedefs def.objects.h
	./makedefs ${.CURDIR}/def.objects.h > hack.onames.h

makedefs: makedefs.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} /dev/null \
	    ${DESTDIR}/var/games/hackdir/perm
.if !exists(${DESTDIR}/var/games/hackdir/record)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 460 /dev/null \
	    ${DESTDIR}/var/games/hackdir/record
.endif
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 440 ${.CURDIR}/rumors \
	    ${DESTDIR}/var/games/hackdir/rumors
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/help  ${.CURDIR}/hh ${.CURDIR}/data \
	    ${DESTDIR}/var/games/hackdir
#	rm -f ${DESTDIR}/var/games/hackdir/bones* \
#	    ${DESTDIR}/var/games/hackdir/save/*

.include <bsd.prog.mk>
