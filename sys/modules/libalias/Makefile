# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet/libalias

KMOD=	libalias
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_pptp.c alias_proxy.c alias_skinny.c alias_smedia.c \
	alias_util.c alias_old.c
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
