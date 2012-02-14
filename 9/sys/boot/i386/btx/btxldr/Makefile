# $FreeBSD$

PROG=	btxldr
INTERNALPROG=
NO_MAN=
SRCS=	btxldr.S

CFLAGS+=-DLOADER_ADDRESS=${LOADER_ADDRESS}

.if defined(BTXLDR_VERBOSE)
CFLAGS+=-DBTXLDR_VERBOSE
.endif

.if ${CC:T:Mclang} == "clang"
# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS+=${.IMPSRC:T:Mbtxldr.S:C/^.+$/-no-integrated-as/}
.endif

LDFLAGS=-e start -Ttext ${LOADER_ADDRESS} -Wl,-N,-S,--oformat,binary

.include <bsd.prog.mk>
