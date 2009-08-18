#	$OpenBSD: Makefile,v 1.18 2006/10/28 14:29:05 mcbride Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
SRCS+=	pfctl_osfp.c pfctl_radix.c pfctl_table.c pfctl_qstats.c
SRCS+=	pfctl_optimize.c pf_ruleset.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes -I${.CURDIR}
YFLAGS=
MAN=	pfctl.8

# Ruleset and Anchor handling
.PATH: ${.CURDIR}/../../sys/net

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
