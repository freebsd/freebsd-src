# $FreeBSD$

.include <src.opts.mk>

MDOCMLDIR=	${.CURDIR}/../../contrib/mdocml
.PATH: ${MDOCMLDIR}

PROG=	mandoc
MAN=	mandoc.1 eqn.7 mandoc_char.7 tbl.7 man.7 mdoc.7 # roff.7
MLINKS=	mandoc.1 mdocml.1
.if ${MK_MANDOCDB} != no && ${MK_MAN_UTILS} != no
MAN+=	apropos.1 makewhatis.8
MLINKS+=	apropos.1 whatis.1
LINKS=	${BINDIR}/mandoc ${BINDIR}/whatis \
	${BINDIR}/mandoc ${BINDIR}/makewhatis \
	${BINDIR}/mandoc ${BINDIR}/apropos
.endif

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
		mdoc_state.c \
		mdoc_validate.c \
		st.c \

LIBROFF_SRCS=	eqn.c \
		roff.c \
		tbl.c \
		tbl_data.c \
		tbl_layout.c \
		tbl_opts.c \

LIB_SRCS=	${LIBMAN_SRCS} \
		${LIBMDOC_SRCS} \
		${LIBROFF_SRCS} \
		chars.c \
		mandoc.c \
		mandoc_aux.c \
		mandoc_ohash.c \
		msec.c \
		preconv.c \
		read.c

HTML_SRCS=	eqn_html.c \
		html.c \
		man_html.c \
		mdoc_html.c \
		tbl_html.c

MAN_SRCS=	mdoc_man.c

TERM_SRCS=	eqn_term.c \
		man_term.c \
		mdoc_term.c \
		term.c \
		term_ascii.c \
		term_ps.c \
		tbl_term.c

DB_SRCS=	mandocdb.c \
		mansearch.c \
		mansearch_const.c \
		tag.c \
		manpath.c

SRCS=		${LIB_SRCS} \
		${HTML_SRCS} \
		${MAN_SRCS} \
		${TERM_SRCS} \
		main.c \
		out.c \
		tree.c

SRCS+=	${DB_SRCS}

WARNS?=	2
CFLAGS+= -DHAVE_CONFIG_H \
	 -D_WITH_GETLINE \
	 -I${.CURDIR}/../../lib/libopenbsd/ \
	 -I${.CURDIR}/../../contrib/sqlite3
LIBADD=	openbsd sqlite3 z

.include <bsd.prog.mk>
