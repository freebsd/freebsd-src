# $FreeBSD: src/lib/libc/nameser/Makefile.inc,v 1.2.2.1 2006/07/17 10:09:56 ume Exp $

# nameser sources
.PATH: ${.CURDIR}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c ns_samedomain.c ns_ttl.c
