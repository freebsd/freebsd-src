
.include <src.opts.mk>

PACKAGE=	runtime
PROG=	traceroute
MAN=	traceroute.8
SRCS=	as.c traceroute.c ifaddrlist.c findsaddr-udp.c
BINOWN=	root
BINMODE=4555

.if !defined(TRACEROUTE_NO_IPSEC)
CFLAGS+= -DIPSEC
.endif
# RTT Jitter on the internet these days means printing 3 decimal places on
# > 1000ms times is plain useless.  Uncomment this to enable variable precision
# reporting, ie: print a variable precision from 0.001ms through 1000ms
# CFLAGS+= -DSANE_PRECISION

.if !defined(TRACEROUTE_NO_IPSEC)
LIBADD+=	ipsec
.endif

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_dns
CFLAGS+=-DWITH_CASPER
.endif

WARNS?=	3

.include <bsd.prog.mk>
