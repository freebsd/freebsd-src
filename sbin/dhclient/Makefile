#	$Id: Makefile,v 1.3.2.1 1999/02/19 10:32:57 obrien Exp $

DIST_DIR= ${.CURDIR}/../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/client ${DIST_DIR}/common

PROG=	dhclient
SRCS=	clparse.c dhclient.c
SRCS+=	alloc.c bpf.c conflex.c convert.c dispatch.c errwarn.c hash.c \
	icmp.c inet.c inet_addr.c memory.c nit.c options.c packet.c \
	parse.c print.c raw.c socket.c tables.c tree.c upf.c

CFLAGS+= -I${DIST_DIR}/includes -I${DIST_DIR}

MAN5=	dhclient.conf.5 dhclient.leases.5 dhcp-options.5
MAN8=	dhclient.8 dhclient-script.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${DIST_DIR}/client/scripts/freebsd \
	    ${DESTDIR}${BINDIR}/dhclient-script

.include <bsd.prog.mk>
