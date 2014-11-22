# $FreeBSD$

MDOCMLDIR=	${.CURDIR}/../../contrib/mdocml
.PATH: ${MDOCMLDIR}

LIB=	mandoc
#NO_PIC=
INTERNALLIB=
MAN=	mandoc.3
SRCS=	mdoc_argv.c mdoc_hash.c mdoc_macro.c mdoc_validate.c \
	mdoc.c arch.c att.c lib.c st.c vol.c \
	man.c man_hash.c man_macro.c man_validate.c \
	roff.c eqn.c tbl.c tbl_data.c tbl_layout.c tbl_opts.c \
	mandoc.c mandoc_aux.c chars.c msec.c read.c

WARNS?=	3
CFLAGS+= -DHAVE_CONFIG_H

.include <bsd.lib.mk>
