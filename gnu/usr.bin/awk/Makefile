# Makefile copyright James Raynard (jraynard@freebsd.org) 1997
#
# $Id: Makefile,v 1.11 1998/05/04 16:18:57 bde Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../../../contrib/awk

PROG=	awk
SRCS=	array.c awktab.y builtin.c eval.c field.c gawkmisc.c io.c main.c \
	msg.c node.c re.c version.c dfa.c getopt.c getopt1.c
MAN1=	doc/awk.1

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
