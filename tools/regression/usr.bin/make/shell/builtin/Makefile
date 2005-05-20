#
# In compat mode (the default without -j) a line that contains no shell
# meta characters and no shell builtins is not passed to the shell but
# executed directly. In our example the ls line without meta characters
# will really execute ls, while the line with meta characters will execute
# our special shell.
#
# $FreeBSD$

all: sh
	@MAKEFLAGS= ${MAKE} -B no-builtin
	@MAKEFLAGS= ${MAKE} -B builtin

sh: sh.sh
	@cp ${.CURDIR}/sh.sh ${.OBJDIR}/sh
	@chmod +x ${.OBJDIR}/sh

.ifmake builtin || no-builtin

.SHELL: path="${.OBJDIR}/sh"

.PHONY: builtin no-builtin

builtin:
	@exec ls -d .

no-builtin:
	@ls -d .

.endif
