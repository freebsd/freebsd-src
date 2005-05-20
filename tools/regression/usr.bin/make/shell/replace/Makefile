#
# Test that we can replace the shell and set all the shell parameters
# (except meta and builtin which have their own test). This is done by
# using a shell script for the shell that echoes all command line
# arguments and its standard input. The shell name should not be one of
# the builtin shells.
#
# XXX There seems to be a problem here in -j1 mode: for the line without
# @- make should switch on echoing again, but it doesn't.
#
# $FreeBSD$

all: shell
	${MAKE} test1
	${MAKE} -j1 test1

shell: sh.sh
	@cp ${.CURDIR}/sh.sh ${.OBJDIR}/shell
	@chmod +x ${.OBJDIR}/shell

.ifmake test1

.SHELL: name="shell" path="${.OBJDIR}/shell"			\
	quiet="be quiet" echo="be verbose" filter="be verbose"	\
	echoFlag="x" errFlag="y"				\
	hasErrCtl=y check="check errors" ignore="ignore errors"

.PHONY: test1
test1:
	-@funny $$
	funnier $$

.endif
