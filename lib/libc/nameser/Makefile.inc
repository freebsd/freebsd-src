# $FreeBSD: src/lib/libc/nameser/Makefile.inc,v 1.2.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

# nameser sources
.PATH: ${.CURDIR}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c ns_samedomain.c ns_ttl.c
