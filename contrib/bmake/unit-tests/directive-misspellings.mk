# $NetBSD: directive-misspellings.mk,v 1.3 2020/12/13 01:10:22 rillig Exp $
#
# Tests for misspelled directives.
#
# Before 2020-12-12, make didn't catch most of these misspellings.  For
# example, the directive ".exporting" was interpreted as if it were spelled
# ".export ing", which would export the variable named "ing" if that existed.
# Another misspelling, as improbable as the others, was that both ".infos" and
# ".information" were aliases to ".info" since the code for these diagnostic
# directives just skipped any letters following the "error", "warn" or "info".

.dinclud "file"
.dinclude "file"
.dincludx "file"
.dincludes "file"		# XXX: the 's' is not meant to be a filename

.erro msg
.errox msg
# no .error since that would exit immediately
# no .errors since that would exit immediately, even with the typo

.expor varname
.export varname
.exporx varname
.exports varname		# Accepted before 2020-12-13 01:07:54.

.export-en			# Accepted before 2020-12-13 01:07:54.
.export-env
.export-env extra argument	# XXX: undetected extra argument
.export-environment		# Accepted before 2020-12-13 01:07:54.

.export-litera varname		# Accepted before 2020-12-13 01:07:54.
.export-literal varname
.export-literax varname		# Accepted before 2020-12-13 01:07:54.
.export-literally varname	# Accepted before 2020-12-13 01:07:54.

.-includ "file"
.-include "file"
.-includx "file"
.-includes "file"		# XXX: the 's' is not meant to be a filename

.includ "file"
.include "file"
.includx "file"
.includex "file"		# XXX: the 's' is not meant to be a filename

.inf msg
.info msg
.infx msg
.infos msg			# Accepted before 2020-12-13 01:07:54.

.sinclud "file"
.sinclude "file"
.sincludx "file"
.sincludes "file"		# XXX: the 's' is not meant to be a filename

.unde varname
.undef varname
.undex varname
.undefs varname			# Accepted before 2020-12-13 01:07:54.

.unexpor varname
.unexport varname
.unexporx varname
.unexports varname		# Accepted before 2020-12-12 18:00:18.

.unexport-en			# Accepted before 2020-12-12 18:11:42.
.unexport-env
.unexport-env extra argument	# Accepted before 2020-12-12 18:00:18.
.unexport-enx			# Accepted before 2020-12-12 18:00:18.
.unexport-envs			# Accepted before 2020-12-12 18:00:18.

.warn msg
.warnin msg
.warning msg
.warninx msg
.warnings msg			# Accepted before 2020-12-13 01:07:54.

all:
