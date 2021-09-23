#
# $FreeBSD$
#

PROG_CXX=	pmcstat
MAN=	pmcstat.8

CFLAGS+= -I${SRCTOP}/lib/libpmcstat
LIBADD=	kvm pmc m tinfow ncursesw pmcstat elf

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c \
pmcpl_callgraph.c pmcpl_gprof.c pmcpl_annotate.c \
pmcpl_annotate_cg.c pmcpl_calltree.c

.include <bsd.prog.mk>
