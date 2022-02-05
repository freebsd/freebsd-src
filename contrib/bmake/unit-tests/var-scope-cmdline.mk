# $NetBSD: var-scope-cmdline.mk,v 1.1 2022/01/23 16:25:54 rillig Exp $
#
# Tests for variables specified on the command line.
#
# Variables that are specified on the command line override those from the
# global scope.
#
# For performance reasons, the actual implementation is more complex than the
# above single-sentence rule, in order to avoid unnecessary lookups in scopes,
# which before var.c 1.586 from 2020-10-25 calculated the hash value of the
# variable name once for each lookup.  Instead, when looking up the value of
# a variable, the search often starts in the global scope since that is where
# most of the variables are stored.  This conflicts with the statement that
# variables from the cmdline scope override global variables, since after the
# common case of finding a variable in the global scope, another lookup would
# be needed in the cmdline scope to ensure that there is no overriding
# variable there.
#
# Instead of this costly lookup scheme, make implements it in a different
# way:
#
#	Whenever a global variable is created, this creation is ignored if
#	there is a cmdline variable of the same name.
#
#	Whenever a cmdline variable is created, any global variable of the
#	same name is deleted.
#
#	Whenever a global variable is deleted, nothing special happens.
#
#	Deleting a cmdline variable is not possible.
#
# These 4 rules provide the guarantee that whenever a global variable exists,
# there cannot be a cmdline variable of the same name.  Therefore, after
# finding a variable in the global scope, no additional lookup is needed in
# the cmdline scope.
#
# The above ruleset provides the same guarantees as the simple rule "cmdline
# overrides global".  Due to an implementation mistake, the actual behavior
# was not entirely equivalent to the simple rule though.  The mistake was
# that when a cmdline variable with '$$' in its name was added, a global
# variable was deleted, but not with the exact same name as the cmdline
# variable.  Instead, the name of the global variable was expanded one more
# time than the name of the cmdline variable.  For variable names that didn't
# have a '$$' in their name, it was implemented correctly all the time.
#
# The bug was added in var.c 1.183 on 2013-07-16, when Var_Set called
# Var_Delete to delete the global variable.  Just two months earlier, in var.c
# 1.174 from 2013-05-18, Var_Delete had started to expand the variable name.
# Together, these two changes made the variable name be expanded twice in a
# row.  This bug was fixed in var.c 1.835 from 2021-02-22.
#
# Another bug was the wrong assumption that "deleting a cmdline variable is
# not possible".  Deleting such a variable has been possible since var.c 1.204
# from 2016-02-19, when the variable modifier ':@' started to delete the
# temporary loop variable after finishing the loop.  It was probably not
# intended back then that a side effect of this seemingly simple change was
# that both global and cmdline variables could now be undefined at will as a
# side effect of evaluating a variable expression.  As of 2021-02-23, this is
# still possible.
#
# Most cmdline variables are set at the very beginning, when parsing the
# command line arguments.  Using the special target '.MAKEFLAGS', it is
# possible to set cmdline variables at any later time.

# A normal global variable, without any cmdline variable nearby.
VAR=	global
.info ${VAR}

# The global variable is "overridden" by simply deleting it and then
# installing the cmdline variable instead.  Since there is no obvious way to
# undefine a cmdline variable, there is no need to remember the old value
# of the global variable could become visible again.
#
# See varmod-loop.mk for a non-obvious way to undefine a cmdline variable.
.MAKEFLAGS: VAR=makeflags
.info ${VAR}

# If Var_SetWithFlags should ever forget to delete the global variable,
# the below line would print "global" instead of the current "makeflags".
.MAKEFLAGS: -V VAR
