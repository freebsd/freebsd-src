# $FreeBSD: src/lib/libc/nameser/Makefile.inc,v 1.2.12.1.8.1 2012/03/03 06:15:13 kensmith Exp $

# nameser sources
.PATH: ${.CURDIR}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c ns_samedomain.c ns_ttl.c

SYM_MAPS+= ${.CURDIR}/nameser/Symbol.map
