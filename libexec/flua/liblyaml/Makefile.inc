WARNS=	1

LYAMLSRC?=	${SRCTOP}/contrib/lyaml
.PATH:		${LYAMLSRC}/ext/yaml ${LYAMLSRC}/lib/lyaml
SRCS+=		emitter.c \
		parser.c \
		scanner.c \
		yaml.c
CFLAGS+=	\
		-I${LYAMLSRC}/ext/yaml \
		-I${SRCTOP}/contrib/libyaml/include \
		-DVERSION=\"6.2.8\"
LIBADD+=	yaml

FILESGROUPS+=	YAML
YAML=		explicit.lua \
		functional.lua \
		implicit.lua \
		init.lua
YAMLDIR=	${SHAREDIR}/flua/lyaml
