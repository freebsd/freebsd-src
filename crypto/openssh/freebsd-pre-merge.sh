#!/bin/sh
#
# $FreeBSD: stable/10/crypto/openssh/freebsd-pre-merge.sh 263691 2014-03-24 19:15:13Z des $
#

:>keywords
:>rcsid
svn list -R | grep -v '/$' | \
while read f ; do
	svn proplist -v $f | grep -q 'FreeBSD=%H' || continue
	egrep -l '^(#|\.\\"|/\*)[[:space:]]+\$FreeBSD[:\$]' $f >>keywords
	egrep -l '__RCSID\("\$FreeBSD[:\$]' $f >>rcsid
done
sort -u keywords rcsid | xargs perl -n -i -e '
	$strip = $ARGV if /\$(Id|OpenBSD):.*\$/;
	print unless (($strip eq $ARGV || /__RCSID/) && /\$FreeBSD[:\$]/);
'
