# $FreeBSD$

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

# Doubly-defined targets.  NetBSD make(1) will warn, ours will silently use the
# ``right'' one.
double:
	@true

double:
	@false

# Some versions of FreeBSD make(1) do not handle a nil LHS in sysvsubst.
sysvmatch:
	@echo EMPTY ${NIL:=foo} LHS | \
		diff -u ${.CURDIR}/regress.sysvmatch.out - || false

failure:
	@echo "FAIL: Test failed: regression detected.  See above."
	@false
