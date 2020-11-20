# $NetBSD: varname-dollar.mk,v 1.3 2020/08/19 05:40:06 rillig Exp $
#
# Tests for the expression "$$", which looks as if it referred to a variable,
# but simply expands to a single '$' sign.
#
# If there really were a special variable named '$', the expressions ${${DOLLAR}}
# and $$ would always expand to the same value.

# Using the dollar sign in variable names is tricky and not recommended.
# To see that using this variable indeed affects the variable '$', run the
# test individually with the -dv option.
DOLLAR=		$$

# At this point, the variable '$' is not defined. Therefore the second line
# returns an empty string.
.info dollar is $$.
.info dollar in braces is ${${DOLLAR}}.

# Now overwrite the '$' variable to see whether '$$' really expands to that
# variable, or whether '$$' is handled by the parser.
${DOLLAR}=	dollar

# At this point, the variable '$' is defined, therefore its value is printed
# in the second .info directive.
.info dollar is $$.
.info dollar in braces is ${${DOLLAR}}.

all:
	@:;
