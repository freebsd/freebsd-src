#
# $FreeBSD: src/tools/regression/acct/Makefile,v 1.1.10.2.2.1 2012/03/03 06:15:13 kensmith Exp $
#
# "make" will compile the acct test programs
#

PROG=	pack
SRCS=	pack.c
NO_MAN=
CFLAGS+= -I${.OBJDIR}

pack.o:	convert.c

CLEANFILES+= convert.c
convert.c: ../../../sys/kern/kern_acct.c
	sed -n '/FLOAT_CONVERSION_START/,/FLOAT_CONVERSION_END/p' $? | \
	sed -e 's/log(/syslog(/' >$@

regress: pack regress.t
	sh ${.CURDIR}/regress.t

.include <bsd.prog.mk>
