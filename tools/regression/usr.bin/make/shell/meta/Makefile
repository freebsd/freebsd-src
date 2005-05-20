#
# In compat mode (the default without -j) a line that contains no shell
# meta characters and no shell builtins is not passed to the shell but
# executed directly. In our example the ls line without meta characters
# will really execute ls, while the line with meta characters will execute
# our special shell.
#
# $FreeBSD$

all: sh
	@MAKEFLAGS= ${MAKE} -B no-meta
	@MAKEFLAGS= ${MAKE} -B meta

sh: sh.sh
	@cp ${.CURDIR}/sh.sh ${.OBJDIR}/sh
	@chmod +x ${.OBJDIR}/sh

.ifmake meta || no-meta

.SHELL: path="${.OBJDIR}/sh"

.PHONY: meta no-meta

meta:
	@ls *

no-meta:
	@ls -d .

.endif
