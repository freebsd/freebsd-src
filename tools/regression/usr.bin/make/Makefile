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
	@echo "Running test PATH_exists"
	@${MAKE} PATH_exists || ${MAKE} failure
	@echo "PASS: Test PATH_exists detected no regression."
	@echo "Running test double_quotes"
	@${MAKE} double_quotes || ${MAKE} failure
	@echo "PASS: Test double_quotes detected no regression."
	@echo "Running test double_quotes2"
	@! ${MAKE} double_quotes2 >/dev/null 2>&1 && true || ${MAKE} failure
	@echo "PASS: Test double_quotes2 detected no regression."
	@echo "Running test pass_cmd_vars"
	@${MAKE} pass_cmd_vars || ${MAKE} failure
	@echo "PASS: Test pass_cmd_vars detected no regression."
	@echo "Running test plus_flag"
	@${MAKE} plus_flag || ${MAKE} failure
	@echo "PASS: Test plus_flag detected no regression."

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

.if make(PATH_exists)
PATH_exists:
.PATH: ${.CURDIR}
.if !exists(${.CURDIR}/) || !exists(${.CURDIR}/.) || !exists(${.CURDIR}/..)
.error exists() failed
.endif
.endif

.if make(double_quotes)
VALUE=	foo ""
double_quotes:
.if ${VALUE:S/$//} != ${VALUE}
.error "" reduced to "
.endif
.endif

.if make(double_quotes2)
double_quotes2:
	@cat /dev/null ""
.endif

#
# Check passing of variable via MAKEFLAGS
#
.if make(pass_cmd_vars)
pass_cmd_vars:
	@${MAKE} CMD1=cmd1 CMD2=cmd2 pass_cmd_vars_1
	@${MAKE} CMD1=cmd1 CMD2=cmd2 pass_cmd_vars_2
	@${MAKE} CMD1=cmd1 CMD2=cmd2 pass_cmd_vars_3
	@${MAKE} CMD1=cmd1 CMD2=cmd2 pass_cmd_vars_4
.endif

.if make(pass_cmd_vars_1)
# Check that the variable definition arrived from the calling make
pass_cmd_vars_1:
	@:

.if ${CMD1} != cmd1 || ${CMD2} != cmd2
.error variables not passed through MAKEFLAGS
.endif

# Check that the variable definition is in MAKEFLAGS
.if ${.MAKEFLAGS:MCMD1=*} != "CMD1=cmd1" || ${.MAKEFLAGS:MCMD2=*} != "CMD2=cmd2"
.error variable assignment not found in $${MAKEFLAGS}
.endif

# Check that the variable definition is not in MFLAGS
.if ${MFLAGS:MCMD1=*} != "" || ${MFLAGS:MCMD2=*} != ""
.error variable assignment found in $${MFLAGS}
.endif
.endif

.if make(pass_cmd_vars_2)
# Check that we cannot override the passed variables
CMD1=foo1
CMD2=foo2

.if ${CMD1} != cmd1 || ${CMD2} != cmd2
.error MAKEFLAGS-passed variables overridden
.endif

pass_cmd_vars_2:
	@:
.endif

.if make(pass_cmd_vars_3)
# Check that we can override the passed variables on the next sub-make's
# command line

pass_cmd_vars_3:
	@${MAKE} CMD1=foo1 pass_cmd_vars_3_1
.endif

.if make(pass_cmd_vars_3_1)
.if ${CMD1} != foo1 || ${CMD2} != cmd2
.error MAKEFLAGS-passed variables not overridden on command line
.endif
pass_cmd_vars_3_1:
	@:
.endif

.if make(pass_cmd_vars_4)
# Ensure that a variable assignment passed via MAKEFLAGS may be overwritten
# by evaluating the .MAKEFLAGS target.

.MAKEFLAGS: CMD1=baz1

pass_cmd_vars_4:
	@${MAKE} pass_cmd_vars_4_1

.if ${CMD1} != baz1 || ${CMD2} != cmd2
.error MAKEFLAGS-passed variables not overridden via .MAKEFLAGS target
.endif

.endif
.if make(pass_cmd_vars_4_1)
.if ${CMD1} != baz1 || ${CMD2} != cmd2
.error MAKEFLAGS-passed variables not overridden via .MAKEFLAGS target (2)
.endif
pass_cmd_vars_4_1:
	@:
.endif

#
# Test whether make supports the '+' flag (meaning: execute even with -n)
#
.if make(plus_flag)
OUT != ${MAKE} -n plus_flag_1
.if ${OUT} != "/tmp"
.error make doesn't handle + flag
.endif
plus_flag:
	@:
.endif
.if make(plus_flag_1)
plus_flag_1:
	+@cd /tmp; pwd
.endif

failure:
	@echo "FAIL: Test failed: regression detected.  See above."
	@false
