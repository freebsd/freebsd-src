# Makefile copyright James Raynard (jraynard@freebsd.org) 1997
#
# $Id: Makefile,v 1.10 1997/10/26 12:53:16 jraynard Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../../../contrib/awk

PROG=	awk
SRCS=	array.c awktab.y builtin.c eval.c field.c gawkmisc.c io.c main.c \
	msg.c node.c re.c version.c dfa.c getopt.c getopt1.c
MAN1=	doc/awk.1

YACC = bison -y
YFLAGS=

# Important for ctype macros!
CFLAGS+=        -funsigned-char

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../../contrib/awk
CFLAGS+=	-DHAVE_CONFIG_H -DGAWK -DDEFPATH=\".\"
CLEANFILES=	awktab.y

SUBDIR+= doc

DPADD+= ${LIBGNUREGEX} ${LIBM}
LDADD+= -lgnuregex -lm

awktab.y: awk.y
	ln -fs ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
