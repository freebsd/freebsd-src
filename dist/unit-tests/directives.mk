# $NetBSD: directives.mk,v 1.5 2020/07/28 20:57:59 rillig Exp $
#
# Tests for parsing directives, in the same order as in the manual page.
#
# Each test group has 10 lines, to keep the line numbers in directives.exp
# stable.
#
# no tests for .error since it exits immediately, see ParseMessage.

.info begin .export tests
.expor				# misspelled
.export				# oops: missing argument
.export VARNAME
.exporting works		# oops: misspelled





.info begin .export-env tests
.export-en			# oops: misspelled
.export-env
.export-environment		# oops: misspelled






.info begin .export-literal tests
.export-litera			# oops: misspelled
.export-literal			# oops: missing argument
.export-literal VARNAME
.export-literally		# oops: misspelled





.info begin .info tests
.inf				# misspelled
.info				# oops: message should be "missing parameter"
.info message
.info		indented message
.information
.information message		# oops: misspelled
.info.man:			# not a message, but a suffix rule


.info begin .undef tests
.unde				# misspelled
.undef				# oops: missing argument
.undefined			# oops: misspelled
.undef VARNAME





.info begin .unexport tests
.unexpor			# misspelled
.unexport			# oops: missing argument
.unexport VARNAME		# ok
.unexporting works		# oops: misspelled





.info begin .unexport-env tests
.unexport-en			# misspelled
.unexport-env			# ok
.unexport-environment		# oops: misspelled






.info begin .warning tests
.warn				# misspelled
.warnin				# misspelled
.warning			# oops: should be "missing argument"
.warning message		# ok
.warnings			# misspelled
.warnings messages		# oops



.info begin .elif misspellings tests, part 1
.if 1
.elif 1				# ok
.elsif 1			# oops: misspelled
.elseif 1			# oops: misspelled
.endif




.info begin .elif misspellings tests, part 2
.if 0
.elif 0				# ok
.elsif 0			# oops: misspelled
.elseif 0			# oops: misspelled
.endif




.info begin .elif misspellings tests, part 3
.if 0
.elsif 0			# oops: misspelled
.endif
.if 0
.elseif 0			# oops: misspelled
.endif



.info which branch is taken on misspelling after false?
.if 0
.elsif 1
.info 1 taken
.elsif 2
.info 2 taken
.else
.info else taken
.endif

.info which branch is taken on misspelling after true?
.if 1
.elsif 1
.info 1 taken
.elsif 2
.info 2 taken
.else
.info else taken
.endif

.indented none
.  indented 2 spaces
.	indented tab
.${:Uinfo} directives cannot be indirect






.include "nonexistent.mk"
.include "/dev/null"		# size 0
# including a directory technically succeeds, but shouldn't.
#.include "."			# directory






.info end of the tests

all:
	@:
