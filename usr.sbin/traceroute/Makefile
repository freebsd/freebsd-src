#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	traceroute
MAN8=	traceroute.8
BINOWN=	root
BINMODE=4555
CFLAGS+=-DHAVE_BZERO=1 -DHAVE_SETLINEBUF=1

# RTT Jitter on the internet these days means printing 3 decimal places on
# > 1000ms times is plain useless.  Uncomment this to enable variable precision
# reporting, ie: print a variable precision from 0.001ms through 1000ms
# CFLAGS+=-DSANE_PRECISION

.include <bsd.prog.mk>
