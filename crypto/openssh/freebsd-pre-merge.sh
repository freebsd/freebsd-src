#!/bin/sh
#
# $FreeBSD$
#

:>keywords
:>rcsid
git ls-files | \
while read f ; do
	egrep -l '^(#|\.\\"|/\*)[[:space:]]+\$FreeBSD[:\$]' $f >>keywords
	egrep -l '__RCSID\("\$FreeBSD[:\$]' $f >>rcsid
done
sort -u keywords rcsid | xargs perl -n -i -e '
	$strip = $ARGV if /\$(Id|OpenBSD):.*\$/;
	print unless (($strip eq $ARGV || /__RCSID/) && /\$FreeBSD[:\$]/);
'
