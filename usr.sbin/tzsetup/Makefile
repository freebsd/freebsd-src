#	$Id: Makefile,v 1.3 1996/01/24 00:11:57 wosch Exp $

PROG=	tzsetup
MAN8=	tzsetup.8
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
