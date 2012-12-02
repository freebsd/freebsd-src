# $FreeBSD$

# pf_ruleset.c is shared between kernel and pfctl
.PATH: ${.CURDIR}/../../sys/netpfil/pf

PROG=	pfctl
MAN=	pfctl.8

SRCS = pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
SRCS+= pfctl_osfp.c pfctl_radix.c pfctl_table.c pfctl_qstats.c
SRCS+= pfctl_optimize.c
SRCS+= pf_ruleset.c

WARNS?=	2
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes
CFLAGS+= -DENABLE_ALTQ -I${.CURDIR}

YFLAGS=

LDADD+=	-lm -lmd
DPADD+=	${LIBM} ${LIBMD}

.include <bsd.prog.mk>
