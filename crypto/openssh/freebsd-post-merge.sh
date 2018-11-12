#!/bin/sh
#
# $FreeBSD$
#

xargs perl -n -i -e '
	print;
	s/\$(Id|OpenBSD): [^\$]*/\$FreeBSD/ && print;
	m/^\#include "includes.h"/ && print "__RCSID(\"\$FreeBSD\$\");\n";
' <keywords

xargs perl -p -i -e '
	s/^\.Dd \$Mdocdate: (\w+) (\d+) (\d+) \$$/.Dd $1 $2, $3/
' <mdocdates
