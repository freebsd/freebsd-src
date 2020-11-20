# $NetBSD: varname-vpath.mk,v 1.3 2020/11/10 00:19:19 rillig Exp $
#
# Tests for the special VPATH variable, which is an obsolete way of
# specifying a colon-separated search path.  This search path is not active
# when the makefiles are read, but only later when the shell commands are run.
#
# Instead of the VPATH, better use the -I option or the special target .PATH.

.if !defined(TEST_MAIN)

all: .SILENT
	rm -rf varname-vpath.dir
	mkdir varname-vpath.dir
	touch varname-vpath.dir/file-in-subdirectory
	rm -rf varname-vpath.dir2
	mkdir varname-vpath.dir2
	touch varname-vpath.dir2/file2-in-subdirectory

	TEST_MAIN=yes VPATH=varname-vpath.dir:varname-vpath.dir2 \
		${MAKE} -f ${MAKEFILE} -dc

	rm -r varname-vpath.dir
	rm -r varname-vpath.dir2

.else

# The VPATH variable does not take effect at parse time.
# It is evaluated only once, between reading the makefiles and making the
# targets.  Therefore it could also be an ordinary variable, it doesn't need
# to be an environment variable or a command line variable.
.  if exists(file-in-subdirectory)
.    error
.  endif
.  if exists(file2-in-subdirectory)
.    error
.  endif

all:
	: ${exists(file-in-subdirectory):L:?yes 1:no 1}
	: ${exists(file2-in-subdirectory):L:?yes 2:no 2}

.endif
