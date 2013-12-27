# $FreeBSD$

MDOCMLDIR=	${.CURDIR}/../../contrib/mdocml
LIBMANDOC=	${.OBJDIR}/../../lib/libmandoc/libmandoc.a
.PATH: ${MDOCMLDIR}

PROG=	mandoc
FILES=	example.style.css external.png style.css
FILESDIR=	${SHAREDIR}/mdocml
LINKS=	mdocml
MAN=	mandoc.1 eqn.7 mandoc_char.7 tbl.7 # man.7 mdoc.7 roff.7
MLINKS= mandoc.1 mdocml.1
SRCS=	eqn_html.c eqn_term.c html.c main.c man_html.c man_term.c mdoc_html.c \
	mdoc_man.c mdoc_term.c out.c tbl_html.c tbl_term.c term.c \
	term_ascii.c term_ps.c tree.c

WARNS?=	3
CFLAGS+= -DHAVE_CONFIG_H -DVERSION="\"1.12.1\""
DPADD=	${LIBMANDOC}
LDADD=	${LIBMANDOC}

.include <bsd.prog.mk>
