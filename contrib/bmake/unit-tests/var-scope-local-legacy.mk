# $NetBSD: var-scope-local-legacy.mk,v 1.3 2023/12/17 14:07:22 rillig Exp $
#
# Tests for legacy target-local variables, such as ${<F} or ${@D}.


# In the global or command line scopes, the legacy forms are not recognized,
# as the target-specific variables are not available either.  The expressions
# are retained so that they can be resolved later, in the target scope.
.if "${@D}" != "\${@D}"
.  error
.endif

# It's possible to define variables of the legacy name in the global or
# command line scope, and they override the target-local variables, leading to
# unnecessary confusion.
@D=	global-value
.if "${@D}" != "global-value"
.  error
.endif


all: .PHONY
	# Only variables of length 2 can be legacy, this one cannot.
	: LEN4=${LEN4:Uundef}_
	# The second character of the name must be 'D' or 'F'.
	: XY=${XY:Uundef}_
	# The first character must name one of the 7 predefined local
	# variables, 'A' is not such a character.
	: AF=${AF:Uundef}_
	# The variable '.MEMBER' is undefined, therefore '%D' and '%F' are
	# undefined as well.
	: %D=${%D:Uundef}_ %F=${%F:Uundef}_
	# The directory name of the target is shadowed by the global variable,
	# it would be '.' otherwise.  The basename is 'all'.
	: @D=${@D:Uundef}_ @F=${@F:Uundef}_
