SHLIB_NAME=	yaml.so

WARNS=	1
LYAMLSRC?=	${SRCTOP}/contrib/lyaml
.PATH:		${LYAMLSRC}/ext/yaml ${LYAMLSRC}/lib/lyaml
SRCS=		emitter.c \
		parser.c \
		scanner.c \
		yaml.c
CFLAGS+=	\
		-I${LYAMLSRC}/ext/yaml \
		-I${SRCTOP}/contrib/libyaml/include \
		-DVERSION=\"6.2.8\"
LIBADD+=	yaml

FILES=		explicit.lua \
		functional.lua \
		implicit.lua \
		init.lua
FILESDIR=	${SHAREDIR}/flua/lyaml

.include <bsd.lib.mk>
