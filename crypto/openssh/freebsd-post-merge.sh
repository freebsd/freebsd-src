#!/bin/sh
#
# $FreeBSD: stable/10/crypto/openssh/freebsd-post-merge.sh 263691 2014-03-24 19:15:13Z des $
#

xargs perl -n -i -e '
	print;
	s/\$(Id|OpenBSD): [^\$]*/\$FreeBSD/ && print;
' <keywords

xargs perl -n -i -e '
	print;
	m/^\#include "includes.h"/ && print "__RCSID(\"\$FreeBSD\$\");\n";
' <rcsid
