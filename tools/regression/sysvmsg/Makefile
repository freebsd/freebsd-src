# $FreeBSD: src/tools/regression/sysvmsg/Makefile,v 1.1 2002/08/15 06:34:37 alfred Exp $
# SysV Message Queue Regression Utility
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:56:59 thorpej Exp $

PROG=	msgtest
NOMAN=	yes

regress:
	@if ./msgtest ./msgtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
