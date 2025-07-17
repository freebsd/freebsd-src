LIBYAML_DIR=	${SRCTOP}/contrib/libyaml

PACKAGE=lib${LIB}
LIB=		yaml
PRIVATELIB=	true
SHLIB_MAJOR=	1
SRCS=		api.c \
		dumper.c \
		emitter.c \
		loader.c \
		parser.c \
		reader.c \
		scanner.c \
		writer.c

.PATH:		${LIBYAML_DIR}/src \
		${LIBYAML_DIR}/include

INCS=		yaml.h

WARNS?=		1
CFLAGS+=	-I${LIBYAML_DIR}/include \
		-I${LIBYAML_DIR}/src \
		-DYAML_VERSION_STRING=\"0.2.5\" \
		-DYAML_VERSION_MAJOR=0 \
		-DYAML_VERSION_MINOR=2 \
		-DYAML_VERSION_PATCH=5

.include <bsd.lib.mk>
