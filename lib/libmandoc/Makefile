# $FreeBSD$

MDOCMLDIR=	${.CURDIR}/../../contrib/mdocml
.PATH: ${MDOCMLDIR}

LIB=	mandoc
#NO_PIC=
INTERNALLIB=
MAN=	mandoc.3 mandoc_headers.3
LIBMAN_SRCS=	man.c \
		man_hash.c \
		man_macro.c \
		man_validate.c

LIBMDOC_SRCS=	att.c \
		lib.c \
		mdoc.c \
		mdoc_argv.c \
		mdoc_hash.c \
		mdoc_macro.c \
		mdoc_validate.c \
		st.c

LIBROFF_SRCS=	eqn.c \
		roff.c \
		tbl.c \
		tbl_data.c \
		tbl_layout.c \
		tbl_opts.c

SRCS=		${LIBMAN_SRCS} \
		${LIBMDOC_SRCS} \
		${LIBROFF_SRCS} \
		chars.c \
		mandoc.c \
		mandoc_aux.c \
		msec.c \
		preconv.c \
		read.c

WARNS?=	3
CFLAGS+= -DHAVE_CONFIG_H

.include <bsd.lib.mk>
