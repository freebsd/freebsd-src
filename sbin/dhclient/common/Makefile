# common
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/common

LIB=	dhcp
INTERNALLIB=		true

SRCS=	alloc.c bpf.c comapi.c conflex.c ctrace.c discover.c dispatch.c \
	dlpi.c dns.c ethernet.c execute.c fddi.c icmp.c inet.c lpf.c \
	memory.c nit.c options.c packet.c parse.c print.c raw.c resolv.c \
	socket.c tables.c tr.c tree.c upf.c

CFLAGS+= -I${DIST_DIR}

MAN=	dhcp-eval.5 dhcp-options.5

.include <bsd.lib.mk>
