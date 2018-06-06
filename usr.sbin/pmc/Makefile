#
# $FreeBSD$
#

.include <src.opts.mk>
PROG_CXX=	pmc
MAN=	
CXXFLAGS+=

LIBADD=	kvm pmc m ncursesw pmcstat elf

SRCS=	pmc.c pmc_util.c cmd_pmc_stat.c \
	cmd_pmc_list.c cmd_pmc_filter.cc \
	cmd_pmc_summary.cc

.include <bsd.prog.mk>
