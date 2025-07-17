#!/bin/bash

set -e
set -o pipefail

${SCRIPTS}/rewrite-index.pl | GIT_INDEX_FILE=$GIT_INDEX_FILE.new git update-index --index-info

if [ -f "$GIT_INDEX_FILE.new" ] ; then
    mv "$GIT_INDEX_FILE.new" "$GIT_INDEX_FILE"
else
    rm "$GIT_INDEX_FILE"
fi

exit 0
