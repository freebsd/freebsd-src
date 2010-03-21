# $FreeBSD: src/lib/libc/resolv/Makefile.inc,v 1.3.14.1 2010/02/10 00:26:20 kensmith Exp $

# resolv sources
.PATH: ${.CURDIR}/resolv

SRCS+=	herror.c h_errno.c mtctxres.c res_comp.c res_data.c res_debug.c \
	res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c \
	res_query.c res_send.c res_state.c res_update.c

SYM_MAPS+= ${.CURDIR}/resolv/Symbol.map
