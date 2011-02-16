# $FreeBSD: src/lib/libc/nameser/Makefile.inc,v 1.2.12.1.6.1 2010/12/21 17:09:25 kensmith Exp $

# nameser sources
.PATH: ${.CURDIR}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c ns_samedomain.c ns_ttl.c

SYM_MAPS+= ${.CURDIR}/nameser/Symbol.map
