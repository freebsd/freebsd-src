# $FreeBSD: src/usr.bin/at/Makefile,v 1.10 1999/08/28 00:59:08 peter Exp $

.include "${.CURDIR}/Makefile.inc"

PROG=	at
CFLAGS+=-Wall
SRCS=   at.c panic.c parsetime.c perm.c
LINKS=	${BINDIR}/at ${BINDIR}/atq \
	${BINDIR}/at ${BINDIR}/atrm \
	${BINDIR}/at ${BINDIR}/batch
MLINKS=	at.1 batch.1 \
	at.1 atq.1 \
	at.1 atrm.1

BINMODE= 4555
MANSRC= .
CLEANFILES += ${MAN1}
MANDEPEND = ${MAN1}

.include <bsd.prog.mk>

${MAN1}: at.man
	@${ECHO} Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e \
		"s@_ATSPOOL_DIR@$(ATSPOOL_DIR)@g; \
		s@_ATJOB_DIR@$(ATJOB_DIR)@g; \
		s@_DEFAULT_BATCH_QUEUE@$(DEFAULT_BATCH_QUEUE)@g; \
		s@_DEFAULT_AT_QUEUE@$(DEFAULT_AT_QUEUE)@g; \
		s@_LOADAVG_MX@$(LOADAVG_MX)@g; \
		s@_PERM_PATH@$(PERM_PATH)@g; \
		s@_LOCKFILE@$(LOCKFILE)@g" \
		< ${.OODATE} > ${.TARGET}

