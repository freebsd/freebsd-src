# $FreeBSD$

# Test for broken LHS expansion.
# This *must* case make(1) to detect a recursive variable, and fail as such.
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
	@${MAKE} double || ${MAKE} failure
	@echo "PASS: Test targets detected no regression."
	@echo "Running test sysvmatch"
	@${MAKE} sysvmatch || ${MAKE} failure
	@echo "PASS: Test sysvmatch detected no regression."
	@echo "Running test lhs_expn"
	@! ${MAKE} lhs_expn && true || ${MAKE} failure
	@echo "PASS: Test lhs_expn detected no regression."

# Doubly-defined targets.  make(1) will warn, but use the "right" one.  If it
# switches to using the "non-right" one, it breaks things worse than a little
# regression test.
double:
	@true

double:
	@false

# Some versions of FreeBSD make(1) do not handle a nil LHS in sysvsubst.
sysvmatch:
	@echo EMPTY ${NIL:=foo} LHS | \
		diff -u ${.CURDIR}/regress.sysvmatch.out - || false

# A bogus target for the lhs_expn test;  If this is reached, then the make(1)
# program has not errored out because of the recursion caused by not expanding
# the left-hand-side's embedded variables above.
lhs_expn:
	@true

failure:
	@echo "FAIL: Test failed: regression detected.  See above."
	@false
