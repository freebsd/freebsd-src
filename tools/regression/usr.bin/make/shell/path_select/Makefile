#
# Change the path for builtin shells. There are two methods to do this.
# This is the second of them when both a path and a name are specified.
# This selects a builtin shell according to the name, but executes it
# from the specified path.
#
# Be sure to include a meta-character into the command line, so that
# really our shell is executed.
#
# $FreeBSD: src/tools/regression/usr.bin/make/shell/path_select/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $
#

.ifmake sh_test

.SHELL: name=sh path=${.CURDIR}/shell
sh_test:
	@: This is the shell.

.elifmake csh_test

.SHELL: name=csh path=${.CURDIR}/shell
csh_test:
	@: This is the C-shell.

.elifmake ksh_test

.SHELL: name=ksh path=${.CURDIR}/shell
ksh_test:
	@: This is the Korn-shell.

.endif
