# $NetBSD: forsubst.mk,v 1.2 2020/10/24 08:34:59 rillig Exp $

all: for-subst

here := ${.PARSEDIR}
# this should not run foul of the parser
.for file in ${.PARSEFILE}
for-subst:	  ${file:S;^;${here}/;g}
	@echo ".for with :S;... OK"
.endfor
