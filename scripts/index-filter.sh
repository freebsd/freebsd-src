#!/bin/bash

set -e
set -o pipefail

git ls-files -s | tee /tmp/bar | sed -n -f ${SCRIPTS}/rewrite-paths.sed | tee /tmp/foo | \
	GIT_INDEX_FILE=$GIT_INDEX_FILE.new git update-index --index-info

if [ -f "$GIT_INDEX_FILE.new" ] ; then
    mv "$GIT_INDEX_FILE.new" "$GIT_INDEX_FILE"
else
    rm "$GIT_INDEX_FILE"
fi

exit 0
