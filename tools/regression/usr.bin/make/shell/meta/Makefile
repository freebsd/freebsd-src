#
# In compat mode (the default without -j) a line that contains no shell
# meta characters and no shell builtins is not passed to the shell but
# executed directly. In our example the ls line without meta characters
# will really execute ls, while the line with meta characters will execute
# our special shell.
#
# $FreeBSD: src/tools/regression/usr.bin/make/shell/meta/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.SHELL: path="${.OBJDIR}/sh"

.PHONY: meta no-meta

meta:
	@ls *

no-meta:
	@ls -d .
