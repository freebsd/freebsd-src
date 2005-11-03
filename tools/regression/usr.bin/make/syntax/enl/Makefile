# $FreeBSD: src/tools/regression/usr.bin/make/syntax/enl/Makefile,v 1.1 2005/06/01 11:26:47 harti Exp $
#
# Test handling of escaped newlines.
#

.ifmake test1

# This should succeed
\
\
\
test1:
	@echo ok

.elifmake test2

# This should fail because the comment continues on the next lines leading
# to an unassociated shell command.
\
#\
\
test2:
	@echo ok

.endif
