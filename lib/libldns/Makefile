# $FreeBSD$

# Vendor sources and generated files
LDNSDIR = ${.CURDIR}/../../contrib/ldns

.PATH: ${LDNSDIR} ${LDNSDIR}/compat

LIB = ldns
INTERNALLIB = true

CFLAGS += -I${LDNSDIR}

SRCS +=	buffer.c
SRCS += dane.c
SRCS +=	dname.c
SRCS +=	dnssec.c
SRCS +=	dnssec_sign.c
SRCS +=	dnssec_verify.c
SRCS +=	dnssec_zone.c
SRCS +=	duration.c
SRCS +=	error.c
SRCS +=	higher.c
SRCS +=	host2str.c
SRCS +=	host2wire.c
SRCS +=	keys.c
SRCS +=	net.c
SRCS +=	packet.c
SRCS +=	parse.c
SRCS +=	rbtree.c
SRCS +=	rdata.c
SRCS +=	resolver.c
SRCS +=	rr.c
SRCS +=	rr_functions.c
SRCS +=	sha1.c
SRCS +=	sha2.c
SRCS +=	str2host.c
SRCS +=	tsig.c
SRCS +=	update.c
SRCS +=	util.c
SRCS +=	wire2host.c
SRCS +=	zone.c

SRCS += b32_ntop.c
SRCS += b32_pton.c
SRCS += b64_ntop.c
SRCS += b64_pton.c

WARNS ?= 3

.include <bsd.lib.mk>
