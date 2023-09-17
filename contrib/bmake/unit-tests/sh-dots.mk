# $NetBSD: sh-dots.mk,v 1.3 2020/10/25 22:04:24 rillig Exp $
#
# Tests for the special shell command line "...", which does not run the
# commands below it but appends them to the list of commands that are run
# at the end.

.MAKEFLAGS: -d0			# switch stdout to being line-buffered

all: first hidden repeated commented indirect indirect-space

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

# The dots don't have to be written literally, they can also come from a
# variable expression.
indirect:
	@echo indirect regular
	${:U...}
	@echo indirect deferred

# If the dots are followed by a space, that space is part of the command and
# thus does not defer the command below it.
indirect-space: .IGNORE
	@echo indirect-space regular
	${:U... }
	@echo indirect-space deferred


# The "..." can appear more than once, even though that doesn't make sense.
# The second "..." is a no-op.
repeated: .IGNORE
	@echo repeated ${.TARGET}
	...
	@echo repeated delayed ${.TARGET}
	...
	@echo repeated delayed twice ${.TARGET}
