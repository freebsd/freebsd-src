# $FreeBSD: src/usr.bin/rpcgen/Makefile,v 1.5.6.1 2001/04/25 11:29:34 ru Exp $

PROG=	rpcgen
SRCS=	rpc_main.c  rpc_clntout.c rpc_cout.c rpc_hout.c rpc_parse.c \
	rpc_sample.c rpc_scan.c rpc_svcout.c rpc_tblout.c rpc_util.c

#
# This is a kludge to work around the fact that this program
# uses 'inline' as a variable name.
#
CFLAGS+=-Dinline=rpcgen_inline

.include <bsd.prog.mk>
