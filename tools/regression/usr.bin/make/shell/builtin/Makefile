#
# In compat mode (the default without -j) a line that contains no shell
# meta characters and no shell builtins is not passed to the shell but
# executed directly. In our example the ls line without meta characters
# will really execute ls, while the line with meta characters will execute
# our special shell.
#
# $FreeBSD: src/tools/regression/usr.bin/make/shell/builtin/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

.SHELL: path="${.CURDIR}/sh"

.PHONY: builtin no-builtin

builtin:
	@exec ls -d .

no-builtin:
	@ls -d .
