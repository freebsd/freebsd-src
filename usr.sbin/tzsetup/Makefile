#	Makefile,v 1.2 1995/04/24 21:04:30 wollman Exp

PROG=	tzsetup
NOMAN=
SRCS=	main.c tzmenu.c menus.c
CFLAGS+=	-I. -I${.CURDIR}
CLEANFILES+=	menus.c
LDADD+=	-ldialog -lncurses -lmytinfo
DPADD+=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

Z=	${.CURDIR}/../../share/zoneinfo
.PATH:	${Z}

ZF=	africa antarctica asia australasia europe northamerica southamerica

menus.c:	grok.pl ${ZF}
	perl ${.CURDIR}/grok.pl `for a in ${ZF}; do echo ${Z}/$$a; done` \
		> ${.TARGET}

.include <bsd.prog.mk>
