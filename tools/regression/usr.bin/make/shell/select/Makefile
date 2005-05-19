#
# We just select the builtin shells and check whether it is really
# executed. This should print just the shell paths. Because we
# normally don't have a ksh, we make this test conditional. This means
# one has to recreate the test results once ksh is installed.
#
# $FreeBSD$
#

all:
	@${MAKE} sh
	@${MAKE} csh
	@if [ -x /bin/ksh ] ; then ${MAKE} ksh ; fi

.ifmake sh

.SHELL: name=sh
sh: print_path

.elifmake csh

.SHELL: name=csh
csh: print_path

.elifmake ksh

.SHELL: name=ksh
ksh: print_path

.endif

print_path:
	@ps -opid,command | awk '$$1=='$$$$' { print $$2; }'
