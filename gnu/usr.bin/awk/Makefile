# Makefile copyright James Raynard (jraynard@freebsd.org) 1997
#
# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../../../contrib/awk

PROG=	gawk
SRCS=	array.c awkgram.y builtin.c eval.c field.c gawkmisc.c io.c main.c \
	msg.c node.c re.c version.c dfa.c getopt.c getopt1.c ext.c profile.c
MAN=	doc/gawk.1

YFLAGS=

# Important for ctype macros!
CFLAGS+=        -funsigned-char

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../../contrib/awk
CFLAGS+=	-DHAVE_CONFIG_H -DGAWK -DDEFPATH=\".\"
WARNS=	0

SUBDIR+= doc

DPADD+= ${LIBGNUREGEX} ${LIBM}
LDADD+= -lgnuregex -lm

.include <bsd.prog.mk>
