
PACKAGE=	lib${LIB}
LIB=		ifconfig
INTERNALLIB=	true

LIBADD=		m

SHLIBDIR?=	/lib
SHLIB_MAJOR=	2

VERSION_DEF=	${LIBCSRCDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

SRCS=		libifconfig.c \
		libifconfig_bridge.c \
		libifconfig_carp.c \
		libifconfig_inet.c \
		libifconfig_inet6.c \
		libifconfig_internal.c \
		libifconfig_lagg.c \
		libifconfig_media.c \
		libifconfig_sfp.c

GEN=		libifconfig_sfp_tables.h \
		libifconfig_sfp_tables.c \
		libifconfig_sfp_tables_internal.h

SRCS+=	${GEN}

.include <src.lua.mk>

.SUFFIXES: .tpl.c .tpl.h
.tpl.c.c .tpl.h.h: sfp.lua
	${LUA} ${.CURDIR}/sfp.lua ${.IMPSRC} >${.TARGET}

CLEANFILES+= ${GEN}

# If libifconfig become public uncomment those two lines
#INCSDIR=	${INCLUDEDIR}
#INCS=		libifconfig.h libifconfig_sfp.h libifconfig_sfp_tables.h

#MAN=		libifconfig.3

CFLAGS+= -I${.CURDIR} -I${.OBJDIR}
NO_WCAST_ALIGN= yes

.include <bsd.lib.mk>
