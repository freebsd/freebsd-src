# Makefile copyright James Raynard (jraynard@freebsd.org) 1997
#
# $Id: Makefile,v 1.9 1997/10/26 12:20:45 ache Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../../../contrib/awk

PROG=	awk
SRCS=	array.c awktab.c builtin.c eval.c field.c gawkmisc.c io.c main.c \
	msg.c node.c re.c version.c dfa.c getopt.c getopt1.c
MAN1=	doc/awk.1

YACC = bison -y

# Important for ctype macros!
CFLAGS+=        -funsigned-char

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../../contrib/awk
CFLAGS+=	-DHAVE_CONFIG_H -DGAWK -DDEFPATH=\".\"
CLEANFILES	+=awktab.c y.output y.tab.h

SUBDIR+= doc

DPADD+= ${LIBGNUREGEX} ${LIBM}
LDADD+= -lgnuregex -lm


awktab.c:	${.CURDIR}/../../../contrib/awk/awk.y
	$(YACC) -v $> && mv y.tab.c awktab.c

.include <bsd.prog.mk>
