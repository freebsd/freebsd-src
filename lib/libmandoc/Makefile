# $FreeBSD$

MDOCMLDIR=	${.CURDIR}/../../contrib/mdocml
.PATH: ${MDOCMLDIR}

LIB=	mandoc
#NO_PIC=
INTERNALLIB=
MAN=	mandoc.3
SRCS=	arch.c att.c chars.c \
	compat_fgetln.c compat_getsubopt.c compat_strlcat.c compat_strlcpy.c \
	eqn.c lib.c man.c man_hash.c man_macro.c man_validate.c mandoc.c \
	mdoc.c mdoc_argv.c mdoc_hash.c mdoc_macro.c mdoc_validate.c \
	msec.c read.c roff.c st.c \
	tbl.c tbl_data.c tbl_layout.c tbl_opts.c vol.c

WARNS?=	3
CFLAGS+= -DHAVE_CONFIG_H -DVERSION="\"1.12.1\""

.include <bsd.lib.mk>
