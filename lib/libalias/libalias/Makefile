# $FreeBSD$

.PATH: ${SRCTOP}/sys/netinet/libalias

PACKAGE=	natd
CONFS=	libalias.conf
LIB=	alias
SHLIBDIR?= /lib
SHLIB_MAJOR= 7
MAN=	libalias.3
SRCS=	alias.c alias_db.c alias_proxy.c alias_util.c alias_mod.c
INCS=	alias.h
MK_WERROR=	no

.include <bsd.lib.mk>
