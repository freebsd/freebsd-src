# $FreeBSD$

MDOCMLDIR=	${.CURDIR}/../../contrib/mdocml
LIBMANDOC=	${.OBJDIR}/../../lib/libmandoc/libmandoc.a
.PATH: ${MDOCMLDIR}

PROG=	mandoc
FILES=	example.style.css style.css
FILESDIR=	${SHAREDIR}/mdocml
LINKS=	mdocml
MAN=	mandoc.1 eqn.7 mandoc_char.7 tbl.7 # man.7 mdoc.7 roff.7
MLINKS= mandoc.1 mdocml.1
SRCS=	main.c mdoc_term.c chars.c term.c tree.c man_term.c eqn_term.c \
	mdoc_man.c
SRCS+=	html.c mdoc_html.c man_html.c out.c eqn_html.c
SRCS+=	term_ps.c term_ascii.c tbl_term.c tbl_html.c
SRCS+=	manpath.c compat_reallocarray.c

WARNS?=	3
CFLAGS+= -DHAVE_CONFIG_H
DPADD=	${LIBMANDOC}
LDADD=	${LIBMANDOC}

.include <bsd.prog.mk>
