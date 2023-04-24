# $NetBSD: meta-ignore.inc,v 1.2 2023/02/25 19:30:32 sjg Exp $

# common logic for testing .MAKE.META.IGNORE_*

# we want a directory outside of .OBJDIR to drop a file
# that our meta file refers to.
# Note: these tests will not work if TMPDIR is /tmp or /var/tmp
# or a subdir thereof
IGNORE:= ${TMPDIR}/ignore
OBJ:=	${TMPDIR}/obj

# this is always ignored so make sure it isn't used above
TMPDIR= /tmp/nothanks

all:	one two three

setup:
	@mkdir -p ${IGNORE} ${OBJ}
	@echo > ${IGNORE}/check
	@rm -f ${OBJ}/check-ignore

makefile:= ${.INCLUDEDFROMDIR}/${.INCLUDEDFROMFILE}
TEST:= ${.INCLUDEDFROMFILE:R:C,.*meta-,,:S,-,_,g:tu}

DESC.one= Initialize check-ignore.meta
DESC.two= Use .MAKE.META.${TEST} - check-ignore is up to date
DESC.three= Skip .MAKE.META.${TEST} - check-ignore is out of date

# just in case someone runs us with -jN
.ORDER: one two three
one two three: .MAKE setup
	@echo "${DESC.${.TARGET}}"; \
	${MAKE} -C ${.CURDIR} -f ${makefile} check-ignore parent=${.TARGET}

.if make(check-ignore)
.MAKEFLAGS: -dM
.MAKE.MODE = meta verbose silent=yes
.OBJDIR: ${OBJ}
.if ${parent} == "two"
.if ${TEST} == "IGNORE_PATHS"
# this is a prefix list - any path that matches
# one of these prefixes will be ignored
.MAKE.META.IGNORE_PATHS = ${IGNORE}
.elif ${TEST} == "IGNORE_PATTERNS"
# more flexible but more expensive
# this example is equivalent to M*/ignore/*
# a match means ignore
.MAKE.META.IGNORE_PATTERNS = */ignore/*
.elif ${TEST} == "IGNORE_FILTER"
# this is the most flexible, but also most expensive
# if this expands to nothing - ignore the path
.MAKE.META.IGNORE_FILTER = N${IGNORE}/*
.endif
.endif

# : < just reads from ${IGNORE}/check
# so that our filemon trace will have a reference to it
# we ensure it is always newer than the target.
check-ignore: .META .NOPATH
	@: < ${IGNORE}/check > ${.TARGET}
	@sleep 1; echo ${.TARGET} > ${IGNORE}/check

.endif
