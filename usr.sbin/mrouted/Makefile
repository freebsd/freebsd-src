#
# Makefile for mrouted, a multicast router, and its auxiliary programs,
# map-mbone and mrinfo.
#
# $Id: Makefile,v 1.3 1994/06/15 11:28:38 jkh Exp $
#
# Modified by: Jim Lowe for FreeBSD 5/17/94
#

IGMP_SRCS=	igmp.c inet.c kern.c
IGMP_OBJS=	igmp.o inet.o kern.o
ROUTER_SRCS=	config.c main.c route.c vif.c
ROUTER_OBJS=	config.o main.o route.o vif.o
MAPPER_SRCS=	mapper.c
MAPPER_OBJS=	mapper.o
MRINFO_SRCS=	mrinfo.c
MRINFO_OBJS=	mrinfo.o
HDRS=		defs.h dvmrp.h route.h vif.h
SRCS= ${IGMP_SRCS} ${ROUTER_SRCS} ${MAPPER_SRCS} ${MRINFO_SRCS}
OBJS= ${IGMP_OBJS} ${ROUTER_OBJS} ${MAPPER_OBJS} ${MRINFO_OBJS}
CLEANFILES+=mrouted map-mbone mrinfo

MAN8= mrouted.8

all: mrouted map-mbone mrinfo

mrouted: ${IGMP_OBJS} ${ROUTER_OBJS}
	rm -f $@
	${CC} ${LDFLAGS} -o $@ ${CFLAGS} ${IGMP_OBJS} ${ROUTER_OBJS}

map-mbone: ${IGMP_OBJS} ${MAPPER_OBJS}
	rm -f $@
	${CC} ${LDFLAGS} -o $@ ${CFLAGS} ${IGMP_OBJS} ${MAPPER_OBJS}

mrinfo: ${IGMP_OBJS} ${MRINFO_OBJS}
	rm -f $@
	${CC} ${LDFLAGS} -o $@ ${CFLAGS} ${IGMP_OBJS} ${MRINFO_OBJS}

install: mrouted mrinfo map-mbone
	install -c mrouted ${DESTDIR}${BINDIR}
	install -c -o root -m 500 mrinfo ${DESTDIR}${BINDIR}
	install -c -o root -m 500 map-mbone ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
