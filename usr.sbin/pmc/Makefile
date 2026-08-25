.include <src.opts.mk>

PACKAGE=	pmc
PROG_CXX=	pmc
MAN=	
CWARNFLAGS.gcc+= -Wno-redundant-decls
CFLAGS+= -I${SRCTOP}/lib/libpmcstat

LIBADD=	pmc m util ncursesw pmcstat elf dwarf

SRCS=	pmc.c pmc_util.c cmd_pmc_stat.c \
	cmd_pmc_list.c cmd_pmc_filter.cc \
	cmd_pmc_summary.cc \
	cmd_pmc_frontend.cc cmd_pmc_record.cc cmd_pmc_info.cc \
	display.cc util.cc view.cc

.include <bsd.prog.mk>
