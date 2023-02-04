# $NetBSD: var-scope-local-legacy.mk,v 1.2 2022/09/27 19:18:45 rillig Exp $
#
# Tests for legacy target-local variables, such as ${<F} or ${@D}.

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
	# The directory name of the target is '.', its basename is 'all'.
	: @D=${@D:Uundef}_ @F=${@F:Uundef}_
