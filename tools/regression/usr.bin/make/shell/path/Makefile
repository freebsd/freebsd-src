#
# Change the path for builtin shells. There are two methods to do this.
# This is the first of them when the basename of the path is equal to
# one of the builtin shells so that the .SHELL target automatically
# also selectes the shell without specifying the name.
#
# Be sure to include a meta-character into the command line, so that
# really our shell is executed.
#
# $FreeBSD: src/tools/regression/usr.bin/make/shell/path/Makefile,v 1.2.24.1 2010/02/10 00:26:20 kensmith Exp $
#

.ifmake sh_test

.SHELL: path=${.OBJDIR}/sh
sh_test:
	@: This is the shell.

.elifmake csh_test

.SHELL: path=${.OBJDIR}/csh
csh_test:
	@: This is the C-shell.

.elifmake ksh_test

.SHELL: path=${.OBJDIR}/ksh
ksh_test:
	@: This is the Korn-shell.

.endif
