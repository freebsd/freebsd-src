# $FreeBSD: src/lib/libugidfw/Makefile,v 1.3.2.1 2003/01/15 08:39:57 chris Exp $

LIB=		ugidfw
SHLIB_MAJOR=	1
SRCS=		ugidfw.c
INCS=		ugidfw.h

MAN+=		bsde_get_rule.3 bsde_get_rule_count.3 bsde_parse_rule.3 \
		bsde_rule_to_string.3 libugidfw.3

MLINKS= 	bsde_get_rule.3 bsde_delete_rule.3
MLINKS+=	bsde_get_rule.3 bsde_set_rule.3
MLINKS+=	bsde_get_rule_count.3 bsde_get_rule_slots.3
MLINKS+=	bsde_parse_rule.3 bsde_parse_rule_string.3

.include <bsd.lib.mk>
