# $NetBSD: dep.mk,v 1.3 2021/12/13 23:38:54 rillig Exp $
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

all:
