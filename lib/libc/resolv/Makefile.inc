# $FreeBSD$

# resolv sources
.PATH: ${.CURDIR}/resolv

SRCS+=	herror.c h_errno.c mtctxres.c res_comp.c res_data.c res_debug.c \
	res_init.c res_mkquery.c res_query.c res_send.c res_state.c
