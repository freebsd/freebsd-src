# $FreeBSD$

PROG	=	isdnd

SRCS	=	rc_parse.y rc_scan.l main.c rc_config.c log.c curses.c	\
		process.c rates.c msghdl.c fsm.c support.c timer.c	\
		exec.c dial.c monitor.c pcause.c controller.c alias.c	\
		y.tab.h

COPTS	+=	-I${.CURDIR}/../isdnmonitor -I${.CURDIR}/../isdntel -I${.OBJDIR}

# compile debug support
COPTS	+=	-DDEBUG

# enable rtprio usage
COPTS	+=	-DUSE_RTPRIO

MAN8	 =	isdnd.8
MAN5	 =	isdnd.rc.5 isdnd.rates.5 isdnd.acct.5

.if !defined(I4B_WITHOUT_CURSES)
COPTS	+=	-DUSE_CURSES
DPADD	 =	${LIBCURSES}
LDADD	 =	-lcurses
.endif

.if defined(I4B_EXTERNAL_MONITOR)
COPTS	+=	-DI4B_EXTERNAL_MONITOR
.if defined(I4B_NOTCPIP_MONITOR)
COPTS	+=	-DI4B_NOTCPIP_MONITOR
.endif
.endif

.include <bsd.prog.mk>
