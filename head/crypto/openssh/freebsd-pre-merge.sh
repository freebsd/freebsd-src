#!/bin/sh
#
# $FreeBSD$
#

find . -type f -name '*.[1-9ch]' | cut -c 3- | \
while read f ; do
	svn propget svn:keywords $f | grep -q . && echo $f
done >keywords
xargs perl -n -i -e '
	$strip = $ARGV if /\$(Id|OpenBSD):.*\$/;
	print unless ($strip eq $ARGV && /\$FreeBSD.*\$/);
' <keywords

find . -type f -name '*.[1-9]' | cut -c 3- | \
	xargs grep -l '^\.Dd ' . >mdocdates
xargs perl -p -i -e '
	s/^\.Dd (\w+) (\d+), (\d+)$/.Dd \$Mdocdate: $1 $2 $3 \$/;
' <mdocdates
