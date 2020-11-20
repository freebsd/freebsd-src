# $NetBSD: forsubst.mk,v 1.3 2020/11/03 17:59:27 rillig Exp $
#
# The parser used to break dependency lines at ';' without regard for
# substitution patterns.  Back then, the first ';' was interpreted as the
# separator between the dependency and its commands.  This (perhaps coupled
# with the new handling of .for variables in ${:U<value>...) caused
# interesting results for lines like:
#
# .for file in ${LIST}
#   for-subst:       ${file:S;^;${here}/;g}
# .endfor
#
# See the commit to unit-tests/forsubst (without the .mk) from 2009-10-07.

all: for-subst

here := ${.PARSEDIR}
# this should not run foul of the parser
.for file in ${.PARSEFILE}
for-subst:	  ${file:S;^;${here}/;g}
	@echo ".for with :S;... OK"
.endfor
