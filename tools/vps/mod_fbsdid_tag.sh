#!/bin/sh -x

set -e

#find . -type f -not -path \.svn -exec sed -i '' 's/__FBSDID(\"\$FreeBSD: projects\/vps\//__FBSDID(\"\$FreeBSD: head\//' \{\} \;
find . -type f -not -path \.svn -exec sed -i '' 's/\$FreeBSD: projects\/vps\//\$FreeBSD: head\//' \{\} \;

exit 0
