#
# $FreeBSD$
#

PROG=	pmc
MAN=	

LIBADD=	kvm pmc m ncursesw pmcstat elf

SRCS=	pmc.c pmc_util.c cmd_pmc_stat.c \
	cmd_pmc_list.c cmd_pmc_filter.c

.include <bsd.prog.mk>
