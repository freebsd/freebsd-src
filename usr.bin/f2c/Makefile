#	Makefile for f2c, a Fortran 77 to C converter

PROG=	f2c

g = -O -g
CFLAGS = $g -DANSI_Libraries -I${.CURDIR} -I.
SHELL = /bin/sh

SRCSd =	main.c init.c gram.c lex.c proc.c equiv.c data.c format.c \
	expr.c exec.c intr.c io.c misc.c error.c mem.c names.c \
	output.c p1output.c pread.c put.c putpcc.c vax.c formatdata.c \
	parse_args.c niceprintf.c cds.c sysdep.c version.c
SRCS =	$(SRCSd) malloc.c

GRAMFILES = ${.CURDIR}/gram.head ${.CURDIR}/gram.dcl ${.CURDIR}/gram.expr\
	${.CURDIR}/gram.exec ${.CURDIR}/gram.io

gram.c:	${GRAMFILES} ${.CURDIR}/defs.h tokdefs.h
	(sed < tokdefs.h "s/#define/%token/" ; \
	cat ${GRAMFILES}) > gram.in
	$(YACC) $(YFLAGS) gram.in
	echo "(expect 4 shift/reduce)"
	sed 's/^# line.*/\/* & *\//' y.tab.c >gram.c
	rm -f gram.in y.tab.c

tokdefs.h: ${.CURDIR}/tokens
	grep -n . <${.CURDIR}/tokens | sed "s/\([^:]*\):\(.*\)/#define \2 \1/" >tokdefs.h

CLEANFILES+=\
	gram.c tokdefs.h

.include <bsd.prog.mk>
