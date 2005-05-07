# $FreeBSD: src/tools/regression/sysvmsg/Makefile,v 1.1.8.1 2005/02/13 07:23:35 obrien Exp $
# SysV Message Queue Regression Utility
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:56:59 thorpej Exp $

PROG=	msgtest
NOMAN=

regress:
	@if ./msgtest ./msgtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
