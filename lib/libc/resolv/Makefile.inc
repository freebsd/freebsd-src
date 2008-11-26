# $FreeBSD: src/lib/libc/resolv/Makefile.inc,v 1.3.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

# resolv sources
.PATH: ${.CURDIR}/resolv

SRCS+=	herror.c h_errno.c mtctxres.c res_comp.c res_data.c res_debug.c \
	res_init.c res_mkquery.c res_query.c res_send.c res_state.c
