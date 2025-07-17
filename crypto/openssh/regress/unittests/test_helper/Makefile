#	$OpenBSD: Makefile,v 1.3 2016/07/04 18:01:44 guenther Exp $

LIB=	test_helper
SRCS=	test_helper.c fuzz.c

NOPROFILE= yes
NOPIC=	yes

# Hack to allow building with SUBDIR in ../../Makefile
regress: all

install:
	@echo -n

.include <bsd.lib.mk>
