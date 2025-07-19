# $NetBSD: cmd-errors-jobs.mk,v 1.16 2025/06/28 22:39:28 rillig Exp $
#
# Demonstrate how errors in expressions affect whether the commands
# are actually executed in jobs mode.

RUN=	@ run() {					\
		echo "begin $$*"			\
		&& ${MAKE} -f ${MAKEFILE} -j1 "$$*"	\
		&& echo "end $$* with status $$?"	\
		|| echo "end $$* with status $$?"	\
		&& echo;				\
	} && run

all:
	${RUN} undefined-direct
	${RUN} undefined-indirect
	${RUN} parse-error-direct
	${RUN} parse-error-indirect
	${RUN} begin-direct
	${RUN} begin-indirect
	${RUN} end-direct
	${RUN} end-indirect


# Undefined variables in expressions are not an error.  They expand to empty
# strings.
# expect: : undefined-direct--eol
# expect: end undefined-direct with status 0
# expect: : undefined-direct--eol
# expect: end undefined-indirect with status 0
undefined-indirect: undefined-direct
undefined-direct:
	: $@-${UNDEFINED}-eol


parse-error-indirect: parse-error-direct
parse-error-direct: parse-error-unclosed-expression
parse-error-direct: parse-error-unclosed-modifier
parse-error-direct: parse-error-unknown-modifier

parse-error-unclosed-expression:
	: unexpected $@-${UNCLOSED

parse-error-unclosed-modifier:
	: unexpected $@-${UNCLOSED:

parse-error-unknown-modifier:
	: unexpected $@-${UNKNOWN:Z}-eol

# expect-not-matches: ^: unexpected
# expect: make: Unclosed variable "UNCLOSED"
# expect: in command ": unexpected $@-${UNCLOSED"
# expect: make: Unclosed expression, expecting "}"
# expect: make: Unknown modifier ":Z"
# expect: end parse-error-direct with status 2
# expect: make: Unclosed variable "UNCLOSED"
# expect: make: Unclosed expression, expecting "}"
# expect: make: Unknown modifier ":Z"
# expect: end parse-error-indirect with status 2


.if make(begin-direct)
begin-direct:
.BEGIN:
	(exit 13) # $@
.endif
# expect: begin begin-direct
# expect: make: stopped making "begin-direct" in unit-tests
# expect: end begin-direct with status 1


.if make(begin-indirect)
begin-indirect:
.BEGIN: before-begin
	: Making $@
before-begin:
	(exit 13) # $@
.endif
# expect: begin begin-indirect
# expect: *** Error code 13 (continuing)
# expect: make: stopped making "begin-indirect" in unit-tests
# expect: end begin-indirect with status 1


.if make(end-direct)
end-direct:
.END:
	(exit 13) # $@
.endif
# expect: begin end-direct
# expect: *** Error code 13 (continuing)
# expect: Stop.
# expect: make: stopped making "end-direct" in unit-tests
# expect: end end-direct with status 1

.if make(end-indirect)
end-indirect:
.END: before-end
	: Making $@
before-end:
	(exit 13) # $@
.endif
# expect: begin end-indirect
# expect: *** Error code 13 (continuing)
# expect: make: stopped making "end-indirect" in unit-tests
# expect: end end-indirect with status 1
