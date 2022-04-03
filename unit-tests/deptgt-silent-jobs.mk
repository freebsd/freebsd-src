# $NetBSD: deptgt-silent-jobs.mk,v 1.2 2022/02/12 11:14:48 rillig Exp $
#
# Ensure that the special dependency target '.SILENT' only affects the amount
# of output, but not the kind of error handling.
#
# History:
#	In job.c 1.83 from 2003.12.20.00.18.22, in an attempt to fix
#	https://gnats.netbsd.org/18573, commands that suppressed error
#	handling were output in jobs mode, even when the global '.SILENT'
#	was set.  This was fixed in job.c 1.452 from 2022-02-12.
#
# See also:
#	https://gnats.netbsd.org/45356

all: compat jobs
.PHONY: all compat jobs test

.SILENT:
test:
	@echo '${VARIANT}: testing 1'
	-echo '${VARIANT}: testing 2'
	echo '${VARIANT}: testing 3'

# expect: compat: testing 1
# expect: compat: testing 2
# expect: compat: testing 3
compat:
	@${MAKE} -r -f ${MAKEFILE} test VARIANT=compat

# expect: jobs: testing 1
# expect: echo 'jobs: testing 2'
# expect: jobs: testing 2
# expect: jobs: testing 3
jobs:
	@${MAKE} -r -f ${MAKEFILE} test VARIANT=jobs -j1
