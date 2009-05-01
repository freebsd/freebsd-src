# $FreeBSD: src/tools/regression/sysvmsg/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $
# SysV Message Queue Regression Utility
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:56:59 thorpej Exp $

PROG=	msgtest
NO_MAN=

regress:
	@if ./msgtest ./msgtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
