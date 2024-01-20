# $NetBSD: recursive.mk,v 1.7 2023/10/19 18:24:33 rillig Exp $
#
# In -dL mode, a variable may get expanded before it makes sense.
# This would stop make from doing anything since the "recursive" error
# is fatal and exits immediately.
#
# The purpose of evaluating that variable early was just to detect
# whether there are unclosed variables.  The variable value is therefore
# parsed with VARE_PARSE_ONLY for that purpose.
#

.MAKEFLAGS: -dL


AM_V_lt=	${am__v_lt_${V}}
am__v_lt_=	${am__v_lt_${AM_DEFAULT_VERBOSITY}}
am__v_lt_0=	--silent
am__v_lt_1=

# Since parse.c 1.243 from 2020-07-31 and before parse.c 1.249 from
# 2020-08-06, when make ran in -dL mode, it reported: "Variable am__v_lt_ is
# recursive."
#
# Seen in pkgsrc/x11/libXfixes, and probably many more package that use
# GNU Automake.
libXfixes_la_LINK=	... ${AM_V_lt} ...


# The purpose of the -dL flag is to detect unclosed variables.  This
# can be achieved by just parsing the variable and not evaluating it.
#
# When the variable is only parsed but not evaluated, bugs in nested
# variables are not discovered.  But these are hard to produce anyway,
# therefore that's acceptable.  In most practical cases, the missing
# brace would be detected directly in the line where it is produced.
MISSING_BRACE_INDIRECT:=	${:U\${MISSING_BRACE}
# expect+1: Unclosed variable "MISSING_PAREN"
UNCLOSED=	$(MISSING_PAREN
# expect+1: Unclosed variable "MISSING_BRACE"
UNCLOSED=	${MISSING_BRACE
UNCLOSED=	${MISSING_BRACE_INDIRECT}
