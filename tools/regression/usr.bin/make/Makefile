# $FreeBSD$

# Test for broken LHS expansion.
# This *must* cause make(1) to detect a recursive variable, and fail as such.
.if make(lhs_expn)
FOO=		${BAR}
BAR${NIL}=	${FOO}
FOO${BAR}=	${FOO}
.endif

DATA1=	helllo
DATA2:=	${DATA1}
DATA3=	${DATA2:S/ll/rr/g}
DATA4:=	${DATA2:S/ll/rr/g}
DATA2?=	allo
DATA5:= ${DATA2:S/ll/ii/g} ${DATA1:S/ll/rr/g}
DATA2=	yello
DATA1:=	${DATA5:S/l/r/g}
NIL=

all:
	@echo "Running test variables"
	@echo 1:${DATA1} 2:${DATA2} 3:${DATA3} 4:${DATA4} 5:${DATA5} | \
		diff -u ${.CURDIR}/regress.variables.out - || ${MAKE} failure
	@echo "PASS: Test variables detected no regression, output matches."
	@echo "Running test targets"
	@${MAKE} double 2>/dev/null || ${MAKE} failure
	@echo "PASS: Test targets detected no regression."
	@echo "Running test sysvmatch"
	@${MAKE} sysvmatch || ${MAKE} failure
	@echo "PASS: Test sysvmatch detected no regression."
	@echo "Running test lhs_expn"
	@! ${MAKE} lhs_expn && true || ${MAKE} failure
	@echo "PASS: Test lhs_expn detected no regression."
	@echo "Running test notdef"
	@${MAKE} notdef || ${MAKE} failure
	@echo "PASS: Test notdef detected no regression."
	@echo "Running test modifiers"
	@${MAKE} modifiers || ${MAKE} failure
	@echo "PASS: Test modifiers detected no regression."
	@echo "Running test funny_targets"
	@${MAKE} funny_targets || ${MAKE} failure
	@echo "PASS: Test funny_targets detected no regression."
	@echo "Running test arith_expr"
	@${MAKE} arith_expr || ${MAKE} failure
	@echo "PASS: Test arith_expr detected no regression."

.if make(double)
# Doubly-defined targets.  make(1) will warn, but use the "right" one.  If it
# switches to using the "non-right" one, it breaks things worse than a little
# regression test.
double:
	@true

double:
	@false
.endif

.if make(sysvmatch)
# Some versions of FreeBSD make(1) do not handle a nil LHS in sysvsubst.
sysvmatch:
	@echo EMPTY ${NIL:=foo} LHS | \
		diff -u ${.CURDIR}/regress.sysvmatch.out - || false
.endif

# A bogus target for the lhs_expn test;  If this is reached, then the make(1)
# program has not errored out because of the recursion caused by not expanding
# the left-hand-side's embedded variables above.
lhs_expn:
	@true

.if make(notdef)
# make(1) claims to only evaluate a conditional as far as is necessary
# to determine its value; that was not always the case.
.undef notdef
notdef:
.if defined(notdef) && ${notdef:U}
.endif
.endif

.if make(modifiers)
# See if make(1) supports the C modifier.
modifiers:
	@if ${MAKE} -V .CURDIR:C/.// 2>&1 >/dev/null | \
	    grep -q "Unknown modifier 'C'"; then \
		false; \
	fi
.endif

.if make(funny_targets)
funny_targets: colons::target exclamation!target
colons::target:
exclamation!target: 
.endif

.if make(arith_expr)
arith_expr:
# See if arithmetic expression parsing is broken.
# The different spacing below is intentional.
VALUE=	0
.if (${VALUE} < 0)||(${VALUE}>0)
.endif
.endif

failure:
	@echo "FAIL: Test failed: regression detected.  See above."
	@false
