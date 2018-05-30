#
# $FreeBSD$
#

PROG=	pmc
MAN=	

LIBADD=	kvm pmc m ncursesw pmcstat elf

SRCS=	pmc.c pmc_util.c cmd_pmc_stat.c

CWARNFLAGS.cmd_pmc_stat.c=	-Wno-format
.include <bsd.prog.mk>
