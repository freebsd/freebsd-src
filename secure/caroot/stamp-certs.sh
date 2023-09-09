#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

files=$(grep -Lr '$FreeBSD' "$scriptdir"/*/*.pem)

if [ -z "$files" ]; then
	1>&2 echo "No certs to stamp."
	exit 0
fi

for f in $files; do
	echo "Stamping $f"
	sed -i.bak -e $'/Extracted from/a\\\n##  with $FreeBSD$' "$f" && \
	    rm "$f.bak"
done

1>&2 echo "Done."
