# common
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/common

LIB=	dhcp
NOPROFILE=		true
INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

SRCS=   raw.c parse.c nit.c icmp.c dispatch.c conflex.c upf.c bpf.c	\
	socket.c lpf.c dlpi.c packet.c tr.c ethernet.c memory.c print.c	\
	options.c inet.c tree.c tables.c alloc.c fddi.c ctrace.c dns.c	\
	resolv.c execute.c discover.c comapi.c

CFLAGS+= -I${DIST_DIR}/includes -I${DIST_DIR}

MAN=	dhcp-eval.5 dhcp-options.5

.include <bsd.lib.mk>
