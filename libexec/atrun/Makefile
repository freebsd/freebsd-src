#	$Id: Makefile,v 1.1 1994/01/05 01:02:56 nate Exp $

MAINSRC= ${.CURDIR}/../../usr.bin/at

.include "$(MAINSRC)/Makefile.inc"

PROG=	atrun
MAN8=	atrun.8

BINDIR= $(ATLIB_DIR)
MANSRC= .
CLEANFILES += ${MAN8}
MANDEPEND = ${MAN8}

CFLAGS+= -I$(MAINSRC)

${MAN8}: atrun.man
	sed -e \
		"s@_ATSPOOL_DIR@$(ATSPOOL_DIR)@g; \
		s@_ATJOB_DIR@$(ATJOB_DIR)@g; \
		s@_ATLIB_DIR@$(ATLIB_DIR)@g; \
		s@_LOADAVG_MX@$(LOADAVG_MX)@g;" \
		< $? > $@

.include <bsd.prog.mk>
