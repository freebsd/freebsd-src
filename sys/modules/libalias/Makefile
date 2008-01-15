# $FreeBSD: src/sys/modules/libalias/Makefile,v 1.2 2005/06/20 08:33:29 glebius Exp $

.PATH: ${.CURDIR}/../../netinet/libalias

KMOD=	libalias
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_pptp.c alias_proxy.c alias_skinny.c alias_smedia.c \
	alias_util.c
INCS=	alias.h

EXPORT_SYMS=	LibAliasInit \
		LibAliasUninit \
		LibAliasSetAddress \
		LibAliasSetMode \
		LibAliasSkinnyPort \
		LibAliasIn \
		LibAliasOut \
		LibAliasRedirectPort \
		LibAliasRedirectAddr \
		LibAliasAddServer \
		LibAliasRedirectDynamic \
		LibAliasRedirectDelete \
		LibAliasProxyRule \
		LibAliasRedirectProto \
		LibAliasSaveFragment \
		LibAliasGetFragment \
		LibAliasFragmentIn \
		LibAliasSetTarget \
		LibAliasCheckNewLink \
		LibAliasInternetChecksum \
		LibAliasUnaliasOut

.include <bsd.kmod.mk>
