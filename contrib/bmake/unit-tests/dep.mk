# $NetBSD: dep.mk,v 1.4 2023/06/01 07:27:30 rillig Exp $
#
# Tests for dependency declarations, such as "target: sources".

.MAIN: all

# As soon as a target is defined using one of the dependency operators, it is
# restricted to this dependency operator and cannot use the others anymore.
only-colon:
# expect+1: Inconsistent operator for only-colon
only-colon!
# expect+1: Inconsistent operator for only-colon
only-colon::
# Ensure that the target still has the original operator.  If it hadn't, there
# would be another error message.
only-colon:


# Before parse.c 1.158 from 2009-10-07, the parser broke dependency lines at
# the first ';', without parsing expressions as such.  It interpreted the
# first ';' as the separator between the dependency and its commands, and the
# '^' as a shell command.
all: for-subst
.for file in ${.PARSEFILE}
for-subst:	  ${file:S;^;./;g}
	@echo ".for with :S;... OK"
.endfor


all:
