# $NetBSD: sh-dots.mk,v 1.1 2020/08/22 11:27:02 rillig Exp $
#
# Tests for the special shell command line "...", which does not run the
# commands below it but appends them to the list of commands that are run
# at the end.

all: first hidden repeated commented

# The ${.TARGET} correctly expands to the target name, even though the
# commands are run separately from the main commands.
first:
	@echo first ${.TARGET}
	...
	@echo first delayed ${.TARGET}

# The dots cannot be prefixed by the usual @-+ characters.
# They must be written exactly as dots.
hidden: .IGNORE
	@echo hidden ${.TARGET}
	@...
	@echo hidden delayed ${.TARGET}

# Since the shell command lines don't recognize '#' as comment character,
# the "..." is not interpreted specially here.
commented: .IGNORE
	@echo commented ${.TARGET}
	...	# Run the below commands later
	@echo commented delayed ${.TARGET}

# The "..." can appear more than once, even though that doesn't make sense.
# The second "..." is a no-op.
repeated: .IGNORE
	@echo repeated ${.TARGET}
	...
	@echo repeated delayed ${.TARGET}
	...
	@echo repeated delayed twice ${.TARGET}
