# $NetBSD: deptgt-end.mk,v 1.6 2020/10/23 19:28:17 rillig Exp $
#
# Tests for the special target .END in dependency declarations,
# which is run after making the desired targets.

VAR=	Should not be expanded.

.BEGIN:
	: $@ '$${VAR}'
	...
	: $@ '$${VAR}' deferred
# Oops: The deferred command must not be expanded twice.
# The Var_Subst in Compat_RunCommand looks suspicious.
# The Var_Subst in JobSaveCommand looks suspicious.

.END:
	: $@ '$${VAR}'
	...
	: $@ '$${VAR}' deferred

# The .END node can define dependencies, just like a regular target.
.END: end-action
end-action: .NOTMAIN
	: $@ '$${VAR}'
	...
	: $@ '$${VAR}' deferred

all:
	: $@ '$${VAR}'
	...
	: $@ '$${VAR}' deferred
# Oops: The deferred command must not be expanded twice.
# The Var_Subst in Compat_RunCommand looks suspicious.
# The Var_Subst in JobSaveCommand looks suspicious.

# The deferred commands are run in the order '.END .BEGIN all'.
# This may be unexpected at first since the natural order would be
# '.BEGIN all .END', but it is implemented correctly.
#
# At the point where the commands of a node with deferred commands are run,
# the deferred commands are appended to the commands of the .END node.
# This happens in Compat_RunCommand, and to prevent an endless loop, the
# deferred commands of the .END node itself are not appended to itself.
# Instead, the deferred commands of the .END node are run as if they were
# immediate commands.
