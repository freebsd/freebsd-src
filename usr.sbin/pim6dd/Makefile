#       @(#)Makefile    8.1 (Berkeley) 6/5/93
#       $FreeBSD$

PROG=   pim6dd
MAN5=	pim6dd.conf.5
MAN8=   pim6dd.8

SRCS=	mld6.c mld6_proto.c\
	inet6.c kern.c main.c config.c debug.c routesock.c vers.c callout.c\
	route.c vif.c timer.c mrt.c pim6.c pim6_proto.c trace.c

CFLAGS+= -DINET6 -DPIM -DIOCTL_OK_ON_RAW_SOCKET

.include <bsd.prog.mk>
