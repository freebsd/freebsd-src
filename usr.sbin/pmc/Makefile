#
# $FreeBSD$
#

.include <src.opts.mk>
PROG_CXX=	pmc
MAN=	
CXXFLAGS+= -O0

LIBADD=	kvm pmc m ncursesw pmcstat elf

SRCS=	pmc.c pmc_util.c cmd_pmc_stat.c \
	cmd_pmc_list.c cmd_pmc_filter.cc

.include <bsd.prog.mk>
