# $Id$

# Convenience rules for the top level directory containing a TET-based test
# suite.

.if !defined(TOP)
.error Make variable \"TOP\" has not been defined.
.endif

.include "${TOP}/mk/elftoolchain.tetvars.mk"

.MAIN: all
.PHONY:	clobber execute tccbuild tccclean test


# Set up the environment for invoking "tcc", based on the target
# specified.

.if !defined(TET_EXECUTE)
TET_EXECUTE=	${.OBJDIR}
.endif

.if make(tccbuild)
TET_OPTIONS+=	-b
.endif

.if make(tccclean)
TET_OPTIONS+=	-c
.endif

.if make(execute) || make(test)
TET_OPTIONS+=	-e
.endif

execute tccbuild tccclean test:
	TET_ROOT=${TET_ROOT} TET_EXECUTE=${TET_EXECUTE} \
		TET_SUITE_ROOT=${.CURDIR} ${TET_ROOT}/bin/tcc ${TET_OPTIONS} .

clobber:	clean
	rm -rf ${TET_RESULTS_DIR} ${TET_TMP_DIR}

# Ensure that a 'make test' does not recurse further into the test suite's
# directory hierarchy.
.if !make(test)
.include "${TOP}/mk/elftoolchain.subdir.mk"
.endif
