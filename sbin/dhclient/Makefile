# ex:ts=8
#
# @(#)Makefile	1.0 (obrien) 2/6/99
# $Id$
#
# Copyright (c) 1999 by David O'Brien
# This file is under a "FreeBSD" copyright.  See /usr/src/sys/sys/copyright.h
# for the terms of the copyright.
#

.PATH:		${.CURDIR}/../../contrib/isc-dhcp/client ${.CURDIR}/../../contrib/isc-dhcp/common

PROG=		dhclient
MAN5=		dhclient.conf.5 dhclient.leases.5 dhcp-options.5
MAN8=		dhclient.8 dhclient-script.8

SRCS=		dhclient.c clparse.c
SRCS+=		raw.c parse.c nit.c icmp.c dispatch.c conflex.c upf.c bpf.c \
		socket.c packet.c memory.c print.c options.c inet.c convert.c \
		tree.c tables.c hash.c alloc.c errwarn.c inet_addr.c

CFLAGS+= 	-I${.CURDIR}/../../contrib/isc-dhcp/includes \
		-I${.CURDIR}/../../contrib/isc-dhcp

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/../../contrib/isc-dhcp/client/scripts/freebsd \
		${BINDIR}/dhclient-script

.include <bsd.prog.mk>
