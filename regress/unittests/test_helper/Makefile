#	$OpenBSD: Makefile,v 1.2 2015/01/20 22:58:57 djm Exp $

LIB=	test_helper
SRCS=	test_helper.c fuzz.c

DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

# Hack to allow building with SUBDIR in ../../Makefile
regress: all

install:
	@echo -n

.include <bsd.lib.mk>
