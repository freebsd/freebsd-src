#
# $Id: Makefile,v 1.1 1993/12/21 19:42:18 wollman Exp $
#

PARSEFLAGS= -DCLOCK_SCHMID -DCLOCK_DCF7000 -DCLOCK_MEINBERG \
	-DCLOCK_RAWDCF -DCLOCK_TRIMSV6

CFLAGS+= -I${.CURDIR}/../include ${PARSEFLAGS}

SRCS= parse.c parse_conf.c clk_meinberg.c clk_schmid.c clk_rawdcf.c \
	clk_dcf7000.c clk_trimble.c

NOMAN=
NOPROFILE=
LIB= parse

install:

.include <bsd.lib.mk>
