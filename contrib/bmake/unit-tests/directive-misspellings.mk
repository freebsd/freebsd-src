# $NetBSD: directive-misspellings.mk,v 1.5 2025/06/28 22:39:28 rillig Exp $
#
# Tests for misspelled directives.
#
# Before 2020-12-12, make didn't catch most of these misspellings.  For
# example, the directive ".exporting" was interpreted as if it were spelled
# ".export ing", which would export the variable named "ing" if that existed.
# Another misspelling, as improbable as the others, was that both ".infos" and
# ".information" were aliases to ".info" since the code for these diagnostic
# directives just skipped any letters following the "error", "warn" or "info".

# expect+1: Unknown directive "dinclud"
.dinclud "file"
.dinclude "file"
# expect+1: Unknown directive "dincludx"
.dincludx "file"
# expect+1: .include filename must be delimited by "" or <>
.dincludes "file"		# XXX: the 's' is not meant to be a filename

# expect+1: Unknown directive "erro"
.erro msg
# expect+1: Unknown directive "errox"
.errox msg
# no .error since that would exit immediately
# no .errors since that would exit immediately, even with the typo

# expect+1: Unknown directive "expor"
.expor varname
.export varname
# expect+1: Unknown directive "exporx"
.exporx varname
# expect+1: Unknown directive "exports"
.exports varname		# Accepted before 2020-12-13 01:07:54.

# expect+1: Unknown directive "export-en"
.export-en			# Accepted before 2020-12-13 01:07:54.
.export-env
.export-env extra argument	# XXX: undetected extra argument
# expect+1: Unknown directive "export-environment"
.export-environment		# Accepted before 2020-12-13 01:07:54.

# expect+1: Unknown directive "export-litera"
.export-litera varname		# Accepted before 2020-12-13 01:07:54.
.export-literal varname
# expect+1: Unknown directive "export-literax"
.export-literax varname		# Accepted before 2020-12-13 01:07:54.
# expect+1: Unknown directive "export-literally"
.export-literally varname	# Accepted before 2020-12-13 01:07:54.

# expect+1: Unknown directive "-includ"
.-includ "file"
.-include "file"
# expect+1: Unknown directive "-includx"
.-includx "file"
# expect+1: .include filename must be delimited by "" or <>
.-includes "file"		# XXX: the 's' is not meant to be a filename

# expect+1: Unknown directive "includ"
.includ "file"
# expect+1: Could not find file
.include "file"
# expect+1: Unknown directive "includx"
.includx "file"
# expect+1: .include filename must be delimited by "" or <>
.includex "file"		# XXX: the 's' is not meant to be a filename

# expect+1: Unknown directive "inf"
.inf msg
# expect+1: msg
.info msg
# expect+1: Unknown directive "infx"
.infx msg
# expect+1: Unknown directive "infos"
.infos msg			# Accepted before 2020-12-13 01:07:54.

# expect+1: Unknown directive "sinclud"
.sinclud "file"
.sinclude "file"
# expect+1: Unknown directive "sincludx"
.sincludx "file"
# expect+1: .include filename must be delimited by "" or <>
.sincludes "file"		# XXX: the 's' is not meant to be a filename

# expect+1: Unknown directive "unde"
.unde varname
.undef varname
# expect+1: Unknown directive "undex"
.undex varname
# expect+1: Unknown directive "undefs"
.undefs varname			# Accepted before 2020-12-13 01:07:54.

# expect+1: Unknown directive "unexpor"
.unexpor varname
.unexport varname
# expect+1: Unknown directive "unexporx"
.unexporx varname
# expect+1: Unknown directive "unexports"
.unexports varname		# Accepted before 2020-12-12 18:00:18.

# expect+1: Unknown directive "unexport-en"
.unexport-en			# Accepted before 2020-12-12 18:11:42.
.unexport-env
# expect+1: The directive .unexport-env does not take arguments
.unexport-env extra argument	# Accepted before 2020-12-12 18:00:18.
# expect+1: Unknown directive "unexport-enx"
.unexport-enx			# Accepted before 2020-12-12 18:00:18.
# expect+1: Unknown directive "unexport-envs"
.unexport-envs			# Accepted before 2020-12-12 18:00:18.

# expect+1: Unknown directive "warn"
.warn msg
# expect+1: Unknown directive "warnin"
.warnin msg
# expect+1: warning: msg
.warning msg
# expect+1: Unknown directive "warninx"
.warninx msg
# expect+1: Unknown directive "warnings"
.warnings msg			# Accepted before 2020-12-13 01:07:54.
